package mi.e2ee.android.ui

import android.content.Context
import android.content.SharedPreferences
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.Reply
import androidx.compose.material.icons.automirrored.filled.Undo
import androidx.compose.material.icons.filled.Call
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Group
import androidx.compose.material.icons.filled.PushPin
import androidx.compose.material.icons.filled.Videocam
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

sealed interface GroupChatItem {
    val id: String
}

data class GroupSystemNotice(
    override val id: String,
    val text: String
) : GroupChatItem

data class GroupMessage(
    override val id: String,
    val sender: String,
    val role: String?,
    val body: String,
    val time: String,
    val isMine: Boolean,
    val status: String = "",
    val readBy: List<String> = emptyList(),
    val isRevoked: Boolean = false,
    val recallSecondsLeft: Int? = null,
    val attachment: Attachment? = null
) : GroupChatItem

data class PendingGroupDelete(
    val message: GroupMessage
)

private sealed interface GroupComposerDialog {
    data object File : GroupComposerDialog
    data object Location : GroupComposerDialog
}

object SampleGroupChat {
    val items: List<GroupChatItem> = listOf(
        GroupSystemNotice("g_sys1", "Aster joined the group"),
        GroupMessage(
            id = "g1",
            sender = "Aster",
            role = "Admin",
            body = "Daily sync in 10 minutes. Please post blockers.",
            time = "09:05",
            isMine = false
        ),
        GroupMessage(
            id = "g2",
            sender = "Me",
            role = null,
            body = "Working on the UI spec. Draft will be shared soon.",
            time = "09:06",
            isMine = true,
            status = "Read",
            readBy = listOf("AS", "RN", "LD"),
            attachment = Attachment(
                AttachmentKind.File,
                "UI-Spec.pdf",
                "230 KB",
                state = TransferState.Downloading,
                progress = 0.42f
            )
        ),
        GroupSystemNotice("g_sys2", "Mina left the group"),
        GroupMessage(
            id = "g3",
            sender = "Rin",
            role = null,
            body = "Thanks @Mina - also check the privacy toggles copy.",
            time = "09:07",
            isMine = false,
            attachment = Attachment(AttachmentKind.Photo, "Privacy.png", "1280x720")
        ),
        GroupMessage(
            id = "g4",
            sender = "Me",
            role = null,
            body = "Noted. I will add the final wording today.",
            time = "09:08",
            isMine = true,
            status = "Delivered",
            readBy = listOf("AS"),
            recallSecondsLeft = 48,
            attachment = Attachment(AttachmentKind.Voice, "Voice note", "0:21")
        ),
        GroupMessage(
            id = "g5",
            sender = "Mina",
            role = null,
            body = "",
            time = "09:09",
            isMine = false,
            isRevoked = true
        ),
        GroupSystemNotice("g_sys3", "Rin was removed"),
        GroupMessage(
            id = "g6",
            sender = "Aster",
            role = "Admin",
            body = "Meet at this location for the review.",
            time = "09:10",
            isMine = false,
            attachment = Attachment(AttachmentKind.Location, "Civic Plaza", "2.1 km away")
        )
    )

    private val auroraItems: List<GroupChatItem> = listOf(
        GroupSystemNotice("a_sys1", "Jun joined the group"),
        GroupMessage(
            id = "a1",
            sender = "Qin",
            role = "Owner",
            body = "Release notes draft is in Drive. Please review.",
            time = "11:12",
            isMine = false,
            attachment = Attachment(AttachmentKind.File, "ReleaseNotes.docx", "410 KB")
        ),
        GroupMessage(
            id = "a2",
            sender = "Me",
            role = null,
            body = "I will check the privacy section and share feedback.",
            time = "11:13",
            isMine = true,
            status = "Read",
            readBy = listOf("QN", "JR", "SG")
        ),
        GroupMessage(
            id = "a3",
            sender = "Jun",
            role = "QA",
            body = "@Mina the test matrix is updated for Android 14.",
            time = "11:15",
            isMine = false,
            attachment = Attachment(AttachmentKind.Photo, "Matrix.png", "1440x900")
        ),
        GroupMessage(
            id = "a4",
            sender = "Me",
            role = null,
            body = "Thanks. I will sync with release ops.",
            time = "11:16",
            isMine = true,
            status = "Delivered",
            readBy = listOf("QN"),
            recallSecondsLeft = 35
        )
    )

    fun itemsFor(conversationId: String?): List<GroupChatItem> {
        return when (conversationId) {
            "c4" -> auroraItems
            else -> items
        }
    }
}

@Composable
fun GroupChatApp() {
    ChatTheme {
        GroupChatScreen(SampleGroupChat.items)
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun GroupChatScreen(
    items: List<GroupChatItem>,
    conversationId: String = "default_group",
    title: String = "Design Ops",
    subtitle: String = "12 members / Secure group",
    onBack: () -> Unit = {},
    onOpenGroupDetail: () -> Unit = {},
    activeCall: GroupCallRoomUi? = null,
    onStartVoiceCall: () -> Unit = {},
    onStartVideoCall: () -> Unit = {},
    onJoinCall: (GroupCallRoomUi) -> Unit = {},
    onLeaveCall: (GroupCallRoomUi) -> Unit = {},
    onSendMessage: (String) -> Boolean = { false },
    onSendFile: (String) -> Boolean = { false },
    onSendLocation: (Double, Double, String) -> Boolean = { _, _, _ -> false },
    onRecallMessage: (String) -> Boolean = { false },
    onDownloadAttachment: (Attachment) -> Unit = {}
) {
    var actionTarget by remember { mutableStateOf<GroupMessage?>(null) }
    var toastMessage by remember { mutableStateOf<String?>(null) }
    var composerReply by remember { mutableStateOf<ReplyPreview?>(null) }
    var composerText by remember { mutableStateOf("") }
    var composerDialog by remember { mutableStateOf<GroupComposerDialog?>(null) }
    var pendingDelete by remember { mutableStateOf<PendingGroupDelete?>(null) }
    val context = LocalContext.current
    val strings = LocalStrings.current
    fun t(key: String, fallback: String): String = strings.get(key, fallback)
    val resolvedConversationId = if (conversationId.isNotBlank()) conversationId else "default_group"
    val prefs = remember { context.getSharedPreferences(GROUP_PREFS_NAME, Context.MODE_PRIVATE) }
    var deletedIds by remember(resolvedConversationId) {
        mutableStateOf(loadGroupDeletedIds(prefs, resolvedConversationId))
    }
    var recalledIds by remember(resolvedConversationId) {
        mutableStateOf(loadGroupRecalledIds(prefs, resolvedConversationId))
    }
    var pinnedId by remember(resolvedConversationId) {
        mutableStateOf(loadGroupPinnedId(prefs, resolvedConversationId))
    }
    val normalizedPinnedId = pinnedId?.takeIf { it.isNotBlank() }
    val normalizedItems = items.map { item ->
        if (item is GroupMessage && recalledIds.contains(item.id)) {
            recalledGroupMessageCopy(item)
        } else {
            item
        }
    }
    val visibleItems = normalizedItems.filterNot { item ->
        item is GroupMessage && deletedIds.contains(item.id)
    }
    val pinnedFromId = normalizedPinnedId?.let { id ->
        visibleItems.filterIsInstance<GroupMessage>()
            .firstOrNull { it.id == id }
    }?.let { message -> buildGroupPinnedMessage(message) }
    val defaultPinned = normalizedItems.filterIsInstance<GroupMessage>()
        .firstOrNull()
        ?.let { message -> buildGroupPinnedMessage(message) }
    val pinnedMessage = when {
        pinnedId == null -> defaultPinned
        pinnedId.isBlank() -> null
        else -> pinnedFromId
    }
    var highlightedMessageId by remember(resolvedConversationId) { mutableStateOf<String?>(null) }
    val listState = rememberLazyListState()
    val scope = rememberCoroutineScope()
    val clipboard = LocalClipboardManager.current

    LaunchedEffect(toastMessage) {
        if (toastMessage != null) {
            delay(1800)
            toastMessage = null
        }
    }
    LaunchedEffect(pendingDelete?.message?.id) {
        val current = pendingDelete ?: return@LaunchedEffect
        delay(3500)
        if (pendingDelete?.message?.id == current.message.id) {
            pendingDelete = null
        }
    }
    LaunchedEffect(highlightedMessageId) {
        val current = highlightedMessageId ?: return@LaunchedEffect
        delay(2200)
        if (highlightedMessageId == current) {
            highlightedMessageId = null
        }
    }

    fun setPinnedMessage(message: GroupMessage?) {
        pinnedId = message?.id ?: ""
        persistGroupPinnedId(prefs, resolvedConversationId, pinnedId)
    }

    fun requestDelete(message: GroupMessage) {
        pendingDelete = PendingGroupDelete(message)
        deletedIds = deletedIds + message.id
        persistGroupDeletedIds(prefs, resolvedConversationId, deletedIds)
        if (pinnedId == message.id) {
            setPinnedMessage(null)
        }
    }

    Scaffold(
        topBar = {
            GroupChatTopBar(
                title = title,
                subtitle = subtitle,
                onBack = onBack,
                onOpenGroupDetail = onOpenGroupDetail,
                onStartVoiceCall = onStartVoiceCall,
                onStartVideoCall = onStartVideoCall
            )
        },
        bottomBar = {
            ComposerBar(
                replyPreview = composerReply,
                message = composerText,
                onMessageChange = { composerText = it },
                onSend = {
                    val ok = onSendMessage(composerText)
                    if (ok) {
                        composerText = ""
                        composerReply = null
                    }
                },
                onReplyDismiss = { composerReply = null },
                onAttachFile = { composerDialog = GroupComposerDialog.File },
                onAttachPhoto = { composerDialog = GroupComposerDialog.File },
                onAttachLocation = { composerDialog = GroupComposerDialog.Location },
                modifier = Modifier
                    .shadow(8.dp, RoundedCornerShape(topStart = 20.dp, topEnd = 20.dp))
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
        ) {
            Column(modifier = Modifier.fillMaxSize()) {
                if (activeCall != null) {
                    GroupCallBanner(
                        room = activeCall,
                        onJoin = { onJoinCall(activeCall) },
                        onLeave = { onLeaveCall(activeCall) }
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                }
                if (pinnedMessage != null) {
                    GroupPinnedBanner(
                        message = pinnedMessage!!,
                        onClick = {
                            val targetId = pinnedMessage?.messageId ?: return@GroupPinnedBanner
                            val index = visibleItems.indexOfFirst { it is GroupMessage && it.id == targetId }
                            if (index != -1) {
                                scope.launch { listState.animateScrollToItem(index) }
                                highlightedMessageId = targetId
                            } else {
                                toastMessage = t("chat_message_not_found", "Message not found")
                            }
                        }
                    )
                }
                LazyColumn(
                    modifier = Modifier.fillMaxSize(),
                    state = listState,
                    contentPadding = PaddingValues(horizontal = 16.dp, vertical = 16.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    itemsIndexed(visibleItems, key = { _, item -> item.id }) { index, item ->
                        when (item) {
                            is GroupSystemNotice -> GroupSystemMessageRow(item.text)
                            is GroupMessage -> GroupMessageRow(
                                message = item,
                                index = index,
                                isHighlighted = highlightedMessageId == item.id,
                                onMessageLongPress = { actionTarget = it },
                                onAttachmentClick = onDownloadAttachment
                            )
                        }
                    }
                }
            }
            if (actionTarget != null) {
                val isPinned = pinnedMessage?.messageId == actionTarget?.id
                GroupMessageActionSheet(
                    message = actionTarget!!,
                    isPinned = isPinned,
                    onDismiss = { actionTarget = null },
                    onAction = { action ->
                        val message = actionTarget ?: return@GroupMessageActionSheet
                        val toast = when (action.id) {
                            "reply" -> {
                                composerReply = ReplyPreview(
                                    sender = message.sender,
                                    snippet = groupReplySnippet(message),
                                    messageId = message.id
                                )
                                t("chat_reply_started", "Reply started")
                            }
                            "copy" -> {
                                clipboard.setText(AnnotatedString(groupMessageCopyText(message)))
                                t("chat_copied", "Copied")
                            }
                            "pin" -> {
                                if (pinnedMessage?.messageId == message.id) {
                                    setPinnedMessage(null)
                                    t("chat_unpinned", "Unpinned")
                                } else {
                                    setPinnedMessage(message)
                                    t("chat_pinned", "Pinned")
                                }
                            }
                            "recall" -> {
                                val ok = onRecallMessage(message.id)
                                if (ok) {
                                    recalledIds = recalledIds + message.id
                                    persistGroupRecalledIds(prefs, resolvedConversationId, recalledIds)
                                    t("chat_recalled", "Recalled")
                                } else {
                                    t("chat_recall_failed", "Recall failed")
                                }
                            }
                            "delete" -> {
                                requestDelete(message)
                                null
                            }
                            else -> action.label
                        }
                        if (toast != null) {
                            toastMessage = toast
                        }
                        actionTarget = null
                    }
                )
            }
            if (composerDialog != null) {
                when (composerDialog) {
                    GroupComposerDialog.File -> {
                        var path by remember { mutableStateOf("") }
                        AlertDialog(
                            onDismissRequest = { composerDialog = null },
                            title = { Text(tr("chat_attach_file", "Send file")) },
                            text = {
                                OutlinedTextField(
                                    value = path,
                                    onValueChange = { path = it },
                                    label = { Text(tr("chat_file_path", "File path")) },
                                    placeholder = { Text(tr("chat_file_path_hint", "/sdcard/Download/file.pdf")) },
                                    singleLine = true
                                )
                            },
                            confirmButton = {
                                TextButton(
                                    onClick = {
                                        if (path.isNotBlank() && onSendFile(path)) {
                                            composerDialog = null
                                        }
                                    },
                                    enabled = path.isNotBlank()
                                ) {
                                    Text(tr("chat_send", "Send"))
                                }
                            },
                            dismissButton = {
                                TextButton(onClick = { composerDialog = null }) {
                                    Text(tr("chat_cancel", "Cancel"))
                                }
                            }
                        )
                    }
                    GroupComposerDialog.Location -> {
                        var label by remember { mutableStateOf("") }
                        var lat by remember { mutableStateOf("") }
                        var lon by remember { mutableStateOf("") }
                        val latValue = lat.toDoubleOrNull()
                        val lonValue = lon.toDoubleOrNull()
                        val canSend = label.isNotBlank() && latValue != null && lonValue != null
                        AlertDialog(
                            onDismissRequest = { composerDialog = null },
                            title = { Text(tr("chat_attach_location", "Share location")) },
                            text = {
                                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                    OutlinedTextField(
                                        value = label,
                                        onValueChange = { label = it },
                                        label = { Text(tr("chat_location_label", "Label")) },
                                        placeholder = { Text(tr("chat_location_label_hint", "Office")) },
                                        singleLine = true
                                    )
                                    OutlinedTextField(
                                        value = lat,
                                        onValueChange = { lat = it },
                                        label = { Text(tr("chat_location_lat", "Latitude")) },
                                        placeholder = { Text("31.2304") },
                                        singleLine = true
                                    )
                                    OutlinedTextField(
                                        value = lon,
                                        onValueChange = { lon = it },
                                        label = { Text(tr("chat_location_lon", "Longitude")) },
                                        placeholder = { Text("121.4737") },
                                        singleLine = true
                                    )
                                }
                            },
                            confirmButton = {
                                TextButton(
                                    onClick = {
                                        val latNum = latValue
                                        val lonNum = lonValue
                                        if (latNum != null && lonNum != null && onSendLocation(latNum, lonNum, label)) {
                                            composerDialog = null
                                        }
                                    },
                                    enabled = canSend
                                ) {
                                    Text(tr("chat_send", "Send"))
                                }
                            },
                            dismissButton = {
                                TextButton(onClick = { composerDialog = null }) {
                                    Text(tr("chat_cancel", "Cancel"))
                                }
                            }
                        )
                    }
                    null -> Unit
                }
            }
            if (pendingDelete != null) {
                GroupUndoPill(
                    text = tr("chat_message_deleted", "Message deleted"),
                    actionLabel = tr("chat_undo", "Undo"),
                    onAction = {
                        val restore = pendingDelete ?: return@GroupUndoPill
                        deletedIds = deletedIds - restore.message.id
                        persistGroupDeletedIds(prefs, resolvedConversationId, deletedIds)
                        pendingDelete = null
                        toastMessage = t("chat_restored", "Restored")
                    },
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .padding(bottom = 72.dp)
                )
            } else if (toastMessage != null) {
                GroupToastPill(
                    text = toastMessage!!,
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .padding(bottom = 72.dp)
                )
            }
        }
    }
}

@Composable
private fun GroupChatTopBar(
    title: String,
    subtitle: String,
    onBack: () -> Unit,
    onOpenGroupDetail: () -> Unit,
    onStartVoiceCall: () -> Unit,
    onStartVideoCall: () -> Unit,
    modifier: Modifier = Modifier
) {
    Surface(
        modifier = modifier
            .fillMaxWidth()
            .shadow(8.dp, RoundedCornerShape(bottomStart = 24.dp, bottomEnd = 24.dp)),
        color = MaterialTheme.colorScheme.surface.copy(alpha = 0.96f),
        tonalElevation = 2.dp
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 6.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            CompactTopBarIconButton(
                icon = Icons.AutoMirrored.Filled.ArrowBack,
                contentDescription = tr("chat_back", "Back"),
                onClick = onBack
            )
            Spacer(modifier = Modifier.width(8.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleMedium
                )
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            CompactTopBarIconButton(
                icon = Icons.Filled.Call,
                contentDescription = tr("group_call_voice", "Voice call"),
                onClick = onStartVoiceCall
            )
            CompactTopBarIconButton(
                icon = Icons.Filled.Videocam,
                contentDescription = tr("group_call_video", "Video call"),
                onClick = onStartVideoCall
            )
            CompactTopBarIconButton(
                icon = Icons.Filled.Group,
                contentDescription = tr("group_members_section", "Members"),
                onClick = onOpenGroupDetail
            )
        }
    }
}

@Composable
private fun GroupCallBanner(
    room: GroupCallRoomUi,
    onJoin: () -> Unit,
    onLeave: () -> Unit
) {
    Card(
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(
                imageVector = if (room.video) Icons.Filled.Videocam else Icons.Filled.Call,
                contentDescription = tr("group_call_active", "Active call"),
                tint = MaterialTheme.colorScheme.primary
            )
            Spacer(modifier = Modifier.width(10.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = tr("group_call_active", "Active call"),
                    style = MaterialTheme.typography.bodyLarge
                )
                Text(
                    text = if (room.video) tr("group_call_video", "Video call") else tr("group_call_voice", "Voice call"),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            TextButton(onClick = onJoin) { Text(tr("group_call_join", "Join")) }
            TextButton(onClick = onLeave) { Text(tr("group_call_leave", "Leave")) }
        }
    }
}

@Composable
private fun CompactTopBarIconButton(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    contentDescription: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .size(28.dp)
            .clip(CircleShape)
            .clickable { onClick() },
        contentAlignment = Alignment.Center
    ) {
        Icon(
            imageVector = icon,
            contentDescription = contentDescription,
            tint = MaterialTheme.colorScheme.onSurface,
            modifier = Modifier.size(16.dp)
        )
    }
}

@Composable
private fun GroupPinnedBanner(message: PinnedMessage, onClick: () -> Unit) {
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() },
        color = MaterialTheme.colorScheme.primary.copy(alpha = 0.08f)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            LabeledChip(label = tr("group_pinned", "Pinned"), tint = MaterialTheme.colorScheme.primary)
            Spacer(modifier = Modifier.width(10.dp))
            Text(
                text = message.snippet,
                style = MaterialTheme.typography.bodyMedium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
    }
}

@Composable
private fun GroupSystemMessageRow(text: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.Center
    ) {
        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(16.dp))
                .background(MaterialTheme.colorScheme.surfaceVariant)
                .padding(horizontal = 12.dp, vertical = 6.dp)
        ) {
            Text(
                text = text,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
private fun GroupMessageRow(
    message: GroupMessage,
    index: Int,
    isHighlighted: Boolean,
    onMessageLongPress: (GroupMessage) -> Unit,
    onAttachmentClick: (Attachment) -> Unit
) {
    var visible by remember { mutableStateOf(false) }
    LaunchedEffect(message.id) {
        delay(index * 40L)
        visible = true
    }
    AnimatedVisibility(
        visible = visible,
        enter = fadeIn(tween(200)) + slideInVertically(tween(200)) { it / 4 }
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = if (message.isMine) Arrangement.End else Arrangement.Start,
            verticalAlignment = Alignment.Top
        ) {
            if (!message.isMine) {
                AvatarBadge(initials = message.sender.take(2).uppercase(), tint = MaterialTheme.colorScheme.primary)
                Spacer(modifier = Modifier.width(10.dp))
            }
            Column(
                horizontalAlignment = if (message.isMine) Alignment.End else Alignment.Start
            ) {
                if (!message.isMine) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            text = message.sender,
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                        if (message.role != null) {
                            Spacer(modifier = Modifier.width(6.dp))
                            LabeledChip(label = message.role, tint = MaterialTheme.colorScheme.secondary)
                        }
                    }
                    Spacer(modifier = Modifier.height(4.dp))
                }
                GroupMessageBubble(
                    message = message,
                    isHighlighted = isHighlighted,
                    onLongPress = { onMessageLongPress(message) },
                    onAttachmentClick = onAttachmentClick
                )
                Spacer(modifier = Modifier.height(4.dp))
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Text(
                        text = message.time,
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    if (message.isMine && message.status.isNotEmpty()) {
                        Text(
                            text = message.status,
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.primary
                        )
                    }
                    if (message.isMine && message.readBy.isNotEmpty()) {
                        GroupReadReceipts(message.readBy)
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalFoundationApi::class)
@Composable
private fun GroupMessageBubble(
    message: GroupMessage,
    isHighlighted: Boolean,
    onLongPress: () -> Unit,
    onAttachmentClick: (Attachment) -> Unit
) {
    val haptics = LocalHapticFeedback.current
    val highlightAlpha by animateFloatAsState(
        targetValue = if (isHighlighted) 1f else 0f,
        animationSpec = tween(220),
        label = "groupHighlightAlpha"
    )
    val shape = RoundedCornerShape(
        topStart = if (message.isMine) 18.dp else 6.dp,
        topEnd = if (message.isMine) 6.dp else 18.dp,
        bottomEnd = 18.dp,
        bottomStart = 18.dp
    )
    val baseBubbleColor = if (message.isMine) {
        MaterialTheme.colorScheme.primary
    } else {
        MaterialTheme.colorScheme.surfaceVariant
    }
    val bubbleColor = if (message.isRevoked) {
        MaterialTheme.colorScheme.surfaceVariant
    } else {
        baseBubbleColor
    }
    val textColor = if (message.isMine) Color.White else MaterialTheme.colorScheme.onSurface
    val highlightColor = if (message.isMine) {
        MaterialTheme.colorScheme.primary
    } else {
        MaterialTheme.colorScheme.secondary
    }

    val annotated = if (!message.isRevoked && message.body.contains("@Mina")) {
        buildAnnotatedString {
            val parts = message.body.split("@Mina")
            append(parts[0])
            withStyle(SpanStyle(color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.SemiBold)) {
                append("@Mina")
            }
            if (parts.size > 1) {
                append(parts[1])
            }
        }
    } else {
        buildAnnotatedString { append(message.body) }
    }

    Column(
        modifier = Modifier
            .widthIn(max = 280.dp)
            .clip(shape)
            .border(
                width = 1.5.dp,
                color = highlightColor.copy(alpha = 0.35f * highlightAlpha),
                shape = shape
            )
            .background(bubbleColor)
            .combinedClickable(
                onClick = {},
                onLongClick = {
                    haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                    onLongPress()
                }
            )
            .padding(horizontal = 14.dp, vertical = 10.dp)
    ) {
        if (message.isRevoked) {
            GroupRevokedMessageRow(isMine = message.isMine)
        } else {
            if (message.body.isNotBlank()) {
                Text(
                    text = annotated,
                    style = MaterialTheme.typography.bodyLarge,
                    color = textColor
                )
            }
            if (message.attachment != null) {
                if (message.body.isNotBlank()) {
                    Spacer(modifier = Modifier.height(8.dp))
                }
                AttachmentBlock(message.attachment, message.isMine, onClick = onAttachmentClick)
            }
        }
    }
}

@Composable
private fun GroupRevokedMessageRow(isMine: Boolean) {
    val textColor = MaterialTheme.colorScheme.onSurfaceVariant
    Row(verticalAlignment = Alignment.CenterVertically) {
        Icon(
            imageVector = Icons.AutoMirrored.Filled.Undo,
            contentDescription = "Message recalled",
            tint = textColor,
            modifier = Modifier.size(14.dp)
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = if (isMine) {
                tr("group_recalled_me", "You recalled a message")
            } else {
                tr("group_recalled_other", "The other person recalled a message")
            },
            style = MaterialTheme.typography.bodyMedium,
            color = textColor
        )
    }
}

@Composable
private fun GroupReadReceipts(readBy: List<String>) {
    val shown = readBy.take(3)
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        shown.forEach { label ->
            Box(
                modifier = Modifier
                    .size(14.dp)
                    .background(MaterialTheme.colorScheme.primary, CircleShape),
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = label.take(1),
                    style = MaterialTheme.typography.labelSmall,
                    color = Color.White,
                    fontSize = 8.sp
                )
            }
        }
        val extra = readBy.size - shown.size
        if (extra > 0) {
            Text(
                text = "+$extra",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
private fun GroupMessageActionSheet(
    message: GroupMessage,
    isPinned: Boolean,
    onDismiss: () -> Unit,
    onAction: (MessageAction) -> Unit
) {
    val recallInitial = message.recallSecondsLeft
    var recallRemaining by remember(message.id, recallInitial) {
        mutableStateOf(recallInitial ?: 0)
    }
    LaunchedEffect(message.id, recallInitial) {
        if (recallInitial != null) {
            var remaining = recallInitial
            recallRemaining = remaining
            while (remaining > 0) {
                delay(1000)
                remaining -= 1
                recallRemaining = remaining
            }
        }
    }
    val canRecall = message.isMine && !message.isRevoked && recallInitial != null && recallRemaining > 0
    val primaryActions = listOf(
        MessageAction("reply", tr("chat_action_reply", "Reply"), Icons.AutoMirrored.Filled.Reply),
        MessageAction("copy", tr("chat_action_copy", "Copy"), Icons.Filled.ContentCopy),
        MessageAction(
            "pin",
            if (isPinned) tr("chat_action_unpin", "Unpin") else tr("chat_action_pin", "Pin"),
            Icons.Filled.PushPin
        )
    )
    val secondaryActions = buildList {
        if (canRecall) {
            add(
                MessageAction(
                    "recall",
                    tr("chat_action_recall", "Recall"),
                    Icons.AutoMirrored.Filled.Undo,
                    isDestructive = true
                )
            )
        }
        add(
            MessageAction(
                "delete",
                tr("chat_action_delete", "Delete"),
                Icons.Filled.Delete,
                isDestructive = true
            )
        )
    }

    Box(modifier = Modifier.fillMaxSize()) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = 0.32f))
                .clickable(onClick = onDismiss)
        )
        Column(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .fillMaxWidth()
                .clip(RoundedCornerShape(topStart = 24.dp, topEnd = 24.dp))
                .background(MaterialTheme.colorScheme.surface)
                .padding(horizontal = 16.dp, vertical = 14.dp)
        ) {
            Box(
                modifier = Modifier
                    .align(Alignment.CenterHorizontally)
                    .width(36.dp)
                    .height(4.dp)
                    .background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(4.dp))
            )
            Spacer(modifier = Modifier.height(12.dp))
            GroupActionPreview(message)
            if (message.isMine && recallInitial != null) {
                Spacer(modifier = Modifier.height(8.dp))
                GroupRecallHintRow(
                    remainingSeconds = recallRemaining,
                    isAvailable = canRecall
                )
            }
            Spacer(modifier = Modifier.height(16.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                primaryActions.forEach { action ->
                    GroupActionIconButton(
                        action = action,
                        modifier = Modifier.weight(1f),
                        onClick = { onAction(action) }
                    )
                }
            }
            if (secondaryActions.isNotEmpty()) {
                Spacer(modifier = Modifier.height(12.dp))
                secondaryActions.forEach { action ->
                    GroupActionRow(
                        action = action,
                        onClick = { onAction(action) }
                    )
                }
            }
        }
    }
}

@Composable
private fun GroupActionPreview(message: GroupMessage) {
    val previewText = groupMessageCopyText(message)
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp))
            .background(MaterialTheme.colorScheme.surfaceVariant)
            .padding(horizontal = 12.dp, vertical = 10.dp)
    ) {
        Text(
            text = "${message.sender} / ${message.time}",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Spacer(modifier = Modifier.height(6.dp))
        Text(
            text = previewText,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurface,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
private fun GroupRecallHintRow(
    remainingSeconds: Int,
    isAvailable: Boolean
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            imageVector = Icons.AutoMirrored.Filled.Undo,
            contentDescription = "Recall available",
            tint = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.size(14.dp)
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = if (isAvailable) {
                tr("group_recall_available", "Recall available %s")
                    .format(formatGroupCountdown(remainingSeconds))
            } else {
                tr("group_recall_expired", "Recall expired")
            },
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
    }
}

@Composable
private fun GroupActionIconButton(
    action: MessageAction,
    modifier: Modifier = Modifier,
    onClick: () -> Unit
) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(16.dp))
            .clickable { onClick() },
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Box(
            modifier = Modifier
                .size(48.dp)
                .background(
                    MaterialTheme.colorScheme.primary.copy(alpha = 0.12f),
                    CircleShape
                ),
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = action.icon,
                contentDescription = action.label,
                tint = MaterialTheme.colorScheme.primary
            )
        }
        Spacer(modifier = Modifier.height(6.dp))
        Text(
            text = action.label,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
    }
}

@Composable
private fun GroupActionRow(action: MessageAction, onClick: () -> Unit) {
    val tint = if (action.isDestructive) MaterialTheme.colorScheme.error
    else MaterialTheme.colorScheme.onSurface
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .clickable { onClick() }
            .padding(horizontal = 8.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            imageVector = action.icon,
            contentDescription = action.label,
            tint = tint
        )
        Spacer(modifier = Modifier.width(12.dp))
        Text(
            text = action.label,
            style = MaterialTheme.typography.bodyLarge,
            color = tint
        )
    }
}

@Composable
private fun GroupToastPill(text: String, modifier: Modifier = Modifier) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surface,
        shadowElevation = 6.dp
    ) {
        Text(
            text = text,
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 10.dp),
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurface
        )
    }
}

@Composable
private fun GroupUndoPill(
    text: String,
    actionLabel: String,
    onAction: () -> Unit,
    modifier: Modifier = Modifier
) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surface,
        shadowElevation = 6.dp
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 16.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text(
                text = text,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurface
            )
            Spacer(modifier = Modifier.width(12.dp))
            Text(
                text = actionLabel,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.clickable { onAction() }
            )
        }
    }
}

private fun formatGroupCountdown(seconds: Int): String {
    val safe = seconds.coerceAtLeast(0)
    val minutes = safe / 60
    val secs = safe % 60
    return "%02d:%02d".format(minutes, secs)
}

private fun groupMessageCopyText(message: GroupMessage): String {
    if (message.isRevoked) {
        return "Message removed"
    }
    if (message.body.isNotBlank()) {
        return message.body
    }
    message.attachment?.let { attachment ->
        return listOfNotNull(attachment.label, attachment.meta)
            .joinToString(" ")
            .trim()
    }
    return "Message"
}

private fun groupReplySnippet(message: GroupMessage): String {
    val base = groupMessageCopyText(message)
    return if (base.length > 64) base.take(64) + "..." else base
}

private fun groupPinnedSnippet(message: GroupMessage): String {
    val base = groupMessageCopyText(message)
    return if (base.length > 90) base.take(90) + "..." else base
}

private fun recalledGroupMessageCopy(message: GroupMessage): GroupMessage {
    return message.copy(
        body = "",
        isRevoked = true,
        status = "Sent",
        readBy = emptyList(),
        recallSecondsLeft = null,
        attachment = null
    )
}

private fun buildGroupPinnedMessage(message: GroupMessage): PinnedMessage {
    return PinnedMessage(
        id = "grp_pin_${message.id}",
        title = "Pinned message",
        snippet = groupPinnedSnippet(message),
        messageId = message.id
    )
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun GroupChatPreview() {
    ChatTheme { GroupChatScreen(SampleGroupChat.items) }
}

private const val GROUP_PREFS_NAME = "mi_chat_prefs"

private fun keyGroupDeleted(conversationId: String) = "grp_deleted_$conversationId"
private fun keyGroupRecalled(conversationId: String) = "grp_recalled_$conversationId"
private fun keyGroupPinned(conversationId: String) = "grp_pinned_$conversationId"

private fun loadGroupDeletedIds(
    prefs: SharedPreferences,
    conversationId: String
): Set<String> {
    return prefs.getStringSet(keyGroupDeleted(conversationId), emptySet())
        ?.toSet()
        ?: emptySet()
}

private fun persistGroupDeletedIds(
    prefs: SharedPreferences,
    conversationId: String,
    ids: Set<String>
) {
    prefs.edit()
        .putStringSet(keyGroupDeleted(conversationId), HashSet(ids))
        .apply()
}

private fun loadGroupRecalledIds(
    prefs: SharedPreferences,
    conversationId: String
): Set<String> {
    return prefs.getStringSet(keyGroupRecalled(conversationId), emptySet())
        ?.toSet()
        ?: emptySet()
}

private fun persistGroupRecalledIds(
    prefs: SharedPreferences,
    conversationId: String,
    ids: Set<String>
) {
    prefs.edit()
        .putStringSet(keyGroupRecalled(conversationId), HashSet(ids))
        .apply()
}

private fun loadGroupPinnedId(
    prefs: SharedPreferences,
    conversationId: String
): String? {
    return prefs.getString(keyGroupPinned(conversationId), null)
}

private fun persistGroupPinnedId(
    prefs: SharedPreferences,
    conversationId: String,
    messageId: String?
) {
    prefs.edit()
        .putString(keyGroupPinned(conversationId), messageId ?: "")
        .apply()
}
