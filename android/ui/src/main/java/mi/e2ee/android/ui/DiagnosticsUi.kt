package mi.e2ee.android.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import mi.e2ee.android.sdk.MediaConfig
import mi.e2ee.android.sdk.MediaPacket

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun DiagnosticsScreen(
    sdk: SdkBridge,
    onBack: () -> Unit = {}
) {
    val timeFormat = remember { SimpleDateFormat("HH:mm:ss", Locale.getDefault()) }
    var remoteMode by remember { mutableStateOf<Boolean?>(null) }
    var mediaConfig by remember { mutableStateOf<MediaConfig?>(null) }
    var mediaConfigResult by remember { mutableStateOf("") }

    var subCallIdHex by remember { mutableStateOf("") }
    var subIsGroup by remember { mutableStateOf(false) }
    var subGroupId by remember { mutableStateOf("") }
    var subResult by remember { mutableStateOf("") }

    var presenceUser by remember { mutableStateOf("") }
    var presenceOnline by remember { mutableStateOf(true) }
    var presenceResult by remember { mutableStateOf("") }

    var receiptUser by remember { mutableStateOf("") }
    var receiptMessageId by remember { mutableStateOf("") }
    var receiptResult by remember { mutableStateOf("") }

    var signalGroupId by remember { mutableStateOf("") }
    var signalCallIdHex by remember { mutableStateOf("") }
    var signalOp by remember { mutableStateOf("1") }
    var signalKeyId by remember { mutableStateOf("1") }
    var signalSeq by remember { mutableStateOf("0") }
    var signalTsMs by remember { mutableStateOf(System.currentTimeMillis().toString()) }
    var signalVideo by remember { mutableStateOf(false) }
    var signalExtHex by remember { mutableStateOf("") }
    var signalResult by remember { mutableStateOf("") }

    var keyGroupId by remember { mutableStateOf("") }
    var keyCallIdHex by remember { mutableStateOf("") }
    var keyId by remember { mutableStateOf("1") }
    var keyMembers by remember { mutableStateOf("") }
    var keyResult by remember { mutableStateOf("") }

    var peerCallIdHex by remember { mutableStateOf("") }
    var peerUsername by remember { mutableStateOf("") }
    var peerPayloadHex by remember { mutableStateOf("") }
    var peerPullMaxPackets by remember { mutableStateOf("32") }
    var peerPullWaitMs by remember { mutableStateOf("0") }
    var peerMediaResult by remember { mutableStateOf("") }

    var groupCallIdHex by remember { mutableStateOf("") }
    var groupIdForMedia by remember { mutableStateOf("") }
    var groupPayloadHex by remember { mutableStateOf("") }
    var groupPullMaxPackets by remember { mutableStateOf("64") }
    var groupPullWaitMs by remember { mutableStateOf("0") }
    var groupMediaResult by remember { mutableStateOf("") }

    var rootPeer by remember { mutableStateOf("") }
    var rootCallIdHex by remember { mutableStateOf("") }
    var rootResult by remember { mutableStateOf("") }

    var previewFileId by remember { mutableStateOf("") }
    var previewFileName by remember { mutableStateOf("") }
    var previewFileSize by remember { mutableStateOf("") }
    var previewPayloadHex by remember { mutableStateOf("") }
    var previewResult by remember { mutableStateOf("") }

    var downloadFileId by remember { mutableStateOf("") }
    var downloadFileName by remember { mutableStateOf("") }
    var downloadFileSize by remember { mutableStateOf("") }
    var downloadFileKeyHex by remember { mutableStateOf("") }
    var downloadWipe by remember { mutableStateOf(false) }
    var downloadResult by remember { mutableStateOf("") }

    var resendTarget by remember { mutableStateOf("") }
    var resendMessageId by remember { mutableStateOf("") }
    var resendText by remember { mutableStateOf("") }
    var resendReplyTo by remember { mutableStateOf("") }
    var resendReplyPreview by remember { mutableStateOf("") }
    var resendIsGroup by remember { mutableStateOf(false) }
    var resendTextResult by remember { mutableStateOf("") }

    var resendFileTarget by remember { mutableStateOf("") }
    var resendFileMessageId by remember { mutableStateOf("") }
    var resendFilePath by remember { mutableStateOf("") }
    var resendFileIsGroup by remember { mutableStateOf(false) }
    var resendFileResult by remember { mutableStateOf("") }

    var resendStickerTarget by remember { mutableStateOf("") }
    var resendStickerMessageId by remember { mutableStateOf("") }
    var resendStickerId by remember { mutableStateOf("") }
    var resendStickerResult by remember { mutableStateOf("") }

    var resendLocationTarget by remember { mutableStateOf("") }
    var resendLocationMessageId by remember { mutableStateOf("") }
    var resendLocationLabel by remember { mutableStateOf("") }
    var resendLocationLat by remember { mutableStateOf("") }
    var resendLocationLon by remember { mutableStateOf("") }
    var resendLocationResult by remember { mutableStateOf("") }

    var resendContactTarget by remember { mutableStateOf("") }
    var resendContactMessageId by remember { mutableStateOf("") }
    var resendContactUsername by remember { mutableStateOf("") }
    var resendContactDisplay by remember { mutableStateOf("") }
    var resendContactResult by remember { mutableStateOf("") }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(tr("diagnostics_title", "Diagnostics")) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                }
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { padding ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            item {
                SectionHeader(text = tr("diagnostics_sdk_info", "SDK info"))
                DiagnosticCard {
                    val versionLabel = sdk.sdkVersion?.let {
                        "${it.major}.${it.minor}.${it.patch} (abi ${it.abi})"
                    } ?: tr("diagnostics_unknown", "Unknown")
                    InfoRow(label = tr("diagnostics_version", "Version"), value = versionLabel)
                    InfoRow(label = tr("diagnostics_caps", "Capabilities"), value = sdk.capabilities.toString())
                    InfoRow(label = tr("diagnostics_device_id", "Device id"), value = sdk.deviceId.ifBlank { "-" })
                    InfoRow(label = tr("diagnostics_token", "Token"), value = sdk.token.ifBlank { "-" })
                    InfoRow(
                        label = tr("diagnostics_remote_status", "Remote status"),
                        value = if (sdk.remoteOk) tr("diagnostics_online", "Online") else {
                            tr("diagnostics_offline", "Offline: %s").format(sdk.remoteError.ifBlank { "-" })
                        }
                    )
                    InfoRow(
                        label = tr("diagnostics_remote_mode", "Remote mode"),
                        value = remoteMode?.let { if (it) "Remote" else "Local" }
                            ?: tr("diagnostics_unknown", "Unknown")
                    )
                    if (sdk.lastError.isNotBlank()) {
                        InfoRow(label = tr("diagnostics_last_error", "Last error"), value = sdk.lastError)
                    }
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        SecondaryButton(
                            label = tr("diagnostics_check_remote_mode", "Check remote mode"),
                            fillMaxWidth = false,
                            onClick = { remoteMode = sdk.isRemoteMode() }
                        )
                    }
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_media_config", "Media config"))
                DiagnosticCard {
                    if (mediaConfig == null) {
                        Text(
                            text = if (mediaConfigResult.isBlank()) {
                                tr("diagnostics_media_config_empty", "Not loaded")
                            } else {
                                mediaConfigResult
                            },
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    } else {
                        MediaConfigRow(label = "audioDelayMs", value = mediaConfig?.audioDelayMs)
                        MediaConfigRow(label = "videoDelayMs", value = mediaConfig?.videoDelayMs)
                        MediaConfigRow(label = "audioMaxFrames", value = mediaConfig?.audioMaxFrames)
                        MediaConfigRow(label = "videoMaxFrames", value = mediaConfig?.videoMaxFrames)
                        MediaConfigRow(label = "pullMaxPackets", value = mediaConfig?.pullMaxPackets)
                        MediaConfigRow(label = "pullWaitMs", value = mediaConfig?.pullWaitMs)
                        MediaConfigRow(label = "groupPullMaxPackets", value = mediaConfig?.groupPullMaxPackets)
                        MediaConfigRow(label = "groupPullWaitMs", value = mediaConfig?.groupPullWaitMs)
                    }
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        SecondaryButton(
                            label = tr("diagnostics_load", "Load"),
                            fillMaxWidth = false,
                            onClick = {
                                mediaConfig = sdk.getMediaConfig()
                                mediaConfigResult = if (mediaConfig == null) {
                                    sdk.lastError.ifBlank { tr("diagnostics_load_failed", "Failed to load") }
                                } else {
                                    ""
                                }
                            }
                        )
                    }
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_media_subs", "Media subscriptions"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = subCallIdHex,
                        onValueChange = { subCallIdHex = it.trim() },
                        label = { Text(tr("diagnostics_call_id_hex", "Call id (hex)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(tr("diagnostics_is_group", "Is group"))
                        Spacer(modifier = Modifier.weight(1f))
                        Switch(checked = subIsGroup, onCheckedChange = { subIsGroup = it })
                    }
                    if (subIsGroup) {
                        OutlinedTextField(
                            value = subGroupId,
                            onValueChange = { subGroupId = it },
                            label = { Text(tr("diagnostics_group_id", "Group id")) },
                            modifier = Modifier.fillMaxWidth()
                        )
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        PrimaryButton(
                            label = tr("diagnostics_add_subscription", "Add subscription"),
                            fillMaxWidth = false,
                            onClick = {
                                val callId = parseCallIdHex(subCallIdHex)
                                if (callId == null) {
                                    subResult = tr("diagnostics_invalid_call_id", "Invalid call id")
                                } else if (subIsGroup && subGroupId.isBlank()) {
                                    subResult = tr("diagnostics_group_required", "Group id required")
                                } else {
                                    val ok = sdk.addMediaSubscription(
                                        callId,
                                        isGroup = subIsGroup,
                                        groupId = subGroupId.takeIf { subIsGroup }
                                    )
                                    subResult = if (ok) {
                                        tr("diagnostics_subscription_added", "Subscription added")
                                    } else {
                                        sdk.lastError.ifBlank { tr("diagnostics_subscription_failed", "Failed") }
                                    }
                                }
                            }
                        )
                        SecondaryButton(
                            label = tr("diagnostics_clear", "Clear"),
                            fillMaxWidth = false,
                            onClick = {
                                sdk.clearMediaSubscriptions()
                                subResult = tr("diagnostics_cleared", "Cleared")
                            }
                        )
                    }
                    ResultText(text = subResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_presence", "Presence"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = presenceUser,
                        onValueChange = { presenceUser = it },
                        label = { Text(tr("diagnostics_username", "Username")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(tr("diagnostics_online", "Online"))
                        Spacer(modifier = Modifier.weight(1f))
                        Switch(checked = presenceOnline, onCheckedChange = { presenceOnline = it })
                    }
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        PrimaryButton(
                            label = tr("diagnostics_send_presence", "Send presence"),
                            fillMaxWidth = false,
                            enabled = presenceUser.isNotBlank(),
                            onClick = {
                                sdk.sendPresence(presenceUser.trim(), presenceOnline)
                                presenceResult = tr("diagnostics_presence_sent", "Presence sent")
                            }
                        )
                    }
                    ResultText(text = presenceResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_read_receipt", "Read receipt"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = receiptUser,
                        onValueChange = { receiptUser = it },
                        label = { Text(tr("diagnostics_peer", "Peer username")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = receiptMessageId,
                        onValueChange = { receiptMessageId = it },
                        label = { Text(tr("diagnostics_message_id", "Message id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        PrimaryButton(
                            label = tr("diagnostics_send_receipt", "Send receipt"),
                            fillMaxWidth = false,
                            enabled = receiptUser.isNotBlank() && receiptMessageId.isNotBlank(),
                            onClick = {
                                sdk.sendReadReceipt(receiptUser.trim(), receiptMessageId.trim())
                                receiptResult = tr("diagnostics_receipt_sent", "Read receipt sent")
                            }
                        )
                    }
                    ResultText(text = receiptResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_group_call_signal", "Group call signal"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = signalGroupId,
                        onValueChange = { signalGroupId = it },
                        label = { Text(tr("diagnostics_group_id", "Group id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = signalCallIdHex,
                        onValueChange = { signalCallIdHex = it.trim() },
                        label = { Text(tr("diagnostics_call_id_hex", "Call id (hex)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        OutlinedTextField(
                            value = signalOp,
                            onValueChange = { signalOp = it },
                            label = { Text(tr("diagnostics_op", "Op")) },
                            modifier = Modifier.weight(1f)
                        )
                        OutlinedTextField(
                            value = signalKeyId,
                            onValueChange = { signalKeyId = it },
                            label = { Text(tr("diagnostics_key_id", "Key id")) },
                            modifier = Modifier.weight(1f)
                        )
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        OutlinedTextField(
                            value = signalSeq,
                            onValueChange = { signalSeq = it },
                            label = { Text(tr("diagnostics_seq", "Seq")) },
                            modifier = Modifier.weight(1f)
                        )
                        OutlinedTextField(
                            value = signalTsMs,
                            onValueChange = { signalTsMs = it },
                            label = { Text(tr("diagnostics_ts_ms", "Timestamp ms")) },
                            modifier = Modifier.weight(1f)
                        )
                    }
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(tr("diagnostics_video", "Video"))
                        Spacer(modifier = Modifier.weight(1f))
                        Switch(checked = signalVideo, onCheckedChange = { signalVideo = it })
                    }
                    OutlinedTextField(
                        value = signalExtHex,
                        onValueChange = { signalExtHex = it.trim() },
                        label = { Text(tr("diagnostics_ext_hex", "Ext (hex, optional)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        PrimaryButton(
                            label = tr("diagnostics_send_signal", "Send signal"),
                            fillMaxWidth = false,
                            onClick = {
                                val callId = parseCallIdHex(signalCallIdHex)
                                val op = signalOp.toIntOrNull()
                                val key = signalKeyId.toIntOrNull()
                                val seq = signalSeq.toIntOrNull()
                                val ts = signalTsMs.toLongOrNull()
                                if (signalGroupId.isBlank() || callId == null || op == null || key == null || seq == null || ts == null) {
                                    signalResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val ext = hexToBytesOrNull(signalExtHex)
                                    if (signalExtHex.isNotBlank() && ext == null) {
                                        signalResult = tr("diagnostics_invalid_input", "Invalid input")
                                    } else {
                                        val result = sdk.sendGroupCallSignal(op, signalGroupId.trim(), callId, signalVideo, key, seq, ts, ext)
                                        signalResult = if (result == null) {
                                            sdk.lastError.ifBlank { tr("diagnostics_send_failed", "Send failed") }
                                        } else {
                                            tr("diagnostics_signal_ok", "keyId=%d members=%d")
                                                .format(result.keyId, result.members.size)
                                        }
                                    }
                                }
                            }
                        )
                    }
                    ResultText(text = signalResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_group_call_key", "Group call key"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = keyGroupId,
                        onValueChange = { keyGroupId = it },
                        label = { Text(tr("diagnostics_group_id", "Group id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = keyCallIdHex,
                        onValueChange = { keyCallIdHex = it.trim() },
                        label = { Text(tr("diagnostics_call_id_hex", "Call id (hex)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = keyId,
                        onValueChange = { keyId = it },
                        label = { Text(tr("diagnostics_key_id", "Key id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = keyMembers,
                        onValueChange = { keyMembers = it },
                        label = { Text(tr("diagnostics_members_csv", "Members (csv)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        SecondaryButton(
                            label = tr("diagnostics_get_key", "Get key"),
                            fillMaxWidth = false,
                            onClick = {
                                val callId = parseCallIdHex(keyCallIdHex)
                                val parsedKeyId = keyId.toIntOrNull()
                                if (keyGroupId.isBlank() || callId == null || parsedKeyId == null) {
                                    keyResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val keyBytes = sdk.getGroupCallKey(keyGroupId.trim(), callId, parsedKeyId)
                                    keyResult = keyBytes?.let { bytesToHex(it) }
                                        ?: tr("diagnostics_no_key", "No key")
                                }
                            }
                        )
                        SecondaryButton(
                            label = tr("diagnostics_rotate_key", "Rotate"),
                            fillMaxWidth = false,
                            onClick = {
                                val callId = parseCallIdHex(keyCallIdHex)
                                val parsedKeyId = keyId.toIntOrNull()
                                val members = parseCsv(keyMembers)
                                if (keyGroupId.isBlank() || callId == null || parsedKeyId == null || members.isEmpty()) {
                                    keyResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val ok = sdk.rotateGroupCallKey(keyGroupId.trim(), callId, parsedKeyId, members)
                                    keyResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                }
                            }
                        )
                        SecondaryButton(
                            label = tr("diagnostics_request_key", "Request"),
                            fillMaxWidth = false,
                            onClick = {
                                val callId = parseCallIdHex(keyCallIdHex)
                                val parsedKeyId = keyId.toIntOrNull()
                                val members = parseCsv(keyMembers)
                                if (keyGroupId.isBlank() || callId == null || parsedKeyId == null || members.isEmpty()) {
                                    keyResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val ok = sdk.requestGroupCallKey(keyGroupId.trim(), callId, parsedKeyId, members)
                                    keyResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                }
                            }
                        )
                    }
                    ResultText(text = keyResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_media_peer", "Media push/pull (peer)"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = peerUsername,
                        onValueChange = { peerUsername = it },
                        label = { Text(tr("diagnostics_peer", "Peer username")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = peerCallIdHex,
                        onValueChange = { peerCallIdHex = it.trim() },
                        label = { Text(tr("diagnostics_call_id_hex", "Call id (hex)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = peerPayloadHex,
                        onValueChange = { peerPayloadHex = it.trim() },
                        label = { Text(tr("diagnostics_payload_hex", "Payload (hex)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        OutlinedTextField(
                            value = peerPullMaxPackets,
                            onValueChange = { peerPullMaxPackets = it },
                            label = { Text(tr("diagnostics_max_packets", "Max packets")) },
                            modifier = Modifier.weight(1f)
                        )
                        OutlinedTextField(
                            value = peerPullWaitMs,
                            onValueChange = { peerPullWaitMs = it },
                            label = { Text(tr("diagnostics_wait_ms", "Wait ms")) },
                            modifier = Modifier.weight(1f)
                        )
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        PrimaryButton(
                            label = tr("diagnostics_push", "Push"),
                            fillMaxWidth = false,
                            onClick = {
                                val callId = parseCallIdHex(peerCallIdHex)
                                val payload = hexToBytesOrNull(peerPayloadHex)
                                if (peerUsername.isBlank() || callId == null || payload == null) {
                                    peerMediaResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val ok = sdk.pushMedia(peerUsername.trim(), callId, payload)
                                    peerMediaResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                }
                            }
                        )
                        SecondaryButton(
                            label = tr("diagnostics_pull", "Pull"),
                            fillMaxWidth = false,
                            onClick = {
                                val callId = parseCallIdHex(peerCallIdHex)
                                val maxPackets = peerPullMaxPackets.toIntOrNull() ?: 0
                                val waitMs = peerPullWaitMs.toIntOrNull() ?: 0
                                if (callId == null) {
                                    peerMediaResult = tr("diagnostics_invalid_call_id", "Invalid call id")
                                } else {
                                    val packets = sdk.pullMedia(callId, maxPackets, waitMs)
                                    peerMediaResult = formatPacketsResult(packets)
                                }
                            }
                        )
                    }
                    ResultText(text = peerMediaResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_media_group", "Media push/pull (group)"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = groupIdForMedia,
                        onValueChange = { groupIdForMedia = it },
                        label = { Text(tr("diagnostics_group_id", "Group id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = groupCallIdHex,
                        onValueChange = { groupCallIdHex = it.trim() },
                        label = { Text(tr("diagnostics_call_id_hex", "Call id (hex)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = groupPayloadHex,
                        onValueChange = { groupPayloadHex = it.trim() },
                        label = { Text(tr("diagnostics_payload_hex", "Payload (hex)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        OutlinedTextField(
                            value = groupPullMaxPackets,
                            onValueChange = { groupPullMaxPackets = it },
                            label = { Text(tr("diagnostics_max_packets", "Max packets")) },
                            modifier = Modifier.weight(1f)
                        )
                        OutlinedTextField(
                            value = groupPullWaitMs,
                            onValueChange = { groupPullWaitMs = it },
                            label = { Text(tr("diagnostics_wait_ms", "Wait ms")) },
                            modifier = Modifier.weight(1f)
                        )
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        PrimaryButton(
                            label = tr("diagnostics_push", "Push"),
                            fillMaxWidth = false,
                            onClick = {
                                val callId = parseCallIdHex(groupCallIdHex)
                                val payload = hexToBytesOrNull(groupPayloadHex)
                                if (groupIdForMedia.isBlank() || callId == null || payload == null) {
                                    groupMediaResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val ok = sdk.pushGroupMedia(groupIdForMedia.trim(), callId, payload)
                                    groupMediaResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                }
                            }
                        )
                        SecondaryButton(
                            label = tr("diagnostics_pull", "Pull"),
                            fillMaxWidth = false,
                            onClick = {
                                val callId = parseCallIdHex(groupCallIdHex)
                                val maxPackets = groupPullMaxPackets.toIntOrNull() ?: 0
                                val waitMs = groupPullWaitMs.toIntOrNull() ?: 0
                                if (callId == null) {
                                    groupMediaResult = tr("diagnostics_invalid_call_id", "Invalid call id")
                                } else {
                                    val packets = sdk.pullGroupMedia(callId, maxPackets, waitMs)
                                    groupMediaResult = formatPacketsResult(packets)
                                }
                            }
                        )
                    }
                    ResultText(text = groupMediaResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_media_root", "Derive media root"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = rootPeer,
                        onValueChange = { rootPeer = it },
                        label = { Text(tr("diagnostics_peer", "Peer username")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = rootCallIdHex,
                        onValueChange = { rootCallIdHex = it.trim() },
                        label = { Text(tr("diagnostics_call_id_hex", "Call id (hex)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        PrimaryButton(
                            label = tr("diagnostics_derive", "Derive"),
                            fillMaxWidth = false,
                            onClick = {
                                val callId = parseCallIdHex(rootCallIdHex)
                                if (rootPeer.isBlank() || callId == null) {
                                    rootResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val root = sdk.deriveMediaRoot(rootPeer.trim(), callId)
                                    rootResult = root?.let { bytesToHex(it) }
                                        ?: sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                }
                            }
                        )
                    }
                    ResultText(text = rootResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_attachment_preview", "Store attachment preview"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = previewFileId,
                        onValueChange = { previewFileId = it },
                        label = { Text(tr("diagnostics_file_id", "File id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = previewFileName,
                        onValueChange = { previewFileName = it },
                        label = { Text(tr("diagnostics_file_name", "File name")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = previewFileSize,
                        onValueChange = { previewFileSize = it },
                        label = { Text(tr("diagnostics_file_size", "File size")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = previewPayloadHex,
                        onValueChange = { previewPayloadHex = it.trim() },
                        label = { Text(tr("diagnostics_payload_hex", "Payload (hex)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        PrimaryButton(
                            label = tr("diagnostics_store", "Store"),
                            fillMaxWidth = false,
                            onClick = {
                                val size = previewFileSize.toLongOrNull()
                                val payload = hexToBytesOrNull(previewPayloadHex)
                                if (previewFileId.isBlank() || previewFileName.isBlank() || size == null || payload == null) {
                                    previewResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val ok = sdk.storeAttachmentPreviewBytes(
                                        previewFileId.trim(),
                                        previewFileName.trim(),
                                        size,
                                        payload
                                    )
                                    previewResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                }
                            }
                        )
                    }
                    ResultText(text = previewResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_download_bytes", "Download file to bytes"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = downloadFileId,
                        onValueChange = { downloadFileId = it },
                        label = { Text(tr("diagnostics_file_id", "File id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = downloadFileName,
                        onValueChange = { downloadFileName = it },
                        label = { Text(tr("diagnostics_file_name", "File name")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = downloadFileSize,
                        onValueChange = { downloadFileSize = it },
                        label = { Text(tr("diagnostics_file_size", "File size")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = downloadFileKeyHex,
                        onValueChange = { downloadFileKeyHex = it.trim() },
                        label = { Text(tr("diagnostics_file_key_hex", "File key (hex)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(tr("diagnostics_wipe_after_read", "Wipe after read"))
                        Spacer(modifier = Modifier.weight(1f))
                        Switch(checked = downloadWipe, onCheckedChange = { downloadWipe = it })
                    }
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        PrimaryButton(
                            label = tr("diagnostics_download", "Download"),
                            fillMaxWidth = false,
                            onClick = {
                                val size = downloadFileSize.toLongOrNull()
                                val fileKey = hexToBytesOrNull(downloadFileKeyHex)
                                if (downloadFileId.isBlank() || downloadFileName.isBlank() || size == null || fileKey == null) {
                                    downloadResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val bytes = sdk.downloadChatFileToBytes(
                                        downloadFileId.trim(),
                                        fileKey,
                                        downloadFileName.trim(),
                                        size,
                                        downloadWipe
                                    )
                                    downloadResult = if (bytes == null) {
                                        sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                    } else {
                                        tr("diagnostics_bytes_len", "Bytes: %d").format(bytes.size)
                                    }
                                }
                            }
                        )
                    }
                    ResultText(text = downloadResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_resend_text", "Resend text"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = resendTarget,
                        onValueChange = { resendTarget = it },
                        label = { Text(tr("diagnostics_target", "Target (peer or group)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendMessageId,
                        onValueChange = { resendMessageId = it },
                        label = { Text(tr("diagnostics_message_id", "Message id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendText,
                        onValueChange = { resendText = it },
                        label = { Text(tr("diagnostics_text", "Text")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendReplyTo,
                        onValueChange = { resendReplyTo = it },
                        label = { Text(tr("diagnostics_reply_to", "Reply message id (optional)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendReplyPreview,
                        onValueChange = { resendReplyPreview = it },
                        label = { Text(tr("diagnostics_reply_preview", "Reply preview (optional)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(tr("diagnostics_is_group", "Is group"))
                        Spacer(modifier = Modifier.weight(1f))
                        Switch(checked = resendIsGroup, onCheckedChange = { resendIsGroup = it })
                    }
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        PrimaryButton(
                            label = tr("diagnostics_resend", "Resend"),
                            fillMaxWidth = false,
                            onClick = {
                                if (resendTarget.isBlank() || resendMessageId.isBlank() || resendText.isBlank()) {
                                    resendTextResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else if (resendIsGroup) {
                                    val ok = sdk.resendGroupText(resendTarget.trim(), resendMessageId.trim(), resendText.trim())
                                    resendTextResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                } else if (resendReplyTo.isNotBlank()) {
                                    val ok = sdk.resendPrivateTextWithReply(
                                        resendTarget.trim(),
                                        resendMessageId.trim(),
                                        resendText.trim(),
                                        resendReplyTo.trim(),
                                        resendReplyPreview.trim()
                                    )
                                    resendTextResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                } else {
                                    val ok = sdk.resendPrivateText(resendTarget.trim(), resendMessageId.trim(), resendText.trim())
                                    resendTextResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                }
                            }
                        )
                    }
                    ResultText(text = resendTextResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_resend_file", "Resend file"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = resendFileTarget,
                        onValueChange = { resendFileTarget = it },
                        label = { Text(tr("diagnostics_target", "Target (peer or group)")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendFileMessageId,
                        onValueChange = { resendFileMessageId = it },
                        label = { Text(tr("diagnostics_message_id", "Message id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendFilePath,
                        onValueChange = { resendFilePath = it },
                        label = { Text(tr("diagnostics_file_path", "File path")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(tr("diagnostics_is_group", "Is group"))
                        Spacer(modifier = Modifier.weight(1f))
                        Switch(checked = resendFileIsGroup, onCheckedChange = { resendFileIsGroup = it })
                    }
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        PrimaryButton(
                            label = tr("diagnostics_resend", "Resend"),
                            fillMaxWidth = false,
                            onClick = {
                                if (resendFileTarget.isBlank() || resendFileMessageId.isBlank() || resendFilePath.isBlank()) {
                                    resendFileResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else if (resendFileIsGroup) {
                                    val ok = sdk.resendGroupFile(resendFileTarget.trim(), resendFileMessageId.trim(), resendFilePath.trim())
                                    resendFileResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                } else {
                                    val ok = sdk.resendPrivateFile(resendFileTarget.trim(), resendFileMessageId.trim(), resendFilePath.trim())
                                    resendFileResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                }
                            }
                        )
                    }
                    ResultText(text = resendFileResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_resend_sticker", "Resend sticker"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = resendStickerTarget,
                        onValueChange = { resendStickerTarget = it },
                        label = { Text(tr("diagnostics_peer", "Peer username")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendStickerMessageId,
                        onValueChange = { resendStickerMessageId = it },
                        label = { Text(tr("diagnostics_message_id", "Message id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendStickerId,
                        onValueChange = { resendStickerId = it },
                        label = { Text(tr("diagnostics_sticker_id", "Sticker id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        PrimaryButton(
                            label = tr("diagnostics_resend", "Resend"),
                            fillMaxWidth = false,
                            onClick = {
                                if (resendStickerTarget.isBlank() || resendStickerMessageId.isBlank() || resendStickerId.isBlank()) {
                                    resendStickerResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val ok = sdk.resendPrivateSticker(
                                        resendStickerTarget.trim(),
                                        resendStickerMessageId.trim(),
                                        resendStickerId.trim()
                                    )
                                    resendStickerResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                }
                            }
                        )
                    }
                    ResultText(text = resendStickerResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_resend_location", "Resend location"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = resendLocationTarget,
                        onValueChange = { resendLocationTarget = it },
                        label = { Text(tr("diagnostics_peer", "Peer username")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendLocationMessageId,
                        onValueChange = { resendLocationMessageId = it },
                        label = { Text(tr("diagnostics_message_id", "Message id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendLocationLabel,
                        onValueChange = { resendLocationLabel = it },
                        label = { Text(tr("diagnostics_location_label", "Label")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        OutlinedTextField(
                            value = resendLocationLat,
                            onValueChange = { resendLocationLat = it },
                            label = { Text(tr("diagnostics_lat", "Latitude")) },
                            modifier = Modifier.weight(1f)
                        )
                        OutlinedTextField(
                            value = resendLocationLon,
                            onValueChange = { resendLocationLon = it },
                            label = { Text(tr("diagnostics_lon", "Longitude")) },
                            modifier = Modifier.weight(1f)
                        )
                    }
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        PrimaryButton(
                            label = tr("diagnostics_resend", "Resend"),
                            fillMaxWidth = false,
                            onClick = {
                                val lat = resendLocationLat.toDoubleOrNull()
                                val lon = resendLocationLon.toDoubleOrNull()
                                if (resendLocationTarget.isBlank() || resendLocationMessageId.isBlank() ||
                                    resendLocationLabel.isBlank() || lat == null || lon == null
                                ) {
                                    resendLocationResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val ok = sdk.resendPrivateLocation(
                                        resendLocationTarget.trim(),
                                        resendLocationMessageId.trim(),
                                        lat,
                                        lon,
                                        resendLocationLabel.trim()
                                    )
                                    resendLocationResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                }
                            }
                        )
                    }
                    ResultText(text = resendLocationResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_resend_contact", "Resend contact"))
                DiagnosticCard {
                    OutlinedTextField(
                        value = resendContactTarget,
                        onValueChange = { resendContactTarget = it },
                        label = { Text(tr("diagnostics_peer", "Peer username")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendContactMessageId,
                        onValueChange = { resendContactMessageId = it },
                        label = { Text(tr("diagnostics_message_id", "Message id")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendContactUsername,
                        onValueChange = { resendContactUsername = it },
                        label = { Text(tr("diagnostics_contact_username", "Contact username")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    OutlinedTextField(
                        value = resendContactDisplay,
                        onValueChange = { resendContactDisplay = it },
                        label = { Text(tr("diagnostics_contact_display", "Contact display")) },
                        modifier = Modifier.fillMaxWidth()
                    )
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        PrimaryButton(
                            label = tr("diagnostics_resend", "Resend"),
                            fillMaxWidth = false,
                            onClick = {
                                if (resendContactTarget.isBlank() || resendContactMessageId.isBlank() ||
                                    resendContactUsername.isBlank() || resendContactDisplay.isBlank()
                                ) {
                                    resendContactResult = tr("diagnostics_invalid_input", "Invalid input")
                                } else {
                                    val ok = sdk.resendPrivateContact(
                                        resendContactTarget.trim(),
                                        resendContactMessageId.trim(),
                                        resendContactUsername.trim(),
                                        resendContactDisplay.trim()
                                    )
                                    resendContactResult = if (ok) tr("diagnostics_ok", "OK") else sdk.lastError.ifBlank { tr("diagnostics_failed", "Failed") }
                                }
                            }
                        )
                    }
                    ResultText(text = resendContactResult)
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_relay_logs", "Relay events"))
                DiagnosticCard {
                    val relay = sdk.mediaRelayEvents
                    if (relay.isEmpty()) {
                        Text(
                            text = tr("diagnostics_no_relay", "No relay events"),
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    } else {
                        relay.take(10).forEach { entry ->
                            LogRow(
                                headline = "${timeFormat.format(Date(entry.timestampMs))} " +
                                    (if (entry.isGroup) "group" else "peer"),
                                detail = listOfNotNull(
                                    entry.groupId.takeIf { it.isNotBlank() },
                                    entry.peer.takeIf { it.isNotBlank() },
                                    entry.sender.takeIf { it.isNotBlank() }
                                ).joinToString(" / ").ifBlank { "-" },
                                payload = "bytes=${entry.payloadSize} ${entry.payloadPreview}"
                            )
                        }
                    }
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        SecondaryButton(
                            label = tr("diagnostics_clear", "Clear"),
                            fillMaxWidth = false,
                            onClick = { sdk.clearMediaRelayEvents() }
                        )
                    }
                }
            }

            item {
                SectionHeader(text = tr("diagnostics_offline_payloads", "Offline payloads"))
                DiagnosticCard {
                    val offline = sdk.offlinePayloads
                    if (offline.isEmpty()) {
                        Text(
                            text = tr("diagnostics_no_offline", "No offline payloads"),
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    } else {
                        offline.take(10).forEach { entry ->
                            LogRow(
                                headline = timeFormat.format(Date(entry.timestampMs)),
                                detail = listOfNotNull(
                                    entry.groupId.takeIf { it.isNotBlank() },
                                    entry.peer.takeIf { it.isNotBlank() },
                                    entry.sender.takeIf { it.isNotBlank() }
                                ).joinToString(" / ").ifBlank { "-" },
                                payload = "bytes=${entry.payloadSize} ${entry.payloadPreview}"
                            )
                        }
                    }
                    Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                        SecondaryButton(
                            label = tr("diagnostics_clear", "Clear"),
                            fillMaxWidth = false,
                            onClick = { sdk.clearOfflinePayloads() }
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun DiagnosticCard(content: @Composable ColumnScope.() -> Unit) {
    Card(
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
            content = content
        )
    }
}

@Composable
private fun InfoRow(label: String, value: String) {
    Column(modifier = Modifier.fillMaxWidth()) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Text(
            text = value,
            style = MaterialTheme.typography.bodyMedium,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
    }
}

@Composable
private fun MediaConfigRow(label: String, value: Int?) {
    InfoRow(label = label, value = value?.toString() ?: "-")
}

@Composable
private fun ResultText(text: String) {
    if (text.isNotBlank()) {
        Text(
            text = text,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
    }
}

@Composable
private fun LogRow(headline: String, detail: String, payload: String) {
    Column(modifier = Modifier.fillMaxWidth()) {
        Text(text = headline, style = MaterialTheme.typography.labelMedium)
        Text(
            text = detail,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
        Text(
            text = payload,
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis
        )
        Spacer(modifier = Modifier.height(8.dp))
    }
}

private fun parseCallIdHex(hex: String): ByteArray? {
    val bytes = hexToBytesOrNull(hex) ?: return null
    return if (bytes.size == 16) bytes else null
}

private fun parseCsv(value: String): Array<String> {
    return value.split(",")
        .map { it.trim() }
        .filter { it.isNotEmpty() }
        .toTypedArray()
}

private fun formatPacketsResult(packets: Array<MediaPacket>): String {
    if (packets.isEmpty()) return "Packets: 0"
    val first = packets.first()
    val preview = bytesToHex(first.payload.take(12).toByteArray())
    return "Packets: ${packets.size} | first=${first.sender} ${first.payload.size} bytes ${preview}"
}

private fun bytesToHex(bytes: ByteArray): String {
    val sb = StringBuilder(bytes.size * 2)
    for (b in bytes) {
        sb.append(String.format("%02x", b))
    }
    return sb.toString()
}

private fun hexToBytesOrNull(text: String): ByteArray? {
    val cleaned = text.trim().lowercase(Locale.getDefault())
    if (cleaned.isEmpty()) return null
    if (cleaned.length % 2 != 0) return null
    return try {
        val out = ByteArray(cleaned.length / 2)
        var i = 0
        while (i < cleaned.length) {
            val byte = cleaned.substring(i, i + 2).toInt(16)
            out[i / 2] = byte.toByte()
            i += 2
        }
        out
    } catch (ex: NumberFormatException) {
        null
    }
}
