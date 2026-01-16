#ifndef MI_E2EE_TRUST_STORE_H
#define MI_E2EE_TRUST_STORE_H

#include <cstdint>
#include <string>

namespace mi::client::security {

struct TrustEntry {
  std::string fingerprint;
  bool tls_required{false};
};

std::string EndpointKey(const std::string& host, std::uint16_t port);
std::string NormalizeFingerprint(std::string v);
std::string NormalizeCode(const std::string& input);
bool IsHex64(const std::string& v);

bool LoadTrustEntry(const std::string& path, const std::string& endpoint,
                    TrustEntry& out_entry);
bool StoreTrustEntry(const std::string& path, const std::string& endpoint,
                     const TrustEntry& entry, std::string& error);

}  // namespace mi::client::security

#endif  // MI_E2EE_TRUST_STORE_H
