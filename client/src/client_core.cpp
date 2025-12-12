#include "client_core.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <utility>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "../server/include/c_api.h"
#include "../server/include/frame.h"
#include "../server/include/protocol.h"
#include "client_config.h"

namespace mi::client {

namespace {

#ifdef _WIN32
bool EnsureWinsock() {
  static std::once_flag once;
  static int status = -1;
  std::call_once(once, []() {
    WSADATA wsa;
    status = WSAStartup(MAKEWORD(2, 2), &wsa);
  });
  return status == 0;
}
#endif

bool TcpRoundTrip(const std::string& host, std::uint16_t port,
                  const std::vector<std::uint8_t>& in_bytes,
                  std::vector<std::uint8_t>& out_bytes) {
  out_bytes.clear();
  if (host.empty() || port == 0 || in_bytes.empty()) {
    return false;
  }

#ifdef _WIN32
  if (!EnsureWinsock()) {
    return false;
  }
#endif

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* result = nullptr;
  const std::string port_str = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0) {
    return false;
  }

#ifdef _WIN32
  SOCKET sock = INVALID_SOCKET;
#else
  int sock = -1;
#endif
  for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    sock =
#ifdef _WIN32
        ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == INVALID_SOCKET) {
      continue;
    }
    if (::connect(sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) {
      break;
    }
    closesocket(sock);
    sock = INVALID_SOCKET;
#else
        ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock < 0) {
      continue;
    }
    if (::connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }
    ::close(sock);
    sock = -1;
#endif
  }
  freeaddrinfo(result);

#ifdef _WIN32
  if (sock == INVALID_SOCKET) {
    return false;
  }
#else
  if (sock < 0) {
    return false;
  }
#endif

  std::size_t sent = 0;
  while (sent < in_bytes.size()) {
    const auto remaining = in_bytes.size() - sent;
    const int chunk =
        remaining >
                static_cast<std::size_t>((std::numeric_limits<int>::max)())
            ? (std::numeric_limits<int>::max)()
            : static_cast<int>(remaining);
#ifdef _WIN32
    const int n = ::send(sock,
                         reinterpret_cast<const char*>(in_bytes.data() + sent),
                         chunk, 0);
#else
    const ssize_t n =
        ::send(sock, in_bytes.data() + sent, static_cast<std::size_t>(chunk), 0);
#endif
    if (n <= 0) {
#ifdef _WIN32
      closesocket(sock);
#else
      ::close(sock);
#endif
      return false;
    }
    sent += static_cast<std::size_t>(n);
  }

#ifdef _WIN32
  shutdown(sock, SD_SEND);
#else
  shutdown(sock, SHUT_WR);
#endif

  std::vector<std::uint8_t> buf;
  std::uint8_t tmp[4096];
  while (true) {
#ifdef _WIN32
    const int n = ::recv(sock, reinterpret_cast<char*>(tmp),
                         static_cast<int>(sizeof(tmp)), 0);
#else
    const ssize_t n = ::recv(sock, tmp, sizeof(tmp), 0);
#endif
    if (n > 0) {
      buf.insert(buf.end(), tmp, tmp + n);
      continue;
    }
    if (n == 0) {
      break;
    }
#ifdef _WIN32
    closesocket(sock);
#else
    ::close(sock);
#endif
    return false;
  }

#ifdef _WIN32
  closesocket(sock);
#else
  ::close(sock);
#endif

  out_bytes = std::move(buf);
  return !out_bytes.empty();
}

}  // namespace

ClientCore::ClientCore() = default;

ClientCore::~ClientCore() {
  Logout();
  if (local_handle_) {
    mi_server_destroy(local_handle_);
    local_handle_ = nullptr;
  }
}

bool ClientCore::Init(const std::string& config_path) {
  config_path_ = config_path;
  ClientConfig cfg;
  std::string err;
  remote_mode_ = LoadClientConfig(config_path_, cfg, err);
  if (remote_mode_) {
    server_ip_ = cfg.server_ip;
    server_port_ = cfg.server_port;
    if (local_handle_) {
      mi_server_destroy(local_handle_);
      local_handle_ = nullptr;
    }
    token_.clear();
    send_seq_ = 0;
    recv_seq_ = 0;
    return !server_ip_.empty() && server_port_ != 0;
  }

  server_ip_.clear();
  server_port_ = 0;
  if (local_handle_) {
    mi_server_destroy(local_handle_);
    local_handle_ = nullptr;
  }
  local_handle_ = mi_server_create(config_path.c_str());
  return local_handle_ != nullptr;
}

bool ClientCore::Login(const std::string& username,
                       const std::string& password) {
  username_ = username;
  password_ = password;

  mi::server::Frame login;
  login.type = mi::server::FrameType::kLogin;
  mi::server::proto::WriteString(username, login.payload);
  mi::server::proto::WriteString(password, login.payload);
  const auto bytes = mi::server::EncodeFrame(login);

  std::vector<std::uint8_t> resp_vec;
  if (!ProcessRaw(bytes, resp_vec)) {
    return false;
  }

  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp)) {
    return false;
  }
  if (resp.type != mi::server::FrameType::kLogin || resp.payload.empty()) {
    return false;
  }
  std::size_t off = 1;
  std::string token_or_error;
  if (!mi::server::proto::ReadString(resp.payload, off, token_or_error)) {
    return false;
  }
  if (resp.payload[0] == 0) {
    return false;
  }
  token_ = std::move(token_or_error);

  std::string err;
  if (!mi::server::DeriveKeysFromCredentials(username, password, keys_, err)) {
    token_.clear();
    return false;
  }
  channel_ = mi::server::SecureChannel(keys_);
  send_seq_ = 0;
  recv_seq_ = 0;
  return true;
}

bool ClientCore::Logout() {
  if (token_.empty()) {
    return true;
  }
  std::vector<std::uint8_t> ignore;
  ProcessEncrypted(mi::server::FrameType::kLogout, {}, ignore);
  token_.clear();
  return true;
}

bool ClientCore::EnsureChannel() {
  if (token_.empty()) {
    return false;
  }
  if (remote_mode_) {
    return !server_ip_.empty() && server_port_ != 0;
  }
  return local_handle_ != nullptr;
}

bool ClientCore::ProcessRaw(const std::vector<std::uint8_t>& in_bytes,
                            std::vector<std::uint8_t>& out_bytes) {
  out_bytes.clear();
  if (in_bytes.empty()) {
    return false;
  }
  if (remote_mode_) {
    return TcpRoundTrip(server_ip_, server_port_, in_bytes, out_bytes);
  }
  if (!local_handle_) {
    return false;
  }
  std::uint8_t* resp_buf = nullptr;
  std::size_t resp_len = 0;
  if (!mi_server_process(local_handle_, in_bytes.data(), in_bytes.size(),
                         &resp_buf, &resp_len)) {
    return false;
  }
  out_bytes.assign(resp_buf, resp_buf + resp_len);
  mi_server_free(resp_buf);
  return !out_bytes.empty();
}

bool ClientCore::ProcessEncrypted(mi::server::FrameType type,
                                  const std::vector<std::uint8_t>& plain,
                                  std::vector<std::uint8_t>& out_plain) {
  if (!EnsureChannel()) {
    return false;
  }
  std::vector<std::uint8_t> cipher;
  if (!channel_.Encrypt(send_seq_, plain, cipher)) {
    return false;
  }
  send_seq_++;

  mi::server::Frame f;
  f.type = type;
  mi::server::proto::WriteString(token_, f.payload);
  f.payload.insert(f.payload.end(), cipher.begin(), cipher.end());
  const auto bytes = mi::server::EncodeFrame(f);

  std::vector<std::uint8_t> resp_vec;
  if (!ProcessRaw(bytes, resp_vec)) {
    return false;
  }

  mi::server::Frame resp_frame;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp_frame)) {
    return false;
  }
  std::size_t off = 0;
  std::string resp_token;
  if (!mi::server::proto::ReadString(resp_frame.payload, off, resp_token)) {
    return false;
  }
  if (resp_token != token_) {
    return false;
  }
  std::vector<std::uint8_t> resp_cipher(resp_frame.payload.begin() + off,
                                        resp_frame.payload.end());
  if (!channel_.Decrypt(resp_cipher, recv_seq_, out_plain)) {
    return false;
  }
  recv_seq_++;
  return true;
}

bool ClientCore::JoinGroup(const std::string& group_id) {
  std::vector<std::uint8_t> plain;
  plain.push_back(0);  // join action
  mi::server::proto::WriteString(group_id, plain);
  std::vector<std::uint8_t> resp_plain;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupEvent, plain,
                        resp_plain)) {
    return false;
  }
  return !resp_plain.empty() && resp_plain[0] != 0;
}

bool ClientCore::SendGroupMessage(const std::string& group_id,
                                  std::uint32_t threshold) {
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  mi::server::proto::WriteUint32(threshold, plain);
  std::vector<std::uint8_t> resp_plain;
  if (!ProcessEncrypted(mi::server::FrameType::kMessage, plain, resp_plain)) {
    return false;
  }
  return !resp_plain.empty() && resp_plain[0] != 0;
}

bool ClientCore::SendOffline(const std::string& recipient,
                             const std::vector<std::uint8_t>& payload) {
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(recipient, plain);
  mi::server::proto::WriteBytes(payload, plain);
  std::vector<std::uint8_t> resp_plain;
  if (!ProcessEncrypted(mi::server::FrameType::kOfflinePush, plain,
                        resp_plain)) {
    return false;
  }
  return !resp_plain.empty() && resp_plain[0] == 1;
}

std::vector<std::vector<std::uint8_t>> ClientCore::PullOffline() {
  std::vector<std::vector<std::uint8_t>> messages;
  if (!EnsureChannel()) {
    return messages;
  }

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kOfflinePull, {}, resp_payload)) {
    return messages;
  }
  std::size_t offset = 0;
  if (resp_payload.empty() || resp_payload[0] == 0) {
    return messages;
  }
  offset = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, offset, count)) {
    return messages;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    std::vector<std::uint8_t> msg;
    if (!mi::server::proto::ReadBytes(resp_payload, offset, msg)) {
      break;
    }
    messages.push_back(std::move(msg));
  }
  return messages;
}

std::vector<ClientCore::FriendEntry> ClientCore::ListFriends() {
  std::vector<FriendEntry> out;
  if (!EnsureChannel()) {
    return out;
  }
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kFriendList, {}, resp_payload)) {
    return out;
  }
  if (resp_payload.empty() || resp_payload[0] == 0) {
    return out;
  }
  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    FriendEntry e;
    if (!mi::server::proto::ReadString(resp_payload, off, e.username)) {
      break;
    }
    if (off < resp_payload.size()) {
      std::string remark;
      if (!mi::server::proto::ReadString(resp_payload, off, remark)) {
        break;
      }
      e.remark = std::move(remark);
    }
    out.push_back(std::move(e));
  }
  return out;
}

bool ClientCore::AddFriend(const std::string& friend_username,
                           const std::string& remark) {
  if (!EnsureChannel()) {
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(friend_username, plain);
  mi::server::proto::WriteString(remark, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kFriendAdd, plain,
                        resp_payload)) {
    return false;
  }
  return !resp_payload.empty() && resp_payload[0] == 1;
}

bool ClientCore::SetFriendRemark(const std::string& friend_username,
                                 const std::string& remark) {
  if (!EnsureChannel()) {
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(friend_username, plain);
  mi::server::proto::WriteString(remark, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kFriendRemarkSet, plain,
                        resp_payload)) {
    return false;
  }
  return !resp_payload.empty() && resp_payload[0] == 1;
}

}  // namespace mi::client
