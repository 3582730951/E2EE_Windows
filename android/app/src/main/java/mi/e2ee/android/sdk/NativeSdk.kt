package mi.e2ee.android.sdk

object NativeSdk {
    private const val BACKEND_LIB_NAME = "mi_e2ee_backend"
    private const val JNI_LIB_NAME = "mi_e2ee_jni"

    val available: Boolean = runCatching {
        System.loadLibrary(BACKEND_LIB_NAME)
        System.loadLibrary(JNI_LIB_NAME)
        true
    }.getOrDefault(false)

    external fun getVersion(): SdkVersion
    external fun getCapabilities(): Int

    external fun createClient(configPath: String?): Long
    external fun lastCreateError(): String
    external fun destroyClient(handle: Long)

    external fun lastError(handle: Long): String
    external fun token(handle: Long): String
    external fun deviceId(handle: Long): String
    external fun remoteOk(handle: Long): Boolean
    external fun remoteError(handle: Long): String
    external fun isRemoteMode(handle: Long): Boolean
    external fun relogin(handle: Long): Boolean

    external fun hasPendingServerTrust(handle: Long): Boolean
    external fun pendingServerFingerprint(handle: Long): String
    external fun pendingServerPin(handle: Long): String
    external fun trustPendingServer(handle: Long, pin: String): Boolean

    external fun hasPendingPeerTrust(handle: Long): Boolean
    external fun pendingPeerUsername(handle: Long): String
    external fun pendingPeerFingerprint(handle: Long): String
    external fun pendingPeerPin(handle: Long): String
    external fun trustPendingPeer(handle: Long, pin: String): Boolean

    external fun register(handle: Long, username: String, password: String): Boolean
    external fun login(handle: Long, username: String, password: String): Boolean
    external fun logout(handle: Long): Boolean
    external fun heartbeat(handle: Long): Boolean

    external fun sendPrivateText(handle: Long, peerUsername: String, text: String): String?
    external fun sendPrivateTextWithReply(
        handle: Long,
        peerUsername: String,
        text: String,
        replyToMessageId: String,
        replyPreview: String
    ): String?
    external fun resendPrivateText(
        handle: Long,
        peerUsername: String,
        messageId: String,
        text: String
    ): Boolean
    external fun resendPrivateTextWithReply(
        handle: Long,
        peerUsername: String,
        messageId: String,
        text: String,
        replyToMessageId: String,
        replyPreview: String
    ): Boolean
    external fun sendGroupText(handle: Long, groupId: String, text: String): String?
    external fun resendGroupText(
        handle: Long,
        groupId: String,
        messageId: String,
        text: String
    ): Boolean
    external fun sendPrivateFile(handle: Long, peerUsername: String, filePath: String): String?
    external fun resendPrivateFile(
        handle: Long,
        peerUsername: String,
        messageId: String,
        filePath: String
    ): Boolean
    external fun sendGroupFile(handle: Long, groupId: String, filePath: String): String?
    external fun resendGroupFile(
        handle: Long,
        groupId: String,
        messageId: String,
        filePath: String
    ): Boolean
    external fun sendPrivateSticker(handle: Long, peerUsername: String, stickerId: String): String?
    external fun resendPrivateSticker(
        handle: Long,
        peerUsername: String,
        messageId: String,
        stickerId: String
    ): Boolean
    external fun sendPrivateLocation(
        handle: Long,
        peerUsername: String,
        latE7: Int,
        lonE7: Int,
        label: String
    ): String?
    external fun resendPrivateLocation(
        handle: Long,
        peerUsername: String,
        messageId: String,
        latE7: Int,
        lonE7: Int,
        label: String
    ): Boolean
    external fun sendPrivateContact(
        handle: Long,
        peerUsername: String,
        cardUsername: String,
        cardDisplay: String
    ): String?
    external fun resendPrivateContact(
        handle: Long,
        peerUsername: String,
        messageId: String,
        cardUsername: String,
        cardDisplay: String
    ): Boolean
    external fun sendReadReceipt(handle: Long, peerUsername: String, messageId: String): Boolean
    external fun sendTyping(handle: Long, peerUsername: String, typing: Boolean): Boolean
    external fun sendPresence(handle: Long, peerUsername: String, online: Boolean): Boolean

    external fun addFriend(handle: Long, friendUsername: String, remark: String): Boolean
    external fun setFriendRemark(handle: Long, friendUsername: String, remark: String): Boolean
    external fun deleteFriend(handle: Long, friendUsername: String): Boolean
    external fun setUserBlocked(handle: Long, blockedUsername: String, blocked: Boolean): Boolean
    external fun sendFriendRequest(handle: Long, targetUsername: String, remark: String): Boolean
    external fun respondFriendRequest(handle: Long, requesterUsername: String, accept: Boolean): Boolean
    external fun listFriends(handle: Long): Array<FriendEntry>
    external fun syncFriends(handle: Long): SyncFriendsResult
    external fun listFriendRequests(handle: Long): Array<FriendRequestEntry>

    external fun listDevices(handle: Long): Array<DeviceEntry>
    external fun kickDevice(handle: Long, deviceId: String): Boolean

    external fun joinGroup(handle: Long, groupId: String): Boolean
    external fun leaveGroup(handle: Long, groupId: String): Boolean
    external fun createGroup(handle: Long): String?
    external fun sendGroupInvite(handle: Long, groupId: String, peerUsername: String): String?
    external fun listGroupMembers(handle: Long, groupId: String): Array<GroupMemberEntry>
    external fun setGroupMemberRole(
        handle: Long,
        groupId: String,
        peerUsername: String,
        role: Int
    ): Boolean
    external fun kickGroupMember(handle: Long, groupId: String, peerUsername: String): Boolean

    external fun startGroupCall(handle: Long, groupId: String, video: Boolean): GroupCallInfo?
    external fun joinGroupCall(
        handle: Long,
        groupId: String,
        callId: ByteArray,
        video: Boolean
    ): GroupCallInfo?
    external fun leaveGroupCall(handle: Long, groupId: String, callId: ByteArray): Boolean
    external fun getGroupCallKey(
        handle: Long,
        groupId: String,
        callId: ByteArray,
        keyId: Int
    ): ByteArray?
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

    external fun loadChatHistory(
        handle: Long,
        convId: String,
        isGroup: Boolean,
        limit: Int
    ): Array<HistoryEntry>
    external fun deleteChatHistory(
        handle: Long,
        convId: String,
        isGroup: Boolean,
        deleteAttachments: Boolean,
        secureWipe: Boolean
    ): Boolean
    external fun setHistoryEnabled(handle: Long, enabled: Boolean): Boolean
    external fun clearAllHistory(handle: Long, deleteAttachments: Boolean, secureWipe: Boolean): Boolean

    external fun beginDevicePairingPrimary(handle: Long): String?
    external fun pollDevicePairingRequests(handle: Long): Array<DevicePairingRequest>
    external fun approveDevicePairingRequest(
        handle: Long,
        deviceId: String,
        requestIdHex: String
    ): Boolean
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
    external fun deriveMediaRoot(
        handle: Long,
        peerUsername: String,
        callId: ByteArray
    ): ByteArray?
    external fun pushMedia(
        handle: Long,
        peerUsername: String,
        callId: ByteArray,
        packet: ByteArray
    ): Boolean
    external fun pullMedia(
        handle: Long,
        callId: ByteArray,
        maxPackets: Int,
        waitMs: Int
    ): Array<MediaPacket>
    external fun pushGroupMedia(
        handle: Long,
        groupId: String,
        callId: ByteArray,
        packet: ByteArray
    ): Boolean
    external fun pullGroupMedia(
        handle: Long,
        callId: ByteArray,
        maxPackets: Int,
        waitMs: Int
    ): Array<MediaPacket>
    external fun addMediaSubscription(
        handle: Long,
        callId: ByteArray,
        isGroup: Boolean,
        groupId: String?
    ): Boolean
    external fun clearMediaSubscriptions(handle: Long)

    external fun pollEvents(handle: Long, maxEvents: Int, waitMs: Int): Array<SdkEvent>
}
