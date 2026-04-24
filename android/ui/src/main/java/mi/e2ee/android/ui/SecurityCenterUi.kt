package mi.e2ee.android.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
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
import androidx.compose.ui.platform.testTag
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
    val transportTone: SecurityStateTone,
    val transportLabel: String,
    val transportDetail: String,
    val deviceDisplayId: String,
    val linkedDeviceCount: Int,
    val devices: List<DeviceUi>,
    val rootAuthPubkey: String,
    val rootAuthCode: String,
    val rootCountdown: Int
)

@Composable
fun SecurityCenterScreen(
    sdk: SdkBridge,
    title: String = tr("security_center_title", "Security Center"),
    previewMode: Boolean = false,
    onBack: () -> Unit = {}
) {
    val bridgeEnabled = !previewMode
    val sampleTransportLabel = tr("security_center_transport_ok", "Encrypted")
    val sampleTransportDetail = tr("security_center_active_secs", "%ds ago").format(4)
    val sampleSnapshot = remember(sampleTransportLabel, sampleTransportDetail) {
        sampleSecurityCenterSnapshot(
            transportLabel = sampleTransportLabel,
            transportDetail = sampleTransportDetail
        )
    }
    val clipboard = LocalClipboardManager.current
    val invalidRootAuthText = tr("security_center_root_auth_invalid", "Invalid public key")
    var rootAuthPubkey by remember(previewMode) {
        mutableStateOf(
            if (previewMode) sampleSnapshot.rootAuthPubkey else sdk.rootAuthPubkey().orEmpty()
        )
    }
    var rootAuthCode by remember(previewMode) {
        mutableStateOf(
            if (previewMode) sampleSnapshot.rootAuthCode else sdk.currentRootAuthCode().orEmpty()
        )
    }
    var rootCountdown by remember(previewMode) {
        mutableIntStateOf(
            if (previewMode) sampleSnapshot.rootCountdown else 0
        )
    }
    var showRootAuthDialog by remember { mutableStateOf(false) }
    var rootAuthInput by remember { mutableStateOf("") }
    var rootAuthError by remember { mutableStateOf<String?>(null) }
    var pendingKickDeviceId by remember { mutableStateOf<String?>(null) }

    LaunchedEffect(bridgeEnabled) {
        if (bridgeEnabled) {
            sdk.refreshDevices()
        }
    }
    LaunchedEffect(bridgeEnabled, previewMode) {
        if (previewMode) {
            rootAuthPubkey = sampleSnapshot.rootAuthPubkey
            rootAuthCode = sampleSnapshot.rootAuthCode
            rootCountdown = sampleSnapshot.rootCountdown
            return@LaunchedEffect
        }
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
        transportTone = when {
            sdk.remoteOk && sdk.lastError.isBlank() -> SecurityStateTone.Healthy
            sdk.lastError.isNotBlank() -> SecurityStateTone.Review
            else -> SecurityStateTone.Checking
        },
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
    val snapshot = if (previewMode) sampleSnapshot else liveSnapshot

    SecurityCenterScreen(
        snapshot = snapshot,
        title = title,
        showBackButton = true,
        onBack = onBack,
        onRefresh = {
            if (bridgeEnabled) {
                sdk.heartbeat()
                sdk.refreshDevices()
                rootAuthPubkey = sdk.rootAuthPubkey().orEmpty()
                rootAuthCode = sdk.currentRootAuthCode().orEmpty()
            }
        },
        onReconnect = {
            if (bridgeEnabled) {
                sdk.relogin()
            }
        },
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
            if (bridgeEnabled) {
                sdk.clearRootAuthPubkey()
                rootAuthPubkey = ""
                rootAuthCode = ""
            } else {
                rootAuthPubkey = sampleSnapshot.rootAuthPubkey
                rootAuthCode = sampleSnapshot.rootAuthCode
            }
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
                        if (!bridgeEnabled) {
                            rootAuthError = null
                            rootAuthInput = ""
                            showRootAuthDialog = false
                        } else if (sdk.initRootAuthPubkey(rootAuthInput)) {
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
                        "This signs the device out immediately."
                    )
                )
            },
            confirmButton = {
                TextButton(
                    onClick = {
                        if (bridgeEnabled) {
                            sdk.kickDevice(deviceId)
                        }
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
    val colors = phaseOneColors()
    Scaffold(
        topBar = {
            // CenterAlignedTopAppBar(
            GlassTopAppBar(
                leadingContent = if (showBackButton) {
                    {
                        UiToolbarIconButton(
                            icon = MiOwnedIcons.ArrowBack,
                            contentDescription = tr("security_center_back", "Back"),
                            onClick = onBack
                        )
                    }
                } else {
                    null
                },
                actions = {
                    UiToolbarIconButton(
                        icon = MiOwnedIcons.Refresh,
                        contentDescription = tr("security_center_refresh_short", "Refresh"),
                        tone = UiIconTone.Primary,
                        onClick = onRefresh
                    )
                },
                titleContent = {
                    Text(
                        text = title,
                        modifier = Modifier.padding(
                            top = PhaseOneTokens.LargeTitleTopPadding,
                            bottom = PhaseOneTokens.LargeTitleBottomPadding
                        ),
                        style = phaseOneLargeTitleTextStyle(),
                        color = colors.onSurface
                    )
                    Text(
                        text = tr("security_center_subtitle", "Transport, device trust, and approval protection"),
                        style = MaterialTheme.typography.bodySmall,
                        color = colors.onSurfaceMuted
                    )
                }
            )
        },
        containerColor = colors.background
    ) { padding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .background(colors.background)
                .padding(horizontal = 12.dp)
                .testTag("security-center-screen"),
            contentPadding = PaddingValues(top = 10.dp, bottom = 28.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            item(key = "security-summary") {
                SecuritySummaryStrip(
                    snapshot = snapshot,
                    onRefresh = onRefresh
                )
            }
            item(key = "security-overview") {
                SecurityOverviewSection(
                    snapshot = snapshot,
                    onCopyDevice = onCopyDevice,
                    onCopyCode = onCopyCode,
                    onCopyKey = onCopyKey,
                    onClearRootAuth = onClearRootAuth,
                    onKickDevice = onKickDevice,
                    onOpenRootAuthSetup = onOpenRootAuthSetup,
                    onReconnect = onReconnect
                )
            }
        }
    }
}

@Composable
private fun SecuritySummaryStrip(
    snapshot: SecurityCenterSnapshot,
    onRefresh: () -> Unit
) {
    val summary = when (snapshot.transportTone) {
        SecurityStateTone.Healthy -> tr("security_center_status_healthy", "Secure transport healthy")
        SecurityStateTone.Checking -> tr("security_center_transport_checking", "Checking")
        SecurityStateTone.Review -> tr("security_center_status_review", "Transport attention required")
        SecurityStateTone.Blocked -> tr("security_center_transport_attention", "Needs review")
    }
    val colors = phaseOneColors()
    Surface(
        shape = RoundedCornerShape(18.dp),
        color = colors.surface,
        border = androidx.compose.foundation.BorderStroke(1.dp, colors.cardBorder)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 14.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            UiSemanticIcon(
                icon = MiOwnedIcons.ShieldCheck,
                contentDescription = summary,
                tone = when (snapshot.transportTone) {
                    SecurityStateTone.Healthy -> UiIconTone.Accent
                    SecurityStateTone.Checking -> UiIconTone.Primary
                    SecurityStateTone.Review -> UiIconTone.Warning
                    SecurityStateTone.Blocked -> UiIconTone.Danger
                },
                size = 36.dp,
                iconSize = 16.dp
            )
            Spacer(modifier = Modifier.width(12.dp))
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = summary,
                    style = MaterialTheme.typography.bodyLarge.copy(fontWeight = FontWeight.SemiBold),
                    color = colors.onSurface
                )
                Text(
                    text = snapshot.transportDetail,
                    style = MaterialTheme.typography.bodySmall,
                    color = colors.onSurfaceMuted
                )
            }
            UiToolbarIconButton(
                icon = MiOwnedIcons.Refresh,
                contentDescription = tr("security_center_refresh_short", "Refresh"),
                tone = UiIconTone.Primary,
                onClick = onRefresh
            )
        }
    }
}

@Composable
private fun SecurityOverviewSection(
    snapshot: SecurityCenterSnapshot,
    onCopyDevice: () -> Unit,
    onCopyCode: () -> Unit,
    onCopyKey: () -> Unit,
    onClearRootAuth: () -> Unit,
    onKickDevice: (String) -> Unit,
    onOpenRootAuthSetup: () -> Unit,
    onReconnect: () -> Unit
) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        SectionHeader(
            text = tr("security_center_overview_section", "Overview"),
            modifier = Modifier.padding(start = 6.dp)
        )
        InsetGroupedCard {
            SecurityNavRow(
                icon = MiOwnedIcons.Devices,
                label = tr("security_center_current_device", "Current device"),
                detail = "",
                tone = SecurityStateTone.Healthy,
                onClick = onCopyDevice
            )
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(start = 56.dp)
                    .height(1.dp)
                    .background(phaseOneColors().outline.copy(alpha = 0.28f))
            )
            SecurityNavRow(
                icon = MiOwnedIcons.Devices,
                label = tr("settings_devices", "Devices"),
                detail = "",
                tone = if (snapshot.linkedDeviceCount > 0) SecurityStateTone.Healthy else SecurityStateTone.Checking,
                onClick = { if (snapshot.devices.isNotEmpty()) onKickDevice(snapshot.devices.first().deviceId) }
            )
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(start = 56.dp)
                    .height(1.dp)
                    .background(phaseOneColors().outline.copy(alpha = 0.28f))
            )
            SecurityNavRow(
                icon = MiOwnedIcons.Shield,
                label = tr("security_center_transport", "Transport"),
                detail = "",
                tone = snapshot.transportTone,
                onClick = onReconnect
            )
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(start = 56.dp)
                    .height(1.dp)
                    .background(phaseOneColors().outline.copy(alpha = 0.28f))
            )
            SecurityNavRow(
                icon = if (snapshot.rootAuthPubkey.isBlank()) MiOwnedIcons.Lock else MiOwnedIcons.ShieldCheck,
                label = tr("security_center_approval_guard", "Approval protection"),
                detail = "",
                tone = if (snapshot.rootAuthPubkey.isBlank()) SecurityStateTone.Review else SecurityStateTone.Healthy,
                onClick = onOpenRootAuthSetup
            )
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(start = 56.dp)
                    .height(1.dp)
                    .background(phaseOneColors().outline.copy(alpha = 0.28f))
            )
            SecurityNavRow(
                icon = if (snapshot.rootAuthPubkey.isBlank()) MiOwnedIcons.Lock else MiOwnedIcons.ShieldCheck,
                label = tr("security_center_root_auth_short", "Root auth"),
                detail = "",
                tone = if (snapshot.rootAuthPubkey.isBlank()) SecurityStateTone.Review else SecurityStateTone.Healthy,
                onClick = if (snapshot.rootAuthPubkey.isBlank()) onOpenRootAuthSetup else onCopyKey
            )
        }
        if (snapshot.rootAuthCode.isNotBlank()) {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                PrimaryButton(
                    label = tr("security_center_root_auth_init", "Set root auth public key"),
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
        }
    }
}

@Composable
private fun SecurityNavRow(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    detail: String,
    tone: SecurityStateTone = SecurityStateTone.Checking,
    onClick: (() -> Unit)? = null
) {
    val colors = phaseOneColors()
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .then(if (onClick != null) Modifier.clickable(onClick = onClick) else Modifier)
            .clip(RoundedCornerShape(16.dp))
            .padding(horizontal = 12.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        UiSemanticIcon(
            icon = icon,
            contentDescription = label,
            tone = when (tone) {
                SecurityStateTone.Healthy -> UiIconTone.Accent
                SecurityStateTone.Checking -> UiIconTone.Primary
                SecurityStateTone.Review -> UiIconTone.Warning
                SecurityStateTone.Blocked -> UiIconTone.Danger
            },
            size = 34.dp,
            iconSize = 16.dp
        )
        Spacer(modifier = Modifier.width(10.dp))
        Column(modifier = Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Text(
                text = label,
                style = MaterialTheme.typography.bodyLarge,
                color = colors.onSurface,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            if (detail.isNotBlank()) {
                Text(
                    text = detail,
                    style = MaterialTheme.typography.bodySmall,
                    color = colors.onSurfaceMuted,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis
                )
            }
        }
        Icon(
            imageVector = MiOwnedIcons.ChevronRight,
            contentDescription = null,
            tint = colors.onSurfaceMuted,
            modifier = Modifier.size(16.dp)
        )
    }
}

@Composable
private fun SecurityDeviceRow(
    device: DeviceUi,
    isCurrent: Boolean,
    index: Int,
    onKickDevice: (String) -> Unit,
    showDivider: Boolean
) {
    val colors = phaseOneColors()
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 14.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        IdentityAvatar(
            label = device.displayId.ifBlank { "Device" },
            seed = device.deviceId,
            kind = IdentityAvatarKind.Device,
            size = 40.dp
        )
        Spacer(modifier = Modifier.width(10.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = securityFriendlyDeviceName(device.displayId, isCurrent, index),
                style = MaterialTheme.typography.bodyLarge.copy(fontWeight = FontWeight.SemiBold),
                color = colors.onSurface,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = formatSecurityLastSeen(device.lastSeenSec),
                style = MaterialTheme.typography.bodySmall,
                color = colors.onSurfaceMuted
            )
        }
        if (isCurrent) {
            Text(
                text = tr("security_center_this_device", "This device"),
                style = MaterialTheme.typography.labelMedium,
                color = colors.accent
            )
        } else {
            TextButton(onClick = { onKickDevice(device.deviceId) }) {
                Text(tr("security_center_device_remove_confirm", "Force sign out"))
            }
        }
    }
    if (showDivider) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .padding(start = 64.dp)
                .height(1.dp)
                .background(colors.outline.copy(alpha = 0.28f))
        )
    }
}

@Composable
private fun SecurityFactCard(
    label: String,
    value: String,
    detail: String,
    tone: SecurityStateTone = SecurityStateTone.Checking,
    multiline: Boolean = false,
    monospaced: Boolean = false,
    onClick: (() -> Unit)? = null
) {
    val colors = phaseOneColors()
    SurfaceSectionCard(
        modifier = Modifier
            .fillMaxWidth()
            .then(if (onClick != null) Modifier.clickable(onClick = onClick) else Modifier)
    ) {
        Column(
            modifier = Modifier.fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                MediaHintChip(
                    kind = MediaHintKind.Link,
                    label = label
                )
                Spacer(modifier = Modifier.width(8.dp))
                UiStatusCountBadge(
                    label = when (tone) {
                        SecurityStateTone.Healthy -> tr("security_state_healthy", "Healthy")
                        SecurityStateTone.Checking -> tr("security_state_checking", "Checking")
                        SecurityStateTone.Review -> tr("security_state_review", "Review")
                        SecurityStateTone.Blocked -> tr("security_state_blocked", "Blocked")
                    },
                    tone = when (tone) {
                        SecurityStateTone.Healthy -> UiBadgeTone.Accent
                        SecurityStateTone.Checking -> UiBadgeTone.Primary
                        SecurityStateTone.Review -> UiBadgeTone.Warning
                        SecurityStateTone.Blocked -> UiBadgeTone.Danger
                    }
                )
            }
            Text(
                text = value,
                style = MaterialTheme.typography.bodyLarge.copy(
                    fontFamily = if (monospaced) FontFamily.Monospace else FontFamily.Default
                ),
                color = colors.onSurface,
                maxLines = if (multiline) 4 else 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = detail,
                style = MaterialTheme.typography.bodySmall,
                color = colors.onSurfaceMuted,
                maxLines = if (multiline) 3 else 2,
                overflow = TextOverflow.Ellipsis
            )
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

@Composable
private fun securityFriendlyDeviceName(displayId: String, isCurrent: Boolean, index: Int): String {
    if (isCurrent) {
        return tr("security_center_device_phone", "This phone")
    }
    val lowered = displayId.lowercase()
    return when {
        lowered.contains("tablet") || lowered.contains("pad") -> tr("security_center_device_tablet", "Tablet")
        lowered.contains("desktop") || lowered.contains("desk") || lowered.contains("laptop") -> tr("security_center_device_desktop", "Desktop")
        else -> tr("security_center_device_linked", "Linked device %d").format(index)
    }
}

private fun shortHex(value: String): String {
    if (value.length <= 20) {
        return value
    }
    return "${value.take(12)}...${value.takeLast(8)}"
}

private fun sampleSecurityCenterSnapshot(
    transportLabel: String,
    transportDetail: String
): SecurityCenterSnapshot {
    return SecurityCenterSnapshot(
        transportHealthy = true,
        transportTone = SecurityStateTone.Healthy,
        transportLabel = transportLabel,
        transportDetail = transportDetail,
        deviceDisplayId = "phone-current",
        linkedDeviceCount = 3,
        devices = listOf(
            DeviceUi("device-current", "phone-current", 0),
            DeviceUi("device-ipad", "tablet-linked", 280),
            DeviceUi("device-laptop", "desktop-linked", 5400)
        ),
        rootAuthPubkey = "ready",
        rootAuthCode = "",
        rootCountdown = 3
    )
}

@Composable
fun SecurityCenterPreviewScene() {
    val sampleTransportLabel = tr("security_center_transport_ok", "Encrypted")
    val sampleTransportDetail = tr("security_center_active_secs", "%ds ago").format(4)
    SecurityCenterScreen(
        snapshot = sampleSecurityCenterSnapshot(
            transportLabel = sampleTransportLabel,
            transportDetail = sampleTransportDetail
        ),
        title = tr("security_center_title", "Security Center"),
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
