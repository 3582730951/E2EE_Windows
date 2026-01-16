#include "platform_net.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <vector>

namespace mi::platform::net {

bool EnsureInitialized() {
  static std::once_flag once;
  static int status = -1;
  std::call_once(once, []() {
    WSADATA wsa;
    status = WSAStartup(MAKEWORD(2, 2), &wsa);
  });
  return status == 0;
}

bool SetNonBlocking(Socket sock) {
  u_long mode = 1;
  return ioctlsocket(sock, FIONBIO, &mode) == 0;
}

bool SetRecvTimeout(Socket sock, std::uint32_t timeout_ms) {
  const DWORD timeout = static_cast<DWORD>(timeout_ms);
  return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                    reinterpret_cast<const char*>(&timeout),
                    static_cast<int>(sizeof(timeout))) == 0;
}

bool SetSendTimeout(Socket sock, std::uint32_t timeout_ms) {
  const DWORD timeout = static_cast<DWORD>(timeout_ms);
  return setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
                    reinterpret_cast<const char*>(&timeout),
                    static_cast<int>(sizeof(timeout))) == 0;
}

bool WaitForReadable(Socket sock, std::uint32_t timeout_ms) {
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(sock, &readfds);
  timeval tv{};
  tv.tv_sec = static_cast<long>(timeout_ms / 1000u);
  tv.tv_usec = static_cast<long>((timeout_ms % 1000u) * 1000u);
  const int rc = select(0, &readfds, nullptr, nullptr, &tv);
  return rc > 0 && FD_ISSET(sock, &readfds);
}

bool SocketWouldBlock() {
  const int err = WSAGetLastError();
  return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
}

bool SendAll(Socket sock, const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return true;
  }
  std::size_t sent = 0;
  while (sent < len) {
    const std::size_t remaining = len - sent;
    const int chunk =
        remaining >
                static_cast<std::size_t>((std::numeric_limits<int>::max)())
            ? (std::numeric_limits<int>::max)()
            : static_cast<int>(remaining);
    const int n = ::send(sock, reinterpret_cast<const char*>(data + sent),
                         chunk, 0);
    if (n <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

bool RecvSome(Socket sock, std::vector<std::uint8_t>& out) {
  std::uint8_t tmp[4096];
  const int n =
      ::recv(sock, reinterpret_cast<char*>(tmp), static_cast<int>(sizeof(tmp)),
             0);
  if (n <= 0) {
    return false;
  }
  out.insert(out.end(), tmp, tmp + n);
  return true;
}

bool RecvExact(Socket sock, std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return true;
  }
  std::size_t got = 0;
  while (got < len) {
    const std::size_t remaining = len - got;
    const int chunk =
        remaining >
                static_cast<std::size_t>((std::numeric_limits<int>::max)())
            ? (std::numeric_limits<int>::max)()
            : static_cast<int>(remaining);
    const int n =
        ::recv(sock, reinterpret_cast<char*>(data + got), chunk, 0);
    if (n <= 0) {
      return false;
    }
    got += static_cast<std::size_t>(n);
  }
  return true;
}

int Send(Socket sock, const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return 0;
  }
  const int chunk =
      len > static_cast<std::size_t>((std::numeric_limits<int>::max)())
          ? (std::numeric_limits<int>::max)()
          : static_cast<int>(len);
  return ::send(sock, reinterpret_cast<const char*>(data), chunk, 0);
}

int Recv(Socket sock, std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return 0;
  }
  const int chunk =
      len > static_cast<std::size_t>((std::numeric_limits<int>::max)())
          ? (std::numeric_limits<int>::max)()
          : static_cast<int>(len);
  return ::recv(sock, reinterpret_cast<char*>(data), chunk, 0);
}

int SendTo(Socket sock, const std::uint8_t* data, std::size_t len,
           const sockaddr* addr, socklen_t addr_len) {
  if (!data || len == 0 || !addr) {
    return 0;
  }
  const int chunk =
      len > static_cast<std::size_t>((std::numeric_limits<int>::max)())
          ? (std::numeric_limits<int>::max)()
          : static_cast<int>(len);
  return ::sendto(sock, reinterpret_cast<const char*>(data), chunk, 0, addr,
                  static_cast<int>(addr_len));
}

int RecvFrom(Socket sock, std::uint8_t* data, std::size_t len, sockaddr* addr,
             socklen_t* addr_len) {
  if (!data || len == 0) {
    return 0;
  }
  const int chunk =
      len > static_cast<std::size_t>((std::numeric_limits<int>::max)())
          ? (std::numeric_limits<int>::max)()
          : static_cast<int>(len);
  int addr_len_int = addr_len ? static_cast<int>(*addr_len) : 0;
  int* addr_len_ptr = addr_len ? &addr_len_int : nullptr;
  const int n = ::recvfrom(sock, reinterpret_cast<char*>(data), chunk, 0, addr,
                           addr_len_ptr);
  if (addr_len && addr_len_ptr) {
    *addr_len = static_cast<socklen_t>(addr_len_int);
  }
  return n;
}

bool ConnectTcp(const std::string& host, std::uint16_t port, Socket& out,
                std::string& error) {
  out = kInvalidSocket;
  error.clear();
  if (host.empty() || port == 0) {
    error = "invalid endpoint";
    return false;
  }
  if (!EnsureInitialized()) {
    error = "winsock init failed";
    return false;
  }

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* result = nullptr;
  const std::string port_str = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0) {
    error = "dns resolve failed";
    return false;
  }

  for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    Socket sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == kInvalidSocket) {
      continue;
    }
    if (::connect(sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) {
      out = sock;
      break;
    }
    closesocket(sock);
  }
  freeaddrinfo(result);

  if (out == kInvalidSocket) {
    error = "connect failed";
    return false;
  }
  return true;
}

bool ConnectUdp(const std::string& host, std::uint16_t port, Socket& out,
                std::string& error) {
  out = kInvalidSocket;
  error.clear();
  if (host.empty() || port == 0) {
    error = "invalid endpoint";
    return false;
  }
  if (!EnsureInitialized()) {
    error = "winsock init failed";
    return false;
  }

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = IPPROTO_UDP;

  addrinfo* result = nullptr;
  const std::string port_str = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0) {
    error = "dns resolve failed";
    return false;
  }

  for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    Socket sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == kInvalidSocket) {
      continue;
    }
    if (::connect(sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) {
      out = sock;
      break;
    }
    closesocket(sock);
  }
  freeaddrinfo(result);

  if (out == kInvalidSocket) {
    error = "connect failed";
    return false;
  }
  return true;
}

bool BindUdpSocket(std::uint16_t port, Socket& out, std::string& error) {
  out = kInvalidSocket;
  error.clear();
  if (port == 0) {
    error = "invalid endpoint";
    return false;
  }
  if (!EnsureInitialized()) {
    error = "winsock init failed";
    return false;
  }

  Socket sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == kInvalidSocket) {
    error = "udp socket failed";
    return false;
  }

  int yes = 1;
  (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&yes), sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) ==
      SOCKET_ERROR) {
    error = "udp bind failed: " + std::to_string(WSAGetLastError());
    closesocket(sock);
    return false;
  }

  if (!SetNonBlocking(sock)) {
    error = "udp non-blocking failed";
    closesocket(sock);
    return false;
  }

  out = sock;
  return true;
}

bool CreateTcpListener(std::uint16_t port, Socket& out, std::string& error) {
  out = kInvalidSocket;
  error.clear();
  if (port == 0) {
    error = "invalid endpoint";
    return false;
  }
  if (!EnsureInitialized()) {
    error = "winsock init failed";
    return false;
  }
  Socket sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock == kInvalidSocket) {
    error = "tcp socket failed: " + std::to_string(WSAGetLastError());
    return false;
  }
  int yes = 1;
  (void)setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char*>(&yes), sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) ==
      SOCKET_ERROR) {
    error = "bind(0.0.0.0:" + std::to_string(port) +
            ") failed: " + std::to_string(WSAGetLastError());
    closesocket(sock);
    return false;
  }
  if (::listen(sock, 8) == SOCKET_ERROR) {
    error = "listen(0.0.0.0:" + std::to_string(port) +
            ") failed: " + std::to_string(WSAGetLastError());
    closesocket(sock);
    return false;
  }
  out = sock;
  return true;
}

bool AcceptTcp(Socket listen_sock, Socket& out, std::string& remote_ip,
               std::string& error) {
  out = kInvalidSocket;
  remote_ip.clear();
  error.clear();
  sockaddr_in cli{};
  int len = sizeof(cli);
  SOCKET client =
      ::accept(listen_sock, reinterpret_cast<sockaddr*>(&cli), &len);
  if (client == INVALID_SOCKET) {
    error = "accept failed";
    return false;
  }
  char ip_buf[64] = {};
  const char* ip_ptr =
      InetNtopA(AF_INET, const_cast<IN_ADDR*>(&cli.sin_addr), ip_buf,
                static_cast<DWORD>(sizeof(ip_buf)));
  if (ip_ptr) {
    remote_ip.assign(ip_ptr);
  }
  out = client;
  return true;
}

bool SockaddrToIp(const sockaddr* addr, socklen_t addr_len, std::string& out) {
  out.clear();
  if (!addr || addr_len == 0) {
    return false;
  }
  if (addr->sa_family == AF_INET) {
    const auto* in = reinterpret_cast<const sockaddr_in*>(addr);
    char ip_buf[64] = {};
    const char* ip_ptr =
        InetNtopA(AF_INET, const_cast<IN_ADDR*>(&in->sin_addr), ip_buf,
                  static_cast<DWORD>(sizeof(ip_buf)));
    if (!ip_ptr) {
      return false;
    }
    out.assign(ip_ptr);
    return true;
  }
  if (addr->sa_family == AF_INET6) {
    const auto* in6 = reinterpret_cast<const sockaddr_in6*>(addr);
    char ip_buf[64] = {};
    const char* ip_ptr =
        InetNtopA(AF_INET6, const_cast<IN6_ADDR*>(&in6->sin6_addr), ip_buf,
                  static_cast<DWORD>(sizeof(ip_buf)));
    if (!ip_ptr) {
      return false;
    }
    out.assign(ip_ptr);
    return true;
  }
  return false;
}

bool SockaddrToEndpoint(const sockaddr* addr, socklen_t addr_len,
                        std::string& out) {
  out.clear();
  std::string ip;
  if (!SockaddrToIp(addr, addr_len, ip)) {
    return false;
  }
  std::uint16_t port = 0;
  if (addr->sa_family == AF_INET) {
    const auto* in = reinterpret_cast<const sockaddr_in*>(addr);
    port = ntohs(in->sin_port);
  } else if (addr->sa_family == AF_INET6) {
    const auto* in6 = reinterpret_cast<const sockaddr_in6*>(addr);
    port = ntohs(in6->sin6_port);
  } else {
    return false;
  }
  out = ip + ":" + std::to_string(port);
  return true;
}

int Poll(PollFd* fds, std::size_t count, std::uint32_t timeout_ms) {
  if (!fds || count == 0) {
    return 0;
  }
  if (count >
      static_cast<std::size_t>((std::numeric_limits<ULONG>::max)())) {
    return -1;
  }
  std::vector<WSAPOLLFD> native;
  native.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    native[i].fd = fds[i].sock;
    native[i].events = 0;
    if (fds[i].events & kPollIn) {
      native[i].events |= POLLRDNORM;
    }
    if (fds[i].events & kPollOut) {
      native[i].events |= POLLWRNORM;
    }
    native[i].revents = 0;
  }
  const int rc = WSAPoll(native.data(), static_cast<ULONG>(count),
                         static_cast<INT>(timeout_ms));
  if (rc <= 0) {
    for (std::size_t i = 0; i < count; ++i) {
      fds[i].revents = 0;
    }
    return rc;
  }
  for (std::size_t i = 0; i < count; ++i) {
    short out = 0;
    if (native[i].revents & (POLLIN | POLLRDNORM)) {
      out |= kPollIn;
    }
    if (native[i].revents & (POLLOUT | POLLWRNORM)) {
      out |= kPollOut;
    }
    if (native[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
      out |= kPollErr;
    }
    fds[i].revents = out;
  }
  return rc;
}

bool ShutdownSend(Socket sock) {
  return shutdown(sock, SD_SEND) == 0;
}

void CloseSocket(Socket& sock) {
  if (sock != kInvalidSocket) {
    closesocket(sock);
    sock = kInvalidSocket;
  }
}

}  // namespace mi::platform::net
