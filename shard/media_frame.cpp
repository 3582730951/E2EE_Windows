#include "media_frame.h"

#include <cstring>

namespace mi::media {

namespace {
void WriteLe64(std::uint64_t v, std::vector<std::uint8_t>& out) {
  for (int i = 0; i < 8; ++i) {
    out.push_back(static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF));
  }
}

bool ReadLe64(const std::vector<std::uint8_t>& data, std::size_t& off,
              std::uint64_t& out) {
  if (off + 8 > data.size()) {
    return false;
  }
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i) {
    v |= static_cast<std::uint64_t>(data[off + static_cast<std::size_t>(i)])
         << (i * 8);
  }
  off += 8;
  out = v;
  return true;
}
}  // namespace

bool EncodeMediaFrame(const MediaFrame& frame, std::vector<std::uint8_t>& out) {
  out.clear();
  out.reserve(1 + 1 + 1 + 1 + 8 + frame.call_id.size() + frame.payload.size());
  out.push_back(kMediaFrameVersion);
  out.push_back(static_cast<std::uint8_t>(frame.kind));
  out.push_back(frame.flags);
  out.push_back(0);
  WriteLe64(frame.timestamp_ms, out);
  out.insert(out.end(), frame.call_id.begin(), frame.call_id.end());
  out.insert(out.end(), frame.payload.begin(), frame.payload.end());
  return true;
}

bool DecodeMediaFrame(const std::vector<std::uint8_t>& data, MediaFrame& out) {
  out = MediaFrame{};
  const std::size_t min_size = 1 + 1 + 1 + 1 + 8 + out.call_id.size();
  if (data.size() < min_size) {
    return false;
  }
  std::size_t off = 0;
  const std::uint8_t version = data[off++];
  if (version != kMediaFrameVersion) {
    return false;
  }
  out.kind = static_cast<StreamKind>(data[off++]);
  out.flags = data[off++];
  off++;  // reserved
  if (!ReadLe64(data, off, out.timestamp_ms)) {
    return false;
  }
  if (off + out.call_id.size() > data.size()) {
    return false;
  }
  std::memcpy(out.call_id.data(), data.data() + off, out.call_id.size());
  off += out.call_id.size();
  out.payload.assign(data.begin() + off, data.end());
  return true;
}

}  // namespace mi::media
