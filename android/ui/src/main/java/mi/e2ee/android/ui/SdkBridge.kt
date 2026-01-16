package mi.e2ee.android.ui

import android.content.Context
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.runtime.snapshots.SnapshotStateList
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.abs
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancelChildren
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mi.e2ee.android.sdk.DeviceEntry
import mi.e2ee.android.sdk.DevicePairingRequest
import mi.e2ee.android.sdk.FriendEntry
import mi.e2ee.android.sdk.FriendRequestEntry
import mi.e2ee.android.sdk.GroupCallInfo
import mi.e2ee.android.sdk.GroupCallSignalResult
import mi.e2ee.android.sdk.GroupMemberEntry
import mi.e2ee.android.sdk.HistoryEntry
import mi.e2ee.android.sdk.HistoryKind
import mi.e2ee.android.sdk.HistoryStatus
import mi.e2ee.android.sdk.MediaConfig
import mi.e2ee.android.sdk.MediaPacket
import mi.e2ee.android.sdk.MiEventType
import mi.e2ee.android.sdk.NativeSdk
import mi.e2ee.android.sdk.SdkEvent
import mi.e2ee.android.sdk.SdkVersion
import mi.e2ee.android.sdk.SyncFriendsResult

private const val PREFS_NAME = "mi_chat_prefs"
private const val PREFS_HISTORY_ENABLED = "history_enabled"
private const val PREFS_READ_RECEIPTS = "read_receipts"
private const val PREFS_SCREENSHOT_ALERTS = "screenshot_alerts"
private const val PREFS_PRIVACY_LAST_SEEN = "privacy_last_seen"
private const val PREFS_PRIVACY_PROFILE = "privacy_profile"
private const val PREFS_PRIVACY_INVITES = "privacy_invites"

private const val DEFAULT_CONFIG_FILE = "client_config.ini"
private const val DEFAULT_CONFIG_DIR = "config"

private const val RECALL_PREFIX = "[recall]:"
private const val CALL_VOICE_PREFIX = "[call]voice:"
private const val CALL_VIDEO_PREFIX = "[call]video:"
private const val CALL_END_PREFIX = "[call]end:"

private const val POLL_INTERVAL_MS = 600L
private const val FRIEND_SYNC_INTERVAL_MS = 2_000L
private const val REQUEST_SYNC_INTERVAL_MS = 4_000L
private const val HEARTBEAT_INTERVAL_MS = 5_000L

class SdkBridge(private val context: Context) {
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

    private val chatItems = mutableStateMapOf<String, SnapshotStateList<ChatItem>>()
    private val groupItems = mutableStateMapOf<String, SnapshotStateList<GroupChatItem>>()
    private val groupMembers = mutableStateMapOf<String, SnapshotStateList<GroupMemberUi>>()

    private var activeConversationId: String? = null
    private var activeConversationIsGroup = false

    private var handle: Long = 0L
    private var pollJob: Job? = null
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    private val timeFormat = SimpleDateFormat("HH:mm", Locale.getDefault())

    fun init(configPath: String? = null): Boolean {
        if (!NativeSdk.available) {
            seedMockData()
            statusMessage = "Native SDK not loaded; using mock data"
            return false
        }
        sdkVersion = runCatching { NativeSdk.getVersion() }.getOrNull()
        capabilities = runCatching { NativeSdk.getCapabilities() }.getOrDefault(0)
        val resolvedPath = ensureConfigFile(configPath)
        handle = NativeSdk.createClient(resolvedPath)
        if (handle == 0L) {
            lastError = NativeSdk.lastCreateError()
            statusMessage = "Init failed"
            return false
        }
        initialized = true
        deviceId = NativeSdk.deviceId(handle)
        remoteOk = NativeSdk.remoteOk(handle)
        remoteError = NativeSdk.remoteError(handle)
        loadPrivacyPrefs()
        NativeSdk.setHistoryEnabled(handle, historyEnabled)
        refreshTrustState()
        statusMessage = null
        return true
    }

    fun dispose() {
        stopPolling()
        if (handle != 0L) {
            NativeSdk.logout(handle)
            NativeSdk.destroyClient(handle)
            handle = 0L
        }
        scope.coroutineContext.cancelChildren()
    }

    fun ensureLoggedOut() {
        stopPolling()
        loggedIn = false
        token = ""
        username = ""
        activeConversationId = null
        activeConversationIsGroup = false
    }

    fun register(usernameInput: String, password: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.register(handle, usernameInput, password)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        statusMessage = if (ok) "Register success" else "Register failed"
        refreshTrustState()
        return ok
    }

    fun login(usernameInput: String, password: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.login(handle, usernameInput, password)
        if (!ok) {
            lastError = NativeSdk.lastError(handle)
            statusMessage = "Login failed"
            ensureLoggedOut()
            return false
        }
        token = NativeSdk.token(handle)
        username = usernameInput.trim()
        loggedIn = true
        statusMessage = "Login success"
        lastError = ""
        startPolling()
        refreshFriends(true)
        refreshFriendRequests()
        refreshDevices()
        refreshTrustState()
        return true
    }

    fun logout() {
        if (handle != 0L) {
            NativeSdk.logout(handle)
        }
        ensureLoggedOut()
    }

    fun relogin(): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.relogin(handle)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun heartbeat(): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.heartbeat(handle)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun trustPendingServer(pin: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.trustPendingServer(handle, pin)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        refreshTrustState()
        return ok
    }

    fun trustPendingPeer(pin: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.trustPendingPeer(handle, pin)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        refreshTrustState()
        return ok
    }

    fun setActiveConversation(convId: String, isGroup: Boolean) {
        activeConversationId = convId
        activeConversationIsGroup = isGroup
        updateConversation(convId, isGroup) { it.copy(unreadCount = 0, mentionCount = 0, isTyping = false) }
    }

    fun clearActiveConversation() {
        activeConversationId = null
        activeConversationIsGroup = false
    }

    fun chatItemsFor(convId: String): SnapshotStateList<ChatItem> {
        return chatItems.getOrPut(convId) { mutableStateListOf(SystemNotice("notice_${convId}", "Messages are end-to-end encrypted.")) }
    }

    fun groupItemsFor(groupId: String): SnapshotStateList<GroupChatItem> {
        return groupItems.getOrPut(groupId) { mutableStateListOf(GroupSystemNotice("notice_${groupId}", "Messages are end-to-end encrypted.")) }
    }

    fun groupMembersFor(groupId: String): SnapshotStateList<GroupMemberUi> {
        return groupMembers.getOrPut(groupId) { mutableStateListOf() }
    }

    fun loadHistory(convId: String, isGroup: Boolean) {
        if (!ensureHandle()) return
        val entries = NativeSdk.loadChatHistory(handle, convId, isGroup, 200)
        if (isGroup) {
            val list = groupItemsFor(convId)
            list.clear()
            list.add(GroupSystemNotice("notice_${convId}", "Messages are end-to-end encrypted."))
            list.addAll(entries.mapNotNull { historyToGroupItem(it) })
        } else {
            val list = chatItemsFor(convId)
            list.clear()
            list.add(SystemNotice("notice_${convId}", "Messages are end-to-end encrypted."))
            list.addAll(entries.mapNotNull { historyToChatItem(it) })
        }
    }
    fun sendText(convId: String, text: String, reply: ReplyPreview? = null, isGroup: Boolean = false): Boolean {
        if (!ensureHandle()) return false
        val trimmed = text.trim()
        if (trimmed.isEmpty()) return false
        val replyId = reply?.messageId
        val messageId = if (isGroup) {
            NativeSdk.sendGroupText(handle, convId, trimmed)
        } else if (!replyId.isNullOrBlank()) {
            NativeSdk.sendPrivateTextWithReply(
                handle,
                convId,
                trimmed,
                replyToMessageId = replyId,
                replyPreview = reply.snippet
            )
        } else {
            NativeSdk.sendPrivateText(handle, convId, trimmed)
        }
        val ok = messageId != null
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        if (ok) {
            val msg = ChatMessage(
                id = messageId ?: generateFallbackId(),
                sender = username.ifBlank { "Me" },
                body = trimmed,
                time = nowTimeLabel(),
                isMine = true,
                status = MessageStatus.Sent,
                replyTo = reply
            )
            if (isGroup) {
                val groupMsg = GroupMessage(
                    id = msg.id,
                    sender = msg.sender,
                    role = null,
                    body = msg.body,
                    time = msg.time,
                    isMine = true,
                    status = msg.status.name
                )
                groupItemsFor(convId).add(groupMsg)
                updateConversation(convId, true) { it.copy(lastMessage = msg.body, time = msg.time) }
            } else {
                chatItemsFor(convId).add(msg)
                updateConversation(convId, false) { it.copy(lastMessage = msg.body, time = msg.time) }
            }
        }
        return ok
    }

    fun sendRecall(convId: String, messageId: String, isGroup: Boolean): Boolean {
        if (!ensureHandle()) return false
        val payload = "$RECALL_PREFIX$messageId"
        val result = if (isGroup) {
            NativeSdk.sendGroupText(handle, convId, payload)
        } else {
            NativeSdk.sendPrivateText(handle, convId, payload)
        }
        val ok = result != null
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        if (ok) {
            applyRecall(convId, messageId, isGroup)
        }
        return ok
    }

    fun sendTyping(peerUsername: String, typing: Boolean) {
        if (!ensureHandle() || peerUsername.isBlank()) return
        NativeSdk.sendTyping(handle, peerUsername, typing)
    }

    fun sendPresence(peerUsername: String, online: Boolean) {
        if (!ensureHandle() || peerUsername.isBlank()) return
        NativeSdk.sendPresence(handle, peerUsername, online)
    }

    fun sendCallInvite(peerUsername: String, video: Boolean): Boolean {
        if (!ensureHandle() || peerUsername.isBlank()) return false
        val payload = if (video) CALL_VIDEO_PREFIX else CALL_VOICE_PREFIX
        val result = NativeSdk.sendPrivateText(handle, peerUsername, payload)
        val ok = result != null
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        if (ok) {
            appendSystemNotice(
                peerUsername,
                if (video) "Outgoing video call" else "Outgoing voice call",
                false
            )
        }
        return ok
    }

    fun sendReadReceipt(peerUsername: String, messageId: String) {
        if (!ensureHandle() || peerUsername.isBlank() || messageId.isBlank()) return
        if (!readReceiptsEnabled) return
        NativeSdk.sendReadReceipt(handle, peerUsername, messageId)
    }

    fun sendFile(convId: String, path: String, isGroup: Boolean): Boolean {
        if (!ensureHandle()) return false
        val result = if (isGroup) {
            NativeSdk.sendGroupFile(handle, convId, path)
        } else {
            NativeSdk.sendPrivateFile(handle, convId, path)
        }
        val ok = result != null
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun sendSticker(convId: String, stickerId: String): Boolean {
        if (!ensureHandle()) return false
        val result = NativeSdk.sendPrivateSticker(handle, convId, stickerId)
        val ok = result != null
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun sendLocation(convId: String, lat: Double, lon: Double, label: String, isGroup: Boolean): Boolean {
        if (!ensureHandle()) return false
        if (!lat.isFinite() || !lon.isFinite()) return false
        val latE7 = (lat * 10000000.0).toLong().toInt()
        val lonE7 = (lon * 10000000.0).toLong().toInt()
        val result = if (isGroup) {
            NativeSdk.sendGroupText(handle, convId, "Location: $label ($lat,$lon)")
        } else {
            NativeSdk.sendPrivateLocation(handle, convId, latE7, lonE7, label)
        }
        val ok = result != null
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun sendContact(convId: String, cardUsername: String, cardDisplay: String): Boolean {
        if (!ensureHandle()) return false
        val result = NativeSdk.sendPrivateContact(handle, convId, cardUsername, cardDisplay)
        val ok = result != null
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun addFriend(targetUsername: String, remark: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.addFriend(handle, targetUsername, remark)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        refreshFriends(true)
        return ok
    }

    fun setFriendRemark(friendUsername: String, remark: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.setFriendRemark(handle, friendUsername, remark)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        if (ok) {
            refreshFriends(true)
        }
        return ok
    }

    fun sendFriendRequest(targetUsername: String, remark: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.sendFriendRequest(handle, targetUsername, remark)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        refreshFriendRequests()
        return ok
    }

    fun respondFriendRequest(requester: String, accept: Boolean): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.respondFriendRequest(handle, requester, accept)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        refreshFriendRequests()
        if (accept) {
            refreshFriends(true)
        }
        return ok
    }

    fun deleteFriend(friendUsername: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.deleteFriend(handle, friendUsername)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        refreshFriends(true)
        return ok
    }

    fun setUserBlocked(username: String, blocked: Boolean): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.setUserBlocked(handle, username, blocked)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        if (ok) {
            blockedUsers[username] = blocked
        }
        return ok
    }

    fun joinGroup(groupId: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.joinGroup(handle, groupId)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        if (ok) {
            ensureGroup(groupId)
        }
        return ok
    }

    fun leaveGroup(groupId: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.leaveGroup(handle, groupId)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun createGroup(): String? {
        if (!ensureHandle()) return null
        val groupId = NativeSdk.createGroup(handle)
        lastError = if (groupId != null) "" else NativeSdk.lastError(handle)
        if (groupId != null) {
            ensureGroup(groupId)
        }
        return groupId
    }

    fun sendGroupInvite(groupId: String, peerUsername: String): String? {
        if (!ensureHandle()) return null
        val messageId = NativeSdk.sendGroupInvite(handle, groupId, peerUsername)
        lastError = if (messageId != null) "" else NativeSdk.lastError(handle)
        return messageId
    }

    fun refreshGroupMembers(groupId: String) {
        if (!ensureHandle()) return
        val list = groupMembersFor(groupId)
        val entries = NativeSdk.listGroupMembers(handle, groupId)
        list.clear()
        list.addAll(entries.map { GroupMemberUi(it.username, it.role) })
    }

    fun setGroupMemberRole(groupId: String, peerUsername: String, role: Int): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.setGroupMemberRole(handle, groupId, peerUsername, role)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        if (ok) {
            refreshGroupMembers(groupId)
        }
        return ok
    }

    fun kickGroupMember(groupId: String, peerUsername: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.kickGroupMember(handle, groupId, peerUsername)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        if (ok) {
            refreshGroupMembers(groupId)
        }
        return ok
    }

    fun refreshDevices() {
        if (!ensureHandle()) return
        val entries = NativeSdk.listDevices(handle)
        devices.clear()
        devices.addAll(entries.map { DeviceUi(it.deviceId, it.lastSeenSec) })
    }

    fun kickDevice(deviceId: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.kickDevice(handle, deviceId)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        if (ok) {
            refreshDevices()
        }
        return ok
    }

    fun deleteChatHistory(convId: String, isGroup: Boolean, deleteAttachments: Boolean, secureWipe: Boolean): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.deleteChatHistory(handle, convId, isGroup, deleteAttachments, secureWipe)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun setHistoryEnabled(enabled: Boolean): Boolean {
        historyEnabled = enabled
        prefs.edit().putBoolean(PREFS_HISTORY_ENABLED, enabled).apply()
        if (!ensureHandle()) return false
        val ok = NativeSdk.setHistoryEnabled(handle, enabled)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun clearAllHistory(deleteAttachments: Boolean, secureWipe: Boolean): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.clearAllHistory(handle, deleteAttachments, secureWipe)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun beginDevicePairingPrimary(): String? {
        if (!ensureHandle()) return null
        val code = NativeSdk.beginDevicePairingPrimary(handle)
        lastError = if (code != null) "" else NativeSdk.lastError(handle)
        return code
    }

    fun pollDevicePairingRequests() {
        if (!ensureHandle()) return
        val entries = NativeSdk.pollDevicePairingRequests(handle)
        pairingRequests.clear()
        pairingRequests.addAll(entries.map { PairingRequestUi(it.deviceId, it.requestIdHex) })
    }

    fun approveDevicePairingRequest(deviceId: String, requestId: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.approveDevicePairingRequest(handle, deviceId, requestId)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        pollDevicePairingRequests()
        return ok
    }

    fun beginDevicePairingLinked(pairingCode: String): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.beginDevicePairingLinked(handle, pairingCode)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun pollDevicePairingLinked(): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.pollDevicePairingLinked(handle)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun cancelDevicePairing() {
        if (!ensureHandle()) return
        NativeSdk.cancelDevicePairing(handle)
    }

    fun storeAttachmentPreviewBytes(fileId: String, fileName: String, fileSize: Long, bytes: ByteArray): Boolean {
        if (!ensureHandle()) return false
        val ok = NativeSdk.storeAttachmentPreviewBytes(handle, fileId, fileName, fileSize, bytes)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
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
        val ok = NativeSdk.downloadChatFileToPath(handle, fileId, fileKey, fileName, fileSize, outPath, wipeAfterRead)
        lastError = if (ok) "" else NativeSdk.lastError(handle)
        return ok
    }

    fun downloadChatFileToBytes(
        fileId: String,
        fileKey: ByteArray,
        fileName: String,
        fileSize: Long,
        wipeAfterRead: Boolean
    ): ByteArray? {
        if (!ensureHandle()) return null
        val bytes = NativeSdk.downloadChatFileToBytes(handle, fileId, fileKey, fileName, fileSize, wipeAfterRead)
        lastError = if (bytes != null) "" else NativeSdk.lastError(handle)
        return bytes
    }

    fun getMediaConfig(): MediaConfig? {
        if (!ensureHandle()) return null
        return NativeSdk.getMediaConfig(handle)
    }

    fun deriveMediaRoot(peerUsername: String, callId: ByteArray): ByteArray? {
        if (!ensureHandle()) return null
        return NativeSdk.deriveMediaRoot(handle, peerUsername, callId)
    }

    fun pushMedia(peerUsername: String, callId: ByteArray, packet: ByteArray): Boolean {
        if (!ensureHandle()) return false
        return NativeSdk.pushMedia(handle, peerUsername, callId, packet)
    }

    fun pullMedia(callId: ByteArray, maxPackets: Int, waitMs: Int): Array<MediaPacket> {
        if (!ensureHandle()) return emptyArray()
        return NativeSdk.pullMedia(handle, callId, maxPackets, waitMs)
    }

    fun pushGroupMedia(groupId: String, callId: ByteArray, packet: ByteArray): Boolean {
        if (!ensureHandle()) return false
        return NativeSdk.pushGroupMedia(handle, groupId, callId, packet)
    }

    fun pullGroupMedia(callId: ByteArray, maxPackets: Int, waitMs: Int): Array<MediaPacket> {
        if (!ensureHandle()) return emptyArray()
        return NativeSdk.pullGroupMedia(handle, callId, maxPackets, waitMs)
    }

    fun addMediaSubscription(callId: ByteArray, isGroup: Boolean, groupId: String? = null): Boolean {
        if (!ensureHandle()) return false
        return NativeSdk.addMediaSubscription(handle, callId, isGroup, groupId)
    }

    fun clearMediaSubscriptions() {
        if (!ensureHandle()) return
        NativeSdk.clearMediaSubscriptions(handle)
    }

    fun startGroupCall(groupId: String, video: Boolean): GroupCallInfo? {
        if (!ensureHandle()) return null
        val info = NativeSdk.startGroupCall(handle, groupId, video)
        lastError = if (info != null) "" else NativeSdk.lastError(handle)
        if (info != null) {
            val room = GroupCallRoomUi(
                groupId = groupId,
                callId = bytesToHex(info.callId),
                video = video
            )
            val existing = groupCallRooms.indexOfFirst { it.groupId == groupId }
            if (existing == -1) {
                groupCallRooms.add(room)
            } else {
                groupCallRooms[existing] = room
            }
        }
        return info
    }

    fun joinGroupCall(groupId: String, callId: ByteArray, video: Boolean): GroupCallInfo? {
        if (!ensureHandle()) return null
        val info = NativeSdk.joinGroupCall(handle, groupId, callId, video)
        lastError = if (info != null) "" else NativeSdk.lastError(handle)
        if (info != null) {
            val room = GroupCallRoomUi(
                groupId = groupId,
                callId = bytesToHex(callId),
                video = video
            )
            val existing = groupCallRooms.indexOfFirst { it.groupId == groupId }
            if (existing == -1) {
                groupCallRooms.add(room)
            } else {
                groupCallRooms[existing] = room
            }
        }
        return info
    }

    fun joinGroupCallHex(groupId: String, callIdHex: String, video: Boolean): GroupCallInfo? {
        return joinGroupCall(groupId, hexToBytes(callIdHex), video)
    }

    fun leaveGroupCallHex(groupId: String, callIdHex: String): Boolean {
        return leaveGroupCall(groupId, hexToBytes(callIdHex))
    }

    fun leaveGroupCall(groupId: String, callId: ByteArray): Boolean {
        if (!ensureHandle()) return false
        return NativeSdk.leaveGroupCall(handle, groupId, callId)
    }

    fun getGroupCallKey(groupId: String, callId: ByteArray, keyId: Int): ByteArray? {
        if (!ensureHandle()) return null
        return NativeSdk.getGroupCallKey(handle, groupId, callId, keyId)
    }

    fun rotateGroupCallKey(groupId: String, callId: ByteArray, keyId: Int, members: Array<String>): Boolean {
        if (!ensureHandle()) return false
        return NativeSdk.rotateGroupCallKey(handle, groupId, callId, keyId, members)
    }

    fun requestGroupCallKey(groupId: String, callId: ByteArray, keyId: Int, members: Array<String>): Boolean {
        if (!ensureHandle()) return false
        return NativeSdk.requestGroupCallKey(handle, groupId, callId, keyId, members)
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
        return NativeSdk.sendGroupCallSignal(handle, op, groupId, callId, video, keyId, seq, tsMs, ext)
    }

    fun setReadReceiptsEnabled(enabled: Boolean) {
        readReceiptsEnabled = enabled
        prefs.edit().putBoolean(PREFS_READ_RECEIPTS, enabled).apply()
    }

    fun setScreenshotAlertsEnabled(enabled: Boolean) {
        screenshotAlertsEnabled = enabled
        prefs.edit().putBoolean(PREFS_SCREENSHOT_ALERTS, enabled).apply()
    }

    fun setPrivacyLastSeen(value: String) {
        privacyLastSeen = value
        prefs.edit().putString(PREFS_PRIVACY_LAST_SEEN, value).apply()
    }

    fun setPrivacyProfilePhoto(value: String) {
        privacyProfilePhoto = value
        prefs.edit().putString(PREFS_PRIVACY_PROFILE, value).apply()
    }

    fun setPrivacyGroupInvites(value: String) {
        privacyGroupInvites = value
        prefs.edit().putString(PREFS_PRIVACY_INVITES, value).apply()
    }
    private fun startPolling() {
        stopPolling()
        pollJob = scope.launch {
            var lastFriendSync = 0L
            var lastRequestSync = 0L
            var lastHeartbeat = 0L
            while (isActive && loggedIn) {
                withContext(Dispatchers.IO) {
                    if (handle != 0L) {
                        val events = NativeSdk.pollEvents(handle, 64, 0)
                        if (events.isNotEmpty()) {
                            withContext(Dispatchers.Main) {
                                handleEvents(events)
                            }
                        }
                    }
                }
                val now = System.currentTimeMillis()
                if (now - lastFriendSync > FRIEND_SYNC_INTERVAL_MS) {
                    refreshFriends(false)
                    lastFriendSync = now
                }
                if (now - lastRequestSync > REQUEST_SYNC_INTERVAL_MS) {
                    refreshFriendRequests()
                    lastRequestSync = now
                }
                if (now - lastHeartbeat > HEARTBEAT_INTERVAL_MS) {
                    heartbeat()
                    lastHeartbeat = now
                }
                delay(POLL_INTERVAL_MS)
            }
        }
    }

    private fun stopPolling() {
        pollJob?.cancel()
        pollJob = null
    }

    private fun handleEvents(events: Array<SdkEvent>) {
        events.forEach { event ->
            when (event.type) {
                MiEventType.CHAT_TEXT -> handleIncomingChatText(event)
                MiEventType.CHAT_FILE -> handleIncomingChatFile(event)
                MiEventType.CHAT_STICKER -> handleIncomingChatSticker(event)
                MiEventType.GROUP_TEXT -> handleIncomingGroupText(event)
                MiEventType.GROUP_FILE -> handleIncomingGroupFile(event)
                MiEventType.GROUP_INVITE -> handleGroupInvite(event)
                MiEventType.GROUP_NOTICE -> handleGroupNotice(event)
                MiEventType.OUTGOING_TEXT -> handleOutgoingText(event)
                MiEventType.OUTGOING_FILE -> handleOutgoingFile(event)
                MiEventType.OUTGOING_STICKER -> handleOutgoingSticker(event)
                MiEventType.OUTGOING_GROUP_TEXT -> handleOutgoingGroupText(event)
                MiEventType.OUTGOING_GROUP_FILE -> handleOutgoingGroupFile(event)
                MiEventType.DELIVERY -> handleDelivery(event)
                MiEventType.READ_RECEIPT -> handleReadReceipt(event)
                MiEventType.TYPING -> handleTyping(event)
                MiEventType.PRESENCE -> handlePresence(event)
                MiEventType.GROUP_CALL -> handleGroupCall(event)
            }
        }
    }

    private fun handleIncomingChatText(event: SdkEvent) {
        val text = event.text
        if (text.startsWith(RECALL_PREFIX)) {
            val targetId = text.removePrefix(RECALL_PREFIX).trim()
            if (targetId.isNotEmpty()) {
                applyRecall(event.sender, targetId, false)
                return
            }
        }
        if (text.startsWith(CALL_END_PREFIX)) {
            appendSystemNotice(event.sender, "Call ended", false)
            return
        }
        if (text.startsWith(CALL_VOICE_PREFIX) || text.startsWith(CALL_VIDEO_PREFIX)) {
            appendSystemNotice(event.sender, "Incoming call", false)
            return
        }
        val msg = ChatMessage(
            id = event.messageId,
            sender = event.sender,
            body = text,
            time = nowTimeLabel(),
            isMine = false,
            status = MessageStatus.Delivered
        )
        appendChatMessage(event.sender, msg)
        if (activeConversationId == event.sender && !activeConversationIsGroup) {
            sendReadReceipt(event.sender, event.messageId)
        }
    }

    private fun handleIncomingChatFile(event: SdkEvent) {
        val attachment = Attachment(
            kind = AttachmentKind.File,
            label = event.fileName.ifBlank { "File" },
            meta = humanSize(event.fileSize),
            fileId = event.fileId,
            fileKeyHex = bytesToHex(event.fileKey),
            fileSize = event.fileSize
        )
        val msg = ChatMessage(
            id = event.messageId,
            sender = event.sender,
            body = "",
            time = nowTimeLabel(),
            isMine = false,
            status = MessageStatus.Delivered,
            attachment = attachment
        )
        appendChatMessage(event.sender, msg)
        if (activeConversationId == event.sender && !activeConversationIsGroup) {
            sendReadReceipt(event.sender, event.messageId)
        }
    }

    private fun handleIncomingChatSticker(event: SdkEvent) {
        val attachment = Attachment(
            kind = AttachmentKind.Sticker,
            label = event.stickerId.ifBlank { "Sticker" },
            meta = "",
            fileId = "",
            fileKeyHex = ""
        )
        val msg = ChatMessage(
            id = event.messageId,
            sender = event.sender,
            body = "",
            time = nowTimeLabel(),
            isMine = false,
            status = MessageStatus.Delivered,
            attachment = attachment
        )
        appendChatMessage(event.sender, msg)
        if (activeConversationId == event.sender && !activeConversationIsGroup) {
            sendReadReceipt(event.sender, event.messageId)
        }
    }

    private fun handleOutgoingText(event: SdkEvent) {
        val text = event.text
        if (text.startsWith(RECALL_PREFIX) ||
            text.startsWith(CALL_END_PREFIX) ||
            text.startsWith(CALL_VOICE_PREFIX) ||
            text.startsWith(CALL_VIDEO_PREFIX)
        ) {
            return
        }
        val msg = ChatMessage(
            id = event.messageId,
            sender = username.ifBlank { "Me" },
            body = text,
            time = nowTimeLabel(),
            isMine = true,
            status = MessageStatus.Sent
        )
        appendChatMessage(event.peer, msg, outgoing = true)
    }

    private fun handleOutgoingFile(event: SdkEvent) {
        val attachment = Attachment(
            kind = AttachmentKind.File,
            label = event.fileName.ifBlank { "File" },
            meta = humanSize(event.fileSize),
            fileId = event.fileId,
            fileKeyHex = bytesToHex(event.fileKey),
            fileSize = event.fileSize
        )
        val msg = ChatMessage(
            id = event.messageId,
            sender = username.ifBlank { "Me" },
            body = "",
            time = nowTimeLabel(),
            isMine = true,
            status = MessageStatus.Sent,
            attachment = attachment
        )
        appendChatMessage(event.peer, msg, outgoing = true)
    }

    private fun handleOutgoingSticker(event: SdkEvent) {
        val attachment = Attachment(
            kind = AttachmentKind.Sticker,
            label = event.stickerId.ifBlank { "Sticker" },
            meta = "",
            fileId = "",
            fileKeyHex = ""
        )
        val msg = ChatMessage(
            id = event.messageId,
            sender = username.ifBlank { "Me" },
            body = "",
            time = nowTimeLabel(),
            isMine = true,
            status = MessageStatus.Sent,
            attachment = attachment
        )
        appendChatMessage(event.peer, msg, outgoing = true)
    }
    private fun handleIncomingGroupText(event: SdkEvent) {
        val text = event.text
        if (text.startsWith(RECALL_PREFIX)) {
            val targetId = text.removePrefix(RECALL_PREFIX).trim()
            if (targetId.isNotEmpty()) {
                applyRecall(event.groupId, targetId, true)
                return
            }
        }
        val msg = GroupMessage(
            id = event.messageId,
            sender = event.sender,
            role = null,
            body = text,
            time = nowTimeLabel(),
            isMine = false,
            status = ""
        )
        appendGroupMessage(event.groupId, msg)
    }

    private fun handleIncomingGroupFile(event: SdkEvent) {
        val attachment = Attachment(
            kind = AttachmentKind.File,
            label = event.fileName.ifBlank { "File" },
            meta = humanSize(event.fileSize),
            fileId = event.fileId,
            fileKeyHex = bytesToHex(event.fileKey),
            fileSize = event.fileSize
        )
        val msg = GroupMessage(
            id = event.messageId,
            sender = event.sender,
            role = null,
            body = "",
            time = nowTimeLabel(),
            isMine = false,
            status = "",
            attachment = attachment
        )
        appendGroupMessage(event.groupId, msg)
    }

    private fun handleOutgoingGroupText(event: SdkEvent) {
        val text = event.text
        if (text.startsWith(RECALL_PREFIX)) {
            return
        }
        val msg = GroupMessage(
            id = event.messageId,
            sender = username.ifBlank { "Me" },
            role = null,
            body = text,
            time = nowTimeLabel(),
            isMine = true,
            status = "Sent"
        )
        appendGroupMessage(event.groupId, msg, outgoing = true)
    }

    private fun handleOutgoingGroupFile(event: SdkEvent) {
        val attachment = Attachment(
            kind = AttachmentKind.File,
            label = event.fileName.ifBlank { "File" },
            meta = humanSize(event.fileSize),
            fileId = event.fileId,
            fileKeyHex = bytesToHex(event.fileKey),
            fileSize = event.fileSize
        )
        val msg = GroupMessage(
            id = event.messageId,
            sender = username.ifBlank { "Me" },
            role = null,
            body = "",
            time = nowTimeLabel(),
            isMine = true,
            status = "Sent",
            attachment = attachment
        )
        appendGroupMessage(event.groupId, msg, outgoing = true)
    }

    private fun handleGroupInvite(event: SdkEvent) {
        appendGroupNotice(event.groupId, "Group invite from ${event.sender}")
    }

    private fun handleGroupNotice(event: SdkEvent) {
        val text = when (event.noticeKind) {
            1 -> "${event.target} joined the group"
            2 -> "${event.target} left the group"
            3 -> "${event.target} was removed"
            4 -> "${event.target} role changed"
            else -> "Group updated"
        }
        appendGroupNotice(event.groupId, text)
    }

    private fun handleDelivery(event: SdkEvent) {
        updateMessageStatus(event.peer, event.messageId, MessageStatus.Delivered)
    }

    private fun handleReadReceipt(event: SdkEvent) {
        updateMessageStatus(event.peer, event.messageId, MessageStatus.Read)
    }

    private fun handleTyping(event: SdkEvent) {
        updateConversation(event.peer, false) {
            it.copy(isTyping = event.typing)
        }
    }

    private fun handlePresence(event: SdkEvent) {
        val status = if (event.online) "Online" else "Offline"
        updateConversation(event.peer, false) { it.copy(isTyping = false) }
        val index = friends.indexOfFirst { it.username == event.peer }
        if (index != -1) {
            val current = friends[index]
            friends[index] = current.copy(status = status)
        }
    }

    private fun handleGroupCall(event: SdkEvent) {
        if (event.groupId.isBlank()) return
        val existing = groupCallRooms.indexOfFirst { it.groupId == event.groupId }
        if (event.callOp == 0 || event.callOp == 4) {
            if (existing != -1) {
                groupCallRooms.removeAt(existing)
            }
            return
        }
        val room = GroupCallRoomUi(
            groupId = event.groupId,
            callId = bytesToHex(event.callId),
            video = (event.callMediaFlags and 0x2) != 0
        )
        if (existing == -1) {
            groupCallRooms.add(room)
        } else {
            groupCallRooms[existing] = room
        }
    }

    private fun appendChatMessage(convId: String, message: ChatMessage, outgoing: Boolean = false) {
        val list = chatItemsFor(convId)
        list.add(message)
        updateConversation(convId, false) {
            val unread = if (shouldIncrementUnread(convId, false, outgoing)) it.unreadCount + 1 else it.unreadCount
            it.copy(lastMessage = previewMessage(message), time = message.time, unreadCount = unread, isTyping = false)
        }
    }

    private fun appendGroupMessage(groupId: String, message: GroupMessage, outgoing: Boolean = false) {
        val list = groupItemsFor(groupId)
        list.add(message)
        updateConversation(groupId, true) {
            val unread = if (shouldIncrementUnread(groupId, true, outgoing)) it.unreadCount + 1 else it.unreadCount
            it.copy(lastMessage = previewMessage(message), time = message.time, unreadCount = unread)
        }
    }

    private fun appendSystemNotice(convId: String, text: String, isGroup: Boolean) {
        if (isGroup) {
            groupItemsFor(convId).add(GroupSystemNotice("sys_${generateFallbackId()}", text))
        } else {
            chatItemsFor(convId).add(SystemNotice("sys_${generateFallbackId()}", text))
        }
    }

    private fun appendGroupNotice(groupId: String, text: String) {
        groupItemsFor(groupId).add(GroupSystemNotice("notice_${generateFallbackId()}", text))
        ensureGroup(groupId)
    }

    private fun updateMessageStatus(convId: String, messageId: String, status: MessageStatus) {
        if (convId.isBlank() || messageId.isBlank()) return
        val list = chatItemsFor(convId)
        val index = list.indexOfFirst { it is ChatMessage && it.id == messageId }
        if (index != -1) {
            val current = list[index] as ChatMessage
            list[index] = current.copy(status = status)
        }
    }

    private fun applyRecall(convId: String, messageId: String, isGroup: Boolean) {
        if (messageId.isBlank()) return
        if (isGroup) {
            val list = groupItemsFor(convId)
            val index = list.indexOfFirst { it is GroupMessage && it.id == messageId }
            if (index != -1) {
                val current = list[index] as GroupMessage
                list[index] = recalledGroupMessageCopy(current)
            }
        } else {
            val list = chatItemsFor(convId)
            val index = list.indexOfFirst { it is ChatMessage && it.id == messageId }
            if (index != -1) {
                val current = list[index] as ChatMessage
                list[index] = recalledMessageCopy(current)
            }
        }
    }
    private fun refreshFriends(forceList: Boolean) {
        if (!ensureHandle()) return
        val sync = runCatching { NativeSdk.syncFriends(handle) }.getOrNull()
        val entries = if (sync != null) {
            if (sync.changed || forceList) sync.entries else NativeSdk.listFriends(handle)
        } else {
            NativeSdk.listFriends(handle)
        }
        updateFriends(entries)
    }

    private fun refreshFriendRequests() {
        if (!ensureHandle()) return
        updateFriendRequests(NativeSdk.listFriendRequests(handle))
    }

    private fun updateFriends(entries: Array<FriendEntry>) {
        friends.clear()
        friends.addAll(entries.map { FriendUi(it.username, it.remark, status = "") })
        entries.forEach { ensureConversation(it.username, false, it.remark) }
    }

    private fun updateFriendRequests(entries: Array<FriendRequestEntry>) {
        friendRequests.clear()
        friendRequests.addAll(entries.map { FriendRequestUi(it.requesterUsername, it.requesterRemark) })
    }

    private fun refreshTrustState() {
        if (!ensureHandle()) return
        hasPendingServerTrust = NativeSdk.hasPendingServerTrust(handle)
        pendingServerFingerprint = NativeSdk.pendingServerFingerprint(handle)
        pendingServerPin = NativeSdk.pendingServerPin(handle)
        hasPendingPeerTrust = NativeSdk.hasPendingPeerTrust(handle)
        pendingPeerUsername = NativeSdk.pendingPeerUsername(handle)
        pendingPeerFingerprint = NativeSdk.pendingPeerFingerprint(handle)
        pendingPeerPin = NativeSdk.pendingPeerPin(handle)
    }

    private fun ensureConversation(convId: String, isGroup: Boolean, displayName: String = convId) {
        val existing = conversations.indexOfFirst { it.id == convId }
        if (existing != -1) return
        val item = ConversationPreview(
            id = convId,
            initials = initials(displayName),
            name = displayName,
            lastMessage = "",
            time = "",
            unreadCount = 0,
            isPinned = prefs.getBoolean(keyPinned(convId), false),
            isMuted = prefs.getBoolean(keyMuted(convId), false),
            isGroup = isGroup,
            isTyping = false
        )
        conversations.add(item)
    }

    private fun updateConversation(convId: String, isGroup: Boolean, transform: (ConversationPreview) -> ConversationPreview) {
        val index = conversations.indexOfFirst { it.id == convId }
        if (index == -1) {
            ensureConversation(convId, isGroup)
            return
        }
        val updated = transform(conversations[index])
        conversations[index] = updated
        persistConversation(updated)
    }

    private fun ensureGroup(groupId: String) {
        val existing = groups.indexOfFirst { it.id == groupId }
        if (existing == -1) {
            groups.add(GroupUi(groupId, groupId))
        }
        ensureConversation(groupId, true, groupId)
    }

    fun togglePin(convId: String) {
        updateConversation(convId, isGroupConversation(convId)) { it.copy(isPinned = !it.isPinned) }
    }

    fun toggleMute(convId: String) {
        updateConversation(convId, isGroupConversation(convId)) { it.copy(isMuted = !it.isMuted) }
    }

    fun markRead(convId: String) {
        updateConversation(convId, isGroupConversation(convId)) { it.copy(unreadCount = 0, mentionCount = 0) }
    }

    private fun isGroupConversation(convId: String): Boolean {
        return conversations.firstOrNull { it.id == convId }?.isGroup ?: false
    }

    private fun shouldIncrementUnread(convId: String, isGroup: Boolean, outgoing: Boolean): Boolean {
        if (outgoing) return false
        return !(activeConversationId == convId && activeConversationIsGroup == isGroup)
    }

    private fun persistConversation(item: ConversationPreview) {
        prefs.edit()
            .putBoolean(keyPinned(item.id), item.isPinned)
            .putBoolean(keyMuted(item.id), item.isMuted)
            .putInt(keyUnread(item.id), item.unreadCount)
            .putInt(keyMention(item.id), item.mentionCount)
            .apply()
    }

    private fun keyPinned(id: String) = "conv_pinned_$id"
    private fun keyUnread(id: String) = "conv_unread_$id"
    private fun keyMention(id: String) = "conv_mention_$id"
    private fun keyMuted(id: String) = "conv_muted_$id"

    private fun previewMessage(message: ChatMessage): String {
        return when {
            message.isRevoked -> "Message removed"
            message.body.isNotBlank() -> message.body
            message.attachment != null -> message.attachment.label
            else -> "Message"
        }
    }

    private fun previewMessage(message: GroupMessage): String {
        return when {
            message.isRevoked -> "Message removed"
            message.body.isNotBlank() -> message.body
            message.attachment != null -> message.attachment.label
            else -> "Message"
        }
    }
    private fun nowTimeLabel(): String = timeFormat.format(Date())

    private fun initials(name: String): String {
        val trimmed = name.trim()
        if (trimmed.isEmpty()) return "?"
        val parts = trimmed.split(" ")
        val first = parts.firstOrNull()?.take(1)?.uppercase(Locale.getDefault()) ?: "?"
        val second = parts.getOrNull(1)?.take(1)?.uppercase(Locale.getDefault())
            ?: trimmed.drop(1).take(1).uppercase(Locale.getDefault())
        return (first + second).take(2)
    }

    private fun humanSize(size: Long): String {
        if (size <= 0) return "0 B"
        val units = arrayOf("B", "KB", "MB", "GB")
        var value = size.toDouble()
        var idx = 0
        while (value >= 1024 && idx < units.lastIndex) {
            value /= 1024
            idx += 1
        }
        return String.format(Locale.getDefault(), "%.1f %s", value, units[idx])
    }

    private fun bytesToHex(bytes: ByteArray): String {
        val sb = StringBuilder(bytes.size * 2)
        for (b in bytes) {
            sb.append(String.format("%02x", b))
        }
        return sb.toString()
    }

    private fun hexToBytes(hex: String): ByteArray {
        val cleaned = hex.trim().lowercase(Locale.getDefault())
        if (cleaned.isEmpty()) return ByteArray(0)
        val len = cleaned.length
        val out = ByteArray(len / 2)
        var i = 0
        while (i < len - 1) {
            val byte = cleaned.substring(i, i + 2).toInt(16)
            out[i / 2] = byte.toByte()
            i += 2
        }
        return out
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

    private fun generateFallbackId(): String {
        val now = System.currentTimeMillis()
        val rand = abs(now.hashCode())
        return "m_${now}_$rand"
    }

    private fun historyToChatItem(entry: HistoryEntry): ChatItem? {
        return when (entry.kind) {
            HistoryKind.TEXT -> ChatMessage(
                id = entry.messageId,
                sender = entry.sender,
                body = entry.text,
                time = timeFormat.format(Date(entry.timestampSec * 1000L)),
                isMine = entry.outgoing,
                status = historyStatus(entry.status)
            )
            HistoryKind.FILE -> ChatMessage(
                id = entry.messageId,
                sender = entry.sender,
                body = "",
                time = timeFormat.format(Date(entry.timestampSec * 1000L)),
                isMine = entry.outgoing,
                status = historyStatus(entry.status),
                attachment = Attachment(
                    kind = AttachmentKind.File,
                    label = entry.fileName.ifBlank { "File" },
                    meta = humanSize(entry.fileSize),
                    fileId = entry.fileId,
                    fileKeyHex = bytesToHex(entry.fileKey),
                    fileSize = entry.fileSize
                )
            )
            HistoryKind.STICKER -> ChatMessage(
                id = entry.messageId,
                sender = entry.sender,
                body = "",
                time = timeFormat.format(Date(entry.timestampSec * 1000L)),
                isMine = entry.outgoing,
                status = historyStatus(entry.status),
                attachment = Attachment(
                    kind = AttachmentKind.Sticker,
                    label = entry.stickerId.ifBlank { "Sticker" },
                    meta = ""
                )
            )
            HistoryKind.SYSTEM -> SystemNotice(
                id = entry.messageId,
                text = entry.text
            )
            else -> null
        }
    }

    private fun historyToGroupItem(entry: HistoryEntry): GroupChatItem? {
        return when (entry.kind) {
            HistoryKind.TEXT -> GroupMessage(
                id = entry.messageId,
                sender = entry.sender,
                role = null,
                body = entry.text,
                time = timeFormat.format(Date(entry.timestampSec * 1000L)),
                isMine = entry.outgoing,
                status = historyStatus(entry.status).name
            )
            HistoryKind.FILE -> GroupMessage(
                id = entry.messageId,
                sender = entry.sender,
                role = null,
                body = "",
                time = timeFormat.format(Date(entry.timestampSec * 1000L)),
                isMine = entry.outgoing,
                status = historyStatus(entry.status).name,
                attachment = Attachment(
                    kind = AttachmentKind.File,
                    label = entry.fileName.ifBlank { "File" },
                    meta = humanSize(entry.fileSize),
                    fileId = entry.fileId,
                    fileKeyHex = bytesToHex(entry.fileKey),
                    fileSize = entry.fileSize
                )
            )
            HistoryKind.SYSTEM -> GroupSystemNotice(
                id = entry.messageId,
                text = entry.text
            )
            else -> null
        }
    }

    private fun historyStatus(status: Int): MessageStatus {
        return when (status) {
            HistoryStatus.SENT -> MessageStatus.Sent
            HistoryStatus.DELIVERED -> MessageStatus.Delivered
            HistoryStatus.READ -> MessageStatus.Read
            HistoryStatus.FAILED -> MessageStatus.Failed
            else -> MessageStatus.Sent
        }
    }
    private fun ensureHandle(): Boolean {
        if (handle == 0L) {
            lastError = "SDK not initialized"
            return false
        }
        return true
    }

    private fun loadPrivacyPrefs() {
        historyEnabled = prefs.getBoolean(PREFS_HISTORY_ENABLED, true)
        readReceiptsEnabled = prefs.getBoolean(PREFS_READ_RECEIPTS, true)
        screenshotAlertsEnabled = prefs.getBoolean(PREFS_SCREENSHOT_ALERTS, true)
        privacyLastSeen = prefs.getString(PREFS_PRIVACY_LAST_SEEN, "contacts") ?: "contacts"
        privacyProfilePhoto = prefs.getString(PREFS_PRIVACY_PROFILE, "contacts") ?: "contacts"
        privacyGroupInvites = prefs.getString(PREFS_PRIVACY_INVITES, "contacts") ?: "contacts"
    }

    private fun ensureConfigFile(configPath: String?): String {
        if (!configPath.isNullOrBlank()) {
            return configPath
        }
        val baseDir = File(context.filesDir, DEFAULT_CONFIG_DIR)
        if (!baseDir.exists()) {
            baseDir.mkdirs()
        }
        val cfgFile = File(baseDir, DEFAULT_CONFIG_FILE)
        if (!cfgFile.exists()) {
            cfgFile.writeText(defaultConfig())
        }
        return cfgFile.absolutePath
    }

    private fun defaultConfig(): String {
        return """
[client]
server_ip=127.0.0.1
server_port=9000
use_tls=0
require_tls=0
trust_store=server_trust.ini
require_pinned_fingerprint=0
pinned_fingerprint=
auth_mode=opaque
allow_legacy_login=0

[proxy]
type=none
host=
port=0
username=
password=

[device_sync]
enabled=0
role=primary
key_path=e2ee_state/device_sync_key.bin

[identity]
rotation_days=90
legacy_retention_days=180
tpm_enable=0
tpm_require=0

[traffic]
cover_traffic_mode=auto
cover_traffic_interval_sec=30

[performance]
pqc_precompute_pool=2

[kt]
require_signature=0
gossip_alert_threshold=3
root_pubkey_hex=
root_pubkey_path=kt_root_pub.bin

[kcp]
enable=0
server_port=0
mtu=1400
snd_wnd=256
rcv_wnd=256
nodelay=1
interval=10
resend=2
nc=1
min_rto=30
request_timeout_ms=5000
session_idle_sec=60

[media]
audio_delay_ms=60
video_delay_ms=120
audio_max_frames=256
video_max_frames=256
pull_max_packets=32
pull_wait_ms=0
group_pull_max_packets=64
group_pull_wait_ms=0
""".trimIndent()
    }

    private fun seedMockData() {
        friends.clear()
        friendRequests.clear()
        groups.clear()
        conversations.clear()
        chatItems.clear()
        groupItems.clear()
        val sample = sampleConversations()
        conversations.addAll(sample)
        sample.forEach { conv ->
            if (conv.isGroup) {
                groupItems[conv.id] = mutableStateListOf<GroupChatItem>().apply {
                    addAll(SampleGroupChat.itemsFor(conv.id))
                }
            } else {
                chatItems[conv.id] = mutableStateListOf<ChatItem>().apply {
                    addAll(SampleChat.itemsFor(conv.id))
                }
            }
        }
        friends.addAll(
            listOf(
                FriendUi("aster", "Aster Stone", "Online"),
                FriendUi("lena", "Lena Novak", "Last seen 2h ago")
            )
        )
        groups.add(GroupUi("c4", "Project Aurora"))
        friendRequests.add(FriendRequestUi("kai", "Met in group call"))
    }
}

data class FriendUi(val username: String, val remark: String, val status: String)

data class FriendRequestUi(val username: String, val remark: String)

data class GroupUi(val id: String, val name: String)

data class GroupMemberUi(val username: String, val role: Int)

data class DeviceUi(val deviceId: String, val lastSeenSec: Int)

data class PairingRequestUi(val deviceId: String, val requestId: String)

data class GroupCallRoomUi(val groupId: String, val callId: String, val video: Boolean)
