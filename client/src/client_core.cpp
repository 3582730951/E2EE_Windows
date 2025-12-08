#include "client_core.h"

#include <cstdlib>
#include <cstring>

#include "../server/include/c_api.h"
#include "../server/include/frame.h"
#include "../server/include/protocol.h"
#include "client_config.h"

namespace mi::client {

ClientCore::ClientCore() = default;

ClientCore::~ClientCore() {
  Logout();
  if (handle_) {
    mi_server_destroy(handle_);
    handle_ = nullptr;
  }
}

bool ClientCore::Init(const std::string& config_path) {
  config_path_ = config_path;
  ClientConfig cfg;
  std::string err;
  if (!LoadClientConfig(config_path_, cfg, err)) {
    // 允许继续，后续可按需使用 server_ip/server_port
  }
  handle_ = mi_server_create(config_path.c_str());
  return handle_ != nullptr;
}

bool ClientCore::Login(const std::string& username,
                       const std::string& password) {
  if (!handle_) {
    return false;
  }
  username_ = username;
  password_ = password;

  char* out_token = nullptr;
  if (!mi_server_login(handle_, username.c_str(), password.c_str(),
                       &out_token)) {
    return false;
  }
  token_.assign(out_token);
  mi_server_free(reinterpret_cast<std::uint8_t*>(out_token));

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
  if (!handle_ || token_.empty()) {
    return true;
  }
  mi_server_logout(handle_, token_.c_str());
  token_.clear();
  return true;
}

bool ClientCore::EnsureChannel() {
  return handle_ && !token_.empty();
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
  auto bytes = mi::server::EncodeFrame(f);

  std::uint8_t* resp_buf = nullptr;
  std::size_t resp_len = 0;
  if (!mi_server_process(handle_, bytes.data(), bytes.size(), &resp_buf,
                         &resp_len)) {
    return false;
  }

  std::vector<std::uint8_t> resp_vec(resp_buf, resp_buf + resp_len);
  mi_server_free(resp_buf);

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
  if (!EnsureChannel()) {
    return false;
  }
  mi::server::Frame f;
  f.type = mi::server::FrameType::kOfflinePush;
  mi::server::proto::WriteString(token_, f.payload);
  mi::server::proto::WriteString(recipient, f.payload);
  mi::server::proto::WriteBytes(payload, f.payload);
  auto bytes = mi::server::EncodeFrame(f);

  std::uint8_t* resp_buf = nullptr;
  std::size_t resp_len = 0;
  if (!mi_server_process(handle_, bytes.data(), bytes.size(), &resp_buf,
                         &resp_len)) {
    return false;
  }
  std::vector<std::uint8_t> resp_vec(resp_buf, resp_buf + resp_len);
  mi_server_free(resp_buf);

  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp)) {
    return false;
  }
  return !resp.payload.empty() && resp.payload[0] == 1;
}

std::vector<std::vector<std::uint8_t>> ClientCore::PullOffline() {
  std::vector<std::vector<std::uint8_t>> messages;
  if (!EnsureChannel()) {
    return messages;
  }
  mi::server::Frame f;
  f.type = mi::server::FrameType::kOfflinePull;
  mi::server::proto::WriteString(token_, f.payload);
  auto bytes = mi::server::EncodeFrame(f);

  std::uint8_t* resp_buf = nullptr;
  std::size_t resp_len = 0;
  if (!mi_server_process(handle_, bytes.data(), bytes.size(), &resp_buf,
                         &resp_len)) {
    return messages;
  }
  std::vector<std::uint8_t> resp_vec(resp_buf, resp_buf + resp_len);
  mi_server_free(resp_buf);

  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp)) {
    return messages;
  }
  std::size_t offset = 0;
  if (resp.payload.empty() || resp.payload[0] == 0) {
    return messages;
  }
  offset = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp.payload, offset, count)) {
    return messages;
  }
  for (std::uint32_t i = 0; i < count; ++i) {
    std::vector<std::uint8_t> msg;
    if (!mi::server::proto::ReadBytes(resp.payload, offset, msg)) {
      break;
    }
    messages.push_back(std::move(msg));
  }
  return messages;
}

}  // namespace mi::client
