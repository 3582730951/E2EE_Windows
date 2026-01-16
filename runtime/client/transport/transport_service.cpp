#include "transport_service.h"

#include "client_core.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "c_api.h"
#include "chat_history_store.h"
#include "frame.h"
#include "constant_time.h"
#include "hex_utils.h"
#include "ikcp.h"
#include "platform_net.h"
#include "platform_random.h"
#include "platform_time.h"
#include "platform_tls.h"
#include "protocol.h"

namespace mi::client {

namespace {

constexpr std::uint8_t kKcpCookieCmd = 0xFF;
constexpr std::uint8_t kKcpCookieHello = 1;
constexpr std::uint8_t kKcpCookieChallenge = 2;
constexpr std::uint8_t kKcpCookieResponse = 3;
constexpr std::size_t kKcpCookieBytes = 16;
constexpr std::size_t kKcpCookiePacketBytes = 24;

std::string FingerprintSas80Hex(const std::string& sha256_hex) {
  std::vector<std::uint8_t> fp_bytes;
  if (!mi::common::HexToBytes(sha256_hex, fp_bytes) ||
      fp_bytes.size() != 32) {
    return {};
  }

  std::vector<std::uint8_t> msg;
  static constexpr char kPrefix[] = "MI_SERVER_CERT_SAS_V1";
  msg.insert(msg.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  msg.insert(msg.end(), fp_bytes.begin(), fp_bytes.end());

  const std::string h = mi::common::Sha256Hex(msg.data(), msg.size());
  if (h.size() < 20) {
    return {};
  }
  return mi::common::GroupHex4(h.substr(0, 20));
}

bool RandomUint32(std::uint32_t& out) {
  return mi::platform::RandomUint32(out);
}

std::uint32_t NowMs() {
  return static_cast<std::uint32_t>(mi::platform::NowSteadyMs());
}

bool TlsReadFrameBuffered(mi::platform::net::Socket sock,
                          mi::platform::tls::ClientContext& ctx,
                          std::vector<std::uint8_t>& enc_buf,
                          std::vector<std::uint8_t>& plain_buf,
                          std::size_t& plain_off,
                          std::vector<std::uint8_t>& out_frame) {
  out_frame.clear();
  if (plain_off > plain_buf.size()) {
    plain_buf.clear();
    plain_off = 0;
  }

  while (true) {
    const std::size_t avail =
        plain_buf.size() >= plain_off ? (plain_buf.size() - plain_off) : 0;
    if (avail >= mi::server::kFrameHeaderSize) {
      mi::server::FrameType type;
      std::uint32_t payload_len = 0;
      if (!mi::server::DecodeFrameHeader(plain_buf.data() + plain_off, avail,
                                         type, payload_len)) {
        return false;
      }
      (void)type;
      const std::size_t total =
          mi::server::kFrameHeaderSize + static_cast<std::size_t>(payload_len);
      if (avail >= total) {
        out_frame.assign(
            plain_buf.begin() + static_cast<std::ptrdiff_t>(plain_off),
            plain_buf.begin() + static_cast<std::ptrdiff_t>(plain_off + total));
        plain_off += total;
        if (plain_off >= plain_buf.size()) {
          plain_buf.clear();
          plain_off = 0;
        } else if (plain_off > (1024u * 1024u)) {
          std::vector<std::uint8_t> compact(
              plain_buf.begin() + static_cast<std::ptrdiff_t>(plain_off),
              plain_buf.end());
          plain_buf.swap(compact);
          plain_off = 0;
        }
        return true;
      }
    }

    std::vector<std::uint8_t> plain_chunk;
    if (!mi::platform::tls::DecryptToPlain(sock, ctx, enc_buf, plain_chunk)) {
      return false;
    }
    if (!plain_chunk.empty()) {
      plain_buf.insert(plain_buf.end(), plain_chunk.begin(), plain_chunk.end());
    }
  }
}

}  // namespace

struct ClientCore::RemoteStream {
  std::string host;
  std::uint16_t port{0};
  bool use_tls{false};
  bool use_kcp{false};
  KcpConfig kcp_cfg;
  ProxyConfig proxy;
  std::string pinned_fingerprint;

  ikcpcb* kcp{nullptr};
  std::uint32_t kcp_conv{0};
  std::vector<std::uint8_t> kcp_recv_buf;
  std::uint64_t kcp_last_active_ms{0};

  mi::platform::net::Socket sock{mi::platform::net::kInvalidSocket};
  mi::platform::tls::ClientContext tls_ctx;
  std::vector<std::uint8_t> enc_buf;
  std::vector<std::uint8_t> plain_buf;
  std::size_t plain_off{0};

  RemoteStream(std::string host_in, std::uint16_t port_in, bool use_tls_in,
               bool use_kcp_in, KcpConfig kcp_cfg_in, ProxyConfig proxy_in,
               std::string pinned_fingerprint_in)
      : host(std::move(host_in)),
        port(port_in),
        use_tls(use_tls_in),
        use_kcp(use_kcp_in),
        kcp_cfg(std::move(kcp_cfg_in)),
        proxy(std::move(proxy_in)),
        pinned_fingerprint(std::move(pinned_fingerprint_in)) {}

  ~RemoteStream() { Close(); }

  bool Matches(const std::string& host_in, std::uint16_t port_in,
               bool use_tls_in, bool use_kcp_in, const KcpConfig& kcp_cfg_in,
               const ProxyConfig& proxy_in,
               const std::string& pinned_fingerprint_in) const {
    if (host != host_in || port != port_in || use_tls != use_tls_in ||
        use_kcp != use_kcp_in || pinned_fingerprint != pinned_fingerprint_in) {
      return false;
    }
    if (use_kcp) {
      if (kcp_cfg.enable != kcp_cfg_in.enable ||
          kcp_cfg.server_port != kcp_cfg_in.server_port ||
          kcp_cfg.mtu != kcp_cfg_in.mtu ||
          kcp_cfg.snd_wnd != kcp_cfg_in.snd_wnd ||
          kcp_cfg.rcv_wnd != kcp_cfg_in.rcv_wnd ||
          kcp_cfg.nodelay != kcp_cfg_in.nodelay ||
          kcp_cfg.interval != kcp_cfg_in.interval ||
          kcp_cfg.resend != kcp_cfg_in.resend ||
          kcp_cfg.nc != kcp_cfg_in.nc ||
          kcp_cfg.min_rto != kcp_cfg_in.min_rto ||
          kcp_cfg.request_timeout_ms != kcp_cfg_in.request_timeout_ms ||
          kcp_cfg.session_idle_sec != kcp_cfg_in.session_idle_sec) {
        return false;
      }
    }
    return proxy.type == proxy_in.type && proxy.host == proxy_in.host &&
           proxy.port == proxy_in.port && proxy.username == proxy_in.username &&
           proxy.password == proxy_in.password;
  }

  void Close() {
    if (kcp) {
      ikcp_release(kcp);
      kcp = nullptr;
    }
    kcp_recv_buf.clear();
    kcp_conv = 0;
    kcp_last_active_ms = 0;
    mi::platform::tls::Close(tls_ctx);
    mi::platform::net::CloseSocket(sock);
    enc_buf.clear();
    plain_buf.clear();
    plain_off = 0;
  }

  static int KcpOutput(const char* buf, int len, ikcpcb* /*kcp*/, void* user) {
    if (!buf || len <= 0 || !user) {
      return -1;
    }
    auto* self = static_cast<RemoteStream*>(user);
    const int sent = mi::platform::net::Send(
        self->sock, reinterpret_cast<const std::uint8_t*>(buf),
        static_cast<std::size_t>(len));
    return sent == len ? 0 : -1;
  }

  bool ConnectPlain(std::string& error) {
    error.clear();
    if (host.empty() || port == 0) {
      error = "invalid endpoint";
      return false;
    }

    const bool use_proxy = proxy.enabled();
    if (use_proxy && proxy.type != ProxyType::kSocks5) {
      error = "unsupported proxy";
      return false;
    }
    const std::string connect_host = use_proxy ? proxy.host : host;
    const std::uint16_t connect_port = use_proxy ? proxy.port : port;

    mi::platform::net::Socket new_sock = mi::platform::net::kInvalidSocket;
    if (!mi::platform::net::ConnectTcp(connect_host, connect_port, new_sock,
                                       error)) {
      return false;
    }
    const std::uint32_t timeout_ms = 30000;
    mi::platform::net::SetRecvTimeout(new_sock, timeout_ms);
    mi::platform::net::SetSendTimeout(new_sock, timeout_ms);

    if (!use_proxy) {
      sock = new_sock;
      return true;
    }

    const auto close_socket = [&]() {
      mi::platform::net::CloseSocket(new_sock);
    };

    std::vector<std::uint8_t> req;
    req.reserve(3 + 1 + proxy.username.size() + 1 + proxy.password.size());
    req.push_back(0x05);
    if (proxy.username.empty() && proxy.password.empty()) {
      req.push_back(0x01);
      req.push_back(0x00);
    } else {
      req.push_back(0x02);
      req.push_back(0x00);
      req.push_back(0x02);
    }
    if (!mi::platform::net::SendAll(new_sock, req.data(), req.size())) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }

    std::uint8_t rep[2] = {};
    if (!mi::platform::net::RecvExact(new_sock, rep, sizeof(rep)) ||
        rep[0] != 0x05) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }
    if (rep[1] == 0x02) {
      if (proxy.username.size() > 255 || proxy.password.size() > 255) {
        error = "proxy auth failed";
        close_socket();
        return false;
      }
      std::vector<std::uint8_t> auth;
      auth.reserve(3 + proxy.username.size() + proxy.password.size());
      auth.push_back(0x01);
      auth.push_back(static_cast<std::uint8_t>(proxy.username.size()));
      auth.insert(auth.end(), proxy.username.begin(), proxy.username.end());
      auth.push_back(static_cast<std::uint8_t>(proxy.password.size()));
      auth.insert(auth.end(), proxy.password.begin(), proxy.password.end());
      if (!mi::platform::net::SendAll(new_sock, auth.data(), auth.size())) {
        error = "proxy auth failed";
        close_socket();
        return false;
      }
      std::uint8_t auth_rep[2] = {};
      if (!mi::platform::net::RecvExact(new_sock, auth_rep,
                                        sizeof(auth_rep)) ||
          auth_rep[1] != 0x00) {
        error = "proxy auth failed";
        close_socket();
        return false;
      }
    } else if (rep[1] != 0x00) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }

    if (host.size() > 255) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }

    std::vector<std::uint8_t> connect_req;
    connect_req.reserve(4 + 1 + host.size() + 2);
    connect_req.push_back(0x05);
    connect_req.push_back(0x01);
    connect_req.push_back(0x00);
    connect_req.push_back(0x03);
    connect_req.push_back(static_cast<std::uint8_t>(host.size()));
    connect_req.insert(connect_req.end(), host.begin(), host.end());
    connect_req.push_back(static_cast<std::uint8_t>((port >> 8) & 0xFF));
    connect_req.push_back(static_cast<std::uint8_t>(port & 0xFF));
    if (!mi::platform::net::SendAll(new_sock, connect_req.data(),
                                    connect_req.size())) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }

    std::uint8_t rep2[4] = {};
    if (!mi::platform::net::RecvExact(new_sock, rep2, sizeof(rep2)) ||
        rep2[0] != 0x05 || rep2[1] != 0x00) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }

    {
      std::size_t to_read = 0;
      if (rep2[3] == 0x01) {
        to_read = 4 + 2;
      } else if (rep2[3] == 0x03) {
        std::uint8_t len_byte = 0;
        if (!mi::platform::net::RecvExact(new_sock, &len_byte, 1)) {
          error = "proxy connect failed";
          close_socket();
          return false;
        }
        to_read = static_cast<std::size_t>(len_byte) + 2;
      } else if (rep2[3] == 0x04) {
        to_read = 16 + 2;
      } else {
        error = "proxy connect failed";
        close_socket();
        return false;
      }
      std::vector<std::uint8_t> discard;
      discard.resize(to_read);
      if (!mi::platform::net::RecvExact(new_sock, discard.data(),
                                        discard.size())) {
        error = "proxy connect failed";
        close_socket();
        return false;
      }
    }

    sock = new_sock;
    return true;
  }

  bool ConnectKcp(std::string& error) {
    error.clear();
    if (host.empty() || port == 0) {
      error = "invalid endpoint";
      return false;
    }
    if (proxy.enabled()) {
      error = "kcp does not support proxy";
      return false;
    }

    mi::platform::net::Socket new_sock = mi::platform::net::kInvalidSocket;
    if (!mi::platform::net::ConnectUdp(host, port, new_sock, error)) {
      return false;
    }

    if (!mi::platform::net::SetNonBlocking(new_sock)) {
      error = "kcp non-blocking failed";
      mi::platform::net::CloseSocket(new_sock);
      return false;
    }

    sock = new_sock;
    std::uint32_t conv = 0;
    if (!RandomUint32(conv) || conv == 0) {
      conv = NowMs() ^ 0xA5A5A5A5u;
    }

    auto write_le32 = [](std::uint32_t v, std::uint8_t out[4]) {
      out[0] = static_cast<std::uint8_t>(v & 0xFF);
      out[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
      out[2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
      out[3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
    };
    auto read_le32 = [](const std::uint8_t in[4]) -> std::uint32_t {
      return static_cast<std::uint32_t>(in[0]) |
             (static_cast<std::uint32_t>(in[1]) << 8) |
             (static_cast<std::uint32_t>(in[2]) << 16) |
             (static_cast<std::uint32_t>(in[3]) << 24);
    };
    auto build_cookie_packet =
        [&](std::uint8_t type,
            const std::array<std::uint8_t, kKcpCookieBytes>& cookie,
            std::array<std::uint8_t, kKcpCookiePacketBytes>& out) {
          write_le32(conv, out.data());
          out[4] = kKcpCookieCmd;
          out[5] = type;
          out[6] = 0;
          out[7] = 0;
          std::memcpy(out.data() + 8, cookie.data(), cookie.size());
        };
    auto send_cookie_packet =
        [&](std::uint8_t type,
            const std::array<std::uint8_t, kKcpCookieBytes>& cookie) -> bool {
          std::array<std::uint8_t, kKcpCookiePacketBytes> out{};
          build_cookie_packet(type, cookie, out);
          const int sent =
              mi::platform::net::Send(sock, out.data(), out.size());
          return sent == static_cast<int>(out.size());
        };

    if (!send_cookie_packet(kKcpCookieHello, {})) {
      error = "kcp cookie hello failed";
      Close();
      return false;
    }

    const std::uint64_t start_ms = mi::platform::NowSteadyMs();
    std::array<std::uint8_t, kKcpCookieBytes> cookie{};
    bool got_cookie = false;
    while (true) {
      std::uint8_t buf[64] = {};
      const int n = mi::platform::net::Recv(sock, buf, sizeof(buf));
      if (n > 0) {
        if (static_cast<std::size_t>(n) >= kKcpCookiePacketBytes &&
            buf[4] == kKcpCookieCmd && read_le32(buf) == conv &&
            buf[5] == kKcpCookieChallenge) {
          std::memcpy(cookie.data(), buf + 8, cookie.size());
          got_cookie = true;
          break;
        }
        continue;
      }
      if (n == 0) {
        error = "kcp cookie recv failed";
        Close();
        return false;
      }
      if (!mi::platform::net::SocketWouldBlock()) {
        error = "kcp cookie recv failed";
        Close();
        return false;
      }
      const std::uint64_t elapsed_ms =
          mi::platform::NowSteadyMs() - start_ms;
      if (elapsed_ms > static_cast<std::uint64_t>(kcp_cfg.request_timeout_ms)) {
        error = "kcp cookie timeout";
        Close();
        return false;
      }
      mi::platform::SleepMs(5);
    }

    if (!got_cookie ||
        !send_cookie_packet(kKcpCookieResponse, cookie)) {
      error = "kcp cookie response failed";
      Close();
      return false;
    }

    kcp_conv = conv;
    kcp = ikcp_create(conv, this);
    if (!kcp) {
      error = "kcp create failed";
      Close();
      return false;
    }
    kcp->output = KcpOutput;
    ikcp_setmtu(kcp, static_cast<int>(kcp_cfg.mtu));
    ikcp_wndsize(kcp, static_cast<int>(kcp_cfg.snd_wnd),
                 static_cast<int>(kcp_cfg.rcv_wnd));
    ikcp_nodelay(kcp, static_cast<int>(kcp_cfg.nodelay),
                 static_cast<int>(kcp_cfg.interval),
                 static_cast<int>(kcp_cfg.resend),
                 static_cast<int>(kcp_cfg.nc));
    if (kcp_cfg.min_rto > 0) {
      kcp->rx_minrto = static_cast<int>(kcp_cfg.min_rto);
    }
    kcp_recv_buf.resize(std::max<std::uint32_t>(kcp_cfg.mtu, 1200u) + 256u);
    kcp_last_active_ms = mi::platform::NowSteadyMs();
    return true;
  }

  bool ConnectTls(std::string& out_server_fingerprint, std::string& error) {
    out_server_fingerprint.clear();
    if (!mi::platform::tls::IsSupported()) {
      error = "tls unsupported";
      return false;
    }
    if (!ConnectPlain(error)) {
      return false;
    }
    std::vector<std::uint8_t> cert_der;
    std::vector<std::uint8_t> extra;
    if (!mi::platform::tls::ClientHandshake(sock, host, tls_ctx, cert_der,
                                            extra, error)) {
      Close();
      return false;
    }

    out_server_fingerprint =
        mi::common::Sha256Hex(cert_der.data(), cert_der.size());
    if (out_server_fingerprint.empty()) {
      error = "cert fingerprint failed";
      Close();
      return false;
    }
    if (pinned_fingerprint.empty()) {
      error = "server not trusted";
      Close();
      return false;
    }
    if (!mi::common::ConstantTimeEqual(pinned_fingerprint,
                                       out_server_fingerprint)) {
      error = "server fingerprint changed";
      Close();
      return false;
    }

    enc_buf = std::move(extra);
    return true;
  }

  bool Connect(std::string& out_server_fingerprint, std::string& error) {
    out_server_fingerprint.clear();
    if (use_kcp) {
      return ConnectKcp(error);
    }
    if (use_tls) {
      return ConnectTls(out_server_fingerprint, error);
    }
    return ConnectPlain(error);
  }

  bool SendAndRecv(const std::vector<std::uint8_t>& in_bytes,
                   std::vector<std::uint8_t>& out_bytes,
                   std::string& error) {
    out_bytes.clear();
    error.clear();
    if (use_kcp) {
      const std::uint64_t now_ms64 = mi::platform::NowSteadyMs();
      if (kcp_cfg.session_idle_sec > 0 && kcp_last_active_ms != 0) {
        const std::uint64_t idle_ms = now_ms64 - kcp_last_active_ms;
        if (idle_ms >
            static_cast<std::uint64_t>(kcp_cfg.session_idle_sec) * 1000u) {
          error = "kcp idle timeout";
          return false;
        }
      }

      if (!kcp || sock == mi::platform::net::kInvalidSocket) {
        error = "not connected";
        return false;
      }
      if (in_bytes.empty()) {
        error = "empty request";
        return false;
      }

      if (ikcp_send(kcp, reinterpret_cast<const char*>(in_bytes.data()),
                    static_cast<int>(in_bytes.size())) < 0) {
        error = "kcp send failed";
        return false;
      }
      ikcp_flush(kcp);
      kcp_last_active_ms = now_ms64;

      const std::uint32_t start_ms = NowMs();
      const std::uint32_t timeout_ms =
          kcp_cfg.request_timeout_ms == 0 ? 5000u
                                          : kcp_cfg.request_timeout_ms;
      if (kcp_recv_buf.empty()) {
        kcp_recv_buf.resize(1400u + 256u);
      }
      auto& datagram = kcp_recv_buf;

      while (true) {
        const std::uint32_t now_ms = NowMs();
        if (now_ms - start_ms >= timeout_ms) {
          error = "kcp timeout";
          return false;
        }

        while (true) {
          const int n = mi::platform::net::Recv(sock, datagram.data(),
                                                datagram.size());
          if (n < 0) {
            if (mi::platform::net::SocketWouldBlock()) {
              break;
            }
            error = "kcp recv failed";
            return false;
          }
          if (n > 0) {
            ikcp_input(kcp, reinterpret_cast<const char*>(datagram.data()), n);
            kcp_last_active_ms = mi::platform::NowSteadyMs();
          } else {
            break;
          }
        }

        const int peek = ikcp_peeksize(kcp);
        if (peek > 0) {
          out_bytes.resize(static_cast<std::size_t>(peek));
          const int n = ikcp_recv(
              kcp, reinterpret_cast<char*>(out_bytes.data()), peek);
          if (n > 0) {
            out_bytes.resize(static_cast<std::size_t>(n));
            return true;
          }
          out_bytes.clear();
        }

        const std::uint32_t check = ikcp_check(kcp, now_ms);
        const std::uint32_t wait_ms =
            check > now_ms ? (check - now_ms) : 1u;
        const std::uint32_t remaining = timeout_ms - (now_ms - start_ms);
        const std::uint32_t sleep_ms = std::min(wait_ms, remaining);
        mi::platform::net::WaitForReadable(sock, sleep_ms);
        ikcp_update(kcp, NowMs());
      }
    }
    if (use_tls) {
      if (!mi::platform::tls::IsSupported()) {
        error = "tls unsupported";
        return false;
      }
      if (sock == mi::platform::net::kInvalidSocket) {
        error = "not connected";
        return false;
      }
      if (!mi::platform::tls::EncryptAndSend(sock, tls_ctx, in_bytes)) {
        error = "tls send failed";
        return false;
      }
      if (!TlsReadFrameBuffered(sock, tls_ctx, enc_buf, plain_buf, plain_off,
                                out_bytes)) {
        error = "tls recv failed";
        return false;
      }
      return !out_bytes.empty();
    }

    if (sock == mi::platform::net::kInvalidSocket) {
      error = "not connected";
      return false;
    }

    if (!mi::platform::net::SendAll(sock, in_bytes.data(), in_bytes.size())) {
      error = "tcp send failed";
      return false;
    }

    std::uint8_t header[mi::server::kFrameHeaderSize] = {};
    if (!mi::platform::net::RecvExact(sock, header, sizeof(header))) {
      error = "tcp recv failed";
      return false;
    }
    mi::server::FrameType type;
    std::uint32_t payload_len = 0;
    if (!mi::server::DecodeFrameHeader(header, sizeof(header), type,
                                       payload_len)) {
      error = "tcp recv failed";
      return false;
    }
    (void)type;
    out_bytes.resize(mi::server::kFrameHeaderSize + payload_len);
    std::memcpy(out_bytes.data(), header, sizeof(header));
    if (payload_len > 0 &&
        !mi::platform::net::RecvExact(
            sock, out_bytes.data() + mi::server::kFrameHeaderSize,
            payload_len)) {
      error = "tcp recv failed";
      out_bytes.clear();
      return false;
    }
    return true;
  }
};

ClientCore::ClientCore() = default;

ClientCore::~ClientCore() {
  Logout();
  if (local_handle_) {
    mi_server_destroy(local_handle_);
    local_handle_ = nullptr;
  }
}

void TransportService::ResetRemoteStream(ClientCore& core) const {
  std::lock_guard<std::mutex> lock(core.remote_stream_mutex_);
  core.remote_stream_.reset();
}

bool TransportService::EnsureChannel(ClientCore& core) const {
  if (core.token_.empty()) {
    return false;
  }
  if (core.remote_mode_) {
    return !core.server_ip_.empty() && core.server_port_ != 0;
  }
  return core.local_handle_ != nullptr;
}

bool TransportService::ProcessRaw(ClientCore& core,
                                  const std::vector<std::uint8_t>& in_bytes,
                                  std::vector<std::uint8_t>& out_bytes) const {
  out_bytes.clear();
  if (in_bytes.empty()) {
    return false;
  }
  if (core.remote_mode_) {
    const auto set_remote_ok = [&](bool ok, const std::string& err) {
      core.remote_ok_ = ok;
      if (ok) {
        core.remote_error_.clear();
      } else {
        core.remote_error_ = err;
      }
    };

    std::lock_guard<std::mutex> lock(core.remote_stream_mutex_);
    if (!core.remote_stream_ ||
        !core.remote_stream_->Matches(core.server_ip_, core.server_port_,
                                      core.use_tls_, core.use_kcp_,
                                      core.kcp_cfg_, core.proxy_,
                                      core.pinned_server_fingerprint_)) {
      core.remote_stream_.reset();
      core.remote_stream_ = std::make_unique<ClientCore::RemoteStream>(
          core.server_ip_, core.server_port_, core.use_tls_, core.use_kcp_,
          core.kcp_cfg_, core.proxy_, core.pinned_server_fingerprint_);
      std::string fingerprint;
      std::string err;
      if (!core.remote_stream_->Connect(fingerprint, err)) {
        core.remote_stream_.reset();
        if (!fingerprint.empty()) {
          core.pending_server_fingerprint_ = fingerprint;
          core.pending_server_pin_ = FingerprintSas80Hex(fingerprint);
          core.last_error_ =
              core.pinned_server_fingerprint_.empty()
                  ? "server not trusted, confirm sas"
                  : "server fingerprint changed, confirm sas";
          set_remote_ok(false, core.last_error_);
          return false;
        }
        if (err.empty()) {
          if (core.use_kcp_) {
            core.last_error_ = "kcp connect failed";
          } else if (core.use_tls_) {
            core.last_error_ = "tls connect failed";
          } else {
            core.last_error_ = "tcp connect failed";
          }
        } else {
          core.last_error_ = err;
        }
        set_remote_ok(false, core.last_error_);
        return false;
      }
      core.pending_server_fingerprint_.clear();
      core.pending_server_pin_.clear();
    }

    std::string err;
    if (!core.remote_stream_->SendAndRecv(in_bytes, out_bytes, err)) {
      core.remote_stream_.reset();
      if (!err.empty()) {
        core.last_error_ = err;
      } else if (core.use_kcp_) {
        core.last_error_ = "kcp request failed";
      } else if (core.use_tls_) {
        core.last_error_ = "tls request failed";
      } else {
        core.last_error_ = "tcp request failed";
      }
      set_remote_ok(false, core.last_error_);
      return false;
    }
    set_remote_ok(true, {});
    return true;
  }
  core.remote_ok_ = true;
  core.remote_error_.clear();
  if (!core.local_handle_) {
    return false;
  }
  std::uint8_t* resp_buf = nullptr;
  std::size_t resp_len = 0;
  if (!mi_server_process(core.local_handle_, in_bytes.data(), in_bytes.size(),
                         &resp_buf, &resp_len)) {
    return false;
  }
  out_bytes.assign(resp_buf, resp_buf + resp_len);
  mi_server_free(resp_buf);
  return !out_bytes.empty();
}

bool TransportService::ProcessEncrypted(
    ClientCore& core, mi::server::FrameType type,
    const std::vector<std::uint8_t>& plain,
    std::vector<std::uint8_t>& out_plain) const {
  if (!EnsureChannel(core)) {
    return false;
  }
  std::vector<std::uint8_t> cipher;
  if (!core.channel_.Encrypt(core.send_seq_, type, plain, cipher)) {
    return false;
  }
  core.send_seq_++;

  mi::server::Frame f;
  f.type = type;
  f.payload.reserve(2 + core.token_.size() + cipher.size());
  mi::server::proto::WriteString(core.token_, f.payload);
  f.payload.insert(f.payload.end(), cipher.begin(), cipher.end());
  const auto bytes = mi::server::EncodeFrame(f);

  std::vector<std::uint8_t> resp_vec;
  if (!ProcessRaw(core, bytes, resp_vec)) {
    return false;
  }

  mi::server::FrameView resp_view;
  if (!mi::server::DecodeFrameView(resp_vec.data(), resp_vec.size(),
                                   resp_view)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "invalid server response";
    }
    return false;
  }
  const mi::server::proto::ByteView payload_view{resp_view.payload,
                                                 resp_view.payload_len};
  std::size_t off = 0;
  std::string_view resp_token;
  if (!mi::server::proto::ReadStringView(payload_view, off, resp_token)) {
    if (resp_view.type == mi::server::FrameType::kLogout) {
      std::string server_err;
      if (payload_view.data && payload_view.size > 1) {
        std::size_t off2 = 1;
        std::string_view err_view;
        if (mi::server::proto::ReadStringView(payload_view, off2, err_view)) {
          server_err.assign(err_view.begin(), err_view.end());
        }
      }
      core.last_error_ =
          server_err.empty() ? "session invalid" : server_err;
      core.token_.clear();
      core.prekey_published_ = false;
      return false;
    }
    if (core.last_error_.empty()) {
      core.last_error_ = "invalid server response";
    }
    return false;
  }
  if (!mi::common::ConstantTimeEqual(resp_token, core.token_)) {
    core.last_error_ = "session invalid";
    core.token_.clear();
    core.prekey_published_ = false;
    return false;
  }
  const std::size_t cipher_len =
      payload_view.size >= off ? payload_view.size - off : 0;
  const std::uint8_t* cipher_ptr =
      payload_view.data ? payload_view.data + off : nullptr;
  if (!core.channel_.Decrypt(cipher_ptr, cipher_len, resp_view.type,
                             out_plain)) {
    if (core.last_error_.empty()) {
      core.last_error_ = "decrypt failed";
    }
    return false;
  }
  return true;
}

}  // namespace mi::client
