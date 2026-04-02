package mi.e2ee.android.ui

import androidx.compose.foundation.background
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
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import java.io.File
import mi.e2ee.android.BuildConfig
import mi.e2ee.android.sdk.GroupMemberRole

@Composable
fun UiHost(
    sdk: SdkBridge,
    themeMode: Int = ThemeMode.FollowSystem,
    onThemeModeChange: (Int) -> Unit = {}
) {
    val facade = remember(sdk) { SdkUiBridgeFacade(sdk) }
    UiHost(
        facade = facade,
        themeMode = themeMode,
        onThemeModeChange = onThemeModeChange
    )
}

@Composable
private fun UiHost(
    facade: UiBridgeFacade,
    themeMode: Int = ThemeMode.FollowSystem,
    onThemeModeChange: (Int) -> Unit = {}
) {
    val sdk = facade.sdk
    val navState = rememberUiNavigationState()
    val current = navState.current
    val context = LocalContext.current
    val callController = remember(context, sdk) { CallMediaController(context, sdk) }

    fun navigate(screen: FlowScreen) {
        navState.navigate(screen)
    }

    fun resetTo(screen: FlowScreen) {
        navState.resetTo(screen)
    }

    fun goBack() {
        navState.goBack()
    }

    LaunchedEffect(facade.loggedIn) {
        if (facade.loggedIn) {
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
        AppBackdrop()
        when (current) {
        FlowScreen.Login -> LoginScreen(
            onRegister = { navigate(FlowScreen.Register) },
            onLogin = { username, password, rootCode ->
                if (facade.login(username, password, rootCode)) {
                    resetTo(FlowScreen.Conversations)
                }
            },
            onShowQr = { username -> navigate(FlowScreen.QrLoginDisplay(username)) },
            onScanQr = { navigate(FlowScreen.QrLoginScan) },
            errorMessage = sdk.lastError.takeIf { it.isNotBlank() },
            statusMessage = sdk.statusMessage,
            remoteError = if (sdk.remoteOk) "" else sdk.remoteError
        )
        FlowScreen.Register -> RegisterScreen(
            onLogin = { goBack() },
            onCreateAccount = { username, password ->
                val ok = facade.register(username, password)
                if (ok) {
                    facade.login(username, password, "")
                    resetTo(FlowScreen.Conversations)
                }
            },
            errorMessage = sdk.lastError.takeIf { it.isNotBlank() },
            statusMessage = sdk.statusMessage
        )
        is FlowScreen.QrLoginDisplay -> QrLoginDisplayScreen(
            sdk = sdk,
            username = current.username,
            onBack = { goBack() },
            onLoggedIn = { resetTo(FlowScreen.Conversations) }
        )
        FlowScreen.QrLoginScan -> QrLoginScanScreen(
            sdk = sdk,
            onBack = { goBack() }
        )
        FlowScreen.Conversations -> ConversationListScreen(
            conversations = sdk.conversations,
            onTogglePin = { facade.togglePinned(it.id) },
            onToggleRead = { facade.markConversationRead(it.id) },
            onToggleMute = { facade.toggleConversationMute(it.id) },
            onDeleteConversation = { conversation -> facade.deleteConversation(conversation) },
            onOpenConversation = { conversation -> navigate(facade.openConversationRoute(conversation)) },
            onOpenSettings = { resetTo(FlowScreen.Settings) },
            onOpenContacts = { resetTo(FlowScreen.AddFriend) },
            onOpenCalls = { resetTo(FlowScreen.Calls) },
            onOpenNewGroup = {
                val route = facade.createGroupAndRoute()
                if (route != null) {
                    navigate(route)
                }
            }
        )
        FlowScreen.Calls -> CallsHomeScreen(
            pendingCall = sdk.pendingCall,
            activePeerCall = sdk.activePeerCall,
            activeGroupCall = sdk.activeGroupCall,
            groupRooms = sdk.groupCallRooms,
            onOpenPeerCall = { state -> navigate(FlowScreen.PeerCall(state.callIdHex)) },
            onOpenGroupCall = { state -> navigate(FlowScreen.GroupCall(state.groupId, state.callIdHex)) },
            onAcceptPendingCall = {
                val state = facade.acceptIncomingCall()
                if (state != null) {
                    navigate(FlowScreen.PeerCall(state.callIdHex))
                }
            },
            onJoinGroupRoom = { room ->
                val info = sdk.joinGroupCallHex(room.groupId, room.callId, room.video)
                val active = sdk.activeGroupCall
                if (info != null && active != null) {
                    navigate(FlowScreen.GroupCall(room.groupId, active.callIdHex))
                }
            },
            onOpenChats = { resetTo(FlowScreen.Conversations) },
            onOpenContacts = { resetTo(FlowScreen.AddFriend) },
            onOpenSettings = { resetTo(FlowScreen.Settings) }
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
            showBackButton = navState.stackSnapshot().size > 1,
            onBack = { goBack() },
            onOpenSecurityCenter = { navigate(FlowScreen.SecurityCenter) },
            onOpenAccount = { navigate(FlowScreen.Account) },
            onOpenPrivacy = { navigate(FlowScreen.Privacy) },
            onOpenDiagnostics = { navigate(FlowScreen.Diagnostics) },
            onOpenChats = { resetTo(FlowScreen.Conversations) },
            onOpenCalls = { resetTo(FlowScreen.Calls) },
            onOpenContacts = { resetTo(FlowScreen.AddFriend) }
        )
        FlowScreen.SecurityCenter -> SecurityCenterScreen(
            sdk = sdk,
            title = tr("security_center_title", "Security Center"),
            onBack = { goBack() }
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
            showBackButton = navState.stackSnapshot().size > 1,
            onBack = { goBack() },
            onOpenRequests = { navigate(FlowScreen.FriendRequests) },
            onScanQr = { navigate(FlowScreen.QrLoginScan) },
            onContactSelected = { friend -> navigate(FlowScreen.ContactDetail(friend.username)) },
            onOpenChats = { resetTo(FlowScreen.Conversations) },
            onOpenCalls = { resetTo(FlowScreen.Calls) },
            onOpenSettings = { resetTo(FlowScreen.Settings) },
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
                onDismissRequest = { facade.declineIncomingCall() },
                title = { Text(tr("call_incoming_title", "Incoming call")) },
                text = {
                    Text(
                        tr("call_incoming_body", "%s is calling you").format(incoming.peerUsername)
                    )
                },
                confirmButton = {
                    TextButton(onClick = {
                        val state = facade.acceptIncomingCall()
                        if (state != null) {
                            navigate(FlowScreen.PeerCall(state.callIdHex))
                        }
                    }) {
                        Text(tr("call_accept", "Accept"))
                    }
                },
                dismissButton = {
                    TextButton(onClick = { facade.declineIncomingCall() }) {
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
                onConfirm = { pin -> facade.trustPendingServer(pin) },
                onDismiss = {}
            )
        }
        if (sdk.hasPendingPeerTrust) {
            TrustDialog(
                title = tr("trust_peer_title", "Peer trust required"),
                fingerprint = sdk.pendingPeerFingerprint,
                pinHint = sdk.pendingPeerPin,
                subtitle = tr("trust_peer_subtitle", "Verify %s").format(sdk.pendingPeerUsername),
                onConfirm = { pin -> facade.trustPendingPeer(pin) },
                onDismiss = {}
            )
        }
    }
}

@Composable
private fun AppBackdrop() {
    val background = MaterialTheme.colorScheme.background
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(
                        background,
                        MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.03f),
                        background
                    ),
                    startY = 0f,
                    endY = 1800f
                )
            )
    )
}

@Preview(showBackground = true, widthDp = 412, heightDp = 915)
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
