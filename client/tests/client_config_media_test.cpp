#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "client_config.h"

namespace {

std::filesystem::path MakeTempDir(const std::string& name_prefix) {
  std::error_code ec;
  auto base = std::filesystem::temp_directory_path(ec);
  if (base.empty()) {
    base = std::filesystem::current_path(ec);
  }
  if (base.empty()) {
    base = std::filesystem::path{"."};
  }
  std::filesystem::path dir = base / name_prefix;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir;
}

}  // namespace

int main() {
  const auto dir = MakeTempDir("mi_e2ee_client_config_media_test");
  const auto path = dir / "client_config.ini";

  std::ofstream out(path);
  out << "[client]\n";
  out << "server_ip=127.0.0.1\n";
  out << "server_port=9000\n";
  out << "use_tls=1\n";
  out << "require_tls=1\n";
  out << "trust_store=server_trust.ini\n";
  out << "require_pinned_fingerprint=1\n";
  out << "auth_mode=opaque\n";
  out << "allow_legacy_login=0\n";
  out << "\n";
  out << "[media]\n";
  out << "audio_delay_ms=80\n";
  out << "video_delay_ms=140\n";
  out << "audio_max_frames=0\n";
  out << "video_max_frames=512\n";
  out << "pull_max_packets=999\n";
  out << "pull_wait_ms=2000\n";
  out << "group_pull_max_packets=0\n";
  out << "group_pull_wait_ms=1500\n";
  out.close();

  mi::client::ClientConfig cfg;
  std::string err;
  assert(mi::client::LoadClientConfig(path.string(), cfg, err));
  assert(err.empty());
  assert(cfg.media.audio_delay_ms == 80);
  assert(cfg.media.video_delay_ms == 140);
  assert(cfg.media.audio_max_frames == 256);
  assert(cfg.media.video_max_frames == 512);
  assert(cfg.media.pull_max_packets == 256);
  assert(cfg.media.pull_wait_ms == 1000);
  assert(cfg.media.group_pull_max_packets == 64);
  assert(cfg.media.group_pull_wait_ms == 1000);
  return 0;
}
