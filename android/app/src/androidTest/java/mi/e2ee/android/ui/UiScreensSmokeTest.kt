package mi.e2ee.android.ui

import androidx.activity.ComponentActivity
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithTag
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

        composeRule.onNodeWithTag("conversation-list-screen").assertIsDisplayed()
        composeRule.onNodeWithTag("conversation-search").assertIsDisplayed()
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

        composeRule.onNodeWithTag("chat-screen").assertIsDisplayed()
        composeRule.onNodeWithText("Aster Stone").assertIsDisplayed()
        composeRule.onNodeWithText("Perfect. Shipping the build after tests.").assertIsDisplayed()
        composeRule.onNodeWithText("Write a message...").assertIsDisplayed()
    }

    @Test
    fun registerScreenShowsIdentityInputs() {
        composeRule.setContent {
            ChatTheme {
                RegisterScreen()
            }
        }

        composeRule.onNodeWithTag("register-screen").assertIsDisplayed()
        composeRule.onNodeWithText("Display name").assertIsDisplayed()
        composeRule.onNodeWithText("Confirm password").assertIsDisplayed()
    }

    @Test
    fun settingsScreenShowsAccountAndPrivacySections() {
        composeRule.setContent {
            ChatTheme {
                SettingsScreen(sdk = SdkBridge(composeRule.activity))
            }
        }

        composeRule.onNodeWithTag("settings-screen").assertIsDisplayed()
        composeRule.onNodeWithText("Account and security").assertIsDisplayed()
        composeRule.onNodeWithText("Privacy").assertIsDisplayed()
    }

    @Test
    fun groupChatScreenShowsTitleAndMessages() {
        composeRule.setContent {
            ChatTheme {
                GroupChatScreen(items = SampleGroupChat.items)
            }
        }

        composeRule.onNodeWithText("Design Ops").assertIsDisplayed()
        composeRule.onNodeWithText("12 members / Secure group").assertIsDisplayed()
        composeRule.onNodeWithText("Aster joined the group").assertIsDisplayed()
    }

    @Test
    fun friendRequestsScreenShowsActions() {
        val requests = listOf(
            FriendRequestUi(username = "mina", remark = "Hi, let's connect")
        )
        composeRule.setContent {
            ChatTheme {
                FriendRequestsScreen(requests = requests)
            }
        }

        composeRule.onNodeWithText("Friend requests").assertIsDisplayed()
        composeRule.onNodeWithText("mina").assertIsDisplayed()
        composeRule.onNodeWithText("Accept").assertIsDisplayed()
        composeRule.onNodeWithText("Decline").assertIsDisplayed()
    }
}
