#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "display_contract.h"

namespace {

std::filesystem::path make_temp_dir() {
  auto dir = std::filesystem::temp_directory_path() /
             ("mi_e2ee_ui_display_contract_test_" +
              std::to_string(static_cast<unsigned long long>(
                  std::chrono::steady_clock::now().time_since_epoch().count())));
  std::filesystem::create_directories(dir);
  return dir;
}

void write_config(const std::filesystem::path& path,
                  const std::string& client_lines) {
  std::ofstream out(path, std::ios::binary);
  out << "[client]\n";
  out << client_lines;
}

mi::client::ui::display_contract::GatewayDisplayInfo read_info(
    const std::filesystem::path& path) {
  return mi::client::ui::display_contract::BuildGatewayDisplayInfo(
      QString::fromStdString(path.string()));
}

}

int main() {
  const auto dir = make_temp_dir();
  const auto cfg = dir / "client_config.ini";

  write_config(cfg, "server_ip=\nserver_port=9443\n");
  auto info = read_info(cfg);
  assert(info.state.isEmpty());
  assert(info.detail.isEmpty());

  write_config(cfg, "server_ip=chat.internal\nserver_port=9443\n");
  info = read_info(cfg);
  assert(info.state == QStringLiteral("已固定"));
  assert(info.detail == QStringLiteral("chat.internal:9443"));

  write_config(cfg,
              "server_ip=chat.internal\n"
              "server_port=9443\n"
              "require_pinned_fingerprint=0\n"
              "trust_store=server_trust.ini\n");
  info = read_info(cfg);
  assert(info.state == QStringLiteral("远程接入"));

  write_config(cfg,
              "server_ip=chat.internal\n"
              "server_port=9443\n"
              "tls_verify_mode=ca\n"
              "trust_store=server_trust.ini\n");
  info = read_info(cfg);
  assert(info.state == QStringLiteral("远程接入"));

  write_config(cfg,
              "server_ip=chat.internal\n"
              "server_port=9443\n"
              "tls_verify_mode=hybrid\n"
              "trust_store=server_trust.ini\n");
  info = read_info(cfg);
  assert(info.state == QStringLiteral("远程接入"));

  write_config(cfg,
              "server_ip=chat.internal\n"
              "server_port=9443\n"
              "tls_verify_mode=ca\n"
              "pinned_fingerprint="
              "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n");
  info = read_info(cfg);
  assert(info.state == QStringLiteral("已固定"));

  std::filesystem::remove_all(dir);
  return 0;
}
