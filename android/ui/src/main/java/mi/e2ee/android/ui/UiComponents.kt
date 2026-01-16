package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
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
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp

@Composable
fun SectionHeader(text: String, modifier: Modifier = Modifier) {
    Text(
        text = text.uppercase(),
        modifier = modifier,
        style = MaterialTheme.typography.labelSmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant
    )
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
        shape = RoundedCornerShape(16.dp),
        colors = ButtonDefaults.buttonColors(
            containerColor = MaterialTheme.colorScheme.primary,
            contentColor = Color.White,
            disabledContainerColor = MaterialTheme.colorScheme.surfaceVariant,
            disabledContentColor = MaterialTheme.colorScheme.onSurfaceVariant
        ),
        enabled = enabled,
        onClick = onClick
    ) {
        Text(text = label, style = MaterialTheme.typography.titleMedium)
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
        shape = RoundedCornerShape(16.dp),
        onClick = onClick
    ) {
        Text(text = label, style = MaterialTheme.typography.titleMedium)
    }
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
            .clip(RoundedCornerShape(14.dp))
            .background(tint.copy(alpha = 0.15f))
            .padding(horizontal = 10.dp, vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        StatusDot(color = tint, size = 6.dp)
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
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Text(
                text = rightLink,
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.primary,
                textDecoration = TextDecoration.Underline,
                modifier = Modifier.clickable { onRightClick() }
            )
        }
    }
}
