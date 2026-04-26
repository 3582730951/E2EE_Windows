import 'dart:ffi';

import 'package:ffi/ffi.dart';

final class MiSdkVersion extends Struct {
  @Uint32()
  external int major;

  @Uint32()
  external int minor;

  @Uint32()
  external int patch;

  @Uint32()
  external int abi;
}

final class MiFriendEntry extends Struct {
  external Pointer<Utf8> username;
  external Pointer<Utf8> remark;
}

final class MiFriendRequestEntry extends Struct {
  external Pointer<Utf8> requesterUsername;
  external Pointer<Utf8> requesterRemark;
}

final class MiDeviceEntry extends Struct {
  external Pointer<Utf8> deviceId;
  external Pointer<Utf8> displayId;

  @Uint32()
  external int lastSeenSec;
}

final class MiDevicePairingRequest extends Struct {
  external Pointer<Utf8> deviceId;
  external Pointer<Utf8> displayId;
  external Pointer<Utf8> requestIdHex;
}

final class MiGroupMemberEntry extends Struct {
  external Pointer<Utf8> username;

  @Uint32()
  external int role;
}

final class MiGroupCallMember extends Struct {
  external Pointer<Utf8> username;
}

final class MiMediaPacket extends Struct {
  external Pointer<Utf8> sender;
  external Pointer<Uint8> payload;

  @Uint32()
  external int payloadLen;
}

final class MiMediaConfig extends Struct {
  @Uint32()
  external int audioDelayMs;

  @Uint32()
  external int videoDelayMs;

  @Uint32()
  external int audioMaxFrames;

  @Uint32()
  external int videoMaxFrames;

  @Uint32()
  external int pullMaxPackets;

  @Uint32()
  external int pullWaitMs;

  @Uint32()
  external int groupPullMaxPackets;

  @Uint32()
  external int groupPullWaitMs;
}

final class MiHistoryEntry extends Struct {
  @Uint32()
  external int kind;

  @Uint32()
  external int status;

  @Uint8()
  external int isGroup;

  @Uint8()
  external int outgoing;

  @Uint8()
  external int reserved0;

  @Uint8()
  external int reserved1;

  @Uint64()
  external int timestampSec;

  external Pointer<Utf8> convId;
  external Pointer<Utf8> sender;
  external Pointer<Utf8> messageId;
  external Pointer<Utf8> text;
  external Pointer<Utf8> fileId;
  external Pointer<Uint8> fileKey;

  @Uint32()
  external int fileKeyLen;

  external Pointer<Utf8> fileName;

  @Uint64()
  external int fileSize;

  external Pointer<Utf8> stickerId;
  external Pointer<Uint8> canonicalEnvelope;

  @Uint32()
  external int canonicalEnvelopeLen;

  @Uint32()
  external int messageType;
}

final class MiEvent extends Struct {
  @Uint32()
  external int type;

  @Uint64()
  external int tsMs;

  external Pointer<Utf8> peer;
  external Pointer<Utf8> sender;
  external Pointer<Utf8> groupId;
  external Pointer<Utf8> messageId;
  external Pointer<Utf8> text;
  external Pointer<Utf8> fileId;
  external Pointer<Utf8> fileName;

  @Uint64()
  external int fileSize;

  external Pointer<Uint8> fileKey;

  @Uint32()
  external int fileKeyLen;

  external Pointer<Utf8> stickerId;

  @Uint32()
  external int noticeKind;

  external Pointer<Utf8> actor;
  external Pointer<Utf8> target;

  @Uint32()
  external int role;

  @Uint8()
  external int typing;

  @Uint8()
  external int online;

  @Uint8()
  external int reserved0;

  @Uint8()
  external int reserved1;

  @Array.multi([16])
  external Array<Uint8> callId;

  @Uint32()
  external int callKeyId;

  @Uint32()
  external int callOp;

  @Uint8()
  external int callMediaFlags;

  @Uint8()
  external int callReserved0;

  @Uint8()
  external int callReserved1;

  @Uint8()
  external int callReserved2;

  external Pointer<Uint8> payload;

  @Uint32()
  external int payloadLen;
}

typedef _MiClientGetVersionNative = Void Function(Pointer<MiSdkVersion>);
typedef _MiClientGetVersionDart = void Function(Pointer<MiSdkVersion>);

typedef _MiClientGetCapabilitiesNative = Uint32 Function();
typedef _MiClientGetCapabilitiesDart = int Function();

typedef _MiClientCreateNative = Pointer<Void> Function(Pointer<Utf8>);
typedef _MiClientCreateDart = Pointer<Void> Function(Pointer<Utf8>);

typedef _MiClientLastCreateErrorNative = Pointer<Utf8> Function();
typedef _MiClientLastCreateErrorDart = Pointer<Utf8> Function();

typedef _MiClientDestroyNative = Void Function(Pointer<Void>);
typedef _MiClientDestroyDart = void Function(Pointer<Void>);

typedef _MiClientLastErrorNative = Pointer<Utf8> Function(Pointer<Void>);
typedef _MiClientLastErrorDart = Pointer<Utf8> Function(Pointer<Void>);

typedef _MiClientRemoteErrorNative = Pointer<Utf8> Function(Pointer<Void>);
typedef _MiClientRemoteErrorDart = Pointer<Utf8> Function(Pointer<Void>);

typedef _MiClientDeviceIdNative = Pointer<Utf8> Function(Pointer<Void>);
typedef _MiClientDeviceIdDart = Pointer<Utf8> Function(Pointer<Void>);

typedef _MiClientDeviceDisplayIdNative = Pointer<Utf8> Function(Pointer<Void>);
typedef _MiClientDeviceDisplayIdDart = Pointer<Utf8> Function(Pointer<Void>);

typedef _MiClientLoginNative =
    Int32 Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);
typedef _MiClientLoginDart =
    int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);

typedef _MiClientLogoutNative = Int32 Function(Pointer<Void>);
typedef _MiClientLogoutDart = int Function(Pointer<Void>);

typedef _MiClientHeartbeatNative = Int32 Function(Pointer<Void>);
typedef _MiClientHeartbeatDart = int Function(Pointer<Void>);

typedef _MiClientSendPrivateTextNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );
typedef _MiClientSendPrivateTextDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );

typedef _MiClientSendGroupTextNative =
    Int32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );
typedef _MiClientSendGroupTextDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Pointer<Utf8>,
      Pointer<Pointer<Utf8>>,
    );

typedef _MiClientListFriendsNative =
    Uint32 Function(Pointer<Void>, Pointer<MiFriendEntry>, Uint32);
typedef _MiClientListFriendsDart =
    int Function(Pointer<Void>, Pointer<MiFriendEntry>, int);

typedef _MiClientListDevicesNative =
    Uint32 Function(Pointer<Void>, Pointer<MiDeviceEntry>, Uint32);
typedef _MiClientListDevicesDart =
    int Function(Pointer<Void>, Pointer<MiDeviceEntry>, int);

typedef _MiClientLoadChatHistoryNative =
    Uint32 Function(
      Pointer<Void>,
      Pointer<Utf8>,
      Int32,
      Uint32,
      Pointer<MiHistoryEntry>,
      Uint32,
    );
typedef _MiClientLoadChatHistoryDart =
    int Function(
      Pointer<Void>,
      Pointer<Utf8>,
      int,
      int,
      Pointer<MiHistoryEntry>,
      int,
    );

typedef _MiClientPollEventNative =
    Uint32 Function(Pointer<Void>, Pointer<MiEvent>, Uint32, Uint32);
typedef _MiClientPollEventDart =
    int Function(Pointer<Void>, Pointer<MiEvent>, int, int);

typedef _MiClientFreeNative = Void Function(Pointer<Void>);
typedef _MiClientFreeDart = void Function(Pointer<Void>);

class MiClientBindings {
  MiClientBindings(DynamicLibrary library)
    : getVersion = library
          .lookupFunction<_MiClientGetVersionNative, _MiClientGetVersionDart>(
            'mi_client_get_version',
          ),
      getCapabilities = library
          .lookupFunction<
            _MiClientGetCapabilitiesNative,
            _MiClientGetCapabilitiesDart
          >('mi_client_get_capabilities'),
      create = library
          .lookupFunction<_MiClientCreateNative, _MiClientCreateDart>(
            'mi_client_create',
          ),
      lastCreateError = library
          .lookupFunction<
            _MiClientLastCreateErrorNative,
            _MiClientLastCreateErrorDart
          >('mi_client_last_create_error'),
      destroy = library
          .lookupFunction<_MiClientDestroyNative, _MiClientDestroyDart>(
            'mi_client_destroy',
          ),
      lastError = library
          .lookupFunction<_MiClientLastErrorNative, _MiClientLastErrorDart>(
            'mi_client_last_error',
          ),
      remoteError = library
          .lookupFunction<_MiClientRemoteErrorNative, _MiClientRemoteErrorDart>(
            'mi_client_remote_error',
          ),
      deviceId = library
          .lookupFunction<_MiClientDeviceIdNative, _MiClientDeviceIdDart>(
            'mi_client_device_id',
          ),
      deviceDisplayId = library
          .lookupFunction<
            _MiClientDeviceDisplayIdNative,
            _MiClientDeviceDisplayIdDart
          >('mi_client_device_display_id'),
      login = library.lookupFunction<_MiClientLoginNative, _MiClientLoginDart>(
        'mi_client_login',
      ),
      logout = library
          .lookupFunction<_MiClientLogoutNative, _MiClientLogoutDart>(
            'mi_client_logout',
          ),
      heartbeat = library
          .lookupFunction<_MiClientHeartbeatNative, _MiClientHeartbeatDart>(
            'mi_client_heartbeat',
          ),
      sendPrivateText = library
          .lookupFunction<
            _MiClientSendPrivateTextNative,
            _MiClientSendPrivateTextDart
          >('mi_client_send_private_text'),
      sendGroupText = library
          .lookupFunction<
            _MiClientSendGroupTextNative,
            _MiClientSendGroupTextDart
          >('mi_client_send_group_text'),
      listFriends = library
          .lookupFunction<_MiClientListFriendsNative, _MiClientListFriendsDart>(
            'mi_client_list_friends',
          ),
      listDevices = library
          .lookupFunction<_MiClientListDevicesNative, _MiClientListDevicesDart>(
            'mi_client_list_devices',
          ),
      loadChatHistory = library
          .lookupFunction<
            _MiClientLoadChatHistoryNative,
            _MiClientLoadChatHistoryDart
          >('mi_client_load_chat_history'),
      pollEvent = library
          .lookupFunction<_MiClientPollEventNative, _MiClientPollEventDart>(
            'mi_client_poll_event',
          ),
      free = library.lookupFunction<_MiClientFreeNative, _MiClientFreeDart>(
        'mi_client_free',
      );

  final void Function(Pointer<MiSdkVersion>) getVersion;
  final int Function() getCapabilities;
  final Pointer<Void> Function(Pointer<Utf8>) create;
  final Pointer<Utf8> Function() lastCreateError;
  final void Function(Pointer<Void>) destroy;
  final Pointer<Utf8> Function(Pointer<Void>) lastError;
  final Pointer<Utf8> Function(Pointer<Void>) remoteError;
  final Pointer<Utf8> Function(Pointer<Void>) deviceId;
  final Pointer<Utf8> Function(Pointer<Void>) deviceDisplayId;
  final int Function(Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>) login;
  final int Function(Pointer<Void>) logout;
  final int Function(Pointer<Void>) heartbeat;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Pointer<Utf8>>,
  )
  sendPrivateText;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    Pointer<Utf8>,
    Pointer<Pointer<Utf8>>,
  )
  sendGroupText;
  final int Function(Pointer<Void>, Pointer<MiFriendEntry>, int) listFriends;
  final int Function(Pointer<Void>, Pointer<MiDeviceEntry>, int) listDevices;
  final int Function(
    Pointer<Void>,
    Pointer<Utf8>,
    int,
    int,
    Pointer<MiHistoryEntry>,
    int,
  )
  loadChatHistory;
  final int Function(Pointer<Void>, Pointer<MiEvent>, int, int) pollEvent;
  final void Function(Pointer<Void>) free;
}
