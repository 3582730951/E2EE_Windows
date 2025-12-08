#include "c_api.h"

#include <cstdlib>
#include <cstring>
#include <vector>

#include "listener.h"
#include "server_app.h"
#include "frame.h"
#include "api_service.h"
#include "session_manager.h"

struct mi_server_handle {
  mi::server::ServerApp app;
  mi::server::Listener* listener{nullptr};
};

extern "C" {

mi_server_handle* mi_server_create(const char* config_path) {
  auto handle = static_cast<mi_server_handle*>(std::malloc(sizeof(mi_server_handle)));
  if (!handle) {
    return nullptr;
  }
  new (&handle->app) mi::server::ServerApp();
  std::string err;
  const std::string path = config_path ? std::string(config_path) : "config.ini";
  if (!handle->app.Init(path, err)) {
    handle->app.~ServerApp();
    std::free(handle);
    return nullptr;
  }
  handle->listener = new mi::server::Listener(&handle->app);
  return handle;
}

void mi_server_destroy(mi_server_handle* handle) {
  if (!handle) {
    return;
  }
  delete handle->listener;
  handle->app.~ServerApp();
  std::free(handle);
}

int mi_server_process(mi_server_handle* handle,
                      const std::uint8_t* data,
                      std::size_t len,
                      std::uint8_t** out_buf,
                      std::size_t* out_len) {
  if (!handle || !handle->listener || !data || len == 0 || !out_buf || !out_len) {
    return 0;
  }
  std::vector<std::uint8_t> out;
  std::vector<std::uint8_t> in_bytes(data, data + len);
  if (!handle->listener->Process(in_bytes, out)) {
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
}

void mi_server_free(std::uint8_t* buf) {
  std::free(buf);
}

int mi_server_login(mi_server_handle* handle,
                    const char* username,
                    const char* password,
                    char** out_token) {
  if (!handle || !username || !password || !out_token) {
    return 0;
  }
  mi::server::Session session;
  std::string error;
  auto* sessions = handle->app.sessions();
  if (!sessions) {
    return 0;
  }
  if (!sessions->Login(username, password, session, error)) {
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
}

int mi_server_logout(mi_server_handle* handle, const char* token) {
  if (!handle || !token) {
    return 0;
  }
  auto* sessions = handle->app.sessions();
  if (!sessions) {
    return 0;
  }
  sessions->Logout(token);
  return 1;
}

}  // extern "C"
