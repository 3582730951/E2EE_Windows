package mi.e2ee.android.ui

import android.content.Context
import android.content.Intent
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createEmptyComposeRule
import androidx.compose.ui.test.onAllNodesWithTag
import androidx.compose.ui.test.onAllNodesWithText
import androidx.compose.ui.test.onNodeWithTag
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.core.app.ActivityScenario
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import mi.e2ee.android.MainActivity
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class UiScreensSmokeTest {
    @get:Rule
    val composeRule = createEmptyComposeRule()

    @Test
    fun screenshotBootstrapDetailStartsInHostAndReturnsToChatList() {
        val scenario = launchScreenshotScene("detail")
        try {
            waitForTag("chat-screen")
            composeRule.onNodeWithTag("chat-screen").assertIsDisplayed()

            scenario.onActivity { activity ->
                activity.onBackPressedDispatcher.onBackPressed()
            }

            waitForTag("conversation-list-screen")
            composeRule.onNodeWithTag("conversation-list-screen").assertIsDisplayed()
        } finally {
            scenario.close()
        }
    }

    @Test
    fun screenshotBootstrapSecurityCenterReturnsToSettingsRoot() {
        val scenario = launchScreenshotScene("security_center")
        try {
            waitForTag("security-center-screen")
            composeRule.onNodeWithTag("security-center-screen").assertIsDisplayed()

            scenario.onActivity { activity ->
                activity.onBackPressedDispatcher.onBackPressed()
            }

            waitForTag("settings-screen")
            composeRule.onNodeWithTag("settings-screen").assertIsDisplayed()
        } finally {
            scenario.close()
        }
    }

    @Test
    fun screenshotBootstrapCallsUsesRootCallsScreen() {
        val scenario = launchScreenshotScene("calls")
        try {
            waitForTag("calls-screen")
            waitForText("Weekend House")
            composeRule.onNodeWithText("Weekend House").assertIsDisplayed()
        } finally {
            scenario.close()
        }
    }

    @Test
    fun previewGroupConversationUsesGroupRouteWithoutSdkBootstrap() {
        val scenario = launchScreenshotScene("chats")
        try {
            waitForTag("conversation-list-screen")
            composeRule.onNodeWithText("Dinner Plan").performClick()

            waitForText("Dinner in ten minutes. Let us meet downstairs.")
            composeRule.onNodeWithText("Dinner in ten minutes. Let us meet downstairs.").assertIsDisplayed()

            scenario.onActivity { activity ->
                activity.onBackPressedDispatcher.onBackPressed()
            }

            waitForTag("conversation-list-screen")
            composeRule.onNodeWithTag("conversation-list-screen").assertIsDisplayed()
        } finally {
            scenario.close()
        }
    }

    private fun launchScreenshotScene(scene: String): ActivityScenario<MainActivity> {
        val context = ApplicationProvider.getApplicationContext<Context>()
        val intent = Intent(context, MainActivity::class.java)
            .putExtra(MainActivity.EXTRA_SCREENSHOT_MODE, scene)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        return ActivityScenario.launch(intent)
    }

    private fun waitForTag(tag: String) {
        composeRule.waitUntil(timeoutMillis = 10_000) {
            composeRule.onAllNodesWithTag(tag, useUnmergedTree = true)
                .fetchSemanticsNodes().isNotEmpty()
        }
    }

    private fun waitForText(text: String) {
        composeRule.waitUntil(timeoutMillis = 10_000) {
            composeRule.onAllNodesWithText(text, useUnmergedTree = true)
                .fetchSemanticsNodes().isNotEmpty()
        }
    }
}
