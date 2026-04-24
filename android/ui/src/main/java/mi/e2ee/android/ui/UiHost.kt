package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.activity.compose.BackHandler
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
import androidx.compose.ui.unit.dp
import java.io.File
import mi.e2ee.android.BuildConfig
import mi.e2ee.android.sdk.GroupMemberRole

data class UiHostPreviewState(
    val conversations: List<ConversationPreview>,
    val chatItems: Map<String, List<ChatItem>>,
    val groupChatItems: Map<String, List<GroupChatItem>>,
    val pendingCall: IncomingCall?,
    val activePeerCall: PeerCallState?,
    val activeGroupCall: GroupCallState?,
    val groupRooms: List<GroupCallRoomUi>
) {
    fun chatItemsFor(conversationId: String): List<ChatItem> {
        return chatItems[conversationId].orEmpty()
    }

    fun groupChatItemsFor(conversationId: String): List<GroupChatItem> {
        return groupChatItems[conversationId].orEmpty()
    }
}

@Composable
fun UiHost(
    sdk: SdkBridge,
    themeMode: Int = ThemeMode.FollowSystem,
    onThemeModeChange: (Int) -> Unit = {},
    screenshotBootstrapState: ScreenshotBootstrapState? = null,
    screenshotPreviewState: UiHostPreviewState? = null
) {
    val facade = remember(sdk) { SdkUiBridgeFacade(sdk) }
    UiHost(
        facade = facade,
        themeMode = themeMode,
        onThemeModeChange = onThemeModeChange,
        screenshotBootstrapState = screenshotBootstrapState,
        screenshotPreviewState = screenshotPreviewState
    )
}

@Composable
private fun UiHost(
    facade: UiBridgeFacade,
    themeMode: Int = ThemeMode.FollowSystem,
    onThemeModeChange: (Int) -> Unit = {},
    screenshotBootstrapState: ScreenshotBootstrapState? = null,
    screenshotPreviewState: UiHostPreviewState? = null
) {
    val sdk = facade.sdk
    val navState = rememberUiNavigationState(initial = screenshotBootstrapState?.rootScreen() ?: FlowScreen.Login)
    val current = navState.current
    val context = LocalContext.current
    val callController = remember(context, sdk) { CallMediaController(context, sdk) }
    val canNavigateBack = navState.stackSnapshot().size > 1
    val previewMode = screenshotBootstrapState != null && screenshotPreviewState != null
    val previewState = screenshotPreviewState

    fun navigate(screen: FlowScreen) {
        navState.navigate(screen)
    }

    fun resetTo(screen: FlowScreen) {
        navState.resetTo(screen)
    }

    fun goBack() {
        navState.goBack()
    }

    LaunchedEffect(facade.loggedIn, previewMode) {
        if (!previewMode) {
            if (facade.loggedIn) {
                resetTo(FlowScreen.Conversations)
            } else {
                callController.stop()
                resetTo(FlowScreen.Login)
            }
        }
    }
    LaunchedEffect(screenshotBootstrapState?.sceneId) {
        val bootstrap = screenshotBootstrapState ?: return@LaunchedEffect
        val bootstrapRoot = bootstrap.rootScreen()
        val bootstrapTarget = bootstrap.targetScreen()
        if (bootstrapRoot != bootstrapTarget && navState.current == bootstrapRoot) {
            navigate(bootstrapTarget)
        }
    }
    BackHandler(enabled = canNavigateBack) {
        goBack()
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

    fun previewConversation(convId: String): ConversationPreview? {
        return previewState?.conversations?.firstOrNull { it.id == convId }
    }

    fun openConversationRoute(conversation: ConversationPreview): FlowScreen {
        return if (previewMode) {
            if (conversation.isGroup) {
                FlowScreen.GroupChat(conversation.id)
            } else {
                FlowScreen.Chat(conversation.id)
            }
        } else {
            facade.openConversationRoute(conversation)
        }
    }

    Box(modifier = Modifier.fillMaxSize()) {
        AppBackdrop()
        when (current) {
        FlowScreen.Login -> LoginScreen(
            initialUsername = "",
            initialPassword = "",
            onRegister = { navigate(FlowScreen.Register) },
            onLogin = { username, password, rootCode ->
                if (facade.login(username, password, rootCode)) {
                    resetTo(FlowScreen.Conversations)
                }
            },
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
            conversations = previewState?.conversations ?: sdk.conversations,
            onTogglePin = { conversation ->
                if (!previewMode) {
                    facade.togglePinned(conversation.id)
                }
            },
            onToggleRead = { conversation ->
                if (!previewMode) {
                    facade.markConversationRead(conversation.id)
                }
            },
            onToggleMute = { conversation ->
                if (!previewMode) {
                    facade.toggleConversationMute(conversation.id)
                }
            },
            onDeleteConversation = { conversation ->
                if (!previewMode) {
                    facade.deleteConversation(conversation)
                }
            },
            onOpenConversation = { conversation -> navigate(openConversationRoute(conversation)) },
            onOpenSettings = { resetTo(FlowScreen.Settings) },
            onOpenContacts = { resetTo(FlowScreen.AddFriend) },
            onOpenCalls = { resetTo(FlowScreen.Calls) },
            onOpenNewGroup = {
                if (!previewMode) {
                    val route = facade.createGroupAndRoute()
                    if (route != null) {
                        navigate(route)
                    }
                }
            }
        )
        FlowScreen.Calls -> CallsHomeScreen(
            pendingCall = previewState?.pendingCall ?: sdk.pendingCall,
            activePeerCall = previewState?.activePeerCall ?: sdk.activePeerCall,
            activeGroupCall = previewState?.activeGroupCall ?: sdk.activeGroupCall,
            groupRooms = previewState?.groupRooms ?: sdk.groupCallRooms,
            onOpenPeerCall = { state -> navigate(FlowScreen.PeerCall(state.callIdHex)) },
            onOpenGroupCall = { state -> navigate(FlowScreen.GroupCall(state.groupId, state.callIdHex)) },
            onAcceptPendingCall = {
                if (!previewMode) {
                    val state = facade.acceptIncomingCall()
                    if (state != null) {
                        navigate(FlowScreen.PeerCall(state.callIdHex))
                    }
                }
            },
            onJoinGroupRoom = { room ->
                if (!previewMode) {
                    val info = sdk.joinGroupCallHex(room.groupId, room.callId, room.video)
                    val active = sdk.activeGroupCall
                    if (info != null && active != null) {
                        navigate(FlowScreen.GroupCall(room.groupId, active.callIdHex))
                    }
                }
            },
            onOpenChats = { resetTo(FlowScreen.Conversations) },
            onOpenContacts = { resetTo(FlowScreen.AddFriend) },
            onOpenSettings = { resetTo(FlowScreen.Settings) }
        )
        is FlowScreen.Chat -> {
            val convId = current.conversationId
            val conversation = previewConversation(convId) ?: sdk.conversations.firstOrNull { it.id == convId }
            val title = conversation?.name ?: convId
            val initials = conversation?.initials ?: title.take(2).uppercase()
            val status = if (previewMode) {
                if (conversation?.isTyping == true) "Typing..." else "Online"
            } else {
                sdk.friends.firstOrNull { it.username == convId }?.status
                    ?: tr("chat_status_unknown", "Unknown")
            }
            ChatScreen(
                items = previewState?.chatItemsFor(convId) ?: sdk.chatItemsFor(convId),
                conversationId = convId,
                title = title,
                status = status,
                initials = initials,
                selfInitials = selfInitials(),
                showTyping = conversation?.isTyping ?: false,
                onBack = {
                    if (!previewMode) {
                        sdk.clearActiveConversation()
                    }
                    goBack()
                },
                onOpenAccount = {
                    if (!previewMode) {
                        navigate(FlowScreen.Account)
                    }
                },
                onOpenSettings = { navigate(FlowScreen.Settings) },
                onStartCall = {
                    if (!previewMode) {
                        val state = sdk.startPeerCall(convId, video = false)
                        if (state != null) {
                            navigate(FlowScreen.PeerCall(state.callIdHex))
                        }
                    }
                },
                onStartVideoCall = {
                    if (!previewMode) {
                        val state = sdk.startPeerCall(convId, video = true)
                        if (state != null) {
                            navigate(FlowScreen.PeerCall(state.callIdHex))
                        }
                    }
                },
                onSendPresence = { online ->
                    if (!previewMode) {
                        sdk.sendPresence(convId, online)
                    }
                },
                onSendReadReceipt = { messageId ->
                    if (!previewMode) {
                        sdk.sendReadReceipt(convId, messageId)
                    }
                },
                onResendText = { messageId, text ->
                    if (!previewMode) {
                        sdk.resendPrivateText(convId, messageId, text)
                    } else {
                        false
                    }
                },
                onResendTextWithReply = { messageId, text, replyId, preview ->
                    if (!previewMode) {
                        sdk.resendPrivateTextWithReply(convId, messageId, text, replyId, preview)
                    } else {
                        false
                    }
                },
                onResendFile = { messageId, filePath ->
                    if (!previewMode) {
                        sdk.resendPrivateFile(convId, messageId, filePath)
                    } else {
                        false
                    }
                },
                onSendMessage = { text, reply ->
                    if (!previewMode) {
                        sdk.sendText(convId, text, reply, isGroup = false)
                    } else {
                        false
                    }
                },
                onSendFile = { path ->
                    if (!previewMode) {
                        sdk.sendFile(convId, path, isGroup = false)
                    } else {
                        false
                    }
                },
                onSendLocation = { lat, lon, label ->
                    if (!previewMode) {
                        sdk.sendLocation(convId, lat, lon, label, isGroup = false)
                    } else {
                        false
                    }
                },
                onSendSticker = { stickerId ->
                    if (!previewMode) {
                        sdk.sendSticker(convId, stickerId)
                    } else {
                        false
                    }
                },
                onSendContact = { cardUsername, cardDisplay ->
                    if (!previewMode) {
                        sdk.sendContact(convId, cardUsername, cardDisplay)
                    } else {
                        false
                    }
                },
                onTyping = { typing ->
                    if (!previewMode) {
                        sdk.sendTyping(convId, typing)
                    }
                },
                onRecallMessage = { messageId ->
                    if (!previewMode) {
                        sdk.sendRecall(convId, messageId, isGroup = false)
                    } else {
                        false
                    }
                },
                onDownloadAttachment = { attachment ->
                    if (!previewMode) {
                        downloadAttachment(attachment)
                    }
                }
            )
        }
        is FlowScreen.GroupChat -> {
            val groupId = current.groupId
            val previewConversation = previewConversation(groupId)
            val conversation = previewConversation ?: sdk.conversations.firstOrNull { it.id == groupId }
            val groupName = if (previewMode) {
                conversation?.name ?: groupId
            } else {
                sdk.groups.firstOrNull { it.id == groupId }?.name ?: groupId
            }
            val members = if (previewMode) emptyList() else sdk.groupMembersFor(groupId)
            val subtitle = if (!previewMode && members.isNotEmpty()) {
                tr("group_member_count", "%d members / Secure group").format(members.size)
            } else {
                tr("group_member_count", "Secure group")
            }
            val activeCall = if (previewMode) {
                previewState?.groupRooms?.firstOrNull { it.groupId == groupId || it.groupId == groupName }
            } else {
                sdk.groupCallRooms.firstOrNull { it.groupId == groupId }
            }
            GroupChatScreen(
                items = if (previewMode) {
                    previewState?.groupChatItemsFor(groupId).orEmpty()
                } else {
                    sdk.groupItemsFor(groupId)
                },
                conversationId = groupId,
                title = conversation?.name ?: groupName,
                subtitle = subtitle,
                onBack = {
                    if (!previewMode) {
                        sdk.clearActiveConversation()
                    }
                    goBack()
                },
                onOpenGroupDetail = {
                    if (!previewMode) {
                        navigate(FlowScreen.GroupDetail(groupId))
                    }
                },
                activeCall = activeCall,
                onStartVoiceCall = {
                    if (!previewMode) {
                        val info = sdk.startGroupCall(groupId, video = false)
                        val active = sdk.activeGroupCall
                        if (info != null && active != null) {
                            navigate(FlowScreen.GroupCall(groupId, active.callIdHex))
                        }
                    }
                },
                onStartVideoCall = {
                    if (!previewMode) {
                        val info = sdk.startGroupCall(groupId, video = true)
                        val active = sdk.activeGroupCall
                        if (info != null && active != null) {
                            navigate(FlowScreen.GroupCall(groupId, active.callIdHex))
                        }
                    }
                },
                onJoinCall = { room ->
                    if (!previewMode) {
                        val info = sdk.joinGroupCallHex(groupId, room.callId, room.video)
                        val active = sdk.activeGroupCall
                        if (info != null && active != null) {
                            navigate(FlowScreen.GroupCall(groupId, active.callIdHex))
                        }
                    }
                },
                onLeaveCall = { room ->
                    if (!previewMode) {
                        sdk.leaveGroupCallHex(groupId, room.callId)
                        val active = sdk.activeGroupCall
                        if (active != null && active.groupId == groupId && active.callIdHex == room.callId) {
                            callController.stop()
                        }
                    }
                },
                onSendMessage = { text ->
                    if (!previewMode) {
                        sdk.sendText(groupId, text, isGroup = true)
                    } else {
                        false
                    }
                },
                onSendFile = { path ->
                    if (!previewMode) {
                        sdk.sendFile(groupId, path, isGroup = true)
                    } else {
                        false
                    }
                },
                onSendLocation = { lat, lon, label ->
                    if (!previewMode) {
                        sdk.sendLocation(groupId, lat, lon, label, isGroup = true)
                    } else {
                        false
                    }
                },
                onRecallMessage = { messageId ->
                    if (!previewMode) {
                        sdk.sendRecall(groupId, messageId, isGroup = true)
                    } else {
                        false
                    }
                },
                onResendText = { messageId, text ->
                    if (!previewMode) {
                        sdk.resendGroupText(groupId, messageId, text)
                    } else {
                        false
                    }
                },
                onResendFile = { messageId, filePath ->
                    if (!previewMode) {
                        sdk.resendGroupFile(groupId, messageId, filePath)
                    } else {
                        false
                    }
                },
                onDownloadAttachment = { attachment ->
                    if (!previewMode) {
                        downloadAttachment(attachment)
                    }
                }
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
            onOpenChats = { resetTo(FlowScreen.Conversations) },
            onOpenCalls = { resetTo(FlowScreen.Calls) },
            onOpenContacts = { resetTo(FlowScreen.AddFriend) }
        )
        FlowScreen.SecurityCenter -> SecurityCenterScreen(
            sdk = sdk,
            title = tr("security_center_title", "Security Center"),
            previewMode = previewMode,
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
