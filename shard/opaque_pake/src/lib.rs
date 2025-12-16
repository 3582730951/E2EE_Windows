use std::panic::AssertUnwindSafe;

use opaque_ke::{
    ClientLogin, ClientLoginFinishParameters, ClientRegistration,
    ClientRegistrationFinishParameters, CipherSuite, CredentialFinalization,
    CredentialRequest, CredentialResponse, ServerLogin, ServerLoginParameters, ServerRegistration,
    ServerSetup, Identifiers,
};
use rand::rngs::OsRng;
use serde::{Deserialize, Serialize};
use sha2::Sha512;

const MI_OPAQUE_CONTEXT: &[u8] = b"mi_e2ee_opaque_v1";
const MI_OPAQUE_SERVER_ID: &[u8] = b"mi_e2ee_server_v1";

struct MiOpaqueCipherSuite;

impl CipherSuite for MiOpaqueCipherSuite {
    type OprfCs = opaque_ke::Ristretto255;
    type KeyExchange = opaque_ke::TripleDh<opaque_ke::Ristretto255, Sha512>;
    type Ksf = argon2::Argon2<'static>;
}

#[derive(Serialize, Deserialize)]
struct ClientRegistrationState {
    inner: ClientRegistration<MiOpaqueCipherSuite>,
}

#[derive(Serialize, Deserialize)]
struct ClientLoginState {
    inner: ClientLogin<MiOpaqueCipherSuite>,
}

#[derive(Serialize, Deserialize)]
struct ServerLoginState {
    inner: ServerLogin<MiOpaqueCipherSuite>,
}

fn alloc_out(bytes: Vec<u8>, out_ptr: *mut *mut u8, out_len: *mut usize) -> Result<(), ()> {
    if out_ptr.is_null() || out_len.is_null() {
        return Err(());
    }
    let boxed = bytes.into_boxed_slice();
    let len = boxed.len();
    let ptr = Box::into_raw(boxed) as *mut u8;
    unsafe {
        *out_ptr = ptr;
        *out_len = len;
    }
    Ok(())
}

fn set_error(msg: &str, out_err_ptr: *mut *mut u8, out_err_len: *mut usize) {
    if out_err_ptr.is_null() || out_err_len.is_null() {
        return;
    }
    let _ = alloc_out(msg.as_bytes().to_vec(), out_err_ptr, out_err_len);
}

fn ok_clear_error(out_err_ptr: *mut *mut u8, out_err_len: *mut usize) {
    if out_err_ptr.is_null() || out_err_len.is_null() {
        return;
    }
    unsafe {
        *out_err_ptr = std::ptr::null_mut();
        *out_err_len = 0;
    }
}

fn catch_and_report<F: FnOnce() -> Result<(), String>>(
    f: F,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    ok_clear_error(out_err_ptr, out_err_len);
    match std::panic::catch_unwind(AssertUnwindSafe(f)) {
        Ok(Ok(())) => 0,
        Ok(Err(msg)) => {
            set_error(&msg, out_err_ptr, out_err_len);
            2
        }
        Err(_) => {
            set_error("panic", out_err_ptr, out_err_len);
            3
        }
    }
}

#[no_mangle]
pub extern "C" fn mi_opaque_free(ptr: *mut u8, len: usize) {
    if ptr.is_null() || len == 0 {
        return;
    }
    unsafe {
        let slice = std::slice::from_raw_parts_mut(ptr, len);
        drop(Box::from_raw(slice));
    }
}

#[no_mangle]
pub extern "C" fn mi_opaque_server_setup_generate(
    out_setup_ptr: *mut *mut u8,
    out_setup_len: *mut usize,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    catch_and_report(
        || {
            let mut rng = OsRng;
            let setup = ServerSetup::<MiOpaqueCipherSuite>::new(&mut rng);
            let bytes = setup.serialize().to_vec();
            alloc_out(bytes, out_setup_ptr, out_setup_len)
                .map_err(|_| "bad output".to_string())?;
            Ok(())
        },
        out_err_ptr,
        out_err_len,
    )
}

#[no_mangle]
pub extern "C" fn mi_opaque_server_setup_validate(
    setup_ptr: *const u8,
    setup_len: usize,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    catch_and_report(
        || {
            if setup_ptr.is_null() || setup_len == 0 {
                return Err("setup empty".to_string());
            }
            let setup_bytes = unsafe { std::slice::from_raw_parts(setup_ptr, setup_len) };
            let _ = ServerSetup::<MiOpaqueCipherSuite>::deserialize(setup_bytes)
                .map_err(|_| "setup invalid".to_string())?;
            Ok(())
        },
        out_err_ptr,
        out_err_len,
    )
}

#[no_mangle]
pub extern "C" fn mi_opaque_create_user_password_file(
    setup_ptr: *const u8,
    setup_len: usize,
    user_id_ptr: *const u8,
    user_id_len: usize,
    password_ptr: *const u8,
    password_len: usize,
    out_file_ptr: *mut *mut u8,
    out_file_len: *mut usize,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    catch_and_report(
        || {
            if setup_ptr.is_null() || setup_len == 0 {
                return Err("setup empty".to_string());
            }
            if user_id_ptr.is_null() || user_id_len == 0 {
                return Err("user id empty".to_string());
            }
            if password_ptr.is_null() || password_len == 0 {
                return Err("password empty".to_string());
            }

            let setup_bytes = unsafe { std::slice::from_raw_parts(setup_ptr, setup_len) };
            let user_id = unsafe { std::slice::from_raw_parts(user_id_ptr, user_id_len) };
            let password = unsafe { std::slice::from_raw_parts(password_ptr, password_len) };

            let server_setup = ServerSetup::<MiOpaqueCipherSuite>::deserialize(setup_bytes)
                .map_err(|_| "setup invalid".to_string())?;

            let mut rng = OsRng;
            let client_start =
                ClientRegistration::<MiOpaqueCipherSuite>::start(&mut rng, password)
                    .map_err(|_| "client register start failed".to_string())?;
            let server_start = ServerRegistration::<MiOpaqueCipherSuite>::start(
                &server_setup,
                client_start.message,
                user_id,
            )
            .map_err(|_| "server register start failed".to_string())?;
            let finish = client_start
                .state
                .finish(
                    &mut rng,
                    password,
                    server_start.message,
                    ClientRegistrationFinishParameters::new(
                        Identifiers {
                            client: Some(user_id),
                            server: Some(MI_OPAQUE_SERVER_ID),
                        },
                        None,
                    ),
                )
                .map_err(|_| "client register finish failed".to_string())?;
            let password_file =
                ServerRegistration::<MiOpaqueCipherSuite>::finish(finish.message)
                    .serialize()
                    .to_vec();
            alloc_out(password_file, out_file_ptr, out_file_len)
                .map_err(|_| "bad output".to_string())?;
            Ok(())
        },
        out_err_ptr,
        out_err_len,
    )
}

#[no_mangle]
pub extern "C" fn mi_opaque_client_register_start(
    password_ptr: *const u8,
    password_len: usize,
    out_req_ptr: *mut *mut u8,
    out_req_len: *mut usize,
    out_state_ptr: *mut *mut u8,
    out_state_len: *mut usize,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    catch_and_report(
        || {
            if password_ptr.is_null() || password_len == 0 {
                return Err("password empty".to_string());
            }
            let password = unsafe { std::slice::from_raw_parts(password_ptr, password_len) };
            let mut rng = OsRng;
            let start = ClientRegistration::<MiOpaqueCipherSuite>::start(&mut rng, password)
                .map_err(|_| "client register start failed".to_string())?;

            let state = ClientRegistrationState { inner: start.state };
            let state_bytes =
                bincode::serialize(&state).map_err(|_| "state serialize failed".to_string())?;
            alloc_out(start.message.serialize().to_vec(), out_req_ptr, out_req_len)
                .map_err(|_| "bad output".to_string())?;
            alloc_out(state_bytes, out_state_ptr, out_state_len)
                .map_err(|_| "bad output".to_string())?;
            Ok(())
        },
        out_err_ptr,
        out_err_len,
    )
}

#[no_mangle]
pub extern "C" fn mi_opaque_server_register_response(
    setup_ptr: *const u8,
    setup_len: usize,
    user_id_ptr: *const u8,
    user_id_len: usize,
    req_ptr: *const u8,
    req_len: usize,
    out_resp_ptr: *mut *mut u8,
    out_resp_len: *mut usize,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    catch_and_report(
        || {
            if setup_ptr.is_null() || setup_len == 0 {
                return Err("setup empty".to_string());
            }
            if user_id_ptr.is_null() || user_id_len == 0 {
                return Err("user id empty".to_string());
            }
            if req_ptr.is_null() || req_len == 0 {
                return Err("request empty".to_string());
            }
            let setup_bytes = unsafe { std::slice::from_raw_parts(setup_ptr, setup_len) };
            let user_id = unsafe { std::slice::from_raw_parts(user_id_ptr, user_id_len) };
            let req_bytes = unsafe { std::slice::from_raw_parts(req_ptr, req_len) };
            let server_setup = ServerSetup::<MiOpaqueCipherSuite>::deserialize(setup_bytes)
                .map_err(|_| "setup invalid".to_string())?;

            let req = opaque_ke::RegistrationRequest::<MiOpaqueCipherSuite>::deserialize(req_bytes)
                .map_err(|_| "request invalid".to_string())?;
            let start = ServerRegistration::<MiOpaqueCipherSuite>::start(&server_setup, req, user_id)
                .map_err(|_| "server register start failed".to_string())?;
            alloc_out(start.message.serialize().to_vec(), out_resp_ptr, out_resp_len)
                .map_err(|_| "bad output".to_string())?;
            Ok(())
        },
        out_err_ptr,
        out_err_len,
    )
}

#[no_mangle]
pub extern "C" fn mi_opaque_client_register_finish(
    user_id_ptr: *const u8,
    user_id_len: usize,
    password_ptr: *const u8,
    password_len: usize,
    state_ptr: *const u8,
    state_len: usize,
    resp_ptr: *const u8,
    resp_len: usize,
    out_upload_ptr: *mut *mut u8,
    out_upload_len: *mut usize,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    catch_and_report(
        || {
            if user_id_ptr.is_null() || user_id_len == 0 {
                return Err("user id empty".to_string());
            }
            if password_ptr.is_null() || password_len == 0 {
                return Err("password empty".to_string());
            }
            if state_ptr.is_null() || state_len == 0 {
                return Err("state empty".to_string());
            }
            if resp_ptr.is_null() || resp_len == 0 {
                return Err("response empty".to_string());
            }
            let user_id = unsafe { std::slice::from_raw_parts(user_id_ptr, user_id_len) };
            let password = unsafe { std::slice::from_raw_parts(password_ptr, password_len) };
            let state_bytes = unsafe { std::slice::from_raw_parts(state_ptr, state_len) };
            let resp_bytes = unsafe { std::slice::from_raw_parts(resp_ptr, resp_len) };
            let mut rng = OsRng;

            let state: ClientRegistrationState =
                bincode::deserialize(state_bytes).map_err(|_| "state invalid".to_string())?;
            let resp =
                opaque_ke::RegistrationResponse::<MiOpaqueCipherSuite>::deserialize(resp_bytes)
                    .map_err(|_| "response invalid".to_string())?;
            let finish = state
                .inner
                .finish(
                    &mut rng,
                    password,
                    resp,
                    ClientRegistrationFinishParameters::new(
                        Identifiers {
                            client: Some(user_id),
                            server: Some(MI_OPAQUE_SERVER_ID),
                        },
                        None,
                    ),
                )
                .map_err(|_| "client register finish failed".to_string())?;

            alloc_out(finish.message.serialize().to_vec(), out_upload_ptr, out_upload_len)
                .map_err(|_| "bad output".to_string())?;
            Ok(())
        },
        out_err_ptr,
        out_err_len,
    )
}

#[no_mangle]
pub extern "C" fn mi_opaque_server_register_finish(
    upload_ptr: *const u8,
    upload_len: usize,
    out_file_ptr: *mut *mut u8,
    out_file_len: *mut usize,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    catch_and_report(
        || {
            if upload_ptr.is_null() || upload_len == 0 {
                return Err("upload empty".to_string());
            }
            let upload_bytes = unsafe { std::slice::from_raw_parts(upload_ptr, upload_len) };
            let upload =
                opaque_ke::RegistrationUpload::<MiOpaqueCipherSuite>::deserialize(upload_bytes)
                    .map_err(|_| "upload invalid".to_string())?;
            let password_file =
                ServerRegistration::<MiOpaqueCipherSuite>::finish(upload)
                    .serialize()
                    .to_vec();
            alloc_out(password_file, out_file_ptr, out_file_len)
                .map_err(|_| "bad output".to_string())?;
            Ok(())
        },
        out_err_ptr,
        out_err_len,
    )
}

#[no_mangle]
pub extern "C" fn mi_opaque_client_login_start(
    password_ptr: *const u8,
    password_len: usize,
    out_req_ptr: *mut *mut u8,
    out_req_len: *mut usize,
    out_state_ptr: *mut *mut u8,
    out_state_len: *mut usize,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    catch_and_report(
        || {
            if password_ptr.is_null() || password_len == 0 {
                return Err("password empty".to_string());
            }
            let password = unsafe { std::slice::from_raw_parts(password_ptr, password_len) };
            let mut rng = OsRng;
            let start = ClientLogin::<MiOpaqueCipherSuite>::start(&mut rng, password)
                .map_err(|_| "client login start failed".to_string())?;
            let state = ClientLoginState { inner: start.state };
            let state_bytes =
                bincode::serialize(&state).map_err(|_| "state serialize failed".to_string())?;
            alloc_out(start.message.serialize().to_vec(), out_req_ptr, out_req_len)
                .map_err(|_| "bad output".to_string())?;
            alloc_out(state_bytes, out_state_ptr, out_state_len)
                .map_err(|_| "bad output".to_string())?;
            Ok(())
        },
        out_err_ptr,
        out_err_len,
    )
}

#[no_mangle]
pub extern "C" fn mi_opaque_server_login_start(
    setup_ptr: *const u8,
    setup_len: usize,
    user_id_ptr: *const u8,
    user_id_len: usize,
    has_password_file: i32,
    password_file_ptr: *const u8,
    password_file_len: usize,
    req_ptr: *const u8,
    req_len: usize,
    out_resp_ptr: *mut *mut u8,
    out_resp_len: *mut usize,
    out_state_ptr: *mut *mut u8,
    out_state_len: *mut usize,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    catch_and_report(
        || {
            if setup_ptr.is_null() || setup_len == 0 {
                return Err("setup empty".to_string());
            }
            if user_id_ptr.is_null() || user_id_len == 0 {
                return Err("user id empty".to_string());
            }
            if req_ptr.is_null() || req_len == 0 {
                return Err("request empty".to_string());
            }
            let setup_bytes = unsafe { std::slice::from_raw_parts(setup_ptr, setup_len) };
            let user_id = unsafe { std::slice::from_raw_parts(user_id_ptr, user_id_len) };
            let req_bytes = unsafe { std::slice::from_raw_parts(req_ptr, req_len) };
            let server_setup = ServerSetup::<MiOpaqueCipherSuite>::deserialize(setup_bytes)
                .map_err(|_| "setup invalid".to_string())?;

            let password_file = if has_password_file != 0 {
                if password_file_ptr.is_null() || password_file_len == 0 {
                    return Err("password file empty".to_string());
                }
                let file_bytes =
                    unsafe { std::slice::from_raw_parts(password_file_ptr, password_file_len) };
                Some(
                    ServerRegistration::<MiOpaqueCipherSuite>::deserialize(file_bytes)
                        .map_err(|_| "password file invalid".to_string())?,
                )
            } else {
                None
            };

            let req = CredentialRequest::<MiOpaqueCipherSuite>::deserialize(req_bytes)
                .map_err(|_| "request invalid".to_string())?;

            let params = ServerLoginParameters {
                context: Some(MI_OPAQUE_CONTEXT),
                identifiers: Identifiers {
                    client: Some(user_id),
                    server: Some(MI_OPAQUE_SERVER_ID),
                },
            };
            let mut rng = OsRng;
            let start = ServerLogin::start(
                &mut rng,
                &server_setup,
                password_file,
                req,
                user_id,
                params,
            )
            .map_err(|_| "server login start failed".to_string())?;

            let state = ServerLoginState { inner: start.state };
            let state_bytes =
                bincode::serialize(&state).map_err(|_| "state serialize failed".to_string())?;
            alloc_out(start.message.serialize().to_vec(), out_resp_ptr, out_resp_len)
                .map_err(|_| "bad output".to_string())?;
            alloc_out(state_bytes, out_state_ptr, out_state_len)
                .map_err(|_| "bad output".to_string())?;
            Ok(())
        },
        out_err_ptr,
        out_err_len,
    )
}

#[no_mangle]
pub extern "C" fn mi_opaque_client_login_finish(
    user_id_ptr: *const u8,
    user_id_len: usize,
    password_ptr: *const u8,
    password_len: usize,
    state_ptr: *const u8,
    state_len: usize,
    resp_ptr: *const u8,
    resp_len: usize,
    out_final_ptr: *mut *mut u8,
    out_final_len: *mut usize,
    out_session_key_ptr: *mut *mut u8,
    out_session_key_len: *mut usize,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    catch_and_report(
        || {
            if user_id_ptr.is_null() || user_id_len == 0 {
                return Err("user id empty".to_string());
            }
            if password_ptr.is_null() || password_len == 0 {
                return Err("password empty".to_string());
            }
            if state_ptr.is_null() || state_len == 0 {
                return Err("state empty".to_string());
            }
            if resp_ptr.is_null() || resp_len == 0 {
                return Err("response empty".to_string());
            }
            let user_id = unsafe { std::slice::from_raw_parts(user_id_ptr, user_id_len) };
            let password = unsafe { std::slice::from_raw_parts(password_ptr, password_len) };
            let state_bytes = unsafe { std::slice::from_raw_parts(state_ptr, state_len) };
            let resp_bytes = unsafe { std::slice::from_raw_parts(resp_ptr, resp_len) };
            let mut rng = OsRng;

            let state: ClientLoginState =
                bincode::deserialize(state_bytes).map_err(|_| "state invalid".to_string())?;
            let resp = CredentialResponse::<MiOpaqueCipherSuite>::deserialize(resp_bytes)
                .map_err(|_| "response invalid".to_string())?;

            let finish = state
                .inner
                .finish(
                    &mut rng,
                    password,
                    resp,
                    ClientLoginFinishParameters::new(
                        Some(MI_OPAQUE_CONTEXT),
                        Identifiers {
                            client: Some(user_id),
                            server: Some(MI_OPAQUE_SERVER_ID),
                        },
                        None,
                    ),
                )
                .map_err(|_| "client login finish failed".to_string())?;

            alloc_out(finish.message.serialize().to_vec(), out_final_ptr, out_final_len)
                .map_err(|_| "bad output".to_string())?;
            alloc_out(finish.session_key.to_vec(), out_session_key_ptr, out_session_key_len)
                .map_err(|_| "bad output".to_string())?;
            Ok(())
        },
        out_err_ptr,
        out_err_len,
    )
}

#[no_mangle]
pub extern "C" fn mi_opaque_server_login_finish(
    user_id_ptr: *const u8,
    user_id_len: usize,
    state_ptr: *const u8,
    state_len: usize,
    final_ptr: *const u8,
    final_len: usize,
    out_session_key_ptr: *mut *mut u8,
    out_session_key_len: *mut usize,
    out_err_ptr: *mut *mut u8,
    out_err_len: *mut usize,
) -> i32 {
    catch_and_report(
        || {
            if user_id_ptr.is_null() || user_id_len == 0 {
                return Err("user id empty".to_string());
            }
            if state_ptr.is_null() || state_len == 0 {
                return Err("state empty".to_string());
            }
            if final_ptr.is_null() || final_len == 0 {
                return Err("finalization empty".to_string());
            }
            let user_id = unsafe { std::slice::from_raw_parts(user_id_ptr, user_id_len) };
            let state_bytes = unsafe { std::slice::from_raw_parts(state_ptr, state_len) };
            let final_bytes = unsafe { std::slice::from_raw_parts(final_ptr, final_len) };

            let state: ServerLoginState =
                bincode::deserialize(state_bytes).map_err(|_| "state invalid".to_string())?;
            let finalization = CredentialFinalization::<MiOpaqueCipherSuite>::deserialize(final_bytes)
                .map_err(|_| "finalization invalid".to_string())?;

            let params = ServerLoginParameters {
                context: Some(MI_OPAQUE_CONTEXT),
                identifiers: Identifiers {
                    client: Some(user_id),
                    server: Some(MI_OPAQUE_SERVER_ID),
                },
            };
            let finish = state
                .inner
                .finish(finalization, params)
                .map_err(|_| "server login finish failed".to_string())?;
            alloc_out(finish.session_key.to_vec(), out_session_key_ptr, out_session_key_len)
                .map_err(|_| "bad output".to_string())?;
            Ok(())
        },
        out_err_ptr,
        out_err_len,
    )
}
