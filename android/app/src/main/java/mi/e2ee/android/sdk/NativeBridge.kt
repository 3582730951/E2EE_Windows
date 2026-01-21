package mi.e2ee.android.sdk

import android.content.Context
import mi.e2ee.android.ui.SdkBridge

object NativeBridge {
    private const val BACKEND_LIB_NAME = "mi_e2ee_backend"
    private const val JNI_LIB_NAME = "mi_e2ee_jni"

    val available: Boolean = runCatching {
        System.loadLibrary(BACKEND_LIB_NAME)
        System.loadLibrary(JNI_LIB_NAME)
        true
    }.getOrDefault(false)

    external fun create(context: Context, callback: SdkBridge): Long
    external fun destroy(handle: Long)

    external fun init(handle: Long, configPath: String?): Boolean
    external fun dispose(handle: Long)
    external fun ensureLoggedOut(handle: Long)

    external fun register(handle: Long, username: String, password: String): Boolean
    external fun login(handle: Long, username: String, password: String): Boolean
    external fun logout(handle: Long)
    external fun relogin(handle: Long): Boolean
    external fun isRemoteMode(handle: Long): Boolean
    external fun heartbeat(handle: Long): Boolean

    external fun trustPendingServer(handle: Long, pin: String): Boolean
    external fun trustPendingPeer(handle: Long, pin: String): Boolean

    external fun setActiveConversation(handle: Long, convId: String, isGroup: Boolean)
    external fun clearActiveConversation(handle: Long)
    external fun loadHistory(handle: Long, convId: String, isGroup: Boolean)

    external fun sendText(
        handle: Long,
        convId: String,
        text: String,
        replyMessageId: String?,
        replyPreview: String?,
        isGroup: Boolean
    ): Boolean
    external fun resendPrivateText(handle: Long, peerUsername: String, messageId: String, text: String): Boolean
    external fun resendPrivateTextWithReply(
        handle: Long,
        peerUsername: String,
        messageId: String,
        text: String,
        replyMessageId: String,
        replyPreview: String
    ): Boolean
    external fun resendGroupText(handle: Long, groupId: String, messageId: String, text: String): Boolean
    external fun resendPrivateFile(handle: Long, peerUsername: String, messageId: String, filePath: String): Boolean
    external fun resendGroupFile(handle: Long, groupId: String, messageId: String, filePath: String): Boolean
    external fun resendPrivateSticker(handle: Long, peerUsername: String, messageId: String, stickerId: String): Boolean
    external fun resendPrivateLocation(
        handle: Long,
        peerUsername: String,
        messageId: String,
        latE7: Int,
        lonE7: Int,
        label: String
    ): Boolean
    external fun resendPrivateContact(
        handle: Long,
        peerUsername: String,
        messageId: String,
        cardUsername: String,
        cardDisplay: String
    ): Boolean
    external fun sendRecall(handle: Long, convId: String, messageId: String, isGroup: Boolean): Boolean
    external fun sendTyping(handle: Long, peerUsername: String, typing: Boolean)
    external fun sendPresence(handle: Long, peerUsername: String, online: Boolean)
    external fun sendReadReceipt(handle: Long, peerUsername: String, messageId: String)
    external fun sendFile(handle: Long, convId: String, path: String, isGroup: Boolean): Boolean
    external fun sendSticker(handle: Long, convId: String, stickerId: String): Boolean
    external fun sendLocation(handle: Long, convId: String, lat: Double, lon: Double, label: String, isGroup: Boolean): Boolean
    external fun sendContact(handle: Long, convId: String, cardUsername: String, cardDisplay: String): Boolean

    external fun startPeerCall(handle: Long, peerUsername: String, video: Boolean): PeerCallState?
    external fun acceptIncomingCall(handle: Long): PeerCallState?
    external fun declineIncomingCall(handle: Long)
    external fun endPeerCall(handle: Long)

    external fun addFriend(handle: Long, targetUsername: String, remark: String): Boolean
    external fun setFriendRemark(handle: Long, friendUsername: String, remark: String): Boolean
    external fun sendFriendRequest(handle: Long, targetUsername: String, remark: String): Boolean
    external fun respondFriendRequest(handle: Long, requester: String, accept: Boolean): Boolean
    external fun deleteFriend(handle: Long, friendUsername: String): Boolean
    external fun setUserBlocked(handle: Long, username: String, blocked: Boolean): Boolean

    external fun joinGroup(handle: Long, groupId: String): Boolean
    external fun leaveGroup(handle: Long, groupId: String): Boolean
    external fun createGroup(handle: Long): String?
    external fun sendGroupInvite(handle: Long, groupId: String, peerUsername: String): String?
    external fun refreshGroupMembers(handle: Long, groupId: String)
    external fun setGroupMemberRole(handle: Long, groupId: String, peerUsername: String, role: Int): Boolean
    external fun kickGroupMember(handle: Long, groupId: String, peerUsername: String): Boolean

    external fun refreshDevices(handle: Long)
    external fun kickDevice(handle: Long, deviceId: String): Boolean

    external fun deleteChatHistory(handle: Long, convId: String, isGroup: Boolean, deleteAttachments: Boolean, secureWipe: Boolean): Boolean
    external fun setHistoryEnabled(handle: Long, enabled: Boolean): Boolean
    external fun clearAllHistory(handle: Long, deleteAttachments: Boolean, secureWipe: Boolean): Boolean

    external fun beginDevicePairingPrimary(handle: Long): String?
    external fun pollDevicePairingRequests(handle: Long)
    external fun approveDevicePairingRequest(handle: Long, deviceId: String, requestId: String): Boolean
    external fun beginDevicePairingLinked(handle: Long, pairingCode: String): Boolean
    external fun pollDevicePairingLinked(handle: Long): Boolean
    external fun cancelDevicePairing(handle: Long)

    external fun storeAttachmentPreviewBytes(
        handle: Long,
        fileId: String,
        fileName: String,
        fileSize: Long,
        bytes: ByteArray
    ): Boolean
    external fun downloadChatFileToPath(
        handle: Long,
        fileId: String,
        fileKey: ByteArray,
        fileName: String,
        fileSize: Long,
        outPath: String,
        wipeAfterRead: Boolean
    ): Boolean
    external fun downloadChatFileToBytes(
        handle: Long,
        fileId: String,
        fileKey: ByteArray,
        fileName: String,
        fileSize: Long,
        wipeAfterRead: Boolean
    ): ByteArray?

    external fun getMediaConfig(handle: Long): MediaConfig?
    external fun deriveMediaRoot(handle: Long, peerUsername: String, callId: ByteArray): ByteArray?
    external fun pushMedia(handle: Long, peerUsername: String, callId: ByteArray, packet: ByteArray): Boolean
    external fun pullMedia(handle: Long, callId: ByteArray, maxPackets: Int, waitMs: Int): Array<MediaPacket>
    external fun pushGroupMedia(handle: Long, groupId: String, callId: ByteArray, packet: ByteArray): Boolean
    external fun pullGroupMedia(handle: Long, callId: ByteArray, maxPackets: Int, waitMs: Int): Array<MediaPacket>
    external fun addMediaSubscription(handle: Long, callId: ByteArray, isGroup: Boolean, groupId: String?): Boolean
    external fun clearMediaSubscriptions(handle: Long)

    external fun startGroupCall(handle: Long, groupId: String, video: Boolean): GroupCallInfo?
    external fun joinGroupCall(handle: Long, groupId: String, callId: ByteArray, video: Boolean): GroupCallInfo?
    external fun leaveGroupCall(handle: Long, groupId: String, callId: ByteArray): Boolean
    external fun getGroupCallKey(handle: Long, groupId: String, callId: ByteArray, keyId: Int): ByteArray?
    external fun rotateGroupCallKey(
        handle: Long,
        groupId: String,
        callId: ByteArray,
        keyId: Int,
        members: Array<String>
    ): Boolean
    external fun requestGroupCallKey(
        handle: Long,
        groupId: String,
        callId: ByteArray,
        keyId: Int,
        members: Array<String>
    ): Boolean
    external fun sendGroupCallSignal(
        handle: Long,
        op: Int,
        groupId: String,
        callId: ByteArray,
        video: Boolean,
        keyId: Int,
        seq: Int,
        tsMs: Long,
        ext: ByteArray?
    ): GroupCallSignalResult?

    external fun setReadReceiptsEnabled(handle: Long, enabled: Boolean)
    external fun setScreenshotAlertsEnabled(handle: Long, enabled: Boolean)
    external fun setPrivacyLastSeen(handle: Long, value: String)
    external fun setPrivacyProfilePhoto(handle: Long, value: String)
    external fun setPrivacyGroupInvites(handle: Long, value: String)

    external fun clearMediaRelayEvents(handle: Long)
    external fun clearOfflinePayloads(handle: Long)

    external fun togglePin(handle: Long, convId: String)
    external fun toggleMute(handle: Long, convId: String)
    external fun markRead(handle: Long, convId: String)

    external fun clientHandle(handle: Long): Long
}
