#include "network_server.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <thread>
#ifdef MI_E2EE_ENABLE_TCP_SERVER
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
#endif
#endif

#include "crypto.h"
#include "frame.h"
#include "platform_net.h"
#include "platform_time.h"
#include "platform_tls.h"

namespace mi::server {

#ifdef MI_E2EE_ENABLE_TCP_SERVER
#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif
#endif

#ifdef _WIN32
namespace {
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
}  // namespace
#endif

#ifdef MI_E2EE_ENABLE_TCP_SERVER
namespace {

bool TlsReadFrameBuffered(mi::platform::net::Socket sock,
                          mi::platform::tls::ServerContext& ctx,
                          std::vector<std::uint8_t>& enc_buf,
                          std::vector<std::uint8_t>& plain_buf,
                          std::size_t& plain_off,
                          std::vector<std::uint8_t>& out_frame) {
  out_frame.clear();
  if (plain_off > plain_buf.size()) {
    plain_buf.clear();
    plain_off = 0;
  }

  std::vector<std::uint8_t> plain_chunk;
  while (true) {
    const std::size_t avail =
        plain_buf.size() >= plain_off ? (plain_buf.size() - plain_off) : 0;
    if (avail >= kFrameHeaderSize) {
      FrameType type;
      std::uint32_t payload_len = 0;
      if (!DecodeFrameHeader(plain_buf.data() + plain_off, avail, type,
                             payload_len)) {
        return false;
      }
      (void)type;
      const std::size_t total = kFrameHeaderSize + payload_len;
      if (avail >= total) {
        out_frame.assign(plain_buf.begin() + static_cast<std::ptrdiff_t>(plain_off),
                         plain_buf.begin() +
                             static_cast<std::ptrdiff_t>(plain_off + total));
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

    if (!mi::platform::tls::ServerDecryptToPlain(sock, ctx, enc_buf, plain_chunk)) {
      return false;
    }
    if (!plain_chunk.empty()) {
      plain_buf.insert(plain_buf.end(), plain_chunk.begin(), plain_chunk.end());
    }
  }
}
}  // namespace

struct NetworkServer::TlsServer {
  mi::platform::tls::ServerCredentials cred{};
};

void NetworkServer::TlsServerDeleter::operator()(TlsServer* p) const {
  if (p) {
    mi::platform::tls::Close(p->cred);
  }
  delete p;
}
#endif  // MI_E2EE_ENABLE_TCP_SERVER

#ifdef MI_E2EE_ENABLE_TCP_SERVER
namespace {
constexpr int kReactorPollTimeoutMs = 50;
constexpr std::size_t kReactorCompactThreshold = 1024u * 1024u;

bool SetNonBlocking(SocketHandle sock) {
  return mi::platform::net::SetNonBlocking(
      static_cast<mi::platform::net::Socket>(sock));
}

int SendRaw(SocketHandle sock, const std::uint8_t* data, std::size_t len) {
  return mi::platform::net::Send(
      static_cast<mi::platform::net::Socket>(sock), data, len);
}

int RecvRaw(SocketHandle sock, std::uint8_t* data, std::size_t len) {
  return mi::platform::net::Recv(
      static_cast<mi::platform::net::Socket>(sock), data, len);
}

bool WouldBlock() {
  return mi::platform::net::SocketWouldBlock();
}

void CloseSocketHandle(SocketHandle sock) {
  mi::platform::net::Socket tmp =
      static_cast<mi::platform::net::Socket>(sock);
  mi::platform::net::CloseSocket(tmp);
}

using PollFd = mi::platform::net::PollFd;
constexpr short kPollIn = mi::platform::net::kPollIn;
constexpr short kPollOut = mi::platform::net::kPollOut;
constexpr short kPollErr = mi::platform::net::kPollErr;
}  // namespace
#endif  // MI_E2EE_ENABLE_TCP_SERVER

#ifdef MI_E2EE_ENABLE_TCP_SERVER
struct NetworkServer::Connection {
  SocketHandle sock{kInvalidSocket};
  std::string remote_ip;
  std::uint64_t bytes_total{0};
  std::vector<std::uint8_t> recv_buf;
  std::size_t recv_off{0};
  std::vector<std::uint8_t> send_buf;
  std::size_t send_off{0};
  std::vector<std::uint8_t> response_buf;
  struct TlsState {
    bool handshake_done{false};
    mi::platform::tls::ServerContext ctx;
    std::vector<std::uint8_t> enc_in;
    std::vector<std::uint8_t> enc_tmp;

    ~TlsState() { mi::platform::tls::Close(ctx); }
  };
  std::unique_ptr<TlsState> tls;
#ifdef _WIN32
  std::mutex iocp_mutex;
  std::vector<std::uint8_t> iocp_recv_tmp;
  bool iocp_recv_pending{false};
  bool iocp_send_pending{false};
  std::deque<std::vector<std::uint8_t>> iocp_send_queue;
  std::size_t iocp_send_off{0};
#endif
  bool closed{false};
};

class NetworkServer::Reactor {
 public:
  explicit Reactor(NetworkServer* server) : server_(server) {}
  ~Reactor() { Stop(); }

  void Start() {
    running_.store(true);
    thread_ = std::thread(&Reactor::Loop, this);
  }

  void Stop() {
    running_.store(false);
    if (thread_.joinable()) {
      thread_.join();
    }
    CloseAll();
  }

  void AddConnection(std::shared_ptr<Connection> conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.push_back(std::move(conn));
  }

 private:
  void DrainPending() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (pending_.empty()) {
      return;
    }
    for (auto& conn : pending_) {
      connections_.push_back(std::move(conn));
    }
    pending_.clear();
  }

  void CloseConnection(const std::shared_ptr<Connection>& conn) {
    if (!conn || conn->closed) {
      return;
    }
    conn->closed = true;
    if (conn->sock != kInvalidSocket) {
      CloseSocketHandle(conn->sock);
      conn->sock = kInvalidSocket;
    }
    server_->ReleaseConnectionSlot(conn->remote_ip);
  }

  void CloseAll() {
    for (auto& conn : connections_) {
      CloseConnection(conn);
    }
    connections_.clear();
    pending_.clear();
  }

  void HandleWrite(const std::shared_ptr<Connection>& conn) {
    if (!conn || conn->closed) {
      return;
    }
    while (conn->send_off < conn->send_buf.size()) {
      const std::size_t remaining = conn->send_buf.size() - conn->send_off;
      const int want =
          remaining >
                  static_cast<std::size_t>((std::numeric_limits<int>::max)())
              ? (std::numeric_limits<int>::max)()
              : static_cast<int>(remaining);
      const int n = SendRaw(conn->sock,
                            conn->send_buf.data() + conn->send_off,
                            static_cast<std::size_t>(want));
      if (n > 0) {
        conn->send_off += static_cast<std::size_t>(n);
        continue;
      }
      if (n == 0) {
        CloseConnection(conn);
      } else if (WouldBlock()) {
        return;
      } else {
        CloseConnection(conn);
      }
      return;
    }
    if (conn->send_off >= conn->send_buf.size()) {
      conn->send_buf.clear();
      conn->send_off = 0;
    }
  }

  bool HandleFrame(const std::shared_ptr<Connection>& conn,
                   const std::uint8_t* data, std::size_t len) {
    if (!conn || conn->closed || !server_->listener_) {
      return false;
    }
    if (conn->bytes_total + len > server_->limits_.max_connection_bytes) {
      return false;
    }
    conn->bytes_total += len;
    auto& response = conn->response_buf;
    response.clear();
    TransportKind kind = TransportKind::kTcp;
    if (conn->tls) {
      kind = TransportKind::kTls;
    }
    if (!server_->listener_->Process(data, len, response, conn->remote_ip,
                                     kind)) {
      return false;
    }
    if (conn->bytes_total + response.size() >
        server_->limits_.max_connection_bytes) {
      return false;
    }
    conn->bytes_total += response.size();
    if (!response.empty()) {
      if (conn->tls) {
        if (!EncryptTlsPayload(conn, response)) {
          return false;
        }
      } else {
        if (conn->send_buf.empty()) {
          conn->send_buf.swap(response);
        } else {
          conn->send_buf.insert(conn->send_buf.end(), response.begin(),
                                response.end());
        }
      }
    }
    return true;
  }

  bool EnsureTlsHandshake(const std::shared_ptr<Connection>& conn) {
    if (!conn || !conn->tls) {
      return true;
    }
    if (!server_ || !server_->tls_) {
      return false;
    }
    auto& tls = *conn->tls;
    if (tls.handshake_done) {
      return true;
    }
    if (tls.enc_in.empty()) {
      return true;
    }
    std::string tls_err;
    bool done = false;
    if (!mi::platform::tls::ServerHandshakeStep(
            server_->tls_->cred, tls.ctx, tls.enc_in, tls.enc_tmp, done,
            tls_err)) {
      return false;
    }
    if (!tls.enc_tmp.empty()) {
      conn->send_buf.insert(conn->send_buf.end(), tls.enc_tmp.begin(),
                            tls.enc_tmp.end());
    }
    if (done) {
      tls.handshake_done = true;
    }
    return true;
  }

  bool DecryptTlsData(const std::shared_ptr<Connection>& conn) {
    if (!conn || !conn->tls || !conn->tls->handshake_done) {
      return true;
    }
    auto& tls = *conn->tls;
    if (tls.enc_in.empty()) {
      return true;
    }
    std::vector<std::uint8_t> plain;
    bool need_more = false;
    if (!mi::platform::tls::ServerDecryptBuffer(tls.ctx, tls.enc_in, plain,
                                                need_more)) {
      return false;
    }
    if (!plain.empty()) {
      conn->recv_buf.insert(conn->recv_buf.end(), plain.begin(), plain.end());
    }
    return true;
  }

  bool EncryptTlsPayload(const std::shared_ptr<Connection>& conn,
                         const std::vector<std::uint8_t>& plain) {
    if (!conn || !conn->tls || !conn->tls->handshake_done) {
      return false;
    }
    auto& tmp = conn->tls->enc_tmp;
    if (!mi::platform::tls::ServerEncryptBuffer(conn->tls->ctx, plain, tmp)) {
      return false;
    }
    if (!tmp.empty()) {
      conn->send_buf.insert(conn->send_buf.end(), tmp.begin(), tmp.end());
    }
    return true;
  }

  void HandleRead(const std::shared_ptr<Connection>& conn) {
    if (!conn || conn->closed) {
      return;
    }
    if (conn->tls) {
      std::uint8_t tmp[4096];
      for (;;) {
        const int n = RecvRaw(conn->sock, tmp, sizeof(tmp));
        if (n > 0) {
          conn->tls->enc_in.insert(conn->tls->enc_in.end(), tmp, tmp + n);
          continue;
        }
        if (n == 0) {
          CloseConnection(conn);
        } else if (!WouldBlock()) {
          CloseConnection(conn);
        }
        break;
      }
      if (conn->closed) {
        return;
      }
      if (!EnsureTlsHandshake(conn)) {
        CloseConnection(conn);
        return;
      }
      if (!conn->tls->handshake_done) {
        return;
      }
      if (!DecryptTlsData(conn)) {
        CloseConnection(conn);
        return;
      }
    } else {
      std::uint8_t tmp[4096];
      for (;;) {
        const int n = RecvRaw(conn->sock, tmp, sizeof(tmp));
        if (n > 0) {
          conn->recv_buf.insert(conn->recv_buf.end(), tmp, tmp + n);
          continue;
        }
        if (n == 0) {
          CloseConnection(conn);
        } else if (!WouldBlock()) {
          CloseConnection(conn);
        }
        break;
      }
    }

    while (!conn->closed) {
      const std::size_t avail =
          conn->recv_buf.size() >= conn->recv_off
              ? (conn->recv_buf.size() - conn->recv_off)
              : 0;
      if (avail < kFrameHeaderSize) {
        break;
      }
      FrameType type;
      std::uint32_t payload_len = 0;
      if (!DecodeFrameHeader(conn->recv_buf.data() + conn->recv_off, avail,
                             type, payload_len)) {
        CloseConnection(conn);
        return;
      }
      const std::size_t total = kFrameHeaderSize + payload_len;
      if (avail < total) {
        break;
      }
      if (!HandleFrame(conn, conn->recv_buf.data() + conn->recv_off, total)) {
        CloseConnection(conn);
        return;
      }
      conn->recv_off += total;
      if (conn->recv_off >= conn->recv_buf.size()) {
        conn->recv_buf.clear();
        conn->recv_off = 0;
      } else if (conn->recv_off > kReactorCompactThreshold) {
        std::vector<std::uint8_t> compact(
            conn->recv_buf.begin() +
                static_cast<std::ptrdiff_t>(conn->recv_off),
            conn->recv_buf.end());
        conn->recv_buf.swap(compact);
        conn->recv_off = 0;
      }
    }
  }

  void Loop() {
    while (running_.load()) {
      DrainPending();
      if (connections_.empty()) {
        mi::platform::SleepMs(kReactorPollTimeoutMs);
        continue;
      }
      std::vector<PollFd> fds;
      fds.reserve(connections_.size());
      for (const auto& conn : connections_) {
        if (!conn || conn->closed || conn->sock == kInvalidSocket) {
          continue;
        }
        PollFd p{};
        p.sock = conn->sock;
        p.events = kPollIn;
        if (!conn->send_buf.empty()) {
          p.events |= kPollOut;
        }
        p.revents = 0;
        fds.push_back(p);
      }
      if (fds.empty()) {
        mi::platform::SleepMs(kReactorPollTimeoutMs);
        continue;
      }
      const int rc = mi::platform::net::Poll(
          fds.data(), fds.size(), kReactorPollTimeoutMs);
      if (rc <= 0) {
        continue;
      }
      std::size_t idx = 0;
      for (auto& conn : connections_) {
        if (!conn || conn->closed || conn->sock == kInvalidSocket) {
          continue;
        }
        if (idx >= fds.size()) {
          break;
        }
        const short revents = fds[idx].revents;
        if (revents & kPollErr) {
          CloseConnection(conn);
          idx++;
          continue;
        }
        if (revents & kPollIn) {
          HandleRead(conn);
        }
        if ((revents & kPollOut) && !conn->send_buf.empty()) {
          HandleWrite(conn);
        }
        idx++;
      }

      connections_.erase(
          std::remove_if(connections_.begin(), connections_.end(),
                         [](const std::shared_ptr<Connection>& conn) {
                           return !conn || conn->closed ||
                                  conn->sock == kInvalidSocket;
                         }),
          connections_.end());
    }
  }

  NetworkServer* server_{nullptr};
  std::atomic<bool> running_{false};
  std::thread thread_;
  std::mutex mutex_;
  std::vector<std::shared_ptr<Connection>> pending_;
  std::vector<std::shared_ptr<Connection>> connections_;
};

#ifdef _WIN32
class NetworkServer::IocpEngine {
 public:
  explicit IocpEngine(NetworkServer* server) : server_(server) {}
  ~IocpEngine() { Stop(); }

  bool Start(std::string& error) {
    error.clear();
    if (running_.load()) {
      return true;
    }
    iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!iocp_) {
      const DWORD last = GetLastError();
      error = "CreateIoCompletionPort failed: " + std::to_string(last) + " " +
              Win32ErrorMessage(last);
      return false;
    }
    running_.store(true);
    std::uint32_t count = server_ ? server_->limits_.max_io_threads : 0;
    if (count == 0) {
      const auto hc = std::thread::hardware_concurrency();
      count = hc == 0 ? 2u : std::min<std::uint32_t>(4u, hc);
    }
    threads_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
      threads_.emplace_back(&IocpEngine::WorkerLoop, this);
    }
    return true;
  }

  void Stop() {
    running_.store(false);
    if (iocp_) {
      for (std::size_t i = 0; i < threads_.size(); ++i) {
        PostQueuedCompletionStatus(iocp_, 0, 0, nullptr);
      }
    }
    for (auto& t : threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
    threads_.clear();
    CleanupAll();
    if (iocp_) {
      CloseHandle(iocp_);
      iocp_ = nullptr;
    }
  }

  void AddConnection(std::shared_ptr<Connection> conn) {
    if (!conn || !server_) {
      return;
    }
    if (!iocp_) {
      server_->ReleaseConnectionSlot(conn->remote_ip);
      CloseSocketHandle(conn->sock);
      conn->sock = kInvalidSocket;
      return;
    }
    if (!CreateIoCompletionPort(reinterpret_cast<HANDLE>(conn->sock), iocp_, 0,
                                 0)) {
      const DWORD last = GetLastError();
      (void)last;
      server_->ReleaseConnectionSlot(conn->remote_ip);
      CloseSocketHandle(conn->sock);
      conn->sock = kInvalidSocket;
      return;
    }
    conn->recv_buf.reserve(8192);
    conn->iocp_recv_tmp.resize(4096);
    if (server_ && server_->tls_enable_ && server_->tls_) {
      conn->tls = std::make_unique<Connection::TlsState>();
      conn->tls->enc_in.reserve(8192);
      conn->tls->enc_tmp.reserve(8192);
      conn->send_buf.reserve(8192);
    }
    {
      std::lock_guard<std::mutex> lock(conn_mutex_);
      connections_.push_back(conn);
    }
    PostRecv(conn);
  }

 private:
  enum class OpKind { kRecv, kSend };

  struct IocpOp {
    OVERLAPPED overlapped{};
    WSABUF buf{};
    OpKind kind{OpKind::kRecv};
    std::shared_ptr<Connection> conn;
  };

  void CleanupAll() {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    for (auto& conn : connections_) {
      CloseConnection(conn);
    }
    connections_.clear();
  }

  void CloseConnection(const std::shared_ptr<Connection>& conn) {
    if (!conn || conn->closed) {
      return;
    }
    conn->closed = true;
    if (conn->sock != kInvalidSocket) {
      CloseSocketHandle(conn->sock);
      conn->sock = kInvalidSocket;
    }
    if (server_) {
      server_->ReleaseConnectionSlot(conn->remote_ip);
    }
  }

  void CleanupClosed() {
    std::lock_guard<std::mutex> lock(conn_mutex_);
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
                       [](const std::shared_ptr<Connection>& conn) {
                         return !conn || conn->closed ||
                                conn->sock == kInvalidSocket;
                       }),
        connections_.end());
  }

  void PostRecv(const std::shared_ptr<Connection>& conn) {
    if (!conn || conn->closed) {
      return;
    }
    if (conn->iocp_recv_pending) {
      return;
    }
    auto* op = new IocpOp();
    op->kind = OpKind::kRecv;
    op->conn = conn;
    op->buf.buf =
        reinterpret_cast<char*>(conn->iocp_recv_tmp.data());
    op->buf.len = static_cast<ULONG>(conn->iocp_recv_tmp.size());
    DWORD flags = 0;
    DWORD bytes = 0;
    conn->iocp_recv_pending = true;
    const int rc = WSARecv(conn->sock, &op->buf, 1, &bytes, &flags,
                           &op->overlapped, nullptr);
    if (rc == SOCKET_ERROR) {
      const int err = WSAGetLastError();
      if (err != WSA_IO_PENDING) {
        conn->iocp_recv_pending = false;
        delete op;
        CloseConnection(conn);
      }
    }
  }

  void PostSendLocked(const std::shared_ptr<Connection>& conn) {
    if (!conn || conn->closed) {
      return;
    }
    if (conn->iocp_send_pending || conn->iocp_send_queue.empty()) {
      return;
    }
    auto& front = conn->iocp_send_queue.front();
    if (front.empty()) {
      conn->iocp_send_queue.pop_front();
      conn->iocp_send_off = 0;
      return;
    }
    auto* op = new IocpOp();
    op->kind = OpKind::kSend;
    op->conn = conn;
    op->buf.buf = reinterpret_cast<char*>(
        front.data() + conn->iocp_send_off);
    op->buf.len = static_cast<ULONG>(
        front.size() - conn->iocp_send_off);
    DWORD bytes = 0;
    conn->iocp_send_pending = true;
    const int rc = WSASend(conn->sock, &op->buf, 1, &bytes, 0,
                           &op->overlapped, nullptr);
    if (rc == SOCKET_ERROR) {
      const int err = WSAGetLastError();
      if (err != WSA_IO_PENDING) {
        conn->iocp_send_pending = false;
        delete op;
        CloseConnection(conn);
      }
    }
  }

  void QueueSendLocked(const std::shared_ptr<Connection>& conn,
                       std::vector<std::uint8_t>&& payload) {
    if (!conn || conn->closed || payload.empty()) {
      return;
    }
    conn->iocp_send_queue.push_back(std::move(payload));
    PostSendLocked(conn);
  }

  bool HandleFrameLocked(const std::shared_ptr<Connection>& conn,
                         const std::uint8_t* data, std::size_t len) {
    if (!conn || conn->closed || !server_ || !server_->listener_) {
      return false;
    }
    if (conn->bytes_total + len > server_->limits_.max_connection_bytes) {
      return false;
    }
    conn->bytes_total += len;
    auto& response = conn->response_buf;
    response.clear();
    TransportKind kind = TransportKind::kTcp;
#ifdef _WIN32
    if (conn->tls) {
      kind = TransportKind::kTls;
    }
#endif
    if (!server_->listener_->Process(data, len, response, conn->remote_ip,
                                     kind)) {
      return false;
    }
    if (conn->bytes_total + response.size() >
        server_->limits_.max_connection_bytes) {
      return false;
    }
    conn->bytes_total += response.size();
    if (!response.empty()) {
#ifdef _WIN32
      if (conn->tls) {
        if (!EncryptTlsPayloadLocked(conn, response)) {
          return false;
        }
        FlushTlsSendLocked(conn);
      } else
#endif
      {
        std::vector<std::uint8_t> payload = std::move(response);
        QueueSendLocked(conn, std::move(payload));
      }
      response.clear();
      response.reserve(4096);
    }
    return true;
  }

#ifdef _WIN32
  void FlushTlsSendLocked(const std::shared_ptr<Connection>& conn) {
    if (!conn || conn->send_buf.empty()) {
      return;
    }
    std::vector<std::uint8_t> payload = std::move(conn->send_buf);
    conn->send_buf.clear();
    conn->send_buf.reserve(4096);
    QueueSendLocked(conn, std::move(payload));
  }

  bool EnsureTlsHandshakeLocked(const std::shared_ptr<Connection>& conn) {
    if (!conn || !conn->tls) {
      return true;
    }
    if (!server_ || !server_->tls_) {
      return false;
    }
    auto& tls = *conn->tls;
    if (tls.handshake_done) {
      return true;
    }
    if (tls.enc_in.empty()) {
      return true;
    }
    std::string tls_err;
    bool done = false;
    if (!mi::platform::tls::ServerHandshakeStep(
            server_->tls_->cred, tls.ctx, tls.enc_in, tls.enc_tmp, done,
            tls_err)) {
      return false;
    }
    if (!tls.enc_tmp.empty()) {
      conn->send_buf.insert(conn->send_buf.end(), tls.enc_tmp.begin(),
                            tls.enc_tmp.end());
    }
    if (done) {
      tls.handshake_done = true;
    }
    return true;
  }

  bool DecryptTlsDataLocked(const std::shared_ptr<Connection>& conn) {
    if (!conn || !conn->tls || !conn->tls->handshake_done) {
      return true;
    }
    auto& tls = *conn->tls;
    if (tls.enc_in.empty()) {
      return true;
    }
    std::vector<std::uint8_t> plain;
    bool need_more = false;
    if (!mi::platform::tls::ServerDecryptBuffer(tls.ctx, tls.enc_in, plain,
                                                need_more)) {
      return false;
    }
    if (!plain.empty()) {
      conn->recv_buf.insert(conn->recv_buf.end(), plain.begin(), plain.end());
    }
    return true;
  }

  bool EncryptTlsPayloadLocked(const std::shared_ptr<Connection>& conn,
                               const std::vector<std::uint8_t>& plain) {
    if (!conn || !conn->tls || !conn->tls->handshake_done) {
      return false;
    }
    auto& tmp = conn->tls->enc_tmp;
    if (!mi::platform::tls::ServerEncryptBuffer(conn->tls->ctx, plain, tmp)) {
      return false;
    }
    if (!tmp.empty()) {
      conn->send_buf.insert(conn->send_buf.end(), tmp.begin(), tmp.end());
    }
    return true;
  }
#endif

  void HandleIncomingLocked(const std::shared_ptr<Connection>& conn) {
    while (!conn->closed) {
      const std::size_t avail =
          conn->recv_buf.size() >= conn->recv_off
              ? (conn->recv_buf.size() - conn->recv_off)
              : 0;
      if (avail < kFrameHeaderSize) {
        break;
      }
      FrameType type;
      std::uint32_t payload_len = 0;
      if (!DecodeFrameHeader(conn->recv_buf.data() + conn->recv_off, avail,
                             type, payload_len)) {
        CloseConnection(conn);
        return;
      }
      const std::size_t total = kFrameHeaderSize + payload_len;
      if (avail < total) {
        break;
      }
      if (!HandleFrameLocked(conn, conn->recv_buf.data() + conn->recv_off,
                             total)) {
        CloseConnection(conn);
        return;
      }
      conn->recv_off += total;
      if (conn->recv_off >= conn->recv_buf.size()) {
        conn->recv_buf.clear();
        conn->recv_off = 0;
      } else if (conn->recv_off > kReactorCompactThreshold) {
        std::vector<std::uint8_t> compact(
            conn->recv_buf.begin() +
                static_cast<std::ptrdiff_t>(conn->recv_off),
            conn->recv_buf.end());
        conn->recv_buf.swap(compact);
        conn->recv_off = 0;
      }
    }
  }

  void WorkerLoop() {
    while (running_.load()) {
      DWORD bytes = 0;
      ULONG_PTR key = 0;
      OVERLAPPED* overlapped = nullptr;
      const BOOL ok =
          GetQueuedCompletionStatus(iocp_, &bytes, &key, &overlapped, 1000);
      if (!running_.load()) {
        break;
      }
      if (!overlapped) {
        if ((sweep_.fetch_add(1, std::memory_order_relaxed) & 0xFFu) == 0u) {
          CleanupClosed();
        }
        continue;
      }
      auto* op = reinterpret_cast<IocpOp*>(overlapped);
      auto conn = op->conn;
      if (!conn) {
        delete op;
        continue;
      }
      if (!ok || bytes == 0) {
        std::lock_guard<std::mutex> lock(conn->iocp_mutex);
        if (op->kind == OpKind::kRecv) {
          conn->iocp_recv_pending = false;
        } else {
          conn->iocp_send_pending = false;
        }
        CloseConnection(conn);
        delete op;
        continue;
      }
      if (op->kind == OpKind::kRecv) {
        bool close_conn = false;
        bool should_post = true;
        {
          std::lock_guard<std::mutex> lock(conn->iocp_mutex);
          conn->iocp_recv_pending = false;
#ifdef _WIN32
          if (conn->tls) {
            conn->tls->enc_in.insert(conn->tls->enc_in.end(),
                                     conn->iocp_recv_tmp.begin(),
                                     conn->iocp_recv_tmp.begin() +
                                         static_cast<std::ptrdiff_t>(bytes));
            if (!EnsureTlsHandshakeLocked(conn)) {
              close_conn = true;
            } else {
              FlushTlsSendLocked(conn);
              if (conn->tls->handshake_done) {
                if (!DecryptTlsDataLocked(conn)) {
                  close_conn = true;
                } else {
                  HandleIncomingLocked(conn);
                  FlushTlsSendLocked(conn);
                }
              }
            }
          } else
#endif
          {
            conn->recv_buf.insert(conn->recv_buf.end(),
                                  conn->iocp_recv_tmp.begin(),
                                  conn->iocp_recv_tmp.begin() +
                                      static_cast<std::ptrdiff_t>(bytes));
            HandleIncomingLocked(conn);
          }
          if (conn->closed) {
            should_post = false;
          }
        }
        if (close_conn) {
          CloseConnection(conn);
          should_post = false;
        }
        if (should_post) {
          PostRecv(conn);
        }
      } else {
        {
          std::lock_guard<std::mutex> lock(conn->iocp_mutex);
          conn->iocp_send_pending = false;
          if (!conn->iocp_send_queue.empty()) {
            auto& front = conn->iocp_send_queue.front();
            conn->iocp_send_off += static_cast<std::size_t>(bytes);
            if (conn->iocp_send_off >= front.size()) {
              conn->iocp_send_queue.pop_front();
              conn->iocp_send_off = 0;
            }
          }
          PostSendLocked(conn);
        }
      }
      delete op;
    }
  }

  NetworkServer* server_{nullptr};
  HANDLE iocp_{nullptr};
  std::atomic<bool> running_{false};
  std::vector<std::thread> threads_;
  std::mutex conn_mutex_;
  std::vector<std::shared_ptr<Connection>> connections_;
  std::atomic<std::uint64_t> sweep_{0};
};
#endif  // _WIN32
#else  // MI_E2EE_ENABLE_TCP_SERVER
struct NetworkServer::Connection {};

class NetworkServer::Reactor {
 public:
  explicit Reactor(NetworkServer*) {}
  void Start() {}
  void Stop() {}
  void AddConnection(std::shared_ptr<Connection>) {}
};

#ifdef _WIN32
class NetworkServer::IocpEngine {
 public:
  explicit IocpEngine(NetworkServer*) {}
  bool Start(std::string& error) {
    error = "tcp server not built";
    return false;
  }
  void Stop() {}
  void AddConnection(std::shared_ptr<Connection>) {}
};
#endif  // _WIN32
#endif  // MI_E2EE_ENABLE_TCP_SERVER

NetworkServer::NetworkServer(Listener* listener, std::uint16_t port,
                             bool tls_enable, std::string tls_cert,
                             bool iocp_enable, NetworkServerLimits limits)
    : listener_(listener),
      port_(port),
      tls_enable_(tls_enable),
      tls_cert_(std::move(tls_cert)),
      iocp_enable_(iocp_enable),
      limits_(limits) {}

NetworkServer::~NetworkServer() { Stop(); }

bool NetworkServer::Start(std::string& error) {
  error.clear();
  if (running_.load()) {
    return true;
  }
  if (!listener_ || port_ == 0) {
    error = "invalid listener/port";
    return false;
  }
#ifndef MI_E2EE_ENABLE_TCP_SERVER
  error = "tcp server not built (enable MI_E2EE_ENABLE_TCP_SERVER)";
  return false;
#endif
#ifdef MI_E2EE_ENABLE_TCP_SERVER
  if (tls_enable_) {
    if (mi::platform::tls::IsStubbed()) {
      error = "tls stub build";
      return false;
    }
    if (!mi::platform::tls::IsSupported()) {
      error = "tls not supported on this platform";
      return false;
    }
    auto tls = std::unique_ptr<TlsServer, TlsServerDeleter>(new TlsServer());
    std::string tls_err;
    if (!mi::platform::tls::ServerInitCredentials(tls_cert_, tls->cred,
                                                  tls_err)) {
      error = tls_err.empty() ? "tls init failed" : tls_err;
      return false;
    }
    tls_ = std::move(tls);
  }
#endif
#ifdef _WIN32
  use_iocp_ = iocp_enable_;
#else
  use_iocp_ = false;
#endif
#ifdef MI_E2EE_ENABLE_TCP_SERVER
  std::string sock_err;
  if (!StartSocket(sock_err)) {
    error = sock_err.empty() ? "start socket failed" : sock_err;
    return false;
  }
#endif
  running_.store(true);
  StartWorkers();
  if (use_iocp_) {
    std::string iocp_err;
    if (!StartIocp(iocp_err)) {
      error = iocp_err.empty() ? "iocp start failed" : iocp_err;
      StopSocket();
      StopWorkers();
      running_.store(false);
      return false;
    }
  } else {
    StartReactors();
  }
  worker_ = std::thread(&NetworkServer::Run, this);
  return true;
}

void NetworkServer::Stop() {
  running_.store(false);
#ifdef MI_E2EE_ENABLE_TCP_SERVER
  StopSocket();
#endif
  if (worker_.joinable()) {
    worker_.join();
  }
  StopIocp();
  StopReactors();
  StopWorkers();
  // Wait until connections drain to avoid use-after-free.
  while (active_connections_.load(std::memory_order_relaxed) != 0) {
    mi::platform::SleepMs(50);
  }
}

void NetworkServer::StartWorkers() {
  pool_running_.store(true);
  std::uint32_t count = limits_.max_worker_threads;
  if (count == 0) {
    const auto hc = std::thread::hardware_concurrency();
    count = hc == 0 ? 4u : hc;
  }
  worker_threads_.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    worker_threads_.emplace_back(&NetworkServer::WorkerLoop, this);
  }
}

void NetworkServer::StopWorkers() {
  pool_running_.store(false);
  work_cv_.notify_all();
  for (auto& t : worker_threads_) {
    if (t.joinable()) {
      t.join();
    }
  }
  worker_threads_.clear();
}

void NetworkServer::StartReactors() {
#ifdef MI_E2EE_ENABLE_TCP_SERVER
  std::uint32_t count = limits_.max_io_threads;
  if (count == 0) {
    const auto hc = std::thread::hardware_concurrency();
    if (hc == 0) {
      count = 2;
    } else {
      count = std::min<std::uint32_t>(4u, hc);
    }
  }
  reactors_.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    auto reactor = std::make_unique<Reactor>(this);
    reactor->Start();
    reactors_.push_back(std::move(reactor));
  }
#endif
}

void NetworkServer::StopReactors() {
#ifdef MI_E2EE_ENABLE_TCP_SERVER
  for (auto& reactor : reactors_) {
    if (reactor) {
      reactor->Stop();
    }
  }
  reactors_.clear();
#endif
}

bool NetworkServer::StartIocp(std::string& error) {
  error.clear();
#ifdef _WIN32
#ifdef MI_E2EE_ENABLE_TCP_SERVER
  if (!iocp_) {
    auto engine = std::make_unique<IocpEngine>(this);
    if (!engine->Start(error)) {
      return false;
    }
    iocp_ = std::move(engine);
  }
  return true;
#else
  error = "tcp server not built";
  return false;
#endif
#else
  error = "iocp not supported";
  return false;
#endif
}

void NetworkServer::StopIocp() {
#ifdef _WIN32
  if (iocp_) {
    iocp_->Stop();
    iocp_.reset();
  }
#endif
}

void NetworkServer::AssignConnection(std::shared_ptr<Connection> conn) {
#ifdef MI_E2EE_ENABLE_TCP_SERVER
  if (!conn) {
    return;
  }
  if (reactors_.empty()) {
    CloseSocketHandle(conn->sock);
    conn->sock = kInvalidSocket;
    ReleaseConnectionSlot(conn->remote_ip);
    return;
  }
  const std::uint32_t idx =
      next_reactor_.fetch_add(1, std::memory_order_relaxed) %
      static_cast<std::uint32_t>(reactors_.size());
  reactors_[idx]->AddConnection(std::move(conn));
#endif
}

void NetworkServer::WorkerLoop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(work_mutex_);
      work_cv_.wait(lock, [this] {
        return !pool_running_.load() || !work_queue_.empty();
      });
      if (!pool_running_.load() && work_queue_.empty()) {
        return;
      }
      task = std::move(work_queue_.front());
      work_queue_.pop_front();
    }
    if (task) {
      task();
    }
  }
}

bool NetworkServer::EnqueueTask(std::function<void()> task) {
  if (!task) {
    return false;
  }
  std::lock_guard<std::mutex> lock(work_mutex_);
  if (!pool_running_.load()) {
    return false;
  }
  if (work_queue_.size() >= limits_.max_pending_tasks) {
    return false;
  }
  work_queue_.push_back(std::move(task));
  work_cv_.notify_one();
  return true;
}

bool NetworkServer::TryAcquireConnectionSlot(const std::string& remote_ip) {
  const std::uint32_t prev =
      active_connections_.fetch_add(1, std::memory_order_relaxed);
  if (prev >= limits_.max_connections) {
    active_connections_.fetch_sub(1, std::memory_order_relaxed);
    return false;
  }

  if (remote_ip.empty()) {
    return true;
  }
  std::lock_guard<std::mutex> lock(conn_mutex_);
  const auto it = connections_by_ip_.find(remote_ip);
  const std::uint32_t current =
      it == connections_by_ip_.end() ? 0u : it->second;
  if (current >= limits_.max_connections_per_ip) {
    active_connections_.fetch_sub(1, std::memory_order_relaxed);
    return false;
  }
  if (it == connections_by_ip_.end()) {
    connections_by_ip_.emplace(remote_ip, 1u);
  } else {
    it->second++;
  }
  return true;
}

void NetworkServer::ReleaseConnectionSlot(const std::string& remote_ip) {
  active_connections_.fetch_sub(1, std::memory_order_relaxed);
  if (remote_ip.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(conn_mutex_);
  const auto it = connections_by_ip_.find(remote_ip);
  if (it == connections_by_ip_.end()) {
    return;
  }
  if (it->second <= 1) {
    connections_by_ip_.erase(it);
    return;
  }
  it->second--;
}

void NetworkServer::Run() {
  //  TCP/KCP 
#ifndef MI_E2EE_ENABLE_TCP_SERVER
  while (running_.load()) {
    mi::platform::SleepMs(1000);
  }
#else
  const bool use_reactor = !reactors_.empty();
#ifdef _WIN32
  const bool use_iocp = use_iocp_ && iocp_;
#else
  const bool use_iocp = false;
#endif
  while (running_.load()) {
    SocketHandle client = kInvalidSocket;
    std::string remote_ip;
    std::string accept_error;
    if (!mi::platform::net::AcceptTcp(
            static_cast<mi::platform::net::Socket>(listen_fd_), client,
            remote_ip, accept_error)) {
      continue;
    }
    if (!TryAcquireConnectionSlot(remote_ip)) {
      CloseSocketHandle(client);
      continue;
    }
#ifdef _WIN32
    if (use_iocp) {
      auto conn = std::make_shared<Connection>();
      conn->sock = client;
      conn->remote_ip = remote_ip;
      conn->recv_buf.reserve(8192);
      iocp_->AddConnection(std::move(conn));
      continue;
    }
#endif
    if (use_reactor) {
      if (!SetNonBlocking(client)) {
        ReleaseConnectionSlot(remote_ip);
        CloseSocketHandle(client);
        continue;
      }
      auto conn = std::make_shared<Connection>();
      conn->sock = client;
      conn->remote_ip = remote_ip;
      conn->recv_buf.reserve(8192);
      if (tls_enable_ && tls_) {
        conn->tls = std::make_unique<Connection::TlsState>();
        conn->tls->enc_in.reserve(8192);
      }
      AssignConnection(std::move(conn));
      continue;
    }
    {
      constexpr std::uint32_t timeout_ms = 30000;
      mi::platform::net::SetRecvTimeout(client, timeout_ms);
      mi::platform::net::SetSendTimeout(client, timeout_ms);
    }

    try {
      auto task = [this, client, remote_ip]() mutable {
        struct SlotGuard {
          NetworkServer* server;
          std::string ip;
          ~SlotGuard() { server->ReleaseConnectionSlot(ip); }
        } slot{this, remote_ip};

        try {
          std::uint64_t bytes_total = 0;

          const auto recv_exact = [&](std::uint8_t* data,
                                      std::size_t len) -> bool {
            if (len == 0) {
              return true;
            }
            std::size_t got = 0;
            while (got < len) {
              const std::size_t remaining = len - got;
              const int want =
                  remaining >
                          static_cast<std::size_t>(
                              (std::numeric_limits<int>::max)())
                      ? (std::numeric_limits<int>::max)()
                      : static_cast<int>(remaining);
              const int n = RecvRaw(
                  client, data + got, static_cast<std::size_t>(want));
              if (n <= 0) {
                return false;
              }
              got += static_cast<std::size_t>(n);
            }
            return true;
          };

          const auto send_all = [&](const std::uint8_t* data,
                                    std::size_t len) -> bool {
            if (!data || len == 0) {
              return true;
            }
            std::size_t sent = 0;
            while (sent < len) {
              const std::size_t remaining = len - sent;
              const int chunk =
                  remaining >
                          static_cast<std::size_t>(
                              (std::numeric_limits<int>::max)())
                      ? (std::numeric_limits<int>::max)()
                      : static_cast<int>(remaining);
              const int n = SendRaw(
                  client, data + sent, static_cast<std::size_t>(chunk));
              if (n <= 0) {
                return false;
              }
              sent += static_cast<std::size_t>(n);
            }
            return true;
          };

          if (tls_enable_ && tls_) {
            mi::platform::tls::ServerContext ctx;
            std::vector<std::uint8_t> enc_buf;
            std::string tls_err;
            if (!mi::platform::tls::ServerHandshake(client, tls_->cred, ctx,
                                                    enc_buf, tls_err)) {
              CloseSocketHandle(client);
              return;
            }
            std::vector<std::uint8_t> plain_buf;
            std::size_t plain_off = 0;
            std::vector<std::uint8_t> request;
            std::vector<std::uint8_t> response;
            while (running_.load()) {
              if (!TlsReadFrameBuffered(client, ctx, enc_buf, plain_buf,
                                        plain_off, request)) {
                break;
              }
              bytes_total += request.size();
              if (bytes_total > limits_.max_connection_bytes) {
                break;
              }
              response.clear();
              if (!listener_->Process(request, response, slot.ip,
                                      TransportKind::kTls)) {
                break;
              }
              bytes_total += response.size();
              if (bytes_total > limits_.max_connection_bytes) {
                break;
              }
              if (!response.empty() &&
                  !mi::platform::tls::ServerEncryptAndSend(client, ctx,
                                                          response)) {
                break;
              }
            }
            mi::platform::tls::Close(ctx);
            CloseSocketHandle(client);
            return;
          }

          std::vector<std::uint8_t> request;
          std::vector<std::uint8_t> response;
          while (running_.load()) {
            std::uint8_t header[kFrameHeaderSize] = {};
            if (!recv_exact(header, sizeof(header))) {
              break;
            }
            FrameType type;
            std::uint32_t payload_len = 0;
            if (!DecodeFrameHeader(header, sizeof(header), type, payload_len)) {
              break;
            }
            (void)type;
            const std::size_t total = kFrameHeaderSize + payload_len;
            bytes_total += total;
            if (bytes_total > limits_.max_connection_bytes) {
              break;
            }
            request.resize(total);
            std::memcpy(request.data(), header, sizeof(header));
            if (payload_len > 0 &&
                !recv_exact(request.data() + kFrameHeaderSize, payload_len)) {
              break;
            }

            response.clear();
            if (!listener_->Process(request, response, slot.ip,
                                    TransportKind::kTcp)) {
              break;
            }
            bytes_total += response.size();
            if (bytes_total > limits_.max_connection_bytes) {
              break;
            }
            if (!response.empty() &&
                !send_all(response.data(), response.size())) {
              break;
            }
          }
          CloseSocketHandle(client);
        } catch (...) {
          CloseSocketHandle(client);
        }
      };
      if (!EnqueueTask(std::move(task))) {
        ReleaseConnectionSlot(remote_ip);
        CloseSocketHandle(client);
      }
    } catch (...) {
      ReleaseConnectionSlot(remote_ip);
      CloseSocketHandle(client);
    }
  }
#endif
}

#ifdef MI_E2EE_ENABLE_TCP_SERVER
bool NetworkServer::StartSocket(std::string& error) {
  error.clear();
  mi::platform::net::Socket sock = mi::platform::net::kInvalidSocket;
  if (!mi::platform::net::CreateTcpListener(port_, sock, error)) {
    return false;
  }
  listen_fd_ = static_cast<std::intptr_t>(sock);
  return true;
}

void NetworkServer::StopSocket() {
  if (listen_fd_ != -1) {
    auto sock = static_cast<mi::platform::net::Socket>(listen_fd_);
    mi::platform::net::CloseSocket(sock);
    listen_fd_ = -1;
  }
}
#else
bool NetworkServer::StartSocket(std::string& error) {
  error = "tcp server not built";
  return false;
}

void NetworkServer::StopSocket() {}
#endif

}  // namespace mi::server
