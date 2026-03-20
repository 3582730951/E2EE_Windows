#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "../common/constant_time.h"
#include "../common/hex_utils.h"
#include "../platform/include/platform_net.h"
#include "../platform/include/platform_tls.h"
#include "../server/include/frame.h"
#include "../server/include/protocol.h"

namespace {

constexpr std::uint32_t kSocketTimeoutMs = 5000;

struct ConnectionState {
  mi::platform::net::Socket sock{mi::platform::net::kInvalidSocket};
  mi::platform::tls::ClientContext tls_ctx;
  std::vector<std::uint8_t> enc_buf;
  std::vector<std::uint8_t> plain_buf;
  std::size_t plain_off{0};

  ~ConnectionState() {
    mi::platform::tls::Close(tls_ctx);
    mi::platform::net::CloseSocket(sock);
  }
};

struct Options {
  std::string host{"127.0.0.1"};
  std::uint16_t port{9000};
  std::string token;
  std::size_t width{48};
  bool use_tls{false};
  bool tls_verify_peer{true};
  bool tls_verify_hostname{true};
  std::string tls_ca_bundle;
  std::string tls_pinned_fingerprint;
  bool help{false};
};

void PrintUsage() {
  std::cout << "usage: mi_e2ee_ops_health_view --token <ops_token> "
               "[--host 127.0.0.1] [--port 9000] [--width 48] [--tls]\n";
  std::cout << "       [--ca-bundle <path>] [--tls-pinned-fingerprint <sha256_hex>]\n";
  std::cout << "       [--tls-no-verify-hostname] [--tls-no-verify-peer]\n";
  std::cout << "default path is plaintext loopback; enable --tls for verified TLS.\n";
  std::cout << "--tls-no-verify-peer is only allowed together with "
               "--tls-pinned-fingerprint.\n";
}

bool ParseArgs(int argc, char** argv, Options& out) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      out.help = true;
      return true;
    }
    if (arg == "--host" && i + 1 < argc) {
      out.host = argv[++i];
      continue;
    }
    if (arg == "--port" && i + 1 < argc) {
      const int v = std::stoi(argv[++i]);
      if (v <= 0 || v > 65535) {
        return false;
      }
      out.port = static_cast<std::uint16_t>(v);
      continue;
    }
    if (arg == "--token" && i + 1 < argc) {
      out.token = argv[++i];
      continue;
    }
    if (arg == "--tls") {
      out.use_tls = true;
      continue;
    }
    if (arg == "--ca-bundle" && i + 1 < argc) {
      out.tls_ca_bundle = argv[++i];
      continue;
    }
    if (arg == "--tls-pinned-fingerprint" && i + 1 < argc) {
      out.tls_pinned_fingerprint = argv[++i];
      continue;
    }
    if (arg == "--tls-no-verify-hostname") {
      out.tls_verify_hostname = false;
      continue;
    }
    if (arg == "--tls-no-verify-peer") {
      out.tls_verify_peer = false;
      continue;
    }
    if (arg == "--width" && i + 1 < argc) {
      const int v = std::stoi(argv[++i]);
      if (v < 8) {
        return false;
      }
      out.width = static_cast<std::size_t>(v);
      continue;
    }
    return false;
  }
  return true;
}

std::string NormalizeSha256Hex(std::string_view value) {
  std::string normalized;
  normalized.reserve(value.size());
  for (char ch : value) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (std::isxdigit(uch) != 0) {
      normalized.push_back(
          static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
      continue;
    }
    if (ch == ':' || ch == '-' || std::isspace(uch) != 0) {
      continue;
    }
    return {};
  }
  return normalized;
}

bool ValidateOptions(Options& options, std::string& error) {
  error.clear();
  const bool has_tls_overrides =
      !options.tls_ca_bundle.empty() ||
      !options.tls_pinned_fingerprint.empty() ||
      !options.tls_verify_peer || !options.tls_verify_hostname;
  if (!options.use_tls && has_tls_overrides) {
    error = "tls options require --tls";
    return false;
  }
  if (!options.use_tls) {
    return true;
  }
  if (!options.tls_pinned_fingerprint.empty()) {
    options.tls_pinned_fingerprint =
        NormalizeSha256Hex(options.tls_pinned_fingerprint);
    std::vector<std::uint8_t> fp_bytes;
    if (options.tls_pinned_fingerprint.size() != 64 ||
        !mi::common::HexToBytes(options.tls_pinned_fingerprint, fp_bytes) ||
        fp_bytes.size() != 32) {
      error = "tls pinned fingerprint must be a SHA-256 hex digest";
      return false;
    }
  }
  if (!options.tls_verify_peer && options.tls_pinned_fingerprint.empty()) {
    error =
        "--tls-no-verify-peer requires --tls-pinned-fingerprint for fail-close pinning";
    return false;
  }
  return true;
}

bool ReadFramePlain(mi::platform::net::Socket sock,
                    std::vector<std::uint8_t>& out_frame,
                    std::string& error) {
  error.clear();
  std::vector<std::uint8_t> header(mi::server::kFrameHeaderSize);
  if (!mi::platform::net::RecvExact(sock, header.data(), header.size())) {
    error = "recv header failed";
    return false;
  }
  mi::server::FrameType type{};
  std::uint32_t payload_len = 0;
  if (!mi::server::DecodeFrameHeader(header.data(), header.size(), type,
                                     payload_len)) {
    error = "invalid frame header";
    return false;
  }
  if (payload_len > mi::server::kMaxFramePayloadBytes) {
    error = "payload too large";
    return false;
  }
  out_frame = std::move(header);
  if (payload_len == 0) {
    return true;
  }
  const std::size_t old_size = out_frame.size();
  out_frame.resize(old_size + payload_len);
  if (!mi::platform::net::RecvExact(sock, out_frame.data() + old_size,
                                    payload_len)) {
    error = "recv payload failed";
    out_frame.clear();
    return false;
  }
  return true;
}

bool ReadFrameTlsBuffered(mi::platform::net::Socket sock,
                          mi::platform::tls::ClientContext& tls_ctx,
                          std::vector<std::uint8_t>& enc_buf,
                          std::vector<std::uint8_t>& plain_buf,
                          std::size_t& plain_off,
                          std::vector<std::uint8_t>& out_frame,
                          std::string& error) {
  error.clear();
  out_frame.clear();
  if (plain_off > plain_buf.size()) {
    plain_buf.clear();
    plain_off = 0;
  }

  while (true) {
    const std::size_t avail =
        plain_buf.size() >= plain_off ? (plain_buf.size() - plain_off) : 0;
    if (avail >= mi::server::kFrameHeaderSize) {
      mi::server::FrameType type{};
      std::uint32_t payload_len = 0;
      if (!mi::server::DecodeFrameHeader(plain_buf.data() + plain_off, avail,
                                         type, payload_len)) {
        error = "invalid frame header";
        return false;
      }
      const std::size_t total =
          mi::server::kFrameHeaderSize + static_cast<std::size_t>(payload_len);
      if (payload_len > mi::server::kMaxFramePayloadBytes) {
        error = "payload too large";
        return false;
      }
      if (avail >= total) {
        out_frame.assign(
            plain_buf.begin() + static_cast<std::ptrdiff_t>(plain_off),
            plain_buf.begin() + static_cast<std::ptrdiff_t>(plain_off + total));
        plain_off += total;
        if (plain_off >= plain_buf.size()) {
          plain_buf.clear();
          plain_off = 0;
        }
        return true;
      }
    }

    std::vector<std::uint8_t> plain_chunk;
    if (!mi::platform::tls::DecryptToPlain(sock, tls_ctx, enc_buf,
                                           plain_chunk)) {
      error = "tls receive failed";
      return false;
    }
    if (!plain_chunk.empty()) {
      plain_buf.insert(plain_buf.end(), plain_chunk.begin(), plain_chunk.end());
    }
  }
}

bool Connect(ConnectionState& state, const Options& options,
             std::string& error) {
  error.clear();
  if (!mi::platform::net::EnsureInitialized()) {
    error = "socket init failed";
    return false;
  }
  if (!mi::platform::net::ConnectTcp(options.host, options.port, state.sock,
                                     error)) {
    return false;
  }
  mi::platform::net::SetRecvTimeout(state.sock, kSocketTimeoutMs);
  mi::platform::net::SetSendTimeout(state.sock, kSocketTimeoutMs);
  if (!options.use_tls) {
    return true;
  }
  if (mi::platform::tls::IsStubbed()) {
    error = "tls stub build";
    return false;
  }
  if (!mi::platform::tls::IsSupported()) {
    error = "tls unsupported";
    return false;
  }

  mi::platform::tls::ClientVerifyConfig verify{};
  verify.verify_peer = options.tls_verify_peer;
  verify.verify_hostname = options.tls_verify_hostname;
  verify.ca_bundle_path = options.tls_ca_bundle;

  std::vector<std::uint8_t> cert_der;
  if (!mi::platform::tls::ClientHandshake(state.sock, options.host, verify,
                                          state.tls_ctx, cert_der, state.enc_buf,
                                          error)) {
    return false;
  }
  if (!options.tls_pinned_fingerprint.empty()) {
    const std::string actual_fp =
        mi::common::Sha256Hex(cert_der.data(), cert_der.size());
    if (actual_fp.empty()) {
      error = "server fingerprint failed";
      return false;
    }
    if (!mi::common::ConstantTimeEqual(options.tls_pinned_fingerprint,
                                       actual_fp)) {
      error = "server fingerprint changed";
      return false;
    }
  }
  return true;
}

struct PerfSample {
  std::uint64_t ts_sec{0};
  std::uint64_t cpu_x100{0};
  std::uint64_t rss_bytes{0};
};

struct HealthReport {
  std::uint32_t version{0};
  std::uint64_t uptime_sec{0};
  std::uint64_t total{0};
  std::uint64_t ok{0};
  std::uint64_t fail{0};
  std::uint64_t decode_fail{0};
  std::uint64_t rate_limited{0};
  std::uint64_t avg_latency_us{0};
  std::uint64_t max_latency_us{0};
  std::uint64_t p50{0};
  std::uint64_t p95{0};
  std::uint64_t p99{0};
  std::uint64_t cpu_x100{0};
  std::uint64_t rss_bytes{0};
  std::uint64_t sessions{0};
  std::uint64_t pending_opaque{0};
  std::uint64_t login_failures{0};
  std::uint64_t queue_recipients{0};
  std::uint64_t queue_messages{0};
  std::uint64_t queue_bytes{0};
  std::uint64_t queue_generic{0};
  std::uint64_t queue_private{0};
  std::uint64_t queue_group_cipher{0};
  std::uint64_t queue_device_sync{0};
  std::uint64_t queue_group_notice{0};
  std::uint64_t storage_files{0};
  std::uint64_t storage_bytes{0};
  std::vector<PerfSample> samples;
};

bool ReadU64(const std::vector<std::uint8_t>& payload, std::size_t& offset,
             std::uint64_t& out) {
  return mi::server::proto::ReadUint64(payload, offset, out);
}

bool ParseHealthPayload(const std::vector<std::uint8_t>& payload,
                        HealthReport& out, std::string& error) {
  error.clear();
  if (payload.empty()) {
    error = "empty payload";
    return false;
  }
  std::size_t offset = 0;
  const std::uint8_t status = payload[offset++];
  if (status == 0) {
    std::string err;
    if (mi::server::proto::ReadString(payload, offset, err)) {
      error = err;
    } else {
      error = "request failed";
    }
    return false;
  }
  if (!mi::server::proto::ReadUint32(payload, offset, out.version)) {
    error = "invalid version";
    return false;
  }
  if (!ReadU64(payload, offset, out.uptime_sec) ||
      !ReadU64(payload, offset, out.total) ||
      !ReadU64(payload, offset, out.ok) ||
      !ReadU64(payload, offset, out.fail) ||
      !ReadU64(payload, offset, out.decode_fail) ||
      !ReadU64(payload, offset, out.rate_limited) ||
      !ReadU64(payload, offset, out.avg_latency_us) ||
      !ReadU64(payload, offset, out.max_latency_us) ||
      !ReadU64(payload, offset, out.p50) ||
      !ReadU64(payload, offset, out.p95) ||
      !ReadU64(payload, offset, out.p99) ||
      !ReadU64(payload, offset, out.cpu_x100) ||
      !ReadU64(payload, offset, out.rss_bytes) ||
      !ReadU64(payload, offset, out.sessions) ||
      !ReadU64(payload, offset, out.pending_opaque) ||
      !ReadU64(payload, offset, out.login_failures) ||
      !ReadU64(payload, offset, out.queue_recipients) ||
      !ReadU64(payload, offset, out.queue_messages) ||
      !ReadU64(payload, offset, out.queue_bytes) ||
      !ReadU64(payload, offset, out.queue_generic) ||
      !ReadU64(payload, offset, out.queue_private) ||
      !ReadU64(payload, offset, out.queue_group_cipher) ||
      !ReadU64(payload, offset, out.queue_device_sync) ||
      !ReadU64(payload, offset, out.queue_group_notice) ||
      !ReadU64(payload, offset, out.storage_files) ||
      !ReadU64(payload, offset, out.storage_bytes)) {
    error = "payload truncated";
    return false;
  }
  std::uint32_t sample_count = 0;
  if (!mi::server::proto::ReadUint32(payload, offset, sample_count)) {
    error = "missing samples";
    return false;
  }
  out.samples.clear();
  out.samples.reserve(sample_count);
  for (std::uint32_t i = 0; i < sample_count; ++i) {
    PerfSample sample{};
    if (!ReadU64(payload, offset, sample.ts_sec) ||
        !ReadU64(payload, offset, sample.cpu_x100) ||
        !ReadU64(payload, offset, sample.rss_bytes)) {
      error = "sample truncated";
      return false;
    }
    out.samples.push_back(sample);
  }
  return true;
}

std::string FormatBytes(std::uint64_t bytes) {
  double value = static_cast<double>(bytes);
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  int unit = 0;
  while (value >= 1024.0 && unit < 4) {
    value /= 1024.0;
    ++unit;
  }
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(value < 10.0 ? 2 : 1) << value << " "
      << units[unit];
  return oss.str();
}

std::vector<double> Resample(const std::vector<double>& values,
                             std::size_t width) {
  if (values.empty()) {
    return {};
  }
  if (width == 0) {
    return values;
  }
  if (values.size() <= width) {
    return values;
  }
  std::vector<double> out;
  out.reserve(width);
  for (std::size_t i = 0; i < width; ++i) {
    const std::size_t start = (i * values.size()) / width;
    const std::size_t end = ((i + 1) * values.size()) / width;
    const std::size_t stop = std::max(start + 1, end);
    double sum = 0.0;
    std::size_t count = 0;
    for (std::size_t j = start; j < stop && j < values.size(); ++j) {
      sum += values[j];
      ++count;
    }
    out.push_back(count == 0 ? 0.0 : (sum / static_cast<double>(count)));
  }
  return out;
}

std::string RenderSparkline(const std::vector<double>& values,
                            std::size_t width, double& out_min,
                            double& out_max) {
  static const char kBars[] = " .:-=+*#%@";
  static constexpr std::size_t kBarCount = sizeof(kBars) - 1;
  if (values.empty()) {
    out_min = out_max = 0.0;
    return "(no samples)";
  }
  const auto series = Resample(values, width);
  out_min = *std::min_element(series.begin(), series.end());
  out_max = *std::max_element(series.begin(), series.end());
  double span = out_max - out_min;
  if (span <= 0.000001) {
    span = 1.0;
  }
  std::string out;
  out.reserve(series.size());
  for (double v : series) {
    const double t = (v - out_min) / span;
    const std::size_t idx = static_cast<std::size_t>(
        std::min<double>(kBarCount - 1, std::max<double>(0.0, t * (kBarCount - 1))));
    out.push_back(kBars[idx]);
  }
  return out;
}

void PrintReport(const HealthReport& report, std::size_t width) {
  const double cpu_pct = static_cast<double>(report.cpu_x100) / 100.0;
  std::cout << "version: " << report.version << "\n";
  std::cout << "uptime: " << report.uptime_sec << "s\n";
  std::cout << "requests: total " << report.total << ", ok " << report.ok
            << ", fail " << report.fail << ", decode_fail "
            << report.decode_fail << ", rate_limited " << report.rate_limited
            << "\n";
  std::cout << "latency_us: avg " << report.avg_latency_us << ", p50 "
            << report.p50 << ", p95 " << report.p95 << ", p99 " << report.p99
            << ", max " << report.max_latency_us << "\n";
  std::cout << "cpu: " << std::fixed << std::setprecision(2) << cpu_pct
            << "%, rss: " << FormatBytes(report.rss_bytes) << "\n";
  std::cout << "sessions: " << report.sessions << ", pending_opaque "
            << report.pending_opaque << ", login_failures "
            << report.login_failures << "\n";
  std::cout << "queue: recipients " << report.queue_recipients << ", messages "
            << report.queue_messages << ", bytes "
            << FormatBytes(report.queue_bytes) << "\n";
  std::cout << "queue: generic " << report.queue_generic << ", private "
            << report.queue_private << ", group_cipher "
            << report.queue_group_cipher << ", device_sync "
            << report.queue_device_sync << ", group_notice "
            << report.queue_group_notice << "\n";
  std::cout << "storage: files " << report.storage_files << ", bytes "
            << FormatBytes(report.storage_bytes) << "\n";

  if (report.samples.empty()) {
    std::cout << "perf: no samples\n";
    return;
  }

  std::vector<double> cpu_series;
  std::vector<double> rss_series;
  cpu_series.reserve(report.samples.size());
  rss_series.reserve(report.samples.size());
  std::uint64_t ts_min = report.samples.front().ts_sec;
  std::uint64_t ts_max = report.samples.front().ts_sec;
  for (const auto& s : report.samples) {
    cpu_series.push_back(static_cast<double>(s.cpu_x100) / 100.0);
    rss_series.push_back(static_cast<double>(s.rss_bytes));
    ts_min = std::min(ts_min, s.ts_sec);
    ts_max = std::max(ts_max, s.ts_sec);
  }

  auto avg = [](const std::vector<double>& values) -> double {
    if (values.empty()) {
      return 0.0;
    }
    double sum = 0.0;
    for (double v : values) {
      sum += v;
    }
    return sum / static_cast<double>(values.size());
  };

  double cpu_min = 0.0, cpu_max = 0.0;
  const std::string cpu_line = RenderSparkline(cpu_series, width, cpu_min, cpu_max);
  double rss_min = 0.0, rss_max = 0.0;
  const std::string rss_line = RenderSparkline(rss_series, width, rss_min, rss_max);
  const double cpu_avg = avg(cpu_series);
  const double rss_avg = avg(rss_series);

  std::cout << "perf: samples " << report.samples.size() << ", span "
            << (ts_max >= ts_min ? (ts_max - ts_min) : 0) << "s\n";
  std::cout << "cpu%: min " << std::fixed << std::setprecision(2) << cpu_min
            << ", avg " << cpu_avg << ", max " << cpu_max << "\n";
  std::cout << "cpu curve: " << cpu_line << "\n";
  std::cout << "rss: min " << FormatBytes(static_cast<std::uint64_t>(rss_min))
            << ", avg " << FormatBytes(static_cast<std::uint64_t>(rss_avg))
            << ", max " << FormatBytes(static_cast<std::uint64_t>(rss_max))
            << "\n";
  std::cout << "rss curve: " << rss_line << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  std::string err;
  if (!ParseArgs(argc, argv, options) || options.help) {
    PrintUsage();
    return options.help ? 0 : 1;
  }
  if (options.token.empty()) {
    PrintUsage();
    return 1;
  }
  if (!ValidateOptions(options, err)) {
    std::cerr << err << "\n";
    return 1;
  }

  ConnectionState state;
  if (!Connect(state, options, err)) {
    std::cerr << "connect failed: " << err << "\n";
    return 1;
  }

  mi::server::Frame req;
  req.type = mi::server::FrameType::kHealthCheck;
  mi::server::proto::WriteString(options.token, req.payload);
  std::vector<std::uint8_t> frame = mi::server::EncodeFrame(req);
  if (options.use_tls) {
    if (!mi::platform::tls::EncryptAndSend(state.sock, state.tls_ctx, frame)) {
      std::cerr << "send failed\n";
      return 1;
    }
  } else if (!mi::platform::net::SendAll(state.sock, frame.data(), frame.size())) {
    std::cerr << "send failed\n";
    return 1;
  }

  std::vector<std::uint8_t> full;
  if (options.use_tls) {
    if (!ReadFrameTlsBuffered(state.sock, state.tls_ctx, state.enc_buf,
                              state.plain_buf, state.plain_off, full, err)) {
      std::cerr << err << "\n";
      return 1;
    }
  } else if (!ReadFramePlain(state.sock, full, err)) {
    std::cerr << err << "\n";
    return 1;
  }

  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(full.data(), full.size(), resp)) {
    std::cerr << "decode response failed\n";
    return 1;
  }
  if (resp.type != mi::server::FrameType::kHealthCheck) {
    std::cerr << "unexpected response type\n";
    return 1;
  }
  HealthReport report;
  if (!ParseHealthPayload(resp.payload, report, err)) {
    std::cerr << "health check failed: " << err << "\n";
    return 1;
  }
  PrintReport(report, options.width);
  return 0;
}
