#include "client_core.h"

#include <string>

#include "constant_time.h"
#include "security_service.h"
#include "trust_store.h"

namespace mi::client {

bool ClientCore::TrustPendingPeer(const std::string& pin) {
  last_error_.clear();
  if (!EnsureE2ee()) {
    return false;
  }
  std::string err;
  if (!e2ee_.TrustPendingPeer(pin, err)) {
    last_error_ = err.empty() ? "trust peer failed" : err;
    return false;
  }
  last_error_.clear();
  return true;
}

bool ClientCore::TrustPendingServer(const std::string& pin) {
  last_error_.clear();
  if (!remote_mode_ || !use_tls_) {
    last_error_ = "tls not enabled";
    return false;
  }
  if (pending_server_fingerprint_.empty() || pending_server_pin_.empty()) {
    last_error_ = "no pending server trust";
    return false;
  }
  const std::string normalized_pin = security::NormalizeCode(pin);
  const std::string expected_pin = security::NormalizeCode(pending_server_pin_);
  if (!mi::common::ConstantTimeEqual(normalized_pin, expected_pin)) {
    last_error_ = "sas mismatch";
    return false;
  }
  SecurityService security_service;
  if (trust_store_path_.empty()) {
    trust_store_path_ =
        security_service.DefaultTrustStorePath(config_path_, {});
  }
  std::string err;
  security::TrustEntry entry;
  entry.fingerprint = pending_server_fingerprint_;
  entry.tls_required = require_tls_;
  const bool ok = security_service.StoreTrustEntry(
      trust_store_path_, server_ip_, server_port_, entry, err);
  if (!ok) {
    last_error_ = err.empty() ? "store trust failed" : err;
    return false;
  }
  pinned_server_fingerprint_ = pending_server_fingerprint_;
  pending_server_fingerprint_.clear();
  pending_server_pin_.clear();
  ResetRemoteStream();
  last_error_.clear();
  return true;
}

}  // namespace mi::client
