#include <fstream>
#include <string>
#include <vector>

#include "connection_handler.h"
#include "frame.h"
#include "key_transparency.h"
#include "protocol.h"
#include "server_app.h"

using mi::server::ConnectionHandler;
using mi::server::DecodeFrame;
using mi::server::EncodeFrame;
using mi::server::Frame;
using mi::server::FrameType;
using mi::server::ServerApp;
using mi::server::proto::ReadUint32;
using mi::server::proto::ReadUint64;
using mi::server::proto::WriteString;

static void WriteFile(const std::string& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  f << content;
}

int main() {
  WriteFile("config.ini",
            "[mode]\nmode=1\n"
            "[server]\n"
            "list_port=7777\n"
            "offline_dir=.\n"
            "ops_enable=1\n"
            "ops_allow_remote=0\n"
            "ops_token=abcdefghijklmnop\n"
            "key_protection=none\n"
            "kt_signing_key=kt_signing_key.bin\n");
  WriteFile("test_user.txt", "alice:secret\n");
  {
    std::vector<std::uint8_t> key(mi::server::kKtSthSigSecretKeyBytes, 0x11);
    std::ofstream kf("kt_signing_key.bin", std::ios::binary | std::ios::trunc);
    if (!kf) {
      return 1;
    }
    kf.write(reinterpret_cast<const char*>(key.data()),
             static_cast<std::streamsize>(key.size()));
  }

  ServerApp app;
  std::string err;
  if (!app.Init("config.ini", err)) {
    return 1;
  }

  ConnectionHandler handler(&app);

  Frame req;
  req.type = FrameType::kHealthCheck;
  WriteString("abcdefghijklmnop", req.payload);
  const auto bytes = EncodeFrame(req);

  std::vector<std::uint8_t> out_bytes;
  if (!handler.OnData(bytes.data(), bytes.size(), out_bytes, "127.0.0.1")) {
    return 1;
  }
  Frame resp;
  if (!DecodeFrame(out_bytes.data(), out_bytes.size(), resp)) {
    return 1;
  }
  if (resp.type != FrameType::kHealthCheck || resp.payload.empty() ||
      resp.payload[0] != 1) {
    return 1;
  }
  std::size_t off = 1;
  std::uint32_t ver = 0;
  std::uint64_t uptime = 0;
  if (!ReadUint32(resp.payload, off, ver) ||
      !ReadUint64(resp.payload, off, uptime) || ver != 2) {
    return 1;
  }

  // Wrong token should fail.
  Frame bad_token_req;
  bad_token_req.type = FrameType::kHealthCheck;
  WriteString("bad_token", bad_token_req.payload);
  const auto bad_token_bytes = EncodeFrame(bad_token_req);
  out_bytes.clear();
  if (!handler.OnData(bad_token_bytes.data(), bad_token_bytes.size(), out_bytes,
                      "127.0.0.1")) {
    return 1;
  }
  if (!DecodeFrame(out_bytes.data(), out_bytes.size(), resp) || resp.payload.empty() ||
      resp.payload[0] != 0) {
    return 1;
  }

  // Non-loopback IP should fail when ops_allow_remote=0.
  out_bytes.clear();
  if (!handler.OnData(bytes.data(), bytes.size(), out_bytes, "8.8.8.8")) {
    return 1;
  }
  if (!DecodeFrame(out_bytes.data(), out_bytes.size(), resp) || resp.payload.empty() ||
      resp.payload[0] != 0) {
    return 1;
  }

  return 0;
}
