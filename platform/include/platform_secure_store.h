#ifndef MI_E2EE_PLATFORM_SECURE_STORE_H
#define MI_E2EE_PLATFORM_SECURE_STORE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mi::platform {

enum class SecureStoreScope : std::uint8_t { kUser = 0, kMachine = 1 };

bool SecureStoreSupported();

bool ProtectSecureBlobScoped(const std::vector<std::uint8_t>& plain,
                             const std::uint8_t* entropy,
                             std::size_t entropy_len,
                             SecureStoreScope scope,
                             std::vector<std::uint8_t>& out,
                             std::string& error);

bool UnprotectSecureBlobScoped(const std::vector<std::uint8_t>& blob,
                               const std::uint8_t* entropy,
                               std::size_t entropy_len,
                               SecureStoreScope scope,
                               std::vector<std::uint8_t>& out,
                               std::string& error);

bool ProtectSecureBlob(const std::vector<std::uint8_t>& plain,
                       const std::uint8_t* entropy,
                       std::size_t entropy_len,
                       std::vector<std::uint8_t>& out,
                       std::string& error);

bool UnprotectSecureBlob(const std::vector<std::uint8_t>& blob,
                         const std::uint8_t* entropy,
                         std::size_t entropy_len,
                         std::vector<std::uint8_t>& out,
                         std::string& error);

}  // namespace mi::platform

#endif  // MI_E2EE_PLATFORM_SECURE_STORE_H
