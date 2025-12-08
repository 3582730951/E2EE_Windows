#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>

#include "server_app.h"
#include "network_server.h"

namespace {

void LogError(const std::string& msg) { std::cerr << "[error] " << msg << '\n'; }
void LogInfo(const std::string& msg) { std::cout << "[info] " << msg << '\n'; }

std::vector<std::uint8_t> HexToBytes(const std::string& hex) {
  std::vector<std::uint8_t> out;
  std::string clean;
  for (char c : hex) {
    if (!std::isspace(static_cast<unsigned char>(c))) clean.push_back(c);
  }
  if (clean.size() % 2 != 0) return out;
  out.reserve(clean.size() / 2);
  for (std::size_t i = 0; i < clean.size(); i += 2) {
    const std::string byte_str = clean.substr(i, 2);
    const std::uint8_t b =
        static_cast<std::uint8_t>(std::strtoul(byte_str.c_str(), nullptr, 16));
    out.push_back(b);
  }
  return out;
}

std::string BytesToHex(const std::vector<std::uint8_t>& data) {
  static const char* hex = "0123456789abcdef";
  std::string out;
  out.reserve(data.size() * 2);
  for (auto b : data) {
    out.push_back(hex[b >> 4]);
    out.push_back(hex[b & 0x0F]);
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string config_path = (argc > 1) ? argv[1] : "config.ini";
  bool use_stdin = false;
  if (argc > 2 && std::string(argv[2]) == "--stdin") {
    use_stdin = true;
  }

  std::string error;
  mi::server::ServerApp app;
  if (!app.Init(config_path, error)) {
    LogError(error);
    return 1;
  }

  const auto& cfg = app.config();
  LogInfo(std::string("server config loaded. mode=") +
          (cfg.mode == mi::server::AuthMode::kDemo ? "demo" : "mysql") +
          " listen_port=" + std::to_string(cfg.server.listen_port));

  if (use_stdin) {
    LogInfo("stdin mode: paste hex frame, Ctrl+D to process");
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    const auto bytes = HexToBytes(ss.str());
    if (bytes.empty()) {
      LogError("no bytes read");
      return 1;
    }
    mi::server::Frame in;
    if (!mi::server::DecodeFrame(bytes.data(), bytes.size(), in)) {
      LogError("decode frame failed");
      return 1;
    }
    mi::server::Frame out;
    if (!app.HandleFrame(in, out, error)) {
      LogError(error);
      return 1;
    }
    const auto out_bytes = mi::server::EncodeFrame(out);
    std::cout << BytesToHex(out_bytes) << std::endl;
    return 0;
  }

  mi::server::Listener listener(&app);
  mi::server::NetworkServer net(&listener, cfg.server.listen_port);
  if (!net.Start()) {
    LogError("network server start failed (placeholder)");
    return 1;
  }
  LogInfo("server initialized (network placeholder running)");
  // 占位运行，保持主线程。
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
  return 0;
}
