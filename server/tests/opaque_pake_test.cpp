#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "auth_provider.h"
#include "opaque_pake.h"
#include "session_manager.h"

namespace {

struct RustBuf {
  std::uint8_t* ptr{nullptr};
  std::size_t len{0};
  ~RustBuf() {
    if (ptr && len) {
      mi_opaque_free(ptr, len);
    }
  }
};

std::string RustError(const RustBuf& err, const char* fallback) {
  if (err.ptr && err.len) {
    return std::string(reinterpret_cast<const char*>(err.ptr), err.len);
  }
  return fallback ? std::string(fallback) : std::string();
}

}  // namespace

int main() {
  RustBuf setup;
  RustBuf err;
  if (mi_opaque_server_setup_generate(&setup.ptr, &setup.len, &err.ptr,
                                      &err.len) != 0 ||
      !setup.ptr || setup.len == 0) {
    return 1;
  }
  std::vector<std::uint8_t> setup_vec(setup.ptr, setup.ptr + setup.len);

  auto auth = std::make_unique<mi::server::DemoAuthProvider>(
      mi::server::DemoUserTable{});
  mi::server::SessionManager mgr(std::move(auth), std::chrono::minutes(30),
                                 std::move(setup_vec));

  const std::string username = "alice";
  const std::string password = "alice123";

  // Client registration start.
  RustBuf reg_req;
  RustBuf reg_state;
  RustBuf reg_err;
  if (mi_opaque_client_register_start(
          reinterpret_cast<const std::uint8_t*>(password.data()),
          password.size(), &reg_req.ptr, &reg_req.len, &reg_state.ptr,
          &reg_state.len, &reg_err.ptr, &reg_err.len) != 0 ||
      !reg_req.ptr || reg_req.len == 0 || !reg_state.ptr || reg_state.len == 0) {
    return 1;
  }

  mi::server::OpaqueRegisterStartRequest rs_req;
  rs_req.username = username;
  rs_req.registration_request.assign(reg_req.ptr, reg_req.ptr + reg_req.len);
  mi::server::OpaqueRegisterStartServerHello rs_hello;
  std::string sm_err;
  if (!mgr.OpaqueRegisterStart(rs_req, rs_hello, sm_err)) {
    return 1;
  }

  // Client registration finish.
  RustBuf upload;
  RustBuf reg_err2;
  const std::vector<std::uint8_t> reg_state_vec(reg_state.ptr,
                                                reg_state.ptr + reg_state.len);
  if (mi_opaque_client_register_finish(
          reinterpret_cast<const std::uint8_t*>(username.data()), username.size(),
          reinterpret_cast<const std::uint8_t*>(password.data()),
          password.size(), reg_state_vec.data(), reg_state_vec.size(),
          rs_hello.registration_response.data(),
          rs_hello.registration_response.size(), &upload.ptr, &upload.len,
          &reg_err2.ptr, &reg_err2.len) != 0 ||
      !upload.ptr || upload.len == 0) {
    return 1;
  }

  mi::server::OpaqueRegisterFinishRequest rf_req;
  rf_req.username = username;
  rf_req.registration_upload.assign(upload.ptr, upload.ptr + upload.len);
  if (!mgr.OpaqueRegisterFinish(rf_req, sm_err)) {
    return 1;
  }

  // Client login start.
  RustBuf cred_req;
  RustBuf login_state;
  RustBuf login_err;
  if (mi_opaque_client_login_start(
          reinterpret_cast<const std::uint8_t*>(password.data()), password.size(),
          &cred_req.ptr, &cred_req.len, &login_state.ptr, &login_state.len,
          &login_err.ptr, &login_err.len) != 0 ||
      !cred_req.ptr || cred_req.len == 0 || !login_state.ptr ||
      login_state.len == 0) {
    return 1;
  }

  mi::server::OpaqueLoginStartRequest ls_req;
  ls_req.username = username;
  ls_req.credential_request.assign(cred_req.ptr, cred_req.ptr + cred_req.len);
  mi::server::OpaqueLoginStartServerHello ls_hello;
  if (!mgr.OpaqueLoginStart(ls_req, ls_hello, sm_err)) {
    return 1;
  }

  // Client login finish -> server login finish.
  RustBuf finalization;
  RustBuf session_key;
  RustBuf login_err2;
  const std::vector<std::uint8_t> login_state_vec(
      login_state.ptr, login_state.ptr + login_state.len);
  if (mi_opaque_client_login_finish(
          reinterpret_cast<const std::uint8_t*>(username.data()), username.size(),
          reinterpret_cast<const std::uint8_t*>(password.data()),
          password.size(), login_state_vec.data(), login_state_vec.size(),
          ls_hello.credential_response.data(), ls_hello.credential_response.size(),
          &finalization.ptr, &finalization.len, &session_key.ptr, &session_key.len,
          &login_err2.ptr, &login_err2.len) != 0 ||
      !finalization.ptr || finalization.len == 0 || !session_key.ptr ||
      session_key.len == 0) {
    return 1;
  }

  mi::server::OpaqueLoginFinishRequest lf_req;
  lf_req.login_id = ls_hello.login_id;
  lf_req.credential_finalization.assign(finalization.ptr,
                                        finalization.ptr + finalization.len);
  mi::server::Session session;
  if (!mgr.OpaqueLoginFinish(lf_req, session, sm_err)) {
    return 1;
  }
  if (session.token.empty() || session.username != username) {
    return 1;
  }

  // Wrong password should fail (either client finish fails or server finish fails).
  const std::string wrong_password = "wrong";
  RustBuf wrong_req;
  RustBuf wrong_state;
  RustBuf wrong_err;
  if (mi_opaque_client_login_start(
          reinterpret_cast<const std::uint8_t*>(wrong_password.data()),
          wrong_password.size(), &wrong_req.ptr, &wrong_req.len, &wrong_state.ptr,
          &wrong_state.len, &wrong_err.ptr, &wrong_err.len) != 0 ||
      !wrong_req.ptr || wrong_req.len == 0 || !wrong_state.ptr ||
      wrong_state.len == 0) {
    return 1;
  }

  mi::server::OpaqueLoginStartRequest wls_req;
  wls_req.username = username;
  wls_req.credential_request.assign(wrong_req.ptr, wrong_req.ptr + wrong_req.len);
  mi::server::OpaqueLoginStartServerHello wls_hello;
  if (!mgr.OpaqueLoginStart(wls_req, wls_hello, sm_err)) {
    return 1;
  }

  RustBuf wfinal;
  RustBuf wsk;
  RustBuf werr2;
  const std::vector<std::uint8_t> wrong_state_vec(wrong_state.ptr,
                                                  wrong_state.ptr + wrong_state.len);
  const int wrong_finish_rc = mi_opaque_client_login_finish(
      reinterpret_cast<const std::uint8_t*>(username.data()), username.size(),
      reinterpret_cast<const std::uint8_t*>(wrong_password.data()),
      wrong_password.size(), wrong_state_vec.data(), wrong_state_vec.size(),
      wls_hello.credential_response.data(), wls_hello.credential_response.size(),
      &wfinal.ptr, &wfinal.len, &wsk.ptr, &wsk.len, &werr2.ptr, &werr2.len);
  if (wrong_finish_rc == 0 && wfinal.ptr && wfinal.len != 0) {
    mi::server::OpaqueLoginFinishRequest wlf_req;
    wlf_req.login_id = wls_hello.login_id;
    wlf_req.credential_finalization.assign(wfinal.ptr, wfinal.ptr + wfinal.len);
    mi::server::Session wsession;
    if (mgr.OpaqueLoginFinish(wlf_req, wsession, sm_err)) {
      return 1;
    }
  }

  return 0;
}
