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

void WriteConfig(const std::filesystem::path& path,
                 const std::string& extra_lines) {
  std::ofstream out(path);
  out << "[client]\n";
  out << "server_ip=127.0.0.1\n";
  out << "server_port=9000\n";
  out << "use_tls=1\n";
  out << "require_tls=1\n";
  out << "auth_mode=opaque\n";
  out << "allow_legacy_login=0\n";
  out << extra_lines;
  out.close();
}

}  // namespace

int main() {
  const auto dir = MakeTempDir("mi_e2ee_client_config_tls_test");
  const auto path = dir / "client_config.ini";

  {
    WriteConfig(path, "tls_verify_mode=hybrid\n");
    mi::client::ClientConfig cfg;
    std::string err;
    assert(mi::client::LoadClientConfig(path.string(), cfg, err));
    assert(err.empty());
    assert(cfg.tls_verify_mode == mi::client::TlsVerifyMode::kHybrid);
    assert(cfg.require_pinned_fingerprint == false);
  }

  {
    WriteConfig(path, "tls_verify_mode=pin\n");
    mi::client::ClientConfig cfg;
    std::string err;
    assert(mi::client::LoadClientConfig(path.string(), cfg, err));
    assert(err.empty());
    assert(cfg.tls_verify_mode == mi::client::TlsVerifyMode::kPin);
    assert(cfg.require_pinned_fingerprint == true);
  }

  {
    WriteConfig(path, "require_pinned_fingerprint=0\n");
    mi::client::ClientConfig cfg;
    std::string err;
    assert(mi::client::LoadClientConfig(path.string(), cfg, err));
    assert(err.empty());
    assert(cfg.tls_verify_mode == mi::client::TlsVerifyMode::kCa);
    assert(cfg.require_pinned_fingerprint == false);
  }

  return 0;
}
