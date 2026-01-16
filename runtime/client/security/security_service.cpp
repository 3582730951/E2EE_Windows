#include "security_service.h"

#include <filesystem>

#include "endpoint_hardening.h"
#include "trust_store.h"

namespace mi::client {

bool SecurityService::LoadTrustFromConfig(
    const ClientConfig& cfg,
    const std::filesystem::path& data_dir,
    const std::string& server_ip,
    std::uint16_t server_port,
    bool require_tls,
    std::string& out_trust_store_path,
    std::string& out_pinned_fingerprint,
    bool& out_trust_store_tls_required,
    std::string& error) const {
  out_trust_store_path.clear();
  out_pinned_fingerprint.clear();
  out_trust_store_tls_required = false;
  error.clear();

  if (!cfg.trust_store.empty()) {
    std::filesystem::path trust = cfg.trust_store;
    if (!trust.is_absolute()) {
      trust = data_dir / trust;
    }
    out_trust_store_path = trust.string();
    security::TrustEntry entry;
    if (security::LoadTrustEntry(
            out_trust_store_path,
            security::EndpointKey(server_ip, server_port), entry)) {
      out_pinned_fingerprint = entry.fingerprint;
      out_trust_store_tls_required = entry.tls_required;
    }
  }

  if (!cfg.pinned_fingerprint.empty()) {
    const std::string pin =
        security::NormalizeFingerprint(cfg.pinned_fingerprint);
    if (!security::IsHex64(pin)) {
      error = "pinned_fingerprint invalid";
      return false;
    }
    out_pinned_fingerprint = pin;
    if (!out_trust_store_path.empty()) {
      security::TrustEntry entry{pin, require_tls};
      std::string store_err;
      if (!StoreTrustEntry(out_trust_store_path, server_ip, server_port, entry,
                           store_err)) {
        error = store_err.empty() ? "store trust failed" : store_err;
        return false;
      }
      out_trust_store_tls_required = entry.tls_required;
    }
  }

  return true;
}

bool SecurityService::StoreTrustEntry(const std::string& trust_store_path,
                                      const std::string& server_ip,
                                      std::uint16_t server_port,
                                      const security::TrustEntry& entry,
                                      std::string& error) const {
  return security::StoreTrustEntry(
      trust_store_path, security::EndpointKey(server_ip, server_port), entry,
      error);
}

std::string SecurityService::DefaultTrustStorePath(
    const std::string& config_path,
    const std::filesystem::path& data_dir) const {
  std::filesystem::path trust = "server_trust.ini";
  if (!config_path.empty()) {
    std::filesystem::path base = data_dir;
    if (base.empty()) {
      const auto cfg_dir = ResolveConfigDir(config_path);
      base = ResolveDataDir(cfg_dir);
    }
    if (!base.empty()) {
      trust = base / trust;
    }
  }
  return trust.string();
}

void SecurityService::StartEndpointHardening() const noexcept {
  security::StartEndpointHardening();
}

}  // namespace mi::client
