#include "client_core.h"
#include "auth_service.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

#include "protocol.h"

namespace mi::client {

bool ClientCore::Register(const std::string& username,
                          const std::string& password) {
  AuthService auth_service;
  return auth_service.Register(*this, username, password);
}

bool ClientCore::Login(const std::string& username,
                       const std::string& password) {
  AuthService auth_service;
  return auth_service.Login(*this, username, password);
}

bool ClientCore::Relogin() {
  AuthService auth_service;
  return auth_service.Relogin(*this);
}

bool ClientCore::Logout() {
  AuthService auth_service;
  return auth_service.Logout(*this);
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
