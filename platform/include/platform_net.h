#ifndef MI_E2EE_PLATFORM_NET_H
#define MI_E2EE_PLATFORM_NET_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif

namespace mi::platform::net {

#ifdef _WIN32
using Socket = SOCKET;
constexpr Socket kInvalidSocket = INVALID_SOCKET;
#else
using Socket = int;
constexpr Socket kInvalidSocket = -1;
#endif

bool EnsureInitialized();
bool SetNonBlocking(Socket sock);
bool SetRecvTimeout(Socket sock, std::uint32_t timeout_ms);
bool SetSendTimeout(Socket sock, std::uint32_t timeout_ms);
bool WaitForReadable(Socket sock, std::uint32_t timeout_ms);
bool SocketWouldBlock();

bool SendAll(Socket sock, const std::uint8_t* data, std::size_t len);
bool RecvSome(Socket sock, std::vector<std::uint8_t>& out);
bool RecvExact(Socket sock, std::uint8_t* data, std::size_t len);
int Send(Socket sock, const std::uint8_t* data, std::size_t len);
int Recv(Socket sock, std::uint8_t* data, std::size_t len);
int SendTo(Socket sock, const std::uint8_t* data, std::size_t len,
           const sockaddr* addr, socklen_t addr_len);
int RecvFrom(Socket sock, std::uint8_t* data, std::size_t len,
             sockaddr* addr, socklen_t* addr_len);

bool ConnectTcp(const std::string& host, std::uint16_t port, Socket& out,
                std::string& error);
bool ConnectUdp(const std::string& host, std::uint16_t port, Socket& out,
                std::string& error);
bool BindUdpSocket(std::uint16_t port, Socket& out, std::string& error);
bool CreateTcpListener(std::uint16_t port, Socket& out, std::string& error);
bool AcceptTcp(Socket listen_sock, Socket& out, std::string& remote_ip,
               std::string& error);
bool SockaddrToIp(const sockaddr* addr, socklen_t addr_len, std::string& out);
bool SockaddrToEndpoint(const sockaddr* addr, socklen_t addr_len,
                        std::string& out);

constexpr short kPollIn = 0x01;
constexpr short kPollOut = 0x02;
constexpr short kPollErr = 0x04;

struct PollFd {
  Socket sock{kInvalidSocket};
  short events{0};
  short revents{0};
};

int Poll(PollFd* fds, std::size_t count, std::uint32_t timeout_ms);

bool ShutdownSend(Socket sock);
void CloseSocket(Socket& sock);

}  // namespace mi::platform::net

#endif  // MI_E2EE_PLATFORM_NET_H
