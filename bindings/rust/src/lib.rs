use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_uint, c_void};
use std::ptr;

#[cfg(feature = "bindgen")]
pub mod generated {
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

#[repr(C)]
pub struct mi_client_handle {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mi_sdk_version {
    pub major: u32,
    pub minor: u32,
    pub patch: u32,
    pub abi: u32,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mi_event_t {
    pub type_: u32,
    pub ts_ms: u64,
    pub peer: *const c_char,
    pub sender: *const c_char,
    pub group_id: *const c_char,
    pub message_id: *const c_char,
    pub text: *const c_char,
    pub file_id: *const c_char,
    pub file_name: *const c_char,
    pub file_size: u64,
    pub file_key: *const u8,
    pub file_key_len: u32,
    pub sticker_id: *const c_char,
    pub notice_kind: u32,
    pub actor: *const c_char,
    pub target: *const c_char,
    pub role: u32,
    pub typing: u8,
    pub online: u8,
    pub reserved0: u8,
    pub reserved1: u8,
    pub call_id: [u8; 16],
    pub call_key_id: u32,
    pub call_op: u32,
    pub call_media_flags: u8,
    pub call_reserved0: u8,
    pub call_reserved1: u8,
    pub call_reserved2: u8,
    pub payload: *const u8,
    pub payload_len: u32,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mi_friend_entry_t {
    pub username: *const c_char,
    pub remark: *const c_char,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mi_friend_request_entry_t {
    pub requester_username: *const c_char,
    pub requester_remark: *const c_char,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mi_device_entry_t {
    pub device_id: *const c_char,
    pub last_seen_sec: u32,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mi_device_pairing_request_t {
    pub device_id: *const c_char,
    pub request_id_hex: *const c_char,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mi_group_member_entry_t {
    pub username: *const c_char,
    pub role: u32,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mi_group_call_member_t {
    pub username: *const c_char,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mi_media_packet_t {
    pub sender: *const c_char,
    pub payload: *const u8,
    pub payload_len: u32,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mi_media_config_t {
    pub audio_delay_ms: u32,
    pub video_delay_ms: u32,
    pub audio_max_frames: u32,
    pub video_max_frames: u32,
    pub pull_max_packets: u32,
    pub pull_wait_ms: u32,
    pub group_pull_max_packets: u32,
    pub group_pull_wait_ms: u32,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct mi_history_entry_t {
    pub kind: u32,
    pub status: u32,
    pub is_group: u8,
    pub outgoing: u8,
    pub reserved0: u8,
    pub reserved1: u8,
    pub timestamp_sec: u64,
    pub conv_id: *const c_char,
    pub sender: *const c_char,
    pub message_id: *const c_char,
    pub text: *const c_char,
    pub file_id: *const c_char,
    pub file_key: *const u8,
    pub file_key_len: u32,
    pub file_name: *const c_char,
    pub file_size: u64,
    pub sticker_id: *const c_char,
}

const MI_E2EE_SDK_ABI_VERSION: u32 = 1;

type MiProgressCallback = Option<extern "C" fn(u64, u64, *mut c_void)>;
impl Default for mi_event_t {
    fn default() -> Self {
        unsafe { std::mem::zeroed() }
    }
}

impl Default for mi_media_packet_t {
    fn default() -> Self {
        unsafe { std::mem::zeroed() }
    }
}

impl Default for mi_group_call_member_t {
    fn default() -> Self {
        unsafe { std::mem::zeroed() }
    }
}

impl Default for mi_group_member_entry_t {
    fn default() -> Self {
        unsafe { std::mem::zeroed() }
    }
}

impl Default for mi_device_entry_t {
    fn default() -> Self {
        unsafe { std::mem::zeroed() }
    }
}

impl Default for mi_device_pairing_request_t {
    fn default() -> Self {
        unsafe { std::mem::zeroed() }
    }
}

impl Default for mi_history_entry_t {
    fn default() -> Self {
        unsafe { std::mem::zeroed() }
    }
}

impl Default for mi_media_config_t {
    fn default() -> Self {
        unsafe { std::mem::zeroed() }
    }
}

extern "C" {
    fn mi_client_get_version(out_version: *mut mi_sdk_version);
    fn mi_client_get_capabilities() -> c_uint;
    fn mi_client_create(config_path: *const c_char) -> *mut mi_client_handle;
    fn mi_client_last_create_error() -> *const c_char;
    fn mi_client_destroy(handle: *mut mi_client_handle);
    fn mi_client_last_error(handle: *mut mi_client_handle) -> *const c_char;
    fn mi_client_token(handle: *mut mi_client_handle) -> *const c_char;
    fn mi_client_device_id(handle: *mut mi_client_handle) -> *const c_char;
    fn mi_client_remote_ok(handle: *mut mi_client_handle) -> c_int;
    fn mi_client_remote_error(handle: *mut mi_client_handle) -> *const c_char;
    fn mi_client_is_remote_mode(handle: *mut mi_client_handle) -> c_int;
    fn mi_client_relogin(handle: *mut mi_client_handle) -> c_int;

    fn mi_client_has_pending_server_trust(handle: *mut mi_client_handle) -> c_int;
    fn mi_client_pending_server_fingerprint(handle: *mut mi_client_handle) -> *const c_char;
    fn mi_client_pending_server_pin(handle: *mut mi_client_handle) -> *const c_char;
    fn mi_client_trust_pending_server(handle: *mut mi_client_handle, pin: *const c_char) -> c_int;

    fn mi_client_has_pending_peer_trust(handle: *mut mi_client_handle) -> c_int;
    fn mi_client_pending_peer_username(handle: *mut mi_client_handle) -> *const c_char;
    fn mi_client_pending_peer_fingerprint(handle: *mut mi_client_handle) -> *const c_char;
    fn mi_client_pending_peer_pin(handle: *mut mi_client_handle) -> *const c_char;
    fn mi_client_trust_pending_peer(handle: *mut mi_client_handle, pin: *const c_char) -> c_int;

    fn mi_client_register(handle: *mut mi_client_handle,
                          username: *const c_char,
                          password: *const c_char) -> c_int;
    fn mi_client_login(handle: *mut mi_client_handle,
                       username: *const c_char,
                       password: *const c_char) -> c_int;
    fn mi_client_logout(handle: *mut mi_client_handle) -> c_int;
    fn mi_client_heartbeat(handle: *mut mi_client_handle) -> c_int;

    fn mi_client_send_private_text(handle: *mut mi_client_handle,
                                   peer_username: *const c_char,
                                   text_utf8: *const c_char,
                                   out_message_id_hex: *mut *mut c_char) -> c_int;
    fn mi_client_send_private_text_with_reply(handle: *mut mi_client_handle,
                                              peer_username: *const c_char,
                                              text_utf8: *const c_char,
                                              reply_to_message_id_hex: *const c_char,
                                              reply_preview_utf8: *const c_char,
                                              out_message_id_hex: *mut *mut c_char) -> c_int;
    fn mi_client_resend_private_text(handle: *mut mi_client_handle,
                                     peer_username: *const c_char,
                                     message_id_hex: *const c_char,
                                     text_utf8: *const c_char) -> c_int;
    fn mi_client_resend_private_text_with_reply(handle: *mut mi_client_handle,
                                                peer_username: *const c_char,
                                                message_id_hex: *const c_char,
                                                text_utf8: *const c_char,
                                                reply_to_message_id_hex: *const c_char,
                                                reply_preview_utf8: *const c_char) -> c_int;
    fn mi_client_send_group_text(handle: *mut mi_client_handle,
                                 group_id: *const c_char,
                                 text_utf8: *const c_char,
                                 out_message_id_hex: *mut *mut c_char) -> c_int;
    fn mi_client_resend_group_text(handle: *mut mi_client_handle,
                                   group_id: *const c_char,
                                   message_id_hex: *const c_char,
                                   text_utf8: *const c_char) -> c_int;
    fn mi_client_send_private_file(handle: *mut mi_client_handle,
                                   peer_username: *const c_char,
                                   file_path_utf8: *const c_char,
                                   out_message_id_hex: *mut *mut c_char) -> c_int;
    fn mi_client_resend_private_file(handle: *mut mi_client_handle,
                                     peer_username: *const c_char,
                                     message_id_hex: *const c_char,
                                     file_path_utf8: *const c_char) -> c_int;
    fn mi_client_send_group_file(handle: *mut mi_client_handle,
                                 group_id: *const c_char,
                                 file_path_utf8: *const c_char,
                                 out_message_id_hex: *mut *mut c_char) -> c_int;
    fn mi_client_resend_group_file(handle: *mut mi_client_handle,
                                   group_id: *const c_char,
                                   message_id_hex: *const c_char,
                                   file_path_utf8: *const c_char) -> c_int;
    fn mi_client_send_private_sticker(handle: *mut mi_client_handle,
                                      peer_username: *const c_char,
                                      sticker_id: *const c_char,
                                      out_message_id_hex: *mut *mut c_char) -> c_int;
    fn mi_client_resend_private_sticker(handle: *mut mi_client_handle,
                                        peer_username: *const c_char,
                                        message_id_hex: *const c_char,
                                        sticker_id: *const c_char) -> c_int;
    fn mi_client_send_private_location(handle: *mut mi_client_handle,
                                       peer_username: *const c_char,
                                       lat_e7: i32,
                                       lon_e7: i32,
                                       label_utf8: *const c_char,
                                       out_message_id_hex: *mut *mut c_char) -> c_int;
    fn mi_client_resend_private_location(handle: *mut mi_client_handle,
                                         peer_username: *const c_char,
                                         message_id_hex: *const c_char,
                                         lat_e7: i32,
                                         lon_e7: i32,
                                         label_utf8: *const c_char) -> c_int;
    fn mi_client_send_private_contact(handle: *mut mi_client_handle,
                                      peer_username: *const c_char,
                                      card_username: *const c_char,
                                      card_display: *const c_char,
                                      out_message_id_hex: *mut *mut c_char) -> c_int;
    fn mi_client_resend_private_contact(handle: *mut mi_client_handle,
                                        peer_username: *const c_char,
                                        message_id_hex: *const c_char,
                                        card_username: *const c_char,
                                        card_display: *const c_char) -> c_int;
    fn mi_client_send_read_receipt(handle: *mut mi_client_handle,
                                   peer_username: *const c_char,
                                   message_id_hex: *const c_char) -> c_int;
    fn mi_client_send_typing(handle: *mut mi_client_handle,
                             peer_username: *const c_char,
                             typing: c_int) -> c_int;
    fn mi_client_send_presence(handle: *mut mi_client_handle,
                               peer_username: *const c_char,
                               online: c_int) -> c_int;
    fn mi_client_add_friend(handle: *mut mi_client_handle,
                            friend_username: *const c_char,
                            remark: *const c_char) -> c_int;
    fn mi_client_set_friend_remark(handle: *mut mi_client_handle,
                                   friend_username: *const c_char,
                                   remark: *const c_char) -> c_int;
    fn mi_client_delete_friend(handle: *mut mi_client_handle,
                               friend_username: *const c_char) -> c_int;
    fn mi_client_set_user_blocked(handle: *mut mi_client_handle,
                                  blocked_username: *const c_char,
                                  blocked: c_int) -> c_int;
    fn mi_client_send_friend_request(handle: *mut mi_client_handle,
                                     target_username: *const c_char,
                                     remark: *const c_char) -> c_int;
    fn mi_client_respond_friend_request(handle: *mut mi_client_handle,
                                        requester_username: *const c_char,
                                        accept: c_int) -> c_int;
    fn mi_client_list_friends(handle: *mut mi_client_handle,
                              out_entries: *mut mi_friend_entry_t,
                              max_entries: c_uint) -> c_uint;
    fn mi_client_sync_friends(handle: *mut mi_client_handle,
                              out_entries: *mut mi_friend_entry_t,
                              max_entries: c_uint,
                              out_changed: *mut c_int) -> c_uint;
    fn mi_client_list_friend_requests(handle: *mut mi_client_handle,
                                      out_entries: *mut mi_friend_request_entry_t,
                                      max_entries: c_uint) -> c_uint;
    fn mi_client_list_devices(handle: *mut mi_client_handle,
                              out_entries: *mut mi_device_entry_t,
                              max_entries: c_uint) -> c_uint;
    fn mi_client_kick_device(handle: *mut mi_client_handle,
                             device_id: *const c_char) -> c_int;

    fn mi_client_join_group(handle: *mut mi_client_handle,
                            group_id: *const c_char) -> c_int;
    fn mi_client_leave_group(handle: *mut mi_client_handle,
                             group_id: *const c_char) -> c_int;
    fn mi_client_create_group(handle: *mut mi_client_handle,
                              out_group_id: *mut *mut c_char) -> c_int;
    fn mi_client_send_group_invite(handle: *mut mi_client_handle,
                                   group_id: *const c_char,
                                   peer_username: *const c_char,
                                   out_message_id_hex: *mut *mut c_char) -> c_int;
    fn mi_client_list_group_members_info(handle: *mut mi_client_handle,
                                         group_id: *const c_char,
                                         out_entries: *mut mi_group_member_entry_t,
                                         max_entries: c_uint) -> c_uint;
    fn mi_client_set_group_member_role(handle: *mut mi_client_handle,
                                       group_id: *const c_char,
                                       peer_username: *const c_char,
                                       role: c_uint) -> c_int;
    fn mi_client_kick_group_member(handle: *mut mi_client_handle,
                                   group_id: *const c_char,
                                   peer_username: *const c_char) -> c_int;
    fn mi_client_start_group_call(handle: *mut mi_client_handle,
                                  group_id: *const c_char,
                                  video: c_int,
                                  out_call_id: *mut u8,
                                  out_call_id_len: c_uint,
                                  out_key_id: *mut c_uint) -> c_int;
    fn mi_client_join_group_call(handle: *mut mi_client_handle,
                                 group_id: *const c_char,
                                 call_id: *const u8,
                                 call_id_len: c_uint,
                                 video: c_int,
                                 out_key_id: *mut c_uint) -> c_int;
    fn mi_client_leave_group_call(handle: *mut mi_client_handle,
                                  group_id: *const c_char,
                                  call_id: *const u8,
                                  call_id_len: c_uint) -> c_int;
    fn mi_client_get_group_call_key(handle: *mut mi_client_handle,
                                    group_id: *const c_char,
                                    call_id: *const u8,
                                    call_id_len: c_uint,
                                    key_id: c_uint,
                                    out_key: *mut u8,
                                    out_key_len: c_uint) -> c_int;
    fn mi_client_rotate_group_call_key(handle: *mut mi_client_handle,
                                       group_id: *const c_char,
                                       call_id: *const u8,
                                       call_id_len: c_uint,
                                       key_id: c_uint,
                                       members: *const *const c_char,
                                       member_count: c_uint) -> c_int;
    fn mi_client_request_group_call_key(handle: *mut mi_client_handle,
                                        group_id: *const c_char,
                                        call_id: *const u8,
                                        call_id_len: c_uint,
                                        key_id: c_uint,
                                        members: *const *const c_char,
                                        member_count: c_uint) -> c_int;
    fn mi_client_send_group_call_signal(handle: *mut mi_client_handle,
                                        op: u8,
                                        group_id: *const c_char,
                                        call_id: *const u8,
                                        call_id_len: c_uint,
                                        video: c_int,
                                        key_id: c_uint,
                                        seq: c_uint,
                                        ts_ms: u64,
                                        ext: *const u8,
                                        ext_len: c_uint,
                                        out_call_id: *mut u8,
                                        out_call_id_len: c_uint,
                                        out_key_id: *mut c_uint,
                                        out_members: *mut mi_group_call_member_t,
                                        max_members: c_uint,
                                        out_member_count: *mut c_uint) -> c_int;

    fn mi_client_load_chat_history(handle: *mut mi_client_handle,
                                   conv_id: *const c_char,
                                   is_group: c_int,
                                   limit: c_uint,
                                   out_entries: *mut mi_history_entry_t,
                                   max_entries: c_uint) -> c_uint;
    fn mi_client_delete_chat_history(handle: *mut mi_client_handle,
                                     conv_id: *const c_char,
                                     is_group: c_int,
                                     delete_attachments: c_int,
                                     secure_wipe: c_int) -> c_int;
    fn mi_client_set_history_enabled(handle: *mut mi_client_handle,
                                     enabled: c_int) -> c_int;
    fn mi_client_clear_all_history(handle: *mut mi_client_handle,
                                   delete_attachments: c_int,
                                   secure_wipe: c_int) -> c_int;
    fn mi_client_begin_device_pairing_primary(handle: *mut mi_client_handle,
                                              out_pairing_code: *mut *mut c_char) -> c_int;
    fn mi_client_poll_device_pairing_requests(handle: *mut mi_client_handle,
                                              out_entries: *mut mi_device_pairing_request_t,
                                              max_entries: c_uint) -> c_uint;
    fn mi_client_approve_device_pairing_request(handle: *mut mi_client_handle,
                                                device_id: *const c_char,
                                                request_id_hex: *const c_char) -> c_int;
    fn mi_client_begin_device_pairing_linked(handle: *mut mi_client_handle,
                                             pairing_code: *const c_char) -> c_int;
    fn mi_client_poll_device_pairing_linked(handle: *mut mi_client_handle,
                                            out_completed: *mut c_int) -> c_int;
    fn mi_client_cancel_device_pairing(handle: *mut mi_client_handle);

    fn mi_client_store_attachment_preview_bytes(handle: *mut mi_client_handle,
                                                file_id: *const c_char,
                                                file_name: *const c_char,
                                                file_size: u64,
                                                bytes: *const u8,
                                                bytes_len: c_uint) -> c_int;
    fn mi_client_download_chat_file_to_path(handle: *mut mi_client_handle,
                                            file_id: *const c_char,
                                            file_key: *const u8,
                                            file_key_len: c_uint,
                                            file_name: *const c_char,
                                            file_size: u64,
                                            out_path_utf8: *const c_char,
                                            wipe_after_read: c_int,
                                            on_progress: MiProgressCallback,
                                            user_data: *mut c_void) -> c_int;
    fn mi_client_download_chat_file_to_bytes(handle: *mut mi_client_handle,
                                             file_id: *const c_char,
                                             file_key: *const u8,
                                             file_key_len: c_uint,
                                             file_name: *const c_char,
                                             file_size: u64,
                                             wipe_after_read: c_int,
                                             out_bytes: *mut *mut u8,
                                             out_len: *mut u64) -> c_int;

    fn mi_client_get_media_config(handle: *mut mi_client_handle,
                                  out_config: *mut mi_media_config_t) -> c_int;
    fn mi_client_derive_media_root(handle: *mut mi_client_handle,
                                   peer_username: *const c_char,
                                   call_id: *const u8,
                                   call_id_len: c_uint,
                                   out_media_root: *mut u8,
                                   out_media_root_len: c_uint) -> c_int;
    fn mi_client_push_media(handle: *mut mi_client_handle,
                            peer_username: *const c_char,
                            call_id: *const u8,
                            call_id_len: c_uint,
                            packet: *const u8,
                            packet_len: c_uint) -> c_int;
    fn mi_client_pull_media(handle: *mut mi_client_handle,
                            call_id: *const u8,
                            call_id_len: c_uint,
                            max_packets: c_uint,
                            wait_ms: c_uint,
                            out_packets: *mut mi_media_packet_t) -> c_uint;
    fn mi_client_push_group_media(handle: *mut mi_client_handle,
                                  group_id: *const c_char,
                                  call_id: *const u8,
                                  call_id_len: c_uint,
                                  packet: *const u8,
                                  packet_len: c_uint) -> c_int;
    fn mi_client_pull_group_media(handle: *mut mi_client_handle,
                                  call_id: *const u8,
                                  call_id_len: c_uint,
                                  max_packets: c_uint,
                                  wait_ms: c_uint,
                                  out_packets: *mut mi_media_packet_t) -> c_uint;
    fn mi_client_add_media_subscription(handle: *mut mi_client_handle,
                                        call_id: *const u8,
                                        call_id_len: c_uint,
                                        is_group: c_int,
                                        group_id: *const c_char) -> c_int;
    fn mi_client_clear_media_subscriptions(handle: *mut mi_client_handle);

    fn mi_client_poll_event(handle: *mut mi_client_handle,
                            out_events: *mut mi_event_t,
                            max_events: c_uint,
                            wait_ms: c_uint) -> c_uint;
    fn mi_client_free(buf: *mut c_void);
}
#[derive(Debug, Clone)]
pub struct Event {
    pub event_type: u32,
    pub ts_ms: u64,
    pub peer: Option<String>,
    pub sender: Option<String>,
    pub group_id: Option<String>,
    pub message_id: Option<String>,
    pub text: Option<String>,
    pub file_id: Option<String>,
    pub file_name: Option<String>,
    pub file_size: u64,
    pub file_key: Vec<u8>,
    pub sticker_id: Option<String>,
    pub notice_kind: u32,
    pub actor: Option<String>,
    pub target: Option<String>,
    pub role: u32,
    pub typing: bool,
    pub online: bool,
    pub call_id: [u8; 16],
    pub call_key_id: u32,
    pub call_op: u32,
    pub call_media_flags: u8,
    pub payload: Vec<u8>,
}

#[derive(Debug, Clone)]
pub struct FriendEntry {
    pub username: String,
    pub remark: String,
}

#[derive(Debug, Clone)]
pub struct FriendRequestEntry {
    pub requester_username: String,
    pub requester_remark: String,
}

#[derive(Debug, Clone)]
pub struct DeviceEntry {
    pub device_id: String,
    pub last_seen_sec: u32,
}

#[derive(Debug, Clone)]
pub struct DevicePairingRequest {
    pub device_id: String,
    pub request_id_hex: String,
}

#[derive(Debug, Clone)]
pub struct GroupMemberEntry {
    pub username: String,
    pub role: u32,
}

#[derive(Debug, Clone)]
pub struct GroupCallMember {
    pub username: String,
}

#[derive(Debug, Clone)]
pub struct MediaPacket {
    pub sender: String,
    pub payload: Vec<u8>,
}

#[derive(Debug, Clone, Copy)]
pub struct MediaConfig {
    pub audio_delay_ms: u32,
    pub video_delay_ms: u32,
    pub audio_max_frames: u32,
    pub video_max_frames: u32,
    pub pull_max_packets: u32,
    pub pull_wait_ms: u32,
    pub group_pull_max_packets: u32,
    pub group_pull_wait_ms: u32,
}

#[derive(Debug, Clone)]
pub struct HistoryEntry {
    pub kind: u32,
    pub status: u32,
    pub is_group: bool,
    pub outgoing: bool,
    pub timestamp_sec: u64,
    pub conv_id: Option<String>,
    pub sender: Option<String>,
    pub message_id: Option<String>,
    pub text: Option<String>,
    pub file_id: Option<String>,
    pub file_key: Vec<u8>,
    pub file_name: Option<String>,
    pub file_size: u64,
    pub sticker_id: Option<String>,
}

#[derive(Debug, Clone)]
pub struct GroupCallStart {
    pub call_id: [u8; 16],
    pub key_id: u32,
}

#[derive(Debug, Clone)]
pub struct GroupCallSignalResponse {
    pub call_id: [u8; 16],
    pub key_id: u32,
    pub members: Vec<GroupCallMember>,
    pub member_count: u32,
}

fn opt_string(ptr: *const c_char) -> Option<String> {
    if ptr.is_null() {
        return None;
    }
    unsafe { Some(CStr::from_ptr(ptr).to_string_lossy().into_owned()) }
}

fn copy_bytes(ptr: *const u8, len: u32) -> Vec<u8> {
    if ptr.is_null() || len == 0 {
        return Vec::new();
    }
    unsafe { std::slice::from_raw_parts(ptr, len as usize).to_vec() }
}

fn copy_bytes_u64(ptr: *const u8, len: u64) -> Vec<u8> {
    if ptr.is_null() || len == 0 {
        return Vec::new();
    }
    if len > (usize::MAX as u64) {
        return Vec::new();
    }
    unsafe { std::slice::from_raw_parts(ptr, len as usize).to_vec() }
}

fn take_c_string(ptr: *mut c_char) -> String {
    if ptr.is_null() {
        return String::new();
    }
    let s = unsafe { CStr::from_ptr(ptr).to_string_lossy().into_owned() };
    unsafe { mi_client_free(ptr as *mut c_void) };
    s
}

fn friend_from_c(entry: &mi_friend_entry_t) -> FriendEntry {
    FriendEntry {
        username: opt_string(entry.username).unwrap_or_default(),
        remark: opt_string(entry.remark).unwrap_or_default(),
    }
}

fn friend_request_from_c(entry: &mi_friend_request_entry_t) -> FriendRequestEntry {
    FriendRequestEntry {
        requester_username: opt_string(entry.requester_username).unwrap_or_default(),
        requester_remark: opt_string(entry.requester_remark).unwrap_or_default(),
    }
}

fn device_from_c(entry: &mi_device_entry_t) -> DeviceEntry {
    DeviceEntry {
        device_id: opt_string(entry.device_id).unwrap_or_default(),
        last_seen_sec: entry.last_seen_sec,
    }
}

fn pairing_request_from_c(entry: &mi_device_pairing_request_t) -> DevicePairingRequest {
    DevicePairingRequest {
        device_id: opt_string(entry.device_id).unwrap_or_default(),
        request_id_hex: opt_string(entry.request_id_hex).unwrap_or_default(),
    }
}

fn group_member_from_c(entry: &mi_group_member_entry_t) -> GroupMemberEntry {
    GroupMemberEntry {
        username: opt_string(entry.username).unwrap_or_default(),
        role: entry.role,
    }
}

fn group_call_member_from_c(entry: &mi_group_call_member_t) -> GroupCallMember {
    GroupCallMember {
        username: opt_string(entry.username).unwrap_or_default(),
    }
}

fn media_packet_from_c(entry: &mi_media_packet_t) -> MediaPacket {
    MediaPacket {
        sender: opt_string(entry.sender).unwrap_or_default(),
        payload: copy_bytes(entry.payload, entry.payload_len),
    }
}

fn history_from_c(entry: &mi_history_entry_t) -> HistoryEntry {
    HistoryEntry {
        kind: entry.kind,
        status: entry.status,
        is_group: entry.is_group != 0,
        outgoing: entry.outgoing != 0,
        timestamp_sec: entry.timestamp_sec,
        conv_id: opt_string(entry.conv_id),
        sender: opt_string(entry.sender),
        message_id: opt_string(entry.message_id),
        text: opt_string(entry.text),
        file_id: opt_string(entry.file_id),
        file_key: copy_bytes(entry.file_key, entry.file_key_len),
        file_name: opt_string(entry.file_name),
        file_size: entry.file_size,
        sticker_id: opt_string(entry.sticker_id),
    }
}

pub struct Client {
    handle: *mut mi_client_handle,
}

struct ProgressHolder {
    cb: Box<dyn FnMut(u64, u64) + 'static>,
}

extern "C" fn progress_trampoline(done: u64, total: u64, user_data: *mut c_void) {
    if user_data.is_null() {
        return;
    }
    let holder = unsafe { &mut *(user_data as *mut ProgressHolder) };
    (holder.cb)(done, total);
}
impl Client {
    pub fn version() -> mi_sdk_version {
        let mut v = mi_sdk_version {
            major: 0,
            minor: 0,
            patch: 0,
            abi: 0,
        };
        unsafe { mi_client_get_version(&mut v) };
        v
    }

    fn ensure_abi() -> Result<(), String> {
        let v = Self::version();
        if v.abi != MI_E2EE_SDK_ABI_VERSION {
            return Err(format!(
                "mi_e2ee sdk abi mismatch: {} != {}",
                v.abi, MI_E2EE_SDK_ABI_VERSION
            ));
        }
        Ok(())
    }

    pub fn check_abi() -> Result<(), String> {
        Self::ensure_abi()
    }

    pub fn capabilities() -> u32 {
        unsafe { mi_client_get_capabilities() as u32 }
    }

    pub fn last_create_error() -> String {
        let ptr = unsafe { mi_client_last_create_error() };
        opt_string(ptr).unwrap_or_default()
    }

    pub fn new(config_path: Option<&str>) -> Result<Self, String> {
        Self::ensure_abi()?;
        let c_path = match config_path {
            Some(path) => Some(CString::new(path).map_err(|_| "config path contains null".to_string())?),
            None => None,
        };
        let handle = unsafe {
            mi_client_create(match &c_path {
                Some(path) => path.as_ptr(),
                None => ptr::null(),
            })
        };
        if handle.is_null() {
            return Err(Self::last_create_error());
        }
        Ok(Client { handle })
    }

    pub fn last_error(&self) -> String {
        if self.handle.is_null() {
            return String::new();
        }
        let ptr = unsafe { mi_client_last_error(self.handle) };
        opt_string(ptr).unwrap_or_default()
    }

    pub fn token(&self) -> String {
        if self.handle.is_null() {
            return String::new();
        }
        let ptr = unsafe { mi_client_token(self.handle) };
        opt_string(ptr).unwrap_or_default()
    }

    pub fn device_id(&self) -> String {
        if self.handle.is_null() {
            return String::new();
        }
        let ptr = unsafe { mi_client_device_id(self.handle) };
        opt_string(ptr).unwrap_or_default()
    }

    pub fn remote_ok(&self) -> bool {
        if self.handle.is_null() {
            return false;
        }
        unsafe { mi_client_remote_ok(self.handle) != 0 }
    }

    pub fn remote_error(&self) -> String {
        if self.handle.is_null() {
            return String::new();
        }
        let ptr = unsafe { mi_client_remote_error(self.handle) };
        opt_string(ptr).unwrap_or_default()
    }

    pub fn is_remote_mode(&self) -> bool {
        if self.handle.is_null() {
            return false;
        }
        unsafe { mi_client_is_remote_mode(self.handle) != 0 }
    }

    pub fn relogin(&self) -> Result<(), String> {
        let ok = unsafe { mi_client_relogin(self.handle) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn has_pending_server_trust(&self) -> bool {
        if self.handle.is_null() {
            return false;
        }
        unsafe { mi_client_has_pending_server_trust(self.handle) != 0 }
    }

    pub fn pending_server_fingerprint(&self) -> String {
        if self.handle.is_null() {
            return String::new();
        }
        let ptr = unsafe { mi_client_pending_server_fingerprint(self.handle) };
        opt_string(ptr).unwrap_or_default()
    }

    pub fn pending_server_pin(&self) -> String {
        if self.handle.is_null() {
            return String::new();
        }
        let ptr = unsafe { mi_client_pending_server_pin(self.handle) };
        opt_string(ptr).unwrap_or_default()
    }

    pub fn trust_pending_server(&self, pin: &str) -> Result<(), String> {
        let p = CString::new(pin).map_err(|_| "pin contains null".to_string())?;
        let ok = unsafe { mi_client_trust_pending_server(self.handle, p.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn has_pending_peer_trust(&self) -> bool {
        if self.handle.is_null() {
            return false;
        }
        unsafe { mi_client_has_pending_peer_trust(self.handle) != 0 }
    }

    pub fn pending_peer_username(&self) -> String {
        if self.handle.is_null() {
            return String::new();
        }
        let ptr = unsafe { mi_client_pending_peer_username(self.handle) };
        opt_string(ptr).unwrap_or_default()
    }

    pub fn pending_peer_fingerprint(&self) -> String {
        if self.handle.is_null() {
            return String::new();
        }
        let ptr = unsafe { mi_client_pending_peer_fingerprint(self.handle) };
        opt_string(ptr).unwrap_or_default()
    }

    pub fn pending_peer_pin(&self) -> String {
        if self.handle.is_null() {
            return String::new();
        }
        let ptr = unsafe { mi_client_pending_peer_pin(self.handle) };
        opt_string(ptr).unwrap_or_default()
    }

    pub fn trust_pending_peer(&self, pin: &str) -> Result<(), String> {
        let p = CString::new(pin).map_err(|_| "pin contains null".to_string())?;
        let ok = unsafe { mi_client_trust_pending_peer(self.handle, p.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }
    pub fn register(&self, username: &str, password: &str) -> Result<(), String> {
        let u = CString::new(username).map_err(|_| "username contains null".to_string())?;
        let p = CString::new(password).map_err(|_| "password contains null".to_string())?;
        let ok = unsafe { mi_client_register(self.handle, u.as_ptr(), p.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn login(&self, username: &str, password: &str) -> Result<(), String> {
        let u = CString::new(username).map_err(|_| "username contains null".to_string())?;
        let p = CString::new(password).map_err(|_| "password contains null".to_string())?;
        let ok = unsafe { mi_client_login(self.handle, u.as_ptr(), p.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn logout(&self) -> Result<(), String> {
        let ok = unsafe { mi_client_logout(self.handle) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn heartbeat(&self) -> Result<(), String> {
        let ok = unsafe { mi_client_heartbeat(self.handle) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_private_text(&self, peer: &str, text: &str) -> Result<String, String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let text_c = CString::new(text).map_err(|_| "text contains null".to_string())?;
        let mut out: *mut c_char = ptr::null_mut();
        let ok = unsafe { mi_client_send_private_text(self.handle, peer_c.as_ptr(), text_c.as_ptr(), &mut out) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(take_c_string(out))
    }

    pub fn send_private_text_with_reply(&self,
                                        peer: &str,
                                        text: &str,
                                        reply_to_message_id: &str,
                                        reply_preview: &str) -> Result<String, String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let text_c = CString::new(text).map_err(|_| "text contains null".to_string())?;
        let reply_id = CString::new(reply_to_message_id)
            .map_err(|_| "reply_to_message_id contains null".to_string())?;
        let reply_preview = CString::new(reply_preview)
            .map_err(|_| "reply_preview contains null".to_string())?;
        let mut out: *mut c_char = ptr::null_mut();
        let ok = unsafe {
            mi_client_send_private_text_with_reply(
                self.handle,
                peer_c.as_ptr(),
                text_c.as_ptr(),
                reply_id.as_ptr(),
                reply_preview.as_ptr(),
                &mut out,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(take_c_string(out))
    }

    pub fn resend_private_text(&self, peer: &str, message_id: &str, text: &str) -> Result<(), String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let msg_c = CString::new(message_id).map_err(|_| "message id contains null".to_string())?;
        let text_c = CString::new(text).map_err(|_| "text contains null".to_string())?;
        let ok = unsafe { mi_client_resend_private_text(self.handle, peer_c.as_ptr(), msg_c.as_ptr(), text_c.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn resend_private_text_with_reply(&self,
                                          peer: &str,
                                          message_id: &str,
                                          text: &str,
                                          reply_to_message_id: &str,
                                          reply_preview: &str) -> Result<(), String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let msg_c = CString::new(message_id).map_err(|_| "message id contains null".to_string())?;
        let text_c = CString::new(text).map_err(|_| "text contains null".to_string())?;
        let reply_id = CString::new(reply_to_message_id)
            .map_err(|_| "reply_to_message_id contains null".to_string())?;
        let reply_preview = CString::new(reply_preview)
            .map_err(|_| "reply_preview contains null".to_string())?;
        let ok = unsafe {
            mi_client_resend_private_text_with_reply(
                self.handle,
                peer_c.as_ptr(),
                msg_c.as_ptr(),
                text_c.as_ptr(),
                reply_id.as_ptr(),
                reply_preview.as_ptr(),
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_group_text(&self, group_id: &str, text: &str) -> Result<String, String> {
        let group_c = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let text_c = CString::new(text).map_err(|_| "text contains null".to_string())?;
        let mut out: *mut c_char = ptr::null_mut();
        let ok = unsafe { mi_client_send_group_text(self.handle, group_c.as_ptr(), text_c.as_ptr(), &mut out) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(take_c_string(out))
    }

    pub fn resend_group_text(&self, group_id: &str, message_id: &str, text: &str) -> Result<(), String> {
        let group_c = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let msg_c = CString::new(message_id).map_err(|_| "message id contains null".to_string())?;
        let text_c = CString::new(text).map_err(|_| "text contains null".to_string())?;
        let ok = unsafe { mi_client_resend_group_text(self.handle, group_c.as_ptr(), msg_c.as_ptr(), text_c.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_private_file(&self, peer: &str, file_path: &str) -> Result<String, String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let path_c = CString::new(file_path).map_err(|_| "file path contains null".to_string())?;
        let mut out: *mut c_char = ptr::null_mut();
        let ok = unsafe { mi_client_send_private_file(self.handle, peer_c.as_ptr(), path_c.as_ptr(), &mut out) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(take_c_string(out))
    }

    pub fn resend_private_file(&self, peer: &str, message_id: &str, file_path: &str) -> Result<(), String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let msg_c = CString::new(message_id).map_err(|_| "message id contains null".to_string())?;
        let path_c = CString::new(file_path).map_err(|_| "file path contains null".to_string())?;
        let ok = unsafe { mi_client_resend_private_file(self.handle, peer_c.as_ptr(), msg_c.as_ptr(), path_c.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_group_file(&self, group_id: &str, file_path: &str) -> Result<String, String> {
        let group_c = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let path_c = CString::new(file_path).map_err(|_| "file path contains null".to_string())?;
        let mut out: *mut c_char = ptr::null_mut();
        let ok = unsafe { mi_client_send_group_file(self.handle, group_c.as_ptr(), path_c.as_ptr(), &mut out) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(take_c_string(out))
    }

    pub fn resend_group_file(&self, group_id: &str, message_id: &str, file_path: &str) -> Result<(), String> {
        let group_c = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let msg_c = CString::new(message_id).map_err(|_| "message id contains null".to_string())?;
        let path_c = CString::new(file_path).map_err(|_| "file path contains null".to_string())?;
        let ok = unsafe { mi_client_resend_group_file(self.handle, group_c.as_ptr(), msg_c.as_ptr(), path_c.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_private_sticker(&self, peer: &str, sticker_id: &str) -> Result<String, String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let sticker_c = CString::new(sticker_id).map_err(|_| "sticker id contains null".to_string())?;
        let mut out: *mut c_char = ptr::null_mut();
        let ok = unsafe { mi_client_send_private_sticker(self.handle, peer_c.as_ptr(), sticker_c.as_ptr(), &mut out) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(take_c_string(out))
    }

    pub fn resend_private_sticker(&self, peer: &str, message_id: &str, sticker_id: &str) -> Result<(), String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let msg_c = CString::new(message_id).map_err(|_| "message id contains null".to_string())?;
        let sticker_c = CString::new(sticker_id).map_err(|_| "sticker id contains null".to_string())?;
        let ok = unsafe { mi_client_resend_private_sticker(self.handle, peer_c.as_ptr(), msg_c.as_ptr(), sticker_c.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_private_location(&self,
                                 peer: &str,
                                 lat_e7: i32,
                                 lon_e7: i32,
                                 label: &str) -> Result<String, String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let label_c = CString::new(label).map_err(|_| "label contains null".to_string())?;
        let mut out: *mut c_char = ptr::null_mut();
        let ok = unsafe {
            mi_client_send_private_location(
                self.handle,
                peer_c.as_ptr(),
                lat_e7,
                lon_e7,
                label_c.as_ptr(),
                &mut out,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(take_c_string(out))
    }

    pub fn resend_private_location(&self,
                                   peer: &str,
                                   message_id: &str,
                                   lat_e7: i32,
                                   lon_e7: i32,
                                   label: &str) -> Result<(), String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let msg_c = CString::new(message_id).map_err(|_| "message id contains null".to_string())?;
        let label_c = CString::new(label).map_err(|_| "label contains null".to_string())?;
        let ok = unsafe {
            mi_client_resend_private_location(
                self.handle,
                peer_c.as_ptr(),
                msg_c.as_ptr(),
                lat_e7,
                lon_e7,
                label_c.as_ptr(),
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_private_contact(&self,
                                peer: &str,
                                card_username: &str,
                                card_display: &str) -> Result<String, String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let card_user_c = CString::new(card_username)
            .map_err(|_| "card username contains null".to_string())?;
        let card_display_c = CString::new(card_display)
            .map_err(|_| "card display contains null".to_string())?;
        let mut out: *mut c_char = ptr::null_mut();
        let ok = unsafe {
            mi_client_send_private_contact(
                self.handle,
                peer_c.as_ptr(),
                card_user_c.as_ptr(),
                card_display_c.as_ptr(),
                &mut out,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(take_c_string(out))
    }

    pub fn resend_private_contact(&self,
                                  peer: &str,
                                  message_id: &str,
                                  card_username: &str,
                                  card_display: &str) -> Result<(), String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let msg_c = CString::new(message_id).map_err(|_| "message id contains null".to_string())?;
        let card_user_c = CString::new(card_username)
            .map_err(|_| "card username contains null".to_string())?;
        let card_display_c = CString::new(card_display)
            .map_err(|_| "card display contains null".to_string())?;
        let ok = unsafe {
            mi_client_resend_private_contact(
                self.handle,
                peer_c.as_ptr(),
                msg_c.as_ptr(),
                card_user_c.as_ptr(),
                card_display_c.as_ptr(),
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_read_receipt(&self, peer: &str, message_id: &str) -> Result<(), String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let msg_c = CString::new(message_id).map_err(|_| "message id contains null".to_string())?;
        let ok = unsafe { mi_client_send_read_receipt(self.handle, peer_c.as_ptr(), msg_c.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_typing(&self, peer: &str, typing: bool) -> Result<(), String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let ok = unsafe { mi_client_send_typing(self.handle, peer_c.as_ptr(), if typing { 1 } else { 0 }) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_presence(&self, peer: &str, online: bool) -> Result<(), String> {
        let peer_c = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let ok = unsafe { mi_client_send_presence(self.handle, peer_c.as_ptr(), if online { 1 } else { 0 }) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }
    pub fn add_friend(&self, username: &str, remark: &str) -> Result<(), String> {
        let u = CString::new(username).map_err(|_| "username contains null".to_string())?;
        let r = CString::new(remark).map_err(|_| "remark contains null".to_string())?;
        let ok = unsafe { mi_client_add_friend(self.handle, u.as_ptr(), r.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn set_friend_remark(&self, username: &str, remark: &str) -> Result<(), String> {
        let u = CString::new(username).map_err(|_| "username contains null".to_string())?;
        let r = CString::new(remark).map_err(|_| "remark contains null".to_string())?;
        let ok = unsafe { mi_client_set_friend_remark(self.handle, u.as_ptr(), r.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn delete_friend(&self, username: &str) -> Result<(), String> {
        let u = CString::new(username).map_err(|_| "username contains null".to_string())?;
        let ok = unsafe { mi_client_delete_friend(self.handle, u.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn set_user_blocked(&self, username: &str, blocked: bool) -> Result<(), String> {
        let u = CString::new(username).map_err(|_| "username contains null".to_string())?;
        let ok = unsafe { mi_client_set_user_blocked(self.handle, u.as_ptr(), if blocked { 1 } else { 0 }) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_friend_request(&self, username: &str, remark: &str) -> Result<(), String> {
        let u = CString::new(username).map_err(|_| "username contains null".to_string())?;
        let r = CString::new(remark).map_err(|_| "remark contains null".to_string())?;
        let ok = unsafe { mi_client_send_friend_request(self.handle, u.as_ptr(), r.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn respond_friend_request(&self, username: &str, accept: bool) -> Result<(), String> {
        let u = CString::new(username).map_err(|_| "username contains null".to_string())?;
        let ok = unsafe { mi_client_respond_friend_request(self.handle, u.as_ptr(), if accept { 1 } else { 0 }) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn list_friends(&self, max_entries: u32) -> Vec<FriendEntry> {
        if self.handle.is_null() || max_entries == 0 {
            return Vec::new();
        }
        let mut buffer = vec![mi_friend_entry_t { username: ptr::null(), remark: ptr::null() }; max_entries as usize];
        let count = unsafe { mi_client_list_friends(self.handle, buffer.as_mut_ptr(), max_entries) };
        buffer.truncate(count as usize);
        buffer.iter().map(friend_from_c).collect()
    }

    pub fn sync_friends(&self, max_entries: u32) -> Result<(Vec<FriendEntry>, bool), String> {
        if self.handle.is_null() || max_entries == 0 {
            return Ok((Vec::new(), false));
        }
        let mut buffer = vec![mi_friend_entry_t { username: ptr::null(), remark: ptr::null() }; max_entries as usize];
        let mut changed: c_int = 0;
        let count = unsafe {
            mi_client_sync_friends(self.handle, buffer.as_mut_ptr(), max_entries, &mut changed)
        };
        let err = self.last_error();
        if !err.is_empty() {
            return Err(err);
        }
        buffer.truncate(count as usize);
        let friends = buffer.iter().map(friend_from_c).collect();
        Ok((friends, changed != 0))
    }

    pub fn list_friend_requests(&self, max_entries: u32) -> Vec<FriendRequestEntry> {
        if self.handle.is_null() || max_entries == 0 {
            return Vec::new();
        }
        let mut buffer = vec![mi_friend_request_entry_t { requester_username: ptr::null(), requester_remark: ptr::null() }; max_entries as usize];
        let count = unsafe {
            mi_client_list_friend_requests(self.handle, buffer.as_mut_ptr(), max_entries)
        };
        buffer.truncate(count as usize);
        buffer.iter().map(friend_request_from_c).collect()
    }

    pub fn list_devices(&self, max_entries: u32) -> Result<Vec<DeviceEntry>, String> {
        if self.handle.is_null() || max_entries == 0 {
            return Ok(Vec::new());
        }
        let mut buffer = vec![mi_device_entry_t::default(); max_entries as usize];
        let count = unsafe { mi_client_list_devices(self.handle, buffer.as_mut_ptr(), max_entries) };
        let err = self.last_error();
        if !err.is_empty() {
            return Err(err);
        }
        buffer.truncate(count as usize);
        Ok(buffer.iter().map(device_from_c).collect())
    }

    pub fn kick_device(&self, device_id: &str) -> Result<(), String> {
        let d = CString::new(device_id).map_err(|_| "device id contains null".to_string())?;
        let ok = unsafe { mi_client_kick_device(self.handle, d.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn join_group(&self, group_id: &str) -> Result<(), String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let ok = unsafe { mi_client_join_group(self.handle, g.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn leave_group(&self, group_id: &str) -> Result<(), String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let ok = unsafe { mi_client_leave_group(self.handle, g.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn create_group(&self) -> Result<String, String> {
        let mut out: *mut c_char = ptr::null_mut();
        let ok = unsafe { mi_client_create_group(self.handle, &mut out) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(take_c_string(out))
    }

    pub fn send_group_invite(&self, group_id: &str, peer: &str) -> Result<String, String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let p = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let mut out: *mut c_char = ptr::null_mut();
        let ok = unsafe { mi_client_send_group_invite(self.handle, g.as_ptr(), p.as_ptr(), &mut out) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(take_c_string(out))
    }

    pub fn list_group_members_info(&self, group_id: &str, max_entries: u32) -> Result<Vec<GroupMemberEntry>, String> {
        if max_entries == 0 {
            return Ok(Vec::new());
        }
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let mut buffer = vec![mi_group_member_entry_t::default(); max_entries as usize];
        let count = unsafe {
            mi_client_list_group_members_info(self.handle, g.as_ptr(), buffer.as_mut_ptr(), max_entries)
        };
        let err = self.last_error();
        if !err.is_empty() {
            return Err(err);
        }
        buffer.truncate(count as usize);
        Ok(buffer.iter().map(group_member_from_c).collect())
    }

    pub fn set_group_member_role(&self, group_id: &str, peer: &str, role: u32) -> Result<(), String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let p = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let ok = unsafe { mi_client_set_group_member_role(self.handle, g.as_ptr(), p.as_ptr(), role as c_uint) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn kick_group_member(&self, group_id: &str, peer: &str) -> Result<(), String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let p = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        let ok = unsafe { mi_client_kick_group_member(self.handle, g.as_ptr(), p.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }
    pub fn start_group_call(&self, group_id: &str, video: bool) -> Result<GroupCallStart, String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let mut call_id = [0u8; 16];
        let mut key_id: c_uint = 0;
        let ok = unsafe {
            mi_client_start_group_call(
                self.handle,
                g.as_ptr(),
                if video { 1 } else { 0 },
                call_id.as_mut_ptr(),
                call_id.len() as c_uint,
                &mut key_id,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(GroupCallStart {
            call_id,
            key_id: key_id as u32,
        })
    }

    pub fn join_group_call(&self, group_id: &str, call_id: &[u8], video: bool) -> Result<u32, String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        if call_id.len() != 16 {
            return Err("call id invalid".to_string());
        }
        let mut key_id: c_uint = 0;
        let ok = unsafe {
            mi_client_join_group_call(
                self.handle,
                g.as_ptr(),
                call_id.as_ptr(),
                call_id.len() as c_uint,
                if video { 1 } else { 0 },
                &mut key_id,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(key_id as u32)
    }

    pub fn leave_group_call(&self, group_id: &str, call_id: &[u8]) -> Result<(), String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        if call_id.len() != 16 {
            return Err("call id invalid".to_string());
        }
        let ok = unsafe { mi_client_leave_group_call(self.handle, g.as_ptr(), call_id.as_ptr(), call_id.len() as c_uint) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn get_group_call_key(&self, group_id: &str, call_id: &[u8], key_id: u32) -> Result<Vec<u8>, String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        if call_id.len() != 16 {
            return Err("call id invalid".to_string());
        }
        let mut out = vec![0u8; 32];
        let ok = unsafe {
            mi_client_get_group_call_key(
                self.handle,
                g.as_ptr(),
                call_id.as_ptr(),
                call_id.len() as c_uint,
                key_id as c_uint,
                out.as_mut_ptr(),
                out.len() as c_uint,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(out)
    }

    pub fn rotate_group_call_key(&self,
                                 group_id: &str,
                                 call_id: &[u8],
                                 key_id: u32,
                                 members: &[String]) -> Result<(), String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        if call_id.len() != 16 {
            return Err("call id invalid".to_string());
        }
        let mut c_members = Vec::with_capacity(members.len());
        let mut ptrs = Vec::with_capacity(members.len());
        for m in members {
            let c = CString::new(m.as_str()).map_err(|_| "member contains null".to_string())?;
            ptrs.push(c.as_ptr());
            c_members.push(c);
        }
        let ok = unsafe {
            mi_client_rotate_group_call_key(
                self.handle,
                g.as_ptr(),
                call_id.as_ptr(),
                call_id.len() as c_uint,
                key_id as c_uint,
                ptrs.as_ptr(),
                ptrs.len() as c_uint,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn request_group_call_key(&self,
                                  group_id: &str,
                                  call_id: &[u8],
                                  key_id: u32,
                                  members: &[String]) -> Result<(), String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        if call_id.len() != 16 {
            return Err("call id invalid".to_string());
        }
        let mut c_members = Vec::with_capacity(members.len());
        let mut ptrs = Vec::with_capacity(members.len());
        for m in members {
            let c = CString::new(m.as_str()).map_err(|_| "member contains null".to_string())?;
            ptrs.push(c.as_ptr());
            c_members.push(c);
        }
        let ok = unsafe {
            mi_client_request_group_call_key(
                self.handle,
                g.as_ptr(),
                call_id.as_ptr(),
                call_id.len() as c_uint,
                key_id as c_uint,
                ptrs.as_ptr(),
                ptrs.len() as c_uint,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn send_group_call_signal(&self,
                                  op: u8,
                                  group_id: &str,
                                  call_id: Option<&[u8]>,
                                  video: bool,
                                  key_id: u32,
                                  seq: u32,
                                  ts_ms: u64,
                                  ext: Option<&[u8]>,
                                  max_members: u32) -> Result<GroupCallSignalResponse, String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        let (call_ptr, call_len) = match call_id {
            Some(id) => {
                if id.len() != 16 {
                    return Err("call id invalid".to_string());
                }
                (id.as_ptr(), id.len() as c_uint)
            }
            None => (ptr::null(), 0),
        };
        let (ext_ptr, ext_len) = match ext {
            Some(data) => (data.as_ptr(), data.len() as c_uint),
            None => (ptr::null(), 0),
        };
        let mut out_call_id = [0u8; 16];
        let mut out_key_id: c_uint = 0;
        let mut out_member_count: c_uint = 0;
        let mut members_buf = vec![mi_group_call_member_t::default(); max_members as usize];
        let ok = unsafe {
            mi_client_send_group_call_signal(
                self.handle,
                op,
                g.as_ptr(),
                call_ptr,
                call_len,
                if video { 1 } else { 0 },
                key_id as c_uint,
                seq as c_uint,
                ts_ms,
                ext_ptr,
                ext_len,
                out_call_id.as_mut_ptr(),
                out_call_id.len() as c_uint,
                &mut out_key_id,
                members_buf.as_mut_ptr(),
                max_members,
                &mut out_member_count,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        let take = std::cmp::min(out_member_count as usize, members_buf.len());
        members_buf.truncate(take);
        let members = members_buf.iter().map(group_call_member_from_c).collect();
        Ok(GroupCallSignalResponse {
            call_id: out_call_id,
            key_id: out_key_id as u32,
            members,
            member_count: out_member_count as u32,
        })
    }
    pub fn load_chat_history(&self,
                             conv_id: &str,
                             is_group: bool,
                             limit: u32,
                             max_entries: u32) -> Result<Vec<HistoryEntry>, String> {
        if max_entries == 0 {
            return Ok(Vec::new());
        }
        let c = CString::new(conv_id).map_err(|_| "conv id contains null".to_string())?;
        let mut buffer = vec![mi_history_entry_t::default(); max_entries as usize];
        let count = unsafe {
            mi_client_load_chat_history(
                self.handle,
                c.as_ptr(),
                if is_group { 1 } else { 0 },
                limit as c_uint,
                buffer.as_mut_ptr(),
                max_entries as c_uint,
            )
        };
        let err = self.last_error();
        if !err.is_empty() {
            return Err(err);
        }
        buffer.truncate(count as usize);
        Ok(buffer.iter().map(history_from_c).collect())
    }

    pub fn delete_chat_history(&self,
                               conv_id: &str,
                               is_group: bool,
                               delete_attachments: bool,
                               secure_wipe: bool) -> Result<(), String> {
        let c = CString::new(conv_id).map_err(|_| "conv id contains null".to_string())?;
        let ok = unsafe {
            mi_client_delete_chat_history(
                self.handle,
                c.as_ptr(),
                if is_group { 1 } else { 0 },
                if delete_attachments { 1 } else { 0 },
                if secure_wipe { 1 } else { 0 },
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn set_history_enabled(&self, enabled: bool) -> Result<(), String> {
        let ok = unsafe { mi_client_set_history_enabled(self.handle, if enabled { 1 } else { 0 }) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn clear_all_history(&self, delete_attachments: bool, secure_wipe: bool) -> Result<(), String> {
        let ok = unsafe {
            mi_client_clear_all_history(
                self.handle,
                if delete_attachments { 1 } else { 0 },
                if secure_wipe { 1 } else { 0 },
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn begin_device_pairing_primary(&self) -> Result<String, String> {
        let mut out: *mut c_char = ptr::null_mut();
        let ok = unsafe { mi_client_begin_device_pairing_primary(self.handle, &mut out) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(take_c_string(out))
    }

    pub fn poll_device_pairing_requests(&self, max_entries: u32) -> Result<Vec<DevicePairingRequest>, String> {
        if max_entries == 0 {
            return Ok(Vec::new());
        }
        let mut buffer = vec![mi_device_pairing_request_t::default(); max_entries as usize];
        let count = unsafe {
            mi_client_poll_device_pairing_requests(self.handle, buffer.as_mut_ptr(), max_entries)
        };
        let err = self.last_error();
        if !err.is_empty() {
            return Err(err);
        }
        buffer.truncate(count as usize);
        Ok(buffer.iter().map(pairing_request_from_c).collect())
    }

    pub fn approve_device_pairing_request(&self, device_id: &str, request_id_hex: &str) -> Result<(), String> {
        let d = CString::new(device_id).map_err(|_| "device id contains null".to_string())?;
        let r = CString::new(request_id_hex).map_err(|_| "request id contains null".to_string())?;
        let ok = unsafe { mi_client_approve_device_pairing_request(self.handle, d.as_ptr(), r.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn begin_device_pairing_linked(&self, pairing_code: &str) -> Result<(), String> {
        let c = CString::new(pairing_code).map_err(|_| "pairing code contains null".to_string())?;
        let ok = unsafe { mi_client_begin_device_pairing_linked(self.handle, c.as_ptr()) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn poll_device_pairing_linked(&self) -> Result<bool, String> {
        let mut completed: c_int = 0;
        let ok = unsafe { mi_client_poll_device_pairing_linked(self.handle, &mut completed) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(completed != 0)
    }

    pub fn cancel_device_pairing(&self) {
        unsafe { mi_client_cancel_device_pairing(self.handle) };
    }

    pub fn store_attachment_preview_bytes(&self,
                                          file_id: &str,
                                          file_name: &str,
                                          file_size: u64,
                                          bytes: &[u8]) -> Result<(), String> {
        let f = CString::new(file_id).map_err(|_| "file id contains null".to_string())?;
        let n = CString::new(file_name).map_err(|_| "file name contains null".to_string())?;
        let ok = unsafe {
            mi_client_store_attachment_preview_bytes(
                self.handle,
                f.as_ptr(),
                n.as_ptr(),
                file_size,
                bytes.as_ptr(),
                bytes.len() as c_uint,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn download_chat_file_to_path<F: FnMut(u64, u64) + 'static>(
        &self,
        file_id: &str,
        file_key: &[u8],
        file_name: Option<&str>,
        file_size: u64,
        out_path: &str,
        wipe_after_read: bool,
        mut on_progress: Option<F>,
    ) -> Result<(), String> {
        if file_key.len() != 32 {
            return Err("file key invalid".to_string());
        }
        let f = CString::new(file_id).map_err(|_| "file id contains null".to_string())?;
        let n = match file_name {
            Some(name) => Some(CString::new(name).map_err(|_| "file name contains null".to_string())?),
            None => None,
        };
        let p = CString::new(out_path).map_err(|_| "out path contains null".to_string())?;
        let mut holder = on_progress.take().map(|cb| ProgressHolder { cb: Box::new(cb) });
        let (cb, user_data) = match holder.as_mut() {
            Some(h) => (Some(progress_trampoline as extern "C" fn(u64, u64, *mut c_void)), h as *mut _ as *mut c_void),
            None => (None, ptr::null_mut()),
        };
        let ok = unsafe {
            mi_client_download_chat_file_to_path(
                self.handle,
                f.as_ptr(),
                file_key.as_ptr(),
                file_key.len() as c_uint,
                match &n {
                    Some(name) => name.as_ptr(),
                    None => ptr::null(),
                },
                file_size,
                p.as_ptr(),
                if wipe_after_read { 1 } else { 0 },
                cb,
                user_data,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn download_chat_file_to_bytes(&self,
                                       file_id: &str,
                                       file_key: &[u8],
                                       file_name: Option<&str>,
                                       file_size: u64,
                                       wipe_after_read: bool) -> Result<Vec<u8>, String> {
        if file_key.len() != 32 {
            return Err("file key invalid".to_string());
        }
        let f = CString::new(file_id).map_err(|_| "file id contains null".to_string())?;
        let n = match file_name {
            Some(name) => Some(CString::new(name).map_err(|_| "file name contains null".to_string())?),
            None => None,
        };
        let mut out_ptr: *mut u8 = ptr::null_mut();
        let mut out_len: u64 = 0;
        let ok = unsafe {
            mi_client_download_chat_file_to_bytes(
                self.handle,
                f.as_ptr(),
                file_key.as_ptr(),
                file_key.len() as c_uint,
                match &n {
                    Some(name) => name.as_ptr(),
                    None => ptr::null(),
                },
                file_size,
                if wipe_after_read { 1 } else { 0 },
                &mut out_ptr,
                &mut out_len,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        if out_ptr.is_null() || out_len == 0 {
            return Ok(Vec::new());
        }
        let bytes = copy_bytes_u64(out_ptr as *const u8, out_len);
        unsafe { mi_client_free(out_ptr as *mut c_void) };
        Ok(bytes)
    }
    pub fn get_media_config(&self) -> Result<MediaConfig, String> {
        let mut cfg = mi_media_config_t::default();
        let ok = unsafe { mi_client_get_media_config(self.handle, &mut cfg) };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(MediaConfig {
            audio_delay_ms: cfg.audio_delay_ms,
            video_delay_ms: cfg.video_delay_ms,
            audio_max_frames: cfg.audio_max_frames,
            video_max_frames: cfg.video_max_frames,
            pull_max_packets: cfg.pull_max_packets,
            pull_wait_ms: cfg.pull_wait_ms,
            group_pull_max_packets: cfg.group_pull_max_packets,
            group_pull_wait_ms: cfg.group_pull_wait_ms,
        })
    }

    pub fn derive_media_root(&self, peer: &str, call_id: &[u8]) -> Result<Vec<u8>, String> {
        let p = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        if call_id.len() != 16 {
            return Err("call id invalid".to_string());
        }
        let mut out = vec![0u8; 32];
        let ok = unsafe {
            mi_client_derive_media_root(
                self.handle,
                p.as_ptr(),
                call_id.as_ptr(),
                call_id.len() as c_uint,
                out.as_mut_ptr(),
                out.len() as c_uint,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(out)
    }

    pub fn push_media(&self, peer: &str, call_id: &[u8], packet: &[u8]) -> Result<(), String> {
        let p = CString::new(peer).map_err(|_| "peer contains null".to_string())?;
        if call_id.len() != 16 {
            return Err("call id invalid".to_string());
        }
        let ok = unsafe {
            mi_client_push_media(
                self.handle,
                p.as_ptr(),
                call_id.as_ptr(),
                call_id.len() as c_uint,
                packet.as_ptr(),
                packet.len() as c_uint,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn pull_media(&self, call_id: &[u8], max_packets: u32, wait_ms: u32) -> Result<Vec<MediaPacket>, String> {
        if call_id.len() != 16 {
            return Err("call id invalid".to_string());
        }
        if max_packets == 0 {
            return Ok(Vec::new());
        }
        let mut buffer = vec![mi_media_packet_t::default(); max_packets as usize];
        let count = unsafe {
            mi_client_pull_media(
                self.handle,
                call_id.as_ptr(),
                call_id.len() as c_uint,
                max_packets,
                wait_ms,
                buffer.as_mut_ptr(),
            )
        };
        let err = self.last_error();
        if !err.is_empty() {
            return Err(err);
        }
        buffer.truncate(count as usize);
        Ok(buffer.iter().map(media_packet_from_c).collect())
    }

    pub fn push_group_media(&self, group_id: &str, call_id: &[u8], packet: &[u8]) -> Result<(), String> {
        let g = CString::new(group_id).map_err(|_| "group id contains null".to_string())?;
        if call_id.len() != 16 {
            return Err("call id invalid".to_string());
        }
        let ok = unsafe {
            mi_client_push_group_media(
                self.handle,
                g.as_ptr(),
                call_id.as_ptr(),
                call_id.len() as c_uint,
                packet.as_ptr(),
                packet.len() as c_uint,
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn pull_group_media(&self, call_id: &[u8], max_packets: u32, wait_ms: u32) -> Result<Vec<MediaPacket>, String> {
        if call_id.len() != 16 {
            return Err("call id invalid".to_string());
        }
        if max_packets == 0 {
            return Ok(Vec::new());
        }
        let mut buffer = vec![mi_media_packet_t::default(); max_packets as usize];
        let count = unsafe {
            mi_client_pull_group_media(
                self.handle,
                call_id.as_ptr(),
                call_id.len() as c_uint,
                max_packets,
                wait_ms,
                buffer.as_mut_ptr(),
            )
        };
        let err = self.last_error();
        if !err.is_empty() {
            return Err(err);
        }
        buffer.truncate(count as usize);
        Ok(buffer.iter().map(media_packet_from_c).collect())
    }

    pub fn add_media_subscription(&self, call_id: &[u8], is_group: bool, group_id: Option<&str>) -> Result<(), String> {
        if call_id.len() != 16 {
            return Err("call id invalid".to_string());
        }
        let group = match group_id {
            Some(g) => Some(CString::new(g).map_err(|_| "group id contains null".to_string())?),
            None => None,
        };
        let ok = unsafe {
            mi_client_add_media_subscription(
                self.handle,
                call_id.as_ptr(),
                call_id.len() as c_uint,
                if is_group { 1 } else { 0 },
                match &group {
                    Some(g) => g.as_ptr(),
                    None => ptr::null(),
                },
            )
        };
        if ok == 0 {
            return Err(self.last_error());
        }
        Ok(())
    }

    pub fn clear_media_subscriptions(&self) {
        unsafe { mi_client_clear_media_subscriptions(self.handle) };
    }

    pub fn poll_events(&self, max_events: u32, wait_ms: u32) -> Vec<Event> {
        if self.handle.is_null() || max_events == 0 {
            return Vec::new();
        }
        let mut buffer = vec![mi_event_t::default(); max_events as usize];
        let count = unsafe { mi_client_poll_event(self.handle, buffer.as_mut_ptr(), max_events, wait_ms) };
        buffer.truncate(count as usize);
        buffer
            .into_iter()
            .map(|ev| Event {
                event_type: ev.type_,
                ts_ms: ev.ts_ms,
                peer: opt_string(ev.peer),
                sender: opt_string(ev.sender),
                group_id: opt_string(ev.group_id),
                message_id: opt_string(ev.message_id),
                text: opt_string(ev.text),
                file_id: opt_string(ev.file_id),
                file_name: opt_string(ev.file_name),
                file_size: ev.file_size,
                file_key: copy_bytes(ev.file_key, ev.file_key_len),
                sticker_id: opt_string(ev.sticker_id),
                notice_kind: ev.notice_kind,
                actor: opt_string(ev.actor),
                target: opt_string(ev.target),
                role: ev.role,
                typing: ev.typing != 0,
                online: ev.online != 0,
                call_id: ev.call_id,
                call_key_id: ev.call_key_id,
                call_op: ev.call_op,
                call_media_flags: ev.call_media_flags,
                payload: copy_bytes(ev.payload, ev.payload_len),
            })
            .collect()
    }
}

impl Drop for Client {
    fn drop(&mut self) {
        if !self.handle.is_null() {
            unsafe { mi_client_destroy(self.handle) };
            self.handle = ptr::null_mut();
        }
    }
}
