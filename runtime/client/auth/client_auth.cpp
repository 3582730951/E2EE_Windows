#include "client_core.h"
#include "auth_service.h"

#include <algorithm>
#include <array>
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
  if (!RegisterDevice(root_code)) {
    if (last_error_ == "root auth required" ||
        last_error_ == "root auth invalid") {
      auth_service.Logout(*this);
    }
    return false;
  }
  return true;
}

bool ClientCore::BeginQrLogin(std::string& out_payload) {
  out_payload.clear();
  last_error_.clear();
  if (!token_.empty()) {
    last_error_ = "already logged in";
    return false;
  }
  CancelQrLogin();
  if (!LoadOrCreateDeviceId()) {
    if (last_error_.empty()) {
      last_error_ = "device id unavailable";
    }
    return false;
  }
  if (device_id_.empty()) {
    last_error_ = "device id unavailable";
    return false;
  }

  mi::server::Frame req;
  req.type = mi::server::FrameType::kQrLoginInit;
  if (!mi::server::proto::WriteString(device_id_, req.payload)) {
    last_error_ = "device id invalid";
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
  if (!mi::server::proto::ReadString(resp.payload, off, qr_id) ||
      !mi::server::proto::ReadString(resp.payload, off, secret_hex) ||
      off != resp.payload.size() || qr_id.empty() || secret_hex.empty()) {
    last_error_ = "qr login init response invalid";
    return false;
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
  static constexpr char kQrPrefix[] = "mi_e2ee://qr-login?v=1";
  qr_login_payload_ = std::string(kQrPrefix) + "&id=" + qr_id +
                      "&s=" + secret_hex + "&d=" + device_id_;
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
  password_.clear();
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
  if (username_.empty() || password_.empty()) {
    last_error_ = "no cached credentials";
    return false;
  }
  const std::string root_code = GetEnvValue("MI_E2EE_ROOT_AUTH_CODE");
  return LoginWithRootCode(username_, password_, root_code);
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

}  // namespace mi::client
