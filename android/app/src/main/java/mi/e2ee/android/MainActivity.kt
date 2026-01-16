package mi.e2ee.android

import android.content.Context
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.platform.LocalContext
import mi.e2ee.android.ui.ChatTheme
import mi.e2ee.android.ui.ProvideLocalization
import mi.e2ee.android.ui.SdkBridge
import mi.e2ee.android.ui.ThemeMode
import mi.e2ee.android.ui.UiHost

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            val context = LocalContext.current
            var themeMode by rememberSaveable { mutableStateOf(loadThemeMode(context)) }
            LaunchedEffect(themeMode) {
                saveThemeMode(context, themeMode)
            }
            val sdk = remember(context) { SdkBridge(context) }
            LaunchedEffect(Unit) {
                sdk.init()
            }
            DisposableEffect(Unit) {
                onDispose { sdk.dispose() }
            }
            ProvideLocalization {
                ChatTheme(mode = themeMode) {
                    UiHost(
                        sdk = sdk,
                        themeMode = themeMode,
                        onThemeModeChange = { themeMode = it }
                    )
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
    }
}
