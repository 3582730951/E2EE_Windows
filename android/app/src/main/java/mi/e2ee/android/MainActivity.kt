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
import mi.e2ee.android.ui.ChatTheme
import mi.e2ee.android.ui.ChatScreen
import mi.e2ee.android.ui.ConversationListScreen
import mi.e2ee.android.ui.ConversationPreview
import mi.e2ee.android.ui.ProvideLocalization
import mi.e2ee.android.ui.SampleChat
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
        val saved = prefs.getInt(KEY_THEME_MODE, ThemeMode.ForceDark)
        return when (saved) {
            ThemeMode.FollowSystem,
            ThemeMode.ForceDark,
            ThemeMode.ForceLight -> saved
            else -> ThemeMode.ForceDark
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
        "detail" -> ChatScreen(items = SampleChat.items)
        "settings" -> SettingsScreen(sdk = remember(context) { SdkBridge(context) })
        else -> ConversationListScreen(conversations = previewConversations())
    }
}

private fun previewConversations(): List<ConversationPreview> = listOf(
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
    )
)
