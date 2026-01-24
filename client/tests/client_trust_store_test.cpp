#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "client_config.h"
#include "security_service.h"
#include "trust_store.h"

namespace {

#define FAIL()                                                     \
  do {                                                             \
    std::cerr << "client_trust_store_test failed at " << __FILE__  \
              << ":" << __LINE__ << "\n";                          \
    return 1;                                                      \
  } while (false)

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
  out << "trust_store=server_trust.ini\n";
  out << "auth_mode=opaque\n";
  out << "allow_legacy_login=0\n";
  out << extra_lines;
  out.close();
}

}  // namespace

int main() {
  {
    const auto dir = MakeTempDir("mi_e2ee_trust_store_hybrid");
    const auto path = dir / "client_config.ini";
    const std::string pin =
        "AABBCCDDEEFF00112233445566778899AABBCCDDEEFF00112233445566778899";
    WriteConfig(path, "tls_verify_mode=hybrid\npinned_fingerprint=" + pin + "\n");

    mi::client::ClientConfig cfg;
    std::string err;
    if (!mi::client::LoadClientConfig(path.string(), cfg, err)) {
      FAIL();
    }

    mi::client::SecurityService security;
    std::string trust_store_path;
    std::string out_pin;
    bool tls_required = false;
    const bool allow_pinned = (cfg.tls_verify_mode != mi::client::TlsVerifyMode::kCa);
    if (!security.LoadTrustFromConfig(cfg, dir, cfg.server_ip, cfg.server_port,
                                      cfg.require_tls, allow_pinned,
                                      trust_store_path, out_pin, tls_required,
                                      err)) {
      FAIL();
    }
    if (out_pin != mi::client::security::NormalizeFingerprint(pin)) {
      FAIL();
    }
    if (trust_store_path.empty()) {
      FAIL();
    }
    std::error_code ec;
    if (!std::filesystem::exists(trust_store_path, ec) || ec) {
      FAIL();
    }
    mi::client::security::TrustEntry entry;
    if (!mi::client::security::LoadTrustEntry(
            trust_store_path,
            mi::client::security::EndpointKey(cfg.server_ip, cfg.server_port),
            entry)) {
      FAIL();
    }
    if (entry.fingerprint != out_pin || !entry.tls_required) {
      FAIL();
    }
  }

  {
    const auto dir = MakeTempDir("mi_e2ee_trust_store_ca");
    const auto path = dir / "client_config.ini";
    const std::string pin =
        "00112233445566778899AABBCCDDEEFF00112233445566778899AABBCCDDEEFF";
    WriteConfig(path, "tls_verify_mode=ca\npinned_fingerprint=" + pin + "\n");

    mi::client::ClientConfig cfg;
    std::string err;
    if (!mi::client::LoadClientConfig(path.string(), cfg, err)) {
      FAIL();
    }

    mi::client::SecurityService security;
    std::string trust_store_path;
    std::string out_pin;
    bool tls_required = false;
    const bool allow_pinned = (cfg.tls_verify_mode != mi::client::TlsVerifyMode::kCa);
    if (!security.LoadTrustFromConfig(cfg, dir, cfg.server_ip, cfg.server_port,
                                      cfg.require_tls, allow_pinned,
                                      trust_store_path, out_pin, tls_required,
                                      err)) {
      FAIL();
    }
    if (!out_pin.empty()) {
      FAIL();
    }
    std::error_code ec;
    if (!trust_store_path.empty() &&
        std::filesystem::exists(trust_store_path, ec) && !ec) {
      FAIL();
    }
  }

  return 0;
}
