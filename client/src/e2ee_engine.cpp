#include "e2ee_engine.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <random>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#endif

#include "crypto.h"
#include "monocypher.h"

extern "C" {
int PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair(std::uint8_t* pk,
                                             std::uint8_t* sk);
int PQCLEAN_MLKEM768_CLEAN_crypto_kem_enc(std::uint8_t* ct, std::uint8_t* ss,
                                         const std::uint8_t* pk);
int PQCLEAN_MLKEM768_CLEAN_crypto_kem_dec(std::uint8_t* ss,
                                         const std::uint8_t* ct,
                                         const std::uint8_t* sk);

int PQCLEAN_MLDSA65_CLEAN_crypto_sign_keypair(std::uint8_t* pk,
                                             std::uint8_t* sk);
int PQCLEAN_MLDSA65_CLEAN_crypto_sign_signature(std::uint8_t* sig,
                                               std::size_t* siglen,
                                               const std::uint8_t* m,
                                               std::size_t mlen,
                                               const std::uint8_t* sk);
int PQCLEAN_MLDSA65_CLEAN_crypto_sign_verify(const std::uint8_t* sig,
                                            std::size_t siglen,
                                            const std::uint8_t* m,
                                            std::size_t mlen,
                                            const std::uint8_t* pk);
}

namespace mi::client::e2ee {

namespace {

std::string Trim(const std::string& input) {
  const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  auto begin = std::find_if_not(input.begin(), input.end(), is_space);
  auto end = std::find_if_not(input.rbegin(), input.rend(), is_space).base();
  if (begin >= end) {
    return {};
  }
  return std::string(begin, end);
}

std::string StripInlineComment(const std::string& input) {
  for (std::size_t i = 0; i < input.size(); ++i) {
    const char ch = input[i];
    if ((ch == '#' || ch == ';') &&
        (i == 0 ||
         std::isspace(static_cast<unsigned char>(input[i - 1])) != 0)) {
      return Trim(input.substr(0, i));
    }
  }
  return input;
}

bool RandomBytes(std::uint8_t* out, std::size_t len) {
  if (!out || len == 0) {
    return false;
  }
#ifdef _WIN32
  return BCryptGenRandom(nullptr, out, static_cast<ULONG>(len),
                         BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
  std::random_device rd;
  for (std::size_t i = 0; i < len; ++i) {
    out[i] = static_cast<std::uint8_t>(rd());
  }
  return true;
#endif
}

std::string Sha256Hex(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return {};
  }
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(data, len, d);
  static constexpr char kHex[] = "0123456789abcdef";
  std::string out;
  out.resize(d.bytes.size() * 2);
  for (std::size_t i = 0; i < d.bytes.size(); ++i) {
    out[i * 2] = kHex[d.bytes[i] >> 4];
    out[i * 2 + 1] = kHex[d.bytes[i] & 0x0F];
  }
  return out;
}

int HexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

bool HexToBytes(const std::string& hex, std::vector<std::uint8_t>& out) {
  out.clear();
  if ((hex.size() % 2) != 0) {
    return false;
  }
  out.reserve(hex.size() / 2);
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    const int hi = HexNibble(hex[i]);
    const int lo = HexNibble(hex[i + 1]);
    if (hi < 0 || lo < 0) {
      out.clear();
      return false;
    }
    out.push_back(
        static_cast<std::uint8_t>((static_cast<unsigned>(hi) << 4) |
                                  static_cast<unsigned>(lo)));
  }
  return true;
}

std::string GroupHex4(const std::string& hex) {
  if (hex.empty()) {
    return {};
  }
  std::string out;
  out.reserve(hex.size() + (hex.size() / 4));
  for (std::size_t i = 0; i < hex.size(); ++i) {
    if (i != 0 && (i % 4) == 0) {
      out.push_back('-');
    }
    out.push_back(hex[i]);
  }
  return out;
}

std::string NormalizeCode(const std::string& input) {
  std::string out;
  out.reserve(input.size());
  for (char c : input) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (std::isspace(uc) != 0 || c == '-') {
      continue;
    }
    if (c >= 'A' && c <= 'Z') {
      out.push_back(static_cast<char>(c - 'A' + 'a'));
      continue;
    }
    out.push_back(c);
  }
  return out;
}

std::string Sas80HexFromFingerprint(const std::string& sha256_hex) {
  std::vector<std::uint8_t> fp_bytes;
  if (!HexToBytes(sha256_hex, fp_bytes) || fp_bytes.size() != 32) {
    return {};
  }

  std::vector<std::uint8_t> msg;
  static constexpr char kPrefix[] = "MI_PEER_ID_SAS_V1";
  msg.insert(msg.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  msg.insert(msg.end(), fp_bytes.begin(), fp_bytes.end());

  const std::string h = Sha256Hex(msg.data(), msg.size());
  if (h.size() < 20) {
    return {};
  }
  return GroupHex4(h.substr(0, 20));
}

std::string FingerprintPeer(const std::uint8_t* id_sig_pk,
                            std::size_t id_sig_pk_len,
                            const std::array<std::uint8_t, 32>& id_dh_pk) {
  if (!id_sig_pk || id_sig_pk_len == 0) {
    return {};
  }
  std::vector<std::uint8_t> buf;
  buf.reserve(id_sig_pk_len + id_dh_pk.size());
  buf.insert(buf.end(), id_sig_pk, id_sig_pk + id_sig_pk_len);
  buf.insert(buf.end(), id_dh_pk.begin(), id_dh_pk.end());
  return Sha256Hex(buf.data(), buf.size());
}

bool ReadAll(const std::filesystem::path& path, std::vector<std::uint8_t>& out,
             std::string& error) {
  error.clear();
  out.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

bool WriteAll(const std::filesystem::path& path,
              const std::vector<std::uint8_t>& data, std::string& error) {
  error.clear();
  std::error_code ec;
  const auto dir = path.has_parent_path() ? path.parent_path()
                                          : std::filesystem::path{};
  if (!dir.empty()) {
    std::filesystem::create_directories(dir, ec);
  }
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "write failed";
    return false;
  }
  if (!data.empty()) {
    f.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
  }
  f.close();
  return true;
}

#ifdef _WIN32
bool StartsWithBytes(const std::vector<std::uint8_t>& data,
                     const char* prefix, std::size_t prefix_len) {
  if (!prefix || prefix_len == 0 || data.size() < prefix_len) {
    return false;
  }
  return std::memcmp(data.data(), prefix, prefix_len) == 0;
}

bool MaybeUnprotectIdentityDpapi(const std::vector<std::uint8_t>& in,
                                std::vector<std::uint8_t>& out_plain,
                                bool& out_was_dpapi,
                                std::string& error) {
  error.clear();
  out_plain.clear();
  out_was_dpapi = false;
  static constexpr char kMagic[] = "MI_E2EE_IDENTITY_DPAPI1";
  if (!StartsWithBytes(in, kMagic, sizeof(kMagic) - 1)) {
    out_plain = in;
    return true;
  }
  if (in.size() < (sizeof(kMagic) - 1 + 4)) {
    error = "identity dpapi header truncated";
    return false;
  }
  std::size_t off = sizeof(kMagic) - 1;
  const std::uint32_t blob_len =
      static_cast<std::uint32_t>(in[off]) |
      (static_cast<std::uint32_t>(in[off + 1]) << 8) |
      (static_cast<std::uint32_t>(in[off + 2]) << 16) |
      (static_cast<std::uint32_t>(in[off + 3]) << 24);
  off += 4;
  if (off + blob_len != in.size()) {
    error = "identity dpapi size invalid";
    return false;
  }

  DATA_BLOB blob_in;
  blob_in.cbData = blob_len;
  blob_in.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(in.data() + off));

  static constexpr char kEntropy[] = "MI_E2EE_IDENTITY_ENTROPY_V1";
  DATA_BLOB entropy;
  entropy.cbData = static_cast<DWORD>(sizeof(kEntropy) - 1);
  entropy.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(kEntropy));

  DATA_BLOB blob_out;
  blob_out.cbData = 0;
  blob_out.pbData = nullptr;
  const BOOL ok = CryptUnprotectData(&blob_in, nullptr, &entropy, nullptr,
                                    nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                                    &blob_out);
  if (!ok || !blob_out.pbData || blob_out.cbData == 0) {
    error = "CryptUnprotectData failed";
    if (blob_out.pbData) {
      LocalFree(blob_out.pbData);
    }
    return false;
  }
  out_plain.assign(blob_out.pbData, blob_out.pbData + blob_out.cbData);
  LocalFree(blob_out.pbData);
  out_was_dpapi = true;
  return true;
}

bool ProtectIdentityDpapi(const std::vector<std::uint8_t>& plain,
                          std::vector<std::uint8_t>& out_wrapped,
                          std::string& error) {
  error.clear();
  out_wrapped.clear();
  if (plain.empty()) {
    error = "identity empty";
    return false;
  }

  DATA_BLOB blob_in;
  blob_in.cbData = static_cast<DWORD>(plain.size());
  blob_in.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(plain.data()));

  static constexpr char kEntropy[] = "MI_E2EE_IDENTITY_ENTROPY_V1";
  DATA_BLOB entropy;
  entropy.cbData = static_cast<DWORD>(sizeof(kEntropy) - 1);
  entropy.pbData = const_cast<BYTE*>(reinterpret_cast<const BYTE*>(kEntropy));

  DATA_BLOB blob_out;
  blob_out.cbData = 0;
  blob_out.pbData = nullptr;
  const BOOL ok = CryptProtectData(&blob_in, nullptr, &entropy, nullptr, nullptr,
                                  CRYPTPROTECT_UI_FORBIDDEN, &blob_out);
  if (!ok || !blob_out.pbData || blob_out.cbData == 0) {
    error = "CryptProtectData failed";
    if (blob_out.pbData) {
      LocalFree(blob_out.pbData);
    }
    return false;
  }

  static constexpr char kMagic[] = "MI_E2EE_IDENTITY_DPAPI1";
  out_wrapped.reserve((sizeof(kMagic) - 1) + 4 + blob_out.cbData);
  out_wrapped.insert(out_wrapped.end(), kMagic, kMagic + sizeof(kMagic) - 1);
  const std::uint32_t len = static_cast<std::uint32_t>(blob_out.cbData);
  out_wrapped.push_back(static_cast<std::uint8_t>(len & 0xFF));
  out_wrapped.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
  out_wrapped.push_back(static_cast<std::uint8_t>((len >> 16) & 0xFF));
  out_wrapped.push_back(static_cast<std::uint8_t>((len >> 24) & 0xFF));
  out_wrapped.insert(out_wrapped.end(), blob_out.pbData,
                     blob_out.pbData + blob_out.cbData);
  LocalFree(blob_out.pbData);
  return true;
}
#endif

bool HkdfSha256(const std::uint8_t* ikm, std::size_t ikm_len,
                const std::uint8_t* salt, std::size_t salt_len,
                const char* info, std::vector<std::uint8_t>& out,
                std::size_t out_len) {
  out.clear();
  out.resize(out_len);
  return mi::server::crypto::HkdfSha256(
      ikm, ikm_len, salt, salt_len,
      reinterpret_cast<const std::uint8_t*>(info), std::strlen(info),
      out.data(), out.size());
}

bool KdfRk(const std::array<std::uint8_t, 32>& rk,
           const std::array<std::uint8_t, 32>& dh,
           std::array<std::uint8_t, 32>& out_rk,
           std::array<std::uint8_t, 32>& out_ck) {
  std::vector<std::uint8_t> buf;
  if (!HkdfSha256(dh.data(), dh.size(), rk.data(), rk.size(),
                  "mi_e2ee_dr_rk_v1", buf, 64)) {
    return false;
  }
  std::memcpy(out_rk.data(), buf.data(), 32);
  std::memcpy(out_ck.data(), buf.data() + 32, 32);
  return true;
}

bool KdfRkHybrid(const std::array<std::uint8_t, 32>& rk,
                 const std::array<std::uint8_t, 32>& dh,
                 const std::array<std::uint8_t, 32>& kem_ss,
                 std::array<std::uint8_t, 32>& out_rk,
                 std::array<std::uint8_t, 32>& out_ck) {
  std::array<std::uint8_t, 64> ikm{};
  std::memcpy(ikm.data(), dh.data(), 32);
  std::memcpy(ikm.data() + 32, kem_ss.data(), kem_ss.size());
  std::vector<std::uint8_t> buf;
  if (!HkdfSha256(ikm.data(), ikm.size(), rk.data(), rk.size(),
                  "mi_e2ee_dr_rk_hybrid_v1", buf, 64)) {
    return false;
  }
  std::memcpy(out_rk.data(), buf.data(), 32);
  std::memcpy(out_ck.data(), buf.data() + 32, 32);
  return true;
}

bool KdfCk(const std::array<std::uint8_t, 32>& ck,
           std::array<std::uint8_t, 32>& out_ck,
           std::array<std::uint8_t, 32>& out_mk) {
  std::vector<std::uint8_t> buf;
  if (!HkdfSha256(ck.data(), ck.size(), nullptr, 0, "mi_e2ee_dr_ck_v1", buf, 64)) {
    return false;
  }
  std::memcpy(out_ck.data(), buf.data(), 32);
  std::memcpy(out_mk.data(), buf.data() + 32, 32);
  return true;
}

std::array<std::uint8_t, 32> X25519(const std::array<std::uint8_t, 32>& sk,
                                    const std::array<std::uint8_t, 32>& pk) {
  std::array<std::uint8_t, 32> out{};
  crypto_x25519(out.data(), sk.data(), pk.data());
  return out;
}

std::vector<std::uint8_t> BuildSpkSigMessage(
    std::uint32_t spk_id, const std::array<std::uint8_t, 32>& id_dh_pk,
    const std::array<std::uint8_t, 32>& spk_pk, const std::uint8_t* kem_pk,
    std::size_t kem_pk_len) {
  std::vector<std::uint8_t> msg;
  msg.reserve(4 + 4 + 32 + 32 + kem_pk_len);
  msg.push_back('M');
  msg.push_back('I');
  msg.push_back('S');
  msg.push_back('P');
  msg.push_back(static_cast<std::uint8_t>(spk_id & 0xFF));
  msg.push_back(static_cast<std::uint8_t>((spk_id >> 8) & 0xFF));
  msg.push_back(static_cast<std::uint8_t>((spk_id >> 16) & 0xFF));
  msg.push_back(static_cast<std::uint8_t>((spk_id >> 24) & 0xFF));
  msg.insert(msg.end(), id_dh_pk.begin(), id_dh_pk.end());
  msg.insert(msg.end(), spk_pk.begin(), spk_pk.end());
  if (kem_pk && kem_pk_len > 0) {
    msg.insert(msg.end(), kem_pk, kem_pk + kem_pk_len);
  }
  return msg;
}

std::vector<std::uint8_t> BuildPreKeySigMessage(
    const std::vector<std::uint8_t>& prekey_header_ad_prefix) {
  std::vector<std::uint8_t> msg;
  msg.reserve(4 + prekey_header_ad_prefix.size());
  msg.push_back('M');
  msg.push_back('I');
  msg.push_back('P');
  msg.push_back('K');
  msg.insert(msg.end(), prekey_header_ad_prefix.begin(),
             prekey_header_ad_prefix.end());
  return msg;
}

}  // namespace

namespace {
constexpr std::uint32_t kMaxSkip = 2000;
constexpr std::size_t kMaxSkippedMessageKeys = 2048;
}  // namespace

std::size_t Engine::SkippedKeyIdHash::operator()(const SkippedKeyId& v) const noexcept {
  std::size_t h = 0;
  for (std::size_t i = 0; i < v.dh.size(); i += 8) {
    std::uint64_t chunk = 0;
    std::memcpy(&chunk, v.dh.data() + i, sizeof(chunk));
    h ^= std::hash<std::uint64_t>{}(chunk) + 0x9e3779b97f4a7c15ULL + (h << 6) +
         (h >> 2);
  }
  h ^= std::hash<std::uint32_t>{}(v.n) + 0x9e3779b97f4a7c15ULL + (h << 6) +
       (h >> 2);
  return h;
}

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::Init(const std::filesystem::path& state_dir, std::string& error) {
  error.clear();
  state_dir_ = state_dir;
  if (state_dir_.empty()) {
    error = "state_dir empty";
    return false;
  }
  identity_path_ = state_dir_ / "identity.bin";
  trust_path_ = state_dir_ / "peer_trust.ini";

  std::error_code ec;
  std::filesystem::create_directories(state_dir_, ec);

  if (!LoadOrCreateIdentity(error)) {
    return false;
  }
  if (!LoadTrustStore(error)) {
    return false;
  }
  return true;
}

void Engine::SetLocalUsername(std::string username) {
  local_username_ = std::move(username);
}

bool Engine::LoadOrCreateIdentity(std::string& error) {
  error.clear();
  std::vector<std::uint8_t> bytes;
  std::string read_err;
  const bool exists = ReadAll(identity_path_, bytes, read_err);
  if (exists) {
#ifdef _WIN32
    bool was_dpapi = false;
    std::vector<std::uint8_t> plain_bytes;
    std::string unprotect_err;
    if (!MaybeUnprotectIdentityDpapi(bytes, plain_bytes, was_dpapi,
                                     unprotect_err)) {
      error = unprotect_err.empty() ? "identity unprotect failed" : unprotect_err;
      return false;
    }
    bytes = std::move(plain_bytes);
    const bool migrate_dpapi = !was_dpapi;
#else
    const bool migrate_dpapi = false;
#endif

    std::size_t off = 0;
    if (bytes.size() < 1) {
      error = "identity truncated";
      return false;
    }
    const std::uint8_t version = bytes[off++];
    if (version == 1) {
      if (bytes.size() != (1 + 32 + 32 + 4 + 32)) {
        error = "identity size invalid";
        return false;
      }
      off += 32;  // legacy ed25519 seed
      std::memcpy(id_dh_sk_.data(), bytes.data() + off, 32);
      off += 32;
      spk_id_ = static_cast<std::uint32_t>(bytes[off]) |
                (static_cast<std::uint32_t>(bytes[off + 1]) << 8) |
                (static_cast<std::uint32_t>(bytes[off + 2]) << 16) |
                (static_cast<std::uint32_t>(bytes[off + 3]) << 24);
      off += 4;
      std::memcpy(spk_sk_.data(), bytes.data() + off, 32);

      if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair(kem_pk_.data(), kem_sk_.data()) !=
          0) {
        error = "mlkem keypair failed";
        return false;
      }

      if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_keypair(id_sig_pk_.data(),
                                                    id_sig_sk_.data()) != 0) {
        error = "mldsa keypair failed";
        return false;
      }

      if (!DeriveIdentity(error)) {
        return false;
      }
      return SaveIdentity(error);
    }

    if (version == 2) {
      if (bytes.size() != (1 + 32 + 32 + 4 + 32 + kKemSecretKeyBytes +
                           kKemPublicKeyBytes)) {
        error = "identity size invalid";
        return false;
      }
      off += 32;  // legacy ed25519 seed
      std::memcpy(id_dh_sk_.data(), bytes.data() + off, 32);
      off += 32;
      spk_id_ = static_cast<std::uint32_t>(bytes[off]) |
                (static_cast<std::uint32_t>(bytes[off + 1]) << 8) |
                (static_cast<std::uint32_t>(bytes[off + 2]) << 16) |
                (static_cast<std::uint32_t>(bytes[off + 3]) << 24);
      off += 4;
      std::memcpy(spk_sk_.data(), bytes.data() + off, 32);
      off += 32;
      std::memcpy(kem_sk_.data(), bytes.data() + off, kem_sk_.size());
      off += kem_sk_.size();
      std::memcpy(kem_pk_.data(), bytes.data() + off, kem_pk_.size());

      if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_keypair(id_sig_pk_.data(),
                                                    id_sig_sk_.data()) != 0) {
        error = "mldsa keypair failed";
        return false;
      }

      if (!DeriveIdentity(error)) {
        return false;
      }
      return SaveIdentity(error);
    }

    if (version != kIdentityVersion) {
      error = "identity version mismatch";
      return false;
    }
    if (bytes.size() != (1 + kSigSecretKeyBytes + kSigPublicKeyBytes + 32 + 4 +
                         32 + kKemSecretKeyBytes + kKemPublicKeyBytes)) {
      error = "identity size invalid";
      return false;
    }
    std::memcpy(id_sig_sk_.data(), bytes.data() + off, id_sig_sk_.size());
    off += id_sig_sk_.size();
    std::memcpy(id_sig_pk_.data(), bytes.data() + off, id_sig_pk_.size());
    off += id_sig_pk_.size();
    std::memcpy(id_dh_sk_.data(), bytes.data() + off, 32);
    off += 32;
    spk_id_ = static_cast<std::uint32_t>(bytes[off]) |
              (static_cast<std::uint32_t>(bytes[off + 1]) << 8) |
              (static_cast<std::uint32_t>(bytes[off + 2]) << 16) |
              (static_cast<std::uint32_t>(bytes[off + 3]) << 24);
    off += 4;
    std::memcpy(spk_sk_.data(), bytes.data() + off, 32);
    off += 32;
    std::memcpy(kem_sk_.data(), bytes.data() + off, kem_sk_.size());
    off += kem_sk_.size();
    std::memcpy(kem_pk_.data(), bytes.data() + off, kem_pk_.size());
    if (!DeriveIdentity(error)) {
      return false;
    }
    if (migrate_dpapi) {
      return SaveIdentity(error);
    }
    return true;
  }

  if (!RandomBytes(id_dh_sk_.data(), id_dh_sk_.size()) ||
      !RandomBytes(spk_sk_.data(), spk_sk_.size())) {
    error = "rng failed";
    return false;
  }
  if (!RandomBytes(reinterpret_cast<std::uint8_t*>(&spk_id_), sizeof(spk_id_))) {
    error = "rng failed";
    return false;
  }
  if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair(kem_pk_.data(), kem_sk_.data()) !=
      0) {
    error = "mlkem keypair failed";
    return false;
  }
  if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_keypair(id_sig_pk_.data(),
                                                id_sig_sk_.data()) != 0) {
    error = "mldsa keypair failed";
    return false;
  }
  if (!DeriveIdentity(error)) {
    return false;
  }
  return SaveIdentity(error);
}

bool Engine::SaveIdentity(std::string& error) const {
  std::vector<std::uint8_t> out;
  out.reserve(1 + kSigSecretKeyBytes + kSigPublicKeyBytes + 32 + 4 + 32 +
              kKemSecretKeyBytes + kKemPublicKeyBytes);
  out.push_back(kIdentityVersion);
  out.insert(out.end(), id_sig_sk_.begin(), id_sig_sk_.end());
  out.insert(out.end(), id_sig_pk_.begin(), id_sig_pk_.end());
  out.insert(out.end(), id_dh_sk_.begin(), id_dh_sk_.end());
  out.push_back(static_cast<std::uint8_t>(spk_id_ & 0xFF));
  out.push_back(static_cast<std::uint8_t>((spk_id_ >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((spk_id_ >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((spk_id_ >> 24) & 0xFF));
  out.insert(out.end(), spk_sk_.begin(), spk_sk_.end());
  out.insert(out.end(), kem_sk_.begin(), kem_sk_.end());
  out.insert(out.end(), kem_pk_.begin(), kem_pk_.end());
#ifdef _WIN32
  std::vector<std::uint8_t> wrapped;
  if (!ProtectIdentityDpapi(out, wrapped, error)) {
    return false;
  }
  return WriteAll(identity_path_, wrapped, error);
#else
  return WriteAll(identity_path_, out, error);
#endif
}

bool Engine::DeriveIdentity(std::string& error) {
  error.clear();
  crypto_x25519_public_key(id_dh_pk_.data(), id_dh_sk_.data());
  crypto_x25519_public_key(spk_pk_.data(), spk_sk_.data());

  const auto msg = BuildSpkSigMessage(spk_id_, id_dh_pk_, spk_pk_, kem_pk_.data(),
                                      kem_pk_.size());
  std::size_t sig_len = 0;
  if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_signature(spk_sig_.data(), &sig_len,
                                                 msg.data(), msg.size(),
                                                 id_sig_sk_.data()) != 0) {
    error = "mldsa sign failed";
    return false;
  }
  if (sig_len != spk_sig_.size()) {
    error = "mldsa signature size invalid";
    return false;
  }
  return true;
}

bool Engine::LoadTrustStore(std::string& error) {
  error.clear();
  trusted_peers_.clear();
  std::ifstream f(trust_path_);
  if (!f.is_open()) {
    return true;  // empty trust store is ok
  }
  std::string line;
  while (std::getline(f, line)) {
    const std::string t = StripInlineComment(Trim(line));
    if (t.empty()) {
      continue;
    }
    const auto pos = t.find('=');
    if (pos == std::string::npos) {
      continue;
    }
    const std::string key = Trim(t.substr(0, pos));
    const std::string val = Trim(t.substr(pos + 1));
    if (key.empty() || val.empty()) {
      continue;
    }
    trusted_peers_[key] = val;
  }
  return true;
}

bool Engine::SaveTrustStore(std::string& error) const {
  error.clear();
  std::vector<std::pair<std::string, std::string>> entries;
  entries.reserve(trusted_peers_.size());
  for (const auto& kv : trusted_peers_) {
    entries.emplace_back(kv.first, kv.second);
  }
  std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  std::error_code ec;
  const auto dir = trust_path_.has_parent_path() ? trust_path_.parent_path()
                                                 : std::filesystem::path{};
  if (!dir.empty()) {
    std::filesystem::create_directories(dir, ec);
  }
  std::ofstream out(trust_path_, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "write trust store failed";
    return false;
  }
  out << "# mi_e2ee peer trust store\n";
  out << "# format: username=sha256(identity_keys)_hex\n";
  for (const auto& kv : entries) {
    out << kv.first << "=" << kv.second << "\n";
  }
  out.close();
  return true;
}

void Engine::SetPendingTrust(const std::string& peer_username,
                             const std::string& fingerprint_hex) {
  pending_.peer_username = peer_username;
  pending_.fingerprint_hex = fingerprint_hex;
  pending_.pin6 = Sas80HexFromFingerprint(fingerprint_hex);
}

bool Engine::TrustPendingPeer(const std::string& pin, std::string& error) {
  error.clear();
  if (pending_.peer_username.empty() || pending_.fingerprint_hex.empty() ||
      pending_.pin6.empty()) {
    error = "no pending peer trust";
    return false;
  }
  if (NormalizeCode(pin) != NormalizeCode(pending_.pin6)) {
    error = "sas mismatch";
    return false;
  }
  trusted_peers_[pending_.peer_username] = pending_.fingerprint_hex;
  if (!SaveTrustStore(error)) {
    return false;
  }

  const std::string peer = pending_.peer_username;
  pending_ = PendingPeerTrust{};

  auto it = pending_payloads_.find(peer);
  if (it != pending_payloads_.end()) {
    auto payloads = std::move(it->second);
    pending_payloads_.erase(it);
    for (const auto& p : payloads) {
      PrivateMessage msg;
      std::string dec_err;
      if (DecryptFromPayload(peer, p, msg, dec_err)) {
        ready_messages_.push_back(std::move(msg));
      }
    }
  }
  return true;
}

std::vector<PrivateMessage> Engine::DrainReadyMessages() {
  std::vector<PrivateMessage> out;
  out.swap(ready_messages_);
  return out;
}

bool Engine::BuildPublishBundle(std::vector<std::uint8_t>& out_bundle,
                                std::string& error) const {
  error.clear();
  out_bundle.clear();
  out_bundle.reserve(1 + kSigPublicKeyBytes + 32 + 4 + 32 + kKemPublicKeyBytes +
                     kSigBytes);
  out_bundle.push_back(kProtocolVersion);
  out_bundle.insert(out_bundle.end(), id_sig_pk_.begin(), id_sig_pk_.end());
  out_bundle.insert(out_bundle.end(), id_dh_pk_.begin(), id_dh_pk_.end());
  out_bundle.push_back(static_cast<std::uint8_t>(spk_id_ & 0xFF));
  out_bundle.push_back(static_cast<std::uint8_t>((spk_id_ >> 8) & 0xFF));
  out_bundle.push_back(static_cast<std::uint8_t>((spk_id_ >> 16) & 0xFF));
  out_bundle.push_back(static_cast<std::uint8_t>((spk_id_ >> 24) & 0xFF));
  out_bundle.insert(out_bundle.end(), spk_pk_.begin(), spk_pk_.end());
  out_bundle.insert(out_bundle.end(), kem_pk_.begin(), kem_pk_.end());
  out_bundle.insert(out_bundle.end(), spk_sig_.begin(), spk_sig_.end());
  return true;
}

bool Engine::ParsePeerBundle(const std::vector<std::uint8_t>& peer_bundle,
                             PeerBundle& out,
                             std::string& error) const {
  error.clear();
  if (peer_bundle.size() !=
      (1 + kSigPublicKeyBytes + 32 + 4 + 32 + kKemPublicKeyBytes + kSigBytes)) {
    error = "bundle size invalid";
    return false;
  }
  std::size_t off = 0;
  const std::uint8_t version = peer_bundle[off++];
  if (version != kProtocolVersion) {
    error = "bundle version mismatch";
    return false;
  }
  std::memcpy(out.id_sig_pk.data(), peer_bundle.data() + off, out.id_sig_pk.size());
  off += out.id_sig_pk.size();
  std::memcpy(out.id_dh_pk.data(), peer_bundle.data() + off, 32);
  off += 32;
  out.spk_id = static_cast<std::uint32_t>(peer_bundle[off]) |
               (static_cast<std::uint32_t>(peer_bundle[off + 1]) << 8) |
               (static_cast<std::uint32_t>(peer_bundle[off + 2]) << 16) |
               (static_cast<std::uint32_t>(peer_bundle[off + 3]) << 24);
  off += 4;
  std::memcpy(out.spk_pk.data(), peer_bundle.data() + off, 32);
  off += 32;
  std::memcpy(out.kem_pk.data(), peer_bundle.data() + off, out.kem_pk.size());
  off += out.kem_pk.size();
  std::memcpy(out.spk_sig.data(), peer_bundle.data() + off, out.spk_sig.size());

  const auto msg = BuildSpkSigMessage(out.spk_id, out.id_dh_pk, out.spk_pk,
                                      out.kem_pk.data(), out.kem_pk.size());
  if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_verify(out.spk_sig.data(),
                                              out.spk_sig.size(), msg.data(),
                                              msg.size(),
                                              out.id_sig_pk.data()) != 0) {
    error = "bundle signature invalid";
    return false;
  }
  return true;
}

bool Engine::CheckTrustedForSend(const std::string& peer_username,
                                 const std::string& fingerprint_hex,
                                 std::string& error) {
  error.clear();
  auto it = trusted_peers_.find(peer_username);
  if (it == trusted_peers_.end()) {
    SetPendingTrust(peer_username, fingerprint_hex);
    error = "peer not trusted";
    return false;
  }
  if (it->second != fingerprint_hex) {
    SetPendingTrust(peer_username, fingerprint_hex);
    error = "peer fingerprint changed";
    return false;
  }
  return true;
}

void Engine::EnforceSkippedMkLimit(Session& session) {
  while (session.skipped_mks.size() > kMaxSkippedMessageKeys) {
    if (session.skipped_order.empty()) {
      session.skipped_mks.clear();
      return;
    }
    const auto id = session.skipped_order.front();
    session.skipped_order.pop_front();
    auto it = session.skipped_mks.find(id);
    if (it != session.skipped_mks.end()) {
      session.skipped_mks.erase(it);
    }
  }
}

bool Engine::TryDecryptWithSkippedMk(
    Session& session,
    const std::array<std::uint8_t, 32>& dh,
    std::uint32_t n,
    const std::vector<std::uint8_t>& header_ad,
    const std::array<std::uint8_t, 24>& nonce,
    const std::vector<std::uint8_t>& cipher_text,
    const std::array<std::uint8_t, 16>& mac,
    std::vector<std::uint8_t>& out_plain) {
  out_plain.clear();
  const SkippedKeyId id{dh, n};
  auto it = session.skipped_mks.find(id);
  if (it == session.skipped_mks.end()) {
    return false;
  }
  const auto& mk = it->second;
  out_plain.resize(cipher_text.size());
  const int ok = crypto_aead_unlock(out_plain.data(), mac.data(), mk.data(),
                                    nonce.data(),
                                    header_ad.data(), header_ad.size(),
                                    cipher_text.data(), cipher_text.size());
  if (ok != 0) {
    out_plain.clear();
    return false;
  }
  session.skipped_mks.erase(it);
  return true;
}

bool Engine::InitSessionAsInitiator(const std::string& peer_username,
                                    const PeerBundle& peer,
                                    std::array<std::uint8_t, kKemCiphertextBytes>& out_kem_ct,
                                    Session& out_session,
                                    std::string& error) {
  error.clear();

  if (!CheckTrustedForSend(
          peer_username,
          FingerprintPeer(peer.id_sig_pk.data(), peer.id_sig_pk.size(),
                          peer.id_dh_pk),
                           error)) {
    return false;
  }

  std::array<std::uint8_t, 32> eph_sk{};
  if (!RandomBytes(eph_sk.data(), eph_sk.size())) {
    error = "rng failed";
    return false;
  }
  std::array<std::uint8_t, 32> eph_pk{};
  crypto_x25519_public_key(eph_pk.data(), eph_sk.data());

  const auto dh1 = X25519(id_dh_sk_, peer.spk_pk);
  const auto dh2 = X25519(eph_sk, peer.id_dh_pk);
  const auto dh3 = X25519(eph_sk, peer.spk_pk);

  std::array<std::uint8_t, kKemSharedSecretBytes> kem_ss{};
  if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_enc(out_kem_ct.data(), kem_ss.data(),
                                           peer.kem_pk.data()) != 0) {
    error = "mlkem enc failed";
    return false;
  }

  std::array<std::uint8_t, 96 + kKemSharedSecretBytes> secret{};
  std::memcpy(secret.data(), dh1.data(), 32);
  std::memcpy(secret.data() + 32, dh2.data(), 32);
  std::memcpy(secret.data() + 64, dh3.data(), 32);
  std::memcpy(secret.data() + 96, kem_ss.data(), kem_ss.size());

  std::vector<std::uint8_t> hk;
  if (!HkdfSha256(secret.data(), secret.size(), nullptr, 0,
                  "mi_e2ee_x3dh_hybrid_v1", hk, 64)) {
    error = "hkdf failed";
    return false;
  }

  out_session = Session{};
  out_session.peer_username = peer_username;
  out_session.peer_fingerprint_hex =
      FingerprintPeer(peer.id_sig_pk.data(), peer.id_sig_pk.size(), peer.id_dh_pk);
  std::memcpy(out_session.rk.data(), hk.data(), 32);
  std::memcpy(out_session.ck_s.data(), hk.data() + 32, 32);
  out_session.has_ck_s = true;
  out_session.has_ck_r = false;
  out_session.dhs_sk = eph_sk;
  out_session.dhs_pk = eph_pk;
  out_session.dhr_pk = peer.spk_pk;
  if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair(out_session.kem_s_pk.data(),
                                               out_session.kem_s_sk.data()) !=
      0) {
    error = "mlkem ratchet keypair failed";
    return false;
  }
  out_session.kem_r_pk = peer.kem_pk;

  return true;
}

bool Engine::InitSessionAsResponder(const std::string& peer_username,
                                    const PeerBundle& peer,
                                    const std::array<std::uint8_t, 32>& sender_eph_pk,
                                    const std::array<std::uint8_t, kKemPublicKeyBytes>& sender_ratchet_kem_pk,
                                    const std::array<std::uint8_t, kKemCiphertextBytes>& kem_ct,
                                    Session& out_session,
                                    std::string& error) {
  error.clear();
  if (peer.spk_id != spk_id_) {
    error = "spk_id mismatch";
    return false;
  }

  const auto dh1 = X25519(spk_sk_, peer.id_dh_pk);
  const auto dh2 = X25519(id_dh_sk_, sender_eph_pk);
  const auto dh3 = X25519(spk_sk_, sender_eph_pk);

  std::array<std::uint8_t, kKemSharedSecretBytes> kem_ss{};
  if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_dec(kem_ss.data(), kem_ct.data(),
                                           kem_sk_.data()) != 0) {
    error = "mlkem dec failed";
    return false;
  }

  std::array<std::uint8_t, 96 + kKemSharedSecretBytes> secret{};
  std::memcpy(secret.data(), dh1.data(), 32);
  std::memcpy(secret.data() + 32, dh2.data(), 32);
  std::memcpy(secret.data() + 64, dh3.data(), 32);
  std::memcpy(secret.data() + 96, kem_ss.data(), kem_ss.size());

  std::vector<std::uint8_t> hk;
  if (!HkdfSha256(secret.data(), secret.size(), nullptr, 0,
                  "mi_e2ee_x3dh_hybrid_v1", hk, 64)) {
    error = "hkdf failed";
    return false;
  }

  out_session = Session{};
  out_session.peer_username = peer_username;
  out_session.peer_fingerprint_hex =
      FingerprintPeer(peer.id_sig_pk.data(), peer.id_sig_pk.size(), peer.id_dh_pk);
  std::memcpy(out_session.rk.data(), hk.data(), 32);
  std::memcpy(out_session.ck_r.data(), hk.data() + 32, 32);
  out_session.has_ck_r = true;
  out_session.has_ck_s = false;
  out_session.dhs_sk = spk_sk_;
  out_session.dhs_pk = spk_pk_;
  out_session.dhr_pk = sender_eph_pk;
  out_session.kem_r_pk = sender_ratchet_kem_pk;
  return true;
}

bool Engine::EncryptMessage(Session& session, std::uint8_t msg_type,
                            const std::vector<std::uint8_t>& header_ad,
                            const std::vector<std::uint8_t>& plaintext,
                            std::vector<std::uint8_t>& out_payload,
                            std::string& error) {
  error.clear();
  out_payload.clear();
  if (!session.has_ck_s) {
    error = "no send chain";
    return false;
  }

  std::array<std::uint8_t, 32> next_ck{};
  std::array<std::uint8_t, 32> mk{};
  if (!KdfCk(session.ck_s, next_ck, mk)) {
    error = "kdf_ck failed";
    return false;
  }
  const std::uint32_t n = session.ns;
  session.ck_s = next_ck;
  session.ns++;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    error = "rng failed";
    return false;
  }

  std::vector<std::uint8_t> cipher;
  cipher.resize(plaintext.size());
  std::array<std::uint8_t, 16> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), mk.data(), nonce.data(),
                   header_ad.data(), header_ad.size(),
                   plaintext.data(), plaintext.size());

  out_payload = header_ad;
  out_payload.insert(out_payload.end(), nonce.begin(), nonce.end());
  out_payload.insert(out_payload.end(), mac.begin(), mac.end());
  out_payload.insert(out_payload.end(), cipher.begin(), cipher.end());
  (void)msg_type;
  (void)n;
  return true;
}

bool Engine::DecryptWithSession(Session& session, std::uint8_t msg_type,
                                const std::vector<std::uint8_t>& header_ad,
                                std::uint32_t n,
                                const std::array<std::uint8_t, 24>& nonce,
                                const std::vector<std::uint8_t>& cipher_text,
                                const std::array<std::uint8_t, 16>& mac,
                                std::vector<std::uint8_t>& out_plain,
                                std::string& error) {
  error.clear();
  out_plain.clear();
  if (!session.has_ck_r) {
    error = "no recv chain";
    return false;
  }

  if (TryDecryptWithSkippedMk(session, session.dhr_pk, n, header_ad, nonce,
                              cipher_text, mac, out_plain)) {
    return true;
  }

  if (n < session.nr) {
    error = "replayed or too old";
    return false;
  }
  if (n - session.nr > kMaxSkip) {
    error = "too many skipped";
    return false;
  }

  std::array<std::uint8_t, 32> ck = session.ck_r;
  std::uint32_t nr = session.nr;
  std::vector<std::pair<SkippedKeyId, std::array<std::uint8_t, 32>>> pending;
  pending.reserve(static_cast<std::size_t>(n - nr));

  while (nr < n) {
    std::array<std::uint8_t, 32> next_ck{};
    std::array<std::uint8_t, 32> mk{};
    if (!KdfCk(ck, next_ck, mk)) {
      error = "kdf_ck failed";
      return false;
    }
    pending.emplace_back(SkippedKeyId{session.dhr_pk, nr}, mk);
    ck = next_ck;
    nr++;
  }

  std::array<std::uint8_t, 32> next_ck{};
  std::array<std::uint8_t, 32> mk{};
  if (!KdfCk(ck, next_ck, mk)) {
    error = "kdf_ck failed";
    return false;
  }

  out_plain.resize(cipher_text.size());
  const int ok = crypto_aead_unlock(out_plain.data(), mac.data(), mk.data(),
                                    nonce.data(),
                                    header_ad.data(), header_ad.size(),
                                    cipher_text.data(), cipher_text.size());
  if (ok != 0) {
    error = "auth failed";
    out_plain.clear();
    return false;
  }

  for (const auto& e : pending) {
    const auto& id = e.first;
    const auto& key = e.second;
    if (session.skipped_mks.emplace(id, key).second) {
      session.skipped_order.push_back(id);
    }
  }
  EnforceSkippedMkLimit(session);

  session.ck_r = next_ck;
  session.nr = n + 1;
  (void)msg_type;
  return true;
}

bool Engine::RatchetOnReceive(
    Session& session, const std::array<std::uint8_t, 32>& new_dhr,
    const std::array<std::uint8_t, kKemPublicKeyBytes>& new_kem_r_pk,
    const std::array<std::uint8_t, kKemCiphertextBytes>& kem_ct,
    std::string& error) {
  error.clear();
  session.pn = session.ns;
  session.ns = 0;
  session.nr = 0;
  session.has_ck_s = false;

  const auto dh_recv = X25519(session.dhs_sk, new_dhr);
  std::array<std::uint8_t, kKemSharedSecretBytes> kem_ss{};
  if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_dec(
          kem_ss.data(), kem_ct.data(), session.kem_s_sk.data()) != 0) {
    error = "mlkem ratchet dec failed";
    return false;
  }

  std::array<std::uint8_t, 32> rk1{};
  std::array<std::uint8_t, 32> ck_r{};
  if (!KdfRkHybrid(session.rk, dh_recv, kem_ss, rk1, ck_r)) {
    error = "kdf_rk failed";
    return false;
  }

  session.rk = rk1;
  session.ck_r = ck_r;
  session.has_ck_r = true;
  session.dhr_pk = new_dhr;
  session.kem_r_pk = new_kem_r_pk;
  return true;
}

bool Engine::EncryptToPeer(const std::string& peer_username,
                           const std::vector<std::uint8_t>& peer_bundle,
                           const std::vector<std::uint8_t>& plaintext,
                           std::vector<std::uint8_t>& out_payload,
                           std::string& error) {
  error.clear();
  out_payload.clear();
  if (peer_username.empty()) {
    error = "peer empty";
    return false;
  }
  if (plaintext.empty()) {
    error = "plaintext empty";
    return false;
  }

  auto it = sessions_.find(peer_username);
  if (it == sessions_.end()) {
    if (peer_bundle.empty()) {
      error = "peer bundle missing";
      return false;
    }
    PeerBundle peer{};
    if (!ParsePeerBundle(peer_bundle, peer, error)) {
      return false;
    }
    Session session;
    std::array<std::uint8_t, kKemCiphertextBytes> kem_ct{};
    if (!InitSessionAsInitiator(peer_username, peer, kem_ct, session, error)) {
      return false;
    }

    const std::uint32_t n = session.ns;

    std::vector<std::uint8_t> ad_prefix;
    ad_prefix.reserve(1 + 1 + 4 + id_sig_pk_.size() + 32 + 32 +
                      kKemPublicKeyBytes + kKemCiphertextBytes + 4 + kSigBytes);
    ad_prefix.push_back(kProtocolVersion);
    ad_prefix.push_back(kMsgPreKey);
    ad_prefix.push_back(static_cast<std::uint8_t>(peer.spk_id & 0xFF));
    ad_prefix.push_back(static_cast<std::uint8_t>((peer.spk_id >> 8) & 0xFF));
    ad_prefix.push_back(static_cast<std::uint8_t>((peer.spk_id >> 16) & 0xFF));
    ad_prefix.push_back(static_cast<std::uint8_t>((peer.spk_id >> 24) & 0xFF));
    ad_prefix.insert(ad_prefix.end(), id_sig_pk_.begin(), id_sig_pk_.end());
    ad_prefix.insert(ad_prefix.end(), id_dh_pk_.begin(), id_dh_pk_.end());
    ad_prefix.insert(ad_prefix.end(), session.dhs_pk.begin(), session.dhs_pk.end());
    ad_prefix.insert(ad_prefix.end(), session.kem_s_pk.begin(), session.kem_s_pk.end());
    ad_prefix.insert(ad_prefix.end(), kem_ct.begin(), kem_ct.end());
    ad_prefix.push_back(static_cast<std::uint8_t>(n & 0xFF));
    ad_prefix.push_back(static_cast<std::uint8_t>((n >> 8) & 0xFF));
    ad_prefix.push_back(static_cast<std::uint8_t>((n >> 16) & 0xFF));
    ad_prefix.push_back(static_cast<std::uint8_t>((n >> 24) & 0xFF));

    const auto sig_msg = BuildPreKeySigMessage(ad_prefix);
    std::array<std::uint8_t, kSigBytes> prekey_sig{};
    std::size_t sig_len = 0;
    if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_signature(prekey_sig.data(), &sig_len,
                                                   sig_msg.data(), sig_msg.size(),
                                                   id_sig_sk_.data()) != 0) {
      error = "mldsa prekey sign failed";
      return false;
    }
    if (sig_len != prekey_sig.size()) {
      error = "mldsa prekey signature size invalid";
      return false;
    }

    std::vector<std::uint8_t> ad = std::move(ad_prefix);
    ad.insert(ad.end(), prekey_sig.begin(), prekey_sig.end());

    if (!EncryptMessage(session, kMsgPreKey, ad, plaintext, out_payload, error)) {
      return false;
    }

    sessions_[peer_username] = session;
    return true;
  }

  Session& session = it->second;
  if (!CheckTrustedForSend(peer_username, session.peer_fingerprint_hex, error)) {
    return false;
  }

  bool started_new_send_chain = false;
  std::array<std::uint8_t, kKemCiphertextBytes> ratchet_kem_ct{};
  if (!session.has_ck_s) {
    std::array<std::uint8_t, 32> new_dhs_sk{};
    if (!RandomBytes(new_dhs_sk.data(), new_dhs_sk.size())) {
      error = "rng failed";
      return false;
    }
    std::array<std::uint8_t, 32> new_dhs_pk{};
    crypto_x25519_public_key(new_dhs_pk.data(), new_dhs_sk.data());

    std::array<std::uint8_t, kKemSecretKeyBytes> new_kem_s_sk{};
    std::array<std::uint8_t, kKemPublicKeyBytes> new_kem_s_pk{};
    if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair(new_kem_s_pk.data(),
                                                 new_kem_s_sk.data()) != 0) {
      error = "mlkem ratchet keypair failed";
      return false;
    }

    std::array<std::uint8_t, kKemSharedSecretBytes> kem_ss{};
    if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_enc(ratchet_kem_ct.data(), kem_ss.data(),
                                             session.kem_r_pk.data()) != 0) {
      error = "mlkem ratchet enc failed";
      return false;
    }

    const auto dh = X25519(new_dhs_sk, session.dhr_pk);
    std::array<std::uint8_t, 32> rk{};
    std::array<std::uint8_t, 32> ck_s{};
    if (!KdfRkHybrid(session.rk, dh, kem_ss, rk, ck_s)) {
      error = "kdf_rk failed";
      return false;
    }
    session.rk = rk;
    session.ck_s = ck_s;
    session.has_ck_s = true;
    session.dhs_sk = new_dhs_sk;
    session.dhs_pk = new_dhs_pk;
    session.kem_s_sk = new_kem_s_sk;
    session.kem_s_pk = new_kem_s_pk;
    session.ns = 0;
    started_new_send_chain = true;
  }

  const std::uint32_t n = session.ns;
  const std::uint32_t pn = session.pn;
  std::vector<std::uint8_t> ad;
  ad.reserve(1 + 1 + 32 + 4 + 4 +
             (n == 0 ? (kKemPublicKeyBytes + kKemCiphertextBytes) : 0));
  ad.push_back(kProtocolVersion);
  ad.push_back(kMsgRatchet);
  ad.insert(ad.end(), session.dhs_pk.begin(), session.dhs_pk.end());
  ad.push_back(static_cast<std::uint8_t>(pn & 0xFF));
  ad.push_back(static_cast<std::uint8_t>((pn >> 8) & 0xFF));
  ad.push_back(static_cast<std::uint8_t>((pn >> 16) & 0xFF));
  ad.push_back(static_cast<std::uint8_t>((pn >> 24) & 0xFF));
  ad.push_back(static_cast<std::uint8_t>(n & 0xFF));
  ad.push_back(static_cast<std::uint8_t>((n >> 8) & 0xFF));
  ad.push_back(static_cast<std::uint8_t>((n >> 16) & 0xFF));
  ad.push_back(static_cast<std::uint8_t>((n >> 24) & 0xFF));
  if (n == 0) {
    if (!started_new_send_chain) {
      error = "ratchet state invalid";
      return false;
    }
    ad.insert(ad.end(), session.kem_s_pk.begin(), session.kem_s_pk.end());
    ad.insert(ad.end(), ratchet_kem_ct.begin(), ratchet_kem_ct.end());
  }

  return EncryptMessage(session, kMsgRatchet, ad, plaintext, out_payload, error);
}

bool Engine::DecryptFromPayload(const std::string& peer_username,
                                const std::vector<std::uint8_t>& payload,
                                PrivateMessage& out_message,
                                std::string& error) {
  error.clear();
  out_message = PrivateMessage{};
  if (peer_username.empty()) {
    error = "peer empty";
    return false;
  }
  if (payload.size() < 2) {
    error = "payload too short";
    return false;
  }
  std::size_t off = 0;
  const std::uint8_t version = payload[off++];
  const std::uint8_t msg_type = payload[off++];
  if (version != kProtocolVersion) {
    error = "version mismatch";
    return false;
  }

  if (msg_type == kMsgPreKey) {
    if (payload.size() <
        2 + 4 + kSigPublicKeyBytes + 32 + 32 + kKemPublicKeyBytes +
            kKemCiphertextBytes + 4 + 24 +
            kSigBytes + 16) {
      error = "prekey payload truncated";
      return false;
    }
    const std::uint32_t spk_id = static_cast<std::uint32_t>(payload[off]) |
                                 (static_cast<std::uint32_t>(payload[off + 1]) << 8) |
                                 (static_cast<std::uint32_t>(payload[off + 2]) << 16) |
                                 (static_cast<std::uint32_t>(payload[off + 3]) << 24);
    off += 4;
    PeerBundle peer{};
    peer.spk_id = spk_id;
    std::memcpy(peer.id_sig_pk.data(), payload.data() + off,
                peer.id_sig_pk.size());
    off += peer.id_sig_pk.size();
    std::memcpy(peer.id_dh_pk.data(), payload.data() + off, 32);
    off += 32;
    std::array<std::uint8_t, 32> sender_eph_pk{};
    std::memcpy(sender_eph_pk.data(), payload.data() + off, 32);
    off += 32;

    std::array<std::uint8_t, kKemPublicKeyBytes> sender_ratchet_kem_pk{};
    std::memcpy(sender_ratchet_kem_pk.data(), payload.data() + off,
                sender_ratchet_kem_pk.size());
    off += sender_ratchet_kem_pk.size();

    std::array<std::uint8_t, kKemCiphertextBytes> kem_ct{};
    std::memcpy(kem_ct.data(), payload.data() + off, kem_ct.size());
    off += kem_ct.size();
    const std::uint32_t n = static_cast<std::uint32_t>(payload[off]) |
                            (static_cast<std::uint32_t>(payload[off + 1]) << 8) |
                            (static_cast<std::uint32_t>(payload[off + 2]) << 16) |
                            (static_cast<std::uint32_t>(payload[off + 3]) << 24);
    off += 4;

    std::array<std::uint8_t, kSigBytes> prekey_sig{};
    std::memcpy(prekey_sig.data(), payload.data() + off, prekey_sig.size());
    off += prekey_sig.size();

    {
      std::vector<std::uint8_t> ad_prefix(payload.begin(), payload.begin() + off - prekey_sig.size());
      const auto sig_msg = BuildPreKeySigMessage(ad_prefix);
      if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_verify(
              prekey_sig.data(), prekey_sig.size(), sig_msg.data(),
              sig_msg.size(), peer.id_sig_pk.data()) != 0) {
        error = "prekey signature invalid";
        return false;
      }
    }

    std::vector<std::uint8_t> ad(payload.begin(), payload.begin() + off);

    std::array<std::uint8_t, 24> nonce{};
    std::memcpy(nonce.data(), payload.data() + off, nonce.size());
    off += nonce.size();
    std::array<std::uint8_t, 16> mac{};
    std::memcpy(mac.data(), payload.data() + off, mac.size());
    off += mac.size();
    std::vector<std::uint8_t> cipher(payload.begin() + off, payload.end());

    const std::string fingerprint =
        FingerprintPeer(peer.id_sig_pk.data(), peer.id_sig_pk.size(), peer.id_dh_pk);
    auto trust_it = trusted_peers_.find(peer_username);
    if (trust_it == trusted_peers_.end() || trust_it->second != fingerprint) {
      SetPendingTrust(peer_username, fingerprint);
      pending_payloads_[peer_username].push_back(payload);
      error = (trust_it == trusted_peers_.end()) ? "peer not trusted"
                                                 : "peer fingerprint changed";
      return false;
    }

    Session session;
    if (!InitSessionAsResponder(peer_username, peer, sender_eph_pk,
                                sender_ratchet_kem_pk, kem_ct, session, error)) {
      return false;
    }

    std::vector<std::uint8_t> plain;
    if (!DecryptWithSession(session, msg_type, ad, n, nonce, cipher, mac, plain,
                            error)) {
      return false;
    }

    sessions_[peer_username] = session;
    out_message.from_username = peer_username;
    out_message.plaintext = std::move(plain);
    return true;
  }

  if (msg_type == kMsgRatchet) {
    if (payload.size() < 2 + 32 + 4 + 4 + 24 + 16) {
      error = "ratchet payload truncated";
      return false;
    }
    std::array<std::uint8_t, 32> sender_dhs_pk{};
    std::memcpy(sender_dhs_pk.data(), payload.data() + off, 32);
    off += 32;
    const std::uint32_t pn = static_cast<std::uint32_t>(payload[off]) |
                            (static_cast<std::uint32_t>(payload[off + 1]) << 8) |
                            (static_cast<std::uint32_t>(payload[off + 2]) << 16) |
                            (static_cast<std::uint32_t>(payload[off + 3]) << 24);
    off += 4;
    const std::uint32_t n = static_cast<std::uint32_t>(payload[off]) |
                            (static_cast<std::uint32_t>(payload[off + 1]) << 8) |
                            (static_cast<std::uint32_t>(payload[off + 2]) << 16) |
                            (static_cast<std::uint32_t>(payload[off + 3]) << 24);
    off += 4;

    auto it = sessions_.find(peer_username);
    if (it == sessions_.end()) {
      error = "no session";
      return false;
    }
    Session& session = it->second;
    if (!CheckTrustedForSend(peer_username, session.peer_fingerprint_hex, error)) {
      pending_payloads_[peer_username].push_back(payload);
      return false;
    }

    std::array<std::uint8_t, kKemPublicKeyBytes> sender_kem_pk{};
    std::array<std::uint8_t, kKemCiphertextBytes> kem_ct{};
    if (n == 0) {
      if (payload.size() <
          off + sender_kem_pk.size() + kem_ct.size() + 24 + 16) {
        error = "ratchet payload truncated";
        return false;
      }
      std::memcpy(sender_kem_pk.data(), payload.data() + off,
                  sender_kem_pk.size());
      off += sender_kem_pk.size();
      std::memcpy(kem_ct.data(), payload.data() + off, kem_ct.size());
      off += kem_ct.size();
    }

    std::vector<std::uint8_t> ad(payload.begin(), payload.begin() + off);

    std::array<std::uint8_t, 24> nonce{};
    std::memcpy(nonce.data(), payload.data() + off, nonce.size());
    off += nonce.size();
    std::array<std::uint8_t, 16> mac{};
    std::memcpy(mac.data(), payload.data() + off, mac.size());
    off += mac.size();
    std::vector<std::uint8_t> cipher(payload.begin() + off, payload.end());

    std::vector<std::uint8_t> plain;
    if (TryDecryptWithSkippedMk(session, sender_dhs_pk, n, ad, nonce, cipher, mac,
                               plain)) {
      out_message.from_username = peer_username;
      out_message.plaintext = std::move(plain);
      return true;
    }

    const bool new_chain = (sender_dhs_pk != session.dhr_pk);
    if (new_chain) {
      if (n != 0) {
        error = "ratchet header invalid";
        return false;
      }
      if (pn < session.nr) {
        error = "ratchet pn invalid";
        return false;
      }
      if (pn - session.nr > kMaxSkip) {
        error = "too many skipped";
        return false;
      }

      Session cand = session;
      if (!cand.has_ck_r) {
        if (pn != 0 || cand.nr != 0 || !cand.skipped_mks.empty()) {
          error = "ratchet state invalid";
          return false;
        }
      } else {
      while (cand.nr < pn) {
        std::array<std::uint8_t, 32> next_ck{};
        std::array<std::uint8_t, 32> mk{};
        if (!KdfCk(cand.ck_r, next_ck, mk)) {
          error = "kdf_ck failed";
          return false;
        }
        const SkippedKeyId id{cand.dhr_pk, cand.nr};
        if (cand.skipped_mks.emplace(id, mk).second) {
          cand.skipped_order.push_back(id);
        }
        cand.ck_r = next_ck;
        cand.nr++;
      }
      EnforceSkippedMkLimit(cand);
      }

      if (!RatchetOnReceive(cand, sender_dhs_pk, sender_kem_pk, kem_ct, error)) {
        return false;
      }

      if (!DecryptWithSession(cand, msg_type, ad, n, nonce, cipher, mac, plain,
                              error)) {
        return false;
      }
      session = std::move(cand);
    } else {
      if (!DecryptWithSession(session, msg_type, ad, n, nonce, cipher, mac, plain,
                              error)) {
        return false;
      }
    }
    out_message.from_username = peer_username;
    out_message.plaintext = std::move(plain);
    return true;
  }

  error = "unknown message type";
  return false;
}

bool Engine::SignDetached(const std::vector<std::uint8_t>& message,
                          std::vector<std::uint8_t>& out_sig,
                          std::string& error) const {
  error.clear();
  out_sig.clear();
  if (message.empty()) {
    error = "message empty";
    return false;
  }
  out_sig.resize(kSigBytes);
  std::size_t sig_len = 0;
  if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_signature(out_sig.data(), &sig_len,
                                                 message.data(), message.size(),
                                                 id_sig_sk_.data()) != 0) {
    out_sig.clear();
    error = "mldsa sign failed";
    return false;
  }
  if (sig_len != out_sig.size()) {
    out_sig.clear();
    error = "mldsa signature size invalid";
    return false;
  }
  return true;
}

bool Engine::VerifyDetached(const std::vector<std::uint8_t>& message,
                            const std::vector<std::uint8_t>& sig,
                            const std::vector<std::uint8_t>& pk,
                            std::string& error) {
  error.clear();
  if (message.empty()) {
    error = "message empty";
    return false;
  }
  if (sig.size() != kSigBytes) {
    error = "signature size invalid";
    return false;
  }
  if (pk.size() != kSigPublicKeyBytes) {
    error = "public key size invalid";
    return false;
  }
  if (PQCLEAN_MLDSA65_CLEAN_crypto_sign_verify(sig.data(), sig.size(),
                                              message.data(), message.size(),
                                              pk.data()) != 0) {
    error = "signature invalid";
    return false;
  }
  return true;
}

bool Engine::ExtractPeerIdentityFromBundle(
    const std::vector<std::uint8_t>& peer_bundle,
    std::vector<std::uint8_t>& out_id_sig_pk,
    std::array<std::uint8_t, 32>& out_id_dh_pk,
    std::string& out_fingerprint_hex,
    std::string& error) const {
  error.clear();
  out_id_sig_pk.clear();
  out_id_dh_pk.fill(0);
  out_fingerprint_hex.clear();
  PeerBundle peer;
  if (!ParsePeerBundle(peer_bundle, peer, error)) {
    return false;
  }
  out_id_sig_pk.assign(peer.id_sig_pk.begin(), peer.id_sig_pk.end());
  out_id_dh_pk = peer.id_dh_pk;
  out_fingerprint_hex =
      FingerprintPeer(peer.id_sig_pk.data(), peer.id_sig_pk.size(), peer.id_dh_pk);
  if (out_fingerprint_hex.empty()) {
    error = "fingerprint failed";
    out_id_sig_pk.clear();
    out_id_dh_pk.fill(0);
    return false;
  }
  return true;
}

bool Engine::EnsurePeerTrusted(const std::string& peer_username,
                               const std::string& fingerprint_hex,
                               std::string& error) {
  return CheckTrustedForSend(peer_username, fingerprint_hex, error);
}

}  // namespace mi::client::e2ee
