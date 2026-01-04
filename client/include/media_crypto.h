#ifndef MI_E2EE_CLIENT_MEDIA_CRYPTO_H
#define MI_E2EE_CLIENT_MEDIA_CRYPTO_H

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../shard/media_frame.h"

namespace mi::client::media {

constexpr std::uint8_t kMediaPacketVersion = 3;

struct MediaPacket {
  std::uint8_t version{kMediaPacketVersion};
  mi::media::StreamKind kind{mi::media::StreamKind::kAudio};
  std::uint32_t key_id{1};
  std::uint32_t seq{0};
  std::array<std::uint8_t, 16> tag{};
  std::vector<std::uint8_t> cipher;
};

struct MediaKeyPair {
  std::array<std::uint8_t, 32> send_ck{};
  std::array<std::uint8_t, 32> recv_ck{};
};

bool EncodeMediaPacket(const MediaPacket& packet, std::vector<std::uint8_t>& out);
bool DecodeMediaPacket(const std::vector<std::uint8_t>& data, MediaPacket& out);
bool PeekMediaPacketHeader(const std::vector<std::uint8_t>& data,
                           mi::media::StreamKind& out_kind,
                           std::uint32_t& out_seq);
bool PeekMediaPacketHeaderWithKeyId(const std::vector<std::uint8_t>& data,
                                    mi::media::StreamKind& out_kind,
                                    std::uint32_t& out_key_id,
                                    std::uint32_t& out_seq);

bool DeriveStreamChainKeys(const std::array<std::uint8_t, 32>& media_root,
                           mi::media::StreamKind kind,
                           bool initiator,
                           MediaKeyPair& out_keys);

class MediaRatchet {
 public:
  MediaRatchet(const std::array<std::uint8_t, 32>& chain_key,
               mi::media::StreamKind kind,
               std::uint32_t start_seq = 0,
               std::uint32_t key_id = 1);

  bool EncryptFrame(const mi::media::MediaFrame& frame,
                    std::vector<std::uint8_t>& out_packet,
                    std::string& error);

  bool DecryptFrame(const std::vector<std::uint8_t>& packet,
                    mi::media::MediaFrame& out_frame,
                    std::string& error);

  std::uint32_t next_seq() const { return next_seq_; }

 private:
  bool DeriveMessageKey(std::uint32_t seq, std::array<std::uint8_t, 32>& out_mk,
                        std::string& error);
  void StoreSkipped(std::uint32_t seq, const std::array<std::uint8_t, 32>& mk);
  bool LoadSkipped(std::uint32_t seq, std::array<std::uint8_t, 32>& out_mk);

  std::array<std::uint8_t, 32> ck_{};
  std::uint32_t next_seq_{0};
  std::uint32_t key_id_{1};
  mi::media::StreamKind kind_{mi::media::StreamKind::kAudio};
  std::unordered_map<std::uint32_t, std::array<std::uint8_t, 32>> skipped_;
  std::deque<std::uint32_t> skipped_order_;
};

}  // namespace mi::client::media

#endif  // MI_E2EE_CLIENT_MEDIA_CRYPTO_H
