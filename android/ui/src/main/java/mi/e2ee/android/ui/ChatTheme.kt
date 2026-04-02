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

private val Primary = Color(0xFF2563EB)
private val PrimaryDark = Color(0xFF1D4ED8)
private val Accent = Color(0xFF059669)
private val Warning = Color(0xFFD97706)
private val Danger = Color(0xFFDC2626)
private val Background = Color(0xFFF8FAFC)
private val Surface = Color(0xFFFFFFFF)
private val SurfaceVariant = Color(0xFFF3F6FB)
private val TextPrimary = Color(0xFF0F172A)
private val TextSecondary = Color(0xFF64748B)
private val Divider = Color(0xFFDBE4F0)

private val DarkPrimary = Color(0xFF4E7FFF)
private val DarkPrimaryContainer = Color(0xFF17336E)
private val DarkAccent = Color(0xFF10B981)
private val DarkWarning = Color(0xFFF59E0B)
private val DarkDanger = Color(0xFFF87171)
private val DarkBackground = Color(0xFF0C1520)
private val DarkSurface = Color(0xFF111C27)
private val DarkSurfaceVariant = Color(0xFF172330)
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
        fontSize = 28.sp,
        lineHeight = 32.sp
    ),
    headlineMedium = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.SemiBold,
        fontSize = 24.sp,
        lineHeight = 30.sp
    ),
    headlineSmall = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.SemiBold,
        fontSize = 20.sp,
        lineHeight = 26.sp
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
    titleSmall = TextStyle(
        fontFamily = Sans,
        fontWeight = FontWeight.Medium,
        fontSize = 13.sp,
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
        fontFamily = Sans,
        fontWeight = FontWeight.Normal,
        fontSize = 13.sp,
        lineHeight = 18.sp
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
    val CornerSmall = 12.dp
    val CornerMedium = 14.dp
    val CornerLarge = 16.dp
    val IconContainerXs = 20.dp
    val IconContainerSm = 34.dp
    val IconContainerMd = 38.dp
    val IconContainerLg = 42.dp
    val IconGlyphXs = 11.dp
    val IconGlyphSm = 14.dp
    val IconGlyphMd = 16.dp
    val IconGlyphLg = 18.dp
    val BadgeCorner = 8.dp
    val IconSize = 36.dp
    val IconCorner = 10.dp
    val SectionSpacing = 8.dp
    val ItemSpacing = 4.dp
    const val IconContainerAlpha = 0.08f
    const val SurfaceBorderAlpha = 0.10f
    const val MotionFastMs = 120
    const val MotionStandardMs = 180
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
