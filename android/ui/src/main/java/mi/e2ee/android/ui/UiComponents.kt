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
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
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
import androidx.compose.material3.SuggestionChip
import androidx.compose.material3.SuggestionChipDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

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

@Composable
private fun iconPalette(tone: UiIconTone, active: Boolean): UiIconPalette {
    val emphasizedAlpha = if (active) 0.08f else 0.03f
    return when (tone) {
        UiIconTone.Primary -> UiIconPalette(
            container = MaterialTheme.colorScheme.primary.copy(alpha = emphasizedAlpha),
            content = MaterialTheme.colorScheme.primary,
            border = MaterialTheme.colorScheme.primary.copy(alpha = if (active) 0.12f else 0.08f)
        )
        UiIconTone.Accent -> UiIconPalette(
            container = MaterialTheme.colorScheme.secondary.copy(alpha = emphasizedAlpha),
            content = MaterialTheme.colorScheme.secondary,
            border = MaterialTheme.colorScheme.secondary.copy(alpha = if (active) 0.12f else 0.08f)
        )
        UiIconTone.Warning -> UiIconPalette(
            container = MaterialTheme.colorScheme.tertiary.copy(alpha = emphasizedAlpha),
            content = MaterialTheme.colorScheme.tertiary,
            border = MaterialTheme.colorScheme.tertiary.copy(alpha = if (active) 0.12f else 0.08f)
        )
        UiIconTone.Danger -> UiIconPalette(
            container = MaterialTheme.colorScheme.error.copy(alpha = emphasizedAlpha),
            content = MaterialTheme.colorScheme.error,
            border = MaterialTheme.colorScheme.error.copy(alpha = if (active) 0.12f else 0.08f)
        )
        UiIconTone.Neutral -> UiIconPalette(
            container = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = if (active) 0.14f else 0.08f),
            content = MaterialTheme.colorScheme.onSurfaceVariant,
            border = MaterialTheme.colorScheme.outline.copy(alpha = if (active) 0.12f else 0.08f)
        )
    }
}

@Composable
private fun badgePalette(tone: UiBadgeTone): UiBadgePalette {
    return when (tone) {
        UiBadgeTone.Primary -> UiBadgePalette(
            container = MaterialTheme.colorScheme.primary.copy(alpha = 0.08f),
            content = MaterialTheme.colorScheme.primary,
            border = MaterialTheme.colorScheme.primary.copy(alpha = 0.12f)
        )
        UiBadgeTone.Accent -> UiBadgePalette(
            container = MaterialTheme.colorScheme.secondary.copy(alpha = 0.08f),
            content = MaterialTheme.colorScheme.secondary,
            border = MaterialTheme.colorScheme.secondary.copy(alpha = 0.12f)
        )
        UiBadgeTone.Warning -> UiBadgePalette(
            container = MaterialTheme.colorScheme.tertiary.copy(alpha = 0.08f),
            content = MaterialTheme.colorScheme.tertiary,
            border = MaterialTheme.colorScheme.tertiary.copy(alpha = 0.12f)
        )
        UiBadgeTone.Danger -> UiBadgePalette(
            container = MaterialTheme.colorScheme.error.copy(alpha = 0.08f),
            content = MaterialTheme.colorScheme.error,
            border = MaterialTheme.colorScheme.error.copy(alpha = 0.12f)
        )
        UiBadgeTone.Neutral -> UiBadgePalette(
            container = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.36f),
            content = MaterialTheme.colorScheme.onSurfaceVariant,
            border = MaterialTheme.colorScheme.outline.copy(alpha = 0.12f)
        )
    }
}

@Composable
private fun UiIconFrame(
    modifier: Modifier = Modifier,
    size: Dp,
    cornerRadius: Dp,
    containerColor: Color,
    borderColor: Color,
    framed: Boolean = true,
    onClick: (() -> Unit)? = null,
    content: @Composable BoxScope.() -> Unit
) {
    val shape = RoundedCornerShape(cornerRadius)
    val baseModifier = Modifier
        .size(size)
        .clip(shape)
    val frameModifier = (if (framed) {
        baseModifier
            .background(containerColor)
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
        size = ChatUiTokens.IconContainerSm,
        iconSize = ChatUiTokens.IconGlyphSm,
        framed = true,
        onClick = onClick
    )
}

@Composable
fun SectionHeader(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text.uppercase(),
        modifier = modifier,
        style = MaterialTheme.typography.labelSmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.85f)
    )
}

@Composable
fun SurfaceSectionCard(
    modifier: Modifier = Modifier,
    content: @Composable ColumnScope.() -> Unit
) {
    val borderColor = MaterialTheme.colorScheme.outline.copy(alpha = ChatUiTokens.SurfaceBorderAlpha)
    Card(
        shape = RoundedCornerShape(ChatUiTokens.CornerLarge),
        colors = CardDefaults.cardColors(
            containerColor = MaterialTheme.colorScheme.surface
        ),
        border = BorderStroke(1.dp, borderColor),
        modifier = modifier
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp, vertical = 10.dp),
            content = content
        )
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
            .height(48.dp),
        shape = RoundedCornerShape(ChatUiTokens.CornerMedium),
        colors = ButtonDefaults.buttonColors(
            containerColor = MaterialTheme.colorScheme.primary,
            contentColor = Color.White,
            disabledContainerColor = MaterialTheme.colorScheme.surfaceVariant,
            disabledContentColor = MaterialTheme.colorScheme.onSurfaceVariant
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
            .height(48.dp),
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
            .defaultMinSize(minHeight = 16.dp)
            .padding(horizontal = horizontalPadding, vertical = 0.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        content()
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
    val brush = Brush.linearGradient(
        colors = listOf(tint, tint.copy(alpha = 0.7f))
    )
    Box(
        modifier = Modifier
            .size(size)
            .clip(CircleShape)
            .background(brush),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = initials,
            style = MaterialTheme.typography.titleMedium,
            color = Color.White,
            fontWeight = FontWeight.SemiBold
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
