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
    val emphasizedAlpha = if (active) 0.22f else ChatUiTokens.IconContainerAlpha
    return when (tone) {
        UiIconTone.Primary -> UiIconPalette(
            container = MaterialTheme.colorScheme.primary.copy(alpha = emphasizedAlpha),
            content = MaterialTheme.colorScheme.primary,
            border = MaterialTheme.colorScheme.primary.copy(alpha = if (active) 0.36f else 0.28f)
        )
        UiIconTone.Accent -> UiIconPalette(
            container = MaterialTheme.colorScheme.secondary.copy(alpha = emphasizedAlpha),
            content = MaterialTheme.colorScheme.secondary,
            border = MaterialTheme.colorScheme.secondary.copy(alpha = if (active) 0.36f else 0.28f)
        )
        UiIconTone.Warning -> UiIconPalette(
            container = MaterialTheme.colorScheme.tertiary.copy(alpha = emphasizedAlpha),
            content = MaterialTheme.colorScheme.tertiary,
            border = MaterialTheme.colorScheme.tertiary.copy(alpha = if (active) 0.36f else 0.28f)
        )
        UiIconTone.Danger -> UiIconPalette(
            container = MaterialTheme.colorScheme.error.copy(alpha = emphasizedAlpha),
            content = MaterialTheme.colorScheme.error,
            border = MaterialTheme.colorScheme.error.copy(alpha = if (active) 0.36f else 0.28f)
        )
        UiIconTone.Neutral -> UiIconPalette(
            container = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = if (active) 0.92f else 0.78f),
            content = MaterialTheme.colorScheme.onSurfaceVariant,
            border = MaterialTheme.colorScheme.outline.copy(alpha = if (active) 0.48f else 0.34f)
        )
    }
}

@Composable
private fun badgePalette(tone: UiBadgeTone): UiBadgePalette {
    return when (tone) {
        UiBadgeTone.Primary -> UiBadgePalette(
            container = MaterialTheme.colorScheme.primary.copy(alpha = 0.16f),
            content = MaterialTheme.colorScheme.primary,
            border = MaterialTheme.colorScheme.primary.copy(alpha = 0.32f)
        )
        UiBadgeTone.Accent -> UiBadgePalette(
            container = MaterialTheme.colorScheme.secondary.copy(alpha = 0.16f),
            content = MaterialTheme.colorScheme.secondary,
            border = MaterialTheme.colorScheme.secondary.copy(alpha = 0.32f)
        )
        UiBadgeTone.Warning -> UiBadgePalette(
            container = MaterialTheme.colorScheme.tertiary.copy(alpha = 0.16f),
            content = MaterialTheme.colorScheme.tertiary,
            border = MaterialTheme.colorScheme.tertiary.copy(alpha = 0.32f)
        )
        UiBadgeTone.Danger -> UiBadgePalette(
            container = MaterialTheme.colorScheme.error.copy(alpha = 0.16f),
            content = MaterialTheme.colorScheme.error,
            border = MaterialTheme.colorScheme.error.copy(alpha = 0.32f)
        )
        UiBadgeTone.Neutral -> UiBadgePalette(
            container = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.72f),
            content = MaterialTheme.colorScheme.onSurfaceVariant,
            border = MaterialTheme.colorScheme.outline.copy(alpha = 0.38f)
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
    onClick: (() -> Unit)? = null,
    content: @Composable BoxScope.() -> Unit
) {
    val shape = RoundedCornerShape(cornerRadius)
    val frameModifier = Modifier
        .size(size)
        .clip(shape)
        .background(containerColor)
        .border(1.dp, borderColor, shape)
        .let { base -> if (onClick != null) base.clickable { onClick() } else base }
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
    onClick: (() -> Unit)? = null
) {
    val palette = iconPalette(tone, active)
    UiIconFrame(
        modifier = modifier,
        size = size,
        cornerRadius = cornerRadius,
        containerColor = palette.container,
        borderColor = palette.border,
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
            containerColor = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.42f)
        ),
        border = BorderStroke(1.dp, borderColor),
        modifier = modifier
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
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
            .height(52.dp),
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
            style = MaterialTheme.typography.titleMedium,
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
            .height(52.dp),
        shape = RoundedCornerShape(ChatUiTokens.CornerMedium),
        onClick = onClick
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.titleMedium,
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
        borderColor = contentColor.copy(alpha = 0.28f)
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
        borderColor = contentColor.copy(alpha = 0.28f)
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
    tone: UiIconTone = UiIconTone.Neutral
) {
    val badgeTone = when (tone) {
        UiIconTone.Primary -> UiBadgeTone.Primary
        UiIconTone.Accent -> UiBadgeTone.Accent
        UiIconTone.Neutral -> UiBadgeTone.Neutral
        UiIconTone.Warning -> UiBadgeTone.Warning
        UiIconTone.Danger -> UiBadgeTone.Danger
    }
    val palette = badgePalette(badgeTone)
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
    horizontalPadding: Dp = 6.dp,
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
            .defaultMinSize(minHeight = ChatUiTokens.IconContainerXs)
            .padding(horizontal = horizontalPadding, vertical = 2.dp),
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
        modifier = modifier.size(18.dp),
        tint = color.copy(alpha = 0.85f)
    )
}

@Composable
fun AvatarBadge(initials: String, tint: Color, size: Dp = 44.dp) {
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
    Row(
        modifier = modifier
            .clip(RoundedCornerShape(ChatUiTokens.BadgeCorner))
            .background(tint.copy(alpha = 0.15f))
            .border(
                width = 1.dp,
                color = tint.copy(alpha = 0.32f),
                shape = RoundedCornerShape(ChatUiTokens.BadgeCorner)
            )
            .padding(horizontal = 10.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        StatusDot(color = tint, size = 5.dp)
        Spacer(modifier = Modifier.width(6.dp))
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = tint
        )
    }
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
