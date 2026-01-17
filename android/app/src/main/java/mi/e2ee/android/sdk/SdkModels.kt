package mi.e2ee.android.sdk

data class SdkVersion(
    val major: Int,
    val minor: Int,
    val patch: Int,
    val abi: Int
)

data class FriendEntry(
    val username: String,
    val remark: String
)

data class FriendRequestEntry(
    val requesterUsername: String,
    val requesterRemark: String
)

data class DeviceEntry(
    val deviceId: String,
    val lastSeenSec: Int
)

data class GroupMemberEntry(
    val username: String,
    val role: Int
)

data class GroupCallMember(
    val username: String
)

data class DevicePairingRequest(
    val deviceId: String,
    val requestIdHex: String
)

data class MediaPacket(
    val sender: String,
    val payload: ByteArray
)

data class MediaVideoFrame(
    val width: Int,
    val height: Int,
    val stride: Int,
    val keyframe: Boolean,
    val data: ByteArray
)

data class MediaConfig(
    val audioDelayMs: Int,
    val videoDelayMs: Int,
    val audioMaxFrames: Int,
    val videoMaxFrames: Int,
    val pullMaxPackets: Int,
    val pullWaitMs: Int,
    val groupPullMaxPackets: Int,
    val groupPullWaitMs: Int
)

data class HistoryEntry(
    val kind: Int,
    val status: Int,
    val isGroup: Boolean,
    val outgoing: Boolean,
    val timestampSec: Long,
    val convId: String,
    val sender: String,
    val messageId: String,
    val text: String,
    val fileId: String,
    val fileKey: ByteArray,
    val fileName: String,
    val fileSize: Long,
    val stickerId: String
)

data class GroupCallInfo(
    val callId: ByteArray,
    val keyId: Int
)

data class GroupCallSignalResult(
    val callId: ByteArray,
    val keyId: Int,
    val members: Array<GroupCallMember>
)

data class SyncFriendsResult(
    val changed: Boolean,
    val entries: Array<FriendEntry>
)

data class SdkEvent(
    val type: Int,
    val tsMs: Long,
    val peer: String,
    val sender: String,
    val groupId: String,
    val messageId: String,
    val text: String,
    val fileId: String,
    val fileName: String,
    val fileSize: Long,
    val fileKey: ByteArray,
    val stickerId: String,
    val noticeKind: Int,
    val actor: String,
    val target: String,
    val role: Int,
    val typing: Boolean,
    val online: Boolean,
    val callId: ByteArray,
    val callKeyId: Int,
    val callOp: Int,
    val callMediaFlags: Int,
    val payload: ByteArray
)

object MiEventType {
    const val NONE = 0
    const val CHAT_TEXT = 1
    const val CHAT_FILE = 2
    const val CHAT_STICKER = 3
    const val GROUP_TEXT = 4
    const val GROUP_FILE = 5
    const val GROUP_INVITE = 6
    const val GROUP_NOTICE = 7
    const val OUTGOING_TEXT = 8
    const val OUTGOING_FILE = 9
    const val OUTGOING_STICKER = 10
    const val OUTGOING_GROUP_TEXT = 11
    const val OUTGOING_GROUP_FILE = 12
    const val DELIVERY = 13
    const val READ_RECEIPT = 14
    const val TYPING = 15
    const val PRESENCE = 16
    const val GROUP_CALL = 17
    const val MEDIA_RELAY = 18
    const val GROUP_MEDIA_RELAY = 19
    const val OFFLINE_PAYLOAD = 20
}

object HistoryKind {
    const val TEXT = 1
    const val FILE = 2
    const val STICKER = 3
    const val SYSTEM = 4
}

object HistoryStatus {
    const val SENT = 0
    const val DELIVERED = 1
    const val READ = 2
    const val FAILED = 3
}

object GroupMemberRole {
    const val OWNER = 0
    const val ADMIN = 1
    const val MEMBER = 2
}
