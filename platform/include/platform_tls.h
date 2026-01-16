#ifndef MI_E2EE_PLATFORM_TLS_H
#define MI_E2EE_PLATFORM_TLS_H

#include <cstdint>
#include <string>
#include <vector>

#include "platform_net.h"

namespace mi::platform::tls {

struct ClientContext {
  void* impl{nullptr};
};

struct ServerCredentials {
  void* impl{nullptr};
};

struct ServerContext {
  void* impl{nullptr};
};

bool IsSupported();
bool ClientHandshake(net::Socket sock, const std::string& host,
                     ClientContext& ctx,
                     std::vector<std::uint8_t>& out_server_cert_der,
                     std::vector<std::uint8_t>& out_enc_buf,
                     std::string& error);
bool EncryptAndSend(net::Socket sock, ClientContext& ctx,
                    const std::vector<std::uint8_t>& plain);
bool DecryptToPlain(net::Socket sock, ClientContext& ctx,
                    std::vector<std::uint8_t>& enc_buf,
                    std::vector<std::uint8_t>& plain_out);
void Close(ClientContext& ctx);

bool ServerInitCredentials(const std::string& pfx_path,
                           ServerCredentials& out,
                           std::string& error);
bool ServerHandshake(net::Socket sock, ServerCredentials& creds,
                     ServerContext& ctx,
                     std::vector<std::uint8_t>& out_extra,
                     std::string& error);
bool ServerHandshakeStep(ServerCredentials& creds, ServerContext& ctx,
                         std::vector<std::uint8_t>& in_buf,
                         std::vector<std::uint8_t>& out_tokens,
                         bool& out_done,
                         std::string& error);
bool ServerEncryptAndSend(net::Socket sock, ServerContext& ctx,
                          const std::vector<std::uint8_t>& plain);
bool ServerDecryptToPlain(net::Socket sock, ServerContext& ctx,
                          std::vector<std::uint8_t>& enc_buf,
                          std::vector<std::uint8_t>& plain_out);
bool ServerEncryptBuffer(ServerContext& ctx,
                         const std::vector<std::uint8_t>& plain,
                         std::vector<std::uint8_t>& out_cipher);
bool ServerDecryptBuffer(ServerContext& ctx,
                         std::vector<std::uint8_t>& enc_buf,
                         std::vector<std::uint8_t>& plain_out,
                         bool& out_need_more);
void Close(ServerContext& ctx);
void Close(ServerCredentials& creds);

}  // namespace mi::platform::tls

#endif  // MI_E2EE_PLATFORM_TLS_H
