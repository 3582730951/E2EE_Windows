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
import androidx.compose.material.icons.filled.ChevronRight
import androidx.compose.material.icons.filled.Devices
import androidx.compose.material.icons.filled.Key
import androidx.compose.material.icons.filled.QrCode
import androidx.compose.material.icons.filled.Shield
import androidx.compose.material.icons.filled.VerifiedUser
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp


@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AccountScreen(
    sdk: SdkBridge,
    onBack: () -> Unit = {}
) {
    val pairingCode = remember { mutableStateOf<String?>(null) }
    val linkedCode = remember { mutableStateOf("") }
    val linkedStatus = remember { mutableStateOf<String?>(null) }
    val strings = LocalStrings.current
    fun t(key: String, fallback: String): String = strings.get(key, fallback)
    LaunchedEffect(Unit) {
        sdk.refreshDevices()
        sdk.pollDevicePairingRequests()
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("account_title", "Account")) },
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
            AccountHeader(
                displayName = sdk.username.ifBlank { tr("account_user_placeholder", "MI User") },
                username = sdk.username.ifBlank { "mi_user" },
                deviceId = sdk.deviceId
            )
            SectionHeader(text = tr("account_security", "Security"))
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth()) {
                    AccountRow(
                        Icons.Filled.Key,
                        tr("account_change_password", "Change password"),
                        tr("account_change_password_sub", "Last updated 12 days ago")
                    )
                    DividerLine()
                    AccountRow(
                        Icons.Filled.Shield,
                        tr("account_two_factor", "Two-factor authentication"),
                        tr("account_enabled", "Enabled")
                    )
                    DividerLine()
                    AccountRow(
                        Icons.Filled.VerifiedUser,
                        tr("account_encrypted_backup", "Encrypted backup"),
                        tr("account_enabled", "Enabled"),
                        trailing = {
                            LabeledChip(
                                label = tr("account_encrypted", "Encrypted"),
                                tint = MaterialTheme.colorScheme.primary
                            )
                        }
                    )
                }
            }
            SectionHeader(text = tr("account_identity", "Identity"))
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth()) {
                    AccountRow(
                        Icons.Filled.QrCode,
                        tr("account_my_qr", "My QR code"),
                        tr("account_my_qr_sub", "Share securely")
                    )
                }
            }
            SectionHeader(text = tr("account_devices_sessions", "Devices & sessions"))
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth()) {
                    if (sdk.devices.isEmpty()) {
                        Text(
                            text = tr("account_no_devices", "No devices"),
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.padding(16.dp)
                        )
                    } else {
                        sdk.devices.forEachIndexed { index, device ->
                            DeviceRow(
                                device = device,
                                isCurrent = device.deviceId == sdk.deviceId,
                                onKick = { sdk.kickDevice(device.deviceId) }
                            )
                            if (index < sdk.devices.lastIndex) {
                                DividerLine()
                            }
                        }
                    }
                }
            }
            SectionHeader(text = tr("account_pairing", "Device pairing"))
            Card(
                shape = RoundedCornerShape(20.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(modifier = Modifier.fillMaxWidth().padding(16.dp)) {
                    Text(text = tr("account_pair_primary", "Pair a new device"), style = MaterialTheme.typography.bodyLarge)
                    Spacer(modifier = Modifier.height(8.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        FilledTonalButton(
                            onClick = {
                                pairingCode.value = sdk.beginDevicePairingPrimary()
                                sdk.pollDevicePairingRequests()
                            },
                            colors = ButtonDefaults.filledTonalButtonColors(
                                containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.12f),
                                contentColor = MaterialTheme.colorScheme.primary
                            )
                        ) {
                            Text(tr("account_pair_generate", "Generate code"))
                        }
                        FilledTonalButton(
                            onClick = { sdk.pollDevicePairingRequests() },
                            colors = ButtonDefaults.filledTonalButtonColors(
                                containerColor = MaterialTheme.colorScheme.secondary.copy(alpha = 0.12f),
                                contentColor = MaterialTheme.colorScheme.secondary
                            )
                        ) {
                            Text(tr("account_pair_refresh", "Refresh"))
                        }
                    }
                    if (!pairingCode.value.isNullOrBlank()) {
                        Spacer(modifier = Modifier.height(8.dp))
                        LabeledChip(
                            label = tr("account_pair_code", "Code: %s").format(pairingCode.value),
                            tint = MaterialTheme.colorScheme.primary
                        )
                    }
                    if (sdk.pairingRequests.isNotEmpty()) {
                        Spacer(modifier = Modifier.height(12.dp))
                        Text(text = tr("account_pair_requests", "Pending requests"), style = MaterialTheme.typography.bodyMedium)
                        Spacer(modifier = Modifier.height(6.dp))
                        sdk.pairingRequests.forEach { request ->
                            Row(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(vertical = 6.dp),
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text(
                                    text = request.deviceId,
                                    style = MaterialTheme.typography.bodyMedium,
                                    modifier = Modifier.weight(1f)
                                )
                                TextButton(
                                    onClick = { sdk.approveDevicePairingRequest(request.deviceId, request.requestId) }
                                ) {
                                    Text(tr("account_pair_approve", "Approve"))
                                }
                            }
                        }
                    }
                    Spacer(modifier = Modifier.height(12.dp))
                    Text(text = tr("account_pair_linked", "Link this device"), style = MaterialTheme.typography.bodyLarge)
                    Spacer(modifier = Modifier.height(8.dp))
                    OutlinedTextField(
                        value = linkedCode.value,
                        onValueChange = { linkedCode.value = it },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = { Text(tr("account_pair_code_hint", "Enter pairing code")) },
                        shape = RoundedCornerShape(16.dp),
                        singleLine = true
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        FilledTonalButton(
                            onClick = {
                                if (linkedCode.value.isNotBlank()) {
                                    val ok = sdk.beginDevicePairingLinked(linkedCode.value.trim())
                                    linkedStatus.value = if (ok) {
                                        t("account_pair_linking", "Linking...")
                                    } else {
                                        t("account_pair_failed", "Failed")
                                    }
                                }
                            },
                            enabled = linkedCode.value.isNotBlank(),
                            colors = ButtonDefaults.filledTonalButtonColors(
                                containerColor = MaterialTheme.colorScheme.primary.copy(alpha = 0.12f),
                                contentColor = MaterialTheme.colorScheme.primary
                            )
                        ) {
                            Text(tr("account_pair_start", "Start"))
                        }
                        FilledTonalButton(
                            onClick = {
                                val ok = sdk.pollDevicePairingLinked()
                                linkedStatus.value = if (ok) {
                                    t("account_pair_done", "Linked")
                                } else {
                                    t("account_pair_pending", "Pending")
                                }
                            },
                            colors = ButtonDefaults.filledTonalButtonColors(
                                containerColor = MaterialTheme.colorScheme.secondary.copy(alpha = 0.12f),
                                contentColor = MaterialTheme.colorScheme.secondary
                            )
                        ) {
                            Text(tr("account_pair_check", "Check"))
                        }
                        FilledTonalButton(
                            onClick = { sdk.cancelDevicePairing() },
                            colors = ButtonDefaults.filledTonalButtonColors(
                                containerColor = MaterialTheme.colorScheme.error.copy(alpha = 0.12f),
                                contentColor = MaterialTheme.colorScheme.error
                            )
                        ) {
                            Text(tr("account_pair_cancel", "Cancel"))
                        }
                    }
                    if (!linkedStatus.value.isNullOrBlank()) {
                        Spacer(modifier = Modifier.height(8.dp))
                        Text(
                            text = linkedStatus.value.orEmpty(),
                            style = MaterialTheme.typography.labelSmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                }
            }
            SectionHeader(text = tr("account_sign_out", "Sign out"))
            PrimaryButton(
                label = tr("account_sign_out", "Sign out"),
                onClick = { sdk.logout() }
            )
        }
    }
}

@Composable
private fun AccountHeader(
    displayName: String,
    username: String,
    deviceId: String
) {
    Card(
        shape = RoundedCornerShape(24.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            AvatarBadge(initials = displayName.take(2).uppercase(), tint = MaterialTheme.colorScheme.primary, size = 62.dp)
            Spacer(modifier = Modifier.width(16.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(text = displayName, style = MaterialTheme.typography.titleLarge)
                Text(
                    text = username,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Spacer(modifier = Modifier.height(6.dp))
                if (deviceId.isNotBlank()) {
                    LabeledChip(
                        label = tr("account_device_id", "Device: %s").format(deviceId),
                        tint = MaterialTheme.colorScheme.primary
                    )
                }
            }
            Icon(Icons.Filled.ChevronRight, contentDescription = "Edit")
        }
    }
}

@Composable
private fun AccountRow(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    title: String,
    subtitle: String,
    trailing: @Composable () -> Unit = { Icon(Icons.Filled.ChevronRight, contentDescription = "Open") }
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
        trailing()
    }
}

@Composable
private fun DeviceRow(
    device: DeviceUi,
    isCurrent: Boolean,
    onKick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .size(36.dp)
                .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.12f), RoundedCornerShape(10.dp)),
            contentAlignment = Alignment.Center
        ) {
            Icon(Icons.Filled.Devices, contentDescription = device.deviceId)
        }
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(text = device.deviceId, style = MaterialTheme.typography.bodyLarge)
            Text(
                text = tr("account_device_last_seen", "Last seen %s").format(formatLastSeen(device.lastSeenSec)),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Text(
                text = if (isCurrent) tr("account_this_device", "This device") else tr("account_other_device", "Other device"),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        if (isCurrent) {
            LabeledChip(label = tr("account_this_device", "This device"), tint = MaterialTheme.colorScheme.primary)
        } else {
            Text(
                text = tr("account_force_sign_out", "Force sign out"),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.error,
                modifier = Modifier.clickable { onKick() }
            )
        }
    }
}

@Composable
private fun formatLastSeen(seconds: Int): String {
    return when {
        seconds <= 0 -> tr("account_active_now", "Active now")
        seconds < 60 -> tr("account_active_secs", "%ds ago").format(seconds)
        seconds < 3600 -> tr("account_active_mins", "%dm ago").format(seconds / 60)
        seconds < 86400 -> tr("account_active_hours", "%dh ago").format(seconds / 3600)
        else -> tr("account_active_days", "%dd ago").format(seconds / 86400)
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
private fun AccountPreview() {
    val context = LocalContext.current
    val sdk = remember(context) { SdkBridge(context) }
    LaunchedEffect(Unit) { sdk.init() }
    ChatTheme { AccountScreen(sdk = sdk) }
}
