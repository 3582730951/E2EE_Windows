#include "client_config.h"
#include "client_config_crypto.h"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::filesystem::path MakeTempDir() {
  auto dir = std::filesystem::temp_directory_path() /
             ("mi_e2ee_client_config_encryption_test_" +
              std::to_string(static_cast<unsigned long long>(
                  std::chrono::steady_clock::now().time_since_epoch().count())));
  std::filesystem::create_directories(dir);
  return dir;
}

std::string ReadText(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
}

std::string PlainConfig() {
  return "[client]\n"
         "server_ip=chat.example.test\n"
         "server_port=9443\n"
         "use_tls=1\n"
         "require_tls=1\n"
         "trust_store=server_trust.ini\n"
         "require_pinned_fingerprint=1\n"
         "pinned_fingerprint="
         "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n"
         "tls_verify_mode=pin\n"
         "tls_ca_bundle_path=\n"
         "tls_verify_hostname=1\n"
         "auth_mode=opaque\n"
         "\n"
         "[proxy]\n"
         "type=none\n"
         "host=\n"
         "port=0\n"
         "username=\n"
         "password=\n"
         "\n"
         "[device_sync]\n"
         "enabled=1\n"
         "role=primary\n"
         "key_path=e2ee_state/device_sync_key.bin\n"
         "\n"
         "[kt]\n"
         "require_signature=1\n"
         "root_pubkey_path=kt_root_pub.bin\n";
}

}  // namespace

int main() {
  const auto dir = MakeTempDir();
  const auto cfg_path = dir / "client_config.ini";
  const std::string plain = PlainConfig();

  std::string error;
  assert(mi::client::WriteEncryptedClientConfigText(cfg_path, plain, error));

  const std::string raw = ReadText(cfg_path);
  assert(raw.find("server_ip=") == std::string::npos);
  assert(raw.find("chat.example.test") == std::string::npos);
  assert(raw.find("pinned_fingerprint=") == std::string::npos);
  assert(raw.find("0123456789abcdef") == std::string::npos);

  mi::client::ClientConfig loaded;
  assert(mi::client::LoadClientConfig(cfg_path.string(), loaded, error));
  assert(loaded.server_ip == "chat.example.test");
  assert(loaded.server_port == 9443);
  assert(loaded.use_tls);
  assert(loaded.require_tls);
  assert(loaded.tls_verify_mode == mi::client::TlsVerifyMode::kPin);
  assert(loaded.pinned_fingerprint ==
         "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");

  const auto tool_cfg = dir / "tool_client_config.ini";
  const std::string tool_cfg_text = tool_cfg.string();
  std::vector<const char*> args = {
      "mi_e2ee_client_config_tool",
      "--config",
      tool_cfg_text.c_str(),
      "--server",
      "ci.example.test",
      "--port",
      "9444",
      "--pinned-fingerprint",
      "abcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcd",
      "--kt-root-pub",
      "kt_root_pub.bin",
      "--non-interactive",
  };
  assert(mi::client::RunClientConfigTool(
             static_cast<int>(args.size()),
             const_cast<char**>(args.data())) == 0);
  const std::string tool_raw = ReadText(tool_cfg);
  assert(tool_raw.find("ci.example.test") == std::string::npos);
  assert(tool_raw.find("server_ip=") == std::string::npos);
  assert(mi::client::LoadClientConfig(tool_cfg.string(), loaded, error));
  assert(loaded.server_ip == "ci.example.test");
  assert(loaded.server_port == 9444);
  assert(loaded.kt.root_pubkey_path == "kt_root_pub.bin");

  const auto missing_server_cfg = dir / "missing_server.ini";
  const std::string missing_server_cfg_text = missing_server_cfg.string();
  std::vector<const char*> missing_server_args = {
      "mi_e2ee_client_config_tool",
      "--config",
      missing_server_cfg_text.c_str(),
      "--port",
      "9444",
      "--pinned-fingerprint",
      "abcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcd",
      "--non-interactive",
  };
  assert(mi::client::RunClientConfigTool(
             static_cast<int>(missing_server_args.size()),
             const_cast<char**>(missing_server_args.data())) != 0);

  const auto missing_port_cfg = dir / "missing_port.ini";
  const std::string missing_port_cfg_text = missing_port_cfg.string();
  std::vector<const char*> missing_port_args = {
      "mi_e2ee_client_config_tool",
      "--config",
      missing_port_cfg_text.c_str(),
      "--server",
      "ci.example.test",
      "--pinned-fingerprint",
      "abcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcdefabcd",
      "--non-interactive",
  };
  assert(mi::client::RunClientConfigTool(
             static_cast<int>(missing_port_args.size()),
             const_cast<char**>(missing_port_args.data())) != 0);

  std::filesystem::remove_all(dir);
  return 0;
}
