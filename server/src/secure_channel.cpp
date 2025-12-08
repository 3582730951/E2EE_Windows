#include "secure_channel.h"

#include <algorithm>
#include <cstring>

#include "crypto.h"
#include "pake.h"

namespace mi::server {

SecureChannel::SecureChannel(const DerivedKeys& keys) {
  enc_key_ = keys.kcp_key;
  auth_key_ = keys.header_key;
  ratchet_root_ = keys.ratchet_root;
}

bool SecureChannel::DeriveMessageKey(
    const std::array<std::uint8_t, 32>& ratchet_root,
    std::uint64_t counter,
    std::array<std::uint8_t, 32>& out_key) {
  return mi::server::DeriveMessageKey(ratchet_root, counter, out_key);
}

namespace {

constexpr std::size_t kNonceSize = 12;
constexpr std::size_t kTagSize = 32;

void BuildNonce(std::uint64_t seq, std::uint8_t out[kNonceSize]) {
  // 8  + 4 
  for (int i = 0; i < 8; ++i) {
    out[i] = static_cast<std::uint8_t>((seq >> (i * 8)) & 0xFF);
  }
  out[8] = out[9] = out[10] = out[11] = 0;
}

void Keystream(const std::array<std::uint8_t, 32>& key,
               const std::uint8_t* nonce, std::size_t need,
               std::vector<std::uint8_t>& out) {
  out.resize(need);
  std::size_t produced = 0;
  std::uint32_t counter = 0;
  while (produced < need) {
    std::uint8_t block_input[kNonceSize + 4];
    std::memcpy(block_input, nonce, kNonceSize);
    block_input[kNonceSize + 0] = static_cast<std::uint8_t>(counter & 0xFF);
    block_input[kNonceSize + 1] =
        static_cast<std::uint8_t>((counter >> 8) & 0xFF);
    block_input[kNonceSize + 2] =
        static_cast<std::uint8_t>((counter >> 16) & 0xFF);
    block_input[kNonceSize + 3] =
        static_cast<std::uint8_t>((counter >> 24) & 0xFF);
    crypto::Sha256Digest digest;
    crypto::HmacSha256(key.data(), key.size(), block_input,
                       sizeof(block_input), digest);
    const std::size_t to_copy =
        std::min<std::size_t>(digest.bytes.size(), need - produced);
    std::memcpy(out.data() + produced, digest.bytes.data(), to_copy);
    produced += to_copy;
    counter++;
  }
}

bool ComputeTag(const std::array<std::uint8_t, 32>& auth_key,
                const std::vector<std::uint8_t>& nonce_and_cipher,
                std::uint8_t out[kTagSize]) {
  crypto::Sha256Digest tag;
  crypto::HmacSha256(auth_key.data(), auth_key.size(),
                     nonce_and_cipher.data(), nonce_and_cipher.size(), tag);
  std::memcpy(out, tag.bytes.data(), kTagSize);
  return true;
}

}  // namespace

bool SecureChannel::Encrypt(std::uint64_t seq,
                            const std::vector<std::uint8_t>& plaintext,
                            std::vector<std::uint8_t>& out) {
  std::array<std::uint8_t, 32> enc = enc_key_;
  std::array<std::uint8_t, 32> auth = auth_key_;
  DerivePerMessageKeys(seq, enc, auth);

  std::uint8_t nonce[kNonceSize];
  BuildNonce(seq, nonce);

  std::vector<std::uint8_t> ks;
  Keystream(enc, nonce, plaintext.size(), ks);

  std::vector<std::uint8_t> cipher(plaintext.size());
  for (std::size_t i = 0; i < plaintext.size(); ++i) {
    cipher[i] = static_cast<std::uint8_t>(plaintext[i] ^ ks[i]);
  }

  out.clear();
  out.insert(out.end(), nonce, nonce + kNonceSize);
  out.insert(out.end(), cipher.begin(), cipher.end());

  std::uint8_t tag[kTagSize];
  ComputeTag(auth, out, tag);
  out.insert(out.end(), tag, tag + kTagSize);
  return true;
}

bool SecureChannel::Decrypt(const std::vector<std::uint8_t>& input,
                            std::uint64_t expected_seq,
                            std::vector<std::uint8_t>& out_plain) {
  if (input.size() < kNonceSize + kTagSize) {
    return false;
  }
  std::array<std::uint8_t, 32> enc = enc_key_;
  std::array<std::uint8_t, 32> auth = auth_key_;
  DerivePerMessageKeys(expected_seq, enc, auth);

  const std::size_t cipher_len = input.size() - kNonceSize - kTagSize;
  const std::uint8_t* nonce = input.data();
  const std::uint8_t* cipher = input.data() + kNonceSize;
  const std::uint8_t* tag = input.data() + kNonceSize + cipher_len;

  // Optional: verify seq matches
  std::uint64_t seq = 0;
  for (int i = 0; i < 8; ++i) {
    seq |= static_cast<std::uint64_t>(nonce[i]) << (i * 8);
  }
  if (seq != expected_seq) {
    return false;
  }

  std::vector<std::uint8_t> mac_input;
  mac_input.insert(mac_input.end(), nonce, nonce + kNonceSize);
  mac_input.insert(mac_input.end(), cipher, cipher + cipher_len);
  std::uint8_t calc[kTagSize];
  ComputeTag(auth, mac_input, calc);
  if (std::memcmp(calc, tag, kTagSize) != 0) {
    return false;
  }

  std::vector<std::uint8_t> ks;
  Keystream(enc, nonce, cipher_len, ks);
  out_plain.resize(cipher_len);
  for (std::size_t i = 0; i < cipher_len; ++i) {
    out_plain[i] = static_cast<std::uint8_t>(cipher[i] ^ ks[i]);
  }
  return true;
}

bool SecureChannel::HasRatchetRoot() const {
  for (auto b : ratchet_root_) {
    if (b != 0) {
      return true;
    }
  }
  return false;
}

bool SecureChannel::DerivePerMessageKeys(
    std::uint64_t seq,
    std::array<std::uint8_t, 32>& enc_key,
    std::array<std::uint8_t, 32>& auth_key) const {
  //  ratchet_root enc/auth
  if (!HasRatchetRoot()) {
    return true;
  }
  std::uint8_t info[12];
  for (int i = 0; i < 8; ++i) {
    info[i] = static_cast<std::uint8_t>((seq >> (i * 8)) & 0xFF);
  }
  info[8] = info[9] = info[10] = info[11] = 0;
  std::array<std::uint8_t, 64> buf{};
  const bool ok = crypto::HkdfSha256(
      ratchet_root_.data(), ratchet_root_.size(),
      nullptr, 0,
      info, sizeof(info),
      buf.data(), buf.size());
  if (!ok) {
    return false;
  }
  std::memcpy(enc_key.data(), buf.data(), 32);
  std::memcpy(auth_key.data(), buf.data() + 32, 32);
  return true;
}

}  // namespace mi::server
