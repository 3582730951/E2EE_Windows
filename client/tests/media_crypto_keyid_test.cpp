#include <cassert>
#include <cstdint>
#include <vector>

#include "media_crypto.h"

namespace {

std::array<std::uint8_t, 16> MakeCallId() {
  std::array<std::uint8_t, 16> id{};
  for (std::size_t i = 0; i < id.size(); ++i) {
    id[i] = static_cast<std::uint8_t>(0xA0 + i);
  }
  return id;
}

}  // namespace

int main() {
  using mi::client::media::DecodeMediaPacket;
  using mi::client::media::DeriveStreamChainKeys;
  using mi::client::media::EncodeMediaPacket;
  using mi::client::media::MediaKeyPair;
  using mi::client::media::MediaPacket;
  using mi::client::media::MediaRatchet;
  using mi::client::media::PeekMediaPacketHeaderWithKeyId;
  using mi::media::MediaFrame;
  using mi::media::StreamKind;

  MediaFrame frame;
  frame.call_id = MakeCallId();
  frame.kind = StreamKind::kAudio;
  frame.timestamp_ms = 1234;
  frame.payload = {1, 2, 3};

  std::array<std::uint8_t, 32> media_root{};
  media_root.fill(0x11);
  MediaKeyPair keys;
  assert(DeriveStreamChainKeys(media_root, StreamKind::kAudio, true, keys));

  MediaRatchet sender(keys.send_ck, StreamKind::kAudio, 0, 7);
  MediaRatchet receiver(keys.recv_ck, StreamKind::kAudio, 0, 7);
  std::vector<std::uint8_t> packet;
  std::string err;
  assert(sender.EncryptFrame(frame, packet, err));

  StreamKind kind = StreamKind::kVideo;
  std::uint32_t key_id = 0;
  std::uint32_t seq = 0;
  assert(PeekMediaPacketHeaderWithKeyId(packet, kind, key_id, seq));
  assert(kind == StreamKind::kAudio);
  assert(key_id == 7);

  MediaFrame out;
  assert(receiver.DecryptFrame(packet, out, err));
  assert(out.payload == frame.payload);

  MediaPacket legacy;
  legacy.version = 2;
  legacy.kind = StreamKind::kAudio;
  legacy.seq = 5;
  legacy.cipher = {9, 9, 9};
  std::vector<std::uint8_t> legacy_bytes;
  assert(EncodeMediaPacket(legacy, legacy_bytes));

  MediaPacket decoded;
  assert(DecodeMediaPacket(legacy_bytes, decoded));
  assert(decoded.version == 2);
  assert(decoded.key_id == 1);
  assert(decoded.seq == 5);

  assert(PeekMediaPacketHeaderWithKeyId(legacy_bytes, kind, key_id, seq));
  assert(key_id == 1);
  assert(seq == 5);

  return 0;
}
