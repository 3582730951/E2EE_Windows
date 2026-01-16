#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "media_crypto.h"
#include "media_jitter_buffer.h"

namespace {

std::array<std::uint8_t, 16> MakeCallId() {
  std::array<std::uint8_t, 16> id{};
  for (std::size_t i = 0; i < id.size(); ++i) {
    id[i] = static_cast<std::uint8_t>(i);
  }
  return id;
}

}  // namespace

int main() {
  using mi::client::media::DeriveStreamChainKeys;
  using mi::client::media::MediaJitterBuffer;
  using mi::client::media::MediaKeyPair;
  using mi::client::media::MediaRatchet;
  using mi::client::media::PeekMediaPacketHeader;
  using mi::media::MediaFrame;
  using mi::media::StreamKind;

  // MediaFrame encode/decode roundtrip.
  MediaFrame frame;
  frame.call_id = MakeCallId();
  frame.kind = StreamKind::kAudio;
  frame.flags = 0x01;
  frame.timestamp_ms = 1234;
  frame.payload = {'t', 'e', 's', 't'};

  std::vector<std::uint8_t> encoded;
  assert(mi::media::EncodeMediaFrame(frame, encoded));
  MediaFrame decoded;
  assert(mi::media::DecodeMediaFrame(encoded, decoded));
  assert(decoded.call_id == frame.call_id);
  assert(decoded.kind == frame.kind);
  assert(decoded.flags == frame.flags);
  assert(decoded.timestamp_ms == frame.timestamp_ms);
  assert(decoded.payload == frame.payload);

  // Media ratchet encrypt/decrypt.
  std::array<std::uint8_t, 32> media_root{};
  media_root.fill(0x11);
  MediaKeyPair audio_keys;
  MediaKeyPair audio_keys_remote;
  assert(DeriveStreamChainKeys(media_root, StreamKind::kAudio, true, audio_keys));
  assert(
      DeriveStreamChainKeys(media_root, StreamKind::kAudio, false, audio_keys_remote));

  MediaRatchet sender(audio_keys.send_ck, StreamKind::kAudio);
  MediaRatchet receiver(audio_keys_remote.recv_ck, StreamKind::kAudio);

  std::vector<std::uint8_t> packet;
  std::string err;
  assert(sender.EncryptFrame(frame, packet, err));
  assert(err.empty());

  StreamKind kind = StreamKind::kVideo;
  std::uint32_t seq = 0;
  assert(PeekMediaPacketHeader(packet, kind, seq));
  assert(kind == StreamKind::kAudio);
  assert(seq == 0);

  MediaFrame out;
  assert(receiver.DecryptFrame(packet, out, err));
  assert(err.empty());
  assert(out.call_id == frame.call_id);
  assert(out.kind == frame.kind);
  assert(out.payload == frame.payload);

  // Wrong kind should fail fast.
  MediaKeyPair video_keys;
  assert(DeriveStreamChainKeys(media_root, StreamKind::kVideo, true, video_keys));
  MediaRatchet wrong(video_keys.recv_ck, StreamKind::kVideo);
  assert(!wrong.DecryptFrame(packet, out, err));
  assert(!err.empty());

  // Jitter buffer ordering and delay.
  MediaJitterBuffer jitter(50, 4);
  MediaFrame f1 = frame;
  f1.timestamp_ms = 100;
  MediaFrame f2 = frame;
  f2.timestamp_ms = 120;

  jitter.Push(f1, 1000);
  MediaFrame popped;
  assert(!jitter.PopReady(1049, popped));
  assert(jitter.PopReady(1050, popped));
  assert(popped.timestamp_ms == 100);

  jitter.Push(f2, 1010);
  assert(!jitter.PopReady(1069, popped));
  assert(jitter.PopReady(1070, popped));
  assert(popped.timestamp_ms == 120);

  return 0;
}
