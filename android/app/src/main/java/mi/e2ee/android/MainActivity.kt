package mi.e2ee.android

import android.content.Context
import android.os.Bundle
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
import mi.e2ee.android.ui.ChatScreen
import mi.e2ee.android.ui.CallsHomeScreen
import mi.e2ee.android.ui.ConversationListScreen
import mi.e2ee.android.ui.ConversationPreview
import mi.e2ee.android.ui.DayMarker
import mi.e2ee.android.ui.GroupCallRoomUi
import mi.e2ee.android.ui.IncomingCall
import mi.e2ee.android.ui.LoginScreen
import mi.e2ee.android.ui.MessageStatus
import mi.e2ee.android.ui.PeerCallState
import mi.e2ee.android.ui.ProvideLocalization
import mi.e2ee.android.ui.SecurityCenterScreen
import mi.e2ee.android.ui.SdkBridge
import mi.e2ee.android.ui.SettingsScreen
import mi.e2ee.android.ui.ThemeMode
import mi.e2ee.android.ui.UiHost

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val screenshotMode = intent?.getStringExtra(EXTRA_SCREENSHOT_MODE)
        setContent {
            val context = LocalContext.current
            var themeMode by rememberSaveable { mutableStateOf(loadThemeMode(context)) }
            LaunchedEffect(themeMode) {
                saveThemeMode(context, themeMode)
            }
            ProvideLocalization {
                ChatTheme(mode = themeMode) {
                    if (screenshotMode != null) {
                        MainScreenshotScene(mode = screenshotMode, context = context)
                    } else {
                        val sdk = remember(context) { SdkBridge(context) }
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

    private companion object {
        const val PREFS_NAME = "mi_chat_prefs"
        const val KEY_THEME_MODE = "theme_mode"
        const val EXTRA_SCREENSHOT_MODE = "mi.e2ee.android.extra.SCREENSHOT_MODE"
    }
}

@Composable
private fun MainScreenshotScene(mode: String, context: Context) {
    when (mode.lowercase()) {
        "login", "auth_login" -> LoginScreen(
            initialUsername = "aster@mi.internal",
            initialPassword = "trust-build-2026",
            statusMessage = "Pinned gateway verified."
        )
        "detail", "chat_detail" -> ChatScreen(
            items = previewChatItems(),
            title = "Aster Stone",
            status = "Online",
            initials = "AS",
            showTyping = false
        )
        "calls", "calls_home" -> CallsHomeScreen(
            pendingCall = IncomingCall(
                peerUsername = "Mira Chen",
                callId = byteArrayOf(0x01, 0x02),
                callIdHex = "0102",
                video = false
            ),
            activePeerCall = PeerCallState(
                peerUsername = "Aster Stone",
                callId = byteArrayOf(0x0A, 0x0B),
                callIdHex = "0a0b",
                video = true,
                initiator = true
            ),
            activeGroupCall = null,
            groupRooms = previewCallRooms()
        )
        "security_center" -> SecurityCenterScreen(
            sdk = remember(context) { SdkBridge(context) },
            title = "Security Center",
            previewMode = true
        )
        "settings", "settings_home" ->
            SettingsScreen(
                sdk = remember(context) { SdkBridge(context) },
                themeMode = ThemeMode.FollowSystem,
                onThemeModeChange = {},
                showBackButton = false,
                onBack = {},
                onOpenSecurityCenter = {},
                onOpenAccount = {},
                onOpenPrivacy = {},
                onOpenDiagnostics = {},
                onOpenChats = {},
                onOpenCalls = {},
                onOpenContacts = {}
            )
        "chats", "chat_list" -> ConversationListScreen(conversations = previewConversations())
        else -> ConversationListScreen(conversations = previewConversations())
    }
}

private fun previewConversations(): List<ConversationPreview> = listOf(
    ConversationPreview(
        id = "c5",
        initials = "PT",
        name = "Platform",
        lastMessage = "Draft: verify API33 smoke gate",
        time = "10:03",
        unreadCount = 0,
        isPinned = true,
        isMuted = false,
        isGroup = true,
        isTyping = false,
        draft = "verify API33 smoke gate"
    ),
    ConversationPreview(
        id = "c1",
        initials = "AS",
        name = "Aster Stone",
        lastMessage = "Encrypted check-in",
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
        lastMessage = "Screenshot pass is green.",
        time = "07:52",
        unreadCount = 0,
        isPinned = false,
        isMuted = false,
        isGroup = false,
        isTyping = false
    ),
    ConversationPreview(
        id = "c3",
        initials = "OS",
        name = "Ops Sync",
        lastMessage = "Queue cap increased to 512.",
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
        initials = "TG",
        name = "Threat Guild",
        lastMessage = "Rotation completed",
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
        lastMessage = "Transport settled after the reconnect.",
        time = "Yesterday",
        unreadCount = 1,
        isPinned = false,
        isMuted = false,
        isGroup = false,
        isTyping = false
    ),
    ConversationPreview(
        id = "c7",
        initials = "IN",
        name = "Infra Notes",
        lastMessage = "Pinned gateway fingerprint rolled forward.",
        time = "Tue",
        unreadCount = 0,
        isPinned = false,
        isMuted = true,
        isGroup = true,
        isTyping = false
    ),
    ConversationPreview(
        id = "c8",
        initials = "LC",
        name = "Launch Crew",
        lastMessage = "Need final release note approval.",
        time = "Wed",
        unreadCount = 3,
        isPinned = false,
        isMuted = false,
        isGroup = true,
        isTyping = false,
        mentionCount = 1
    )
)

private fun previewChatItems(): List<ChatItem> = listOf(
    DayMarker(id = "day-today", label = "Today"),
    ChatMessage(
        id = "m1",
        sender = "Aster",
        body = "Morning. I mapped the edge cases into a short checklist.",
        time = "08:12",
        isMine = false
    ),
    ChatMessage(
        id = "m2",
        sender = "Me",
        body = "Great. Send the checklist and the risk notes.",
        time = "08:13",
        isMine = true,
        status = MessageStatus.Read,
        replyTo = null
    ),
    ChatMessage(
        id = "m3",
        sender = "Me",
        body = "Also flag the retry storms after reconnect so Ops can review it.",
        time = "08:14",
        isMine = true,
        status = MessageStatus.Delivered
    ),
    ChatMessage(
        id = "m4",
        sender = "Aster",
        body = "Uploading now. The top risk is retry storms after reconnect.",
        time = "08:15",
        isMine = false,
        attachment = Attachment(
            kind = AttachmentKind.File,
            label = "Checklist.pdf",
            meta = "230 KB"
        )
    ),
    ChatMessage(
        id = "m5",
        sender = "Me",
        body = "Received. I will add backoff, cap the queue depth, and send a clean summary.",
        time = "08:16",
        isMine = true,
        status = MessageStatus.Sent
    )
)

private fun previewCallRooms(): List<GroupCallRoomUi> = listOf(
    GroupCallRoomUi(
        groupId = "Threat Guild",
        callId = "room-a",
        video = true
    ),
    GroupCallRoomUi(
        groupId = "Ops Sync",
        callId = "room-b",
        video = false
    )
)
