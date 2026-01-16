#include "auth_service.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include "client_core.h"
#include "chat_history_store.h"
#include "secure_buffer.h"
#include "secure_store_util.h"
#include "opaque_pake.h"
#include "path_security.h"
#include "platform_fs.h"
#include "platform_random.h"
#include "protocol.h"

namespace mi::client {

namespace pfs = mi::platform::fs;

namespace {

constexpr std::size_t kMaxOpaqueMessageBytes = 16 * 1024;
constexpr std::size_t kMaxOpaqueSessionKeyBytes = 1024;
constexpr std::size_t kMaxDeviceIdFileBytes = 4 * 1024;

std::string Trim(const std::string& input) {
  const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  auto begin = std::find_if_not(input.begin(), input.end(), is_space);
  auto end = std::find_if_not(input.rbegin(), input.rend(), is_space).base();
  if (begin >= end) {
    return {};
  }
  return std::string(begin, end);
}

std::string BytesToHexLower(const std::uint8_t* data, std::size_t len) {
  static constexpr char kHex[] = "0123456789abcdef";
  if (!data || len == 0) {
    return {};
  }
  std::string out;
  out.resize(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out[i * 2] = kHex[data[i] >> 4];
    out[i * 2 + 1] = kHex[data[i] & 0x0F];
  }
  return out;
}

bool RandomBytes(std::uint8_t* out, std::size_t len) {
  return mi::platform::RandomBytes(out, len);
}

}  // namespace

bool AuthService::Register(ClientCore& core, const std::string& username,
                           const std::string& password) const {
  core.last_error_.clear();
  core.username_ = username;
  core.password_ = password;
  if (username.empty() || password.empty()) {
    core.last_error_ = "credentials empty";
    return false;
  }

  if (core.auth_mode_ != AuthMode::kOpaque) {
    core.last_error_ = "register requires auth_mode=opaque";
    return false;
  }

  struct RustBuf {
    std::uint8_t* ptr{nullptr};
    std::size_t len{0};
    ~RustBuf() {
      if (ptr && len) {
        if (len > kMaxOpaqueMessageBytes) {
          return;  // avoid passing suspicious length to the Rust allocator
        }
        mi_opaque_free(ptr, len);
      }
    }
  };

  auto RustError = [&](const RustBuf& err,
                       const char* fallback) -> std::string {
    if (err.ptr && err.len && err.len <= kMaxOpaqueMessageBytes) {
      return std::string(reinterpret_cast<const char*>(err.ptr), err.len);
    }
    return fallback ? std::string(fallback) : std::string();
  };

  RustBuf req;
  RustBuf state;
  RustBuf err;
  const int start_rc = mi_opaque_client_register_start(
      reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
      &req.ptr, &req.len, &state.ptr, &state.len, &err.ptr, &err.len);
  if (start_rc != 0 || !req.ptr || req.len == 0 || !state.ptr ||
      state.len == 0) {
    core.last_error_ = RustError(err, "opaque register start failed");
    return false;
  }
  const std::vector<std::uint8_t> req_vec(req.ptr, req.ptr + req.len);
  std::vector<std::uint8_t> state_vec(state.ptr, state.ptr + state.len);
  mi::common::ScopedWipe state_wipe(state_vec);

  mi::server::Frame start;
  start.type = mi::server::FrameType::kOpaqueRegisterStart;
  if (!mi::server::proto::WriteString(username, start.payload) ||
      !mi::server::proto::WriteBytes(req_vec, start.payload)) {
    core.last_error_ = "opaque register start payload too large";
    return false;
  }

  std::vector<std::uint8_t> resp_vec;
  if (!core.ProcessRaw(mi::server::EncodeFrame(start), resp_vec)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "opaque register start failed";
    }
    return false;
  }

  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kOpaqueRegisterStart ||
      resp.payload.empty()) {
    core.last_error_ = "opaque register start response invalid";
    return false;
  }

  std::size_t off = 1;
  if (resp.payload[0] == 0) {
    std::string err_msg;
    mi::server::proto::ReadString(resp.payload, off, err_msg);
    core.last_error_ =
        err_msg.empty() ? "opaque register start failed" : err_msg;
    return false;
  }
  std::vector<std::uint8_t> reg_resp;
  if (!mi::server::proto::ReadBytes(resp.payload, off, reg_resp) ||
      off != resp.payload.size() || reg_resp.empty()) {
    core.last_error_ = "opaque register start response invalid";
    return false;
  }

  RustBuf upload;
  RustBuf err2;
  const int finish_rc = mi_opaque_client_register_finish(
      reinterpret_cast<const std::uint8_t*>(username.data()), username.size(),
      reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
      state_vec.data(), state_vec.size(), reg_resp.data(), reg_resp.size(),
      &upload.ptr, &upload.len, &err2.ptr, &err2.len);
  if (finish_rc != 0 || !upload.ptr || upload.len == 0) {
    core.last_error_ = RustError(err2, "opaque register finish failed");
    return false;
  }
  const std::vector<std::uint8_t> upload_vec(upload.ptr,
                                             upload.ptr + upload.len);

  mi::server::Frame finish;
  finish.type = mi::server::FrameType::kOpaqueRegisterFinish;
  if (!mi::server::proto::WriteString(username, finish.payload) ||
      !mi::server::proto::WriteBytes(upload_vec, finish.payload)) {
    core.last_error_ = "opaque register finish payload too large";
    return false;
  }

  resp_vec.clear();
  if (!core.ProcessRaw(mi::server::EncodeFrame(finish), resp_vec)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "opaque register finish failed";
    }
    return false;
  }

  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kOpaqueRegisterFinish ||
      resp.payload.empty()) {
    core.last_error_ = "opaque register finish response invalid";
    return false;
  }
  off = 1;
  if (resp.payload[0] == 0) {
    std::string err_msg;
    mi::server::proto::ReadString(resp.payload, off, err_msg);
    core.last_error_ =
        err_msg.empty() ? "opaque register finish failed" : err_msg;
    return false;
  }
  if (off != resp.payload.size()) {
    core.last_error_ = "opaque register finish response invalid";
    return false;
  }

  core.last_error_.clear();
  return true;
}

bool AuthService::Login(ClientCore& core, const std::string& username,
                        const std::string& password) const {
#if 0
  core.username_ = username;
  core.password_ = password;
  core.last_error_.clear();

  constexpr std::size_t kKemSecretKeyBytes = 2400;
  static_assert(mi::server::kX25519PublicKeyBytes == 32);
  static_assert(mi::server::kMlKem768SharedSecretBytes == 32);

  std::array<std::uint8_t, 32> client_nonce{};
  if (!RandomBytes(client_nonce.data(), client_nonce.size())) {
    core.last_error_ = "rng failed";
    return false;
  }

  std::array<std::uint8_t, 32> client_dh_sk{};
  std::array<std::uint8_t, 32> client_dh_pk{};
  std::array<std::uint8_t, mi::server::kMlKem768PublicKeyBytes> client_kem_pk{};
  std::array<std::uint8_t, kKemSecretKeyBytes> client_kem_sk{};
  if (!RandomBytes(client_dh_sk.data(), client_dh_sk.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  crypto_x25519_public_key(client_dh_pk.data(), client_dh_sk.data());
  if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair(client_kem_pk.data(),
                                                client_kem_sk.data()) != 0) {
    core.last_error_ = "mlkem keypair failed";
    return false;
  }

  mi::server::Frame start;
  start.type = mi::server::FrameType::kPakeStart;
  mi::server::proto::WriteString(username, start.payload);
  mi::server::proto::WriteUint32(
      static_cast<std::uint32_t>(client_nonce.size()), start.payload);
  start.payload.insert(start.payload.end(), client_nonce.begin(),
                       client_nonce.end());
  mi::server::proto::WriteUint32(
      static_cast<std::uint32_t>(client_dh_pk.size()), start.payload);
  start.payload.insert(start.payload.end(), client_dh_pk.begin(),
                       client_dh_pk.end());
  mi::server::proto::WriteUint32(
      static_cast<std::uint32_t>(client_kem_pk.size()), start.payload);
  start.payload.insert(start.payload.end(), client_kem_pk.begin(),
                       client_kem_pk.end());

  std::vector<std::uint8_t> resp_vec;
  if (!core.ProcessRaw(mi::server::EncodeFrame(start), resp_vec)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "pake start failed";
    }
    return false;
  }

  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp)) {
    core.last_error_ = "pake start response invalid";
    return false;
  }
  if (resp.type != mi::server::FrameType::kPakeStart || resp.payload.empty()) {
    core.last_error_ = "pake start response invalid";
    return false;
  }

  std::size_t off = 1;
  if (resp.payload[0] == 0) {
    std::string err;
    mi::server::proto::ReadString(resp.payload, off, err);
    core.last_error_ = err.empty() ? "pake start failed" : err;
    return false;
  }

  std::string pake_id;
  std::vector<std::uint8_t> salt;
  std::vector<std::uint8_t> server_nonce;
  std::vector<std::uint8_t> server_dh_pk;
  std::vector<std::uint8_t> kem_ct;
  std::vector<std::uint8_t> server_proof;
  std::uint32_t argon_blocks = 0;
  std::uint32_t argon_passes = 0;
  if (!mi::server::proto::ReadString(resp.payload, off, pake_id) ||
      off >= resp.payload.size()) {
    core.last_error_ = "pake start response invalid";
    return false;
  }
  const std::uint8_t scheme = resp.payload[off++];
  if (!mi::server::proto::ReadUint32(resp.payload, off, argon_blocks) ||
      !mi::server::proto::ReadUint32(resp.payload, off, argon_passes) ||
      !mi::server::proto::ReadBytes(resp.payload, off, salt) ||
      !mi::server::proto::ReadBytes(resp.payload, off, server_nonce) ||
      !mi::server::proto::ReadBytes(resp.payload, off, server_dh_pk) ||
      !mi::server::proto::ReadBytes(resp.payload, off, kem_ct) ||
      !mi::server::proto::ReadBytes(resp.payload, off, server_proof) ||
      off != resp.payload.size()) {
    core.last_error_ = "pake start response invalid";
    return false;
  }
  if (server_nonce.size() != 32 ||
      server_dh_pk.size() != mi::server::kX25519PublicKeyBytes ||
      kem_ct.size() != mi::server::kMlKem768CiphertextBytes ||
      server_proof.size() != 32) {
    core.last_error_ = "pake start response invalid";
    return false;
  }

  std::array<std::uint8_t, 32> pw_key{};
  if (scheme == static_cast<std::uint8_t>(mi::server::PakePwScheme::kSha256)) {
    mi::server::crypto::Sha256Digest d;
    mi::server::crypto::Sha256(
        reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
        d);
    pw_key = d.bytes;
  } else if (scheme ==
             static_cast<std::uint8_t>(mi::server::PakePwScheme::kSaltedSha256)) {
    std::vector<std::uint8_t> msg;
    msg.reserve(salt.size() + password.size());
    msg.insert(msg.end(), salt.begin(), salt.end());
    msg.insert(msg.end(), password.begin(), password.end());
    mi::server::crypto::Sha256Digest d;
    mi::server::crypto::Sha256(msg.data(), msg.size(), d);
    pw_key = d.bytes;
  } else if (scheme ==
             static_cast<std::uint8_t>(mi::server::PakePwScheme::kArgon2id)) {
    if (argon_blocks == 0 || argon_passes == 0 || argon_blocks > 8192 ||
        argon_passes > 16 || salt.empty()) {
      core.last_error_ = "argon2id params invalid";
      return false;
    }
    std::vector<std::uint8_t> work_area;
    work_area.resize(static_cast<std::size_t>(argon_blocks) * 1024);

    crypto_argon2_config cfg;
    cfg.algorithm = CRYPTO_ARGON2_ID;
    cfg.nb_blocks = argon_blocks;
    cfg.nb_passes = argon_passes;
    cfg.nb_lanes = 1;

    crypto_argon2_inputs in;
    in.pass = reinterpret_cast<const std::uint8_t*>(password.data());
    in.pass_size = static_cast<std::uint32_t>(password.size());
    in.salt = salt.data();
    in.salt_size = static_cast<std::uint32_t>(salt.size());

    crypto_argon2(pw_key.data(), static_cast<std::uint32_t>(pw_key.size()),
                  work_area.data(), cfg, in, crypto_argon2_no_extras);
  } else {
    core.last_error_ = "pake scheme unsupported";
    return false;
  }

  std::array<std::uint8_t, 32> server_dh_pk_arr{};
  std::copy_n(server_dh_pk.begin(), server_dh_pk_arr.size(),
              server_dh_pk_arr.begin());
  std::array<std::uint8_t, 32> dh_shared{};
  crypto_x25519(dh_shared.data(), client_dh_sk.data(), server_dh_pk_arr.data());
  if (IsAllZero(dh_shared.data(), dh_shared.size())) {
    core.last_error_ = "x25519 shared invalid";
    return false;
  }

  std::array<std::uint8_t, 32> kem_shared{};
  if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_dec(kem_shared.data(), kem_ct.data(),
                                           client_kem_sk.data()) != 0) {
    core.last_error_ = "mlkem decaps failed";
    return false;
  }

  const auto build_transcript = [&]() {
    std::vector<std::uint8_t> t;
    static constexpr char kPrefix[] = "mi_e2ee_pake_login_v1";
    t.insert(t.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
    t.push_back(0);
    t.insert(t.end(), username.begin(), username.end());
    t.push_back(0);
    t.insert(t.end(), pake_id.begin(), pake_id.end());
    t.push_back(0);
    t.push_back(scheme);
    mi::server::proto::WriteUint32(argon_blocks, t);
    mi::server::proto::WriteUint32(argon_passes, t);
    const std::uint16_t salt_len = static_cast<std::uint16_t>(salt.size());
    t.push_back(static_cast<std::uint8_t>(salt_len & 0xFF));
    t.push_back(static_cast<std::uint8_t>((salt_len >> 8) & 0xFF));
    t.insert(t.end(), salt.begin(), salt.end());
    t.insert(t.end(), client_nonce.begin(), client_nonce.end());
    t.insert(t.end(), server_nonce.begin(), server_nonce.end());
    t.insert(t.end(), client_dh_pk.begin(), client_dh_pk.end());
    t.insert(t.end(), server_dh_pk.begin(), server_dh_pk.end());
    t.insert(t.end(), client_kem_pk.begin(), client_kem_pk.end());
    t.insert(t.end(), kem_ct.begin(), kem_ct.end());
    return t;
  };

  const auto transcript = build_transcript();
  mi::server::crypto::Sha256Digest transcript_hash;
  mi::server::crypto::Sha256(transcript.data(), transcript.size(),
                             transcript_hash);

  std::array<std::uint8_t, 64> ikm{};
  std::copy_n(dh_shared.begin(), dh_shared.size(), ikm.begin() + 0);
  std::copy_n(kem_shared.begin(), kem_shared.size(), ikm.begin() + 32);

  std::array<std::uint8_t, 32> handshake_key{};
  static constexpr char kInfoPrefix[] = "mi_e2ee_pake_hybrid_v1";
  std::uint8_t info[sizeof(kInfoPrefix) - 1 + 32];
  std::memcpy(info, kInfoPrefix, sizeof(kInfoPrefix) - 1);
  std::memcpy(info + (sizeof(kInfoPrefix) - 1), transcript_hash.bytes.data(),
              transcript_hash.bytes.size());
  if (!mi::server::crypto::HkdfSha256(ikm.data(), ikm.size(), pw_key.data(),
                                      pw_key.size(), info, sizeof(info),
                                      handshake_key.data(),
                                      handshake_key.size())) {
    core.last_error_ = "hkdf failed";
    return false;
  }

  static constexpr char kServerProofPrefix[] = "mi_e2ee_pake_server_proof_v1";
  std::uint8_t proof_msg[sizeof(kServerProofPrefix) - 1 + 32];
  std::memcpy(proof_msg, kServerProofPrefix, sizeof(kServerProofPrefix) - 1);
  std::memcpy(proof_msg + (sizeof(kServerProofPrefix) - 1),
              transcript_hash.bytes.data(), transcript_hash.bytes.size());
  mi::server::crypto::Sha256Digest expected_server_proof;
  mi::server::crypto::HmacSha256(
      handshake_key.data(), handshake_key.size(), proof_msg, sizeof(proof_msg),
      expected_server_proof);
  std::array<std::uint8_t, 32> server_proof_arr{};
  std::copy_n(server_proof.begin(), server_proof_arr.size(),
              server_proof_arr.begin());
  if (expected_server_proof.bytes != server_proof_arr) {
    core.last_error_ = "server proof invalid";
    return false;
  }

  static constexpr char kClientProofPrefix[] = "mi_e2ee_pake_client_proof_v1";
  std::uint8_t client_msg[sizeof(kClientProofPrefix) - 1 + 32];
  std::memcpy(client_msg, kClientProofPrefix, sizeof(kClientProofPrefix) - 1);
  std::memcpy(client_msg + (sizeof(kClientProofPrefix) - 1),
              transcript_hash.bytes.data(), transcript_hash.bytes.size());
  mi::server::crypto::Sha256Digest client_proof;
  mi::server::crypto::HmacSha256(handshake_key.data(), handshake_key.size(),
                                 client_msg, sizeof(client_msg), client_proof);

  mi::server::Frame finish;
  finish.type = mi::server::FrameType::kPakeFinish;
  mi::server::proto::WriteString(pake_id, finish.payload);
  mi::server::proto::WriteUint32(
      static_cast<std::uint32_t>(client_proof.bytes.size()), finish.payload);
  finish.payload.insert(finish.payload.end(), client_proof.bytes.begin(),
                        client_proof.bytes.end());
  resp_vec.clear();
  if (!core.ProcessRaw(mi::server::EncodeFrame(finish), resp_vec)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "pake finish failed";
    }
    return false;
  }

  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kPakeFinish ||
      resp.payload.empty()) {
    core.last_error_ = "pake finish response invalid";
    return false;
  }
  off = 1;
  std::string token_or_error;
  if (!mi::server::proto::ReadString(resp.payload, off, token_or_error) ||
      off != resp.payload.size()) {
    core.last_error_ = "pake finish response invalid";
    return false;
  }
  if (resp.payload[0] == 0) {
    core.last_error_ =
        token_or_error.empty() ? "pake finish failed" : token_or_error;
    return false;
  }
  core.token_ = std::move(token_or_error);

  std::string err;
  if (!mi::server::DeriveKeysFromPakeHandshake(handshake_key, username,
                                               core.token_, core.transport_kind_,
                                               core.keys_, err)) {
    core.token_.clear();
    core.last_error_ = err.empty() ? "key derivation failed" : err;
    return false;
  }

  core.channel_ =
      mi::server::SecureChannel(core.keys_,
                                mi::server::SecureChannelRole::kClient);
  core.send_seq_ = 0;
  core.prekey_published_ = false;
  if (core.e2ee_inited_) {
    core.e2ee_.SetLocalUsername(core.username_);
  }
  core.last_error_.clear();
  return true;
#endif

  core.last_error_.clear();
  core.username_ = username;
  core.password_ = password;
  core.token_.clear();
  core.send_seq_ = 0;
  core.prekey_published_ = false;

  if (username.empty() || password.empty()) {
    core.last_error_ = "credentials empty";
    return false;
  }

  if (core.auth_mode_ == AuthMode::kLegacy) {
    mi::server::Frame login;
    login.type = mi::server::FrameType::kLogin;
    if (!mi::server::proto::WriteString(username, login.payload) ||
        !mi::server::proto::WriteString(password, login.payload)) {
      core.last_error_ = "credentials too long";
      return false;
    }

    std::vector<std::uint8_t> resp_vec;
    if (!core.ProcessRaw(mi::server::EncodeFrame(login), resp_vec)) {
      if (core.last_error_.empty()) {
        core.last_error_ = "login failed";
      }
      return false;
    }

    mi::server::Frame resp;
    if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
        resp.type != mi::server::FrameType::kLogin || resp.payload.empty()) {
      core.last_error_ = "login response invalid";
      return false;
    }

    std::size_t off = 1;
    std::string token_or_error;
    if (!mi::server::proto::ReadString(resp.payload, off, token_or_error) ||
        off != resp.payload.size()) {
      core.last_error_ = "login response invalid";
      return false;
    }
    if (resp.payload[0] == 0) {
      core.last_error_ =
          token_or_error.empty() ? "login failed" : token_or_error;
      return false;
    }
    core.token_ = std::move(token_or_error);

    std::string key_err;
    if (!mi::server::DeriveKeysFromCredentials(username, password,
                                               core.transport_kind_, core.keys_,
                                               key_err)) {
      core.token_.clear();
      core.last_error_ = key_err.empty() ? "key derivation failed" : key_err;
      return false;
    }

    core.channel_ = mi::server::SecureChannel(
        core.keys_, mi::server::SecureChannelRole::kClient);
    core.send_seq_ = 0;
    core.prekey_published_ = false;
    if (core.e2ee_inited_) {
      core.e2ee_.SetLocalUsername(core.username_);
    }
    if (core.history_enabled_ && !core.e2ee_state_dir_.empty()) {
      auto store = std::make_unique<ChatHistoryStore>();
      std::string hist_err;
      if (store->Init(core.e2ee_state_dir_, core.username_, hist_err)) {
        core.history_store_ = std::move(store);
        core.WarmupHistoryOnStartup();
      } else {
        core.history_store_.reset();
      }
    } else {
      core.history_store_.reset();
    }
    core.friend_sync_version_ = 0;
    core.last_error_.clear();
    return true;
  }

  struct RustBuf {
    std::uint8_t* ptr{nullptr};
    std::size_t len{0};
    ~RustBuf() {
      if (ptr && len) {
        if (len > kMaxOpaqueMessageBytes) {
          return;  // avoid passing suspicious length to the Rust allocator
        }
        mi_opaque_free(ptr, len);
      }
    }
  };

  auto RustError = [&](const RustBuf& err, const char* fallback) -> std::string {
    if (err.ptr && err.len && err.len <= kMaxOpaqueMessageBytes) {
      return std::string(reinterpret_cast<const char*>(err.ptr), err.len);
    }
    return fallback ? std::string(fallback) : std::string();
  };

  RustBuf req;
  RustBuf state;
  RustBuf err;
  const int start_rc = mi_opaque_client_login_start(
      reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
      &req.ptr, &req.len, &state.ptr, &state.len, &err.ptr, &err.len);
  if (start_rc != 0 || !req.ptr || req.len == 0 || !state.ptr ||
      state.len == 0) {
    core.last_error_ = RustError(err, "opaque login start failed");
    return false;
  }
  if (req.len > kMaxOpaqueMessageBytes || state.len > kMaxOpaqueMessageBytes) {
    core.last_error_ = "opaque message too large";
    return false;
  }

  const std::vector<std::uint8_t> req_vec(req.ptr, req.ptr + req.len);
  std::vector<std::uint8_t> state_vec(state.ptr, state.ptr + state.len);
  mi::common::ScopedWipe state_wipe(state_vec);

  mi::server::Frame start;
  start.type = mi::server::FrameType::kOpaqueLoginStart;
  if (!mi::server::proto::WriteString(username, start.payload) ||
      !mi::server::proto::WriteBytes(req_vec, start.payload)) {
    core.last_error_ = "opaque login start payload too large";
    return false;
  }

  std::vector<std::uint8_t> resp_vec;
  if (!core.ProcessRaw(mi::server::EncodeFrame(start), resp_vec)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "opaque login start failed";
    }
    return false;
  }

  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kOpaqueLoginStart ||
      resp.payload.empty()) {
    core.last_error_ = "opaque login start response invalid";
    return false;
  }

  std::size_t off = 1;
  if (resp.payload[0] == 0) {
    std::string err_msg;
    mi::server::proto::ReadString(resp.payload, off, err_msg);
    core.last_error_ =
        err_msg.empty() ? "opaque login start failed" : err_msg;
    return false;
  }

  std::string login_id;
  std::vector<std::uint8_t> cred_resp;
  if (!mi::server::proto::ReadString(resp.payload, off, login_id) ||
      !mi::server::proto::ReadBytes(resp.payload, off, cred_resp) ||
      off != resp.payload.size() || login_id.empty() || cred_resp.empty()) {
    core.last_error_ = "opaque login start response invalid";
    return false;
  }
  if (cred_resp.size() > kMaxOpaqueMessageBytes) {
    core.last_error_ = "opaque message too large";
    return false;
  }

  RustBuf finalization;
  RustBuf session_key;
  RustBuf err2;
  const int finish_rc = mi_opaque_client_login_finish(
      reinterpret_cast<const std::uint8_t*>(username.data()), username.size(),
      reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
      state_vec.data(), state_vec.size(), cred_resp.data(), cred_resp.size(),
      &finalization.ptr, &finalization.len, &session_key.ptr, &session_key.len,
      &err2.ptr, &err2.len);
  if (finish_rc != 0 || !finalization.ptr || finalization.len == 0 ||
      !session_key.ptr || session_key.len == 0) {
    const std::string rust_err = RustError(err2, "opaque login finish failed");
    core.last_error_ =
        (rust_err == "client login finish failed") ? "invalid credentials"
                                                   : rust_err;
    return false;
  }
  if (finalization.len > kMaxOpaqueMessageBytes ||
      session_key.len > kMaxOpaqueSessionKeyBytes) {
    core.last_error_ = "opaque message too large";
    return false;
  }
  const std::vector<std::uint8_t> final_vec(finalization.ptr,
                                            finalization.ptr +
                                                finalization.len);
  mi::common::SecureBuffer session_key_buf(session_key.ptr, session_key.len);

  mi::server::Frame finish;
  finish.type = mi::server::FrameType::kOpaqueLoginFinish;
  if (!mi::server::proto::WriteString(login_id, finish.payload) ||
      !mi::server::proto::WriteBytes(final_vec, finish.payload)) {
    core.last_error_ = "opaque login finish payload too large";
    return false;
  }

  resp_vec.clear();
  if (!core.ProcessRaw(mi::server::EncodeFrame(finish), resp_vec)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "opaque login finish failed";
    }
    return false;
  }

  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kOpaqueLoginFinish ||
      resp.payload.empty()) {
    core.last_error_ = "opaque login finish response invalid";
    return false;
  }

  off = 1;
  std::string token_or_error;
  if (!mi::server::proto::ReadString(resp.payload, off, token_or_error) ||
      off != resp.payload.size()) {
    core.last_error_ = "opaque login finish response invalid";
    return false;
  }
  if (resp.payload[0] == 0) {
    core.last_error_ =
        token_or_error.empty() ? "opaque login finish failed" : token_or_error;
    return false;
  }
  core.token_ = std::move(token_or_error);

  std::string key_err;
  if (!mi::server::DeriveKeysFromOpaqueSessionKey(
          session_key_buf.bytes(), username, core.token_, core.transport_kind_,
          core.keys_, key_err)) {
    core.token_.clear();
    core.last_error_ = key_err.empty() ? "key derivation failed" : key_err;
    return false;
  }

  core.channel_ =
      mi::server::SecureChannel(core.keys_,
                                mi::server::SecureChannelRole::kClient);
  core.send_seq_ = 0;
  core.prekey_published_ = false;
  if (core.e2ee_inited_) {
    core.e2ee_.SetLocalUsername(core.username_);
  }
  if (core.history_enabled_ && !core.e2ee_state_dir_.empty()) {
    auto store = std::make_unique<ChatHistoryStore>();
    std::string hist_err;
    if (store->Init(core.e2ee_state_dir_, core.username_, hist_err)) {
      core.history_store_ = std::move(store);
      core.WarmupHistoryOnStartup();
    } else {
      core.history_store_.reset();
    }
  } else {
    core.history_store_.reset();
  }
  core.friend_sync_version_ = 0;
  core.last_error_.clear();
  return true;
}

bool AuthService::Relogin(ClientCore& core) const {
  core.last_error_.clear();
  if (core.username_.empty() || core.password_.empty()) {
    core.last_error_ = "no cached credentials";
    return false;
  }
  return Login(core, core.username_, core.password_);
}

bool AuthService::Logout(ClientCore& core) const {
  core.ResetRemoteStream();
  if (core.token_.empty()) {
    return true;
  }
  std::vector<std::uint8_t> ignore;
  core.ProcessEncrypted(mi::server::FrameType::kLogout, {}, ignore);
  core.token_.clear();
  core.prekey_published_ = false;
  core.e2ee_ = mi::client::e2ee::Engine{};
  core.e2ee_.SetPqcPoolSize(core.pqc_precompute_pool_);
  core.e2ee_inited_ = false;
  core.peer_id_cache_.clear();
  core.group_sender_keys_.clear();
  core.pending_sender_key_dists_.clear();
  core.sender_key_req_last_sent_.clear();
  core.pending_group_cipher_.clear();
  core.group_delivery_map_.clear();
  core.group_delivery_order_.clear();
  core.chat_seen_ids_.clear();
  core.chat_seen_order_.clear();
  core.FlushHistoryOnShutdown();
  core.history_store_.reset();
  core.cover_traffic_last_sent_ms_ = 0;
  core.friend_sync_version_ = 0;
  core.last_error_.clear();
  return true;
}

bool AuthService::LoadKtState(ClientCore& core) const {
  core.kt_tree_size_ = 0;
  core.kt_root_.fill(0);
  if (core.kt_state_path_.empty()) {
    return true;
  }
  std::ifstream f(core.kt_state_path_, std::ios::binary);
  if (!f.is_open()) {
    return true;
  }
  char magic[8];
  f.read(magic, sizeof(magic));
  if (!f.good() || std::memcmp(magic, "MIKTSTH1", 8) != 0) {
    return true;
  }
  std::uint8_t size_buf[8];
  f.read(reinterpret_cast<char*>(size_buf), sizeof(size_buf));
  if (!f.good()) {
    return true;
  }
  std::uint64_t size = 0;
  for (int i = 0; i < 8; ++i) {
    size |= (static_cast<std::uint64_t>(size_buf[i]) << (i * 8));
  }
  std::uint8_t root_buf[32];
  f.read(reinterpret_cast<char*>(root_buf), sizeof(root_buf));
  if (!f.good()) {
    return true;
  }
  core.kt_tree_size_ = size;
  std::memcpy(core.kt_root_.data(), root_buf, sizeof(root_buf));
  return true;
}

bool AuthService::SaveKtState(ClientCore& core) const {
  if (core.kt_state_path_.empty()) {
    return true;
  }
  std::error_code ec;
  const auto dir = core.kt_state_path_.has_parent_path()
                       ? core.kt_state_path_.parent_path()
                       : std::filesystem::path{};
  if (!dir.empty()) {
    pfs::CreateDirectories(dir, ec);
  }
  std::vector<std::uint8_t> out;
  out.reserve(8 + 8 + core.kt_root_.size());
  out.insert(out.end(), "MIKTSTH1", "MIKTSTH1" + 8);
  std::uint8_t size_buf[8];
  for (int i = 0; i < 8; ++i) {
    size_buf[i] =
        static_cast<std::uint8_t>((core.kt_tree_size_ >> (i * 8)) & 0xFF);
  }
  out.insert(out.end(), size_buf, size_buf + sizeof(size_buf));
  out.insert(out.end(), core.kt_root_.begin(), core.kt_root_.end());
  if (!pfs::AtomicWrite(core.kt_state_path_, out.data(), out.size(), ec)) {
    return false;
  }
  return true;
}

bool AuthService::LoadOrCreateDeviceId(ClientCore& core) const {
  if (!core.device_id_.empty()) {
    return true;
  }
  if (core.e2ee_state_dir_.empty()) {
    return true;
  }

  std::error_code ec;
  pfs::CreateDirectories(core.e2ee_state_dir_, ec);

  const auto path = core.e2ee_state_dir_ / "device_id.txt";

  std::vector<std::uint8_t> bytes;
  std::uint64_t size = 0;
  if (pfs::Exists(path, ec)) {
    if (ec) {
      core.last_error_ = "device id path error";
      return false;
    }
    size = pfs::FileSize(path, ec);
    if (ec) {
      core.last_error_ = "device id size stat failed";
      return false;
    }
    if (size > kMaxDeviceIdFileBytes) {
      core.last_error_ = "device id file too large";
      return false;
    }
    std::string perm_err;
    if (!mi::shard::security::CheckPathNotWorldWritable(path, perm_err)) {
      core.last_error_ =
          perm_err.empty() ? "device id permissions insecure" : perm_err;
      return false;
    }
    std::ifstream f(path, std::ios::binary);
    if (f.is_open()) {
      bytes.resize(static_cast<std::size_t>(size));
      if (!bytes.empty()) {
        f.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
        if (!f || f.gcount() != static_cast<std::streamsize>(bytes.size())) {
          core.last_error_ = "device id read failed";
          return false;
        }
      }
    }
  } else if (ec) {
    core.last_error_ = "device id path error";
    return false;
  }

  if (!bytes.empty()) {
    std::vector<std::uint8_t> plain;
    mi::common::ScopedWipe plain_wipe(plain);
    bool was_dpapi = false;
    static constexpr char kMagic[] = "MI_E2EE_DEVICE_ID_DPAPI1";
    static constexpr char kEntropy[] = "MI_E2EE_DEVICE_ID_ENTROPY_V1";
    std::string dpapi_err;
    if (!MaybeUnprotectSecureStore(bytes, kMagic, kEntropy, plain, was_dpapi,
                             dpapi_err)) {
      core.last_error_ =
          dpapi_err.empty() ? "device id unprotect failed" : dpapi_err;
      return false;
    }
    std::string id = Trim(
        std::string(reinterpret_cast<const char*>(plain.data()), plain.size()));
    if (id.size() != 32) {
      core.last_error_ = "device id invalid";
      return false;
    }
    for (char& ch : id) {
      const unsigned char uc = static_cast<unsigned char>(ch);
      if (!(std::isdigit(uc) || (uc >= 'a' && uc <= 'f') ||
            (uc >= 'A' && uc <= 'F'))) {
        core.last_error_ = "device id invalid";
        return false;
      }
      ch = static_cast<char>(std::tolower(uc));
    }
    core.device_id_ = id;

    if (!was_dpapi) {
      std::string perm_err;
      if (!mi::shard::security::CheckPathNotWorldWritable(path, perm_err)) {
        core.last_error_ =
            perm_err.empty() ? "device id permissions insecure" : perm_err;
        return false;
      }
      std::vector<std::uint8_t> canonical(core.device_id_.begin(),
                                          core.device_id_.end());
      mi::common::ScopedWipe canonical_wipe(canonical);
      std::vector<std::uint8_t> wrapped;
      std::string wrap_err;
      if (!ProtectSecureStore(canonical, kMagic, kEntropy, wrapped, wrap_err)) {
        core.last_error_ =
            wrap_err.empty() ? "device id protect failed" : wrap_err;
        return false;
      }
      std::error_code write_ec;
      if (!pfs::AtomicWrite(path, wrapped.data(), wrapped.size(), write_ec)) {
        core.last_error_ = "device id write failed";
        return false;
      }
#ifndef _WIN32
      {
        std::error_code perm_ec;
        std::filesystem::permissions(
            path,
            std::filesystem::perms::owner_read |
                std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace, perm_ec);
      }
#endif
    }
    return true;
  }

  std::array<std::uint8_t, 16> rnd{};
  if (!RandomBytes(rnd.data(), rnd.size())) {
    core.last_error_ = "rng failed";
    return false;
  }
  core.device_id_ = BytesToHexLower(rnd.data(), rnd.size());
  if (core.device_id_.empty()) {
    core.last_error_ = "device id generation failed";
    return false;
  }

  std::string perm_err;
  if (!mi::shard::security::CheckPathNotWorldWritable(path, perm_err)) {
    core.last_error_ =
        perm_err.empty() ? "device id permissions insecure" : perm_err;
    return false;
  }
  std::vector<std::uint8_t> plain(core.device_id_.begin(),
                                  core.device_id_.end());
  mi::common::ScopedWipe plain_wipe(plain);
  static constexpr char kMagic[] = "MI_E2EE_DEVICE_ID_DPAPI1";
  static constexpr char kEntropy[] = "MI_E2EE_DEVICE_ID_ENTROPY_V1";
  std::vector<std::uint8_t> wrapped;
  std::string wrap_err;
  if (!ProtectSecureStore(plain, kMagic, kEntropy, wrapped, wrap_err)) {
    core.last_error_ = wrap_err.empty() ? "device id protect failed" : wrap_err;
    return false;
  }

  std::error_code write_ec;
  if (!pfs::AtomicWrite(path, wrapped.data(), wrapped.size(), write_ec)) {
    core.last_error_ = "device id write failed";
    return false;
  }
#ifndef _WIN32
  {
    std::error_code perm_ec;
    std::filesystem::permissions(
        path,
        std::filesystem::perms::owner_read |
            std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace, perm_ec);
  }
#endif
  return true;
}

}  // namespace mi::client
