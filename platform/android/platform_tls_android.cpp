#include "platform_tls.h"

namespace mi::platform::tls {

bool IsSupported() {
  return false;
}

bool ClientHandshake(net::Socket /*sock*/, const std::string& /*host*/,
                     ClientContext& ctx,
                     std::vector<std::uint8_t>& out_server_cert_der,
                     std::vector<std::uint8_t>& out_enc_buf,
                     std::string& error) {
  out_server_cert_der.clear();
  out_enc_buf.clear();
  error = "tls unsupported";
  ctx.impl = nullptr;
  return false;
}

bool EncryptAndSend(net::Socket /*sock*/, ClientContext& /*ctx*/,
                    const std::vector<std::uint8_t>& /*plain*/) {
  return false;
}

bool DecryptToPlain(net::Socket /*sock*/, ClientContext& /*ctx*/,
                    std::vector<std::uint8_t>& enc_buf,
                    std::vector<std::uint8_t>& plain_out) {
  enc_buf.clear();
  plain_out.clear();
  return false;
}

void Close(ClientContext& ctx) {
  ctx.impl = nullptr;
}

bool ServerInitCredentials(const std::string& /*pfx_path*/,
                           ServerCredentials& out,
                           std::string& error) {
  out.impl = nullptr;
  error = "tls unsupported";
  return false;
}

bool ServerHandshake(net::Socket /*sock*/, ServerCredentials& /*creds*/,
                     ServerContext& ctx,
                     std::vector<std::uint8_t>& out_extra,
                     std::string& error) {
  out_extra.clear();
  error = "tls unsupported";
  ctx.impl = nullptr;
  return false;
}

bool ServerHandshakeStep(ServerCredentials& /*creds*/, ServerContext& /*ctx*/,
                         std::vector<std::uint8_t>& in_buf,
                         std::vector<std::uint8_t>& out_tokens,
                         bool& out_done,
                         std::string& error) {
  in_buf.clear();
  out_tokens.clear();
  out_done = false;
  error = "tls unsupported";
  return false;
}

bool ServerEncryptAndSend(net::Socket /*sock*/, ServerContext& /*ctx*/,
                          const std::vector<std::uint8_t>& /*plain*/) {
  return false;
}

bool ServerDecryptToPlain(net::Socket /*sock*/, ServerContext& /*ctx*/,
                          std::vector<std::uint8_t>& enc_buf,
                          std::vector<std::uint8_t>& plain_out) {
  enc_buf.clear();
  plain_out.clear();
  return false;
}

bool ServerEncryptBuffer(ServerContext& /*ctx*/,
                         const std::vector<std::uint8_t>& /*plain*/,
                         std::vector<std::uint8_t>& out_cipher) {
  out_cipher.clear();
  return false;
}

bool ServerDecryptBuffer(ServerContext& /*ctx*/,
                         std::vector<std::uint8_t>& enc_buf,
                         std::vector<std::uint8_t>& plain_out,
                         bool& out_need_more) {
  enc_buf.clear();
  plain_out.clear();
  out_need_more = false;
  return false;
}

void Close(ServerContext& ctx) {
  ctx.impl = nullptr;
}

void Close(ServerCredentials& creds) {
  creds.impl = nullptr;
}

}  // namespace mi::platform::tls
