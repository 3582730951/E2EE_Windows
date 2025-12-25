#include "secure_channel.h"

#include <algorithm>
#include <cstring>

#include "monocypher.h"

namespace mi::server {

namespace {
constexpr std::size_t kSeqHeaderSize = 8;
constexpr std::size_t kNonceSize = 24;
constexpr std::size_t kTagSize = 16;
constexpr std::size_t kReplayWindowBits = 64;

void StoreLe64(std::uint64_t v, std::uint8_t out[8]) {
  for (int i = 0; i < 8; ++i) {
    out[i] = static_cast<std::uint8_t>((v >> (i * 8)) & 0xFF);
  }
}

std::uint64_t LoadLe64(const std::uint8_t in[8]) {
  std::uint64_t v = 0;
  for (int i = 0; i < 8; ++i) {
    v |= static_cast<std::uint64_t>(in[i]) << (i * 8);
  }
  return v;
}

void BuildNonce(std::uint64_t seq, std::uint8_t out[kNonceSize]) {
  StoreLe64(seq, out);
  std::memset(out + 8, 0, kNonceSize - 8);
}

void BuildAd(FrameType frame_type, std::uint64_t seq,
             std::uint8_t out[2 + kSeqHeaderSize]) {
  const std::uint16_t t = static_cast<std::uint16_t>(frame_type);
  out[0] = static_cast<std::uint8_t>(t & 0xFF);
  out[1] = static_cast<std::uint8_t>((t >> 8) & 0xFF);
  StoreLe64(seq, out + 2);
}

void DeriveDirectionalKey(const std::array<std::uint8_t, 32>& base_key,
                          const char* label,
                          std::array<std::uint8_t, 32>& out_key) {
  crypto_blake2b_keyed(out_key.data(), out_key.size(),
                       base_key.data(), base_key.size(),
                       reinterpret_cast<const std::uint8_t*>(label),
                       std::strlen(label));
}

}  // namespace

SecureChannel::SecureChannel(const DerivedKeys& keys, SecureChannelRole role) {
  std::array<std::uint8_t, 32> c2s{};
  std::array<std::uint8_t, 32> s2c{};
  DeriveDirectionalKey(keys.kcp_key, "mi_e2ee_secure_channel_v2_c2s", c2s);
  DeriveDirectionalKey(keys.kcp_key, "mi_e2ee_secure_channel_v2_s2c", s2c);
  if (role == SecureChannelRole::kClient) {
    tx_key_ = c2s;
    rx_key_ = s2c;
  } else {
    tx_key_ = s2c;
    rx_key_ = c2s;
  }
}

bool SecureChannel::Encrypt(std::uint64_t seq,
                            FrameType frame_type,
                            const std::vector<std::uint8_t>& plaintext,
                            std::vector<std::uint8_t>& out) {
  std::uint8_t nonce[kNonceSize];
  BuildNonce(seq, nonce);
  std::uint8_t ad[2 + kSeqHeaderSize];
  BuildAd(frame_type, seq, ad);

  out.resize(kSeqHeaderSize + plaintext.size() + kTagSize);
  StoreLe64(seq, out.data());
  std::uint8_t* cipher = out.data() + kSeqHeaderSize;
  std::uint8_t* mac = out.data() + kSeqHeaderSize + plaintext.size();
  const std::uint8_t* plain = plaintext.empty() ? nullptr : plaintext.data();
  crypto_aead_lock(cipher, mac, tx_key_.data(), nonce, ad, sizeof(ad), plain,
                   plaintext.size());
  return true;
}

bool SecureChannel::CanAcceptSeq(std::uint64_t seq) const {
  if (!recv_inited_) {
    return true;
  }
  if (seq > recv_max_seq_) {
    return true;
  }
  const std::uint64_t diff = recv_max_seq_ - seq;
  if (diff >= kReplayWindowBits) {
    return false;
  }
  return ((recv_window_ >> diff) & 1ULL) == 0;
}

void SecureChannel::MarkSeqReceived(std::uint64_t seq) {
  if (!recv_inited_) {
    recv_inited_ = true;
    recv_max_seq_ = seq;
    recv_window_ = 1ULL;
    return;
  }
  if (seq > recv_max_seq_) {
    const std::uint64_t shift = seq - recv_max_seq_;
    if (shift >= kReplayWindowBits) {
      recv_window_ = 1ULL;
    } else {
      recv_window_ = (recv_window_ << shift) | 1ULL;
    }
    recv_max_seq_ = seq;
    return;
  }
  const std::uint64_t diff = recv_max_seq_ - seq;
  if (diff < kReplayWindowBits) {
    recv_window_ |= (1ULL << diff);
  }
}

bool SecureChannel::Decrypt(const std::vector<std::uint8_t>& input,
                            FrameType frame_type,
                            std::vector<std::uint8_t>& out_plain) {
  return Decrypt(input.data(), input.size(), frame_type, out_plain);
}

bool SecureChannel::Decrypt(const std::uint8_t* input, std::size_t len,
                            FrameType frame_type,
                            std::vector<std::uint8_t>& out_plain) {
  out_plain.clear();
  if (!input || len < kSeqHeaderSize + kTagSize) {
    return false;
  }
  const std::uint64_t seq = LoadLe64(input);
  if (!CanAcceptSeq(seq)) {
    return false;
  }
  const std::size_t cipher_len = len - kSeqHeaderSize - kTagSize;
  const std::uint8_t* cipher = input + kSeqHeaderSize;
  const std::uint8_t* mac = input + kSeqHeaderSize + cipher_len;

  std::uint8_t nonce[kNonceSize];
  BuildNonce(seq, nonce);
  std::uint8_t ad[2 + kSeqHeaderSize];
  BuildAd(frame_type, seq, ad);

  out_plain.resize(cipher_len);
  std::uint8_t* plain = cipher_len == 0 ? nullptr : out_plain.data();
  const int ok = crypto_aead_unlock(plain, mac, rx_key_.data(), nonce, ad,
                                   sizeof(ad), cipher, cipher_len);
  if (ok != 0) {
    out_plain.clear();
    return false;
  }
  MarkSeqReceived(seq);
  return true;
}

}  // namespace mi::server
