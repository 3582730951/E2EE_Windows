package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.SuggestionChip
import androidx.compose.material3.SuggestionChipDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

enum class UiIconTone {
    Primary,
    Accent,
    Neutral,
    Warning,
    Danger
}

enum class UiBadgeTone {
    Primary,
    Accent,
    Neutral,
    Warning,
    Danger
}

enum class IdentityAvatarKind {
    Person,
    Group,
    Device,
    System
}

enum class PresenceState {
    Online,
    Typing,
    Muted,
    Secure,
    Busy
}

enum class MediaHintKind {
    Photo,
    File,
    Voice,
    Link
}

enum class SecurityStateTone {
    Healthy,
    Checking,
    Review,
    Blocked
}

data class PhaseOneColors(
    val background: Color,
    val surface: Color,
    val surfaceVariant: Color,
    val primary: Color,
    val accent: Color,
    val warning: Color,
    val danger: Color,
    val onSurface: Color,
    val onSurfaceMuted: Color,
    val outline: Color,
    val cardBorder: Color,
    val glass: Color,
    val glassBorder: Color
)

object PhaseOneTokens {
    val GroupedCorner = 16.dp
    val LargeTitleTopPadding = 8.dp
    val LargeTitleBottomPadding = 4.dp
    val DockCorner = 28.dp
    val AvatarList = 50.dp
}

private val PhaseOneLightColors = PhaseOneColors(
    background = Color(0xFFF2F2F7),
    surface = Color(0xFFFFFFFF),
    surfaceVariant = Color(0xFFF7F8FC),
    primary = Color(0xFF3390EC),
    accent = Color(0xFF55B59A),
    warning = Color(0xFFF5A524),
    danger = Color(0xFFE15C64),
    onSurface = Color(0xFF111418),
    onSurfaceMuted = Color(0xFF7B8592),
    outline = Color(0xFFD9DEE7),
    cardBorder = Color(0xFFE6E9F0),
    glass = Color(0xF2FFFFFF),
    glassBorder = Color(0x99FFFFFF)
)

private val PhaseOneDarkColors = PhaseOneColors(
    background = Color(0xFF0E1621),
    surface = Color(0xFF18222D),
    surfaceVariant = Color(0xFF1D2936),
    primary = Color(0xFF3390EC),
    accent = Color(0xFF66C8AB),
    warning = Color(0xFFF6B54B),
    danger = Color(0xFFFF7B87),
    onSurface = Color(0xFFF1F5FB),
    onSurfaceMuted = Color(0xFFA7B6C8),
    outline = Color(0xFF2A3949),
    cardBorder = Color(0x1FFFFFFF),
    glass = Color(0xF218222D),
    glassBorder = Color(0x24FFFFFF)
)

@Composable
fun phaseOneColors(): PhaseOneColors {
    return if (isSystemInDarkTheme()) PhaseOneDarkColors else PhaseOneLightColors
}

@Composable
fun phaseOneLargeTitleTextStyle(): TextStyle {
    return MaterialTheme.typography.displayLarge.copy(
        fontWeight = FontWeight.SemiBold,
        letterSpacing = (-0.4).sp,
        lineHeight = 38.sp
    )
}

@Composable
fun phaseOneNavLabelTextStyle(): TextStyle {
    return MaterialTheme.typography.labelMedium.copy(
        fontWeight = FontWeight.Medium,
        letterSpacing = 0.1.sp,
        lineHeight = 14.sp
    )
}

@Composable
fun phaseOneTimestampTextStyle(): TextStyle {
    return MaterialTheme.typography.labelSmall.copy(
        fontWeight = FontWeight.Medium,
        letterSpacing = 0.sp,
        lineHeight = 13.sp
    )
}

private data class UiIconPalette(
    val container: Color,
    val content: Color,
    val border: Color
)

private data class UiBadgePalette(
    val container: Color,
    val content: Color,
    val border: Color
)

private data class IdentityPalette(
    val start: Color,
    val end: Color,
    val overlay: Color,
    val border: Color,
    val content: Color
)

@Composable
private fun iconPalette(tone: UiIconTone, active: Boolean): UiIconPalette {
    val colors = phaseOneColors()
    val emphasizedAlpha = if (active) 0.10f else 0.04f
    return when (tone) {
        UiIconTone.Primary -> UiIconPalette(
            container = colors.primary.copy(alpha = emphasizedAlpha + 0.02f),
            content = colors.primary,
            border = colors.primary.copy(alpha = if (active) 0.18f else 0.08f)
        )
        UiIconTone.Accent -> UiIconPalette(
            container = colors.accent.copy(alpha = emphasizedAlpha + 0.02f),
            content = colors.accent,
            border = colors.accent.copy(alpha = if (active) 0.18f else 0.08f)
        )
        UiIconTone.Warning -> UiIconPalette(
            container = colors.warning.copy(alpha = emphasizedAlpha + 0.02f),
            content = colors.warning,
            border = colors.warning.copy(alpha = if (active) 0.18f else 0.08f)
        )
        UiIconTone.Danger -> UiIconPalette(
            container = colors.danger.copy(alpha = emphasizedAlpha + 0.02f),
            content = colors.danger,
            border = colors.danger.copy(alpha = if (active) 0.18f else 0.08f)
        )
        UiIconTone.Neutral -> UiIconPalette(
            container = colors.surfaceVariant.copy(alpha = if (active) 0.92f else 0.78f),
            content = colors.onSurfaceMuted,
            border = colors.outline.copy(alpha = if (active) 0.18f else 0.10f)
        )
    }
}

@Composable
private fun badgePalette(tone: UiBadgeTone): UiBadgePalette {
    val colors = phaseOneColors()
    return when (tone) {
        UiBadgeTone.Primary -> UiBadgePalette(
            container = colors.primary.copy(alpha = 0.12f),
            content = colors.primary,
            border = colors.primary.copy(alpha = 0.18f)
        )
        UiBadgeTone.Accent -> UiBadgePalette(
            container = colors.accent.copy(alpha = 0.12f),
            content = colors.accent,
            border = colors.accent.copy(alpha = 0.16f)
        )
        UiBadgeTone.Warning -> UiBadgePalette(
            container = colors.warning.copy(alpha = 0.14f),
            content = colors.warning,
            border = colors.warning.copy(alpha = 0.18f)
        )
        UiBadgeTone.Danger -> UiBadgePalette(
            container = colors.danger.copy(alpha = 0.14f),
            content = colors.danger,
            border = colors.danger.copy(alpha = 0.18f)
        )
        UiBadgeTone.Neutral -> UiBadgePalette(
            container = colors.surfaceVariant.copy(alpha = 0.96f),
            content = colors.onSurfaceMuted,
            border = colors.outline.copy(alpha = 0.16f)
        )
    }
}

@Composable
private fun identityPalette(seed: String, kind: IdentityAvatarKind): IdentityPalette {
    val options = listOf(
        IdentityPalette(
            start = Color(0xFFE5F1FF),
            end = Color(0xFFD6E9FF),
            overlay = Color(0xFFB9D8FB),
            border = Color(0xFFD6E7F8),
            content = Color(0xFF24507A)
        ),
        IdentityPalette(
            start = Color(0xFFE4F9F2),
            end = Color(0xFFD2F2E7),
            overlay = Color(0xFFAEE4D3),
            border = Color(0xFFD5EDE3),
            content = Color(0xFF276B5A)
        ),
        IdentityPalette(
            start = Color(0xFFFFF0D9),
            end = Color(0xFFFDE6C3),
            overlay = Color(0xFFF3D09A),
            border = Color(0xFFF4E0BD),
            content = Color(0xFF8B5C1E)
        ),
        IdentityPalette(
            start = Color(0xFFFCE6F1),
            end = Color(0xFFF7D9EA),
            overlay = Color(0xFFEFC3DD),
            border = Color(0xFFF0D8E5),
            content = Color(0xFF8A4167)
        ),
        IdentityPalette(
            start = Color(0xFFF0E8FF),
            end = Color(0xFFE6DCFF),
            overlay = Color(0xFFD6C5FF),
            border = Color(0xFFE3DAF8),
            content = Color(0xFF5C4790)
        )
    )
    val colors = phaseOneColors()
    val systemPalette = IdentityPalette(
        start = colors.primary.copy(alpha = 0.18f),
        end = colors.accent.copy(alpha = 0.22f),
        overlay = colors.primary.copy(alpha = 0.12f),
        border = colors.outline.copy(alpha = 0.18f),
        content = Color.White
    )
    val devicePalette = IdentityPalette(
        start = colors.surfaceVariant,
        end = colors.primary.copy(alpha = 0.12f),
        overlay = colors.surfaceVariant,
        border = colors.outline.copy(alpha = 0.16f),
        content = colors.primary
    )
    if (kind == IdentityAvatarKind.System) {
        return systemPalette
    }
    if (kind == IdentityAvatarKind.Device) {
        return devicePalette
    }
    val index = remember(seed, kind) { (seed.hashCode().ushr(1)) % options.size }
    return options[index]
}

@Composable
private fun UiIconFrame(
    modifier: Modifier = Modifier,
    size: Dp,
    cornerRadius: Dp,
    containerColor: Color,
    borderColor: Color,
    framed: Boolean = true,
    shape: Shape = RoundedCornerShape(cornerRadius),
    onClick: (() -> Unit)? = null,
    content: @Composable BoxScope.() -> Unit
) {
    val baseModifier = Modifier
        .size(size)
        .clip(shape)
    val frameModifier = (if (framed) {
        baseModifier
            .background(containerColor, shape)
            .border(1.dp, borderColor, shape)
    } else {
        baseModifier
    }).let { base -> if (onClick != null) base.clickable { onClick() } else base }
    Box(
        modifier = modifier.then(frameModifier),
        contentAlignment = Alignment.Center,
        content = content
    )
}

@Composable
fun UiSemanticIcon(
    icon: ImageVector,
    contentDescription: String?,
    modifier: Modifier = Modifier,
    tone: UiIconTone = UiIconTone.Primary,
    active: Boolean = false,
    size: Dp = ChatUiTokens.IconContainerMd,
    cornerRadius: Dp = ChatUiTokens.IconCorner,
    iconSize: Dp = ChatUiTokens.IconGlyphMd,
    framed: Boolean = true,
    onClick: (() -> Unit)? = null
) {
    val palette = iconPalette(tone, active)
    UiIconFrame(
        modifier = modifier,
        size = size,
        cornerRadius = cornerRadius,
        containerColor = palette.container,
        borderColor = palette.border,
        framed = framed,
        onClick = onClick
    ) {
        Icon(
            imageVector = icon,
            contentDescription = contentDescription,
            tint = palette.content,
            modifier = Modifier.size(iconSize)
        )
    }
}

@Composable
fun UiToolbarIconButton(
    icon: ImageVector,
    contentDescription: String?,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    tone: UiIconTone = UiIconTone.Neutral
) {
    UiSemanticIcon(
        icon = icon,
        contentDescription = contentDescription,
        modifier = modifier,
        tone = tone,
        size = 34.dp,
        cornerRadius = 999.dp,
        iconSize = 15.dp,
        framed = true,
        onClick = onClick
    )
}

@Composable
fun SectionHeader(text: String, modifier: Modifier = Modifier) {
    val colors = phaseOneColors()
    Text(
        text = text,
        modifier = modifier,
        style = phaseOneNavLabelTextStyle(),
        color = colors.onSurfaceMuted
    )
}

@Composable
fun SurfaceSectionCard(
    modifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit
) {
    val borderColor = MaterialTheme.colorScheme.outline.copy(alpha = ChatUiTokens.SurfaceBorderAlpha)
    Card(
        shape = RoundedCornerShape(ChatUiTokens.CornerXLarge),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surface
        ),
        border = BorderStroke(1.dp, borderColor),
        modifier = modifier
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 12.dp),
            content = content
        )
    }
}

@Composable
fun InsetGroupedCard(
    modifier: Modifier = Modifier,
    shape: Shape = RoundedCornerShape(PhaseOneTokens.GroupedCorner),
    contentPadding: PaddingValues = PaddingValues(0.dp),
    content: @Composable ColumnScope.() -> Unit
) {
    val colors = phaseOneColors()
    Surface(
        modifier = modifier.fillMaxWidth(),
        shape = shape,
        color = colors.surface,
        border = BorderStroke(1.dp, colors.cardBorder),
        shadowElevation = 0.dp
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(contentPadding),
            content = content
        )
    }
}

@Composable
fun GlassTopAppBar(
    modifier: Modifier = Modifier,
    shape: Shape = RoundedCornerShape(
        bottomStart = PhaseOneTokens.DockCorner,
        bottomEnd = PhaseOneTokens.DockCorner
    ),
    contentPadding: PaddingValues = PaddingValues(start = 18.dp, end = 18.dp, top = 2.dp, bottom = 6.dp),
    leadingContent: (@Composable RowScope.() -> Unit)? = null,
    actions: @Composable RowScope.() -> Unit = {},
    titleContent: @Composable ColumnScope.() -> Unit,
    bottomContent: (@Composable ColumnScope.() -> Unit)? = null
) {
    val colors = phaseOneColors()
    Surface(
        modifier = modifier.fillMaxWidth(),
        shape = shape,
        color = colors.glass,
        border = BorderStroke(1.dp, colors.glassBorder),
        shadowElevation = 0.dp
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .statusBarsPadding()
                .padding(contentPadding),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                if (leadingContent != null) {
                    Row(
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        content = leadingContent
                    )
                } else {
                    Spacer(modifier = Modifier.width(1.dp))
                }
                Spacer(modifier = Modifier.weight(1f))
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                    content = actions
                )
            }
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(4.dp),
                content = titleContent
            )
            if (bottomContent != null) {
                Column(
                    modifier = Modifier.fillMaxWidth(),
                    content = bottomContent
                )
            }
        }
    }
}

@Composable
fun PrimaryButton(
    label: String,
    modifier: Modifier = Modifier,
    fillMaxWidth: Boolean = true,
    enabled: Boolean = true,
    onClick: () -> Unit = {}
) {
    val widthModifier = if (fillMaxWidth) Modifier.fillMaxWidth() else Modifier
    Button(
        modifier = modifier
            .then(widthModifier)
            .height(46.dp),
        shape = RoundedCornerShape(ChatUiTokens.CornerMedium),
        colors = ButtonDefaults.buttonColors(
            containerColor = MaterialTheme.colorScheme.primary,
            contentColor = Color.White,
            disabledContainerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.32f),
            disabledContentColor = Color.White.copy(alpha = 0.92f)
        ),
        enabled = enabled,
        onClick = onClick
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelLarge,
            fontWeight = FontWeight.SemiBold
        )
    }
}

@Composable
fun SecondaryButton(
    label: String,
    modifier: Modifier = Modifier,
    fillMaxWidth: Boolean = true,
    onClick: () -> Unit = {}
) {
    val widthModifier = if (fillMaxWidth) Modifier.fillMaxWidth() else Modifier
    OutlinedButton(
        modifier = modifier
            .then(widthModifier)
            .height(46.dp),
        shape = RoundedCornerShape(ChatUiTokens.CornerMedium),
        border = BorderStroke(
            width = 1.dp,
            color = MaterialTheme.colorScheme.outline.copy(alpha = ChatUiTokens.SurfaceBorderAlpha)
        ),
        colors = ButtonDefaults.outlinedButtonColors(
            containerColor = MaterialTheme.colorScheme.surface,
            contentColor = MaterialTheme.colorScheme.onSurface
        ),
        onClick = onClick
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelLarge,
            fontWeight = FontWeight.Medium
        )
    }
}

@Composable
fun UiTokenIcon(
    label: String,
    modifier: Modifier = Modifier,
    size: Dp = ChatUiTokens.IconSize,
    cornerRadius: Dp = ChatUiTokens.IconCorner,
    containerColor: Color = MaterialTheme.colorScheme.primary.copy(alpha = ChatUiTokens.IconContainerAlpha),
    contentColor: Color = MaterialTheme.colorScheme.primary,
    textStyle: TextStyle = MaterialTheme.typography.labelSmall
) {
    UiIconFrame(
        modifier = modifier,
        size = size,
        cornerRadius = cornerRadius,
        containerColor = containerColor,
        borderColor = contentColor.copy(alpha = ChatUiTokens.SurfaceBorderAlpha),
        framed = true
    ) {
        Text(
            text = label.uppercase(),
            style = textStyle,
            color = contentColor,
            fontWeight = FontWeight.SemiBold,
            maxLines = 1
        )
    }
}

@Composable
fun UiGlyphIcon(
    icon: ImageVector,
    contentDescription: String?,
    modifier: Modifier = Modifier,
    size: Dp = ChatUiTokens.IconSize,
    cornerRadius: Dp = ChatUiTokens.IconCorner,
    iconSize: Dp = 17.dp,
    containerColor: Color = MaterialTheme.colorScheme.primary.copy(alpha = ChatUiTokens.IconContainerAlpha),
    contentColor: Color = MaterialTheme.colorScheme.primary
) {
    UiIconFrame(
        modifier = modifier,
        size = size,
        cornerRadius = cornerRadius,
        containerColor = containerColor,
        borderColor = contentColor.copy(alpha = ChatUiTokens.SurfaceBorderAlpha),
        framed = true
    ) {
        Icon(
            imageVector = icon,
            contentDescription = contentDescription,
            tint = contentColor,
            modifier = Modifier.size(iconSize)
        )
    }
}

@Composable
fun UiStatusIconBadge(
    icon: ImageVector,
    contentDescription: String?,
    modifier: Modifier = Modifier,
    tone: UiIconTone = UiIconTone.Neutral,
    framed: Boolean = false
) {
    val badgeTone = when (tone) {
        UiIconTone.Primary -> UiBadgeTone.Primary
        UiIconTone.Accent -> UiBadgeTone.Accent
        UiIconTone.Neutral -> UiBadgeTone.Neutral
        UiIconTone.Warning -> UiBadgeTone.Warning
        UiIconTone.Danger -> UiBadgeTone.Danger
    }
    val palette = badgePalette(badgeTone)
    if (framed) {
        UiBadgeFrame(
            modifier = modifier,
            palette = palette,
            horizontalPadding = 4.dp
        ) {
            Icon(
                imageVector = icon,
                contentDescription = contentDescription,
                tint = palette.content,
                modifier = Modifier.size(ChatUiTokens.IconGlyphXs)
            )
        }
    } else {
        Icon(
            imageVector = icon,
            contentDescription = contentDescription,
            tint = palette.content,
            modifier = modifier.size(ChatUiTokens.IconGlyphSm)
        )
    }
}

@Composable
fun UiStatusCountBadge(
    label: String,
    modifier: Modifier = Modifier,
    tone: UiBadgeTone = UiBadgeTone.Primary
) {
    val palette = badgePalette(tone)
    UiBadgeFrame(
        modifier = modifier,
        palette = palette
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = palette.content,
            fontWeight = FontWeight.Medium
        )
    }
}

@Composable
private fun UiBadgeFrame(
    modifier: Modifier = Modifier,
    palette: UiBadgePalette,
    horizontalPadding: Dp = 5.dp,
    content: @Composable RowScope.() -> Unit
) {
    Row(
        modifier = modifier
            .clip(RoundedCornerShape(ChatUiTokens.BadgeCorner))
            .background(palette.container)
            .border(
                width = 1.dp,
                color = palette.border,
                shape = RoundedCornerShape(ChatUiTokens.BadgeCorner)
            )
            .defaultMinSize(minHeight = 18.dp)
            .padding(horizontal = horizontalPadding, vertical = 0.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        content()
    }
}

@Composable
private fun presenceColor(state: PresenceState): Color {
    return when (state) {
        PresenceState.Online -> MaterialTheme.colorScheme.secondary
        PresenceState.Typing -> MaterialTheme.colorScheme.primary
        PresenceState.Muted -> MaterialTheme.colorScheme.onSurfaceVariant
        PresenceState.Secure -> MaterialTheme.colorScheme.secondary
        PresenceState.Busy -> MaterialTheme.colorScheme.tertiary
    }
}

@Composable
fun IdentityAvatar(
    label: String,
    modifier: Modifier = Modifier,
    seed: String = label,
    kind: IdentityAvatarKind = IdentityAvatarKind.Person,
    size: Dp = ChatUiTokens.AvatarMedium,
    presenceState: PresenceState? = null,
    badgeIcon: ImageVector? = null
) {
    val palette = identityPalette(seed, kind)
    val shape = if (kind == IdentityAvatarKind.Device) {
        RoundedCornerShape((size.value * 0.30f).dp)
    } else {
        CircleShape
    }
    Box(
        modifier = modifier.size(size),
        contentAlignment = Alignment.Center
    ) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .clip(shape)
                .background(
                    brush = Brush.linearGradient(
                        colors = listOf(palette.start, palette.end)
                    ),
                    shape = shape
                )
                .border(1.dp, palette.border.copy(alpha = 0.28f), shape)
        ) {
            when (kind) {
                IdentityAvatarKind.Person -> {
                    Text(
                        text = identityToken(label, 2),
                        modifier = Modifier.align(Alignment.Center),
                        style = MaterialTheme.typography.titleMedium,
                        color = palette.content,
                        fontWeight = FontWeight.SemiBold
                    )
                }
                IdentityAvatarKind.Group -> GroupAvatarCollage(label = label, palette = palette)
                IdentityAvatarKind.Device -> {
                    UiSemanticIcon(
                        icon = badgeIcon ?: MiOwnedIcons.Devices,
                        contentDescription = label,
                        modifier = Modifier.align(Alignment.Center),
                        tone = UiIconTone.Primary,
                        size = size * 0.52f,
                        cornerRadius = (size.value * 0.16f).dp,
                        iconSize = size * 0.24f,
                        framed = false
                    )
                }
                IdentityAvatarKind.System -> {
                    UiSemanticIcon(
                        icon = badgeIcon ?: MiOwnedIcons.ShieldCheck,
                        contentDescription = label,
                        modifier = Modifier.align(Alignment.Center),
                        tone = UiIconTone.Primary,
                        size = size * 0.52f,
                        cornerRadius = (size.value * 0.16f).dp,
                        iconSize = size * 0.24f,
                        framed = false
                    )
                }
            }
        }
        if (badgeIcon != null && kind != IdentityAvatarKind.Device && kind != IdentityAvatarKind.System) {
            Box(
                modifier = Modifier
                    .align(Alignment.BottomEnd)
                    .offset(x = 2.dp, y = 2.dp)
            ) {
                UiSemanticIcon(
                    icon = badgeIcon,
                    contentDescription = null,
                    tone = UiIconTone.Primary,
                    size = (size * 0.32f).coerceAtLeast(16.dp),
                    cornerRadius = 999.dp,
                    iconSize = (size * 0.15f).coerceAtLeast(9.dp)
                )
            }
        }
        if (presenceState != null) {
            Box(
                modifier = Modifier
                    .align(Alignment.BottomEnd)
                    .offset(x = 2.dp, y = 2.dp)
                    .size((size * 0.24f).coerceAtLeast(10.dp))
                    .clip(CircleShape)
                    .background(presenceColor(presenceState), CircleShape)
                    .border(2.dp, MaterialTheme.colorScheme.surface, CircleShape)
            )
        }
    }
}

@Composable
private fun GroupAvatarCollage(label: String, palette: IdentityPalette) {
    val tokens = groupTokens(label)
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(4.dp),
        verticalArrangement = Arrangement.spacedBy(3.dp)
    ) {
        Row(
            modifier = Modifier.weight(1f),
            horizontalArrangement = Arrangement.spacedBy(3.dp)
        ) {
            GroupAvatarCell(tokens[0], palette, Modifier.weight(1f))
            GroupAvatarCell(tokens[1], palette, Modifier.weight(1f))
        }
        Row(
            modifier = Modifier.weight(1f),
            horizontalArrangement = Arrangement.spacedBy(3.dp)
        ) {
            GroupAvatarCell(tokens[2], palette, Modifier.weight(1f))
            GroupAvatarCell(tokens[3], palette, Modifier.weight(1f))
        }
    }
}

@Composable
private fun GroupAvatarCell(label: String, palette: IdentityPalette, modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .fillMaxSize()
            .clip(RoundedCornerShape(999.dp))
            .background(palette.overlay.copy(alpha = 0.26f)),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = palette.content,
            fontWeight = FontWeight.SemiBold
        )
    }
}

private fun identityToken(label: String, maxChars: Int): String {
    val pieces = label
        .trim()
        .split(Regex("\\s+"))
        .filter { it.isNotBlank() }
    val token = when {
        pieces.size >= 2 -> buildString {
            append(pieces.first().first())
            append(pieces.last().first())
        }
        label.isNotBlank() -> label.filter { it.isLetterOrDigit() }.take(maxChars)
        else -> "MI"
    }
    return token.uppercase()
}

private fun groupTokens(label: String): List<String> {
    val pieces = label
        .trim()
        .split(Regex("\\s+"))
        .filter { it.isNotBlank() }
        .map { it.take(1).uppercase() }
    if (pieces.size >= 4) {
        return pieces.take(4)
    }
    val fallback = identityToken(label, 2).padEnd(4, '•').take(4)
    return (pieces + fallback.map { it.toString() }).take(4)
}

@Composable
fun MediaHintChip(
    kind: MediaHintKind,
    label: String,
    modifier: Modifier = Modifier,
    emphasized: Boolean = false,
    showLabel: Boolean = false
) {
    val tone = when (kind) {
        MediaHintKind.Photo -> UiBadgeTone.Primary
        MediaHintKind.File -> UiBadgeTone.Accent
        MediaHintKind.Voice -> UiBadgeTone.Warning
        MediaHintKind.Link -> UiBadgeTone.Neutral
    }
    val icon = when (kind) {
        MediaHintKind.Photo -> MiOwnedIcons.Photo
        MediaHintKind.File -> MiOwnedIcons.File
        MediaHintKind.Voice -> MiOwnedIcons.Play
        MediaHintKind.Link -> MiOwnedIcons.Link
    }
    val palette = badgePalette(tone)
    Row(
        modifier = modifier
            .clip(RoundedCornerShape(999.dp))
            .background(
                if (emphasized) palette.container.copy(alpha = 0.95f) else palette.container
            )
            .border(1.dp, palette.border, RoundedCornerShape(999.dp))
            .padding(
                horizontal = if (showLabel) 8.dp else 6.dp,
                vertical = 3.dp
            ),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            imageVector = icon,
            contentDescription = label,
            modifier = Modifier.size(12.dp),
            tint = palette.content
        )
        if (showLabel) {
            Spacer(modifier = Modifier.width(5.dp))
            Text(
                text = label,
                style = MaterialTheme.typography.labelSmall,
                color = palette.content,
                fontWeight = FontWeight.Medium
            )
        }
    }
}

@Composable
fun SecurityStateStrip(
    tone: SecurityStateTone,
    title: String,
    detail: String,
    modifier: Modifier = Modifier,
    actionLabel: String? = null,
    onAction: (() -> Unit)? = null
) {
    val accent = when (tone) {
        SecurityStateTone.Healthy -> MaterialTheme.colorScheme.secondary
        SecurityStateTone.Checking -> MaterialTheme.colorScheme.primary
        SecurityStateTone.Review -> MaterialTheme.colorScheme.tertiary
        SecurityStateTone.Blocked -> MaterialTheme.colorScheme.error
    }
    val icon = when (tone) {
        SecurityStateTone.Healthy -> MiOwnedIcons.ShieldCheck
        SecurityStateTone.Checking -> MiOwnedIcons.Clock
        SecurityStateTone.Review -> MiOwnedIcons.Alert
        SecurityStateTone.Blocked -> MiOwnedIcons.Lock
    }
    SurfaceSectionCard(
        modifier = modifier
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            UiSemanticIcon(
                icon = icon,
                contentDescription = title,
                tone = when (tone) {
                    SecurityStateTone.Healthy -> UiIconTone.Accent
                    SecurityStateTone.Checking -> UiIconTone.Primary
                    SecurityStateTone.Review -> UiIconTone.Warning
                    SecurityStateTone.Blocked -> UiIconTone.Danger
                },
                size = ChatUiTokens.IconContainerLg,
                iconSize = ChatUiTokens.IconGlyphLg
            )
            Spacer(modifier = Modifier.width(12.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleMedium,
                    color = MaterialTheme.colorScheme.onSurface
                )
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    text = detail,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            if (!actionLabel.isNullOrBlank() && onAction != null) {
                SecondaryButton(
                    label = actionLabel,
                    fillMaxWidth = false,
                    modifier = Modifier.defaultMinSize(minWidth = 84.dp),
                    onClick = onAction
                )
            } else {
                Box(
                    modifier = Modifier
                        .size(10.dp)
                        .clip(CircleShape)
                        .background(accent)
                )
            }
        }
    }
}

@Composable
fun EmptyStateIllustration(
    icon: ImageVector,
    contentDescription: String,
    modifier: Modifier = Modifier,
    tone: UiIconTone = UiIconTone.Primary,
    chipLabel: String? = null
) {
    val palette = iconPalette(tone, active = true)
    Box(
        modifier = modifier,
        contentAlignment = Alignment.Center
    ) {
        Box(
            modifier = Modifier
                .size(ChatUiTokens.IllustrationFrame)
                .clip(RoundedCornerShape(ChatUiTokens.CornerXLarge))
                .background(
                    Brush.radialGradient(
                        colors = listOf(
                            palette.container.copy(alpha = 0.95f),
                            MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.28f)
                        )
                    )
                )
                .border(
                    1.dp,
                    palette.border.copy(alpha = 0.35f),
                    RoundedCornerShape(ChatUiTokens.CornerXLarge)
                )
        )
        Box(
            modifier = Modifier
                .size(78.dp)
                .clip(RoundedCornerShape(24.dp))
                .background(
                    Brush.linearGradient(
                        listOf(palette.content.copy(alpha = 0.92f), palette.content.copy(alpha = 0.72f))
                    )
                ),
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = icon,
                contentDescription = contentDescription,
                modifier = Modifier.size(30.dp),
                tint = Color.White
            )
        }
        Box(
            modifier = Modifier
                .align(Alignment.TopStart)
                .offset(x = 18.dp, y = 16.dp)
                .size(14.dp)
                .clip(CircleShape)
                .background(palette.content.copy(alpha = 0.22f))
        )
        Box(
            modifier = Modifier
                .align(Alignment.BottomEnd)
                .offset(x = (-16).dp, y = (-18).dp)
                .size(18.dp)
                .clip(CircleShape)
                .background(palette.content.copy(alpha = 0.16f))
        )
        if (!chipLabel.isNullOrBlank()) {
            UiStatusCountBadge(
                label = chipLabel,
                tone = when (tone) {
                    UiIconTone.Primary -> UiBadgeTone.Primary
                    UiIconTone.Accent -> UiBadgeTone.Accent
                    UiIconTone.Warning -> UiBadgeTone.Warning
                    UiIconTone.Danger -> UiBadgeTone.Danger
                    UiIconTone.Neutral -> UiBadgeTone.Neutral
                },
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .offset(y = 10.dp)
            )
        }
    }
}

@Composable
fun UiChevron(
    modifier: Modifier = Modifier,
    color: Color = MaterialTheme.colorScheme.onSurfaceVariant
) {
    Icon(
        imageVector = MiOwnedIcons.ChevronRight,
        contentDescription = null,
        modifier = modifier.size(16.dp),
        tint = color.copy(alpha = 0.82f)
    )
}

@Composable
fun AvatarBadge(initials: String, tint: Color, size: Dp = 40.dp) {
    val label = initials.ifBlank { "MI" }
    Box(
        modifier = Modifier.size(size)
    ) {
        IdentityAvatar(
            label = label,
            seed = "$label-${tint.value}",
            kind = IdentityAvatarKind.Person,
            size = size
        )
    }
}

@Composable
fun StatusDot(color: Color, size: Dp = 8.dp) {
    Box(
        modifier = Modifier
            .size(size)
            .clip(CircleShape)
            .background(color)
    )
}

@Composable
fun LabeledChip(label: String, tint: Color, modifier: Modifier = Modifier) {
    SuggestionChip(
        onClick = {},
        modifier = modifier,
        label = {
            Text(
                text = label,
                style = MaterialTheme.typography.labelSmall
            )
        },
        icon = {
            StatusDot(color = tint, size = 6.dp)
        },
        border = BorderStroke(1.dp, tint.copy(alpha = ChatUiTokens.SurfaceBorderAlpha)),
        colors = SuggestionChipDefaults.suggestionChipColors(
            containerColor = tint.copy(alpha = 0.1f),
            labelColor = tint,
            iconContentColor = tint
        )
    )
}

@Composable
fun AuthFooterRow(
    leftText: String,
    onLeftClick: () -> Unit,
    rightPrefix: String,
    rightLink: String,
    onRightClick: () -> Unit = {}
) {
    Box(modifier = Modifier.fillMaxWidth()) {
        Text(
            text = leftText,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.primary,
            modifier = Modifier
                .align(Alignment.CenterStart)
                .clickable { onLeftClick() }
        )
        Row(
            modifier = Modifier.align(Alignment.CenterEnd),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(4.dp)
        ) {
            Text(
                text = rightPrefix,
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Text(
                text = rightLink,
                style = MaterialTheme.typography.labelMedium,
                color = MaterialTheme.colorScheme.primary,
                textDecoration = TextDecoration.Underline,
                modifier = Modifier.clickable { onRightClick() }
            )
        }
    }
}
