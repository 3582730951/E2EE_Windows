package mi.e2ee.rootauth

import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createAndroidComposeRule
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performScrollTo
import androidx.test.ext.junit.runners.AndroidJUnit4
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class RootAuthActivityTest {
    @get:Rule
    val composeRule = createAndroidComposeRule<RootAuthActivity>()

    @Test
    fun showsPrimaryRootAuthActions() {
        val activity = composeRule.activity
        composeRule.onNodeWithText(activity.getString(R.string.title)).assertIsDisplayed()
        composeRule.onNodeWithText(activity.getString(R.string.copy_code)).assertIsDisplayed()
        composeRule.onNodeWithText(activity.getString(R.string.copy_pubkey)).assertIsDisplayed()
        composeRule.onNodeWithText(activity.getString(R.string.manual_title))
            .performScrollTo()
            .assertIsDisplayed()
        composeRule.onNodeWithText(activity.getString(R.string.scan_button))
            .performScrollTo()
            .assertIsDisplayed()
    }
}
