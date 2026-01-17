package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Block
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Visibility
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PrivacyScreen(
    sdk: SdkBridge,
    onBack: () -> Unit = {},
    onOpenBlockedUsers: () -> Unit = {}
) {
    val deleteAttachments = remember { mutableStateOf(true) }
    val secureWipe = remember { mutableStateOf(false) }
    val blockedCount = sdk.blockedUsers.values.count { it }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("privacy_title", "Privacy")) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                }
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            SectionHeader(text = tr("privacy_visibility", "Visibility"))
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
                    Text(
                        text = tr("privacy_last_seen", "Last seen"),
                        style = MaterialTheme.typography.bodyLarge
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    SegmentedChoice(
                        selected = sdk.privacyLastSeen,
                        onSelect = { sdk.setPrivacyLastSeen(it) }
                    )
                    Spacer(modifier = Modifier.height(12.dp))
                    Text(
                        text = tr("privacy_profile_photo", "Profile photo"),
                        style = MaterialTheme.typography.bodyLarge
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    SegmentedChoice(
                        selected = sdk.privacyProfilePhoto,
                        onSelect = { sdk.setPrivacyProfilePhoto(it) }
                    )
                    Spacer(modifier = Modifier.height(12.dp))
                    Text(
                        text = tr("privacy_group_invites", "Group invites"),
                        style = MaterialTheme.typography.bodyLarge
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    SegmentedChoice(
                        selected = sdk.privacyGroupInvites,
                        onSelect = { sdk.setPrivacyGroupInvites(it) }
                    )
                }
            }

            SectionHeader(text = tr("privacy_messaging", "Messaging"))
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth()) {
                    ToggleRow(
                        icon = Icons.Filled.Visibility,
                        title = tr("privacy_read_receipts", "Read receipts"),
                        subtitle = tr(
                            "privacy_read_receipts_sub",
                            "Allow others to see when messages are read"
                        ),
                        checked = sdk.readReceiptsEnabled,
                        onCheckedChange = { sdk.setReadReceiptsEnabled(it) }
                    )
                    if (!sdk.readReceiptsEnabled) {
                        Text(
                            text = tr(
                                "privacy_read_receipts_hint",
                                "When read receipts are off, you can't see others' read status"
                            ),
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(horizontal = 16.dp, vertical = 6.dp)
                        )
                    }
                    DividerLine()
                    ToggleRow(
                        icon = Icons.Filled.CheckCircle,
                        title = tr("privacy_screenshot_alerts", "Screenshot alerts"),
                        subtitle = tr(
                            "privacy_screenshot_alerts_sub",
                            "Notify when screenshots are taken"
                        ),
                        checked = sdk.screenshotAlertsEnabled,
                        onCheckedChange = { sdk.setScreenshotAlertsEnabled(it) }
                    )
                }
            }

            SectionHeader(text = tr("privacy_history", "History"))
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth()) {
                    ToggleRow(
                        icon = Icons.Filled.Lock,
                        title = tr("privacy_history_enabled", "Store chat history"),
                        subtitle = tr("privacy_history_enabled_sub", "Allow saving chat history locally"),
                        checked = sdk.historyEnabled,
                        onCheckedChange = { sdk.setHistoryEnabled(it) }
                    )
                    DividerLine()
                    ToggleRow(
                        icon = Icons.Filled.CheckCircle,
                        title = tr("privacy_history_delete_attachments", "Delete attachments"),
                        subtitle = tr("privacy_history_delete_attachments_sub", "Remove media when clearing history"),
                        checked = deleteAttachments.value,
                        onCheckedChange = { deleteAttachments.value = it }
                    )
                    DividerLine()
                    ToggleRow(
                        icon = Icons.Filled.Block,
                        title = tr("privacy_history_secure_wipe", "Secure wipe"),
                        subtitle = tr("privacy_history_secure_wipe_sub", "Overwrite deleted content"),
                        checked = secureWipe.value,
                        onCheckedChange = { secureWipe.value = it }
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 16.dp, vertical = 12.dp),
                        horizontalArrangement = Arrangement.End
                    ) {
                        SecondaryButton(
                            label = tr("privacy_clear_history", "Clear all history"),
                            fillMaxWidth = false,
                            onClick = {
                                sdk.clearAllHistory(deleteAttachments.value, secureWipe.value)
                            }
                        )
                    }
                }
            }

            SectionHeader(text = tr("privacy_protection", "Protection"))
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth()) {
                    SimplePrivacyRow(
                        icon = Icons.Filled.Lock,
                        title = tr("privacy_passcode_lock", "Passcode lock"),
                        subtitle = tr("account_enabled", "Enabled")
                    )
                    DividerLine()
                    SimplePrivacyRow(
                        icon = Icons.Filled.Block,
                        title = tr("privacy_blocked_users", "Blocked users"),
                        subtitle = if (blockedCount > 0) {
                            tr("privacy_blocked_count", "%d users").format(blockedCount)
                        } else {
                            tr("privacy_blocked_none", "None")
                        },
                        onClick = onOpenBlockedUsers
                    )
                }
            }
        }
    }
}

@Composable
private fun ToggleRow(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    title: String,
    subtitle: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .size(36.dp)
                .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.12f), RoundedCornerShape(10.dp)),
            contentAlignment = Alignment.Center
        ) {
            Icon(icon, contentDescription = title)
        }
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(text = title, style = MaterialTheme.typography.bodyLarge)
            Text(
                text = subtitle,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        Switch(checked = checked, onCheckedChange = onCheckedChange)
    }
}

@Composable
private fun SimplePrivacyRow(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    title: String,
    subtitle: String,
    onClick: (() -> Unit)? = null
) {
    val clickModifier = if (onClick != null) {
        Modifier.clickable { onClick() }
    } else {
        Modifier
    }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .then(clickModifier)
            .padding(horizontal = 16.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .size(36.dp)
                .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.12f), RoundedCornerShape(10.dp)),
            contentAlignment = Alignment.Center
        ) {
            Icon(icon, contentDescription = title)
        }
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(text = title, style = MaterialTheme.typography.bodyLarge)
            Text(
                text = subtitle,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
private fun SegmentedChoice(selected: String, onSelect: (String) -> Unit) {
    val options = listOf(
        "everyone" to tr("privacy_option_everyone", "Everyone"),
        "contacts" to tr("privacy_option_contacts", "Contacts"),
        "nobody" to tr("privacy_option_nobody", "Nobody")
    )
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        options.forEach { (key, option) ->
            val isSelected = key == selected
            Box(
                modifier = Modifier
                    .weight(1f)
                    .height(34.dp)
                    .background(
                        if (isSelected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                        RoundedCornerShape(12.dp)
                    )
                    .clickable { onSelect(key) },
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = option,
                    style = MaterialTheme.typography.bodyMedium,
                    color = if (isSelected) Color.White else MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
    }
}

@Composable
private fun DividerLine() {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(1.dp)
            .background(MaterialTheme.colorScheme.outline)
            .padding(horizontal = 16.dp)
    )
}

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun PrivacyPreview() {
    val context = LocalContext.current
    val sdk = remember(context) { SdkBridge(context) }
    LaunchedEffect(Unit) { sdk.init() }
    ChatTheme { PrivacyScreen(sdk = sdk) }
}
