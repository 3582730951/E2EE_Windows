#include "c_api.h"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <vector>

#include "listener.h"
#include "server_app.h"
#include "frame.h"
#include "api_service.h"
#include "session_manager.h"

struct mi_server_handle {
  mi::server::ServerApp app;
  std::unique_ptr<mi::server::Listener> listener;
};

extern "C" {

void mi_server_get_version(mi_sdk_version* out_version) {
  if (!out_version) {
    return;
  }
  out_version->major = MI_E2EE_SERVER_SDK_VERSION_MAJOR;
  out_version->minor = MI_E2EE_SERVER_SDK_VERSION_MINOR;
  out_version->patch = MI_E2EE_SERVER_SDK_VERSION_PATCH;
  out_version->abi = MI_E2EE_SERVER_SDK_ABI_VERSION;
}

std::uint32_t mi_server_get_capabilities(void) {
  std::uint32_t caps =
      MI_SERVER_CAP_TLS | MI_SERVER_CAP_KCP | MI_SERVER_CAP_OPAQUE |
      MI_SERVER_CAP_OPS;
#ifdef MI_E2EE_ENABLE_MYSQL
  caps |= MI_SERVER_CAP_MYSQL;
#endif
  return caps;
}

mi_server_handle* mi_server_create(const char* config_path) {
  try {
    auto* handle = new (std::nothrow) mi_server_handle();
    if (!handle) {
      return nullptr;
    }
    std::string err;
    const std::string path =
        config_path ? std::string(config_path) : std::string("config.ini");
    if (!handle->app.Init(path, err)) {
      delete handle;
      return nullptr;
    }
    handle->listener = std::make_unique<mi::server::Listener>(&handle->app);
    return handle;
  } catch (...) {
    return nullptr;
  }
}

void mi_server_destroy(mi_server_handle* handle) {
  try {
    delete handle;
  } catch (...) {
    // never throw across C boundary
  }
}

int mi_server_process(mi_server_handle* handle,
                      const std::uint8_t* data,
                      std::size_t len,
                      std::uint8_t** out_buf,
                      std::size_t* out_len) {
  if (out_buf) {
    *out_buf = nullptr;
  }
  if (out_len) {
    *out_len = 0;
  }
  if (!handle || !handle->listener || !data || len == 0 || !out_buf || !out_len) {
    return 0;
  }
  try {
    std::vector<std::uint8_t> out;
    std::vector<std::uint8_t> in_bytes(data, data + len);
    if (!handle->listener->Process(in_bytes, out, mi::server::TransportKind::kLocal)) {
      return 0;
    }
    *out_len = out.size();
    if (out.empty()) {
      *out_buf = nullptr;
      return 1;
    }
    auto* buf = static_cast<std::uint8_t*>(std::malloc(out.size()));
    if (!buf) {
      return 0;
    }
    std::memcpy(buf, out.data(), out.size());
    *out_buf = buf;
    return 1;
  } catch (...) {
    return 0;
  }
}

void mi_server_free(std::uint8_t* buf) {
  std::free(buf);
}

int mi_server_login(mi_server_handle* handle,
                    const char* username,
                    const char* password,
                    char** out_token) {
  if (out_token) {
    *out_token = nullptr;
  }
  if (!handle || !username || !password || !out_token) {
    return 0;
  }
  try {
    if (!handle->app.config().server.allow_legacy_login) {
      return 0;
    }
    mi::server::Session session;
    std::string error;
    auto* sessions = handle->app.sessions();
    if (!sessions) {
      return 0;
    }
    if (!sessions->Login(username, password, mi::server::TransportKind::kLocal,
                         session, error)) {
      return 0;
    }
    if (session.token.size() > (std::numeric_limits<std::size_t>::max)() - 1) {
      return 0;
    }
    char* buf = static_cast<char*>(std::malloc(session.token.size() + 1));
    if (!buf) {
      return 0;
    }
    std::memcpy(buf, session.token.data(), session.token.size());
    buf[session.token.size()] = '\0';
    *out_token = buf;
    return 1;
  } catch (...) {
    return 0;
  }
}

int mi_server_logout(mi_server_handle* handle, const char* token) {
  if (!handle || !token) {
    return 0;
  }
  try {
    auto* sessions = handle->app.sessions();
    if (!sessions) {
      return 0;
    }
    sessions->Logout(token);
    return 1;
  } catch (...) {
    return 0;
  }
}

}  // extern "C"
