#ifndef MI_E2EE_CLIENT_SECURITY_SERVICE_H
#define MI_E2EE_CLIENT_SECURITY_SERVICE_H

#include <cstdint>
#include <filesystem>
#include <string>

#include "client_config.h"

namespace mi::client::security {
struct TrustEntry;
}

namespace mi::client {

class SecurityService {
 public:
  bool LoadTrustFromConfig(const ClientConfig& cfg,
                           const std::filesystem::path& data_dir,
                           const std::string& server_ip,
                           std::uint16_t server_port,
                           bool require_tls,
                           std::string& out_trust_store_path,
                           std::string& out_pinned_fingerprint,
                           bool& out_trust_store_tls_required,
                           std::string& error) const;

  bool StoreTrustEntry(const std::string& trust_store_path,
                       const std::string& server_ip,
                       std::uint16_t server_port,
                       const security::TrustEntry& entry,
                       std::string& error) const;

  std::string DefaultTrustStorePath(
      const std::string& config_path,
      const std::filesystem::path& data_dir) const;

  void StartEndpointHardening() const noexcept;
};

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_SECURITY_SERVICE_H
