#ifndef MI_E2EE_CLIENT_CONFIG_CRYPTO_H
#define MI_E2EE_CLIENT_CONFIG_CRYPTO_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mi::client {

bool IsEncryptedClientConfig(const std::vector<std::uint8_t>& data);

bool EncryptClientConfigText(std::string_view plaintext,
                             std::vector<std::uint8_t>& out,
                             std::string& error);

bool DecryptClientConfigBlob(const std::vector<std::uint8_t>& blob,
                             std::string& plaintext,
                             std::string& error);

bool WriteEncryptedClientConfigText(const std::filesystem::path& path,
                                    std::string_view plaintext,
                                    std::string& error);

class EncryptedConfigField {
 public:
  EncryptedConfigField() = default;
  ~EncryptedConfigField();

  EncryptedConfigField(const EncryptedConfigField&) = delete;
  EncryptedConfigField& operator=(const EncryptedConfigField&) = delete;
  EncryptedConfigField(EncryptedConfigField&& other) noexcept;
  EncryptedConfigField& operator=(EncryptedConfigField&& other) noexcept;

  bool SetPlain(std::string_view value, std::string& error);
  bool Reveal(std::string& out, std::string& error) const;
  bool empty() const { return blob_.empty(); }
  void Clear();

 private:
  std::vector<std::uint8_t> blob_;
};

int RunClientConfigTool(int argc, char** argv);

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_CONFIG_CRYPTO_H
