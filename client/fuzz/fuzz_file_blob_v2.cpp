#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "file_blob.h"
#include "monocypher.h"

namespace {

constexpr std::uint8_t kFileBlobMagic[4] = {'M', 'I', 'F', '1'};
constexpr std::uint8_t kFileBlobVersionV2 = 2;
constexpr std::uint8_t kFileBlobAlgoDeflate = 1;
constexpr std::uint8_t kFileBlobFlagDoubleCompression = 0x01;

constexpr std::size_t kV2PrefixSize =
    sizeof(kFileBlobMagic) + 1 + 1 + 1 + 1 + 8 + 8 + 8;
constexpr std::size_t kV2HeaderSize = kV2PrefixSize + 24 + 16;

void StoreLe64(std::uint64_t v, std::uint8_t out[8]) {
  for (int i = 0; i < 8; ++i) {
    out[i] = static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF);
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
  if (!data || size == 0) {
    return 0;
  }
  if (size > (1u << 20)) {
    return 0;
  }

  std::array<std::uint8_t, 32> key{};
  for (std::size_t i = 0; i < key.size(); ++i) {
    key[i] = static_cast<std::uint8_t>(data[i % size] ^ (i * 31u));
  }

  std::array<std::uint8_t, 24> nonce{};
  for (std::size_t i = 0; i < nonce.size(); ++i) {
    nonce[i] = static_cast<std::uint8_t>(data[(i + 7) % size] + i);
  }

  const std::uint64_t stage2_size = static_cast<std::uint64_t>(size);
  const std::uint64_t stage1_size =
      1u + static_cast<std::uint64_t>(((data[0] << 8) | data[size - 1]) & 0x3FFF);
  const std::uint64_t original_size =
      1u + static_cast<std::uint64_t>(((data[size / 2] << 8) | data[0]) & 0x3FFF);

  std::vector<std::uint8_t> blob;
  blob.resize(kV2HeaderSize + static_cast<std::size_t>(stage2_size));
  std::uint8_t* p = blob.data();
  std::memcpy(p, kFileBlobMagic, sizeof(kFileBlobMagic));
  p += sizeof(kFileBlobMagic);
  *p++ = kFileBlobVersionV2;
  *p++ = kFileBlobFlagDoubleCompression;
  *p++ = kFileBlobAlgoDeflate;
  *p++ = 0;
  StoreLe64(original_size, p);
  p += 8;
  StoreLe64(stage1_size, p);
  p += 8;
  StoreLe64(stage2_size, p);
  p += 8;

  std::memcpy(blob.data() + kV2PrefixSize, nonce.data(), nonce.size());
  std::uint8_t* mac = blob.data() + kV2PrefixSize + 24;
  std::uint8_t* cipher = blob.data() + kV2HeaderSize;
  crypto_aead_lock(cipher, mac, key.data(), nonce.data(), blob.data(),
                   kV2PrefixSize, data, size);

  std::vector<std::uint8_t> out;
  (void)mi::client::DecryptFileBlobForTooling(blob, key, out);
  return 0;
}

#if defined(MI_E2EE_FUZZ_STANDALONE)
int main(int argc, char** argv) {
  if (argc < 2 || !argv[1]) {
    return 0;
  }
  std::ifstream ifs(argv[1], std::ios::binary);
  if (!ifs) {
    return 0;
  }
  std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(ifs)),
                                 std::istreambuf_iterator<char>());
  if (data.empty()) {
    return 0;
  }
  (void)LLVMFuzzerTestOneInput(data.data(), data.size());
  return 0;
}
#endif
