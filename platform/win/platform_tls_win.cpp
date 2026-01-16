#include "platform_tls.h"

#ifndef SECURITY_WIN32
#define SECURITY_WIN32 1
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <schannel.h>
#include <security.h>
#include <wincrypt.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>

namespace mi::platform::tls {

namespace {

struct ClientContextImpl {
  CredHandle cred{};
  CtxtHandle ctx{};
  bool cred_has{false};
  bool ctx_has{false};
  SecPkgContext_StreamSizes sizes{};

  ~ClientContextImpl() {
    if (ctx_has) {
      DeleteSecurityContext(&ctx);
      ctx_has = false;
    }
    if (cred_has) {
      FreeCredentialsHandle(&cred);
      cred_has = false;
    }
  }
};

struct ScopedCertContext {
  PCCERT_CONTEXT cert{nullptr};
  ~ScopedCertContext() {
    if (cert) {
      CertFreeCertificateContext(cert);
      cert = nullptr;
    }
  }
};

struct ScopedCertStore {
  HCERTSTORE store{nullptr};
  ~ScopedCertStore() {
    if (store) {
      CertCloseStore(store, 0);
      store = nullptr;
    }
  }
};

struct ScopedCryptProv {
  HCRYPTPROV prov{0};
  ~ScopedCryptProv() {
    if (prov) {
      CryptReleaseContext(prov, 0);
      prov = 0;
    }
  }
};

struct ScopedCryptKey {
  HCRYPTKEY key{0};
  ~ScopedCryptKey() {
    if (key) {
      CryptDestroyKey(key);
      key = 0;
    }
  }
};

struct ServerCredentialsImpl {
  HCERTSTORE store{nullptr};
  PCCERT_CONTEXT cert{nullptr};
  CredHandle cred{};
  bool cred_has{false};

  ~ServerCredentialsImpl() {
    if (cred_has) {
      FreeCredentialsHandle(&cred);
      cred_has = false;
    }
    if (cert) {
      CertFreeCertificateContext(cert);
      cert = nullptr;
    }
    if (store) {
      CertCloseStore(store, 0);
      store = nullptr;
    }
  }
};

struct ServerContextImpl {
  CtxtHandle ctx{};
  bool ctx_has{false};
  bool handshake_done{false};
  SecPkgContext_StreamSizes sizes{};
  std::vector<std::uint8_t> scratch;

  ~ServerContextImpl() {
    if (ctx_has) {
      DeleteSecurityContext(&ctx);
      ctx_has = false;
    }
  }
};

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
      (2048u << 16u) | CRYPT_EXPORTABLE;
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
  end.wYear = static_cast<WORD>(end.wYear + 10);

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

bool LoadPfxCert(const std::filesystem::path& pfx_path,
                 HCERTSTORE& store,
                 PCCERT_CONTEXT& cert,
                 std::string& error) {
  error.clear();
  std::ifstream f(pfx_path, std::ios::binary);
  if (!f) {
    error = "tls_cert not found";
    return false;
  }
  std::error_code ec;
  const auto size = std::filesystem::file_size(pfx_path, ec);
  if (ec) {
    error = "tls_cert read failed";
    return false;
  }
  if (size == 0) {
    error = "tls_cert empty";
    return false;
  }
  if (size > static_cast<std::uintmax_t>(
                 (std::numeric_limits<std::size_t>::max)())) {
    error = "tls_cert too large";
    return false;
  }
  std::vector<std::uint8_t> bytes;
  bytes.resize(static_cast<std::size_t>(size));
  f.read(reinterpret_cast<char*>(bytes.data()),
         static_cast<std::streamsize>(bytes.size()));
  if (!f || f.gcount() != static_cast<std::streamsize>(bytes.size())) {
    error = "tls_cert read failed";
    return false;
  }
  CRYPT_DATA_BLOB blob{};
  blob.pbData = bytes.data();
  blob.cbData = static_cast<DWORD>(bytes.size());
  store = PFXImportCertStore(&blob, L"",
                             CRYPT_EXPORTABLE | CRYPT_USER_KEYSET |
                                 PKCS12_ALLOW_OVERWRITE_KEY);
  if (!store) {
    const DWORD last = GetLastError();
    error = "PFXImportCertStore failed: " + std::to_string(last) + " " +
            Win32ErrorMessage(last);
    return false;
  }

  PCCERT_CONTEXT found =
      CertFindCertificateInStore(store, X509_ASN_ENCODING, 0,
                                 CERT_FIND_ANY, nullptr, nullptr);
  if (!found) {
    error = "tls_cert has no certificate";
    return false;
  }
  cert = CertDuplicateCertificateContext(found);
  return cert != nullptr;
}

ServerContextImpl* GetServerContext(ServerContext& ctx,
                                    std::string& error) {
  error.clear();
  if (ctx.impl) {
    return static_cast<ServerContextImpl*>(ctx.impl);
  }
  auto impl = std::make_unique<ServerContextImpl>();
  ctx.impl = impl.release();
  return static_cast<ServerContextImpl*>(ctx.impl);
}

}  // namespace

bool IsSupported() {
  return true;
}

bool ClientHandshake(net::Socket sock, const std::string& host,
                     ClientContext& ctx,
                     std::vector<std::uint8_t>& out_server_cert_der,
                     std::vector<std::uint8_t>& out_enc_buf,
                     std::string& error) {
  out_server_cert_der.clear();
  out_enc_buf.clear();
  error.clear();

  if (!net::EnsureInitialized()) {
    error = "winsock init failed";
    return false;
  }

  auto impl = std::make_unique<ClientContextImpl>();

  SCHANNEL_CRED sch{};
  sch.dwVersion = SCHANNEL_CRED_VERSION;
  sch.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS;

  TimeStamp expiry{};
  SECURITY_STATUS st = AcquireCredentialsHandleW(
      nullptr, const_cast<wchar_t*>(UNISP_NAME_W), SECPKG_CRED_OUTBOUND, nullptr,
      &sch, nullptr, nullptr, &impl->cred, &expiry);
  if (st != SEC_E_OK) {
    error = "AcquireCredentialsHandle failed";
    return false;
  }
  impl->cred_has = true;

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
        &impl->cred, have_ctx ? &impl->ctx : nullptr,
        target.empty() ? nullptr : const_cast<wchar_t*>(target.c_str()),
        req_flags, 0, SECURITY_NATIVE_DREP, in_desc_ptr, 0, &impl->ctx,
        &out_desc, &ctx_attr, &expiry);
    have_ctx = true;
    impl->ctx_has = true;

    if (st == SEC_I_COMPLETE_NEEDED || st == SEC_I_COMPLETE_AND_CONTINUE) {
      CompleteAuthToken(&impl->ctx, &out_desc);
      st = (st == SEC_I_COMPLETE_NEEDED) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
    }

    if (out_buffers[0].pvBuffer && out_buffers[0].cbBuffer > 0) {
      const auto* p =
          reinterpret_cast<const std::uint8_t*>(out_buffers[0].pvBuffer);
      const std::size_t n = out_buffers[0].cbBuffer;
      const bool ok = net::SendAll(sock, p, n);
      FreeContextBuffer(out_buffers[0].pvBuffer);
      out_buffers[0].pvBuffer = nullptr;
      if (!ok) {
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
      if (!net::RecvSome(sock, in_buf)) {
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
        if (!net::RecvSome(sock, in_buf)) {
          error = "tls handshake recv failed";
          return false;
        }
      }
      continue;
    }

    error = "tls handshake failed";
    return false;
  }

  st = QueryContextAttributes(&impl->ctx, SECPKG_ATTR_STREAM_SIZES,
                              &impl->sizes);
  if (st != SEC_E_OK) {
    error = "QueryContextAttributes failed";
    return false;
  }

  ScopedCertContext remote_cert;
  st = QueryContextAttributes(&impl->ctx, SECPKG_ATTR_REMOTE_CERT_CONTEXT,
                              &remote_cert.cert);
  if (st != SEC_E_OK || !remote_cert.cert) {
    error = "remote cert unavailable";
    return false;
  }

  out_server_cert_der.assign(remote_cert.cert->pbCertEncoded,
                             remote_cert.cert->pbCertEncoded +
                                 remote_cert.cert->cbCertEncoded);
  if (out_server_cert_der.empty()) {
    error = "cert fingerprint failed";
    return false;
  }

  out_enc_buf = std::move(extra);
  ctx.impl = impl.release();
  return true;
}

bool EncryptAndSend(net::Socket sock, ClientContext& ctx,
                    const std::vector<std::uint8_t>& plain) {
  if (!ctx.impl) {
    return false;
  }
  auto* impl = static_cast<ClientContextImpl*>(ctx.impl);
  std::size_t sent = 0;
  while (sent < plain.size()) {
    const std::size_t chunk =
        std::min<std::size_t>(plain.size() - sent, impl->sizes.cbMaximumMessage);
    std::vector<std::uint8_t> buf;
    buf.resize(impl->sizes.cbHeader + chunk + impl->sizes.cbTrailer);
    std::memcpy(buf.data() + impl->sizes.cbHeader, plain.data() + sent, chunk);

    SecBuffer buffers[4];
    buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
    buffers[0].pvBuffer = buf.data();
    buffers[0].cbBuffer = impl->sizes.cbHeader;
    buffers[1].BufferType = SECBUFFER_DATA;
    buffers[1].pvBuffer = buf.data() + impl->sizes.cbHeader;
    buffers[1].cbBuffer = static_cast<unsigned long>(chunk);
    buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
    buffers[2].pvBuffer = buf.data() + impl->sizes.cbHeader + chunk;
    buffers[2].cbBuffer = impl->sizes.cbTrailer;
    buffers[3].BufferType = SECBUFFER_EMPTY;
    buffers[3].pvBuffer = nullptr;
    buffers[3].cbBuffer = 0;

    SecBufferDesc desc{};
    desc.ulVersion = SECBUFFER_VERSION;
    desc.cBuffers = 4;
    desc.pBuffers = buffers;

    const SECURITY_STATUS st = EncryptMessage(&impl->ctx, 0, &desc, 0);
    if (st != SEC_E_OK) {
      return false;
    }

    const std::size_t total =
        static_cast<std::size_t>(buffers[0].cbBuffer) +
        static_cast<std::size_t>(buffers[1].cbBuffer) +
        static_cast<std::size_t>(buffers[2].cbBuffer);
    if (!net::SendAll(sock, buf.data(), total)) {
      return false;
    }
    sent += chunk;
  }
  return true;
}

bool DecryptToPlain(net::Socket sock, ClientContext& ctx,
                    std::vector<std::uint8_t>& enc_buf,
                    std::vector<std::uint8_t>& plain_out) {
  plain_out.clear();
  if (!ctx.impl) {
    return false;
  }
  auto* impl = static_cast<ClientContextImpl*>(ctx.impl);
  while (true) {
    if (enc_buf.empty()) {
      if (!net::RecvSome(sock, enc_buf)) {
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

    const SECURITY_STATUS st = DecryptMessage(&impl->ctx, &desc, 0, nullptr);
    if (st == SEC_E_INCOMPLETE_MESSAGE) {
      if (!net::RecvSome(sock, enc_buf)) {
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

void Close(ClientContext& ctx) {
  if (ctx.impl) {
    delete static_cast<ClientContextImpl*>(ctx.impl);
    ctx.impl = nullptr;
  }
}

bool ServerInitCredentials(const std::string& pfx_path,
                           ServerCredentials& out,
                           std::string& error) {
  error.clear();
  if (pfx_path.empty()) {
    error = "tls_cert empty";
    return false;
  }

  auto impl = std::make_unique<ServerCredentialsImpl>();
  const std::filesystem::path path(pfx_path);

  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    std::string gen_err;
    if (!GenerateSelfSignedPfx(path, gen_err)) {
      error = gen_err.empty() ? "generate tls_cert failed" : gen_err;
      return false;
    }
  }

  std::string load_err;
  if (!LoadPfxCert(path, impl->store, impl->cert, load_err)) {
    error = load_err.empty() ? "load tls_cert failed" : load_err;
    return false;
  }

  SCHANNEL_CRED sch{};
  sch.dwVersion = SCHANNEL_CRED_VERSION;
  sch.cCreds = 1;
  sch.paCred = &impl->cert;
  sch.dwFlags = SCH_CRED_NO_DEFAULT_CREDS;

  TimeStamp expiry{};
  const SECURITY_STATUS st =
      AcquireCredentialsHandleW(nullptr, const_cast<wchar_t*>(UNISP_NAME_W),
                                SECPKG_CRED_INBOUND, nullptr, &sch, nullptr,
                                nullptr, &impl->cred, &expiry);
  if (st != SEC_E_OK) {
    std::ostringstream oss;
    oss << "AcquireCredentialsHandle failed: 0x" << std::hex
        << static_cast<unsigned long>(st);
    error = oss.str();
    return false;
  }
  impl->cred_has = true;

  out.impl = impl.release();
  return true;
}

bool ServerHandshake(net::Socket sock, ServerCredentials& creds,
                     ServerContext& ctx,
                     std::vector<std::uint8_t>& out_extra,
                     std::string& error) {
  out_extra.clear();
  error.clear();
  if (!creds.impl) {
    error = "tls credentials missing";
    return false;
  }
  auto* cred = static_cast<ServerCredentialsImpl*>(creds.impl);
  if (!cred->cred_has) {
    error = "tls credentials missing";
    return false;
  }
  auto* impl = GetServerContext(ctx, error);
  if (!impl) {
    error = error.empty() ? "tls context alloc failed" : error;
    return false;
  }

  std::vector<std::uint8_t> in_buf;
  DWORD ctx_attr = 0;
  TimeStamp expiry{};
  bool have_ctx = impl->ctx_has;

  constexpr DWORD req_flags = ASC_REQ_SEQUENCE_DETECT |
                              ASC_REQ_REPLAY_DETECT |
                              ASC_REQ_CONFIDENTIALITY |
                              ASC_REQ_EXTENDED_ERROR |
                              ASC_REQ_ALLOCATE_MEMORY |
                              ASC_REQ_STREAM;

  while (true) {
    if (in_buf.empty()) {
      if (!net::RecvSome(sock, in_buf)) {
        error = "tls handshake recv failed";
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
        &cred->cred, have_ctx ? &impl->ctx : nullptr, &in_desc, req_flags,
        SECURITY_NATIVE_DREP, &impl->ctx, &out_desc, &ctx_attr, &expiry);
    have_ctx = true;
    impl->ctx_has = true;

    if (st == SEC_I_COMPLETE_NEEDED || st == SEC_I_COMPLETE_AND_CONTINUE) {
      CompleteAuthToken(&impl->ctx, &out_desc);
      st = (st == SEC_I_COMPLETE_NEEDED) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
    }

    if (out_buffers[0].pvBuffer && out_buffers[0].cbBuffer > 0) {
      const auto* p =
          reinterpret_cast<const std::uint8_t*>(out_buffers[0].pvBuffer);
      const std::size_t n = out_buffers[0].cbBuffer;
      const bool ok = net::SendAll(sock, p, n);
      FreeContextBuffer(out_buffers[0].pvBuffer);
      out_buffers[0].pvBuffer = nullptr;
      if (!ok) {
        error = "tls send handshake failed";
        return false;
      }
    }

    if (st == SEC_E_INCOMPLETE_MESSAGE) {
      if (!net::RecvSome(sock, in_buf)) {
        error = "tls handshake recv failed";
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
      error = "tls handshake failed";
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
      QueryContextAttributes(&impl->ctx, SECPKG_ATTR_STREAM_SIZES, &impl->sizes);
  if (qs != SEC_E_OK) {
    error = "QueryContextAttributes failed";
    return false;
  }
  impl->handshake_done = true;
  return true;
}

bool ServerHandshakeStep(ServerCredentials& creds, ServerContext& ctx,
                         std::vector<std::uint8_t>& in_buf,
                         std::vector<std::uint8_t>& out_tokens,
                         bool& out_done,
                         std::string& error) {
  out_tokens.clear();
  out_done = false;
  error.clear();
  if (!creds.impl) {
    error = "tls credentials missing";
    return false;
  }
  auto* cred = static_cast<ServerCredentialsImpl*>(creds.impl);
  if (!cred->cred_has) {
    error = "tls credentials missing";
    return false;
  }
  auto* impl = GetServerContext(ctx, error);
  if (!impl) {
    error = error.empty() ? "tls context alloc failed" : error;
    return false;
  }
  if (impl->handshake_done) {
    out_done = true;
    return true;
  }
  if (in_buf.empty()) {
    return true;
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

  DWORD ctx_attr = 0;
  TimeStamp expiry{};
  constexpr DWORD req_flags = ASC_REQ_SEQUENCE_DETECT |
                              ASC_REQ_REPLAY_DETECT |
                              ASC_REQ_CONFIDENTIALITY |
                              ASC_REQ_EXTENDED_ERROR |
                              ASC_REQ_ALLOCATE_MEMORY |
                              ASC_REQ_STREAM;

  SECURITY_STATUS st = AcceptSecurityContext(
      &cred->cred, impl->ctx_has ? &impl->ctx : nullptr, &in_desc, req_flags,
      SECURITY_NATIVE_DREP, &impl->ctx, &out_desc, &ctx_attr, &expiry);
  impl->ctx_has = true;

  if (st == SEC_I_COMPLETE_NEEDED || st == SEC_I_COMPLETE_AND_CONTINUE) {
    CompleteAuthToken(&impl->ctx, &out_desc);
    st = (st == SEC_I_COMPLETE_NEEDED) ? SEC_E_OK : SEC_I_CONTINUE_NEEDED;
  }

  if (out_buffers[0].pvBuffer && out_buffers[0].cbBuffer > 0) {
    const auto* p =
        reinterpret_cast<const std::uint8_t*>(out_buffers[0].pvBuffer);
    const std::size_t n = out_buffers[0].cbBuffer;
    out_tokens.assign(p, p + n);
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
      std::vector<std::uint8_t> keep(in_buf.end() - extra, in_buf.end());
      in_buf.swap(keep);
    } else {
      in_buf.clear();
    }
    return true;
  }
  if (st != SEC_E_OK) {
    error = "tls handshake failed";
    return false;
  }

  if (in_buffers[1].BufferType == SECBUFFER_EXTRA &&
      in_buffers[1].cbBuffer > 0) {
    const std::size_t extra = in_buffers[1].cbBuffer;
    std::vector<std::uint8_t> keep(in_buf.end() - extra, in_buf.end());
    in_buf.swap(keep);
  } else {
    in_buf.clear();
  }

  const SECURITY_STATUS qs =
      QueryContextAttributes(&impl->ctx, SECPKG_ATTR_STREAM_SIZES,
                             &impl->sizes);
  if (qs != SEC_E_OK) {
    error = "QueryContextAttributes failed";
    return false;
  }
  impl->handshake_done = true;
  out_done = true;
  return true;
}

bool ServerEncryptAndSend(net::Socket sock, ServerContext& ctx,
                          const std::vector<std::uint8_t>& plain) {
  std::vector<std::uint8_t> cipher;
  if (!ServerEncryptBuffer(ctx, plain, cipher)) {
    return false;
  }
  if (cipher.empty()) {
    return true;
  }
  return net::SendAll(sock, cipher.data(), cipher.size());
}

bool ServerDecryptToPlain(net::Socket sock, ServerContext& ctx,
                          std::vector<std::uint8_t>& enc_buf,
                          std::vector<std::uint8_t>& plain_out) {
  plain_out.clear();
  if (!ctx.impl) {
    return false;
  }
  while (true) {
    if (enc_buf.empty()) {
      if (!net::RecvSome(sock, enc_buf)) {
        return false;
      }
    }
    bool need_more = false;
    if (!ServerDecryptBuffer(ctx, enc_buf, plain_out, need_more)) {
      return false;
    }
    if (need_more) {
      if (!net::RecvSome(sock, enc_buf)) {
        return false;
      }
      continue;
    }
    return true;
  }
}

bool ServerEncryptBuffer(ServerContext& ctx,
                         const std::vector<std::uint8_t>& plain,
                         std::vector<std::uint8_t>& out_cipher) {
  out_cipher.clear();
  if (!ctx.impl) {
    return false;
  }
  auto* impl = static_cast<ServerContextImpl*>(ctx.impl);
  if (!impl->handshake_done || impl->sizes.cbMaximumMessage == 0) {
    return false;
  }

  std::size_t offset = 0;
  while (offset < plain.size()) {
    const std::size_t chunk =
        std::min<std::size_t>(plain.size() - offset, impl->sizes.cbMaximumMessage);
    impl->scratch.resize(impl->sizes.cbHeader + chunk + impl->sizes.cbTrailer);
    std::memcpy(impl->scratch.data() + impl->sizes.cbHeader,
                plain.data() + offset, chunk);

    SecBuffer buffers[4];
    buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
    buffers[0].pvBuffer = impl->scratch.data();
    buffers[0].cbBuffer = impl->sizes.cbHeader;
    buffers[1].BufferType = SECBUFFER_DATA;
    buffers[1].pvBuffer = impl->scratch.data() + impl->sizes.cbHeader;
    buffers[1].cbBuffer = static_cast<unsigned long>(chunk);
    buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
    buffers[2].pvBuffer =
        impl->scratch.data() + impl->sizes.cbHeader + chunk;
    buffers[2].cbBuffer = impl->sizes.cbTrailer;
    buffers[3].BufferType = SECBUFFER_EMPTY;
    buffers[3].pvBuffer = nullptr;
    buffers[3].cbBuffer = 0;

    SecBufferDesc desc{};
    desc.ulVersion = SECBUFFER_VERSION;
    desc.cBuffers = 4;
    desc.pBuffers = buffers;

    const SECURITY_STATUS st = EncryptMessage(&impl->ctx, 0, &desc, 0);
    if (st != SEC_E_OK) {
      return false;
    }

    const std::size_t total =
        static_cast<std::size_t>(buffers[0].cbBuffer) +
        static_cast<std::size_t>(buffers[1].cbBuffer) +
        static_cast<std::size_t>(buffers[2].cbBuffer);
    if (total > 0) {
      out_cipher.insert(out_cipher.end(), impl->scratch.data(),
                        impl->scratch.data() + total);
    }
    offset += chunk;
  }
  return true;
}

bool ServerDecryptBuffer(ServerContext& ctx,
                         std::vector<std::uint8_t>& enc_buf,
                         std::vector<std::uint8_t>& plain_out,
                         bool& out_need_more) {
  plain_out.clear();
  out_need_more = false;
  if (!ctx.impl) {
    return false;
  }
  auto* impl = static_cast<ServerContextImpl*>(ctx.impl);
  if (!impl->handshake_done) {
    return false;
  }

  while (!enc_buf.empty()) {
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

    const SECURITY_STATUS st = DecryptMessage(&impl->ctx, &desc, 0, nullptr);
    if (st == SEC_E_INCOMPLETE_MESSAGE) {
      out_need_more = true;
      return true;
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
        plain_out.insert(plain_out.end(), p, p + b.cbBuffer);
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
    if (has_extra && extra_len > 0 && extra_len <= enc_buf.size()) {
      std::vector<std::uint8_t> keep(enc_buf.end() - extra_len, enc_buf.end());
      enc_buf.swap(keep);
    } else {
      enc_buf.clear();
      break;
    }
  }
  return true;
}

void Close(ServerContext& ctx) {
  if (ctx.impl) {
    delete static_cast<ServerContextImpl*>(ctx.impl);
    ctx.impl = nullptr;
  }
}

void Close(ServerCredentials& creds) {
  if (creds.impl) {
    delete static_cast<ServerCredentialsImpl*>(creds.impl);
    creds.impl = nullptr;
  }
}

}  // namespace mi::platform::tls
