package mi.e2ee.android.ui

import android.content.Context
import android.content.SharedPreferences
import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.animateDpAsState
import androidx.compose.animation.core.animateFloatAsState
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.gestures.Orientation
import androidx.compose.foundation.gestures.draggable
import androidx.compose.foundation.gestures.rememberDraggableState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.LazyListState
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.AssistChip
import androidx.compose.material3.AssistChipDefaults
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledIconButton
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilledTonalIconButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.IconButtonDefaults
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.ListItem
import androidx.compose.material3.ListItemDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SuggestionChip
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.Switch
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.luminance
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.hapticfeedback.HapticFeedbackType
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalFocusManager
import androidx.compose.ui.platform.LocalHapticFeedback
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlin.math.roundToInt
import mi.e2ee.android.BuildConfig

sealed interface ChatItem {
    val id: String
}

data class DayMarker(
    override val id: String,
    val label: String
) : ChatItem

data class UnreadMarker(
    override val id: String,
    val count: Int
) : ChatItem

data class SystemNotice(
    override val id: String,
    val text: String
) : ChatItem

data class PinnedMessage(
    override val id: String,
    val title: String,
    val snippet: String,
    val messageId: String? = null
) : ChatItem

enum class MessageStatus {
    Sending,
    Sent,
    Delivered,
    Read,
    Failed
}

enum class ChatScreenState {
    Content,
    Empty,
    NetworkError,
    NotFound,
    PermissionDenied
}

data class ReplyPreview(
    val sender: String,
    val snippet: String,
    val messageId: String? = null
)

enum class AttachmentKind {
    File,
    Voice,
    Photo,
    Location,
    Sticker
}

data class Attachment(
    val kind: AttachmentKind,
    val label: String,
    val meta: String,
    val state: TransferState = TransferState.Ready,
    val progress: Float? = null,
    val fileId: String? = null,
    val fileKeyHex: String? = null,
    val fileSize: Long? = null
)

enum class TransferState {
    Ready,
    Downloading,
    Failed
}

data class MessageReaction(
    val label: String,
    val count: Int
)

data class LinkPreview(
    val title: String,
    val domain: String,
    val snippet: String
)

data class MessageAction(
    val id: String,
    val label: String,
    val icon: ImageVector,
    val isDestructive: Boolean = false
)

data class ChatMessage(
    override val id: String,
    val sender: String,
    val body: String,
    val time: String,
    val isMine: Boolean,
    val status: MessageStatus = MessageStatus.Read,
    val isEdited: Boolean = false,
    val isRevoked: Boolean = false,
    val recalledText: String? = null,
    val recallSecondsLeft: Int? = null,
    val readBy: List<String> = emptyList(),
    val forwardedFrom: String? = null,
    val replyTo: ReplyPreview? = null,
    val attachment: Attachment? = null,
    val linkPreview: LinkPreview? = null,
    val reactions: List<MessageReaction> = emptyList()
) : ChatItem

data class PendingMessageDelete(
    val message: ChatMessage
)

private sealed interface ComposerDialog {
    data object File : ComposerDialog
    data object Location : ComposerDialog
    data object Sticker : ComposerDialog
    data object Contact : ComposerDialog
}

internal enum class ComposerSurfaceState {
    Collapsed,
    Text,
    QuickActions,
    Emoji,
    Voice
}

object SampleChat {
    val items: List<ChatItem> = listOf(
        SystemNotice("notice1", "Messages are end-to-end encrypted."),
        PinnedMessage(
            "pin1",
            "Pinned message",
            "Great. Send the checklist and the risk notes.",
            messageId = "m2"
        ),
        DayMarker("day1", "Today"),
        ChatMessage(
            id = "m1",
            sender = "Aster",
            body = "Morning. I mapped the edge cases into a short checklist.",
            time = "08:12",
            isMine = false,
            reactions = listOf(
                MessageReaction("Like", 3),
                MessageReaction("Thanks", 1)
            )
        ),
        ChatMessage(
            id = "m2",
            sender = "Me",
            body = "Great. Send the checklist and the risk notes.",
            time = "08:13",
            isMine = true,
            status = MessageStatus.Read,
            readBy = listOf("AS"),
            replyTo = ReplyPreview("Aster", "Morning. I mapped the edge cases...")
        ),
        ChatMessage(
            id = "m2b",
            sender = "Me",
            body = "Forwarding the priority list from Ops.",
            time = "08:13",
            isMine = true,
            status = MessageStatus.Delivered,
            forwardedFrom = "Ops Channel"
        ),
        ChatMessage(
            id = "m3",
            sender = "Aster",
            body = "Uploading now. The top risk is retry storms after reconnect.",
            time = "08:14",
            isMine = false,
            attachment = Attachment(
                AttachmentKind.File,
                "Checklist.pdf",
                "230 KB",
                state = TransferState.Downloading,
                progress = 0.68f
            )
        ),
        ChatMessage(
            id = "m3b",
            sender = "Me",
            body = "Voice note with context.",
            time = "08:14",
            isMine = true,
            status = MessageStatus.Delivered,
            attachment = Attachment(AttachmentKind.Voice, "Voice message", "0:18")
        ),
        ChatMessage(
            id = "m3c",
            sender = "Aster",
            body = "Sharing the location now.",
            time = "08:15",
            isMine = false,
            attachment = Attachment(AttachmentKind.Location, "Civic Plaza", "2.1 km away")
        ),
        UnreadMarker("unread", 4),
        ChatMessage(
            id = "m4",
            sender = "Me",
            body = "Noted. I will add backoff and cap the queue depth.",
            time = "08:15",
            isMine = true,
            status = MessageStatus.Sent,
            recallSecondsLeft = 45
        ),
        DayMarker("day2", "Yesterday"),
        ChatMessage(
            id = "m5",
            sender = "Me",
            body = "Can you review the shared note for sensitive fields?",
            time = "21:18",
            isMine = true,
            status = MessageStatus.Read,
            isEdited = true,
            readBy = listOf("AS"),
            linkPreview = LinkPreview(
                title = "Privacy checklist",
                domain = "docs.mi.internal",
                snippet = "Checklist for local-only fields and redaction paths."
            )
        ),
        ChatMessage(
            id = "m6",
            sender = "Aster",
            body = "Already clean. I left redaction notes in the doc.",
            time = "21:20",
            isMine = false,
            attachment = Attachment(AttachmentKind.Photo, "Design.png", "1024x768")
        ),
        ChatMessage(
            id = "m6b",
            sender = "Aster",
            body = "",
            time = "21:21",
            isMine = false,
            isRevoked = true
        ),
        ChatMessage(
            id = "m7",
            sender = "Me",
            body = "Perfect. Shipping the build after tests.",
            time = "21:22",
            isMine = true,
            status = MessageStatus.Failed
        )
    )

    private val lenaItems: List<ChatItem> = listOf(
        SystemNotice("l_notice", "End-to-end encryption is enabled."),
        DayMarker("l_day1", "Today"),
        ChatMessage(
            id = "l1",
            sender = "Lena",
            body = "I can take the onboarding copy today.",
            time = "10:02",
            isMine = false
        ),
        ChatMessage(
            id = "l2",
            sender = "Me",
            body = "Thanks. I will send the draft in 30 minutes.",
            time = "10:03",
            isMine = true,
            status = MessageStatus.Delivered,
            readBy = listOf("LN")
        ),
        ChatMessage(
            id = "l3",
            sender = "Lena",
            body = "Sharing the updated illustration set.",
            time = "10:05",
            isMine = false,
            attachment = Attachment(AttachmentKind.Photo, "Onboarding.png", "1280x720")
        ),
        UnreadMarker("l_unread", 2),
        ChatMessage(
            id = "l4",
            sender = "Me",
            body = "Please also review the privacy copy.",
            time = "10:06",
            isMine = true,
            status = MessageStatus.Sent,
            isEdited = true,
            replyTo = ReplyPreview("Lena", "Sharing the updated illustration set.")
        ),
        ChatMessage(
            id = "l5",
            sender = "Lena",
            body = "",
            time = "10:08",
            isMine = false,
            isRevoked = true
        )
    )

    private val riskItems: List<ChatItem> = listOf(
        SystemNotice("r_notice", "Messages are end-to-end encrypted."),
        DayMarker("r_day1", "Today"),
        ChatMessage(
            id = "r1",
            sender = "Helio",
            body = "Draft risk review ready for you.",
            time = "09:30",
            isMine = false
        ),
        ChatMessage(
            id = "r2",
            sender = "Me",
            body = "Share the top risks and the mitigation plan.",
            time = "09:31",
            isMine = true,
            status = MessageStatus.Read,
            readBy = listOf("HR")
        ),
        ChatMessage(
            id = "r3",
            sender = "Helio",
            body = "Linking the risk board.",
            time = "09:33",
            isMine = false,
            linkPreview = LinkPreview(
                title = "Risk board v4",
                domain = "risk.mi.internal",
                snippet = "Backoff, retry storms, privacy redaction."
            )
        ),
        ChatMessage(
            id = "r4",
            sender = "Me",
            body = "Acknowledged. I will sync with Ops.",
            time = "09:34",
            isMine = true,
            status = MessageStatus.Sent,
            recallSecondsLeft = 30
        )
    )

    fun itemsFor(conversationId: String?): List<ChatItem> {
        return when (conversationId) {
            "c3" -> lenaItems
            "c5" -> riskItems
            else -> items
        }
    }
}

@Composable
fun ChatApp() {
    ChatTheme {
        ChatScreen(items = SampleChat.items)
    }
}

@Composable
fun ChatScreen(
    items: List<ChatItem>,
    conversationId: String = "default",
    title: String = "Aster Stone",
    status: String = "Online",
    initials: String = "AS",
    selfInitials: String = "ME",
    showTyping: Boolean = true,
    screenState: ChatScreenState = ChatScreenState.Content,
    onBack: () -> Unit = {},
    onOpenAccount: () -> Unit = {},
    onSearch: () -> Unit = {},
    onOpenSettings: () -> Unit = {},
    onStartCall: () -> Unit = {},
    onStartVideoCall: () -> Unit = {},
    onSendPresence: (Boolean) -> Unit = {},
    onSendReadReceipt: (String) -> Unit = {},
    onResendText: (String, String) -> Boolean = { _, _ -> false },
    onResendTextWithReply: (String, String, String, String) -> Boolean = { _, _, _, _ -> false },
    onResendFile: (String, String) -> Boolean = { _, _ -> false },
    onSendMessage: (String, ReplyPreview?) -> Boolean = { _, _ -> false },
    onSendFile: (String) -> Boolean = { false },
    onSendLocation: (Double, Double, String) -> Boolean = { _, _, _ -> false },
    onSendSticker: (String) -> Boolean = { false },
    onSendContact: (String, String) -> Boolean = { _, _ -> false },
    onTyping: (Boolean) -> Unit = {},
    onRecallMessage: (String) -> Boolean = { false },
    onDownloadAttachment: (Attachment) -> Unit = {}
) {
    var ready by remember { mutableStateOf(false) }
    var actionTarget by remember { mutableStateOf<ChatMessage?>(null) }
    var toastMessage by remember { mutableStateOf<String?>(null) }
    var composerReply by remember { mutableStateOf<ReplyPreview?>(null) }
    var composerText by remember { mutableStateOf("") }
    var composerSurface by remember { mutableStateOf(ComposerSurfaceState.Collapsed) }
    var composerDialog by remember { mutableStateOf<ComposerDialog?>(null) }
    var pendingDelete by remember { mutableStateOf<PendingMessageDelete?>(null) }
    var toolsOpen by remember { mutableStateOf(false) }
    var toolsPresenceOnline by remember { mutableStateOf(true) }
    var toolsReceiptId by remember { mutableStateOf("") }
    var toolsResendId by remember { mutableStateOf("") }
    var toolsResendText by remember { mutableStateOf("") }
    var toolsResendReplyTo by remember { mutableStateOf("") }
    var toolsResendReplyPreview by remember { mutableStateOf("") }
    var toolsResendFileId by remember { mutableStateOf("") }
    var toolsResendFilePath by remember { mutableStateOf("") }
    var toolsResult by remember { mutableStateOf<String?>(null) }
    val context = LocalContext.current
    val focusManager = LocalFocusManager.current
    val composerFocusRequester = remember { FocusRequester() }
    val resolvedConversationId = if (conversationId.isNotBlank()) conversationId else "default"
    val prefs = remember { context.getSharedPreferences(CHAT_PREFS_NAME, Context.MODE_PRIVATE) }
    var highlightedMessageId by remember(resolvedConversationId) { mutableStateOf<String?>(null) }
    val listState = rememberLazyListState()
    val scope = rememberCoroutineScope()
    var composerFieldFocused by remember { mutableStateOf(false) }
    var deletedIds by remember(resolvedConversationId) {
        mutableStateOf(loadDeletedIds(prefs, resolvedConversationId))
    }
    var recalledIds by remember(resolvedConversationId) {
        mutableStateOf(loadRecalledIds(prefs, resolvedConversationId))
    }
    var favoriteIds by remember(resolvedConversationId) {
        mutableStateOf(loadFavoriteIds(prefs, resolvedConversationId))
    }
    var pinnedId by remember(resolvedConversationId) {
        mutableStateOf(loadPinnedId(prefs, resolvedConversationId))
    }
    val pinnedIdValue = pinnedId
    val normalizedPinnedId = pinnedIdValue?.takeIf { it.isNotBlank() }
    val normalizedMessages = items
        .filterNot { it is PinnedMessage }
        .map { item ->
            if (item is ChatMessage && recalledIds.contains(item.id)) {
                recalledMessageCopy(item)
            } else {
                item
            }
        }
    val visibleMessages = normalizedMessages.filterNot { item ->
        item is ChatMessage && deletedIds.contains(item.id)
    }
    val pinnedFromId = normalizedPinnedId?.let { id ->
        visibleMessages.firstOrNull { item ->
            item is ChatMessage && item.id == id
        } as? ChatMessage
    }?.let { message -> buildPinnedMessage(message) }
    val defaultPinned = items.filterIsInstance<PinnedMessage>().firstOrNull()
    val pinnedFromDefault = defaultPinned?.messageId?.let { id ->
        visibleMessages.firstOrNull { item ->
            item is ChatMessage && item.id == id
        } as? ChatMessage
    }?.let { message ->
        buildPinnedMessage(message)
    } ?: defaultPinned
    val pinnedMessage = when {
        pinnedIdValue == null -> pinnedFromDefault
        pinnedIdValue.isBlank() -> null
        else -> pinnedFromId
    }
    val lastIncomingMessage = visibleMessages.filterIsInstance<ChatMessage>().lastOrNull { !it.isMine }
    val lastOutgoingMessage = visibleMessages.filterIsInstance<ChatMessage>().lastOrNull { it.isMine }
    val lastOutgoingFileMessage = visibleMessages.filterIsInstance<ChatMessage>()
        .lastOrNull { it.isMine && it.attachment?.kind == AttachmentKind.File }
    val showDebugTools = BuildConfig.DEBUG
    val clipboard = LocalClipboardManager.current
    val strings = LocalStrings.current
    fun t(key: String, fallback: String): String = strings.get(key, fallback)
    val headerAlpha by animateFloatAsState(
        targetValue = if (ready) 1f else 0f,
        animationSpec = tween(520),
        label = "headerAlpha"
    )
    val headerOffset by animateDpAsState(
        targetValue = if (ready) 0.dp else 10.dp,
        animationSpec = tween(520),
        label = "headerOffset"
    )
    val inputAlpha by animateFloatAsState(
        targetValue = if (ready) 1f else 0f,
        animationSpec = tween(560, delayMillis = 120),
        label = "inputAlpha"
    )
    val inputOffset by animateDpAsState(
        targetValue = if (ready) 0.dp else 10.dp,
        animationSpec = tween(560, delayMillis = 120),
        label = "inputOffset"
    )
    var typingActive by remember(resolvedConversationId) { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        ready = true
    }
    LaunchedEffect(items) {
        pendingDelete = null
        actionTarget = null
        toastMessage = null
    }
    LaunchedEffect(composerText) {
        val active = composerText.isNotBlank()
        if (active != typingActive) {
            onTyping(active)
            typingActive = active
        }
    }
    DisposableEffect(resolvedConversationId) {
        onDispose {
            if (typingActive) {
                onTyping(false)
                typingActive = false
            }
        }
    }
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

    fun openTools() {
        if (!showDebugTools) return
        toolsPresenceOnline = true
        toolsReceiptId = lastIncomingMessage?.id ?: ""
        toolsResendId = lastOutgoingMessage?.id ?: ""
        toolsResendText = lastOutgoingMessage?.body ?: ""
        toolsResendReplyTo = lastOutgoingMessage?.replyTo?.messageId ?: ""
        toolsResendReplyPreview = lastOutgoingMessage?.replyTo?.snippet ?: ""
        toolsResendFileId = lastOutgoingFileMessage?.id ?: ""
        toolsResendFilePath = loadLastFilePath(prefs, resolvedConversationId) ?: ""
        toolsResult = null
        toolsOpen = true
    }

    fun setPinnedMessage(message: ChatMessage?) {
        pinnedId = message?.id ?: ""
        persistPinnedId(prefs, resolvedConversationId, pinnedId)
    }

    fun requestDelete(message: ChatMessage) {
        pendingDelete = PendingMessageDelete(message)
        deletedIds = deletedIds + message.id
        persistDeletedIds(prefs, resolvedConversationId, deletedIds)
        if (pinnedId == message.id) {
            setPinnedMessage(null)
        }
    }

    val effectiveState = when {
        screenState != ChatScreenState.Content -> screenState
        visibleMessages.filterIsInstance<ChatMessage>().isEmpty() -> ChatScreenState.Empty
        else -> ChatScreenState.Content
    }
    val quickActionsVisible = composerSurface == ComposerSurfaceState.QuickActions
    val emojiSurfaceVisible = composerSurface == ComposerSurfaceState.Emoji
    val voiceSurfaceVisible = composerSurface == ComposerSurfaceState.Voice
    val showComposer = effectiveState != ChatScreenState.NotFound &&
        effectiveState != ChatScreenState.PermissionDenied
    fun collapseTextComposer() {
        focusManager.clearFocus(force = true)
        composerFieldFocused = false
        composerSurface = ComposerSurfaceState.Collapsed
    }
    BackHandler(enabled = actionTarget != null ||
        composerDialog != null ||
        composerReply != null ||
        quickActionsVisible ||
        emojiSurfaceVisible ||
        voiceSurfaceVisible ||
        composerFieldFocused) {
        when {
            actionTarget != null -> actionTarget = null
            composerDialog != null -> composerDialog = null
            composerSurface == ComposerSurfaceState.QuickActions ||
                composerSurface == ComposerSurfaceState.Emoji ||
                composerSurface == ComposerSurfaceState.Voice -> {
                composerSurface = ComposerSurfaceState.Collapsed
            }
            composerReply != null -> composerReply = null
            composerFieldFocused -> collapseTextComposer()
        }
    }
    Scaffold(
        topBar = {
            ChatTopBar(
                title = title,
                initials = initials,
                selfInitials = selfInitials,
                onBack = onBack,
                onSelfClick = onOpenAccount,
                onSearch = onSearch,
                onSettings = onOpenSettings,
                onCall = onStartCall,
                onTools = { openTools() },
                modifier = Modifier
                    .alpha(headerAlpha)
                    .padding(top = headerOffset)
            )
        },
        bottomBar = if (showComposer) {
            {
                ComposerBar(
                    message = composerText,
                    onMessageChange = { composerText = it },
                    onSend = {
                        val ok = onSendMessage(composerText, composerReply)
                        if (ok) {
                            composerText = ""
                            composerReply = null
                            composerFocusRequester.requestFocus()
                        }
                    },
                    surfaceState = composerSurface,
                    focusRequester = composerFocusRequester,
                    onTextFieldFocusChange = { focused ->
                        composerFieldFocused = focused
                        if (focused) {
                            composerSurface = ComposerSurfaceState.Text
                        } else if (composerSurface == ComposerSurfaceState.Text) {
                            composerSurface = ComposerSurfaceState.Collapsed
                        }
                    },
                    onToggleQuickActions = {
                        focusManager.clearFocus(force = true)
                        composerFieldFocused = false
                        composerSurface = if (quickActionsVisible) {
                            ComposerSurfaceState.Collapsed
                        } else {
                            ComposerSurfaceState.QuickActions
                        }
                    },
                    onEmoji = {
                        focusManager.clearFocus(force = true)
                        composerFieldFocused = false
                        composerSurface = if (emojiSurfaceVisible) {
                            ComposerSurfaceState.Collapsed
                        } else {
                            ComposerSurfaceState.Emoji
                        }
                    },
                    onVoice = {
                        focusManager.clearFocus(force = true)
                        composerFieldFocused = false
                        composerSurface = if (voiceSurfaceVisible) {
                            ComposerSurfaceState.Collapsed
                        } else {
                            ComposerSurfaceState.Voice
                        }
                    },
                    modifier = Modifier
                        .alpha(inputAlpha)
                        .padding(bottom = inputOffset)
                )
            }
        } else {
            {}
        },
        containerColor = Color.Transparent
    ) { padding ->
        val composerOverlayHeight = when {
            composerReply != null && quickActionsVisible -> 68.dp
            composerReply != null -> 36.dp
            quickActionsVisible -> 32.dp
            emojiSurfaceVisible -> 96.dp
            voiceSurfaceVisible -> 64.dp
            else -> 0.dp
        }
        val composerInset = if (showComposer) {
            padding.calculateBottomPadding() + composerOverlayHeight + 8.dp
        } else {
            10.dp
        }
        Box(
            modifier = Modifier
                .fillMaxSize()
                .testTag("chat-screen")
        ) {
            ChatBackground()
            if (effectiveState == ChatScreenState.Content || effectiveState == ChatScreenState.Empty ||
                effectiveState == ChatScreenState.NetworkError
            ) {
                MessageList(
                    items = visibleMessages,
                    pinnedMessage = pinnedMessage,
                    listState = listState,
                    highlightedMessageId = highlightedMessageId,
                    favoriteIds = favoriteIds,
                    onPinnedClick = { messageId ->
                        val index = visibleMessages.indexOfFirst { it.id == messageId }
                        if (index != -1) {
                            val listIndex = if (pinnedMessage != null) index + 1 else index
                            scope.launch { listState.animateScrollToItem(listIndex) }
                            highlightedMessageId = messageId
                        } else {
                            toastMessage = t("chat_message_not_found", "Message not found")
                        }
                    },
                    showTyping = showTyping && effectiveState == ChatScreenState.Content,
                    onMessageLongPress = { message -> actionTarget = message },
                    onSwipeReply = { message ->
                        composerReply = ReplyPreview(
                            sender = message.sender,
                            snippet = replySnippet(message),
                            messageId = message.id
                        )
                        toastMessage = t("chat_reply_started", "Reply started")
                    },
                    onReEdit = { recalled ->
                        val restored = recalled.recalledText
                        if (!restored.isNullOrBlank()) {
                            composerText = restored
                            toastMessage = t("chat_draft_restored", "Draft restored")
                        }
                    },
                    onAttachmentClick = onDownloadAttachment,
                    contentPadding = PaddingValues(
                        top = padding.calculateTopPadding() + 6.dp,
                        bottom = composerInset,
                        start = 10.dp,
                        end = 10.dp
                    )
                )
            }
            if (showComposer && (composerReply != null || quickActionsVisible)) {
                ComposerAssistOverlay(
                    replyPreview = composerReply,
                    showQuickActions = quickActionsVisible,
                    onReplyDismiss = { composerReply = null },
                    onAttachPhoto = {
                        composerSurface = ComposerSurfaceState.Collapsed
                        composerDialog = ComposerDialog.File
                    },
                    onAttachFile = {
                        composerSurface = ComposerSurfaceState.Collapsed
                        composerDialog = ComposerDialog.File
                    },
                    onAttachLocation = {
                        composerSurface = ComposerSurfaceState.Collapsed
                        composerDialog = ComposerDialog.Location
                    },
                    onAttachContact = {
                        composerSurface = ComposerSurfaceState.Collapsed
                        composerDialog = ComposerDialog.Contact
                    },
                    onAttachSticker = {
                        composerSurface = ComposerSurfaceState.Collapsed
                        composerDialog = ComposerDialog.Sticker
                    },
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .alpha(inputAlpha)
                        .padding(
                            start = 6.dp,
                            end = 6.dp,
                            bottom = padding.calculateBottomPadding() + 4.dp + inputOffset
                        )
                )
            }
            if (showComposer && (emojiSurfaceVisible || voiceSurfaceVisible)) {
                ComposerSurfacePanel(
                    surfaceState = composerSurface,
                    onDismiss = { composerSurface = ComposerSurfaceState.Collapsed },
                    onAppendEmoji = { emoji -> composerText += emoji },
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .padding(
                            start = 6.dp,
                            end = 6.dp,
                            bottom = padding.calculateBottomPadding() + 46.dp + inputOffset
                        )
                )
            }
            if (effectiveState == ChatScreenState.Empty) {
                ChatEmptyState(
                    title = tr("chat_empty_title", "No messages yet"),
                    message = tr("chat_empty_message", "Start the conversation with a secure hello."),
                    actionLabel = tr("chat_empty_action", "Start chatting"),
                    modifier = Modifier
                        .align(Alignment.Center)
                        .padding(horizontal = 32.dp)
                )
            }
            if (effectiveState == ChatScreenState.NetworkError) {
                NetworkBanner(
                    text = tr("chat_network_banner", "Network unstable. Reconnecting..."),
                    modifier = Modifier
                        .align(Alignment.TopCenter)
                        .padding(
                            top = padding.calculateTopPadding() + 12.dp,
                            start = 16.dp,
                            end = 16.dp
                        )
                )
            }
            if (effectiveState == ChatScreenState.NotFound) {
                ChatStatusState(
                    icon = MiOwnedIcons.Alert,
                    title = tr("chat_not_found_title", "Conversation not found"),
                    message = tr(
                        "chat_not_found_message",
                        "This chat no longer exists. Return to the list to continue."
                    ),
                    actionLabel = tr("chat_not_found_action", "Back to chats"),
                    modifier = Modifier
                        .align(Alignment.Center)
                        .padding(horizontal = 32.dp)
                )
            }
            if (effectiveState == ChatScreenState.PermissionDenied) {
                ChatStatusState(
                    icon = MiOwnedIcons.Lock,
                    title = tr("chat_permission_title", "Permission required"),
                    message = tr("chat_permission_message", "You don't have access to this conversation."),
                    actionLabel = tr("chat_permission_action", "Go to settings"),
                    modifier = Modifier
                        .align(Alignment.Center)
                        .padding(horizontal = 32.dp)
                )
            }
            if (actionTarget != null) {
                val isPinned = pinnedMessage?.messageId == actionTarget?.id
                val isFavorite = actionTarget?.id?.let { favoriteIds.contains(it) } == true
                MessageActionSheet(
                    message = actionTarget!!,
                    isPinned = isPinned,
                    isFavorite = isFavorite,
                    onDismiss = { actionTarget = null },
                    onAction = { action ->
                        val message = actionTarget ?: return@MessageActionSheet
                        val toast = when (action.id) {
                            "reply" -> {
                                composerReply = ReplyPreview(
                                    sender = message.sender,
                                    snippet = replySnippet(message),
                                    messageId = message.id
                                )
                                t("chat_reply_started", "Reply started")
                            }
                            "copy" -> {
                                SecureClipboard.copyProtectedText(
                                    clipboard,
                                    messageCopyText(message)
                                )
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
                            "favorite" -> {
                                val updated = if (favoriteIds.contains(message.id)) {
                                    favoriteIds - message.id
                                } else {
                                    favoriteIds + message.id
                                }
                                favoriteIds = updated
                                persistFavoriteIds(prefs, resolvedConversationId, updated)
                                if (updated.contains(message.id)) {
                                    t("chat_saved", "Saved")
                                } else {
                                    t("chat_removed", "Removed")
                                }
                            }
                            "recall" -> {
                                val ok = onRecallMessage(message.id)
                                if (ok) {
                                    recalledIds = recalledIds + message.id
                                    persistRecalledIds(prefs, resolvedConversationId, recalledIds)
                                    t("chat_recalled", "Recalled")
                                } else {
                                    t("chat_recall_failed", "Recall failed")
                                }
                            }
                            "delete" -> {
                                requestDelete(message)
                                null
                            }
                            "forward" -> t("chat_forwarded", "Forwarded")
                            else -> action.label
                        }
                        if (toast != null) {
                            toastMessage = toast
                        }
                        actionTarget = null
                    }
                )
            }
            if (showDebugTools && toolsOpen) {
                AlertDialog(
                    onDismissRequest = { toolsOpen = false },
                    title = { Text(tr("chat_tools_title", "Quick tools")) },
                    text = {
                        Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                            Text(
                                text = tr("chat_tools_presence", "Presence"),
                                style = MaterialTheme.typography.labelMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text(tr("chat_tools_online", "Online"))
                                Spacer(modifier = Modifier.weight(1f))
                                Switch(
                                    checked = toolsPresenceOnline,
                                    onCheckedChange = { toolsPresenceOnline = it }
                                )
                            }
                            TextButton(onClick = {
                                onSendPresence(toolsPresenceOnline)
                                toolsResult = t("chat_tools_presence_sent", "Presence sent")
                            }) {
                                Text(tr("chat_tools_send_presence", "Send presence"))
                            }

                            Text(
                                text = tr("chat_tools_receipt", "Read receipt"),
                                style = MaterialTheme.typography.labelMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                            OutlinedTextField(
                                value = toolsReceiptId,
                                onValueChange = { toolsReceiptId = it },
                                label = { Text(tr("chat_tools_message_id", "Message id")) },
                                keyboardOptions = secureKeyboardOptions(),
                                singleLine = true
                            )
                            TextButton(
                                onClick = {
                                    if (toolsReceiptId.isNotBlank()) {
                                        onSendReadReceipt(toolsReceiptId.trim())
                                        toolsResult = t("chat_tools_receipt_sent", "Read receipt sent")
                                    } else {
                                        toolsResult = t("chat_tools_invalid", "Invalid input")
                                    }
                                }
                            ) {
                                Text(tr("chat_tools_send_receipt", "Send receipt"))
                            }

                            Text(
                                text = tr("chat_tools_resend", "Resend text"),
                                style = MaterialTheme.typography.labelMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                            OutlinedTextField(
                                value = toolsResendId,
                                onValueChange = { toolsResendId = it },
                                label = { Text(tr("chat_tools_message_id", "Message id")) },
                                keyboardOptions = secureKeyboardOptions(),
                                singleLine = true
                            )
                            OutlinedTextField(
                                value = toolsResendText,
                                onValueChange = { toolsResendText = it },
                                label = { Text(tr("chat_tools_text", "Text")) },
                                keyboardOptions = secureKeyboardOptions()
                            )
                            OutlinedTextField(
                                value = toolsResendReplyTo,
                                onValueChange = { toolsResendReplyTo = it },
                                label = { Text(tr("chat_tools_reply_id", "Reply message id (optional)")) },
                                keyboardOptions = secureKeyboardOptions(),
                                singleLine = true
                            )
                            OutlinedTextField(
                                value = toolsResendReplyPreview,
                                onValueChange = { toolsResendReplyPreview = it },
                                label = { Text(tr("chat_tools_reply_preview", "Reply preview (optional)")) },
                                keyboardOptions = secureKeyboardOptions()
                            )
                            TextButton(
                                onClick = {
                                    if (toolsResendId.isBlank() || toolsResendText.isBlank()) {
                                        toolsResult = t("chat_tools_invalid", "Invalid input")
                                    } else if (toolsResendReplyTo.isNotBlank()) {
                                        if (toolsResendReplyPreview.isBlank()) {
                                            toolsResult = t("chat_tools_invalid", "Invalid input")
                                        } else {
                                            val ok = onResendTextWithReply(
                                                toolsResendId.trim(),
                                                toolsResendText.trim(),
                                                toolsResendReplyTo.trim(),
                                                toolsResendReplyPreview.trim()
                                            )
                                            toolsResult = if (ok) {
                                                t("chat_tools_resend_ok", "Resend queued")
                                            } else {
                                                t("chat_tools_resend_failed", "Resend failed")
                                            }
                                        }
                                    } else {
                                        val ok = onResendText(toolsResendId.trim(), toolsResendText.trim())
                                        toolsResult = if (ok) {
                                            t("chat_tools_resend_ok", "Resend queued")
                                        } else {
                                            t("chat_tools_resend_failed", "Resend failed")
                                        }
                                    }
                                }
                            ) {
                                Text(tr("chat_tools_resend_send", "Resend"))
                            }

                            Text(
                                text = tr("chat_tools_resend_file", "Resend file"),
                                style = MaterialTheme.typography.labelMedium,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                            OutlinedTextField(
                                value = toolsResendFileId,
                                onValueChange = { toolsResendFileId = it },
                                label = { Text(tr("chat_tools_message_id", "Message id")) },
                                keyboardOptions = secureKeyboardOptions(),
                                singleLine = true
                            )
                            OutlinedTextField(
                                value = toolsResendFilePath,
                                onValueChange = { toolsResendFilePath = it },
                                label = { Text(tr("chat_tools_file_path", "File path")) },
                                keyboardOptions = secureKeyboardOptions()
                            )
                            TextButton(
                                onClick = {
                                    if (toolsResendFileId.isBlank() || toolsResendFilePath.isBlank()) {
                                        toolsResult = t("chat_tools_invalid", "Invalid input")
                                    } else {
                                        val ok = onResendFile(toolsResendFileId.trim(), toolsResendFilePath.trim())
                                        toolsResult = if (ok) {
                                            t("chat_tools_resend_ok", "Resend queued")
                                        } else {
                                            t("chat_tools_resend_failed", "Resend failed")
                                        }
                                    }
                                }
                            ) {
                                Text(tr("chat_tools_resend_send", "Resend"))
                            }

                            toolsResult?.let { result ->
                                Text(
                                    text = result,
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant
                                )
                            }
                        }
                    },
                    confirmButton = {
                        TextButton(onClick = { toolsOpen = false }) {
                            Text(tr("chat_close", "Close"))
                        }
                    }
                )
            }
            if (composerDialog != null) {
                when (composerDialog) {
                    ComposerDialog.File -> {
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
                                    keyboardOptions = secureKeyboardOptions(),
                                    singleLine = true
                                )
                            },
                            confirmButton = {
                                TextButton(
                                    onClick = {
                                        if (path.isNotBlank() && onSendFile(path)) {
                                            persistLastFilePath(prefs, resolvedConversationId, path)
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
                    ComposerDialog.Location -> {
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
                                        keyboardOptions = secureKeyboardOptions(),
                                        singleLine = true
                                    )
                                    OutlinedTextField(
                                        value = lat,
                                        onValueChange = { lat = it },
                                        label = { Text(tr("chat_location_lat", "Latitude")) },
                                        placeholder = { Text("31.2304") },
                                        keyboardOptions = secureKeyboardOptions(),
                                        singleLine = true
                                    )
                                    OutlinedTextField(
                                        value = lon,
                                        onValueChange = { lon = it },
                                        label = { Text(tr("chat_location_lon", "Longitude")) },
                                        placeholder = { Text("121.4737") },
                                        keyboardOptions = secureKeyboardOptions(),
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
                    ComposerDialog.Sticker -> {
                        var stickerId by remember { mutableStateOf("") }
                        AlertDialog(
                            onDismissRequest = { composerDialog = null },
                            title = { Text(tr("chat_attach_sticker", "Send sticker")) },
                            text = {
                                OutlinedTextField(
                                    value = stickerId,
                                    onValueChange = { stickerId = it },
                                    label = { Text(tr("chat_sticker_id", "Sticker id")) },
                                    placeholder = { Text(tr("chat_sticker_id_hint", "sticker_01")) },
                                    keyboardOptions = secureKeyboardOptions(),
                                    singleLine = true
                                )
                            },
                            confirmButton = {
                                TextButton(
                                    onClick = {
                                        if (stickerId.isNotBlank() && onSendSticker(stickerId)) {
                                            composerDialog = null
                                        }
                                    },
                                    enabled = stickerId.isNotBlank()
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
                    ComposerDialog.Contact -> {
                        var username by remember { mutableStateOf("") }
                        var display by remember { mutableStateOf("") }
                        val canSend = username.isNotBlank() && display.isNotBlank()
                        AlertDialog(
                            onDismissRequest = { composerDialog = null },
                            title = { Text(tr("chat_attach_contact", "Share contact")) },
                            text = {
                                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                                    OutlinedTextField(
                                        value = username,
                                        onValueChange = { username = it },
                                        label = { Text(tr("chat_contact_username", "Username")) },
                                        placeholder = { Text(tr("chat_contact_username_hint", "mi_user")) },
                                        keyboardOptions = secureKeyboardOptions(),
                                        singleLine = true
                                    )
                                    OutlinedTextField(
                                        value = display,
                                        onValueChange = { display = it },
                                        label = { Text(tr("chat_contact_display", "Display name")) },
                                        placeholder = { Text(tr("chat_contact_display_hint", "Mina Ito")) },
                                        keyboardOptions = secureKeyboardOptions(),
                                        singleLine = true
                                    )
                                }
                            },
                            confirmButton = {
                                TextButton(
                                    onClick = {
                                        if (canSend && onSendContact(username, display)) {
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
                UndoPill(
                    text = tr("chat_message_deleted", "Message deleted"),
                    actionLabel = tr("chat_undo", "Undo"),
                    onAction = {
                        val restore = pendingDelete ?: return@UndoPill
                        deletedIds = deletedIds - restore.message.id
                        persistDeletedIds(prefs, resolvedConversationId, deletedIds)
                        pendingDelete = null
                        toastMessage = t("chat_restored", "Restored")
                    },
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .padding(bottom = composerInset + 36.dp)
                )
            } else if (toastMessage != null) {
                ToastPill(
                    text = toastMessage!!,
                    modifier = Modifier
                        .align(Alignment.BottomCenter)
                        .padding(bottom = composerInset + 36.dp)
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ChatTopBar(
    title: String,
    initials: String,
    selfInitials: String,
    onBack: () -> Unit,
    onSelfClick: () -> Unit,
    onSearch: () -> Unit,
    onSettings: () -> Unit,
    onCall: () -> Unit,
    onTools: () -> Unit,
    modifier: Modifier = Modifier
) {
    val colors = phaseOneColors()
    // .height(80.dp)
    // Modifier.align(Alignment.Center)
    // padding(horizontal = 76.dp)
    Surface(
        modifier = modifier.fillMaxWidth(),
        shape = RoundedCornerShape(
            bottomStart = 22.dp,
            bottomEnd = 22.dp
        ),
        color = colors.glass,
        shadowElevation = 0.dp
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .statusBarsPadding()
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(start = 16.dp, end = 16.dp, top = 4.dp, bottom = 8.dp),
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    UiToolbarIconButton(
                        icon = MiOwnedIcons.ArrowBack,
                        contentDescription = tr("chat_back", "Back"),
                        onClick = onBack
                    )
                    Spacer(modifier = Modifier.weight(1f))
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        TopBarActionIcon(
                            icon = MiOwnedIcons.Search,
                            contentDescription = tr("chat_search", "Search"),
                            tone = UiIconTone.Neutral,
                            onClick = onSearch
                        )
                        TopBarActionIcon(
                            icon = MiOwnedIcons.Call,
                            contentDescription = tr("chat_call", "Call"),
                            tone = UiIconTone.Accent,
                            onClick = onCall
                        )
                        TopBarActionIcon(
                            icon = MiOwnedIcons.MoreVertical,
                            contentDescription = tr("chat_more", "More"),
                            tone = UiIconTone.Neutral,
                            onClick = if (BuildConfig.DEBUG) onTools else onSettings
                        )
                        Box(modifier = Modifier.clickable(onClick = onSelfClick)) {
                            IdentityAvatar(
                                label = selfInitials.ifBlank { initials },
                                seed = "chat-self-$selfInitials",
                                kind = IdentityAvatarKind.Person,
                                size = 34.dp
                            )
                        }
                    }
                }
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(72.dp)
                ) {
                    Column(
                        modifier = Modifier
                            .align(Alignment.Center)
                            .padding(horizontal = 76.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        Text(
                            text = title,
                            style = MaterialTheme.typography.titleLarge.copy(
                                fontWeight = FontWeight.SemiBold,
                                letterSpacing = (-0.2).sp
                            ),
                            color = colors.onSurface,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                        Text(
                            text = tr("chat_header_subtitle", "Encrypted conversation"),
                            style = MaterialTheme.typography.bodySmall,
                            color = colors.onSurfaceMuted,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                }
            }
            Box(
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth()
                    .height(0.5.dp)
                    .background(
                        if (MaterialTheme.colorScheme.background.luminance() > 0.5f) {
                            Color.Black.copy(alpha = 0.05f)
                        } else {
                            Color.White.copy(alpha = 0.10f)
                        }
                    )
            )
        }
    }
}

@Composable
private fun TopBarActionIcon(
    icon: ImageVector,
    contentDescription: String,
    tone: UiIconTone,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    UiToolbarIconButton(
        icon = icon,
        contentDescription = contentDescription,
        tone = tone,
        onClick = onClick,
        modifier = modifier
    )
}

@Composable
private fun MessageList(
    items: List<ChatItem>,
    pinnedMessage: PinnedMessage?,
    listState: LazyListState,
    highlightedMessageId: String?,
    favoriteIds: Set<String>,
    onPinnedClick: (String) -> Unit,
    showTyping: Boolean,
    onMessageLongPress: (ChatMessage) -> Unit,
    onSwipeReply: (ChatMessage) -> Unit,
    onReEdit: (ChatMessage) -> Unit,
    onAttachmentClick: (Attachment) -> Unit,
    contentPadding: PaddingValues
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize(),
        state = listState,
        contentPadding = contentPadding,
        verticalArrangement = Arrangement.spacedBy(2.dp)
    ) {
        itemsIndexed(items, key = { _, item -> item.id }) { index, item ->
            when (item) {
                is DayMarker -> DaySeparator(item.label)
                is UnreadMarker -> UnreadSeparator(item.count)
                is SystemNotice -> Unit
                is PinnedMessage -> Unit
                is ChatMessage -> {
                    val previous = items.getOrNull(index - 1) as? ChatMessage
                    val next = items.getOrNull(index + 1) as? ChatMessage
                    val isGroupedAbove = previous?.let {
                        it.isMine == item.isMine && it.sender == item.sender
                    } ?: false
                    val isGroupedBelow = next?.let {
                        it.isMine == item.isMine && it.sender == item.sender
                    } ?: false
                    MessageRow(
                        message = item,
                        index = index,
                        isGroupedAbove = isGroupedAbove,
                        isGroupedBelow = isGroupedBelow,
                        isHighlighted = highlightedMessageId == item.id,
                        isFavorite = favoriteIds.contains(item.id),
                        onMessageLongPress = onMessageLongPress,
                        onSwipeReply = onSwipeReply,
                        onReEdit = onReEdit,
                        onAttachmentClick = onAttachmentClick
                    )
                }
            }
        }
        if (showTyping) {
            item {
                TypingIndicator()
            }
        }
    }
}

@Composable
private fun DaySeparator(label: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.Center
    ) {
        Text(
            text = label.uppercase(),
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
    }
}

@Composable
private fun MessageRow(
    message: ChatMessage,
    index: Int,
    isGroupedAbove: Boolean,
    isGroupedBelow: Boolean,
    isHighlighted: Boolean,
    isFavorite: Boolean,
    onMessageLongPress: (ChatMessage) -> Unit,
    onSwipeReply: (ChatMessage) -> Unit,
    onReEdit: (ChatMessage) -> Unit,
    onAttachmentClick: (Attachment) -> Unit
) {
    var visible by remember { mutableStateOf(false) }
    LaunchedEffect(message.id) {
        delay(index * 50L)
        visible = true
    }

    AnimatedVisibility(
        visible = visible,
        enter = fadeIn(tween(220)) + slideInVertically(tween(220)) { it / 4 }
    ) {
        SwipeReplyRow(
            enabled = !message.isRevoked,
            onReply = { onSwipeReply(message) }
        ) { swipeModifier ->
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(top = if (isGroupedAbove) 1.dp else 5.dp)
                    .then(swipeModifier),
                horizontalArrangement = if (message.isMine) Arrangement.End else Arrangement.Start,
                verticalAlignment = Alignment.Bottom
            ) {
                if (!message.isMine) {
                    if (!isGroupedBelow) {
                        IdentityAvatar(
                            label = message.sender,
                            seed = message.sender,
                            kind = IdentityAvatarKind.Person,
                            size = 24.dp,
                            presenceState = PresenceState.Online
                        )
                    } else {
                        Spacer(modifier = Modifier.width(22.dp))
                    }
                    Spacer(modifier = Modifier.width(5.dp))
                }
                Column(
                    horizontalAlignment = if (message.isMine) Alignment.End else Alignment.Start
                ) {
                    MessageBubble(
                        message = message,
                        isGroupedAbove = isGroupedAbove,
                        isGroupedBelow = isGroupedBelow,
                        isHighlighted = isHighlighted,
                        isFavorite = isFavorite,
                        onLongPress = { onMessageLongPress(message) },
                        onReEdit = { onReEdit(message) },
                        onAttachmentClick = onAttachmentClick
                    )
                    if (message.reactions.isNotEmpty()) {
                        Spacer(modifier = Modifier.height(5.dp))
                        ReactionRow(
                            reactions = message.reactions,
                            alignEnd = message.isMine
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun SwipeReplyRow(
    enabled: Boolean,
    onReply: () -> Unit,
    content: @Composable (Modifier) -> Unit
) {
    val scope = rememberCoroutineScope()
    val haptics = LocalHapticFeedback.current
    val maxReveal = 72.dp
    val maxRevealPx = with(LocalDensity.current) { maxReveal.toPx() }
    val offset = remember { Animatable(0f) }
    val revealProgress = ((-offset.value) / maxRevealPx).coerceIn(0f, 1f)
    val dragState = rememberDraggableState { delta ->
        if (!enabled) {
            return@rememberDraggableState
        }
        val newOffset = (offset.value + delta).coerceIn(-maxRevealPx, 0f)
        scope.launch { offset.snapTo(newOffset) }
    }

    Box(modifier = Modifier.fillMaxWidth()) {
        if (enabled && revealProgress > 0.02f) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(end = 8.dp),
                horizontalArrangement = Arrangement.End,
                verticalAlignment = Alignment.CenterVertically
            ) {
                ReplySwipeAction(revealProgress = revealProgress)
            }
        }
        val dragModifier = if (enabled) {
            Modifier
                .offset { IntOffset(offset.value.roundToInt(), 0) }
                .draggable(
                    state = dragState,
                    orientation = Orientation.Horizontal,
                    onDragStopped = { velocity ->
                        val shouldTrigger = offset.value <= -maxRevealPx * 0.55f || velocity < -900f
                        if (shouldTrigger) {
                            haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                            onReply()
                        }
                        scope.launch { offset.animateTo(0f, tween(160)) }
                    }
                )
        } else {
            Modifier
        }
        content(dragModifier)
    }
}

@Composable
private fun ReplySwipeAction(revealProgress: Float) {
    Box(
        modifier = Modifier
            .size(36.dp)
            .alpha(0.5f + revealProgress * 0.4f)
            .clip(CircleShape)
            .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.09f + revealProgress * 0.11f)),
        contentAlignment = Alignment.Center
    ) {
        Icon(
            imageVector = MiOwnedIcons.Reply,
            contentDescription = tr("chat_quick_reply", "Quick reply"),
            tint = MaterialTheme.colorScheme.primary,
            modifier = Modifier.size(18.dp)
        )
    }
}

@Composable
@OptIn(ExperimentalFoundationApi::class)
private fun MessageBubble(
    message: ChatMessage,
    isGroupedAbove: Boolean,
    isGroupedBelow: Boolean,
    isHighlighted: Boolean,
    isFavorite: Boolean,
    onLongPress: () -> Unit,
    onReEdit: () -> Unit,
    onAttachmentClick: (Attachment) -> Unit
) {
    val haptics = LocalHapticFeedback.current
    val colors = phaseOneColors()
    val highlightAlpha by animateFloatAsState(
        targetValue = if (isHighlighted) 1f else 0f,
        animationSpec = tween(220),
        label = "highlightAlpha"
    )
    val bubbleShape = if (message.isMine) {
        RoundedCornerShape(
            topStart = 18.dp,
            topEnd = if (isGroupedAbove) 8.dp else 18.dp,
            bottomEnd = if (isGroupedBelow) 8.dp else 18.dp,
            bottomStart = 18.dp
        )
    } else {
        RoundedCornerShape(
            topStart = if (isGroupedAbove) 8.dp else 18.dp,
            topEnd = 18.dp,
            bottomEnd = 18.dp,
            bottomStart = if (isGroupedBelow) 8.dp else 18.dp
        )
    }

    val baseBubbleColor = if (message.isMine) {
        colors.primary
    } else {
        colors.surface
    }
    val bubbleColor = if (message.isRevoked) {
        colors.surfaceVariant
    } else {
        baseBubbleColor
    }
    val highlightColor = if (message.isMine) {
        colors.primary
    } else {
        colors.accent
    }
    val bubbleAccent = if (message.isMine) {
        Color.White.copy(alpha = 0.16f)
    } else {
        colors.primary.copy(alpha = 0.08f)
    }

    Box(modifier = Modifier.widthIn(max = 272.dp)) {
        Column(
            modifier = Modifier
                .clip(bubbleShape)
                .border(
                    width = 1.dp,
                    color = if (message.isMine) {
                        Color.White.copy(alpha = 0.12f + (0.08f * highlightAlpha))
                    } else {
                        colors.cardBorder.copy(alpha = 0.84f + 0.10f * highlightAlpha)
                    },
                    shape = bubbleShape
                )
                .then(
                    if (message.isMine) {
                        Modifier.background(
                            brush = Brush.verticalGradient(
                                colors = listOf(
                                    baseBubbleColor,
                                    colors.accent.copy(alpha = 0.94f)
                                )
                            ),
                            shape = bubbleShape
                        )
                    } else {
                        Modifier.background(bubbleColor, bubbleShape)
                    }
                )
                .combinedClickable(
                    onClick = {},
                    onLongClick = {
                        haptics.performHapticFeedback(HapticFeedbackType.LongPress)
                        onLongPress()
                    }
                )
                .padding(horizontal = 13.dp, vertical = 12.dp)
        ) {
            if (message.isRevoked) {
                RevokedMessageRow(
                    isMine = message.isMine,
                    canReEdit = message.isMine && !message.recalledText.isNullOrBlank(),
                    onReEdit = onReEdit
                )
            } else {
                if (message.forwardedFrom != null) {
                    ForwardedRow(message.forwardedFrom, message.isMine)
                    Spacer(modifier = Modifier.height(5.dp))
                }
                if (message.replyTo != null) {
                    ReplyPreviewRow(message.replyTo, message.isMine)
                    Spacer(modifier = Modifier.height(5.dp))
                }
                if (message.body.isNotBlank()) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(14.dp))
                            .background(bubbleAccent)
                    ) {
                        Text(
                            text = message.body,
                            modifier = Modifier.padding(horizontal = 8.dp, vertical = 7.dp),
                            style = MaterialTheme.typography.bodyLarge,
                            color = if (message.isMine) Color.White else colors.onSurface
                        )
                    }
                }
                if (message.linkPreview != null) {
                    Spacer(modifier = Modifier.height(10.dp))
                    LinkPreviewCard(message.linkPreview, message.isMine)
                }
                if (message.attachment != null) {
                    Spacer(modifier = Modifier.height(10.dp))
                    AttachmentBlock(message.attachment, message.isMine, onClick = onAttachmentClick)
                }
            }
            Spacer(modifier = Modifier.height(5.dp))
            MessageMetaRow(
                time = message.time,
                status = message.status,
                isMine = message.isMine,
                isEdited = message.isEdited,
                isFavorite = isFavorite,
                readBy = message.readBy
            )
        }
    }
}

@Composable
private fun ReplyPreviewRow(reply: ReplyPreview, isMine: Boolean) {
    val colors = phaseOneColors()
    val accent = if (isMine) Color.White.copy(alpha = 0.85f) else colors.primary
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .background(accent.copy(alpha = if (isMine) 0.18f else 0.10f))
            .padding(horizontal = 10.dp, vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .width(3.dp)
                .height(20.dp)
                .background(accent, RoundedCornerShape(2.dp))
        )
        Spacer(modifier = Modifier.width(8.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = reply.sender,
                style = MaterialTheme.typography.labelSmall,
                color = accent
            )
            Text(
                text = reply.snippet,
                style = MaterialTheme.typography.bodySmall,
                color = if (isMine) Color.White else colors.onSurface
            )
        }
    }
}

@Composable
private fun ReplyComposerRow(reply: ReplyPreview, onDismiss: () -> Unit) {
    val colors = phaseOneColors()
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(40.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(colors.glass)
            .border(1.dp, colors.glassBorder, RoundedCornerShape(16.dp))
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .width(3.dp)
                .height(18.dp)
                .background(colors.primary, RoundedCornerShape(2.dp))
        )
        Spacer(modifier = Modifier.width(8.dp))
        Text(
            text = tr("chat_replying_to", "Replying to %s").format(reply.sender),
            style = MaterialTheme.typography.labelSmall,
            color = colors.primary
        )
        Spacer(modifier = Modifier.width(8.dp))
        Text(
            text = reply.snippet,
            modifier = Modifier.weight(1f),
            style = MaterialTheme.typography.bodySmall,
            color = colors.onSurfaceMuted,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
        IconButton(onClick = onDismiss, modifier = Modifier.size(28.dp)) {
            Icon(
                imageVector = MiOwnedIcons.Close,
                contentDescription = tr("chat_dismiss", "Dismiss"),
                tint = colors.onSurfaceMuted
            )
        }
    }
}

@Composable
private fun ForwardedRow(label: String, isMine: Boolean) {
    val colors = phaseOneColors()
    val accent = if (isMine) Color.White.copy(alpha = 0.85f) else colors.primary
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .width(3.dp)
                .height(20.dp)
                .background(accent, RoundedCornerShape(2.dp))
        )
        Spacer(modifier = Modifier.width(8.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = tr("chat_forwarded", "Forwarded"),
                style = MaterialTheme.typography.labelSmall,
                color = accent
            )
            Text(
                text = label,
                style = MaterialTheme.typography.labelSmall,
                color = accent.copy(alpha = 0.85f)
            )
        }
    }
}

@Composable
private fun LinkPreviewCard(preview: LinkPreview, isMine: Boolean) {
    val colors = phaseOneColors()
    val surface = if (isMine) Color.White.copy(alpha = 0.15f) else colors.surfaceVariant
    val titleColor = if (isMine) Color.White else colors.onSurface
    val metaColor = if (isMine) Color.White.copy(alpha = 0.72f) else colors.onSurfaceMuted
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp))
            .background(surface)
            .border(1.dp, if (isMine) Color.White.copy(alpha = 0.12f) else colors.cardBorder, RoundedCornerShape(16.dp))
            .padding(12.dp)
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .height(112.dp)
                .clip(RoundedCornerShape(14.dp))
                .background(if (isMine) Color.White.copy(alpha = 0.10f) else colors.surface),
                contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = MiOwnedIcons.Link,
                contentDescription = tr("chat_link_preview", "Link preview"),
                tint = if (isMine) Color.White.copy(alpha = 0.82f) else colors.onSurfaceMuted
            )
        }
        Spacer(modifier = Modifier.height(10.dp))
        MediaHintChip(
            kind = MediaHintKind.Link,
            label = preview.domain
        )
        Spacer(modifier = Modifier.height(8.dp))
        Text(
            text = preview.title,
            style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.SemiBold),
            color = titleColor
        )
        Text(
            text = preview.domain,
            style = MaterialTheme.typography.labelSmall,
            color = metaColor
        )
        Text(
            text = preview.snippet,
            style = MaterialTheme.typography.bodyMedium,
            color = metaColor
        )
    }
}

@Composable
fun AttachmentBlock(
    attachment: Attachment,
    isMine: Boolean,
    onClick: ((Attachment) -> Unit)? = null
) {
    val colors = phaseOneColors()
    val textColor = if (isMine) Color.White else colors.onSurface
    when (attachment.kind) {
        AttachmentKind.File -> {
            val surfaceColor = if (isMine) {
                Color.White.copy(alpha = 0.15f)
            } else {
                colors.surfaceVariant
            }
            val clickModifier = if (onClick != null) {
                Modifier.clickable { onClick(attachment) }
            } else {
                Modifier
            }
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(16.dp))
                    .background(surfaceColor)
                    .border(1.dp, if (isMine) Color.White.copy(alpha = 0.12f) else colors.cardBorder, RoundedCornerShape(16.dp))
                    .then(clickModifier)
                    .padding(horizontal = 12.dp, vertical = 12.dp)
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Box(
                        modifier = Modifier
                            .size(38.dp)
                            .clip(RoundedCornerShape(12.dp))
                            .background(if (isMine) Color.White.copy(alpha = 0.15f) else colors.surface)
                            .border(1.dp, if (isMine) Color.White.copy(alpha = 0.10f) else colors.cardBorder, RoundedCornerShape(12.dp)),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = MiOwnedIcons.File,
                            contentDescription = tr("chat_attachment_file", "File"),
                            tint = if (isMine) Color.White else colors.primary
                        )
                    }
                    Spacer(modifier = Modifier.width(10.dp))
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            text = attachment.label,
                            style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Medium),
                            color = textColor
                        )
                        Text(
                            text = attachment.meta,
                            style = MaterialTheme.typography.labelSmall,
                            color = textColor.copy(alpha = 0.7f)
                        )
                    }
                    Spacer(modifier = Modifier.width(8.dp))
                    MediaHintChip(
                        kind = MediaHintKind.File,
                        label = tr("chat_attachment_file", "File")
                    )
                }
                if (attachment.state == TransferState.Downloading && attachment.progress != null) {
                    Spacer(modifier = Modifier.height(8.dp))
                    val progress = attachment.progress
                    LinearProgressIndicator(
                        progress = { progress },
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(4.dp)
                            .clip(RoundedCornerShape(2.dp)),
                        color = if (isMine) Color.White else colors.primary,
                        trackColor = if (isMine) {
                            Color.White.copy(alpha = 0.3f)
                        } else {
                            colors.surface
                        }
                    )
                }
                if (attachment.state == TransferState.Failed) {
                    Spacer(modifier = Modifier.height(8.dp))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = MiOwnedIcons.Alert,
                            contentDescription = tr("chat_status_failed", "Failed"),
                            tint = colors.danger,
                            modifier = Modifier.size(14.dp)
                        )
                        Spacer(modifier = Modifier.width(6.dp))
                        Text(
                            text = tr("chat_transfer_failed", "Transfer failed"),
                            style = MaterialTheme.typography.labelSmall,
                            color = colors.danger
                        )
                    }
                }
            }
        }
        AttachmentKind.Voice -> {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .background(
                        if (isMine) Color.White.copy(alpha = 0.18f)
                        else MaterialTheme.colorScheme.surface
                    )
                    .padding(horizontal = 10.dp, vertical = 8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                MediaHintChip(
                    kind = MediaHintKind.Voice,
                    label = tr("chat_attachment_voice", "Voice")
                )
                Spacer(modifier = Modifier.width(8.dp))
                Icon(
                    imageVector = MiOwnedIcons.Play,
                    contentDescription = tr("chat_attachment_play", "Play"),
                    tint = if (isMine) Color.White else MaterialTheme.colorScheme.primary
                )
                Spacer(modifier = Modifier.width(8.dp))
                VoiceWaveform(
                    modifier = Modifier.weight(1f),
                    tint = textColor.copy(alpha = 0.8f)
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = attachment.meta,
                    style = MaterialTheme.typography.labelSmall,
                    color = textColor
                )
            }
        }
        AttachmentKind.Photo -> {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .background(
                        if (isMine) Color.White.copy(alpha = 0.18f)
                        else MaterialTheme.colorScheme.surface
                    )
                    .padding(8.dp)
            ) {
                MediaHintChip(
                    kind = MediaHintKind.Photo,
                    label = tr("chat_attachment_photo", "Photo")
                )
                Spacer(modifier = Modifier.height(8.dp))
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(140.dp)
                        .clip(RoundedCornerShape(10.dp))
                        .background(MaterialTheme.colorScheme.surfaceVariant)
                ) {
                    Icon(
                        imageVector = MiOwnedIcons.Photo,
                        contentDescription = tr("chat_attachment_photo", "Photo"),
                        tint = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.align(Alignment.Center)
                    )
                }
                Spacer(modifier = Modifier.height(6.dp))
                Text(
                    text = attachment.label,
                    style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Medium),
                    color = textColor
                )
                Text(
                    text = attachment.meta,
                    style = MaterialTheme.typography.labelSmall,
                    color = textColor.copy(alpha = 0.7f)
                )
            }
        }
        AttachmentKind.Location -> {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .background(
                        if (isMine) Color.White.copy(alpha = 0.18f)
                        else MaterialTheme.colorScheme.surface
                    )
                    .padding(8.dp)
            ) {
                UiStatusCountBadge(
                    label = tr("chat_attachment_location", "Location"),
                    tone = UiBadgeTone.Neutral
                )
                Spacer(modifier = Modifier.height(8.dp))
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(120.dp)
                        .clip(RoundedCornerShape(10.dp))
                        .background(MaterialTheme.colorScheme.surfaceVariant),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = MiOwnedIcons.Location,
                        contentDescription = tr("chat_attachment_location", "Location"),
                        tint = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                Spacer(modifier = Modifier.height(6.dp))
                Text(
                    text = attachment.label,
                    style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Medium),
                    color = textColor
                )
                Text(
                    text = attachment.meta,
                    style = MaterialTheme.typography.labelSmall,
                    color = textColor.copy(alpha = 0.7f)
                )
            }
        }
        AttachmentKind.Sticker -> {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(12.dp))
                    .background(
                        if (isMine) Color.White.copy(alpha = 0.18f)
                        else MaterialTheme.colorScheme.surface
                    )
                    .padding(12.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Icon(
                    imageVector = MiOwnedIcons.Emoji,
                    contentDescription = tr("chat_attachment_sticker", "Sticker"),
                    tint = if (isMine) Color.White else MaterialTheme.colorScheme.primary,
                    modifier = Modifier.size(36.dp)
                )
                Spacer(modifier = Modifier.height(6.dp))
                Text(
                    text = attachment.label,
                    style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Medium),
                    color = textColor
                )
            }
        }
    }
}

@Composable
private fun MessageMetaRow(
    time: String,
    status: MessageStatus,
    isMine: Boolean,
    isEdited: Boolean,
    isFavorite: Boolean,
    readBy: List<String>
) {
    val colors = phaseOneColors()
    val textColor = if (isMine) {
        Color.White.copy(alpha = 0.8f)
    } else {
        colors.onSurfaceMuted
    }
    val statusLabel = when {
        !isMine -> null
        status == MessageStatus.Failed -> tr("chat_status_failed", "Failed")
        status == MessageStatus.Sending -> tr("chat_status_sending", "Sending")
        else -> null
    }
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.End,
        verticalAlignment = Alignment.CenterVertically
    ) {
        if (isFavorite) {
            Icon(
                imageVector = MiOwnedIcons.Star,
                contentDescription = tr("chat_favorite", "Favorite"),
                tint = textColor,
                modifier = Modifier.size(12.dp)
            )
            Spacer(modifier = Modifier.width(6.dp))
        }
        if (isEdited) {
            Text(
                text = tr("chat_edited", "Edited"),
                style = MaterialTheme.typography.labelSmall,
                color = textColor
            )
            Spacer(modifier = Modifier.width(6.dp))
        }
        if (statusLabel != null) {
            Text(
                text = statusLabel,
                style = MaterialTheme.typography.labelSmall,
                color = if (status == MessageStatus.Failed) colors.danger else textColor
            )
            Spacer(modifier = Modifier.width(6.dp))
        }
        Text(
            text = time,
            style = MaterialTheme.typography.labelSmall,
            color = textColor
        )
        if (isMine) {
            Spacer(modifier = Modifier.width(6.dp))
            StatusIcon(status)
            if (status == MessageStatus.Read && readBy.isNotEmpty()) {
                Spacer(modifier = Modifier.width(6.dp))
                ReadReceipts(readBy)
            }
        }
    }
}

@Composable
private fun ReadReceipts(initials: List<String>) {
    val shown = initials.take(2)
    Box(
        modifier = Modifier
            .height(16.dp)
            .width((shown.size * 10 + 6).dp)
    ) {
        shown.forEachIndexed { index, label ->
            MiniAvatar(
                initials = label,
                modifier = Modifier
                    .align(Alignment.CenterStart)
                    .padding(start = (index * 10).dp)
            )
        }
    }
}

@Composable
private fun MiniAvatar(initials: String, modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .size(16.dp)
            .clip(CircleShape)
            .background(MaterialTheme.colorScheme.primary),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = initials.take(1),
            color = Color.White,
            fontSize = 8.sp,
            fontWeight = FontWeight.SemiBold
        )
    }
}

@Composable
private fun RevokedMessageRow(
    isMine: Boolean,
    canReEdit: Boolean,
    onReEdit: () -> Unit
) {
    val textColor = MaterialTheme.colorScheme.onSurfaceVariant
    Row(verticalAlignment = Alignment.CenterVertically) {
        Icon(
            imageVector = MiOwnedIcons.Undo,
            contentDescription = tr("chat_recalled", "Recalled"),
            tint = textColor,
            modifier = Modifier.size(14.dp)
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = if (isMine) {
                tr("chat_recalled_me", "You recalled a message")
            } else {
                tr("chat_recalled_other", "The other person recalled a message")
            },
            style = MaterialTheme.typography.bodyMedium,
            color = textColor
        )
        if (canReEdit) {
            Spacer(modifier = Modifier.width(8.dp))
            Text(
                text = tr("chat_reedit", "Re-edit"),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.primary,
                modifier = Modifier.clickable { onReEdit() }
            )
        }
    }
}

@Composable
private fun BubbleTail(
    color: Color,
    isMine: Boolean,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier.size(width = 14.dp, height = 10.dp)) {
        val path = Path()
        if (isMine) {
            path.moveTo(0f, 0f)
            path.lineTo(size.width, size.height / 2f)
            path.lineTo(0f, size.height)
        } else {
            path.moveTo(size.width, 0f)
            path.lineTo(0f, size.height / 2f)
            path.lineTo(size.width, size.height)
        }
        path.close()
        drawPath(path, color)
    }
}

@Composable
private fun StatusIcon(status: MessageStatus) {
    val icon = when (status) {
        MessageStatus.Sending -> MiOwnedIcons.Clock
        MessageStatus.Sent -> MiOwnedIcons.Check
        MessageStatus.Delivered -> MiOwnedIcons.CheckDouble
        MessageStatus.Read -> MiOwnedIcons.CheckDouble
        MessageStatus.Failed -> MiOwnedIcons.Alert
    }
    val tint = when (status) {
        MessageStatus.Read -> MaterialTheme.colorScheme.secondary
        MessageStatus.Failed -> MaterialTheme.colorScheme.error
        else -> Color.White.copy(alpha = 0.85f)
    }
    Icon(
        imageVector = icon,
        contentDescription = status.name,
        tint = tint,
        modifier = Modifier.size(14.dp)
    )
}

@Composable
private fun ReactionRow(reactions: List<MessageReaction>, alignEnd: Boolean) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = if (alignEnd) Arrangement.End else Arrangement.Start
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            reactions.forEach { reaction ->
                ReactionChip(reaction)
            }
        }
    }
}

@Composable
private fun ReactionChip(reaction: MessageReaction) {
    Box(
        modifier = Modifier
            .clip(RoundedCornerShape(12.dp))
            .background(MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.5f))
            .border(
                width = 1.dp,
                color = MaterialTheme.colorScheme.outline.copy(alpha = 0.14f),
                shape = RoundedCornerShape(12.dp)
            )
            .padding(horizontal = 7.dp, vertical = 3.dp)
    ) {
        Text(
            text = "${reaction.label} ${reaction.count}",
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.9f)
        )
    }
}

@Composable
private fun UnreadSeparator(count: Int) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.Center
    ) {
        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(12.dp))
                .background(MaterialTheme.colorScheme.tertiary.copy(alpha = 0.12f))
                .border(
                    width = 1.dp,
                    color = MaterialTheme.colorScheme.tertiary.copy(alpha = 0.2f),
                    shape = RoundedCornerShape(12.dp)
                )
                .padding(horizontal = 10.dp, vertical = 4.dp)
        ) {
            Text(
                text = tr("chat_unread_count", "Unread %d").format(count),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.tertiary
            )
        }
    }
}

@Composable
private fun PinnedMessageRow(message: PinnedMessage, onClick: () -> Unit) {
    val colors = phaseOneColors()
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp))
            .clickable { onClick() },
        color = colors.surface,
        border = BorderStroke(1.dp, colors.cardBorder),
        tonalElevation = 0.dp,
        shadowElevation = 0.dp,
        shape = RoundedCornerShape(16.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(
                modifier = Modifier
                    .size(36.dp)
                    .background(
                        colors.primary.copy(alpha = 0.15f),
                        RoundedCornerShape(12.dp)
                    ),
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = MiOwnedIcons.Pin,
                    contentDescription = tr("chat_pinned", "Pinned"),
                    tint = colors.primary
                )
            }
            Spacer(modifier = Modifier.width(10.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = message.title,
                    style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.SemiBold),
                    color = colors.onSurface
                )
                Text(
                    text = message.snippet,
                    style = MaterialTheme.typography.bodyMedium,
                    color = colors.onSurfaceMuted,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
            Icon(
                imageVector = MiOwnedIcons.ChevronRight,
                contentDescription = tr("chat_open", "Open"),
                tint = colors.onSurfaceMuted
            )
        }
    }
}

@Composable
private fun SystemNoticeRow(text: String) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.Center
    ) {
        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(14.dp))
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
private fun TypingIndicator() {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.Start
    ) {
        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(16.dp))
                .background(MaterialTheme.colorScheme.surfaceVariant)
                .padding(horizontal = 12.dp, vertical = 8.dp)
        ) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Box(
                    modifier = Modifier
                        .size(6.dp)
                        .clip(CircleShape)
                        .background(MaterialTheme.colorScheme.onSurfaceVariant)
                )
                Box(
                    modifier = Modifier
                        .size(6.dp)
                        .clip(CircleShape)
                        .background(MaterialTheme.colorScheme.onSurfaceVariant)
                )
                Box(
                    modifier = Modifier
                        .size(6.dp)
                        .clip(CircleShape)
                        .background(MaterialTheme.colorScheme.onSurfaceVariant)
                )
            }
        }
    }
}

@Composable
private fun JumpToBottomButton(count: Int, modifier: Modifier = Modifier) {
    Surface(
        modifier = modifier.shadow(2.dp, RoundedCornerShape(12.dp)),
        shape = RoundedCornerShape(12.dp),
        color = MaterialTheme.colorScheme.surface.copy(alpha = 0.92f)
    ) {
        Column(
            modifier = Modifier
                .padding(horizontal = 6.dp, vertical = 5.dp)
                .widthIn(min = 30.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Icon(
                imageVector = MiOwnedIcons.ChevronDown,
                contentDescription = tr("chat_jump_to_bottom", "Jump to bottom"),
                tint = MaterialTheme.colorScheme.onSurface.copy(alpha = 0.86f),
                modifier = Modifier.size(18.dp)
            )
            if (count > 0) {
                Spacer(modifier = Modifier.height(4.dp))
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(8.dp))
                        .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.16f))
                        .padding(horizontal = 6.dp, vertical = 1.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = if (count > 9) "9+" else count.toString(),
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.primary,
                        fontSize = 10.sp
                    )
                }
            }
        }
    }
}

@Composable
private fun ChatEmptyState(
    title: String,
    message: String,
    actionLabel: String,
    modifier: Modifier = Modifier
) {
    ChatStatusState(
        icon = MiOwnedIcons.Chat,
        title = title,
        message = message,
        actionLabel = actionLabel,
        modifier = modifier
    )
}

@Composable
private fun ChatStatusState(
    icon: ImageVector,
    title: String,
    message: String,
    actionLabel: String,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(24.dp))
            .background(MaterialTheme.colorScheme.surface.copy(alpha = 0.92f))
            .padding(horizontal = 20.dp, vertical = 24.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        EmptyStateIllustration(
            icon = icon,
            contentDescription = title,
            modifier = Modifier.size(ChatUiTokens.IllustrationFrame),
            tone = UiIconTone.Primary,
            chipLabel = tr("chat_status_chip", "Secure")
        )
        Spacer(modifier = Modifier.height(16.dp))
        Text(
            text = title,
            style = MaterialTheme.typography.titleLarge,
            color = MaterialTheme.colorScheme.onSurface
        )
        Spacer(modifier = Modifier.height(6.dp))
        Text(
            text = message,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            textAlign = TextAlign.Center
        )
        Spacer(modifier = Modifier.height(16.dp))
        PrimaryButton(
            label = actionLabel,
            fillMaxWidth = false,
            modifier = Modifier.widthIn(min = 160.dp)
        )
    }
}

@Composable
private fun NetworkBanner(text: String, modifier: Modifier = Modifier) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .background(MaterialTheme.colorScheme.tertiary.copy(alpha = 0.18f))
            .padding(horizontal = 12.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            imageVector = MiOwnedIcons.Alert,
            contentDescription = text,
            tint = MaterialTheme.colorScheme.tertiary,
            modifier = Modifier.size(16.dp)
        )
        Spacer(modifier = Modifier.width(8.dp))
        Text(
            text = text,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurface
        )
    }
}

@Composable
private fun MessageActionSheet(
    message: ChatMessage,
    isPinned: Boolean,
    isFavorite: Boolean,
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
        MessageAction("reply", tr("chat_action_reply", "Reply"), MiOwnedIcons.Reply),
        MessageAction("forward", tr("chat_action_forward", "Forward"), MiOwnedIcons.Forward),
        MessageAction("copy", tr("chat_action_copy", "Copy"), MiOwnedIcons.Copy),
        MessageAction(
            "pin",
            if (isPinned) tr("chat_action_unpin", "Unpin") else tr("chat_action_pin", "Pin"),
            MiOwnedIcons.Pin
        )
    )
    val secondaryActions = buildList {
        add(
            MessageAction(
                "favorite",
                if (isFavorite) tr("chat_action_unfavorite", "Unfavorite")
                else tr("chat_action_favorite", "Favorite"),
                MiOwnedIcons.Star
            )
        )
        if (canRecall) {
            add(
                MessageAction(
                    "recall",
                    tr("chat_action_recall", "Recall"),
                    MiOwnedIcons.Undo,
                    isDestructive = true
                )
            )
        }
        add(
            MessageAction(
                "delete",
                tr("chat_action_delete", "Delete"),
                MiOwnedIcons.Delete,
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
            ReactionPickerRow()
            Spacer(modifier = Modifier.height(10.dp))
            ActionMessagePreview(message)
            if (message.isMine && recallInitial != null) {
                Spacer(modifier = Modifier.height(8.dp))
                RecallHintRow(
                    remainingSeconds = recallRemaining,
                    isAvailable = canRecall
                )
            }
            Spacer(modifier = Modifier.height(16.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                primaryActions.forEach { action ->
                    ActionIconButton(
                        action = action,
                        modifier = Modifier.weight(1f),
                        onClick = { onAction(action) }
                    )
                }
            }
            if (secondaryActions.isNotEmpty()) {
                Spacer(modifier = Modifier.height(12.dp))
                secondaryActions.forEach { action ->
                    ActionRow(
                        action = action,
                        onClick = { onAction(action) }
                    )
                }
            }
        }
    }
}

@Composable
private fun ReactionPickerRow() {
    val reactions = listOf("👍", "❤️", "😂", "😮", "😢", "🔥")
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        reactions.forEach { reaction ->
            SuggestionChip(
                onClick = {},
                label = { Text(text = reaction, fontSize = 16.sp) },
                shape = RoundedCornerShape(14.dp)
            )
        }
    }
}

@Composable
private fun RecallHintRow(
    remainingSeconds: Int,
    isAvailable: Boolean
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            imageVector = MiOwnedIcons.Undo,
            contentDescription = if (isAvailable) {
                tr("chat_recall_available", "Recall available %s")
                    .format(formatCountdown(remainingSeconds))
            } else {
                tr("chat_recall_expired", "Recall expired")
            },
            tint = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.size(14.dp)
        )
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = if (isAvailable) {
                tr("chat_recall_available", "Recall available %s")
                    .format(formatCountdown(remainingSeconds))
            } else {
                tr("chat_recall_expired", "Recall expired")
            },
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
    }
}

private fun formatCountdown(seconds: Int): String {
    val safe = seconds.coerceAtLeast(0)
    val minutes = safe / 60
    val secs = safe % 60
    return "%02d:%02d".format(minutes, secs)
}

@Composable
private fun ActionMessagePreview(message: ChatMessage) {
    val previewText = when {
        message.isRevoked -> "Message removed"
        message.body.isNotBlank() -> message.body
        message.attachment != null -> message.attachment.label
        else -> "Attachment"
    }
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp))
            .background(MaterialTheme.colorScheme.surfaceVariant)
            .padding(horizontal = 12.dp, vertical = 10.dp)
    ) {
        Text(
            text = "${message.sender} · ${message.time}",
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
private fun ActionIconButton(
    action: MessageAction,
    modifier: Modifier = Modifier,
    onClick: () -> Unit
) {
    FilledTonalButton(
        onClick = onClick,
        modifier = modifier
            .heightIn(min = 44.dp),
        shape = RoundedCornerShape(14.dp),
        colors = ButtonDefaults.filledTonalButtonColors(
            containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.72f),
            contentColor = if (action.isDestructive) {
                MaterialTheme.colorScheme.error
            } else {
                MaterialTheme.colorScheme.onSurface
            }
        )
    ) {
        Icon(
            imageVector = action.icon,
            contentDescription = action.label,
            modifier = Modifier.size(18.dp)
        )
        Spacer(modifier = Modifier.width(8.dp))
        Text(
            text = action.label,
            style = MaterialTheme.typography.labelLarge,
            maxLines = 1
        )
    }
}

@Composable
private fun ActionRow(action: MessageAction, onClick: () -> Unit) {
    val tint = if (action.isDestructive) MaterialTheme.colorScheme.error
    else MaterialTheme.colorScheme.onSurface
    ListItem(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .clickable { onClick() },
        colors = ListItemDefaults.colors(containerColor = Color.Transparent),
        leadingContent = {
            Icon(
                imageVector = action.icon,
                contentDescription = action.label,
                tint = tint
            )
        },
        headlineContent = {
            Text(
                text = action.label,
                style = MaterialTheme.typography.bodyLarge,
                color = tint
            )
        }
    )
}

@Composable
private fun ToastPill(text: String, modifier: Modifier = Modifier) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surface,
        shadowElevation = 2.dp
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
private fun UndoPill(
    text: String,
    actionLabel: String,
    onAction: () -> Unit,
    modifier: Modifier = Modifier
) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surface,
        shadowElevation = 2.dp
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

private fun messageCopyText(message: ChatMessage): String {
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
    message.linkPreview?.let { preview ->
        return "${preview.title} (${preview.domain})"
    }
    return "Message"
}

internal fun recalledMessageCopy(message: ChatMessage): ChatMessage {
    return message.copy(
        body = "",
        isEdited = false,
        isRevoked = true,
        recalledText = message.body.takeIf { it.isNotBlank() },
        recallSecondsLeft = null,
        status = MessageStatus.Sent,
        forwardedFrom = null,
        replyTo = null,
        attachment = null,
        linkPreview = null,
        reactions = emptyList()
    )
}

private fun replySnippet(message: ChatMessage): String {
    val base = messageCopyText(message)
    return if (base.length > 64) base.take(64) + "..." else base
}

private fun pinnedSnippet(message: ChatMessage): String {
    val base = messageCopyText(message)
    return if (base.length > 90) base.take(90) + "..." else base
}

private fun buildPinnedMessage(message: ChatMessage): PinnedMessage {
    return PinnedMessage(
        id = "pin_${message.id}",
        title = "Pinned message",
        snippet = pinnedSnippet(message),
        messageId = message.id
    )
}

@Composable
private fun VoiceWaveform(modifier: Modifier = Modifier, tint: Color) {
    val bars = listOf(4, 10, 6, 12, 8, 14, 6, 10)
    Row(
        modifier = modifier,
        horizontalArrangement = Arrangement.spacedBy(3.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        bars.forEach { height ->
            Box(
                modifier = Modifier
                    .width(3.dp)
                    .height(height.dp)
                    .background(tint, RoundedCornerShape(2.dp))
            )
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
internal fun ComposerBar(
    modifier: Modifier = Modifier,
    message: String,
    onMessageChange: (String) -> Unit,
    onSend: () -> Unit = {},
    surfaceState: ComposerSurfaceState = ComposerSurfaceState.Collapsed,
    focusRequester: FocusRequester,
    onTextFieldFocusChange: (Boolean) -> Unit = {},
    onToggleQuickActions: () -> Unit,
    onEmoji: () -> Unit = {},
    onVoice: () -> Unit = {}
) {
    val colors = phaseOneColors()
    val showQuickActions = surfaceState == ComposerSurfaceState.QuickActions
    val showVoice = surfaceState == ComposerSurfaceState.Voice
    Surface(
        modifier = modifier
            .fillMaxWidth()
            .navigationBarsPadding()
            .imePadding()
            .padding(horizontal = 8.dp, vertical = 6.dp)
            .height(56.dp),
        shape = RoundedCornerShape(24.dp),
        color = colors.glass,
        border = androidx.compose.foundation.BorderStroke(1.dp, colors.glassBorder),
        tonalElevation = 0.dp
    ) {
        Row(
            modifier = Modifier
                .fillMaxSize()
                .padding(horizontal = 10.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            FilledTonalIconButton(
                onClick = onToggleQuickActions,
                modifier = Modifier.size(36.dp),
                colors = IconButtonDefaults.filledTonalIconButtonColors(
                    containerColor = if (showQuickActions) {
                        colors.primary.copy(alpha = 0.12f)
                    } else {
                        colors.surface
                    },
                    contentColor = if (showQuickActions) {
                        colors.primary
                    } else {
                        colors.onSurfaceMuted
                    }
                )
            ) {
                Icon(
                    imageVector = MiOwnedIcons.Attach,
                    contentDescription = tr("chat_attach", "Attach")
                )
            }
            Spacer(modifier = Modifier.width(6.dp))
            CompactMessageField(
                value = message,
                onValueChange = onMessageChange,
                placeholder = tr("chat_placeholder", "Write a message..."),
                modifier = Modifier.weight(1f),
                focusRequester = focusRequester,
                onFocusChange = onTextFieldFocusChange,
                onEmoji = onEmoji,
                keyboardOptions = KeyboardOptions(
                    autoCorrectEnabled = false,
                    keyboardType = KeyboardType.Text,
                    imeAction = ImeAction.Send
                ),
                keyboardActions = KeyboardActions(
                    onSend = {
                        if (message.isNotBlank()) {
                            onSend()
                        }
                    }
                )
            )
            Spacer(modifier = Modifier.width(6.dp))
            if (message.isNotBlank()) {
                FilledIconButton(
                    onClick = onSend,
                    modifier = Modifier.size(38.dp),
                    colors = IconButtonDefaults.filledIconButtonColors(
                        containerColor = colors.primary,
                        contentColor = Color.White
                    )
                ) {
                    Icon(
                        imageVector = MiOwnedIcons.Send,
                        contentDescription = tr("chat_send", "Send")
                    )
                }
            } else {
                FilledTonalIconButton(
                    onClick = onVoice,
                    modifier = Modifier.size(38.dp),
                    colors = IconButtonDefaults.filledTonalIconButtonColors(
                        containerColor = if (showVoice) {
                            colors.primary.copy(alpha = 0.12f)
                        } else {
                            colors.surface
                        },
                        contentColor = if (showVoice) {
                            colors.primary
                        } else {
                            colors.onSurfaceMuted
                        }
                    )
                ) {
                    Icon(
                        imageVector = MiOwnedIcons.Mic,
                        contentDescription = tr("chat_voice", "Voice input")
                    )
                }
            }
        }
    }
}

@Composable
private fun ComposerAssistOverlay(
    replyPreview: ReplyPreview?,
    showQuickActions: Boolean,
    onReplyDismiss: () -> Unit,
    onAttachPhoto: () -> Unit,
    onAttachFile: () -> Unit,
    onAttachLocation: () -> Unit,
    onAttachContact: () -> Unit,
    onAttachSticker: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        if (showQuickActions) {
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                QuickActionButton(
                    icon = MiOwnedIcons.Photo,
                    label = tr("chat_quick_photo", "Photo"),
                    onClick = onAttachPhoto
                )
                QuickActionButton(
                    icon = MiOwnedIcons.File,
                    label = tr("chat_quick_file", "File"),
                    onClick = onAttachFile
                )
                QuickActionButton(
                    icon = MiOwnedIcons.Location,
                    label = tr("chat_quick_location", "Location"),
                    onClick = onAttachLocation
                )
                QuickActionButton(
                    icon = MiOwnedIcons.PersonAdd,
                    label = tr("chat_quick_contact", "Contact"),
                    onClick = onAttachContact
                )
                QuickActionButton(
                    icon = MiOwnedIcons.Emoji,
                    label = tr("chat_quick_sticker", "Sticker"),
                    onClick = onAttachSticker
                )
            }
        }
        if (replyPreview != null) {
            ReplyComposerRow(
                reply = replyPreview,
                onDismiss = onReplyDismiss
            )
        }
    }
}

@Composable
private fun CompactMessageField(
    value: String,
    onValueChange: (String) -> Unit,
    placeholder: String,
    modifier: Modifier = Modifier,
    focusRequester: FocusRequester,
    onFocusChange: (Boolean) -> Unit,
    onEmoji: () -> Unit,
    keyboardOptions: KeyboardOptions,
    keyboardActions: KeyboardActions
) {
    val colors = phaseOneColors()
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        modifier = modifier
            .height(40.dp)
            .focusRequester(focusRequester)
            .onFocusChanged { onFocusChange(it.isFocused) },
        placeholder = {
            Text(
                text = placeholder,
                style = MaterialTheme.typography.bodyMedium
            )
        },
        textStyle = MaterialTheme.typography.bodyMedium,
        singleLine = true,
        minLines = 1,
        maxLines = 1,
        trailingIcon = {
            IconButton(onClick = onEmoji, modifier = Modifier.size(26.dp)) {
                Icon(
                    imageVector = MiOwnedIcons.Emoji,
                    contentDescription = tr("chat_emoji", "Emoji")
                )
            }
        },
        shape = RoundedCornerShape(20.dp),
        keyboardOptions = keyboardOptions,
        keyboardActions = keyboardActions,
        colors = OutlinedTextFieldDefaults.colors(
            focusedContainerColor = colors.surface.copy(alpha = 0.92f),
            unfocusedContainerColor = colors.surface.copy(alpha = 0.88f),
            focusedBorderColor = colors.primary.copy(alpha = 0.20f),
            unfocusedBorderColor = colors.cardBorder
        )
    )
}

@Composable
private fun ComposerSurfacePanel(
    surfaceState: ComposerSurfaceState,
    onDismiss: () -> Unit,
    onAppendEmoji: (String) -> Unit,
    modifier: Modifier = Modifier
) {
    val colors = phaseOneColors()
    Surface(
        modifier = modifier.fillMaxWidth(),
        shape = RoundedCornerShape(22.dp),
        color = colors.glass,
        border = androidx.compose.foundation.BorderStroke(1.dp, colors.glassBorder),
        tonalElevation = 1.dp
    ) {
        when (surfaceState) {
            ComposerSurfaceState.Emoji -> Column(
                modifier = Modifier.padding(horizontal = 12.dp, vertical = 10.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Text(
                    text = tr("chat_emoji_panel_title", "Emoji"),
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    listOf("👍", "🔥", "✅", "🎯", "👀", "🙌", "🚀", "🛡️").forEach { emoji ->
                        AssistChip(
                            onClick = { onAppendEmoji(emoji) },
                            label = { Text(emoji, fontSize = 18.sp) }
                        )
                    }
                }
            }

            ComposerSurfaceState.Voice -> Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp, vertical = 10.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                Icon(
                    imageVector = MiOwnedIcons.Mic,
                    contentDescription = tr("chat_voice", "Voice input"),
                    tint = MaterialTheme.colorScheme.primary
                )
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = tr("chat_voice_hold_title", "Voice note ready"),
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.onSurface
                    )
                    Text(
                        text = tr("chat_voice_hold_hint", "Recording is not available in this preview. Use text or attachments."),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                TextButton(onClick = onDismiss) {
                    Text(tr("chat_voice_close", "Close"))
                }
            }

            else -> Unit
        }
    }
}

@Composable
private fun QuickActionButton(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    modifier: Modifier = Modifier,
    onClick: () -> Unit = {}
) {
    val colors = phaseOneColors()
    AssistChip(
        onClick = onClick,
        modifier = modifier.height(30.dp),
        shape = RoundedCornerShape(14.dp),
        leadingIcon = {
            Icon(
                imageVector = icon,
                contentDescription = label,
                modifier = Modifier.size(16.dp)
            )
        },
        label = {
            Text(
                text = label,
                style = MaterialTheme.typography.labelMedium
            )
        },
        colors = AssistChipDefaults.assistChipColors(
            containerColor = colors.glass,
            labelColor = colors.onSurface,
            leadingIconContentColor = colors.primary
        )
    )
}

@Composable
private fun ChatBackground() {
    val colors = phaseOneColors()
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(
                Brush.verticalGradient(
                    colors = listOf(
                        colors.background,
                        colors.surfaceVariant.copy(alpha = 0.92f),
                        colors.background
                    )
                )
            )
    )
}

@Preview(showBackground = true, widthDp = 412, heightDp = 915)
@Composable
private fun ChatScreenPreview() {
    ChatApp()
}

private const val CHAT_PREFS_NAME = "mi_chat_prefs"

private fun keyMessageDeleted(conversationId: String) = "msg_deleted_$conversationId"
private fun keyMessageRecalled(conversationId: String) = "msg_recalled_$conversationId"
private fun keyMessagePinned(conversationId: String) = "msg_pinned_$conversationId"
private fun keyMessageFavorite(conversationId: String) = "msg_favorite_$conversationId"
private fun keyLastFilePath(conversationId: String) = "msg_last_file_path_$conversationId"

private fun loadDeletedIds(
    prefs: SharedPreferences,
    conversationId: String
): Set<String> {
    return prefs.getStringSet(keyMessageDeleted(conversationId), emptySet())
        ?.toSet()
        ?: emptySet()
}

private fun persistDeletedIds(
    prefs: SharedPreferences,
    conversationId: String,
    ids: Set<String>
) {
    prefs.edit()
        .putStringSet(keyMessageDeleted(conversationId), HashSet(ids))
        .apply()
}

private fun loadRecalledIds(
    prefs: SharedPreferences,
    conversationId: String
): Set<String> {
    return prefs.getStringSet(keyMessageRecalled(conversationId), emptySet())
        ?.toSet()
        ?: emptySet()
}

private fun persistRecalledIds(
    prefs: SharedPreferences,
    conversationId: String,
    ids: Set<String>
) {
    prefs.edit()
        .putStringSet(keyMessageRecalled(conversationId), HashSet(ids))
        .apply()
}

private fun loadPinnedId(
    prefs: SharedPreferences,
    conversationId: String
): String? {
    return prefs.getString(keyMessagePinned(conversationId), null)
}

private fun persistPinnedId(
    prefs: SharedPreferences,
    conversationId: String,
    messageId: String?
) {
    prefs.edit()
        .putString(keyMessagePinned(conversationId), messageId ?: "")
        .apply()
}

private fun loadFavoriteIds(
    prefs: SharedPreferences,
    conversationId: String
): Set<String> {
    return prefs.getStringSet(keyMessageFavorite(conversationId), emptySet())
        ?.toSet()
        ?: emptySet()
}

private fun persistFavoriteIds(
    prefs: SharedPreferences,
    conversationId: String,
    ids: Set<String>
) {
    prefs.edit()
        .putStringSet(keyMessageFavorite(conversationId), HashSet(ids))
        .apply()
}

private fun loadLastFilePath(
    prefs: SharedPreferences,
    conversationId: String
): String? {
    return prefs.getString(keyLastFilePath(conversationId), null)
}

private fun persistLastFilePath(
    prefs: SharedPreferences,
    conversationId: String,
    path: String
) {
    prefs.edit()
        .putString(keyLastFilePath(conversationId), path)
        .apply()
}
