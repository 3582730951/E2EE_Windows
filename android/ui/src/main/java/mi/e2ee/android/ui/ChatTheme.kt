package mi.e2ee.android.ui

import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import mi.e2ee.android.R

private val Primary = Color(0xFF2F67E8)
private val PrimaryDark = Color(0xFF2249A9)
private val Accent = Color(0xFF1D9771)
private val Warning = Color(0xFFC88A3A)
private val Danger = Color(0xFFD35C61)
private val Background = Color(0xFFF4EFE8)
private val Surface = Color(0xFFFCFAF7)
private val SurfaceVariant = Color(0xFFEAE3D9)
private val TextPrimary = Color(0xFF1F242C)
private val TextSecondary = Color(0xFF6A6F79)
private val Divider = Color(0xFFD9D2C8)

private val DarkPrimary = Color(0xFF4E7FFF)
private val DarkPrimaryContainer = Color(0xFF1B3569)
private val DarkAccent = Color(0xFF35B38A)
private val DarkWarning = Color(0xFFD9A25A)
private val DarkDanger = Color(0xFFF07C7C)
private val DarkBackground = Color(0xFF091117)
private val DarkSurface = Color(0xFF101922)
private val DarkSurfaceVariant = Color(0xFF182431)
private val DarkTextPrimary = Color(0xFFE8EDF4)
private val DarkTextSecondary = Color(0xFF9DAEBC)
private val DarkDivider = Color(0xFF2B3846)

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
        fontSize = 30.sp,
        lineHeight = 34.sp
    ),
    headlineMedium = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.SemiBold,
        fontSize = 26.sp,
        lineHeight = 32.sp
    ),
    headlineSmall = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.SemiBold,
        fontSize = 22.sp,
        lineHeight = 28.sp
    ),
    titleLarge = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.SemiBold,
        fontSize = 21.sp,
        lineHeight = 27.sp
    ),
    titleMedium = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Medium,
        fontSize = 17.sp,
        lineHeight = 23.sp
    ),
    titleSmall = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Medium,
        fontSize = 14.sp,
        lineHeight = 18.sp
    ),
    bodyLarge = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Normal,
        fontSize = 16.sp,
        lineHeight = 24.sp
    ),
    bodyMedium = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Normal,
        fontSize = 14.sp,
        lineHeight = 20.sp
    ),
    labelLarge = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Medium,
        fontSize = 13.sp,
        lineHeight = 16.sp,
        letterSpacing = 0.3.sp
    ),
    labelMedium = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Medium,
        fontSize = 12.sp,
        lineHeight = 15.sp,
        letterSpacing = 0.4.sp
    ),
    labelSmall = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Medium,
        fontSize = 11.sp,
        lineHeight = 14.sp,
        letterSpacing = 0.8.sp
    ),
    bodySmall = TextStyle(
        fontFamily = Mono,
        fontWeight = FontWeight.Medium,
        fontSize = 12.sp,
        lineHeight = 17.sp
    )
)

private val ChatColorScheme = lightColorScheme(
    primary = Primary,
    onPrimary = Color.White,
    primaryContainer = Color(0xFFDDE7FF),
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
    onPrimaryContainer = Color(0xFFD9E6FF),
    secondary = DarkAccent,
    onSecondary = Color(0xFF07271C),
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

object ChatUiTokens {
    val CornerSmall = 14.dp
    val CornerMedium = 18.dp
    val CornerLarge = 22.dp
    val IconContainerXs = 20.dp
    val IconContainerSm = 28.dp
    val IconContainerMd = 32.dp
    val IconContainerLg = 36.dp
    val IconGlyphXs = 11.dp
    val IconGlyphSm = 13.dp
    val IconGlyphMd = 15.dp
    val IconGlyphLg = 17.dp
    val BadgeCorner = 8.dp
    val IconSize = 32.dp
    val IconCorner = 9.dp
    val SectionSpacing = 12.dp
    val ItemSpacing = 8.dp
    const val IconContainerAlpha = 0.1f
    const val SurfaceBorderAlpha = 0.16f
    const val MotionFastMs = 160
    const val MotionStandardMs = 220
}

@Composable
fun ChatTheme(mode: Int = ThemeMode.FollowSystem, content: @Composable () -> Unit) {
    val useDark = when (mode) {
        ThemeMode.FollowSystem -> isSystemInDarkTheme()
        ThemeMode.ForceLight -> false
        ThemeMode.ForceDark -> true
        else -> isSystemInDarkTheme()
    }
    val colors = if (useDark) ChatDarkColorScheme else ChatColorScheme
    MaterialTheme(
        colorScheme = colors,
        typography = ChatTypography,
        content = content
    )
}
