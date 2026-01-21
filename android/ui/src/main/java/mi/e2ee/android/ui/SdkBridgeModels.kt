package mi.e2ee.android.ui

data class FriendUi(val username: String, val remark: String, val status: String)

data class FriendRequestUi(val username: String, val remark: String)

data class GroupUi(val id: String, val name: String)

data class GroupMemberUi(val username: String, val role: Int)

data class DeviceUi(val deviceId: String, val lastSeenSec: Int)

data class PairingRequestUi(val deviceId: String, val requestId: String)

data class MediaRelayLog(
    val timestampMs: Long,
    val isGroup: Boolean,
    val peer: String,
    val sender: String,
    val groupId: String,
    val payloadSize: Int,
    val payloadPreview: String
)

data class OfflinePayloadLog(
    val timestampMs: Long,
    val peer: String,
    val sender: String,
    val groupId: String,
    val payloadSize: Int,
    val payloadPreview: String
)

data class GroupCallRoomUi(val groupId: String, val callId: String, val video: Boolean)

data class IncomingCall(
    val peerUsername: String,
    val callId: ByteArray,
    val callIdHex: String,
    val video: Boolean
)

data class PeerCallState(
    val peerUsername: String,
    val callId: ByteArray,
    val callIdHex: String,
    val video: Boolean,
    val initiator: Boolean
)

data class GroupCallState(
    val groupId: String,
    val callId: ByteArray,
    val callIdHex: String,
    val keyId: Int,
    val video: Boolean,
    val owner: Boolean,
    val keyReady: Boolean
)
