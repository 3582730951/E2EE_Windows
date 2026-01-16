#include "platform_net.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <arpa/inet.h>
#include <fcntl.h>
#include <limits>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace mi::platform::net {

bool EnsureInitialized() {
  return true;
}

bool SetNonBlocking(Socket sock) {
  const int flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool SetRecvTimeout(Socket sock, std::uint32_t timeout_ms) {
  timeval tv{};
  tv.tv_sec = static_cast<long>(timeout_ms / 1000u);
  tv.tv_usec = static_cast<long>((timeout_ms % 1000u) * 1000u);
  return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv,
                    static_cast<socklen_t>(sizeof(tv))) == 0;
}

bool SetSendTimeout(Socket sock, std::uint32_t timeout_ms) {
  timeval tv{};
  tv.tv_sec = static_cast<long>(timeout_ms / 1000u);
  tv.tv_usec = static_cast<long>((timeout_ms % 1000u) * 1000u);
  return setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv,
                    static_cast<socklen_t>(sizeof(tv))) == 0;
}

bool WaitForReadable(Socket sock, std::uint32_t timeout_ms) {
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(sock, &readfds);
  timeval tv{};
  tv.tv_sec = static_cast<long>(timeout_ms / 1000u);
  tv.tv_usec = static_cast<long>((timeout_ms % 1000u) * 1000u);
  const int rc = select(sock + 1, &readfds, nullptr, nullptr, &tv);
  return rc > 0 && FD_ISSET(sock, &readfds);
}

bool SocketWouldBlock() {
  return errno == EAGAIN || errno == EWOULDBLOCK;
}

bool SendAll(Socket sock, const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return true;
  }
  std::size_t sent = 0;
  while (sent < len) {
    const std::size_t remaining = len - sent;
    const std::size_t chunk =
        std::min<std::size_t>(remaining,
                              static_cast<std::size_t>((std::numeric_limits<int>::max)()));
    const ssize_t n = ::send(sock, data + sent, chunk, 0);
    if (n <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

bool RecvSome(Socket sock, std::vector<std::uint8_t>& out) {
  std::uint8_t tmp[4096];
  const ssize_t n = ::recv(sock, tmp, sizeof(tmp), 0);
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
    const std::size_t chunk =
        std::min<std::size_t>(remaining,
                              static_cast<std::size_t>((std::numeric_limits<int>::max)()));
    const ssize_t n = ::recv(sock, data + got, chunk, 0);
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
  const std::size_t chunk =
      std::min<std::size_t>(len,
                            static_cast<std::size_t>((std::numeric_limits<int>::max)()));
  const ssize_t n = ::send(sock, data, chunk, 0);
  if (n < 0) {
    return -1;
  }
  return static_cast<int>(n);
}

int Recv(Socket sock, std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return 0;
  }
  const std::size_t chunk =
      std::min<std::size_t>(len,
                            static_cast<std::size_t>((std::numeric_limits<int>::max)()));
  const ssize_t n = ::recv(sock, data, chunk, 0);
  if (n < 0) {
    return -1;
  }
  return static_cast<int>(n);
}

int SendTo(Socket sock, const std::uint8_t* data, std::size_t len,
           const sockaddr* addr, socklen_t addr_len) {
  if (!data || len == 0 || !addr) {
    return 0;
  }
  const std::size_t chunk =
      std::min<std::size_t>(len,
                            static_cast<std::size_t>((std::numeric_limits<int>::max)()));
  const ssize_t n = ::sendto(sock, data, chunk, 0, addr, addr_len);
  if (n < 0) {
    return -1;
  }
  return static_cast<int>(n);
}

int RecvFrom(Socket sock, std::uint8_t* data, std::size_t len, sockaddr* addr,
             socklen_t* addr_len) {
  if (!data || len == 0) {
    return 0;
  }
  const std::size_t chunk =
      std::min<std::size_t>(len,
                            static_cast<std::size_t>((std::numeric_limits<int>::max)()));
  socklen_t local_len = addr_len ? *addr_len : 0;
  socklen_t* len_ptr = addr_len ? &local_len : nullptr;
  const ssize_t n = ::recvfrom(sock, data, chunk, 0, addr, len_ptr);
  if (addr_len && len_ptr) {
    *addr_len = local_len;
  }
  if (n < 0) {
    return -1;
  }
  return static_cast<int>(n);
}

bool ConnectTcp(const std::string& host, std::uint16_t port, Socket& out,
                std::string& error) {
  out = kInvalidSocket;
  error.clear();
  if (host.empty() || port == 0) {
    error = "invalid endpoint";
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
    if (sock < 0) {
      continue;
    }
    if (::connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
      out = sock;
      break;
    }
    ::close(sock);
  }
  freeaddrinfo(result);

  if (out < 0) {
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
    if (sock < 0) {
      continue;
    }
    if (::connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
      out = sock;
      break;
    }
    ::close(sock);
  }
  freeaddrinfo(result);

  if (out < 0) {
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

  Socket sock = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    error = "udp socket failed";
    return false;
  }

  int yes = 1;
  (void)::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    error = "udp bind failed: " + std::to_string(errno) + " " +
            std::string(std::strerror(errno));
    ::close(sock);
    return false;
  }

  if (!SetNonBlocking(sock)) {
    error = "udp non-blocking failed";
    ::close(sock);
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
  Socket sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    error = "tcp socket failed: " + std::to_string(errno) + " " +
            std::string(std::strerror(errno));
    return false;
  }
  int yes = 1;
  ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    error = "bind(0.0.0.0:" + std::to_string(port) + ") failed: " +
            std::to_string(errno) + " " + std::string(std::strerror(errno));
    ::close(sock);
    return false;
  }
  if (::listen(sock, 8) < 0) {
    error = "listen(0.0.0.0:" + std::to_string(port) + ") failed: " +
            std::to_string(errno) + " " + std::string(std::strerror(errno));
    ::close(sock);
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
  socklen_t len = sizeof(cli);
  const int client =
      ::accept(listen_sock, reinterpret_cast<sockaddr*>(&cli), &len);
  if (client < 0) {
    error = "accept failed";
    return false;
  }
  char ip_buf[64] = {};
  const char* ip_ptr = inet_ntop(AF_INET, &cli.sin_addr, ip_buf,
                                 sizeof(ip_buf));
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
    const char* ip_ptr = inet_ntop(AF_INET, &in->sin_addr, ip_buf,
                                   sizeof(ip_buf));
    if (!ip_ptr) {
      return false;
    }
    out.assign(ip_ptr);
    return true;
  }
  if (addr->sa_family == AF_INET6) {
    const auto* in6 = reinterpret_cast<const sockaddr_in6*>(addr);
    char ip_buf[64] = {};
    const char* ip_ptr = inet_ntop(AF_INET6, &in6->sin6_addr, ip_buf,
                                   sizeof(ip_buf));
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
      static_cast<std::size_t>((std::numeric_limits<nfds_t>::max)())) {
    return -1;
  }
  std::vector<pollfd> native;
  native.resize(count);
  for (std::size_t i = 0; i < count; ++i) {
    native[i].fd = fds[i].sock;
    native[i].events = 0;
    if (fds[i].events & kPollIn) {
      native[i].events |= POLLIN;
    }
    if (fds[i].events & kPollOut) {
      native[i].events |= POLLOUT;
    }
    native[i].revents = 0;
  }
  const int rc =
      ::poll(native.data(), static_cast<nfds_t>(count),
             static_cast<int>(timeout_ms));
  if (rc <= 0) {
    for (std::size_t i = 0; i < count; ++i) {
      fds[i].revents = 0;
    }
    return rc;
  }
  for (std::size_t i = 0; i < count; ++i) {
    short out = 0;
    if (native[i].revents & POLLIN) {
      out |= kPollIn;
    }
    if (native[i].revents & POLLOUT) {
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
  return shutdown(sock, SHUT_WR) == 0;
}

void CloseSocket(Socket& sock) {
  if (sock >= 0) {
    ::close(sock);
    sock = kInvalidSocket;
  }
}

}  // namespace mi::platform::net
