#include "client_core.h"
#include "file_blob.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#ifndef SECURITY_WIN32
#define SECURITY_WIN32 1
#endif
#include <security.h>
#include <schannel.h>
#include <wincrypt.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#if defined(__linux__)
#include <sys/random.h>
#endif
#include <unistd.h>
#endif

#include "monocypher.h"
#include "miniz.h"
#include "opaque_pake.h"
#include "ikcp.h"

extern "C" {
int PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair(std::uint8_t* pk,
                                             std::uint8_t* sk);
int PQCLEAN_MLKEM768_CLEAN_crypto_kem_dec(std::uint8_t* ss,
                                         const std::uint8_t* ct,
                                         const std::uint8_t* sk);
}

#include "../server/include/c_api.h"
#include "../server/include/crypto.h"
#include "../server/include/frame.h"
#include "../server/include/key_transparency.h"
#include "../server/include/protocol.h"
#include "chat_history_store.h"
#include "client_config.h"
#include "dpapi_util.h"
#include "path_security.h"

namespace mi::client {

namespace {

constexpr std::size_t kMaxOpaqueMessageBytes = 16 * 1024;
constexpr std::size_t kMaxOpaqueSessionKeyBytes = 1024;
constexpr std::size_t kKtRootPubkeyBytes = mi::server::kKtSthSigPublicKeyBytes;
constexpr std::size_t kMaxDeviceSyncKeyFileBytes = 64u * 1024u;
constexpr std::uint8_t kKcpCookieCmd = 0xFF;
constexpr std::uint8_t kKcpCookieHello = 1;
constexpr std::uint8_t kKcpCookieChallenge = 2;
constexpr std::uint8_t kKcpCookieResponse = 3;
constexpr std::size_t kKcpCookieBytes = 16;
constexpr std::size_t kKcpCookiePacketBytes = 24;

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

std::string EndpointKey(const std::string& host, std::uint16_t port) {
  return host + ":" + std::to_string(port);
}

struct TrustEntry {
  std::string fingerprint;
  bool tls_required{false};
};

constexpr char kTrustStoreMagic[] = "MI_TRUST1";
constexpr char kTrustStoreEntropy[] = "mi_e2ee_trust_store_v1";

std::string ToLower(std::string s) {
  for (auto& ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

std::filesystem::path ResolveConfigDir(const std::string& config_path) {
  std::filesystem::path cfg_path(config_path);
  std::filesystem::path dir = cfg_path.parent_path();
  if (dir.empty()) {
    std::error_code ec;
    dir = std::filesystem::current_path(ec);
    if (dir.empty()) {
      dir = std::filesystem::path{"."};
    }
    return dir;
  }
  if (dir.is_relative()) {
    std::error_code ec;
    const auto cwd = std::filesystem::current_path(ec);
    if (!cwd.empty()) {
      dir = cwd / dir;
    }
  }
  return dir;
}

std::filesystem::path ResolveDataDir(const std::filesystem::path& config_dir) {
  if (const char* env = std::getenv("MI_E2EE_DATA_DIR")) {
    if (*env) {
      return std::filesystem::path(env);
    }
  }
  if (!config_dir.empty()) {
    const std::string leaf = ToLower(config_dir.filename().string());
    if (leaf == "config") {
      const auto parent = config_dir.parent_path();
      if (!parent.empty()) {
        return parent / "database";
      }
    }
    return config_dir / "database";
  }
  std::error_code ec;
  auto cwd = std::filesystem::current_path(ec);
  if (!cwd.empty()) {
    return cwd / "database";
  }
  return std::filesystem::path{"database"};
}

bool IsLoopbackHost(const std::string& host) {
  const std::string h = ToLower(Trim(host));
  return h == "127.0.0.1" || h == "localhost" || h == "::1";
}

std::string NormalizeFingerprint(std::string v) {
  return ToLower(Trim(v));
}

bool IsHex64(const std::string& v) {
  if (v.size() != 64) {
    return false;
  }
  for (const char c : v) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  return true;
}

int HexNibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  return -1;
}

bool HexToBytes(const std::string& hex, std::vector<std::uint8_t>& out) {
  out.clear();
  if (hex.empty() || (hex.size() % 2) != 0) {
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
    out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
  }
  return true;
}

bool ReadFileBytes(const std::filesystem::path& path,
                   std::vector<std::uint8_t>& out,
                   std::string& error) {
  error.clear();
  out.clear();
  if (path.empty()) {
    error = "kt root pubkey path empty";
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    error = ec ? "kt root pubkey path error" : "kt root pubkey not found";
    return false;
  }
  const auto size = std::filesystem::file_size(path, ec);
  if (ec) {
    error = "kt root pubkey size stat failed";
    return false;
  }
  if (size != kKtRootPubkeyBytes) {
    error = "kt root pubkey size invalid";
    return false;
  }
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    error = "kt root pubkey not found";
    return false;
  }
  out.resize(static_cast<std::size_t>(size));
  ifs.read(reinterpret_cast<char*>(out.data()),
           static_cast<std::streamsize>(out.size()));
  if (!ifs) {
    out.clear();
    error = "kt root pubkey read failed";
    return false;
  }
  return true;
}

bool TryLoadKtRootPubkeyFromLoopback(const std::filesystem::path& base_dir,
                                     const std::string& host,
                                     std::vector<std::uint8_t>& out,
                                     std::string& error) {
  out.clear();
  error.clear();
  if (!IsLoopbackHost(host)) {
    return false;
  }
  std::vector<std::filesystem::path> candidates;
  const std::filesystem::path base = base_dir.empty() ? std::filesystem::path{"."}
                                                      : base_dir;
  candidates.emplace_back(base / "kt_root_pub.bin");
  candidates.emplace_back(base / "offline_store" / "kt_root_pub.bin");
  const auto parent = base.parent_path();
  if (!parent.empty()) {
    candidates.emplace_back(parent / "s" / "kt_root_pub.bin");
    candidates.emplace_back(parent / "s" / "offline_store" / "kt_root_pub.bin");
    candidates.emplace_back(parent / "server" / "kt_root_pub.bin");
    candidates.emplace_back(parent / "server" / "offline_store" / "kt_root_pub.bin");
  }
  std::string last_err;
  for (const auto& path : candidates) {
    std::string read_err;
    if (ReadFileBytes(path, out, read_err)) {
      return true;
    }
    if (!read_err.empty()) {
      last_err = read_err;
    }
  }
  error = last_err.empty() ? "kt root pubkey missing" : last_err;
  return false;
}

bool ParseTrustValue(const std::string& value, TrustEntry& out) {
  out = TrustEntry{};
  if (value.empty()) {
    return false;
  }
  std::string v = value;
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= v.size()) {
    const auto pos = v.find(',', start);
    if (pos == std::string::npos) {
      parts.push_back(Trim(v.substr(start)));
      break;
    }
    parts.push_back(Trim(v.substr(start, pos - start)));
    start = pos + 1;
  }
  if (parts.empty() || parts[0].empty()) {
    return false;
  }
  std::string fp = ToLower(Trim(parts[0]));
  if (!IsHex64(fp)) {
    return false;
  }
  out.fingerprint = fp;
  for (std::size_t i = 1; i < parts.size(); ++i) {
    const std::string token = ToLower(parts[i]);
    if (token == "tls=1" || token == "tls=true" || token == "tls=on" ||
        token == "tls_required=1" || token == "tls_required=true") {
      out.tls_required = true;
    }
  }
  return true;
}

std::string BuildTrustValue(const TrustEntry& entry) {
  if (entry.fingerprint.empty()) {
    return {};
  }
  std::string out = entry.fingerprint;
  if (entry.tls_required) {
    out += ",tls=1";
  }
  return out;
}

bool LoadTrustStoreText(const std::string& path, std::string& out_text,
                        std::string& error) {
  out_text.clear();
  error.clear();
  if (path.empty()) {
    error = "trust store path empty";
    return false;
  }
#ifdef _WIN32
  {
    std::error_code ec;
    if (std::filesystem::exists(path, ec) && !ec) {
      std::string perm_err;
      if (!mi::shard::security::CheckPathNotWorldWritable(path, perm_err)) {
        error = perm_err.empty() ? "trust store permissions insecure" : perm_err;
        return false;
      }
    }
  }
#endif
  std::ifstream f(path, std::ios::binary);
  if (!f.is_open()) {
    return false;
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());
  if (bytes.empty()) {
    return false;
  }
#ifdef _WIN32
  std::vector<std::uint8_t> plain;
  bool was_dpapi = false;
  if (!MaybeUnprotectDpapi(bytes, kTrustStoreMagic, kTrustStoreEntropy, plain,
                           was_dpapi, error)) {
    return false;
  }
  const auto& view = was_dpapi ? plain : bytes;
  out_text.assign(view.begin(), view.end());
#else
  out_text.assign(bytes.begin(), bytes.end());
#endif
  return true;
}

bool StoreTrustStoreText(const std::string& path, const std::string& text,
                         std::string& error) {
  error.clear();
  if (path.empty()) {
    error = "trust store path empty";
    return false;
  }
  std::vector<std::uint8_t> out_bytes;
#ifdef _WIN32
  std::vector<std::uint8_t> plain(text.begin(), text.end());
  if (!ProtectDpapi(plain, kTrustStoreMagic, kTrustStoreEntropy, out_bytes,
                    error)) {
    return false;
  }
#else
  out_bytes.assign(text.begin(), text.end());
#endif
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "open trust store failed";
    return false;
  }
  out.write(reinterpret_cast<const char*>(out_bytes.data()),
            static_cast<std::streamsize>(out_bytes.size()));
  out.close();
  return true;
}

bool LoadTrustEntry(const std::string& path, const std::string& endpoint,
                    TrustEntry& out_entry) {
  out_entry = TrustEntry{};
  if (path.empty() || endpoint.empty()) {
    return false;
  }
  std::string content;
  std::string load_err;
  if (!LoadTrustStoreText(path, content, load_err)) {
    return false;
  }
  std::istringstream iss(content);
  std::string line;
  while (std::getline(iss, line)) {
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
    if (key == endpoint && !val.empty()) {
      TrustEntry entry;
      if (ParseTrustValue(val, entry)) {
        out_entry = entry;
        return true;
      }
      return false;
    }
  }
  return false;
}

bool StoreTrustEntry(const std::string& path, const std::string& endpoint,
                     const TrustEntry& entry, std::string& error) {
  error.clear();
  if (path.empty() || endpoint.empty() || entry.fingerprint.empty()) {
    error = "invalid trust store input";
    return false;
  }

  std::vector<std::pair<std::string, std::string>> entries;
  std::string content;
  std::string load_err;
  if (LoadTrustStoreText(path, content, load_err)) {
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
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
      if (key.empty() || val.empty() || key == endpoint) {
        continue;
      }
      entries.emplace_back(key, val);
    }
  }
  entries.emplace_back(endpoint, BuildTrustValue(entry));
  std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  std::error_code ec;
  const std::filesystem::path p(path);
  const auto dir =
      p.has_parent_path() ? p.parent_path() : std::filesystem::path{};
  if (!dir.empty()) {
    std::filesystem::create_directories(dir, ec);
  }
  std::ostringstream oss;
  oss << "# mi_e2ee client trust store\n";
  oss << "# format: host:port=sha256(cert_der)_hex[,tls=1]\n";
  for (const auto& kv : entries) {
    oss << kv.first << "=" << kv.second << "\n";
  }
  if (!StoreTrustStoreText(path, oss.str(), error)) {
    return false;
  }
  return true;
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

#ifndef _WIN32
bool ReadUrandom(std::uint8_t* out, std::size_t len) {
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0) {
    return false;
  }
  std::size_t offset = 0;
  while (offset < len) {
    const ssize_t n = read(fd, out + offset, len - offset);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (n == 0) {
      break;
    }
    offset += static_cast<std::size_t>(n);
  }
  close(fd);
  return offset == len;
}

bool OsRandomBytes(std::uint8_t* out, std::size_t len) {
#if defined(__linux__)
  const ssize_t rc = getrandom(out, len, 0);
  if (rc == static_cast<ssize_t>(len)) {
    return true;
  }
#endif
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__)
  arc4random_buf(out, len);
  return true;
#endif
  return ReadUrandom(out, len);
}
#endif

bool RandomBytes(std::uint8_t* out, std::size_t len) {
  if (!out || len == 0) {
    return false;
  }
#ifdef _WIN32
  return BCryptGenRandom(nullptr, out, static_cast<ULONG>(len),
                         BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
  return OsRandomBytes(out, len);
#endif
}

bool RandomUint32(std::uint32_t& out) {
  return RandomBytes(reinterpret_cast<std::uint8_t*>(&out), sizeof(out));
}

std::uint32_t NowMs() {
  static const auto kStart = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();
  return static_cast<std::uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(now - kStart)
          .count());
}

bool SetNonBlockingSocket(
#ifdef _WIN32
    SOCKET sock
#else
    int sock
#endif
) {
#ifdef _WIN32
  u_long mode = 1;
  return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
  int flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool WaitForReadable(
#ifdef _WIN32
    SOCKET sock,
#else
    int sock,
#endif
    std::uint32_t timeout_ms) {
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(sock, &readfds);
  timeval tv{};
  tv.tv_sec = static_cast<long>(timeout_ms / 1000u);
  tv.tv_usec = static_cast<long>((timeout_ms % 1000u) * 1000u);
#ifdef _WIN32
  const int rc = select(0, &readfds, nullptr, nullptr, &tv);
#else
  const int rc = select(sock + 1, &readfds, nullptr, nullptr, &tv);
#endif
  return rc > 0 && FD_ISSET(sock, &readfds);
}

bool SocketWouldBlock() {
#ifdef _WIN32
  const int err = WSAGetLastError();
  return err == WSAEWOULDBLOCK;
#else
  return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

bool IsLowEndDevice() {
  const unsigned int hc = std::thread::hardware_concurrency();
  if (hc != 0 && hc <= 4) {
    return true;
  }
#ifdef _WIN32
  MEMORYSTATUSEX ms{};
  ms.dwLength = sizeof(ms);
  if (GlobalMemoryStatusEx(&ms)) {
    constexpr std::uint64_t kLowEndMem =
        4ull * 1024ull * 1024ull * 1024ull;
    if (ms.ullTotalPhys != 0 && ms.ullTotalPhys <= kLowEndMem) {
      return true;
    }
  }
#endif
  return false;
}

bool ResolveCoverTrafficEnabled(const TrafficConfig& cfg) {
  switch (cfg.cover_traffic_mode) {
    case CoverTrafficMode::kOn:
      return true;
    case CoverTrafficMode::kOff:
      return false;
    case CoverTrafficMode::kAuto:
    default:
      return !IsLowEndDevice();
  }
}

constexpr std::uint8_t kPadMagic[4] = {'M', 'I', 'P', 'D'};
constexpr std::size_t kPadHeaderBytes = 8;
constexpr std::size_t kPadBuckets[] = {256, 512, 1024, 2048, 4096, 8192, 16384};

std::size_t SelectPadTarget(std::size_t min_len) {
  for (const auto bucket : kPadBuckets) {
    if (bucket >= min_len) {
      if (bucket == min_len) {
        return bucket;
      }
      std::uint32_t r = 0;
      if (!RandomUint32(r)) {
        return bucket;
      }
      const std::size_t span = bucket - min_len;
      return min_len + (static_cast<std::size_t>(r) % (span + 1));
    }
  }
  const std::size_t round = ((min_len + 4095) / 4096) * 4096;
  if (round <= min_len) {
    return min_len;
  }
  std::uint32_t r = 0;
  if (!RandomUint32(r)) {
    return round;
  }
  const std::size_t span = round - min_len;
  return min_len + (static_cast<std::size_t>(r) % (span + 1));
}

bool PadPayload(const std::vector<std::uint8_t>& plain,
                std::vector<std::uint8_t>& out,
                std::string& error) {
  error.clear();
  out.clear();
  if (plain.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    error = "pad size overflow";
    return false;
  }
  const std::size_t min_len = kPadHeaderBytes + plain.size();
  const std::size_t target_len = SelectPadTarget(min_len);
  out.reserve(target_len);
  out.insert(out.end(), kPadMagic, kPadMagic + sizeof(kPadMagic));
  const std::uint32_t len32 = static_cast<std::uint32_t>(plain.size());
  out.push_back(static_cast<std::uint8_t>(len32 & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len32 >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len32 >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((len32 >> 24) & 0xFF));
  out.insert(out.end(), plain.begin(), plain.end());
  if (out.size() < target_len) {
    const std::size_t pad_len = target_len - out.size();
    const std::size_t offset = out.size();
    out.resize(target_len);
    if (!RandomBytes(out.data() + offset, pad_len)) {
      error = "pad rng failed";
      return false;
    }
  }
  return true;
}

bool UnpadPayload(const std::vector<std::uint8_t>& plain,
                  std::vector<std::uint8_t>& out,
                  std::string& error) {
  error.clear();
  out.clear();
  if (plain.size() < kPadHeaderBytes ||
      std::memcmp(plain.data(), kPadMagic, sizeof(kPadMagic)) != 0) {
    out = plain;
    return true;
  }
  const std::uint32_t len =
      static_cast<std::uint32_t>(plain[4]) |
      (static_cast<std::uint32_t>(plain[5]) << 8) |
      (static_cast<std::uint32_t>(plain[6]) << 16) |
      (static_cast<std::uint32_t>(plain[7]) << 24);
  if (kPadHeaderBytes + len > plain.size()) {
    error = "pad size invalid";
    return false;
  }
  out.assign(plain.begin() + kPadHeaderBytes,
             plain.begin() + kPadHeaderBytes + len);
  return true;
}

bool IsAllZero(const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return true;
  }
  std::uint8_t acc = 0;
  for (std::size_t i = 0; i < len; ++i) {
    acc |= data[i];
  }
  return acc == 0;
}

std::size_t LargestPowerOfTwoLessThan(std::size_t n) {
  if (n <= 1) {
    return 0;
  }
  std::size_t k = 1;
  while ((k << 1) < n) {
    k <<= 1;
  }
  return k;
}

mi::server::Sha256Hash HashNode(const mi::server::Sha256Hash& left,
                                const mi::server::Sha256Hash& right) {
  std::uint8_t buf[1 + 32 + 32];
  buf[0] = 0x01;
  std::memcpy(buf + 1, left.data(), left.size());
  std::memcpy(buf + 1 + 32, right.data(), right.size());
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(buf, sizeof(buf), d);
  return d.bytes;
}

mi::server::Sha256Hash HashLeaf(const std::vector<std::uint8_t>& leaf_data) {
  std::vector<std::uint8_t> buf;
  buf.reserve(1 + leaf_data.size());
  buf.push_back(0x00);
  buf.insert(buf.end(), leaf_data.begin(), leaf_data.end());
  mi::server::crypto::Sha256Digest d;
  mi::server::crypto::Sha256(buf.data(), buf.size(), d);
  return d.bytes;
}

mi::server::Sha256Hash KtLeafHashFromBundle(const std::string& username,
                                            const std::vector<std::uint8_t>& bundle,
                                            std::string& error) {
  error.clear();
  if (username.empty()) {
    error = "username empty";
    return {};
  }
  if (bundle.size() <
      1 + mi::server::kKtIdentitySigPublicKeyBytes +
          mi::server::kKtIdentityDhPublicKeyBytes) {
    error = "bundle invalid";
    return {};
  }

  std::array<std::uint8_t, mi::server::kKtIdentitySigPublicKeyBytes> id_sig_pk{};
  std::array<std::uint8_t, mi::server::kKtIdentityDhPublicKeyBytes> id_dh_pk{};
  std::memcpy(id_sig_pk.data(), bundle.data() + 1, id_sig_pk.size());
  std::memcpy(id_dh_pk.data(), bundle.data() + 1 + id_sig_pk.size(),
              id_dh_pk.size());

  std::vector<std::uint8_t> leaf_data;
  static constexpr char kPrefix[] = "mi_e2ee_kt_leaf_v1";
  leaf_data.reserve(sizeof(kPrefix) - 1 + 1 + username.size() + 1 +
                    id_sig_pk.size() + id_dh_pk.size());
  leaf_data.insert(leaf_data.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  leaf_data.push_back(0);
  leaf_data.insert(leaf_data.end(), username.begin(), username.end());
  leaf_data.push_back(0);
  leaf_data.insert(leaf_data.end(), id_sig_pk.begin(), id_sig_pk.end());
  leaf_data.insert(leaf_data.end(), id_dh_pk.begin(), id_dh_pk.end());
  return HashLeaf(leaf_data);
}

bool RootFromAuditPath(const mi::server::Sha256Hash& leaf_hash,
                       std::size_t leaf_index, std::size_t tree_size,
                       const std::vector<mi::server::Sha256Hash>& audit_path,
                       mi::server::Sha256Hash& out_root) {
  out_root = {};
  if (tree_size == 0 || leaf_index >= tree_size) {
    return false;
  }

  std::size_t remaining = audit_path.size();
  const auto rec = [&](auto&& self, const mi::server::Sha256Hash& leaf,
                       std::size_t m, std::size_t n, std::size_t& end,
                       mi::server::Sha256Hash& out) -> bool {
    if (n == 1) {
      if (end != 0) {
        return false;
      }
      out = leaf;
      return true;
    }
    if (end == 0) {
      return false;
    }
    const std::size_t k = LargestPowerOfTwoLessThan(n);
    if (k == 0) {
      return false;
    }
    const mi::server::Sha256Hash sibling = audit_path[end - 1];
    end--;
    if (m < k) {
      mi::server::Sha256Hash left{};
      if (!self(self, leaf, m, k, end, left)) {
        return false;
      }
      out = HashNode(left, sibling);
      return true;
    }
    mi::server::Sha256Hash right{};
    if (!self(self, leaf, m - k, n - k, end, right)) {
      return false;
    }
    out = HashNode(sibling, right);
    return true;
  };

  std::size_t end = remaining;
  if (!rec(rec, leaf_hash, leaf_index, tree_size, end, out_root)) {
    return false;
  }
  return end == 0;
}

bool ReconstructConsistencySubproof(
    std::size_t m, std::size_t n, bool b,
    const mi::server::Sha256Hash& old_root,
    const std::vector<mi::server::Sha256Hash>& proof,
    std::size_t& end_index,
    mi::server::Sha256Hash& out_old,
    mi::server::Sha256Hash& out_new) {
  if (m == 0 || n == 0 || m > n) {
    return false;
  }
  if (m == n) {
    if (b) {
      out_old = old_root;
      out_new = old_root;
      return true;
    }
    if (end_index == 0) {
      return false;
    }
    const mi::server::Sha256Hash node = proof[end_index - 1];
    end_index--;
    out_old = node;
    out_new = node;
    return true;
  }
  const std::size_t k = LargestPowerOfTwoLessThan(n);
  if (k == 0 || end_index == 0) {
    return false;
  }
  if (m <= k) {
    const mi::server::Sha256Hash right = proof[end_index - 1];
    end_index--;
    mi::server::Sha256Hash left_old{};
    mi::server::Sha256Hash left_new{};
    if (!ReconstructConsistencySubproof(m, k, b, old_root, proof, end_index,
                                        left_old, left_new)) {
      return false;
    }
    out_old = left_old;
    out_new = HashNode(left_new, right);
    return true;
  }

  const mi::server::Sha256Hash left = proof[end_index - 1];
  end_index--;
  mi::server::Sha256Hash right_old{};
  mi::server::Sha256Hash right_new{};
  if (!ReconstructConsistencySubproof(m - k, n - k, false, old_root, proof,
                                      end_index, right_old, right_new)) {
    return false;
  }
  out_old = HashNode(left, right_old);
  out_new = HashNode(left, right_new);
  return true;
}

bool VerifyConsistencyProof(std::size_t old_size, std::size_t new_size,
                            const mi::server::Sha256Hash& old_root,
                            const mi::server::Sha256Hash& new_root,
                            const std::vector<mi::server::Sha256Hash>& proof) {
  if (old_size == 0 || new_size == 0 || old_size > new_size) {
    return false;
  }
  if (old_size == new_size) {
    return proof.empty() && old_root == new_root;
  }
  std::size_t end = proof.size();
  mi::server::Sha256Hash calc_old{};
  mi::server::Sha256Hash calc_new{};
  if (!ReconstructConsistencySubproof(old_size, new_size, true, old_root, proof,
                                      end, calc_old, calc_new)) {
    return false;
  }
  return end == 0 && calc_old == old_root && calc_new == new_root;
}

constexpr std::uint8_t kGossipMagic[8] = {'M', 'I', 'K', 'T', 'G', 'S', 'P', '1'};

std::vector<std::uint8_t> WrapWithGossip(const std::vector<std::uint8_t>& plain,
                                         std::uint64_t tree_size,
                                         const std::array<std::uint8_t, 32>& root) {
  std::vector<std::uint8_t> out;
  out.reserve(sizeof(kGossipMagic) + 8 + root.size() + 4 + plain.size());
  out.insert(out.end(), std::begin(kGossipMagic), std::end(kGossipMagic));
  mi::server::proto::WriteUint64(tree_size, out);
  out.insert(out.end(), root.begin(), root.end());
  mi::server::proto::WriteUint32(static_cast<std::uint32_t>(plain.size()), out);
  out.insert(out.end(), plain.begin(), plain.end());
  return out;
}

bool UnwrapGossip(const std::vector<std::uint8_t>& in,
                  std::uint64_t& out_tree_size,
                  std::array<std::uint8_t, 32>& out_root,
                  std::vector<std::uint8_t>& out_plain) {
  out_tree_size = 0;
  out_root.fill(0);
  out_plain.clear();
  if (in.size() < sizeof(kGossipMagic) + 8 + 32 + 4) {
    return false;
  }
  if (std::memcmp(in.data(), kGossipMagic, sizeof(kGossipMagic)) != 0) {
    return false;
  }
  std::size_t off = sizeof(kGossipMagic);
  std::uint64_t size = 0;
  if (off + 8 > in.size()) {
    return false;
  }
  for (int i = 0; i < 8; ++i) {
    size |= (static_cast<std::uint64_t>(in[off + static_cast<std::size_t>(i)])
             << (i * 8));
  }
  off += 8;
  if (off + out_root.size() > in.size()) {
    return false;
  }
  std::memcpy(out_root.data(), in.data() + off, out_root.size());
  off += out_root.size();
  if (off + 4 > in.size()) {
    return false;
  }
  std::uint32_t len = static_cast<std::uint32_t>(in[off]) |
                      (static_cast<std::uint32_t>(in[off + 1]) << 8) |
                      (static_cast<std::uint32_t>(in[off + 2]) << 16) |
                      (static_cast<std::uint32_t>(in[off + 3]) << 24);
  off += 4;
  if (off + len != in.size()) {
    return false;
  }
  out_tree_size = size;
  out_plain.assign(in.begin() + off, in.end());
  return true;
}

namespace {

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

std::uint64_t NowUnixSeconds() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto sec = std::chrono::duration_cast<std::chrono::seconds>(now).count();
  if (sec <= 0) {
    return 0;
  }
  return static_cast<std::uint64_t>(sec);
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

bool ParsePairingCodeSecret16(const std::string& pairing_code,
                             std::array<std::uint8_t, 16>& out_secret) {
  out_secret.fill(0);
  const std::string norm = NormalizeCode(pairing_code);
  std::vector<std::uint8_t> bytes;
  if (!HexToBytes(norm, bytes) || bytes.size() != out_secret.size()) {
    if (!bytes.empty()) {
      crypto_wipe(bytes.data(), bytes.size());
    }
    return false;
  }
  std::memcpy(out_secret.data(), bytes.data(), out_secret.size());
  crypto_wipe(bytes.data(), bytes.size());
  return true;
}

bool DerivePairingIdAndKey(const std::array<std::uint8_t, 16>& secret,
                           std::string& out_pairing_id_hex,
                           std::array<std::uint8_t, 32>& out_key) {
  out_pairing_id_hex.clear();
  out_key.fill(0);
  static constexpr char kIdPrefix[] = "mi_e2ee_pairing_id_v1";
  std::vector<std::uint8_t> buf;
  buf.reserve(sizeof(kIdPrefix) - 1 + secret.size());
  buf.insert(buf.end(),
             reinterpret_cast<const std::uint8_t*>(kIdPrefix),
             reinterpret_cast<const std::uint8_t*>(kIdPrefix) +
                 (sizeof(kIdPrefix) - 1));
  buf.insert(buf.end(), secret.begin(), secret.end());
  const std::string digest = Sha256Hex(buf.data(), buf.size());
  if (!buf.empty()) {
    crypto_wipe(buf.data(), buf.size());
  }
  if (digest.size() < 32) {
    return false;
  }
  out_pairing_id_hex = digest.substr(0, 32);

  static constexpr char kInfo[] = "mi_e2ee_pairing_key_v1";
  if (!mi::server::crypto::HkdfSha256(
          secret.data(), secret.size(), nullptr, 0,
          reinterpret_cast<const std::uint8_t*>(kInfo), std::strlen(kInfo),
          out_key.data(), out_key.size())) {
    out_pairing_id_hex.clear();
    out_key.fill(0);
    return false;
  }
  return true;
}

bool EncryptPairingPayload(const std::array<std::uint8_t, 32>& key,
                           const std::vector<std::uint8_t>& plaintext,
                           std::vector<std::uint8_t>& out_cipher) {
  out_cipher.clear();
  if (plaintext.empty()) {
    return false;
  }
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'Y'};
  static constexpr std::uint8_t kVer = 1;
  std::uint8_t ad[5];
  std::memcpy(ad + 0, kMagic, sizeof(kMagic));
  ad[4] = kVer;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    return false;
  }

  out_cipher.resize(sizeof(ad) + nonce.size() + 16 + plaintext.size());
  std::memcpy(out_cipher.data(), ad, sizeof(ad));
  std::memcpy(out_cipher.data() + sizeof(ad), nonce.data(), nonce.size());
  std::uint8_t* mac = out_cipher.data() + sizeof(ad) + nonce.size();
  std::uint8_t* cipher = mac + 16;

  crypto_aead_lock(cipher, mac, key.data(), nonce.data(), ad, sizeof(ad),
                   plaintext.data(), plaintext.size());
  return true;
}

bool DecryptPairingPayload(const std::array<std::uint8_t, 32>& key,
                           const std::vector<std::uint8_t>& cipher,
                           std::vector<std::uint8_t>& out_plaintext) {
  out_plaintext.clear();
  if (cipher.size() < (5 + 24 + 16 + 1)) {
    return false;
  }
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'Y'};
  if (std::memcmp(cipher.data(), kMagic, sizeof(kMagic)) != 0) {
    return false;
  }
  if (cipher[4] != 1) {
    return false;
  }

  const std::uint8_t* ad = cipher.data();
  static constexpr std::size_t kAdSize = 5;
  const std::uint8_t* nonce = cipher.data() + kAdSize;
  const std::uint8_t* mac = nonce + 24;
  const std::uint8_t* ctext = mac + 16;
  const std::size_t ctext_len = cipher.size() - kAdSize - 24 - 16;

  out_plaintext.resize(ctext_len);
  const int rc =
      crypto_aead_unlock(out_plaintext.data(), mac, key.data(), nonce, ad,
                         kAdSize, ctext, ctext_len);
  if (rc != 0) {
    out_plaintext.clear();
    return false;
  }
  return true;
}

bool WriteFixed16(const std::array<std::uint8_t, 16>& v,
                  std::vector<std::uint8_t>& out);

bool ReadFixed16(const std::vector<std::uint8_t>& data, std::size_t& offset,
                 std::array<std::uint8_t, 16>& out);

bool EncodePairingRequestPlain(const std::string& device_id,
                              const std::array<std::uint8_t, 16>& request_id,
                              std::vector<std::uint8_t>& out) {
  out.clear();
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'R'};
  static constexpr std::uint8_t kVer = 1;
  out.insert(out.end(), kMagic, kMagic + sizeof(kMagic));
  out.push_back(kVer);
  WriteFixed16(request_id, out);
  return mi::server::proto::WriteString(device_id, out);
}

bool DecodePairingRequestPlain(const std::vector<std::uint8_t>& plain,
                              std::string& out_device_id,
                              std::array<std::uint8_t, 16>& out_request_id) {
  out_device_id.clear();
  out_request_id.fill(0);
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'R'};
  if (plain.size() < (sizeof(kMagic) + 1 + out_request_id.size())) {
    return false;
  }
  std::size_t off = 0;
  if (std::memcmp(plain.data(), kMagic, sizeof(kMagic)) != 0) {
    return false;
  }
  off += sizeof(kMagic);
  if (plain[off++] != 1) {
    return false;
  }
  if (!ReadFixed16(plain, off, out_request_id)) {
    return false;
  }
  return mi::server::proto::ReadString(plain, off, out_device_id) &&
         off == plain.size();
}

bool EncodePairingResponsePlain(const std::array<std::uint8_t, 16>& request_id,
                               const std::array<std::uint8_t, 32>& device_sync_key,
                               std::vector<std::uint8_t>& out) {
  out.clear();
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'S'};
  static constexpr std::uint8_t kVer = 1;
  out.insert(out.end(), kMagic, kMagic + sizeof(kMagic));
  out.push_back(kVer);
  WriteFixed16(request_id, out);
  out.insert(out.end(), device_sync_key.begin(), device_sync_key.end());
  return true;
}

bool DecodePairingResponsePlain(const std::vector<std::uint8_t>& plain,
                               std::array<std::uint8_t, 16>& out_request_id,
                               std::array<std::uint8_t, 32>& out_device_sync_key) {
  out_request_id.fill(0);
  out_device_sync_key.fill(0);
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'P', 'S'};
  if (plain.size() != (sizeof(kMagic) + 1 + out_request_id.size() +
                       out_device_sync_key.size())) {
    return false;
  }
  std::size_t off = 0;
  if (std::memcmp(plain.data(), kMagic, sizeof(kMagic)) != 0) {
    return false;
  }
  off += sizeof(kMagic);
  if (plain[off++] != 1) {
    return false;
  }
  if (!ReadFixed16(plain, off, out_request_id)) {
    return false;
  }
  if (off + out_device_sync_key.size() != plain.size()) {
    return false;
  }
  std::memcpy(out_device_sync_key.data(), plain.data() + off,
              out_device_sync_key.size());
  return true;
}

constexpr std::uint8_t kChatMagic[4] = {'M', 'I', 'C', 'H'};
constexpr std::uint8_t kChatVersion = 1;
constexpr std::uint8_t kChatTypeText = 1;
constexpr std::uint8_t kChatTypeAck = 2;
constexpr std::uint8_t kChatTypeFile = 3;
constexpr std::uint8_t kChatTypeGroupText = 4;
constexpr std::uint8_t kChatTypeGroupInvite = 5;
constexpr std::uint8_t kChatTypeGroupFile = 6;
constexpr std::uint8_t kChatTypeGroupSenderKeyDist = 7;
constexpr std::uint8_t kChatTypeGroupSenderKeyReq = 8;
constexpr std::uint8_t kChatTypeRich = 9;
constexpr std::uint8_t kChatTypeReadReceipt = 10;
constexpr std::uint8_t kChatTypeTyping = 11;
constexpr std::uint8_t kChatTypeSticker = 12;
constexpr std::uint8_t kChatTypePresence = 13;
constexpr std::uint8_t kChatTypeGroupCallKeyDist = 14;
constexpr std::uint8_t kChatTypeGroupCallKeyReq = 15;

constexpr std::uint8_t kGroupCallOpCreate = 1;
constexpr std::uint8_t kGroupCallOpJoin = 2;
constexpr std::uint8_t kGroupCallOpLeave = 3;
constexpr std::uint8_t kGroupCallOpEnd = 4;
constexpr std::uint8_t kGroupCallOpUpdate = 5;
constexpr std::uint8_t kGroupCallOpPing = 6;

constexpr std::size_t kChatHeaderSize = sizeof(kChatMagic) + 1 + 1 + 16;
constexpr std::size_t kChatSeenLimit = 4096;
constexpr std::size_t kPendingGroupCipherLimit = 512;

constexpr std::uint8_t kDeviceSyncEventSendPrivate = 1;
constexpr std::uint8_t kDeviceSyncEventSendGroup = 2;
constexpr std::uint8_t kDeviceSyncEventMessage = 3;
constexpr std::uint8_t kDeviceSyncEventDelivery = 4;
constexpr std::uint8_t kDeviceSyncEventGroupNotice = 5;
constexpr std::uint8_t kDeviceSyncEventRotateKey = 6;
constexpr std::uint8_t kDeviceSyncEventHistorySnapshot = 7;

constexpr std::uint8_t kGroupNoticeJoin = 1;
constexpr std::uint8_t kGroupNoticeLeave = 2;
constexpr std::uint8_t kGroupNoticeKick = 3;
constexpr std::uint8_t kGroupNoticeRoleSet = 4;

constexpr std::uint8_t kHistorySnapshotKindEnvelope = 1;
constexpr std::uint8_t kHistorySnapshotKindSystem = 2;

bool WriteFixed16(const std::array<std::uint8_t, 16>& v,
                  std::vector<std::uint8_t>& out) {
  out.insert(out.end(), v.begin(), v.end());
  return true;
}

bool ReadFixed16(const std::vector<std::uint8_t>& data, std::size_t& offset,
                 std::array<std::uint8_t, 16>& out) {
  if (offset + out.size() > data.size()) {
    return false;
  }
  std::memcpy(out.data(), data.data() + offset, out.size());
  offset += out.size();
  return true;
}

struct DeviceSyncEvent {
  std::uint8_t type{0};
  bool is_group{false};
  bool outgoing{false};
  bool is_read{false};
  std::string conv_id;
  std::string sender;
  std::vector<std::uint8_t> envelope;
  std::array<std::uint8_t, 16> msg_id{};
  std::array<std::uint8_t, 32> new_key{};
  std::string target_device_id;
  std::vector<ChatHistoryMessage> history;
};

bool EncodeDeviceSyncSendPrivate(const std::string& peer_username,
                                const std::vector<std::uint8_t>& envelope,
                                std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventSendPrivate);
  return mi::server::proto::WriteString(peer_username, out) &&
         mi::server::proto::WriteBytes(envelope, out);
}

bool EncodeDeviceSyncSendGroup(const std::string& group_id,
                              const std::vector<std::uint8_t>& envelope,
                              std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventSendGroup);
  return mi::server::proto::WriteString(group_id, out) &&
         mi::server::proto::WriteBytes(envelope, out);
}

bool EncodeDeviceSyncMessage(bool is_group, bool outgoing,
                             const std::string& conv_id,
                             const std::string& sender,
                             const std::vector<std::uint8_t>& envelope,
                             std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventMessage);
  std::uint8_t flags = 0;
  if (is_group) {
    flags |= 0x01;
  }
  if (outgoing) {
    flags |= 0x02;
  }
  out.push_back(flags);
  return mi::server::proto::WriteString(conv_id, out) &&
         mi::server::proto::WriteString(sender, out) &&
         mi::server::proto::WriteBytes(envelope, out);
}

bool EncodeDeviceSyncDelivery(bool is_group, bool is_read,
                              const std::string& conv_id,
                              const std::array<std::uint8_t, 16>& msg_id,
                              std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventDelivery);
  std::uint8_t flags = 0;
  if (is_group) {
    flags |= 0x01;
  }
  if (is_read) {
    flags |= 0x02;
  }
  out.push_back(flags);
  return mi::server::proto::WriteString(conv_id, out) &&
         WriteFixed16(msg_id, out);
}

bool EncodeDeviceSyncGroupNotice(const std::string& group_id,
                                  const std::string& actor_username,
                                  const std::vector<std::uint8_t>& payload,
                                  std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventGroupNotice);
  return mi::server::proto::WriteString(group_id, out) &&
         mi::server::proto::WriteString(actor_username, out) &&
         mi::server::proto::WriteBytes(payload, out);
}

bool EncodeDeviceSyncRotateKey(const std::array<std::uint8_t, 32>& key,
                               std::vector<std::uint8_t>& out) {
  out.clear();
  out.push_back(kDeviceSyncEventRotateKey);
  out.insert(out.end(), key.begin(), key.end());
  return true;
}

bool EncodeHistorySnapshotEntry(const ChatHistoryMessage& msg,
                                std::vector<std::uint8_t>& out) {
  out.clear();
  if (msg.conv_id.empty()) {
    return false;
  }
  if (msg.is_system) {
    if (msg.system_text_utf8.empty()) {
      return false;
    }
    out.push_back(kHistorySnapshotKindSystem);
  } else {
    if (msg.sender.empty() || msg.envelope.empty()) {
      return false;
    }
    out.push_back(kHistorySnapshotKindEnvelope);
  }
  std::uint8_t flags = 0;
  if (msg.is_group) {
    flags |= 0x01;
  }
  if (msg.outgoing) {
    flags |= 0x02;
  }
  out.push_back(flags);

  const std::uint8_t st = static_cast<std::uint8_t>(msg.status);
  if (st > static_cast<std::uint8_t>(ChatHistoryStatus::kFailed)) {
    return false;
  }
  out.push_back(st);

  mi::server::proto::WriteUint64(msg.timestamp_sec, out);
  mi::server::proto::WriteString(msg.conv_id, out);
  if (msg.is_system) {
    mi::server::proto::WriteString(msg.system_text_utf8, out);
    return true;
  }
  return mi::server::proto::WriteString(msg.sender, out) &&
         mi::server::proto::WriteBytes(msg.envelope, out);
}

bool DecodeDeviceSyncEvent(const std::vector<std::uint8_t>& plain,
                           DeviceSyncEvent& out) {
  out = DeviceSyncEvent{};
  if (plain.empty()) {
    return false;
  }
  std::size_t off = 0;
  out.type = plain[off++];
  if (out.type == kDeviceSyncEventSendPrivate) {
    if (!mi::server::proto::ReadString(plain, off, out.conv_id) ||
        !mi::server::proto::ReadBytes(plain, off, out.envelope) ||
        off != plain.size()) {
      return false;
    }
    return true;
  }
  if (out.type == kDeviceSyncEventSendGroup) {
    if (!mi::server::proto::ReadString(plain, off, out.conv_id) ||
        !mi::server::proto::ReadBytes(plain, off, out.envelope) ||
        off != plain.size()) {
      return false;
    }
    return true;
  }
  if (out.type == kDeviceSyncEventMessage) {
    if (off >= plain.size()) {
      return false;
    }
    const std::uint8_t flags = plain[off++];
    out.is_group = (flags & 0x01) != 0;
    out.outgoing = (flags & 0x02) != 0;
    if (!mi::server::proto::ReadString(plain, off, out.conv_id) ||
        !mi::server::proto::ReadString(plain, off, out.sender) ||
        !mi::server::proto::ReadBytes(plain, off, out.envelope) ||
        off != plain.size()) {
      return false;
    }
    return true;
  }
  if (out.type == kDeviceSyncEventDelivery) {
    if (off >= plain.size()) {
      return false;
    }
    const std::uint8_t flags = plain[off++];
    out.is_group = (flags & 0x01) != 0;
    out.is_read = (flags & 0x02) != 0;
    if (!mi::server::proto::ReadString(plain, off, out.conv_id) ||
        !ReadFixed16(plain, off, out.msg_id) || off != plain.size()) {
      return false;
    }
    return true;
  }
  if (out.type == kDeviceSyncEventGroupNotice) {
    out.is_group = true;
    if (!mi::server::proto::ReadString(plain, off, out.conv_id) ||
        !mi::server::proto::ReadString(plain, off, out.sender) ||
        !mi::server::proto::ReadBytes(plain, off, out.envelope) ||
        off != plain.size()) {
      return false;
    }
    return true;
  }
  if (out.type == kDeviceSyncEventHistorySnapshot) {
    if (!mi::server::proto::ReadString(plain, off, out.target_device_id)) {
      return false;
    }
    std::uint32_t count = 0;
    if (!mi::server::proto::ReadUint32(plain, off, count)) {
      return false;
    }
    out.history.clear();
    out.history.reserve(count > 4096u ? 4096u : count);
    for (std::uint32_t i = 0; i < count; ++i) {
      if (off + 1 + 1 + 1 + 8 > plain.size()) {
        return false;
      }
      const std::uint8_t kind = plain[off++];
      const std::uint8_t flags = plain[off++];
      const bool is_group = (flags & 0x01) != 0;
      const bool outgoing = (flags & 0x02) != 0;
      const std::uint8_t st = plain[off++];
      if (st > static_cast<std::uint8_t>(ChatHistoryStatus::kFailed)) {
        return false;
      }
      std::uint64_t ts = 0;
      if (!mi::server::proto::ReadUint64(plain, off, ts)) {
        return false;
      }
      std::string conv_id;
      if (!mi::server::proto::ReadString(plain, off, conv_id) || conv_id.empty()) {
        return false;
      }

      ChatHistoryMessage m;
      m.is_group = is_group;
      m.outgoing = outgoing;
      m.status = static_cast<ChatHistoryStatus>(st);
      m.timestamp_sec = ts;
      m.conv_id = std::move(conv_id);

      if (kind == kHistorySnapshotKindEnvelope) {
        if (!mi::server::proto::ReadString(plain, off, m.sender) ||
            !mi::server::proto::ReadBytes(plain, off, m.envelope) ||
            m.sender.empty() || m.envelope.empty()) {
          return false;
        }
        m.is_system = false;
      } else if (kind == kHistorySnapshotKindSystem) {
        std::string text;
        if (!mi::server::proto::ReadString(plain, off, text) || text.empty()) {
          return false;
        }
        m.is_system = true;
        m.system_text_utf8 = std::move(text);
      } else {
        return false;
      }

      out.history.push_back(std::move(m));
    }
    return off == plain.size();
  }
  if (out.type == kDeviceSyncEventRotateKey) {
    if (off + out.new_key.size() != plain.size()) {
      return false;
    }
    std::memcpy(out.new_key.data(), plain.data() + off, out.new_key.size());
    return true;
  }
  return false;
}

bool DecodeGroupNoticePayload(const std::vector<std::uint8_t>& payload,
                             std::uint8_t& out_kind, std::string& out_target,
                             std::optional<std::uint8_t>& out_role) {
  out_kind = 0;
  out_target.clear();
  out_role = std::nullopt;
  if (payload.empty()) {
    return false;
  }
  std::size_t off = 0;
  out_kind = payload[off++];
  if (!mi::server::proto::ReadString(payload, off, out_target)) {
    return false;
  }
  if (out_kind == kGroupNoticeRoleSet) {
    if (off >= payload.size()) {
      return false;
    }
    out_role = payload[off++];
  }
  return off == payload.size();
}

std::string BytesToHexLower(const std::uint8_t* data, std::size_t len) {
  static constexpr char kHex[] = "0123456789abcdef";
  if (!data || len == 0) {
    return {};
  }
  std::string out;
  out.resize(len * 2);
  for (std::size_t i = 0; i < len; ++i) {
    out[i * 2] = kHex[data[i] >> 4];
    out[i * 2 + 1] = kHex[data[i] & 0x0F];
  }
  return out;
}

bool HexToFixedBytes16(const std::string& hex,
                       std::array<std::uint8_t, 16>& out) {
  std::vector<std::uint8_t> tmp;
  if (!HexToBytes(hex, tmp) || tmp.size() != out.size()) {
    return false;
  }
  std::memcpy(out.data(), tmp.data(), out.size());
  return true;
}

constexpr std::size_t kChatEnvelopeBaseBytes =
    sizeof(kChatMagic) + 1 + 1 + 16;

void ReserveChatEnvelope(std::vector<std::uint8_t>& out, std::size_t extra) {
  out.clear();
  out.reserve(kChatEnvelopeBaseBytes + extra);
}

bool EncodeChatText(const std::array<std::uint8_t, 16>& msg_id,
                    const std::string& text_utf8,
                    std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 2 + text_utf8.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeText);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return mi::server::proto::WriteString(text_utf8, out);
}

bool EncodeChatAck(const std::array<std::uint8_t, 16>& msg_id,
                   std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 0);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeAck);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return true;
}

bool EncodeChatReadReceipt(const std::array<std::uint8_t, 16>& msg_id,
                           std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 0);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeReadReceipt);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return true;
}

bool EncodeChatTyping(const std::array<std::uint8_t, 16>& msg_id, bool typing,
                      std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 1);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeTyping);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  out.push_back(typing ? 1 : 0);
  return true;
}

bool EncodeChatPresence(const std::array<std::uint8_t, 16>& msg_id, bool online,
                        std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 1);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypePresence);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  out.push_back(online ? 1 : 0);
  return true;
}

bool EncodeChatSticker(const std::array<std::uint8_t, 16>& msg_id,
                       const std::string& sticker_id,
                       std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 2 + sticker_id.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeSticker);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return mi::server::proto::WriteString(sticker_id, out);
}

bool EncodeChatGroupText(const std::array<std::uint8_t, 16>& msg_id,
                         const std::string& group_id,
                         const std::string& text_utf8,
                         std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out,
                      2 + group_id.size() + 2 + text_utf8.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupText);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return mi::server::proto::WriteString(group_id, out) &&
         mi::server::proto::WriteString(text_utf8, out);
}

bool EncodeChatGroupInvite(const std::array<std::uint8_t, 16>& msg_id,
                           const std::string& group_id,
                           std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 2 + group_id.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupInvite);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return mi::server::proto::WriteString(group_id, out);
}

std::vector<std::uint8_t> BuildGroupSenderKeyDistSigMessage(
    const std::string& group_id, std::uint32_t version, std::uint32_t iteration,
    const std::array<std::uint8_t, 32>& ck) {
  std::vector<std::uint8_t> msg;
  static constexpr char kPrefix[] = "MI_GSKD_V1";
  msg.reserve(sizeof(kPrefix) - 1 + 2 + group_id.size() + 4 + 4 + 4 + ck.size());
  msg.insert(msg.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  mi::server::proto::WriteString(group_id, msg);
  mi::server::proto::WriteUint32(version, msg);
  mi::server::proto::WriteUint32(iteration, msg);
  mi::server::proto::WriteBytes(ck.data(), ck.size(), msg);
  return msg;
}

bool EncodeChatGroupSenderKeyDist(
    const std::array<std::uint8_t, 16>& msg_id, const std::string& group_id,
    std::uint32_t version, std::uint32_t iteration,
    const std::array<std::uint8_t, 32>& ck,
    const std::vector<std::uint8_t>& sig, std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, group_id.size() + sig.size() + 50);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupSenderKeyDist);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  if (!mi::server::proto::WriteString(group_id, out) ||
      !mi::server::proto::WriteUint32(version, out) ||
      !mi::server::proto::WriteUint32(iteration, out)) {
    out.clear();
    return false;
  }
  if (!mi::server::proto::WriteBytes(ck.data(), ck.size(), out) ||
      !mi::server::proto::WriteBytes(sig, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool DecodeChatGroupSenderKeyDist(
    const std::vector<std::uint8_t>& payload, std::size_t& offset,
    std::string& out_group_id, std::uint32_t& out_version,
    std::uint32_t& out_iteration, std::array<std::uint8_t, 32>& out_ck,
    std::vector<std::uint8_t>& out_sig) {
  out_group_id.clear();
  out_version = 0;
  out_iteration = 0;
  out_ck.fill(0);
  out_sig.clear();
  if (!mi::server::proto::ReadString(payload, offset, out_group_id) ||
      !mi::server::proto::ReadUint32(payload, offset, out_version) ||
      !mi::server::proto::ReadUint32(payload, offset, out_iteration)) {
    return false;
  }
  std::vector<std::uint8_t> ck_bytes;
  if (!mi::server::proto::ReadBytes(payload, offset, ck_bytes) ||
      ck_bytes.size() != out_ck.size()) {
    return false;
  }
  std::memcpy(out_ck.data(), ck_bytes.data(), out_ck.size());
  if (!mi::server::proto::ReadBytes(payload, offset, out_sig)) {
    return false;
  }
  return true;
}

bool EncodeChatGroupSenderKeyReq(const std::array<std::uint8_t, 16>& msg_id,
                                 const std::string& group_id,
                                 std::uint32_t want_version,
                                 std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 2 + group_id.size() + 4);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupSenderKeyReq);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  return mi::server::proto::WriteString(group_id, out) &&
         mi::server::proto::WriteUint32(want_version, out);
}

bool DecodeChatGroupSenderKeyReq(const std::vector<std::uint8_t>& payload,
                                 std::size_t& offset,
                                 std::string& out_group_id,
                                 std::uint32_t& out_want_version) {
  out_group_id.clear();
  out_want_version = 0;
  return mi::server::proto::ReadString(payload, offset, out_group_id) &&
         mi::server::proto::ReadUint32(payload, offset, out_want_version);
}

std::vector<std::uint8_t> BuildGroupCallKeyDistSigMessage(
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::array<std::uint8_t, 32>& call_key) {
  std::vector<std::uint8_t> msg;
  static constexpr char kPrefix[] = "MI_GCKD_V1";
  msg.reserve(sizeof(kPrefix) - 1 + 2 + group_id.size() + call_id.size() + 4 +
              2 + call_key.size());
  msg.insert(msg.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  mi::server::proto::WriteString(group_id, msg);
  msg.insert(msg.end(), call_id.begin(), call_id.end());
  mi::server::proto::WriteUint32(key_id, msg);
  mi::server::proto::WriteBytes(call_key.data(), call_key.size(), msg);
  return msg;
}

bool EncodeChatGroupCallKeyDist(const std::array<std::uint8_t, 16>& msg_id,
                                const std::string& group_id,
                                const std::array<std::uint8_t, 16>& call_id,
                                std::uint32_t key_id,
                                const std::array<std::uint8_t, 32>& call_key,
                                const std::vector<std::uint8_t>& sig,
                                std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, group_id.size() + sig.size() + 80);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupCallKeyDist);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  if (!mi::server::proto::WriteString(group_id, out)) {
    out.clear();
    return false;
  }
  out.insert(out.end(), call_id.begin(), call_id.end());
  if (!mi::server::proto::WriteUint32(key_id, out)) {
    out.clear();
    return false;
  }
  if (!mi::server::proto::WriteBytes(call_key.data(), call_key.size(), out) ||
      !mi::server::proto::WriteBytes(sig, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool DecodeChatGroupCallKeyDist(const std::vector<std::uint8_t>& payload,
                                std::size_t& offset,
                                std::string& out_group_id,
                                std::array<std::uint8_t, 16>& out_call_id,
                                std::uint32_t& out_key_id,
                                std::array<std::uint8_t, 32>& out_call_key,
                                std::vector<std::uint8_t>& out_sig) {
  out_group_id.clear();
  out_call_id.fill(0);
  out_key_id = 0;
  out_call_key.fill(0);
  out_sig.clear();
  if (!mi::server::proto::ReadString(payload, offset, out_group_id)) {
    return false;
  }
  if (offset + out_call_id.size() > payload.size()) {
    return false;
  }
  std::memcpy(out_call_id.data(), payload.data() + offset, out_call_id.size());
  offset += out_call_id.size();
  if (!mi::server::proto::ReadUint32(payload, offset, out_key_id)) {
    return false;
  }
  std::vector<std::uint8_t> key_bytes;
  if (!mi::server::proto::ReadBytes(payload, offset, key_bytes) ||
      key_bytes.size() != out_call_key.size()) {
    return false;
  }
  std::memcpy(out_call_key.data(), key_bytes.data(), out_call_key.size());
  if (!mi::server::proto::ReadBytes(payload, offset, out_sig)) {
    return false;
  }
  return true;
}

bool EncodeChatGroupCallKeyReq(const std::array<std::uint8_t, 16>& msg_id,
                               const std::string& group_id,
                               const std::array<std::uint8_t, 16>& call_id,
                               std::uint32_t want_key_id,
                               std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, group_id.size() + 32);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupCallKeyReq);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  if (!mi::server::proto::WriteString(group_id, out)) {
    out.clear();
    return false;
  }
  out.insert(out.end(), call_id.begin(), call_id.end());
  if (!mi::server::proto::WriteUint32(want_key_id, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool DecodeChatGroupCallKeyReq(const std::vector<std::uint8_t>& payload,
                               std::size_t& offset,
                               std::string& out_group_id,
                               std::array<std::uint8_t, 16>& out_call_id,
                               std::uint32_t& out_want_key_id) {
  out_group_id.clear();
  out_call_id.fill(0);
  out_want_key_id = 0;
  if (!mi::server::proto::ReadString(payload, offset, out_group_id)) {
    return false;
  }
  if (offset + out_call_id.size() > payload.size()) {
    return false;
  }
  std::memcpy(out_call_id.data(), payload.data() + offset, out_call_id.size());
  offset += out_call_id.size();
  return mi::server::proto::ReadUint32(payload, offset, out_want_key_id);
}

constexpr std::uint8_t kRichKindText = 1;
constexpr std::uint8_t kRichKindLocation = 2;
constexpr std::uint8_t kRichKindContactCard = 3;

constexpr std::uint8_t kRichFlagHasReply = 0x01;

struct RichDecoded {
  std::uint8_t kind{0};
  bool has_reply{false};
  std::array<std::uint8_t, 16> reply_to{};
  std::string reply_preview;
  std::string text;
  std::int32_t lat_e7{0};
  std::int32_t lon_e7{0};
  std::string location_label;
  std::string card_username;
  std::string card_display;
};

std::string FormatCoordE7(std::int32_t v_e7) {
  const std::int64_t v64 = static_cast<std::int64_t>(v_e7);
  const bool neg = v64 < 0;
  const std::uint64_t abs = static_cast<std::uint64_t>(neg ? -v64 : v64);
  const std::uint64_t deg = abs / 10000000ULL;
  const std::uint64_t frac = abs % 10000000ULL;
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%s%llu.%07llu", neg ? "-" : "",
                static_cast<unsigned long long>(deg),
                static_cast<unsigned long long>(frac));
  return std::string(buf);
}

bool EncodeChatRichText(const std::array<std::uint8_t, 16>& msg_id,
                        const std::string& text_utf8, bool has_reply,
                        const std::array<std::uint8_t, 16>& reply_to,
                        const std::string& reply_preview_utf8,
                        std::vector<std::uint8_t>& out) {
  std::size_t extra = 2 + 2 + text_utf8.size();
  if (has_reply) {
    extra += reply_to.size() + 2 + reply_preview_utf8.size();
  }
  ReserveChatEnvelope(out, extra);
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeRich);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  out.push_back(kRichKindText);
  std::uint8_t flags = 0;
  if (has_reply) {
    flags |= kRichFlagHasReply;
  }
  out.push_back(flags);
  if (has_reply) {
    out.insert(out.end(), reply_to.begin(), reply_to.end());
    if (!mi::server::proto::WriteString(reply_preview_utf8, out)) {
      out.clear();
      return false;
    }
  }
  return mi::server::proto::WriteString(text_utf8, out);
}

bool EncodeChatRichLocation(const std::array<std::uint8_t, 16>& msg_id,
                            std::int32_t lat_e7, std::int32_t lon_e7,
                            const std::string& label_utf8,
                            std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out, 2 + 8 + 2 + label_utf8.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeRich);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  out.push_back(kRichKindLocation);
  out.push_back(0);
  if (!mi::server::proto::WriteUint32(static_cast<std::uint32_t>(lat_e7), out) ||
      !mi::server::proto::WriteUint32(static_cast<std::uint32_t>(lon_e7), out) ||
      !mi::server::proto::WriteString(label_utf8, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool EncodeChatRichContactCard(const std::array<std::uint8_t, 16>& msg_id,
                               const std::string& card_username,
                               const std::string& card_display,
                               std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out,
                      2 + 2 + card_username.size() + 2 + card_display.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeRich);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  out.push_back(kRichKindContactCard);
  out.push_back(0);
  if (!mi::server::proto::WriteString(card_username, out) ||
      !mi::server::proto::WriteString(card_display, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool DecodeChatRich(const std::vector<std::uint8_t>& payload, std::size_t& offset,
                    RichDecoded& out) {
  out = RichDecoded{};
  if (offset + 2 > payload.size()) {
    return false;
  }
  out.kind = payload[offset++];
  const std::uint8_t flags = payload[offset++];
  out.has_reply = (flags & kRichFlagHasReply) != 0;
  if (out.has_reply) {
    if (!ReadFixed16(payload, offset, out.reply_to) ||
        !mi::server::proto::ReadString(payload, offset, out.reply_preview)) {
      return false;
    }
  }

  if (out.kind == kRichKindText) {
    return mi::server::proto::ReadString(payload, offset, out.text);
  }
  if (out.kind == kRichKindLocation) {
    std::uint32_t lat_u = 0;
    std::uint32_t lon_u = 0;
    if (!mi::server::proto::ReadUint32(payload, offset, lat_u) ||
        !mi::server::proto::ReadUint32(payload, offset, lon_u) ||
        !mi::server::proto::ReadString(payload, offset, out.location_label)) {
      return false;
    }
    out.lat_e7 = static_cast<std::int32_t>(lat_u);
    out.lon_e7 = static_cast<std::int32_t>(lon_u);
    return true;
  }
  if (out.kind == kRichKindContactCard) {
    return mi::server::proto::ReadString(payload, offset, out.card_username) &&
           mi::server::proto::ReadString(payload, offset, out.card_display);
  }
  return false;
}

std::string FormatRichAsText(const RichDecoded& msg) {
  std::string out;
  if (msg.has_reply) {
    out += "【回复】";
    if (!msg.reply_preview.empty()) {
      out += msg.reply_preview;
    } else {
      out += "（引用）";
    }
    out += "\n";
  }

  if (msg.kind == kRichKindText) {
    out += msg.text;
    return out;
  }
  if (msg.kind == kRichKindLocation) {
    out += "【位置】";
    out += msg.location_label.empty() ? "（未命名）" : msg.location_label;
    out += "\nlat:";
    out += FormatCoordE7(msg.lat_e7);
    out += ", lon:";
    out += FormatCoordE7(msg.lon_e7);
    return out;
  }
  if (msg.kind == kRichKindContactCard) {
    out += "【名片】";
    out += msg.card_username.empty() ? "（空）" : msg.card_username;
    if (!msg.card_display.empty()) {
      out += " (";
      out += msg.card_display;
      out += ")";
    }
    return out;
  }
  out += "【未知消息】";
  return out;
}

struct HistorySummaryDecoded {
  ChatHistorySummaryKind kind{ChatHistorySummaryKind::kNone};
  std::string text;
  std::string file_id;
  std::string file_name;
  std::uint64_t file_size{0};
  std::string sticker_id;
  std::int32_t lat_e7{0};
  std::int32_t lon_e7{0};
  std::string location_label;
  std::string card_username;
  std::string card_display;
  std::string group_id;
};

bool DecodeHistorySummary(const std::vector<std::uint8_t>& payload,
                          HistorySummaryDecoded& out) {
  out = HistorySummaryDecoded{};
  const std::size_t header_len = kHistorySummaryMagic.size() + 2;
  if (payload.size() < header_len) {
    return false;
  }
  if (std::memcmp(payload.data(), kHistorySummaryMagic.data(),
                  kHistorySummaryMagic.size()) != 0) {
    return false;
  }
  std::size_t off = kHistorySummaryMagic.size();
  const std::uint8_t version = payload[off++];
  if (version != kHistorySummaryVersion) {
    return false;
  }
  out.kind = static_cast<ChatHistorySummaryKind>(payload[off++]);

  if (out.kind == ChatHistorySummaryKind::kText) {
    return mi::server::proto::ReadString(payload, off, out.text) &&
           off == payload.size();
  }
  if (out.kind == ChatHistorySummaryKind::kFile) {
    return mi::server::proto::ReadUint64(payload, off, out.file_size) &&
           mi::server::proto::ReadString(payload, off, out.file_name) &&
           mi::server::proto::ReadString(payload, off, out.file_id) &&
           off == payload.size();
  }
  if (out.kind == ChatHistorySummaryKind::kSticker) {
    return mi::server::proto::ReadString(payload, off, out.sticker_id) &&
           off == payload.size();
  }
  if (out.kind == ChatHistorySummaryKind::kLocation) {
    std::uint32_t lat_u = 0;
    std::uint32_t lon_u = 0;
    if (!mi::server::proto::ReadUint32(payload, off, lat_u) ||
        !mi::server::proto::ReadUint32(payload, off, lon_u) ||
        !mi::server::proto::ReadString(payload, off, out.location_label) ||
        off != payload.size()) {
      return false;
    }
    out.lat_e7 = static_cast<std::int32_t>(lat_u);
    out.lon_e7 = static_cast<std::int32_t>(lon_u);
    return true;
  }
  if (out.kind == ChatHistorySummaryKind::kContactCard) {
    return mi::server::proto::ReadString(payload, off, out.card_username) &&
           mi::server::proto::ReadString(payload, off, out.card_display) &&
           off == payload.size();
  }
  if (out.kind == ChatHistorySummaryKind::kGroupInvite) {
    return mi::server::proto::ReadString(payload, off, out.group_id) &&
           off == payload.size();
  }
  return false;
}

std::string FormatSummaryAsText(const HistorySummaryDecoded& summary) {
  if (summary.kind == ChatHistorySummaryKind::kLocation ||
      summary.kind == ChatHistorySummaryKind::kContactCard) {
    RichDecoded rich;
    rich.kind = (summary.kind == ChatHistorySummaryKind::kLocation)
                    ? kRichKindLocation
                    : kRichKindContactCard;
    rich.location_label = summary.location_label;
    rich.lat_e7 = summary.lat_e7;
    rich.lon_e7 = summary.lon_e7;
    rich.card_username = summary.card_username;
    rich.card_display = summary.card_display;
    return FormatRichAsText(rich);
  }
  if (summary.kind == ChatHistorySummaryKind::kGroupInvite) {
    return summary.group_id.empty()
               ? std::string("Group invite")
               : (std::string("Group invite: ") + summary.group_id);
  }
  return summary.text;
}

bool ApplyHistorySummary(const std::vector<std::uint8_t>& summary,
                         ClientCore::HistoryEntry& entry) {
  HistorySummaryDecoded decoded;
  if (!DecodeHistorySummary(summary, decoded)) {
    return false;
  }
  if (decoded.kind == ChatHistorySummaryKind::kText) {
    entry.kind = ClientCore::HistoryKind::kText;
    entry.text_utf8 = std::move(decoded.text);
    return true;
  }
  if (decoded.kind == ChatHistorySummaryKind::kFile) {
    entry.kind = ClientCore::HistoryKind::kFile;
    entry.file_id = std::move(decoded.file_id);
    entry.file_name = std::move(decoded.file_name);
    entry.file_size = decoded.file_size;
    return true;
  }
  if (decoded.kind == ChatHistorySummaryKind::kSticker) {
    entry.kind = ClientCore::HistoryKind::kSticker;
    entry.sticker_id = std::move(decoded.sticker_id);
    return true;
  }
  if (decoded.kind == ChatHistorySummaryKind::kLocation ||
      decoded.kind == ChatHistorySummaryKind::kContactCard ||
      decoded.kind == ChatHistorySummaryKind::kGroupInvite) {
    entry.kind = ClientCore::HistoryKind::kText;
    entry.text_utf8 = FormatSummaryAsText(decoded);
    return true;
  }
  return false;
}

bool DecodeChatHeader(const std::vector<std::uint8_t>& payload,
                      std::uint8_t& out_type,
                      std::array<std::uint8_t, 16>& out_id,
                      std::size_t& offset) {
  offset = 0;
  if (payload.size() < kChatHeaderSize) {
    return false;
  }
  if (std::memcmp(payload.data(), kChatMagic, sizeof(kChatMagic)) != 0) {
    return false;
  }
  offset = sizeof(kChatMagic);
  const std::uint8_t version = payload[offset++];
  if (version != kChatVersion) {
    return false;
  }
  out_type = payload[offset++];
  std::memcpy(out_id.data(), payload.data() + offset, out_id.size());
  offset += out_id.size();
  return true;
}

constexpr std::uint8_t kGroupCipherMagic[4] = {'M', 'I', 'G', 'C'};
constexpr std::uint8_t kGroupCipherVersion = 1;
constexpr std::size_t kGroupCipherNonceBytes = 24;
constexpr std::size_t kGroupCipherMacBytes = 16;
constexpr std::size_t kMaxGroupSkippedMessageKeys = 2048;
constexpr std::size_t kMaxGroupSkip = 4096;
constexpr std::uint64_t kGroupSenderKeyRotationThreshold = 10000;
constexpr std::uint64_t kGroupSenderKeyRotationIntervalSec =
    7ull * 24ull * 60ull * 60ull;
constexpr std::chrono::seconds kSenderKeyDistResendInterval{5};

bool KdfGroupCk(const std::array<std::uint8_t, 32>& ck,
                std::array<std::uint8_t, 32>& out_ck,
                std::array<std::uint8_t, 32>& out_mk) {
  std::array<std::uint8_t, 64> buf{};
  static constexpr char kInfo[] = "mi_e2ee_group_sender_ck_v1";
  if (!mi::server::crypto::HkdfSha256(
          ck.data(), ck.size(), nullptr, 0,
          reinterpret_cast<const std::uint8_t*>(kInfo), std::strlen(kInfo),
          buf.data(), buf.size())) {
    return false;
  }
  std::memcpy(out_ck.data(), buf.data(), 32);
  std::memcpy(out_mk.data(), buf.data() + 32, 32);
  return true;
}

template <typename State>
void EnforceGroupSkippedLimit(State& state) {
  while (state.skipped_mks.size() > kMaxGroupSkippedMessageKeys) {
    if (state.skipped_order.empty()) {
      state.skipped_mks.clear();
      return;
    }
    const auto n = state.skipped_order.front();
    state.skipped_order.pop_front();
    state.skipped_mks.erase(n);
  }
}

template <typename State>
bool DeriveGroupMessageKey(State& state, std::uint32_t iteration,
                           std::array<std::uint8_t, 32>& out_mk) {
  out_mk.fill(0);
  if (iteration < state.next_iteration) {
    const auto it = state.skipped_mks.find(iteration);
    if (it == state.skipped_mks.end()) {
      return false;
    }
    out_mk = it->second;
    state.skipped_mks.erase(it);
    return true;
  }

  if (iteration - state.next_iteration > kMaxGroupSkip) {
    return false;
  }

  while (state.next_iteration < iteration) {
    std::array<std::uint8_t, 32> next_ck{};
    std::array<std::uint8_t, 32> mk{};
    if (!KdfGroupCk(state.ck, next_ck, mk)) {
      return false;
    }
    state.skipped_mks.emplace(state.next_iteration, mk);
    state.skipped_order.push_back(state.next_iteration);
    state.ck = next_ck;
    state.next_iteration++;
    EnforceGroupSkippedLimit(state);
  }

  std::array<std::uint8_t, 32> next_ck{};
  if (!KdfGroupCk(state.ck, next_ck, out_mk)) {
    return false;
  }
  state.ck = next_ck;
  state.next_iteration++;
  return true;
}

std::string MakeGroupSenderKeyMapKey(const std::string& group_id,
                                    const std::string& sender_username) {
  return group_id + "|" + sender_username;
}

std::string MakeGroupCallKeyMapKey(const std::string& group_id,
                                   const std::array<std::uint8_t, 16>& call_id) {
  const std::string call_hex =
      BytesToHexLower(call_id.data(), call_id.size());
  return group_id + "|" + call_hex;
}

std::string HashGroupMembers(std::vector<std::string> members) {
  std::sort(members.begin(), members.end());
  std::string joined;
  for (const auto& m : members) {
    joined.append(m);
    joined.push_back('\n');
  }
  return Sha256Hex(reinterpret_cast<const std::uint8_t*>(joined.data()),
                   joined.size());
}

void BuildGroupCipherAd(const std::string& group_id,
                        const std::string& sender_username,
                        std::uint32_t sender_key_version,
                        std::uint32_t sender_key_iteration,
                        std::vector<std::uint8_t>& out) {
  out.clear();
  static constexpr char kPrefix[] = "MI_GMSG_AD_V1";
  out.reserve(sizeof(kPrefix) - 1 + 2 + group_id.size() + 2 +
              sender_username.size() + 4 + 4);
  out.insert(out.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  mi::server::proto::WriteString(group_id, out);
  mi::server::proto::WriteString(sender_username, out);
  mi::server::proto::WriteUint32(sender_key_version, out);
  mi::server::proto::WriteUint32(sender_key_iteration, out);
}

bool EncodeGroupCipherNoSig(const std::string& group_id,
                            const std::string& sender_username,
                            std::uint32_t sender_key_version,
                            std::uint32_t sender_key_iteration,
                            const std::array<std::uint8_t, 24>& nonce,
                            const std::array<std::uint8_t, 16>& mac,
                            const std::vector<std::uint8_t>& cipher,
                            std::vector<std::uint8_t>& out) {
  out.clear();
  out.reserve(sizeof(kGroupCipherMagic) + 1 + 4 + 4 +
              2 + group_id.size() + 2 + sender_username.size() +
              4 + nonce.size() + 4 + mac.size() + 4 + cipher.size());
  out.insert(out.end(), kGroupCipherMagic,
             kGroupCipherMagic + sizeof(kGroupCipherMagic));
  out.push_back(kGroupCipherVersion);
  mi::server::proto::WriteUint32(sender_key_version, out);
  mi::server::proto::WriteUint32(sender_key_iteration, out);
  if (!mi::server::proto::WriteString(group_id, out) ||
      !mi::server::proto::WriteString(sender_username, out)) {
    out.clear();
    return false;
  }
  if (!mi::server::proto::WriteBytes(nonce.data(), nonce.size(), out) ||
      !mi::server::proto::WriteBytes(mac.data(), mac.size(), out) ||
      !mi::server::proto::WriteBytes(cipher, out)) {
    out.clear();
    return false;
  }
  return true;
}

bool DecodeGroupCipher(const std::vector<std::uint8_t>& payload,
                       std::uint32_t& out_sender_key_version,
                       std::uint32_t& out_sender_key_iteration,
                       std::string& out_group_id,
                       std::string& out_sender_username,
                       std::array<std::uint8_t, 24>& out_nonce,
                       std::array<std::uint8_t, 16>& out_mac,
                       std::vector<std::uint8_t>& out_cipher,
                       std::vector<std::uint8_t>& out_sig,
                       std::size_t& out_sig_offset) {
  out_sender_key_version = 0;
  out_sender_key_iteration = 0;
  out_group_id.clear();
  out_sender_username.clear();
  out_nonce.fill(0);
  out_mac.fill(0);
  out_cipher.clear();
  out_sig.clear();
  out_sig_offset = 0;

  if (payload.size() < sizeof(kGroupCipherMagic) + 1) {
    return false;
  }
  if (std::memcmp(payload.data(), kGroupCipherMagic,
                  sizeof(kGroupCipherMagic)) != 0) {
    return false;
  }
  std::size_t off = sizeof(kGroupCipherMagic);
  const std::uint8_t version = payload[off++];
  if (version != kGroupCipherVersion) {
    return false;
  }
  if (!mi::server::proto::ReadUint32(payload, off, out_sender_key_version) ||
      !mi::server::proto::ReadUint32(payload, off, out_sender_key_iteration) ||
      !mi::server::proto::ReadString(payload, off, out_group_id) ||
      !mi::server::proto::ReadString(payload, off, out_sender_username)) {
    return false;
  }
  std::vector<std::uint8_t> nonce_bytes;
  std::vector<std::uint8_t> mac_bytes;
  if (!mi::server::proto::ReadBytes(payload, off, nonce_bytes) ||
      nonce_bytes.size() != kGroupCipherNonceBytes ||
      !mi::server::proto::ReadBytes(payload, off, mac_bytes) ||
      mac_bytes.size() != kGroupCipherMacBytes ||
      !mi::server::proto::ReadBytes(payload, off, out_cipher)) {
    return false;
  }
  std::memcpy(out_nonce.data(), nonce_bytes.data(), out_nonce.size());
  std::memcpy(out_mac.data(), mac_bytes.data(), out_mac.size());
  out_sig_offset = off;
  if (!mi::server::proto::ReadBytes(payload, off, out_sig) ||
      off != payload.size()) {
    return false;
  }
  return true;
}

constexpr std::uint8_t kFileBlobMagic[4] = {'M', 'I', 'F', '1'};
constexpr std::uint8_t kFileBlobVersionV1 = 1;
constexpr std::uint8_t kFileBlobVersionV2 = 2;
constexpr std::uint8_t kFileBlobVersionV3 = 3;
constexpr std::uint8_t kFileBlobVersionV4 = 4;
constexpr std::uint8_t kFileBlobAlgoRaw = 0;
constexpr std::uint8_t kFileBlobAlgoDeflate = 1;
constexpr std::uint8_t kFileBlobFlagDoubleCompression = 0x01;
constexpr std::size_t kFileBlobV1PrefixSize = sizeof(kFileBlobMagic) + 1 + 3;
constexpr std::size_t kFileBlobV1HeaderSize = kFileBlobV1PrefixSize + 24 + 16;
constexpr std::size_t kFileBlobV2PrefixSize =
    sizeof(kFileBlobMagic) + 1 + 1 + 1 + 1 + 8 + 8 + 8;
constexpr std::size_t kFileBlobV2HeaderSize = kFileBlobV2PrefixSize + 24 + 16;
constexpr std::size_t kFileBlobV3PrefixSize =
    sizeof(kFileBlobMagic) + 1 + 1 + 1 + 1 + 4 + 8 + 24;
constexpr std::size_t kFileBlobV3HeaderSize = kFileBlobV3PrefixSize;
constexpr std::size_t kFileBlobV4BaseHeaderSize =
    sizeof(kFileBlobMagic) + 1 + 1 + 1 + 1 + 4 + 8 + 24;
constexpr std::size_t kMaxChatFileBytes = 300u * 1024u * 1024u;
constexpr std::size_t kMaxChatFileBlobBytes = 320u * 1024u * 1024u;
constexpr std::uint32_t kFileBlobV3ChunkBytes = 256u * 1024u;
constexpr std::uint32_t kFileBlobV4PlainChunkBytes = 128u * 1024u;
constexpr std::uint32_t kE2eeBlobChunkBytes = 4u * 1024u * 1024u;
constexpr std::size_t kFileBlobV4PadBuckets[] = {
    64u * 1024u,
    96u * 1024u,
    128u * 1024u,
    160u * 1024u,
    192u * 1024u,
    256u * 1024u,
    384u * 1024u
};

bool LooksLikeAlreadyCompressedFileName(const std::string& file_name) {
  if (file_name.empty()) {
    return false;
  }
  std::string ext;
  const auto dot = file_name.find_last_of('.');
  if (dot != std::string::npos && dot + 1 < file_name.size()) {
    ext = file_name.substr(dot + 1);
  } else {
    return false;
  }
  for (auto& c : ext) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }

  static const std::unordered_set<std::string> kCompressed = {
      "jpg",  "jpeg", "png", "gif", "webp", "bmp", "ico",  "heic",
      "mp4",  "mkv",  "mov", "webm","avi",  "flv", "m4v",
      "mp3",  "m4a",  "aac", "ogg", "opus", "flac", "wav",
      "zip",  "rar",  "7z",  "gz",  "bz2",  "xz",  "zst",
      "pdf",  "docx", "xlsx","pptx"
  };
  return kCompressed.find(ext) != kCompressed.end();
}

std::size_t SelectFileChunkTarget(std::size_t min_len) {
  if (min_len == 0 || min_len > (kE2eeBlobChunkBytes - 16u)) {
    return 0;
  }
  for (const auto bucket : kFileBlobV4PadBuckets) {
    if (bucket >= min_len) {
      if (bucket == min_len) {
        return bucket;
      }
      std::uint32_t r = 0;
      if (!RandomUint32(r)) {
        return bucket;
      }
      const std::size_t span = bucket - min_len;
      return min_len + (static_cast<std::size_t>(r) % (span + 1));
    }
  }
  const std::size_t round = ((min_len + 4095) / 4096) * 4096;
  if (round < min_len || round > (kE2eeBlobChunkBytes - 16u)) {
    return 0;
  }
  std::uint32_t r = 0;
  if (!RandomUint32(r)) {
    return round;
  }
  const std::size_t span = round - min_len;
  return min_len + (static_cast<std::size_t>(r) % (span + 1));
}

bool DeflateCompress(const std::uint8_t* data, std::size_t len, int level,
                     std::vector<std::uint8_t>& out) {
  out.clear();
  if (!data || len == 0) {
    return false;
  }
  if (len >
      static_cast<std::size_t>((std::numeric_limits<mz_ulong>::max)())) {
    return false;
  }

  const mz_ulong src_len = static_cast<mz_ulong>(len);
  const mz_ulong bound = mz_compressBound(src_len);
  std::vector<std::uint8_t> buf;
  buf.resize(static_cast<std::size_t>(bound));
  mz_ulong out_len = bound;
  const int status = mz_compress2(buf.data(), &out_len, data, src_len, level);
  if (status != MZ_OK) {
    crypto_wipe(buf.data(), buf.size());
    return false;
  }
  buf.resize(static_cast<std::size_t>(out_len));
  out = std::move(buf);
  return true;
}

bool DeflateDecompress(const std::uint8_t* data, std::size_t len,
                       std::size_t expected_len,
                       std::vector<std::uint8_t>& out) {
  out.clear();
  if (!data || len == 0 || expected_len == 0) {
    return false;
  }
  if (expected_len >
      static_cast<std::size_t>((std::numeric_limits<mz_ulong>::max)())) {
    return false;
  }
  if (len > static_cast<std::size_t>((std::numeric_limits<mz_ulong>::max)())) {
    return false;
  }

  std::vector<std::uint8_t> buf;
  buf.resize(expected_len);
  mz_ulong out_len = static_cast<mz_ulong>(expected_len);
  const int status = mz_uncompress(buf.data(), &out_len, data,
                                  static_cast<mz_ulong>(len));
  if (status != MZ_OK || out_len != static_cast<mz_ulong>(expected_len)) {
    crypto_wipe(buf.data(), buf.size());
    return false;
  }
  out = std::move(buf);
  return true;
}

bool EncodeChatFile(const std::array<std::uint8_t, 16>& msg_id,
                    std::uint64_t file_size,
                    const std::string& file_name,
                    const std::string& file_id,
                    const std::array<std::uint8_t, 32>& file_key,
                    std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out,
                      8 + 2 + file_name.size() + 2 + file_id.size() +
                          file_key.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeFile);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  if (!mi::server::proto::WriteUint64(file_size, out) ||
      !mi::server::proto::WriteString(file_name, out) ||
      !mi::server::proto::WriteString(file_id, out)) {
    out.clear();
    return false;
  }
  out.insert(out.end(), file_key.begin(), file_key.end());
  return true;
}

bool EncodeChatGroupFile(const std::array<std::uint8_t, 16>& msg_id,
                         const std::string& group_id,
                         std::uint64_t file_size,
                         const std::string& file_name,
                         const std::string& file_id,
                         const std::array<std::uint8_t, 32>& file_key,
                         std::vector<std::uint8_t>& out) {
  ReserveChatEnvelope(out,
                      2 + group_id.size() + 8 + 2 + file_name.size() + 2 +
                          file_id.size() + file_key.size());
  out.insert(out.end(), kChatMagic, kChatMagic + sizeof(kChatMagic));
  out.push_back(kChatVersion);
  out.push_back(kChatTypeGroupFile);
  out.insert(out.end(), msg_id.begin(), msg_id.end());
  if (!mi::server::proto::WriteString(group_id, out) ||
      !mi::server::proto::WriteUint64(file_size, out) ||
      !mi::server::proto::WriteString(file_name, out) ||
      !mi::server::proto::WriteString(file_id, out)) {
    out.clear();
    return false;
  }
  out.insert(out.end(), file_key.begin(), file_key.end());
  return true;
}

bool DecodeChatFile(const std::vector<std::uint8_t>& payload,
                    std::size_t& offset,
                    std::uint64_t& out_file_size,
                    std::string& out_file_name,
                    std::string& out_file_id,
                    std::array<std::uint8_t, 32>& out_file_key) {
  out_file_size = 0;
  out_file_name.clear();
  out_file_id.clear();
  out_file_key.fill(0);
  if (!mi::server::proto::ReadUint64(payload, offset, out_file_size) ||
      !mi::server::proto::ReadString(payload, offset, out_file_name) ||
      !mi::server::proto::ReadString(payload, offset, out_file_id)) {
    return false;
  }
  if (offset + out_file_key.size() != payload.size()) {
    return false;
  }
  std::memcpy(out_file_key.data(), payload.data() + offset, out_file_key.size());
  offset += out_file_key.size();
  return true;
}

bool DecodeChatGroupFile(const std::vector<std::uint8_t>& payload,
                         std::size_t& offset,
                         std::string& out_group_id,
                         std::uint64_t& out_file_size,
                         std::string& out_file_name,
                         std::string& out_file_id,
                         std::array<std::uint8_t, 32>& out_file_key) {
  out_group_id.clear();
  if (!mi::server::proto::ReadString(payload, offset, out_group_id)) {
    return false;
  }
  return DecodeChatFile(payload, offset, out_file_size, out_file_name,
                        out_file_id, out_file_key);
}

bool EncryptFileBlobAdaptive(const std::vector<std::uint8_t>& plaintext,
                             const std::array<std::uint8_t, 32>& key,
                             const std::string& file_name,
                             std::vector<std::uint8_t>& out_blob) {
  out_blob.clear();
  if (plaintext.empty()) {
    return false;
  }
  if (plaintext.size() > kMaxChatFileBytes) {
    return false;
  }

  const bool skip_compress = LooksLikeAlreadyCompressedFileName(file_name);

  if (skip_compress) {
    std::vector<std::uint8_t> header;
    header.reserve(kFileBlobV2PrefixSize);
    header.insert(header.end(), kFileBlobMagic,
                  kFileBlobMagic + sizeof(kFileBlobMagic));
    header.push_back(kFileBlobVersionV2);
    header.push_back(0);
    header.push_back(kFileBlobAlgoRaw);
    header.push_back(0);
    mi::server::proto::WriteUint64(static_cast<std::uint64_t>(plaintext.size()),
                                   header);
    mi::server::proto::WriteUint64(0, header);
    mi::server::proto::WriteUint64(static_cast<std::uint64_t>(plaintext.size()),
                                   header);
    if (header.size() != kFileBlobV2PrefixSize) {
      return false;
    }

    std::array<std::uint8_t, 24> nonce{};
    if (!RandomBytes(nonce.data(), nonce.size())) {
      return false;
    }

    out_blob.resize(header.size() + nonce.size() + 16 + plaintext.size());
    std::memcpy(out_blob.data(), header.data(), header.size());
    std::memcpy(out_blob.data() + header.size(), nonce.data(), nonce.size());
    std::uint8_t* mac = out_blob.data() + header.size() + nonce.size();
    std::uint8_t* cipher = mac + 16;
    crypto_aead_lock(cipher, mac, key.data(), nonce.data(), header.data(),
                     header.size(), plaintext.data(), plaintext.size());
    return true;
  }

  std::vector<std::uint8_t> stage1;
  if (!DeflateCompress(plaintext.data(), plaintext.size(), 1, stage1)) {
    return false;
  }
  if (stage1.size() >= plaintext.size()) {
    crypto_wipe(stage1.data(), stage1.size());
    std::vector<std::uint8_t> header;
    header.reserve(kFileBlobV2PrefixSize);
    header.insert(header.end(), kFileBlobMagic,
                  kFileBlobMagic + sizeof(kFileBlobMagic));
    header.push_back(kFileBlobVersionV2);
    header.push_back(0);
    header.push_back(kFileBlobAlgoRaw);
    header.push_back(0);
    mi::server::proto::WriteUint64(static_cast<std::uint64_t>(plaintext.size()),
                                   header);
    mi::server::proto::WriteUint64(0, header);
    mi::server::proto::WriteUint64(static_cast<std::uint64_t>(plaintext.size()),
                                   header);
    if (header.size() != kFileBlobV2PrefixSize) {
      return false;
    }

    std::array<std::uint8_t, 24> nonce{};
    if (!RandomBytes(nonce.data(), nonce.size())) {
      return false;
    }

    out_blob.resize(header.size() + nonce.size() + 16 + plaintext.size());
    std::memcpy(out_blob.data(), header.data(), header.size());
    std::memcpy(out_blob.data() + header.size(), nonce.data(), nonce.size());
    std::uint8_t* mac = out_blob.data() + header.size() + nonce.size();
    std::uint8_t* cipher = mac + 16;
    crypto_aead_lock(cipher, mac, key.data(), nonce.data(), header.data(),
                     header.size(), plaintext.data(), plaintext.size());
    return true;
  }

  std::vector<std::uint8_t> stage2;
  if (!DeflateCompress(stage1.data(), stage1.size(), 9, stage2)) {
    crypto_wipe(stage1.data(), stage1.size());
    return false;
  }

  std::vector<std::uint8_t> header;
  header.reserve(kFileBlobV2PrefixSize);
  header.insert(header.end(), kFileBlobMagic,
                kFileBlobMagic + sizeof(kFileBlobMagic));
  header.push_back(kFileBlobVersionV2);
  header.push_back(kFileBlobFlagDoubleCompression);
  header.push_back(kFileBlobAlgoDeflate);
  header.push_back(0);
  mi::server::proto::WriteUint64(static_cast<std::uint64_t>(plaintext.size()),
                                 header);
  mi::server::proto::WriteUint64(static_cast<std::uint64_t>(stage1.size()),
                                 header);
  mi::server::proto::WriteUint64(static_cast<std::uint64_t>(stage2.size()),
                                 header);
  if (header.size() != kFileBlobV2PrefixSize) {
    crypto_wipe(stage1.data(), stage1.size());
    crypto_wipe(stage2.data(), stage2.size());
    return false;
  }

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    crypto_wipe(stage1.data(), stage1.size());
    crypto_wipe(stage2.data(), stage2.size());
    return false;
  }

  out_blob.resize(header.size() + nonce.size() + 16 + stage2.size());
  std::memcpy(out_blob.data(), header.data(), header.size());
  std::memcpy(out_blob.data() + header.size(), nonce.data(), nonce.size());
  std::uint8_t* mac = out_blob.data() + header.size() + nonce.size();
  std::uint8_t* cipher = mac + 16;
  crypto_aead_lock(cipher, mac, key.data(), nonce.data(), header.data(),
                   header.size(), stage2.data(), stage2.size());

  crypto_wipe(stage1.data(), stage1.size());
  crypto_wipe(stage2.data(), stage2.size());
  return true;
}

bool DecryptFileBlob(const std::vector<std::uint8_t>& blob,
                     const std::array<std::uint8_t, 32>& key,
                     std::vector<std::uint8_t>& out_plaintext) {
  out_plaintext.clear();
  if (blob.size() < kFileBlobV1HeaderSize) {
    return false;
  }
  if (std::memcmp(blob.data(), kFileBlobMagic, sizeof(kFileBlobMagic)) != 0) {
    return false;
  }
  const std::uint8_t version = blob[sizeof(kFileBlobMagic)];

  std::size_t header_len = 0;
  std::size_t header_size = 0;
  std::uint8_t flags = 0;
  std::uint8_t algo = 0;
  std::uint64_t original_size = 0;
  std::uint64_t stage1_size = 0;
  std::uint64_t stage2_size = 0;
  if (version == kFileBlobVersionV1) {
    header_len = kFileBlobV1PrefixSize;
    header_size = kFileBlobV1HeaderSize;
  } else if (version == kFileBlobVersionV2) {
    header_len = kFileBlobV2PrefixSize;
    header_size = kFileBlobV2HeaderSize;
    if (blob.size() < header_size) {
      return false;
    }
    std::size_t off = sizeof(kFileBlobMagic) + 1;
    if (off + 3 > blob.size()) {
      return false;
    }
    flags = blob[off++];
    algo = blob[off++];
    off++;  // reserved
    if (!mi::server::proto::ReadUint64(blob, off, original_size) ||
        !mi::server::proto::ReadUint64(blob, off, stage1_size) ||
        !mi::server::proto::ReadUint64(blob, off, stage2_size) ||
        off != kFileBlobV2PrefixSize) {
      return false;
    }
    if (original_size == 0 ||
        original_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
      return false;
    }
    if (stage2_size == 0 ||
        stage2_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
      return false;
    }
  } else if (version == kFileBlobVersionV3) {
    header_len = kFileBlobV3PrefixSize;
    header_size = kFileBlobV3HeaderSize;
    if (blob.size() < header_size + 16 + 1) {
      return false;
    }

    std::size_t off = sizeof(kFileBlobMagic) + 1;
    if (off + 3 > blob.size()) {
      return false;
    }
    flags = blob[off++];
    algo = blob[off++];
    off++;  // reserved
    std::uint32_t chunk_size = 0;
    if (!mi::server::proto::ReadUint32(blob, off, chunk_size) ||
        !mi::server::proto::ReadUint64(blob, off, original_size) ||
        off + 24 != kFileBlobV3PrefixSize) {
      return false;
    }
    if (algo != kFileBlobAlgoRaw) {
      return false;
    }
    if (chunk_size == 0 || chunk_size > (kE2eeBlobChunkBytes - 16u)) {
      return false;
    }
    if (original_size == 0 ||
        original_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
      return false;
    }
    const std::uint64_t chunks =
        (original_size + chunk_size - 1) / chunk_size;
    if (chunks == 0 || chunks > (1ull << 31)) {
      return false;
    }
    const std::uint64_t expect =
        static_cast<std::uint64_t>(kFileBlobV3PrefixSize) +
        chunks * 16u + original_size;
    if (expect == 0 ||
        expect > static_cast<std::uint64_t>(kMaxChatFileBlobBytes) ||
        expect != static_cast<std::uint64_t>(blob.size())) {
      return false;
    }

    std::array<std::uint8_t, 24> base_nonce{};
    std::memcpy(base_nonce.data(), blob.data() + off, base_nonce.size());

    out_plaintext.resize(static_cast<std::size_t>(original_size));
    const std::uint8_t* header = blob.data();
    std::size_t blob_off = kFileBlobV3PrefixSize;
    std::uint64_t out_off = 0;
    for (std::uint64_t idx = 0; idx < chunks; ++idx) {
      const std::size_t want =
          static_cast<std::size_t>(std::min<std::uint64_t>(
              chunk_size, original_size - out_off));
      if (want == 0 || blob_off + 16 + want > blob.size()) {
        out_plaintext.clear();
        return false;
      }

      std::array<std::uint8_t, 24> nonce = base_nonce;
      for (int i = 0; i < 8; ++i) {
        nonce[16 + i] = static_cast<std::uint8_t>((idx >> (8 * i)) & 0xFF);
      }

      const std::uint8_t* mac = blob.data() + blob_off;
      const std::uint8_t* cipher = blob.data() + blob_off + 16;
      const int ok = crypto_aead_unlock(out_plaintext.data() + out_off, mac,
                                        key.data(), nonce.data(), header,
                                        header_len, cipher, want);
      if (ok != 0) {
        crypto_wipe(out_plaintext.data(), out_plaintext.size());
        out_plaintext.clear();
        return false;
      }
      blob_off += 16 + want;
      out_off += want;
    }
    if (out_off != original_size || blob_off != blob.size()) {
      crypto_wipe(out_plaintext.data(), out_plaintext.size());
      out_plaintext.clear();
      return false;
    }
    (void)flags;
    return true;
  } else {
    return false;
  }

  const std::uint8_t* header = blob.data();
  const std::uint8_t* nonce = blob.data() + header_len;
  const std::uint8_t* mac = blob.data() + header_len + 24;
  const std::size_t cipher_off = header_size;
  const std::size_t cipher_len = blob.size() - cipher_off;
  if (version == kFileBlobVersionV2 &&
      cipher_len != static_cast<std::size_t>(stage2_size)) {
    return false;
  }

  std::vector<std::uint8_t> stage2_plain;
  stage2_plain.resize(cipher_len);
  const int ok = crypto_aead_unlock(
      stage2_plain.data(), mac, key.data(), nonce, header, header_len,
      blob.data() + cipher_off, cipher_len);
  if (ok != 0) {
    crypto_wipe(stage2_plain.data(), stage2_plain.size());
    return false;
  }

  if (version == kFileBlobVersionV1) {
    out_plaintext = std::move(stage2_plain);
    return true;
  }

  if ((flags & kFileBlobFlagDoubleCompression) == 0) {
    if (original_size != static_cast<std::uint64_t>(stage2_plain.size())) {
      crypto_wipe(stage2_plain.data(), stage2_plain.size());
      return false;
    }
    out_plaintext = std::move(stage2_plain);
    return true;
  }
  if (algo != kFileBlobAlgoDeflate) {
    crypto_wipe(stage2_plain.data(), stage2_plain.size());
    return false;
  }
  if (stage1_size == 0 ||
      stage1_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    crypto_wipe(stage2_plain.data(), stage2_plain.size());
    return false;
  }

  std::vector<std::uint8_t> stage1_plain;
  if (!DeflateDecompress(stage2_plain.data(), stage2_plain.size(),
                         static_cast<std::size_t>(stage1_size),
                         stage1_plain)) {
    crypto_wipe(stage2_plain.data(), stage2_plain.size());
    return false;
  }
  crypto_wipe(stage2_plain.data(), stage2_plain.size());

  std::vector<std::uint8_t> original;
  if (!DeflateDecompress(stage1_plain.data(), stage1_plain.size(),
                         static_cast<std::size_t>(original_size), original)) {
    crypto_wipe(stage1_plain.data(), stage1_plain.size());
    return false;
  }
  crypto_wipe(stage1_plain.data(), stage1_plain.size());

  out_plaintext = std::move(original);
  return true;
}

}  // namespace

#if 0  // moved below (after transport helpers) to satisfy declarations order
void ClientCore::ResetRemoteStream() {
  std::lock_guard<std::mutex> lock(remote_stream_mutex_);
  remote_stream_.reset();
}

struct ClientCore::RemoteStream {
  std::string host;
  std::uint16_t port{0};
  bool use_tls{false};
  bool use_kcp{false};
  KcpConfig kcp_cfg;
  ProxyConfig proxy;
  std::string pinned_fingerprint;

  ikcpcb* kcp{nullptr};
  std::uint32_t kcp_conv{0};
  std::vector<std::uint8_t> kcp_recv_buf;
  std::chrono::steady_clock::time_point kcp_last_active{};

#ifdef _WIN32
  SOCKET sock{INVALID_SOCKET};
  ScopedCredHandle cred;
  ScopedCtxtHandle ctx;
  SecPkgContext_StreamSizes sizes{};
  std::vector<std::uint8_t> enc_buf;
  std::vector<std::uint8_t> plain_buf;
  std::size_t plain_off{0};
#else
  int sock{-1};
#endif

  RemoteStream(std::string host_in, std::uint16_t port_in, bool use_tls_in,
               ProxyConfig proxy_in, std::string pinned_fingerprint_in)
      : host(std::move(host_in)),
        port(port_in),
        use_tls(use_tls_in),
        proxy(std::move(proxy_in)),
        pinned_fingerprint(std::move(pinned_fingerprint_in)) {}

  ~RemoteStream() { Close(); }

  bool Matches(const std::string& host_in, std::uint16_t port_in, bool use_tls_in,
               const ProxyConfig& proxy_in,
               const std::string& pinned_fingerprint_in) const {
    if (host != host_in || port != port_in || use_tls != use_tls_in ||
        pinned_fingerprint != pinned_fingerprint_in) {
      return false;
    }
    return proxy.type == proxy_in.type && proxy.host == proxy_in.host &&
           proxy.port == proxy_in.port && proxy.username == proxy_in.username &&
           proxy.password == proxy_in.password;
  }

  void Close() {
#ifdef _WIN32
    if (sock != INVALID_SOCKET) {
      closesocket(sock);
      sock = INVALID_SOCKET;
    }
#else
    if (sock >= 0) {
      ::close(sock);
      sock = -1;
    }
#endif
    enc_buf.clear();
    plain_buf.clear();
    plain_off = 0;
  }

  bool ConnectPlain(std::string& error) {
    error.clear();
    if (host.empty() || port == 0) {
      error = "invalid endpoint";
      return false;
    }

    const bool use_proxy = proxy.enabled();
    if (use_proxy && proxy.type != ProxyType::kSocks5) {
      error = "unsupported proxy";
      return false;
    }
    const std::string connect_host = use_proxy ? proxy.host : host;
    const std::uint16_t connect_port = use_proxy ? proxy.port : port;

#ifdef _WIN32
    if (!EnsureWinsock()) {
      error = "winsock init failed";
      return false;
    }
#endif

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const std::string port_str = std::to_string(connect_port);
    if (getaddrinfo(connect_host.c_str(), port_str.c_str(), &hints, &result) !=
        0) {
      error = "dns resolve failed";
      return false;
    }

#ifdef _WIN32
    SOCKET new_sock = INVALID_SOCKET;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
      new_sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (new_sock == INVALID_SOCKET) {
        continue;
      }
      if (::connect(new_sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) ==
          0) {
        break;
      }
      closesocket(new_sock);
      new_sock = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    if (new_sock == INVALID_SOCKET) {
      error = "connect failed";
      return false;
    }
    const DWORD timeout_ms = 30000;
    setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms),
               static_cast<int>(sizeof(timeout_ms)));
    setsockopt(new_sock, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout_ms),
               static_cast<int>(sizeof(timeout_ms)));
#else
    int new_sock = -1;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
      new_sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (new_sock < 0) {
        continue;
      }
      if (::connect(new_sock, rp->ai_addr, rp->ai_addrlen) == 0) {
        break;
      }
      ::close(new_sock);
      new_sock = -1;
    }
    freeaddrinfo(result);
    if (new_sock < 0) {
      error = "connect failed";
      return false;
    }
    timeval tv{};
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    ::setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO, &tv,
                static_cast<socklen_t>(sizeof(tv)));
    ::setsockopt(new_sock, SOL_SOCKET, SO_SNDTIMEO, &tv,
                static_cast<socklen_t>(sizeof(tv)));
#endif

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
                    static_cast<std::size_t>((std::numeric_limits<int>::max)())
                ? (std::numeric_limits<int>::max)()
                : static_cast<int>(remaining);
#ifdef _WIN32
        const int n = ::send(new_sock,
                             reinterpret_cast<const char*>(data + sent), chunk,
                             0);
#else
        const ssize_t n =
            ::send(new_sock, data + sent, static_cast<std::size_t>(chunk), 0);
#endif
        if (n <= 0) {
          return false;
        }
        sent += static_cast<std::size_t>(n);
      }
      return true;
    };

    const auto recv_exact = [&](std::uint8_t* data,
                                std::size_t len) -> bool {
      if (!data || len == 0) {
        return true;
      }
      std::size_t got = 0;
      while (got < len) {
        const std::size_t remaining = len - got;
        const int want =
            remaining >
                    static_cast<std::size_t>((std::numeric_limits<int>::max)())
                ? (std::numeric_limits<int>::max)()
                : static_cast<int>(remaining);
#ifdef _WIN32
        const int n =
            ::recv(new_sock, reinterpret_cast<char*>(data + got), want, 0);
#else
        const ssize_t n =
            ::recv(new_sock, data + got, static_cast<std::size_t>(want), 0);
#endif
        if (n <= 0) {
          return false;
        }
        got += static_cast<std::size_t>(n);
      }
      return true;
    };

    if (use_proxy) {
      std::uint8_t hello[4] = {0x05, 0x01, 0x00, 0x00};
      if (!proxy.username.empty() && !proxy.password.empty()) {
        hello[1] = 0x02;
        hello[3] = 0x02;
      }
      std::uint8_t sel[2] = {0, 0};
      if (!send_all(hello, hello[1] == 0x02 ? 4 : 3) ||
          !recv_exact(sel, sizeof(sel)) || sel[0] != 0x05) {
        error = "proxy handshake failed";
        goto fail;
      }

      if (sel[1] == 0x02) {
        if (proxy.username.size() > 255 || proxy.password.size() > 255) {
          error = "proxy auth invalid";
          goto fail;
        }
        std::vector<std::uint8_t> auth;
        auth.reserve(3 + proxy.username.size() + proxy.password.size());
        auth.push_back(0x01);
        auth.push_back(static_cast<std::uint8_t>(proxy.username.size()));
        auth.insert(auth.end(), proxy.username.begin(), proxy.username.end());
        auth.push_back(static_cast<std::uint8_t>(proxy.password.size()));
        auth.insert(auth.end(), proxy.password.begin(), proxy.password.end());
        std::uint8_t ar[2] = {0, 0};
        if (!send_all(auth.data(), auth.size()) ||
            !recv_exact(ar, sizeof(ar)) || ar[0] != 0x01 || ar[1] != 0x00) {
          error = "proxy auth failed";
          goto fail;
        }
      } else if (sel[1] != 0x00) {
        error = "proxy method unsupported";
        goto fail;
      }

      if (host.size() > 255) {
        error = "target host too long";
        goto fail;
      }
      std::vector<std::uint8_t> req;
      req.reserve(7 + host.size());
      req.push_back(0x05);
      req.push_back(0x01);
      req.push_back(0x00);
      req.push_back(0x03);
      req.push_back(static_cast<std::uint8_t>(host.size()));
      req.insert(req.end(), host.begin(), host.end());
      req.push_back(static_cast<std::uint8_t>((port >> 8) & 0xFF));
      req.push_back(static_cast<std::uint8_t>(port & 0xFF));

      std::uint8_t rep[4] = {0, 0, 0, 0};
      if (!send_all(req.data(), req.size()) || !recv_exact(rep, sizeof(rep)) ||
          rep[0] != 0x05 || rep[1] != 0x00) {
        error = "proxy connect failed";
        goto fail;
      }

      std::size_t to_read = 0;
      if (rep[3] == 0x01) {
        to_read = 4 + 2;
      } else if (rep[3] == 0x03) {
        std::uint8_t len_byte = 0;
        if (!recv_exact(&len_byte, 1)) {
          error = "proxy connect failed";
          goto fail;
        }
        to_read = static_cast<std::size_t>(len_byte) + 2;
      } else if (rep[3] == 0x04) {
        to_read = 16 + 2;
      } else {
        error = "proxy connect failed";
        goto fail;
      }
      std::vector<std::uint8_t> discard;
      discard.resize(to_read);
      if (!recv_exact(discard.data(), discard.size())) {
        error = "proxy connect failed";
        goto fail;
      }
    }

    sock = new_sock;
    return true;

  fail:
#ifdef _WIN32
    closesocket(new_sock);
#else
    ::close(new_sock);
#endif
    return false;
  }

#ifdef _WIN32
  bool ConnectTls(std::string& out_server_fingerprint, std::string& error) {
    out_server_fingerprint.clear();
    if (!ConnectPlain(error)) {
      return false;
    }

    SCHANNEL_CRED sch{};
    sch.dwVersion = SCHANNEL_CRED_VERSION;
    sch.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;

    TimeStamp expiry{};
    SECURITY_STATUS st =
        AcquireCredentialsHandleW(nullptr, const_cast<wchar_t*>(UNISP_NAME_W),
                                  SECPKG_CRED_OUTBOUND, nullptr, &sch, nullptr,
                                  nullptr, &cred.cred, &expiry);
    if (st != SEC_E_OK) {
      error = "AcquireCredentialsHandle failed";
      Close();
      return false;
    }
    cred.has = true;

    std::vector<std::uint8_t> in_buf;
    std::vector<std::uint8_t> extra;
    DWORD ctx_attr = 0;

    constexpr DWORD req_flags = ISC_REQ_SEQUENCE_DETECT |
                                ISC_REQ_REPLAY_DETECT |
                                ISC_REQ_CONFIDENTIALITY |
                                ISC_RET_EXTENDED_ERROR |
                                ISC_REQ_ALLOCATE_MEMORY |
                                ISC_REQ_STREAM;

    const std::wstring target = ToWide(host);
    bool have_ctx = false;
    while (true) {
      SecBuffer out_buffers[1];
      out_buffers[0].pvBuffer = nullptr;
      out_buffers[0].cbBuffer = 0;
      out_buffers[0].BufferType = SECBUFFER_TOKEN;

      SecBufferDesc out_desc{};
      out_desc.ulVersion = SECBUFFER_VERSION;
      out_desc.cBuffers = 1;
      out_desc.pBuffers = out_buffers;

      SecBuffer in_buffers[2];
      SecBufferDesc in_desc{};
      SecBufferDesc* in_desc_ptr = nullptr;
      if (!in_buf.empty()) {
        in_buffers[0].pvBuffer = in_buf.data();
        in_buffers[0].cbBuffer = static_cast<unsigned long>(in_buf.size());
        in_buffers[0].BufferType = SECBUFFER_TOKEN;
        in_buffers[1].pvBuffer = nullptr;
        in_buffers[1].cbBuffer = 0;
        in_buffers[1].BufferType = SECBUFFER_EMPTY;
        in_desc.ulVersion = SECBUFFER_VERSION;
        in_desc.cBuffers = 2;
        in_desc.pBuffers = in_buffers;
        in_desc_ptr = &in_desc;
      }

      st = InitializeSecurityContextW(
          &cred.cred, have_ctx ? &ctx.ctx : nullptr,
          target.empty() ? nullptr : const_cast<wchar_t*>(target.c_str()),
          req_flags, 0, SECURITY_NATIVE_DREP, in_desc_ptr, 0, &ctx.ctx,
          &out_desc, &ctx_attr, &expiry);
      have_ctx = true;
      ctx.has = true;

      if (st == SEC_I_COMPLETE_NEEDED || st == SEC_I_COMPLETE_AND_CONTINUE) {
        CompleteAuthToken(&ctx.ctx, &out_desc);
        st = (st == SEC_I_COMPLETE_NEEDED) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
      }

      if (out_buffers[0].pvBuffer && out_buffers[0].cbBuffer > 0) {
        const auto* p =
            reinterpret_cast<const std::uint8_t*>(out_buffers[0].pvBuffer);
        const std::size_t n = out_buffers[0].cbBuffer;
        const bool ok = SendAll(sock, p, n);
        FreeContextBuffer(out_buffers[0].pvBuffer);
        out_buffers[0].pvBuffer = nullptr;
        if (!ok) {
          error = "tls send handshake failed";
          Close();
          return false;
        }
      }

      if (st == SEC_E_OK) {
        if (!in_buf.empty() && in_desc_ptr &&
            in_buffers[1].BufferType == SECBUFFER_EXTRA &&
            in_buffers[1].cbBuffer > 0) {
          const std::size_t extra_len = in_buffers[1].cbBuffer;
          extra.assign(in_buf.end() - extra_len, in_buf.end());
        }
        break;
      }

      if (st == SEC_E_INCOMPLETE_MESSAGE) {
        if (!RecvSome(sock, in_buf)) {
          error = "tls handshake recv failed";
          Close();
          return false;
        }
        continue;
      }

      if (st == SEC_I_CONTINUE_NEEDED) {
        if (!in_buf.empty() && in_desc_ptr &&
            in_buffers[1].BufferType == SECBUFFER_EXTRA &&
            in_buffers[1].cbBuffer > 0) {
          const std::size_t extra_len = in_buffers[1].cbBuffer;
          std::vector<std::uint8_t> keep(in_buf.end() - extra_len, in_buf.end());
          in_buf.swap(keep);
        } else {
          in_buf.clear();
        }
        if (in_buf.empty()) {
          if (!RecvSome(sock, in_buf)) {
            error = "tls handshake recv failed";
            Close();
            return false;
          }
        }
        continue;
      }

      error = "tls handshake failed";
      Close();
      return false;
    }

    st = QueryContextAttributes(&ctx.ctx, SECPKG_ATTR_STREAM_SIZES, &sizes);
    if (st != SEC_E_OK) {
      error = "QueryContextAttributes failed";
      Close();
      return false;
    }

    ScopedCertContext remote_cert;
    st = QueryContextAttributes(&ctx.ctx, SECPKG_ATTR_REMOTE_CERT_CONTEXT,
                                &remote_cert.cert);
    if (st != SEC_E_OK || !remote_cert.cert) {
      error = "remote cert unavailable";
      Close();
      return false;
    }

    out_server_fingerprint = Sha256Hex(remote_cert.cert->pbCertEncoded,
                                       remote_cert.cert->cbCertEncoded);
    if (out_server_fingerprint.empty()) {
      error = "cert fingerprint failed";
      Close();
      return false;
    }
    if (pinned_fingerprint.empty()) {
      error = "server not trusted";
      Close();
      return false;
    }
    if (pinned_fingerprint != out_server_fingerprint) {
      error = "server fingerprint changed";
      Close();
      return false;
    }

    enc_buf = std::move(extra);
    return true;
  }
#endif

  bool Connect(std::string& out_server_fingerprint, std::string& error) {
    out_server_fingerprint.clear();
    if (use_kcp) {
      return ConnectKcp(error);
    }
#ifdef _WIN32
    if (use_tls) {
      return ConnectTls(out_server_fingerprint, error);
    }
#endif
    return ConnectPlain(error);
  }

  bool SendAndRecv(const std::vector<std::uint8_t>& in_bytes,
                   std::vector<std::uint8_t>& out_bytes,
                   std::string& error) {
    out_bytes.clear();
    error.clear();
    if (use_kcp) {
      const auto now = std::chrono::steady_clock::now();
      if (kcp_cfg.session_idle_sec > 0 &&
          kcp_last_active.time_since_epoch().count() != 0) {
        const auto idle = std::chrono::duration_cast<std::chrono::seconds>(
            now - kcp_last_active);
        if (idle.count() > static_cast<long long>(kcp_cfg.session_idle_sec)) {
          error = "kcp idle timeout";
          return false;
        }
      }

      if (!kcp || sock
#ifdef _WIN32
              == INVALID_SOCKET
#else
              < 0
#endif
      ) {
        error = "not connected";
        return false;
      }
      if (in_bytes.empty()) {
        error = "empty request";
        return false;
      }

      if (ikcp_send(kcp, reinterpret_cast<const char*>(in_bytes.data()),
                    static_cast<int>(in_bytes.size())) < 0) {
        error = "kcp send failed";
        return false;
      }
      ikcp_flush(kcp);
      kcp_last_active = now;

      const std::uint32_t start_ms = NowMs();
      const std::uint32_t timeout_ms =
          kcp_cfg.request_timeout_ms == 0 ? 5000u
                                          : kcp_cfg.request_timeout_ms;
      if (kcp_recv_buf.empty()) {
        kcp_recv_buf.resize(std::max<std::uint32_t>(kcp_cfg.mtu, 1200u) + 256u);
      }
      auto& datagram = kcp_recv_buf;

      while (true) {
        const std::uint32_t now_ms = NowMs();
        if (now_ms - start_ms >= timeout_ms) {
          error = "kcp timeout";
          return false;
        }

        while (true) {
#ifdef _WIN32
          const int n = ::recv(sock, reinterpret_cast<char*>(datagram.data()),
                               static_cast<int>(datagram.size()), 0);
          if (n == SOCKET_ERROR) {
            if (SocketWouldBlock()) {
              break;
            }
            error = "kcp recv failed";
            return false;
          }
#else
          const ssize_t n = ::recv(sock, datagram.data(), datagram.size(), 0);
          if (n < 0) {
            if (SocketWouldBlock()) {
              break;
            }
            error = "kcp recv failed";
            return false;
          }
#endif
          if (n > 0) {
            ikcp_input(kcp, reinterpret_cast<const char*>(datagram.data()), n);
            kcp_last_active = std::chrono::steady_clock::now();
          } else {
            break;
          }
        }

        const int peek = ikcp_peeksize(kcp);
        if (peek > 0) {
          out_bytes.resize(static_cast<std::size_t>(peek));
          const int n = ikcp_recv(
              kcp, reinterpret_cast<char*>(out_bytes.data()), peek);
          if (n > 0) {
            out_bytes.resize(static_cast<std::size_t>(n));
            return true;
          }
          out_bytes.clear();
        }

        const std::uint32_t check = ikcp_check(kcp, now_ms);
        const std::uint32_t wait_ms =
            check > now_ms ? (check - now_ms) : 1u;
        const std::uint32_t remaining = timeout_ms - (now_ms - start_ms);
        const std::uint32_t sleep_ms = std::min(wait_ms, remaining);
        WaitForReadable(sock, sleep_ms);
        ikcp_update(kcp, NowMs());
      }
    }
#ifdef _WIN32
    if (use_tls) {
      if (sock == INVALID_SOCKET) {
        error = "not connected";
        return false;
      }
      if (!SchannelEncryptSend(sock, ctx, sizes, in_bytes)) {
        error = "tls send failed";
        return false;
      }
      if (!SchannelReadFrameBuffered(sock, ctx, enc_buf, plain_buf, plain_off,
                                     out_bytes)) {
        error = "tls recv failed";
        return false;
      }
      return !out_bytes.empty();
    }
#endif

#ifdef _WIN32
    if (sock == INVALID_SOCKET) {
      error = "not connected";
      return false;
    }
#else
    if (sock < 0) {
      error = "not connected";
      return false;
    }
#endif

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
                    static_cast<std::size_t>((std::numeric_limits<int>::max)())
                ? (std::numeric_limits<int>::max)()
                : static_cast<int>(remaining);
#ifdef _WIN32
        const int n = ::send(sock,
                             reinterpret_cast<const char*>(data + sent), chunk,
                             0);
#else
        const ssize_t n = ::send(sock, data + sent,
                                 static_cast<std::size_t>(chunk), 0);
#endif
        if (n <= 0) {
          return false;
        }
        sent += static_cast<std::size_t>(n);
      }
      return true;
    };

    const auto recv_exact = [&](std::uint8_t* data,
                                std::size_t len) -> bool {
      if (!data || len == 0) {
        return true;
      }
      std::size_t got = 0;
      while (got < len) {
        const std::size_t remaining = len - got;
        const int want =
            remaining >
                    static_cast<std::size_t>((std::numeric_limits<int>::max)())
                ? (std::numeric_limits<int>::max)()
                : static_cast<int>(remaining);
#ifdef _WIN32
        const int n =
            ::recv(sock, reinterpret_cast<char*>(data + got), want, 0);
#else
        const ssize_t n =
            ::recv(sock, data + got, static_cast<std::size_t>(want), 0);
#endif
        if (n <= 0) {
          return false;
        }
        got += static_cast<std::size_t>(n);
      }
      return true;
    };

    if (!send_all(in_bytes.data(), in_bytes.size())) {
      error = "tcp send failed";
      return false;
    }

    std::uint8_t header[mi::server::kFrameHeaderSize] = {};
    if (!recv_exact(header, sizeof(header))) {
      error = "tcp recv failed";
      return false;
    }
    mi::server::FrameType type;
    std::uint32_t payload_len = 0;
    if (!mi::server::DecodeFrameHeader(header, sizeof(header), type,
                                       payload_len)) {
      error = "tcp recv failed";
      return false;
    }
    (void)type;
    out_bytes.resize(mi::server::kFrameHeaderSize + payload_len);
    std::memcpy(out_bytes.data(), header, sizeof(header));
    if (payload_len > 0 &&
        !recv_exact(out_bytes.data() + mi::server::kFrameHeaderSize,
                    payload_len)) {
      error = "tcp recv failed";
      out_bytes.clear();
      return false;
    }
    return true;
  }
};
#endif  // 0

std::string FingerprintSas80Hex(const std::string& sha256_hex) {
  std::vector<std::uint8_t> fp_bytes;
  if (!HexToBytes(sha256_hex, fp_bytes) || fp_bytes.size() != 32) {
    return {};
  }

  std::vector<std::uint8_t> msg;
  static constexpr char kPrefix[] = "MI_SERVER_CERT_SAS_V1";
  msg.insert(msg.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
  msg.insert(msg.end(), fp_bytes.begin(), fp_bytes.end());

  const std::string h = Sha256Hex(msg.data(), msg.size());
  if (h.size() < 20) {
    return {};
  }
  return GroupHex4(h.substr(0, 20));
}

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
                  const ProxyConfig& proxy,
                  std::vector<std::uint8_t>& out_bytes,
                  std::string& error) {
  out_bytes.clear();
  error.clear();
  if (host.empty() || port == 0 || in_bytes.empty()) {
    error = "invalid request";
    return false;
  }

  const bool use_proxy = proxy.enabled();
  if (use_proxy && proxy.type != ProxyType::kSocks5) {
    error = "unsupported proxy";
    return false;
  }
  const std::string connect_host = use_proxy ? proxy.host : host;
  const std::uint16_t connect_port = use_proxy ? proxy.port : port;

#ifdef _WIN32
  if (!EnsureWinsock()) {
    error = "winsock init failed";
    return false;
  }
#endif

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* result = nullptr;
  const std::string port_str = std::to_string(connect_port);
  if (getaddrinfo(connect_host.c_str(), port_str.c_str(), &hints, &result) != 0) {
    error = "dns resolve failed";
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
    error = "connect failed";
    return false;
  }
#else
  if (sock < 0) {
    error = "connect failed";
    return false;
  }
#endif

  if (use_proxy) {
    if (proxy.username.size() > 255 || proxy.password.size() > 255) {
#ifdef _WIN32
      closesocket(sock);
#else
      ::close(sock);
#endif
      error = "proxy auth too long";
      return false;
    }

    const bool need_auth =
        !proxy.username.empty() || !proxy.password.empty();
    std::vector<std::uint8_t> hello;
    hello.reserve(4);
    hello.push_back(0x05);
    if (need_auth) {
      hello.push_back(0x02);
      hello.push_back(0x00);
      hello.push_back(0x02);
    } else {
      hello.push_back(0x01);
      hello.push_back(0x00);
    }

    auto send_all = [&](const std::uint8_t* data, std::size_t len) -> bool {
      std::size_t sent2 = 0;
      while (sent2 < len) {
        const std::size_t remaining = len - sent2;
        const int chunk =
            remaining >
                    static_cast<std::size_t>((std::numeric_limits<int>::max)())
                ? (std::numeric_limits<int>::max)()
                : static_cast<int>(remaining);
#ifdef _WIN32
        const int n =
            ::send(sock, reinterpret_cast<const char*>(data + sent2), chunk, 0);
#else
        const ssize_t n =
            ::send(sock, data + sent2, static_cast<std::size_t>(chunk), 0);
#endif
        if (n <= 0) {
          return false;
        }
        sent2 += static_cast<std::size_t>(n);
      }
      return true;
    };

    auto recv_exact = [&](std::uint8_t* data, std::size_t len) -> bool {
      std::size_t got = 0;
      while (got < len) {
#ifdef _WIN32
        const int n =
            ::recv(sock, reinterpret_cast<char*>(data + got),
                   static_cast<int>(len - got), 0);
#else
        const ssize_t n = ::recv(sock, data + got, len - got, 0);
#endif
        if (n <= 0) {
          return false;
        }
        got += static_cast<std::size_t>(n);
      }
      return true;
    };

    std::uint8_t sel[2] = {0, 0};
    if (!send_all(hello.data(), hello.size()) || !recv_exact(sel, sizeof(sel))) {
#ifdef _WIN32
      closesocket(sock);
#else
      ::close(sock);
#endif
      error = "proxy handshake failed";
      return false;
    }
    if (sel[0] != 0x05 || sel[1] == 0xFF) {
#ifdef _WIN32
      closesocket(sock);
#else
      ::close(sock);
#endif
      error = "proxy handshake failed";
      return false;
    }
    if (sel[1] == 0x02) {
      std::vector<std::uint8_t> auth;
      auth.reserve(3 + proxy.username.size() + proxy.password.size());
      auth.push_back(0x01);
      auth.push_back(static_cast<std::uint8_t>(proxy.username.size()));
      auth.insert(auth.end(), proxy.username.begin(), proxy.username.end());
      auth.push_back(static_cast<std::uint8_t>(proxy.password.size()));
      auth.insert(auth.end(), proxy.password.begin(), proxy.password.end());

      std::uint8_t ar[2] = {0, 0};
      if (!send_all(auth.data(), auth.size()) || !recv_exact(ar, sizeof(ar)) ||
          ar[0] != 0x01 || ar[1] != 0x00) {
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        error = "proxy auth failed";
        return false;
      }
    } else if (sel[1] != 0x00) {
#ifdef _WIN32
      closesocket(sock);
#else
      ::close(sock);
#endif
      error = "proxy method unsupported";
      return false;
    }

    if (host.size() > 255) {
#ifdef _WIN32
      closesocket(sock);
#else
      ::close(sock);
#endif
      error = "target host too long";
      return false;
    }
    std::vector<std::uint8_t> req;
    req.reserve(7 + host.size());
    req.push_back(0x05);
    req.push_back(0x01);
    req.push_back(0x00);
    req.push_back(0x03);
    req.push_back(static_cast<std::uint8_t>(host.size()));
    req.insert(req.end(), host.begin(), host.end());
    req.push_back(static_cast<std::uint8_t>((port >> 8) & 0xFF));
    req.push_back(static_cast<std::uint8_t>(port & 0xFF));

    std::uint8_t rep[4] = {0, 0, 0, 0};
    if (!send_all(req.data(), req.size()) || !recv_exact(rep, sizeof(rep)) ||
        rep[0] != 0x05 || rep[1] != 0x00) {
#ifdef _WIN32
      closesocket(sock);
#else
      ::close(sock);
#endif
      error = "proxy connect failed";
      return false;
    }

    std::size_t to_read = 0;
    if (rep[3] == 0x01) {
      to_read = 4 + 2;
    } else if (rep[3] == 0x03) {
      std::uint8_t len = 0;
      if (!recv_exact(&len, 1)) {
#ifdef _WIN32
        closesocket(sock);
#else
        ::close(sock);
#endif
        error = "proxy connect failed";
        return false;
      }
      to_read = static_cast<std::size_t>(len) + 2;
    } else if (rep[3] == 0x04) {
      to_read = 16 + 2;
    } else {
#ifdef _WIN32
      closesocket(sock);
#else
      ::close(sock);
#endif
      error = "proxy connect failed";
      return false;
    }
    std::vector<std::uint8_t> discard;
    discard.resize(to_read);
    if (!recv_exact(discard.data(), discard.size())) {
#ifdef _WIN32
      closesocket(sock);
#else
      ::close(sock);
#endif
      error = "proxy connect failed";
      return false;
    }
  }

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
      error = "send failed";
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
  bool have_frame_header = false;
  std::size_t expected_total = 0;
  while (true) {
#ifdef _WIN32
    const int n = ::recv(sock, reinterpret_cast<char*>(tmp),
                         static_cast<int>(sizeof(tmp)), 0);
#else
    const ssize_t n = ::recv(sock, tmp, sizeof(tmp), 0);
#endif
    if (n > 0) {
      buf.insert(buf.end(), tmp, tmp + n);
      if (!have_frame_header && buf.size() >= mi::server::kFrameHeaderSize) {
        mi::server::FrameType type;
        std::uint32_t payload_len = 0;
        if (!mi::server::DecodeFrameHeader(buf.data(), buf.size(), type,
                                           payload_len)) {
#ifdef _WIN32
          closesocket(sock);
#else
          ::close(sock);
#endif
          error = "invalid response";
          return false;
        }
        (void)type;
        have_frame_header = true;
        expected_total =
            mi::server::kFrameHeaderSize + static_cast<std::size_t>(payload_len);
        if (buf.capacity() < expected_total) {
          buf.reserve(expected_total);
        }
      }
      if (have_frame_header && buf.size() >= expected_total) {
        buf.resize(expected_total);
        break;
      }
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
    error = "recv failed";
    return false;
  }

#ifdef _WIN32
  closesocket(sock);
#else
  ::close(sock);
#endif

  out_bytes = std::move(buf);
  if (!have_frame_header || out_bytes.size() != expected_total) {
    error = "truncated response";
    return false;
  }
  if (out_bytes.empty()) {
    error = "empty response";
    return false;
  }
  return true;
}

#ifdef _WIN32
namespace {

constexpr std::size_t kFrameHeaderSize = 12;

struct ScopedCredHandle {
  CredHandle cred{};
  bool has{false};
  ~ScopedCredHandle() {
    if (has) {
      FreeCredentialsHandle(&cred);
      has = false;
    }
  }
  ScopedCredHandle() = default;
  ScopedCredHandle(const ScopedCredHandle&) = delete;
  ScopedCredHandle& operator=(const ScopedCredHandle&) = delete;
};

struct ScopedCtxtHandle {
  CtxtHandle ctx{};
  bool has{false};
  ~ScopedCtxtHandle() {
    if (has) {
      DeleteSecurityContext(&ctx);
      has = false;
    }
  }
  ScopedCtxtHandle() = default;
  ScopedCtxtHandle(const ScopedCtxtHandle&) = delete;
  ScopedCtxtHandle& operator=(const ScopedCtxtHandle&) = delete;
};

struct ScopedCertContext {
  PCCERT_CONTEXT cert{nullptr};
  ~ScopedCertContext() {
    if (cert) {
      CertFreeCertificateContext(cert);
      cert = nullptr;
    }
  }
  ScopedCertContext() = default;
  ScopedCertContext(const ScopedCertContext&) = delete;
  ScopedCertContext& operator=(const ScopedCertContext&) = delete;
};

bool SendAll(SOCKET sock, const std::uint8_t* data, std::size_t len) {
  if (!data || len == 0) {
    return true;
  }
  std::size_t sent = 0;
  while (sent < len) {
    const std::size_t remaining = len - sent;
    const int chunk =
        remaining >
                static_cast<std::size_t>((std::numeric_limits<int>::max)())
            ? (std::numeric_limits<int>::max)()
            : static_cast<int>(remaining);
    const int n = ::send(sock, reinterpret_cast<const char*>(data + sent),
                         chunk, 0);
    if (n <= 0) {
      return false;
    }
    sent += static_cast<std::size_t>(n);
  }
  return true;
}

bool RecvSome(SOCKET sock, std::vector<std::uint8_t>& out) {
  std::uint8_t tmp[4096];
  const int n =
      ::recv(sock, reinterpret_cast<char*>(tmp), static_cast<int>(sizeof(tmp)),
             0);
  if (n <= 0) {
    return false;
  }
  out.insert(out.end(), tmp, tmp + n);
  return true;
}

std::wstring ToWide(const std::string& s) {
  if (s.empty()) {
    return {};
  }
  const int needed =
      MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                          nullptr, 0);
  if (needed <= 0) {
    return {};
  }
  std::wstring out;
  out.resize(static_cast<std::size_t>(needed));
  MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                      out.data(), needed);
  return out;
}

bool SchannelEncryptSend(SOCKET sock, ScopedCtxtHandle& ctx,
                         const SecPkgContext_StreamSizes& sizes,
                         const std::vector<std::uint8_t>& plain) {
  std::size_t sent = 0;
  while (sent < plain.size()) {
    const std::size_t chunk =
        std::min<std::size_t>(plain.size() - sent, sizes.cbMaximumMessage);
    std::vector<std::uint8_t> buf;
    buf.resize(sizes.cbHeader + chunk + sizes.cbTrailer);
    std::memcpy(buf.data() + sizes.cbHeader, plain.data() + sent, chunk);

    SecBuffer buffers[4];
    buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
    buffers[0].pvBuffer = buf.data();
    buffers[0].cbBuffer = sizes.cbHeader;
    buffers[1].BufferType = SECBUFFER_DATA;
    buffers[1].pvBuffer = buf.data() + sizes.cbHeader;
    buffers[1].cbBuffer = static_cast<unsigned long>(chunk);
    buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
    buffers[2].pvBuffer = buf.data() + sizes.cbHeader + chunk;
    buffers[2].cbBuffer = sizes.cbTrailer;
    buffers[3].BufferType = SECBUFFER_EMPTY;
    buffers[3].pvBuffer = nullptr;
    buffers[3].cbBuffer = 0;

    SecBufferDesc desc{};
    desc.ulVersion = SECBUFFER_VERSION;
    desc.cBuffers = 4;
    desc.pBuffers = buffers;

    const SECURITY_STATUS st = EncryptMessage(&ctx.ctx, 0, &desc, 0);
    if (st != SEC_E_OK) {
      return false;
    }

    const std::size_t total =
        static_cast<std::size_t>(buffers[0].cbBuffer) +
        static_cast<std::size_t>(buffers[1].cbBuffer) +
        static_cast<std::size_t>(buffers[2].cbBuffer);
    if (!SendAll(sock, buf.data(), total)) {
      return false;
    }
    sent += chunk;
  }
  return true;
}

bool SchannelDecryptToPlain(SOCKET sock, ScopedCtxtHandle& ctx,
                            std::vector<std::uint8_t>& enc_buf,
                            std::vector<std::uint8_t>& plain_out) {
  plain_out.clear();
  while (true) {
    if (enc_buf.empty()) {
      if (!RecvSome(sock, enc_buf)) {
        return false;
      }
    }

    SecBuffer buffers[4];
    buffers[0].BufferType = SECBUFFER_DATA;
    buffers[0].pvBuffer = enc_buf.data();
    buffers[0].cbBuffer = static_cast<unsigned long>(enc_buf.size());
    buffers[1].BufferType = SECBUFFER_EMPTY;
    buffers[1].pvBuffer = nullptr;
    buffers[1].cbBuffer = 0;
    buffers[2].BufferType = SECBUFFER_EMPTY;
    buffers[2].pvBuffer = nullptr;
    buffers[2].cbBuffer = 0;
    buffers[3].BufferType = SECBUFFER_EMPTY;
    buffers[3].pvBuffer = nullptr;
    buffers[3].cbBuffer = 0;

    SecBufferDesc desc{};
    desc.ulVersion = SECBUFFER_VERSION;
    desc.cBuffers = 4;
    desc.pBuffers = buffers;

    const SECURITY_STATUS st = DecryptMessage(&ctx.ctx, &desc, 0, nullptr);
    if (st == SEC_E_INCOMPLETE_MESSAGE) {
      if (!RecvSome(sock, enc_buf)) {
        return false;
      }
      continue;
    }
    if (st == SEC_I_CONTEXT_EXPIRED) {
      enc_buf.clear();
      return false;
    }
    if (st != SEC_E_OK) {
      return false;
    }

    for (auto& b : buffers) {
      if (b.BufferType == SECBUFFER_DATA && b.pvBuffer && b.cbBuffer > 0) {
        const auto* p = reinterpret_cast<const std::uint8_t*>(b.pvBuffer);
        plain_out.insert(plain_out.end(), p, p + b.cbBuffer);
      }
    }

    for (auto& b : buffers) {
      if (b.BufferType == SECBUFFER_EXTRA && b.cbBuffer > 0) {
        const std::size_t extra = b.cbBuffer;
        std::vector<std::uint8_t> keep(enc_buf.end() - extra, enc_buf.end());
        enc_buf.swap(keep);
        return true;
      }
    }

    enc_buf.clear();
    return true;
  }
}

bool SchannelReadFrame(SOCKET sock, ScopedCtxtHandle& ctx,
                       std::vector<std::uint8_t> enc_buf,
                       std::vector<std::uint8_t>& out_frame) {
  out_frame.clear();
  std::vector<std::uint8_t> plain_buf;
  while (true) {
    std::vector<std::uint8_t> plain_chunk;
    if (!SchannelDecryptToPlain(sock, ctx, enc_buf, plain_chunk)) {
      return false;
    }
    if (!plain_chunk.empty()) {
      plain_buf.insert(plain_buf.end(), plain_chunk.begin(), plain_chunk.end());
    }

    if (plain_buf.size() < kFrameHeaderSize) {
      continue;
    }
    mi::server::FrameType type;
    std::uint32_t payload_len = 0;
    if (!mi::server::DecodeFrameHeader(plain_buf.data(), plain_buf.size(), type,
                                       payload_len)) {
      return false;
    }
    (void)type;
    const std::size_t total = kFrameHeaderSize + payload_len;
    if (plain_buf.size() < total) {
      continue;
    }
    out_frame.assign(plain_buf.begin(), plain_buf.begin() + total);
    return true;
  }
}

bool SchannelReadFrameBuffered(SOCKET sock, ScopedCtxtHandle& ctx,
                               std::vector<std::uint8_t>& enc_buf,
                               std::vector<std::uint8_t>& plain_buf,
                               std::size_t& plain_off,
                               std::vector<std::uint8_t>& out_frame) {
  out_frame.clear();
  if (plain_off > plain_buf.size()) {
    plain_buf.clear();
    plain_off = 0;
  }

  while (true) {
    const std::size_t avail =
        plain_buf.size() >= plain_off ? (plain_buf.size() - plain_off) : 0;
    if (avail >= kFrameHeaderSize) {
      mi::server::FrameType type;
      std::uint32_t payload_len = 0;
      if (!mi::server::DecodeFrameHeader(plain_buf.data() + plain_off, avail,
                                         type, payload_len)) {
        return false;
      }
      (void)type;
      const std::size_t total = kFrameHeaderSize + payload_len;
      if (avail >= total) {
        out_frame.assign(
            plain_buf.begin() + static_cast<std::ptrdiff_t>(plain_off),
            plain_buf.begin() + static_cast<std::ptrdiff_t>(plain_off + total));
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

    std::vector<std::uint8_t> plain_chunk;
    if (!SchannelDecryptToPlain(sock, ctx, enc_buf, plain_chunk)) {
      return false;
    }
    if (!plain_chunk.empty()) {
      plain_buf.insert(plain_buf.end(), plain_chunk.begin(), plain_chunk.end());
    }
  }
}

bool TlsRoundTripSChannel(const std::string& host, std::uint16_t port,
                          const std::vector<std::uint8_t>& in_bytes,
                          const ProxyConfig& proxy,
                          const std::string& pinned_fingerprint,
                          std::string& out_server_fingerprint,
                          std::vector<std::uint8_t>& out_bytes,
                          std::string& error) {
  out_bytes.clear();
  out_server_fingerprint.clear();
  error.clear();
  if (host.empty() || port == 0 || in_bytes.empty()) {
    error = "invalid tls request";
    return false;
  }

  if (!EnsureWinsock()) {
    error = "winsock init failed";
    return false;
  }

  const bool use_proxy = proxy.enabled();
  if (use_proxy && proxy.type != ProxyType::kSocks5) {
    error = "unsupported proxy";
    return false;
  }
  const std::string connect_host = use_proxy ? proxy.host : host;
  const std::uint16_t connect_port = use_proxy ? proxy.port : port;

  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo* result = nullptr;
  const std::string port_str = std::to_string(connect_port);
  if (getaddrinfo(connect_host.c_str(), port_str.c_str(), &hints, &result) != 0) {
    error = "dns resolve failed";
    return false;
  }

  SOCKET sock = INVALID_SOCKET;
  for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
    sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sock == INVALID_SOCKET) {
      continue;
    }
    if (::connect(sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) == 0) {
      break;
    }
    closesocket(sock);
    sock = INVALID_SOCKET;
  }
  freeaddrinfo(result);
  if (sock == INVALID_SOCKET) {
    error = "connect failed";
    return false;
  }

  if (use_proxy) {
    auto recv_exact = [&](std::uint8_t* data, std::size_t len) -> bool {
      std::size_t got = 0;
      while (got < len) {
        const int n =
            ::recv(sock, reinterpret_cast<char*>(data + got),
                   static_cast<int>(len - got), 0);
        if (n <= 0) {
          return false;
        }
        got += static_cast<std::size_t>(n);
      }
      return true;
    };

    if (proxy.username.size() > 255 || proxy.password.size() > 255) {
      closesocket(sock);
      error = "proxy auth too long";
      return false;
    }
    const bool need_auth =
        !proxy.username.empty() || !proxy.password.empty();
    std::vector<std::uint8_t> hello;
    hello.reserve(4);
    hello.push_back(0x05);
    if (need_auth) {
      hello.push_back(0x02);
      hello.push_back(0x00);
      hello.push_back(0x02);
    } else {
      hello.push_back(0x01);
      hello.push_back(0x00);
    }
    std::uint8_t sel[2] = {0, 0};
    if (!SendAll(sock, hello.data(), hello.size()) ||
        !recv_exact(sel, sizeof(sel)) || sel[0] != 0x05 || sel[1] == 0xFF) {
      closesocket(sock);
      error = "proxy handshake failed";
      return false;
    }
    if (sel[1] == 0x02) {
      std::vector<std::uint8_t> auth;
      auth.reserve(3 + proxy.username.size() + proxy.password.size());
      auth.push_back(0x01);
      auth.push_back(static_cast<std::uint8_t>(proxy.username.size()));
      auth.insert(auth.end(), proxy.username.begin(), proxy.username.end());
      auth.push_back(static_cast<std::uint8_t>(proxy.password.size()));
      auth.insert(auth.end(), proxy.password.begin(), proxy.password.end());

      std::uint8_t ar[2] = {0, 0};
      if (!SendAll(sock, auth.data(), auth.size()) ||
          !recv_exact(ar, sizeof(ar)) || ar[0] != 0x01 || ar[1] != 0x00) {
        closesocket(sock);
        error = "proxy auth failed";
        return false;
      }
    } else if (sel[1] != 0x00) {
      closesocket(sock);
      error = "proxy method unsupported";
      return false;
    }

    if (host.size() > 255) {
      closesocket(sock);
      error = "target host too long";
      return false;
    }
    std::vector<std::uint8_t> req;
    req.reserve(7 + host.size());
    req.push_back(0x05);
    req.push_back(0x01);
    req.push_back(0x00);
    req.push_back(0x03);
    req.push_back(static_cast<std::uint8_t>(host.size()));
    req.insert(req.end(), host.begin(), host.end());
    req.push_back(static_cast<std::uint8_t>((port >> 8) & 0xFF));
    req.push_back(static_cast<std::uint8_t>(port & 0xFF));

    std::uint8_t rep[4] = {0, 0, 0, 0};
    if (!SendAll(sock, req.data(), req.size()) ||
        !recv_exact(rep, sizeof(rep)) || rep[0] != 0x05 || rep[1] != 0x00) {
      closesocket(sock);
      error = "proxy connect failed";
      return false;
    }

    std::size_t to_read = 0;
    if (rep[3] == 0x01) {
      to_read = 4 + 2;
    } else if (rep[3] == 0x03) {
      std::uint8_t len = 0;
      if (!recv_exact(&len, 1)) {
        closesocket(sock);
        error = "proxy connect failed";
        return false;
      }
      to_read = static_cast<std::size_t>(len) + 2;
    } else if (rep[3] == 0x04) {
      to_read = 16 + 2;
    } else {
      closesocket(sock);
      error = "proxy connect failed";
      return false;
    }
    std::vector<std::uint8_t> discard;
    discard.resize(to_read);
    if (!recv_exact(discard.data(), discard.size())) {
      closesocket(sock);
      error = "proxy connect failed";
      return false;
    }
  }

  ScopedCredHandle cred;
  SCHANNEL_CRED sch{};
  sch.dwVersion = SCHANNEL_CRED_VERSION;
  sch.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;

  TimeStamp expiry{};
  SECURITY_STATUS st =
      AcquireCredentialsHandleW(nullptr, const_cast<wchar_t*>(UNISP_NAME_W),
                                SECPKG_CRED_OUTBOUND, nullptr, &sch, nullptr,
                                nullptr, &cred.cred, &expiry);
  if (st != SEC_E_OK) {
    closesocket(sock);
    error = "AcquireCredentialsHandle failed";
    return false;
  }
  cred.has = true;

  ScopedCtxtHandle ctx;
  std::vector<std::uint8_t> in_buf;
  std::vector<std::uint8_t> extra;
  DWORD ctx_attr = 0;

  constexpr DWORD req_flags = ISC_REQ_SEQUENCE_DETECT |
                              ISC_REQ_REPLAY_DETECT |
                              ISC_REQ_CONFIDENTIALITY |
                              ISC_RET_EXTENDED_ERROR |
                              ISC_REQ_ALLOCATE_MEMORY |
                              ISC_REQ_STREAM;

  const std::wstring target = ToWide(host);
  bool have_ctx = false;
  while (true) {
    SecBuffer out_buffers[1];
    out_buffers[0].pvBuffer = nullptr;
    out_buffers[0].cbBuffer = 0;
    out_buffers[0].BufferType = SECBUFFER_TOKEN;

    SecBufferDesc out_desc{};
    out_desc.ulVersion = SECBUFFER_VERSION;
    out_desc.cBuffers = 1;
    out_desc.pBuffers = out_buffers;

    SecBuffer in_buffers[2];
    SecBufferDesc in_desc{};
    SecBufferDesc* in_desc_ptr = nullptr;
    if (!in_buf.empty()) {
      in_buffers[0].pvBuffer = in_buf.data();
      in_buffers[0].cbBuffer = static_cast<unsigned long>(in_buf.size());
      in_buffers[0].BufferType = SECBUFFER_TOKEN;
      in_buffers[1].pvBuffer = nullptr;
      in_buffers[1].cbBuffer = 0;
      in_buffers[1].BufferType = SECBUFFER_EMPTY;
      in_desc.ulVersion = SECBUFFER_VERSION;
      in_desc.cBuffers = 2;
      in_desc.pBuffers = in_buffers;
      in_desc_ptr = &in_desc;
    }

    st = InitializeSecurityContextW(
        &cred.cred, have_ctx ? &ctx.ctx : nullptr,
        target.empty() ? nullptr : const_cast<wchar_t*>(target.c_str()),
        req_flags, 0, SECURITY_NATIVE_DREP, in_desc_ptr, 0, &ctx.ctx, &out_desc,
        &ctx_attr, &expiry);
    have_ctx = true;
    ctx.has = true;

    if (st == SEC_I_COMPLETE_NEEDED || st == SEC_I_COMPLETE_AND_CONTINUE) {
      CompleteAuthToken(&ctx.ctx, &out_desc);
      st = (st == SEC_I_COMPLETE_NEEDED) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
    }

    if (out_buffers[0].pvBuffer && out_buffers[0].cbBuffer > 0) {
      const auto* p =
          reinterpret_cast<const std::uint8_t*>(out_buffers[0].pvBuffer);
      const std::size_t n = out_buffers[0].cbBuffer;
      const bool ok = SendAll(sock, p, n);
      FreeContextBuffer(out_buffers[0].pvBuffer);
      out_buffers[0].pvBuffer = nullptr;
      if (!ok) {
        closesocket(sock);
        error = "tls send handshake failed";
        return false;
      }
    }

    if (st == SEC_E_OK) {
      if (!in_buf.empty() && in_desc_ptr &&
          in_buffers[1].BufferType == SECBUFFER_EXTRA &&
          in_buffers[1].cbBuffer > 0) {
        const std::size_t extra_len = in_buffers[1].cbBuffer;
        extra.assign(in_buf.end() - extra_len, in_buf.end());
      }
      break;
    }

    if (st == SEC_E_INCOMPLETE_MESSAGE) {
      if (!RecvSome(sock, in_buf)) {
        closesocket(sock);
        error = "tls handshake recv failed";
        return false;
      }
      continue;
    }

    if (st == SEC_I_CONTINUE_NEEDED) {
      if (!in_buf.empty() && in_desc_ptr &&
          in_buffers[1].BufferType == SECBUFFER_EXTRA &&
          in_buffers[1].cbBuffer > 0) {
        const std::size_t extra_len = in_buffers[1].cbBuffer;
        std::vector<std::uint8_t> keep(in_buf.end() - extra_len, in_buf.end());
        in_buf.swap(keep);
      } else {
        in_buf.clear();
      }
      if (in_buf.empty()) {
        if (!RecvSome(sock, in_buf)) {
          closesocket(sock);
          error = "tls handshake recv failed";
          return false;
        }
      }
      continue;
    }

    closesocket(sock);
    error = "tls handshake failed";
    return false;
  }

  SecPkgContext_StreamSizes sizes{};
  st = QueryContextAttributes(&ctx.ctx, SECPKG_ATTR_STREAM_SIZES, &sizes);
  if (st != SEC_E_OK) {
    closesocket(sock);
    error = "QueryContextAttributes failed";
    return false;
  }

  ScopedCertContext remote_cert;
  st = QueryContextAttributes(&ctx.ctx, SECPKG_ATTR_REMOTE_CERT_CONTEXT,
                              &remote_cert.cert);
  if (st != SEC_E_OK || !remote_cert.cert) {
    closesocket(sock);
    error = "remote cert unavailable";
    return false;
  }
  out_server_fingerprint =
      Sha256Hex(remote_cert.cert->pbCertEncoded, remote_cert.cert->cbCertEncoded);
  if (out_server_fingerprint.empty()) {
    closesocket(sock);
    error = "cert fingerprint failed";
    return false;
  }
  if (pinned_fingerprint.empty()) {
    closesocket(sock);
    error = "server not trusted";
    return false;
  }
  if (pinned_fingerprint != out_server_fingerprint) {
    closesocket(sock);
    error = "server fingerprint changed";
    return false;
  }

  if (!SchannelEncryptSend(sock, ctx, sizes, in_bytes)) {
    closesocket(sock);
    error = "tls send failed";
    return false;
  }
  shutdown(sock, SD_SEND);

  if (!SchannelReadFrame(sock, ctx, std::move(extra), out_bytes)) {
    closesocket(sock);
    error = "tls recv failed";
    return false;
  }

  closesocket(sock);
  return !out_bytes.empty();
}

}  // namespace
#endif  // _WIN32

}  // namespace

std::vector<std::uint8_t> ClientCore::BuildGroupCallKeyDistSigMessage(
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::array<std::uint8_t, 32>& call_key) {
  return ::mi::client::BuildGroupCallKeyDistSigMessage(group_id, call_id, key_id,
                                                       call_key);
}

bool ClientCore::EncodeGroupCallKeyDist(
    const std::array<std::uint8_t, 16>& msg_id,
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::array<std::uint8_t, 32>& call_key,
    const std::vector<std::uint8_t>& sig,
    std::vector<std::uint8_t>& out) {
  return EncodeChatGroupCallKeyDist(msg_id, group_id, call_id, key_id, call_key,
                                    sig, out);
}

bool ClientCore::DecodeGroupCallKeyDist(
    const std::vector<std::uint8_t>& payload,
    std::size_t& offset,
    std::string& out_group_id,
    std::array<std::uint8_t, 16>& out_call_id,
    std::uint32_t& out_key_id,
    std::array<std::uint8_t, 32>& out_call_key,
    std::vector<std::uint8_t>& out_sig) {
  return DecodeChatGroupCallKeyDist(payload, offset, out_group_id, out_call_id,
                                    out_key_id, out_call_key, out_sig);
}

bool ClientCore::EncodeGroupCallKeyReq(
    const std::array<std::uint8_t, 16>& msg_id,
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t want_key_id,
    std::vector<std::uint8_t>& out) {
  return EncodeChatGroupCallKeyReq(msg_id, group_id, call_id, want_key_id, out);
}

bool ClientCore::DecodeGroupCallKeyReq(
    const std::vector<std::uint8_t>& payload,
    std::size_t& offset,
    std::string& out_group_id,
    std::array<std::uint8_t, 16>& out_call_id,
    std::uint32_t& out_want_key_id) {
  return DecodeChatGroupCallKeyReq(payload, offset, out_group_id, out_call_id,
                                   out_want_key_id);
}

bool DecryptFileBlobForTooling(const std::vector<std::uint8_t>& blob,
                               const std::array<std::uint8_t, 32>& key,
                               std::vector<std::uint8_t>& out_plaintext) {
  return DecryptFileBlob(blob, key, out_plaintext);
}

void ClientCore::ResetRemoteStream() {
  std::lock_guard<std::mutex> lock(remote_stream_mutex_);
  remote_stream_.reset();
}

struct ClientCore::RemoteStream {
  std::string host;
  std::uint16_t port{0};
  bool use_tls{false};
  bool use_kcp{false};
  KcpConfig kcp_cfg;
  ProxyConfig proxy;
  std::string pinned_fingerprint;

  ikcpcb* kcp{nullptr};
  std::uint32_t kcp_conv{0};
  std::vector<std::uint8_t> kcp_recv_buf;
  std::chrono::steady_clock::time_point kcp_last_active{};

#ifdef _WIN32
  SOCKET sock{INVALID_SOCKET};
  ScopedCredHandle cred;
  ScopedCtxtHandle ctx;
  SecPkgContext_StreamSizes sizes{};
  std::vector<std::uint8_t> enc_buf;
  std::vector<std::uint8_t> plain_buf;
  std::size_t plain_off{0};
#else
  int sock{-1};
#endif

  RemoteStream(std::string host_in, std::uint16_t port_in, bool use_tls_in,
               bool use_kcp_in, KcpConfig kcp_cfg_in, ProxyConfig proxy_in,
               std::string pinned_fingerprint_in)
      : host(std::move(host_in)),
        port(port_in),
        use_tls(use_tls_in),
        use_kcp(use_kcp_in),
        kcp_cfg(std::move(kcp_cfg_in)),
        proxy(std::move(proxy_in)),
        pinned_fingerprint(std::move(pinned_fingerprint_in)) {}

  ~RemoteStream() { Close(); }

  bool Matches(const std::string& host_in, std::uint16_t port_in,
               bool use_tls_in, bool use_kcp_in, const KcpConfig& kcp_cfg_in,
               const ProxyConfig& proxy_in,
               const std::string& pinned_fingerprint_in) const {
    if (host != host_in || port != port_in || use_tls != use_tls_in ||
        use_kcp != use_kcp_in || pinned_fingerprint != pinned_fingerprint_in) {
      return false;
    }
    if (use_kcp) {
      if (kcp_cfg.enable != kcp_cfg_in.enable ||
          kcp_cfg.server_port != kcp_cfg_in.server_port ||
          kcp_cfg.mtu != kcp_cfg_in.mtu ||
          kcp_cfg.snd_wnd != kcp_cfg_in.snd_wnd ||
          kcp_cfg.rcv_wnd != kcp_cfg_in.rcv_wnd ||
          kcp_cfg.nodelay != kcp_cfg_in.nodelay ||
          kcp_cfg.interval != kcp_cfg_in.interval ||
          kcp_cfg.resend != kcp_cfg_in.resend ||
          kcp_cfg.nc != kcp_cfg_in.nc ||
          kcp_cfg.min_rto != kcp_cfg_in.min_rto ||
          kcp_cfg.request_timeout_ms != kcp_cfg_in.request_timeout_ms ||
          kcp_cfg.session_idle_sec != kcp_cfg_in.session_idle_sec) {
        return false;
      }
    }
    return proxy.type == proxy_in.type && proxy.host == proxy_in.host &&
           proxy.port == proxy_in.port && proxy.username == proxy_in.username &&
           proxy.password == proxy_in.password;
  }

  void Close() {
    if (kcp) {
      ikcp_release(kcp);
      kcp = nullptr;
    }
    kcp_recv_buf.clear();
    kcp_conv = 0;
    kcp_last_active = {};
#ifdef _WIN32
    if (sock != INVALID_SOCKET) {
      closesocket(sock);
      sock = INVALID_SOCKET;
    }
#else
    if (sock >= 0) {
      ::close(sock);
      sock = -1;
    }
#endif
    enc_buf.clear();
    plain_buf.clear();
    plain_off = 0;
  }

  static int KcpOutput(const char* buf, int len, ikcpcb* /*kcp*/, void* user) {
    if (!buf || len <= 0 || !user) {
      return -1;
    }
    auto* self = static_cast<RemoteStream*>(user);
#ifdef _WIN32
    const int sent = ::send(self->sock, buf, len, 0);
    return sent == len ? 0 : -1;
#else
    const ssize_t sent = ::send(self->sock, buf, static_cast<std::size_t>(len), 0);
    return sent == len ? 0 : -1;
#endif
  }

  bool ConnectPlain(std::string& error) {
    error.clear();
    if (host.empty() || port == 0) {
      error = "invalid endpoint";
      return false;
    }

    const bool use_proxy = proxy.enabled();
    if (use_proxy && proxy.type != ProxyType::kSocks5) {
      error = "unsupported proxy";
      return false;
    }
    const std::string connect_host = use_proxy ? proxy.host : host;
    const std::uint16_t connect_port = use_proxy ? proxy.port : port;

#ifdef _WIN32
    if (!EnsureWinsock()) {
      error = "winsock init failed";
      return false;
    }
#endif

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    const std::string port_str = std::to_string(connect_port);
    if (getaddrinfo(connect_host.c_str(), port_str.c_str(), &hints, &result) !=
        0) {
      error = "dns resolve failed";
      return false;
    }

#ifdef _WIN32
    SOCKET new_sock = INVALID_SOCKET;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
      new_sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (new_sock == INVALID_SOCKET) {
        continue;
      }
      if (::connect(new_sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) ==
          0) {
        break;
      }
      closesocket(new_sock);
      new_sock = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    if (new_sock == INVALID_SOCKET) {
      error = "connect failed";
      return false;
    }
    const DWORD timeout_ms = 30000;
    setsockopt(new_sock, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms),
               static_cast<int>(sizeof(timeout_ms)));
    setsockopt(new_sock, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&timeout_ms),
               static_cast<int>(sizeof(timeout_ms)));
#else
    int new_sock = -1;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
      new_sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (new_sock < 0) {
        continue;
      }
      if (::connect(new_sock, rp->ai_addr, rp->ai_addrlen) == 0) {
        break;
      }
      ::close(new_sock);
      new_sock = -1;
    }
    freeaddrinfo(result);
    if (new_sock < 0) {
      error = "connect failed";
      return false;
    }
#endif

    if (!use_proxy) {
      sock = new_sock;
      return true;
    }

    const auto close_socket = [&]() {
#ifdef _WIN32
      closesocket(new_sock);
#else
      ::close(new_sock);
#endif
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
                    static_cast<std::size_t>((std::numeric_limits<int>::max)())
                ? (std::numeric_limits<int>::max)()
                : static_cast<int>(remaining);
#ifdef _WIN32
        const int n = ::send(new_sock,
                             reinterpret_cast<const char*>(data + sent), chunk,
                             0);
#else
        const ssize_t n = ::send(new_sock, data + sent,
                                 static_cast<std::size_t>(chunk), 0);
#endif
        if (n <= 0) {
          return false;
        }
        sent += static_cast<std::size_t>(n);
      }
      return true;
    };

    const auto recv_exact = [&](std::uint8_t* data, std::size_t len) -> bool {
      if (!data || len == 0) {
        return true;
      }
      std::size_t got = 0;
      while (got < len) {
        const std::size_t remaining = len - got;
        const int want =
            remaining >
                    static_cast<std::size_t>((std::numeric_limits<int>::max)())
                ? (std::numeric_limits<int>::max)()
                : static_cast<int>(remaining);
#ifdef _WIN32
        const int n =
            ::recv(new_sock, reinterpret_cast<char*>(data + got), want, 0);
#else
        const ssize_t n =
            ::recv(new_sock, data + got, static_cast<std::size_t>(want), 0);
#endif
        if (n <= 0) {
          return false;
        }
        got += static_cast<std::size_t>(n);
      }
      return true;
    };

    std::vector<std::uint8_t> req;
    req.reserve(3 + 1 + proxy.username.size() + 1 + proxy.password.size());
    req.push_back(0x05);
    if (proxy.username.empty() && proxy.password.empty()) {
      req.push_back(0x01);
      req.push_back(0x00);
    } else {
      req.push_back(0x02);
      req.push_back(0x00);
      req.push_back(0x02);
    }
    if (!send_all(req.data(), req.size())) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }

    std::uint8_t rep[2] = {};
    if (!recv_exact(rep, sizeof(rep)) || rep[0] != 0x05) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }
    if (rep[1] == 0x02) {
      if (proxy.username.size() > 255 || proxy.password.size() > 255) {
        error = "proxy auth failed";
        close_socket();
        return false;
      }
      std::vector<std::uint8_t> auth;
      auth.reserve(3 + proxy.username.size() + proxy.password.size());
      auth.push_back(0x01);
      auth.push_back(static_cast<std::uint8_t>(proxy.username.size()));
      auth.insert(auth.end(), proxy.username.begin(), proxy.username.end());
      auth.push_back(static_cast<std::uint8_t>(proxy.password.size()));
      auth.insert(auth.end(), proxy.password.begin(), proxy.password.end());
      if (!send_all(auth.data(), auth.size())) {
        error = "proxy auth failed";
        close_socket();
        return false;
      }
      std::uint8_t auth_rep[2] = {};
      if (!recv_exact(auth_rep, sizeof(auth_rep)) || auth_rep[1] != 0x00) {
        error = "proxy auth failed";
        close_socket();
        return false;
      }
    } else if (rep[1] != 0x00) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }

    if (host.size() > 255) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }

    std::vector<std::uint8_t> connect_req;
    connect_req.reserve(4 + 1 + host.size() + 2);
    connect_req.push_back(0x05);
    connect_req.push_back(0x01);
    connect_req.push_back(0x00);
    connect_req.push_back(0x03);
    connect_req.push_back(static_cast<std::uint8_t>(host.size()));
    connect_req.insert(connect_req.end(), host.begin(), host.end());
    connect_req.push_back(static_cast<std::uint8_t>((port >> 8) & 0xFF));
    connect_req.push_back(static_cast<std::uint8_t>(port & 0xFF));
    if (!send_all(connect_req.data(), connect_req.size())) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }

    std::uint8_t rep2[4] = {};
    if (!recv_exact(rep2, sizeof(rep2)) || rep2[0] != 0x05 || rep2[1] != 0x00) {
      error = "proxy connect failed";
      close_socket();
      return false;
    }

    {
      std::size_t to_read = 0;
      if (rep2[3] == 0x01) {
        to_read = 4 + 2;
      } else if (rep2[3] == 0x03) {
        std::uint8_t len_byte = 0;
        if (!recv_exact(&len_byte, 1)) {
          error = "proxy connect failed";
          close_socket();
          return false;
        }
        to_read = static_cast<std::size_t>(len_byte) + 2;
      } else if (rep2[3] == 0x04) {
        to_read = 16 + 2;
      } else {
        error = "proxy connect failed";
        close_socket();
        return false;
      }
      std::vector<std::uint8_t> discard;
      discard.resize(to_read);
      if (!recv_exact(discard.data(), discard.size())) {
        error = "proxy connect failed";
        close_socket();
        return false;
      }
    }

    sock = new_sock;
    return true;
  }

  bool ConnectKcp(std::string& error) {
    error.clear();
    if (host.empty() || port == 0) {
      error = "invalid endpoint";
      return false;
    }
    if (proxy.enabled()) {
      error = "kcp does not support proxy";
      return false;
    }

#ifdef _WIN32
    if (!EnsureWinsock()) {
      error = "winsock init failed";
      return false;
    }
#endif

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* result = nullptr;
    const std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &result) != 0) {
      error = "dns resolve failed";
      return false;
    }

#ifdef _WIN32
    SOCKET new_sock = INVALID_SOCKET;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
      new_sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (new_sock == INVALID_SOCKET) {
        continue;
      }
      if (::connect(new_sock, rp->ai_addr, static_cast<int>(rp->ai_addrlen)) ==
          0) {
        break;
      }
      closesocket(new_sock);
      new_sock = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    if (new_sock == INVALID_SOCKET) {
      error = "connect failed";
      return false;
    }
#else
    int new_sock = -1;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
      new_sock = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (new_sock < 0) {
        continue;
      }
      if (::connect(new_sock, rp->ai_addr, rp->ai_addrlen) == 0) {
        break;
      }
      ::close(new_sock);
      new_sock = -1;
    }
    freeaddrinfo(result);
    if (new_sock < 0) {
      error = "connect failed";
      return false;
    }
#endif

    if (!SetNonBlockingSocket(new_sock)) {
      error = "kcp non-blocking failed";
#ifdef _WIN32
      closesocket(new_sock);
#else
      ::close(new_sock);
#endif
      return false;
    }

    sock = new_sock;
    std::uint32_t conv = 0;
    if (!RandomUint32(conv) || conv == 0) {
      conv = NowMs() ^ 0xA5A5A5A5u;
    }

    auto write_le32 = [](std::uint32_t v, std::uint8_t out[4]) {
      out[0] = static_cast<std::uint8_t>(v & 0xFF);
      out[1] = static_cast<std::uint8_t>((v >> 8) & 0xFF);
      out[2] = static_cast<std::uint8_t>((v >> 16) & 0xFF);
      out[3] = static_cast<std::uint8_t>((v >> 24) & 0xFF);
    };
    auto read_le32 = [](const std::uint8_t in[4]) -> std::uint32_t {
      return static_cast<std::uint32_t>(in[0]) |
             (static_cast<std::uint32_t>(in[1]) << 8) |
             (static_cast<std::uint32_t>(in[2]) << 16) |
             (static_cast<std::uint32_t>(in[3]) << 24);
    };
    auto build_cookie_packet =
        [&](std::uint8_t type,
            const std::array<std::uint8_t, kKcpCookieBytes>& cookie,
            std::array<std::uint8_t, kKcpCookiePacketBytes>& out) {
          write_le32(conv, out.data());
          out[4] = kKcpCookieCmd;
          out[5] = type;
          out[6] = 0;
          out[7] = 0;
          std::memcpy(out.data() + 8, cookie.data(), cookie.size());
        };
    auto send_cookie_packet =
        [&](std::uint8_t type,
            const std::array<std::uint8_t, kKcpCookieBytes>& cookie) -> bool {
          std::array<std::uint8_t, kKcpCookiePacketBytes> out{};
          build_cookie_packet(type, cookie, out);
#ifdef _WIN32
          return ::send(sock, reinterpret_cast<const char*>(out.data()),
                        static_cast<int>(out.size()), 0) ==
                 static_cast<int>(out.size());
#else
          return ::send(sock, out.data(), out.size(), 0) ==
                 static_cast<ssize_t>(out.size());
#endif
        };

    if (!send_cookie_packet(kKcpCookieHello, {})) {
      error = "kcp cookie hello failed";
      Close();
      return false;
    }

    const auto start = std::chrono::steady_clock::now();
    std::array<std::uint8_t, kKcpCookieBytes> cookie{};
    bool got_cookie = false;
    while (true) {
      std::uint8_t buf[64] = {};
#ifdef _WIN32
      const int n = ::recv(sock, reinterpret_cast<char*>(buf),
                           static_cast<int>(sizeof(buf)), 0);
#else
      const ssize_t n = ::recv(sock, buf, sizeof(buf), 0);
#endif
      if (n > 0) {
        if (static_cast<std::size_t>(n) >= kKcpCookiePacketBytes &&
            buf[4] == kKcpCookieCmd && read_le32(buf) == conv &&
            buf[5] == kKcpCookieChallenge) {
          std::memcpy(cookie.data(), buf + 8, cookie.size());
          got_cookie = true;
          break;
        }
        continue;
      }
      if (n == 0) {
        error = "kcp cookie recv failed";
        Close();
        return false;
      }
      if (!SocketWouldBlock()) {
        error = "kcp cookie recv failed";
        Close();
        return false;
      }
      const auto elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - start)
              .count();
      if (elapsed > static_cast<long long>(kcp_cfg.request_timeout_ms)) {
        error = "kcp cookie timeout";
        Close();
        return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (!got_cookie ||
        !send_cookie_packet(kKcpCookieResponse, cookie)) {
      error = "kcp cookie response failed";
      Close();
      return false;
    }

    kcp_conv = conv;
    kcp = ikcp_create(conv, this);
    if (!kcp) {
      error = "kcp create failed";
      Close();
      return false;
    }
    kcp->output = KcpOutput;
    ikcp_setmtu(kcp, static_cast<int>(kcp_cfg.mtu));
    ikcp_wndsize(kcp, static_cast<int>(kcp_cfg.snd_wnd),
                 static_cast<int>(kcp_cfg.rcv_wnd));
    ikcp_nodelay(kcp, static_cast<int>(kcp_cfg.nodelay),
                 static_cast<int>(kcp_cfg.interval),
                 static_cast<int>(kcp_cfg.resend),
                 static_cast<int>(kcp_cfg.nc));
    if (kcp_cfg.min_rto > 0) {
      kcp->rx_minrto = static_cast<int>(kcp_cfg.min_rto);
    }
    kcp_recv_buf.resize(std::max<std::uint32_t>(kcp_cfg.mtu, 1200u) + 256u);
    kcp_last_active = std::chrono::steady_clock::now();
    return true;
  }

#ifdef _WIN32
  bool ConnectTls(std::string& out_server_fingerprint, std::string& error) {
    out_server_fingerprint.clear();
    if (!ConnectPlain(error)) {
      return false;
    }

    SCHANNEL_CRED sch{};
    sch.dwVersion = SCHANNEL_CRED_VERSION;
    sch.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;

    TimeStamp expiry{};
    SECURITY_STATUS st =
        AcquireCredentialsHandleW(nullptr, const_cast<wchar_t*>(UNISP_NAME_W),
                                  SECPKG_CRED_OUTBOUND, nullptr, &sch, nullptr,
                                  nullptr, &cred.cred, &expiry);
    if (st != SEC_E_OK) {
      error = "AcquireCredentialsHandle failed";
      Close();
      return false;
    }
    cred.has = true;

    std::vector<std::uint8_t> in_buf;
    std::vector<std::uint8_t> extra;
    DWORD ctx_attr = 0;

    constexpr DWORD req_flags = ISC_REQ_SEQUENCE_DETECT |
                                ISC_REQ_REPLAY_DETECT |
                                ISC_REQ_CONFIDENTIALITY |
                                ISC_RET_EXTENDED_ERROR |
                                ISC_REQ_ALLOCATE_MEMORY |
                                ISC_REQ_STREAM;

    const std::wstring target = ToWide(host);
    bool have_ctx = false;
    while (true) {
      SecBuffer out_buffers[1];
      out_buffers[0].pvBuffer = nullptr;
      out_buffers[0].cbBuffer = 0;
      out_buffers[0].BufferType = SECBUFFER_TOKEN;

      SecBufferDesc out_desc{};
      out_desc.ulVersion = SECBUFFER_VERSION;
      out_desc.cBuffers = 1;
      out_desc.pBuffers = out_buffers;

      SecBuffer in_buffers[2];
      SecBufferDesc in_desc{};
      SecBufferDesc* in_desc_ptr = nullptr;
      if (!in_buf.empty()) {
        in_buffers[0].pvBuffer = in_buf.data();
        in_buffers[0].cbBuffer = static_cast<unsigned long>(in_buf.size());
        in_buffers[0].BufferType = SECBUFFER_TOKEN;
        in_buffers[1].pvBuffer = nullptr;
        in_buffers[1].cbBuffer = 0;
        in_buffers[1].BufferType = SECBUFFER_EMPTY;
        in_desc.ulVersion = SECBUFFER_VERSION;
        in_desc.cBuffers = 2;
        in_desc.pBuffers = in_buffers;
        in_desc_ptr = &in_desc;
      }

      st = InitializeSecurityContextW(
          &cred.cred, have_ctx ? &ctx.ctx : nullptr,
          target.empty() ? nullptr : const_cast<wchar_t*>(target.c_str()),
          req_flags, 0, SECURITY_NATIVE_DREP, in_desc_ptr, 0, &ctx.ctx,
          &out_desc, &ctx_attr, &expiry);
      have_ctx = true;
      ctx.has = true;

      if (st == SEC_I_COMPLETE_NEEDED || st == SEC_I_COMPLETE_AND_CONTINUE) {
        CompleteAuthToken(&ctx.ctx, &out_desc);
        st = (st == SEC_I_COMPLETE_NEEDED) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
      }

      if (out_buffers[0].pvBuffer && out_buffers[0].cbBuffer > 0) {
        const auto* p =
            reinterpret_cast<const std::uint8_t*>(out_buffers[0].pvBuffer);
        const std::size_t n = out_buffers[0].cbBuffer;
        const bool ok = SendAll(sock, p, n);
        FreeContextBuffer(out_buffers[0].pvBuffer);
        out_buffers[0].pvBuffer = nullptr;
        if (!ok) {
          error = "tls send handshake failed";
          Close();
          return false;
        }
      }

      if (st == SEC_E_OK) {
        if (!in_buf.empty() && in_desc_ptr &&
            in_buffers[1].BufferType == SECBUFFER_EXTRA &&
            in_buffers[1].cbBuffer > 0) {
          const std::size_t extra_len = in_buffers[1].cbBuffer;
          extra.assign(in_buf.end() - extra_len, in_buf.end());
        }
        break;
      }

      if (st == SEC_E_INCOMPLETE_MESSAGE) {
        if (!RecvSome(sock, in_buf)) {
          error = "tls handshake recv failed";
          Close();
          return false;
        }
        continue;
      }

      if (st == SEC_I_CONTINUE_NEEDED) {
        if (!in_buf.empty() && in_desc_ptr &&
            in_buffers[1].BufferType == SECBUFFER_EXTRA &&
            in_buffers[1].cbBuffer > 0) {
          const std::size_t extra_len = in_buffers[1].cbBuffer;
          std::vector<std::uint8_t> keep(in_buf.end() - extra_len, in_buf.end());
          in_buf.swap(keep);
        } else {
          in_buf.clear();
        }
        if (in_buf.empty()) {
          if (!RecvSome(sock, in_buf)) {
            error = "tls handshake recv failed";
            Close();
            return false;
          }
        }
        continue;
      }

      error = "tls handshake failed";
      Close();
      return false;
    }

    st = QueryContextAttributes(&ctx.ctx, SECPKG_ATTR_STREAM_SIZES, &sizes);
    if (st != SEC_E_OK) {
      error = "QueryContextAttributes failed";
      Close();
      return false;
    }

    ScopedCertContext remote_cert;
    st = QueryContextAttributes(&ctx.ctx, SECPKG_ATTR_REMOTE_CERT_CONTEXT,
                                &remote_cert.cert);
    if (st != SEC_E_OK || !remote_cert.cert) {
      error = "remote cert unavailable";
      Close();
      return false;
    }

    out_server_fingerprint = Sha256Hex(remote_cert.cert->pbCertEncoded,
                                       remote_cert.cert->cbCertEncoded);
    if (out_server_fingerprint.empty()) {
      error = "cert fingerprint failed";
      Close();
      return false;
    }
    if (pinned_fingerprint.empty()) {
      error = "server not trusted";
      Close();
      return false;
    }
    if (pinned_fingerprint != out_server_fingerprint) {
      error = "server fingerprint changed";
      Close();
      return false;
    }

    enc_buf = std::move(extra);
    return true;
  }
#endif

  bool Connect(std::string& out_server_fingerprint, std::string& error) {
    out_server_fingerprint.clear();
    if (use_kcp) {
      return ConnectKcp(error);
    }
#ifdef _WIN32
    if (use_tls) {
      return ConnectTls(out_server_fingerprint, error);
    }
#endif
    return ConnectPlain(error);
  }

  bool SendAndRecv(const std::vector<std::uint8_t>& in_bytes,
                   std::vector<std::uint8_t>& out_bytes,
                   std::string& error) {
    out_bytes.clear();
    error.clear();
    if (use_kcp) {
      const auto now = std::chrono::steady_clock::now();
      if (kcp_cfg.session_idle_sec > 0 &&
          kcp_last_active.time_since_epoch().count() != 0) {
        const auto idle = std::chrono::duration_cast<std::chrono::seconds>(
            now - kcp_last_active);
        if (idle.count() > static_cast<long long>(kcp_cfg.session_idle_sec)) {
          error = "kcp idle timeout";
          return false;
        }
      }

      if (!kcp || sock
#ifdef _WIN32
              == INVALID_SOCKET
#else
              < 0
#endif
      ) {
        error = "not connected";
        return false;
      }
      if (in_bytes.empty()) {
        error = "empty request";
        return false;
      }

      if (ikcp_send(kcp, reinterpret_cast<const char*>(in_bytes.data()),
                    static_cast<int>(in_bytes.size())) < 0) {
        error = "kcp send failed";
        return false;
      }
      ikcp_flush(kcp);
      kcp_last_active = now;

      const std::uint32_t start_ms = NowMs();
      const std::uint32_t timeout_ms =
          kcp_cfg.request_timeout_ms == 0 ? 5000u
                                          : kcp_cfg.request_timeout_ms;
      if (kcp_recv_buf.empty()) {
        kcp_recv_buf.resize(1400u + 256u);
      }
      auto& datagram = kcp_recv_buf;

      while (true) {
        const std::uint32_t now_ms = NowMs();
        if (now_ms - start_ms >= timeout_ms) {
          error = "kcp timeout";
          return false;
        }

        while (true) {
#ifdef _WIN32
          const int n = ::recv(sock, reinterpret_cast<char*>(datagram.data()),
                               static_cast<int>(datagram.size()), 0);
          if (n == SOCKET_ERROR) {
            if (SocketWouldBlock()) {
              break;
            }
            error = "kcp recv failed";
            return false;
          }
#else
          const ssize_t n = ::recv(sock, datagram.data(), datagram.size(), 0);
          if (n < 0) {
            if (SocketWouldBlock()) {
              break;
            }
            error = "kcp recv failed";
            return false;
          }
#endif
          if (n > 0) {
            ikcp_input(kcp, reinterpret_cast<const char*>(datagram.data()), n);
            kcp_last_active = std::chrono::steady_clock::now();
          } else {
            break;
          }
        }

        const int peek = ikcp_peeksize(kcp);
        if (peek > 0) {
          out_bytes.resize(static_cast<std::size_t>(peek));
          const int n = ikcp_recv(
              kcp, reinterpret_cast<char*>(out_bytes.data()), peek);
          if (n > 0) {
            out_bytes.resize(static_cast<std::size_t>(n));
            return true;
          }
          out_bytes.clear();
        }

        const std::uint32_t check = ikcp_check(kcp, now_ms);
        const std::uint32_t wait_ms =
            check > now_ms ? (check - now_ms) : 1u;
        const std::uint32_t remaining = timeout_ms - (now_ms - start_ms);
        const std::uint32_t sleep_ms = std::min(wait_ms, remaining);
        WaitForReadable(sock, sleep_ms);
        ikcp_update(kcp, NowMs());
      }
    }
#ifdef _WIN32
    if (use_tls) {
      if (sock == INVALID_SOCKET) {
        error = "not connected";
        return false;
      }
      if (!SchannelEncryptSend(sock, ctx, sizes, in_bytes)) {
        error = "tls send failed";
        return false;
      }
      if (!SchannelReadFrameBuffered(sock, ctx, enc_buf, plain_buf, plain_off,
                                     out_bytes)) {
        error = "tls recv failed";
        return false;
      }
      return !out_bytes.empty();
    }
#endif

#ifdef _WIN32
    if (sock == INVALID_SOCKET) {
      error = "not connected";
      return false;
    }
#else
    if (sock < 0) {
      error = "not connected";
      return false;
    }
#endif

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
                    static_cast<std::size_t>((std::numeric_limits<int>::max)())
                ? (std::numeric_limits<int>::max)()
                : static_cast<int>(remaining);
#ifdef _WIN32
        const int n = ::send(sock,
                             reinterpret_cast<const char*>(data + sent), chunk,
                             0);
#else
        const ssize_t n = ::send(sock, data + sent,
                                 static_cast<std::size_t>(chunk), 0);
#endif
        if (n <= 0) {
          return false;
        }
        sent += static_cast<std::size_t>(n);
      }
      return true;
    };

    const auto recv_exact = [&](std::uint8_t* data,
                                std::size_t len) -> bool {
      if (!data || len == 0) {
        return true;
      }
      std::size_t got = 0;
      while (got < len) {
        const std::size_t remaining = len - got;
        const int want =
            remaining >
                    static_cast<std::size_t>((std::numeric_limits<int>::max)())
                ? (std::numeric_limits<int>::max)()
                : static_cast<int>(remaining);
#ifdef _WIN32
        const int n =
            ::recv(sock, reinterpret_cast<char*>(data + got), want, 0);
#else
        const ssize_t n =
            ::recv(sock, data + got, static_cast<std::size_t>(want), 0);
#endif
        if (n <= 0) {
          return false;
        }
        got += static_cast<std::size_t>(n);
      }
      return true;
    };

    if (!send_all(in_bytes.data(), in_bytes.size())) {
      error = "tcp send failed";
      return false;
    }

    std::uint8_t header[mi::server::kFrameHeaderSize] = {};
    if (!recv_exact(header, sizeof(header))) {
      error = "tcp recv failed";
      return false;
    }
    mi::server::FrameType type;
    std::uint32_t payload_len = 0;
    if (!mi::server::DecodeFrameHeader(header, sizeof(header), type,
                                       payload_len)) {
      error = "tcp recv failed";
      return false;
    }
    (void)type;
    out_bytes.resize(mi::server::kFrameHeaderSize + payload_len);
    std::memcpy(out_bytes.data(), header, sizeof(header));
    if (payload_len > 0 &&
        !recv_exact(out_bytes.data() + mi::server::kFrameHeaderSize,
                    payload_len)) {
      error = "tcp recv failed";
      out_bytes.clear();
      return false;
    }
    return true;
  }
};

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
  const bool loaded = LoadClientConfig(config_path_, cfg, err);
  remote_mode_ = loaded;
  const std::filesystem::path config_dir = ResolveConfigDir(config_path_);
  const std::filesystem::path data_dir = ResolveDataDir(config_dir);
  if (!loaded) {
    last_error_ = err;
    if (err == "client section missing") {
      last_error_.clear();
      remote_mode_ = false;
    } else {
      return false;
    }
  }
  if (remote_mode_) {
    server_ip_ = cfg.server_ip;
    use_tls_ = cfg.use_tls;
    require_tls_ = cfg.require_tls;
    use_kcp_ = cfg.kcp.enable;
    kcp_cfg_ = cfg.kcp;
    if (use_kcp_) {
      use_tls_ = false;
      require_tls_ = false;
    }
    server_port_ = use_kcp_ && cfg.kcp.server_port != 0 ? cfg.kcp.server_port
                                                        : cfg.server_port;
    transport_kind_ = use_kcp_
                          ? mi::server::TransportKind::kKcp
                          : (use_tls_ ? mi::server::TransportKind::kTls
                                      : mi::server::TransportKind::kTcp);
    auth_mode_ = cfg.auth_mode;
    proxy_ = cfg.proxy;
    device_sync_enabled_ = cfg.device_sync.enabled;
    device_sync_is_primary_ =
        (cfg.device_sync.role == mi::client::DeviceSyncRole::kPrimary);
    identity_policy_.rotation_days = cfg.identity.rotation_days;
    identity_policy_.legacy_retention_days = cfg.identity.legacy_retention_days;
    identity_policy_.tpm_enable = cfg.identity.tpm_enable;
    identity_policy_.tpm_require = cfg.identity.tpm_require;
    pqc_precompute_pool_ = cfg.perf.pqc_precompute_pool;
    cover_traffic_enabled_ = ResolveCoverTrafficEnabled(cfg.traffic);
    cover_traffic_interval_sec_ = cfg.traffic.cover_traffic_interval_sec;
    cover_traffic_last_sent_ = {};
    trust_store_path_.clear();
    trust_store_tls_required_ = false;
    require_pinned_fingerprint_ = cfg.require_pinned_fingerprint;
    pinned_server_fingerprint_.clear();
    pending_server_fingerprint_.clear();
    pending_server_pin_.clear();
    if (!use_kcp_) {
      if (!cfg.trust_store.empty()) {
        std::filesystem::path trust = cfg.trust_store;
        if (!trust.is_absolute()) {
          trust = data_dir / trust;
        }
        trust_store_path_ = trust.string();
        TrustEntry entry;
        if (LoadTrustEntry(trust_store_path_,
                           EndpointKey(server_ip_, server_port_),
                           entry)) {
          pinned_server_fingerprint_ = entry.fingerprint;
          trust_store_tls_required_ = entry.tls_required;
        }
      }
      if (!cfg.pinned_fingerprint.empty()) {
        const std::string pin = NormalizeFingerprint(cfg.pinned_fingerprint);
        if (!IsHex64(pin)) {
          last_error_ = "pinned_fingerprint invalid";
          return false;
        }
        pinned_server_fingerprint_ = pin;
        if (!trust_store_path_.empty()) {
          TrustEntry entry{pin, require_tls_};
          std::string store_err;
          if (!StoreTrustEntry(trust_store_path_,
                               EndpointKey(server_ip_, server_port_),
                               entry, store_err)) {
            last_error_ = store_err.empty() ? "store trust failed" : store_err;
            return false;
          }
          trust_store_tls_required_ = entry.tls_required;
        }
      }
    } else {
      require_pinned_fingerprint_ = false;
      trust_store_path_.clear();
      pinned_server_fingerprint_.clear();
    }
    if (local_handle_) {
      mi_server_destroy(local_handle_);
      local_handle_ = nullptr;
    }
    token_.clear();
    last_error_.clear();
    send_seq_ = 0;

    e2ee_ = mi::client::e2ee::Engine{};
    e2ee_.SetPqcPoolSize(pqc_precompute_pool_);
    e2ee_inited_ = false;
    prekey_published_ = false;
    std::filesystem::path base = data_dir;
    if (base.empty()) {
      base = config_dir;
    }
    if (base.empty()) {
      base = std::filesystem::path{"."};
    }
    e2ee_state_dir_ = base / "e2ee_state";
    kt_state_path_ = e2ee_state_dir_ / "kt_state.bin";
    kt_require_signature_ = cfg.kt.require_signature;
    kt_gossip_alert_threshold_ = cfg.kt.gossip_alert_threshold;
    kt_root_pubkey_.clear();
    kt_root_pubkey_loaded_ = false;
    kt_gossip_mismatch_count_ = 0;
    kt_gossip_alerted_ = false;
    if (kt_require_signature_) {
      std::vector<std::uint8_t> key_bytes;
      if (!cfg.kt.root_pubkey_path.empty()) {
        std::filesystem::path key_path = cfg.kt.root_pubkey_path;
        if (!key_path.is_absolute()) {
          key_path = config_dir / key_path;
        }
        std::string key_err;
        if (!ReadFileBytes(key_path, key_bytes, key_err)) {
          last_error_ = key_err.empty() ? "kt root pubkey load failed" : key_err;
          return false;
        }
      } else if (!cfg.kt.root_pubkey_hex.empty()) {
        if (!HexToBytes(cfg.kt.root_pubkey_hex, key_bytes)) {
          last_error_ = "kt root pubkey hex invalid";
          return false;
        }
      } else {
        std::string key_err;
        if (!TryLoadKtRootPubkeyFromLoopback(config_dir, server_ip_, key_bytes, key_err)) {
          std::string data_err;
          if (!TryLoadKtRootPubkeyFromLoopback(data_dir, server_ip_, key_bytes, data_err)) {
            if (data_err.empty()) {
              data_err = key_err;
            }
            last_error_ = data_err.empty() ? "kt root pubkey missing" : data_err;
            return false;
          }
        }
      }
      if (key_bytes.size() != mi::server::kKtSthSigPublicKeyBytes) {
        last_error_ = "kt root pubkey size invalid";
        return false;
      }
      kt_root_pubkey_ = std::move(key_bytes);
      kt_root_pubkey_loaded_ = true;
    }
      if (!cfg.device_sync.key_path.empty()) {
        std::filesystem::path kp = cfg.device_sync.key_path;
        if (!kp.is_absolute()) {
          kp = data_dir / kp;
        }
        device_sync_key_path_ = kp;
      } else {
        device_sync_key_path_ = e2ee_state_dir_ / "device_sync_key.bin";
      }
    LoadKtState();
    LoadOrCreateDeviceId();
    if (device_sync_enabled_ && !LoadDeviceSyncKey()) {
      if (device_sync_is_primary_) {
        return false;
      }
      last_error_.clear();
    }
    if (require_tls_ && !use_tls_) {
      last_error_ = "require_tls=1 but use_tls=0";
      return false;
    }
    if (trust_store_tls_required_ && !use_tls_) {
      last_error_ = "tls downgrade detected";
      return false;
    }
    return !server_ip_.empty() && server_port_ != 0;
  }

  server_ip_.clear();
  server_port_ = 0;
  use_tls_ = false;
  require_tls_ = true;
  use_kcp_ = false;
  kcp_cfg_ = KcpConfig{};
  transport_kind_ = mi::server::TransportKind::kLocal;
  auth_mode_ = AuthMode::kLegacy;
  proxy_ = ProxyConfig{};
  device_sync_enabled_ = false;
  device_sync_is_primary_ = true;
  device_sync_key_loaded_ = false;
  device_sync_key_.fill(0);
  device_sync_key_path_.clear();
  device_id_.clear();
  trust_store_path_.clear();
  trust_store_tls_required_ = false;
  require_pinned_fingerprint_ = true;
  pinned_server_fingerprint_.clear();
  pending_server_fingerprint_.clear();
  pending_server_pin_.clear();
  identity_policy_ = mi::client::e2ee::IdentityPolicy{};
  pqc_precompute_pool_ = ClientConfig{}.perf.pqc_precompute_pool;
  cover_traffic_enabled_ = ResolveCoverTrafficEnabled(ClientConfig{}.traffic);
  cover_traffic_interval_sec_ = ClientConfig{}.traffic.cover_traffic_interval_sec;
  cover_traffic_last_sent_ = {};
  last_error_.clear();
  if (local_handle_) {
    mi_server_destroy(local_handle_);
    local_handle_ = nullptr;
  }

  e2ee_ = mi::client::e2ee::Engine{};
  e2ee_.SetPqcPoolSize(pqc_precompute_pool_);
  e2ee_inited_ = false;
  prekey_published_ = false;
  std::filesystem::path base = data_dir;
  if (base.empty()) {
    base = config_dir;
  }
  if (base.empty()) {
    base = std::filesystem::path{"."};
  }
  e2ee_state_dir_ = base / "e2ee_state";
  kt_state_path_ = e2ee_state_dir_ / "kt_state.bin";
  kt_require_signature_ = false;
  kt_gossip_alert_threshold_ = 3;
  kt_root_pubkey_.clear();
  kt_root_pubkey_loaded_ = false;
  kt_gossip_mismatch_count_ = 0;
  kt_gossip_alerted_ = false;
  device_sync_key_path_ = e2ee_state_dir_ / "device_sync_key.bin";
  LoadKtState();
  LoadOrCreateDeviceId();
  local_handle_ = mi_server_create(config_path.c_str());
  return local_handle_ != nullptr;
}

bool ClientCore::Register(const std::string& username,
                          const std::string& password) {
  last_error_.clear();
  username_ = username;
  password_ = password;
  if (username.empty() || password.empty()) {
    last_error_ = "credentials empty";
    return false;
  }

  if (auth_mode_ != AuthMode::kOpaque) {
    last_error_ = "register requires auth_mode=opaque";
    return false;
  }

  struct RustBuf {
    std::uint8_t* ptr{nullptr};
    std::size_t len{0};
    ~RustBuf() {
      if (ptr && len) {
        if (len > kMaxOpaqueMessageBytes) {
          return;  // avoid passing suspicious length to the Rust allocator
        }
        mi_opaque_free(ptr, len);
      }
    }
  };

  auto RustError = [&](const RustBuf& err,
                       const char* fallback) -> std::string {
    if (err.ptr && err.len && err.len <= kMaxOpaqueMessageBytes) {
      return std::string(reinterpret_cast<const char*>(err.ptr), err.len);
    }
    return fallback ? std::string(fallback) : std::string();
  };

  RustBuf req;
  RustBuf state;
  RustBuf err;
  const int start_rc = mi_opaque_client_register_start(
      reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
      &req.ptr, &req.len, &state.ptr, &state.len, &err.ptr, &err.len);
  if (start_rc != 0 || !req.ptr || req.len == 0 || !state.ptr || state.len == 0) {
    last_error_ = RustError(err, "opaque register start failed");
    return false;
  }
  const std::vector<std::uint8_t> req_vec(req.ptr, req.ptr + req.len);
  const std::vector<std::uint8_t> state_vec(state.ptr, state.ptr + state.len);

  mi::server::Frame start;
  start.type = mi::server::FrameType::kOpaqueRegisterStart;
  if (!mi::server::proto::WriteString(username, start.payload) ||
      !mi::server::proto::WriteBytes(req_vec, start.payload)) {
    last_error_ = "opaque register start payload too large";
    return false;
  }

  std::vector<std::uint8_t> resp_vec;
  if (!ProcessRaw(mi::server::EncodeFrame(start), resp_vec)) {
    if (last_error_.empty()) {
      last_error_ = "opaque register start failed";
    }
    return false;
  }

  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kOpaqueRegisterStart ||
      resp.payload.empty()) {
    last_error_ = "opaque register start response invalid";
    return false;
  }

  std::size_t off = 1;
  if (resp.payload[0] == 0) {
    std::string err_msg;
    mi::server::proto::ReadString(resp.payload, off, err_msg);
    last_error_ = err_msg.empty() ? "opaque register start failed" : err_msg;
    return false;
  }
  std::vector<std::uint8_t> reg_resp;
  if (!mi::server::proto::ReadBytes(resp.payload, off, reg_resp) ||
      off != resp.payload.size() || reg_resp.empty()) {
    last_error_ = "opaque register start response invalid";
    return false;
  }

  RustBuf upload;
  RustBuf err2;
  const int finish_rc = mi_opaque_client_register_finish(
      reinterpret_cast<const std::uint8_t*>(username.data()), username.size(),
      reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
      state_vec.data(), state_vec.size(),
      reg_resp.data(), reg_resp.size(),
      &upload.ptr, &upload.len, &err2.ptr, &err2.len);
  if (finish_rc != 0 || !upload.ptr || upload.len == 0) {
    last_error_ = RustError(err2, "opaque register finish failed");
    return false;
  }
  const std::vector<std::uint8_t> upload_vec(upload.ptr, upload.ptr + upload.len);

  mi::server::Frame finish;
  finish.type = mi::server::FrameType::kOpaqueRegisterFinish;
  if (!mi::server::proto::WriteString(username, finish.payload) ||
      !mi::server::proto::WriteBytes(upload_vec, finish.payload)) {
    last_error_ = "opaque register finish payload too large";
    return false;
  }

  resp_vec.clear();
  if (!ProcessRaw(mi::server::EncodeFrame(finish), resp_vec)) {
    if (last_error_.empty()) {
      last_error_ = "opaque register finish failed";
    }
    return false;
  }

  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kOpaqueRegisterFinish ||
      resp.payload.empty()) {
    last_error_ = "opaque register finish response invalid";
    return false;
  }
  off = 1;
  if (resp.payload[0] == 0) {
    std::string err_msg;
    mi::server::proto::ReadString(resp.payload, off, err_msg);
    last_error_ = err_msg.empty() ? "opaque register finish failed" : err_msg;
    return false;
  }
  if (off != resp.payload.size()) {
    last_error_ = "opaque register finish response invalid";
    return false;
  }

  last_error_.clear();
  return true;
}

bool ClientCore::Login(const std::string& username,
                       const std::string& password) {
#if 0
  username_ = username;
  password_ = password;
  last_error_.clear();

  constexpr std::size_t kKemSecretKeyBytes = 2400;
  static_assert(mi::server::kX25519PublicKeyBytes == 32);
  static_assert(mi::server::kMlKem768SharedSecretBytes == 32);

  std::array<std::uint8_t, 32> client_nonce{};
  if (!RandomBytes(client_nonce.data(), client_nonce.size())) {
    last_error_ = "rng failed";
    return false;
  }

  std::array<std::uint8_t, 32> client_dh_sk{};
  std::array<std::uint8_t, 32> client_dh_pk{};
  std::array<std::uint8_t, mi::server::kMlKem768PublicKeyBytes> client_kem_pk{};
  std::array<std::uint8_t, kKemSecretKeyBytes> client_kem_sk{};
  if (!RandomBytes(client_dh_sk.data(), client_dh_sk.size())) {
    last_error_ = "rng failed";
    return false;
  }
  crypto_x25519_public_key(client_dh_pk.data(), client_dh_sk.data());
  if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_keypair(client_kem_pk.data(),
                                                client_kem_sk.data()) != 0) {
    last_error_ = "mlkem keypair failed";
    return false;
  }

  mi::server::Frame start;
  start.type = mi::server::FrameType::kPakeStart;
  mi::server::proto::WriteString(username, start.payload);
  mi::server::proto::WriteUint32(
      static_cast<std::uint32_t>(client_nonce.size()), start.payload);
  start.payload.insert(start.payload.end(), client_nonce.begin(),
                       client_nonce.end());
  mi::server::proto::WriteUint32(
      static_cast<std::uint32_t>(client_dh_pk.size()), start.payload);
  start.payload.insert(start.payload.end(), client_dh_pk.begin(),
                       client_dh_pk.end());
  mi::server::proto::WriteUint32(
      static_cast<std::uint32_t>(client_kem_pk.size()), start.payload);
  start.payload.insert(start.payload.end(), client_kem_pk.begin(),
                       client_kem_pk.end());

  std::vector<std::uint8_t> resp_vec;
  if (!ProcessRaw(mi::server::EncodeFrame(start), resp_vec)) {
    if (last_error_.empty()) {
      last_error_ = "pake start failed";
    }
    return false;
  }

  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp)) {
    last_error_ = "pake start response invalid";
    return false;
  }
  if (resp.type != mi::server::FrameType::kPakeStart || resp.payload.empty()) {
    last_error_ = "pake start response invalid";
    return false;
  }

  std::size_t off = 1;
  if (resp.payload[0] == 0) {
    std::string err;
    mi::server::proto::ReadString(resp.payload, off, err);
    last_error_ = err.empty() ? "pake start failed" : err;
    return false;
  }

  std::string pake_id;
  std::vector<std::uint8_t> salt;
  std::vector<std::uint8_t> server_nonce;
  std::vector<std::uint8_t> server_dh_pk;
  std::vector<std::uint8_t> kem_ct;
  std::vector<std::uint8_t> server_proof;
  std::uint32_t argon_blocks = 0;
  std::uint32_t argon_passes = 0;
  if (!mi::server::proto::ReadString(resp.payload, off, pake_id) ||
      off >= resp.payload.size()) {
    last_error_ = "pake start response invalid";
    return false;
  }
  const std::uint8_t scheme = resp.payload[off++];
  if (!mi::server::proto::ReadUint32(resp.payload, off, argon_blocks) ||
      !mi::server::proto::ReadUint32(resp.payload, off, argon_passes) ||
      !mi::server::proto::ReadBytes(resp.payload, off, salt) ||
      !mi::server::proto::ReadBytes(resp.payload, off, server_nonce) ||
      !mi::server::proto::ReadBytes(resp.payload, off, server_dh_pk) ||
      !mi::server::proto::ReadBytes(resp.payload, off, kem_ct) ||
      !mi::server::proto::ReadBytes(resp.payload, off, server_proof) ||
      off != resp.payload.size()) {
    last_error_ = "pake start response invalid";
    return false;
  }
  if (server_nonce.size() != 32 ||
      server_dh_pk.size() != mi::server::kX25519PublicKeyBytes ||
      kem_ct.size() != mi::server::kMlKem768CiphertextBytes ||
      server_proof.size() != 32) {
    last_error_ = "pake start response invalid";
    return false;
  }

  std::array<std::uint8_t, 32> pw_key{};
  if (scheme == static_cast<std::uint8_t>(mi::server::PakePwScheme::kSha256)) {
    mi::server::crypto::Sha256Digest d;
    mi::server::crypto::Sha256(
        reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
        d);
    pw_key = d.bytes;
  } else if (scheme ==
             static_cast<std::uint8_t>(mi::server::PakePwScheme::kSaltedSha256)) {
    std::vector<std::uint8_t> msg;
    msg.reserve(salt.size() + password.size());
    msg.insert(msg.end(), salt.begin(), salt.end());
    msg.insert(msg.end(), password.begin(), password.end());
    mi::server::crypto::Sha256Digest d;
    mi::server::crypto::Sha256(msg.data(), msg.size(), d);
    pw_key = d.bytes;
  } else if (scheme ==
             static_cast<std::uint8_t>(mi::server::PakePwScheme::kArgon2id)) {
    if (argon_blocks == 0 || argon_passes == 0 || argon_blocks > 8192 ||
        argon_passes > 16 || salt.empty()) {
      last_error_ = "argon2id params invalid";
      return false;
    }
    std::vector<std::uint8_t> work_area;
    work_area.resize(static_cast<std::size_t>(argon_blocks) * 1024);

    crypto_argon2_config cfg;
    cfg.algorithm = CRYPTO_ARGON2_ID;
    cfg.nb_blocks = argon_blocks;
    cfg.nb_passes = argon_passes;
    cfg.nb_lanes = 1;

    crypto_argon2_inputs in;
    in.pass = reinterpret_cast<const std::uint8_t*>(password.data());
    in.pass_size = static_cast<std::uint32_t>(password.size());
    in.salt = salt.data();
    in.salt_size = static_cast<std::uint32_t>(salt.size());

    crypto_argon2(pw_key.data(), static_cast<std::uint32_t>(pw_key.size()),
                  work_area.data(), cfg, in, crypto_argon2_no_extras);
  } else {
    last_error_ = "pake scheme unsupported";
    return false;
  }

  std::array<std::uint8_t, 32> server_dh_pk_arr{};
  std::copy_n(server_dh_pk.begin(), server_dh_pk_arr.size(),
              server_dh_pk_arr.begin());
  std::array<std::uint8_t, 32> dh_shared{};
  crypto_x25519(dh_shared.data(), client_dh_sk.data(), server_dh_pk_arr.data());
  if (IsAllZero(dh_shared.data(), dh_shared.size())) {
    last_error_ = "x25519 shared invalid";
    return false;
  }

  std::array<std::uint8_t, 32> kem_shared{};
  if (PQCLEAN_MLKEM768_CLEAN_crypto_kem_dec(kem_shared.data(), kem_ct.data(),
                                           client_kem_sk.data()) != 0) {
    last_error_ = "mlkem decaps failed";
    return false;
  }

  const auto build_transcript = [&]() {
    std::vector<std::uint8_t> t;
    static constexpr char kPrefix[] = "mi_e2ee_pake_login_v1";
    t.insert(t.end(), kPrefix, kPrefix + sizeof(kPrefix) - 1);
    t.push_back(0);
    t.insert(t.end(), username.begin(), username.end());
    t.push_back(0);
    t.insert(t.end(), pake_id.begin(), pake_id.end());
    t.push_back(0);
    t.push_back(scheme);
    mi::server::proto::WriteUint32(argon_blocks, t);
    mi::server::proto::WriteUint32(argon_passes, t);
    const std::uint16_t salt_len = static_cast<std::uint16_t>(salt.size());
    t.push_back(static_cast<std::uint8_t>(salt_len & 0xFF));
    t.push_back(static_cast<std::uint8_t>((salt_len >> 8) & 0xFF));
    t.insert(t.end(), salt.begin(), salt.end());
    t.insert(t.end(), client_nonce.begin(), client_nonce.end());
    t.insert(t.end(), server_nonce.begin(), server_nonce.end());
    t.insert(t.end(), client_dh_pk.begin(), client_dh_pk.end());
    t.insert(t.end(), server_dh_pk.begin(), server_dh_pk.end());
    t.insert(t.end(), client_kem_pk.begin(), client_kem_pk.end());
    t.insert(t.end(), kem_ct.begin(), kem_ct.end());
    return t;
  };

  const auto transcript = build_transcript();
  mi::server::crypto::Sha256Digest transcript_hash;
  mi::server::crypto::Sha256(transcript.data(), transcript.size(),
                             transcript_hash);

  std::array<std::uint8_t, 64> ikm{};
  std::copy_n(dh_shared.begin(), dh_shared.size(), ikm.begin() + 0);
  std::copy_n(kem_shared.begin(), kem_shared.size(), ikm.begin() + 32);

  std::array<std::uint8_t, 32> handshake_key{};
  static constexpr char kInfoPrefix[] = "mi_e2ee_pake_hybrid_v1";
  std::uint8_t info[sizeof(kInfoPrefix) - 1 + 32];
  std::memcpy(info, kInfoPrefix, sizeof(kInfoPrefix) - 1);
  std::memcpy(info + (sizeof(kInfoPrefix) - 1), transcript_hash.bytes.data(),
              transcript_hash.bytes.size());
  if (!mi::server::crypto::HkdfSha256(ikm.data(), ikm.size(), pw_key.data(),
                                      pw_key.size(), info, sizeof(info),
                                      handshake_key.data(),
                                      handshake_key.size())) {
    last_error_ = "hkdf failed";
    return false;
  }

  static constexpr char kServerProofPrefix[] = "mi_e2ee_pake_server_proof_v1";
  std::uint8_t proof_msg[sizeof(kServerProofPrefix) - 1 + 32];
  std::memcpy(proof_msg, kServerProofPrefix, sizeof(kServerProofPrefix) - 1);
  std::memcpy(proof_msg + (sizeof(kServerProofPrefix) - 1),
              transcript_hash.bytes.data(), transcript_hash.bytes.size());
  mi::server::crypto::Sha256Digest expected_server_proof;
  mi::server::crypto::HmacSha256(
      handshake_key.data(), handshake_key.size(), proof_msg, sizeof(proof_msg),
      expected_server_proof);
  std::array<std::uint8_t, 32> server_proof_arr{};
  std::copy_n(server_proof.begin(), server_proof_arr.size(),
              server_proof_arr.begin());
  if (expected_server_proof.bytes != server_proof_arr) {
    last_error_ = "server proof invalid";
    return false;
  }

  static constexpr char kClientProofPrefix[] = "mi_e2ee_pake_client_proof_v1";
  std::uint8_t client_msg[sizeof(kClientProofPrefix) - 1 + 32];
  std::memcpy(client_msg, kClientProofPrefix, sizeof(kClientProofPrefix) - 1);
  std::memcpy(client_msg + (sizeof(kClientProofPrefix) - 1),
              transcript_hash.bytes.data(), transcript_hash.bytes.size());
  mi::server::crypto::Sha256Digest client_proof;
  mi::server::crypto::HmacSha256(handshake_key.data(), handshake_key.size(),
                                 client_msg, sizeof(client_msg), client_proof);

  mi::server::Frame finish;
  finish.type = mi::server::FrameType::kPakeFinish;
  mi::server::proto::WriteString(pake_id, finish.payload);
  mi::server::proto::WriteUint32(static_cast<std::uint32_t>(client_proof.bytes.size()),
                                 finish.payload);
  finish.payload.insert(finish.payload.end(), client_proof.bytes.begin(),
                        client_proof.bytes.end());
  resp_vec.clear();
  if (!ProcessRaw(mi::server::EncodeFrame(finish), resp_vec)) {
    if (last_error_.empty()) {
      last_error_ = "pake finish failed";
    }
    return false;
  }

  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kPakeFinish ||
      resp.payload.empty()) {
    last_error_ = "pake finish response invalid";
    return false;
  }
  off = 1;
  std::string token_or_error;
  if (!mi::server::proto::ReadString(resp.payload, off, token_or_error) ||
      off != resp.payload.size()) {
    last_error_ = "pake finish response invalid";
    return false;
  }
  if (resp.payload[0] == 0) {
    last_error_ =
        token_or_error.empty() ? "pake finish failed" : token_or_error;
    return false;
  }
  token_ = std::move(token_or_error);

  std::string err;
  if (!mi::server::DeriveKeysFromPakeHandshake(handshake_key, username, token_,
                                               transport_kind_, keys_, err)) {
    token_.clear();
    last_error_ = err.empty() ? "key derivation failed" : err;
    return false;
  }

  channel_ = mi::server::SecureChannel(keys_, mi::server::SecureChannelRole::kClient);
  send_seq_ = 0;
  prekey_published_ = false;
  if (e2ee_inited_) {
    e2ee_.SetLocalUsername(username_);
  }
  last_error_.clear();
  return true;
#endif

  last_error_.clear();
  username_ = username;
  password_ = password;
  token_.clear();
  send_seq_ = 0;
  prekey_published_ = false;

  if (username.empty() || password.empty()) {
    last_error_ = "credentials empty";
    return false;
  }

  if (auth_mode_ == AuthMode::kLegacy) {
    mi::server::Frame login;
    login.type = mi::server::FrameType::kLogin;
    if (!mi::server::proto::WriteString(username, login.payload) ||
        !mi::server::proto::WriteString(password, login.payload)) {
      last_error_ = "credentials too long";
      return false;
    }

    std::vector<std::uint8_t> resp_vec;
    if (!ProcessRaw(mi::server::EncodeFrame(login), resp_vec)) {
      if (last_error_.empty()) {
        last_error_ = "login failed";
      }
      return false;
    }

    mi::server::Frame resp;
    if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
        resp.type != mi::server::FrameType::kLogin || resp.payload.empty()) {
      last_error_ = "login response invalid";
      return false;
    }

    std::size_t off = 1;
    std::string token_or_error;
    if (!mi::server::proto::ReadString(resp.payload, off, token_or_error) ||
        off != resp.payload.size()) {
      last_error_ = "login response invalid";
      return false;
    }
    if (resp.payload[0] == 0) {
      last_error_ = token_or_error.empty() ? "login failed" : token_or_error;
      return false;
    }
    token_ = std::move(token_or_error);

    std::string key_err;
    if (!mi::server::DeriveKeysFromCredentials(username, password,
                                               transport_kind_, keys_, key_err)) {
      token_.clear();
      last_error_ = key_err.empty() ? "key derivation failed" : key_err;
      return false;
    }

    channel_ = mi::server::SecureChannel(
        keys_, mi::server::SecureChannelRole::kClient);
    send_seq_ = 0;
    prekey_published_ = false;
    if (e2ee_inited_) {
      e2ee_.SetLocalUsername(username_);
    }
    if (history_enabled_ && !e2ee_state_dir_.empty()) {
      auto store = std::make_unique<ChatHistoryStore>();
      std::string hist_err;
      if (store->Init(e2ee_state_dir_, username_, hist_err)) {
        history_store_ = std::move(store);
        WarmupHistoryOnStartup();
      } else {
        history_store_.reset();
      }
    } else {
      history_store_.reset();
    }
    friend_sync_version_ = 0;
    last_error_.clear();
    return true;
  }

  struct RustBuf {
    std::uint8_t* ptr{nullptr};
    std::size_t len{0};
    ~RustBuf() {
      if (ptr && len) {
        if (len > kMaxOpaqueMessageBytes) {
          return;  // avoid passing suspicious length to the Rust allocator
        }
        mi_opaque_free(ptr, len);
      }
    }
  };

  auto RustError = [&](const RustBuf& err, const char* fallback) -> std::string {
    if (err.ptr && err.len && err.len <= kMaxOpaqueMessageBytes) {
      return std::string(reinterpret_cast<const char*>(err.ptr), err.len);
    }
    return fallback ? std::string(fallback) : std::string();
  };

  RustBuf req;
  RustBuf state;
  RustBuf err;
  const int start_rc = mi_opaque_client_login_start(
      reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
      &req.ptr, &req.len, &state.ptr, &state.len, &err.ptr, &err.len);
  if (start_rc != 0 || !req.ptr || req.len == 0 || !state.ptr || state.len == 0) {
    last_error_ = RustError(err, "opaque login start failed");
    return false;
  }
  if (req.len > kMaxOpaqueMessageBytes || state.len > kMaxOpaqueMessageBytes) {
    last_error_ = "opaque message too large";
    return false;
  }

  const std::vector<std::uint8_t> req_vec(req.ptr, req.ptr + req.len);
  const std::vector<std::uint8_t> state_vec(state.ptr, state.ptr + state.len);

  mi::server::Frame start;
  start.type = mi::server::FrameType::kOpaqueLoginStart;
  if (!mi::server::proto::WriteString(username, start.payload) ||
      !mi::server::proto::WriteBytes(req_vec, start.payload)) {
    last_error_ = "opaque login start payload too large";
    return false;
  }

  std::vector<std::uint8_t> resp_vec;
  if (!ProcessRaw(mi::server::EncodeFrame(start), resp_vec)) {
    if (last_error_.empty()) {
      last_error_ = "opaque login start failed";
    }
    return false;
  }

  mi::server::Frame resp;
  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kOpaqueLoginStart ||
      resp.payload.empty()) {
    last_error_ = "opaque login start response invalid";
    return false;
  }

  std::size_t off = 1;
  if (resp.payload[0] == 0) {
    std::string err_msg;
    mi::server::proto::ReadString(resp.payload, off, err_msg);
    last_error_ = err_msg.empty() ? "opaque login start failed" : err_msg;
    return false;
  }

  std::string login_id;
  std::vector<std::uint8_t> cred_resp;
  if (!mi::server::proto::ReadString(resp.payload, off, login_id) ||
      !mi::server::proto::ReadBytes(resp.payload, off, cred_resp) ||
      off != resp.payload.size() || login_id.empty() || cred_resp.empty()) {
    last_error_ = "opaque login start response invalid";
    return false;
  }
  if (cred_resp.size() > kMaxOpaqueMessageBytes) {
    last_error_ = "opaque message too large";
    return false;
  }

  RustBuf finalization;
  RustBuf session_key;
  RustBuf err2;
  const int finish_rc = mi_opaque_client_login_finish(
      reinterpret_cast<const std::uint8_t*>(username.data()), username.size(),
      reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
      state_vec.data(), state_vec.size(), cred_resp.data(), cred_resp.size(),
      &finalization.ptr, &finalization.len, &session_key.ptr, &session_key.len,
      &err2.ptr, &err2.len);
  if (finish_rc != 0 || !finalization.ptr || finalization.len == 0 ||
      !session_key.ptr || session_key.len == 0) {
    const std::string rust_err = RustError(err2, "opaque login finish failed");
    last_error_ =
        (rust_err == "client login finish failed") ? "invalid credentials"
                                                   : rust_err;
    return false;
  }
  if (finalization.len > kMaxOpaqueMessageBytes ||
      session_key.len > kMaxOpaqueSessionKeyBytes) {
    last_error_ = "opaque message too large";
    return false;
  }
  const std::vector<std::uint8_t> final_vec(finalization.ptr,
                                            finalization.ptr + finalization.len);
  const std::vector<std::uint8_t> session_key_vec(session_key.ptr,
                                                  session_key.ptr + session_key.len);

  mi::server::Frame finish;
  finish.type = mi::server::FrameType::kOpaqueLoginFinish;
  if (!mi::server::proto::WriteString(login_id, finish.payload) ||
      !mi::server::proto::WriteBytes(final_vec, finish.payload)) {
    last_error_ = "opaque login finish payload too large";
    return false;
  }

  resp_vec.clear();
  if (!ProcessRaw(mi::server::EncodeFrame(finish), resp_vec)) {
    if (last_error_.empty()) {
      last_error_ = "opaque login finish failed";
    }
    return false;
  }

  if (!mi::server::DecodeFrame(resp_vec.data(), resp_vec.size(), resp) ||
      resp.type != mi::server::FrameType::kOpaqueLoginFinish ||
      resp.payload.empty()) {
    last_error_ = "opaque login finish response invalid";
    return false;
  }

  off = 1;
  std::string token_or_error;
  if (!mi::server::proto::ReadString(resp.payload, off, token_or_error) ||
      off != resp.payload.size()) {
    last_error_ = "opaque login finish response invalid";
    return false;
  }
  if (resp.payload[0] == 0) {
    last_error_ =
        token_or_error.empty() ? "opaque login finish failed" : token_or_error;
    return false;
  }
  token_ = std::move(token_or_error);

  std::string key_err;
  if (!mi::server::DeriveKeysFromOpaqueSessionKey(session_key_vec, username,
                                                  token_, transport_kind_,
                                                  keys_, key_err)) {
    token_.clear();
    last_error_ = key_err.empty() ? "key derivation failed" : key_err;
    return false;
  }

  channel_ = mi::server::SecureChannel(keys_, mi::server::SecureChannelRole::kClient);
  send_seq_ = 0;
  prekey_published_ = false;
  if (e2ee_inited_) {
    e2ee_.SetLocalUsername(username_);
  }
  if (history_enabled_ && !e2ee_state_dir_.empty()) {
    auto store = std::make_unique<ChatHistoryStore>();
    std::string hist_err;
    if (store->Init(e2ee_state_dir_, username_, hist_err)) {
      history_store_ = std::move(store);
      WarmupHistoryOnStartup();
    } else {
      history_store_.reset();
    }
  } else {
    history_store_.reset();
  }
  friend_sync_version_ = 0;
  last_error_.clear();
  return true;
}

bool ClientCore::Relogin() {
  last_error_.clear();
  if (username_.empty() || password_.empty()) {
    last_error_ = "no cached credentials";
    return false;
  }
  return Login(username_, password_);
}

bool ClientCore::Logout() {
  ResetRemoteStream();
  if (token_.empty()) {
    return true;
  }
  std::vector<std::uint8_t> ignore;
  ProcessEncrypted(mi::server::FrameType::kLogout, {}, ignore);
  token_.clear();
  prekey_published_ = false;
  e2ee_ = mi::client::e2ee::Engine{};
  e2ee_.SetPqcPoolSize(pqc_precompute_pool_);
  e2ee_inited_ = false;
  peer_id_cache_.clear();
  group_sender_keys_.clear();
  pending_sender_key_dists_.clear();
  sender_key_req_last_sent_.clear();
  pending_group_cipher_.clear();
  group_delivery_map_.clear();
  group_delivery_order_.clear();
  chat_seen_ids_.clear();
  chat_seen_order_.clear();
  FlushHistoryOnShutdown();
  history_store_.reset();
  cover_traffic_last_sent_ = {};
  friend_sync_version_ = 0;
  last_error_.clear();
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

bool ClientCore::EnsureE2ee() {
  if (e2ee_inited_) {
    return true;
  }
  if (e2ee_state_dir_.empty()) {
    const auto cfg_dir = ResolveConfigDir(config_path_);
    const auto data_dir = ResolveDataDir(cfg_dir);
    std::filesystem::path base = data_dir;
    if (base.empty()) {
      base = cfg_dir;
    }
    if (base.empty()) {
      base = std::filesystem::path{"."};
    }
    e2ee_state_dir_ = base / "e2ee_state";
    kt_state_path_ = e2ee_state_dir_ / "kt_state.bin";
    LoadKtState();
  }

  std::string err;
  e2ee_.SetIdentityPolicy(identity_policy_);
  if (!e2ee_.Init(e2ee_state_dir_, err)) {
    last_error_ = err.empty() ? "e2ee init failed" : err;
    return false;
  }
  if (!username_.empty()) {
    e2ee_.SetLocalUsername(username_);
  }
  e2ee_inited_ = true;
  return true;
}

bool ClientCore::LoadKtState() {
  kt_tree_size_ = 0;
  kt_root_.fill(0);
  if (kt_state_path_.empty()) {
    return true;
  }
  std::ifstream f(kt_state_path_, std::ios::binary);
  if (!f.is_open()) {
    return true;
  }
  char magic[8];
  f.read(magic, sizeof(magic));
  if (!f.good() || std::memcmp(magic, "MIKTSTH1", 8) != 0) {
    return true;
  }
  std::uint8_t size_buf[8];
  f.read(reinterpret_cast<char*>(size_buf), sizeof(size_buf));
  if (!f.good()) {
    return true;
  }
  std::uint64_t size = 0;
  for (int i = 0; i < 8; ++i) {
    size |= (static_cast<std::uint64_t>(size_buf[i]) << (i * 8));
  }
  std::uint8_t root_buf[32];
  f.read(reinterpret_cast<char*>(root_buf), sizeof(root_buf));
  if (!f.good()) {
    return true;
  }
  kt_tree_size_ = size;
  std::memcpy(kt_root_.data(), root_buf, sizeof(root_buf));
  return true;
}

bool ClientCore::SaveKtState() {
  if (kt_state_path_.empty()) {
    return true;
  }
  std::error_code ec;
  const auto dir =
      kt_state_path_.has_parent_path() ? kt_state_path_.parent_path()
                                       : std::filesystem::path{};
  if (!dir.empty()) {
    std::filesystem::create_directories(dir, ec);
  }
  const auto tmp = kt_state_path_.string() + ".tmp";
  std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
  if (!f) {
    return false;
  }
  f.write("MIKTSTH1", 8);
  std::uint8_t size_buf[8];
  for (int i = 0; i < 8; ++i) {
    size_buf[i] = static_cast<std::uint8_t>((kt_tree_size_ >> (i * 8)) & 0xFF);
  }
  f.write(reinterpret_cast<const char*>(size_buf), sizeof(size_buf));
  f.write(reinterpret_cast<const char*>(kt_root_.data()),
          static_cast<std::streamsize>(kt_root_.size()));
  f.close();
  if (!f) {
    return false;
  }
  std::filesystem::rename(tmp, kt_state_path_, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    return false;
  }
  return true;
}

bool ClientCore::LoadOrCreateDeviceId() {
  if (!device_id_.empty()) {
    return true;
  }
  if (e2ee_state_dir_.empty()) {
    return true;
  }

  std::error_code ec;
  std::filesystem::create_directories(e2ee_state_dir_, ec);

  const auto path = e2ee_state_dir_ / "device_id.txt";
  {
    std::ifstream f(path, std::ios::binary);
    if (f.is_open()) {
      std::string line;
      std::getline(f, line);
      const std::string id = Trim(line);
      if (id.size() == 32) {
        bool ok = true;
        for (const char c : id) {
          if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                (c >= 'A' && c <= 'F'))) {
            ok = false;
            break;
          }
        }
        if (ok) {
          device_id_ = id;
          for (auto& ch : device_id_) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
          }
          return true;
        }
      }
    }
  }

  std::array<std::uint8_t, 16> rnd{};
  if (!RandomBytes(rnd.data(), rnd.size())) {
    last_error_ = "rng failed";
    return false;
  }
  device_id_ = BytesToHexLower(rnd.data(), rnd.size());
  if (device_id_.empty()) {
    last_error_ = "device id generation failed";
    return false;
  }

  const auto tmp = path.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      last_error_ = "device id write failed";
      return false;
    }
    out.write(device_id_.data(), static_cast<std::streamsize>(device_id_.size()));
    out.close();
    if (!out) {
      last_error_ = "device id write failed";
      std::filesystem::remove(tmp, ec);
      return false;
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    std::filesystem::remove(tmp, ec);
    last_error_ = "device id write failed";
    return false;
  }
  return true;
}

bool ClientCore::LoadDeviceSyncKey() {
  device_sync_key_loaded_ = false;
  device_sync_key_.fill(0);
  if (!device_sync_enabled_) {
    return true;
  }
  if (device_sync_key_path_.empty()) {
    last_error_ = "device sync key path empty";
    return false;
  }

  std::error_code ec;
  if (std::filesystem::exists(device_sync_key_path_, ec)) {
    if (ec) {
      last_error_ = "device sync key path error";
      return false;
    }
    const auto size = std::filesystem::file_size(device_sync_key_path_, ec);
    if (ec) {
      last_error_ = "device sync key size stat failed";
      return false;
    }
    if (size > kMaxDeviceSyncKeyFileBytes) {
      last_error_ = "device sync key too large";
      return false;
    }
  } else if (ec) {
    last_error_ = "device sync key path error";
    return false;
  }

  std::vector<std::uint8_t> bytes;
  {
    std::ifstream f(device_sync_key_path_, std::ios::binary);
    if (f.is_open()) {
      bytes.assign(std::istreambuf_iterator<char>(f),
                   std::istreambuf_iterator<char>());
    }
  }

  if (!bytes.empty()) {
    std::vector<std::uint8_t> plain;
    bool was_dpapi = false;
    static constexpr char kMagic[] = "MI_E2EE_DEVICE_SYNC_KEY_DPAPI1";
    static constexpr char kEntropy[] = "MI_E2EE_DEVICE_SYNC_KEY_ENTROPY_V1";
    std::string dpapi_err;
    if (!MaybeUnprotectDpapi(bytes, kMagic, kEntropy, plain, was_dpapi,
                             dpapi_err)) {
      last_error_ =
          dpapi_err.empty() ? "device sync key unprotect failed" : dpapi_err;
      return false;
    }
    if (plain.size() != device_sync_key_.size()) {
      last_error_ = "device sync key size invalid";
      return false;
    }
    std::memcpy(device_sync_key_.data(), plain.data(), device_sync_key_.size());
    device_sync_key_loaded_ = true;

#ifdef _WIN32
    if (!was_dpapi) {
      std::vector<std::uint8_t> wrapped;
      std::string wrap_err;
      if (ProtectDpapi(plain, kMagic, kEntropy, wrapped, wrap_err)) {
        std::error_code ec;
        const auto tmp = device_sync_key_path_.string() + ".tmp";
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (out) {
          out.write(reinterpret_cast<const char*>(wrapped.data()),
                    static_cast<std::streamsize>(wrapped.size()));
          out.close();
          if (out) {
            std::filesystem::rename(tmp, device_sync_key_path_, ec);
            if (ec) {
              std::filesystem::remove(tmp, ec);
            }
          } else {
            std::filesystem::remove(tmp, ec);
          }
        }
      }
    }
#endif
    return true;
  }

  if (!device_sync_is_primary_) {
    last_error_ = "device sync key missing (linked device)";
    return false;
  }

  std::array<std::uint8_t, 32> k{};
  if (!RandomBytes(k.data(), k.size())) {
    last_error_ = "rng failed";
    return false;
  }
  return StoreDeviceSyncKey(k);
}

bool ClientCore::StoreDeviceSyncKey(const std::array<std::uint8_t, 32>& key) {
  last_error_.clear();
  if (!device_sync_enabled_) {
    last_error_ = "device sync disabled";
    return false;
  }
  if (device_sync_key_path_.empty()) {
    last_error_ = "device sync key path empty";
    return false;
  }
  if (IsAllZero(key.data(), key.size())) {
    last_error_ = "device sync key invalid";
    return false;
  }

  std::error_code ec;
  const auto dir = device_sync_key_path_.has_parent_path()
                       ? device_sync_key_path_.parent_path()
                       : std::filesystem::path{};
  if (!dir.empty()) {
    std::filesystem::create_directories(dir, ec);
  }

  std::vector<std::uint8_t> plain(key.begin(), key.end());
  std::vector<std::uint8_t> out_bytes = plain;
#ifdef _WIN32
  static constexpr char kMagic[] = "MI_E2EE_DEVICE_SYNC_KEY_DPAPI1";
  static constexpr char kEntropy[] = "MI_E2EE_DEVICE_SYNC_KEY_ENTROPY_V1";
  std::string wrap_err;
  std::vector<std::uint8_t> wrapped;
  if (!ProtectDpapi(plain, kMagic, kEntropy, wrapped, wrap_err)) {
    last_error_ = wrap_err.empty() ? "device sync key protect failed" : wrap_err;
    return false;
  }
  out_bytes = std::move(wrapped);
#endif

  const auto tmp = device_sync_key_path_.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      last_error_ = "device sync key write failed";
      return false;
    }
    out.write(reinterpret_cast<const char*>(out_bytes.data()),
              static_cast<std::streamsize>(out_bytes.size()));
    out.close();
    if (!out) {
      last_error_ = "device sync key write failed";
      std::filesystem::remove(tmp, ec);
      return false;
    }
  }

  std::filesystem::rename(tmp, device_sync_key_path_, ec);
  if (ec) {
    std::error_code ec2;
    std::filesystem::remove(device_sync_key_path_, ec2);
    ec.clear();
    std::filesystem::rename(tmp, device_sync_key_path_, ec);
  }
  if (ec) {
    std::filesystem::remove(tmp, ec);
    last_error_ = "device sync key write failed";
    return false;
  }

  if (!IsAllZero(device_sync_key_.data(), device_sync_key_.size())) {
    crypto_wipe(device_sync_key_.data(), device_sync_key_.size());
  }
  device_sync_key_ = key;
  device_sync_key_loaded_ = true;
  return true;
}

bool ClientCore::EncryptDeviceSync(const std::vector<std::uint8_t>& plaintext,
                                   std::vector<std::uint8_t>& out_cipher) {
  out_cipher.clear();
  if (!device_sync_enabled_) {
    last_error_ = "device sync disabled";
    return false;
  }
  if (!device_sync_key_loaded_) {
    last_error_ = "device sync key missing";
    return false;
  }
  if (plaintext.empty()) {
    last_error_ = "device sync plaintext empty";
    return false;
  }

  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'S', 'Y'};
  static constexpr std::uint8_t kVer = 1;
  std::uint8_t ad[5];
  std::memcpy(ad + 0, kMagic, sizeof(kMagic));
  ad[4] = kVer;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    last_error_ = "rng failed";
    return false;
  }

  out_cipher.resize(sizeof(ad) + nonce.size() + 16 + plaintext.size());
  std::memcpy(out_cipher.data(), ad, sizeof(ad));
  std::memcpy(out_cipher.data() + sizeof(ad), nonce.data(), nonce.size());
  std::uint8_t* mac = out_cipher.data() + sizeof(ad) + nonce.size();
  std::uint8_t* cipher = mac + 16;

  crypto_aead_lock(cipher, mac, device_sync_key_.data(), nonce.data(), ad,
                   sizeof(ad), plaintext.data(), plaintext.size());
  return true;
}

bool ClientCore::DecryptDeviceSync(const std::vector<std::uint8_t>& cipher,
                                   std::vector<std::uint8_t>& out_plaintext) {
  out_plaintext.clear();
  if (!device_sync_enabled_) {
    last_error_ = "device sync disabled";
    return false;
  }
  if (!device_sync_key_loaded_) {
    last_error_ = "device sync key missing";
    return false;
  }
  if (cipher.size() < (5 + 24 + 16 + 1)) {
    last_error_ = "device sync cipher invalid";
    return false;
  }
  static constexpr std::uint8_t kMagic[4] = {'M', 'I', 'S', 'Y'};
  if (std::memcmp(cipher.data(), kMagic, sizeof(kMagic)) != 0) {
    last_error_ = "device sync magic mismatch";
    return false;
  }
  if (cipher[4] != 1) {
    last_error_ = "device sync version mismatch";
    return false;
  }

  const std::uint8_t* ad = cipher.data();
  static constexpr std::size_t kAdSize = 5;
  const std::uint8_t* nonce = cipher.data() + kAdSize;
  const std::uint8_t* mac = nonce + 24;
  const std::uint8_t* ctext = mac + 16;
  const std::size_t ctext_len = cipher.size() - kAdSize - 24 - 16;

  out_plaintext.resize(ctext_len);
  const int rc = crypto_aead_unlock(out_plaintext.data(), mac,
                                    device_sync_key_.data(), nonce, ad, kAdSize,
                                    ctext, ctext_len);
  if (rc != 0) {
    out_plaintext.clear();
    last_error_ = "device sync auth failed";
    return false;
  }
  return true;
}

bool ClientCore::PushDeviceSyncCiphertext(const std::vector<std::uint8_t>& cipher) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (!device_sync_enabled_) {
    last_error_ = "device sync disabled";
    return false;
  }
  if (!LoadOrCreateDeviceId()) {
    if (last_error_.empty()) {
      last_error_ = "device id unavailable";
    }
    return false;
  }
  if (cipher.empty()) {
    last_error_ = "payload empty";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(device_id_, plain);
  mi::server::proto::WriteBytes(cipher, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kDeviceSyncPush, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "device sync push failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "device sync push response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "device sync push failed" : server_err;
    return false;
  }
  return true;
}

std::vector<std::vector<std::uint8_t>> ClientCore::PullDeviceSyncCiphertexts() {
  std::vector<std::vector<std::uint8_t>> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }
  if (!device_sync_enabled_) {
    last_error_ = "device sync disabled";
    return out;
  }
  if (!LoadOrCreateDeviceId()) {
    if (last_error_.empty()) {
      last_error_ = "device id unavailable";
    }
    return out;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(device_id_, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kDeviceSyncPull, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "device sync pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "device sync pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "device sync pull failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "device sync pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::vector<std::uint8_t> msg;
    if (!mi::server::proto::ReadBytes(resp_payload, off, msg)) {
      out.clear();
      last_error_ = "device sync pull response invalid";
      return out;
    }
    out.push_back(std::move(msg));
  }
  if (off != resp_payload.size()) {
    out.clear();
    last_error_ = "device sync pull response invalid";
    return out;
  }
  return out;
}

void ClientCore::BestEffortBroadcastDeviceSyncMessage(
    bool is_group, bool outgoing, const std::string& conv_id,
    const std::string& sender, const std::vector<std::uint8_t>& envelope) {
  if (!device_sync_enabled_ || !device_sync_is_primary_) {
    return;
  }

  const std::string saved_err = last_error_;
  if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
    last_error_ = saved_err;
    return;
  }

  std::vector<std::uint8_t> event_plain;
  if (!EncodeDeviceSyncMessage(is_group, outgoing, conv_id, sender, envelope,
                               event_plain)) {
    last_error_ = saved_err;
    return;
  }

  std::vector<std::uint8_t> event_cipher;
  if (!EncryptDeviceSync(event_plain, event_cipher)) {
    last_error_ = saved_err;
    return;
  }
  PushDeviceSyncCiphertext(event_cipher);
  last_error_ = saved_err;
}

void ClientCore::BestEffortBroadcastDeviceSyncDelivery(
    bool is_group, const std::string& conv_id,
    const std::array<std::uint8_t, 16>& msg_id, bool is_read) {
  if (!device_sync_enabled_ || !device_sync_is_primary_) {
    return;
  }

  const std::string saved_err = last_error_;
  if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
    last_error_ = saved_err;
    return;
  }

  std::vector<std::uint8_t> event_plain;
  if (!EncodeDeviceSyncDelivery(is_group, is_read, conv_id, msg_id, event_plain)) {
    last_error_ = saved_err;
    return;
  }

  std::vector<std::uint8_t> event_cipher;
  if (!EncryptDeviceSync(event_plain, event_cipher)) {
    last_error_ = saved_err;
    return;
  }
  PushDeviceSyncCiphertext(event_cipher);
  last_error_ = saved_err;
}

void ClientCore::BestEffortBroadcastDeviceSyncHistorySnapshot(
    const std::string& target_device_id) {
  if (!device_sync_enabled_ || !device_sync_is_primary_) {
    return;
  }
  if (target_device_id.empty()) {
    return;
  }
  if (!history_store_) {
    return;
  }

  const std::string saved_err = last_error_;
  if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
    last_error_ = saved_err;
    return;
  }

  std::vector<ChatHistoryMessage> msgs;
  std::string hist_err;
  if (!history_store_->ExportRecentSnapshot(20, 50, msgs, hist_err) ||
      msgs.empty()) {
    last_error_ = saved_err;
    return;
  }

  static constexpr std::size_t kMaxPlain = 200u * 1024u;
  std::size_t idx = 0;
  while (idx < msgs.size()) {
    std::vector<std::uint8_t> event_plain;
    event_plain.push_back(kDeviceSyncEventHistorySnapshot);
    mi::server::proto::WriteString(target_device_id, event_plain);
    const std::size_t count_pos = event_plain.size();
    mi::server::proto::WriteUint32(0, event_plain);

    std::uint32_t count = 0;
    while (idx < msgs.size()) {
      std::vector<std::uint8_t> entry;
      if (!EncodeHistorySnapshotEntry(msgs[idx], entry)) {
        ++idx;
        continue;
      }
      if (event_plain.size() + entry.size() > kMaxPlain) {
        if (count == 0) {
          ++idx;
        }
        break;
      }
      event_plain.insert(event_plain.end(), entry.begin(), entry.end());
      ++count;
      ++idx;
    }

    if (count == 0) {
      continue;
    }
    event_plain[count_pos + 0] = static_cast<std::uint8_t>(count & 0xFF);
    event_plain[count_pos + 1] = static_cast<std::uint8_t>((count >> 8) & 0xFF);
    event_plain[count_pos + 2] =
        static_cast<std::uint8_t>((count >> 16) & 0xFF);
    event_plain[count_pos + 3] =
        static_cast<std::uint8_t>((count >> 24) & 0xFF);

    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      break;
    }
    if (!PushDeviceSyncCiphertext(event_cipher)) {
      break;
    }
  }

  last_error_ = saved_err;
}

void ClientCore::BestEffortPersistHistoryEnvelope(
    bool is_group,
    bool outgoing,
    const std::string& conv_id,
    const std::string& sender,
    const std::vector<std::uint8_t>& envelope,
    HistoryStatus status,
    std::uint64_t timestamp_sec) {
  if (!history_store_) {
    return;
  }
  const std::string saved_err = last_error_;
  std::string hist_err;
  (void)history_store_->AppendEnvelope(
      is_group, outgoing, conv_id, sender, envelope,
      static_cast<ChatHistoryStatus>(status), timestamp_sec, hist_err);
  last_error_ = saved_err;
}

void ClientCore::BestEffortPersistHistoryStatus(
    bool is_group,
    const std::string& conv_id,
    const std::array<std::uint8_t, 16>& msg_id,
    HistoryStatus status,
    std::uint64_t timestamp_sec) {
  if (!history_store_) {
    return;
  }
  const std::string saved_err = last_error_;
  std::string hist_err;
  (void)history_store_->AppendStatusUpdate(
      is_group, conv_id, msg_id, static_cast<ChatHistoryStatus>(status),
      timestamp_sec, hist_err);
  last_error_ = saved_err;
}

void ClientCore::BestEffortStoreAttachmentPreviewBytes(
    const std::string& file_id,
    const std::string& file_name,
    std::uint64_t file_size,
    const std::vector<std::uint8_t>& bytes) {
  if (!history_store_ || file_id.empty() || bytes.empty()) {
    return;
  }
  const std::string saved_err = last_error_;
  const std::size_t max_bytes = 256u * 1024u;
  const std::size_t take = std::min(bytes.size(), max_bytes);
  if (take == 0) {
    return;
  }
  std::vector<std::uint8_t> preview(bytes.begin(), bytes.begin() + take);
  std::string hist_err;
  (void)history_store_->StoreAttachmentPreview(file_id, file_name, file_size,
                                               preview, hist_err);
  crypto_wipe(preview.data(), preview.size());
  last_error_ = saved_err;
}

void ClientCore::BestEffortStoreAttachmentPreviewFromPath(
    const std::string& file_id,
    const std::string& file_name,
    std::uint64_t file_size,
    const std::filesystem::path& path) {
  if (!history_store_ || file_id.empty() || path.empty()) {
    return;
  }
  const std::string saved_err = last_error_;
  const std::size_t max_bytes = 256u * 1024u;
  std::size_t want = max_bytes;
  if (file_size > 0 &&
      file_size <= static_cast<std::uint64_t>(
                       (std::numeric_limits<std::size_t>::max)())) {
    want = std::min<std::size_t>(max_bytes, static_cast<std::size_t>(file_size));
  }
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    last_error_ = saved_err;
    return;
  }
  std::vector<std::uint8_t> preview;
  preview.resize(want);
  ifs.read(reinterpret_cast<char*>(preview.data()),
           static_cast<std::streamsize>(preview.size()));
  const std::streamsize got = ifs.gcount();
  if (got <= 0) {
    crypto_wipe(preview.data(), preview.size());
    last_error_ = saved_err;
    return;
  }
  preview.resize(static_cast<std::size_t>(got));
  std::string hist_err;
  (void)history_store_->StoreAttachmentPreview(file_id, file_name, file_size,
                                               preview, hist_err);
  crypto_wipe(preview.data(), preview.size());
  last_error_ = saved_err;
}

void ClientCore::WarmupHistoryOnStartup() {
  if (!history_store_) {
    return;
  }
  const std::string saved_err = last_error_;
  std::vector<ChatHistoryMessage> msgs;
  std::string hist_err;
  (void)history_store_->ExportRecentSnapshot(20, 50, msgs, hist_err);
  last_error_ = saved_err;
}

void ClientCore::FlushHistoryOnShutdown() {
  if (!history_store_) {
    return;
  }
  const std::string saved_err = last_error_;
  std::string hist_err;
  (void)history_store_->Flush(hist_err);
  last_error_ = saved_err;
}

bool ClientCore::EnsurePreKeyPublished() {
  if (!EnsureE2ee()) {
    return false;
  }
  bool rotated = false;
  std::string rotate_err;
  if (!e2ee_.MaybeRotatePreKeys(rotated, rotate_err)) {
    last_error_ = rotate_err.empty() ? "prekey rotation failed" : rotate_err;
    return false;
  }
  if (rotated) {
    prekey_published_ = false;
  }
  if (prekey_published_) {
    return true;
  }
  if (!PublishPreKeyBundle()) {
    return false;
  }
  prekey_published_ = true;
  return true;
}

bool ClientCore::MaybeSendCoverTraffic() {
  if (!cover_traffic_enabled_ || cover_traffic_interval_sec_ == 0) {
    return true;
  }
  const auto now = std::chrono::steady_clock::now();
  if (cover_traffic_last_sent_.time_since_epoch().count() != 0 &&
      now - cover_traffic_last_sent_ <
          std::chrono::seconds(cover_traffic_interval_sec_)) {
    return true;
  }
  std::vector<std::uint8_t> payload;
  std::string pad_err;
  if (!PadPayload({}, payload, pad_err)) {
    return false;
  }
  const std::string saved_err = last_error_;
  std::vector<std::uint8_t> ignore;
  const bool ok = ProcessEncrypted(mi::server::FrameType::kHeartbeat, payload,
                                   ignore);
  last_error_ = saved_err;
  if (ok) {
    cover_traffic_last_sent_ = now;
  }
  return ok;
}

bool ClientCore::FetchPreKeyBundle(const std::string& peer_username,
                                  std::vector<std::uint8_t>& out_bundle) {
  out_bundle.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(peer_username, plain);
  mi::server::proto::WriteUint64(kt_tree_size_, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kPreKeyFetch, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "prekey fetch failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "prekey response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, err);
    last_error_ = err.empty() ? "prekey fetch failed" : err;
    return false;
  }
  std::size_t off = 1;
  if (!mi::server::proto::ReadBytes(resp_payload, off, out_bundle)) {
    last_error_ = "prekey response invalid";
    out_bundle.clear();
    return false;
  }
  if (off < resp_payload.size()) {
    std::uint32_t kt_version = 0;
    if (!mi::server::proto::ReadUint32(resp_payload, off, kt_version)) {
      last_error_ = "kt response invalid";
      return false;
    }
    if (kt_version == 1) {
      std::uint64_t tree_size = 0;
      std::vector<std::uint8_t> root_bytes;
      std::uint64_t leaf_index = 0;
      std::uint32_t audit_count = 0;
      std::uint32_t cons_count = 0;
      if (!mi::server::proto::ReadUint64(resp_payload, off, tree_size) ||
          !mi::server::proto::ReadBytes(resp_payload, off, root_bytes) ||
          !mi::server::proto::ReadUint64(resp_payload, off, leaf_index) ||
          !mi::server::proto::ReadUint32(resp_payload, off, audit_count)) {
        last_error_ = "kt response invalid";
        return false;
      }
      if (root_bytes.size() != 32 || tree_size == 0 ||
          leaf_index >= tree_size) {
        last_error_ = "kt response invalid";
        return false;
      }

      std::vector<mi::server::Sha256Hash> audit_path;
      audit_path.reserve(audit_count);
      for (std::uint32_t i = 0; i < audit_count; ++i) {
        std::vector<std::uint8_t> node;
        if (!mi::server::proto::ReadBytes(resp_payload, off, node) ||
            node.size() != 32) {
          last_error_ = "kt response invalid";
          return false;
        }
        mi::server::Sha256Hash h{};
        std::copy_n(node.begin(), h.size(), h.begin());
        audit_path.push_back(h);
      }
      if (!mi::server::proto::ReadUint32(resp_payload, off, cons_count)) {
        last_error_ = "kt response invalid";
        return false;
      }
      std::vector<mi::server::Sha256Hash> cons_path;
      cons_path.reserve(cons_count);
      for (std::uint32_t i = 0; i < cons_count; ++i) {
        std::vector<std::uint8_t> node;
        if (!mi::server::proto::ReadBytes(resp_payload, off, node) ||
            node.size() != 32) {
          last_error_ = "kt response invalid";
          return false;
        }
        mi::server::Sha256Hash h{};
        std::copy_n(node.begin(), h.size(), h.begin());
        cons_path.push_back(h);
      }
      std::vector<std::uint8_t> sth_sig;
      if (!mi::server::proto::ReadBytes(resp_payload, off, sth_sig)) {
        last_error_ = "kt response invalid";
        return false;
      }
      if (off != resp_payload.size()) {
        last_error_ = "kt response invalid";
        return false;
      }

      mi::server::Sha256Hash root{};
      std::copy_n(root_bytes.begin(), root.size(), root.begin());

      std::string leaf_err;
      const auto leaf_hash = KtLeafHashFromBundle(peer_username, out_bundle, leaf_err);
      if (!leaf_err.empty()) {
        last_error_ = leaf_err;
        return false;
      }
      mi::server::Sha256Hash computed_root{};
      if (!RootFromAuditPath(leaf_hash, static_cast<std::size_t>(leaf_index),
                             static_cast<std::size_t>(tree_size), audit_path,
                             computed_root) ||
          computed_root != root) {
        RecordKtGossipMismatch("kt inclusion proof invalid");
        return false;
      }

      if (kt_tree_size_ > 0) {
        if (tree_size < kt_tree_size_) {
          RecordKtGossipMismatch("kt tree rolled back");
          return false;
        }
        if (tree_size == kt_tree_size_) {
          if (root != kt_root_) {
            RecordKtGossipMismatch("kt split view");
            return false;
          }
        } else {
          if (!VerifyConsistencyProof(static_cast<std::size_t>(kt_tree_size_),
                                      static_cast<std::size_t>(tree_size),
                                      kt_root_, root, cons_path)) {
            RecordKtGossipMismatch("kt consistency proof invalid");
            return false;
          }
        }
      }

      if (kt_require_signature_) {
        if (!kt_root_pubkey_loaded_) {
          last_error_ = "kt root pubkey missing";
          return false;
        }
        if (sth_sig.size() != mi::server::kKtSthSigBytes) {
          RecordKtGossipMismatch("kt signature size invalid");
          return false;
        }
        mi::server::KeyTransparencySth sth;
        sth.tree_size = tree_size;
        sth.root = root;
        sth.signature = sth_sig;
        const auto sig_msg = mi::server::BuildKtSthSignatureMessage(sth);
        std::string sig_err;
        if (!mi::client::e2ee::Engine::VerifyDetached(sig_msg, sth_sig,
                                                      kt_root_pubkey_, sig_err)) {
          RecordKtGossipMismatch(sig_err.empty() ? "kt signature invalid" : sig_err);
          return false;
        }
      }
      kt_gossip_mismatch_count_ = 0;
      kt_gossip_alerted_ = false;
      kt_tree_size_ = tree_size;
      kt_root_ = root;
      SaveKtState();
      return true;
    }
    last_error_ = "kt version unsupported";
    return false;
  }
  return true;
}

bool ClientCore::GetPeerIdentityCached(const std::string& peer_username,
                                       CachedPeerIdentity& out,
                                       bool require_trust) {
  out = CachedPeerIdentity{};
  if (!EnsureE2ee()) {
    return false;
  }
  auto it = peer_id_cache_.find(peer_username);
  if (it != peer_id_cache_.end()) {
    out = it->second;
    if (!require_trust) {
      return true;
    }
    std::string trust_err;
    if (!e2ee_.EnsurePeerTrusted(peer_username, out.fingerprint_hex, trust_err)) {
      last_error_ = trust_err.empty() ? "peer not trusted" : trust_err;
      return false;
    }
    return true;
  }

  std::vector<std::uint8_t> bundle;
  if (!FetchPreKeyBundle(peer_username, bundle)) {
    return false;
  }

  std::vector<std::uint8_t> id_sig_pk;
  std::array<std::uint8_t, 32> id_dh_pk{};
  std::string fingerprint;
  std::string parse_err;
  if (!e2ee_.ExtractPeerIdentityFromBundle(bundle, id_sig_pk, id_dh_pk,
                                          fingerprint, parse_err)) {
    last_error_ = parse_err.empty() ? "bundle parse failed" : parse_err;
    return false;
  }

  if (require_trust) {
    std::string trust_err;
    if (!e2ee_.EnsurePeerTrusted(peer_username, fingerprint, trust_err)) {
      last_error_ = trust_err.empty() ? "peer not trusted" : trust_err;
      return false;
    }
  }

  CachedPeerIdentity entry;
  entry.id_sig_pk = std::move(id_sig_pk);
  entry.id_dh_pk = id_dh_pk;
  entry.fingerprint_hex = std::move(fingerprint);
  peer_id_cache_[peer_username] = entry;
  out = entry;
  return true;
}

bool ClientCore::EnsureGroupSenderKeyForSend(
    const std::string& group_id, const std::vector<std::string>& members,
    GroupSenderKeyState*& out_sender_key, std::string& out_warn) {
  out_sender_key = nullptr;
  out_warn.clear();
  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  if (members.empty()) {
    last_error_ = "group member list empty";
    return false;
  }

  const std::string sender_key_map_key =
      MakeGroupSenderKeyMapKey(group_id, username_);
  auto& sender_key = group_sender_keys_[sender_key_map_key];
  if (sender_key.group_id.empty()) {
    sender_key.group_id = group_id;
    sender_key.sender_username = username_;
  }

  const std::string members_hash = HashGroupMembers(members);
  const bool have_key = (sender_key.version != 0 &&
                         !IsAllZero(sender_key.ck.data(), sender_key.ck.size()));
  const std::uint64_t now_sec = NowUnixSeconds();
  if (have_key && sender_key.rotated_at == 0) {
    sender_key.rotated_at = now_sec;
  }
  const bool membership_changed =
      (!sender_key.members_hash.empty() && sender_key.members_hash != members_hash);
  const bool threshold_reached =
      (sender_key.sent_count >= kGroupSenderKeyRotationThreshold);
  const bool time_window_reached =
      (have_key && sender_key.rotated_at != 0 &&
       now_sec > sender_key.rotated_at &&
       (now_sec - sender_key.rotated_at) >= kGroupSenderKeyRotationIntervalSec);

  if (!have_key || membership_changed || threshold_reached || time_window_reached) {
    const std::uint32_t next_version =
        have_key ? (sender_key.version + 1) : 1;
    if (!RandomBytes(sender_key.ck.data(), sender_key.ck.size())) {
      last_error_ = "rng failed";
      return false;
    }
    sender_key.version = next_version;
    sender_key.next_iteration = 0;
    sender_key.members_hash = members_hash;
    sender_key.rotated_at = now_sec;
    sender_key.sent_count = 0;
    sender_key.skipped_mks.clear();
    sender_key.skipped_order.clear();

    for (auto it = pending_sender_key_dists_.begin();
         it != pending_sender_key_dists_.end();) {
      if (it->second.group_id == group_id) {
        it = pending_sender_key_dists_.erase(it);
      } else {
        ++it;
      }
    }

    std::array<std::uint8_t, 16> dist_id{};
    if (!RandomBytes(dist_id.data(), dist_id.size())) {
      last_error_ = "rng failed";
      return false;
    }
    const std::string dist_id_hex = BytesToHexLower(dist_id.data(), dist_id.size());

    const auto sig_msg = BuildGroupSenderKeyDistSigMessage(
        group_id, sender_key.version, sender_key.next_iteration, sender_key.ck);
    std::vector<std::uint8_t> sig;
    std::string sig_err;
    if (!e2ee_.SignDetached(sig_msg, sig, sig_err)) {
      last_error_ = sig_err.empty() ? "sign sender key failed" : sig_err;
      return false;
    }

    std::vector<std::uint8_t> dist_envelope;
    if (!EncodeChatGroupSenderKeyDist(dist_id, group_id, sender_key.version,
                                      sender_key.next_iteration, sender_key.ck,
                                      sig, dist_envelope)) {
      last_error_ = "encode sender key failed";
      return false;
    }

    PendingSenderKeyDistribution pending;
    pending.group_id = group_id;
    pending.version = sender_key.version;
    pending.envelope = dist_envelope;
    pending.last_sent = std::chrono::steady_clock::now();
    for (const auto& m : members) {
      if (!username_.empty() && m == username_) {
        continue;
      }
      pending.pending_members.insert(m);
    }
    pending_sender_key_dists_[dist_id_hex] = std::move(pending);

    std::string first_error;
    for (const auto& m : members) {
      if (!username_.empty() && m == username_) {
        continue;
      }
      const std::string saved_err = last_error_;
      if (!SendGroupSenderKeyEnvelope(group_id, m, dist_envelope) &&
          first_error.empty()) {
        first_error = last_error_;
      }
      last_error_ = saved_err;
    }
    out_warn = first_error;
  }

  const auto now = std::chrono::steady_clock::now();
  for (auto& kv : pending_sender_key_dists_) {
    auto& pending = kv.second;
    if (pending.group_id != group_id || pending.pending_members.empty()) {
      continue;
    }
    if (now - pending.last_sent < kSenderKeyDistResendInterval) {
      continue;
    }
    pending.last_sent = now;
    for (const auto& m : pending.pending_members) {
      const std::string saved_err = last_error_;
      SendGroupSenderKeyEnvelope(pending.group_id, m, pending.envelope);
      last_error_ = saved_err;
    }
  }

  out_sender_key = &sender_key;
  return true;
}

bool ClientCore::StoreGroupCallKey(
    const std::string& group_id, const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id, const std::array<std::uint8_t, 32>& call_key) {
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  if (key_id == 0) {
    last_error_ = "key id invalid";
    return false;
  }
  if (IsAllZero(call_key.data(), call_key.size())) {
    last_error_ = "call key empty";
    return false;
  }
  const std::string map_key = MakeGroupCallKeyMapKey(group_id, call_id);
  auto& state = group_call_keys_[map_key];
  if (state.key_id != 0 && key_id < state.key_id) {
    return false;
  }
  state.group_id = group_id;
  state.call_id = call_id;
  state.key_id = key_id;
  state.call_key = call_key;
  state.updated_at = NowUnixSeconds();
  return true;
}

bool ClientCore::LookupGroupCallKey(
    const std::string& group_id, const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id, std::array<std::uint8_t, 32>& out_key) const {
  out_key.fill(0);
  if (group_id.empty() || key_id == 0) {
    return false;
  }
  const std::string map_key = MakeGroupCallKeyMapKey(group_id, call_id);
  const auto it = group_call_keys_.find(map_key);
  if (it == group_call_keys_.end()) {
    return false;
  }
  if (it->second.key_id != key_id ||
      IsAllZero(it->second.call_key.data(), it->second.call_key.size())) {
    return false;
  }
  out_key = it->second.call_key;
  return true;
}

bool ClientCore::SendGroupCallKeyEnvelope(
    const std::string& group_id, const std::string& peer_username,
    const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id,
    const std::array<std::uint8_t, 32>& call_key) {
  if (group_id.empty() || peer_username.empty()) {
    last_error_ = "invalid params";
    return false;
  }
  std::array<std::uint8_t, 16> dist_id{};
  if (!RandomBytes(dist_id.data(), dist_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  const auto sig_msg =
      BuildGroupCallKeyDistSigMessage(group_id, call_id, key_id, call_key);
  std::vector<std::uint8_t> sig;
  std::string sig_err;
  if (!e2ee_.SignDetached(sig_msg, sig, sig_err)) {
    last_error_ = sig_err.empty() ? "sign call key failed" : sig_err;
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatGroupCallKeyDist(dist_id, group_id, call_id, key_id, call_key,
                                  sig, envelope)) {
    last_error_ = "encode call key failed";
    return false;
  }
  return SendGroupSenderKeyEnvelope(group_id, peer_username, envelope);
}

bool ClientCore::SendGroupCallKeyRequest(
    const std::string& group_id, const std::string& peer_username,
    const std::array<std::uint8_t, 16>& call_id, std::uint32_t key_id) {
  if (group_id.empty() || peer_username.empty()) {
    last_error_ = "invalid params";
    return false;
  }
  std::array<std::uint8_t, 16> req_id{};
  if (!RandomBytes(req_id.data(), req_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> req;
  if (!EncodeChatGroupCallKeyReq(req_id, group_id, call_id, key_id, req)) {
    last_error_ = "encode call key req failed";
    return false;
  }
  return SendGroupSenderKeyEnvelope(group_id, peer_username, req);
}

void ClientCore::ResendPendingSenderKeyDistributions() {
  if (pending_sender_key_dists_.empty()) {
    return;
  }
  const auto now = std::chrono::steady_clock::now();
  for (auto it = pending_sender_key_dists_.begin();
       it != pending_sender_key_dists_.end();) {
    auto& pending = it->second;
    if (pending.pending_members.empty()) {
      it = pending_sender_key_dists_.erase(it);
      continue;
    }
    if (now - pending.last_sent < kSenderKeyDistResendInterval) {
      ++it;
      continue;
    }
    pending.last_sent = now;
    for (const auto& member : pending.pending_members) {
      const std::string saved_err = last_error_;
      SendGroupSenderKeyEnvelope(pending.group_id, member, pending.envelope);
      last_error_ = saved_err;
    }
    ++it;
  }
}

void ClientCore::RecordKtGossipMismatch(const std::string& reason) {
  if (kt_gossip_alert_threshold_ == 0) {
    kt_gossip_alert_threshold_ = 3;
  }
  if (kt_gossip_mismatch_count_ < (std::numeric_limits<std::uint32_t>::max)()) {
    kt_gossip_mismatch_count_++;
  }
  if (kt_gossip_mismatch_count_ >= kt_gossip_alert_threshold_) {
    kt_gossip_alerted_ = true;
    last_error_ = reason.empty() ? "kt gossip alert"
                                 : ("kt gossip alert: " + reason);
    return;
  }
  if (!reason.empty()) {
    last_error_ = reason;
  }
}

bool ClientCore::FetchKtConsistency(
    std::uint64_t old_size, std::uint64_t new_size,
    std::vector<std::array<std::uint8_t, 32>>& out_proof) {
  out_proof.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (old_size == 0 || new_size == 0 || old_size >= new_size) {
    last_error_ = "invalid kt sizes";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteUint64(old_size, plain);
  mi::server::proto::WriteUint64(new_size, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kKeyTransparencyConsistency, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "kt consistency failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "kt response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, err);
    last_error_ = err.empty() ? "kt consistency failed" : err;
    return false;
  }
  std::size_t off = 1;
  std::uint64_t got_old = 0;
  std::uint64_t got_new = 0;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint64(resp_payload, off, got_old) ||
      !mi::server::proto::ReadUint64(resp_payload, off, got_new) ||
      !mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "kt response invalid";
    return false;
  }
  out_proof.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::vector<std::uint8_t> node;
    if (!mi::server::proto::ReadBytes(resp_payload, off, node) ||
        node.size() != 32) {
      last_error_ = "kt response invalid";
      out_proof.clear();
      return false;
    }
    std::array<std::uint8_t, 32> h{};
    std::copy_n(node.begin(), h.size(), h.begin());
    out_proof.push_back(h);
  }
  if (off != resp_payload.size() || got_old != old_size || got_new != new_size) {
    last_error_ = "kt response invalid";
    out_proof.clear();
    return false;
  }
  return true;
}

bool ClientCore::ProcessRaw(const std::vector<std::uint8_t>& in_bytes,
                            std::vector<std::uint8_t>& out_bytes) {
  out_bytes.clear();
  if (in_bytes.empty()) {
    return false;
  }
  if (remote_mode_) {
    const auto set_remote_ok = [&](bool ok, const std::string& err) {
      remote_ok_ = ok;
      if (ok) {
        remote_error_.clear();
      } else {
        remote_error_ = err;
      }
    };

    std::lock_guard<std::mutex> lock(remote_stream_mutex_);
    if (!remote_stream_ ||
        !remote_stream_->Matches(server_ip_, server_port_, use_tls_, use_kcp_,
                                 kcp_cfg_, proxy_, pinned_server_fingerprint_)) {
      remote_stream_.reset();
      remote_stream_ = std::make_unique<RemoteStream>(
          server_ip_, server_port_, use_tls_, use_kcp_, kcp_cfg_, proxy_,
          pinned_server_fingerprint_);
      std::string fingerprint;
      std::string err;
      if (!remote_stream_->Connect(fingerprint, err)) {
        remote_stream_.reset();
        if (!fingerprint.empty()) {
          pending_server_fingerprint_ = fingerprint;
          pending_server_pin_ = FingerprintSas80Hex(fingerprint);
          last_error_ =
              pinned_server_fingerprint_.empty()
                  ? "server not trusted, confirm sas"
                  : "server fingerprint changed, confirm sas";
          set_remote_ok(false, last_error_);
          return false;
        }
        if (err.empty()) {
          if (use_kcp_) {
            last_error_ = "kcp connect failed";
          } else if (use_tls_) {
            last_error_ = "tls connect failed";
          } else {
            last_error_ = "tcp connect failed";
          }
        } else {
          last_error_ = err;
        }
        set_remote_ok(false, last_error_);
        return false;
      }
      pending_server_fingerprint_.clear();
      pending_server_pin_.clear();
    }

    std::string err;
    if (!remote_stream_->SendAndRecv(in_bytes, out_bytes, err)) {
      remote_stream_.reset();
      if (!err.empty()) {
        last_error_ = err;
      } else if (use_kcp_) {
        last_error_ = "kcp request failed";
      } else if (use_tls_) {
        last_error_ = "tls request failed";
      } else {
        last_error_ = "tcp request failed";
      }
      set_remote_ok(false, last_error_);
      return false;
    }
    set_remote_ok(true, {});
    return true;
  }
  remote_ok_ = true;
  remote_error_.clear();
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
  if (!channel_.Encrypt(send_seq_, type, plain, cipher)) {
    return false;
  }
  send_seq_++;

  mi::server::Frame f;
  f.type = type;
  f.payload.reserve(2 + token_.size() + cipher.size());
  mi::server::proto::WriteString(token_, f.payload);
  f.payload.insert(f.payload.end(), cipher.begin(), cipher.end());
  const auto bytes = mi::server::EncodeFrame(f);

  std::vector<std::uint8_t> resp_vec;
  if (!ProcessRaw(bytes, resp_vec)) {
    return false;
  }

  mi::server::FrameView resp_view;
  if (!mi::server::DecodeFrameView(resp_vec.data(), resp_vec.size(),
                                   resp_view)) {
    if (last_error_.empty()) {
      last_error_ = "invalid server response";
    }
    return false;
  }
  const mi::server::proto::ByteView payload_view{resp_view.payload,
                                                 resp_view.payload_len};
  std::size_t off = 0;
  std::string_view resp_token;
  if (!mi::server::proto::ReadStringView(payload_view, off, resp_token)) {
    if (resp_view.type == mi::server::FrameType::kLogout) {
      std::string server_err;
      if (payload_view.data && payload_view.size > 1) {
        std::size_t off2 = 1;
        std::string_view err_view;
        if (mi::server::proto::ReadStringView(payload_view, off2, err_view)) {
          server_err.assign(err_view.begin(), err_view.end());
        }
      }
      last_error_ = server_err.empty() ? "session invalid" : server_err;
      token_.clear();
      prekey_published_ = false;
      return false;
    }
    if (last_error_.empty()) {
      last_error_ = "invalid server response";
    }
    return false;
  }
  if (resp_token != token_) {
    last_error_ = "session invalid";
    token_.clear();
    prekey_published_ = false;
    return false;
  }
  const std::size_t cipher_len =
      payload_view.size >= off ? payload_view.size - off : 0;
  const std::uint8_t* cipher_ptr =
      payload_view.data ? payload_view.data + off : nullptr;
  if (!channel_.Decrypt(cipher_ptr, cipher_len, resp_view.type, out_plain)) {
    if (last_error_.empty()) {
      last_error_ = "decrypt failed";
    }
    return false;
  }
  return true;
}

bool ClientCore::Heartbeat() {
  last_error_.clear();
  std::vector<std::uint8_t> ignore;
  if (!ProcessEncrypted(mi::server::FrameType::kHeartbeat, {}, ignore)) {
    if (last_error_.empty()) {
      last_error_ = "heartbeat failed";
    }
    return false;
  }
  return true;
}

std::vector<ClientCore::DeviceEntry> ClientCore::ListDevices() {
  std::vector<DeviceEntry> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }
  if (device_id_.empty()) {
    LoadOrCreateDeviceId();
  }
  if (device_id_.empty()) {
    last_error_ = "device id unavailable";
    return out;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(device_id_, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kDeviceList, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "device list failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "device list response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "device list failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "device list response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::string dev;
    std::uint32_t age = 0;
    if (!mi::server::proto::ReadString(resp_payload, off, dev) ||
        !mi::server::proto::ReadUint32(resp_payload, off, age)) {
      last_error_ = "device list response invalid";
      out.clear();
      return out;
    }
    DeviceEntry e;
    e.device_id = std::move(dev);
    e.last_seen_sec = age;
    out.push_back(std::move(e));
  }
  if (off != resp_payload.size()) {
    last_error_ = "device list response invalid";
    out.clear();
    return out;
  }
  return out;
}

bool ClientCore::KickDevice(const std::string& target_device_id) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (device_id_.empty()) {
    LoadOrCreateDeviceId();
  }
  if (device_id_.empty()) {
    last_error_ = "device id unavailable";
    return false;
  }
  if (target_device_id.empty()) {
    last_error_ = "device id empty";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(device_id_, plain);
  mi::server::proto::WriteString(target_device_id, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kDeviceKick, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "device kick failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "device kick response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "device kick failed" : server_err;
    return false;
  }
  if (resp_payload.size() != 1) {
    last_error_ = "device kick response invalid";
    return false;
  }

  if (device_sync_enabled_) {
    if (!device_sync_key_loaded_) {
      LoadDeviceSyncKey();
    }
    if (device_sync_key_loaded_) {
      std::array<std::uint8_t, 32> next_key{};
      if (RandomBytes(next_key.data(), next_key.size())) {
        std::vector<std::uint8_t> event_plain;
        if (EncodeDeviceSyncRotateKey(next_key, event_plain)) {
          std::vector<std::uint8_t> event_cipher;
          if (EncryptDeviceSync(event_plain, event_cipher) &&
              PushDeviceSyncCiphertext(event_cipher)) {
            StoreDeviceSyncKey(next_key);
          }
        }
      }
      last_error_.clear();
    }
  }
  return true;
}

bool ClientCore::BeginDevicePairingPrimary(std::string& out_pairing_code) {
  out_pairing_code.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (!device_sync_enabled_) {
    last_error_ = "device sync disabled";
    return false;
  }
  if (!device_sync_is_primary_) {
    last_error_ = "not primary device";
    return false;
  }
  if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
    return false;
  }
  if (!device_sync_key_loaded_) {
    last_error_ = "device sync key missing";
    return false;
  }

  std::array<std::uint8_t, 16> secret{};
  if (!RandomBytes(secret.data(), secret.size())) {
    last_error_ = "rng failed";
    return false;
  }

  std::string pairing_id;
  std::array<std::uint8_t, 32> key{};
  if (!DerivePairingIdAndKey(secret, pairing_id, key)) {
    last_error_ = "pairing derive failed";
    crypto_wipe(secret.data(), secret.size());
    return false;
  }

  out_pairing_code = GroupHex4(BytesToHexLower(secret.data(), secret.size()));
  crypto_wipe(secret.data(), secret.size());

  pairing_active_ = true;
  pairing_is_primary_ = true;
  pairing_wait_response_ = false;
  pairing_id_hex_ = std::move(pairing_id);
  pairing_key_ = key;
  pairing_request_id_.fill(0);
  return true;
}

std::vector<ClientCore::DevicePairingRequest> ClientCore::PollDevicePairingRequests() {
  std::vector<DevicePairingRequest> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }
  if (!pairing_active_ || !pairing_is_primary_ || pairing_id_hex_.empty() ||
      IsAllZero(pairing_key_.data(), pairing_key_.size())) {
    last_error_ = "pairing not active";
    return out;
  }

  std::vector<std::uint8_t> plain;
  plain.push_back(0);  // pull requests
  mi::server::proto::WriteString(pairing_id_hex_, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kDevicePairingPull, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "pairing pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "pairing pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "pairing pull failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "pairing pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::vector<std::uint8_t> msg;
    if (!mi::server::proto::ReadBytes(resp_payload, off, msg)) {
      out.clear();
      last_error_ = "pairing pull response invalid";
      return out;
    }
    std::vector<std::uint8_t> plain_msg;
    if (!DecryptPairingPayload(pairing_key_, msg, plain_msg)) {
      continue;
    }
    std::string device_id;
    std::array<std::uint8_t, 16> request_id{};
    if (!DecodePairingRequestPlain(plain_msg, device_id, request_id)) {
      continue;
    }
    if (device_id.empty() || device_id == device_id_) {
      continue;
    }
    DevicePairingRequest r;
    r.device_id = std::move(device_id);
    r.request_id_hex = BytesToHexLower(request_id.data(), request_id.size());
    out.push_back(std::move(r));
  }
  if (off != resp_payload.size()) {
    out.clear();
    last_error_ = "pairing pull response invalid";
    return out;
  }
  return out;
}

bool ClientCore::ApproveDevicePairingRequest(const DevicePairingRequest& request) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (!pairing_active_ || !pairing_is_primary_ || pairing_id_hex_.empty() ||
      IsAllZero(pairing_key_.data(), pairing_key_.size())) {
    last_error_ = "pairing not active";
    return false;
  }
  if (!device_sync_enabled_ || !device_sync_is_primary_) {
    last_error_ = "device sync not primary";
    return false;
  }
  if (request.device_id.empty() || request.request_id_hex.empty()) {
    last_error_ = "invalid request";
    return false;
  }
  if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
    return false;
  }
  if (!device_sync_key_loaded_) {
    last_error_ = "device sync key missing";
    return false;
  }

  std::array<std::uint8_t, 16> req_id{};
  if (!HexToFixedBytes16(NormalizeCode(request.request_id_hex), req_id)) {
    last_error_ = "invalid request id";
    return false;
  }

  std::vector<std::uint8_t> plain_resp;
  if (!EncodePairingResponsePlain(req_id, device_sync_key_, plain_resp)) {
    last_error_ = "pairing encode failed";
    return false;
  }

  std::vector<std::uint8_t> cipher_resp;
  if (!EncryptPairingPayload(pairing_key_, plain_resp, cipher_resp)) {
    last_error_ = "pairing encrypt failed";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(pairing_id_hex_, plain);
  mi::server::proto::WriteString(request.device_id, plain);
  mi::server::proto::WriteBytes(cipher_resp, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kDevicePairingRespond, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "pairing respond failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "pairing respond response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "pairing respond failed" : server_err;
    return false;
  }
  if (resp_payload.size() != 1) {
    last_error_ = "pairing respond response invalid";
    return false;
  }

  {
    const std::string saved_err = last_error_;
    BestEffortBroadcastDeviceSyncHistorySnapshot(request.device_id);
    last_error_ = saved_err;
  }
  CancelDevicePairing();
  return true;
}

bool ClientCore::BeginDevicePairingLinked(const std::string& pairing_code) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (!device_sync_enabled_) {
    last_error_ = "device sync disabled";
    return false;
  }
  if (device_sync_key_loaded_) {
    last_error_ = "device sync key already present";
    return false;
  }
  if (pairing_code.empty()) {
    last_error_ = "pairing code empty";
    return false;
  }

  std::array<std::uint8_t, 16> secret{};
  if (!ParsePairingCodeSecret16(pairing_code, secret)) {
    last_error_ = "pairing code invalid";
    return false;
  }
  std::string pairing_id;
  std::array<std::uint8_t, 32> key{};
  if (!DerivePairingIdAndKey(secret, pairing_id, key)) {
    crypto_wipe(secret.data(), secret.size());
    last_error_ = "pairing derive failed";
    return false;
  }
  crypto_wipe(secret.data(), secret.size());

  if (!LoadOrCreateDeviceId() || device_id_.empty()) {
    last_error_ = last_error_.empty() ? "device id unavailable" : last_error_;
    return false;
  }
  {
    const std::string saved_err = last_error_;
    (void)PullDeviceSyncCiphertexts();
    last_error_ = saved_err;
  }

  std::array<std::uint8_t, 16> request_id{};
  if (!RandomBytes(request_id.data(), request_id.size())) {
    last_error_ = "rng failed";
    return false;
  }

  std::vector<std::uint8_t> req_plain;
  if (!EncodePairingRequestPlain(device_id_, request_id, req_plain)) {
    last_error_ = "pairing encode failed";
    return false;
  }
  std::vector<std::uint8_t> req_cipher;
  if (!EncryptPairingPayload(key, req_plain, req_cipher)) {
    last_error_ = "pairing encrypt failed";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(pairing_id, plain);
  mi::server::proto::WriteBytes(req_cipher, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kDevicePairingRequest, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "pairing request failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "pairing request response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "pairing request failed" : server_err;
    return false;
  }
  if (resp_payload.size() != 1) {
    last_error_ = "pairing request response invalid";
    return false;
  }

  pairing_active_ = true;
  pairing_is_primary_ = false;
  pairing_wait_response_ = true;
  pairing_id_hex_ = std::move(pairing_id);
  pairing_key_ = key;
  pairing_request_id_ = request_id;
  return true;
}

bool ClientCore::PollDevicePairingLinked(bool& out_completed) {
  out_completed = false;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (!pairing_active_ || pairing_is_primary_ || !pairing_wait_response_ ||
      pairing_id_hex_.empty() ||
      IsAllZero(pairing_key_.data(), pairing_key_.size()) ||
      IsAllZero(pairing_request_id_.data(), pairing_request_id_.size())) {
    last_error_ = "pairing not pending";
    return false;
  }
  if (device_id_.empty()) {
    LoadOrCreateDeviceId();
  }
  if (device_id_.empty()) {
    last_error_ = "device id unavailable";
    return false;
  }

  std::vector<std::uint8_t> plain;
  plain.push_back(1);  // pull responses
  mi::server::proto::WriteString(pairing_id_hex_, plain);
  mi::server::proto::WriteString(device_id_, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kDevicePairingPull, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "pairing pull failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "pairing pull response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "pairing pull failed" : server_err;
    return false;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "pairing pull response invalid";
    return false;
  }

  for (std::uint32_t i = 0; i < count; ++i) {
    std::vector<std::uint8_t> msg;
    if (!mi::server::proto::ReadBytes(resp_payload, off, msg)) {
      last_error_ = "pairing pull response invalid";
      return false;
    }
    std::vector<std::uint8_t> plain_msg;
    if (!DecryptPairingPayload(pairing_key_, msg, plain_msg)) {
      continue;
    }
    std::array<std::uint8_t, 16> req_id{};
    std::array<std::uint8_t, 32> sync_key{};
    if (!DecodePairingResponsePlain(plain_msg, req_id, sync_key)) {
      continue;
    }
    if (req_id != pairing_request_id_) {
      continue;
    }
    if (!StoreDeviceSyncKey(sync_key)) {
      return false;
    }
    CancelDevicePairing();
    out_completed = true;
    return true;
  }
  if (off != resp_payload.size()) {
    last_error_ = "pairing pull response invalid";
    return false;
  }

  return true;
}

void ClientCore::CancelDevicePairing() {
  pairing_active_ = false;
  pairing_is_primary_ = false;
  pairing_wait_response_ = false;
  pairing_id_hex_.clear();
  if (!IsAllZero(pairing_key_.data(), pairing_key_.size())) {
    crypto_wipe(pairing_key_.data(), pairing_key_.size());
  }
  pairing_key_.fill(0);
  if (!IsAllZero(pairing_request_id_.data(), pairing_request_id_.size())) {
    crypto_wipe(pairing_request_id_.data(), pairing_request_id_.size());
  }
  pairing_request_id_.fill(0);
}

bool ClientCore::JoinGroup(const std::string& group_id) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  std::vector<std::uint8_t> plain;
  plain.push_back(0);  // join action
  mi::server::proto::WriteString(group_id, plain);
  std::vector<std::uint8_t> resp_plain;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupEvent, plain,
                        resp_plain)) {
    if (last_error_.empty()) {
      last_error_ = "join group failed";
    }
    return false;
  }
  if (resp_plain.empty()) {
    last_error_ = "join group response empty";
    return false;
  }
  if (resp_plain[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_plain, off, server_err);
    last_error_ = server_err.empty() ? "join group failed" : server_err;
    return false;
  }
  return true;
}

bool ClientCore::LeaveGroup(const std::string& group_id) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  std::vector<std::uint8_t> plain;
  plain.push_back(1);  // leave action
  mi::server::proto::WriteString(group_id, plain);
  std::vector<std::uint8_t> resp_plain;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupEvent, plain,
                        resp_plain)) {
    if (last_error_.empty()) {
      last_error_ = "leave group failed";
    }
    return false;
  }
  if (resp_plain.empty()) {
    last_error_ = "leave group response empty";
    return false;
  }
  if (resp_plain[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_plain, off, server_err);
    last_error_ = server_err.empty() ? "leave group failed" : server_err;
    return false;
  }
  return true;
}

std::vector<std::string> ClientCore::ListGroupMembers(
    const std::string& group_id) {
  std::vector<std::string> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return out;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupMemberList, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "group member list failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "group member list response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "group member list failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "group member list response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::string user;
    if (!mi::server::proto::ReadString(resp_payload, off, user)) {
      last_error_ = "group member list response invalid";
      out.clear();
      return out;
    }
    out.push_back(std::move(user));
  }
  if (off != resp_payload.size()) {
    last_error_ = "group member list response invalid";
    out.clear();
    return out;
  }
  return out;
}

std::vector<ClientCore::GroupMemberInfo> ClientCore::ListGroupMembersInfo(
    const std::string& group_id) {
  std::vector<GroupMemberInfo> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return out;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupMemberInfoList, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "group member info failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "group member info response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "group member info failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "group member info response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::string user;
    if (!mi::server::proto::ReadString(resp_payload, off, user) ||
        off >= resp_payload.size()) {
      last_error_ = "group member info response invalid";
      out.clear();
      return out;
    }
    const std::uint8_t role_u8 = resp_payload[off++];
    if (role_u8 > static_cast<std::uint8_t>(GroupMemberRole::kMember)) {
      last_error_ = "group member info response invalid";
      out.clear();
      return out;
    }
    GroupMemberInfo e;
    e.username = std::move(user);
    e.role = static_cast<GroupMemberRole>(role_u8);
    out.push_back(std::move(e));
  }
  if (off != resp_payload.size()) {
    last_error_ = "group member info response invalid";
    out.clear();
    return out;
  }
  return out;
}

bool ClientCore::SetGroupMemberRole(const std::string& group_id,
                                    const std::string& target_username,
                                    GroupMemberRole role) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty() || target_username.empty()) {
    last_error_ = "invalid params";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  mi::server::proto::WriteString(target_username, plain);
  plain.push_back(static_cast<std::uint8_t>(role));

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupRoleSet, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "group role set failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "group role set response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "group role set failed" : server_err;
    return false;
  }
  if (resp_payload.size() != 1) {
    last_error_ = "group role set response invalid";
    return false;
  }
  return true;
}

bool ClientCore::KickGroupMember(const std::string& group_id,
                                 const std::string& target_username) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty() || target_username.empty()) {
    last_error_ = "invalid params";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  mi::server::proto::WriteString(target_username, plain);
  std::vector<std::uint8_t> resp_plain;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupKickMember, plain,
                        resp_plain)) {
    if (last_error_.empty()) {
      last_error_ = "group kick failed";
    }
    return false;
  }
  if (resp_plain.empty()) {
    last_error_ = "group kick response empty";
    return false;
  }
  if (resp_plain[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_plain, off, server_err);
    last_error_ = server_err.empty() ? "group kick failed" : server_err;
    return false;
  }
  std::size_t off = 1;
  std::uint32_t version = 0;
  if (!mi::server::proto::ReadUint32(resp_plain, off, version) ||
      off >= resp_plain.size()) {
    last_error_ = "group kick response invalid";
    return false;
  }
  const std::uint8_t reason = resp_plain[off++];
  (void)version;
  (void)reason;
  if (off != resp_plain.size()) {
    last_error_ = "group kick response invalid";
    return false;
  }
  return true;
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

bool ClientCore::CreateGroup(std::string& out_group_id) {
  out_group_id.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }

  std::array<std::uint8_t, 16> group_id{};
  if (!RandomBytes(group_id.data(), group_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  out_group_id = BytesToHexLower(group_id.data(), group_id.size());
  if (out_group_id.empty()) {
    last_error_ = "group id generation failed";
    return false;
  }

  if (!JoinGroup(out_group_id)) {
    out_group_id.clear();
    if (last_error_.empty()) {
      last_error_ = "create group failed";
    }
    return false;
  }

  return true;
}

bool ClientCore::SendGroupChatText(const std::string& group_id,
                                   const std::string& text_utf8,
                                   std::string& out_message_id_hex) {
  out_message_id_hex.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }

    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    std::vector<std::uint8_t> plain_envelope;
    if (!EncodeChatGroupText(msg_id, group_id, text_utf8, plain_envelope)) {
      last_error_ = "encode group text failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendGroup(group_id, plain_envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryEnvelope(true, true, group_id, username_, plain_envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }

  const auto members = ListGroupMembers(group_id);
  if (members.empty()) {
    if (last_error_.empty()) {
      last_error_ = "group member list empty";
    }
    return false;
  }

  GroupSenderKeyState* sender_key = nullptr;
  std::string warn;
  if (!EnsureGroupSenderKeyForSend(group_id, members, sender_key, warn)) {
    return false;
  }
  if (!sender_key) {
    last_error_ = "sender key unavailable";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::vector<std::uint8_t> plain_envelope;
  if (!EncodeChatGroupText(msg_id, group_id, text_utf8, plain_envelope)) {
    last_error_ = "encode group text failed";
    return false;
  }
  std::vector<std::uint8_t> padded_envelope;
  std::string pad_err;
  if (!PadPayload(plain_envelope, padded_envelope, pad_err)) {
    last_error_ = pad_err.empty() ? "pad group message failed" : pad_err;
    return false;
  }

  std::array<std::uint8_t, 32> next_ck{};
  std::array<std::uint8_t, 32> mk{};
  if (!KdfGroupCk(sender_key->ck, next_ck, mk)) {
    last_error_ = "kdf failed";
    return false;
  }
  const std::uint32_t iter = sender_key->next_iteration;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> ad;
  BuildGroupCipherAd(group_id, username_, sender_key->version, iter, ad);

  std::vector<std::uint8_t> cipher;
  cipher.resize(padded_envelope.size());
  std::array<std::uint8_t, 16> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), mk.data(), nonce.data(), ad.data(),
                   ad.size(), padded_envelope.data(), padded_envelope.size());

  std::vector<std::uint8_t> wire_no_sig;
  if (!EncodeGroupCipherNoSig(group_id, username_, sender_key->version, iter,
                              nonce, mac, cipher, wire_no_sig)) {
    last_error_ = "encode group cipher failed";
    return false;
  }

  std::vector<std::uint8_t> msg_sig;
  std::string msg_sig_err;
  if (!e2ee_.SignDetached(wire_no_sig, msg_sig, msg_sig_err)) {
    last_error_ = msg_sig_err.empty() ? "sign group message failed" : msg_sig_err;
    return false;
  }

  std::vector<std::uint8_t> wire = std::move(wire_no_sig);
  mi::server::proto::WriteBytes(msg_sig, wire);

  const bool ok = SendGroupCipherMessage(group_id, wire);
  BestEffortPersistHistoryEnvelope(true, true, group_id, username_, plain_envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    out_message_id_hex.clear();
    return false;
  }

  sender_key->ck = next_ck;
  sender_key->next_iteration++;
  sender_key->sent_count++;
  last_error_ = warn;
  if (!out_message_id_hex.empty()) {
    const auto map_it = group_delivery_map_.find(out_message_id_hex);
    if (map_it == group_delivery_map_.end()) {
      group_delivery_map_[out_message_id_hex] = group_id;
      group_delivery_order_.push_back(out_message_id_hex);
      while (group_delivery_order_.size() > kChatSeenLimit) {
        group_delivery_map_.erase(group_delivery_order_.front());
        group_delivery_order_.pop_front();
      }
    } else {
      map_it->second = group_id;
    }
  }
  BestEffortBroadcastDeviceSyncMessage(true, true, group_id, username_,
                                      plain_envelope);
  return true;
}

bool ClientCore::ResendGroupChatText(const std::string& group_id,
                                     const std::string& message_id_hex,
                                     const std::string& text_utf8) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!HexToFixedBytes16(message_id_hex, msg_id)) {
      last_error_ = "invalid message id";
      return false;
    }

    std::vector<std::uint8_t> plain_envelope;
    if (!EncodeChatGroupText(msg_id, group_id, text_utf8, plain_envelope)) {
      last_error_ = "encode group text failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendGroup(group_id, plain_envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryEnvelope(true, true, group_id, username_, plain_envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }

  const auto members = ListGroupMembers(group_id);
  if (members.empty()) {
    if (last_error_.empty()) {
      last_error_ = "group member list empty";
    }
    return false;
  }

  GroupSenderKeyState* sender_key = nullptr;
  std::string warn;
  if (!EnsureGroupSenderKeyForSend(group_id, members, sender_key, warn)) {
    return false;
  }
  if (!sender_key) {
    last_error_ = "sender key unavailable";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    last_error_ = "invalid message id";
    return false;
  }

  std::vector<std::uint8_t> plain_envelope;
  if (!EncodeChatGroupText(msg_id, group_id, text_utf8, plain_envelope)) {
    last_error_ = "encode group text failed";
    return false;
  }
  std::vector<std::uint8_t> padded_envelope;
  std::string pad_err;
  if (!PadPayload(plain_envelope, padded_envelope, pad_err)) {
    last_error_ = pad_err.empty() ? "pad group message failed" : pad_err;
    return false;
  }

  std::array<std::uint8_t, 32> next_ck{};
  std::array<std::uint8_t, 32> mk{};
  if (!KdfGroupCk(sender_key->ck, next_ck, mk)) {
    last_error_ = "kdf failed";
    return false;
  }
  const std::uint32_t iter = sender_key->next_iteration;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> ad;
  BuildGroupCipherAd(group_id, username_, sender_key->version, iter, ad);

  std::vector<std::uint8_t> cipher;
  cipher.resize(padded_envelope.size());
  std::array<std::uint8_t, 16> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), mk.data(), nonce.data(), ad.data(),
                   ad.size(), padded_envelope.data(), padded_envelope.size());

  std::vector<std::uint8_t> wire_no_sig;
  if (!EncodeGroupCipherNoSig(group_id, username_, sender_key->version, iter,
                              nonce, mac, cipher, wire_no_sig)) {
    last_error_ = "encode group cipher failed";
    return false;
  }

  std::vector<std::uint8_t> msg_sig;
  std::string msg_sig_err;
  if (!e2ee_.SignDetached(wire_no_sig, msg_sig, msg_sig_err)) {
    last_error_ = msg_sig_err.empty() ? "sign group message failed" : msg_sig_err;
    return false;
  }

  std::vector<std::uint8_t> wire = std::move(wire_no_sig);
  mi::server::proto::WriteBytes(msg_sig, wire);

  const bool ok = SendGroupCipherMessage(group_id, wire);
  BestEffortPersistHistoryEnvelope(true, true, group_id, username_, plain_envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }

  sender_key->ck = next_ck;
  sender_key->next_iteration++;
  sender_key->sent_count++;
  last_error_ = warn;
  if (!message_id_hex.empty()) {
    const auto map_it = group_delivery_map_.find(message_id_hex);
    if (map_it == group_delivery_map_.end()) {
      group_delivery_map_[message_id_hex] = group_id;
      group_delivery_order_.push_back(message_id_hex);
      while (group_delivery_order_.size() > kChatSeenLimit) {
        group_delivery_map_.erase(group_delivery_order_.front());
        group_delivery_order_.pop_front();
      }
    } else {
      map_it->second = group_id;
    }
  }
  BestEffortBroadcastDeviceSyncMessage(true, true, group_id, username_,
                                      plain_envelope);
  return true;
}

bool ClientCore::SendGroupChatFile(const std::string& group_id,
                                   const std::filesystem::path& file_path,
                                   std::string& out_message_id_hex) {
  out_message_id_hex.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }

    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    std::error_code ec;
    if (file_path.empty() || !std::filesystem::exists(file_path, ec) || ec) {
      last_error_ = "file not found";
      return false;
    }
    if (std::filesystem::is_directory(file_path, ec) || ec) {
      last_error_ = "path is directory";
      return false;
    }
    const std::uint64_t size64 = std::filesystem::file_size(file_path, ec);
    if (ec || size64 == 0) {
      last_error_ = "file empty";
      return false;
    }
    if (size64 > kMaxChatFileBytes) {
      last_error_ = "file too large";
      return false;
    }

    const std::string file_name =
        file_path.has_filename() ? file_path.filename().u8string() : "file";

    std::array<std::uint8_t, 32> file_key{};
    std::string file_id;
    if (!UploadChatFileFromPath(file_path, size64, file_name, file_key,
                                file_id)) {
      return false;
    }

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatGroupFile(msg_id, group_id, size64, file_name, file_id,
                             file_key, envelope)) {
      last_error_ = "encode group file failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendGroup(group_id, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryEnvelope(true, true, group_id, username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }
  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }

  const auto members = ListGroupMembers(group_id);
  if (members.empty()) {
    if (last_error_.empty()) {
      last_error_ = "group member list empty";
    }
    return false;
  }

  GroupSenderKeyState* sender_key = nullptr;
  std::string warn;
  if (!EnsureGroupSenderKeyForSend(group_id, members, sender_key, warn)) {
    return false;
  }
  if (!sender_key) {
    last_error_ = "sender key unavailable";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::error_code ec;
  if (file_path.empty() || !std::filesystem::exists(file_path, ec) || ec) {
    last_error_ = "file not found";
    out_message_id_hex.clear();
    return false;
  }
  if (std::filesystem::is_directory(file_path, ec) || ec) {
    last_error_ = "path is directory";
    out_message_id_hex.clear();
    return false;
  }
  const std::uint64_t size64 = std::filesystem::file_size(file_path, ec);
  if (ec || size64 == 0) {
    last_error_ = "file empty";
    out_message_id_hex.clear();
    return false;
  }
  if (size64 > kMaxChatFileBytes) {
    last_error_ = "file too large";
    out_message_id_hex.clear();
    return false;
  }

  const std::string file_name =
      file_path.has_filename() ? file_path.filename().u8string() : "file";

  std::array<std::uint8_t, 32> file_key{};
  std::string file_id;
  if (!UploadChatFileFromPath(file_path, size64, file_name, file_key, file_id)) {
    out_message_id_hex.clear();
    return false;
  }

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatGroupFile(msg_id, group_id, size64, file_name, file_id,
                           file_key, envelope)) {
    last_error_ = "encode group file failed";
    out_message_id_hex.clear();
    return false;
  }
  std::vector<std::uint8_t> padded_envelope;
  std::string pad_err;
  if (!PadPayload(envelope, padded_envelope, pad_err)) {
    last_error_ = pad_err.empty() ? "pad group message failed" : pad_err;
    out_message_id_hex.clear();
    return false;
  }

  std::array<std::uint8_t, 32> next_ck{};
  std::array<std::uint8_t, 32> mk{};
  if (!KdfGroupCk(sender_key->ck, next_ck, mk)) {
    last_error_ = "kdf failed";
    out_message_id_hex.clear();
    return false;
  }
  const std::uint32_t iter = sender_key->next_iteration;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    last_error_ = "rng failed";
    out_message_id_hex.clear();
    return false;
  }
  std::vector<std::uint8_t> ad;
  BuildGroupCipherAd(group_id, username_, sender_key->version, iter, ad);

  std::vector<std::uint8_t> cipher;
  cipher.resize(padded_envelope.size());
  std::array<std::uint8_t, 16> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), mk.data(), nonce.data(), ad.data(),
                   ad.size(), padded_envelope.data(), padded_envelope.size());

  std::vector<std::uint8_t> wire_no_sig;
  if (!EncodeGroupCipherNoSig(group_id, username_, sender_key->version, iter,
                              nonce, mac, cipher, wire_no_sig)) {
    last_error_ = "encode group cipher failed";
    out_message_id_hex.clear();
    return false;
  }

  std::vector<std::uint8_t> msg_sig;
  std::string msg_sig_err;
  if (!e2ee_.SignDetached(wire_no_sig, msg_sig, msg_sig_err)) {
    last_error_ = msg_sig_err.empty() ? "sign group message failed" : msg_sig_err;
    out_message_id_hex.clear();
    return false;
  }

  std::vector<std::uint8_t> wire = std::move(wire_no_sig);
  mi::server::proto::WriteBytes(msg_sig, wire);

  const bool ok = SendGroupCipherMessage(group_id, wire);
  BestEffortPersistHistoryEnvelope(true, true, group_id, username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    out_message_id_hex.clear();
    return false;
  }

  sender_key->ck = next_ck;
  sender_key->next_iteration++;
  sender_key->sent_count++;
  last_error_ = warn;
  if (!out_message_id_hex.empty()) {
    const auto map_it = group_delivery_map_.find(out_message_id_hex);
    if (map_it == group_delivery_map_.end()) {
      group_delivery_map_[out_message_id_hex] = group_id;
      group_delivery_order_.push_back(out_message_id_hex);
      while (group_delivery_order_.size() > kChatSeenLimit) {
        group_delivery_map_.erase(group_delivery_order_.front());
        group_delivery_order_.pop_front();
      }
    } else {
      map_it->second = group_id;
    }
  }
  BestEffortBroadcastDeviceSyncMessage(true, true, group_id, username_, envelope);
  return true;
}

bool ClientCore::ResendGroupChatFile(const std::string& group_id,
                                     const std::string& message_id_hex,
                                     const std::filesystem::path& file_path) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }

    std::array<std::uint8_t, 16> msg_id{};
    if (!HexToFixedBytes16(message_id_hex, msg_id)) {
      last_error_ = "invalid message id";
      return false;
    }

    std::error_code ec;
    if (file_path.empty() || !std::filesystem::exists(file_path, ec) || ec) {
      last_error_ = "file not found";
      return false;
    }
    if (std::filesystem::is_directory(file_path, ec) || ec) {
      last_error_ = "path is directory";
      return false;
    }
    const std::uint64_t size64 = std::filesystem::file_size(file_path, ec);
    if (ec || size64 == 0) {
      last_error_ = "file empty";
      return false;
    }
    if (size64 > kMaxChatFileBytes) {
      last_error_ = "file too large";
      return false;
    }

    const std::string file_name =
        file_path.has_filename() ? file_path.filename().u8string() : "file";

    std::array<std::uint8_t, 32> file_key{};
    std::string file_id;
    if (!UploadChatFileFromPath(file_path, size64, file_name, file_key,
                                file_id)) {
      return false;
    }

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatGroupFile(msg_id, group_id, size64, file_name, file_id,
                             file_key, envelope)) {
      last_error_ = "encode group file failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendGroup(group_id, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryEnvelope(true, true, group_id, username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }
  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }

  const auto members = ListGroupMembers(group_id);
  if (members.empty()) {
    if (last_error_.empty()) {
      last_error_ = "group member list empty";
    }
    return false;
  }

  GroupSenderKeyState* sender_key = nullptr;
  std::string warn;
  if (!EnsureGroupSenderKeyForSend(group_id, members, sender_key, warn)) {
    return false;
  }
  if (!sender_key) {
    last_error_ = "sender key unavailable";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    last_error_ = "invalid message id";
    return false;
  }

  std::error_code ec;
  if (file_path.empty() || !std::filesystem::exists(file_path, ec) || ec) {
    last_error_ = "file not found";
    return false;
  }
  if (std::filesystem::is_directory(file_path, ec) || ec) {
    last_error_ = "path is directory";
    return false;
  }
  const std::uint64_t size64 = std::filesystem::file_size(file_path, ec);
  if (ec || size64 == 0) {
    last_error_ = "file empty";
    return false;
  }
  if (size64 > kMaxChatFileBytes) {
    last_error_ = "file too large";
    return false;
  }

  const std::string file_name =
      file_path.has_filename() ? file_path.filename().u8string() : "file";

  std::array<std::uint8_t, 32> file_key{};
  std::string file_id;
  if (!UploadChatFileFromPath(file_path, size64, file_name, file_key, file_id)) {
    return false;
  }

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatGroupFile(msg_id, group_id, size64, file_name, file_id,
                           file_key, envelope)) {
    last_error_ = "encode group file failed";
    return false;
  }
  std::vector<std::uint8_t> padded_envelope;
  std::string pad_err;
  if (!PadPayload(envelope, padded_envelope, pad_err)) {
    last_error_ = pad_err.empty() ? "pad group message failed" : pad_err;
    return false;
  }

  std::array<std::uint8_t, 32> next_ck{};
  std::array<std::uint8_t, 32> mk{};
  if (!KdfGroupCk(sender_key->ck, next_ck, mk)) {
    last_error_ = "kdf failed";
    return false;
  }
  const std::uint32_t iter = sender_key->next_iteration;

  std::array<std::uint8_t, 24> nonce{};
  if (!RandomBytes(nonce.data(), nonce.size())) {
    last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> ad;
  BuildGroupCipherAd(group_id, username_, sender_key->version, iter, ad);

  std::vector<std::uint8_t> cipher;
  cipher.resize(padded_envelope.size());
  std::array<std::uint8_t, 16> mac{};
  crypto_aead_lock(cipher.data(), mac.data(), mk.data(), nonce.data(), ad.data(),
                   ad.size(), padded_envelope.data(), padded_envelope.size());

  std::vector<std::uint8_t> wire_no_sig;
  if (!EncodeGroupCipherNoSig(group_id, username_, sender_key->version, iter,
                              nonce, mac, cipher, wire_no_sig)) {
    last_error_ = "encode group cipher failed";
    return false;
  }

  std::vector<std::uint8_t> msg_sig;
  std::string msg_sig_err;
  if (!e2ee_.SignDetached(wire_no_sig, msg_sig, msg_sig_err)) {
    last_error_ = msg_sig_err.empty() ? "sign group message failed" : msg_sig_err;
    return false;
  }

  std::vector<std::uint8_t> wire = std::move(wire_no_sig);
  mi::server::proto::WriteBytes(msg_sig, wire);

  const bool ok = SendGroupCipherMessage(group_id, wire);
  BestEffortPersistHistoryEnvelope(true, true, group_id, username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }

  sender_key->ck = next_ck;
  sender_key->next_iteration++;
  sender_key->sent_count++;
  last_error_ = warn;
  if (!message_id_hex.empty()) {
    const auto map_it = group_delivery_map_.find(message_id_hex);
    if (map_it == group_delivery_map_.end()) {
      group_delivery_map_[message_id_hex] = group_id;
      group_delivery_order_.push_back(message_id_hex);
      while (group_delivery_order_.size() > kChatSeenLimit) {
        group_delivery_map_.erase(group_delivery_order_.front());
        group_delivery_order_.pop_front();
      }
    } else {
      map_it->second = group_id;
    }
  }
  BestEffortBroadcastDeviceSyncMessage(true, true, group_id, username_, envelope);
  return true;
}

bool ClientCore::SendGroupInvite(const std::string& group_id,
                                 const std::string& peer_username,
                                 std::string& out_message_id_hex) {
  out_message_id_hex.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    if (group_id.empty()) {
      last_error_ = "group id empty";
      return false;
    }
    if (peer_username.empty()) {
      last_error_ = "peer empty";
      return false;
    }

    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatGroupInvite(msg_id, group_id, envelope)) {
      last_error_ = "encode group invite failed";
      out_message_id_hex.clear();
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      out_message_id_hex.clear();
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      out_message_id_hex.clear();
      return false;
    }
    if (!PushDeviceSyncCiphertext(event_cipher)) {
      out_message_id_hex.clear();
      return false;
    }
    return true;
  }
  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }

  const auto members = ListGroupMembers(group_id);
  if (members.empty()) {
    if (last_error_.empty()) {
      last_error_ = "group member list empty";
    }
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatGroupInvite(msg_id, group_id, envelope)) {
    last_error_ = "encode group invite failed";
    out_message_id_hex.clear();
    return false;
  }

  if (!SendPrivateE2ee(peer_username, envelope)) {
    out_message_id_hex.clear();
    return false;
  }
  return true;
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

bool ClientCore::SyncFriends(std::vector<FriendEntry>& out, bool& changed) {
  out.clear();
  changed = false;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteUint32(friend_sync_version_, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kFriendSync, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "friend sync failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "friend sync response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "friend sync failed" : server_err;
    return false;
  }
  std::size_t off = 1;
  std::uint32_t version = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, version) ||
      off >= resp_payload.size()) {
    last_error_ = "friend sync response invalid";
    return false;
  }
  const bool changed_flag = (resp_payload[off++] != 0);
  if (changed_flag) {
    std::uint32_t count = 0;
    if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
      last_error_ = "friend sync response invalid";
      return false;
    }
    out.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
      FriendEntry e;
      if (!mi::server::proto::ReadString(resp_payload, off, e.username) ||
          !mi::server::proto::ReadString(resp_payload, off, e.remark)) {
        last_error_ = "friend sync response invalid";
        out.clear();
        return false;
      }
      out.push_back(std::move(e));
    }
  }
  if (off != resp_payload.size()) {
    last_error_ = "friend sync response invalid";
    return false;
  }
  friend_sync_version_ = version;
  changed = changed_flag;
  return true;
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

bool ClientCore::SendFriendRequest(const std::string& target_username,
                                   const std::string& requester_remark) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(target_username, plain);
  mi::server::proto::WriteString(requester_remark, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kFriendRequestSend, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "friend request send failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "friend request response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ =
        server_err.empty() ? "friend request send failed" : server_err;
    return false;
  }
  return true;
}

std::vector<ClientCore::FriendRequestEntry> ClientCore::ListFriendRequests() {
  last_error_.clear();
  std::vector<FriendRequestEntry> out;
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kFriendRequestList, {},
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "friend request list failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "friend request list response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ =
        server_err.empty() ? "friend request list failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "friend request list decode failed";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    FriendRequestEntry e;
    if (!mi::server::proto::ReadString(resp_payload, off, e.requester_username) ||
        !mi::server::proto::ReadString(resp_payload, off, e.requester_remark)) {
      last_error_ = "friend request list decode failed";
      return {};
    }
    out.push_back(std::move(e));
  }
  return out;
}

bool ClientCore::RespondFriendRequest(const std::string& requester_username,
                                      bool accept) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(requester_username, plain);
  mi::server::proto::WriteUint32(accept ? 1u : 0u, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kFriendRequestRespond, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "friend request respond failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "friend request respond response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ =
        server_err.empty() ? "friend request respond failed" : server_err;
    return false;
  }
  return true;
}

bool ClientCore::DeleteFriend(const std::string& friend_username) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(friend_username, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kFriendDelete, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "friend delete failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "friend delete response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "friend delete failed" : server_err;
    return false;
  }
  return true;
}

bool ClientCore::DeleteChatHistory(const std::string& conv_id,
                                   bool is_group,
                                   bool delete_attachments,
                                   bool secure_wipe) {
  last_error_.clear();
  if (!history_store_) {
    return true;
  }
  if (conv_id.empty()) {
    last_error_ = "conv id empty";
    return false;
  }
  std::string err;
  if (!history_store_->DeleteConversation(is_group, conv_id, delete_attachments,
                                          secure_wipe, err)) {
    last_error_ = err.empty() ? "history delete failed" : err;
    return false;
  }
  last_error_.clear();
  return true;
}

bool ClientCore::SetUserBlocked(const std::string& blocked_username,
                               bool blocked) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(blocked_username, plain);
  mi::server::proto::WriteUint32(blocked ? 1u : 0u, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kUserBlockSet, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "block set failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "block set response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "block set failed" : server_err;
    return false;
  }
  return true;
}

bool ClientCore::PublishPreKeyBundle() {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (!EnsureE2ee()) {
    return false;
  }

  std::vector<std::uint8_t> bundle;
  std::string err;
  if (!e2ee_.BuildPublishBundle(bundle, err)) {
    last_error_ = err.empty() ? "build prekey bundle failed" : err;
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteBytes(bundle, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kPreKeyPublish, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "prekey publish failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "prekey publish response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "prekey publish failed" : server_err;
    return false;
  }
  prekey_published_ = true;
  return true;
}

bool ClientCore::SendPrivateE2ee(const std::string& peer_username,
                                 const std::vector<std::uint8_t>& plaintext) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }

  const std::vector<std::uint8_t> app_plain =
      WrapWithGossip(plaintext, kt_tree_size_, kt_root_);

  std::vector<std::uint8_t> payload;
  std::string enc_err;
  if (!e2ee_.EncryptToPeer(peer_username, {}, app_plain, payload, enc_err)) {
    if (enc_err == "peer bundle missing") {
      std::vector<std::uint8_t> peer_bundle;
      if (!FetchPreKeyBundle(peer_username, peer_bundle)) {
        return false;
      }
      if (!e2ee_.EncryptToPeer(peer_username, peer_bundle, app_plain, payload,
                               enc_err)) {
        last_error_ = enc_err.empty() ? "encrypt failed" : enc_err;
        return false;
      }
    } else {
      last_error_ = enc_err.empty() ? "encrypt failed" : enc_err;
      return false;
    }
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(peer_username, plain);
  mi::server::proto::WriteBytes(payload, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kPrivateSend, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "private send failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "private send response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "private send failed" : server_err;
    return false;
  }
  return true;
}

std::vector<mi::client::e2ee::PrivateMessage> ClientCore::PullPrivateE2ee() {
  std::vector<mi::client::e2ee::PrivateMessage> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }
  if (!EnsureE2ee()) {
    return out;
  }
  if (!EnsurePreKeyPublished()) {
    return out;
  }

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kPrivatePull, {}, resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "private pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "private pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "private pull failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "private pull response invalid";
    return out;
  }

  for (std::uint32_t i = 0; i < count; ++i) {
    std::string sender;
    std::vector<std::uint8_t> payload;
    if (!mi::server::proto::ReadString(resp_payload, off, sender) ||
        !mi::server::proto::ReadBytes(resp_payload, off, payload)) {
      last_error_ = "private pull response invalid";
      break;
    }

    mi::client::e2ee::PrivateMessage msg;
    std::string dec_err;
    if (e2ee_.DecryptFromPayload(sender, payload, msg, dec_err)) {
      std::uint64_t peer_tree_size = 0;
      std::array<std::uint8_t, 32> peer_root{};
      std::vector<std::uint8_t> inner_plain;
      if (UnwrapGossip(msg.plaintext, peer_tree_size, peer_root, inner_plain)) {
        msg.plaintext = std::move(inner_plain);
        if (peer_tree_size > 0 && kt_tree_size_ > 0) {
          if (peer_tree_size == kt_tree_size_ && peer_root != kt_root_) {
            last_error_ = "kt gossip mismatch";
          } else if (peer_tree_size > kt_tree_size_) {
            std::vector<std::array<std::uint8_t, 32>> proof;
            if (FetchKtConsistency(kt_tree_size_, peer_tree_size, proof) &&
                VerifyConsistencyProof(
                    static_cast<std::size_t>(kt_tree_size_),
                    static_cast<std::size_t>(peer_tree_size), kt_root_,
                    peer_root, proof)) {
              kt_tree_size_ = peer_tree_size;
              kt_root_ = peer_root;
              SaveKtState();
            } else if (last_error_.empty()) {
              last_error_ = "kt gossip verify failed";
            }
          }
        }
      }
      out.push_back(std::move(msg));
    } else if (last_error_.empty() && !dec_err.empty()) {
      last_error_ = dec_err;
    }
  }
  return out;
}

bool ClientCore::PushMedia(const std::string& recipient,
                           const std::array<std::uint8_t, 16>& call_id,
                           const std::vector<std::uint8_t>& packet) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (recipient.empty()) {
    last_error_ = "recipient empty";
    return false;
  }
  if (packet.empty()) {
    last_error_ = "packet empty";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(recipient, plain);
  WriteFixed16(call_id, plain);
  mi::server::proto::WriteBytes(packet, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kMediaPush, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "media push failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "media push response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "media push failed" : server_err;
    return false;
  }
  return true;
}

std::vector<ClientCore::MediaRelayPacket> ClientCore::PullMedia(
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t max_packets,
    std::uint32_t wait_ms) {
  std::vector<MediaRelayPacket> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }
  if (max_packets == 0) {
    max_packets = 1;
  } else if (max_packets > 256) {
    max_packets = 256;
  }
  if (wait_ms > 1000) {
    wait_ms = 1000;
  }
  std::vector<std::uint8_t> plain;
  WriteFixed16(call_id, plain);
  mi::server::proto::WriteUint32(max_packets, plain);
  mi::server::proto::WriteUint32(wait_ms, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kMediaPull, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "media pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "media pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "media pull failed" : server_err;
    return out;
  }
  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "media pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    MediaRelayPacket packet;
    if (!mi::server::proto::ReadString(resp_payload, off, packet.sender) ||
        !mi::server::proto::ReadBytes(resp_payload, off, packet.payload)) {
      last_error_ = "media pull response invalid";
      break;
    }
    out.push_back(std::move(packet));
  }
  return out;
}

ClientCore::GroupCallSignalResult ClientCore::SendGroupCallSignal(
    std::uint8_t op, const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id, bool video,
    std::uint32_t key_id, std::uint32_t seq, std::uint64_t ts_ms,
    const std::vector<std::uint8_t>& ext) {
  GroupCallSignalResult resp;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    resp.error = last_error_;
    return resp;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    resp.error = last_error_;
    return resp;
  }

  std::vector<std::uint8_t> plain;
  plain.reserve(64 + group_id.size() + ext.size());
  plain.push_back(op);
  mi::server::proto::WriteString(group_id, plain);
  WriteFixed16(call_id, plain);
  const std::uint8_t media_flags = video ? static_cast<std::uint8_t>(0x01 | 0x02)
                                         : static_cast<std::uint8_t>(0x01);
  plain.push_back(media_flags);
  mi::server::proto::WriteUint32(key_id, plain);
  mi::server::proto::WriteUint32(seq, plain);
  if (ts_ms == 0) {
    ts_ms = NowUnixSeconds() * 1000ULL;
  }
  mi::server::proto::WriteUint64(ts_ms, plain);
  mi::server::proto::WriteBytes(ext, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupCallSignal, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "group call signal failed";
    }
    resp.error = last_error_;
    return resp;
  }
  if (resp_payload.empty()) {
    last_error_ = "group call response empty";
    resp.error = last_error_;
    return resp;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "group call failed" : server_err;
    resp.error = last_error_;
    return resp;
  }

  std::size_t off = 1;
  if (!ReadFixed16(resp_payload, off, resp.call_id) ||
      !mi::server::proto::ReadUint32(resp_payload, off, resp.key_id)) {
    last_error_ = "group call response invalid";
    resp.error = last_error_;
    return resp;
  }
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "group call response invalid";
    resp.error = last_error_;
    return resp;
  }
  resp.members.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    std::string member;
    if (!mi::server::proto::ReadString(resp_payload, off, member)) {
      last_error_ = "group call response invalid";
      resp.error = last_error_;
      return resp;
    }
    resp.members.push_back(std::move(member));
  }
  if (off != resp_payload.size()) {
    last_error_ = "group call response invalid";
    resp.error = last_error_;
    return resp;
  }
  resp.success = true;
  return resp;
}

bool ClientCore::StartGroupCall(const std::string& group_id,
                                bool video,
                                std::array<std::uint8_t, 16>& out_call_id,
                                std::uint32_t& out_key_id) {
  out_call_id.fill(0);
  out_key_id = 0;
  last_error_.clear();
  std::array<std::uint8_t, 16> empty{};
  const auto resp =
      SendGroupCallSignal(kGroupCallOpCreate, group_id, empty, video);
  if (!resp.success) {
    return false;
  }
  out_call_id = resp.call_id;
  out_key_id = resp.key_id;

  std::array<std::uint8_t, 32> call_key{};
  if (!RandomBytes(call_key.data(), call_key.size())) {
    last_error_ = "rng failed";
    return false;
  }
  if (!StoreGroupCallKey(group_id, resp.call_id, resp.key_id, call_key)) {
    return false;
  }

  const auto members = ListGroupMembers(group_id);
  if (members.empty()) {
    if (last_error_.empty()) {
      last_error_ = "group member list empty";
    }
    return false;
  }

  std::string first_error;
  for (const auto& member : members) {
    if (!username_.empty() && member == username_) {
      continue;
    }
    const std::string saved_err = last_error_;
    if (!SendGroupCallKeyEnvelope(group_id, member, resp.call_id, resp.key_id,
                                  call_key) &&
        first_error.empty()) {
      first_error = last_error_;
    }
    last_error_ = saved_err;
  }
  if (!first_error.empty()) {
    last_error_ = first_error;
  }
  return true;
}

bool ClientCore::JoinGroupCall(const std::string& group_id,
                               const std::array<std::uint8_t, 16>& call_id,
                               bool video) {
  std::uint32_t key_id = 0;
  return JoinGroupCall(group_id, call_id, video, key_id);
}

bool ClientCore::JoinGroupCall(const std::string& group_id,
                               const std::array<std::uint8_t, 16>& call_id,
                               bool video,
                               std::uint32_t& out_key_id) {
  out_key_id = 0;
  last_error_.clear();
  const auto resp =
      SendGroupCallSignal(kGroupCallOpJoin, group_id, call_id, video);
  if (!resp.success) {
    return false;
  }
  out_key_id = resp.key_id;
  std::array<std::uint8_t, 32> call_key{};
  if (!LookupGroupCallKey(group_id, call_id, resp.key_id, call_key)) {
    bool requested = false;
    for (const auto& member : resp.members) {
      if (!username_.empty() && member == username_) {
        continue;
      }
      const std::string saved_err = last_error_;
      SendGroupCallKeyRequest(group_id, member, call_id, resp.key_id);
      last_error_ = saved_err;
      requested = true;
      break;
    }
    if (!requested) {
      const std::string saved_err = last_error_;
      const auto members = ListGroupMembers(group_id);
      last_error_ = saved_err;
      for (const auto& member : members) {
        if (!username_.empty() && member == username_) {
          continue;
        }
        const std::string saved_err2 = last_error_;
        SendGroupCallKeyRequest(group_id, member, call_id, resp.key_id);
        last_error_ = saved_err2;
        break;
      }
    }
  }
  return true;
}

bool ClientCore::LeaveGroupCall(const std::string& group_id,
                                const std::array<std::uint8_t, 16>& call_id) {
  last_error_.clear();
  const auto resp =
      SendGroupCallSignal(kGroupCallOpLeave, group_id, call_id, false);
  if (!resp.success) {
    return false;
  }
  const std::string map_key = MakeGroupCallKeyMapKey(group_id, call_id);
  group_call_keys_.erase(map_key);
  return true;
}

bool ClientCore::RotateGroupCallKey(
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::vector<std::string>& members) {
  last_error_.clear();
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  if (members.empty()) {
    last_error_ = "group members empty";
    return false;
  }
  if (key_id == 0) {
    last_error_ = "key id invalid";
    return false;
  }
  std::array<std::uint8_t, 32> call_key{};
  if (!RandomBytes(call_key.data(), call_key.size())) {
    last_error_ = "rng failed";
    return false;
  }
  if (!StoreGroupCallKey(group_id, call_id, key_id, call_key)) {
    return false;
  }
  std::string first_error;
  for (const auto& member : members) {
    if (!username_.empty() && member == username_) {
      continue;
    }
    const std::string saved_err = last_error_;
    if (!SendGroupCallKeyEnvelope(group_id, member, call_id, key_id, call_key) &&
        first_error.empty()) {
      first_error = last_error_;
    }
    last_error_ = saved_err;
  }
  if (!first_error.empty()) {
    last_error_ = first_error;
    return false;
  }
  return true;
}

bool ClientCore::RequestGroupCallKey(
    const std::string& group_id,
    const std::array<std::uint8_t, 16>& call_id,
    std::uint32_t key_id,
    const std::vector<std::string>& members) {
  last_error_.clear();
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  if (members.empty()) {
    last_error_ = "group members empty";
    return false;
  }
  if (key_id == 0) {
    last_error_ = "key id invalid";
    return false;
  }
  bool requested = false;
  for (const auto& member : members) {
    if (!username_.empty() && member == username_) {
      continue;
    }
    const std::string saved_err = last_error_;
    SendGroupCallKeyRequest(group_id, member, call_id, key_id);
    last_error_ = saved_err;
    requested = true;
  }
  if (!requested) {
    last_error_ = "no member to request";
    return false;
  }
  return true;
}

bool ClientCore::GetGroupCallKey(const std::string& group_id,
                                 const std::array<std::uint8_t, 16>& call_id,
                                 std::uint32_t key_id,
                                 std::array<std::uint8_t, 32>& out_key) const {
  return LookupGroupCallKey(group_id, call_id, key_id, out_key);
}

std::vector<ClientCore::GroupCallEvent> ClientCore::PullGroupCallEvents(
    std::uint32_t max_events, std::uint32_t wait_ms) {
  std::vector<GroupCallEvent> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }
  if (max_events == 0) {
    max_events = 1;
  } else if (max_events > 256) {
    max_events = 256;
  }
  if (wait_ms > 1000) {
    wait_ms = 1000;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteUint32(max_events, plain);
  mi::server::proto::WriteUint32(wait_ms, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupCallSignalPull, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "group call pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "group call pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "group call pull failed" : server_err;
    return out;
  }
  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "group call pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    if (off >= resp_payload.size()) {
      last_error_ = "group call pull response invalid";
      break;
    }
    GroupCallEvent ev;
    ev.op = resp_payload[off++];
    if (!mi::server::proto::ReadString(resp_payload, off, ev.group_id) ||
        !ReadFixed16(resp_payload, off, ev.call_id) ||
        !mi::server::proto::ReadUint32(resp_payload, off, ev.key_id) ||
        !mi::server::proto::ReadString(resp_payload, off, ev.sender)) {
      last_error_ = "group call pull response invalid";
      break;
    }
    if (off >= resp_payload.size()) {
      last_error_ = "group call pull response invalid";
      break;
    }
    ev.media_flags = resp_payload[off++];
    if (!mi::server::proto::ReadUint64(resp_payload, off, ev.ts_ms)) {
      last_error_ = "group call pull response invalid";
      break;
    }
    out.push_back(std::move(ev));
  }
  return out;
}

bool ClientCore::PushGroupMedia(const std::string& group_id,
                                const std::array<std::uint8_t, 16>& call_id,
                                const std::vector<std::uint8_t>& packet) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  if (packet.empty()) {
    last_error_ = "packet empty";
    return false;
  }
  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  WriteFixed16(call_id, plain);
  mi::server::proto::WriteBytes(packet, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupMediaPush, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "group media push failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "group media push response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "group media push failed" : server_err;
    return false;
  }
  return true;
}

std::vector<ClientCore::MediaRelayPacket> ClientCore::PullGroupMedia(
    const std::array<std::uint8_t, 16>& call_id, std::uint32_t max_packets,
    std::uint32_t wait_ms) {
  std::vector<MediaRelayPacket> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }
  if (max_packets == 0) {
    max_packets = 1;
  } else if (max_packets > 256) {
    max_packets = 256;
  }
  if (wait_ms > 1000) {
    wait_ms = 1000;
  }
  std::vector<std::uint8_t> plain;
  WriteFixed16(call_id, plain);
  mi::server::proto::WriteUint32(max_packets, plain);
  mi::server::proto::WriteUint32(wait_ms, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupMediaPull, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "group media pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "group media pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "group media pull failed" : server_err;
    return out;
  }
  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "group media pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    MediaRelayPacket packet;
    if (!mi::server::proto::ReadString(resp_payload, off, packet.sender) ||
        !mi::server::proto::ReadBytes(resp_payload, off, packet.payload)) {
      last_error_ = "group media pull response invalid";
      break;
    }
    out.push_back(std::move(packet));
  }
  return out;
}

bool ClientCore::DeriveMediaRoot(
    const std::string& peer_username,
    const std::array<std::uint8_t, 16>& call_id,
    std::array<std::uint8_t, 32>& out_media_root,
    std::string& out_error) {
  out_error.clear();
  last_error_.clear();
  out_media_root.fill(0);
  if (!EnsureE2ee()) {
    out_error = last_error_.empty() ? "e2ee not ready" : last_error_;
    return false;
  }
  if (peer_username.empty()) {
    out_error = "peer username empty";
    last_error_ = out_error;
    return false;
  }
  if (!e2ee_.DeriveMediaRoot(peer_username, call_id, out_media_root,
                             out_error)) {
    if (out_error.empty()) {
      out_error = "media root derive failed";
    }
    last_error_ = out_error;
    return false;
  }
  return true;
}

std::vector<mi::client::e2ee::PrivateMessage> ClientCore::DrainReadyPrivateE2ee() {
  std::vector<mi::client::e2ee::PrivateMessage> out;
  last_error_.clear();
  if (!EnsureE2ee()) {
    return out;
  }
  out = e2ee_.DrainReadyMessages();
  for (auto& msg : out) {
    std::uint64_t peer_tree_size = 0;
    std::array<std::uint8_t, 32> peer_root{};
    std::vector<std::uint8_t> inner_plain;
    if (UnwrapGossip(msg.plaintext, peer_tree_size, peer_root, inner_plain)) {
      msg.plaintext = std::move(inner_plain);
      if (peer_tree_size > 0 && kt_tree_size_ > 0) {
        if (peer_tree_size == kt_tree_size_ && peer_root != kt_root_) {
          last_error_ = "kt gossip mismatch";
        } else if (peer_tree_size > kt_tree_size_) {
          std::vector<std::array<std::uint8_t, 32>> proof;
          if (FetchKtConsistency(kt_tree_size_, peer_tree_size, proof) &&
              VerifyConsistencyProof(static_cast<std::size_t>(kt_tree_size_),
                                    static_cast<std::size_t>(peer_tree_size),
                                    kt_root_, peer_root, proof)) {
            kt_tree_size_ = peer_tree_size;
            kt_root_ = peer_root;
            SaveKtState();
          } else if (last_error_.empty()) {
            last_error_ = "kt gossip verify failed";
          }
        }
      }
    }
  }
  return out;
}

bool ClientCore::SendGroupCipherMessage(const std::string& group_id,
                                        const std::vector<std::uint8_t>& payload) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (group_id.empty()) {
    last_error_ = "group id empty";
    return false;
  }
  if (payload.empty()) {
    last_error_ = "payload empty";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  mi::server::proto::WriteBytes(payload, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupCipherSend, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "group send failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "group send response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "group send failed" : server_err;
    return false;
  }
  return true;
}

bool ClientCore::SendGroupSenderKeyEnvelope(
    const std::string& group_id, const std::string& peer_username,
    const std::vector<std::uint8_t>& plaintext) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }
  if (group_id.empty() || peer_username.empty()) {
    last_error_ = "invalid params";
    return false;
  }

  const std::vector<std::uint8_t> app_plain =
      WrapWithGossip(plaintext, kt_tree_size_, kt_root_);

  std::vector<std::uint8_t> payload;
  std::string enc_err;
  if (!e2ee_.EncryptToPeer(peer_username, {}, app_plain, payload, enc_err)) {
    if (enc_err == "peer bundle missing") {
      std::vector<std::uint8_t> peer_bundle;
      if (!FetchPreKeyBundle(peer_username, peer_bundle)) {
        return false;
      }
      if (!e2ee_.EncryptToPeer(peer_username, peer_bundle, app_plain, payload,
                               enc_err)) {
        last_error_ = enc_err.empty() ? "encrypt failed" : enc_err;
        return false;
      }
    } else {
      last_error_ = enc_err.empty() ? "encrypt failed" : enc_err;
      return false;
    }
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(group_id, plain);
  mi::server::proto::WriteString(peer_username, plain);
  mi::server::proto::WriteBytes(payload, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupSenderKeySend, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "group sender key send failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "group sender key response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ =
        server_err.empty() ? "group sender key send failed" : server_err;
    return false;
  }
  return true;
}

std::vector<ClientCore::PendingGroupCipher> ClientCore::PullGroupCipherMessages() {
  std::vector<PendingGroupCipher> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupCipherPull, {}, resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "group pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "group pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "group pull failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "group pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    PendingGroupCipher m;
    if (!mi::server::proto::ReadString(resp_payload, off, m.group_id) ||
        !mi::server::proto::ReadString(resp_payload, off, m.sender_username) ||
        !mi::server::proto::ReadBytes(resp_payload, off, m.payload)) {
      out.clear();
      last_error_ = "group pull response invalid";
      return out;
    }
    out.push_back(std::move(m));
  }
  if (off != resp_payload.size()) {
    out.clear();
    last_error_ = "group pull response invalid";
    return out;
  }
  return out;
}

std::vector<ClientCore::PendingGroupNotice> ClientCore::PullGroupNoticeMessages() {
  std::vector<PendingGroupNotice> out;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return out;
  }

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kGroupNoticePull, {}, resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "group notice pull failed";
    }
    return out;
  }
  if (resp_payload.empty()) {
    last_error_ = "group notice pull response empty";
    return out;
  }
  if (resp_payload[0] == 0) {
    std::size_t off = 1;
    std::string server_err;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ =
        server_err.empty() ? "group notice pull failed" : server_err;
    return out;
  }

  std::size_t off = 1;
  std::uint32_t count = 0;
  if (!mi::server::proto::ReadUint32(resp_payload, off, count)) {
    last_error_ = "group notice pull response invalid";
    return out;
  }
  out.reserve(count);
  for (std::uint32_t i = 0; i < count; ++i) {
    PendingGroupNotice m;
    if (!mi::server::proto::ReadString(resp_payload, off, m.group_id) ||
        !mi::server::proto::ReadString(resp_payload, off, m.sender_username) ||
        !mi::server::proto::ReadBytes(resp_payload, off, m.payload)) {
      out.clear();
      last_error_ = "group notice pull response invalid";
      return out;
    }
    out.push_back(std::move(m));
  }
  if (off != resp_payload.size()) {
    out.clear();
    last_error_ = "group notice pull response invalid";
    return out;
  }
  return out;
}

bool ClientCore::SendChatText(const std::string& peer_username,
                              const std::string& text_utf8,
                              std::string& out_message_id_hex) {
  out_message_id_hex.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    if (peer_username.empty()) {
      last_error_ = "peer empty";
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatText(msg_id, text_utf8, envelope)) {
      last_error_ = "encode chat text failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryEnvelope(false, true, peer_username, username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }
  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatText(msg_id, text_utf8, envelope)) {
    last_error_ = "encode chat text failed";
    return false;
  }
  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryEnvelope(false, true, peer_username, username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

bool ClientCore::ResendChatText(const std::string& peer_username,
                                const std::string& message_id_hex,
                                const std::string& text_utf8) {
  last_error_.clear();
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!HexToFixedBytes16(message_id_hex, msg_id)) {
      last_error_ = "invalid message id";
      return false;
    }
    std::vector<std::uint8_t> envelope;
    if (!EncodeChatText(msg_id, text_utf8, envelope)) {
      last_error_ = "encode chat text failed";
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryEnvelope(false, true, peer_username, username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    last_error_ = "invalid message id";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatText(msg_id, text_utf8, envelope)) {
    last_error_ = "encode chat text failed";
    return false;
  }
  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                ok ? HistoryStatus::kSent
                                   : HistoryStatus::kFailed,
                                NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

bool ClientCore::SendChatTextWithReply(const std::string& peer_username,
                                      const std::string& text_utf8,
                                      const std::string& reply_to_message_id_hex,
                                      const std::string& reply_preview_utf8,
                                      std::string& out_message_id_hex) {
  out_message_id_hex.clear();
  last_error_.clear();
  if (reply_to_message_id_hex.empty()) {
    return SendChatText(peer_username, text_utf8, out_message_id_hex);
  }
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  std::array<std::uint8_t, 16> reply_to{};
  if (!HexToFixedBytes16(reply_to_message_id_hex, reply_to)) {
    last_error_ = "invalid reply message id";
    return false;
  }
  std::string preview = reply_preview_utf8;
  if (preview.size() > 512) {
    preview.resize(512);
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
    std::vector<std::uint8_t> envelope;
    if (!EncodeChatRichText(msg_id, text_utf8, true, reply_to, preview, envelope)) {
      last_error_ = "encode chat rich failed";
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryEnvelope(false, true, peer_username, username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }

  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichText(msg_id, text_utf8, true, reply_to, preview, envelope)) {
    last_error_ = "encode chat rich failed";
    return false;
  }
  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryEnvelope(false, true, peer_username, username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

bool ClientCore::ResendChatTextWithReply(const std::string& peer_username,
                                        const std::string& message_id_hex,
                                        const std::string& text_utf8,
                                        const std::string& reply_to_message_id_hex,
                                        const std::string& reply_preview_utf8) {
  last_error_.clear();
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  if (reply_to_message_id_hex.empty()) {
    return ResendChatText(peer_username, message_id_hex, text_utf8);
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    last_error_ = "invalid message id";
    return false;
  }
  std::array<std::uint8_t, 16> reply_to{};
  if (!HexToFixedBytes16(reply_to_message_id_hex, reply_to)) {
    last_error_ = "invalid reply message id";
    return false;
  }
  std::string preview = reply_preview_utf8;
  if (preview.size() > 512) {
    preview.resize(512);
  }

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichText(msg_id, text_utf8, true, reply_to, preview, envelope)) {
    last_error_ = "encode chat rich failed";
    return false;
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
    return ok;
  }

  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                ok ? HistoryStatus::kSent
                                   : HistoryStatus::kFailed,
                                NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

bool ClientCore::SendChatLocation(const std::string& peer_username,
                                  std::int32_t lat_e7, std::int32_t lon_e7,
                                  const std::string& label_utf8,
                                  std::string& out_message_id_hex) {
  out_message_id_hex.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  if (lat_e7 < -900000000 || lat_e7 > 900000000) {
    last_error_ = "latitude out of range";
    return false;
  }
  if (lon_e7 < -1800000000 || lon_e7 > 1800000000) {
    last_error_ = "longitude out of range";
    return false;
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
    std::vector<std::uint8_t> envelope;
    if (!EncodeChatRichLocation(msg_id, lat_e7, lon_e7, label_utf8, envelope)) {
      last_error_ = "encode chat rich failed";
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return PushDeviceSyncCiphertext(event_cipher);
  }

  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichLocation(msg_id, lat_e7, lon_e7, label_utf8, envelope)) {
    last_error_ = "encode chat rich failed";
    return false;
  }
  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryEnvelope(false, true, peer_username, username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

bool ClientCore::ResendChatLocation(const std::string& peer_username,
                                    const std::string& message_id_hex,
                                    std::int32_t lat_e7, std::int32_t lon_e7,
                                    const std::string& label_utf8) {
  last_error_.clear();
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  if (lat_e7 < -900000000 || lat_e7 > 900000000) {
    last_error_ = "latitude out of range";
    return false;
  }
  if (lon_e7 < -1800000000 || lon_e7 > 1800000000) {
    last_error_ = "longitude out of range";
    return false;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    last_error_ = "invalid message id";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichLocation(msg_id, lat_e7, lon_e7, label_utf8, envelope)) {
    last_error_ = "encode chat rich failed";
    return false;
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
    return ok;
  }

  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                ok ? HistoryStatus::kSent
                                   : HistoryStatus::kFailed,
                                NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

bool ClientCore::SendChatContactCard(const std::string& peer_username,
                                     const std::string& card_username,
                                     const std::string& card_display,
                                     std::string& out_message_id_hex) {
  out_message_id_hex.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  if (card_username.empty()) {
    last_error_ = "card username empty";
    return false;
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
    std::vector<std::uint8_t> envelope;
    if (!EncodeChatRichContactCard(msg_id, card_username, card_display, envelope)) {
      last_error_ = "encode chat rich failed";
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return PushDeviceSyncCiphertext(event_cipher);
  }

  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichContactCard(msg_id, card_username, card_display, envelope)) {
    last_error_ = "encode chat rich failed";
    return false;
  }
  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryEnvelope(false, true, peer_username, username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

bool ClientCore::ResendChatContactCard(const std::string& peer_username,
                                       const std::string& message_id_hex,
                                       const std::string& card_username,
                                       const std::string& card_display) {
  last_error_.clear();
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  if (card_username.empty()) {
    last_error_ = "card username empty";
    return false;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    last_error_ = "invalid message id";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatRichContactCard(msg_id, card_username, card_display, envelope)) {
    last_error_ = "encode chat rich failed";
    return false;
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
    return ok;
  }

  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                ok ? HistoryStatus::kSent
                                   : HistoryStatus::kFailed,
                                NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

bool ClientCore::SendChatSticker(const std::string& peer_username,
                                 const std::string& sticker_id,
                                 std::string& out_message_id_hex) {
  out_message_id_hex.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  if (sticker_id.empty()) {
    last_error_ = "sticker id empty";
    return false;
  }
  if (sticker_id.size() > 128) {
    last_error_ = "sticker id too long";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatSticker(msg_id, sticker_id, envelope)) {
    last_error_ = "encode chat sticker failed";
    return false;
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return PushDeviceSyncCiphertext(event_cipher);
  }

  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }
  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryEnvelope(false, true, peer_username, username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

bool ClientCore::ResendChatSticker(const std::string& peer_username,
                                   const std::string& message_id_hex,
                                   const std::string& sticker_id) {
  last_error_.clear();
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  if (sticker_id.empty()) {
    last_error_ = "sticker id empty";
    return false;
  }
  if (sticker_id.size() > 128) {
    last_error_ = "sticker id too long";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    last_error_ = "invalid message id";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatSticker(msg_id, sticker_id, envelope)) {
    last_error_ = "encode chat sticker failed";
    return false;
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
    return ok;
  }

  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryStatus(false, peer_username, msg_id,
                                ok ? HistoryStatus::kSent
                                   : HistoryStatus::kFailed,
                                NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

bool ClientCore::SendChatReadReceipt(const std::string& peer_username,
                                     const std::string& message_id_hex) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    last_error_ = "invalid message id";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatReadReceipt(msg_id, envelope)) {
    last_error_ = "encode read receipt failed";
    return false;
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return PushDeviceSyncCiphertext(event_cipher);
  }

  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }
  return SendPrivateE2ee(peer_username, envelope);
}

bool ClientCore::SendChatTyping(const std::string& peer_username, bool typing) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatTyping(msg_id, typing, envelope)) {
    last_error_ = "encode typing failed";
    return false;
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return PushDeviceSyncCiphertext(event_cipher);
  }

  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }
  return SendPrivateE2ee(peer_username, envelope);
}

bool ClientCore::SendChatPresence(const std::string& peer_username, bool online) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  std::vector<std::uint8_t> envelope;
  if (!EncodeChatPresence(msg_id, online, envelope)) {
    last_error_ = "encode presence failed";
    return false;
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return PushDeviceSyncCiphertext(event_cipher);
  }

  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }
  return SendPrivateE2ee(peer_username, envelope);
}

bool ClientCore::SendChatFile(const std::string& peer_username,
                              const std::filesystem::path& file_path,
                              std::string& out_message_id_hex) {
  out_message_id_hex.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    if (peer_username.empty()) {
      last_error_ = "peer empty";
      return false;
    }

    std::array<std::uint8_t, 16> msg_id{};
    if (!RandomBytes(msg_id.data(), msg_id.size())) {
      last_error_ = "rng failed";
      return false;
    }
    out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    std::error_code ec;
    if (file_path.empty() || !std::filesystem::exists(file_path, ec) || ec) {
      last_error_ = "file not found";
      return false;
    }
    if (std::filesystem::is_directory(file_path, ec) || ec) {
      last_error_ = "path is directory";
      return false;
    }
    const std::uint64_t size64 = std::filesystem::file_size(file_path, ec);
    if (ec || size64 == 0) {
      last_error_ = "file empty";
      return false;
    }
    if (size64 > kMaxChatFileBytes) {
      last_error_ = "file too large";
      return false;
    }

    const std::string file_name =
        file_path.has_filename() ? file_path.filename().u8string() : "file";

    std::array<std::uint8_t, 32> file_key{};
    std::string file_id;
    if (!UploadChatFileFromPath(file_path, size64, file_name, file_key,
                                file_id)) {
      return false;
    }

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatFile(msg_id, size64, file_name, file_id, file_key, envelope)) {
      last_error_ = "encode chat file failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    return PushDeviceSyncCiphertext(event_cipher);
  }
  if (!EnsureE2ee()) {
    return false;
  }
  if (!EnsurePreKeyPublished()) {
    return false;
  }
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }

  std::array<std::uint8_t, 16> msg_id{};
  if (!RandomBytes(msg_id.data(), msg_id.size())) {
    last_error_ = "rng failed";
    return false;
  }
  out_message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

  std::error_code ec;
  if (file_path.empty() || !std::filesystem::exists(file_path, ec) || ec) {
    last_error_ = "file not found";
    return false;
  }
  if (std::filesystem::is_directory(file_path, ec) || ec) {
    last_error_ = "path is directory";
    return false;
  }
  const std::uint64_t size64 = std::filesystem::file_size(file_path, ec);
  if (ec || size64 == 0) {
    last_error_ = "file empty";
    return false;
  }
  if (size64 > kMaxChatFileBytes) {
    last_error_ = "file too large";
    return false;
  }

  const std::string file_name =
      file_path.has_filename() ? file_path.filename().u8string() : "file";

  std::array<std::uint8_t, 32> file_key{};
  std::string file_id;
  if (!UploadChatFileFromPath(file_path, size64, file_name, file_key, file_id)) {
    return false;
  }

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatFile(msg_id, size64, file_name, file_id, file_key, envelope)) {
    last_error_ = "encode chat file failed";
    return false;
  }
  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryEnvelope(false, true, peer_username, username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

bool ClientCore::ResendChatFile(const std::string& peer_username,
                                const std::string& message_id_hex,
                                const std::filesystem::path& file_path) {
  last_error_.clear();
  if (peer_username.empty()) {
    last_error_ = "peer empty";
    return false;
  }
  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return false;
    }
    std::array<std::uint8_t, 16> msg_id{};
    if (!HexToFixedBytes16(message_id_hex, msg_id)) {
      last_error_ = "invalid message id";
      return false;
    }

    std::error_code ec;
    if (file_path.empty() || !std::filesystem::exists(file_path, ec) || ec) {
      last_error_ = "file not found";
      return false;
    }
    const std::uint64_t size64 = std::filesystem::file_size(file_path, ec);
    if (ec || size64 == 0) {
      last_error_ = "file empty";
      return false;
    }
    if (size64 > kMaxChatFileBytes) {
      last_error_ = "file too large";
      return false;
    }

    const std::string file_name =
        file_path.has_filename() ? file_path.filename().u8string() : "file";

    std::array<std::uint8_t, 32> file_key{};
    std::string file_id;
    if (!UploadChatFileFromPath(file_path, size64, file_name, file_key,
                                file_id)) {
      return false;
    }

    std::vector<std::uint8_t> envelope;
    if (!EncodeChatFile(msg_id, size64, file_name, file_id, file_key, envelope)) {
      last_error_ = "encode chat file failed";
      return false;
    }

    std::vector<std::uint8_t> event_plain;
    if (!EncodeDeviceSyncSendPrivate(peer_username, envelope, event_plain)) {
      last_error_ = "encode device sync failed";
      return false;
    }
    std::vector<std::uint8_t> event_cipher;
    if (!EncryptDeviceSync(event_plain, event_cipher)) {
      return false;
    }
    const bool ok = PushDeviceSyncCiphertext(event_cipher);
    BestEffortPersistHistoryEnvelope(false, true, peer_username, username_, envelope,
                                    ok ? HistoryStatus::kSent
                                       : HistoryStatus::kFailed,
                                    NowUnixSeconds());
    return ok;
  }
  std::array<std::uint8_t, 16> msg_id{};
  if (!HexToFixedBytes16(message_id_hex, msg_id)) {
    last_error_ = "invalid message id";
    return false;
  }

  std::error_code ec;
  if (file_path.empty() || !std::filesystem::exists(file_path, ec) || ec) {
    last_error_ = "file not found";
    return false;
  }
  const std::uint64_t size64 = std::filesystem::file_size(file_path, ec);
  if (ec || size64 == 0) {
    last_error_ = "file empty";
    return false;
  }
  if (size64 > kMaxChatFileBytes) {
    last_error_ = "file too large";
    return false;
  }

  const std::string file_name =
      file_path.has_filename() ? file_path.filename().u8string() : "file";

  std::array<std::uint8_t, 32> file_key{};
  std::string file_id;
  if (!UploadChatFileFromPath(file_path, size64, file_name, file_key, file_id)) {
    return false;
  }

  std::vector<std::uint8_t> envelope;
  if (!EncodeChatFile(msg_id, size64, file_name, file_id, file_key, envelope)) {
    last_error_ = "encode chat file failed";
    return false;
  }
  const bool ok = SendPrivateE2ee(peer_username, envelope);
  BestEffortPersistHistoryEnvelope(false, true, peer_username, username_, envelope,
                                  ok ? HistoryStatus::kSent
                                     : HistoryStatus::kFailed,
                                  NowUnixSeconds());
  if (!ok) {
    return false;
  }
  BestEffortBroadcastDeviceSyncMessage(false, true, peer_username, username_,
                                      envelope);
  return true;
}

ClientCore::ChatPollResult ClientCore::PollChat() {
  ChatPollResult result;
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return result;
  }
  {
    const std::string saved_err = last_error_;
    (void)MaybeSendCoverTraffic();
    last_error_ = saved_err;
  }
  {
    const std::string saved_err = last_error_;
    ResendPendingSenderKeyDistributions();
    last_error_ = saved_err;
  }

  if (device_sync_enabled_ && !device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      return result;
    }

    std::string sync_err;
    const auto pulled = PullDeviceSyncCiphertexts();
    if (!last_error_.empty()) {
      sync_err = last_error_;
    }
    last_error_.clear();

    for (const auto& cipher : pulled) {
      std::vector<std::uint8_t> plain;
      if (!DecryptDeviceSync(cipher, plain)) {
        if (sync_err.empty() && !last_error_.empty()) {
          sync_err = last_error_;
        }
        last_error_.clear();
        continue;
      }

      DeviceSyncEvent ev;
      if (!DecodeDeviceSyncEvent(plain, ev)) {
        continue;
      }

      if (ev.type == kDeviceSyncEventRotateKey) {
        if (!StoreDeviceSyncKey(ev.new_key)) {
          if (sync_err.empty() && !last_error_.empty()) {
            sync_err = last_error_;
          }
          last_error_.clear();
        }
        continue;
      }

      if (ev.type == kDeviceSyncEventHistorySnapshot) {
        if (ev.target_device_id.empty() || ev.target_device_id != device_id_) {
          continue;
        }
        const std::string saved_err = last_error_;
        if (history_store_) {
          for (const auto& m : ev.history) {
            std::string hist_err;
            if (m.is_system) {
              (void)history_store_->AppendSystem(m.is_group, m.conv_id,
                                                 m.system_text_utf8,
                                                 m.timestamp_sec, hist_err);
            } else {
              (void)history_store_->AppendEnvelope(
                  m.is_group, m.outgoing, m.conv_id, m.sender, m.envelope,
                  m.status, m.timestamp_sec, hist_err);
            }
          }
        }
        last_error_ = saved_err;
        continue;
      }

      if (ev.type == kDeviceSyncEventMessage) {
        std::uint8_t type = 0;
        std::array<std::uint8_t, 16> msg_id{};
        std::size_t off = 0;
        if (!DecodeChatHeader(ev.envelope, type, msg_id, off)) {
          continue;
        }
        const std::string id_hex =
            BytesToHexLower(msg_id.data(), msg_id.size());

        if (type == kChatTypeTyping) {
          if (off >= ev.envelope.size()) {
            continue;
          }
          const std::uint8_t state = ev.envelope[off++];
          if (off != ev.envelope.size()) {
            continue;
          }
          ChatTypingEvent te;
          te.from_username = ev.sender;
          te.typing = state != 0;
          result.typing_events.push_back(std::move(te));
          continue;
        }

        if (type == kChatTypePresence) {
          if (off >= ev.envelope.size()) {
            continue;
          }
          const std::uint8_t state = ev.envelope[off++];
          if (off != ev.envelope.size()) {
            continue;
          }
          ChatPresenceEvent pe;
          pe.from_username = ev.sender;
          pe.online = state != 0;
          result.presence_events.push_back(std::move(pe));
          continue;
        }

        if (type == kChatTypeRich) {
          RichDecoded rich;
          if (!DecodeChatRich(ev.envelope, off, rich) || off != ev.envelope.size()) {
            continue;
          }
          std::string text = FormatRichAsText(rich);
          if (ev.outgoing) {
            OutgoingChatTextMessage t;
            t.peer_username = ev.conv_id;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.outgoing_texts.push_back(std::move(t));
          } else {
            ChatTextMessage t;
            t.from_username = ev.sender;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.texts.push_back(std::move(t));
          }
          BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeText) {
          std::string text;
          if (!mi::server::proto::ReadString(ev.envelope, off, text) ||
              off != ev.envelope.size()) {
            continue;
          }
          if (ev.outgoing) {
            OutgoingChatTextMessage t;
            t.peer_username = ev.conv_id;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.outgoing_texts.push_back(std::move(t));
          } else {
            ChatTextMessage t;
            t.from_username = ev.sender;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.texts.push_back(std::move(t));
          }
          BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeFile) {
          std::uint64_t file_size = 0;
          std::string file_name;
          std::string file_id;
          std::array<std::uint8_t, 32> file_key{};
          if (!DecodeChatFile(ev.envelope, off, file_size, file_name, file_id,
                              file_key) ||
              off != ev.envelope.size()) {
            continue;
          }
          if (ev.outgoing) {
            OutgoingChatFileMessage f;
            f.peer_username = ev.conv_id;
            f.message_id_hex = id_hex;
            f.file_id = std::move(file_id);
            f.file_key = file_key;
            f.file_name = std::move(file_name);
            f.file_size = file_size;
            result.outgoing_files.push_back(std::move(f));
          } else {
            ChatFileMessage f;
            f.from_username = ev.sender;
            f.message_id_hex = id_hex;
            f.file_id = std::move(file_id);
            f.file_key = file_key;
            f.file_name = std::move(file_name);
            f.file_size = file_size;
            result.files.push_back(std::move(f));
          }
          BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeSticker) {
          std::string sticker_id;
          if (!mi::server::proto::ReadString(ev.envelope, off, sticker_id) ||
              off != ev.envelope.size()) {
            continue;
          }
          if (ev.outgoing) {
            OutgoingChatStickerMessage s;
            s.peer_username = ev.conv_id;
            s.message_id_hex = id_hex;
            s.sticker_id = std::move(sticker_id);
            result.outgoing_stickers.push_back(std::move(s));
          } else {
            ChatStickerMessage s;
            s.from_username = ev.sender;
            s.message_id_hex = id_hex;
            s.sticker_id = std::move(sticker_id);
            result.stickers.push_back(std::move(s));
          }
          BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeGroupText) {
          std::string group_id;
          std::string text;
          if (!mi::server::proto::ReadString(ev.envelope, off, group_id) ||
              !mi::server::proto::ReadString(ev.envelope, off, text) ||
              off != ev.envelope.size() || group_id != ev.conv_id) {
            continue;
          }
          if (ev.outgoing) {
            OutgoingGroupChatTextMessage t;
            t.group_id = group_id;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.outgoing_group_texts.push_back(std::move(t));
          } else {
            GroupChatTextMessage t;
            t.group_id = group_id;
            t.from_username = ev.sender;
            t.message_id_hex = id_hex;
            t.text_utf8 = std::move(text);
            result.group_texts.push_back(std::move(t));
          }
          BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeGroupFile) {
          std::string group_id;
          std::uint64_t file_size = 0;
          std::string file_name;
          std::string file_id;
          std::array<std::uint8_t, 32> file_key{};
          if (!DecodeChatGroupFile(ev.envelope, off, group_id, file_size,
                                   file_name, file_id, file_key) ||
              off != ev.envelope.size() || group_id != ev.conv_id) {
            continue;
          }
          if (ev.outgoing) {
            OutgoingGroupChatFileMessage f;
            f.group_id = group_id;
            f.message_id_hex = id_hex;
            f.file_id = std::move(file_id);
            f.file_key = file_key;
            f.file_name = std::move(file_name);
            f.file_size = file_size;
            result.outgoing_group_files.push_back(std::move(f));
          } else {
            GroupChatFileMessage f;
            f.group_id = group_id;
            f.from_username = ev.sender;
            f.message_id_hex = id_hex;
            f.file_id = std::move(file_id);
            f.file_key = file_key;
            f.file_name = std::move(file_name);
            f.file_size = file_size;
            result.group_files.push_back(std::move(f));
          }
          BestEffortPersistHistoryEnvelope(ev.is_group, ev.outgoing, ev.conv_id,
                                          ev.sender, ev.envelope,
                                          HistoryStatus::kSent, NowUnixSeconds());
          continue;
        }

        if (type == kChatTypeGroupInvite && !ev.outgoing) {
          std::string group_id;
          if (!mi::server::proto::ReadString(ev.envelope, off, group_id) ||
              off != ev.envelope.size()) {
            continue;
          }
          GroupInviteMessage inv;
          inv.group_id = std::move(group_id);
          inv.from_username = ev.sender;
          inv.message_id_hex = id_hex;
          result.group_invites.push_back(std::move(inv));
          continue;
        }

        continue;
      }

      if (ev.type == kDeviceSyncEventGroupNotice) {
        if (ev.conv_id.empty() || ev.sender.empty() || ev.envelope.empty()) {
          continue;
        }
        std::uint8_t kind = 0;
        std::string target;
        std::optional<std::uint8_t> role;
        if (!DecodeGroupNoticePayload(ev.envelope, kind, target, role)) {
          continue;
        }
        GroupNotice n;
        n.group_id = ev.conv_id;
        n.kind = kind;
        n.actor_username = ev.sender;
        n.target_username = std::move(target);
        if (role.has_value()) {
          const std::uint8_t rb = role.value();
          if (rb <= static_cast<std::uint8_t>(GroupMemberRole::kMember)) {
            n.role = static_cast<GroupMemberRole>(rb);
          }
        }
        result.group_notices.push_back(std::move(n));
        continue;
      }

      if (ev.type == kDeviceSyncEventDelivery) {
        if (ev.conv_id.empty()) {
          continue;
        }
        const std::string id_hex =
            BytesToHexLower(ev.msg_id.data(), ev.msg_id.size());
        if (id_hex.empty()) {
          continue;
        }
        if (ev.is_read) {
          ChatReadReceipt r;
          r.from_username = ev.conv_id;
          r.message_id_hex = id_hex;
          result.read_receipts.push_back(std::move(r));
        } else {
          ChatDelivery d;
          d.from_username = ev.conv_id;
          d.message_id_hex = id_hex;
          result.deliveries.push_back(std::move(d));
        }
        BestEffortPersistHistoryStatus(ev.is_group, ev.conv_id, ev.msg_id,
                                      ev.is_read ? HistoryStatus::kRead
                                                 : HistoryStatus::kDelivered,
                                      NowUnixSeconds());
        continue;
      }
    }

    last_error_ = sync_err;
    return result;
  }

  if (!EnsureE2ee()) {
    return result;
  }
  if (!EnsurePreKeyPublished()) {
    return result;
  }

  std::string sync_err;
  if (device_sync_enabled_ && device_sync_is_primary_) {
    if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
      sync_err = last_error_;
      last_error_.clear();
    }
  }
  if (device_sync_enabled_ && device_sync_is_primary_ && device_sync_key_loaded_) {
    const auto pulled = PullDeviceSyncCiphertexts();
    if (sync_err.empty() && !last_error_.empty()) {
      sync_err = last_error_;
    }
    last_error_.clear();

    const auto send_group_envelope =
        [&](const std::string& group_id, const std::vector<std::uint8_t>& envelope,
            std::string& out_warn) -> bool {
      out_warn.clear();
      if (group_id.empty()) {
        last_error_ = "group id empty";
        return false;
      }

      std::uint8_t type = 0;
      std::array<std::uint8_t, 16> msg_id{};
      std::size_t off = 0;
      if (!DecodeChatHeader(envelope, type, msg_id, off)) {
        last_error_ = "group envelope invalid";
        return false;
      }
      if (type != kChatTypeGroupText && type != kChatTypeGroupFile) {
        last_error_ = "group envelope invalid";
        return false;
      }
      std::size_t tmp_off = off;
      std::string inner_group_id;
      if (!mi::server::proto::ReadString(envelope, tmp_off, inner_group_id) ||
          inner_group_id != group_id) {
        last_error_ = "group envelope invalid";
        return false;
      }

      const auto members = ListGroupMembers(group_id);
      if (members.empty()) {
        if (last_error_.empty()) {
          last_error_ = "group member list empty";
        }
        return false;
      }

      GroupSenderKeyState* sender_key = nullptr;
      std::string warn;
      if (!EnsureGroupSenderKeyForSend(group_id, members, sender_key, warn)) {
        return false;
      }
      out_warn = warn;
      if (!sender_key) {
        last_error_ = "sender key unavailable";
        return false;
      }

      std::array<std::uint8_t, 32> next_ck{};
      std::array<std::uint8_t, 32> mk{};
      if (!KdfGroupCk(sender_key->ck, next_ck, mk)) {
        last_error_ = "kdf failed";
        return false;
      }
      const std::uint32_t iter = sender_key->next_iteration;

      std::array<std::uint8_t, 24> nonce{};
      if (!RandomBytes(nonce.data(), nonce.size())) {
        last_error_ = "rng failed";
        return false;
      }
      std::vector<std::uint8_t> ad;
      BuildGroupCipherAd(group_id, username_, sender_key->version, iter, ad);

      std::vector<std::uint8_t> padded_envelope;
      std::string pad_err;
      if (!PadPayload(envelope, padded_envelope, pad_err)) {
        last_error_ = pad_err.empty() ? "pad group message failed" : pad_err;
        return false;
      }

      std::vector<std::uint8_t> cipher;
      cipher.resize(padded_envelope.size());
      std::array<std::uint8_t, 16> mac{};
      crypto_aead_lock(cipher.data(), mac.data(), mk.data(), nonce.data(),
                       ad.data(), ad.size(), padded_envelope.data(),
                       padded_envelope.size());

      std::vector<std::uint8_t> wire_no_sig;
      if (!EncodeGroupCipherNoSig(group_id, username_, sender_key->version, iter,
                                  nonce, mac, cipher, wire_no_sig)) {
        last_error_ = "encode group cipher failed";
        return false;
      }

      std::vector<std::uint8_t> msg_sig;
      std::string msg_sig_err;
      if (!e2ee_.SignDetached(wire_no_sig, msg_sig, msg_sig_err)) {
        last_error_ =
            msg_sig_err.empty() ? "sign group message failed" : msg_sig_err;
        return false;
      }

      std::vector<std::uint8_t> wire = std::move(wire_no_sig);
      mi::server::proto::WriteBytes(msg_sig, wire);

      if (!SendGroupCipherMessage(group_id, wire)) {
        return false;
      }

      sender_key->ck = next_ck;
      sender_key->next_iteration++;
      sender_key->sent_count++;
      return true;
    };

    for (const auto& cipher : pulled) {
      std::vector<std::uint8_t> plain;
      if (!DecryptDeviceSync(cipher, plain)) {
        if (sync_err.empty() && !last_error_.empty()) {
          sync_err = last_error_;
        }
        last_error_.clear();
        continue;
      }

      DeviceSyncEvent ev;
      if (!DecodeDeviceSyncEvent(plain, ev)) {
        continue;
      }

      if (ev.type == kDeviceSyncEventRotateKey) {
        if (!StoreDeviceSyncKey(ev.new_key)) {
          if (sync_err.empty() && !last_error_.empty()) {
            sync_err = last_error_;
          }
          last_error_.clear();
        }
        continue;
      }

      if (ev.type == kDeviceSyncEventSendPrivate) {
        if (ev.conv_id.empty() || ev.envelope.empty()) {
          continue;
        }
        std::uint8_t type = 0;
        std::array<std::uint8_t, 16> msg_id{};
        std::size_t off = 0;
        if (!DecodeChatHeader(ev.envelope, type, msg_id, off)) {
          continue;
        }
        const std::string id_hex =
            BytesToHexLower(msg_id.data(), msg_id.size());

        const bool can_sync_out =
            (type == kChatTypeText || type == kChatTypeFile || type == kChatTypeRich ||
             type == kChatTypeSticker);

        const std::string saved_err = last_error_;
        const bool sent = SendPrivateE2ee(ev.conv_id, ev.envelope);
        last_error_ = saved_err;
        if (!sent) {
          continue;
        }
        BestEffortPersistHistoryEnvelope(false, true, ev.conv_id, username_,
                                        ev.envelope, HistoryStatus::kSent,
                                        NowUnixSeconds());

        if (type == kChatTypeText) {
          std::string text;
          if (!mi::server::proto::ReadString(ev.envelope, off, text) ||
              off != ev.envelope.size()) {
            continue;
          }
          OutgoingChatTextMessage t;
          t.peer_username = ev.conv_id;
          t.message_id_hex = id_hex;
          t.text_utf8 = std::move(text);
          result.outgoing_texts.push_back(std::move(t));
        } else if (type == kChatTypeFile) {
          std::uint64_t file_size = 0;
          std::string file_name;
          std::string file_id;
          std::array<std::uint8_t, 32> file_key{};
          if (!DecodeChatFile(ev.envelope, off, file_size, file_name, file_id,
                              file_key) ||
              off != ev.envelope.size()) {
            continue;
          }
          OutgoingChatFileMessage f;
          f.peer_username = ev.conv_id;
          f.message_id_hex = id_hex;
          f.file_id = std::move(file_id);
          f.file_key = file_key;
          f.file_name = std::move(file_name);
          f.file_size = file_size;
          result.outgoing_files.push_back(std::move(f));
        } else if (type == kChatTypeRich) {
          RichDecoded rich;
          if (!DecodeChatRich(ev.envelope, off, rich) || off != ev.envelope.size()) {
            continue;
          }
          OutgoingChatTextMessage t;
          t.peer_username = ev.conv_id;
          t.message_id_hex = id_hex;
          t.text_utf8 = FormatRichAsText(rich);
          result.outgoing_texts.push_back(std::move(t));
        } else if (type == kChatTypeSticker) {
          std::string sticker_id;
          if (!mi::server::proto::ReadString(ev.envelope, off, sticker_id) ||
              off != ev.envelope.size()) {
            continue;
          }
          OutgoingChatStickerMessage s;
          s.peer_username = ev.conv_id;
          s.message_id_hex = id_hex;
          s.sticker_id = std::move(sticker_id);
          result.outgoing_stickers.push_back(std::move(s));
        }

        if (can_sync_out) {
          BestEffortBroadcastDeviceSyncMessage(false, true, ev.conv_id, username_,
                                              ev.envelope);
        }
        continue;
      }

      if (ev.type == kDeviceSyncEventSendGroup) {
        if (ev.conv_id.empty() || ev.envelope.empty()) {
          continue;
        }
        std::uint8_t type = 0;
        std::array<std::uint8_t, 16> msg_id{};
        std::size_t off = 0;
        if (!DecodeChatHeader(ev.envelope, type, msg_id, off)) {
          continue;
        }
        const std::string id_hex =
            BytesToHexLower(msg_id.data(), msg_id.size());
        const bool can_sync_out =
            (type == kChatTypeGroupText || type == kChatTypeGroupFile);
        if (!can_sync_out) {
          continue;
        }

        std::string warn;
        const std::string saved_err = last_error_;
        const bool sent = send_group_envelope(ev.conv_id, ev.envelope, warn);
        last_error_ = saved_err;
        if (!sent) {
          continue;
        }
        BestEffortPersistHistoryEnvelope(true, true, ev.conv_id, username_,
                                        ev.envelope, HistoryStatus::kSent,
                                        NowUnixSeconds());

        if (!id_hex.empty()) {
          const auto map_it = group_delivery_map_.find(id_hex);
          if (map_it == group_delivery_map_.end()) {
            group_delivery_map_[id_hex] = ev.conv_id;
            group_delivery_order_.push_back(id_hex);
            while (group_delivery_order_.size() > kChatSeenLimit) {
              group_delivery_map_.erase(group_delivery_order_.front());
              group_delivery_order_.pop_front();
            }
          } else {
            map_it->second = ev.conv_id;
          }
        }

        if (type == kChatTypeGroupText) {
          std::string group_id;
          std::string text;
          if (!mi::server::proto::ReadString(ev.envelope, off, group_id) ||
              !mi::server::proto::ReadString(ev.envelope, off, text) ||
              off != ev.envelope.size() || group_id != ev.conv_id) {
            continue;
          }
          OutgoingGroupChatTextMessage t;
          t.group_id = group_id;
          t.message_id_hex = id_hex;
          t.text_utf8 = std::move(text);
          result.outgoing_group_texts.push_back(std::move(t));
        } else if (type == kChatTypeGroupFile) {
          std::string group_id;
          std::uint64_t file_size = 0;
          std::string file_name;
          std::string file_id;
          std::array<std::uint8_t, 32> file_key{};
          if (!DecodeChatGroupFile(ev.envelope, off, group_id, file_size,
                                   file_name, file_id, file_key) ||
              off != ev.envelope.size() || group_id != ev.conv_id) {
            continue;
          }
          OutgoingGroupChatFileMessage f;
          f.group_id = group_id;
          f.message_id_hex = id_hex;
          f.file_id = std::move(file_id);
          f.file_key = file_key;
          f.file_name = std::move(file_name);
          f.file_size = file_size;
          result.outgoing_group_files.push_back(std::move(f));
        }

        BestEffortBroadcastDeviceSyncMessage(true, true, ev.conv_id, username_,
                                            ev.envelope);
        continue;
      }
    }
  }

  std::string group_notice_err;
  const std::string saved_poll_err = last_error_;
  const auto group_notice_msgs = PullGroupNoticeMessages();
  if (!last_error_.empty()) {
    group_notice_err = last_error_;
  }
  last_error_ = saved_poll_err;
  if (sync_err.empty() && saved_poll_err.empty() && !group_notice_err.empty()) {
    sync_err = group_notice_err;
  }

  if (!group_notice_msgs.empty()) {
    const auto broadcast_notice = [&](const std::string& group_id,
                                      const std::string& actor_username,
                                      const std::vector<std::uint8_t>& payload) {
      if (!device_sync_enabled_ || !device_sync_is_primary_) {
        return;
      }
      const std::string saved_err = last_error_;
      if (!device_sync_key_loaded_ && !LoadDeviceSyncKey()) {
        last_error_ = saved_err;
        return;
      }
      std::vector<std::uint8_t> event_plain;
      if (!EncodeDeviceSyncGroupNotice(group_id, actor_username, payload,
                                       event_plain)) {
        last_error_ = saved_err;
        return;
      }
      std::vector<std::uint8_t> event_cipher;
      if (!EncryptDeviceSync(event_plain, event_cipher)) {
        last_error_ = saved_err;
        return;
      }
      PushDeviceSyncCiphertext(event_cipher);
      last_error_ = saved_err;
    };

    for (const auto& m : group_notice_msgs) {
      if (m.group_id.empty() || m.sender_username.empty() || m.payload.empty()) {
        continue;
      }
      std::uint8_t kind = 0;
      std::string target;
      std::optional<std::uint8_t> role;
      if (!DecodeGroupNoticePayload(m.payload, kind, target, role)) {
        continue;
      }

      GroupNotice n;
      n.group_id = m.group_id;
      n.kind = kind;
      n.actor_username = m.sender_username;
      n.target_username = std::move(target);
      if (role.has_value()) {
        const std::uint8_t rb = role.value();
        if (rb <= static_cast<std::uint8_t>(GroupMemberRole::kMember)) {
          n.role = static_cast<GroupMemberRole>(rb);
        }
      }
      result.group_notices.push_back(std::move(n));

      broadcast_notice(m.group_id, m.sender_username, m.payload);

      if (kind == kGroupNoticeJoin || kind == kGroupNoticeLeave ||
          kind == kGroupNoticeKick) {
        group_membership_dirty_.insert(m.group_id);
      }
    }
  }

  if (!group_membership_dirty_.empty()) {
    std::vector<std::string> pending;
    pending.reserve(group_membership_dirty_.size());
    for (const auto& gid : group_membership_dirty_) {
      pending.push_back(gid);
    }

    std::size_t attempt = 0;
    for (const auto& gid : pending) {
      if (++attempt > 16) {
        break;
      }
      const std::string saved_err = last_error_;
      const auto members = ListGroupMembers(gid);
      const std::string list_err = last_error_;
      if (members.empty()) {
        if (list_err == "not in group") {
          group_membership_dirty_.erase(gid);
        }
        last_error_ = saved_err;
        continue;
      }
      GroupSenderKeyState* sender_key = nullptr;
      std::string warn;
      const bool ok = EnsureGroupSenderKeyForSend(gid, members, sender_key, warn);
      if (ok && sender_key) {
        group_membership_dirty_.erase(gid);
      }
      last_error_ = saved_err;
    }
  }

  const auto pulled = PullPrivateE2ee();
  const std::string pull_err = last_error_;
  const auto ready = DrainReadyPrivateE2ee();
  const std::string ready_err = last_error_;
  last_error_ = !ready_err.empty() ? ready_err : pull_err;

  const auto handle = [&](const mi::client::e2ee::PrivateMessage& msg) {
    if (msg.from_username.empty()) {
      return;
    }
    std::uint8_t type = 0;
    std::array<std::uint8_t, 16> msg_id{};
    std::size_t off = 0;
    if (!DecodeChatHeader(msg.plaintext, type, msg_id, off)) {
      // Legacy plaintext: forward as best-effort utf8 text.
      ChatTextMessage t;
      t.from_username = msg.from_username;
      t.text_utf8.assign(reinterpret_cast<const char*>(msg.plaintext.data()),
                         msg.plaintext.size());
      result.texts.push_back(std::move(t));
      return;
    }

    const std::string id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
    if (type == kChatTypeAck) {
      if (off != msg.plaintext.size()) {
        return;
      }
      const auto pending_it = pending_sender_key_dists_.find(id_hex);
      if (pending_it != pending_sender_key_dists_.end()) {
        pending_it->second.pending_members.erase(msg.from_username);
        if (pending_it->second.pending_members.empty()) {
          pending_sender_key_dists_.erase(pending_it);
        }
        return;
      }
      ChatDelivery d;
      d.from_username = msg.from_username;
      d.message_id_hex = id_hex;
      result.deliveries.push_back(std::move(d));
      bool delivery_is_group = false;
      std::string delivery_conv = msg.from_username;
      const auto g_it = group_delivery_map_.find(id_hex);
      if (g_it != group_delivery_map_.end()) {
        delivery_is_group = true;
        delivery_conv = g_it->second;
      }
      BestEffortBroadcastDeviceSyncDelivery(delivery_is_group, delivery_conv,
                                           msg_id, false);
      return;
    }

    if (type == kChatTypeReadReceipt) {
      if (off != msg.plaintext.size()) {
        return;
      }
      ChatReadReceipt r;
      r.from_username = msg.from_username;
      r.message_id_hex = id_hex;
      result.read_receipts.push_back(std::move(r));
      BestEffortBroadcastDeviceSyncDelivery(false, msg.from_username, msg_id, true);
      return;
    }

    if (type == kChatTypeTyping) {
      if (off >= msg.plaintext.size()) {
        return;
      }
      const std::uint8_t state = msg.plaintext[off++];
      if (off != msg.plaintext.size()) {
        return;
      }
      ChatTypingEvent te;
      te.from_username = msg.from_username;
      te.typing = state != 0;
      result.typing_events.push_back(std::move(te));
      BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypePresence) {
      if (off >= msg.plaintext.size()) {
        return;
      }
      const std::uint8_t state = msg.plaintext[off++];
      if (off != msg.plaintext.size()) {
        return;
      }
      ChatPresenceEvent pe;
      pe.from_username = msg.from_username;
      pe.online = state != 0;
      result.presence_events.push_back(std::move(pe));
      BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeGroupSenderKeyDist) {
      std::string group_id;
      std::uint32_t version = 0;
      std::uint32_t iteration = 0;
      std::array<std::uint8_t, 32> ck{};
      std::vector<std::uint8_t> sig;
      if (!DecodeChatGroupSenderKeyDist(msg.plaintext, off, group_id, version,
                                        iteration, ck, sig) ||
          off != msg.plaintext.size()) {
        return;
      }
      if (group_id.empty() || version == 0 || sig.empty()) {
        return;
      }

      CachedPeerIdentity peer;
      if (!GetPeerIdentityCached(msg.from_username, peer, true)) {
        return;
      }
      const auto sig_msg =
          BuildGroupSenderKeyDistSigMessage(group_id, version, iteration, ck);
      std::string ver_err;
      if (!mi::client::e2ee::Engine::VerifyDetached(sig_msg, sig, peer.id_sig_pk,
                                                    ver_err)) {
        return;
      }

      const std::string key =
          MakeGroupSenderKeyMapKey(group_id, msg.from_username);
      auto& state = group_sender_keys_[key];
      const bool have_key =
          (state.version != 0 && !IsAllZero(state.ck.data(), state.ck.size()));
      const bool accept =
          (!have_key || version > state.version ||
           (version == state.version && iteration >= state.next_iteration));
      if (accept) {
        state.group_id = group_id;
        state.sender_username = msg.from_username;
        state.version = version;
        state.next_iteration = iteration;
        state.ck = ck;
        state.members_hash.clear();
        state.rotated_at = NowUnixSeconds();
        state.sent_count = 0;
        state.skipped_mks.clear();
        state.skipped_order.clear();
      }

      std::vector<std::uint8_t> ack;
      if (EncodeChatAck(msg_id, ack)) {
        const std::string saved_err = last_error_;
        SendPrivateE2ee(msg.from_username, ack);
        last_error_ = saved_err;
      }
      return;
    }

    if (type == kChatTypeGroupSenderKeyReq) {
      std::string group_id;
      std::uint32_t want_version = 0;
      if (!DecodeChatGroupSenderKeyReq(msg.plaintext, off, group_id,
                                       want_version) ||
          off != msg.plaintext.size()) {
        return;
      }
      if (group_id.empty()) {
        return;
      }

      const std::string map_key = MakeGroupSenderKeyMapKey(group_id, username_);
      const auto it = group_sender_keys_.find(map_key);
      if (it == group_sender_keys_.end() || it->second.version == 0 ||
          IsAllZero(it->second.ck.data(), it->second.ck.size())) {
        return;
      }
      if (want_version != 0 && it->second.version < want_version) {
        return;
      }

      {
        const std::string saved_err = last_error_;
        const auto members = ListGroupMembers(group_id);
        last_error_ = saved_err;
        bool allowed = false;
        for (const auto& m : members) {
          if (m == msg.from_username) {
            allowed = true;
            break;
          }
        }
        if (!allowed) {
          return;
        }
      }

      std::array<std::uint8_t, 16> dist_id{};
      if (!RandomBytes(dist_id.data(), dist_id.size())) {
        return;
      }
      const std::string dist_id_hex =
          BytesToHexLower(dist_id.data(), dist_id.size());

      const auto sig_msg = BuildGroupSenderKeyDistSigMessage(
          group_id, it->second.version, it->second.next_iteration, it->second.ck);
      std::vector<std::uint8_t> sig;
      std::string sig_err;
      if (!e2ee_.SignDetached(sig_msg, sig, sig_err)) {
        return;
      }

      std::vector<std::uint8_t> dist_envelope;
      if (!EncodeChatGroupSenderKeyDist(dist_id, group_id, it->second.version,
                                        it->second.next_iteration, it->second.ck,
                                        sig, dist_envelope)) {
        return;
      }

      PendingSenderKeyDistribution pending;
      pending.group_id = group_id;
      pending.version = it->second.version;
      pending.envelope = dist_envelope;
      pending.last_sent = std::chrono::steady_clock::now();
      pending.pending_members.insert(msg.from_username);
      pending_sender_key_dists_[dist_id_hex] = std::move(pending);

      const std::string saved_err = last_error_;
      SendPrivateE2ee(msg.from_username, dist_envelope);
      last_error_ = saved_err;
      return;
    }

    if (type == kChatTypeGroupCallKeyDist) {
      std::string group_id;
      std::array<std::uint8_t, 16> call_id{};
      std::uint32_t key_id = 0;
      std::array<std::uint8_t, 32> call_key{};
      std::vector<std::uint8_t> sig;
      if (!DecodeChatGroupCallKeyDist(msg.plaintext, off, group_id, call_id,
                                      key_id, call_key, sig) ||
          off != msg.plaintext.size()) {
        return;
      }
      if (group_id.empty() || key_id == 0 || sig.empty()) {
        return;
      }

      CachedPeerIdentity peer;
      if (!GetPeerIdentityCached(msg.from_username, peer, true)) {
        return;
      }
      const auto sig_msg =
          BuildGroupCallKeyDistSigMessage(group_id, call_id, key_id, call_key);
      std::string ver_err;
      if (!mi::client::e2ee::Engine::VerifyDetached(sig_msg, sig,
                                                    peer.id_sig_pk, ver_err)) {
        return;
      }

      const std::string map_key = MakeGroupCallKeyMapKey(group_id, call_id);
      const auto it = group_call_keys_.find(map_key);
      const bool accept = (it == group_call_keys_.end() ||
                           it->second.key_id == 0 ||
                           key_id >= it->second.key_id);
      if (accept) {
        StoreGroupCallKey(group_id, call_id, key_id, call_key);
      }

      std::vector<std::uint8_t> ack;
      if (EncodeChatAck(msg_id, ack)) {
        const std::string saved_err = last_error_;
        SendPrivateE2ee(msg.from_username, ack);
        last_error_ = saved_err;
      }
      return;
    }

    if (type == kChatTypeGroupCallKeyReq) {
      std::string group_id;
      std::array<std::uint8_t, 16> call_id{};
      std::uint32_t want_key_id = 0;
      if (!DecodeChatGroupCallKeyReq(msg.plaintext, off, group_id, call_id,
                                     want_key_id) ||
          off != msg.plaintext.size()) {
        return;
      }
      if (group_id.empty() || want_key_id == 0) {
        return;
      }
      std::array<std::uint8_t, 32> call_key{};
      if (!LookupGroupCallKey(group_id, call_id, want_key_id, call_key)) {
        return;
      }

      {
        const std::string saved_err = last_error_;
        const auto members = ListGroupMembers(group_id);
        last_error_ = saved_err;
        bool allowed = false;
        for (const auto& m : members) {
          if (m == msg.from_username) {
            allowed = true;
            break;
          }
        }
        if (!allowed) {
          return;
        }
      }

      std::array<std::uint8_t, 16> dist_id{};
      if (!RandomBytes(dist_id.data(), dist_id.size())) {
        return;
      }
      const auto sig_msg =
          BuildGroupCallKeyDistSigMessage(group_id, call_id, want_key_id, call_key);
      std::vector<std::uint8_t> sig;
      std::string sig_err;
      if (!e2ee_.SignDetached(sig_msg, sig, sig_err)) {
        return;
      }
      std::vector<std::uint8_t> envelope;
      if (!EncodeChatGroupCallKeyDist(dist_id, group_id, call_id, want_key_id,
                                      call_key, sig, envelope)) {
        return;
      }
      const std::string saved_err = last_error_;
      SendGroupSenderKeyEnvelope(group_id, msg.from_username, envelope);
      last_error_ = saved_err;
      return;
    }

    const bool known_type =
        (type == kChatTypeText || type == kChatTypeFile || type == kChatTypeRich ||
         type == kChatTypeSticker || type == kChatTypeGroupText ||
         type == kChatTypeGroupInvite || type == kChatTypeGroupFile);
    if (!known_type) {
      return;
    }

    // Send delivery ack (best effort).
    std::vector<std::uint8_t> ack;
    if (EncodeChatAck(msg_id, ack)) {
      const std::string saved_err = last_error_;
      SendPrivateE2ee(msg.from_username, ack);
      last_error_ = saved_err;
    }

    const std::string seen_key = msg.from_username + "|" + id_hex;
    if (chat_seen_ids_.find(seen_key) != chat_seen_ids_.end()) {
      return;
    }
    chat_seen_ids_.insert(seen_key);
    chat_seen_order_.push_back(seen_key);
    while (chat_seen_order_.size() > kChatSeenLimit) {
      chat_seen_ids_.erase(chat_seen_order_.front());
      chat_seen_order_.pop_front();
    }

    if (type == kChatTypeText) {
      std::string text;
      if (!mi::server::proto::ReadString(msg.plaintext, off, text) ||
          off != msg.plaintext.size()) {
        return;
      }
      ChatTextMessage t;
      t.from_username = msg.from_username;
      t.message_id_hex = id_hex;
      t.text_utf8 = std::move(text);
      result.texts.push_back(std::move(t));
      BestEffortPersistHistoryEnvelope(false, false, msg.from_username,
                                      msg.from_username, msg.plaintext,
                                      HistoryStatus::kSent, NowUnixSeconds());
      BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeRich) {
      RichDecoded rich;
      if (!DecodeChatRich(msg.plaintext, off, rich) || off != msg.plaintext.size()) {
        return;
      }
      ChatTextMessage t;
      t.from_username = msg.from_username;
      t.message_id_hex = id_hex;
      t.text_utf8 = FormatRichAsText(rich);
      result.texts.push_back(std::move(t));
      BestEffortPersistHistoryEnvelope(false, false, msg.from_username,
                                      msg.from_username, msg.plaintext,
                                      HistoryStatus::kSent, NowUnixSeconds());
      BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeFile) {
      std::uint64_t file_size = 0;
      std::string file_name;
      std::string file_id;
      std::array<std::uint8_t, 32> file_key{};
      if (!DecodeChatFile(msg.plaintext, off, file_size, file_name, file_id,
                          file_key) ||
          off != msg.plaintext.size()) {
        return;
      }
      ChatFileMessage f;
      f.from_username = msg.from_username;
      f.message_id_hex = id_hex;
      f.file_id = std::move(file_id);
      f.file_key = file_key;
      f.file_name = std::move(file_name);
      f.file_size = file_size;
      result.files.push_back(std::move(f));
      BestEffortPersistHistoryEnvelope(false, false, msg.from_username,
                                      msg.from_username, msg.plaintext,
                                      HistoryStatus::kSent, NowUnixSeconds());
      BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeSticker) {
      std::string sticker_id;
      if (!mi::server::proto::ReadString(msg.plaintext, off, sticker_id) ||
          off != msg.plaintext.size()) {
        return;
      }
      ChatStickerMessage s;
      s.from_username = msg.from_username;
      s.message_id_hex = id_hex;
      s.sticker_id = std::move(sticker_id);
      result.stickers.push_back(std::move(s));
      BestEffortPersistHistoryEnvelope(false, false, msg.from_username,
                                      msg.from_username, msg.plaintext,
                                      HistoryStatus::kSent, NowUnixSeconds());
      BestEffortBroadcastDeviceSyncMessage(false, false, msg.from_username,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeGroupText) {
      std::string group_id;
      std::string text;
      if (!mi::server::proto::ReadString(msg.plaintext, off, group_id) ||
          !mi::server::proto::ReadString(msg.plaintext, off, text) ||
          off != msg.plaintext.size()) {
        return;
      }
      GroupChatTextMessage t;
      t.group_id = group_id;
      t.from_username = msg.from_username;
      t.message_id_hex = id_hex;
      t.text_utf8 = std::move(text);
      result.group_texts.push_back(std::move(t));
      BestEffortPersistHistoryEnvelope(true, false, group_id, msg.from_username,
                                      msg.plaintext, HistoryStatus::kSent,
                                      NowUnixSeconds());
      BestEffortBroadcastDeviceSyncMessage(true, false, group_id,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeGroupFile) {
      std::string group_id;
      std::uint64_t file_size = 0;
      std::string file_name;
      std::string file_id;
      std::array<std::uint8_t, 32> file_key{};
      if (!DecodeChatGroupFile(msg.plaintext, off, group_id, file_size,
                               file_name, file_id, file_key) ||
          off != msg.plaintext.size()) {
        return;
      }
      GroupChatFileMessage f;
      f.group_id = group_id;
      f.from_username = msg.from_username;
      f.message_id_hex = id_hex;
      f.file_id = std::move(file_id);
      f.file_key = file_key;
      f.file_name = std::move(file_name);
      f.file_size = file_size;
      result.group_files.push_back(std::move(f));
      BestEffortPersistHistoryEnvelope(true, false, group_id, msg.from_username,
                                      msg.plaintext, HistoryStatus::kSent,
                                      NowUnixSeconds());
      BestEffortBroadcastDeviceSyncMessage(true, false, group_id,
                                          msg.from_username, msg.plaintext);
      return;
    }

    if (type == kChatTypeGroupInvite) {
      std::string group_id;
      if (!mi::server::proto::ReadString(msg.plaintext, off, group_id) ||
          off != msg.plaintext.size()) {
        return;
      }
      GroupInviteMessage inv;
      inv.group_id = std::move(group_id);
      inv.from_username = msg.from_username;
      inv.message_id_hex = id_hex;
      result.group_invites.push_back(std::move(inv));
      BestEffortBroadcastDeviceSyncMessage(true, false, inv.group_id,
                                          msg.from_username, msg.plaintext);
      return;
    }
  };

  for (const auto& m : pulled) {
    handle(m);
  }
  for (const auto& m : ready) {
    handle(m);
  }

  const std::string poll_err = last_error_;
  auto group_msgs = PullGroupCipherMessages();
  const std::string group_err = last_error_;
  last_error_ = !poll_err.empty() ? poll_err : group_err;

  std::deque<PendingGroupCipher> work;
  work.swap(pending_group_cipher_);
  for (auto& m : group_msgs) {
    work.push_back(std::move(m));
  }

  const auto now = std::chrono::steady_clock::now();
  const auto send_key_req = [&](const std::string& group_id,
                                const std::string& sender_username,
                                std::uint32_t want_version) {
    const std::string req_key =
        group_id + "|" + sender_username + "|" + std::to_string(want_version);
    const auto it = sender_key_req_last_sent_.find(req_key);
    if (it != sender_key_req_last_sent_.end() &&
        now - it->second < std::chrono::seconds(3)) {
      return;
    }
    sender_key_req_last_sent_[req_key] = now;
    if (sender_key_req_last_sent_.size() > 4096) {
      sender_key_req_last_sent_.clear();
    }

    std::array<std::uint8_t, 16> req_id{};
    if (!RandomBytes(req_id.data(), req_id.size())) {
      return;
    }
    std::vector<std::uint8_t> req;
    if (!EncodeChatGroupSenderKeyReq(req_id, group_id, want_version, req)) {
      return;
    }
    const std::string saved_err = last_error_;
    SendPrivateE2ee(sender_username, req);
    last_error_ = saved_err;
  };

  for (auto& m : work) {
    std::uint32_t sender_key_version = 0;
    std::uint32_t sender_key_iteration = 0;
    std::string group_id;
    std::string sender_username;
    std::array<std::uint8_t, 24> nonce{};
    std::array<std::uint8_t, 16> mac{};
    std::vector<std::uint8_t> cipher;
    std::vector<std::uint8_t> sig;
    std::size_t sig_offset = 0;
    if (!DecodeGroupCipher(m.payload, sender_key_version, sender_key_iteration,
                           group_id, sender_username, nonce, mac, cipher, sig,
                           sig_offset)) {
      continue;
    }
    if ((!m.group_id.empty() && group_id != m.group_id) ||
        (!m.sender_username.empty() && sender_username != m.sender_username)) {
      continue;
    }
    if (group_id.empty() || sender_username.empty() || sig.empty() ||
        sig_offset == 0 || sig_offset > m.payload.size()) {
      continue;
    }

    CachedPeerIdentity peer;
    if (!GetPeerIdentityCached(sender_username, peer, true)) {
      pending_group_cipher_.push_back(std::move(m));
      continue;
    }

    std::vector<std::uint8_t> signed_part;
    signed_part.assign(m.payload.begin(), m.payload.begin() + sig_offset);
    std::string sig_err;
    if (!mi::client::e2ee::Engine::VerifyDetached(signed_part, sig, peer.id_sig_pk,
                                                  sig_err)) {
      continue;
    }

    const std::string key = MakeGroupSenderKeyMapKey(group_id, sender_username);
    auto it = group_sender_keys_.find(key);
    if (it == group_sender_keys_.end() || it->second.version == 0 ||
        IsAllZero(it->second.ck.data(), it->second.ck.size()) ||
        it->second.version < sender_key_version) {
      send_key_req(group_id, sender_username, sender_key_version);
      pending_group_cipher_.push_back(std::move(m));
      continue;
    }
    if (it->second.version > sender_key_version) {
      continue;
    }

    GroupSenderKeyState tmp = it->second;
    std::array<std::uint8_t, 32> mk{};
    if (!DeriveGroupMessageKey(tmp, sender_key_iteration, mk)) {
      send_key_req(group_id, sender_username, sender_key_version);
      continue;
    }

    std::vector<std::uint8_t> ad;
    BuildGroupCipherAd(group_id, sender_username, sender_key_version,
                       sender_key_iteration, ad);

    std::vector<std::uint8_t> plain;
    plain.resize(cipher.size());
    const int ok = crypto_aead_unlock(plain.data(), mac.data(), mk.data(),
                                      nonce.data(), ad.data(), ad.size(),
                                      cipher.data(), cipher.size());
    if (ok != 0) {
      crypto_wipe(plain.data(), plain.size());
      send_key_req(group_id, sender_username, sender_key_version);
      continue;
    }
    std::vector<std::uint8_t> unpadded;
    std::string pad_err;
    if (!UnpadPayload(plain, unpadded, pad_err)) {
      crypto_wipe(plain.data(), plain.size());
      continue;
    }
    crypto_wipe(plain.data(), plain.size());
    plain = std::move(unpadded);
    it->second = std::move(tmp);

    std::uint8_t type = 0;
    std::array<std::uint8_t, 16> msg_id{};
    std::size_t off = 0;
    if (!DecodeChatHeader(plain, type, msg_id, off)) {
      crypto_wipe(plain.data(), plain.size());
      continue;
    }

    std::vector<std::uint8_t> ack;
    if (EncodeChatAck(msg_id, ack)) {
      const std::string saved_err = last_error_;
      SendPrivateE2ee(sender_username, ack);
      last_error_ = saved_err;
    }

    const std::string id_hex = BytesToHexLower(msg_id.data(), msg_id.size());
    const std::string seen_key = group_id + "|" + sender_username + "|" + id_hex;
    if (chat_seen_ids_.find(seen_key) != chat_seen_ids_.end()) {
      crypto_wipe(plain.data(), plain.size());
      continue;
    }
    chat_seen_ids_.insert(seen_key);
    chat_seen_order_.push_back(seen_key);
    while (chat_seen_order_.size() > kChatSeenLimit) {
      chat_seen_ids_.erase(chat_seen_order_.front());
      chat_seen_order_.pop_front();
    }

    if (type == kChatTypeGroupText) {
      std::string inner_group_id;
      std::string text;
      if (!mi::server::proto::ReadString(plain, off, inner_group_id) ||
          !mi::server::proto::ReadString(plain, off, text) ||
          off != plain.size() || inner_group_id != group_id) {
        crypto_wipe(plain.data(), plain.size());
        continue;
      }
      GroupChatTextMessage t;
      t.group_id = group_id;
      t.from_username = sender_username;
      t.message_id_hex = id_hex;
      t.text_utf8 = std::move(text);
      result.group_texts.push_back(std::move(t));
      BestEffortPersistHistoryEnvelope(true, false, group_id, sender_username,
                                      plain, HistoryStatus::kSent,
                                      NowUnixSeconds());
      BestEffortBroadcastDeviceSyncMessage(true, false, group_id, sender_username,
                                          plain);
    } else if (type == kChatTypeGroupFile) {
      std::string inner_group_id;
      std::uint64_t file_size = 0;
      std::string file_name;
      std::string file_id;
      std::array<std::uint8_t, 32> file_key{};
      if (!DecodeChatGroupFile(plain, off, inner_group_id, file_size, file_name,
                               file_id, file_key) ||
          off != plain.size() || inner_group_id != group_id) {
        crypto_wipe(plain.data(), plain.size());
        continue;
      }
      GroupChatFileMessage f;
      f.group_id = group_id;
      f.from_username = sender_username;
      f.message_id_hex = id_hex;
      f.file_id = std::move(file_id);
      f.file_key = file_key;
      f.file_name = std::move(file_name);
      f.file_size = file_size;
      result.group_files.push_back(std::move(f));
      BestEffortPersistHistoryEnvelope(true, false, group_id, sender_username,
                                      plain, HistoryStatus::kSent,
                                      NowUnixSeconds());
      BestEffortBroadcastDeviceSyncMessage(true, false, group_id, sender_username,
                                          plain);
    }

    crypto_wipe(plain.data(), plain.size());
  }

  while (pending_group_cipher_.size() > kPendingGroupCipherLimit) {
    pending_group_cipher_.pop_front();
  }

  if (last_error_.empty() && !sync_err.empty()) {
    last_error_ = sync_err;
  }
  return result;
}

bool ClientCore::DownloadChatFileToPath(
    const ChatFileMessage& file,
    const std::filesystem::path& out_path,
    bool wipe_after_read,
    const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) {
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (file.file_id.empty()) {
    last_error_ = "file id empty";
    return false;
  }
  if (out_path.empty()) {
    last_error_ = "output path empty";
    return false;
  }

  if (file.file_size > (8u * 1024u * 1024u)) {
    const bool ok =
        DownloadE2eeFileBlobV3ToPath(file.file_id, file.file_key, out_path,
                                     wipe_after_read, on_progress);
    if (ok) {
      BestEffortStoreAttachmentPreviewFromPath(file.file_id, file.file_name,
                                               file.file_size, out_path);
    }
    return ok;
  }

  std::vector<std::uint8_t> blob;
  if (!DownloadE2eeFileBlob(file.file_id, blob, wipe_after_read, on_progress)) {
    return false;
  }

  std::vector<std::uint8_t> plaintext;
  if (!DecryptFileBlob(blob, file.file_key, plaintext)) {
    last_error_ = "file decrypt failed";
    return false;
  }
  BestEffortStoreAttachmentPreviewBytes(file.file_id, file.file_name,
                                        file.file_size, plaintext);

  std::error_code ec;
  const auto parent =
      out_path.has_parent_path() ? out_path.parent_path() : std::filesystem::path{};
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
  }
  std::ofstream ofs(out_path, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    last_error_ = "open output file failed";
    return false;
  }
  ofs.write(reinterpret_cast<const char*>(plaintext.data()),
            static_cast<std::streamsize>(plaintext.size()));
  if (!ofs) {
    last_error_ = "write output file failed";
    return false;
  }
  ofs.close();
  return true;
}

bool ClientCore::DownloadChatFileToBytes(const ChatFileMessage& file,
                                        std::vector<std::uint8_t>& out_bytes,
                                        bool wipe_after_read) {
  out_bytes.clear();
  last_error_.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (file.file_id.empty()) {
    last_error_ = "file id empty";
    return false;
  }

  std::vector<std::uint8_t> blob;
  if (!DownloadE2eeFileBlob(file.file_id, blob, wipe_after_read, nullptr)) {
    return false;
  }

  std::vector<std::uint8_t> plaintext;
  if (!DecryptFileBlob(blob, file.file_key, plaintext)) {
    last_error_ = "file decrypt failed";
    return false;
  }

  out_bytes = std::move(plaintext);
  BestEffortStoreAttachmentPreviewBytes(file.file_id, file.file_name,
                                        file.file_size, out_bytes);
  return true;
}

std::vector<ClientCore::HistoryEntry> ClientCore::LoadChatHistory(
    const std::string& conv_id, bool is_group, std::size_t limit) {
  std::vector<HistoryEntry> out;
  last_error_.clear();
  if (!history_store_) {
    return out;
  }
  if (conv_id.empty()) {
    last_error_ = "conv id empty";
    return out;
  }

  std::vector<ChatHistoryMessage> msgs;
  std::string err;
  if (!history_store_->LoadConversation(is_group, conv_id, limit, msgs, err)) {
    last_error_ = err.empty() ? "history load failed" : err;
    return out;
  }

  out.reserve(msgs.size());
  for (auto& m : msgs) {
    HistoryEntry e;
    e.is_group = is_group;
    e.outgoing = m.outgoing;
    e.timestamp_sec = m.timestamp_sec;
    e.conv_id = conv_id;
    e.sender = m.sender;
    e.status = static_cast<HistoryStatus>(m.status);

    if (m.is_system) {
      e.kind = HistoryKind::kSystem;
      e.text_utf8 = std::move(m.system_text_utf8);
      out.push_back(std::move(e));
      continue;
    }

    const auto trySummary = [&]() -> bool {
      if (ApplyHistorySummary(m.summary, e)) {
        out.push_back(std::move(e));
        return true;
      }
      return false;
    };

    std::uint8_t type = 0;
    std::array<std::uint8_t, 16> msg_id{};
    std::size_t off = 0;
    if (!DecodeChatHeader(m.envelope, type, msg_id, off)) {
      (void)trySummary();
      continue;
    }
    e.message_id_hex = BytesToHexLower(msg_id.data(), msg_id.size());

    if (type == kChatTypeText) {
      std::string text;
      if (!mi::server::proto::ReadString(m.envelope, off, text) ||
          off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kText;
      e.text_utf8 = std::move(text);
      out.push_back(std::move(e));
      continue;
    }

    if (type == kChatTypeRich) {
      RichDecoded rich;
      if (!DecodeChatRich(m.envelope, off, rich) || off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kText;
      e.text_utf8 = FormatRichAsText(rich);
      out.push_back(std::move(e));
      continue;
    }

    if (type == kChatTypeFile) {
      std::uint64_t file_size = 0;
      std::string file_name;
      std::string file_id;
      std::array<std::uint8_t, 32> file_key{};
      if (!DecodeChatFile(m.envelope, off, file_size, file_name, file_id,
                          file_key) ||
          off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kFile;
      e.file_id = std::move(file_id);
      e.file_key = file_key;
      e.file_name = std::move(file_name);
      e.file_size = file_size;
      out.push_back(std::move(e));
      continue;
    }

    if (type == kChatTypeSticker) {
      std::string sticker_id;
      if (!mi::server::proto::ReadString(m.envelope, off, sticker_id) ||
          off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kSticker;
      e.sticker_id = std::move(sticker_id);
      out.push_back(std::move(e));
      continue;
    }

    if (type == kChatTypeGroupText) {
      std::string group_id;
      std::string text;
      if (!mi::server::proto::ReadString(m.envelope, off, group_id) ||
          !mi::server::proto::ReadString(m.envelope, off, text) ||
          off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kText;
      e.text_utf8 = std::move(text);
      out.push_back(std::move(e));
      continue;
    }

    if (type == kChatTypeGroupFile) {
      std::string group_id;
      std::uint64_t file_size = 0;
      std::string file_name;
      std::string file_id;
      std::array<std::uint8_t, 32> file_key{};
      if (!DecodeChatGroupFile(m.envelope, off, group_id, file_size, file_name,
                               file_id, file_key) ||
          off != m.envelope.size()) {
        (void)trySummary();
        continue;
      }
      e.kind = HistoryKind::kFile;
      e.file_id = std::move(file_id);
      e.file_key = file_key;
      e.file_name = std::move(file_name);
      e.file_size = file_size;
      out.push_back(std::move(e));
      continue;
    }

    (void)trySummary();
  }
  return out;
}

bool ClientCore::AddHistorySystemMessage(const std::string& conv_id,
                                        bool is_group,
                                        const std::string& text_utf8) {
  last_error_.clear();
  if (!history_store_) {
    return true;
  }
  if (conv_id.empty()) {
    last_error_ = "conv id empty";
    return false;
  }
  if (text_utf8.empty()) {
    last_error_ = "system text empty";
    return false;
  }
  std::string err;
  if (!history_store_->AppendSystem(is_group, conv_id, text_utf8, NowUnixSeconds(),
                                    err)) {
    last_error_ = err.empty() ? "history write failed" : err;
    return false;
  }
  return true;
}

void ClientCore::SetHistoryEnabled(bool enabled) {
  history_enabled_ = enabled;
  if (!history_enabled_) {
    history_store_.reset();
    return;
  }
  if (history_store_ || username_.empty() || e2ee_state_dir_.empty()) {
    return;
  }
  auto store = std::make_unique<ChatHistoryStore>();
  std::string hist_err;
  if (store->Init(e2ee_state_dir_, username_, hist_err)) {
    history_store_ = std::move(store);
    WarmupHistoryOnStartup();
  } else {
    history_store_.reset();
  }
}

bool ClientCore::ClearAllHistory(bool delete_attachments,
                                 bool secure_wipe,
                                 std::string& error) {
  error.clear();
  if (username_.empty() || e2ee_state_dir_.empty()) {
    error = "history user empty";
    return false;
  }
  if (history_store_) {
    if (!history_store_->ClearAll(delete_attachments, secure_wipe, error)) {
      return false;
    }
    history_store_.reset();
    return true;
  }
  auto store = std::make_unique<ChatHistoryStore>();
  if (!store->Init(e2ee_state_dir_, username_, error)) {
    return false;
  }
  if (!store->ClearAll(delete_attachments, secure_wipe, error)) {
    return false;
  }
  return true;
}

bool ClientCore::UploadE2eeFileBlob(const std::vector<std::uint8_t>& blob,
                                    std::string& out_file_id) {
  out_file_id.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (blob.empty()) {
    last_error_ = "empty payload";
    return false;
  }
  if (blob.size() > kMaxChatFileBlobBytes) {
    last_error_ = "payload too large";
    return false;
  }

  if (blob.size() > (8u * 1024u * 1024u)) {
    std::string file_id;
    std::string upload_id;
    if (!StartE2eeFileBlobUpload(static_cast<std::uint64_t>(blob.size()), file_id,
                                 upload_id)) {
      if (last_error_.empty()) {
        last_error_ = "file upload start failed";
      }
      return false;
    }

    std::uint64_t off = 0;
    while (off < static_cast<std::uint64_t>(blob.size())) {
      const std::size_t remaining =
          static_cast<std::size_t>(blob.size() - off);
      const std::size_t chunk_len =
          std::min<std::size_t>(remaining, kE2eeBlobChunkBytes);
      std::vector<std::uint8_t> chunk;
      chunk.assign(blob.data() + off, blob.data() + off + chunk_len);

      std::uint64_t received = 0;
      if (!UploadE2eeFileBlobChunk(file_id, upload_id, off, chunk, received)) {
        if (last_error_.empty()) {
          last_error_ = "file upload chunk failed";
        }
        return false;
      }
      if (received != off + chunk_len) {
        last_error_ = "file upload chunk response invalid";
        return false;
      }
      off = received;
    }

    if (!FinishE2eeFileBlobUpload(file_id, upload_id,
                                  static_cast<std::uint64_t>(blob.size()))) {
      if (last_error_.empty()) {
        last_error_ = "file upload finish failed";
      }
      return false;
    }
    out_file_id = std::move(file_id);
    return true;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteBytes(blob, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kE2eeFileUpload, plain,
                        resp_payload)) {
    if (last_error_.empty()) {
      last_error_ = "file upload failed";
    }
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "file upload response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ = server_err.empty() ? "file upload failed" : server_err;
    return false;
  }
  std::size_t off = 1;
  std::string file_id;
  std::uint64_t size = 0;
  if (!mi::server::proto::ReadString(resp_payload, off, file_id) ||
      !mi::server::proto::ReadUint64(resp_payload, off, size) ||
      off != resp_payload.size() || file_id.empty()) {
    last_error_ = "file upload response invalid";
    return false;
  }
  out_file_id = std::move(file_id);
  return true;
}

bool ClientCore::DownloadE2eeFileBlob(
    const std::string& file_id,
    std::vector<std::uint8_t>& out_blob,
    bool wipe_after_read,
    const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) {
  out_blob.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty()) {
    last_error_ = "file id empty";
    return false;
  }

  std::string download_id;
  std::uint64_t size = 0;
  if (!StartE2eeFileBlobDownload(file_id, wipe_after_read, download_id, size)) {
    if (last_error_.empty()) {
      last_error_ = "file download start failed";
    }
    return false;
  }

  if (size == 0 || size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    last_error_ = "file download response invalid";
    return false;
  }

  std::vector<std::uint8_t> blob;
  blob.reserve(static_cast<std::size_t>(size));
  if (on_progress) {
    on_progress(0, size);
  }

  std::uint64_t off = 0;
  bool eof = false;
  while (off < size) {
    const std::uint64_t remaining = size - off;
    const std::uint32_t max_len =
        static_cast<std::uint32_t>(
            std::min<std::uint64_t>(remaining, kE2eeBlobChunkBytes));
    std::vector<std::uint8_t> chunk;
    bool chunk_eof = false;
    if (!DownloadE2eeFileBlobChunk(file_id, download_id, off, max_len, chunk,
                                   chunk_eof)) {
      if (last_error_.empty()) {
        last_error_ = "file download chunk failed";
      }
      return false;
    }
    if (chunk.empty()) {
      last_error_ = "file download chunk empty";
      return false;
    }
    blob.insert(blob.end(), chunk.begin(), chunk.end());
    off += static_cast<std::uint64_t>(chunk.size());
    eof = chunk_eof;
    if (on_progress) {
      on_progress(off, size);
    }
    if (eof) {
      break;
    }
  }

  if (off != size || !eof || blob.size() != static_cast<std::size_t>(size)) {
    last_error_ = "file download incomplete";
    return false;
  }

  out_blob = std::move(blob);
  return true;
}

bool ClientCore::StartE2eeFileBlobUpload(std::uint64_t expected_size,
                                        std::string& out_file_id,
                                        std::string& out_upload_id) {
  out_file_id.clear();
  out_upload_id.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (expected_size == 0 ||
      expected_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    last_error_ = "payload too large";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteUint64(expected_size, plain);
  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kE2eeFileUploadStart, plain,
                        resp_payload)) {
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "file upload start response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ =
        server_err.empty() ? "file upload start failed" : server_err;
    return false;
  }

  std::size_t off = 1;
  std::string file_id;
  std::string upload_id;
  if (!mi::server::proto::ReadString(resp_payload, off, file_id) ||
      !mi::server::proto::ReadString(resp_payload, off, upload_id) ||
      off != resp_payload.size() || file_id.empty() || upload_id.empty()) {
    last_error_ = "file upload start response invalid";
    return false;
  }
  out_file_id = std::move(file_id);
  out_upload_id = std::move(upload_id);
  return true;
}

bool ClientCore::UploadE2eeFileBlobChunk(const std::string& file_id,
                                        const std::string& upload_id,
                                        std::uint64_t offset,
                                        const std::vector<std::uint8_t>& chunk,
                                        std::uint64_t& out_bytes_received) {
  out_bytes_received = 0;
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty() || upload_id.empty()) {
    last_error_ = "invalid session";
    return false;
  }
  if (chunk.empty()) {
    last_error_ = "empty payload";
    return false;
  }
  if (chunk.size() > kE2eeBlobChunkBytes) {
    last_error_ = "chunk too large";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(file_id, plain);
  mi::server::proto::WriteString(upload_id, plain);
  mi::server::proto::WriteUint64(offset, plain);
  mi::server::proto::WriteBytes(chunk, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kE2eeFileUploadChunk, plain,
                        resp_payload)) {
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "file upload chunk response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ =
        server_err.empty() ? "file upload chunk failed" : server_err;
    return false;
  }

  std::size_t off = 1;
  std::uint64_t received = 0;
  if (!mi::server::proto::ReadUint64(resp_payload, off, received) ||
      off != resp_payload.size()) {
    last_error_ = "file upload chunk response invalid";
    return false;
  }
  out_bytes_received = received;
  return true;
}

bool ClientCore::FinishE2eeFileBlobUpload(const std::string& file_id,
                                         const std::string& upload_id,
                                         std::uint64_t total_size) {
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty() || upload_id.empty()) {
    last_error_ = "invalid session";
    return false;
  }
  if (total_size == 0 ||
      total_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    last_error_ = "payload too large";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(file_id, plain);
  mi::server::proto::WriteString(upload_id, plain);
  mi::server::proto::WriteUint64(total_size, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kE2eeFileUploadFinish, plain,
                        resp_payload)) {
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "file upload finish response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ =
        server_err.empty() ? "file upload finish failed" : server_err;
    return false;
  }
  std::size_t off = 1;
  std::uint64_t size = 0;
  if (!mi::server::proto::ReadUint64(resp_payload, off, size) ||
      off != resp_payload.size() || size != total_size) {
    last_error_ = "file upload finish response invalid";
    return false;
  }
  return true;
}

bool ClientCore::StartE2eeFileBlobDownload(const std::string& file_id,
                                          bool wipe_after_read,
                                          std::string& out_download_id,
                                          std::uint64_t& out_size) {
  out_download_id.clear();
  out_size = 0;
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty()) {
    last_error_ = "file id empty";
    return false;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(file_id, plain);
  plain.push_back(wipe_after_read ? 1 : 0);

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kE2eeFileDownloadStart, plain,
                        resp_payload)) {
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "file download start response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ =
        server_err.empty() ? "file download start failed" : server_err;
    return false;
  }
  std::size_t off = 1;
  std::string download_id;
  std::uint64_t size = 0;
  if (!mi::server::proto::ReadString(resp_payload, off, download_id) ||
      !mi::server::proto::ReadUint64(resp_payload, off, size) ||
      off != resp_payload.size() || download_id.empty()) {
    last_error_ = "file download start response invalid";
    return false;
  }

  out_download_id = std::move(download_id);
  out_size = size;
  return true;
}

bool ClientCore::DownloadE2eeFileBlobChunk(const std::string& file_id,
                                          const std::string& download_id,
                                          std::uint64_t offset,
                                          std::uint32_t max_len,
                                          std::vector<std::uint8_t>& out_chunk,
                                          bool& out_eof) {
  out_chunk.clear();
  out_eof = false;
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty() || download_id.empty()) {
    last_error_ = "invalid session";
    return false;
  }
  if (max_len == 0 || max_len > kE2eeBlobChunkBytes) {
    max_len = kE2eeBlobChunkBytes;
  }

  std::vector<std::uint8_t> plain;
  mi::server::proto::WriteString(file_id, plain);
  mi::server::proto::WriteString(download_id, plain);
  mi::server::proto::WriteUint64(offset, plain);
  mi::server::proto::WriteUint32(max_len, plain);

  std::vector<std::uint8_t> resp_payload;
  if (!ProcessEncrypted(mi::server::FrameType::kE2eeFileDownloadChunk, plain,
                        resp_payload)) {
    return false;
  }
  if (resp_payload.empty()) {
    last_error_ = "file download chunk response empty";
    return false;
  }
  if (resp_payload[0] == 0) {
    std::string server_err;
    std::size_t off = 1;
    mi::server::proto::ReadString(resp_payload, off, server_err);
    last_error_ =
        server_err.empty() ? "file download chunk failed" : server_err;
    return false;
  }

  std::size_t off = 1;
  std::uint64_t resp_off = 0;
  if (!mi::server::proto::ReadUint64(resp_payload, off, resp_off) ||
      off >= resp_payload.size()) {
    last_error_ = "file download chunk response invalid";
    return false;
  }
  const bool eof = resp_payload[off++] != 0;
  std::vector<std::uint8_t> chunk;
  if (!mi::server::proto::ReadBytes(resp_payload, off, chunk) ||
      off != resp_payload.size()) {
    last_error_ = "file download chunk response invalid";
    return false;
  }
  if (resp_off != offset) {
    last_error_ = "file download chunk response invalid";
    return false;
  }
  if (chunk.size() > max_len) {
    last_error_ = "file download chunk response invalid";
    return false;
  }

  out_chunk = std::move(chunk);
  out_eof = eof;
  return true;
}

bool ClientCore::UploadE2eeFileBlobV3FromPath(
    const std::filesystem::path& file_path, std::uint64_t plaintext_size,
    const std::array<std::uint8_t, 32>& file_key, std::string& out_file_id) {
  out_file_id.clear();
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (file_path.empty()) {
    last_error_ = "file path empty";
    return false;
  }
  if (plaintext_size == 0 ||
      plaintext_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
    last_error_ = "file too large";
    return false;
  }

  const std::uint64_t chunks =
      (plaintext_size + kFileBlobV4PlainChunkBytes - 1) / kFileBlobV4PlainChunkBytes;
  if (chunks == 0 || chunks > (1ull << 31) ||
      chunks > static_cast<std::uint64_t>((std::numeric_limits<std::uint32_t>::max)())) {
    last_error_ = "file size invalid";
    return false;
  }
  std::vector<std::uint32_t> chunk_sizes;
  chunk_sizes.reserve(static_cast<std::size_t>(chunks));
  std::uint64_t remaining = plaintext_size;
  std::uint64_t payload_bytes = 0;
  for (std::uint64_t idx = 0; idx < chunks; ++idx) {
    const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(
        remaining, kFileBlobV4PlainChunkBytes));
    const std::size_t min_len = want + 4;
    const std::size_t target_len = SelectFileChunkTarget(min_len);
    if (target_len == 0) {
      last_error_ = "file chunk size invalid";
      return false;
    }
    chunk_sizes.push_back(static_cast<std::uint32_t>(target_len));
    payload_bytes += 16u + static_cast<std::uint64_t>(target_len);
    remaining -= want;
  }
  const std::size_t header_size =
      kFileBlobV4BaseHeaderSize + chunk_sizes.size() * 4;
  const std::uint64_t blob_size =
      static_cast<std::uint64_t>(header_size) + payload_bytes;
  if (blob_size == 0 ||
      blob_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    last_error_ = "payload too large";
    return false;
  }

  std::vector<std::uint8_t> header;
  header.reserve(header_size);
  header.insert(header.end(), kFileBlobMagic,
                kFileBlobMagic + sizeof(kFileBlobMagic));
  header.push_back(kFileBlobVersionV4);
  header.push_back(0);  // flags
  header.push_back(kFileBlobAlgoRaw);
  header.push_back(0);  // reserved
  mi::server::proto::WriteUint32(static_cast<std::uint32_t>(chunks), header);
  mi::server::proto::WriteUint64(plaintext_size, header);
  std::array<std::uint8_t, 24> base_nonce{};
  if (!RandomBytes(base_nonce.data(), base_nonce.size())) {
    last_error_ = "rng failed";
    return false;
  }
  header.insert(header.end(), base_nonce.begin(), base_nonce.end());
  for (const auto chunk_len : chunk_sizes) {
    mi::server::proto::WriteUint32(chunk_len, header);
  }
  if (header.size() != header_size) {
    last_error_ = "blob header invalid";
    return false;
  }

  std::string file_id;
  std::string upload_id;
  if (!StartE2eeFileBlobUpload(blob_size, file_id, upload_id)) {
    return false;
  }

  std::uint64_t off = 0;
  {
    std::uint64_t received = 0;
    if (!UploadE2eeFileBlobChunk(file_id, upload_id, off, header, received)) {
      return false;
    }
    if (received != header.size()) {
      last_error_ = "file upload chunk response invalid";
      return false;
    }
    off = received;
  }

  std::ifstream ifs(file_path, std::ios::binary);
  if (!ifs) {
    last_error_ = "open file failed";
    return false;
  }

  std::vector<std::uint8_t> plain;
  plain.resize(kFileBlobV4PlainChunkBytes);
  remaining = plaintext_size;
  for (std::uint64_t idx = 0; idx < chunks; ++idx) {
    const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(
        remaining, kFileBlobV4PlainChunkBytes));
    ifs.read(reinterpret_cast<char*>(plain.data()),
             static_cast<std::streamsize>(want));
    if (!ifs) {
      last_error_ = "read file failed";
      crypto_wipe(plain.data(), plain.size());
      return false;
    }

    const std::uint32_t target_len =
        chunk_sizes[static_cast<std::size_t>(idx)];
    if (target_len < 4u + want) {
      last_error_ = "file chunk size invalid";
      crypto_wipe(plain.data(), plain.size());
      return false;
    }
    std::vector<std::uint8_t> padded;
    padded.resize(target_len);
    padded[0] = static_cast<std::uint8_t>(want & 0xFF);
    padded[1] = static_cast<std::uint8_t>((want >> 8) & 0xFF);
    padded[2] = static_cast<std::uint8_t>((want >> 16) & 0xFF);
    padded[3] = static_cast<std::uint8_t>((want >> 24) & 0xFF);
    if (want > 0) {
      std::memcpy(padded.data() + 4, plain.data(), want);
    }
    const std::size_t pad_len = padded.size() - 4 - want;
    if (pad_len > 0 &&
        !RandomBytes(padded.data() + 4 + want, pad_len)) {
      last_error_ = "rng failed";
      crypto_wipe(plain.data(), plain.size());
      crypto_wipe(padded.data(), padded.size());
      return false;
    }

    std::vector<std::uint8_t> record;
    record.resize(16u + padded.size());
    std::array<std::uint8_t, 24> nonce = base_nonce;
    for (int i = 0; i < 8; ++i) {
      nonce[16 + i] = static_cast<std::uint8_t>((idx >> (8 * i)) & 0xFF);
    }
    crypto_aead_lock(record.data() + 16, record.data(), file_key.data(),
                     nonce.data(), header.data(), header.size(), padded.data(),
                     padded.size());
    crypto_wipe(plain.data(), want);
    crypto_wipe(padded.data(), padded.size());

    std::uint64_t received = 0;
    if (!UploadE2eeFileBlobChunk(file_id, upload_id, off, record, received)) {
      return false;
    }
    if (received != off + record.size()) {
      last_error_ = "file upload chunk response invalid";
      return false;
    }
    off = received;

    remaining -= want;
  }
  crypto_wipe(plain.data(), plain.size());

  if (!FinishE2eeFileBlobUpload(file_id, upload_id, blob_size)) {
    return false;
  }

  out_file_id = std::move(file_id);
  return true;
}

bool ClientCore::DownloadE2eeFileBlobV3ToPath(
    const std::string& file_id, const std::array<std::uint8_t, 32>& file_key,
    const std::filesystem::path& out_path, bool wipe_after_read,
    const std::function<void(std::uint64_t, std::uint64_t)>& on_progress) {
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (file_id.empty()) {
    last_error_ = "file id empty";
    return false;
  }
  if (out_path.empty()) {
    last_error_ = "output path empty";
    return false;
  }

  std::string download_id;
  std::uint64_t blob_size = 0;
  if (!StartE2eeFileBlobDownload(file_id, wipe_after_read, download_id,
                                 blob_size)) {
    return false;
  }
  if (blob_size < kFileBlobV3PrefixSize + 16 + 1 ||
      blob_size > static_cast<std::uint64_t>(kMaxChatFileBlobBytes)) {
    last_error_ = "file download response invalid";
    return false;
  }

  std::vector<std::uint8_t> header;
  bool eof = false;
  if (!DownloadE2eeFileBlobChunk(
          file_id, download_id, 0,
          static_cast<std::uint32_t>(kFileBlobV3PrefixSize), header, eof)) {
    return false;
  }
  if (header.size() != kFileBlobV3PrefixSize) {
    last_error_ = "file blob header invalid";
    return false;
  }
  if (std::memcmp(header.data(), kFileBlobMagic, sizeof(kFileBlobMagic)) != 0) {
    last_error_ = "file blob header invalid";
    return false;
  }
  const std::uint8_t version = header[sizeof(kFileBlobMagic)];
  if (version != kFileBlobVersionV3 && version != kFileBlobVersionV4) {
    last_error_ = "file blob version mismatch";
    return false;
  }

  std::size_t h = sizeof(kFileBlobMagic) + 1;
  const std::uint8_t flags = header[h++];
  const std::uint8_t algo = header[h++];
  h++;  // reserved

  if (version == kFileBlobVersionV3) {
    std::uint32_t chunk_size = 0;
    std::uint64_t original_size = 0;
    if (!mi::server::proto::ReadUint32(header, h, chunk_size) ||
        !mi::server::proto::ReadUint64(header, h, original_size) ||
        h + 24 != header.size()) {
      last_error_ = "file blob header invalid";
      return false;
    }
    (void)flags;
    if (algo != kFileBlobAlgoRaw || chunk_size == 0 || original_size == 0 ||
        chunk_size > (kE2eeBlobChunkBytes - 16u) ||
        original_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
      last_error_ = "file blob header invalid";
      return false;
    }

    std::array<std::uint8_t, 24> base_nonce{};
    std::memcpy(base_nonce.data(), header.data() + h, base_nonce.size());

    const std::uint64_t chunks = (original_size + chunk_size - 1) / chunk_size;
    const std::uint64_t expect =
        static_cast<std::uint64_t>(kFileBlobV3PrefixSize) + chunks * 16u +
        original_size;
    if (expect != blob_size) {
      last_error_ = "file blob size mismatch";
      return false;
    }

    std::error_code ec;
    const auto parent = out_path.has_parent_path()
                            ? out_path.parent_path()
                            : std::filesystem::path{};
    if (!parent.empty()) {
      std::filesystem::create_directories(parent, ec);
    }
    const auto temp_path = out_path.string() + ".part";
    std::ofstream ofs(temp_path, std::ios::binary | std::ios::trunc);
    if (!ofs) {
      last_error_ = "open output file failed";
      return false;
    }

    std::uint64_t blob_off = kFileBlobV3PrefixSize;
    std::uint64_t written = 0;
    if (on_progress) {
      on_progress(0, original_size);
    }
    for (std::uint64_t idx = 0; idx < chunks; ++idx) {
      const std::size_t want = static_cast<std::size_t>(std::min<std::uint64_t>(
          chunk_size, original_size - written));
      const std::uint32_t record_len =
          static_cast<std::uint32_t>(16 + want);
      std::vector<std::uint8_t> record;
      bool record_eof = false;
      if (!DownloadE2eeFileBlobChunk(file_id, download_id, blob_off, record_len,
                                     record, record_eof)) {
        crypto_wipe(record.data(), record.size());
        return false;
      }
      if (record.size() != record_len) {
        crypto_wipe(record.data(), record.size());
        last_error_ = "file download chunk invalid";
        return false;
      }

      std::array<std::uint8_t, 24> nonce = base_nonce;
      for (int i = 0; i < 8; ++i) {
        nonce[16 + i] = static_cast<std::uint8_t>((idx >> (8 * i)) & 0xFF);
      }

      std::vector<std::uint8_t> plain;
      plain.resize(want);
      const std::uint8_t* mac = record.data();
      const std::uint8_t* cipher = record.data() + 16;
      const int ok = crypto_aead_unlock(plain.data(), mac, file_key.data(),
                                        nonce.data(), header.data(), header.size(),
                                        cipher, want);
      crypto_wipe(record.data(), record.size());
      if (ok != 0) {
        crypto_wipe(plain.data(), plain.size());
        last_error_ = "file decrypt failed";
        return false;
      }

      ofs.write(reinterpret_cast<const char*>(plain.data()),
                static_cast<std::streamsize>(plain.size()));
      crypto_wipe(plain.data(), plain.size());
      if (!ofs) {
        last_error_ = "write output file failed";
        return false;
      }

      blob_off += record_len;
      written += want;
      eof = record_eof;
      if (on_progress) {
        on_progress(written, original_size);
      }
    }
    ofs.close();
    if (written != original_size || blob_off != blob_size || !eof) {
      last_error_ = "file download incomplete";
      std::filesystem::remove(temp_path, ec);
      return false;
    }

    std::filesystem::remove(out_path, ec);
    std::filesystem::rename(temp_path, out_path, ec);
    if (ec) {
      std::filesystem::remove(temp_path, ec);
      last_error_ = "finalize output failed";
      return false;
    }
    return true;
  }

  std::uint32_t chunk_count = 0;
  std::uint64_t original_size = 0;
  if (!mi::server::proto::ReadUint32(header, h, chunk_count) ||
      !mi::server::proto::ReadUint64(header, h, original_size) ||
      h + 24 != header.size()) {
    last_error_ = "file blob header invalid";
    return false;
  }
  (void)flags;
  if (algo != kFileBlobAlgoRaw || chunk_count == 0 || original_size == 0 ||
      original_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
    last_error_ = "file blob header invalid";
    return false;
  }

  std::array<std::uint8_t, 24> base_nonce{};
  std::memcpy(base_nonce.data(), header.data() + h, base_nonce.size());

  const std::size_t header_size =
      kFileBlobV4BaseHeaderSize + static_cast<std::size_t>(chunk_count) * 4u;
  if (header_size < kFileBlobV4BaseHeaderSize ||
      header_size > static_cast<std::size_t>(blob_size)) {
    last_error_ = "file blob header invalid";
    return false;
  }
  if (header_size > header.size()) {
    const std::size_t need = header_size - header.size();
    if (need > kE2eeBlobChunkBytes) {
      last_error_ = "file blob header invalid";
      return false;
    }
    std::vector<std::uint8_t> tail;
    bool tail_eof = false;
    if (!DownloadE2eeFileBlobChunk(
            file_id, download_id, header.size(),
            static_cast<std::uint32_t>(need), tail, tail_eof)) {
      return false;
    }
    header.insert(header.end(), tail.begin(), tail.end());
  }
  if (header.size() != header_size) {
    last_error_ = "file blob header invalid";
    return false;
  }

  std::vector<std::uint32_t> chunk_sizes;
  chunk_sizes.reserve(chunk_count);
  std::uint64_t payload_expect = 0;
  std::size_t table_off = kFileBlobV4BaseHeaderSize;
  for (std::uint32_t i = 0; i < chunk_count; ++i) {
    std::uint32_t chunk_len = 0;
    if (!mi::server::proto::ReadUint32(header, table_off, chunk_len)) {
      last_error_ = "file blob header invalid";
      return false;
    }
    if (chunk_len < 4u || chunk_len > (kE2eeBlobChunkBytes - 16u)) {
      last_error_ = "file blob header invalid";
      return false;
    }
    chunk_sizes.push_back(chunk_len);
    payload_expect += 16u + static_cast<std::uint64_t>(chunk_len);
  }
  if (table_off != header.size()) {
    last_error_ = "file blob header invalid";
    return false;
  }
  const std::uint64_t expect =
      static_cast<std::uint64_t>(header_size) + payload_expect;
  if (expect != blob_size) {
    last_error_ = "file blob size mismatch";
    return false;
  }

  std::error_code ec;
  const auto parent = out_path.has_parent_path()
                          ? out_path.parent_path()
                          : std::filesystem::path{};
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
  }
  const auto temp_path = out_path.string() + ".part";
  std::ofstream ofs(temp_path, std::ios::binary | std::ios::trunc);
  if (!ofs) {
    last_error_ = "open output file failed";
    return false;
  }

  std::uint64_t blob_off = header_size;
  std::uint64_t written = 0;
  if (on_progress) {
    on_progress(0, original_size);
  }
  for (std::uint64_t idx = 0; idx < chunk_sizes.size(); ++idx) {
    const std::uint32_t chunk_len = chunk_sizes[static_cast<std::size_t>(idx)];
    const std::uint32_t record_len = static_cast<std::uint32_t>(16u + chunk_len);
    std::vector<std::uint8_t> record;
    bool record_eof = false;
    if (!DownloadE2eeFileBlobChunk(file_id, download_id, blob_off, record_len,
                                   record, record_eof)) {
      crypto_wipe(record.data(), record.size());
      return false;
    }
    if (record.size() != record_len) {
      crypto_wipe(record.data(), record.size());
      last_error_ = "file download chunk invalid";
      return false;
    }

    std::array<std::uint8_t, 24> nonce = base_nonce;
    for (int i = 0; i < 8; ++i) {
      nonce[16 + i] = static_cast<std::uint8_t>((idx >> (8 * i)) & 0xFF);
    }

    std::vector<std::uint8_t> plain;
    plain.resize(chunk_len);
    const std::uint8_t* mac = record.data();
    const std::uint8_t* cipher = record.data() + 16;
    const int ok = crypto_aead_unlock(plain.data(), mac, file_key.data(),
                                      nonce.data(), header.data(), header.size(),
                                      cipher, chunk_len);
    crypto_wipe(record.data(), record.size());
    if (ok != 0) {
      crypto_wipe(plain.data(), plain.size());
      last_error_ = "file decrypt failed";
      return false;
    }
    if (plain.size() < 4) {
      crypto_wipe(plain.data(), plain.size());
      last_error_ = "file blob chunk invalid";
      return false;
    }
    const std::uint32_t actual_len =
        static_cast<std::uint32_t>(plain[0]) |
        (static_cast<std::uint32_t>(plain[1]) << 8) |
        (static_cast<std::uint32_t>(plain[2]) << 16) |
        (static_cast<std::uint32_t>(plain[3]) << 24);
    if (actual_len > (plain.size() - 4) ||
        written + actual_len > original_size) {
      crypto_wipe(plain.data(), plain.size());
      last_error_ = "file blob chunk invalid";
      return false;
    }

    ofs.write(reinterpret_cast<const char*>(plain.data() + 4),
              static_cast<std::streamsize>(actual_len));
    crypto_wipe(plain.data(), plain.size());
    if (!ofs) {
      last_error_ = "write output file failed";
      return false;
    }

    blob_off += record_len;
    written += actual_len;
    eof = record_eof;
    if (on_progress) {
      on_progress(written, original_size);
    }
  }
  ofs.close();
  if (written != original_size || blob_off != blob_size || !eof) {
    last_error_ = "file download incomplete";
    std::filesystem::remove(temp_path, ec);
    return false;
  }

  std::filesystem::remove(out_path, ec);
  std::filesystem::rename(temp_path, out_path, ec);
  if (ec) {
    std::filesystem::remove(temp_path, ec);
    last_error_ = "finalize output failed";
    return false;
  }

  return true;
}

bool ClientCore::UploadChatFileFromPath(const std::filesystem::path& file_path,
                                       std::uint64_t file_size,
                                       const std::string& file_name,
                                       std::array<std::uint8_t, 32>& out_file_key,
                                       std::string& out_file_id) {
  out_file_id.clear();
  out_file_key.fill(0);
  if (!EnsureChannel()) {
    last_error_ = "not logged in";
    return false;
  }
  if (file_path.empty()) {
    last_error_ = "file not found";
    return false;
  }
  if (file_size == 0 ||
      file_size > static_cast<std::uint64_t>(kMaxChatFileBytes)) {
    last_error_ = "file too large";
    return false;
  }

  if (!RandomBytes(out_file_key.data(), out_file_key.size())) {
    last_error_ = "rng failed";
    return false;
  }

  if (file_size > (8u * 1024u * 1024u)) {
    const bool ok = UploadE2eeFileBlobV3FromPath(file_path, file_size,
                                                 out_file_key, out_file_id);
    if (ok) {
      BestEffortStoreAttachmentPreviewFromPath(out_file_id, file_name,
                                               file_size, file_path);
    }
    return ok;
  }

  if (file_size > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
    last_error_ = "file too large";
    return false;
  }

  std::ifstream ifs(file_path, std::ios::binary);
  if (!ifs) {
    last_error_ = "open file failed";
    return false;
  }

  std::vector<std::uint8_t> plaintext;
  plaintext.resize(static_cast<std::size_t>(file_size));
  ifs.read(reinterpret_cast<char*>(plaintext.data()),
           static_cast<std::streamsize>(plaintext.size()));
  if (!ifs) {
    crypto_wipe(plaintext.data(), plaintext.size());
    last_error_ = "read file failed";
    return false;
  }

  std::vector<std::uint8_t> preview;
  const std::size_t max_preview = 256u * 1024u;
  const std::size_t take = std::min(plaintext.size(), max_preview);
  if (take > 0) {
    preview.assign(plaintext.begin(), plaintext.begin() + take);
  }

  std::vector<std::uint8_t> blob;
  const bool encrypted_ok =
      EncryptFileBlobAdaptive(plaintext, out_file_key, file_name, blob);
  crypto_wipe(plaintext.data(), plaintext.size());
  std::vector<std::uint8_t>().swap(plaintext);
  if (!encrypted_ok) {
    last_error_ = "file encrypt failed";
    return false;
  }

  if (!UploadE2eeFileBlob(blob, out_file_id)) {
    return false;
  }
  if (!preview.empty()) {
    BestEffortStoreAttachmentPreviewBytes(out_file_id, file_name, file_size,
                                          preview);
    crypto_wipe(preview.data(), preview.size());
  }
  return true;
}

bool ClientCore::TrustPendingPeer(const std::string& pin) {
  last_error_.clear();
  if (!EnsureE2ee()) {
    return false;
  }
  std::string err;
  if (!e2ee_.TrustPendingPeer(pin, err)) {
    last_error_ = err.empty() ? "trust peer failed" : err;
    return false;
  }
  last_error_.clear();
  return true;
}

bool ClientCore::TrustPendingServer(const std::string& pin) {
  last_error_.clear();
  if (!remote_mode_ || !use_tls_) {
    last_error_ = "tls not enabled";
    return false;
  }
  if (pending_server_fingerprint_.empty() || pending_server_pin_.empty()) {
    last_error_ = "no pending server trust";
    return false;
  }
  if (NormalizeCode(pin) != NormalizeCode(pending_server_pin_)) {
    last_error_ = "sas mismatch";
    return false;
  }
  if (trust_store_path_.empty()) {
    std::filesystem::path trust = "server_trust.ini";
    if (!config_path_.empty()) {
      const auto cfg_dir = ResolveConfigDir(config_path_);
      const auto data_dir = ResolveDataDir(cfg_dir);
      trust = data_dir / trust;
    }
    trust_store_path_ = trust.string();
  }
  std::string err;
  TrustEntry entry;
  entry.fingerprint = pending_server_fingerprint_;
  entry.tls_required = require_tls_;
  const bool ok = StoreTrustEntry(
      trust_store_path_, EndpointKey(server_ip_, server_port_), entry, err);
  if (!ok) {
    last_error_ = err.empty() ? "store trust failed" : err;
    return false;
  }
  pinned_server_fingerprint_ = pending_server_fingerprint_;
  pending_server_fingerprint_.clear();
  pending_server_pin_.clear();
  ResetRemoteStream();
  last_error_.clear();
  return true;
}

}  // namespace mi::client
