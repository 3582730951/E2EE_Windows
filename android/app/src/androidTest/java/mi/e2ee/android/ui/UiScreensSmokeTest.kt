package mi.e2ee.android.ui

import androidx.activity.ComponentActivity
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class UiScreensSmokeTest {
    @get:Rule
    val composeRule = createAndroidComposeRule<ComponentActivity>()

    @Test
    fun loginScreenShowsPrimaryActions() {
        composeRule.setContent {
            ChatTheme {
                LoginScreen()
            }
        }

        composeRule.onNodeWithText("Welcome back").assertIsDisplayed()
        composeRule.onNodeWithText("Sign in").assertIsDisplayed()
        composeRule.onNodeWithText("Show QR").assertIsDisplayed()
        composeRule.onNodeWithText("Scan QR").assertIsDisplayed()
    }

    @Test
    fun conversationListShowsChatsAndSearch() {
        val conversations = listOf(
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
            )
        )
        composeRule.setContent {
            ChatTheme {
                ConversationListScreen(conversations = conversations)
            }
        }

        composeRule.onNodeWithText("Chats").assertIsDisplayed()
        composeRule.onNodeWithText("Search chats").assertIsDisplayed()
        composeRule.onNodeWithText("Aster Stone").assertIsDisplayed()
        composeRule.onNodeWithText("Threat Guild").assertIsDisplayed()
    }

    @Test
    fun chatScreenShowsPinnedAndComposer() {
        composeRule.setContent {
            ChatTheme {
                ChatScreen(items = SampleChat.items)
            }
        }

        composeRule.onNodeWithText("Aster Stone").assertIsDisplayed()
        composeRule.onNodeWithText("Messages are end-to-end encrypted.").assertIsDisplayed()
        composeRule.onNodeWithText("Pinned message").assertIsDisplayed()
        composeRule.onNodeWithText("Write a message...").assertIsDisplayed()
    }
}
