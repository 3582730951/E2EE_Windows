#include "network_server.h"

#include <chrono>
#include <iostream>
#include <thread>
#ifdef MI_E2EE_ENABLE_TCP_SERVER
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#endif

namespace mi::server {

NetworkServer::NetworkServer(Listener* listener, std::uint16_t port)
    : listener_(listener), port_(port) {}

NetworkServer::~NetworkServer() { Stop(); }

bool NetworkServer::Start() {
  if (running_.load()) {
    return true;
  }
  if (!listener_ || port_ == 0) {
    return false;
  }
#ifdef MI_E2EE_ENABLE_TCP_SERVER
  if (!StartSocket()) {
    return false;
  }
#endif
  running_.store(true);
  worker_ = std::thread(&NetworkServer::Run, this);
  return true;
}

void NetworkServer::Stop() {
  running_.store(false);
  if (worker_.joinable()) {
    worker_.join();
  }
#ifdef MI_E2EE_ENABLE_TCP_SERVER
  StopSocket();
#endif
}

void NetworkServer::Run() {
  //  TCP/KCP 
#ifndef MI_E2EE_ENABLE_TCP_SERVER
  while (running_.load()) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
#else
  while (running_.load()) {
    sockaddr_in cli{};
    socklen_t len = sizeof(cli);
#ifdef _WIN32
    SOCKET client = accept(listen_fd_, reinterpret_cast<sockaddr*>(&cli), &len);
    if (client == INVALID_SOCKET) {
      continue;
    }
#else
    int client = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&cli), &len);
    if (client < 0) {
      continue;
    }
#endif
    std::vector<std::uint8_t> buf(4096);
#ifdef _WIN32
    int n = recv(client, reinterpret_cast<char*>(buf.data()),
                 static_cast<int>(buf.size()), 0);
#else
    ssize_t n = ::recv(client, buf.data(), buf.size(), 0);
#endif
    if (n > 0) {
      buf.resize(static_cast<std::size_t>(n));
      std::vector<std::uint8_t> out;
      if (listener_->Process(buf, out) && !out.empty()) {
#ifdef _WIN32
        send(client, reinterpret_cast<const char*>(out.data()),
             static_cast<int>(out.size()), 0);
#else
        ::send(client, out.data(), out.size(), 0);
#endif
      }
    }
#ifdef _WIN32
    closesocket(client);
#else
    ::close(client);
#endif
  }
#endif
}

#ifdef MI_E2EE_ENABLE_TCP_SERVER
bool NetworkServer::StartSocket() {
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    return false;
  }
#endif
  listen_fd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
  if (listen_fd_ < 0) {
    return false;
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  int yes = 1;
#ifdef _WIN32
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    StopSocket();
    return false;
  }
  if (::listen(listen_fd_, 8) < 0) {
    StopSocket();
    return false;
  }
  return true;
}

void NetworkServer::StopSocket() {
  if (listen_fd_ >= 0) {
#ifdef _WIN32
    closesocket(listen_fd_);
    WSACleanup();
#else
    ::close(listen_fd_);
#endif
    listen_fd_ = -1;
  }
}
#endif

}  // namespace mi::server
