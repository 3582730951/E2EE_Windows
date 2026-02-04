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
import kotlinx.coroutines.delay
import java.util.Locale
import java.util.concurrent.Executors

@Composable
fun RootAuthScreen() {
    val context = LocalContext.current
    val store = remember { RootAuthStore(context) }
    val clipboard = LocalClipboardManager.current
    val lifecycleOwner = LocalLifecycleOwner.current

    var secretInput by remember { mutableStateOf(store.loadSecret().orEmpty()) }
    var savedSecret by remember { mutableStateOf(store.loadSecret().orEmpty()) }
    var code by remember { mutableStateOf("------") }
    var remaining by remember { mutableStateOf(0) }
    var status by remember { mutableStateOf("") }
    var statusError by remember { mutableStateOf(false) }
    var scanInfo by remember { mutableStateOf<ScanInfo?>(null) }
    var scanError by remember { mutableStateOf<String?>(null) }
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

    LaunchedEffect(savedSecret) {
        while (true) {
            val result = store.currentCode(savedSecret)
            code = result.code
            remaining = result.remaining
            delay(1000)
        }
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
            value = secretInput,
            onValueChange = { secretInput = it },
            label = { Text(stringResource(id = R.string.secret_hint)) },
            modifier = Modifier.fillMaxWidth()
        )
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            Button(onClick = {
                if (store.saveSecret(secretInput)) {
                    savedSecret = secretInput.trim().lowercase(Locale.US)
                    status = context.getString(R.string.secret_saved)
                    statusError = false
                } else {
                    status = context.getString(R.string.secret_invalid)
                    statusError = true
                }
            }) {
                Text(stringResource(id = R.string.save))
            }
            OutlinedButton(onClick = {
                store.clearSecret()
                secretInput = ""
                savedSecret = ""
                status = context.getString(R.string.secret_cleared)
                statusError = false
            }) {
                Text(stringResource(id = R.string.clear))
            }
            OutlinedButton(onClick = {
                if (code != "------") {
                    clipboard.setText(AnnotatedString(code))
                }
            }) {
                Text(stringResource(id = R.string.copy_code))
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
            if (hasCameraPermission) {
                showScanner = true
            } else {
                permissionLauncher.launch(Manifest.permission.CAMERA)
            }
        }) {
            Text(stringResource(id = R.string.scan_button))
        }

        scanInfo?.let { info ->
            val proof = if (savedSecret.isNotBlank() && !info.deviceId.isNullOrBlank()) {
                store.currentProof(savedSecret, info.deviceId!!)
            } else {
                null
            }
            val authString = if (!proof.isNullOrBlank() && code != "------") {
                "$code:$proof"
            } else {
                null
            }
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
                            is ScanPayload.RootSecret -> {
                                if (store.saveSecret(parsed.secret)) {
                                    savedSecret = parsed.secret
                                    secretInput = parsed.secret
                                    status = context.getString(R.string.secret_saved)
                                    statusError = false
                                } else {
                                    status = context.getString(R.string.secret_invalid)
                                    statusError = true
                                }
                            }
                            is ScanPayload.QrLogin -> {
                                val detail = parsed.deviceId?.let { id ->
                                    context.getString(R.string.scan_device, id)
                                }
                                scanInfo = ScanInfo(
                                    context.getString(R.string.scan_login_title),
                                    detail,
                                    parsed.deviceId
                                )
                            }
                            null -> {
                                scanError = context.getString(R.string.scan_invalid)
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
    val deviceId: String?
)

private sealed class ScanPayload {
    data class RootSecret(val secret: String) : ScanPayload()
    data class QrLogin(val qrId: String, val deviceId: String?) : ScanPayload()
}

private fun parseScanPayload(raw: String): ScanPayload? {
    return try {
        val uri = Uri.parse(raw)
        if (uri.scheme != "mi_e2ee") {
            return null
        }
        if (uri.host == "root-auth") {
            val secret = uri.getQueryParameter("secret") ?: return null
            return ScanPayload.RootSecret(secret)
        }
        if (uri.host == "qr-login") {
            val id = uri.getQueryParameter("id") ?: return null
            val deviceId = uri.getQueryParameter("d")
            return ScanPayload.QrLogin(id, deviceId)
        }
        null
    } catch (_: Exception) {
        null
    }
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
