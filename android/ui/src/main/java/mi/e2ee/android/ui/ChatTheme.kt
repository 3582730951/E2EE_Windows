package mi.e2ee.android.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp
import mi.e2ee.android.R

private val Primary = Color(0xFF2AABEE)
private val PrimaryDark = Color(0xFF1E84BA)
private val Accent = Color(0xFF28C76F)
private val Warning = Color(0xFFFF9F1C)
private val Danger = Color(0xFFFF4D4F)
private val Background = Color(0xFFF5F7FA)
private val Surface = Color(0xFFFFFFFF)
private val SurfaceVariant = Color(0xFFF0F2F5)
private val TextPrimary = Color(0xFF1C1C1E)
private val TextSecondary = Color(0xFF6B7280)
private val Divider = Color(0xFFE5E7EB)

private val DarkPrimary = Color(0xFF5BA7FF)
private val DarkPrimaryContainer = Color(0xFF123A5A)
private val DarkAccent = Color(0xFF2ED47A)
private val DarkWarning = Color(0xFFFFB547)
private val DarkDanger = Color(0xFFFF6B6B)
private val DarkBackground = Color(0xFF0E141B)
private val DarkSurface = Color(0xFF151C25)
private val DarkSurfaceVariant = Color(0xFF1D2633)
private val DarkTextPrimary = Color(0xFFE7ECF2)
private val DarkTextSecondary = Color(0xFF9CB0C7)
private val DarkDivider = Color(0xFF2B3646)

private val Sans = FontFamily(
    Font(R.font.source_sans_3_regular, FontWeight.Normal),
    Font(R.font.source_sans_3_medium, FontWeight.Medium),
    Font(R.font.source_sans_3_semibold, FontWeight.SemiBold),
    Font(R.font.source_han_sans_sc_regular, FontWeight.Normal),
    Font(R.font.source_han_sans_sc_medium, FontWeight.Medium),
    Font(R.font.source_han_sans_sc_semibold, FontWeight.SemiBold)
)
private val Mono = FontFamily(
    Font(R.font.jetbrains_mono_regular, FontWeight.Normal),
    Font(R.font.jetbrains_mono_medium, FontWeight.Medium)
)

private val ChatTypography = Typography(
    displayLarge = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.SemiBold,
        fontSize = 24.sp,
        lineHeight = 28.sp
    ),
    titleLarge = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.SemiBold,
        fontSize = 20.sp,
        lineHeight = 24.sp
    ),
    titleMedium = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Medium,
        fontSize = 16.sp,
        lineHeight = 22.sp
    ),
    bodyLarge = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Normal,
        fontSize = 15.sp,
        lineHeight = 22.sp
    ),
    bodyMedium = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Normal,
        fontSize = 13.sp,
        lineHeight = 18.sp
    ),
    labelSmall = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Medium,
        fontSize = 11.sp,
        lineHeight = 14.sp,
        letterSpacing = 0.6.sp
    ),
    bodySmall = TextStyle(
        fontFamily = Mono,
        fontWeight = FontWeight.Medium,
        fontSize = 13.sp,
        lineHeight = 18.sp
    )
)

private val ChatColorScheme = lightColorScheme(
    primary = Primary,
    onPrimary = Color.White,
    primaryContainer = Color(0xFFD7F1FE),
    onPrimaryContainer = PrimaryDark,
    secondary = Accent,
    onSecondary = Color.White,
    tertiary = Warning,
    error = Danger,
    background = Background,
    onBackground = TextPrimary,
    surface = Surface,
    onSurface = TextPrimary,
    surfaceVariant = SurfaceVariant,
    onSurfaceVariant = TextSecondary,
    outline = Divider
)

private val ChatDarkColorScheme = darkColorScheme(
    primary = DarkPrimary,
    onPrimary = Color.White,
    primaryContainer = DarkPrimaryContainer,
    onPrimaryContainer = Color(0xFFBEE3FF),
    secondary = DarkAccent,
    onSecondary = Color(0xFF0B2C1E),
    tertiary = DarkWarning,
    error = DarkDanger,
    background = DarkBackground,
    onBackground = DarkTextPrimary,
    surface = DarkSurface,
    onSurface = DarkTextPrimary,
    surfaceVariant = DarkSurfaceVariant,
    onSurfaceVariant = DarkTextSecondary,
    outline = DarkDivider
)

object ThemeMode {
    const val FollowSystem = 0
    const val ForceDark = 1
    const val ForceLight = 2
}

@Composable
fun ChatTheme(mode: Int = ThemeMode.ForceDark, content: @Composable () -> Unit) {
    val useDark = when (mode) {
        ThemeMode.FollowSystem -> isSystemInDarkTheme()
        ThemeMode.ForceLight -> false
        ThemeMode.ForceDark -> true
        else -> true
    }
    val colors = if (useDark) ChatDarkColorScheme else ChatColorScheme
    MaterialTheme(
        colorScheme = colors,
        typography = ChatTypography,
        content = content
    )
}
