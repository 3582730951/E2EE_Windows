#ifndef MI_E2EE_CLIENT_FILE_BLOB_H
#define MI_E2EE_CLIENT_FILE_BLOB_H

#include <array>
#include <cstdint>
#include <vector>

namespace mi::client {

bool DecryptFileBlobForTooling(const std::vector<std::uint8_t>& blob,
                               const std::array<std::uint8_t, 32>& key,
                               std::vector<std::uint8_t>& out_plaintext);

}  // namespace mi::client

#endif  // MI_E2EE_CLIENT_FILE_BLOB_H

