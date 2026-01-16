#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#include "frame.h"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                      std::size_t size) {
  if (!data || size == 0) {
    return 0;
  }
  if (size > (mi::server::kFrameHeaderSize + mi::server::kMaxFramePayloadBytes)) {
    return 0;
  }
  mi::server::Frame frame;
  (void)mi::server::DecodeFrame(data, size, frame);
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
  std::error_code ec;
  const auto size = std::filesystem::file_size(argv[1], ec);
  if (ec || size == 0) {
    return 0;
  }
  if (size > static_cast<std::uintmax_t>(
                 (std::numeric_limits<std::size_t>::max)())) {
    return 0;
  }
  std::vector<std::uint8_t> data;
  data.resize(static_cast<std::size_t>(size));
  ifs.read(reinterpret_cast<char*>(data.data()),
           static_cast<std::streamsize>(data.size()));
  if (!ifs || ifs.gcount() != static_cast<std::streamsize>(data.size())) {
    return 0;
  }
  if (data.empty()) {
    return 0;
  }
  (void)LLVMFuzzerTestOneInput(data.data(), data.size());
  return 0;
}
#endif
