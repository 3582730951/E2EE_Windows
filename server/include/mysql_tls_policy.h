#ifndef MI_E2EE_SERVER_MYSQL_TLS_POLICY_H
#define MI_E2EE_SERVER_MYSQL_TLS_POLICY_H

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

#ifdef MI_E2EE_ENABLE_MYSQL
#include <mysql.h>
#endif

namespace mi::server::mysql_tls {

#ifdef MI_E2EE_ENABLE_MYSQL

enum class SslMode : unsigned char {
  kRequired = 0,
  kPreferred = 1,
  kDisabled = 2,
};

inline SslMode DefaultSslMode() {
#if defined(MI_E2EE_SECURE_RELEASE)
  return SslMode::kRequired;
#else
  return SslMode::kPreferred;
#endif
}

inline SslMode ParseSslModeFromEnv() {
  const char* env = std::getenv("MI_E2EE_MYSQL_SSL_MODE");
  if (!env || *env == '\0') {
    return DefaultSslMode();
  }
  std::string value(env);
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  if (value == "required" || value == "require" || value == "on" ||
      value == "true" || value == "1") {
    return SslMode::kRequired;
  }
#if defined(MI_E2EE_SECURE_RELEASE)
  // Secure release forbids weakening DB transport policy at runtime.
  return SslMode::kRequired;
#else
  if (value == "preferred" || value == "prefer") {
    return SslMode::kPreferred;
  }
  if (value == "disabled" || value == "disable" || value == "off" ||
      value == "false" || value == "0") {
    return SslMode::kDisabled;
  }
  return DefaultSslMode();
#endif
}

inline void ApplySslModeOption(MYSQL* conn, SslMode mode) {
  if (!conn) {
    return;
  }

#if defined(MYSQL_OPT_SSL_MODE)
  unsigned int ssl_mode = 0;
#if defined(SSL_MODE_PREFERRED)
  ssl_mode = SSL_MODE_PREFERRED;
#endif
#if defined(SSL_MODE_REQUIRED)
  if (mode == SslMode::kRequired) {
    ssl_mode = SSL_MODE_REQUIRED;
  }
#endif
#if defined(SSL_MODE_DISABLED)
  if (mode == SslMode::kDisabled) {
    ssl_mode = SSL_MODE_DISABLED;
  }
#endif
  (void)mysql_options(conn, MYSQL_OPT_SSL_MODE, &ssl_mode);
#elif defined(MYSQL_OPT_SSL_ENFORCE)
  const bool enforce = (mode == SslMode::kRequired);
  (void)mysql_options(conn, MYSQL_OPT_SSL_ENFORCE, &enforce);
#endif
}

inline unsigned long ConnectFlagsForSslMode(SslMode mode) {
  unsigned long flags = 0;
#ifdef CLIENT_SSL
  if (mode != SslMode::kDisabled) {
    flags |= CLIENT_SSL;
  }
#endif
  return flags;
}

inline bool VerifyNegotiatedSsl(MYSQL* conn, SslMode mode) {
  if (mode != SslMode::kRequired || !conn) {
    return true;
  }
  const char* cipher = mysql_get_ssl_cipher(conn);
  return cipher && *cipher != '\0';
}

#endif  // MI_E2EE_ENABLE_MYSQL

}  // namespace mi::server::mysql_tls

#endif  // MI_E2EE_SERVER_MYSQL_TLS_POLICY_H
