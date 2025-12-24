#include "media_crypto.h"

#include <cstring>

#include "../server/include/crypto.h"
#include "monocypher.h"

namespace mi::client::media {

namespace {
constexpr std::uint32_t kMaxMediaSkip = 2048;
constexpr std::size_t kMaxMediaSkippedKeys = 512;

void WriteLe32(std::uint32_t v, std::uint8_t out[4]) {
  out[0] = static_cast<std::uint8_t>(v & 0xFF);
  out[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
  out[2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
  out[3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
}

bool ReadLe32(const std::vector<std::uint8_t>& data, std::size_t& off,
              std::uint32_t& out) {
  if (off + 4 > data.size()) {
    return false;
  }
  out = static_cast<std::uint32_t>(data[off]) |
        (static_cast<std::uint32_t>(data[off + 1]) << 8) |
        (static_cast<std::uint32_t>(data[off + 2]) << 16) |
        (static_cast<std::uint32_t>(data[off + 3]) << 24);
  off += 4;
  return true;
}

void BuildNonce(std::uint32_t seq, std::uint8_t out[24]) {
  std::uint8_t tmp[4];
  WriteLe32(seq, tmp);
  std::memset(out, 0, 24);
  std::memcpy(out, tmp, sizeof(tmp));
}

bool KdfMediaCk(const std::array<std::uint8_t, 32>& ck,
                std::array<std::uint8_t, 32>& out_ck,
                std::array<std::uint8_t, 32>& out_mk) {
  std::array<std::uint8_t, 64> buf{};
  static constexpr char kInfo[] = "mi_e2ee_media_ck_v1";
  if (!mi::server::crypto::HkdfSha256(
          ck.data(), ck.size(), nullptr, 0,
          reinterpret_cast<const std::uint8_t*>(kInfo), std::strlen(kInfo),
          buf.data(), buf.size())) {
    return false;
  }
  std::memcpy(out_ck.data(), buf.data(), 32);
  std::memcpy(out_mk.data(), buf.data() + 32, 32);
  return true;
}
}  // namespace

bool EncodeMediaPacket(const MediaPacket& packet,
                       std::vector<std::uint8_t>& out) {
  out.clear();
  out.reserve(1 + 1 + 4 + packet.tag.size() + packet.cipher.size());
  out.push_back(kMediaPacketVersion);
  out.push_back(static_cast<std::uint8_t>(packet.kind));
  std::uint8_t seq_bytes[4];
  WriteLe32(packet.seq, seq_bytes);
  out.insert(out.end(), seq_bytes, seq_bytes + sizeof(seq_bytes));
  out.insert(out.end(), packet.tag.begin(), packet.tag.end());
  out.insert(out.end(), packet.cipher.begin(), packet.cipher.end());
  return true;
}

bool DecodeMediaPacket(const std::vector<std::uint8_t>& data, MediaPacket& out) {
  out = MediaPacket{};
  const std::size_t min_size = 1 + 1 + 4 + out.tag.size();
  if (data.size() < min_size) {
    return false;
  }
  std::size_t off = 0;
  const std::uint8_t version = data[off++];
  if (version != kMediaPacketVersion) {
    return false;
  }
  out.kind = static_cast<mi::media::StreamKind>(data[off++]);
  if (!ReadLe32(data, off, out.seq)) {
    return false;
  }
  if (off + out.tag.size() > data.size()) {
    return false;
  }
  std::memcpy(out.tag.data(), data.data() + off, out.tag.size());
  off += out.tag.size();
  out.cipher.assign(data.begin() + off, data.end());
  return true;
}

bool PeekMediaPacketHeader(const std::vector<std::uint8_t>& data,
                           mi::media::StreamKind& out_kind,
                           std::uint32_t& out_seq) {
  const std::size_t min_size = 1 + 1 + 4;
  if (data.size() < min_size) {
    return false;
  }
  std::size_t off = 0;
  const std::uint8_t version = data[off++];
  if (version != kMediaPacketVersion) {
    return false;
  }
  out_kind = static_cast<mi::media::StreamKind>(data[off++]);
  return ReadLe32(data, off, out_seq);
}

bool DeriveStreamChainKeys(const std::array<std::uint8_t, 32>& media_root,
                           mi::media::StreamKind kind,
                           bool initiator,
                           MediaKeyPair& out_keys) {
  out_keys = MediaKeyPair{};
  std::array<std::uint8_t, 64> buf{};
  const char* label = (kind == mi::media::StreamKind::kVideo)
                          ? "mi_e2ee_media_video_v1"
                          : "mi_e2ee_media_audio_v1";
  if (!mi::server::crypto::HkdfSha256(
          media_root.data(), media_root.size(), nullptr, 0,
          reinterpret_cast<const std::uint8_t*>(label), std::strlen(label),
          buf.data(), buf.size())) {
    return false;
  }
  if (initiator) {
    std::memcpy(out_keys.send_ck.data(), buf.data(), 32);
    std::memcpy(out_keys.recv_ck.data(), buf.data() + 32, 32);
  } else {
    std::memcpy(out_keys.recv_ck.data(), buf.data(), 32);
    std::memcpy(out_keys.send_ck.data(), buf.data() + 32, 32);
  }
  return true;
}

MediaRatchet::MediaRatchet(const std::array<std::uint8_t, 32>& chain_key,
                           mi::media::StreamKind kind,
                           std::uint32_t start_seq)
    : ck_(chain_key), next_seq_(start_seq), kind_(kind) {}

bool MediaRatchet::EncryptFrame(const mi::media::MediaFrame& frame,
                                std::vector<std::uint8_t>& out_packet,
                                std::string& error) {
  error.clear();
  if (frame.kind != kind_) {
    error = "media kind mismatch";
    return false;
  }
  std::vector<std::uint8_t> plain;
  if (!mi::media::EncodeMediaFrame(frame, plain)) {
    error = "media frame encode failed";
    return false;
  }

  std::array<std::uint8_t, 32> next_ck{};
  std::array<std::uint8_t, 32> mk{};
  if (!KdfMediaCk(ck_, next_ck, mk)) {
    error = "media kdf failed";
    return false;
  }

  MediaPacket packet;
  packet.kind = kind_;
  packet.seq = next_seq_;
  packet.cipher.resize(plain.size());

  std::uint8_t nonce[24];
  BuildNonce(packet.seq, nonce);
  std::uint8_t ad[1 + 1 + 4];
  ad[0] = kMediaPacketVersion;
  ad[1] = static_cast<std::uint8_t>(kind_);
  WriteLe32(packet.seq, ad + 2);

  crypto_aead_lock(packet.cipher.data(), packet.tag.data(), mk.data(), nonce,
                   ad, sizeof(ad), plain.data(), plain.size());

  ck_ = next_ck;
  next_seq_++;
  return EncodeMediaPacket(packet, out_packet);
}

bool MediaRatchet::DecryptFrame(const std::vector<std::uint8_t>& packet,
                                mi::media::MediaFrame& out_frame,
                                std::string& error) {
  error.clear();
  MediaPacket parsed;
  if (!DecodeMediaPacket(packet, parsed)) {
    error = "media packet decode failed";
    return false;
  }
  if (parsed.kind != kind_) {
    error = "media kind mismatch";
    return false;
  }

  std::array<std::uint8_t, 32> mk{};
  if (!DeriveMessageKey(parsed.seq, mk, error)) {
    return false;
  }

  std::uint8_t nonce[24];
  BuildNonce(parsed.seq, nonce);
  std::uint8_t ad[1 + 1 + 4];
  ad[0] = kMediaPacketVersion;
  ad[1] = static_cast<std::uint8_t>(kind_);
  WriteLe32(parsed.seq, ad + 2);

  std::vector<std::uint8_t> plain;
  plain.resize(parsed.cipher.size());
  const int ok = crypto_aead_unlock(
      plain.data(), parsed.tag.data(), mk.data(), nonce,
      ad, sizeof(ad), parsed.cipher.data(), parsed.cipher.size());
  if (ok != 0) {
    error = "media decrypt failed";
    return false;
  }
  if (!mi::media::DecodeMediaFrame(plain, out_frame)) {
    error = "media frame decode failed";
    return false;
  }
  return true;
}

bool MediaRatchet::DeriveMessageKey(std::uint32_t seq,
                                    std::array<std::uint8_t, 32>& out_mk,
                                    std::string& error) {
  error.clear();
  if (seq < next_seq_) {
    if (!LoadSkipped(seq, out_mk)) {
      error = "media message expired";
      return false;
    }
    return true;
  }
  if (seq - next_seq_ > kMaxMediaSkip) {
    error = "media gap too large";
    return false;
  }
  while (next_seq_ < seq) {
    std::array<std::uint8_t, 32> next_ck{};
    std::array<std::uint8_t, 32> mk{};
    if (!KdfMediaCk(ck_, next_ck, mk)) {
      error = "media kdf failed";
      return false;
    }
    StoreSkipped(next_seq_, mk);
    ck_ = next_ck;
    next_seq_++;
  }

  std::array<std::uint8_t, 32> next_ck{};
  if (!KdfMediaCk(ck_, next_ck, out_mk)) {
    error = "media kdf failed";
    return false;
  }
  ck_ = next_ck;
  next_seq_ = seq + 1;
  return true;
}

void MediaRatchet::StoreSkipped(std::uint32_t seq,
                                const std::array<std::uint8_t, 32>& mk) {
  if (skipped_.emplace(seq, mk).second) {
    skipped_order_.push_back(seq);
  }
  while (skipped_.size() > kMaxMediaSkippedKeys) {
    if (skipped_order_.empty()) {
      skipped_.clear();
      return;
    }
    const auto drop = skipped_order_.front();
    skipped_order_.pop_front();
    skipped_.erase(drop);
  }
}

bool MediaRatchet::LoadSkipped(std::uint32_t seq,
                               std::array<std::uint8_t, 32>& out_mk) {
  auto it = skipped_.find(seq);
  if (it == skipped_.end()) {
    return false;
  }
  out_mk = it->second;
  skipped_.erase(it);
  return true;
}

}  // namespace mi::client::media
