package mi.e2ee.android.ui

import androidx.compose.foundation.background
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
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
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
                title = { Text(title) },
                navigationIcon = {
                    if (showBackButton) {
                        IconButton(onClick = onBack) {
                            Icon(MiOwnedIcons.ArrowBack, contentDescription = "Back")
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
                .padding(horizontal = 16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            item(key = "summary") {
                SecuritySummaryCard(snapshot = snapshot, onRefresh = onRefresh)
            }
            item(key = "devices-header") {
                SectionHeader(text = tr("security_center_devices", "Devices and sessions"))
            }
            item(key = "devices-card") {
                SecurityDevicesCard(snapshot = snapshot, onKickDevice = onKickDevice)
            }
            item(key = "transport-header") {
                SectionHeader(text = tr("security_center_transport", "Transport"))
            }
            item(key = "transport-card") {
                SecurityTransportCard(
                    snapshot = snapshot,
                    onReconnect = onReconnect,
                    onCopyDevice = onCopyDevice
                )
            }
            item(key = "root-auth-header") {
                SectionHeader(text = tr("security_center_root_auth", "Root Auth"))
            }
            item(key = "root-auth-card") {
                SecurityRootAuthCard(
                    snapshot = snapshot,
                    onCopyCode = onCopyCode,
                    onCopyKey = onCopyKey,
                    onOpenRootAuthSetup = onOpenRootAuthSetup,
                    onClearRootAuth = onClearRootAuth
                )
            }
            item(key = "bottom-spacer") {
                Spacer(modifier = Modifier.height(20.dp))
            }
        }
    }
}

@Composable
private fun SecuritySummaryCard(
    snapshot: SecurityCenterSnapshot,
    onRefresh: () -> Unit
) {
    SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
        Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.Top
            ) {
                Column(
                    modifier = Modifier.weight(1f),
                    verticalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    Text(
                        text = tr("security_center_title_overline", "SECURITY CENTER"),
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Text(
                        text = tr("security_center_summary_title", "Trust, devices, and session health"),
                        style = MaterialTheme.typography.titleLarge,
                        color = MaterialTheme.colorScheme.onSurface
                    )
                    Text(
                        text = tr(
                            "security_center_summary_body",
                            "Default state stays quiet. Review transport, devices, and approval identity only when you need proof."
                        ),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                Spacer(modifier = Modifier.width(12.dp))
                UiSemanticIcon(
                    icon = MiOwnedIcons.CheckCircle,
                    contentDescription = tr("security_center_refresh", "Refresh state"),
                    tone = if (snapshot.transportHealthy) UiIconTone.Primary else UiIconTone.Warning,
                    onClick = onRefresh
                )
            }

            SecurityStatusStrip(
                title = if (snapshot.transportHealthy) {
                    tr("security_center_status_healthy", "Secure transport healthy")
                } else {
                    tr("security_center_status_review", "Transport attention required")
                },
                detail = snapshot.transportDetail,
                tone = if (snapshot.transportHealthy) UiIconTone.Primary else UiIconTone.Warning,
                icon = if (snapshot.transportHealthy) MiOwnedIcons.ShieldCheck else MiOwnedIcons.Shield
            )

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SecurityMetricTile(
                    modifier = Modifier.weight(1f),
                    icon = MiOwnedIcons.Devices,
                    label = tr("security_center_metric_device", "Current device"),
                    value = snapshot.deviceDisplayId,
                    tone = UiIconTone.Primary,
                    monospaced = true
                )
                SecurityMetricTile(
                    modifier = Modifier.weight(1f),
                    icon = MiOwnedIcons.Group,
                    label = tr("security_center_metric_linked", "Linked devices"),
                    value = tr("security_center_metric_count", "%d devices")
                        .format(snapshot.linkedDeviceCount),
                    tone = UiIconTone.Accent
                )
                SecurityMetricTile(
                    modifier = Modifier.weight(1f),
                    icon = MiOwnedIcons.Key,
                    label = tr("security_center_metric_root_auth", "Root auth"),
                    value = if (snapshot.rootAuthPubkey.isBlank()) {
                        tr("security_center_metric_not_set", "Not set")
                    } else {
                        tr("security_center_metric_active", "Active")
                    },
                    tone = if (snapshot.rootAuthPubkey.isBlank()) UiIconTone.Warning else UiIconTone.Primary
                )
            }
        }
    }
}

@Composable
private fun SecurityDevicesCard(
    snapshot: SecurityCenterSnapshot,
    onKickDevice: (String) -> Unit
) {
    SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
            if (snapshot.devices.isEmpty()) {
                SecurityStatusStrip(
                    title = tr("security_center_devices_single", "Only this device is active"),
                    detail = tr(
                        "security_center_devices_single_body",
                        "No additional linked sessions are available from the client bridge."
                    ),
                    tone = UiIconTone.Neutral,
                    icon = MiOwnedIcons.Devices
                )
            } else {
                snapshot.devices.forEachIndexed { index, device ->
                    SecurityDeviceRow(
                        device = device,
                        isCurrent = device.displayId == snapshot.deviceDisplayId,
                        onKickDevice = onKickDevice
                    )
                    if (index < snapshot.devices.lastIndex) {
                        SecurityDivider()
                    }
                }
            }
        }
    }
}

@Composable
private fun SecurityTransportCard(
    snapshot: SecurityCenterSnapshot,
    onReconnect: () -> Unit,
    onCopyDevice: () -> Unit
) {
    SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
        Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text(
                text = tr(
                    "security_center_transport_body",
                    "Chat keeps this state compact. Open this page when you need transport detail or a reconnect action."
                ),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SecurityMetricTile(
                    modifier = Modifier.weight(1f),
                    icon = MiOwnedIcons.Lock,
                    label = tr("security_center_transport_session", "Session"),
                    value = snapshot.transportLabel,
                    tone = if (snapshot.transportHealthy) UiIconTone.Primary else UiIconTone.Warning
                )
                SecurityMetricTile(
                    modifier = Modifier.weight(1f),
                    icon = MiOwnedIcons.Check,
                    label = tr("security_center_transport_alerts", "Alerts"),
                    value = if (snapshot.transportHealthy) {
                        tr("security_center_transport_none", "No alerts")
                    } else {
                        tr("security_center_transport_attention", "Needs review")
                    },
                    tone = if (snapshot.transportHealthy) UiIconTone.Accent else UiIconTone.Warning
                )
            }

            SecurityStatusStrip(
                title = tr("security_center_transport_detail", "Transport detail"),
                detail = snapshot.transportDetail,
                tone = if (snapshot.transportHealthy) UiIconTone.Primary else UiIconTone.Warning,
                icon = MiOwnedIcons.Link
            )

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SecondaryButton(
                    label = tr("security_center_reconnect", "Reconnect"),
                    modifier = Modifier.weight(1f),
                    fillMaxWidth = false,
                    onClick = onReconnect
                )
                SecondaryButton(
                    label = tr("security_center_copy_device", "Copy device ID"),
                    modifier = Modifier.weight(1f),
                    fillMaxWidth = false,
                    onClick = onCopyDevice
                )
            }
        }
    }
}

@Composable
private fun SecurityRootAuthCard(
    snapshot: SecurityCenterSnapshot,
    onCopyCode: () -> Unit,
    onCopyKey: () -> Unit,
    onOpenRootAuthSetup: () -> Unit,
    onClearRootAuth: () -> Unit
) {
    SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
        Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text(
                text = tr(
                    "security_center_root_auth_body",
                    "Root authorization remains available for device approval, but it no longer dominates the main shell."
                ),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )

            if (snapshot.rootAuthPubkey.isBlank()) {
                SecurityStatusStrip(
                    title = tr("security_center_root_auth_missing", "Root auth not configured"),
                    detail = tr(
                        "security_center_root_auth_missing_body",
                        "Initialize a signing identity before approving linked-device requests from this phone."
                    ),
                    tone = UiIconTone.Warning,
                    icon = MiOwnedIcons.Key
                )
            } else {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    SecurityMetricTile(
                        modifier = Modifier.weight(1f),
                        icon = MiOwnedIcons.QrCode,
                        label = tr("security_center_root_auth_code", "Approval code"),
                        value = snapshot.rootAuthCode.ifBlank { "------" },
                        tone = UiIconTone.Primary,
                        monospaced = true
                    )
                    SecurityMetricTile(
                        modifier = Modifier.weight(1f),
                        icon = MiOwnedIcons.Clock,
                        label = tr("security_center_root_auth_rotation", "Refresh"),
                        value = tr("security_center_root_auth_countdown", "%ds left")
                            .format(snapshot.rootCountdown),
                        tone = UiIconTone.Accent,
                        monospaced = true
                    )
                }

                Text(
                    text = shortHex(snapshot.rootAuthPubkey),
                    style = MaterialTheme.typography.bodyMedium.copy(fontFamily = FontFamily.Monospace),
                    color = MaterialTheme.colorScheme.onSurface,
                    modifier = Modifier
                        .fillMaxWidth()
                        .background(
                            MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.24f),
                            RoundedCornerShape(16.dp)
                        )
                        .padding(horizontal = 12.dp, vertical = 10.dp)
                )
            }

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                PrimaryButton(
                    label = if (snapshot.rootAuthPubkey.isBlank()) {
                        tr("security_center_root_auth_add", "Set public key")
                    } else {
                        tr("security_center_root_auth_update", "Update key")
                    },
                    modifier = Modifier.weight(1f),
                    fillMaxWidth = false,
                    onClick = onOpenRootAuthSetup
                )
                SecondaryButton(
                    label = tr("security_center_root_auth_clear", "Clear"),
                    modifier = Modifier.weight(1f),
                    fillMaxWidth = false,
                    onClick = onClearRootAuth
                )
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SecondaryButton(
                    label = tr("security_center_root_auth_copy_code", "Copy code"),
                    modifier = Modifier.weight(1f),
                    fillMaxWidth = false,
                    onClick = onCopyCode
                )
                SecondaryButton(
                    label = tr("security_center_root_auth_copy_key", "Copy key"),
                    modifier = Modifier.weight(1f),
                    fillMaxWidth = false,
                    onClick = onCopyKey
                )
            }
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
        modifier = Modifier.fillMaxWidth(),
        verticalAlignment = Alignment.CenterVertically
    ) {
        UiSemanticIcon(
            icon = MiOwnedIcons.Devices,
            contentDescription = device.displayId.ifBlank { "Device" },
            tone = if (isCurrent) UiIconTone.Primary else UiIconTone.Neutral,
            size = ChatUiTokens.IconContainerSm,
            iconSize = ChatUiTokens.IconGlyphSm
        )
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = device.displayId.ifBlank { tr("security_center_device_unknown", "Unavailable") },
                style = MaterialTheme.typography.titleMedium,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = tr("security_center_device_last_seen", "Last seen %s")
                    .format(formatSecurityLastSeen(device.lastSeenSec)),
                style = MaterialTheme.typography.bodyMedium,
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
private fun SecurityMetricTile(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    value: String,
    modifier: Modifier = Modifier,
    tone: UiIconTone = UiIconTone.Primary,
    monospaced: Boolean = false
) {
    Column(
        modifier = modifier
            .background(
                MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.18f),
                RoundedCornerShape(16.dp)
            )
            .padding(10.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        UiSemanticIcon(
            icon = icon,
            contentDescription = label,
            tone = tone,
            size = ChatUiTokens.IconContainerSm,
            iconSize = ChatUiTokens.IconGlyphSm
        )
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
            fontWeight = FontWeight.SemiBold,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
private fun SecurityStatusStrip(
    title: String,
    detail: String,
    tone: UiIconTone,
    icon: androidx.compose.ui.graphics.vector.ImageVector
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(
                MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.2f),
                RoundedCornerShape(16.dp)
            )
            .padding(12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        UiSemanticIcon(
            icon = icon,
            contentDescription = title,
            tone = tone,
            size = ChatUiTokens.IconContainerSm,
            iconSize = ChatUiTokens.IconGlyphSm
        )
        Spacer(modifier = Modifier.width(12.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(text = title, style = MaterialTheme.typography.bodyLarge)
            Text(
                text = detail,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
    }
}

@Composable
private fun SecurityDivider() {
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .height(1.dp)
            .background(MaterialTheme.colorScheme.outline.copy(alpha = 0.18f))
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

@Preview(showBackground = true, widthDp = 390, heightDp = 844)
@Composable
private fun SecurityCenterPreview() {
    ChatTheme {
        SecurityCenterPreviewScene()
    }
}
