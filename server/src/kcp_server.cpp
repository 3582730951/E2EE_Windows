#include "kcp_server.h"

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "crypto.h"
#include "ikcp.h"

namespace mi::server {

namespace {

constexpr std::uint32_t kTickMsMin = 5;
constexpr std::uint32_t kTickMsMax = 50;
constexpr std::uint8_t kKcpCookieCmd = 0xFF;
constexpr std::uint8_t kKcpCookieHello = 1;
constexpr std::uint8_t kKcpCookieChallenge = 2;
constexpr std::uint8_t kKcpCookieResponse = 3;
constexpr std::uint32_t kKcpCookieWindowMs = 30000;
constexpr std::size_t kKcpCookieBytes = 16;
constexpr std::size_t kKcpCookiePacketBytes = 24;

std::uint32_t NowMs() {
  static const auto kStart = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();
  return static_cast<std::uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now - kStart)
          .count());
}

std::string IpToString(const sockaddr_storage& addr) {
  char buf[64] = {};
  if (addr.ss_family == AF_INET) {
    const auto* in = reinterpret_cast<const sockaddr_in*>(&addr);
#ifdef _WIN32
    if (InetNtopA(AF_INET, const_cast<IN_ADDR*>(&in->sin_addr), buf,
                  static_cast<DWORD>(sizeof(buf)))) {
      return buf;
    }
#else
    if (inet_ntop(AF_INET, &in->sin_addr, buf, sizeof(buf))) {
      return buf;
    }
#endif
  }
  return {};
}

void WriteLe32(std::uint32_t v, std::uint8_t out[4]) {
  out[0] = static_cast<std::uint8_t>(v & 0xFF);
  out[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
  out[2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
  out[3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
}

std::uint32_t ReadLe32(const std::uint8_t in[4]) {
  return static_cast<std::uint32_t>(in[0]) |
         (static_cast<std::uint32_t>(in[1]) << 8) |
         (static_cast<std::uint32_t>(in[2]) << 16) |
         (static_cast<std::uint32_t>(in[3]) << 24);
}

void WriteLe64(std::uint64_t v, std::uint8_t out[8]) {
  for (int i = 0; i < 8; ++i) {
    out[i] = static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF);
  }
}

struct CookiePacket {
  std::uint32_t conv{0};
  std::uint8_t type{0};
  std::array<std::uint8_t, kKcpCookieBytes> cookie{};
};

bool ParseCookiePacket(const std::uint8_t* data, std::size_t len,
                       CookiePacket& out) {
  if (!data || len < kKcpCookiePacketBytes) {
    return false;
  }
  if (data[4] != kKcpCookieCmd) {
    return false;
  }
  out.conv = ReadLe32(data);
  out.type = data[5];
  std::memcpy(out.cookie.data(), data + 8, out.cookie.size());
  return true;
}

void BuildCookiePacket(std::uint32_t conv, std::uint8_t type,
                       const std::array<std::uint8_t, kKcpCookieBytes>& cookie,
                       std::array<std::uint8_t, kKcpCookiePacketBytes>& out) {
  WriteLe32(conv, out.data());
  out[4] = kKcpCookieCmd;
  out[5] = type;
  out[6] = 0;
  out[7] = 0;
  std::memcpy(out.data() + 8, cookie.data(), cookie.size());
}

bool ConstantTimeEqual(const std::array<std::uint8_t, kKcpCookieBytes>& a,
                       const std::array<std::uint8_t, kKcpCookieBytes>& b) {
  std::uint8_t acc = 0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    acc |= static_cast<std::uint8_t>(a[i] ^ b[i]);
  }
  return acc == 0;
}

std::array<std::uint8_t, kKcpCookieBytes> BuildCookie(
    const std::array<std::uint8_t, 32>& secret,
    const sockaddr_storage& addr, socklen_t addr_len, std::uint32_t conv,
    std::uint64_t bucket) {
  std::vector<std::uint8_t> buf;
  buf.reserve(secret.size() + static_cast<std::size_t>(addr_len) + 12);
  buf.insert(buf.end(), secret.begin(), secret.end());
  const auto* addr_bytes =
      reinterpret_cast<const std::uint8_t*>(&addr);
  buf.insert(buf.end(), addr_bytes,
             addr_bytes + static_cast<std::size_t>(addr_len));
  std::uint8_t le32[4] = {};
  WriteLe32(conv, le32);
  buf.insert(buf.end(), le32, le32 + sizeof(le32));
  std::uint8_t le64[8] = {};
  WriteLe64(bucket, le64);
  buf.insert(buf.end(), le64, le64 + sizeof(le64));

  mi::server::crypto::Sha256Digest digest;
  mi::server::crypto::Sha256(buf.data(), buf.size(), digest);
  std::array<std::uint8_t, kKcpCookieBytes> out{};
  std::copy_n(digest.bytes.begin(), out.size(), out.begin());
  return out;
}

std::string EndpointToString(const sockaddr_storage& addr) {
  if (addr.ss_family == AF_INET) {
    const auto* in = reinterpret_cast<const sockaddr_in*>(&addr);
    std::string ip = IpToString(addr);
    if (!ip.empty()) {
      const std::uint16_t port = ntohs(in->sin_port);
      ip += ":" + std::to_string(port);
    }
    return ip;
  }
  return {};
}

bool SetNonBlocking(std::intptr_t sock) {
#ifdef _WIN32
  u_long mode = 1;
  return ioctlsocket(static_cast<SOCKET>(sock), FIONBIO, &mode) == 0;
#else
  int flags = fcntl(static_cast<int>(sock), F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(static_cast<int>(sock), F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool WouldBlock() {
#ifdef _WIN32
  const int err = WSAGetLastError();
  return err == WSAEWOULDBLOCK;
#else
  return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

#ifdef _WIN32
std::string Win32ErrorMessage(DWORD code) {
  LPSTR msg = nullptr;
  const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS;
  const DWORD n = FormatMessageA(
      flags, nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPSTR>(&msg), 0, nullptr);
  std::string out;
  if (n && msg) {
    out.assign(msg, msg + n);
  }
  if (msg) {
    LocalFree(msg);
  }
  while (!out.empty() && (out.back() == '\r' || out.back() == '\n')) {
    out.pop_back();
  }
  return out;
}
#endif

struct KcpSession {
  ikcpcb* kcp{nullptr};
  std::uint32_t conv{0};
  std::intptr_t sock{-1};
  sockaddr_storage addr{};
  socklen_t addr_len{0};
  std::string remote_ip;
  std::string remote_endpoint;
  std::uint64_t last_active_ms{0};
  std::uint64_t bytes_total{0};
};

int KcpOutput(const char* buf, int len, ikcpcb* /*kcp*/, void* user) {
  if (!buf || len <= 0 || !user) {
    return -1;
  }
  auto* sess = static_cast<KcpSession*>(user);
  const auto* data = reinterpret_cast<const std::uint8_t*>(buf);
#ifdef _WIN32
  const int sent = sendto(static_cast<SOCKET>(sess->sock),
                          reinterpret_cast<const char*>(data), len, 0,
                          reinterpret_cast<const sockaddr*>(&sess->addr),
                          sess->addr_len);
  return sent == len ? 0 : -1;
#else
  const ssize_t sent = sendto(static_cast<int>(sess->sock), data,
                              static_cast<std::size_t>(len), 0,
                              reinterpret_cast<const sockaddr*>(&sess->addr),
                              sess->addr_len);
  return sent == len ? 0 : -1;
#endif
}

}  // namespace

KcpServer::KcpServer(Listener* listener, std::uint16_t port, KcpOptions options,
                     NetworkServerLimits limits)
    : listener_(listener),
      port_(port),
      options_(options),
      limits_(limits) {}

KcpServer::~KcpServer() { Stop(); }

bool KcpServer::Start(std::string& error) {
  error.clear();
  if (running_.load()) {
    return true;
  }
  if (!listener_ || port_ == 0) {
    error = "invalid listener/port";
    return false;
  }
  if (!InitCookieSecret(error)) {
    return false;
  }
  if (!StartSocket(error)) {
    return false;
  }
  running_.store(true);
  worker_ = std::thread(&KcpServer::Run, this);
  return true;
}

void KcpServer::Stop() {
  running_.store(false);
  StopSocket();
  if (worker_.joinable()) {
    worker_.join();
  }
}

bool KcpServer::StartSocket(std::string& error) {
  error.clear();
#ifdef _WIN32
  WSADATA wsa;
  const int wsa_rc = WSAStartup(MAKEWORD(2, 2), &wsa);
  if (wsa_rc != 0) {
    error = "WSAStartup failed: " + std::to_string(wsa_rc) + " " +
            Win32ErrorMessage(static_cast<DWORD>(wsa_rc));
    return false;
  }
#endif
#ifdef _WIN32
  const SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    const DWORD last = WSAGetLastError();
    error = "socket(AF_INET,SOCK_DGRAM) failed: " + std::to_string(last) +
            " " + Win32ErrorMessage(last);
    WSACleanup();
    return false;
  }
  sock_ = static_cast<std::intptr_t>(sock);
#else
  const int sock = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    const int last = errno;
    error = "socket(AF_INET,SOCK_DGRAM) failed: " + std::to_string(last) +
            " " + std::strerror(last);
    return false;
  }
  sock_ = static_cast<std::intptr_t>(sock);
#endif

  int yes = 1;
#ifdef _WIN32
  setsockopt(static_cast<SOCKET>(sock_), SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
  ::setsockopt(static_cast<int>(sock_), SOL_SOCKET, SO_REUSEADDR, &yes,
               sizeof(yes));
#endif

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
#ifdef _WIN32
  if (::bind(static_cast<SOCKET>(sock_), reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) == SOCKET_ERROR) {
    const DWORD last = WSAGetLastError();
    error = "bind(0.0.0.0:" + std::to_string(port_) + ") failed: " +
            std::to_string(last) + " " + Win32ErrorMessage(last);
    StopSocket();
    return false;
  }
#else
  if (::bind(static_cast<int>(sock_), reinterpret_cast<sockaddr*>(&addr),
             sizeof(addr)) < 0) {
    const int last = errno;
    error = "bind(0.0.0.0:" + std::to_string(port_) + ") failed: " +
            std::to_string(last) + " " + std::strerror(last);
    StopSocket();
    return false;
  }
#endif

  if (!SetNonBlocking(sock_)) {
    error = "set non-blocking failed";
    StopSocket();
    return false;
  }
  return true;
}

void KcpServer::StopSocket() {
  if (sock_ != -1) {
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(sock_));
    WSACleanup();
#else
    ::close(static_cast<int>(sock_));
#endif
    sock_ = -1;
  }
}

bool KcpServer::InitCookieSecret(std::string& error) {
  error.clear();
  if (cookie_ready_) {
    return true;
  }
  if (!mi::server::crypto::RandomBytes(cookie_secret_.data(),
                                       cookie_secret_.size())) {
    error = "kcp cookie rng failed";
    return false;
  }
  cookie_ready_ = true;
  return true;
}

bool KcpServer::TryAcquireConnectionSlot(const std::string& remote_ip) {
  std::lock_guard<std::mutex> lock(conn_mutex_);
  if (active_connections_ >= limits_.max_connections) {
    return false;
  }
  if (!remote_ip.empty()) {
    const auto it = connections_by_ip_.find(remote_ip);
    const std::uint32_t current =
        it == connections_by_ip_.end() ? 0u : it->second;
    if (current >= limits_.max_connections_per_ip) {
      return false;
    }
    if (it == connections_by_ip_.end()) {
      connections_by_ip_.emplace(remote_ip, 1u);
    } else {
      it->second++;
    }
  }
  active_connections_++;
  return true;
}

void KcpServer::ReleaseConnectionSlot(const std::string& remote_ip) {
  std::lock_guard<std::mutex> lock(conn_mutex_);
  if (active_connections_ > 0) {
    active_connections_--;
  }
  if (remote_ip.empty()) {
    return;
  }
  const auto it = connections_by_ip_.find(remote_ip);
  if (it == connections_by_ip_.end()) {
    return;
  }
  if (it->second <= 1) {
    connections_by_ip_.erase(it);
  } else {
    it->second--;
  }
}

void KcpServer::Run() {
  std::unordered_map<std::uint32_t, std::unique_ptr<KcpSession>> sessions;
  const std::uint32_t tick_ms =
      std::max(kTickMsMin, std::min(kTickMsMax, options_.interval));
  const std::uint64_t idle_ms =
      std::max<std::uint64_t>(1000u,
                              static_cast<std::uint64_t>(
                                  options_.session_idle_sec) * 1000u);

  std::vector<std::uint8_t> recv_buf;
  recv_buf.resize(std::max<std::uint32_t>(options_.mtu, 1200u) + 256u);

  while (running_.load()) {
    fd_set readfds;
    FD_ZERO(&readfds);
#ifdef _WIN32
    FD_SET(static_cast<SOCKET>(sock_), &readfds);
    TIMEVAL tv{};
    tv.tv_sec = 0;
    tv.tv_usec = static_cast<long>(tick_ms) * 1000;
    select(0, &readfds, nullptr, nullptr, &tv);
#else
    FD_SET(static_cast<int>(sock_), &readfds);
    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = static_cast<long>(tick_ms) * 1000;
    select(static_cast<int>(sock_) + 1, &readfds, nullptr, nullptr, &tv);
#endif

    const std::uint32_t now = NowMs();

    while (running_.load()) {
      sockaddr_storage peer_addr{};
      socklen_t peer_len = sizeof(peer_addr);
#ifdef _WIN32
      const int n = recvfrom(static_cast<SOCKET>(sock_),
                             reinterpret_cast<char*>(recv_buf.data()),
                             static_cast<int>(recv_buf.size()), 0,
                             reinterpret_cast<sockaddr*>(&peer_addr),
                             &peer_len);
      if (n == SOCKET_ERROR) {
        if (WouldBlock()) {
          break;
        }
        continue;
      }
#else
      const ssize_t n = recvfrom(static_cast<int>(sock_), recv_buf.data(),
                                 recv_buf.size(), 0,
                                 reinterpret_cast<sockaddr*>(&peer_addr),
                                 &peer_len);
      if (n < 0) {
        if (WouldBlock()) {
          break;
        }
        continue;
      }
#endif
      if (n <= 0) {
        break;
      }
      if (static_cast<std::size_t>(n) < sizeof(IUINT32)) {
        continue;
      }

      const IUINT32 conv = ikcp_getconv(recv_buf.data());
      auto it = sessions.find(conv);
      if (it == sessions.end()) {
        CookiePacket cookie_pkt{};
        if (!ParseCookiePacket(recv_buf.data(), static_cast<std::size_t>(n),
                               cookie_pkt) ||
            cookie_pkt.conv != conv) {
          continue;
        }
        if (cookie_pkt.type == kKcpCookieHello) {
          const std::uint64_t bucket =
              static_cast<std::uint64_t>(NowMs() / kKcpCookieWindowMs);
          const auto cookie =
              BuildCookie(cookie_secret_, peer_addr, peer_len, conv, bucket);
          std::array<std::uint8_t, kKcpCookiePacketBytes> out{};
          BuildCookiePacket(conv, kKcpCookieChallenge, cookie, out);
#ifdef _WIN32
          sendto(static_cast<SOCKET>(sock_),
                 reinterpret_cast<const char*>(out.data()),
                 static_cast<int>(out.size()), 0,
                 reinterpret_cast<const sockaddr*>(&peer_addr), peer_len);
#else
          sendto(static_cast<int>(sock_), out.data(), out.size(), 0,
                 reinterpret_cast<const sockaddr*>(&peer_addr), peer_len);
#endif
          continue;
        }
        if (cookie_pkt.type != kKcpCookieResponse) {
          continue;
        }
        const std::uint64_t bucket =
            static_cast<std::uint64_t>(NowMs() / kKcpCookieWindowMs);
        const auto cookie_now =
            BuildCookie(cookie_secret_, peer_addr, peer_len, conv, bucket);
        const auto cookie_prev =
            bucket > 0 ? BuildCookie(cookie_secret_, peer_addr, peer_len, conv,
                                     bucket - 1)
                       : cookie_now;
        if (!ConstantTimeEqual(cookie_pkt.cookie, cookie_now) &&
            !ConstantTimeEqual(cookie_pkt.cookie, cookie_prev)) {
          continue;
        }

        const std::string remote_ip = IpToString(peer_addr);
        const std::string remote_endpoint = EndpointToString(peer_addr);
        if (!TryAcquireConnectionSlot(remote_ip)) {
          continue;
        }
        auto sess = std::make_unique<KcpSession>();
        sess->conv = conv;
        sess->sock = sock_;
        sess->addr = peer_addr;
        sess->addr_len = peer_len;
        sess->remote_ip = remote_ip;
        sess->remote_endpoint = remote_endpoint;
        sess->last_active_ms = now;
        sess->bytes_total = 0;
        sess->kcp = ikcp_create(conv, sess.get());
        if (!sess->kcp) {
          ReleaseConnectionSlot(remote_ip);
          continue;
        }
        sess->kcp->output = KcpOutput;
        ikcp_setmtu(sess->kcp, static_cast<int>(options_.mtu));
        ikcp_wndsize(sess->kcp, static_cast<int>(options_.snd_wnd),
                     static_cast<int>(options_.rcv_wnd));
        ikcp_nodelay(sess->kcp, static_cast<int>(options_.nodelay),
                     static_cast<int>(options_.interval),
                     static_cast<int>(options_.resend),
                     static_cast<int>(options_.nc));
        if (options_.min_rto > 0) {
          sess->kcp->rx_minrto = static_cast<int>(options_.min_rto);
        }
        it = sessions.emplace(conv, std::move(sess)).first;
        continue;
      } else {
        const std::string remote_endpoint = EndpointToString(peer_addr);
        if (!remote_endpoint.empty() &&
            it->second->remote_endpoint != remote_endpoint) {
          continue;
        }
      }

      auto* sess = it->second.get();
      sess->last_active_ms = now;
      sess->bytes_total += static_cast<std::uint64_t>(n);
      if (sess->bytes_total > limits_.max_connection_bytes) {
        ReleaseConnectionSlot(sess->remote_ip);
        ikcp_release(sess->kcp);
        sessions.erase(it);
        continue;
      }
      ikcp_input(sess->kcp, reinterpret_cast<const char*>(recv_buf.data()),
                 n);
    }

    for (auto it = sessions.begin(); it != sessions.end();) {
      std::vector<std::uint8_t> request;
      std::vector<std::uint8_t> response;
      auto* sess = it->second.get();
      ikcp_update(sess->kcp, now);

      bool drop = false;
      for (;;) {
        const int peek = ikcp_peeksize(sess->kcp);
        if (peek <= 0) {
          break;
        }
        request.resize(static_cast<std::size_t>(peek));
        const int n = ikcp_recv(sess->kcp,
                                reinterpret_cast<char*>(request.data()),
                                peek);
        if (n <= 0) {
          break;
        }
        request.resize(static_cast<std::size_t>(n));

        sess->bytes_total += request.size();
        if (sess->bytes_total > limits_.max_connection_bytes) {
          drop = true;
          break;
        }

        response.clear();
        if (!listener_->Process(request, response, sess->remote_ip,
                                TransportKind::kKcp)) {
          drop = true;
          break;
        }
        sess->last_active_ms = now;
        sess->bytes_total += response.size();
        if (sess->bytes_total > limits_.max_connection_bytes) {
          drop = true;
          break;
        }
        if (!response.empty()) {
          ikcp_send(sess->kcp,
                    reinterpret_cast<const char*>(response.data()),
                    static_cast<int>(response.size()));
          ikcp_flush(sess->kcp);
        }
      }

      const std::uint64_t idle_for = now - sess->last_active_ms;
      if (drop || idle_for > idle_ms) {
        ReleaseConnectionSlot(sess->remote_ip);
        ikcp_release(sess->kcp);
        it = sessions.erase(it);
        continue;
      }
      ++it;
    }
  }

  for (auto& entry : sessions) {
    ReleaseConnectionSlot(entry.second->remote_ip);
    ikcp_release(entry.second->kcp);
  }
  sessions.clear();
}

}  // namespace mi::server
