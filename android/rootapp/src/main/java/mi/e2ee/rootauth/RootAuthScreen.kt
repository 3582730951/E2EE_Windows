package mi.e2ee.rootauth

import android.Manifest
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Handler
import android.os.Looper
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.Divider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.core.content.ContextCompat
import com.google.mlkit.vision.barcode.BarcodeScanning
import com.google.mlkit.vision.common.InputImage
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.Locale
import java.util.concurrent.Executors

@Composable
fun RootAuthScreen() {
    val context = LocalContext.current
    val store = remember { RootAuthStore(context) }
    val clipboard = LocalClipboardManager.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val scope = rememberCoroutineScope()

    var publicKey by remember { mutableStateOf(store.publicKeyHex()) }
    var code by remember { mutableStateOf("------") }
    var remaining by remember { mutableStateOf(0) }
    var manualDeviceId by remember { mutableStateOf("") }
    var status by remember { mutableStateOf("") }
    var statusError by remember { mutableStateOf(false) }
    var scanInfo by remember { mutableStateOf<ScanInfo?>(null) }
    var scanError by remember { mutableStateOf<String?>(null) }
    var approveStatus by remember { mutableStateOf("") }
    var approveError by remember { mutableStateOf("") }
    var approveBusy by remember { mutableStateOf(false) }
    var showScanner by remember { mutableStateOf(false) }
    var hasCameraPermission by remember {
        mutableStateOf(
            ContextCompat.checkSelfPermission(context, Manifest.permission.CAMERA) ==
                PackageManager.PERMISSION_GRANTED
        )
    }

    val permissionLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestPermission()
    ) { granted ->
        hasCameraPermission = granted
        if (granted) {
            showScanner = true
        }
    }

    LaunchedEffect(Unit) {
        while (true) {
            val result = store.currentCode()
            code = result.code
            remaining = result.remaining
            delay(1000)
        }
    }

    val manualAuthString = if (manualDeviceId.isBlank()) {
        ""
    } else {
        store.currentAuthString(manualDeviceId.trim(), "device_register") ?: ""
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Text(
            text = stringResource(id = R.string.title),
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.SemiBold
        )
        Divider()
        Text(
            text = stringResource(id = R.string.code_label),
            style = MaterialTheme.typography.labelLarge
        )
        Text(
            text = code,
            style = MaterialTheme.typography.displaySmall
        )
        Text(
            text = stringResource(id = R.string.remaining_label, remaining),
            style = MaterialTheme.typography.bodyMedium
        )
        if (status.isNotBlank()) {
            Text(
                text = status,
                color = if (statusError) MaterialTheme.colorScheme.error
                else MaterialTheme.colorScheme.primary
            )
        }
        Spacer(modifier = Modifier.height(8.dp))
        OutlinedTextField(
            value = publicKey,
            onValueChange = {},
            label = { Text(stringResource(id = R.string.pubkey_label)) },
            modifier = Modifier.fillMaxWidth(),
            readOnly = true
        )
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Button(onClick = {
                if (code != "------") {
                    clipboard.setText(AnnotatedString(code))
                }
            }) {
                Text(stringResource(id = R.string.copy_code))
            }
            OutlinedButton(onClick = {
                if (publicKey.isNotBlank()) {
                    clipboard.setText(AnnotatedString(publicKey))
                }
            }) {
                Text(stringResource(id = R.string.copy_pubkey))
            }
            OutlinedButton(onClick = {
                publicKey = store.regenerateKeyPair()
                status = context.getString(R.string.key_regenerated)
                statusError = false
            }) {
                Text(stringResource(id = R.string.regen_key))
            }
        }

        Divider(modifier = Modifier.padding(top = 8.dp))
        Text(
            text = stringResource(id = R.string.manual_title),
            style = MaterialTheme.typography.titleMedium
        )
        Text(
            text = stringResource(id = R.string.manual_hint),
            style = MaterialTheme.typography.bodyMedium
        )
        OutlinedTextField(
            value = manualDeviceId,
            onValueChange = { manualDeviceId = it },
            label = { Text(stringResource(id = R.string.manual_device_label)) },
            modifier = Modifier.fillMaxWidth()
        )
        if (manualAuthString.isNotBlank()) {
            Text(
                text = manualAuthString,
                style = MaterialTheme.typography.bodySmall
            )
        } else {
            Text(
                text = stringResource(id = R.string.manual_auth_placeholder),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.secondary
            )
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(onClick = {
                if (manualAuthString.isNotBlank()) {
                    clipboard.setText(AnnotatedString(manualAuthString))
                }
            }) {
                Text(stringResource(id = R.string.copy_auth_string))
            }
        }

        Divider(modifier = Modifier.padding(top = 8.dp))
        Text(
            text = stringResource(id = R.string.scan_title),
            style = MaterialTheme.typography.titleMedium
        )
        Text(
            text = stringResource(id = R.string.scan_hint),
            style = MaterialTheme.typography.bodyMedium
        )
        Button(onClick = {
            scanError = null
            scanInfo = null
            approveStatus = ""
            approveError = ""
            if (hasCameraPermission) {
                showScanner = true
            } else {
                permissionLauncher.launch(Manifest.permission.CAMERA)
            }
        }) {
            Text(stringResource(id = R.string.scan_button))
        }

        scanInfo?.let { info ->
            val codeValue = if (code == "------") "" else code
            val qrContext = buildQrContext(info.qrId, info.secretHex)
            val authString = if (!info.deviceId.isNullOrBlank()) {
                store.currentAuthString(info.deviceId!!, qrContext)
            } else {
                null
            }
            val canApprove = !authString.isNullOrBlank() &&
                !info.username.isNullOrBlank() &&
                !info.host.isNullOrBlank() &&
                info.port != null &&
                !info.deviceId.isNullOrBlank()
            Surface(
                shape = RoundedCornerShape(12.dp),
                color = MaterialTheme.colorScheme.surfaceVariant
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(12.dp),
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Text(text = info.title, style = MaterialTheme.typography.titleSmall)
                    info.detail?.let { Text(text = it, style = MaterialTheme.typography.bodySmall) }
                    info.username?.let {
                        Text(text = stringResource(id = R.string.scan_user, it),
                            style = MaterialTheme.typography.bodySmall)
                    }
                    if (!info.host.isNullOrBlank() && info.port != null) {
                        Text(
                            text = stringResource(id = R.string.scan_host, info.host!!, info.port!!),
                            style = MaterialTheme.typography.bodySmall
                        )
                        Text(
                            text = stringResource(
                                id = if (info.useTls) R.string.scan_tls_on else R.string.scan_tls_off
                            ),
                            style = MaterialTheme.typography.bodySmall
                        )
                        info.fingerprint?.let {
                            Text(
                                text = stringResource(id = R.string.scan_fingerprint, it),
                                style = MaterialTheme.typography.bodySmall
                            )
                        }
                    } else {
                        Text(
                            text = stringResource(id = R.string.scan_missing_info),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.error
                        )
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedButton(onClick = {
                            if (code != "------") {
                                clipboard.setText(AnnotatedString(code))
                            }
                        }) {
                            Text(stringResource(id = R.string.copy_code))
                        }
                        if (!authString.isNullOrBlank()) {
                            OutlinedButton(onClick = {
                                clipboard.setText(AnnotatedString(authString))
                            }) {
                                Text(stringResource(id = R.string.copy_auth_string))
                            }
                        }
                    }
                    if (codeValue.isBlank()) {
                        Text(
                            text = stringResource(id = R.string.scan_missing_key),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.error
                        )
                    }
                    OutlinedButton(
                        onClick = {
                            approveBusy = true
                            approveStatus = context.getString(R.string.scan_approve_progress)
                            approveError = ""
                            scope.launch {
                                val result = withContext(Dispatchers.IO) {
                                    QrLoginApproveClient.approve(
                                        username = info.username!!,
                                        qrId = info.qrId,
                                        secretHex = info.secretHex,
                                        deviceId = info.deviceId!!,
                                        rootCode = authString!!,
                                        host = info.host!!,
                                        port = info.port!!,
                                        useTls = info.useTls,
                                        fingerprint = info.fingerprint
                                    )
                                }
                                approveBusy = false
                                if (result.success) {
                                    approveStatus = context.getString(R.string.scan_approve_ok)
                                    approveError = ""
                                } else {
                                    approveStatus = ""
                                    val err = result.error ?: context.getString(
                                        R.string.scan_approve_unknown
                                    )
                                    approveError = context.getString(R.string.scan_approve_failed, err)
                                }
                            }
                        },
                        enabled = canApprove && !approveBusy
                    ) {
                        Text(stringResource(id = R.string.scan_approve))
                    }
                    if (approveStatus.isNotBlank()) {
                        Text(
                            text = approveStatus,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.primary
                        )
                    }
                    if (approveError.isNotBlank()) {
                        Text(
                            text = approveError,
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.error
                        )
                    }
                }
            }
        }

        scanError?.let { err ->
            Text(text = err, color = MaterialTheme.colorScheme.error)
        }
    }

    if (showScanner) {
        Dialog(onDismissRequest = { showScanner = false }) {
            Surface(shape = RoundedCornerShape(16.dp)) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(12.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    QrScannerView(lifecycleOwner) { raw ->
                        val parsed = parseScanPayload(raw)
                        when (parsed) {
                            is ScanPayload.QrLogin -> {
                                val detail = parsed.deviceId?.let { id ->
                                    context.getString(R.string.scan_device, id)
                                }
                                scanInfo = ScanInfo(
                                    context.getString(R.string.scan_login_title),
                                    detail,
                                    parsed.qrId,
                                    parsed.secretHex,
                                    parsed.deviceId,
                                    parsed.username,
                                    parsed.host,
                                    parsed.port,
                                    parsed.useTls,
                                    parsed.fingerprint
                                )
                                approveStatus = ""
                                approveError = ""
                            }
                            null -> {
                                scanError = context.getString(R.string.scan_invalid)
                                approveStatus = ""
                                approveError = ""
                            }
                        }
                        showScanner = false
                    }
                    OutlinedButton(onClick = { showScanner = false }) {
                        Text(stringResource(id = R.string.scan_close))
                    }
                }
            }
        }
    }
}

private data class ScanInfo(
    val title: String,
    val detail: String?,
    val qrId: String,
    val secretHex: String,
    val deviceId: String?,
    val username: String?,
    val host: String?,
    val port: Int?,
    val useTls: Boolean,
    val fingerprint: String?
)

private sealed class ScanPayload {
    data class QrLogin(
        val qrId: String,
        val secretHex: String,
        val deviceId: String?,
        val username: String?,
        val host: String?,
        val port: Int?,
        val useTls: Boolean,
        val fingerprint: String?
    ) : ScanPayload()
}

private fun parseScanPayload(raw: String): ScanPayload? {
    return try {
        val uri = Uri.parse(raw)
        if (uri.scheme != "mi_e2ee") {
            return null
        }
        if (uri.host == "qr-login") {
            val id = uri.getQueryParameter("id") ?: return null
            val secret = uri.getQueryParameter("s") ?: return null
            val deviceId = uri.getQueryParameter("d")
            val username = uri.getQueryParameter("u")
            val host = uri.getQueryParameter("h")
            val port = uri.getQueryParameter("p")?.toIntOrNull()
            val tlsParam = uri.getQueryParameter("tls")?.toIntOrNull()
            val useTls = tlsParam?.let { it != 0 } ?: true
            val fingerprint = uri.getQueryParameter("fp")
            return ScanPayload.QrLogin(id, secret, deviceId, username, host, port, useTls, fingerprint)
        }
        null
    } catch (_: Exception) {
        null
    }
}

private fun buildQrContext(qrId: String, secretHex: String): String {
    val clean = secretHex.trim().lowercase(Locale.US)
    return "qr:$qrId:$clean"
}

@Composable
private fun QrScannerView(
    lifecycleOwner: androidx.lifecycle.LifecycleOwner,
    onQrFound: (String) -> Unit
) {
    val context = LocalContext.current
    val previewView = remember {
        PreviewView(context).apply {
            scaleType = PreviewView.ScaleType.FILL_CENTER
        }
    }
    val executor = remember { Executors.newSingleThreadExecutor() }
    val scanner = remember { BarcodeScanning.getClient() }
    val mainHandler = remember { Handler(Looper.getMainLooper()) }
    var active by remember { mutableStateOf(true) }

    DisposableEffect(Unit) {
        onDispose {
            active = false
            scanner.close()
            executor.shutdown()
        }
    }

    LaunchedEffect(Unit) {
        val cameraProviderFuture = ProcessCameraProvider.getInstance(context)
        cameraProviderFuture.addListener({
            val cameraProvider = cameraProviderFuture.get()
            val preview = Preview.Builder().build().also {
                it.setSurfaceProvider(previewView.surfaceProvider)
            }
            val analysis = ImageAnalysis.Builder()
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .build()
            analysis.setAnalyzer(executor) { imageProxy ->
                if (!active) {
                    imageProxy.close()
                    return@setAnalyzer
                }
                val mediaImage = imageProxy.image
                if (mediaImage == null) {
                    imageProxy.close()
                    return@setAnalyzer
                }
                val image = InputImage.fromMediaImage(
                    mediaImage,
                    imageProxy.imageInfo.rotationDegrees
                )
                scanner.process(image)
                    .addOnSuccessListener { barcodes ->
                        if (!active) return@addOnSuccessListener
                        val raw = barcodes.firstOrNull()?.rawValue
                        if (!raw.isNullOrBlank()) {
                            active = false
                            mainHandler.post { onQrFound(raw) }
                        }
                    }
                    .addOnCompleteListener { imageProxy.close() }
            }
            cameraProvider.unbindAll()
            cameraProvider.bindToLifecycle(
                lifecycleOwner,
                CameraSelector.DEFAULT_BACK_CAMERA,
                preview,
                analysis
            )
        }, ContextCompat.getMainExecutor(context))
    }

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .aspectRatio(1f)
            .background(Color.Black)
            .padding(4.dp)
    ) {
        androidx.compose.ui.viewinterop.AndroidView(
            factory = { previewView },
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black)
        )
    }
}
