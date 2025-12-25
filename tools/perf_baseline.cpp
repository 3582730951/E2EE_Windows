#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "../server/include/frame.h"
#include "../server/include/offline_storage.h"

namespace {

struct BenchConfig {
  bool quick{false};
  std::size_t frame_payload{1024};
  std::size_t offline_bytes{8u * 1024u * 1024u};
  std::uint32_t frame_iters{60000};
  std::uint32_t decode_iters{60000};
};

struct Metric {
  std::string name;
  double value{0.0};
  std::string unit;
};

double ElapsedSeconds(std::chrono::steady_clock::time_point start,
                      std::chrono::steady_clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::duration<double>>(end - start)
      .count();
}

void PrintMetric(const Metric& metric) {
  std::cout << metric.name << ": " << metric.value;
  if (!metric.unit.empty()) {
    std::cout << " " << metric.unit;
  }
  std::cout << "\n";
}

bool BenchFrameEncode(const BenchConfig& cfg, Metric& ops, Metric& mbps) {
  mi::server::Frame frame;
  frame.type = mi::server::FrameType::kMessage;
  frame.payload.assign(cfg.frame_payload, 0xAB);

  std::vector<std::uint8_t> out;
  std::uint64_t bytes = 0;
  std::uint64_t checksum = 0;

  const auto start = std::chrono::steady_clock::now();
  for (std::uint32_t i = 0; i < cfg.frame_iters; ++i) {
    frame.payload[0] = static_cast<std::uint8_t>(i & 0xFFu);
    mi::server::EncodeFrame(frame, out);
    bytes += out.size();
    if (!out.empty()) {
      checksum += out[0];
    }
  }
  const auto end = std::chrono::steady_clock::now();
  const double seconds = ElapsedSeconds(start, end);
  if (seconds <= 0.0) {
    return false;
  }
  ops = {"frame_encode_ops", static_cast<double>(cfg.frame_iters) / seconds,
         "ops/s"};
  mbps = {"frame_encode_mbps", (bytes / (1024.0 * 1024.0)) / seconds, "MB/s"};
  if (checksum == 0 && cfg.frame_iters > 0) {
    return false;
  }
  return true;
}

bool BenchFrameDecode(const BenchConfig& cfg, Metric& ops, Metric& mbps) {
  mi::server::Frame frame;
  frame.type = mi::server::FrameType::kMessage;
  frame.payload.assign(cfg.frame_payload, 0xCD);

  std::vector<std::uint8_t> encoded;
  mi::server::EncodeFrame(frame, encoded);
  if (encoded.empty()) {
    return false;
  }

  std::uint64_t bytes = 0;
  std::uint64_t checksum = 0;
  mi::server::FrameView view{};

  const auto start = std::chrono::steady_clock::now();
  for (std::uint32_t i = 0; i < cfg.decode_iters; ++i) {
    if (!mi::server::DecodeFrameView(encoded.data(), encoded.size(), view)) {
      return false;
    }
    bytes += encoded.size();
    checksum += view.payload_len;
  }
  const auto end = std::chrono::steady_clock::now();
  const double seconds = ElapsedSeconds(start, end);
  if (seconds <= 0.0) {
    return false;
  }
  ops = {"frame_decode_ops", static_cast<double>(cfg.decode_iters) / seconds,
         "ops/s"};
  mbps = {"frame_decode_mbps", (bytes / (1024.0 * 1024.0)) / seconds, "MB/s"};
  if (checksum == 0 && cfg.decode_iters > 0) {
    return false;
  }
  return true;
}

bool BenchOfflineStorage(const BenchConfig& cfg,
                         Metric& put_mbps,
                         Metric& fetch_mbps,
                         std::string& error) {
  error.clear();
  const auto base =
      std::filesystem::temp_directory_path() / "mi_e2ee_perf_offline";
  std::error_code ec;
  std::filesystem::remove_all(base, ec);
  std::filesystem::create_directories(base, ec);
  if (ec) {
    error = "offline temp dir failed";
    return false;
  }

  mi::server::OfflineStorage storage(base, std::chrono::seconds(60));
  std::vector<std::uint8_t> data(cfg.offline_bytes, 0x5A);

  const auto start_put = std::chrono::steady_clock::now();
  auto put = storage.Put("bench", data);
  const auto end_put = std::chrono::steady_clock::now();
  if (!put.success) {
    error = put.error.empty() ? "offline put failed" : put.error;
    return false;
  }

  const auto start_fetch = std::chrono::steady_clock::now();
  std::string fetch_err;
  auto fetched = storage.Fetch(put.file_id, put.file_key, true, fetch_err);
  const auto end_fetch = std::chrono::steady_clock::now();
  if (!fetched) {
    error = fetch_err.empty() ? "offline fetch failed" : fetch_err;
    return false;
  }

  const double put_sec = ElapsedSeconds(start_put, end_put);
  const double fetch_sec = ElapsedSeconds(start_fetch, end_fetch);
  if (put_sec <= 0.0 || fetch_sec <= 0.0) {
    error = "offline timing invalid";
    return false;
  }

  put_mbps = {"offline_put_mbps",
              (cfg.offline_bytes / (1024.0 * 1024.0)) / put_sec, "MB/s"};
  fetch_mbps = {"offline_fetch_mbps",
                (cfg.offline_bytes / (1024.0 * 1024.0)) / fetch_sec, "MB/s"};

  std::filesystem::remove_all(base, ec);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  BenchConfig cfg;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--quick") {
      cfg.quick = true;
    } else if (arg == "--payload" && i + 1 < argc) {
      cfg.frame_payload = static_cast<std::size_t>(std::stoul(argv[++i]));
    }
  }
  if (cfg.quick) {
    cfg.frame_iters = 15000;
    cfg.decode_iters = 15000;
    cfg.offline_bytes = 2u * 1024u * 1024u;
  }

  std::cout << "mi_e2ee perf baseline\n";

  Metric enc_ops, enc_mbps;
  if (BenchFrameEncode(cfg, enc_ops, enc_mbps)) {
    PrintMetric(enc_ops);
    PrintMetric(enc_mbps);
  } else {
    std::cerr << "frame encode bench failed\n";
    return 1;
  }

  Metric dec_ops, dec_mbps;
  if (BenchFrameDecode(cfg, dec_ops, dec_mbps)) {
    PrintMetric(dec_ops);
    PrintMetric(dec_mbps);
  } else {
    std::cerr << "frame decode bench failed\n";
    return 1;
  }

  Metric put_mbps, fetch_mbps;
  std::string err;
  if (BenchOfflineStorage(cfg, put_mbps, fetch_mbps, err)) {
    PrintMetric(put_mbps);
    PrintMetric(fetch_mbps);
  } else {
    std::cerr << "offline storage bench failed: " << err << "\n";
    return 1;
  }

  return 0;
}
