import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'sdk_ffi_bindings.dart';

typedef MiProgressCallbackNative =
    Void Function(Uint64 done, Uint64 total, Pointer<Void> userData);

typedef _HandleStringGetterNative = Pointer<Utf8> Function(Pointer<Void>);
typedef _HandleStringGetterDart = Pointer<Utf8> Function(Pointer<Void>);

typedef _HandleIntNative = Int32 Function(Pointer<Void>);
typedef _HandleIntDart = int Function(Pointer<Void>);

typedef _HandleVoidNative = Void Function(Pointer<Void>);
typedef _HandleVoidDart = void Function(Pointer<Void>);

typedef _HandleStringIntNative = Int32 Function(Pointer<Void>, Pointer<Utf8>);
typedef _HandleStringIntDart = int Function(Pointer<Void>, Pointer<Utf8>);

typedef _HandleStringStringIntNative =
    Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);
typedef _HandleStringStringIntDart =
    int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);

typedef _HandleStringStringStringIntNative =
    Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>);
typedef _HandleStringStringStringIntDart =
    int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>);

typedef _Handle4StringIntNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
    );
typedef _Handle4StringIntDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
    );

typedef _StringOutNative =
    Int32 Function(Pointer<Void>, Pointer<Pointer<Utf8>>);
typedef _StringOutDart = int Function(Pointer<Void>, Pointer<Pointer<Utf8>>);

typedef _HandleStringOutNative =
    Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Pointer<Utf8>>);
typedef _HandleStringOutDart =
    int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Pointer<Utf8>>);

typedef _HandleStringStringOutNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );
typedef _HandleStringStringOutDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );

typedef _HandleStringStringStringOutNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );
typedef _HandleStringStringStringOutDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );

typedef _SendWithReplyNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );
typedef _SendWithReplyDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );

typedef _ResendWithReplyNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
    );
typedef _ResendWithReplyDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Utf8>,
    );

typedef _SendLocationNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Int32,
      Int32,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );
typedef _SendLocationDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      int,
      int,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );

typedef _ResendLocationNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Int32,
      Int32,
      Pointer<Utf8>,
    );
typedef _ResendLocationDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      int,
      int,
      Pointer<Utf8>,
    );

typedef _ReadReceiptNative =
    Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);
typedef _ReadReceiptDart =
    int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);

typedef _PresenceNative = Int32 Function(Pointer<Void>, Pointer<Utf8>, Int32);
typedef _PresenceDart = int Function(Pointer<Void>, Pointer<Utf8>, int);

typedef _SetHistoryEnabledNative = Int32 Function(Pointer<Void>, Int32);
typedef _SetHistoryEnabledDart = int Function(Pointer<Void>, int);

typedef _ListFriendRequestsNative =
    Uint32 Function(Pointer<Void>, Pointer<MiFriendRequestEntry>, Uint32);
typedef _ListFriendRequestsDart =
    int Function(Pointer<Void>, Pointer<MiFriendRequestEntry>, int);

typedef _SyncFriendsNative =
    Uint32 Function(
      Pointer<Void>,
      Pointer<MiFriendEntry>,
      Uint32,
      Pointer<Int32>,
    );
typedef _SyncFriendsDart =
    int Function(Pointer<Void>, Pointer<MiFriendEntry>, int, Pointer<Int32>);

typedef _ListGroupMembersNative =
    Uint32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<MiGroupMemberEntry>,
      Uint32,
    );
typedef _ListGroupMembersDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<MiGroupMemberEntry>,
      int,
    );

typedef _SetGroupRoleNative =
    Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, Uint32);
typedef _SetGroupRoleDart =
    int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, int);

typedef _StartGroupCallNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Int32,
      Pointer<Uint8>,
      Uint32,
      Pointer<Uint32>,
    );
typedef _StartGroupCallDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      int,
      Pointer<Uint8>,
      int,
      Pointer<Uint32>,
    );

typedef _JoinGroupCallNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      Uint32,
      Int32,
      Pointer<Uint32>,
    );
typedef _JoinGroupCallDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      int,
      int,
      Pointer<Uint32>,
    );

typedef _CallIdActionNative =
    Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, Uint32);
typedef _CallIdActionDart =
    int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, int);

typedef _GetGroupCallKeyNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      Uint32,
      Uint32,
      Pointer<Uint8>,
      Uint32,
    );
typedef _GetGroupCallKeyDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      int,
      int,
      Pointer<Uint8>,
      int,
    );

typedef _GroupCallKeyRequestNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      Uint32,
      Uint32,
      Pointer<Pointer<Utf8>>,
      Uint32,
    );
typedef _GroupCallKeyRequestDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      int,
      int,
      Pointer<Pointer<Utf8>>,
      int,
    );

typedef _GroupCallSignalNative =
    Int32 Function(
      Pointer<Void>,
      Uint8,
      Pointer<Utf8>,
      Pointer<Uint8>,
      Uint32,
      Int32,
      Uint32,
      Uint32,
      Uint64,
      Pointer<Uint8>,
      Uint32,
      Pointer<Uint8>,
      Uint32,
      Pointer<Uint32>,
      Pointer<MiGroupCallMember>,
      Uint32,
      Pointer<Uint32>,
    );
typedef _GroupCallSignalDart =
    int Function(
      Pointer<Void>,
      int,
      Pointer<Utf8>,
      Pointer<Uint8>,
      int,
      int,
      int,
      int,
      int,
      Pointer<Uint8>,
      int,
      Pointer<Uint8>,
      int,
      Pointer<Uint32>,
      Pointer<MiGroupCallMember>,
      int,
      Pointer<Uint32>,
    );

typedef _HistorySnapshotNative =
    Uint32 Function(
      Pointer<Void>,
      Uint32,
      Uint32,
      Pointer<MiHistoryEntry>,
      Uint32,
    );
typedef _HistorySnapshotDart =
    int Function(Pointer<Void>, int, int, Pointer<MiHistoryEntry>, int);

typedef _DeleteHistoryNative =
    Int32 Function(Pointer<Void>, Pointer<Utf8>, Int32, Int32, Int32);
typedef _DeleteHistoryDart =
    int Function(Pointer<Void>, Pointer<Utf8>, int, int, int);

typedef _PairingRequestsNative =
    Uint32 Function(Pointer<Void>, Pointer<MiDevicePairingRequest>, Uint32);
typedef _PairingRequestsDart =
    int Function(Pointer<Void>, Pointer<MiDevicePairingRequest>, int);

typedef _ApprovePairingNative =
    Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);
typedef _ApprovePairingDart =
    int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);

typedef _PollLinkedPairingNative =
    Int32 Function(Pointer<Void>, Pointer<Int32>);
typedef _PollLinkedPairingDart = int Function(Pointer<Void>, Pointer<Int32>);

typedef _StorePreviewNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Uint64,
      Pointer<Uint8>,
      Uint32,
    );
typedef _StorePreviewDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      int,
      Pointer<Uint8>,
      int,
    );

typedef _DownloadToPathNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      Uint32,
      Pointer<Utf8>,
      Uint64,
      Pointer<Utf8>,
      Int32,
      Pointer<NativeFunction<MiProgressCallbackNative>>,
      Pointer<Void>,
    );
typedef _DownloadToPathDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      int,
      Pointer<Utf8>,
      int,
      Pointer<Utf8>,
      int,
      Pointer<NativeFunction<MiProgressCallbackNative>>,
      Pointer<Void>,
    );

typedef _DownloadToBytesNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      Uint32,
      Pointer<Utf8>,
      Uint64,
      Int32,
      Pointer<Pointer<Uint8>>,
      Pointer<Uint64>,
    );
typedef _DownloadToBytesDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      int,
      Pointer<Utf8>,
      int,
      int,
      Pointer<Pointer<Uint8>>,
      Pointer<Uint64>,
    );

typedef _MediaConfigNative =
    Int32 Function(Pointer<Void>, Pointer<MiMediaConfig>);
typedef _MediaConfigDart = int Function(Pointer<Void>, Pointer<MiMediaConfig>);

typedef _DeriveMediaRootNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      Uint32,
      Pointer<Uint8>,
      Uint32,
    );
typedef _DeriveMediaRootDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      int,
      Pointer<Uint8>,
      int,
    );

typedef _PushMediaNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      Uint32,
      Pointer<Uint8>,
      Uint32,
    );
typedef _PushMediaDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      int,
      Pointer<Uint8>,
      int,
    );

typedef _PullMediaNative =
    Uint32 Function(
      Pointer<Void>,
      Pointer<Uint8>,
      Uint32,
      Uint32,
      Uint32,
      Pointer<MiMediaPacket>,
    );
typedef _PullMediaDart =
    int Function(
      Pointer<Void>,
      Pointer<Uint8>,
      int,
      int,
      int,
      Pointer<MiMediaPacket>,
    );

typedef _PushGroupMediaNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      Uint32,
      Pointer<Uint8>,
      Uint32,
    );
typedef _PushGroupMediaDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Uint8>,
      int,
      Pointer<Uint8>,
      int,
    );

typedef _AddMediaSubscriptionNative =
    Int32 Function(Pointer<Void>, Pointer<Uint8>, Uint32, Int32, Pointer<Utf8>);
typedef _AddMediaSubscriptionDart =
    int Function(Pointer<Void>, Pointer<Uint8>, int, int, Pointer<Utf8>);

class MiClientExtraBindings {
  MiClientExtraBindings(DynamicLibrary library)
    : token = library
          .lookupFunction<_HandleStringGetterNative, _HandleStringGetterDart>(
            'mi_client_token',
          ),
      remoteOk = library.lookupFunction<_HandleIntNative, _HandleIntDart>(
        'mi_client_remote_ok',
      ),
      isRemoteMode = library.lookupFunction<_HandleIntNative, _HandleIntDart>(
        'mi_client_is_remote_mode',
      ),
      relogin = library.lookupFunction<_HandleIntNative, _HandleIntDart>(
        'mi_client_relogin',
      ),
      hasPendingServerTrust = library
          .lookupFunction<_HandleIntNative, _HandleIntDart>(
            'mi_client_has_pending_server_trust',
          ),
      pendingServerFingerprint = library
          .lookupFunction<_HandleStringGetterNative, _HandleStringGetterDart>(
            'mi_client_pending_server_fingerprint',
          ),
      pendingServerPin = library
          .lookupFunction<_HandleStringGetterNative, _HandleStringGetterDart>(
            'mi_client_pending_server_pin',
          ),
      trustPendingServer = library
          .lookupFunction<_HandleStringIntNative, _HandleStringIntDart>(
            'mi_client_trust_pending_server',
          ),
      hasPendingPeerTrust = library
          .lookupFunction<_HandleIntNative, _HandleIntDart>(
            'mi_client_has_pending_peer_trust',
          ),
      pendingPeerUsername = library
          .lookupFunction<_HandleStringGetterNative, _HandleStringGetterDart>(
            'mi_client_pending_peer_username',
          ),
      pendingPeerFingerprint = library
          .lookupFunction<_HandleStringGetterNative, _HandleStringGetterDart>(
            'mi_client_pending_peer_fingerprint',
          ),
      pendingPeerPin = library
          .lookupFunction<_HandleStringGetterNative, _HandleStringGetterDart>(
            'mi_client_pending_peer_pin',
          ),
      trustPendingPeer = library
          .lookupFunction<_HandleStringIntNative, _HandleStringIntDart>(
            'mi_client_trust_pending_peer',
          ),
      register = library
          .lookupFunction<
            _HandleStringStringIntNative,
            _HandleStringStringIntDart
          >('mi_client_register'),
      loginWithRootCode = library
          .lookupFunction<
            _HandleStringStringStringIntNative,
            _HandleStringStringStringIntDart
          >('mi_client_login_with_root_code'),
      registerDevice = library
          .lookupFunction<_HandleStringIntNative, _HandleStringIntDart>(
            'mi_client_register_device',
          ),
      rootAuthInit = library
          .lookupFunction<_HandleStringIntNative, _HandleStringIntDart>(
            'mi_client_root_auth_init',
          ),
      beginQrLoginWithUsername = library
          .lookupFunction<_HandleStringOutNative, _HandleStringOutDart>(
            'mi_client_begin_qr_login_with_username',
          ),
      beginQrLogin = library.lookupFunction<_StringOutNative, _StringOutDart>(
        'mi_client_begin_qr_login',
      ),
      pollQrLogin = library
          .lookupFunction<
            Int32 Function(
              Pointer<Void>,
              Pointer<Int32>,
              Pointer<Pointer<Utf8>>,
            ),
            int Function(Pointer<Void>, Pointer<Int32>, Pointer<Pointer<Utf8>>)
          >('mi_client_poll_qr_login'),
      approveQrLogin = library
          .lookupFunction<
            _HandleStringStringIntNative,
            _HandleStringStringIntDart
          >('mi_client_approve_qr_login'),
      approveQrLoginWithRootCode = library
          .lookupFunction<
            _HandleStringStringStringIntNative,
            _HandleStringStringStringIntDart
          >('mi_client_approve_qr_login_with_root_code'),
      cancelQrLogin = library
          .lookupFunction<_HandleVoidNative, _HandleVoidDart>(
            'mi_client_cancel_qr_login',
          ),
      publishPrekeys = library.lookupFunction<_HandleIntNative, _HandleIntDart>(
        'mi_client_publish_prekeys',
      ),
      sendPrivateTextWithReply = library
          .lookupFunction<_SendWithReplyNative, _SendWithReplyDart>(
            'mi_client_send_private_text_with_reply',
          ),
      resendPrivateText = library
          .lookupFunction<
            _HandleStringStringStringIntNative,
            _HandleStringStringStringIntDart
          >('mi_client_resend_private_text'),
      resendPrivateTextWithReply = library
          .lookupFunction<_ResendWithReplyNative, _ResendWithReplyDart>(
            'mi_client_resend_private_text_with_reply',
          ),
      resendGroupText = library
          .lookupFunction<
            _HandleStringStringStringIntNative,
            _HandleStringStringStringIntDart
          >('mi_client_resend_group_text'),
      sendPrivateFile = library
          .lookupFunction<
            _HandleStringStringOutNative,
            _HandleStringStringOutDart
          >('mi_client_send_private_file'),
      resendPrivateFile = library
          .lookupFunction<
            _HandleStringStringStringIntNative,
            _HandleStringStringStringIntDart
          >('mi_client_resend_private_file'),
      sendGroupFile = library
          .lookupFunction<
            _HandleStringStringOutNative,
            _HandleStringStringOutDart
          >('mi_client_send_group_file'),
      resendGroupFile = library
          .lookupFunction<
            _HandleStringStringStringIntNative,
            _HandleStringStringStringIntDart
          >('mi_client_resend_group_file'),
      sendPrivateSticker = library
          .lookupFunction<
            _HandleStringStringOutNative,
            _HandleStringStringOutDart
          >('mi_client_send_private_sticker'),
      resendPrivateSticker = library
          .lookupFunction<
            _HandleStringStringStringIntNative,
            _HandleStringStringStringIntDart
          >('mi_client_resend_private_sticker'),
      sendPrivateLocation = library
          .lookupFunction<_SendLocationNative, _SendLocationDart>(
            'mi_client_send_private_location',
          ),
      resendPrivateLocation = library
          .lookupFunction<_ResendLocationNative, _ResendLocationDart>(
            'mi_client_resend_private_location',
          ),
      sendPrivateContact = library
          .lookupFunction<
            _HandleStringStringStringOutNative,
            _HandleStringStringStringOutDart
          >('mi_client_send_private_contact'),
      resendPrivateContact = library
          .lookupFunction<_Handle4StringIntNative, _Handle4StringIntDart>(
            'mi_client_resend_private_contact',
          ),
      sendReadReceipt = library
          .lookupFunction<_ReadReceiptNative, _ReadReceiptDart>(
            'mi_client_send_read_receipt',
          ),
      sendTyping = library.lookupFunction<_PresenceNative, _PresenceDart>(
        'mi_client_send_typing',
      ),
      sendPresence = library.lookupFunction<_PresenceNative, _PresenceDart>(
        'mi_client_send_presence',
      ),
      addFriend = library
          .lookupFunction<
            _HandleStringStringIntNative,
            _HandleStringStringIntDart
          >('mi_client_add_friend'),
      setFriendRemark = library
          .lookupFunction<
            _HandleStringStringIntNative,
            _HandleStringStringIntDart
          >('mi_client_set_friend_remark'),
      deleteFriend = library
          .lookupFunction<_HandleStringIntNative, _HandleStringIntDart>(
            'mi_client_delete_friend',
          ),
      setUserBlocked = library.lookupFunction<_PresenceNative, _PresenceDart>(
        'mi_client_set_user_blocked',
      ),
      sendFriendRequest = library
          .lookupFunction<
            _HandleStringStringIntNative,
            _HandleStringStringIntDart
          >('mi_client_send_friend_request'),
      respondFriendRequest = library
          .lookupFunction<_PresenceNative, _PresenceDart>(
            'mi_client_respond_friend_request',
          ),
      syncFriends = library
          .lookupFunction<_SyncFriendsNative, _SyncFriendsDart>(
            'mi_client_sync_friends',
          ),
      listFriendRequests = library
          .lookupFunction<_ListFriendRequestsNative, _ListFriendRequestsDart>(
            'mi_client_list_friend_requests',
          ),
      kickDevice = library
          .lookupFunction<_HandleStringIntNative, _HandleStringIntDart>(
            'mi_client_kick_device',
          ),
      joinGroup = library
          .lookupFunction<_HandleStringIntNative, _HandleStringIntDart>(
            'mi_client_join_group',
          ),
      leaveGroup = library
          .lookupFunction<_HandleStringIntNative, _HandleStringIntDart>(
            'mi_client_leave_group',
          ),
      createGroup = library.lookupFunction<_StringOutNative, _StringOutDart>(
        'mi_client_create_group',
      ),
      sendGroupInvite = library
          .lookupFunction<
            _HandleStringStringOutNative,
            _HandleStringStringOutDart
          >('mi_client_send_group_invite'),
      listGroupMembersInfo = library
          .lookupFunction<_ListGroupMembersNative, _ListGroupMembersDart>(
            'mi_client_list_group_members_info',
          ),
      setGroupMemberRole = library
          .lookupFunction<_SetGroupRoleNative, _SetGroupRoleDart>(
            'mi_client_set_group_member_role',
          ),
      kickGroupMember = library
          .lookupFunction<
            _HandleStringStringIntNative,
            _HandleStringStringIntDart
          >('mi_client_kick_group_member'),
      startGroupCall = library
          .lookupFunction<_StartGroupCallNative, _StartGroupCallDart>(
            'mi_client_start_group_call',
          ),
      joinGroupCall = library
          .lookupFunction<_JoinGroupCallNative, _JoinGroupCallDart>(
            'mi_client_join_group_call',
          ),
      leaveGroupCall = library
          .lookupFunction<_CallIdActionNative, _CallIdActionDart>(
            'mi_client_leave_group_call',
          ),
      getGroupCallKey = library
          .lookupFunction<_GetGroupCallKeyNative, _GetGroupCallKeyDart>(
            'mi_client_get_group_call_key',
          ),
      rotateGroupCallKey = library
          .lookupFunction<_GroupCallKeyRequestNative, _GroupCallKeyRequestDart>(
            'mi_client_rotate_group_call_key',
          ),
      requestGroupCallKey = library
          .lookupFunction<_GroupCallKeyRequestNative, _GroupCallKeyRequestDart>(
            'mi_client_request_group_call_key',
          ),
      sendGroupCallSignal = library
          .lookupFunction<_GroupCallSignalNative, _GroupCallSignalDart>(
            'mi_client_send_group_call_signal',
          ),
      exportRecentHistorySnapshot = library
          .lookupFunction<_HistorySnapshotNative, _HistorySnapshotDart>(
            'mi_client_export_recent_history_snapshot',
          ),
      deleteChatHistory = library
          .lookupFunction<_DeleteHistoryNative, _DeleteHistoryDart>(
            'mi_client_delete_chat_history',
          ),
      setHistoryEnabled = library
          .lookupFunction<_SetHistoryEnabledNative, _SetHistoryEnabledDart>(
            'mi_client_set_history_enabled',
          ),
      clearAllHistory = library
          .lookupFunction<
            Int32 Function(Pointer<Void>, Int32, Int32),
            int Function(Pointer<Void>, int, int)
          >('mi_client_clear_all_history'),
      beginDevicePairingPrimary = library
          .lookupFunction<_StringOutNative, _StringOutDart>(
            'mi_client_begin_device_pairing_primary',
          ),
      pollDevicePairingRequests = library
          .lookupFunction<_PairingRequestsNative, _PairingRequestsDart>(
            'mi_client_poll_device_pairing_requests',
          ),
      approveDevicePairingRequest = library
          .lookupFunction<_ApprovePairingNative, _ApprovePairingDart>(
            'mi_client_approve_device_pairing_request',
          ),
      beginDevicePairingLinked = library
          .lookupFunction<_HandleStringIntNative, _HandleStringIntDart>(
            'mi_client_begin_device_pairing_linked',
          ),
      pollDevicePairingLinked = library
          .lookupFunction<_PollLinkedPairingNative, _PollLinkedPairingDart>(
            'mi_client_poll_device_pairing_linked',
          ),
      cancelDevicePairing = library
          .lookupFunction<_HandleVoidNative, _HandleVoidDart>(
            'mi_client_cancel_device_pairing',
          ),
      storeAttachmentPreviewBytes = library
          .lookupFunction<_StorePreviewNative, _StorePreviewDart>(
            'mi_client_store_attachment_preview_bytes',
          ),
      downloadChatFileToPath = library
          .lookupFunction<_DownloadToPathNative, _DownloadToPathDart>(
            'mi_client_download_chat_file_to_path',
          ),
      downloadChatFileToBytes = library
          .lookupFunction<_DownloadToBytesNative, _DownloadToBytesDart>(
            'mi_client_download_chat_file_to_bytes',
          ),
      getMediaConfig = library
          .lookupFunction<_MediaConfigNative, _MediaConfigDart>(
            'mi_client_get_media_config',
          ),
      deriveMediaRoot = library
          .lookupFunction<_DeriveMediaRootNative, _DeriveMediaRootDart>(
            'mi_client_derive_media_root',
          ),
      pushMedia = library.lookupFunction<_PushMediaNative, _PushMediaDart>(
        'mi_client_push_media',
      ),
      pullMedia = library.lookupFunction<_PullMediaNative, _PullMediaDart>(
        'mi_client_pull_media',
      ),
      pushGroupMedia = library
          .lookupFunction<_PushGroupMediaNative, _PushGroupMediaDart>(
            'mi_client_push_group_media',
          ),
      pullGroupMedia = library.lookupFunction<_PullMediaNative, _PullMediaDart>(
        'mi_client_pull_group_media',
      ),
      addMediaSubscription = library
          .lookupFunction<
            _AddMediaSubscriptionNative,
            _AddMediaSubscriptionDart
          >('mi_client_add_media_subscription'),
      clearMediaSubscriptions = library
          .lookupFunction<_HandleVoidNative, _HandleVoidDart>(
            'mi_client_clear_media_subscriptions',
          );

  final Pointer<Utf8> Function(Pointer<Void>) token;
  final int Function(Pointer<Void>) remoteOk;
  final int Function(Pointer<Void>) isRemoteMode;
  final int Function(Pointer<Void>) relogin;
  final int Function(Pointer<Void>) hasPendingServerTrust;
  final Pointer<Utf8> Function(Pointer<Void>) pendingServerFingerprint;
  final Pointer<Utf8> Function(Pointer<Void>) pendingServerPin;
  final int Function(Pointer<Void>, Pointer<Utf8>) trustPendingServer;
  final int Function(Pointer<Void>) hasPendingPeerTrust;
  final Pointer<Utf8> Function(Pointer<Void>) pendingPeerUsername;
  final Pointer<Utf8> Function(Pointer<Void>) pendingPeerFingerprint;
  final Pointer<Utf8> Function(Pointer<Void>) pendingPeerPin;
  final int Function(Pointer<Void>, Pointer<Utf8>) trustPendingPeer;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>) register;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>)
  loginWithRootCode;
  final int Function(Pointer<Void>, Pointer<Utf8>) registerDevice;
  final int Function(Pointer<Void>, Pointer<Utf8>) rootAuthInit;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Pointer<Utf8>>)
  beginQrLoginWithUsername;
  final int Function(Pointer<Void>, Pointer<Pointer<Utf8>>) beginQrLogin;
  final int Function(Pointer<Void>, Pointer<Int32>, Pointer<Pointer<Utf8>>)
  pollQrLogin;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
  approveQrLogin;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>)
  approveQrLoginWithRootCode;
  final void Function(Pointer<Void>) cancelQrLogin;
  final int Function(Pointer<Void>) publishPrekeys;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Pointer<Utf8>>,
  )
  sendPrivateTextWithReply;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>)
  resendPrivateText;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Utf8>,
  )
  resendPrivateTextWithReply;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>)
  resendGroupText;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Pointer<Utf8>>,
  )
  sendPrivateFile;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>)
  resendPrivateFile;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Pointer<Utf8>>,
  )
  sendGroupFile;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>)
  resendGroupFile;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Pointer<Utf8>>,
  )
  sendPrivateSticker;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>)
  resendPrivateSticker;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    int,
    int,
    Pointer<Utf8>,
    Pointer<Pointer<Utf8>>,
  )
  sendPrivateLocation;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    int,
    int,
    Pointer<Utf8>,
  )
  resendPrivateLocation;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Pointer<Utf8>>,
  )
  sendPrivateContact;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Utf8>,
  )
  resendPrivateContact;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
  sendReadReceipt;
  final int Function(Pointer<Void>, Pointer<Utf8>, int) sendTyping;
  final int Function(Pointer<Void>, Pointer<Utf8>, int) sendPresence;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>) addFriend;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
  setFriendRemark;
  final int Function(Pointer<Void>, Pointer<Utf8>) deleteFriend;
  final int Function(Pointer<Void>, Pointer<Utf8>, int) setUserBlocked;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
  sendFriendRequest;
  final int Function(Pointer<Void>, Pointer<Utf8>, int) respondFriendRequest;
  final int Function(Pointer<Void>, Pointer<MiFriendEntry>, int, Pointer<Int32>)
  syncFriends;
  final int Function(Pointer<Void>, Pointer<MiFriendRequestEntry>, int)
  listFriendRequests;
  final int Function(Pointer<Void>, Pointer<Utf8>) kickDevice;
  final int Function(Pointer<Void>, Pointer<Utf8>) joinGroup;
  final int Function(Pointer<Void>, Pointer<Utf8>) leaveGroup;
  final int Function(Pointer<Void>, Pointer<Pointer<Utf8>>) createGroup;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Pointer<Utf8>>,
  )
  sendGroupInvite;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<MiGroupMemberEntry>,
    int,
  )
  listGroupMembersInfo;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>, int)
  setGroupMemberRole;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
  kickGroupMember;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    int,
    Pointer<Uint8>,
    int,
    Pointer<Uint32>,
  )
  startGroupCall;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Uint8>,
    int,
    int,
    Pointer<Uint32>,
  )
  joinGroupCall;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Uint8>, int)
  leaveGroupCall;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Uint8>,
    int,
    int,
    Pointer<Uint8>,
    int,
  )
  getGroupCallKey;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Uint8>,
    int,
    int,
    Pointer<Pointer<Utf8>>,
    int,
  )
  rotateGroupCallKey;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Uint8>,
    int,
    int,
    Pointer<Pointer<Utf8>>,
    int,
  )
  requestGroupCallKey;
  final int Function(
    Pointer<Void>,
    int,
    Pointer<Utf8>,
    Pointer<Uint8>,
    int,
    int,
    int,
    int,
    int,
    Pointer<Uint8>,
    int,
    Pointer<Uint8>,
    int,
    Pointer<Uint32>,
    Pointer<MiGroupCallMember>,
    int,
    Pointer<Uint32>,
  )
  sendGroupCallSignal;
  final int Function(Pointer<Void>, int, int, Pointer<MiHistoryEntry>, int)
  exportRecentHistorySnapshot;
  final int Function(Pointer<Void>, Pointer<Utf8>, int, int, int)
  deleteChatHistory;
  final int Function(Pointer<Void>, int) setHistoryEnabled;
  final int Function(Pointer<Void>, int, int) clearAllHistory;
  final int Function(Pointer<Void>, Pointer<Pointer<Utf8>>)
  beginDevicePairingPrimary;
  final int Function(Pointer<Void>, Pointer<MiDevicePairingRequest>, int)
  pollDevicePairingRequests;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>)
  approveDevicePairingRequest;
  final int Function(Pointer<Void>, Pointer<Utf8>) beginDevicePairingLinked;
  final int Function(Pointer<Void>, Pointer<Int32>) pollDevicePairingLinked;
  final void Function(Pointer<Void>) cancelDevicePairing;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    int,
    Pointer<Uint8>,
    int,
  )
  storeAttachmentPreviewBytes;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Uint8>,
    int,
    Pointer<Utf8>,
    int,
    Pointer<Utf8>,
    int,
    Pointer<NativeFunction<MiProgressCallbackNative>>,
    Pointer<Void>,
  )
  downloadChatFileToPath;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Uint8>,
    int,
    Pointer<Utf8>,
    int,
    int,
    Pointer<Pointer<Uint8>>,
    Pointer<Uint64>,
  )
  downloadChatFileToBytes;
  final int Function(Pointer<Void>, Pointer<MiMediaConfig>) getMediaConfig;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Uint8>,
    int,
    Pointer<Uint8>,
    int,
  )
  deriveMediaRoot;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Uint8>,
    int,
    Pointer<Uint8>,
    int,
  )
  pushMedia;
  final int Function(
    Pointer<Void>,
    Pointer<Uint8>,
    int,
    int,
    int,
    Pointer<MiMediaPacket>,
  )
  pullMedia;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Uint8>,
    int,
    Pointer<Uint8>,
    int,
  )
  pushGroupMedia;
  final int Function(
    Pointer<Void>,
    Pointer<Uint8>,
    int,
    int,
    int,
    Pointer<MiMediaPacket>,
  )
  pullGroupMedia;
  final int Function(Pointer<Void>, Pointer<Uint8>, int, int, Pointer<Utf8>)
  addMediaSubscription;
  final void Function(Pointer<Void>) clearMediaSubscriptions;
}
