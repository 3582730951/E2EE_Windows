#include "connection_handler.h"

#include <algorithm>
#include <cctype>
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <utility>

#include "protocol.h"
#include "secure_channel.h"

namespace mi::server {

ConnectionHandler::ConnectionHandler(ServerApp* app)
    : app_(app) {
  metrics_.started_at = std::chrono::steady_clock::now();
}

namespace {
bool WritePlainLogoutError(const std::string& error,
                           std::vector<std::uint8_t>& out_bytes) {
  Frame out;
  out.type = FrameType::kLogout;
  out.payload.clear();
  out.payload.push_back(0);
  proto::WriteString(error.empty() ? std::string("session invalid") : error,
                     out.payload);
  out_bytes = EncodeFrame(out);
  return true;
}

bool ConstantTimeEqual(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) {
    return false;
  }
  std::uint8_t acc = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    acc |= static_cast<std::uint8_t>(a[i] ^ b[i]);
  }
  return acc == 0;
}

bool IsLoopbackIp(std::string_view ip) {
  if (ip.empty()) {
    return true;
  }
  if (ip == "127.0.0.1" || ip == "::1") {
    return true;
  }
  if (ip.size() >= 4 && ip.rfind("127.", 0) == 0) {
    return true;
  }
  return false;
}

void UpdateMax(std::atomic<std::uint64_t>& current, std::uint64_t value) {
  std::uint64_t prev = current.load(std::memory_order_relaxed);
  while (value > prev &&
         !current.compare_exchange_weak(prev, value, std::memory_order_relaxed)) {
  }
}

bool LooksLikeSessionToken(std::string_view token) {
  if (token.size() != 64) {
    return false;
  }
  for (const unsigned char ch : token) {
    if (std::isxdigit(ch) == 0) {
      return false;
    }
  }
  return true;
}
}  // namespace

bool ConnectionHandler::AllowUnauthByIp(const std::string& remote_ip) {
  if (remote_ip.empty()) {
    return true;
  }
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mutex_);

  if ((++unauth_ops_ & 0xFFu) == 0u) {
    CleanupUnauthStateLocked(now);
  }

  auto& entry = unauth_by_ip_[remote_ip];
  entry.bucket.last_seen = now;
  if (entry.ban_until.time_since_epoch() != std::chrono::steady_clock::duration{} &&
      now < entry.ban_until) {
    return false;
  }

  static constexpr double kCapacity = 12.0;
  static constexpr double kRefillPerSec = 0.5;
  if (entry.bucket.last.time_since_epoch() == std::chrono::steady_clock::duration{}) {
    entry.bucket.tokens = kCapacity;
    entry.bucket.last = now;
  }

  const double dt =
      std::chrono::duration_cast<std::chrono::duration<double>>(now - entry.bucket.last)
          .count();
  if (dt > 0.0) {
    entry.bucket.tokens = std::min(kCapacity, entry.bucket.tokens + dt * kRefillPerSec);
    entry.bucket.last = now;
  }

  if (entry.bucket.tokens < 1.0) {
    return false;
  }
  entry.bucket.tokens -= 1.0;
  return true;
}

void ConnectionHandler::ReportUnauthOutcome(const std::string& remote_ip,
                                            bool success) {
  if (remote_ip.empty() || success) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = unauth_by_ip_.find(remote_ip);
  if (it == unauth_by_ip_.end()) {
    return;
  }
  auto& entry = it->second;
  entry.bucket.last_seen = now;

  static constexpr auto kWindow = std::chrono::minutes(10);
  static constexpr std::uint32_t kThreshold = 20;
  static constexpr auto kBan = std::chrono::minutes(5);

  if (entry.first_failure.time_since_epoch() == std::chrono::steady_clock::duration{} ||
      now - entry.first_failure > kWindow) {
    entry.first_failure = now;
    entry.failures = 1;
    return;
  }
  entry.failures++;
  if (entry.failures >= kThreshold) {
    entry.ban_until = now + kBan;
    entry.failures = 0;
    entry.first_failure = now;
  }
}

void ConnectionHandler::CleanupUnauthStateLocked(
    std::chrono::steady_clock::time_point now) {
  if (unauth_by_ip_.size() < 1024) {
    return;
  }
  static constexpr auto kTtl = std::chrono::minutes(30);
  for (auto it = unauth_by_ip_.begin(); it != unauth_by_ip_.end();) {
    const auto last = it->second.bucket.last_seen;
    if (last.time_since_epoch() != std::chrono::steady_clock::duration{} &&
        now - last > kTtl) {
      it = unauth_by_ip_.erase(it);
      continue;
    }
    ++it;
  }
}

bool ConnectionHandler::IsAuthTokenBanned(const std::string& token) {
  if (token.empty()) {
    return false;
  }
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mutex_);

  if ((++auth_ops_ & 0xFFu) == 0u) {
    CleanupAuthTokenStateLocked(now);
  }

  auto it = auth_by_token_.find(token);
  if (it == auth_by_token_.end()) {
    return false;
  }
  it->second.last_seen = now;
  if (it->second.ban_until.time_since_epoch() !=
          std::chrono::steady_clock::duration{} &&
      now < it->second.ban_until) {
    return true;
  }
  return false;
}

void ConnectionHandler::ReportAuthDecryptFailure(const std::string& token) {
  if (token.empty()) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(mutex_);

  if ((++auth_ops_ & 0xFFu) == 0u) {
    CleanupAuthTokenStateLocked(now);
  }

  auto& entry = auth_by_token_[token];
  entry.last_seen = now;

  static constexpr auto kWindow = std::chrono::minutes(2);
  static constexpr std::uint32_t kThreshold = 12;
  static constexpr auto kBan = std::chrono::minutes(2);

  if (entry.first_failure.time_since_epoch() ==
          std::chrono::steady_clock::duration{} ||
      now - entry.first_failure > kWindow) {
    entry.first_failure = now;
    entry.failures = 1;
    entry.ban_until = std::chrono::steady_clock::time_point{};
    return;
  }

  entry.failures++;
  if (entry.failures >= kThreshold) {
    entry.ban_until = now + kBan;
    entry.failures = 0;
    entry.first_failure = now;
  }
}

void ConnectionHandler::ClearAuthDecryptFailures(const std::string& token) {
  if (token.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  auth_by_token_.erase(token);
}

void ConnectionHandler::CleanupAuthTokenStateLocked(
    std::chrono::steady_clock::time_point now) {
  if (auth_by_token_.size() < 1024) {
    return;
  }
  static constexpr auto kTtl = std::chrono::minutes(30);
  for (auto it = auth_by_token_.begin(); it != auth_by_token_.end();) {
    const auto last = it->second.last_seen;
    if (last.time_since_epoch() != std::chrono::steady_clock::duration{} &&
        now - last > kTtl) {
      it = auth_by_token_.erase(it);
      continue;
    }
    ++it;
  }
}

bool ConnectionHandler::OnData(const std::uint8_t* data, std::size_t len,
                               std::vector<std::uint8_t>& out_bytes,
                               const std::string& remote_ip) {
  if (!app_) {
    return false;
  }
  const auto start = std::chrono::steady_clock::now();
  Frame in;
  if (!DecodeFrame(data, len, in)) {
    metrics_.decode_fail.fetch_add(1, std::memory_order_relaxed);
    return false;
  }
  metrics_.requests_total.fetch_add(1, std::memory_order_relaxed);
  const auto finish = [&](bool success) {
    const auto latency_us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start)
            .count());
    metrics_.total_latency_us.fetch_add(latency_us, std::memory_order_relaxed);
    UpdateMax(metrics_.max_latency_us, latency_us);
    if (success) {
      metrics_.requests_ok.fetch_add(1, std::memory_order_relaxed);
    } else {
      metrics_.requests_fail.fetch_add(1, std::memory_order_relaxed);
    }
  };
  Frame out;
  std::string error;

  if (in.type == FrameType::kLogin ||
      in.type == FrameType::kOpaqueLoginStart ||
      in.type == FrameType::kOpaqueLoginFinish ||
      in.type == FrameType::kOpaqueRegisterStart ||
      in.type == FrameType::kOpaqueRegisterFinish ||
      in.type == FrameType::kHealthCheck) {
    if (!AllowUnauthByIp(remote_ip)) {
      out.type = in.type;
      out.payload.clear();
      out.payload.push_back(0);
      proto::WriteString("rate limited", out.payload);
      out_bytes = EncodeFrame(out);
      metrics_.rate_limited.fetch_add(1, std::memory_order_relaxed);
      finish(false);
      return true;
    }
    if (in.type == FrameType::kHealthCheck) {
      out.type = in.type;
      out.payload.clear();

      std::size_t offset = 0;
      std::string token;
      if (!proto::ReadString(in.payload, offset, token) ||
          offset != in.payload.size()) {
        out.payload.push_back(0);
        proto::WriteString("invalid request", out.payload);
        out_bytes = EncodeFrame(out);
        ReportUnauthOutcome(remote_ip, false);
        finish(false);
        return true;
      }

      const auto& cfg = app_->config().server;
      const bool enabled = cfg.ops_enable;
      const bool allowed_ip = cfg.ops_allow_remote || IsLoopbackIp(remote_ip);
      const std::string expected = cfg.ops_token.get();
      const bool ok_token = !expected.empty() && ConstantTimeEqual(token, expected);

      if (!enabled) {
        out.payload.push_back(0);
        proto::WriteString("unsupported", out.payload);
      } else if (!allowed_ip || !ok_token) {
        out.payload.push_back(0);
        proto::WriteString("unauthorized", out.payload);
      } else {
        out.payload.push_back(1);
        proto::WriteUint32(1, out.payload);  // version

        const auto now = std::chrono::steady_clock::now();
        const auto uptime_sec = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                now - metrics_.started_at)
                .count());
        proto::WriteUint64(uptime_sec, out.payload);

        const auto total =
            metrics_.requests_total.load(std::memory_order_relaxed);
        const auto ok =
            metrics_.requests_ok.load(std::memory_order_relaxed);
        const auto fail =
            metrics_.requests_fail.load(std::memory_order_relaxed);
        const auto decode_fail =
            metrics_.decode_fail.load(std::memory_order_relaxed);
        const auto rate_limited =
            metrics_.rate_limited.load(std::memory_order_relaxed);
        const auto total_latency_us =
            metrics_.total_latency_us.load(std::memory_order_relaxed);
        const auto max_latency_us =
            metrics_.max_latency_us.load(std::memory_order_relaxed);
        const auto avg_latency_us =
            total == 0 ? 0 : (total_latency_us / total);

        proto::WriteUint64(total, out.payload);
        proto::WriteUint64(ok, out.payload);
        proto::WriteUint64(fail, out.payload);
        proto::WriteUint64(decode_fail, out.payload);
        proto::WriteUint64(rate_limited, out.payload);
        proto::WriteUint64(avg_latency_us, out.payload);
        proto::WriteUint64(max_latency_us, out.payload);

        if (auto* sessions = app_->sessions()) {
          const auto stats = sessions->GetStats();
          proto::WriteUint64(stats.sessions, out.payload);
          proto::WriteUint64(stats.pending_opaque, out.payload);
          proto::WriteUint64(stats.login_failure_entries, out.payload);
        } else {
          proto::WriteUint64(0, out.payload);
          proto::WriteUint64(0, out.payload);
          proto::WriteUint64(0, out.payload);
        }

        if (auto* queue = app_->offline_queue()) {
          const auto stats = queue->GetStats();
          proto::WriteUint64(stats.recipients, out.payload);
          proto::WriteUint64(stats.messages, out.payload);
          proto::WriteUint64(stats.bytes, out.payload);
          proto::WriteUint64(stats.generic_messages, out.payload);
          proto::WriteUint64(stats.private_messages, out.payload);
          proto::WriteUint64(stats.group_cipher_messages, out.payload);
          proto::WriteUint64(stats.device_sync_messages, out.payload);
          proto::WriteUint64(stats.group_notice_messages, out.payload);
        } else {
          for (int i = 0; i < 8; ++i) {
            proto::WriteUint64(0, out.payload);
          }
        }

        if (auto* storage = app_->offline_storage()) {
          const auto stats = storage->GetStats();
          proto::WriteUint64(stats.files, out.payload);
          proto::WriteUint64(stats.bytes, out.payload);
        } else {
          proto::WriteUint64(0, out.payload);
          proto::WriteUint64(0, out.payload);
        }
      }

      const bool success = !out.payload.empty() && out.payload[0] != 0;
      out_bytes = EncodeFrame(out);
      finish(success);
      return true;
    }
    if (!app_->HandleFrame(in, out, error)) {
      finish(false);
      return false;
    }
    if (!out.payload.empty()) {
      ReportUnauthOutcome(remote_ip, out.payload[0] != 0);
    }
    out_bytes = EncodeFrame(out);
    finish(out.payload.empty() || out.payload[0] != 0);
    return true;
  }

  // payload = token_len(2) + token(utf8) + cipher
  std::size_t offset = 0;
  std::string token;
  if (!proto::ReadString(in.payload, offset, token)) {
    finish(false);
    return false;
  }
  if (!LooksLikeSessionToken(token)) {
    finish(false);
    return false;
  }

  const std::vector<std::uint8_t> cipher(in.payload.begin() + offset,
                                         in.payload.end());
  if (cipher.empty()) {
    finish(false);
    return false;
  }

  if (IsAuthTokenBanned(token)) {
    metrics_.rate_limited.fetch_add(1, std::memory_order_relaxed);
    finish(false);
    return false;
  }

  std::shared_ptr<ChannelState> state;
  {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    const auto it = channel_states_.find(token);
    if (it != channel_states_.end()) {
      state = it->second;
    }
  }
  if (!state) {
    auto keys = app_->sessions()->GetKeys(token);
    if (!keys.has_value()) {
      const bool ok = WritePlainLogoutError({}, out_bytes);
      finish(false);
      return ok;
    }
    auto new_state = std::make_shared<ChannelState>();
    new_state->channel = SecureChannel(*keys, SecureChannelRole::kServer);
    {
      std::lock_guard<std::mutex> lock(channel_mutex_);
      const auto [it, inserted] = channel_states_.emplace(token, new_state);
      (void)inserted;
      state = it->second;
    }
  }

  std::lock_guard<std::mutex> state_lock(state->mutex);

  std::vector<std::uint8_t> plain;
  if (!state->channel.Decrypt(cipher, in.type, plain)) {
    ReportAuthDecryptFailure(token);
    const bool ok = WritePlainLogoutError({}, out_bytes);
    finish(false);
    return ok;
  }
  ClearAuthDecryptFailures(token);

  Frame inner;
  inner.type = in.type;
  inner.payload = plain;

  if (!app_->HandleFrameWithToken(inner, out, token, error)) {
    finish(false);
    return false;
  }

  std::vector<std::uint8_t> cipher_out;
  if (!state->channel.Encrypt(state->send_seq, out.type, out.payload,
                              cipher_out)) {
    finish(false);
    return false;
  }
  state->send_seq++;

  Frame envelope;
  envelope.type = out.type;
  envelope.payload.reserve(token.size() + 2 + cipher_out.size());
  proto::WriteString(token, envelope.payload);
  envelope.payload.insert(envelope.payload.end(), cipher_out.begin(),
                          cipher_out.end());

  out_bytes = EncodeFrame(envelope);
  if (out.type == FrameType::kLogout) {
    std::lock_guard<std::mutex> lock(channel_mutex_);
    channel_states_.erase(token);
    ClearAuthDecryptFailures(token);
  }
  finish(out.payload.empty() || out.payload[0] != 0);
  return true;
}

}  // namespace mi::server
