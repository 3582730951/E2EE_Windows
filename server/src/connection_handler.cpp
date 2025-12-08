#include "connection_handler.h"

#include <vector>
#include <unordered_map>
#include <string>

#include "protocol.h"
#include "secure_channel.h"

namespace mi::server {

ConnectionHandler::ConnectionHandler(ServerApp* app)
    : app_(app) {}

namespace {
struct ChannelState {
  SecureChannel channel;
  std::uint64_t send_seq{0};
  std::uint64_t recv_seq{0};
};
}  // namespace

bool ConnectionHandler::OnData(const std::uint8_t* data, std::size_t len,
                               std::vector<std::uint8_t>& out_bytes) {
  if (!app_) {
    return false;
  }
  Frame in;
  if (!DecodeFrame(data, len, in)) {
    return false;
  }
  Frame out;
  std::string error;

  if (in.type == FrameType::kLogin) {
    if (!app_->HandleFrame(in, out, error)) {
      return false;
    }
    out_bytes = EncodeFrame(out);
    return true;
  }

  // payload = token_len(2) + token(utf8) + cipher
  std::size_t offset = 0;
  std::string token;
  if (!proto::ReadString(in.payload, offset, token)) {
    return false;
  }

  const std::vector<std::uint8_t> cipher(in.payload.begin() + offset,
                                         in.payload.end());
  if (cipher.empty()) {
    return false;
  }

  ChannelState& state = channel_states_[token];
  if (state.send_seq == 0 && state.recv_seq == 0) {
    auto keys = app_->sessions()->GetKeys(token);
    if (!keys.has_value()) {
      channel_states_.erase(token);
      return false;
    }
    state.channel = SecureChannel(*keys);
  }

  std::vector<std::uint8_t> plain;
  if (!state.channel.Decrypt(cipher, state.recv_seq, plain)) {
    return false;
  }
  state.recv_seq++;

  Frame inner;
  inner.type = in.type;
  inner.payload = plain;

  if (!app_->HandleFrameWithToken(inner, out, token, error)) {
    return false;
  }

  std::vector<std::uint8_t> cipher_out;
  if (!state.channel.Encrypt(state.send_seq, out.payload, cipher_out)) {
    return false;
  }
  state.send_seq++;

  Frame envelope;
  envelope.type = out.type;
  envelope.payload.reserve(token.size() + 2 + cipher_out.size());
  proto::WriteString(token, envelope.payload);
  envelope.payload.insert(envelope.payload.end(), cipher_out.begin(),
                          cipher_out.end());

  out_bytes = EncodeFrame(envelope);
  if (out.type == FrameType::kLogout) {
    channel_states_.erase(token);
  }
  return true;
}

}  // namespace mi::server
