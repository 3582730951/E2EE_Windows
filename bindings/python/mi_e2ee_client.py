import ctypes
import os

MI_E2EE_SDK_ABI_VERSION = 1
MI_MAX_FRIEND_ENTRIES = 512
MI_MAX_FRIEND_REQUEST_ENTRIES = 256
MI_MAX_DEVICE_ENTRIES = 256
MI_MAX_GROUP_MEMBER_ENTRIES = 1024
MI_MAX_GROUP_CALL_MEMBERS = 64
MI_MAX_HISTORY_ENTRIES = 256
MI_MAX_MEDIA_PACKETS = 64
MI_CALL_ID_LEN = 16
MI_MEDIA_ROOT_LEN = 32
MI_GROUP_CALL_KEY_LEN = 32

class MiSdkVersion(ctypes.Structure):
    _fields_ = [
        ("major", ctypes.c_uint32),
        ("minor", ctypes.c_uint32),
        ("patch", ctypes.c_uint32),
        ("abi", ctypes.c_uint32),
    ]

class MiEvent(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_uint32),
        ("ts_ms", ctypes.c_uint64),
        ("peer", ctypes.c_char_p),
        ("sender", ctypes.c_char_p),
        ("group_id", ctypes.c_char_p),
        ("message_id", ctypes.c_char_p),
        ("text", ctypes.c_char_p),
        ("file_id", ctypes.c_char_p),
        ("file_name", ctypes.c_char_p),
        ("file_size", ctypes.c_uint64),
        ("file_key", ctypes.POINTER(ctypes.c_uint8)),
        ("file_key_len", ctypes.c_uint32),
        ("sticker_id", ctypes.c_char_p),
        ("notice_kind", ctypes.c_uint32),
        ("actor", ctypes.c_char_p),
        ("target", ctypes.c_char_p),
        ("role", ctypes.c_uint32),
        ("typing", ctypes.c_uint8),
        ("online", ctypes.c_uint8),
        ("reserved0", ctypes.c_uint8),
        ("reserved1", ctypes.c_uint8),
        ("call_id", ctypes.c_uint8 * 16),
        ("call_key_id", ctypes.c_uint32),
        ("call_op", ctypes.c_uint32),
        ("call_media_flags", ctypes.c_uint8),
        ("call_reserved0", ctypes.c_uint8),
        ("call_reserved1", ctypes.c_uint8),
        ("call_reserved2", ctypes.c_uint8),
        ("payload", ctypes.POINTER(ctypes.c_uint8)),
        ("payload_len", ctypes.c_uint32),
    ]

class MiFriendEntry(ctypes.Structure):
    _fields_ = [
        ("username", ctypes.c_char_p),
        ("remark", ctypes.c_char_p),
    ]

class MiFriendRequestEntry(ctypes.Structure):
    _fields_ = [
        ("requester_username", ctypes.c_char_p),
        ("requester_remark", ctypes.c_char_p),
    ]

class MiDeviceEntry(ctypes.Structure):
    _fields_ = [
        ("device_id", ctypes.c_char_p),
        ("last_seen_sec", ctypes.c_uint32),
    ]

class MiDevicePairingRequest(ctypes.Structure):
    _fields_ = [
        ("device_id", ctypes.c_char_p),
        ("request_id_hex", ctypes.c_char_p),
    ]

class MiGroupMemberEntry(ctypes.Structure):
    _fields_ = [
        ("username", ctypes.c_char_p),
        ("role", ctypes.c_uint32),
    ]

class MiGroupCallMember(ctypes.Structure):
    _fields_ = [
        ("username", ctypes.c_char_p),
    ]

class MiMediaPacket(ctypes.Structure):
    _fields_ = [
        ("sender", ctypes.c_char_p),
        ("payload", ctypes.POINTER(ctypes.c_uint8)),
        ("payload_len", ctypes.c_uint32),
    ]

class MiMediaConfig(ctypes.Structure):
    _fields_ = [
        ("audio_delay_ms", ctypes.c_uint32),
        ("video_delay_ms", ctypes.c_uint32),
        ("audio_max_frames", ctypes.c_uint32),
        ("video_max_frames", ctypes.c_uint32),
        ("pull_max_packets", ctypes.c_uint32),
        ("pull_wait_ms", ctypes.c_uint32),
        ("group_pull_max_packets", ctypes.c_uint32),
        ("group_pull_wait_ms", ctypes.c_uint32),
    ]

class MiHistoryEntry(ctypes.Structure):
    _fields_ = [
        ("kind", ctypes.c_uint32),
        ("status", ctypes.c_uint32),
        ("is_group", ctypes.c_uint8),
        ("outgoing", ctypes.c_uint8),
        ("reserved0", ctypes.c_uint8),
        ("reserved1", ctypes.c_uint8),
        ("timestamp_sec", ctypes.c_uint64),
        ("conv_id", ctypes.c_char_p),
        ("sender", ctypes.c_char_p),
        ("message_id", ctypes.c_char_p),
        ("text", ctypes.c_char_p),
        ("file_id", ctypes.c_char_p),
        ("file_key", ctypes.POINTER(ctypes.c_uint8)),
        ("file_key_len", ctypes.c_uint32),
        ("file_name", ctypes.c_char_p),
        ("file_size", ctypes.c_uint64),
        ("sticker_id", ctypes.c_char_p),
    ]

MiProgressCallback = ctypes.CFUNCTYPE(None, ctypes.c_uint64, ctypes.c_uint64, ctypes.c_void_p)


def _load_library():
    env = os.getenv("MI_E2EE_CLIENT_DLL")
    if env:
        return ctypes.CDLL(env)
    candidates = [
        "mi_e2ee_client_sdk.dll",
        "libmi_e2ee_client_sdk.so",
        "libmi_e2ee_client_sdk.dylib",
        "mi_e2ee_client_sdk",
        "mi_e2ee_client_core.dll",
        "libmi_e2ee_client_core.so",
        "libmi_e2ee_client_core.dylib",
        "mi_e2ee_client_core",
    ]
    for name in candidates:
        try:
            return ctypes.CDLL(name)
        except OSError:
            continue
    raise OSError("mi_e2ee_client_core shared library not found")


_lib = _load_library()
_abi_checked = False

_lib.mi_client_get_version.argtypes = [ctypes.POINTER(MiSdkVersion)]
_lib.mi_client_get_version.restype = None

_lib.mi_client_get_capabilities.argtypes = []
_lib.mi_client_get_capabilities.restype = ctypes.c_uint32

_lib.mi_client_create.argtypes = [ctypes.c_char_p]
_lib.mi_client_create.restype = ctypes.c_void_p

_lib.mi_client_last_create_error.argtypes = []
_lib.mi_client_last_create_error.restype = ctypes.c_char_p

_lib.mi_client_destroy.argtypes = [ctypes.c_void_p]
_lib.mi_client_destroy.restype = None

_lib.mi_client_last_error.argtypes = [ctypes.c_void_p]
_lib.mi_client_last_error.restype = ctypes.c_char_p

_lib.mi_client_token.argtypes = [ctypes.c_void_p]
_lib.mi_client_token.restype = ctypes.c_char_p

_lib.mi_client_device_id.argtypes = [ctypes.c_void_p]
_lib.mi_client_device_id.restype = ctypes.c_char_p

_lib.mi_client_remote_ok.argtypes = [ctypes.c_void_p]
_lib.mi_client_remote_ok.restype = ctypes.c_int

_lib.mi_client_remote_error.argtypes = [ctypes.c_void_p]
_lib.mi_client_remote_error.restype = ctypes.c_char_p

_lib.mi_client_is_remote_mode.argtypes = [ctypes.c_void_p]
_lib.mi_client_is_remote_mode.restype = ctypes.c_int

_lib.mi_client_relogin.argtypes = [ctypes.c_void_p]
_lib.mi_client_relogin.restype = ctypes.c_int

_lib.mi_client_has_pending_server_trust.argtypes = [ctypes.c_void_p]
_lib.mi_client_has_pending_server_trust.restype = ctypes.c_int

_lib.mi_client_pending_server_fingerprint.argtypes = [ctypes.c_void_p]
_lib.mi_client_pending_server_fingerprint.restype = ctypes.c_char_p

_lib.mi_client_pending_server_pin.argtypes = [ctypes.c_void_p]
_lib.mi_client_pending_server_pin.restype = ctypes.c_char_p

_lib.mi_client_trust_pending_server.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_lib.mi_client_trust_pending_server.restype = ctypes.c_int

_lib.mi_client_has_pending_peer_trust.argtypes = [ctypes.c_void_p]
_lib.mi_client_has_pending_peer_trust.restype = ctypes.c_int

_lib.mi_client_pending_peer_username.argtypes = [ctypes.c_void_p]
_lib.mi_client_pending_peer_username.restype = ctypes.c_char_p

_lib.mi_client_pending_peer_fingerprint.argtypes = [ctypes.c_void_p]
_lib.mi_client_pending_peer_fingerprint.restype = ctypes.c_char_p

_lib.mi_client_pending_peer_pin.argtypes = [ctypes.c_void_p]
_lib.mi_client_pending_peer_pin.restype = ctypes.c_char_p

_lib.mi_client_trust_pending_peer.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_lib.mi_client_trust_pending_peer.restype = ctypes.c_int

_lib.mi_client_register.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_register.restype = ctypes.c_int

_lib.mi_client_login.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_login.restype = ctypes.c_int

_lib.mi_client_logout.argtypes = [ctypes.c_void_p]
_lib.mi_client_logout.restype = ctypes.c_int

_lib.mi_client_heartbeat.argtypes = [ctypes.c_void_p]
_lib.mi_client_heartbeat.restype = ctypes.c_int

_lib.mi_client_send_private_text_with_reply.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
]
_lib.mi_client_send_private_text_with_reply.restype = ctypes.c_int

_lib.mi_client_resend_private_text.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_resend_private_text.restype = ctypes.c_int

_lib.mi_client_resend_private_text_with_reply.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
]
_lib.mi_client_resend_private_text_with_reply.restype = ctypes.c_int

_lib.mi_client_resend_group_text.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_resend_group_text.restype = ctypes.c_int

_lib.mi_client_send_private_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p)]
_lib.mi_client_send_private_file.restype = ctypes.c_int

_lib.mi_client_resend_private_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_resend_private_file.restype = ctypes.c_int

_lib.mi_client_send_group_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p)]
_lib.mi_client_send_group_file.restype = ctypes.c_int

_lib.mi_client_resend_group_file.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_resend_group_file.restype = ctypes.c_int

_lib.mi_client_send_private_sticker.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.POINTER(ctypes.c_char_p)]
_lib.mi_client_send_private_sticker.restype = ctypes.c_int

_lib.mi_client_resend_private_sticker.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_resend_private_sticker.restype = ctypes.c_int

_lib.mi_client_send_private_location.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_int32,
    ctypes.c_int32,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
]
_lib.mi_client_send_private_location.restype = ctypes.c_int

_lib.mi_client_resend_private_location.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_int32,
    ctypes.c_int32,
    ctypes.c_char_p,
]
_lib.mi_client_resend_private_location.restype = ctypes.c_int

_lib.mi_client_send_private_contact.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
]
_lib.mi_client_send_private_contact.restype = ctypes.c_int

_lib.mi_client_resend_private_contact.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
]
_lib.mi_client_resend_private_contact.restype = ctypes.c_int

_lib.mi_client_send_read_receipt.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_send_read_receipt.restype = ctypes.c_int

_lib.mi_client_send_typing.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
_lib.mi_client_send_typing.restype = ctypes.c_int

_lib.mi_client_send_presence.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
_lib.mi_client_send_presence.restype = ctypes.c_int

_lib.mi_client_add_friend.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_add_friend.restype = ctypes.c_int

_lib.mi_client_delete_friend.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_lib.mi_client_delete_friend.restype = ctypes.c_int

_lib.mi_client_set_friend_remark.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_set_friend_remark.restype = ctypes.c_int

_lib.mi_client_set_user_blocked.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
_lib.mi_client_set_user_blocked.restype = ctypes.c_int

_lib.mi_client_send_friend_request.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_send_friend_request.restype = ctypes.c_int

_lib.mi_client_respond_friend_request.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
_lib.mi_client_respond_friend_request.restype = ctypes.c_int

_lib.mi_client_list_friends.argtypes = [ctypes.c_void_p,
                                        ctypes.POINTER(MiFriendEntry),
                                        ctypes.c_uint32]
_lib.mi_client_list_friends.restype = ctypes.c_uint32

_lib.mi_client_sync_friends.argtypes = [ctypes.c_void_p,
                                        ctypes.POINTER(MiFriendEntry),
                                        ctypes.c_uint32,
                                        ctypes.POINTER(ctypes.c_int)]
_lib.mi_client_sync_friends.restype = ctypes.c_uint32

_lib.mi_client_list_friend_requests.argtypes = [ctypes.c_void_p,
                                                ctypes.POINTER(MiFriendRequestEntry),
                                                ctypes.c_uint32]
_lib.mi_client_list_friend_requests.restype = ctypes.c_uint32

_lib.mi_client_list_devices.argtypes = [ctypes.c_void_p,
                                        ctypes.POINTER(MiDeviceEntry),
                                        ctypes.c_uint32]
_lib.mi_client_list_devices.restype = ctypes.c_uint32

_lib.mi_client_kick_device.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_lib.mi_client_kick_device.restype = ctypes.c_int

_lib.mi_client_join_group.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_lib.mi_client_join_group.restype = ctypes.c_int

_lib.mi_client_leave_group.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_lib.mi_client_leave_group.restype = ctypes.c_int

_lib.mi_client_create_group.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_char_p)]
_lib.mi_client_create_group.restype = ctypes.c_int

_lib.mi_client_send_group_invite.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
]
_lib.mi_client_send_group_invite.restype = ctypes.c_int

_lib.mi_client_list_group_members_info.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(MiGroupMemberEntry),
    ctypes.c_uint32,
]
_lib.mi_client_list_group_members_info.restype = ctypes.c_uint32

_lib.mi_client_set_group_member_role.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint32]
_lib.mi_client_set_group_member_role.restype = ctypes.c_int

_lib.mi_client_kick_group_member.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_kick_group_member.restype = ctypes.c_int

_lib.mi_client_start_group_call.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.POINTER(ctypes.c_uint32),
]
_lib.mi_client_start_group_call.restype = ctypes.c_int

_lib.mi_client_join_group_call.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_uint32),
]
_lib.mi_client_join_group_call.restype = ctypes.c_int

_lib.mi_client_leave_group_call.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
]
_lib.mi_client_leave_group_call.restype = ctypes.c_int

_lib.mi_client_get_group_call_key.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
]
_lib.mi_client_get_group_call_key.restype = ctypes.c_int

_lib.mi_client_rotate_group_call_key.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.c_uint32,
]
_lib.mi_client_rotate_group_call_key.restype = ctypes.c_int

_lib.mi_client_request_group_call_key.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.POINTER(ctypes.c_char_p),
    ctypes.c_uint32,
]
_lib.mi_client_request_group_call_key.restype = ctypes.c_int

_lib.mi_client_send_group_call_signal.argtypes = [
    ctypes.c_void_p,
    ctypes.c_uint8,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.c_int,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_uint64,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.POINTER(ctypes.c_uint32),
    ctypes.POINTER(MiGroupCallMember),
    ctypes.c_uint32,
    ctypes.POINTER(ctypes.c_uint32),
]
_lib.mi_client_send_group_call_signal.restype = ctypes.c_int

_lib.mi_client_load_chat_history.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_int,
    ctypes.c_uint32,
    ctypes.POINTER(MiHistoryEntry),
    ctypes.c_uint32,
]
_lib.mi_client_load_chat_history.restype = ctypes.c_uint32

_lib.mi_client_delete_chat_history.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int]
_lib.mi_client_delete_chat_history.restype = ctypes.c_int

_lib.mi_client_set_history_enabled.argtypes = [ctypes.c_void_p, ctypes.c_int]
_lib.mi_client_set_history_enabled.restype = ctypes.c_int

_lib.mi_client_clear_all_history.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
_lib.mi_client_clear_all_history.restype = ctypes.c_int

_lib.mi_client_begin_device_pairing_primary.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_char_p)]
_lib.mi_client_begin_device_pairing_primary.restype = ctypes.c_int

_lib.mi_client_poll_device_pairing_requests.argtypes = [ctypes.c_void_p, ctypes.POINTER(MiDevicePairingRequest), ctypes.c_uint32]
_lib.mi_client_poll_device_pairing_requests.restype = ctypes.c_uint32

_lib.mi_client_approve_device_pairing_request.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p]
_lib.mi_client_approve_device_pairing_request.restype = ctypes.c_int

_lib.mi_client_begin_device_pairing_linked.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
_lib.mi_client_begin_device_pairing_linked.restype = ctypes.c_int

_lib.mi_client_poll_device_pairing_linked.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
_lib.mi_client_poll_device_pairing_linked.restype = ctypes.c_int

_lib.mi_client_cancel_device_pairing.argtypes = [ctypes.c_void_p]
_lib.mi_client_cancel_device_pairing.restype = None

_lib.mi_client_store_attachment_preview_bytes.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.c_uint64,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
]
_lib.mi_client_store_attachment_preview_bytes.restype = ctypes.c_int

_lib.mi_client_download_chat_file_to_path.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.c_char_p,
    ctypes.c_uint64,
    ctypes.c_char_p,
    ctypes.c_int,
    MiProgressCallback,
    ctypes.c_void_p,
]
_lib.mi_client_download_chat_file_to_path.restype = ctypes.c_int

_lib.mi_client_download_chat_file_to_bytes.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.c_char_p,
    ctypes.c_uint64,
    ctypes.c_int,
    ctypes.POINTER(ctypes.POINTER(ctypes.c_uint8)),
    ctypes.POINTER(ctypes.c_uint64),
]
_lib.mi_client_download_chat_file_to_bytes.restype = ctypes.c_int

_lib.mi_client_get_media_config.argtypes = [ctypes.c_void_p, ctypes.POINTER(MiMediaConfig)]
_lib.mi_client_get_media_config.restype = ctypes.c_int

_lib.mi_client_derive_media_root.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
]
_lib.mi_client_derive_media_root.restype = ctypes.c_int

_lib.mi_client_push_media.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
]
_lib.mi_client_push_media.restype = ctypes.c_int

_lib.mi_client_pull_media.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.POINTER(MiMediaPacket),
]
_lib.mi_client_pull_media.restype = ctypes.c_uint32

_lib.mi_client_push_group_media.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
]
_lib.mi_client_push_group_media.restype = ctypes.c_int

_lib.mi_client_pull_group_media.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.c_uint32,
    ctypes.POINTER(MiMediaPacket),
]
_lib.mi_client_pull_group_media.restype = ctypes.c_uint32

_lib.mi_client_add_media_subscription.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(ctypes.c_uint8),
    ctypes.c_uint32,
    ctypes.c_int,
    ctypes.c_char_p,
]
_lib.mi_client_add_media_subscription.restype = ctypes.c_int

_lib.mi_client_clear_media_subscriptions.argtypes = [ctypes.c_void_p]
_lib.mi_client_clear_media_subscriptions.restype = None

_lib.mi_client_send_private_text.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
]
_lib.mi_client_send_private_text.restype = ctypes.c_int

_lib.mi_client_send_group_text.argtypes = [
    ctypes.c_void_p,
    ctypes.c_char_p,
    ctypes.c_char_p,
    ctypes.POINTER(ctypes.c_char_p),
]
_lib.mi_client_send_group_text.restype = ctypes.c_int

_lib.mi_client_poll_event.argtypes = [
    ctypes.c_void_p,
    ctypes.POINTER(MiEvent),
    ctypes.c_uint32,
    ctypes.c_uint32,
]
_lib.mi_client_poll_event.restype = ctypes.c_uint32

_lib.mi_client_free.argtypes = [ctypes.c_void_p]
_lib.mi_client_free.restype = None


def sdk_version():
    out = MiSdkVersion()
    _lib.mi_client_get_version(ctypes.byref(out))
    return out


def capabilities():
    return _lib.mi_client_get_capabilities()


def _check_abi():
    global _abi_checked
    if _abi_checked:
        return
    ver = sdk_version()
    if ver.abi != MI_E2EE_SDK_ABI_VERSION:
        raise RuntimeError(
            f"mi_e2ee sdk abi mismatch: {ver.abi} != {MI_E2EE_SDK_ABI_VERSION}"
        )
    _abi_checked = True


def check_abi():
    _check_abi()


def last_create_error():
    return _decode_cstr(_lib.mi_client_last_create_error()) or ""


def _decode_cstr(value):
    if not value:
        return None
    if isinstance(value, bytes):
        return value.decode("utf-8", errors="replace")
    return ctypes.cast(value, ctypes.c_char_p).value.decode("utf-8", errors="replace")


def _copy_bytes(ptr, length):
    if not ptr or length == 0:
        return b""
    return ctypes.string_at(ptr, length)


def _encode_utf8(value):
    if value is None:
        return None
    if isinstance(value, bytes):
        return value
    return str(value).encode("utf-8")


def _as_bytes(value):
    if value is None:
        return None
    if isinstance(value, bytes):
        return value
    if isinstance(value, bytearray):
        return bytes(value)
    if isinstance(value, memoryview):
        return value.tobytes()
    return bytes(value)


def _as_u8_buffer(data):
    data = _as_bytes(data)
    if data is None:
        return None, 0
    if len(data) == 0:
        return None, 0
    buf = (ctypes.c_uint8 * len(data)).from_buffer_copy(data)
    return buf, len(data)


def _as_call_id(call_id):
    data = _as_bytes(call_id)
    if data is None or len(data) != MI_CALL_ID_LEN:
        raise ValueError("call_id must be 16 bytes")
    return (ctypes.c_uint8 * MI_CALL_ID_LEN).from_buffer_copy(data)


def _as_str_list(values):
    if not values:
        return None, 0
    arr = (ctypes.c_char_p * len(values))()
    for i, value in enumerate(values):
        arr[i] = _encode_utf8(value)
    return arr, len(values)


def _take_c_string(out_ptr):
    if not out_ptr:
        return ""
    value = _decode_cstr(out_ptr.value) or ""
    _lib.mi_client_free(ctypes.cast(out_ptr, ctypes.c_void_p))
    return value


class Client:
    def __init__(self, config_path=None):
        _check_abi()
        if config_path is None:
            c_path = None
        else:
            c_path = config_path.encode("utf-8")
        handle = _lib.mi_client_create(c_path)
        if not handle:
            err = last_create_error()
            raise RuntimeError(err or "mi_client_create failed")
        self._handle = handle
        self._progress_refs = []

    def close(self):
        if self._handle:
            _lib.mi_client_destroy(self._handle)
            self._handle = None

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()

    def _raise_on_error(self, ok):
        if ok == 0:
            err = self.last_error()
            raise RuntimeError(err or "operation failed")

    def _call_with_out_message(self, func, *args):
        out = ctypes.c_char_p()
        ok = func(self._handle, *args, ctypes.byref(out))
        value = _take_c_string(out)
        self._raise_on_error(ok)
        return value

    def last_error(self):
        if not self._handle:
            return ""
        return _decode_cstr(_lib.mi_client_last_error(self._handle)) or ""

    def token(self):
        return _decode_cstr(_lib.mi_client_token(self._handle)) or ""

    def device_id(self):
        return _decode_cstr(_lib.mi_client_device_id(self._handle)) or ""

    def remote_ok(self):
        return bool(_lib.mi_client_remote_ok(self._handle))

    def remote_error(self):
        return _decode_cstr(_lib.mi_client_remote_error(self._handle)) or ""

    def is_remote_mode(self):
        return bool(_lib.mi_client_is_remote_mode(self._handle))

    def relogin(self):
        ok = _lib.mi_client_relogin(self._handle)
        self._raise_on_error(ok)

    def heartbeat(self):
        ok = _lib.mi_client_heartbeat(self._handle)
        self._raise_on_error(ok)

    def has_pending_server_trust(self):
        return bool(_lib.mi_client_has_pending_server_trust(self._handle))

    def pending_server_fingerprint(self):
        return _decode_cstr(_lib.mi_client_pending_server_fingerprint(self._handle))

    def pending_server_pin(self):
        return _decode_cstr(_lib.mi_client_pending_server_pin(self._handle))

    def trust_pending_server(self, pin):
        ok = _lib.mi_client_trust_pending_server(self._handle, _encode_utf8(pin))
        self._raise_on_error(ok)

    def has_pending_peer_trust(self):
        return bool(_lib.mi_client_has_pending_peer_trust(self._handle))

    def pending_peer_username(self):
        return _decode_cstr(_lib.mi_client_pending_peer_username(self._handle))

    def pending_peer_fingerprint(self):
        return _decode_cstr(_lib.mi_client_pending_peer_fingerprint(self._handle))

    def pending_peer_pin(self):
        return _decode_cstr(_lib.mi_client_pending_peer_pin(self._handle))

    def trust_pending_peer(self, pin):
        ok = _lib.mi_client_trust_pending_peer(self._handle, _encode_utf8(pin))
        self._raise_on_error(ok)

    def register(self, username, password):
        ok = _lib.mi_client_register(self._handle,
                                     username.encode("utf-8"),
                                     password.encode("utf-8"))
        self._raise_on_error(ok)

    def login(self, username, password):
        ok = _lib.mi_client_login(self._handle,
                                  username.encode("utf-8"),
                                  password.encode("utf-8"))
        self._raise_on_error(ok)

    def logout(self):
        ok = _lib.mi_client_logout(self._handle)
        self._raise_on_error(ok)

    def add_friend(self, username, remark=""):
        remark_bytes = remark.encode("utf-8") if remark else None
        ok = _lib.mi_client_add_friend(self._handle,
                                       username.encode("utf-8"),
                                       remark_bytes)
        self._raise_on_error(ok)

    def delete_friend(self, username):
        ok = _lib.mi_client_delete_friend(self._handle, username.encode("utf-8"))
        self._raise_on_error(ok)

    def set_friend_remark(self, username, remark):
        ok = _lib.mi_client_set_friend_remark(self._handle,
                                              username.encode("utf-8"),
                                              _encode_utf8(remark))
        self._raise_on_error(ok)

    def set_user_blocked(self, username, blocked=True):
        ok = _lib.mi_client_set_user_blocked(self._handle,
                                             username.encode("utf-8"),
                                             1 if blocked else 0)
        self._raise_on_error(ok)

    def send_friend_request(self, username, remark=""):
        remark_bytes = remark.encode("utf-8") if remark else None
        ok = _lib.mi_client_send_friend_request(self._handle,
                                                username.encode("utf-8"),
                                                remark_bytes)
        self._raise_on_error(ok)

    def respond_friend_request(self, requester, accept):
        ok = _lib.mi_client_respond_friend_request(self._handle,
                                                   requester.encode("utf-8"),
                                                   1 if accept else 0)
        self._raise_on_error(ok)

    def list_friends(self, max_entries=MI_MAX_FRIEND_ENTRIES):
        if max_entries <= 0:
            return []
        buf = (MiFriendEntry * max_entries)()
        count = _lib.mi_client_list_friends(self._handle, buf, max_entries)
        friends = []
        for i in range(count):
            entry = buf[i]
            friends.append({
                "username": _decode_cstr(entry.username) or "",
                "remark": _decode_cstr(entry.remark) or "",
            })
        return friends

    def sync_friends(self, max_entries=MI_MAX_FRIEND_ENTRIES):
        if max_entries <= 0:
            return [], False
        buf = (MiFriendEntry * max_entries)()
        changed = ctypes.c_int()
        count = _lib.mi_client_sync_friends(self._handle, buf, max_entries, ctypes.byref(changed))
        err = self.last_error()
        if err:
            raise RuntimeError(err)
        friends = []
        for i in range(count):
            entry = buf[i]
            friends.append({
                "username": _decode_cstr(entry.username) or "",
                "remark": _decode_cstr(entry.remark) or "",
            })
        return friends, bool(changed.value)

    def list_friend_requests(self, max_entries=MI_MAX_FRIEND_REQUEST_ENTRIES):
        if max_entries <= 0:
            return []
        buf = (MiFriendRequestEntry * max_entries)()
        count = _lib.mi_client_list_friend_requests(self._handle, buf, max_entries)
        requests = []
        for i in range(count):
            entry = buf[i]
            requests.append({
                "requester_username": _decode_cstr(entry.requester_username) or "",
                "requester_remark": _decode_cstr(entry.requester_remark) or "",
            })
        return requests

    def list_devices(self, max_entries=MI_MAX_DEVICE_ENTRIES):
        if max_entries <= 0:
            return []
        buf = (MiDeviceEntry * max_entries)()
        count = _lib.mi_client_list_devices(self._handle, buf, max_entries)
        devices = []
        for i in range(count):
            entry = buf[i]
            devices.append({
                "device_id": _decode_cstr(entry.device_id) or "",
                "last_seen_sec": entry.last_seen_sec,
            })
        return devices

    def kick_device(self, device_id):
        ok = _lib.mi_client_kick_device(self._handle, _encode_utf8(device_id))
        self._raise_on_error(ok)

    def join_group(self, group_id):
        ok = _lib.mi_client_join_group(self._handle, _encode_utf8(group_id))
        self._raise_on_error(ok)

    def leave_group(self, group_id):
        ok = _lib.mi_client_leave_group(self._handle, _encode_utf8(group_id))
        self._raise_on_error(ok)

    def create_group(self):
        out = ctypes.c_char_p()
        ok = _lib.mi_client_create_group(self._handle, ctypes.byref(out))
        group_id = _take_c_string(out)
        self._raise_on_error(ok)
        return group_id

    def send_group_invite(self, group_id, peer_username):
        return self._call_with_out_message(
            _lib.mi_client_send_group_invite,
            _encode_utf8(group_id),
            _encode_utf8(peer_username),
        )

    def list_group_members(self, group_id, max_entries=MI_MAX_GROUP_MEMBER_ENTRIES):
        if max_entries <= 0:
            return []
        buf = (MiGroupMemberEntry * max_entries)()
        count = _lib.mi_client_list_group_members_info(self._handle,
                                                       _encode_utf8(group_id),
                                                       buf,
                                                       max_entries)
        members = []
        for i in range(count):
            entry = buf[i]
            members.append({
                "username": _decode_cstr(entry.username) or "",
                "role": entry.role,
            })
        return members

    def set_group_member_role(self, group_id, peer_username, role):
        ok = _lib.mi_client_set_group_member_role(self._handle,
                                                  _encode_utf8(group_id),
                                                  _encode_utf8(peer_username),
                                                  int(role))
        self._raise_on_error(ok)

    def kick_group_member(self, group_id, peer_username):
        ok = _lib.mi_client_kick_group_member(self._handle,
                                              _encode_utf8(group_id),
                                              _encode_utf8(peer_username))
        self._raise_on_error(ok)

    def start_group_call(self, group_id, video=False):
        out_call_id = (ctypes.c_uint8 * MI_CALL_ID_LEN)()
        out_key_id = ctypes.c_uint32()
        ok = _lib.mi_client_start_group_call(self._handle,
                                             _encode_utf8(group_id),
                                             1 if video else 0,
                                             out_call_id,
                                             MI_CALL_ID_LEN,
                                             ctypes.byref(out_key_id))
        self._raise_on_error(ok)
        return {
            "call_id": bytes(out_call_id),
            "key_id": out_key_id.value,
        }

    def join_group_call(self, group_id, call_id, video=False):
        call_id_buf = _as_call_id(call_id)
        out_key_id = ctypes.c_uint32()
        ok = _lib.mi_client_join_group_call(self._handle,
                                            _encode_utf8(group_id),
                                            call_id_buf,
                                            MI_CALL_ID_LEN,
                                            1 if video else 0,
                                            ctypes.byref(out_key_id))
        self._raise_on_error(ok)
        return out_key_id.value

    def leave_group_call(self, group_id, call_id):
        call_id_buf = _as_call_id(call_id)
        ok = _lib.mi_client_leave_group_call(self._handle,
                                             _encode_utf8(group_id),
                                             call_id_buf,
                                             MI_CALL_ID_LEN)
        self._raise_on_error(ok)

    def get_group_call_key(self, group_id, call_id, key_id, key_len=MI_GROUP_CALL_KEY_LEN):
        if key_len <= 0:
            raise ValueError("key_len must be positive")
        call_id_buf = _as_call_id(call_id)
        out_key = (ctypes.c_uint8 * key_len)()
        ok = _lib.mi_client_get_group_call_key(self._handle,
                                               _encode_utf8(group_id),
                                               call_id_buf,
                                               MI_CALL_ID_LEN,
                                               int(key_id),
                                               out_key,
                                               key_len)
        self._raise_on_error(ok)
        return bytes(out_key)

    def rotate_group_call_key(self, group_id, call_id, key_id, members=None):
        call_id_buf = _as_call_id(call_id)
        member_arr, member_count = _as_str_list(members)
        ok = _lib.mi_client_rotate_group_call_key(self._handle,
                                                  _encode_utf8(group_id),
                                                  call_id_buf,
                                                  MI_CALL_ID_LEN,
                                                  int(key_id),
                                                  member_arr,
                                                  member_count)
        self._raise_on_error(ok)

    def request_group_call_key(self, group_id, call_id, key_id, members=None):
        call_id_buf = _as_call_id(call_id)
        member_arr, member_count = _as_str_list(members)
        ok = _lib.mi_client_request_group_call_key(self._handle,
                                                   _encode_utf8(group_id),
                                                   call_id_buf,
                                                   MI_CALL_ID_LEN,
                                                   int(key_id),
                                                   member_arr,
                                                   member_count)
        self._raise_on_error(ok)

    def send_group_call_signal(self,
                               op,
                               group_id,
                               call_id,
                               video=False,
                               key_id=0,
                               seq=0,
                               ts_ms=0,
                               ext=None,
                               max_members=MI_MAX_GROUP_CALL_MEMBERS):
        if max_members <= 0:
            raise ValueError("max_members must be positive")
        call_id_buf = _as_call_id(call_id)
        ext_buf, ext_len = _as_u8_buffer(ext)
        out_call_id = (ctypes.c_uint8 * MI_CALL_ID_LEN)()
        out_key_id = ctypes.c_uint32()
        out_members = (MiGroupCallMember * max_members)()
        out_member_count = ctypes.c_uint32()
        ok = _lib.mi_client_send_group_call_signal(self._handle,
                                                   int(op),
                                                   _encode_utf8(group_id),
                                                   call_id_buf,
                                                   MI_CALL_ID_LEN,
                                                   1 if video else 0,
                                                   int(key_id),
                                                   int(seq),
                                                   int(ts_ms),
                                                   ext_buf,
                                                   ext_len,
                                                   out_call_id,
                                                   MI_CALL_ID_LEN,
                                                   ctypes.byref(out_key_id),
                                                   out_members,
                                                   max_members,
                                                   ctypes.byref(out_member_count))
        self._raise_on_error(ok)
        members = []
        for i in range(out_member_count.value):
            entry = out_members[i]
            members.append({
                "username": _decode_cstr(entry.username) or "",
            })
        return {
            "call_id": bytes(out_call_id),
            "key_id": out_key_id.value,
            "members": members,
        }

    def load_chat_history(self, conv_id, is_group=False, limit=0, max_entries=MI_MAX_HISTORY_ENTRIES):
        if max_entries <= 0:
            return []
        buf = (MiHistoryEntry * max_entries)()
        count = _lib.mi_client_load_chat_history(self._handle,
                                                 _encode_utf8(conv_id),
                                                 1 if is_group else 0,
                                                 int(limit),
                                                 buf,
                                                 max_entries)
        history = []
        for i in range(count):
            entry = buf[i]
            history.append({
                "kind": entry.kind,
                "status": entry.status,
                "is_group": bool(entry.is_group),
                "outgoing": bool(entry.outgoing),
                "timestamp_sec": entry.timestamp_sec,
                "conv_id": _decode_cstr(entry.conv_id),
                "sender": _decode_cstr(entry.sender),
                "message_id": _decode_cstr(entry.message_id),
                "text": _decode_cstr(entry.text),
                "file_id": _decode_cstr(entry.file_id),
                "file_key": _copy_bytes(entry.file_key, entry.file_key_len),
                "file_name": _decode_cstr(entry.file_name),
                "file_size": entry.file_size,
                "sticker_id": _decode_cstr(entry.sticker_id),
            })
        return history

    def delete_chat_history(self, conv_id, is_group=False, delete_attachments=False, secure_wipe=False):
        ok = _lib.mi_client_delete_chat_history(self._handle,
                                                _encode_utf8(conv_id),
                                                1 if is_group else 0,
                                                1 if delete_attachments else 0,
                                                1 if secure_wipe else 0)
        self._raise_on_error(ok)

    def set_history_enabled(self, enabled):
        ok = _lib.mi_client_set_history_enabled(self._handle, 1 if enabled else 0)
        self._raise_on_error(ok)

    def clear_all_history(self, delete_attachments=False, secure_wipe=False):
        ok = _lib.mi_client_clear_all_history(self._handle,
                                              1 if delete_attachments else 0,
                                              1 if secure_wipe else 0)
        self._raise_on_error(ok)

    def begin_device_pairing_primary(self):
        out = ctypes.c_char_p()
        ok = _lib.mi_client_begin_device_pairing_primary(self._handle, ctypes.byref(out))
        code = _take_c_string(out)
        self._raise_on_error(ok)
        return code

    def poll_device_pairing_requests(self, max_entries=MI_MAX_DEVICE_ENTRIES):
        if max_entries <= 0:
            return []
        buf = (MiDevicePairingRequest * max_entries)()
        count = _lib.mi_client_poll_device_pairing_requests(self._handle, buf, max_entries)
        requests = []
        for i in range(count):
            entry = buf[i]
            requests.append({
                "device_id": _decode_cstr(entry.device_id) or "",
                "request_id_hex": _decode_cstr(entry.request_id_hex) or "",
            })
        return requests

    def approve_device_pairing_request(self, device_id, request_id_hex):
        ok = _lib.mi_client_approve_device_pairing_request(self._handle,
                                                           _encode_utf8(device_id),
                                                           _encode_utf8(request_id_hex))
        self._raise_on_error(ok)

    def begin_device_pairing_linked(self, pairing_code):
        ok = _lib.mi_client_begin_device_pairing_linked(self._handle,
                                                        _encode_utf8(pairing_code))
        self._raise_on_error(ok)

    def poll_device_pairing_linked(self):
        completed = ctypes.c_int()
        ok = _lib.mi_client_poll_device_pairing_linked(self._handle,
                                                       ctypes.byref(completed))
        self._raise_on_error(ok)
        return bool(completed.value)

    def cancel_device_pairing(self):
        _lib.mi_client_cancel_device_pairing(self._handle)

    def store_attachment_preview_bytes(self, file_id, file_name, file_size, data):
        data_buf, data_len = _as_u8_buffer(data)
        ok = _lib.mi_client_store_attachment_preview_bytes(self._handle,
                                                           _encode_utf8(file_id),
                                                           _encode_utf8(file_name),
                                                           int(file_size),
                                                           data_buf,
                                                           data_len)
        self._raise_on_error(ok)

    def download_chat_file_to_path(self,
                                   file_id,
                                   file_key,
                                   file_name,
                                   file_size,
                                   out_path,
                                   wipe_after_read=False,
                                   on_progress=None,
                                   user_data=None):
        key_buf, key_len = _as_u8_buffer(file_key)
        if key_len == 0:
            raise ValueError("file_key is required")
        cb = None
        user_ptr = None
        if user_data is not None:
            if isinstance(user_data, ctypes.c_void_p):
                user_ptr = user_data
            elif isinstance(user_data, int):
                user_ptr = ctypes.c_void_p(user_data)
            else:
                raise TypeError("user_data must be int or ctypes.c_void_p")
        if on_progress:
            def _adapter(done, total, udata):
                try:
                    on_progress(done, total, udata)
                except TypeError:
                    on_progress(done, total)
            cb = MiProgressCallback(_adapter)
            self._progress_refs.append(cb)
        try:
            ok = _lib.mi_client_download_chat_file_to_path(self._handle,
                                                           _encode_utf8(file_id),
                                                           key_buf,
                                                           key_len,
                                                           _encode_utf8(file_name),
                                                           int(file_size),
                                                           _encode_utf8(out_path),
                                                           1 if wipe_after_read else 0,
                                                           cb,
                                                           user_ptr)
        finally:
            if cb:
                self._progress_refs.remove(cb)
        self._raise_on_error(ok)

    def download_chat_file_to_bytes(self,
                                    file_id,
                                    file_key,
                                    file_name,
                                    file_size,
                                    wipe_after_read=False):
        key_buf, key_len = _as_u8_buffer(file_key)
        if key_len == 0:
            raise ValueError("file_key is required")
        out_bytes = ctypes.POINTER(ctypes.c_uint8)()
        out_len = ctypes.c_uint64()
        ok = _lib.mi_client_download_chat_file_to_bytes(self._handle,
                                                        _encode_utf8(file_id),
                                                        key_buf,
                                                        key_len,
                                                        _encode_utf8(file_name),
                                                        int(file_size),
                                                        1 if wipe_after_read else 0,
                                                        ctypes.byref(out_bytes),
                                                        ctypes.byref(out_len))
        if ok == 0:
            if out_bytes:
                _lib.mi_client_free(ctypes.cast(out_bytes, ctypes.c_void_p))
            self._raise_on_error(ok)
        data = b""
        if out_bytes and out_len.value:
            data = ctypes.string_at(out_bytes, out_len.value)
        if out_bytes:
            _lib.mi_client_free(ctypes.cast(out_bytes, ctypes.c_void_p))
        return data

    def get_media_config(self):
        cfg = MiMediaConfig()
        ok = _lib.mi_client_get_media_config(self._handle, ctypes.byref(cfg))
        self._raise_on_error(ok)
        return {
            "audio_delay_ms": cfg.audio_delay_ms,
            "video_delay_ms": cfg.video_delay_ms,
            "audio_max_frames": cfg.audio_max_frames,
            "video_max_frames": cfg.video_max_frames,
            "pull_max_packets": cfg.pull_max_packets,
            "pull_wait_ms": cfg.pull_wait_ms,
            "group_pull_max_packets": cfg.group_pull_max_packets,
            "group_pull_wait_ms": cfg.group_pull_wait_ms,
        }

    def derive_media_root(self, peer_username, call_id):
        call_id_buf = _as_call_id(call_id)
        out_root = (ctypes.c_uint8 * MI_MEDIA_ROOT_LEN)()
        ok = _lib.mi_client_derive_media_root(self._handle,
                                              _encode_utf8(peer_username),
                                              call_id_buf,
                                              MI_CALL_ID_LEN,
                                              out_root,
                                              MI_MEDIA_ROOT_LEN)
        self._raise_on_error(ok)
        return bytes(out_root)

    def push_media(self, peer_username, call_id, packet):
        call_id_buf = _as_call_id(call_id)
        packet_buf, packet_len = _as_u8_buffer(packet)
        if packet_len == 0:
            raise ValueError("packet is required")
        ok = _lib.mi_client_push_media(self._handle,
                                       _encode_utf8(peer_username),
                                       call_id_buf,
                                       MI_CALL_ID_LEN,
                                       packet_buf,
                                       packet_len)
        self._raise_on_error(ok)

    def pull_media(self, call_id, max_packets=MI_MAX_MEDIA_PACKETS, wait_ms=0):
        if max_packets <= 0:
            return []
        call_id_buf = _as_call_id(call_id)
        buf = (MiMediaPacket * max_packets)()
        count = _lib.mi_client_pull_media(self._handle,
                                          call_id_buf,
                                          MI_CALL_ID_LEN,
                                          max_packets,
                                          int(wait_ms),
                                          buf)
        packets = []
        for i in range(count):
            entry = buf[i]
            packets.append({
                "sender": _decode_cstr(entry.sender),
                "payload": _copy_bytes(entry.payload, entry.payload_len),
            })
        return packets

    def push_group_media(self, group_id, call_id, packet):
        call_id_buf = _as_call_id(call_id)
        packet_buf, packet_len = _as_u8_buffer(packet)
        if packet_len == 0:
            raise ValueError("packet is required")
        ok = _lib.mi_client_push_group_media(self._handle,
                                             _encode_utf8(group_id),
                                             call_id_buf,
                                             MI_CALL_ID_LEN,
                                             packet_buf,
                                             packet_len)
        self._raise_on_error(ok)

    def pull_group_media(self, call_id, max_packets=MI_MAX_MEDIA_PACKETS, wait_ms=0):
        if max_packets <= 0:
            return []
        call_id_buf = _as_call_id(call_id)
        buf = (MiMediaPacket * max_packets)()
        count = _lib.mi_client_pull_group_media(self._handle,
                                                call_id_buf,
                                                MI_CALL_ID_LEN,
                                                max_packets,
                                                int(wait_ms),
                                                buf)
        packets = []
        for i in range(count):
            entry = buf[i]
            packets.append({
                "sender": _decode_cstr(entry.sender),
                "payload": _copy_bytes(entry.payload, entry.payload_len),
            })
        return packets

    def add_media_subscription(self, call_id, is_group=False, group_id=None):
        call_id_buf = _as_call_id(call_id)
        group = None
        if is_group:
            if not group_id:
                raise ValueError("group_id required for group subscription")
            group = _encode_utf8(group_id)
        ok = _lib.mi_client_add_media_subscription(self._handle,
                                                   call_id_buf,
                                                   MI_CALL_ID_LEN,
                                                   1 if is_group else 0,
                                                   group)
        self._raise_on_error(ok)

    def clear_media_subscriptions(self):
        _lib.mi_client_clear_media_subscriptions(self._handle)

    def send_private_text(self, peer, text):
        return self._call_with_out_message(
            _lib.mi_client_send_private_text,
            _encode_utf8(peer),
            _encode_utf8(text),
        )

    def send_private_text_with_reply(self,
                                     peer,
                                     text,
                                     reply_to_message_id,
                                     reply_preview):
        return self._call_with_out_message(
            _lib.mi_client_send_private_text_with_reply,
            _encode_utf8(peer),
            _encode_utf8(text),
            _encode_utf8(reply_to_message_id),
            _encode_utf8(reply_preview),
        )

    def resend_private_text(self, peer, message_id, text):
        ok = _lib.mi_client_resend_private_text(self._handle,
                                                _encode_utf8(peer),
                                                _encode_utf8(message_id),
                                                _encode_utf8(text))
        self._raise_on_error(ok)

    def resend_private_text_with_reply(self,
                                       peer,
                                       message_id,
                                       text,
                                       reply_to_message_id,
                                       reply_preview):
        ok = _lib.mi_client_resend_private_text_with_reply(self._handle,
                                                           _encode_utf8(peer),
                                                           _encode_utf8(message_id),
                                                           _encode_utf8(text),
                                                           _encode_utf8(reply_to_message_id),
                                                           _encode_utf8(reply_preview))
        self._raise_on_error(ok)

    def send_group_text(self, group_id, text):
        return self._call_with_out_message(
            _lib.mi_client_send_group_text,
            _encode_utf8(group_id),
            _encode_utf8(text),
        )

    def resend_group_text(self, group_id, message_id, text):
        ok = _lib.mi_client_resend_group_text(self._handle,
                                              _encode_utf8(group_id),
                                              _encode_utf8(message_id),
                                              _encode_utf8(text))
        self._raise_on_error(ok)

    def send_private_file(self, peer, file_path):
        return self._call_with_out_message(
            _lib.mi_client_send_private_file,
            _encode_utf8(peer),
            _encode_utf8(file_path),
        )

    def resend_private_file(self, peer, message_id, file_path):
        ok = _lib.mi_client_resend_private_file(self._handle,
                                                _encode_utf8(peer),
                                                _encode_utf8(message_id),
                                                _encode_utf8(file_path))
        self._raise_on_error(ok)

    def send_group_file(self, group_id, file_path):
        return self._call_with_out_message(
            _lib.mi_client_send_group_file,
            _encode_utf8(group_id),
            _encode_utf8(file_path),
        )

    def resend_group_file(self, group_id, message_id, file_path):
        ok = _lib.mi_client_resend_group_file(self._handle,
                                              _encode_utf8(group_id),
                                              _encode_utf8(message_id),
                                              _encode_utf8(file_path))
        self._raise_on_error(ok)

    def send_private_sticker(self, peer, sticker_id):
        return self._call_with_out_message(
            _lib.mi_client_send_private_sticker,
            _encode_utf8(peer),
            _encode_utf8(sticker_id),
        )

    def resend_private_sticker(self, peer, message_id, sticker_id):
        ok = _lib.mi_client_resend_private_sticker(self._handle,
                                                   _encode_utf8(peer),
                                                   _encode_utf8(message_id),
                                                   _encode_utf8(sticker_id))
        self._raise_on_error(ok)

    def send_private_location(self, peer, lat_e7, lon_e7, label=None):
        return self._call_with_out_message(
            _lib.mi_client_send_private_location,
            _encode_utf8(peer),
            int(lat_e7),
            int(lon_e7),
            _encode_utf8(label),
        )

    def resend_private_location(self, peer, message_id, lat_e7, lon_e7, label=None):
        ok = _lib.mi_client_resend_private_location(self._handle,
                                                    _encode_utf8(peer),
                                                    _encode_utf8(message_id),
                                                    int(lat_e7),
                                                    int(lon_e7),
                                                    _encode_utf8(label))
        self._raise_on_error(ok)

    def send_private_contact(self, peer, card_username, card_display=None):
        return self._call_with_out_message(
            _lib.mi_client_send_private_contact,
            _encode_utf8(peer),
            _encode_utf8(card_username),
            _encode_utf8(card_display),
        )

    def resend_private_contact(self, peer, message_id, card_username, card_display=None):
        ok = _lib.mi_client_resend_private_contact(self._handle,
                                                   _encode_utf8(peer),
                                                   _encode_utf8(message_id),
                                                   _encode_utf8(card_username),
                                                   _encode_utf8(card_display))
        self._raise_on_error(ok)

    def send_read_receipt(self, peer, message_id):
        ok = _lib.mi_client_send_read_receipt(self._handle,
                                              _encode_utf8(peer),
                                              _encode_utf8(message_id))
        self._raise_on_error(ok)

    def send_typing(self, peer, typing=True):
        ok = _lib.mi_client_send_typing(self._handle,
                                        _encode_utf8(peer),
                                        1 if typing else 0)
        self._raise_on_error(ok)

    def send_presence(self, peer, online=True):
        ok = _lib.mi_client_send_presence(self._handle,
                                          _encode_utf8(peer),
                                          1 if online else 0)
        self._raise_on_error(ok)

    def poll_events(self, max_events=32, wait_ms=0):
        if max_events <= 0:
            return []
        buf = (MiEvent * max_events)()
        count = _lib.mi_client_poll_event(self._handle, buf, max_events, wait_ms)
        events = []
        for i in range(count):
            ev = buf[i]
            events.append({
                "type": ev.type,
                "ts_ms": ev.ts_ms,
                "peer": _decode_cstr(ev.peer),
                "sender": _decode_cstr(ev.sender),
                "group_id": _decode_cstr(ev.group_id),
                "message_id": _decode_cstr(ev.message_id),
                "text": _decode_cstr(ev.text),
                "file_id": _decode_cstr(ev.file_id),
                "file_name": _decode_cstr(ev.file_name),
                "file_size": ev.file_size,
                "file_key": _copy_bytes(ev.file_key, ev.file_key_len),
                "sticker_id": _decode_cstr(ev.sticker_id),
                "notice_kind": ev.notice_kind,
                "actor": _decode_cstr(ev.actor),
                "target": _decode_cstr(ev.target),
                "role": ev.role,
                "typing": bool(ev.typing),
                "online": bool(ev.online),
                "call_id": bytes(ev.call_id),
                "call_key_id": ev.call_key_id,
                "call_op": ev.call_op,
                "call_media_flags": ev.call_media_flags,
                "payload": _copy_bytes(ev.payload, ev.payload_len),
            })
        return events
