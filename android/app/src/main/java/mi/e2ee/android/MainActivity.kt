package mi.e2ee.android

import android.content.Context
import android.os.Build
import android.os.Bundle
import android.view.View
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import mi.e2ee.android.ui.Attachment
import mi.e2ee.android.ui.AttachmentKind
import mi.e2ee.android.ui.ChatItem
import mi.e2ee.android.ui.ChatMessage
import mi.e2ee.android.ui.ChatTheme
import mi.e2ee.android.ui.ConversationPreview
import mi.e2ee.android.ui.DayMarker
import mi.e2ee.android.ui.EndpointThreatDetector
import mi.e2ee.android.ui.GroupCallRoomUi
import mi.e2ee.android.ui.GroupChatItem
import mi.e2ee.android.ui.IncomingCall
import mi.e2ee.android.ui.MessageStatus
import mi.e2ee.android.ui.PeerCallState
import mi.e2ee.android.ui.ProvideLocalization
import mi.e2ee.android.ui.SdkBridge
import mi.e2ee.android.ui.SampleGroupChat
import mi.e2ee.android.ui.ThemeMode
import mi.e2ee.android.ui.UiHost
import mi.e2ee.android.ui.UiHostPreviewState
import mi.e2ee.android.ui.resolveScreenshotBootstrapState

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val screenshotMode = intent?.getStringExtra(EXTRA_SCREENSHOT_MODE)
        hardenWindowForPrivateUi(screenshotMode == null)
        val endpointThreatReport = EndpointThreatDetector.evaluate(this)
        if (endpointThreatReport.blocked) {
            finishAndRemoveTask()
            return
        }
        setContent {
            val context = LocalContext.current
            var themeMode by rememberSaveable { mutableStateOf(loadThemeMode(context)) }
            LaunchedEffect(themeMode) {
                saveThemeMode(context, themeMode)
            }
            ProvideLocalization {
                ChatTheme(mode = themeMode) {
                    if (screenshotMode != null) {
                        MainScreenshotScene(
                            mode = screenshotMode,
                            context = context,
                            themeMode = themeMode
                        )
                    } else {
                        val sdk = remember(context, endpointThreatReport) {
                            SdkBridge(context, endpointThreatReport)
                        }
                        LaunchedEffect(Unit) {
                            sdk.init()
                        }
                        DisposableEffect(Unit) {
                            onDispose { sdk.dispose() }
                        }
                        UiHost(
                            sdk = sdk,
                            themeMode = themeMode,
                            onThemeModeChange = { themeMode = it }
                        )
                    }
                }
            }
        }
    }

    private fun loadThemeMode(context: Context): Int {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val saved = prefs.getInt(KEY_THEME_MODE, ThemeMode.FollowSystem)
        return when (saved) {
            ThemeMode.FollowSystem,
            ThemeMode.ForceDark,
            ThemeMode.ForceLight -> saved
            else -> ThemeMode.FollowSystem
        }
    }

    private fun saveThemeMode(context: Context, mode: Int) {
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            .edit()
            .putInt(KEY_THEME_MODE, mode)
            .apply()
    }

    private fun hardenWindowForPrivateUi(blockScreenCapture: Boolean) {
        if (blockScreenCapture) {
            window.setFlags(
                WindowManager.LayoutParams.FLAG_SECURE,
                WindowManager.LayoutParams.FLAG_SECURE
            )
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            window.decorView.importantForAutofill =
                View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.decorView.importantForContentCapture =
                View.IMPORTANT_FOR_CONTENT_CAPTURE_NO_EXCLUDE_DESCENDANTS
        }
    }

    companion object {
        const val EXTRA_SCREENSHOT_MODE = "mi.e2ee.android.extra.SCREENSHOT_MODE"

        private const val PREFS_NAME = "mi_chat_prefs"
        private const val KEY_THEME_MODE = "theme_mode"
    }
}

@Composable
private fun MainScreenshotScene(
    mode: String,
    context: Context,
    themeMode: Int
) {
    val bootstrapState = remember(mode) { resolveScreenshotBootstrapState(mode) }
    val previewSdk = remember(context) { SdkBridge(context) }
    val previewState = remember(bootstrapState.sceneId) {
        previewUiHostStateForScene(bootstrapState.sceneId)
    }
    DisposableEffect(previewSdk) {
        onDispose { previewSdk.dispose() }
    }
    UiHost(
        sdk = previewSdk,
        themeMode = themeMode,
        onThemeModeChange = {},
        screenshotBootstrapState = bootstrapState,
        screenshotPreviewState = previewState
    )
}

private fun previewUiHostStateForScene(sceneId: String): UiHostPreviewState {
    val conversations = previewConversations()
    val chatItems = conversations.associate { conversation ->
        conversation.id to previewChatItems(conversation.id)
    }
    val groupChatItems = conversations
        .filter { it.isGroup }
        .associate { conversation ->
            conversation.id to previewGroupChatItems(conversation.id)
        }
    val activePeer = if (sceneId == "calls_home") {
        null
    } else {
        PeerCallState(
            peerUsername = "Aster Stone",
            callId = byteArrayOf(0x0A, 0x0B),
            callIdHex = "0a0b",
            video = true,
            initiator = true
        )
    }
    val pending = if (sceneId == "calls_home") {
        null
    } else {
        IncomingCall(
            peerUsername = "Mira Chen",
            callId = byteArrayOf(0x01, 0x02),
            callIdHex = "0102",
            video = false
        )
    }
    return UiHostPreviewState(
        conversations = conversations,
        chatItems = chatItems,
        groupChatItems = groupChatItems,
        pendingCall = pending,
        activePeerCall = activePeer,
        activeGroupCall = null,
        groupRooms = previewCallRooms()
    )
}

private fun previewGroupChatItems(conversationId: String): List<GroupChatItem> {
    return SampleGroupChat.itemsFor(conversationId)
}

private fun previewConversations(): List<ConversationPreview> = listOf(
    ConversationPreview(
        id = "c5",
        initials = "RW",
        name = "River Walk",
        lastMessage = "Map pin shared",
        time = "10:03",
        unreadCount = 0,
        isPinned = true,
        isMuted = false,
        isGroup = true,
        isTyping = false,
        draft = "Bring the blue blanket"
    ),
    ConversationPreview(
        id = "c1",
        initials = "AS",
        name = "Aster Stone",
        lastMessage = "Voice note attached",
        time = "09:41",
        unreadCount = 2,
        isPinned = true,
        isMuted = false,
        isGroup = false,
        isTyping = false
    ),
    ConversationPreview(
        id = "c2",
        initials = "LN",
        name = "Lena North",
        lastMessage = "The lake looks calm today.",
        time = "07:52",
        unreadCount = 0,
        isPinned = false,
        isMuted = false,
        isGroup = false,
        isTyping = false
    ),
    ConversationPreview(
        id = "c3",
        initials = "DP",
        name = "Dinner Plan",
        lastMessage = "Lobby at 7:30?",
        time = "Yesterday",
        unreadCount = 5,
        isPinned = false,
        isMuted = false,
        isGroup = true,
        isTyping = false,
        mentionCount = 2
    ),
    ConversationPreview(
        id = "c4",
        initials = "RH",
        name = "Rhea",
        lastMessage = "Can we move this after lunch?",
        time = "Mon",
        unreadCount = 0,
        isPinned = false,
        isMuted = false,
        isGroup = false,
        isTyping = true
    ),
    ConversationPreview(
        id = "g1",
        initials = "WH",
        name = "Weekend House",
        lastMessage = "The deck lights are on.",
        time = "08:15",
        unreadCount = 0,
        isPinned = false,
        isMuted = true,
        isGroup = true,
        isTyping = false
    ),
    ConversationPreview(
        id = "c6",
        initials = "MC",
        name = "Mira Chen",
        lastMessage = "I am downstairs.",
        time = "Yesterday",
        unreadCount = 1,
        isPinned = false,
        isMuted = false,
        isGroup = false,
        isTyping = false
    ),
    ConversationPreview(
        id = "c7",
        initials = "PC",
        name = "Photo Club",
        lastMessage = "Shared the station album",
        time = "Tue",
        unreadCount = 0,
        isPinned = false,
        isMuted = true,
        isGroup = true,
        isTyping = false
    ),
    ConversationPreview(
        id = "c8",
        initials = "FM",
        name = "Family",
        lastMessage = "Dinner starts at eight.",
        time = "Wed",
        unreadCount = 3,
        isPinned = false,
        isMuted = false,
        isGroup = true,
        isTyping = false,
        mentionCount = 1
    )
)

private fun previewChatItems(conversationId: String): List<ChatItem> {
    return when (conversationId) {
        "c5" -> listOf(
            DayMarker(id = "day-river", label = "Today"),
            ChatMessage(
                id = "p1",
                sender = "Rhea",
                body = "I dropped the riverside pin in the thread.",
                time = "10:01",
                isMine = false
            ),
            ChatMessage(
                id = "p2",
                sender = "Me",
                body = "Got it. I will meet you by the bridge.",
                time = "10:02",
                isMine = true,
                status = MessageStatus.Read
            ),
            ChatMessage(
                id = "p3",
                sender = "Rhea",
                body = "Great. Sunset should be around 7:10.",
                time = "10:03",
                isMine = false
            )
        )
        else -> listOf(
            DayMarker(id = "day-today", label = "Today"),
            ChatMessage(
                id = "m1",
                sender = "Aster",
                body = "Morning. I saved a short list for the cafe stop.",
                time = "08:12",
                isMine = false
            ),
            ChatMessage(
                id = "m2",
                sender = "Me",
                body = "Perfect. Send the list and the map pin.",
                time = "08:13",
                isMine = true,
                status = MessageStatus.Read,
                replyTo = null
            ),
            ChatMessage(
                id = "m3",
                sender = "Me",
                body = "Also keep one table by the window if you arrive first.",
                time = "08:14",
                isMine = true,
                status = MessageStatus.Delivered
            ),
            ChatMessage(
                id = "m4",
                sender = "Aster",
                body = "Uploading now. I added the tram stop and the cafe name.",
                time = "08:15",
                isMine = false,
                attachment = Attachment(
                    kind = AttachmentKind.File,
                    label = "CafeList.pdf",
                    meta = "230 KB"
                )
            ),
            ChatMessage(
                id = "m5",
                sender = "Me",
                body = "Received. I will bring it up when everyone arrives.",
                time = "08:16",
                isMine = true,
                status = MessageStatus.Sent
            )
        )
    }
}

private fun previewCallRooms(): List<GroupCallRoomUi> = listOf(
    GroupCallRoomUi(
        groupId = "Weekend House",
        callId = "room-a",
        video = true
    ),
    GroupCallRoomUi(
        groupId = "Dinner Plan",
        callId = "room-b",
        video = false
    ),
    GroupCallRoomUi(
        groupId = "Family",
        callId = "room-c",
        video = false
    ),
    GroupCallRoomUi(
        groupId = "River Walk",
        callId = "room-d",
        video = true
    ),
    GroupCallRoomUi(
        groupId = "Photo Club",
        callId = "room-e",
        video = false
    )
)
