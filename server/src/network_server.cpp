#include "network_server.h"

#include <algorithm>
#include <chrono>
#include <cerrno>
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
#ifndef SECURITY_WIN32
#define SECURITY_WIN32 1
#endif
#include <security.h>
#include <schannel.h>
#include <wincrypt.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#endif

#include "crypto.h"
#include "frame.h"

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
#ifdef MI_E2EE_ENABLE_TCP_SERVER
namespace {

struct ScopedCertStore {
  HCERTSTORE store{nullptr};
  ~ScopedCertStore() {
    if (store) {
      CertCloseStore(store, 0);
      store = nullptr;
    }
  }
  ScopedCertStore() = default;
  ScopedCertStore(const ScopedCertStore&) = delete;
  ScopedCertStore& operator=(const ScopedCertStore&) = delete;
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

struct ScopedCryptProv {
  HCRYPTPROV prov{0};
  ~ScopedCryptProv() {
    if (prov) {
      CryptReleaseContext(prov, 0);
      prov = 0;
    }
  }
  ScopedCryptProv() = default;
  ScopedCryptProv(const ScopedCryptProv&) = delete;
  ScopedCryptProv& operator=(const ScopedCryptProv&) = delete;
};

struct ScopedCryptKey {
  HCRYPTKEY key{0};
  ~ScopedCryptKey() {
    if (key) {
      CryptDestroyKey(key);
      key = 0;
    }
  }
  ScopedCryptKey() = default;
  ScopedCryptKey(const ScopedCryptKey&) = delete;
  ScopedCryptKey& operator=(const ScopedCryptKey&) = delete;
};

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
  ScopedCtxtHandle(ScopedCtxtHandle&& other) noexcept {
    ctx = other.ctx;
    has = other.has;
    other.has = false;
  }
  ScopedCtxtHandle& operator=(ScopedCtxtHandle&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    if (has) {
      DeleteSecurityContext(&ctx);
    }
    ctx = other.ctx;
    has = other.has;
    other.has = false;
    return *this;
  }
};

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

bool GenerateSelfSignedPfx(const std::filesystem::path& out_path,
                           std::string& error) {
  error.clear();
  if (out_path.empty()) {
    error = "tls_cert empty";
    return false;
  }

  std::error_code ec;
  const auto dir = out_path.has_parent_path() ? out_path.parent_path()
                                              : std::filesystem::path{};
  if (!dir.empty()) {
    std::filesystem::create_directories(dir, ec);
  }

  static constexpr wchar_t kContainerName[] = L"mi_e2ee_tls_key";
  ScopedCryptProv prov;
  if (!CryptAcquireContextW(&prov.prov, kContainerName, nullptr, PROV_RSA_AES,
                            CRYPT_NEWKEYSET)) {
    const DWORD last = GetLastError();
    if (last == NTE_EXISTS) {
      if (!CryptAcquireContextW(&prov.prov, kContainerName, nullptr,
                                PROV_RSA_AES, 0)) {
        const DWORD ec2 = GetLastError();
        error = "CryptAcquireContext failed: " + std::to_string(ec2) + " " +
                Win32ErrorMessage(ec2);
        return false;
      }
    } else {
      error = "CryptAcquireContext failed: " + std::to_string(last) + " " +
              Win32ErrorMessage(last);
      return false;
    }
  }

  ScopedCryptKey key;
  const DWORD key_flags =
      (2048u << 16u) | CRYPT_EXPORTABLE;  // 2048-bit RSA
  if (!CryptGenKey(prov.prov, AT_KEYEXCHANGE, key_flags, &key.key)) {
    const DWORD last = GetLastError();
    error = "CryptGenKey failed: " + std::to_string(last) + " " +
            Win32ErrorMessage(last);
    return false;
  }

  DWORD name_len = 0;
  if (!CertStrToNameW(X509_ASN_ENCODING, L"CN=MI_E2EE_Server",
                      CERT_X500_NAME_STR, nullptr, nullptr, &name_len,
                      nullptr) ||
      name_len == 0) {
    const DWORD last = GetLastError();
    error = "CertStrToName sizing failed: " + std::to_string(last) + " " +
            Win32ErrorMessage(last);
    return false;
  }
  std::vector<std::uint8_t> name_buf(name_len);
  if (!CertStrToNameW(X509_ASN_ENCODING, L"CN=MI_E2EE_Server",
                      CERT_X500_NAME_STR, nullptr, name_buf.data(), &name_len,
                      nullptr)) {
    const DWORD last = GetLastError();
    error = "CertStrToName failed: " + std::to_string(last) + " " +
            Win32ErrorMessage(last);
    return false;
  }

  CERT_NAME_BLOB subject{};
  subject.cbData = name_len;
  subject.pbData = name_buf.data();

  CRYPT_KEY_PROV_INFO key_prov{};
  key_prov.pwszContainerName = const_cast<wchar_t*>(kContainerName);
  key_prov.pwszProvName = nullptr;
  key_prov.dwProvType = PROV_RSA_AES;
  key_prov.dwFlags = 0;
  key_prov.cProvParam = 0;
  key_prov.rgProvParam = nullptr;
  key_prov.dwKeySpec = AT_KEYEXCHANGE;

  SYSTEMTIME start{};
  SYSTEMTIME end{};
  GetSystemTime(&start);
  end = start;
  end.wYear = static_cast<WORD>(end.wYear + 10);  // 10 years

  ScopedCertContext cert;
  cert.cert = CertCreateSelfSignCertificate(
      prov.prov, &subject, 0, &key_prov, nullptr, &start, &end, nullptr);
  if (!cert.cert) {
    const DWORD last = GetLastError();
    error = "CertCreateSelfSignCertificate failed: " + std::to_string(last) +
            " " + Win32ErrorMessage(last);
    return false;
  }

  ScopedCertStore mem_store;
  mem_store.store = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0,
                                  CERT_STORE_CREATE_NEW_FLAG, nullptr);
  if (!mem_store.store) {
    const DWORD last = GetLastError();
    error = "CertOpenStore failed: " + std::to_string(last) + " " +
            Win32ErrorMessage(last);
    return false;
  }
  if (!CertAddCertificateContextToStore(mem_store.store, cert.cert,
                                        CERT_STORE_ADD_REPLACE_EXISTING,
                                        nullptr)) {
    const DWORD last = GetLastError();
    error = "CertAddCertificateContextToStore failed: " + std::to_string(last) +
            " " + Win32ErrorMessage(last);
    return false;
  }

  CRYPT_DATA_BLOB pfx_blob{};
  const wchar_t* pfx_pass = L"";
  const DWORD export_flags =
      EXPORT_PRIVATE_KEYS | REPORT_NOT_ABLE_TO_EXPORT_PRIVATE_KEY |
      REPORT_NO_PRIVATE_KEY;
  if (!PFXExportCertStoreEx(mem_store.store, &pfx_blob, pfx_pass, nullptr,
                             export_flags) ||
      pfx_blob.cbData == 0) {
    const DWORD last = GetLastError();
    error = "PFXExportCertStoreEx sizing failed: " + std::to_string(last) + " " +
            Win32ErrorMessage(last);
    return false;
  }

  std::vector<std::uint8_t> pfx_bytes(pfx_blob.cbData);
  pfx_blob.pbData = pfx_bytes.data();
  if (!PFXExportCertStoreEx(mem_store.store, &pfx_blob, pfx_pass, nullptr,
                             export_flags) ||
      pfx_blob.cbData == 0) {
    const DWORD last = GetLastError();
    error = "PFXExportCertStoreEx failed: " + std::to_string(last) + " " +
            Win32ErrorMessage(last);
    return false;
  }

  std::ofstream out(out_path, std::ios::binary | std::ios::trunc);
  if (!out) {
    error = "write tls_cert failed";
    return false;
  }
  out.write(reinterpret_cast<const char*>(pfx_bytes.data()),
            static_cast<std::streamsize>(pfx_bytes.size()));
  out.close();
  return true;
}

bool LoadPfxCert(const std::filesystem::path& pfx_path, ScopedCertStore& store,
                 ScopedCertContext& cert, std::string& error) {
  error.clear();
  std::ifstream f(pfx_path, std::ios::binary);
  if (!f) {
    error = "tls_cert not found";
    return false;
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());
  if (bytes.empty()) {
    error = "tls_cert empty";
    return false;
  }
  CRYPT_DATA_BLOB blob{};
  blob.pbData = bytes.data();
  blob.cbData = static_cast<DWORD>(bytes.size());
  store.store = PFXImportCertStore(&blob, L"",
                                   CRYPT_EXPORTABLE | CRYPT_USER_KEYSET |
                                       PKCS12_ALLOW_OVERWRITE_KEY);
  if (!store.store) {
    const DWORD last = GetLastError();
    error = "PFXImportCertStore failed: " + std::to_string(last) + " " +
            Win32ErrorMessage(last);
    return false;
  }

  PCCERT_CONTEXT found =
      CertFindCertificateInStore(store.store, X509_ASN_ENCODING, 0,
                                 CERT_FIND_ANY, nullptr, nullptr);
  if (!found) {
    error = "tls_cert has no certificate";
    return false;
  }
  cert.cert = CertDuplicateCertificateContext(found);
  return cert.cert != nullptr;
}

bool InitSchannelServerCred(const std::filesystem::path& pfx_path,
                            ScopedCredHandle& out_cred,
                            ScopedCertStore& out_store,
                            ScopedCertContext& out_cert,
                            std::string& error) {
  error.clear();
  std::error_code ec;
  if (!std::filesystem::exists(pfx_path, ec)) {
    std::string gen_err;
    if (!GenerateSelfSignedPfx(pfx_path, gen_err)) {
      error = gen_err.empty() ? "generate tls_cert failed" : gen_err;
      return false;
    }
  }

  std::string load_err;
  if (!LoadPfxCert(pfx_path, out_store, out_cert, load_err)) {
    error = load_err.empty() ? "load tls_cert failed" : load_err;
    return false;
  }

  SCHANNEL_CRED sch{};
  sch.dwVersion = SCHANNEL_CRED_VERSION;
  sch.cCreds = 1;
  sch.paCred = &out_cert.cert;
  sch.dwFlags = SCH_CRED_NO_DEFAULT_CREDS;

  TimeStamp expiry{};
  const SECURITY_STATUS st =
      AcquireCredentialsHandleW(nullptr, const_cast<wchar_t*>(UNISP_NAME_W),
                                SECPKG_CRED_INBOUND, nullptr, &sch, nullptr,
                                nullptr, &out_cred.cred, &expiry);
  if (st != SEC_E_OK) {
    std::ostringstream oss;
    oss << "AcquireCredentialsHandle failed: 0x" << std::hex
        << static_cast<unsigned long>(st);
    error = oss.str();
    return false;
  }
  out_cred.has = true;
  return true;
}

bool SchannelAccept(SOCKET sock, ScopedCredHandle& cred, ScopedCtxtHandle& ctx,
                    SecPkgContext_StreamSizes& sizes,
                    std::vector<std::uint8_t>& out_extra) {
  out_extra.clear();

  std::vector<std::uint8_t> in_buf;
  DWORD ctx_attr = 0;
  TimeStamp expiry{};
  bool have_ctx = false;

  constexpr DWORD req_flags = ASC_REQ_SEQUENCE_DETECT |
                              ASC_REQ_REPLAY_DETECT |
                              ASC_REQ_CONFIDENTIALITY |
                              ASC_REQ_EXTENDED_ERROR |
                              ASC_REQ_ALLOCATE_MEMORY |
                              ASC_REQ_STREAM;

  while (true) {
    if (in_buf.empty()) {
      if (!RecvSome(sock, in_buf)) {
        return false;
      }
    }

    SecBuffer in_buffers[2];
    in_buffers[0].pvBuffer = in_buf.data();
    in_buffers[0].cbBuffer = static_cast<unsigned long>(in_buf.size());
    in_buffers[0].BufferType = SECBUFFER_TOKEN;
    in_buffers[1].pvBuffer = nullptr;
    in_buffers[1].cbBuffer = 0;
    in_buffers[1].BufferType = SECBUFFER_EMPTY;

    SecBufferDesc in_desc{};
    in_desc.ulVersion = SECBUFFER_VERSION;
    in_desc.cBuffers = 2;
    in_desc.pBuffers = in_buffers;

    SecBuffer out_buffers[1];
    out_buffers[0].pvBuffer = nullptr;
    out_buffers[0].cbBuffer = 0;
    out_buffers[0].BufferType = SECBUFFER_TOKEN;

    SecBufferDesc out_desc{};
    out_desc.ulVersion = SECBUFFER_VERSION;
    out_desc.cBuffers = 1;
    out_desc.pBuffers = out_buffers;

    SECURITY_STATUS st = AcceptSecurityContext(
        &cred.cred, have_ctx ? &ctx.ctx : nullptr, &in_desc, req_flags,
        SECURITY_NATIVE_DREP, &ctx.ctx, &out_desc, &ctx_attr, &expiry);
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
        return false;
      }
    }

    if (st == SEC_E_INCOMPLETE_MESSAGE) {
      if (!RecvSome(sock, in_buf)) {
        return false;
      }
      continue;
    }
    if (st == SEC_I_CONTINUE_NEEDED) {
      if (in_buffers[1].BufferType == SECBUFFER_EXTRA &&
          in_buffers[1].cbBuffer > 0) {
        const std::size_t extra = in_buffers[1].cbBuffer;
        std::vector<std::uint8_t> keep(in_buf.end() - extra, in_buf.end());
        in_buf.swap(keep);
      } else {
        in_buf.clear();
      }
      continue;
    }
    if (st != SEC_E_OK) {
      return false;
    }

    if (in_buffers[1].BufferType == SECBUFFER_EXTRA &&
        in_buffers[1].cbBuffer > 0) {
      const std::size_t extra = in_buffers[1].cbBuffer;
      out_extra.assign(in_buf.end() - extra, in_buf.end());
    }
    break;
  }

  const SECURITY_STATUS qs =
      QueryContextAttributes(&ctx.ctx, SECPKG_ATTR_STREAM_SIZES, &sizes);
  return qs == SEC_E_OK;
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

    if (!SchannelDecryptToPlain(sock, ctx, enc_buf, plain_chunk)) {
      return false;
    }
    if (!plain_chunk.empty()) {
      plain_buf.insert(plain_buf.end(), plain_chunk.begin(), plain_chunk.end());
    }
  }
}

}  // namespace

struct NetworkServer::TlsServer {
  ScopedCredHandle cred{};
  ScopedCertStore store{};
  ScopedCertContext cert{};
};
#else
struct NetworkServer::TlsServer {};
#endif  // MI_E2EE_ENABLE_TCP_SERVER

void NetworkServer::TlsServerDeleter::operator()(TlsServer* p) const {
  delete p;
}
#endif

#ifdef MI_E2EE_ENABLE_TCP_SERVER
namespace {
constexpr int kReactorPollTimeoutMs = 50;
constexpr std::size_t kReactorCompactThreshold = 1024u * 1024u;

bool SetNonBlocking(SocketHandle sock) {
#ifdef _WIN32
  u_long mode = 1;
  return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
  const int flags = fcntl(sock, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  return fcntl(sock, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

bool WouldBlock() {
#ifdef _WIN32
  const int err = WSAGetLastError();
  return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
#else
  return errno == EWOULDBLOCK || errno == EAGAIN;
#endif
}

void CloseSocketHandle(SocketHandle sock) {
#ifdef _WIN32
  closesocket(sock);
#else
  ::close(sock);
#endif
}

#ifdef _WIN32
using PollFd = WSAPOLLFD;
constexpr short kPollIn = POLLRDNORM;
constexpr short kPollOut = POLLWRNORM;
inline int PollSockets(PollFd* fds, ULONG count, INT timeout_ms) {
  return WSAPoll(fds, count, timeout_ms);
}
#else
using PollFd = pollfd;
constexpr short kPollIn = POLLIN;
constexpr short kPollOut = POLLOUT;
inline int PollSockets(PollFd* fds, nfds_t count, int timeout_ms) {
  return ::poll(fds, count, timeout_ms);
}
#endif
}  // namespace
#endif  // MI_E2EE_ENABLE_TCP_SERVER

struct NetworkServer::Connection {
  SocketHandle sock{kInvalidSocket};
  std::string remote_ip;
  std::uint64_t bytes_total{0};
  std::vector<std::uint8_t> recv_buf;
  std::size_t recv_off{0};
  std::vector<std::uint8_t> send_buf;
  std::size_t send_off{0};
  std::vector<std::uint8_t> response_buf;
#ifdef _WIN32
  std::mutex iocp_mutex;
  std::vector<std::uint8_t> iocp_recv_tmp;
  bool iocp_recv_pending{false};
  bool iocp_send_pending{false};
  std::deque<std::vector<std::uint8_t>> iocp_send_queue;
  std::size_t iocp_send_off{0};
  struct TlsState {
    ScopedCtxtHandle ctx;
    SecPkgContext_StreamSizes sizes{};
    bool handshake_done{false};
    std::vector<std::uint8_t> enc_in;
    std::vector<std::uint8_t> enc_tmp;
  };
  std::unique_ptr<TlsState> tls;
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
#ifdef _WIN32
      const int n = ::send(conn->sock,
                           reinterpret_cast<const char*>(
                               conn->send_buf.data() + conn->send_off),
                           want, 0);
#else
      const ssize_t n = ::send(
          conn->sock, conn->send_buf.data() + conn->send_off,
          static_cast<std::size_t>(want), 0);
#endif
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
        if (!EncryptTlsPayload(conn, response)) {
          return false;
        }
      } else
#endif
      {
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

#ifdef _WIN32
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

    SecBuffer in_buffers[2];
    in_buffers[0].pvBuffer = tls.enc_in.data();
    in_buffers[0].cbBuffer = static_cast<unsigned long>(tls.enc_in.size());
    in_buffers[0].BufferType = SECBUFFER_TOKEN;
    in_buffers[1].pvBuffer = nullptr;
    in_buffers[1].cbBuffer = 0;
    in_buffers[1].BufferType = SECBUFFER_EMPTY;

    SecBufferDesc in_desc{};
    in_desc.ulVersion = SECBUFFER_VERSION;
    in_desc.cBuffers = 2;
    in_desc.pBuffers = in_buffers;

    SecBuffer out_buffers[1];
    out_buffers[0].pvBuffer = nullptr;
    out_buffers[0].cbBuffer = 0;
    out_buffers[0].BufferType = SECBUFFER_TOKEN;

    SecBufferDesc out_desc{};
    out_desc.ulVersion = SECBUFFER_VERSION;
    out_desc.cBuffers = 1;
    out_desc.pBuffers = out_buffers;

    DWORD ctx_attr = 0;
    TimeStamp expiry{};
    constexpr DWORD req_flags = ASC_REQ_SEQUENCE_DETECT |
                                ASC_REQ_REPLAY_DETECT |
                                ASC_REQ_CONFIDENTIALITY |
                                ASC_REQ_EXTENDED_ERROR |
                                ASC_REQ_ALLOCATE_MEMORY |
                                ASC_REQ_STREAM;

    SECURITY_STATUS st = AcceptSecurityContext(
        &server_->tls_->cred.cred,
        tls.ctx.has ? &tls.ctx.ctx : nullptr,
        &in_desc, req_flags, SECURITY_NATIVE_DREP, &tls.ctx.ctx,
        &out_desc, &ctx_attr, &expiry);
    tls.ctx.has = true;

    if (st == SEC_I_COMPLETE_NEEDED || st == SEC_I_COMPLETE_AND_CONTINUE) {
      CompleteAuthToken(&tls.ctx.ctx, &out_desc);
      st = (st == SEC_I_COMPLETE_NEEDED) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
    }

    if (out_buffers[0].pvBuffer && out_buffers[0].cbBuffer > 0) {
      const auto* p =
          reinterpret_cast<const std::uint8_t*>(out_buffers[0].pvBuffer);
      const std::size_t n = out_buffers[0].cbBuffer;
      conn->send_buf.insert(conn->send_buf.end(), p, p + n);
      FreeContextBuffer(out_buffers[0].pvBuffer);
      out_buffers[0].pvBuffer = nullptr;
    }

    if (st == SEC_E_INCOMPLETE_MESSAGE) {
      return true;
    }
    if (st == SEC_I_CONTINUE_NEEDED) {
      if (in_buffers[1].BufferType == SECBUFFER_EXTRA &&
          in_buffers[1].cbBuffer > 0) {
        const std::size_t extra = in_buffers[1].cbBuffer;
        std::vector<std::uint8_t> keep(tls.enc_in.end() - extra,
                                       tls.enc_in.end());
        tls.enc_in.swap(keep);
      } else {
        tls.enc_in.clear();
      }
      return true;
    }
    if (st != SEC_E_OK) {
      return false;
    }

    if (in_buffers[1].BufferType == SECBUFFER_EXTRA &&
        in_buffers[1].cbBuffer > 0) {
      const std::size_t extra = in_buffers[1].cbBuffer;
      std::vector<std::uint8_t> keep(tls.enc_in.end() - extra,
                                     tls.enc_in.end());
      tls.enc_in.swap(keep);
    } else {
      tls.enc_in.clear();
    }

    const SECURITY_STATUS qs =
        QueryContextAttributes(&tls.ctx.ctx, SECPKG_ATTR_STREAM_SIZES,
                               &tls.sizes);
    if (qs != SEC_E_OK) {
      return false;
    }
    tls.handshake_done = true;
    return true;
  }

  bool DecryptTlsData(const std::shared_ptr<Connection>& conn) {
    if (!conn || !conn->tls || !conn->tls->handshake_done) {
      return true;
    }
    auto& tls = *conn->tls;
    auto& enc = tls.enc_in;
    if (enc.empty()) {
      return true;
    }
    while (!enc.empty()) {
      SecBuffer buffers[4];
      buffers[0].BufferType = SECBUFFER_DATA;
      buffers[0].pvBuffer = enc.data();
      buffers[0].cbBuffer = static_cast<unsigned long>(enc.size());
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

      const SECURITY_STATUS st = DecryptMessage(&tls.ctx.ctx, &desc, 0, nullptr);
      if (st == SEC_E_INCOMPLETE_MESSAGE) {
        break;
      }
      if (st == SEC_I_CONTEXT_EXPIRED || st == SEC_I_RENEGOTIATE) {
        return false;
      }
      if (st != SEC_E_OK) {
        return false;
      }

      for (const auto& b : buffers) {
        if (b.BufferType == SECBUFFER_DATA && b.pvBuffer && b.cbBuffer > 0) {
          const auto* p = reinterpret_cast<const std::uint8_t*>(b.pvBuffer);
          conn->recv_buf.insert(conn->recv_buf.end(), p, p + b.cbBuffer);
        }
      }

      bool has_extra = false;
      std::size_t extra_len = 0;
      for (const auto& b : buffers) {
        if (b.BufferType == SECBUFFER_EXTRA && b.cbBuffer > 0) {
          has_extra = true;
          extra_len = b.cbBuffer;
          break;
        }
      }
      if (has_extra && extra_len > 0 && extra_len <= enc.size()) {
        std::vector<std::uint8_t> keep(enc.end() - extra_len, enc.end());
        enc.swap(keep);
      } else {
        enc.clear();
        break;
      }
    }
    return true;
  }

  bool EncryptTlsPayload(const std::shared_ptr<Connection>& conn,
                         const std::vector<std::uint8_t>& plain) {
    if (!conn || !conn->tls || !conn->tls->handshake_done) {
      return false;
    }
    const auto& sizes = conn->tls->sizes;
    if (sizes.cbMaximumMessage == 0) {
      return false;
    }
    std::size_t offset = 0;
    auto& tmp = conn->tls->enc_tmp;
    while (offset < plain.size()) {
      const std::size_t chunk = std::min<std::size_t>(
          plain.size() - offset, sizes.cbMaximumMessage);
      tmp.resize(sizes.cbHeader + chunk + sizes.cbTrailer);
      std::memcpy(tmp.data() + sizes.cbHeader, plain.data() + offset, chunk);

      SecBuffer buffers[4];
      buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
      buffers[0].pvBuffer = tmp.data();
      buffers[0].cbBuffer = sizes.cbHeader;
      buffers[1].BufferType = SECBUFFER_DATA;
      buffers[1].pvBuffer = tmp.data() + sizes.cbHeader;
      buffers[1].cbBuffer = static_cast<unsigned long>(chunk);
      buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
      buffers[2].pvBuffer = tmp.data() + sizes.cbHeader + chunk;
      buffers[2].cbBuffer = sizes.cbTrailer;
      buffers[3].BufferType = SECBUFFER_EMPTY;
      buffers[3].pvBuffer = nullptr;
      buffers[3].cbBuffer = 0;

      SecBufferDesc desc{};
      desc.ulVersion = SECBUFFER_VERSION;
      desc.cBuffers = 4;
      desc.pBuffers = buffers;

      const SECURITY_STATUS st = EncryptMessage(&conn->tls->ctx.ctx, 0, &desc, 0);
      if (st != SEC_E_OK) {
        return false;
      }
      const std::size_t total =
          static_cast<std::size_t>(buffers[0].cbBuffer) +
          static_cast<std::size_t>(buffers[1].cbBuffer) +
          static_cast<std::size_t>(buffers[2].cbBuffer);
      if (total > 0) {
        conn->send_buf.insert(conn->send_buf.end(), tmp.data(),
                              tmp.data() + total);
      }
      offset += chunk;
    }
    return true;
  }
#endif

  void HandleRead(const std::shared_ptr<Connection>& conn) {
    if (!conn || conn->closed) {
      return;
    }
#ifdef _WIN32
    if (conn->tls) {
      std::uint8_t tmp[4096];
      for (;;) {
        const int n = ::recv(conn->sock, reinterpret_cast<char*>(tmp),
                             static_cast<int>(sizeof(tmp)), 0);
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
    } else
#endif
    {
      std::uint8_t tmp[4096];
      for (;;) {
#ifdef _WIN32
        const int n = ::recv(conn->sock, reinterpret_cast<char*>(tmp),
                             static_cast<int>(sizeof(tmp)), 0);
#else
        const ssize_t n =
            ::recv(conn->sock, tmp, sizeof(tmp), 0);
#endif
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
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kReactorPollTimeoutMs));
        continue;
      }
      std::vector<PollFd> fds;
      fds.reserve(connections_.size());
      for (const auto& conn : connections_) {
        if (!conn || conn->closed || conn->sock == kInvalidSocket) {
          continue;
        }
        PollFd p{};
        p.fd = conn->sock;
        p.events = kPollIn;
        if (!conn->send_buf.empty()) {
          p.events |= kPollOut;
        }
        p.revents = 0;
        fds.push_back(p);
      }
      if (fds.empty()) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(kReactorPollTimeoutMs));
        continue;
      }
#ifdef _WIN32
      const int rc =
          PollSockets(fds.data(), static_cast<ULONG>(fds.size()),
                      kReactorPollTimeoutMs);
#else
      const int rc =
          PollSockets(fds.data(), static_cast<nfds_t>(fds.size()),
                      kReactorPollTimeoutMs);
#endif
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
        if (revents & (POLLERR | POLLHUP | POLLNVAL)) {
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

    SecBuffer in_buffers[2];
    in_buffers[0].pvBuffer = tls.enc_in.data();
    in_buffers[0].cbBuffer = static_cast<unsigned long>(tls.enc_in.size());
    in_buffers[0].BufferType = SECBUFFER_TOKEN;
    in_buffers[1].pvBuffer = nullptr;
    in_buffers[1].cbBuffer = 0;
    in_buffers[1].BufferType = SECBUFFER_EMPTY;

    SecBufferDesc in_desc{};
    in_desc.ulVersion = SECBUFFER_VERSION;
    in_desc.cBuffers = 2;
    in_desc.pBuffers = in_buffers;

    SecBuffer out_buffers[1];
    out_buffers[0].pvBuffer = nullptr;
    out_buffers[0].cbBuffer = 0;
    out_buffers[0].BufferType = SECBUFFER_TOKEN;

    SecBufferDesc out_desc{};
    out_desc.ulVersion = SECBUFFER_VERSION;
    out_desc.cBuffers = 1;
    out_desc.pBuffers = out_buffers;

    DWORD ctx_attr = 0;
    TimeStamp expiry{};
    constexpr DWORD req_flags = ASC_REQ_SEQUENCE_DETECT |
                                ASC_REQ_REPLAY_DETECT |
                                ASC_REQ_CONFIDENTIALITY |
                                ASC_REQ_EXTENDED_ERROR |
                                ASC_REQ_ALLOCATE_MEMORY |
                                ASC_REQ_STREAM;

    SECURITY_STATUS st = AcceptSecurityContext(
        &server_->tls_->cred.cred,
        tls.ctx.has ? &tls.ctx.ctx : nullptr,
        &in_desc, req_flags, SECURITY_NATIVE_DREP, &tls.ctx.ctx,
        &out_desc, &ctx_attr, &expiry);
    tls.ctx.has = true;

    if (st == SEC_I_COMPLETE_NEEDED || st == SEC_I_COMPLETE_AND_CONTINUE) {
      CompleteAuthToken(&tls.ctx.ctx, &out_desc);
      st = (st == SEC_I_COMPLETE_NEEDED) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
    }

    if (out_buffers[0].pvBuffer && out_buffers[0].cbBuffer > 0) {
      const auto* p =
          reinterpret_cast<const std::uint8_t*>(out_buffers[0].pvBuffer);
      const std::size_t n = out_buffers[0].cbBuffer;
      conn->send_buf.insert(conn->send_buf.end(), p, p + n);
      FreeContextBuffer(out_buffers[0].pvBuffer);
      out_buffers[0].pvBuffer = nullptr;
    }

    if (st == SEC_E_INCOMPLETE_MESSAGE) {
      return true;
    }
    if (st == SEC_I_CONTINUE_NEEDED) {
      if (in_buffers[1].BufferType == SECBUFFER_EXTRA &&
          in_buffers[1].cbBuffer > 0) {
        const std::size_t extra = in_buffers[1].cbBuffer;
        std::vector<std::uint8_t> keep(tls.enc_in.end() - extra,
                                       tls.enc_in.end());
        tls.enc_in.swap(keep);
      } else {
        tls.enc_in.clear();
      }
      return true;
    }
    if (st != SEC_E_OK) {
      return false;
    }

    if (in_buffers[1].BufferType == SECBUFFER_EXTRA &&
        in_buffers[1].cbBuffer > 0) {
      const std::size_t extra = in_buffers[1].cbBuffer;
      std::vector<std::uint8_t> keep(tls.enc_in.end() - extra,
                                     tls.enc_in.end());
      tls.enc_in.swap(keep);
    } else {
      tls.enc_in.clear();
    }

    const SECURITY_STATUS qs =
        QueryContextAttributes(&tls.ctx.ctx, SECPKG_ATTR_STREAM_SIZES,
                               &tls.sizes);
    if (qs != SEC_E_OK) {
      return false;
    }
    tls.handshake_done = true;
    return true;
  }

  bool DecryptTlsDataLocked(const std::shared_ptr<Connection>& conn) {
    if (!conn || !conn->tls || !conn->tls->handshake_done) {
      return true;
    }
    auto& tls = *conn->tls;
    auto& enc = tls.enc_in;
    if (enc.empty()) {
      return true;
    }
    while (!enc.empty()) {
      SecBuffer buffers[4];
      buffers[0].BufferType = SECBUFFER_DATA;
      buffers[0].pvBuffer = enc.data();
      buffers[0].cbBuffer = static_cast<unsigned long>(enc.size());
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

      const SECURITY_STATUS st = DecryptMessage(&tls.ctx.ctx, &desc, 0, nullptr);
      if (st == SEC_E_INCOMPLETE_MESSAGE) {
        break;
      }
      if (st == SEC_I_CONTEXT_EXPIRED || st == SEC_I_RENEGOTIATE) {
        return false;
      }
      if (st != SEC_E_OK) {
        return false;
      }

      for (const auto& b : buffers) {
        if (b.BufferType == SECBUFFER_DATA && b.pvBuffer && b.cbBuffer > 0) {
          const auto* p = reinterpret_cast<const std::uint8_t*>(b.pvBuffer);
          conn->recv_buf.insert(conn->recv_buf.end(), p, p + b.cbBuffer);
        }
      }

      bool has_extra = false;
      std::size_t extra_len = 0;
      for (const auto& b : buffers) {
        if (b.BufferType == SECBUFFER_EXTRA && b.cbBuffer > 0) {
          has_extra = true;
          extra_len = b.cbBuffer;
          break;
        }
      }
      if (has_extra && extra_len > 0 && extra_len <= enc.size()) {
        std::vector<std::uint8_t> keep(enc.end() - extra_len, enc.end());
        enc.swap(keep);
      } else {
        enc.clear();
        break;
      }
    }
    return true;
  }

  bool EncryptTlsPayloadLocked(const std::shared_ptr<Connection>& conn,
                               const std::vector<std::uint8_t>& plain) {
    if (!conn || !conn->tls || !conn->tls->handshake_done) {
      return false;
    }
    const auto& sizes = conn->tls->sizes;
    if (sizes.cbMaximumMessage == 0) {
      return false;
    }
    std::size_t offset = 0;
    auto& tmp = conn->tls->enc_tmp;
    while (offset < plain.size()) {
      const std::size_t chunk = std::min<std::size_t>(
          plain.size() - offset, sizes.cbMaximumMessage);
      tmp.resize(sizes.cbHeader + chunk + sizes.cbTrailer);
      std::memcpy(tmp.data() + sizes.cbHeader, plain.data() + offset, chunk);

      SecBuffer buffers[4];
      buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
      buffers[0].pvBuffer = tmp.data();
      buffers[0].cbBuffer = sizes.cbHeader;
      buffers[1].BufferType = SECBUFFER_DATA;
      buffers[1].pvBuffer = tmp.data() + sizes.cbHeader;
      buffers[1].cbBuffer = static_cast<unsigned long>(chunk);
      buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
      buffers[2].pvBuffer = tmp.data() + sizes.cbHeader + chunk;
      buffers[2].cbBuffer = sizes.cbTrailer;
      buffers[3].BufferType = SECBUFFER_EMPTY;
      buffers[3].pvBuffer = nullptr;
      buffers[3].cbBuffer = 0;

      SecBufferDesc desc{};
      desc.ulVersion = SECBUFFER_VERSION;
      desc.cBuffers = 4;
      desc.pBuffers = buffers;

      const SECURITY_STATUS st = EncryptMessage(&conn->tls->ctx.ctx, 0, &desc, 0);
      if (st != SEC_E_OK) {
        return false;
      }
      const std::size_t total =
          static_cast<std::size_t>(buffers[0].cbBuffer) +
          static_cast<std::size_t>(buffers[1].cbBuffer) +
          static_cast<std::size_t>(buffers[2].cbBuffer);
      if (total > 0) {
        conn->send_buf.insert(conn->send_buf.end(), tmp.data(),
                              tmp.data() + total);
      }
      offset += chunk;
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
#if defined(MI_E2EE_ENABLE_TCP_SERVER) && !defined(_WIN32)
  if (tls_enable_) {
    error = "tls not supported on this platform";
    return false;
  }
#endif
#if defined(_WIN32) && defined(MI_E2EE_ENABLE_TCP_SERVER)
  if (tls_enable_) {
    auto tls = std::unique_ptr<TlsServer, TlsServerDeleter>(new TlsServer());
    std::string tls_err;
    if (!InitSchannelServerCred(tls_cert_, tls->cred, tls->store, tls->cert,
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
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
#else
  const bool use_reactor = !reactors_.empty();
  const bool use_iocp = use_iocp_ && iocp_;
  while (running_.load()) {
    sockaddr_in cli{};
    socklen_t len = sizeof(cli);
#ifdef _WIN32
    SOCKET client = accept(static_cast<SOCKET>(listen_fd_),
                           reinterpret_cast<sockaddr*>(&cli), &len);
    if (client == INVALID_SOCKET) {
      continue;
    }
    char ip_buf[64] = {};
    const char* ip_ptr =
        InetNtopA(AF_INET, const_cast<in_addr*>(&cli.sin_addr), ip_buf,
                  static_cast<DWORD>(sizeof(ip_buf)));
    const std::string remote_ip = ip_ptr ? std::string(ip_ptr) : std::string();
    if (!TryAcquireConnectionSlot(remote_ip)) {
      closesocket(client);
      continue;
    }
    if (use_iocp) {
      auto conn = std::make_shared<Connection>();
      conn->sock = client;
      conn->remote_ip = remote_ip;
      conn->recv_buf.reserve(8192);
      iocp_->AddConnection(std::move(conn));
      continue;
    }
    if (use_reactor) {
      if (!SetNonBlocking(client)) {
        ReleaseConnectionSlot(remote_ip);
        closesocket(client);
        continue;
      }
      auto conn = std::make_shared<Connection>();
      conn->sock = client;
      conn->remote_ip = remote_ip;
      conn->recv_buf.reserve(8192);
#ifdef _WIN32
      if (tls_enable_) {
        conn->tls = std::make_unique<Connection::TlsState>();
        conn->tls->enc_in.reserve(8192);
      }
#endif
      AssignConnection(std::move(conn));
      continue;
    }
    {
      const DWORD timeout_ms = 30000;
      setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&timeout_ms),
                 static_cast<int>(sizeof(timeout_ms)));
      setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
                 reinterpret_cast<const char*>(&timeout_ms),
                 static_cast<int>(sizeof(timeout_ms)));
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
              const int n =
                  recv(client, reinterpret_cast<char*>(data + got), want, 0);
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
              const int n = send(client,
                                 reinterpret_cast<const char*>(data + sent),
                                 chunk, 0);
              if (n <= 0) {
                return false;
              }
              sent += static_cast<std::size_t>(n);
            }
            return true;
          };

          if (tls_enable_ && tls_) {
            ScopedCtxtHandle ctx;
            SecPkgContext_StreamSizes sizes{};
            std::vector<std::uint8_t> enc_buf;
            if (!SchannelAccept(client, tls_->cred, ctx, sizes, enc_buf)) {
              closesocket(client);
              return;
            }
            std::vector<std::uint8_t> plain_buf;
            std::size_t plain_off = 0;
            std::vector<std::uint8_t> request;
            std::vector<std::uint8_t> response;
            while (running_.load()) {
              if (!SchannelReadFrameBuffered(client, ctx, enc_buf, plain_buf,
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
                  !SchannelEncryptSend(client, ctx, sizes, response)) {
                break;
              }
            }
            closesocket(client);
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
          closesocket(client);
        } catch (...) {
          closesocket(client);
        }
      };
      if (!EnqueueTask(std::move(task))) {
        ReleaseConnectionSlot(remote_ip);
        closesocket(client);
      }
    } catch (...) {
      ReleaseConnectionSlot(remote_ip);
      closesocket(client);
    }
#else
    int client = ::accept(static_cast<int>(listen_fd_),
                          reinterpret_cast<sockaddr*>(&cli), &len);
    if (client < 0) {
      continue;
    }
    char ip_buf[64] = {};
    const char* ip_ptr =
        inet_ntop(AF_INET, &cli.sin_addr, ip_buf, sizeof(ip_buf));
    const std::string remote_ip = ip_ptr ? std::string(ip_ptr) : std::string();
    if (!TryAcquireConnectionSlot(remote_ip)) {
      ::close(client);
      continue;
    }
    if (use_reactor) {
      if (!SetNonBlocking(client)) {
        ReleaseConnectionSlot(remote_ip);
        ::close(client);
        continue;
      }
      auto conn = std::make_shared<Connection>();
      conn->sock = client;
      conn->remote_ip = remote_ip;
      conn->recv_buf.reserve(8192);
      AssignConnection(std::move(conn));
      continue;
    }
    {
      timeval tv{};
      tv.tv_sec = 30;
      tv.tv_usec = 0;
      ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv,
                  static_cast<socklen_t>(sizeof(tv)));
      ::setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &tv,
                  static_cast<socklen_t>(sizeof(tv)));
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
              const ssize_t n =
                  ::recv(client, data + got, static_cast<std::size_t>(want), 0);
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
              const ssize_t n = ::send(
                  client, data + sent, static_cast<std::size_t>(chunk), 0);
              if (n <= 0) {
                return false;
              }
              sent += static_cast<std::size_t>(n);
            }
            return true;
          };

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
          ::close(client);
        } catch (...) {
          ::close(client);
        }
      };
      if (!EnqueueTask(std::move(task))) {
        ReleaseConnectionSlot(remote_ip);
        ::close(client);
      }
    } catch (...) {
      ReleaseConnectionSlot(remote_ip);
      ::close(client);
    }
#endif
  }
#endif
}

#ifdef MI_E2EE_ENABLE_TCP_SERVER
bool NetworkServer::StartSocket(std::string& error) {
  error.clear();
#ifdef _WIN32
  WSADATA wsa;
  const int wsa_rc = WSAStartup(MAKEWORD(2, 2), &wsa);
  if (wsa_rc != 0) {
    error = "WSAStartup failed: " + std::to_string(wsa_rc) + " " + Win32ErrorMessage(static_cast<DWORD>(wsa_rc));
    return false;
  }
#endif
#ifdef _WIN32
  const SOCKET sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) {
    const DWORD last = WSAGetLastError();
    error = "socket(AF_INET,SOCK_STREAM) failed: " + std::to_string(last) + " " + Win32ErrorMessage(last);
    WSACleanup();
    return false;
  }
  listen_fd_ = static_cast<std::intptr_t>(sock);
#else
  const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    const int last = errno;
    error = "socket(AF_INET,SOCK_STREAM) failed: " + std::to_string(last) + " " + std::strerror(last);
    return false;
  }
  listen_fd_ = static_cast<std::intptr_t>(sock);
#endif
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  int yes = 1;
#ifdef _WIN32
  setsockopt(static_cast<SOCKET>(listen_fd_), SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
  ::setsockopt(static_cast<int>(listen_fd_), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
#ifdef _WIN32
  if (::bind(static_cast<SOCKET>(listen_fd_), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
    const DWORD last = WSAGetLastError();
    error = "bind(0.0.0.0:" + std::to_string(port_) + ") failed: " + std::to_string(last) + " " + Win32ErrorMessage(last);
    StopSocket();
    return false;
  }
  if (::listen(static_cast<SOCKET>(listen_fd_), 8) == SOCKET_ERROR) {
    const DWORD last = WSAGetLastError();
    error = "listen(0.0.0.0:" + std::to_string(port_) + ") failed: " + std::to_string(last) + " " + Win32ErrorMessage(last);
    StopSocket();
    return false;
  }
#else
  if (::bind(static_cast<int>(listen_fd_), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    const int last = errno;
    error = "bind(0.0.0.0:" + std::to_string(port_) + ") failed: " + std::to_string(last) + " " + std::strerror(last);
    StopSocket();
    return false;
  }
  if (::listen(static_cast<int>(listen_fd_), 8) < 0) {
    const int last = errno;
    error = "listen(0.0.0.0:" + std::to_string(port_) + ") failed: " + std::to_string(last) + " " + std::strerror(last);
    StopSocket();
    return false;
  }
#endif
  return true;
}

void NetworkServer::StopSocket() {
  if (listen_fd_ != -1) {
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(listen_fd_));
    WSACleanup();
#else
    ::close(static_cast<int>(listen_fd_));
#endif
    listen_fd_ = -1;
  }
}
#endif

}  // namespace mi::server
