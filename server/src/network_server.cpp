#include "network_server.h"

#include <chrono>
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
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif
#endif

#include "crypto.h"
#include "frame.h"

namespace mi::server {


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

    std::vector<std::uint8_t> plain_chunk;
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

NetworkServer::NetworkServer(Listener* listener, std::uint16_t port,
                             bool tls_enable, std::string tls_cert,
                             NetworkServerLimits limits)
    : listener_(listener),
      port_(port),
      tls_enable_(tls_enable),
      tls_cert_(std::move(tls_cert)),
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
#ifdef MI_E2EE_ENABLE_TCP_SERVER
  if (!StartSocket()) {
    error = "start socket failed";
    return false;
  }
#endif
  running_.store(true);
  worker_ = std::thread(&NetworkServer::Run, this);
  return true;
}

void NetworkServer::Stop() {
  running_.store(false);
  if (worker_.joinable()) {
    worker_.join();
  }
#ifdef MI_E2EE_ENABLE_TCP_SERVER
  StopSocket();
#endif
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
  while (running_.load()) {
    sockaddr_in cli{};
    socklen_t len = sizeof(cli);
#ifdef _WIN32
    SOCKET client = accept(listen_fd_, reinterpret_cast<sockaddr*>(&cli), &len);
    if (client == INVALID_SOCKET) {
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
    char ip_buf[64] = {};
    const char* ip_ptr =
        InetNtopA(AF_INET, const_cast<in_addr*>(&cli.sin_addr), ip_buf,
                  static_cast<DWORD>(sizeof(ip_buf)));
    const std::string remote_ip = ip_ptr ? std::string(ip_ptr) : std::string();
    if (!TryAcquireConnectionSlot(remote_ip)) {
      closesocket(client);
      continue;
    }

    try {
      std::thread([this, client, remote_ip]() mutable {
        struct SlotGuard {
          NetworkServer* server;
          std::string ip;
          ~SlotGuard() { server->ReleaseConnectionSlot(ip); }
        } slot{this, std::move(remote_ip)};

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
          while (running_.load()) {
            std::vector<std::uint8_t> request;
            if (!SchannelReadFrameBuffered(client, ctx, enc_buf, plain_buf,
                                           plain_off, request)) {
              break;
            }
            bytes_total += request.size();
            if (bytes_total > limits_.max_connection_bytes) {
              break;
            }
            std::vector<std::uint8_t> response;
            if (!listener_->Process(request, response, slot.ip)) {
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
          std::vector<std::uint8_t> request;
          request.resize(total);
          std::memcpy(request.data(), header, sizeof(header));
          if (payload_len > 0 &&
              !recv_exact(request.data() + kFrameHeaderSize, payload_len)) {
            break;
          }

          std::vector<std::uint8_t> response;
          if (!listener_->Process(request, response, slot.ip)) {
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
      }).detach();
    } catch (...) {
      ReleaseConnectionSlot(remote_ip);
      closesocket(client);
    }
#else
    int client = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&cli), &len);
    if (client < 0) {
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
    char ip_buf[64] = {};
    const char* ip_ptr =
        inet_ntop(AF_INET, &cli.sin_addr, ip_buf, sizeof(ip_buf));
    const std::string remote_ip = ip_ptr ? std::string(ip_ptr) : std::string();
    if (!TryAcquireConnectionSlot(remote_ip)) {
      ::close(client);
      continue;
    }

    try {
      std::thread([this, client, remote_ip]() mutable {
        struct SlotGuard {
          NetworkServer* server;
          std::string ip;
          ~SlotGuard() { server->ReleaseConnectionSlot(ip); }
        } slot{this, std::move(remote_ip)};

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
          std::vector<std::uint8_t> request;
          request.resize(total);
          std::memcpy(request.data(), header, sizeof(header));
          if (payload_len > 0 &&
              !recv_exact(request.data() + kFrameHeaderSize, payload_len)) {
            break;
          }

          std::vector<std::uint8_t> response;
          if (!listener_->Process(request, response, slot.ip)) {
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
      }).detach();
    } catch (...) {
      ReleaseConnectionSlot(remote_ip);
      ::close(client);
    }
#endif
  }
#endif
}

#ifdef MI_E2EE_ENABLE_TCP_SERVER
bool NetworkServer::StartSocket() {
#ifdef _WIN32
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    return false;
  }
#endif
  listen_fd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
  if (listen_fd_ < 0) {
    return false;
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  int yes = 1;
#ifdef _WIN32
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    StopSocket();
    return false;
  }
  if (::listen(listen_fd_, 8) < 0) {
    StopSocket();
    return false;
  }
  return true;
}

void NetworkServer::StopSocket() {
  if (listen_fd_ >= 0) {
#ifdef _WIN32
    closesocket(listen_fd_);
    WSACleanup();
#else
    ::close(listen_fd_);
#endif
    listen_fd_ = -1;
  }
}
#endif

}  // namespace mi::server
