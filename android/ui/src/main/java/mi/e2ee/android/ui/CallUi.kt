package mi.e2ee.android.ui

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.view.PreviewView
import androidx.compose.foundation.Image
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
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.BugReport
import androidx.compose.material.icons.filled.CallEnd
import androidx.compose.material.icons.filled.MicOff
import androidx.compose.material.icons.filled.VideocamOff
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import mi.e2ee.android.BuildConfig

@Composable
fun PeerCallScreen(
    controller: CallMediaController,
    call: PeerCallState,
    onHangup: () -> Unit,
    onAddSubscription: () -> Unit = {},
    onClearSubscriptions: () -> Unit = {}
) {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val required = remember(call.video) {
        buildList {
            add(Manifest.permission.RECORD_AUDIO)
            if (call.video) {
                add(Manifest.permission.CAMERA)
            }
        }.toTypedArray()
    }
    var granted by remember {
        mutableStateOf(hasPermissions(context, required))
    }
    val launcher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        granted = required.all { result[it] == true }
    }

    LaunchedEffect(required) {
        if (!granted) {
            launcher.launch(required)
        }
    }

    val previewView = remember(call.video) {
        PreviewView(context).apply {
            scaleType = PreviewView.ScaleType.FILL_CENTER
        }
    }

    LaunchedEffect(granted, call.callIdHex) {
        if (granted) {
            controller.startPeerCall(call, lifecycleOwner, if (call.video) previewView else null)
        }
    }

    DisposableEffect(call.callIdHex) {
        onDispose { controller.stop() }
    }

    if (!granted) {
        CallPermissionScreen(
            title = tr("call_permission_title", "Permissions needed"),
            body = if (call.video) {
                tr("call_permission_body_video", "Allow microphone and camera access to start the call.")
            } else {
                tr("call_permission_body_audio", "Allow microphone access to start the call.")
            },
            onRetry = { launcher.launch(required) },
            onHangup = onHangup
        )
        return
    }

    CallLayout(
        title = call.peerUsername,
        subtitle = if (call.video) tr("call_video", "Video call") else tr("call_voice", "Voice call"),
        videoEnabled = call.video,
        previewView = previewView,
        remoteVideo = controller.remoteVideo,
        error = controller.lastError,
        callIdHex = call.callIdHex,
        onAddSubscription = onAddSubscription,
        onClearSubscriptions = onClearSubscriptions,
        onHangup = onHangup
    )
}

@Composable
fun GroupCallScreen(
    controller: CallMediaController,
    call: GroupCallState,
    title: String,
    onHangup: () -> Unit,
    onAddSubscription: () -> Unit = {},
    onClearSubscriptions: () -> Unit = {}
) {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val required = remember(call.video) {
        buildList {
            add(Manifest.permission.RECORD_AUDIO)
            if (call.video) {
                add(Manifest.permission.CAMERA)
            }
        }.toTypedArray()
    }
    var granted by remember {
        mutableStateOf(hasPermissions(context, required))
    }
    val launcher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        granted = required.all { result[it] == true }
    }

    LaunchedEffect(required) {
        if (!granted) {
            launcher.launch(required)
        }
    }

    val previewView = remember(call.video) {
        PreviewView(context).apply {
            scaleType = PreviewView.ScaleType.FILL_CENTER
        }
    }

    LaunchedEffect(granted, call.callIdHex, call.keyId, call.keyReady) {
        if (granted && call.keyReady) {
            controller.startGroupCall(call, lifecycleOwner, if (call.video) previewView else null)
        }
    }

    DisposableEffect(call.callIdHex) {
        onDispose { controller.stop() }
    }

    if (!granted) {
        CallPermissionScreen(
            title = tr("call_permission_title", "Permissions needed"),
            body = if (call.video) {
                tr("call_permission_body_video", "Allow microphone and camera access to start the call.")
            } else {
                tr("call_permission_body_audio", "Allow microphone access to start the call.")
            },
            onRetry = { launcher.launch(required) },
            onHangup = onHangup
        )
        return
    }

    val statusNote = if (call.keyReady) {
        ""
    } else {
        tr("call_waiting_key", "Waiting for call key")
    }
    CallLayout(
        title = title,
        subtitle = if (call.video) tr("call_group_video", "Group video call") else tr("call_group_voice", "Group voice call"),
        videoEnabled = call.video,
        previewView = previewView,
        remoteVideo = controller.remoteVideo,
        error = controller.lastError,
        statusNote = statusNote,
        callIdHex = call.callIdHex,
        groupId = call.groupId,
        keyId = call.keyId,
        keyReady = call.keyReady,
        onAddSubscription = onAddSubscription,
        onClearSubscriptions = onClearSubscriptions,
        onHangup = onHangup
    )
}

@Composable
private fun CallLayout(
    title: String,
    subtitle: String,
    videoEnabled: Boolean,
    previewView: PreviewView,
    remoteVideo: android.graphics.Bitmap?,
    error: String,
    statusNote: String = "",
    callIdHex: String,
    groupId: String? = null,
    keyId: Int? = null,
    keyReady: Boolean = true,
    onAddSubscription: (() -> Unit)? = null,
    onClearSubscriptions: (() -> Unit)? = null,
    onHangup: () -> Unit
) {
    val clipboard = LocalClipboardManager.current
    val showDebugTools = BuildConfig.DEBUG
    var toolsOpen by remember(callIdHex) { mutableStateOf(false) }
    var toolsResult by remember(callIdHex) { mutableStateOf<String?>(null) }
    Box(modifier = Modifier.fillMaxSize()) {
        if (videoEnabled) {
            if (remoteVideo != null) {
                Image(
                    bitmap = remoteVideo.asImageBitmap(),
                    contentDescription = null,
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize()
                )
            } else {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(MaterialTheme.colorScheme.surfaceVariant)
                )
            }
        } else {
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .background(MaterialTheme.colorScheme.surfaceVariant),
                verticalArrangement = Arrangement.Center,
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                Icon(
                    imageVector = Icons.Filled.MicOff,
                    contentDescription = null,
                    modifier = Modifier.size(72.dp),
                    tint = MaterialTheme.colorScheme.onSurfaceVariant
                )
                Spacer(modifier = Modifier.height(12.dp))
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.titleMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }

        if (showDebugTools) {
            IconButton(
                onClick = { toolsOpen = true },
                modifier = Modifier
                    .align(Alignment.TopEnd)
                    .padding(12.dp)
            ) {
                Icon(
                    imageVector = Icons.Filled.BugReport,
                    contentDescription = tr("call_tools", "Call tools"),
                    tint = MaterialTheme.colorScheme.onSurface
                )
            }
        }

        if (showDebugTools && toolsOpen) {
            AlertDialog(
                onDismissRequest = { toolsOpen = false },
                title = { Text(tr("call_tools_title", "Call tools")) },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text(
                            text = tr("call_tools_call_id", "Call id: %s").format(callIdHex),
                            style = MaterialTheme.typography.bodySmall
                        )
                        if (!groupId.isNullOrBlank()) {
                            Text(
                                text = tr("call_tools_group_id", "Group: %s").format(groupId),
                                style = MaterialTheme.typography.bodySmall
                            )
                        }
                        if (keyId != null) {
                            val keyLabel = if (keyReady) {
                                tr("call_tools_key_ready", "ready")
                            } else {
                                tr("call_tools_key_pending", "pending")
                            }
                            Text(
                                text = tr("call_tools_key_id", "Key: %d (%s)").format(keyId, keyLabel),
                                style = MaterialTheme.typography.bodySmall
                            )
                        }
                        if (statusNote.isNotBlank()) {
                            Text(
                                text = statusNote,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                        if (error.isNotBlank()) {
                            Text(
                                text = tr("call_tools_error", "Error: %s").format(error),
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.error
                            )
                        }
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            TextButton(onClick = {
                                clipboard.setText(AnnotatedString(callIdHex))
                                toolsResult = tr("call_tools_copied", "Copied")
                            }) {
                                Text(tr("call_tools_copy", "Copy call id"))
                            }
                            if (onAddSubscription != null) {
                                TextButton(onClick = {
                                    onAddSubscription()
                                    toolsResult = tr("call_tools_sub_added", "Subscription added")
                                }) {
                                    Text(tr("call_tools_subscribe", "Add subscription"))
                                }
                            }
                        }
                        if (onClearSubscriptions != null) {
                            TextButton(onClick = {
                                onClearSubscriptions()
                                toolsResult = tr("call_tools_sub_cleared", "Subscriptions cleared")
                            }) {
                                Text(tr("call_tools_clear_subs", "Clear subscriptions"))
                            }
                        }
                        toolsResult?.let { result ->
                            Text(
                                text = result,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }
                },
                confirmButton = {
                    TextButton(onClick = { toolsOpen = false }) {
                        Text(tr("chat_close", "Close"))
                    }
                }
            )
        }

        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(20.dp)
        ) {
            Text(
                text = title,
                style = MaterialTheme.typography.headlineSmall,
                fontWeight = FontWeight.SemiBold,
                color = MaterialTheme.colorScheme.onSurface
            )
            Text(
                text = subtitle,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }

        if (statusNote.isNotBlank()) {
            Card(
                modifier = Modifier
                    .align(Alignment.Center)
                    .padding(16.dp)
            ) {
                Text(
                    text = statusNote,
                    style = MaterialTheme.typography.bodyMedium,
                    modifier = Modifier.padding(horizontal = 16.dp, vertical = 10.dp)
                )
            }
        }

        if (videoEnabled) {
            androidx.compose.ui.viewinterop.AndroidView(
                factory = { previewView },
                modifier = Modifier
                    .align(Alignment.TopEnd)
                    .padding(16.dp)
                    .size(140.dp, 200.dp)
                    .clip(RoundedCornerShape(16.dp))
                    .background(MaterialTheme.colorScheme.surface)
            )
        } else {
            Icon(
                imageVector = Icons.Filled.VideocamOff,
                contentDescription = null,
                modifier = Modifier
                    .align(Alignment.TopEnd)
                    .padding(16.dp)
                    .size(28.dp),
                tint = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }

        Row(
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .padding(24.dp),
            horizontalArrangement = Arrangement.Center,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Button(
                onClick = onHangup,
                colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error),
                shape = CircleShape,
                modifier = Modifier.size(64.dp)
            ) {
                Icon(
                    imageVector = Icons.Filled.CallEnd,
                    contentDescription = tr("call_hangup", "Hang up"),
                    tint = MaterialTheme.colorScheme.onError
                )
            }
        }

        if (error.isNotBlank()) {
            Card(
                modifier = Modifier
                    .align(Alignment.BottomStart)
                    .padding(16.dp)
            ) {
                Text(
                    text = error,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.error,
                    modifier = Modifier.padding(10.dp)
                )
            }
        }
    }
}

@Composable
private fun CallPermissionScreen(
    title: String,
    body: String,
    onRetry: () -> Unit,
    onHangup: () -> Unit
) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.surfaceVariant),
        contentAlignment = Alignment.Center
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(text = title, style = MaterialTheme.typography.titleLarge)
            Spacer(modifier = Modifier.height(12.dp))
            Text(text = body, style = MaterialTheme.typography.bodyMedium)
            Spacer(modifier = Modifier.height(20.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Button(onClick = onRetry) {
                    Text(tr("call_permission_retry", "Grant"))
                }
                Button(onClick = onHangup, colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error)) {
                    Text(tr("call_hangup", "Hang up"))
                }
            }
        }
    }
}

private fun hasPermissions(context: Context, permissions: Array<String>): Boolean {
    return permissions.all { perm ->
        ContextCompat.checkSelfPermission(context, perm) == PackageManager.PERMISSION_GRANTED
    }
}
