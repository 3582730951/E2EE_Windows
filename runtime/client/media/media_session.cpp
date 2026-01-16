#include "media_session.h"

#include "platform_time.h"

namespace mi::client::media {

namespace {
std::uint64_t NowMs() {
  return mi::platform::NowSteadyMs();
}

MediaStreamStats& StatsForKind(MediaSessionStats& stats,
                               mi::media::StreamKind kind) {
  return (kind == mi::media::StreamKind::kVideo) ? stats.video : stats.audio;
}
}  // namespace

MediaSession::MediaSession(MediaTransport& transport,
                           MediaSessionConfig config)
    : transport_(transport),
      config_(std::move(config)),
      audio_jitter_(config_.audio_delay_ms, config_.audio_max_frames),
      video_jitter_(config_.video_delay_ms, config_.video_max_frames) {}

bool MediaSession::Init(std::string& error) {
  error.clear();
  ready_ = false;
  media_root_.fill(0);

  if (config_.peer_username.empty()) {
    error = "peer username empty";
    return false;
  }
  if (!transport_.DeriveMediaRoot(config_.peer_username, config_.call_id,
                                  media_root_, error)) {
    if (error.empty()) {
      error = "media root derive failed";
    }
    return false;
  }

  if (config_.enable_audio) {
    MediaKeyPair audio_keys;
    if (!DeriveStreamChainKeys(media_root_, mi::media::StreamKind::kAudio,
                               config_.initiator, audio_keys)) {
      error = "audio chain key derive failed";
      return false;
    }
    audio_send_ = std::make_unique<MediaRatchet>(
        audio_keys.send_ck, mi::media::StreamKind::kAudio);
    audio_recv_ = std::make_unique<MediaRatchet>(
        audio_keys.recv_ck, mi::media::StreamKind::kAudio);
  }

  if (config_.enable_video) {
    MediaKeyPair video_keys;
    if (!DeriveStreamChainKeys(media_root_, mi::media::StreamKind::kVideo,
                               config_.initiator, video_keys)) {
      error = "video chain key derive failed";
      return false;
    }
    video_send_ = std::make_unique<MediaRatchet>(
        video_keys.send_ck, mi::media::StreamKind::kVideo);
    video_recv_ = std::make_unique<MediaRatchet>(
        video_keys.recv_ck, mi::media::StreamKind::kVideo);
  }

  ready_ = true;
  return true;
}

bool MediaSession::SendFrame(mi::media::StreamKind kind,
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
  if (!transport_.PushMedia(config_.peer_username, config_.call_id, *packet,
                            out_error)) {
    return false;
  }
  StatsForKind(stats_, kind).frames_sent++;
  return true;
}

bool MediaSession::SendAudioFrame(const std::vector<std::uint8_t>& payload,
                                  std::uint64_t timestamp_ms,
                                  std::uint8_t flags) {
  return SendFrame(mi::media::StreamKind::kAudio, payload, timestamp_ms, flags);
}

bool MediaSession::SendVideoFrame(const std::vector<std::uint8_t>& payload,
                                  std::uint64_t timestamp_ms,
                                  std::uint8_t flags) {
  return SendFrame(mi::media::StreamKind::kVideo, payload, timestamp_ms, flags);
}

bool MediaSession::HandleIncomingPacket(const std::string& sender,
                                        const std::vector<std::uint8_t>& packet,
                                        std::string& error) {
  if (!ready_) {
    error = "media session not ready";
    return false;
  }
  if (!config_.peer_username.empty() && sender != config_.peer_username) {
    return false;
  }
  mi::media::StreamKind kind = mi::media::StreamKind::kAudio;
  std::uint32_t seq = 0;
  if (!PeekMediaPacketHeader(packet, kind, seq)) {
    error = "media packet header invalid";
    return false;
  }

  MediaRatchet* ratchet = nullptr;
  MediaJitterBuffer* jitter = nullptr;
  if (kind == mi::media::StreamKind::kAudio) {
    ratchet = audio_recv_.get();
    jitter = &audio_jitter_;
  } else if (kind == mi::media::StreamKind::kVideo) {
    ratchet = video_recv_.get();
    jitter = &video_jitter_;
  }
  if (!ratchet || !jitter) {
    StatsForKind(stats_, kind).frames_drop++;
    return false;
  }

  mi::media::MediaFrame frame;
  std::string err;
  if (!ratchet->DecryptFrame(packet, frame, err)) {
    StatsForKind(stats_, kind).decrypt_fail++;
    if (error.empty()) {
      error = err.empty() ? "media decrypt failed" : err;
    }
    return false;
  }
  if (frame.call_id != config_.call_id) {
    StatsForKind(stats_, kind).frames_drop++;
    return false;
  }
  jitter->Push(frame, NowMs());
  StatsForKind(stats_, kind).frames_recv++;
  return true;
}

bool MediaSession::PollIncoming(std::uint32_t max_packets,
                                std::uint32_t wait_ms,
                                std::string& error) {
  error.clear();
  if (!ready_) {
    error = "media session not ready";
    return false;
  }
  pull_packets_.clear();
  if (max_packets > pull_packets_.capacity()) {
    pull_packets_.reserve(max_packets);
  }
  if (!transport_.PullMedia(config_.call_id, max_packets, wait_ms,
                            pull_packets_, error)) {
    if (error.empty()) {
      error = "media pull failed";
    }
    return false;
  }
  for (const auto& entry : pull_packets_) {
    std::string pkt_err;
    if (!HandleIncomingPacket(entry.sender, entry.payload, pkt_err) &&
        error.empty()) {
      error = pkt_err;
    }
  }
  return true;
}

bool MediaSession::PopAudioFrame(std::uint64_t now_ms,
                                 mi::media::MediaFrame& out) {
  return audio_jitter_.PopReady(now_ms, out);
}

bool MediaSession::PopVideoFrame(std::uint64_t now_ms,
                                 mi::media::MediaFrame& out) {
  return video_jitter_.PopReady(now_ms, out);
}

}  // namespace mi::client::media
