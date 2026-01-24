#include "platform_tls.h"

#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif

namespace mi::platform::tls {

namespace {

#if defined(__APPLE__)
bool LoadAppleKeychainCaBundle(SSL_CTX* ctx, std::string& error) {
  error.clear();
  if (!ctx) {
    error = "tls ctx missing";
    return false;
  }
  CFArrayRef anchors = nullptr;
  const OSStatus status = SecTrustCopyAnchorCertificates(&anchors);
  if (status != errSecSuccess || !anchors) {
    error = "tls ca bundle missing";
    return false;
  }

  X509_STORE* store = SSL_CTX_get_cert_store(ctx);
  if (!store) {
    CFRelease(anchors);
    error = "tls ca bundle load failed";
    return false;
  }

  bool added_any = false;
  const CFIndex count = CFArrayGetCount(anchors);
  for (CFIndex i = 0; i < count; ++i) {
    auto* cert = static_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(anchors, i)));
    if (!cert) {
      continue;
    }
    CFDataRef data = SecCertificateCopyData(cert);
    if (!data) {
      continue;
    }
    const auto* bytes =
        reinterpret_cast<const unsigned char*>(CFDataGetBytePtr(data));
    const CFIndex len = CFDataGetLength(data);
    if (!bytes || len <= 0) {
      CFRelease(data);
      continue;
    }
    const unsigned char* p = bytes;
    X509* x509 = d2i_X509(nullptr, &p, static_cast<long>(len));
    if (!x509) {
      CFRelease(data);
      continue;
    }
    ERR_clear_error();
    if (X509_STORE_add_cert(store, x509) == 1) {
      added_any = true;
    } else {
      const unsigned long err = ERR_peek_last_error();
      if (ERR_GET_LIB(err) == ERR_LIB_X509 &&
          ERR_GET_REASON(err) == X509_R_CERT_ALREADY_IN_HASH_TABLE) {
        added_any = true;
      }
      ERR_clear_error();
    }
    X509_free(x509);
    CFRelease(data);
  }
  CFRelease(anchors);

  if (!added_any) {
    error = "tls ca bundle missing";
    return false;
  }
  return true;
}
#endif

bool LoadDefaultCaBundle(SSL_CTX* ctx, std::string& error) {
  if (!ctx) {
    error = "tls ctx missing";
    return false;
  }
  const bool default_ok = SSL_CTX_set_default_verify_paths(ctx) == 1;
#if defined(__APPLE__)
  std::string keychain_err;
  if (LoadAppleKeychainCaBundle(ctx, keychain_err)) {
    return true;
  }
#endif
#if defined(__ANDROID__)
  const char* const candidates[] = {
      "/apex/com.android.conscrypt/cacerts",
      "/system/etc/security/cacerts",
      "/system/etc/security/cacerts_google",
  };
  for (const char* path : candidates) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
      continue;
    }
    if (std::filesystem::is_directory(path, ec) && !ec) {
      if (SSL_CTX_load_verify_locations(ctx, nullptr, path) == 1) {
        return true;
      }
    }
  }
#else
  const char* const candidates[] = {
#if defined(__APPLE__)
      "/etc/ssl/cert.pem",
      "/etc/ssl/certs/ca-certificates.crt",
      "/usr/local/etc/openssl@3/cert.pem",
      "/opt/homebrew/etc/openssl@3/cert.pem",
      "/usr/local/etc/openssl/cert.pem",
      "/opt/homebrew/etc/openssl/cert.pem",
#else
      "/etc/ssl/certs/ca-certificates.crt",
      "/etc/pki/tls/certs/ca-bundle.crt",
      "/etc/ssl/ca-bundle.pem",
      "/etc/ssl/cert.pem",
      "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
      "/usr/local/share/certs/ca-root-nss.crt",
#endif
  };
  for (const char* path : candidates) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
      continue;
    }
    const bool is_dir = std::filesystem::is_directory(path, ec);
    if (ec) {
      continue;
    }
    const char* ca_file = is_dir ? nullptr : path;
    const char* ca_dir = is_dir ? path : nullptr;
    if (SSL_CTX_load_verify_locations(ctx, ca_file, ca_dir) == 1) {
      return true;
    }
  }
#endif
  if (default_ok) {
    return true;
  }
  error = "tls ca bundle missing";
  return false;
}

struct ClientContextImpl {
  SSL_CTX* ctx{nullptr};
  SSL* ssl{nullptr};

  ~ClientContextImpl() {
    if (ssl) {
      SSL_shutdown(ssl);
      SSL_free(ssl);
      ssl = nullptr;
    }
    if (ctx) {
      SSL_CTX_free(ctx);
      ctx = nullptr;
    }
  }
};

struct ServerCredentialsImpl {
  SSL_CTX* ctx{nullptr};

  ~ServerCredentialsImpl() {
    if (ctx) {
      SSL_CTX_free(ctx);
      ctx = nullptr;
    }
  }
};

struct ServerContextImpl {
  SSL* ssl{nullptr};
  BIO* rbio{nullptr};
  BIO* wbio{nullptr};
  bool handshake_done{false};

  ~ServerContextImpl() {
    if (ssl) {
      SSL_free(ssl);
      ssl = nullptr;
    }
    rbio = nullptr;
    wbio = nullptr;
  }
};

bool EnsureOpenSsl() {
  static std::once_flag init_once;
  static bool ok = false;
  std::call_once(init_once, []() {
    ok = OPENSSL_init_ssl(0, nullptr) == 1;
  });
  return ok;
}

std::string GetOpenSslError() {
  const unsigned long err = ERR_get_error();
  if (err == 0) {
    return "openssl error";
  }
  char buf[256] = {};
  ERR_error_string_n(err, buf, sizeof(buf));
  return std::string(buf);
}

std::string ToLowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool IsPkcs12Path(const std::filesystem::path& path) {
  const std::string ext = ToLowerAscii(path.extension().string());
  return ext == ".pfx" || ext == ".p12";
}

bool ReadFileBytes(const std::filesystem::path& path,
                   std::vector<std::uint8_t>& out,
                   std::string& error) {
  out.clear();
  error.clear();
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    error = "tls_cert not found";
    return false;
  }
  std::error_code ec;
  const auto size = std::filesystem::file_size(path, ec);
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
  out.resize(static_cast<std::size_t>(size));
  f.read(reinterpret_cast<char*>(out.data()),
         static_cast<std::streamsize>(out.size()));
  if (!f || f.gcount() != static_cast<std::streamsize>(out.size())) {
    error = "tls_cert read failed";
    out.clear();
    return false;
  }
  return true;
}

bool GenerateSelfSigned(const std::filesystem::path& out_path,
                        std::string& error) {
  error.clear();
  if (out_path.empty()) {
    error = "tls_cert empty";
    return false;
  }
  if (!EnsureOpenSsl()) {
    error = "openssl init failed";
    return false;
  }

  std::error_code ec;
  const auto dir = out_path.has_parent_path() ? out_path.parent_path()
                                              : std::filesystem::path{};
  if (!dir.empty()) {
    std::filesystem::create_directories(dir, ec);
  }

  EVP_PKEY* pkey = EVP_PKEY_new();
  if (!pkey) {
    error = "EVP_PKEY_new failed";
    return false;
  }

  RSA* rsa = RSA_new();
  BIGNUM* e = BN_new();
  if (!rsa || !e || BN_set_word(e, RSA_F4) != 1 ||
      RSA_generate_key_ex(rsa, 2048, e, nullptr) != 1) {
    if (e) {
      BN_free(e);
    }
    if (rsa) {
      RSA_free(rsa);
    }
    EVP_PKEY_free(pkey);
    error = GetOpenSslError();
    return false;
  }
  BN_free(e);

  if (EVP_PKEY_assign_RSA(pkey, rsa) != 1) {
    RSA_free(rsa);
    EVP_PKEY_free(pkey);
    error = "EVP_PKEY_assign_RSA failed";
    return false;
  }

  X509* cert = X509_new();
  if (!cert) {
    EVP_PKEY_free(pkey);
    error = "X509_new failed";
    return false;
  }
  X509_set_version(cert, 2);
  ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
  X509_gmtime_adj(X509_get_notBefore(cert), 0);
  X509_gmtime_adj(X509_get_notAfter(cert), 10L * 365L * 24L * 60L * 60L);
  X509_set_pubkey(cert, pkey);

  X509_NAME* name = X509_get_subject_name(cert);
  if (!name ||
      X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
                                 reinterpret_cast<const unsigned char*>("MI_E2EE_Server"),
                                 -1, -1, 0) != 1 ||
      X509_set_issuer_name(cert, name) != 1) {
    X509_free(cert);
    EVP_PKEY_free(pkey);
    error = GetOpenSslError();
    return false;
  }

  if (X509_sign(cert, pkey, EVP_sha256()) == 0) {
    X509_free(cert);
    EVP_PKEY_free(pkey);
    error = GetOpenSslError();
    return false;
  }

  const bool write_pkcs12 = IsPkcs12Path(out_path);
  bool ok = false;
  if (write_pkcs12) {
    PKCS12* p12 = PKCS12_create("", "mi_e2ee_server", pkey, cert,
                                nullptr, 0, 0, 0, 0, 0);
    if (!p12) {
      error = GetOpenSslError();
    } else {
      FILE* fp = fopen(out_path.string().c_str(), "wb");
      if (!fp) {
        error = "write tls_cert failed";
      } else {
        ok = i2d_PKCS12_fp(fp, p12) == 1;
        fclose(fp);
        if (!ok) {
          error = GetOpenSslError();
        }
      }
      PKCS12_free(p12);
    }
  } else {
    FILE* fp = fopen(out_path.string().c_str(), "wb");
    if (!fp) {
      error = "write tls_cert failed";
    } else {
      const bool wrote_key = PEM_write_PrivateKey(fp, pkey, nullptr, nullptr, 0,
                                                  nullptr, nullptr) == 1;
      const bool wrote_cert = PEM_write_X509(fp, cert) == 1;
      fclose(fp);
      ok = wrote_key && wrote_cert;
      if (!ok) {
        error = GetOpenSslError();
      }
    }
  }

  X509_free(cert);
  EVP_PKEY_free(pkey);
  return ok;
}

bool LoadPkcs12(const std::filesystem::path& path,
                SSL_CTX* ctx,
                std::string& error) {
  std::vector<std::uint8_t> bytes;
  if (!ReadFileBytes(path, bytes, error)) {
    return false;
  }
  const unsigned char* p = bytes.data();
  PKCS12* p12 = d2i_PKCS12(nullptr, &p, static_cast<long>(bytes.size()));
  if (!p12) {
    error = GetOpenSslError();
    return false;
  }
  EVP_PKEY* pkey = nullptr;
  X509* cert = nullptr;
  STACK_OF(X509)* ca = nullptr;
  const int parsed = PKCS12_parse(p12, "", &pkey, &cert, &ca);
  PKCS12_free(p12);
  if (parsed != 1 || !pkey || !cert) {
    if (pkey) {
      EVP_PKEY_free(pkey);
    }
    if (cert) {
      X509_free(cert);
    }
    if (ca) {
      sk_X509_pop_free(ca, X509_free);
    }
    error = GetOpenSslError();
    return false;
  }

  bool ok = true;
  if (SSL_CTX_use_certificate(ctx, cert) != 1) {
    ok = false;
    error = GetOpenSslError();
  }
  if (ok && SSL_CTX_use_PrivateKey(ctx, pkey) != 1) {
    ok = false;
    error = GetOpenSslError();
  }
  if (ok && SSL_CTX_check_private_key(ctx) != 1) {
    ok = false;
    error = "tls private key mismatch";
  }
  if (ok && ca) {
    const int count = sk_X509_num(ca);
    for (int i = 0; i < count; ++i) {
      X509* chain = sk_X509_value(ca, i);
      if (chain) {
        X509* dup = X509_dup(chain);
        if (dup) {
          SSL_CTX_add_extra_chain_cert(ctx, dup);
        }
      }
    }
  }

  EVP_PKEY_free(pkey);
  X509_free(cert);
  if (ca) {
    sk_X509_pop_free(ca, X509_free);
  }
  return ok;
}

bool LoadPem(const std::filesystem::path& path,
             SSL_CTX* ctx,
             std::string& error) {
  error.clear();
  BIO* cert_bio = BIO_new_file(path.string().c_str(), "rb");
  if (!cert_bio) {
    error = "tls_cert not found";
    return false;
  }
  X509* cert = PEM_read_bio_X509(cert_bio, nullptr, nullptr, nullptr);
  BIO_free(cert_bio);
  if (!cert) {
    error = GetOpenSslError();
    return false;
  }

  BIO* key_bio = BIO_new_file(path.string().c_str(), "rb");
  if (!key_bio) {
    X509_free(cert);
    error = "tls_cert not found";
    return false;
  }
  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
  BIO_free(key_bio);
  if (!pkey) {
    X509_free(cert);
    error = GetOpenSslError();
    return false;
  }

  bool ok = true;
  if (SSL_CTX_use_certificate(ctx, cert) != 1) {
    ok = false;
    error = GetOpenSslError();
  }
  if (ok && SSL_CTX_use_PrivateKey(ctx, pkey) != 1) {
    ok = false;
    error = GetOpenSslError();
  }
  if (ok && SSL_CTX_check_private_key(ctx) != 1) {
    ok = false;
    error = "tls private key mismatch";
  }

  EVP_PKEY_free(pkey);
  X509_free(cert);
  return ok;
}

void DrainBio(BIO* bio, std::vector<std::uint8_t>& out) {
  if (!bio) {
    return;
  }
  std::uint8_t buf[4096];
  for (;;) {
    const int n = BIO_read(bio, buf, static_cast<int>(sizeof(buf)));
    if (n > 0) {
      out.insert(out.end(), buf, buf + n);
      continue;
    }
    break;
  }
}

ServerContextImpl* GetServerContext(ServerCredentials& creds, ServerContext& ctx,
                                    std::string& error) {
  error.clear();
  if (ctx.impl) {
    return static_cast<ServerContextImpl*>(ctx.impl);
  }
  if (!creds.impl) {
    error = "tls credentials missing";
    return nullptr;
  }
  auto* cred = static_cast<ServerCredentialsImpl*>(creds.impl);
  if (!cred->ctx) {
    error = "tls credentials missing";
    return nullptr;
  }
  auto impl = std::make_unique<ServerContextImpl>();
  impl->ssl = SSL_new(cred->ctx);
  if (!impl->ssl) {
    error = GetOpenSslError();
    return nullptr;
  }
  impl->rbio = BIO_new(BIO_s_mem());
  impl->wbio = BIO_new(BIO_s_mem());
  if (!impl->rbio || !impl->wbio) {
    if (impl->rbio) {
      BIO_free(impl->rbio);
      impl->rbio = nullptr;
    }
    if (impl->wbio) {
      BIO_free(impl->wbio);
      impl->wbio = nullptr;
    }
    error = "BIO_new failed";
    return nullptr;
  }
  SSL_set_bio(impl->ssl, impl->rbio, impl->wbio);
  SSL_set_accept_state(impl->ssl);
  ctx.impl = impl.release();
  return static_cast<ServerContextImpl*>(ctx.impl);
}

}  // namespace

bool IsSupported() {
  return EnsureOpenSsl();
}

bool IsStubbed() {
  return false;
}

const char* ProviderName() {
  return "openssl";
}

bool ClientHandshake(net::Socket sock, const std::string& host,
                     const ClientVerifyConfig& verify,
                     ClientContext& ctx,
                     std::vector<std::uint8_t>& out_server_cert_der,
                     std::vector<std::uint8_t>& out_enc_buf,
                     std::string& error) {
  out_server_cert_der.clear();
  out_enc_buf.clear();
  error.clear();

  if (!EnsureOpenSsl()) {
    error = "openssl init failed";
    return false;
  }

  auto impl = std::make_unique<ClientContextImpl>();
  impl->ctx = SSL_CTX_new(TLS_client_method());
  if (!impl->ctx) {
    error = "SSL_CTX_new failed";
    return false;
  }

  SSL_CTX_set_options(impl->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
  SSL_CTX_set_options(impl->ctx, SSL_OP_NO_COMPRESSION);
  SSL_CTX_set_min_proto_version(impl->ctx, TLS1_2_VERSION);
  if (verify.verify_peer) {
    std::string ca_path_str;
    if (!verify.ca_bundle_path.empty()) {
      const std::filesystem::path ca_path(verify.ca_bundle_path);
      ca_path_str = ca_path.string();
      std::error_code ec;
      const bool is_dir = std::filesystem::is_directory(ca_path, ec);
      const char* ca_file = is_dir ? nullptr : ca_path_str.c_str();
      const char* ca_dir = is_dir ? ca_path_str.c_str() : nullptr;
      if (SSL_CTX_load_verify_locations(impl->ctx, ca_file, ca_dir) != 1) {
        error = "tls ca bundle load failed";
        return false;
      }
    } else {
      if (!LoadDefaultCaBundle(impl->ctx, error)) {
        return false;
      }
    }
    SSL_CTX_set_verify(impl->ctx, SSL_VERIFY_PEER, nullptr);
  } else {
    SSL_CTX_set_verify(impl->ctx, SSL_VERIFY_NONE, nullptr);
  }

  impl->ssl = SSL_new(impl->ctx);
  if (!impl->ssl) {
    error = "SSL_new failed";
    return false;
  }
  SSL_set_mode(impl->ssl, SSL_MODE_AUTO_RETRY);
  if (!host.empty()) {
    SSL_set_tlsext_host_name(impl->ssl, host.c_str());
  }
  if (verify.verify_peer && verify.verify_hostname && !host.empty()) {
    if (SSL_set1_host(impl->ssl, host.c_str()) != 1) {
      error = "tls host verify setup failed";
      return false;
    }
  }
  if (SSL_set_fd(impl->ssl, sock) != 1) {
    error = "SSL_set_fd failed";
    return false;
  }

  while (true) {
    const int ret = SSL_connect(impl->ssl);
    if (ret == 1) {
      break;
    }
    const int err = SSL_get_error(impl->ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
      continue;
    }
    error = GetOpenSslError();
    return false;
  }
  if (verify.verify_peer) {
    const long verify_result = SSL_get_verify_result(impl->ssl);
    if (verify_result != X509_V_OK) {
      error = X509_verify_cert_error_string(verify_result);
      return false;
    }
  }

  X509* cert = SSL_get_peer_certificate(impl->ssl);
  if (!cert) {
    error = "remote cert unavailable";
    return false;
  }
  const int cert_len = i2d_X509(cert, nullptr);
  if (cert_len <= 0) {
    X509_free(cert);
    error = "cert encode failed";
    return false;
  }
  out_server_cert_der.resize(static_cast<std::size_t>(cert_len));
  unsigned char* out_ptr = out_server_cert_der.data();
  if (i2d_X509(cert, &out_ptr) <= 0) {
    X509_free(cert);
    out_server_cert_der.clear();
    error = "cert encode failed";
    return false;
  }
  X509_free(cert);

  ctx.impl = impl.release();
  return true;
}

bool EncryptAndSend(net::Socket /*sock*/, ClientContext& ctx,
                    const std::vector<std::uint8_t>& plain) {
  if (!ctx.impl) {
    return false;
  }
  auto* impl = static_cast<ClientContextImpl*>(ctx.impl);
  std::size_t sent = 0;
  while (sent < plain.size()) {
    const std::size_t remaining = plain.size() - sent;
    const std::size_t chunk =
        std::min<std::size_t>(remaining,
                              static_cast<std::size_t>((std::numeric_limits<int>::max)()));
    const int ret =
        SSL_write(impl->ssl, plain.data() + sent, static_cast<int>(chunk));
    if (ret <= 0) {
      const int err = SSL_get_error(impl->ssl, ret);
      if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        continue;
      }
      return false;
    }
    sent += static_cast<std::size_t>(ret);
  }
  return true;
}

bool DecryptToPlain(net::Socket /*sock*/, ClientContext& ctx,
                    std::vector<std::uint8_t>& enc_buf,
                    std::vector<std::uint8_t>& plain_out) {
  plain_out.clear();
  enc_buf.clear();
  if (!ctx.impl) {
    return false;
  }
  auto* impl = static_cast<ClientContextImpl*>(ctx.impl);
  std::uint8_t buf[4096];
  while (true) {
    const int ret = SSL_read(impl->ssl, buf, sizeof(buf));
    if (ret > 0) {
      plain_out.assign(buf, buf + ret);
      return true;
    }
    const int err = SSL_get_error(impl->ssl, ret);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
      continue;
    }
    return false;
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
  if (!EnsureOpenSsl()) {
    error = "openssl init failed";
    return false;
  }
  if (pfx_path.empty()) {
    error = "tls_cert empty";
    return false;
  }

  const std::filesystem::path path(pfx_path);
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    std::string gen_err;
    if (!GenerateSelfSigned(path, gen_err)) {
      error = gen_err.empty() ? "generate tls_cert failed" : gen_err;
      return false;
    }
  }

  auto impl = std::make_unique<ServerCredentialsImpl>();
  impl->ctx = SSL_CTX_new(TLS_server_method());
  if (!impl->ctx) {
    error = "SSL_CTX_new failed";
    return false;
  }
  SSL_CTX_set_options(impl->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
  SSL_CTX_set_options(impl->ctx, SSL_OP_NO_COMPRESSION);
  SSL_CTX_set_min_proto_version(impl->ctx, TLS1_2_VERSION);

  std::string load_err;
  const bool is_pfx = IsPkcs12Path(path);
  const bool loaded =
      is_pfx ? LoadPkcs12(path, impl->ctx, load_err)
             : LoadPem(path, impl->ctx, load_err);
  if (!loaded) {
    error = load_err.empty() ? "load tls_cert failed" : load_err;
    return false;
  }

  out.impl = impl.release();
  return true;
}

bool ServerHandshake(net::Socket sock, ServerCredentials& creds,
                     ServerContext& ctx,
                     std::vector<std::uint8_t>& out_extra,
                     std::string& error) {
  out_extra.clear();
  error.clear();
  std::vector<std::uint8_t> in_buf;
  std::vector<std::uint8_t> out_tokens;
  bool done = false;
  while (!done) {
    if (in_buf.empty()) {
      if (!net::RecvSome(sock, in_buf)) {
        error = "tls handshake recv failed";
        return false;
      }
    }
    if (!ServerHandshakeStep(creds, ctx, in_buf, out_tokens, done, error)) {
      return false;
    }
    if (!out_tokens.empty()) {
      if (!net::SendAll(sock, out_tokens.data(), out_tokens.size())) {
        error = "tls send handshake failed";
        return false;
      }
    }
  }
  out_extra.swap(in_buf);
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
  auto* impl = GetServerContext(creds, ctx, error);
  if (!impl) {
    return false;
  }
  if (impl->handshake_done) {
    out_done = true;
    return true;
  }
  if (!in_buf.empty()) {
    const int wrote = BIO_write(impl->rbio, in_buf.data(),
                                static_cast<int>(in_buf.size()));
    if (wrote <= 0) {
      error = GetOpenSslError();
      return false;
    }
    in_buf.erase(in_buf.begin(),
                 in_buf.begin() + static_cast<std::ptrdiff_t>(wrote));
  }

  const int ret = SSL_do_handshake(impl->ssl);
  if (ret == 1) {
    impl->handshake_done = true;
    out_done = true;
  } else {
    const int err = SSL_get_error(impl->ssl, ret);
    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
      error = GetOpenSslError();
      return false;
    }
  }

  if (impl->wbio) {
    DrainBio(impl->wbio, out_tokens);
  }
  if (impl->handshake_done && impl->rbio) {
    in_buf.clear();
    DrainBio(impl->rbio, in_buf);
  }
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
    bool need_more = false;
    if (!ServerDecryptBuffer(ctx, enc_buf, plain_out, need_more)) {
      return false;
    }
    if (!plain_out.empty()) {
      return true;
    }
    if (need_more) {
      if (!net::RecvSome(sock, enc_buf)) {
        return false;
      }
      continue;
    }
    if (!net::RecvSome(sock, enc_buf)) {
      return false;
    }
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
  if (!impl->handshake_done) {
    return false;
  }

  std::size_t offset = 0;
  while (offset < plain.size()) {
    const std::size_t remaining = plain.size() - offset;
    const int chunk =
        remaining > static_cast<std::size_t>((std::numeric_limits<int>::max)())
            ? (std::numeric_limits<int>::max)()
            : static_cast<int>(remaining);
    const int ret = SSL_write(impl->ssl, plain.data() + offset, chunk);
    if (ret <= 0) {
      const int err = SSL_get_error(impl->ssl, ret);
      if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
        DrainBio(impl->wbio, out_cipher);
        continue;
      }
      return false;
    }
    offset += static_cast<std::size_t>(ret);
    DrainBio(impl->wbio, out_cipher);
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

  if (!enc_buf.empty()) {
    const int wrote = BIO_write(impl->rbio, enc_buf.data(),
                                static_cast<int>(enc_buf.size()));
    if (wrote <= 0) {
      return false;
    }
    enc_buf.erase(enc_buf.begin(),
                  enc_buf.begin() + static_cast<std::ptrdiff_t>(wrote));
  }

  std::uint8_t buf[4096];
  while (true) {
    const int ret = SSL_read(impl->ssl, buf, sizeof(buf));
    if (ret > 0) {
      plain_out.insert(plain_out.end(), buf, buf + ret);
      continue;
    }
    const int err = SSL_get_error(impl->ssl, ret);
    if (err == SSL_ERROR_WANT_READ) {
      out_need_more = true;
      return true;
    }
    if (err == SSL_ERROR_WANT_WRITE) {
      return false;
    }
    return false;
  }
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
