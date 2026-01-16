#include "group_call_session.h"

#include "platform_time.h"

namespace mi::client::media {

namespace {
std::uint64_t NowMs() {
  return mi::platform::NowSteadyMs();
}

bool BuildRatchetsForKey(const std::array<std::uint8_t, 32>& call_key,
                         std::uint32_t key_id,
                         mi::media::StreamKind kind,
                         std::unique_ptr<MediaRatchet>& out_send,
                         std::unique_ptr<MediaRatchet>& out_recv) {
  MediaKeyPair keys;
  if (!DeriveStreamChainKeys(call_key, kind, true, keys)) {
    return false;
  }
  out_send = std::make_unique<MediaRatchet>(keys.send_ck, kind, 0, key_id);
  out_recv = std::make_unique<MediaRatchet>(keys.send_ck, kind, 0, key_id);
  return true;
}
}  // namespace

GroupCallSession::GroupCallSession(MediaTransport& transport,
                                   GroupCallSessionConfig config)
    : transport_(transport), config_(std::move(config)) {}

bool GroupCallSession::Init(std::string& error) {
  error.clear();
  ready_ = false;
  if (config_.group_id.empty()) {
    error = "group id empty";
    return false;
  }
  if (config_.call_id == std::array<std::uint8_t, 16>{}) {
    error = "call id empty";
    return false;
  }
  if (!SetActiveKey(config_.key_id, error)) {
    return false;
  }
  ready_ = true;
  return true;
}

bool GroupCallSession::SetActiveKey(std::uint32_t key_id, std::string& error) {
  error.clear();
  if (key_id == 0) {
    error = "key id invalid";
    return false;
  }
  std::array<std::uint8_t, 32> call_key{};
  if (!transport_.GetGroupCallKey(config_.group_id, config_.call_id, key_id,
                                  call_key, error)) {
    if (error.empty()) {
      error = "call key missing";
    }
    return false;
  }

  if (config_.enable_audio) {
    std::unique_ptr<MediaRatchet> dummy;
    if (!BuildRatchetsForKey(call_key, key_id, mi::media::StreamKind::kAudio,
                             audio_send_, dummy)) {
      error = "audio key derive failed";
      return false;
    }
  }
  if (config_.enable_video) {
    std::unique_ptr<MediaRatchet> dummy;
    if (!BuildRatchetsForKey(call_key, key_id, mi::media::StreamKind::kVideo,
                             video_send_, dummy)) {
      error = "video key derive failed";
      return false;
    }
  }
  active_key_id_ = key_id;
  return true;
}

bool GroupCallSession::SendFrame(mi::media::StreamKind kind,
                                 const std::vector<std::uint8_t>& payload,
                                 std::uint64_t timestamp_ms,
                                 std::uint8_t flags) {
  if (!ready_) {
    return false;
  }
  if (payload.empty()) {
    return false;
  }
  MediaRatchet* ratchet = nullptr;
  std::vector<std::uint8_t>* packet = nullptr;
  if (kind == mi::media::StreamKind::kAudio) {
    ratchet = audio_send_.get();
    packet = &audio_packet_buf_;
  } else if (kind == mi::media::StreamKind::kVideo) {
    ratchet = video_send_.get();
    packet = &video_packet_buf_;
  }
  if (!ratchet || !packet) {
    return false;
  }

  mi::media::MediaFrame frame;
  frame.call_id = config_.call_id;
  frame.kind = kind;
  frame.flags = flags;
  frame.timestamp_ms = timestamp_ms;
  frame.payload = payload;

  std::string err;
  if (!ratchet->EncryptFrame(frame, *packet, err)) {
    return false;
  }
  std::string out_error;
  if (!transport_.PushGroupMedia(config_.group_id, config_.call_id, *packet,
                                 out_error)) {
    return false;
  }
  return true;
}

bool GroupCallSession::SendAudioFrame(const std::vector<std::uint8_t>& payload,
                                      std::uint64_t timestamp_ms,
                                      std::uint8_t flags) {
  return SendFrame(mi::media::StreamKind::kAudio, payload, timestamp_ms, flags);
}

bool GroupCallSession::SendVideoFrame(const std::vector<std::uint8_t>& payload,
                                      std::uint64_t timestamp_ms,
                                      std::uint8_t flags) {
  return SendFrame(mi::media::StreamKind::kVideo, payload, timestamp_ms, flags);
}

GroupCallSession::SenderState* GroupCallSession::EnsureSenderState(
    const std::string& sender, std::uint32_t key_id, std::string& error) {
  error.clear();
  auto it = senders_.find(sender);
  if (it != senders_.end()) {
    if (it->second.key_id == key_id) {
      return &it->second;
    }
    senders_.erase(it);
  }

  std::array<std::uint8_t, 32> call_key{};
  if (!transport_.GetGroupCallKey(config_.group_id, config_.call_id, key_id,
                                  call_key, error)) {
    if (error.empty()) {
      error = "call key missing";
    }
    return nullptr;
  }
  SenderState state;
  state.key_id = key_id;
  state.audio_jitter =
      MediaJitterBuffer(config_.audio_delay_ms, config_.audio_max_frames);
  state.video_jitter =
      MediaJitterBuffer(config_.video_delay_ms, config_.video_max_frames);
  if (config_.enable_audio) {
    std::unique_ptr<MediaRatchet> send;
    if (!BuildRatchetsForKey(call_key, key_id, mi::media::StreamKind::kAudio,
                             send, state.audio_recv)) {
      error = "audio key derive failed";
      return nullptr;
    }
  }
  if (config_.enable_video) {
    std::unique_ptr<MediaRatchet> send;
    if (!BuildRatchetsForKey(call_key, key_id, mi::media::StreamKind::kVideo,
                             send, state.video_recv)) {
      error = "video key derive failed";
      return nullptr;
    }
  }
  auto [inserted_it, inserted] = senders_.emplace(sender, std::move(state));
  (void)inserted;
  return &inserted_it->second;
}

bool GroupCallSession::HandleIncomingPacket(
    const std::string& sender, const std::vector<std::uint8_t>& packet,
    std::string& error) {
  error.clear();
  if (!ready_) {
    error = "group call not ready";
    return false;
  }
  mi::media::StreamKind kind = mi::media::StreamKind::kAudio;
  std::uint32_t key_id = 0;
  std::uint32_t seq = 0;
  if (!PeekMediaPacketHeaderWithKeyId(packet, kind, key_id, seq)) {
    error = "media packet header invalid";
    return false;
  }
  SenderState* state = EnsureSenderState(sender, key_id, error);
  if (!state) {
    return false;
  }
  MediaRatchet* ratchet = nullptr;
  MediaJitterBuffer* jitter = nullptr;
  if (kind == mi::media::StreamKind::kAudio) {
    ratchet = state->audio_recv.get();
    jitter = &state->audio_jitter;
  } else if (kind == mi::media::StreamKind::kVideo) {
    ratchet = state->video_recv.get();
    jitter = &state->video_jitter;
  }
  if (!ratchet || !jitter) {
    return false;
  }
  mi::media::MediaFrame frame;
  std::string err;
  if (!ratchet->DecryptFrame(packet, frame, err)) {
    error = err.empty() ? "media decrypt failed" : err;
    return false;
  }
  if (frame.call_id != config_.call_id) {
    return false;
  }
  jitter->Push(frame, NowMs());
  return true;
}

bool GroupCallSession::PollIncoming(std::uint32_t max_packets,
                                    std::uint32_t wait_ms,
                                    std::string& error) {
  error.clear();
  if (!ready_) {
    error = "group call not ready";
    return false;
  }
  pull_packets_.clear();
  if (max_packets > pull_packets_.capacity()) {
    pull_packets_.reserve(max_packets);
  }
  if (!transport_.PullGroupMedia(config_.call_id, max_packets, wait_ms,
                                 pull_packets_, error)) {
    if (error.empty()) {
      error = "group media pull failed";
    }
    return false;
  }
  for (const auto& entry : pull_packets_) {
    std::string pkt_err;
    HandleIncomingPacket(entry.sender, entry.payload, pkt_err);
    if (error.empty() && !pkt_err.empty()) {
      error = pkt_err;
    }
  }
  return true;
}

bool GroupCallSession::PopAudioFrame(std::uint64_t now_ms,
                                     GroupMediaFrame& out) {
  for (auto& kv : senders_) {
    mi::media::MediaFrame frame;
    if (kv.second.audio_jitter.PopReady(now_ms, frame)) {
      out.sender = kv.first;
      out.frame = std::move(frame);
      return true;
    }
  }
  return false;
}

bool GroupCallSession::PopVideoFrame(std::uint64_t now_ms,
                                     GroupMediaFrame& out) {
  for (auto& kv : senders_) {
    mi::media::MediaFrame frame;
    if (kv.second.video_jitter.PopReady(now_ms, frame)) {
      out.sender = kv.first;
      out.frame = std::move(frame);
      return true;
    }
  }
  return false;
}

}  // namespace mi::client::media
