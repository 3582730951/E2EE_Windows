package mi.e2ee.android.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import java.io.File
import mi.e2ee.android.BuildConfig
import mi.e2ee.android.sdk.GroupMemberRole

private sealed interface FlowScreen {
    data object Login : FlowScreen
    data object Register : FlowScreen
    data object Conversations : FlowScreen
    data class Chat(val conversationId: String) : FlowScreen
    data class GroupChat(val groupId: String) : FlowScreen
    data object Settings : FlowScreen
    data object Account : FlowScreen
    data object Privacy : FlowScreen
    data object Diagnostics : FlowScreen
    data object AddFriend : FlowScreen
    data object FriendRequests : FlowScreen
    data class ContactDetail(val username: String) : FlowScreen
    data class GroupDetail(val groupId: String) : FlowScreen
    data class AddGroupMembers(val groupId: String) : FlowScreen
    data class PeerCall(val callIdHex: String) : FlowScreen
    data class GroupCall(val groupId: String, val callIdHex: String) : FlowScreen
    data object BlockedUsers : FlowScreen
}

@Composable
fun UiHost(
    sdk: SdkBridge,
    themeMode: Int = ThemeMode.ForceDark,
    onThemeModeChange: (Int) -> Unit = {}
) {
    var stack by remember { mutableStateOf(listOf<FlowScreen>(FlowScreen.Login)) }
    val current = stack.last()
    val context = LocalContext.current
    val callController = remember(context, sdk) { CallMediaController(context, sdk) }

    fun navigate(screen: FlowScreen) {
        stack = stack + screen
    }

    fun resetTo(screen: FlowScreen) {
        stack = listOf(screen)
    }

    fun goBack() {
        if (stack.size > 1) {
            stack = stack.dropLast(1)
        }
    }

    LaunchedEffect(sdk.loggedIn) {
        if (sdk.loggedIn) {
            resetTo(FlowScreen.Conversations)
        } else {
            callController.stop()
            resetTo(FlowScreen.Login)
        }
    }

    fun selfInitials(): String {
        val trimmed = sdk.username.trim()
        return if (trimmed.isNotEmpty()) trimmed.take(2).uppercase() else "ME"
    }

    fun downloadAttachment(attachment: Attachment) {
        val baseDir = File(context.getExternalFilesDir(null), "downloads")
        if (!baseDir.exists()) {
            baseDir.mkdirs()
        }
        val name = attachment.label.ifBlank { "file.bin" }
        val outPath = File(baseDir, name).absolutePath
        sdk.downloadAttachmentToPath(attachment, outPath, wipeAfterRead = false)
    }

    Box(modifier = Modifier.fillMaxSize()) {
        when (current) {
        FlowScreen.Login -> LoginScreen(
            onRegister = { navigate(FlowScreen.Register) },
            onLogin = { username, password ->
                if (sdk.login(username, password)) {
                    resetTo(FlowScreen.Conversations)
                }
            },
            errorMessage = sdk.lastError.takeIf { it.isNotBlank() },
            statusMessage = sdk.statusMessage,
            remoteError = if (sdk.remoteOk) "" else sdk.remoteError
        )
        FlowScreen.Register -> RegisterScreen(
            onLogin = { goBack() },
            onCreateAccount = { username, password ->
                val ok = sdk.register(username, password)
                if (ok) {
                    sdk.login(username, password)
                    resetTo(FlowScreen.Conversations)
                }
            },
            errorMessage = sdk.lastError.takeIf { it.isNotBlank() },
            statusMessage = sdk.statusMessage
        )
        FlowScreen.Conversations -> ConversationListScreen(
            conversations = sdk.conversations,
            onTogglePin = { sdk.togglePin(it.id) },
            onToggleRead = { sdk.markRead(it.id) },
            onToggleMute = { sdk.toggleMute(it.id) },
            onDeleteConversation = { conversation ->
                sdk.deleteChatHistory(conversation.id, conversation.isGroup, deleteAttachments = true, secureWipe = false)
            },
            onOpenConversation = { conversation ->
                sdk.setActiveConversation(conversation.id, conversation.isGroup)
                sdk.loadHistory(conversation.id, conversation.isGroup)
                if (conversation.isGroup) {
                    sdk.refreshGroupMembers(conversation.id)
                    navigate(FlowScreen.GroupChat(conversation.id))
                } else {
                    navigate(FlowScreen.Chat(conversation.id))
                }
            },
            onOpenSettings = { navigate(FlowScreen.Settings) },
            onOpenContacts = { navigate(FlowScreen.AddFriend) },
            onOpenNewGroup = {
                val groupId = sdk.createGroup()
                if (groupId != null) {
                    navigate(FlowScreen.AddGroupMembers(groupId))
                }
            }
        )
        is FlowScreen.Chat -> {
            val convId = current.conversationId
            val conversation = sdk.conversations.firstOrNull { it.id == convId }
            val title = conversation?.name ?: convId
            val initials = conversation?.initials ?: title.take(2).uppercase()
            val status = sdk.friends.firstOrNull { it.username == convId }?.status
                ?: tr("chat_status_unknown", "Unknown")
            ChatScreen(
                items = sdk.chatItemsFor(convId),
                conversationId = convId,
                title = title,
                status = status,
                initials = initials,
                selfInitials = selfInitials(),
                showTyping = conversation?.isTyping ?: false,
                onBack = {
                    sdk.clearActiveConversation()
                    goBack()
                },
                onOpenAccount = { navigate(FlowScreen.Account) },
                onOpenSettings = { navigate(FlowScreen.Settings) },
                onStartCall = {
                    val state = sdk.startPeerCall(convId, video = false)
                    if (state != null) {
                        navigate(FlowScreen.PeerCall(state.callIdHex))
                    }
                },
                onStartVideoCall = {
                    val state = sdk.startPeerCall(convId, video = true)
                    if (state != null) {
                        navigate(FlowScreen.PeerCall(state.callIdHex))
                    }
                },
                onSendPresence = { online -> sdk.sendPresence(convId, online) },
                onSendReadReceipt = { messageId -> sdk.sendReadReceipt(convId, messageId) },
                onResendText = { messageId, text -> sdk.resendPrivateText(convId, messageId, text) },
                onResendTextWithReply = { messageId, text, replyId, preview ->
                    sdk.resendPrivateTextWithReply(convId, messageId, text, replyId, preview)
                },
                onResendFile = { messageId, filePath ->
                    sdk.resendPrivateFile(convId, messageId, filePath)
                },
                onSendMessage = { text, reply -> sdk.sendText(convId, text, reply, isGroup = false) },
                onSendFile = { path -> sdk.sendFile(convId, path, isGroup = false) },
                onSendLocation = { lat, lon, label -> sdk.sendLocation(convId, lat, lon, label, isGroup = false) },
                onSendSticker = { stickerId -> sdk.sendSticker(convId, stickerId) },
                onSendContact = { cardUsername, cardDisplay -> sdk.sendContact(convId, cardUsername, cardDisplay) },
                onTyping = { typing -> sdk.sendTyping(convId, typing) },
                onRecallMessage = { messageId -> sdk.sendRecall(convId, messageId, isGroup = false) },
                onDownloadAttachment = { attachment -> downloadAttachment(attachment) }
            )
        }
        is FlowScreen.GroupChat -> {
            val groupId = current.groupId
            val conversation = sdk.conversations.firstOrNull { it.id == groupId }
            val groupName = sdk.groups.firstOrNull { it.id == groupId }?.name ?: groupId
            val members = sdk.groupMembersFor(groupId)
            val subtitle = if (members.isNotEmpty()) {
                tr("group_member_count", "%d members / Secure group").format(members.size)
            } else {
                tr("group_member_count", "Secure group")
            }
            val activeCall = sdk.groupCallRooms.firstOrNull { it.groupId == groupId }
            GroupChatScreen(
                items = sdk.groupItemsFor(groupId),
                conversationId = groupId,
                title = conversation?.name ?: groupName,
                subtitle = subtitle,
                onBack = {
                    sdk.clearActiveConversation()
                    goBack()
                },
                onOpenGroupDetail = { navigate(FlowScreen.GroupDetail(groupId)) },
                activeCall = activeCall,
                onStartVoiceCall = {
                    val info = sdk.startGroupCall(groupId, video = false)
                    val active = sdk.activeGroupCall
                    if (info != null && active != null) {
                        navigate(FlowScreen.GroupCall(groupId, active.callIdHex))
                    }
                },
                onStartVideoCall = {
                    val info = sdk.startGroupCall(groupId, video = true)
                    val active = sdk.activeGroupCall
                    if (info != null && active != null) {
                        navigate(FlowScreen.GroupCall(groupId, active.callIdHex))
                    }
                },
                onJoinCall = { room ->
                    val info = sdk.joinGroupCallHex(groupId, room.callId, room.video)
                    val active = sdk.activeGroupCall
                    if (info != null && active != null) {
                        navigate(FlowScreen.GroupCall(groupId, active.callIdHex))
                    }
                },
                onLeaveCall = { room ->
                    sdk.leaveGroupCallHex(groupId, room.callId)
                    val active = sdk.activeGroupCall
                    if (active != null && active.groupId == groupId && active.callIdHex == room.callId) {
                        callController.stop()
                    }
                },
                onSendMessage = { text -> sdk.sendText(groupId, text, isGroup = true) },
                onSendFile = { path -> sdk.sendFile(groupId, path, isGroup = true) },
                onSendLocation = { lat, lon, label -> sdk.sendLocation(groupId, lat, lon, label, isGroup = true) },
                onRecallMessage = { messageId -> sdk.sendRecall(groupId, messageId, isGroup = true) },
                onResendText = { messageId, text -> sdk.resendGroupText(groupId, messageId, text) },
                onResendFile = { messageId, filePath -> sdk.resendGroupFile(groupId, messageId, filePath) },
                onDownloadAttachment = { attachment -> downloadAttachment(attachment) }
            )
        }
        is FlowScreen.PeerCall -> {
            val callState = sdk.activePeerCall
            if (callState == null || callState.callIdHex != current.callIdHex) {
                LaunchedEffect(callState?.callIdHex, current.callIdHex) {
                    callController.stop()
                    goBack()
                }
            } else {
                PeerCallScreen(
                    controller = callController,
                    call = callState,
                    onHangup = {
                        sdk.endPeerCall()
                        callController.stop()
                        goBack()
                    },
                    onAddSubscription = {
                        sdk.addMediaSubscription(callState.callId, isGroup = false)
                    },
                    onClearSubscriptions = { sdk.clearMediaSubscriptions() }
                )
            }
        }
        is FlowScreen.GroupCall -> {
            val callState = sdk.activeGroupCall
            if (callState == null || callState.callIdHex != current.callIdHex || callState.groupId != current.groupId) {
                LaunchedEffect(callState?.callIdHex, current.callIdHex, current.groupId) {
                    callController.stop()
                    goBack()
                }
            } else {
                val groupTitle = sdk.groups.firstOrNull { it.id == callState.groupId }?.name ?: callState.groupId
                GroupCallScreen(
                    controller = callController,
                    call = callState,
                    title = groupTitle,
                    onHangup = {
                        sdk.leaveGroupCallHex(callState.groupId, callState.callIdHex)
                        callController.stop()
                        goBack()
                    },
                    onAddSubscription = {
                        sdk.addMediaSubscription(callState.callId, isGroup = true, groupId = callState.groupId)
                    },
                    onClearSubscriptions = { sdk.clearMediaSubscriptions() }
                )
            }
        }
        FlowScreen.Settings -> SettingsScreen(
            sdk = sdk,
            themeMode = themeMode,
            onThemeModeChange = onThemeModeChange,
            onBack = { goBack() },
            onOpenAccount = { navigate(FlowScreen.Account) },
            onOpenPrivacy = { navigate(FlowScreen.Privacy) },
            onOpenDiagnostics = { navigate(FlowScreen.Diagnostics) },
            onOpenChats = { navigate(FlowScreen.Conversations) },
            onOpenContacts = { navigate(FlowScreen.AddFriend) }
        )
        FlowScreen.Account -> AccountScreen(sdk = sdk, onBack = { goBack() })
        FlowScreen.Privacy -> PrivacyScreen(
            sdk = sdk,
            onBack = { goBack() },
            onOpenBlockedUsers = { navigate(FlowScreen.BlockedUsers) }
        )
        FlowScreen.Diagnostics -> {
            if (BuildConfig.DEBUG) {
                DiagnosticsScreen(
                    sdk = sdk,
                    onBack = { goBack() }
                )
            } else {
                LaunchedEffect(Unit) { goBack() }
            }
        }
        FlowScreen.BlockedUsers -> BlockedUsersScreen(
            sdk = sdk,
            onBack = { goBack() }
        )
        FlowScreen.AddFriend -> AddFriendScreen(
            friends = sdk.friends,
            requests = sdk.friendRequests,
            onBack = { goBack() },
            onOpenRequests = { navigate(FlowScreen.FriendRequests) },
            onContactSelected = { friend -> navigate(FlowScreen.ContactDetail(friend.username)) },
            onOpenChats = { navigate(FlowScreen.Conversations) },
            onOpenSettings = { navigate(FlowScreen.Settings) },
            onSendRequest = { username, remark -> sdk.sendFriendRequest(username, remark) },
            onAddFriend = { username, remark -> sdk.addFriend(username, remark) },
            onJoinGroup = { groupId -> sdk.joinGroup(groupId) }
        )
        FlowScreen.FriendRequests -> FriendRequestsScreen(
            requests = sdk.friendRequests,
            onBack = { goBack() },
            onAccept = { request -> sdk.respondFriendRequest(request.username, true) },
            onDecline = { request -> sdk.respondFriendRequest(request.username, false) }
        )
        is FlowScreen.ContactDetail -> {
            val friend = sdk.friends.firstOrNull { it.username == current.username }
                ?: FriendUi(current.username, "", "")
            ContactDetailScreen(
                friend = friend,
                isBlocked = sdk.blockedUsers[current.username] == true,
                onBack = { goBack() },
                onMessage = {
                    sdk.setActiveConversation(friend.username, false)
                    sdk.loadHistory(friend.username, false)
                    navigate(FlowScreen.Chat(friend.username))
                },
                onCall = {
                    val state = sdk.startPeerCall(friend.username, video = false)
                    if (state != null) {
                        navigate(FlowScreen.PeerCall(state.callIdHex))
                    }
                },
                onDelete = { sdk.deleteFriend(friend.username) },
                onToggleBlock = { blocked -> sdk.setUserBlocked(friend.username, blocked) },
                onUpdateRemark = { remark -> sdk.setFriendRemark(friend.username, remark) }
            )
        }
        is FlowScreen.GroupDetail -> {
            val groupId = current.groupId
            LaunchedEffect(groupId) {
                sdk.refreshGroupMembers(groupId)
            }
            val members = sdk.groupMembersFor(groupId)
            val selfRole = members.firstOrNull { it.username == sdk.username }?.role
            val canManage = selfRole == GroupMemberRole.OWNER || selfRole == GroupMemberRole.ADMIN
            val groupName = sdk.groups.firstOrNull { it.id == groupId }?.name ?: groupId
            GroupDetailScreen(
                groupId = groupId,
                groupName = groupName,
                members = members,
                selfUsername = sdk.username,
                canInvite = canManage,
                canManage = canManage,
                onBack = { goBack() },
                onAddMembers = { navigate(FlowScreen.AddGroupMembers(groupId)) },
                onSetRole = { username, role -> sdk.setGroupMemberRole(groupId, username, role) },
                onKick = { username -> sdk.kickGroupMember(groupId, username) },
                onLeaveGroup = {
                    sdk.leaveGroup(groupId)
                    resetTo(FlowScreen.Conversations)
                }
            )
        }
        is FlowScreen.AddGroupMembers -> {
            val groupId = current.groupId
            val members = sdk.groupMembersFor(groupId)
            val selfRole = members.firstOrNull { it.username == sdk.username }?.role
            val canInvite = selfRole == GroupMemberRole.OWNER || selfRole == GroupMemberRole.ADMIN
            AddGroupMembersScreen(
                groupId = groupId,
                friends = sdk.friends,
                canInvite = canInvite,
                onBack = { goBack() },
                onAddMembers = { selected ->
                    selected.forEach { friend -> sdk.sendGroupInvite(groupId, friend.username) }
                    goBack()
                }
            )
        }
        }
        val incoming = sdk.pendingCall
        if (incoming != null) {
            AlertDialog(
                onDismissRequest = { sdk.declineIncomingCall() },
                title = { Text(tr("call_incoming_title", "Incoming call")) },
                text = {
                    Text(
                        tr("call_incoming_body", "%s is calling you").format(incoming.peerUsername)
                    )
                },
                confirmButton = {
                    TextButton(onClick = {
                        val state = sdk.acceptIncomingCall()
                        if (state != null) {
                            navigate(FlowScreen.PeerCall(state.callIdHex))
                        }
                    }) {
                        Text(tr("call_accept", "Accept"))
                    }
                },
                dismissButton = {
                    TextButton(onClick = { sdk.declineIncomingCall() }) {
                        Text(tr("call_decline", "Decline"))
                    }
                }
            )
        }
        if (sdk.hasPendingServerTrust) {
            TrustDialog(
                title = tr("trust_server_title", "Server trust required"),
                fingerprint = sdk.pendingServerFingerprint,
                pinHint = sdk.pendingServerPin,
                onConfirm = { pin -> sdk.trustPendingServer(pin) },
                onDismiss = {}
            )
        }
        if (sdk.hasPendingPeerTrust) {
            TrustDialog(
                title = tr("trust_peer_title", "Peer trust required"),
                fingerprint = sdk.pendingPeerFingerprint,
                pinHint = sdk.pendingPeerPin,
                subtitle = tr("trust_peer_subtitle", "Verify %s").format(sdk.pendingPeerUsername),
                onConfirm = { pin -> sdk.trustPendingPeer(pin) },
                onDismiss = {}
            )
        }
    }
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun UiHostPreview() {
    val context = LocalContext.current
    val sdk = remember(context) { SdkBridge(context) }
    LaunchedEffect(Unit) { sdk.init() }
    ChatTheme { UiHost(sdk = sdk) }
}

@Composable
private fun TrustDialog(
    title: String,
    fingerprint: String,
    pinHint: String,
    subtitle: String? = null,
    onConfirm: (String) -> Unit,
    onDismiss: () -> Unit
) {
    var pin by remember(pinHint) { mutableStateOf(pinHint) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(title) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                if (!subtitle.isNullOrBlank()) {
                    Text(text = subtitle, style = MaterialTheme.typography.bodyMedium)
                }
                Text(
                    text = tr("trust_fingerprint", "Fingerprint: %s").format(fingerprint),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                OutlinedTextField(
                    value = pin,
                    onValueChange = { pin = it },
                    label = { Text(tr("trust_pin", "PIN")) },
                    placeholder = { Text(pinHint) },
                    singleLine = true
                )
            }
        },
        confirmButton = {
            TextButton(onClick = { onConfirm(pin) }) {
                Text(tr("trust_confirm", "Trust"))
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(tr("chat_cancel", "Cancel"))
            }
        }
    )
}
