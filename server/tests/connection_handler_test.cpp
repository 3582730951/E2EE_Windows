#include <cassert>
#include <fstream>
#include <string>
#include <vector>

#include "connection_handler.h"
#include "frame.h"
#include "key_transparency.h"
#include "protocol.h"
#include "secure_channel.h"
#include "server_app.h"

using mi::server::ConnectionHandler;
using mi::server::Frame;
using mi::server::FrameType;
using mi::server::SecureChannel;
using mi::server::SecureChannelRole;
using mi::server::ServerApp;
using mi::server::proto::WriteString;
using mi::server::proto::WriteUint32;

static void WriteFile(const std::string& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  f << content;
}

int main() {
  WriteFile("config.ini",
            "[mode]\nmode=1\n[server]\nlist_port=7778\n"
            "offline_dir=.\n"
            "tls_enable=1\n"
            "require_tls=1\n"
            "tls_cert=mi_e2ee_server.pfx\n"
            "kt_signing_key=kt_signing_key.bin\n");
  WriteFile("test_user.txt", "u1:p1\n");
  {
    std::vector<std::uint8_t> key(mi::server::kKtSthSigSecretKeyBytes, 0x22);
    std::ofstream kf("kt_signing_key.bin", std::ios::binary | std::ios::trunc);
    if (!kf) {
      return 1;
    }
    kf.write(reinterpret_cast<const char*>(key.data()),
             static_cast<std::streamsize>(key.size()));
  }

  ServerApp app;
  std::string err;
  bool ok = app.Init("config.ini", err);
  if (!ok) {
    return 1;
  }

  ConnectionHandler handler(&app);

  Frame login;
  login.type = FrameType::kLogin;
  WriteString("u1", login.payload);
  WriteString("p1", login.payload);
  auto bytes = mi::server::EncodeFrame(login);

  std::vector<std::uint8_t> resp_bytes;
  ok = handler.OnData(bytes.data(), bytes.size(), resp_bytes, "127.0.0.1",
                      mi::server::TransportKind::kTcp);
  if (!ok) {
    return 1;
  }

  Frame tls_resp;
  ok = mi::server::DecodeFrame(resp_bytes.data(), resp_bytes.size(), tls_resp);
  if (!ok || tls_resp.type != FrameType::kLogin || tls_resp.payload.empty() ||
      tls_resp.payload[0] != 0) {
    return 1;
  }

  resp_bytes.clear();
  ok = handler.OnData(bytes.data(), bytes.size(), resp_bytes, {});
  if (!ok) {
    return 1;
  }

  Frame resp;
  ok = mi::server::DecodeFrame(resp_bytes.data(), resp_bytes.size(), resp);
  if (!ok || resp.type != FrameType::kLogin || resp.payload.empty() ||
      resp.payload[0] != 1) {
    return 1;
  }

  // Extract token for group ops
  std::size_t off = 1;
  std::string token;
  ok = mi::server::proto::ReadString(resp.payload, off, token);
  if (!ok || token.empty()) {
    return 1;
  }

  // Prepare secure channel using session keys
  auto keys = app.sessions()->GetKeys(token);
  if (!keys.has_value()) {
    return 1;
  }
  SecureChannel ch(*keys, SecureChannelRole::kClient);

  // Build encrypted group join frame: payload = token (clear) + cipher(action+gid)
  std::vector<std::uint8_t> plain_join;
  plain_join.push_back(0);  // action join
  mi::server::proto::WriteString("g1", plain_join);
  std::vector<std::uint8_t> cipher_join;
  ok = ch.Encrypt(0, FrameType::kGroupEvent, plain_join, cipher_join);
  if (!ok) {
    return 1;
  }

  Frame group_join;
  group_join.type = FrameType::kGroupEvent;
  mi::server::proto::WriteString(token, group_join.payload);
  group_join.payload.insert(group_join.payload.end(), cipher_join.begin(),
                            cipher_join.end());
  auto join_bytes = mi::server::EncodeFrame(group_join);

  resp_bytes.clear();
  ok = handler.OnData(join_bytes.data(), join_bytes.size(), resp_bytes, {});
  if (!ok) {
    return 1;
  }

  Frame join_resp;
  ok = mi::server::DecodeFrame(resp_bytes.data(), resp_bytes.size(), join_resp);
  if (!ok || join_resp.type != FrameType::kGroupEvent) {
    return 1;
  }
  // Decrypt response payload
  std::size_t off2 = 0;
  std::string resp_token;
  ok = mi::server::proto::ReadString(join_resp.payload, off2, resp_token);
  if (!ok) {
    return 1;
  }
  std::vector<std::uint8_t> resp_cipher(join_resp.payload.begin() + off2,
                                        join_resp.payload.end());
  std::vector<std::uint8_t> resp_plain;
  ok = ch.Decrypt(resp_cipher, join_resp.type, resp_plain);
  if (!ok || resp_plain.empty() || resp_plain[0] != 1) {
    return 1;
  }

  // Group message triggers rotation with threshold 1
  std::vector<std::uint8_t> plain_msg;
  mi::server::proto::WriteString("g1", plain_msg);
  mi::server::proto::WriteUint32(1, plain_msg);
  std::vector<std::uint8_t> cipher_msg;
  ok = ch.Encrypt(1, FrameType::kMessage, plain_msg, cipher_msg);
  if (!ok) {
    return 1;
  }

  Frame msg_frame;
  msg_frame.type = FrameType::kMessage;
  mi::server::proto::WriteString(token, msg_frame.payload);
  msg_frame.payload.insert(msg_frame.payload.end(), cipher_msg.begin(),
                           cipher_msg.end());
  auto msg_bytes = mi::server::EncodeFrame(msg_frame);

  resp_bytes.clear();
  ok = handler.OnData(msg_bytes.data(), msg_bytes.size(), resp_bytes, {});
  if (!ok) {
    return 1;
  }

  Frame msg_resp;
  ok = mi::server::DecodeFrame(resp_bytes.data(), resp_bytes.size(), msg_resp);
  if (!ok) {
    return 1;
  }
  std::size_t resp_off = 0;
  std::string resp_token2;
  ok = mi::server::proto::ReadString(msg_resp.payload, resp_off, resp_token2);
  if (!ok) {
    return 1;
  }
  std::vector<std::uint8_t> msg_resp_cipher(msg_resp.payload.begin() + resp_off,
                                            msg_resp.payload.end());
  std::vector<std::uint8_t> msg_resp_plain;
  ok = ch.Decrypt(msg_resp_cipher, msg_resp.type, msg_resp_plain);
  if (!ok || msg_resp_plain.empty() || msg_resp_plain[0] != 1) {
    return 1;
  }

  // Regression: decrypt failure must not reset server->client seq to 0.
  std::vector<std::uint8_t> bad_cipher;
  ok = ch.Encrypt(2, FrameType::kFriendList, {}, bad_cipher);
  if (!ok || bad_cipher.empty()) {
    return 1;
  }
  bad_cipher.back() ^= 0x01;
  Frame bad;
  bad.type = FrameType::kFriendList;
  mi::server::proto::WriteString(token, bad.payload);
  bad.payload.insert(bad.payload.end(), bad_cipher.begin(), bad_cipher.end());
  const auto bad_bytes = mi::server::EncodeFrame(bad);

  resp_bytes.clear();
  ok = handler.OnData(bad_bytes.data(), bad_bytes.size(), resp_bytes, {});
  if (!ok) {
    return 1;
  }

  std::vector<std::uint8_t> list_cipher;
  ok = ch.Encrypt(3, FrameType::kFriendList, {}, list_cipher);
  if (!ok) {
    return 1;
  }
  Frame list_req;
  list_req.type = FrameType::kFriendList;
  mi::server::proto::WriteString(token, list_req.payload);
  list_req.payload.insert(list_req.payload.end(), list_cipher.begin(),
                          list_cipher.end());
  const auto list_bytes = mi::server::EncodeFrame(list_req);

  resp_bytes.clear();
  ok = handler.OnData(list_bytes.data(), list_bytes.size(), resp_bytes, {});
  if (!ok) {
    return 1;
  }
  Frame list_resp;
  ok = mi::server::DecodeFrame(resp_bytes.data(), resp_bytes.size(), list_resp);
  if (!ok || list_resp.type != FrameType::kFriendList) {
    return 1;
  }
  std::size_t list_off = 0;
  std::string list_token;
  ok = mi::server::proto::ReadString(list_resp.payload, list_off, list_token);
  if (!ok || list_token != token) {
    return 1;
  }
  std::vector<std::uint8_t> list_resp_cipher(list_resp.payload.begin() + list_off,
                                             list_resp.payload.end());
  std::size_t seq_off = 0;
  std::uint64_t server_seq = 0;
  ok = mi::server::proto::ReadUint64(list_resp_cipher, seq_off, server_seq);
  if (!ok || server_seq != 2) {
    return 1;
  }
  std::vector<std::uint8_t> list_plain;
  ok = ch.Decrypt(list_resp_cipher, list_resp.type, list_plain);
  if (!ok || list_plain.empty() || list_plain[0] != 1) {
    return 1;
  }

  return 0;
}
