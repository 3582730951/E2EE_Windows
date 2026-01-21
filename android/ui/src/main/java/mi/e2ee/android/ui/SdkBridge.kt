package mi.e2ee.android.ui

import android.content.Context
import android.os.Handler
import android.os.Looper
import androidx.annotation.Keep
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshots.SnapshotStateList
import mi.e2ee.android.sdk.GroupCallInfo
import mi.e2ee.android.sdk.GroupCallSignalResult
import mi.e2ee.android.sdk.MediaConfig
import mi.e2ee.android.sdk.MediaPacket
import mi.e2ee.android.sdk.NativeBridge
import mi.e2ee.android.sdk.SdkVersion

class SdkBridge(private val context: Context) {
    private val mainHandler = Handler(Looper.getMainLooper())
    private var nativeHandle: Long = 0L
    private var clientHandleValue: Long = 0L

    var initialized by mutableStateOf(false)
        private set
    var loggedIn by mutableStateOf(false)
        private set
    var username by mutableStateOf("")
        private set
    var token by mutableStateOf("")
        private set
    var deviceId by mutableStateOf("")
        private set
    var lastError by mutableStateOf("")
        private set
    var statusMessage by mutableStateOf<String?>(null)
        private set
    var remoteOk by mutableStateOf(true)
        private set
    var remoteError by mutableStateOf("")
        private set
    var sdkVersion by mutableStateOf<SdkVersion?>(null)
        private set
    var capabilities by mutableStateOf(0)
        private set

    var hasPendingServerTrust by mutableStateOf(false)
        private set
    var pendingServerFingerprint by mutableStateOf("")
        private set
    var pendingServerPin by mutableStateOf("")
        private set
    var hasPendingPeerTrust by mutableStateOf(false)
        private set
    var pendingPeerUsername by mutableStateOf("")
        private set
    var pendingPeerFingerprint by mutableStateOf("")
        private set
    var pendingPeerPin by mutableStateOf("")
        private set

    var pendingCall by mutableStateOf<IncomingCall?>(null)
        private set
    var activePeerCall by mutableStateOf<PeerCallState?>(null)
        private set
    var activeGroupCall by mutableStateOf<GroupCallState?>(null)
        private set

    var historyEnabled by mutableStateOf(true)
        private set
    var readReceiptsEnabled by mutableStateOf(true)
        private set
    var screenshotAlertsEnabled by mutableStateOf(true)
        private set
    var privacyLastSeen by mutableStateOf("contacts")
        private set
    var privacyProfilePhoto by mutableStateOf("contacts")
        private set
    var privacyGroupInvites by mutableStateOf("contacts")
        private set

    val friends = mutableStateListOf<FriendUi>()
    val friendRequests = mutableStateListOf<FriendRequestUi>()
    val groups = mutableStateListOf<GroupUi>()
    val devices = mutableStateListOf<DeviceUi>()
    val pairingRequests = mutableStateListOf<PairingRequestUi>()
    val conversations = mutableStateListOf<ConversationPreview>()
    val groupCallRooms = mutableStateListOf<GroupCallRoomUi>()
    val blockedUsers = mutableStateMapOf<String, Boolean>()
    val mediaRelayEvents = mutableStateListOf<MediaRelayLog>()
    val offlinePayloads = mutableStateListOf<OfflinePayloadLog>()

    private val chatItems = mutableStateMapOf<String, SnapshotStateList<ChatItem>>()
    private val groupItems = mutableStateMapOf<String, SnapshotStateList<GroupChatItem>>()
    private val groupMembers = mutableStateMapOf<String, SnapshotStateList<GroupMemberUi>>()

    private fun ensureHandle(): Boolean {
        if (nativeHandle != 0L) {
            return true
        }
        if (!NativeBridge.available) {
            runOnMain {
                lastError = "Native SDK not available"
                statusMessage = "Native SDK not available"
            }
            return false
        }
        nativeHandle = NativeBridge.create(context, this)
        if (nativeHandle == 0L) {
            runOnMain {
                lastError = "Native bridge init failed"
                statusMessage = "Native bridge init failed"
            }
            return false
        }
        return true
    }

    private fun runOnMain(block: () -> Unit) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            block()
        } else {
            mainHandler.post(block)
        }
    }

    fun init(configPath: String? = null): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.init(nativeHandle, configPath)
    }

    fun dispose() {
        if (nativeHandle != 0L) {
            NativeBridge.dispose(nativeHandle)
            NativeBridge.destroy(nativeHandle)
            nativeHandle = 0L
        }
    }

    fun ensureLoggedOut() {
        if (!ensureHandle()) return
        NativeBridge.ensureLoggedOut(nativeHandle)
    }

    fun register(usernameInput: String, password: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.register(nativeHandle, usernameInput, password)
    }

    fun login(usernameInput: String, password: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.login(nativeHandle, usernameInput, password)
    }

    fun logout() {
        if (!ensureHandle()) return
        NativeBridge.logout(nativeHandle)
    }

    fun relogin(): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.relogin(nativeHandle)
    }

    fun isRemoteMode(): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.isRemoteMode(nativeHandle)
    }

    fun heartbeat(): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.heartbeat(nativeHandle)
    }

    fun trustPendingServer(pin: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.trustPendingServer(nativeHandle, pin)
    }

    fun trustPendingPeer(pin: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.trustPendingPeer(nativeHandle, pin)
    }

    fun setActiveConversation(convId: String, isGroup: Boolean) {
        if (!ensureHandle()) return
        NativeBridge.setActiveConversation(nativeHandle, convId, isGroup)
    }

    fun clearActiveConversation() {
        if (!ensureHandle()) return
        NativeBridge.clearActiveConversation(nativeHandle)
    }

    fun chatItemsFor(convId: String): SnapshotStateList<ChatItem> {
        return chatItems.getOrPut(convId) { mutableStateListOf() }
    }

    fun groupItemsFor(groupId: String): SnapshotStateList<GroupChatItem> {
        return groupItems.getOrPut(groupId) { mutableStateListOf() }
    }

    fun groupMembersFor(groupId: String): SnapshotStateList<GroupMemberUi> {
        return groupMembers.getOrPut(groupId) { mutableStateListOf() }
    }

    fun loadHistory(convId: String, isGroup: Boolean) {
        if (!ensureHandle()) return
        NativeBridge.loadHistory(nativeHandle, convId, isGroup)
    }

    fun sendText(convId: String, text: String, reply: ReplyPreview? = null, isGroup: Boolean = false): Boolean {
        if (!ensureHandle()) return false
        val replyId = reply?.messageId
        val replyPreview = reply?.snippet
        return NativeBridge.sendText(nativeHandle, convId, text, replyId, replyPreview, isGroup)
    }

    fun resendPrivateText(peerUsername: String, messageId: String, text: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.resendPrivateText(nativeHandle, peerUsername, messageId, text)
    }

    fun resendPrivateTextWithReply(
        peerUsername: String,
        messageId: String,
        text: String,
        replyId: String,
        replyPreview: String
    ): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.resendPrivateTextWithReply(nativeHandle, peerUsername, messageId, text, replyId, replyPreview)
    }

    fun resendGroupText(groupId: String, messageId: String, text: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.resendGroupText(nativeHandle, groupId, messageId, text)
    }

    fun resendPrivateFile(peerUsername: String, messageId: String, filePath: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.resendPrivateFile(nativeHandle, peerUsername, messageId, filePath)
    }

    fun resendGroupFile(groupId: String, messageId: String, filePath: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.resendGroupFile(nativeHandle, groupId, messageId, filePath)
    }

    fun resendPrivateSticker(peerUsername: String, messageId: String, stickerId: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.resendPrivateSticker(nativeHandle, peerUsername, messageId, stickerId)
    }

    fun resendPrivateLocation(
        peerUsername: String,
        messageId: String,
        lat: Double,
        lon: Double,
        label: String
    ): Boolean {
        if (!ensureHandle()) return false
        if (!lat.isFinite() || !lon.isFinite()) return false
        val latE7 = (lat * 10000000.0).toLong().toInt()
        val lonE7 = (lon * 10000000.0).toLong().toInt()
        return NativeBridge.resendPrivateLocation(nativeHandle, peerUsername, messageId, latE7, lonE7, label)
    }

    fun resendPrivateContact(
        peerUsername: String,
        messageId: String,
        cardUsername: String,
        cardDisplay: String
    ): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.resendPrivateContact(nativeHandle, peerUsername, messageId, cardUsername, cardDisplay)
    }

    fun sendRecall(convId: String, messageId: String, isGroup: Boolean): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.sendRecall(nativeHandle, convId, messageId, isGroup)
    }

    fun sendTyping(peerUsername: String, typing: Boolean) {
        if (!ensureHandle()) return
        NativeBridge.sendTyping(nativeHandle, peerUsername, typing)
    }

    fun sendPresence(peerUsername: String, online: Boolean) {
        if (!ensureHandle()) return
        NativeBridge.sendPresence(nativeHandle, peerUsername, online)
    }

    fun startPeerCall(peerUsername: String, video: Boolean): PeerCallState? {
        if (!ensureHandle()) return null
        return NativeBridge.startPeerCall(nativeHandle, peerUsername, video)
    }

    fun acceptIncomingCall(): PeerCallState? {
        if (!ensureHandle()) return null
        return NativeBridge.acceptIncomingCall(nativeHandle)
    }

    fun declineIncomingCall() {
        if (!ensureHandle()) return
        NativeBridge.declineIncomingCall(nativeHandle)
    }

    fun endPeerCall() {
        if (!ensureHandle()) return
        NativeBridge.endPeerCall(nativeHandle)
    }

    fun sendReadReceipt(peerUsername: String, messageId: String) {
        if (!ensureHandle()) return
        NativeBridge.sendReadReceipt(nativeHandle, peerUsername, messageId)
    }

    fun sendFile(convId: String, path: String, isGroup: Boolean): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.sendFile(nativeHandle, convId, path, isGroup)
    }

    fun sendSticker(convId: String, stickerId: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.sendSticker(nativeHandle, convId, stickerId)
    }

    fun sendLocation(convId: String, lat: Double, lon: Double, label: String, isGroup: Boolean): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.sendLocation(nativeHandle, convId, lat, lon, label, isGroup)
    }

    fun sendContact(convId: String, cardUsername: String, cardDisplay: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.sendContact(nativeHandle, convId, cardUsername, cardDisplay)
    }

    fun addFriend(targetUsername: String, remark: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.addFriend(nativeHandle, targetUsername, remark)
    }

    fun setFriendRemark(friendUsername: String, remark: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.setFriendRemark(nativeHandle, friendUsername, remark)
    }

    fun sendFriendRequest(targetUsername: String, remark: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.sendFriendRequest(nativeHandle, targetUsername, remark)
    }

    fun respondFriendRequest(requester: String, accept: Boolean): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.respondFriendRequest(nativeHandle, requester, accept)
    }

    fun deleteFriend(friendUsername: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.deleteFriend(nativeHandle, friendUsername)
    }

    fun setUserBlocked(username: String, blocked: Boolean): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.setUserBlocked(nativeHandle, username, blocked)
    }

    fun joinGroup(groupId: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.joinGroup(nativeHandle, groupId)
    }

    fun leaveGroup(groupId: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.leaveGroup(nativeHandle, groupId)
    }

    fun createGroup(): String? {
        if (!ensureHandle()) return null
        return NativeBridge.createGroup(nativeHandle)
    }

    fun sendGroupInvite(groupId: String, peerUsername: String): String? {
        if (!ensureHandle()) return null
        return NativeBridge.sendGroupInvite(nativeHandle, groupId, peerUsername)
    }

    fun refreshGroupMembers(groupId: String) {
        if (!ensureHandle()) return
        NativeBridge.refreshGroupMembers(nativeHandle, groupId)
    }

    fun setGroupMemberRole(groupId: String, peerUsername: String, role: Int): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.setGroupMemberRole(nativeHandle, groupId, peerUsername, role)
    }

    fun kickGroupMember(groupId: String, peerUsername: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.kickGroupMember(nativeHandle, groupId, peerUsername)
    }

    fun refreshDevices() {
        if (!ensureHandle()) return
        NativeBridge.refreshDevices(nativeHandle)
    }

    fun kickDevice(deviceId: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.kickDevice(nativeHandle, deviceId)
    }

    fun deleteChatHistory(convId: String, isGroup: Boolean, deleteAttachments: Boolean, secureWipe: Boolean): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.deleteChatHistory(nativeHandle, convId, isGroup, deleteAttachments, secureWipe)
    }

    fun setHistoryEnabled(enabled: Boolean): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.setHistoryEnabled(nativeHandle, enabled)
    }

    fun clearAllHistory(deleteAttachments: Boolean, secureWipe: Boolean): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.clearAllHistory(nativeHandle, deleteAttachments, secureWipe)
    }

    fun beginDevicePairingPrimary(): String? {
        if (!ensureHandle()) return null
        return NativeBridge.beginDevicePairingPrimary(nativeHandle)
    }

    fun pollDevicePairingRequests() {
        if (!ensureHandle()) return
        NativeBridge.pollDevicePairingRequests(nativeHandle)
    }

    fun approveDevicePairingRequest(deviceId: String, requestId: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.approveDevicePairingRequest(nativeHandle, deviceId, requestId)
    }

    fun beginDevicePairingLinked(pairingCode: String): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.beginDevicePairingLinked(nativeHandle, pairingCode)
    }

    fun pollDevicePairingLinked(): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.pollDevicePairingLinked(nativeHandle)
    }

    fun cancelDevicePairing() {
        if (!ensureHandle()) return
        NativeBridge.cancelDevicePairing(nativeHandle)
    }

    fun storeAttachmentPreviewBytes(fileId: String, fileName: String, fileSize: Long, bytes: ByteArray): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.storeAttachmentPreviewBytes(nativeHandle, fileId, fileName, fileSize, bytes)
    }

    fun downloadChatFileToPath(
        fileId: String,
        fileKey: ByteArray,
        fileName: String,
        fileSize: Long,
        outPath: String,
        wipeAfterRead: Boolean
    ): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.downloadChatFileToPath(nativeHandle, fileId, fileKey, fileName, fileSize, outPath, wipeAfterRead)
    }

    fun downloadChatFileToBytes(
        fileId: String,
        fileKey: ByteArray,
        fileName: String,
        fileSize: Long,
        wipeAfterRead: Boolean
    ): ByteArray? {
        if (!ensureHandle()) return null
        return NativeBridge.downloadChatFileToBytes(nativeHandle, fileId, fileKey, fileName, fileSize, wipeAfterRead)
    }

    fun getMediaConfig(): MediaConfig? {
        if (!ensureHandle()) return null
        return NativeBridge.getMediaConfig(nativeHandle)
    }

    internal fun clientHandle(): Long {
        return clientHandleValue
    }

    fun deriveMediaRoot(peerUsername: String, callId: ByteArray): ByteArray? {
        if (!ensureHandle()) return null
        return NativeBridge.deriveMediaRoot(nativeHandle, peerUsername, callId)
    }

    fun pushMedia(peerUsername: String, callId: ByteArray, packet: ByteArray): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.pushMedia(nativeHandle, peerUsername, callId, packet)
    }

    fun pullMedia(callId: ByteArray, maxPackets: Int, waitMs: Int): Array<MediaPacket> {
        if (!ensureHandle()) return emptyArray()
        return NativeBridge.pullMedia(nativeHandle, callId, maxPackets, waitMs)
    }

    fun pushGroupMedia(groupId: String, callId: ByteArray, packet: ByteArray): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.pushGroupMedia(nativeHandle, groupId, callId, packet)
    }

    fun pullGroupMedia(callId: ByteArray, maxPackets: Int, waitMs: Int): Array<MediaPacket> {
        if (!ensureHandle()) return emptyArray()
        return NativeBridge.pullGroupMedia(nativeHandle, callId, maxPackets, waitMs)
    }

    fun addMediaSubscription(callId: ByteArray, isGroup: Boolean, groupId: String? = null): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.addMediaSubscription(nativeHandle, callId, isGroup, groupId)
    }

    fun clearMediaSubscriptions() {
        if (!ensureHandle()) return
        NativeBridge.clearMediaSubscriptions(nativeHandle)
    }

    fun startGroupCall(groupId: String, video: Boolean): GroupCallInfo? {
        if (!ensureHandle()) return null
        return NativeBridge.startGroupCall(nativeHandle, groupId, video)
    }

    fun joinGroupCall(groupId: String, callId: ByteArray, video: Boolean): GroupCallInfo? {
        if (!ensureHandle()) return null
        return NativeBridge.joinGroupCall(nativeHandle, groupId, callId, video)
    }

    fun joinGroupCallHex(groupId: String, callIdHex: String, video: Boolean): GroupCallInfo? {
        return joinGroupCall(groupId, hexToBytes(callIdHex), video)
    }

    fun leaveGroupCallHex(groupId: String, callIdHex: String): Boolean {
        return leaveGroupCall(groupId, hexToBytes(callIdHex))
    }

    fun leaveGroupCall(groupId: String, callId: ByteArray): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.leaveGroupCall(nativeHandle, groupId, callId)
    }

    fun getGroupCallKey(groupId: String, callId: ByteArray, keyId: Int): ByteArray? {
        if (!ensureHandle()) return null
        return NativeBridge.getGroupCallKey(nativeHandle, groupId, callId, keyId)
    }

    fun rotateGroupCallKey(groupId: String, callId: ByteArray, keyId: Int, members: Array<String>): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.rotateGroupCallKey(nativeHandle, groupId, callId, keyId, members)
    }

    fun requestGroupCallKey(groupId: String, callId: ByteArray, keyId: Int, members: Array<String>): Boolean {
        if (!ensureHandle()) return false
        return NativeBridge.requestGroupCallKey(nativeHandle, groupId, callId, keyId, members)
    }

    fun sendGroupCallSignal(
        op: Int,
        groupId: String,
        callId: ByteArray,
        video: Boolean,
        keyId: Int,
        seq: Int,
        tsMs: Long,
        ext: ByteArray?
    ): GroupCallSignalResult? {
        if (!ensureHandle()) return null
        return NativeBridge.sendGroupCallSignal(nativeHandle, op, groupId, callId, video, keyId, seq, tsMs, ext)
    }

    fun setReadReceiptsEnabled(enabled: Boolean) {
        if (!ensureHandle()) return
        NativeBridge.setReadReceiptsEnabled(nativeHandle, enabled)
    }

    fun setScreenshotAlertsEnabled(enabled: Boolean) {
        if (!ensureHandle()) return
        NativeBridge.setScreenshotAlertsEnabled(nativeHandle, enabled)
    }

    fun setPrivacyLastSeen(value: String) {
        if (!ensureHandle()) return
        NativeBridge.setPrivacyLastSeen(nativeHandle, value)
    }

    fun setPrivacyProfilePhoto(value: String) {
        if (!ensureHandle()) return
        NativeBridge.setPrivacyProfilePhoto(nativeHandle, value)
    }

    fun setPrivacyGroupInvites(value: String) {
        if (!ensureHandle()) return
        NativeBridge.setPrivacyGroupInvites(nativeHandle, value)
    }

    fun clearMediaRelayEvents() {
        if (!ensureHandle()) return
        NativeBridge.clearMediaRelayEvents(nativeHandle)
    }

    fun clearOfflinePayloads() {
        if (!ensureHandle()) return
        NativeBridge.clearOfflinePayloads(nativeHandle)
    }

    fun togglePin(convId: String) {
        if (!ensureHandle()) return
        NativeBridge.togglePin(nativeHandle, convId)
    }

    fun toggleMute(convId: String) {
        if (!ensureHandle()) return
        NativeBridge.toggleMute(nativeHandle, convId)
    }

    fun markRead(convId: String) {
        if (!ensureHandle()) return
        NativeBridge.markRead(nativeHandle, convId)
    }

    fun downloadAttachmentToPath(
        attachment: Attachment,
        outPath: String,
        wipeAfterRead: Boolean
    ): Boolean {
        if (attachment.kind != AttachmentKind.File) return false
        val fileId = attachment.fileId ?: return false
        val fileKeyHex = attachment.fileKeyHex ?: return false
        val fileSize = attachment.fileSize ?: return false
        val fileKey = hexToBytes(fileKeyHex)
        return downloadChatFileToPath(fileId, fileKey, attachment.label, fileSize, outPath, wipeAfterRead)
    }

    @Keep
    fun onSessionState(
        initialized: Boolean,
        loggedIn: Boolean,
        username: String,
        token: String,
        deviceId: String,
        lastError: String,
        statusMessage: String?,
        remoteOk: Boolean,
        remoteError: String,
        sdkVersion: SdkVersion?,
        capabilities: Int,
        clientHandle: Long
    ) {
        runOnMain {
            this.initialized = initialized
            this.loggedIn = loggedIn
            this.username = username
            this.token = token
            this.deviceId = deviceId
            this.lastError = lastError
            this.statusMessage = statusMessage
            this.remoteOk = remoteOk
            this.remoteError = remoteError
            this.sdkVersion = sdkVersion
            this.capabilities = capabilities
            this.clientHandleValue = clientHandle
        }
    }

    @Keep
    fun onTrustState(
        hasPendingServerTrust: Boolean,
        pendingServerFingerprint: String,
        pendingServerPin: String,
        hasPendingPeerTrust: Boolean,
        pendingPeerUsername: String,
        pendingPeerFingerprint: String,
        pendingPeerPin: String
    ) {
        runOnMain {
            this.hasPendingServerTrust = hasPendingServerTrust
            this.pendingServerFingerprint = pendingServerFingerprint
            this.pendingServerPin = pendingServerPin
            this.hasPendingPeerTrust = hasPendingPeerTrust
            this.pendingPeerUsername = pendingPeerUsername
            this.pendingPeerFingerprint = pendingPeerFingerprint
            this.pendingPeerPin = pendingPeerPin
        }
    }

    @Keep
    fun onPrivacyState(
        historyEnabled: Boolean,
        readReceiptsEnabled: Boolean,
        screenshotAlertsEnabled: Boolean,
        privacyLastSeen: String,
        privacyProfilePhoto: String,
        privacyGroupInvites: String
    ) {
        runOnMain {
            this.historyEnabled = historyEnabled
            this.readReceiptsEnabled = readReceiptsEnabled
            this.screenshotAlertsEnabled = screenshotAlertsEnabled
            this.privacyLastSeen = privacyLastSeen
            this.privacyProfilePhoto = privacyProfilePhoto
            this.privacyGroupInvites = privacyGroupInvites
        }
    }

    @Keep
    fun onPendingCallUpdated(call: IncomingCall?) {
        runOnMain { pendingCall = call }
    }

    @Keep
    fun onActivePeerCallUpdated(call: PeerCallState?) {
        runOnMain { activePeerCall = call }
    }

    @Keep
    fun onActiveGroupCallUpdated(call: GroupCallState?) {
        runOnMain { activeGroupCall = call }
    }

    @Keep
    fun onGroupCallRoomsUpdated(rooms: Array<GroupCallRoomUi>) {
        runOnMain { groupCallRooms.replaceWith(rooms) }
    }

    @Keep
    fun onFriendsUpdated(items: Array<FriendUi>) {
        runOnMain { friends.replaceWith(items) }
    }

    @Keep
    fun onFriendRequestsUpdated(items: Array<FriendRequestUi>) {
        runOnMain { friendRequests.replaceWith(items) }
    }

    @Keep
    fun onGroupsUpdated(items: Array<GroupUi>) {
        runOnMain { groups.replaceWith(items) }
    }

    @Keep
    fun onDevicesUpdated(items: Array<DeviceUi>) {
        runOnMain { devices.replaceWith(items) }
    }

    @Keep
    fun onPairingRequestsUpdated(items: Array<PairingRequestUi>) {
        runOnMain { pairingRequests.replaceWith(items) }
    }

    @Keep
    fun onConversationsUpdated(items: Array<ConversationPreview>) {
        runOnMain { conversations.replaceWith(items) }
    }

    @Keep
    fun onBlockedUsersUpdated(items: Array<String>) {
        runOnMain {
            blockedUsers.clear()
            items.forEach { username ->
                blockedUsers[username] = true
            }
        }
    }

    @Keep
    fun onChatItemsUpdated(convId: String, items: Array<ChatItem>) {
        runOnMain {
            chatItems.getOrPut(convId) { mutableStateListOf() }.replaceWith(items)
        }
    }

    @Keep
    fun onGroupItemsUpdated(groupId: String, items: Array<GroupChatItem>) {
        runOnMain {
            groupItems.getOrPut(groupId) { mutableStateListOf() }.replaceWith(items)
        }
    }

    @Keep
    fun onGroupMembersUpdated(groupId: String, items: Array<GroupMemberUi>) {
        runOnMain {
            groupMembers.getOrPut(groupId) { mutableStateListOf() }.replaceWith(items)
        }
    }

    @Keep
    fun onMediaRelayEventsUpdated(items: Array<MediaRelayLog>) {
        runOnMain { mediaRelayEvents.replaceWith(items) }
    }

    @Keep
    fun onOfflinePayloadsUpdated(items: Array<OfflinePayloadLog>) {
        runOnMain { offlinePayloads.replaceWith(items) }
    }

    private fun hexToBytes(hex: String): ByteArray {
        val cleaned = hex.trim().lowercase()
        if (cleaned.isEmpty()) return ByteArray(0)
        val output = ByteArray(cleaned.length / 2)
        var i = 0
        while (i < cleaned.length) {
            val hi = cleaned[i].digitToIntOrNull(16) ?: 0
            val lo = cleaned[i + 1].digitToIntOrNull(16) ?: 0
            output[i / 2] = ((hi shl 4) or lo).toByte()
            i += 2
        }
        return output
    }

    private fun <T> SnapshotStateList<T>.replaceWith(items: Array<out T>) {
        clear()
        addAll(items)
    }
}
