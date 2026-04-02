package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp
import kotlin.math.max
import kotlinx.coroutines.delay

private data class SecurityCenterSnapshot(
    val transportHealthy: Boolean,
    val transportLabel: String,
    val transportDetail: String,
    val deviceDisplayId: String,
    val linkedDeviceCount: Int,
    val devices: List<DeviceUi>,
    val rootAuthPubkey: String,
    val rootAuthCode: String,
    val rootCountdown: Int
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SecurityCenterScreen(
    sdk: SdkBridge,
    title: String = tr("security_center_title", "Security Center"),
    previewMode: Boolean = false,
    onBack: () -> Unit = {}
) {
    val clipboard = LocalClipboardManager.current
    val invalidRootAuthText = tr("security_center_root_auth_invalid", "Invalid public key")
    var rootAuthPubkey by remember { mutableStateOf(sdk.rootAuthPubkey().orEmpty()) }
    var rootAuthCode by remember { mutableStateOf(sdk.currentRootAuthCode().orEmpty()) }
    var rootCountdown by remember { mutableIntStateOf(0) }
    var showRootAuthDialog by remember { mutableStateOf(false) }
    var rootAuthInput by remember { mutableStateOf("") }
    var rootAuthError by remember { mutableStateOf<String?>(null) }
    var pendingKickDeviceId by remember { mutableStateOf<String?>(null) }

    LaunchedEffect(Unit) {
        sdk.refreshDevices()
    }
    LaunchedEffect(Unit) {
        while (true) {
            val nowSec = System.currentTimeMillis() / 1000
            val step = 5
            rootCountdown = step - (nowSec % step).toInt()
            rootAuthPubkey = sdk.rootAuthPubkey().orEmpty()
            rootAuthCode = sdk.currentRootAuthCode().orEmpty()
            delay(1000)
        }
    }

    val liveSnapshot = SecurityCenterSnapshot(
        transportHealthy = sdk.remoteOk && sdk.lastError.isBlank(),
        transportLabel = when {
            sdk.remoteOk && sdk.lastError.isBlank() -> tr("security_center_transport_ok", "Encrypted")
            sdk.lastError.isNotBlank() -> tr("security_center_transport_attention", "Needs review")
            else -> tr("security_center_transport_checking", "Checking")
        },
        transportDetail = sdk.lastError.ifBlank {
            sdk.statusMessage ?: tr("security_center_transport_idle", "Client is standing by.")
        },
        deviceDisplayId = sdk.deviceDisplayId.ifBlank {
            tr("security_center_device_unknown", "Unavailable")
        },
        linkedDeviceCount = max(sdk.devices.size, if (sdk.deviceDisplayId.isBlank()) 0 else 1),
        devices = sdk.devices.toList(),
        rootAuthPubkey = rootAuthPubkey,
        rootAuthCode = rootAuthCode,
        rootCountdown = rootCountdown
    )
    val snapshot = if (previewMode && !sdk.initialized &&
        liveSnapshot.devices.isEmpty() &&
        liveSnapshot.rootAuthPubkey.isBlank()) {
        sampleSecurityCenterSnapshot()
    } else {
        liveSnapshot
    }

    SecurityCenterScreen(
        snapshot = snapshot,
        title = title,
        showBackButton = true,
        onBack = onBack,
        onRefresh = {
            sdk.heartbeat()
            sdk.refreshDevices()
            rootAuthPubkey = sdk.rootAuthPubkey().orEmpty()
            rootAuthCode = sdk.currentRootAuthCode().orEmpty()
        },
        onReconnect = { sdk.relogin() },
        onCopyDevice = { clipboard.setText(AnnotatedString(snapshot.deviceDisplayId)) },
        onCopyCode = {
            if (snapshot.rootAuthCode.isNotBlank()) {
                clipboard.setText(AnnotatedString(snapshot.rootAuthCode))
            }
        },
        onCopyKey = {
            if (snapshot.rootAuthPubkey.isNotBlank()) {
                clipboard.setText(AnnotatedString(snapshot.rootAuthPubkey))
            }
        },
        onOpenRootAuthSetup = { showRootAuthDialog = true },
        onClearRootAuth = {
            sdk.clearRootAuthPubkey()
            rootAuthPubkey = ""
            rootAuthCode = ""
        },
        onKickDevice = { deviceId -> pendingKickDeviceId = deviceId }
    )

    if (showRootAuthDialog) {
        AlertDialog(
            onDismissRequest = {
                rootAuthError = null
                rootAuthInput = ""
                showRootAuthDialog = false
            },
            title = { Text(tr("security_center_root_auth_init", "Set root auth public key")) },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedTextField(
                        value = rootAuthInput,
                        onValueChange = { rootAuthInput = it },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = {
                            Text(tr("security_center_root_auth_hint", "Enter 64-hex public key"))
                        },
                        singleLine = true
                    )
                    if (!rootAuthError.isNullOrBlank()) {
                        Text(
                            text = rootAuthError.orEmpty(),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.error
                        )
                    }
                }
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        if (sdk.initRootAuthPubkey(rootAuthInput)) {
                            rootAuthPubkey = sdk.rootAuthPubkey().orEmpty()
                            rootAuthCode = sdk.currentRootAuthCode().orEmpty()
                            rootAuthError = null
                            rootAuthInput = ""
                            showRootAuthDialog = false
                        } else {
                            rootAuthError = sdk.lastError.ifBlank { invalidRootAuthText }
                        }
                    }
                ) {
                    Text(tr("security_center_root_auth_save", "Save"))
                }
            },
            dismissButton = {
                TextButton(
                    onClick = {
                        rootAuthError = null
                        rootAuthInput = ""
                        showRootAuthDialog = false
                    }
                ) {
                    Text(tr("security_center_root_auth_cancel", "Cancel"))
                }
            }
        )
    }

    pendingKickDeviceId?.let { deviceId ->
        AlertDialog(
            onDismissRequest = { pendingKickDeviceId = null },
            title = { Text(tr("security_center_device_remove_title", "Remove linked device")) },
            text = {
                Text(
                    tr(
                        "security_center_device_remove_body",
                        "This signs the selected device out and invalidates its current session. Continue only if you trust the local device list."
                    )
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        sdk.kickDevice(deviceId)
                        pendingKickDeviceId = null
                    }
                ) {
                    Text(tr("security_center_device_remove_confirm", "Force sign out"))
                }
            },
            dismissButton = {
                TextButton(onClick = { pendingKickDeviceId = null }) {
                    Text(tr("security_center_device_remove_cancel", "Cancel"))
                }
            }
        )
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun SecurityCenterScreen(
    snapshot: SecurityCenterSnapshot,
    title: String,
    showBackButton: Boolean,
    onBack: () -> Unit,
    onRefresh: () -> Unit,
    onReconnect: () -> Unit,
    onCopyDevice: () -> Unit,
    onCopyCode: () -> Unit,
    onCopyKey: () -> Unit,
    onOpenRootAuthSetup: () -> Unit,
    onClearRootAuth: () -> Unit,
    onKickDevice: (String) -> Unit
) {
    Scaffold(
        topBar = {
            TopAppBar(
                modifier = Modifier.height(56.dp),
                title = {
                    Text(
                        text = title,
                        style = MaterialTheme.typography.titleMedium
                    )
                },
                navigationIcon = {
                    if (showBackButton) {
                        IconButton(onClick = onBack) {
                            Icon(
                                MiOwnedIcons.ArrowBack,
                                contentDescription = tr("security_center_back", "Back")
                            )
                        }
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(
                    containerColor = MaterialTheme.colorScheme.surface.copy(alpha = 0.92f),
                    titleContentColor = MaterialTheme.colorScheme.onSurface
                )
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(horizontal = 14.dp),
            contentPadding = PaddingValues(top = 8.dp, bottom = 18.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            item(key = "summary") {
                SecuritySummaryStrip(snapshot = snapshot, onRefresh = onRefresh)
            }
            item(key = "devices-header") {
                SectionHeader(text = tr("security_center_devices", "Devices and sessions"))
            }
            item(key = "devices-card") {
                SecurityDevicesSection(snapshot = snapshot, onKickDevice = onKickDevice)
            }
            item(key = "transport-trust-header") {
                SectionHeader(text = tr("security_center_transport_trust", "Transport and trust"))
            }
            item(key = "transport-trust-card") {
                SecurityTransportTrustSection(
                    snapshot = snapshot,
                    onReconnect = onReconnect,
                    onCopyDevice = onCopyDevice,
                    onOpenRootAuthSetup = onOpenRootAuthSetup,
                    onCopyCode = onCopyCode,
                    onCopyKey = onCopyKey,
                    onClearRootAuth = onClearRootAuth
                )
            }
            item(key = "bottom-spacer") {
                Spacer(modifier = Modifier.height(8.dp))
            }
        }
    }
}

@Composable
private fun SecuritySummaryStrip(
    snapshot: SecurityCenterSnapshot,
    onRefresh: () -> Unit
) {
    val toneColor = if (snapshot.transportHealthy) {
        MaterialTheme.colorScheme.primary
    } else {
        MaterialTheme.colorScheme.error
    }
    val summary = if (snapshot.transportHealthy) {
        tr("security_center_status_healthy", "Secure transport healthy")
    } else {
        tr("security_center_status_review", "Transport attention required")
    }
    val deviceSummary = tr(
        "security_center_device_count",
        "%d active devices"
    ).format(snapshot.linkedDeviceCount)
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(
                toneColor.copy(alpha = 0.08f),
                RoundedCornerShape(999.dp)
            )
            .height(40.dp)
            .padding(horizontal = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        UiTokenIcon(
            label = if (snapshot.transportHealthy) "OK" else "!",
            size = 18.dp,
            cornerRadius = 6.dp,
            containerColor = toneColor.copy(alpha = 0.10f),
            contentColor = toneColor,
            textStyle = MaterialTheme.typography.labelSmall
        )
        Spacer(modifier = Modifier.width(8.dp))
        Text(
            text = "$summary • $deviceSummary",
            modifier = Modifier.weight(1f),
            style = MaterialTheme.typography.labelLarge,
            color = MaterialTheme.colorScheme.onSurface,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis
        )
        TextButton(
            onClick = onRefresh,
            contentPadding = PaddingValues(horizontal = 6.dp, vertical = 0.dp)
        ) {
            Text(tr("security_center_refresh_short", "Refresh"))
        }
    }
}

@Composable
private fun SecurityDevicesSection(
    snapshot: SecurityCenterSnapshot,
    onKickDevice: (String) -> Unit
) {
    val currentDevice = snapshot.devices.firstOrNull { it.displayId == snapshot.deviceDisplayId }
        ?: DeviceUi("current-device", snapshot.deviceDisplayId, 0)
    val linkedDevices = snapshot.devices.filterNot { it.displayId == snapshot.deviceDisplayId }
    SecurityPrimaryGroup {
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(
                text = tr("security_center_current_device", "Current device"),
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            SecurityDeviceRow(
                device = currentDevice,
                isCurrent = true,
                onKickDevice = onKickDevice
            )
            if (linkedDevices.isNotEmpty()) {
                SecurityDivider()
                Text(
                    text = tr("security_center_linked_devices", "Linked devices"),
                    style = MaterialTheme.typography.labelSmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
                linkedDevices.forEachIndexed { index, device ->
                    SecurityDeviceRow(
                        device = device,
                        isCurrent = false,
                        onKickDevice = onKickDevice
                    )
                    if (index < linkedDevices.lastIndex) {
                        SecurityDivider()
                    }
                }
            } else {
                SecurityInfoLine(
                    label = tr("security_center_devices_single", "No linked devices"),
                    value = tr(
                        "security_center_devices_single_body",
                        "Only this phone is active right now."
                    )
                )
            }
        }
    }
}

@Composable
private fun SecurityTransportTrustSection(
    snapshot: SecurityCenterSnapshot,
    onReconnect: () -> Unit,
    onCopyDevice: () -> Unit,
    onOpenRootAuthSetup: () -> Unit,
    onCopyCode: () -> Unit,
    onCopyKey: () -> Unit,
    onClearRootAuth: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 2.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        SecurityInfoLine(
            label = tr("security_center_transport", "Transport"),
            value = snapshot.transportLabel
        )
        SecurityDivider()
        SecurityInfoLine(
            label = tr("security_center_transport_detail", "Transport detail"),
            value = snapshot.transportDetail,
            multiline = true
        )
        SecurityDivider()
        SecurityInfoLine(
            label = tr("security_center_gateway", "Gateway"),
            value = snapshot.deviceDisplayId,
            monospaced = true
        )
        SecurityDivider()
        if (snapshot.rootAuthPubkey.isBlank()) {
            SecurityInfoLine(
                label = tr("security_center_trust", "Trust"),
                value = tr(
                    "security_center_root_auth_missing_body",
                    "Initialize a signing identity before approving linked-device requests from this phone."
                ),
                multiline = true
            )
        } else {
            SecurityInfoLine(
                label = tr("security_center_root_auth_code", "Approval code"),
                value = snapshot.rootAuthCode.ifBlank { "------" },
                monospaced = true
            )
            SecurityDivider()
            SecurityInfoLine(
                label = tr("security_center_approval_identity", "Approval identity"),
                value = shortHex(snapshot.rootAuthPubkey),
                multiline = true,
                monospaced = true
            )
            SecurityDivider()
            SecurityInfoLine(
                label = tr("security_center_root_auth_rotation", "Refresh"),
                value = tr("security_center_root_auth_countdown", "%ds left")
                    .format(snapshot.rootCountdown)
            )
        }
        SecurityActionLinksRow(
            primary = tr("security_center_reconnect", "Reconnect"),
            secondary = tr("security_center_copy_device", "Copy device ID"),
            tertiary = if (snapshot.rootAuthPubkey.isBlank()) {
                tr("security_center_root_auth_add", "Set public key")
            } else {
                tr("security_center_root_auth_update", "Update key")
            },
            onPrimary = onReconnect,
            onSecondary = onCopyDevice,
            onTertiary = onOpenRootAuthSetup
        )
        if (snapshot.rootAuthPubkey.isNotBlank()) {
            SecurityActionLinksRow(
                primary = tr("security_center_root_auth_copy_code", "Copy code"),
                secondary = tr("security_center_root_auth_copy_key", "Copy key"),
                tertiary = tr("security_center_root_auth_clear", "Clear"),
                onPrimary = onCopyCode,
                onSecondary = onCopyKey,
                onTertiary = onClearRootAuth
            )
        }
    }
}

@Composable
private fun SecurityPrimaryGroup(content: @Composable () -> Unit) {
    val shape = RoundedCornerShape(ChatUiTokens.CornerLarge)
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(shape)
            .background(MaterialTheme.colorScheme.surface)
            .border(
                width = 1.dp,
                color = MaterialTheme.colorScheme.outline.copy(alpha = ChatUiTokens.SurfaceBorderAlpha),
                shape = shape
            )
            .padding(horizontal = 12.dp, vertical = 10.dp)
    ) {
        content()
    }
}

@Composable
private fun SecurityActionLinksRow(
    primary: String,
    secondary: String,
    tertiary: String,
    onPrimary: () -> Unit,
    onSecondary: () -> Unit,
    onTertiary: () -> Unit
) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        TextButton(onClick = onPrimary, modifier = Modifier.weight(1f)) {
            Text(primary)
        }
        TextButton(onClick = onSecondary, modifier = Modifier.weight(1f)) {
            Text(secondary)
        }
        TextButton(onClick = onTertiary, modifier = Modifier.weight(1f)) {
            Text(tertiary)
        }
    }
}

@Composable
private fun SecurityDeviceRow(
    device: DeviceUi,
    isCurrent: Boolean,
    onKickDevice: (String) -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        UiSemanticIcon(
            icon = MiOwnedIcons.Devices,
            contentDescription = device.displayId.ifBlank { "Device" },
            tone = UiIconTone.Primary,
            size = 32.dp,
            iconSize = 14.dp
        )
        Spacer(modifier = Modifier.width(10.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = device.displayId.ifBlank { tr("security_center_device_unknown", "Unavailable") },
                style = MaterialTheme.typography.bodyLarge,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = tr("security_center_device_last_seen", "Last seen %s")
                    .format(formatSecurityLastSeen(device.lastSeenSec)),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        if (isCurrent) {
            LabeledChip(
                label = tr("security_center_this_device", "This device"),
                tint = MaterialTheme.colorScheme.primary
            )
        } else {
            TextButton(onClick = { onKickDevice(device.deviceId) }) {
                Text(
                    text = tr("security_center_sign_out", "Force sign out"),
                    color = MaterialTheme.colorScheme.error
                )
            }
        }
    }
}

@Composable
private fun SecurityInfoLine(
    label: String,
    value: String,
    multiline: Boolean = false,
    monospaced: Boolean = false
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 2.dp)
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Text(
            text = value,
            style = MaterialTheme.typography.bodyMedium.copy(
                fontFamily = if (monospaced) FontFamily.Monospace else FontFamily.Default
            ),
            color = MaterialTheme.colorScheme.onSurface,
            maxLines = if (multiline) 4 else 1,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
private fun SecurityDivider() {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(1.dp)
            .background(MaterialTheme.colorScheme.outline.copy(alpha = 0.10f))
    )
}

@Composable
private fun formatSecurityLastSeen(seconds: Int): String {
    return when {
        seconds <= 0 -> tr("security_center_active_now", "Active now")
        seconds < 60 -> tr("security_center_active_secs", "%ds ago").format(seconds)
        seconds < 3600 -> tr("security_center_active_mins", "%dm ago").format(seconds / 60)
        seconds < 86400 -> tr("security_center_active_hours", "%dh ago").format(seconds / 3600)
        else -> tr("security_center_active_days", "%dd ago").format(seconds / 86400)
    }
}

private fun shortHex(value: String): String {
    if (value.length <= 20) {
        return value
    }
    return "${value.take(12)}...${value.takeLast(8)}"
}

private fun sampleSecurityCenterSnapshot(): SecurityCenterSnapshot {
    return SecurityCenterSnapshot(
        transportHealthy = true,
        transportLabel = "Encrypted",
        transportDetail = "Pinned transport is healthy. Last heartbeat succeeded 4s ago.",
        deviceDisplayId = "MI-A13F-7C91",
        linkedDeviceCount = 3,
        devices = listOf(
            DeviceUi("device-current", "MI-A13F-7C91", 0),
            DeviceUi("device-ipad", "Pad-F2D0-11AA", 280),
            DeviceUi("device-laptop", "Desk-9CC0-219D", 5400)
        ),
        rootAuthPubkey = "8fdca349d2aa1bc5a1e84c6b8023d4d4afba1a6f0d81291c4b27fae6837ef2a0",
        rootAuthCode = "284391",
        rootCountdown = 3
    )
}

@Composable
fun SecurityCenterPreviewScene() {
    SecurityCenterScreen(
        snapshot = sampleSecurityCenterSnapshot(),
        title = "Security Center",
        showBackButton = false,
        onBack = {},
        onRefresh = {},
        onReconnect = {},
        onCopyDevice = {},
        onCopyCode = {},
        onCopyKey = {},
        onOpenRootAuthSetup = {},
        onClearRootAuth = {},
        onKickDevice = { _ -> }
    )
}

@Preview(showBackground = true, widthDp = 412, heightDp = 915)
@Composable
private fun SecurityCenterPreview() {
    ChatTheme {
        SecurityCenterPreviewScene()
    }
}
