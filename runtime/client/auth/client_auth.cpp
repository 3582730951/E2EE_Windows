#include "client_core.h"
#include "auth_service.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include "chat_history_store.h"
#include "crypto.h"
#include "hex_utils.h"
#include "monocypher.h"
#include "platform_time.h"
#include "protocol.h"

namespace mi::client {

namespace {

std::string GetEnvValue(const char* name) {
  if (!name || *name == '\0') {
    return {};
  }
  const char* v = std::getenv(name);
  return v ? std::string(v) : std::string();
}

std::string UrlEncode(const std::string& input) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string out;
  out.reserve(input.size());
  for (unsigned char ch : input) {
    if ((ch >= 'a' && ch <= 'z') ||
        (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') ||
        ch == '-' || ch == '_' || ch == '.' || ch == '~') {
      out.push_back(static_cast<char>(ch));
    } else {
      out.push_back('%');
      out.push_back(kHex[(ch >> 4) & 0x0F]);
      out.push_back(kHex[ch & 0x0F]);
    }
  }
  return out;
}

}  // namespace

bool ClientCore::Register(const std::string& username,
                          const std::string& password) {
  AuthService auth_service;
  return auth_service.Register(*this, username, password);
}

bool ClientCore::Login(const std::string& username,
                       const std::string& password) {
  const std::string root_code = GetEnvValue("MI_E2EE_ROOT_AUTH_CODE");
  return LoginWithRootCode(username, password, root_code);
}

bool ClientCore::LoginWithRootCode(const std::string& username,
                                   const std::string& password,
                                   const std::string& root_code) {
  AuthService auth_service;
  if (!auth_service.Login(*this, username, password)) {
    return false;
  }
  const int max_attempts = 3;
  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    if (RegisterDevice(root_code)) {
      return true;
    }
    if (last_error_ == "root auth required" ||
        last_error_ == "root auth invalid") {
      auth_service.Logout(*this);
      return false;
    }
    if (attempt + 1 < max_attempts) {
      ResetRemoteStream();
      mi::platform::SleepMs(200);
    }
  }
  return false;
}

bool ClientCore::BeginQrLogin(std::string& out_payload) {
  return BeginQrLoginWithUsername(std::string(), out_payload);
}

bool ClientCore::BeginQrLoginWithUsername(const std::string& username,
                                          std::string& out_payload) {
  out_payload.clear();
  last_error_.clear();
  if (!token_.empty()) {
    last_error_ = "already logged in";
    return false;
  }
  CancelQrLogin();
  if (!LoadOrCreateDeviceClaimId()) {
    if (last_error_.empty()) {
      last_error_ = "device claim id unavailable";
    }
    return false;
  }
  if (device_claim_id_.empty()) {
    last_error_ = "device claim id unavailable";
    return false;
  }

  mi::server::Frame req;
  req.type = mi::server::FrameType::kQrLoginInit;
  if (username.empty()) {
    if (!mi::server::proto::WriteString(device_claim_id_, req.payload)) {
      last_error_ = "qr login request invalid";
      return false;
    }
  } else if (!mi::server::proto::WriteString(username, req.payload) ||
             !mi::server::proto::WriteString(device_claim_id_, req.payload)) {
    last_error_ = "qr login request invalid";
    return false;
  }

  std::vector<std::uint8_t> resp_vec;
  if (!ProcessRaw(mi::server::EncodeFrame(req), resp_vec)) {
    if (last_error_.empty()) {
      last_error_ = "qr login init failed";
    }
    return false;
  }
  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kQrLoginInit ||
      resp.payload.empty()) {
    last_error_ = "qr login init response invalid";
    return false;
  }
  std::size_t off = 1;
  if (resp.payload[0] == 0) {
    std::string err;
    mi::server::proto::ReadString(resp.payload, off, err);
    last_error_ = err.empty() ? "qr login init failed" : err;
    return false;
  }
  std::string qr_id;
  std::string secret_hex;
  std::string display_id;
  if (!mi::server::proto::ReadString(resp.payload, off, qr_id) ||
      !mi::server::proto::ReadString(resp.payload, off, secret_hex)) {
    last_error_ = "qr login init response invalid";
    return false;
  }
  if (off < resp.payload.size()) {
    if (!mi::server::proto::ReadString(resp.payload, off, display_id) ||
        off != resp.payload.size()) {
      last_error_ = "qr login init response invalid";
      return false;
    }
  } else if (off != resp.payload.size()) {
    last_error_ = "qr login init response invalid";
    return false;
  }
  if (qr_id.empty() || secret_hex.empty()) {
    last_error_ = "qr login init response invalid";
    return false;
  }
  if (!display_id.empty()) {
    if (display_id.size() != 32) {
      last_error_ = "qr login init response invalid";
      return false;
    }
    for (const char ch : display_id) {
      const unsigned char uc = static_cast<unsigned char>(ch);
      if (!(std::isdigit(uc) || (uc >= 'a' && uc <= 'f') ||
            (uc >= 'A' && uc <= 'F'))) {
        last_error_ = "qr login init response invalid";
        return false;
      }
    }
    std::transform(display_id.begin(), display_id.end(), display_id.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    device_auth_id_ = display_id;
    AuthService().SaveDeviceAuthId(*this);
  }
  std::vector<std::uint8_t> secret_bytes;
  if (!mi::common::HexToBytes(secret_hex, secret_bytes) ||
      secret_bytes.size() != qr_login_secret_.size()) {
    last_error_ = "qr login secret invalid";
    return false;
  }
  std::copy_n(secret_bytes.begin(), qr_login_secret_.size(),
              qr_login_secret_.begin());
  qr_login_id_ = qr_id;
  qr_login_secret_hex_ = secret_hex;
  qr_login_active_ = true;
  static constexpr char kQrPrefix[] = "mi_e2ee://qr-login?v=2";
  qr_login_payload_ = std::string(kQrPrefix) + "&id=" + qr_id +
                      "&s=" + secret_hex;
  const std::string qr_display =
      !display_id.empty() ? display_id : device_auth_id_;
  if (!qr_display.empty()) {
    qr_login_payload_ += "&d=" + qr_display;
  }
  if (!username.empty()) {
    qr_login_payload_ += "&u=" + UrlEncode(username);
  }
  if (remote_mode_ && !server_ip_.empty()) {
    const std::uint16_t port =
        server_port_plain_ != 0 ? server_port_plain_ : server_port_;
    if (port != 0) {
      qr_login_payload_ += "&h=" + UrlEncode(server_ip_);
      qr_login_payload_ += "&p=" + std::to_string(port);
      qr_login_payload_ += "&tls=" + std::string(qr_login_use_tls_ ? "1" : "0");
      if (!pinned_server_fingerprint_.empty()) {
        qr_login_payload_ += "&fp=" + pinned_server_fingerprint_;
      }
    }
  }
  out_payload = qr_login_payload_;
  return true;
}

bool ClientCore::PollQrLogin(bool& out_completed) {
  out_completed = false;
  last_error_.clear();
  if (!qr_login_active_ || qr_login_id_.empty() ||
      qr_login_secret_hex_.empty()) {
    last_error_ = "qr login not active";
    return false;
  }
  mi::server::Frame req;
  req.type = mi::server::FrameType::kQrLoginPoll;
  if (!mi::server::proto::WriteString(qr_login_id_, req.payload) ||
      !mi::server::proto::WriteString(qr_login_secret_hex_, req.payload)) {
    last_error_ = "qr login request invalid";
    return false;
  }
  std::vector<std::uint8_t> resp_vec;
  if (!ProcessRaw(mi::server::EncodeFrame(req), resp_vec)) {
    if (last_error_.empty()) {
      last_error_ = "qr login poll failed";
    }
    return false;
  }
  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kQrLoginPoll ||
      resp.payload.empty()) {
    last_error_ = "qr login poll response invalid";
    return false;
  }
  std::size_t off = 1;
  if (resp.payload[0] == 0) {
    std::string err;
    mi::server::proto::ReadString(resp.payload, off, err);
    last_error_ = err.empty() ? "qr login poll failed" : err;
    return false;
  }
  if (off >= resp.payload.size()) {
    last_error_ = "qr login poll response invalid";
    return false;
  }
  const bool completed = resp.payload[off++] != 0;
  if (!completed) {
    out_completed = false;
    return true;
  }
  std::string token;
  std::string username;
  if (!mi::server::proto::ReadString(resp.payload, off, token) ||
      !mi::server::proto::ReadString(resp.payload, off, username) ||
      off != resp.payload.size()) {
    last_error_ = "qr login poll response invalid";
    return false;
  }
  if (token.empty() || username.empty()) {
    last_error_ = "qr login poll response invalid";
    return false;
  }

  std::vector<std::uint8_t> session_key(qr_login_secret_.begin(),
                                        qr_login_secret_.end());
  token_ = token;
  username_ = username;
  std::string key_err;
  if (!mi::server::DeriveKeysFromOpaqueSessionKey(
          session_key, username, token_, transport_kind_, keys_, key_err)) {
    token_.clear();
    last_error_ = key_err.empty() ? "key derivation failed" : key_err;
    CancelQrLogin();
    return false;
  }

  channel_ = mi::server::SecureChannel(
      keys_, mi::server::SecureChannelRole::kClient);
  send_seq_ = 0;
  prekey_published_ = false;
  if (e2ee_inited_) {
    e2ee_.SetLocalUsername(username_);
  }
  if (history_enabled_ && !e2ee_state_dir_.empty()) {
    auto store = std::make_unique<ChatHistoryStore>();
    std::string hist_err;
    if (store->Init(e2ee_state_dir_, username_, hist_err)) {
      history_store_ = std::move(store);
      WarmupHistoryOnStartup();
    } else {
      history_store_.reset();
    }
  } else {
    history_store_.reset();
  }
  friend_sync_version_ = 0;
  CancelQrLogin();
  out_completed = true;
  last_error_.clear();
  return true;
}

bool ClientCore::ApproveQrLogin(const std::string& qr_id,
                                const std::string& qr_secret_hex) {
  return ApproveQrLogin(qr_id, qr_secret_hex, std::string());
}

bool ClientCore::ApproveQrLogin(const std::string& qr_id,
                                const std::string& qr_secret_hex,
                                const std::string& root_code) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (qr_id.empty() || qr_secret_hex.empty()) {
    last_error_ = "qr login data missing";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(qr_id, plain);
  mi::server::proto::WriteString(qr_secret_hex, plain);
  if (!root_code.empty()) {
    mi::server::proto::WriteString(root_code, plain);
  }
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kQrLoginApprove, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "qr login approve failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "qr login approve response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, err);
    last_error_ = err.empty() ? "qr login approve failed" : err;
    return false;
  }
  if (resp_payload.size() != 1) {
    last_error_ = "qr login approve response invalid";
    return false;
  }
  return true;
}

void ClientCore::CancelQrLogin() {
  if (!qr_login_secret_hex_.empty()) {
    qr_login_secret_hex_.clear();
  }
  if (!qr_login_payload_.empty()) {
    qr_login_payload_.clear();
  }
  if (!qr_login_id_.empty()) {
    qr_login_id_.clear();
  }
  crypto_wipe(qr_login_secret_.data(), qr_login_secret_.size());
  qr_login_secret_.fill(0);
  qr_login_active_ = false;
}

bool ClientCore::Relogin() {
  last_error_ = "explicit login required";
  return false;
}

bool ClientCore::Logout() {
  CancelQrLogin();
  AuthService auth_service;
  return auth_service.Logout(*this);
}

bool ClientCore::PublishPreKeys() {
  return EnsurePreKeyPublished();
}

bool ClientCore::LoadKtState() {
  AuthService auth_service;
  return auth_service.LoadKtState(*this);
}

bool ClientCore::SaveKtState() {
  AuthService auth_service;
  return auth_service.SaveKtState(*this);
}

void ClientCore::RecordKtGossipMismatch(const std::string& reason) {
  if (kt_gossip_alert_threshold_ == 0) {
    kt_gossip_alert_threshold_ = 3;
  }
  if (kt_gossip_mismatch_count_ < (std::numeric_limits<std::uint32_t>::max)()) {
    kt_gossip_mismatch_count_++;
  }
  if (kt_gossip_mismatch_count_ >= kt_gossip_alert_threshold_) {
    kt_gossip_alerted_ = true;
    last_error_ = reason.empty() ? "kt gossip alert"
                                 : ("kt gossip alert: " + reason);
    return;
  }
  if (!reason.empty()) {
    last_error_ = reason;
  }
}

bool ClientCore::FetchKtConsistency(
    std::uint64_t old_size, std::uint64_t new_size,
    std::vector<std::array<std::uint8_t, 32>>& out_proof) {
  out_proof.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (old_size == 0 || new_size == 0 || old_size >= new_size) {
    last_error_ = "invalid kt sizes";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteUint64(old_size, plain);
  mi::server::proto::WriteUint64(new_size, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kKeyTransparencyConsistency, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "kt consistency failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "kt response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, err);
    last_error_ = err.empty() ? "kt consistency failed" : err;
    return false;
  }
  std::size_t off = 1;
  std::uint64_t got_old = 0;
  std::uint64_t got_new = 0;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint64(resp_payload, off, got_old) ||
      !mi::server::proto::ReadUint64(resp_payload, off, got_new) ||
      !mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "kt response invalid";
    return false;
  }
  out_proof.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::vector<std::uint8_t> node;
    if (!mi::server::proto::ReadBytes(resp_payload, off, node) ||
        node.size() != 32) {
      last_error_ = "kt response invalid";
      out_proof.clear();
      return false;
    }
    std::array<std::uint8_t, 32> h{};
    std::copy_n(node.begin(), h.size(), h.begin());
    out_proof.push_back(h);
  }
  if (off != resp_payload.size() || got_old != old_size || got_new != new_size) {
    last_error_ = "kt response invalid";
    out_proof.clear();
    return false;
  }
  return true;
}



bool ClientCore::EnsureE2ee() {
  if (e2ee_inited_) {
    return true;
  }
  if (e2ee_state_dir_.empty()) {
    const auto cfg_dir = ResolveConfigDir(config_path_);
    const auto data_dir = ResolveDataDir(cfg_dir);
    std::filesystem::path base = data_dir;
    if (base.empty()) {
      base = cfg_dir;
    }
    if (base.empty()) {
      base = std::filesystem::path{"."};
    }
    e2ee_state_dir_ = base / "e2ee_state";
    kt_state_path_ = e2ee_state_dir_ / "kt_state.bin";
    LoadKtState();
  }

  std::string err;
  e2ee_.SetIdentityPolicy(identity_policy_);
  if (!e2ee_.Init(e2ee_state_dir_, err)) {
    last_error_ = err.empty() ? "e2ee init failed" : err;
    return false;
  }
  if (!username_.empty()) {
    e2ee_.SetLocalUsername(username_);
  }
  e2ee_inited_ = true;
  return true;
}

bool ClientCore::LoadOrCreateDeviceId() {
  AuthService auth_service;
  return auth_service.LoadOrCreateDeviceId(*this);
}

bool ClientCore::LoadOrCreateDeviceClaimId() {
  AuthService auth_service;
  return auth_service.LoadOrCreateDeviceClaimId(*this);
}

bool ClientCore::LoadOrCreateDeviceAuthId() {
  AuthService auth_service;
  return auth_service.LoadOrCreateDeviceAuthId(*this);
}

}  // namespace mi::client
