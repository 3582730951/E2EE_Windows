package mi.e2ee.android.ui

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
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
import androidx.compose.foundation.Image
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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Error
import androidx.compose.material.icons.filled.QrCode
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
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
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import com.google.mlkit.vision.barcode.BarcodeScanning
import com.google.mlkit.vision.common.InputImage
import com.google.zxing.BarcodeFormat
import com.google.zxing.qrcode.QRCodeWriter
import java.util.concurrent.Executors

@Composable
fun QrLoginDisplayScreen(
    sdk: SdkBridge,
    onBack: () -> Unit,
    onLoggedIn: () -> Unit
) {
    val strings = LocalStrings.current
    fun t(key: String, fallback: String): String = strings.get(key, fallback)
    var payload by remember { mutableStateOf<String?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    var status by remember { mutableStateOf("") }
    var showRootDialog by remember { mutableStateOf(false) }
    var rootCodeInput by remember { mutableStateOf("") }
    var rootError by remember { mutableStateOf<String?>(null) }

    LaunchedEffect(Unit) {
        payload = sdk.beginQrLogin()
        if (payload == null) {
            error = t("qr_login_init_failed", "Failed to start QR login")
        }
    }

    DisposableEffect(Unit) {
        onDispose { sdk.cancelQrLogin() }
    }

    LaunchedEffect(payload) {
        if (payload.isNullOrBlank()) return@LaunchedEffect
        status = t("qr_login_wait", "Waiting for approval...")
        while (true) {
            val state = sdk.pollQrLogin()
            if (state < 0) {
                error = sdk.lastError.ifBlank {
                    t("qr_login_poll_failed", "QR login failed")
                }
                break
            }
            if (state == 1) {
                sdk.cancelQrLogin()
                if (sdk.registerDevice("")) {
                    onLoggedIn()
                } else {
                    val err = sdk.lastError
                    if (err.contains("root auth")) {
                        rootCodeInput = ""
                        rootError = null
                        showRootDialog = true
                    } else {
                        error = err.ifBlank {
                            t("qr_login_register_failed", "Device registration failed")
                        }
                        sdk.logout()
                    }
                }
                break
            }
            kotlinx.coroutines.delay(1200)
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = onBack) {
                Icon(Icons.Filled.ArrowBack, contentDescription = "Back")
            }
            Text(
                text = t("qr_login_title", "QR login"),
                style = MaterialTheme.typography.titleLarge
            )
        }
        Card(
            shape = RoundedCornerShape(20.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                if (!error.isNullOrBlank()) {
                    StatusBanner(
                        message = error ?: "",
                        icon = Icons.Filled.Error,
                        tint = MaterialTheme.colorScheme.error
                    )
                } else {
                    StatusBanner(
                        message = status,
                        icon = Icons.Filled.QrCode,
                        tint = MaterialTheme.colorScheme.primary
                    )
                }
                if (!payload.isNullOrBlank()) {
                    val qrBitmap = rememberQrBitmap(payload ?: "", 240)
                    Image(
                        bitmap = qrBitmap,
                        contentDescription = "QR",
                        modifier = Modifier
                            .size(240.dp)
                            .clip(RoundedCornerShape(16.dp))
                    )
                } else {
                    Box(
                        modifier = Modifier
                            .size(240.dp)
                            .clip(RoundedCornerShape(16.dp))
                            .background(MaterialTheme.colorScheme.surfaceVariant),
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = t("qr_login_empty", "QR unavailable"),
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                }
                OutlinedButton(onClick = onBack) {
                    Text(t("qr_login_cancel", "Cancel"))
                }
            }
        }
        Spacer(modifier = Modifier.weight(1f))
    }

    if (showRootDialog) {
        AlertDialog(
            onDismissRequest = {},
            title = { Text(t("qr_login_root_title", "Root auth required")) },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(t("qr_login_root_body", "Enter the 6-digit code from the Root Auth app."))
                    OutlinedTextField(
                        value = rootCodeInput,
                        onValueChange = { value ->
                            rootCodeInput = value.filter { it.isDigit() }.take(6)
                        },
                        placeholder = { Text(t("qr_login_root_hint", "6-digit code")) },
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Number,
                            imeAction = ImeAction.Done
                        ),
                        singleLine = true
                    )
                    if (!rootError.isNullOrBlank()) {
                        Text(
                            text = rootError ?: "",
                            color = MaterialTheme.colorScheme.error,
                            style = MaterialTheme.typography.bodySmall
                        )
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    val ok = sdk.registerDevice(rootCodeInput.trim())
                    if (ok) {
                        showRootDialog = false
                        onLoggedIn()
                    } else {
                        rootError = sdk.lastError.ifBlank {
                            t("qr_login_root_failed", "Root auth failed")
                        }
                    }
                }) {
                    Text(t("qr_login_root_confirm", "Confirm"))
                }
            },
            dismissButton = {
                TextButton(onClick = {
                    showRootDialog = false
                    sdk.logout()
                    onBack()
                }) {
                    Text(t("qr_login_root_cancel", "Cancel"))
                }
            }
        )
    }
}

@Composable
fun QrLoginScanScreen(
    sdk: SdkBridge,
    onBack: () -> Unit
) {
    val strings = LocalStrings.current
    fun t(key: String, fallback: String): String = strings.get(key, fallback)
    if (!sdk.loggedIn) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(24.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = t("qr_scan_login_required", "Please sign in to approve login."),
                style = MaterialTheme.typography.titleMedium
            )
            Button(onClick = onBack) {
                Text(t("qr_scan_back", "Back"))
            }
        }
        return
    }

    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val required = remember { arrayOf(Manifest.permission.CAMERA) }
    var granted by remember { mutableStateOf(hasPermissions(context, required)) }
    val launcher = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result ->
        granted = required.all { result[it] == true }
    }

    LaunchedEffect(Unit) {
        if (!granted) {
            launcher.launch(required)
        }
    }

    var scanned by remember { mutableStateOf<QrLoginPayload?>(null) }
    var scanError by remember { mutableStateOf<String?>(null) }
    var approved by remember { mutableStateOf(false) }
    var scanKey by remember { mutableStateOf(0) }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = onBack) {
                Icon(Icons.Filled.ArrowBack, contentDescription = "Back")
            }
            Text(
                text = t("qr_scan_title", "Scan QR"),
                style = MaterialTheme.typography.titleLarge
            )
        }

        if (!granted) {
            PermissionNotice(
                title = t("qr_scan_permission", "Camera permission needed"),
                body = t("qr_scan_permission_body", "Allow camera access to scan the QR code."),
                onRetry = { launcher.launch(required) }
            )
            Spacer(modifier = Modifier.weight(1f))
            return
        }

        Card(
            shape = RoundedCornerShape(20.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                if (scanError != null) {
                    StatusBanner(
                        message = scanError ?: "",
                        icon = Icons.Filled.Error,
                        tint = MaterialTheme.colorScheme.error
                    )
                } else if (approved) {
                    StatusBanner(
                        message = t("qr_scan_approved", "Login authorized"),
                        icon = Icons.Filled.CheckCircle,
                        tint = MaterialTheme.colorScheme.primary
                    )
                } else {
                    StatusBanner(
                        message = t("qr_scan_hint", "Align the QR code within the frame."),
                        icon = Icons.Filled.QrCode,
                        tint = MaterialTheme.colorScheme.primary
                    )
                }

                if (!approved) {
                    QrScannerView(
                        lifecycleOwner = lifecycleOwner,
                        restartKey = scanKey,
                        onQrFound = { raw ->
                            val parsed = parseQrLoginPayload(raw)
                            if (parsed == null) {
                                scanError = t("qr_scan_invalid", "Invalid QR code")
                            } else {
                                scanError = null
                                scanned = parsed
                            }
                        }
                    )
                    if (scanError != null) {
                        Spacer(modifier = Modifier.height(8.dp))
                        OutlinedButton(onClick = {
                            scanError = null
                            scanned = null
                            approved = false
                            scanKey += 1
                        }) {
                            Text(t("qr_scan_rescan", "Rescan"))
                        }
                    }
                }
            }
        }

        scanned?.let { payload ->
            val deviceLabel = payload.deviceId?.takeLast(8) ?: "?"
            Card(
                shape = RoundedCornerShape(18.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text(
                        text = t("qr_scan_confirm", "Authorize new device login?"),
                        style = MaterialTheme.typography.titleMedium
                    )
                    Text(
                        text = t("qr_scan_device", "Device: %s").format(deviceLabel),
                        style = MaterialTheme.typography.bodyMedium
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        OutlinedButton(onClick = {
                            scanned = null
                            scanError = null
                            approved = false
                            scanKey += 1
                        }) {
                            Text(t("qr_scan_rescan", "Rescan"))
                        }
                        Button(
                            onClick = {
                                val ok = sdk.approveQrLogin(payload.qrId, payload.secret)
                                if (ok) {
                                    approved = true
                                    scanned = null
                                } else {
                                    scanError = sdk.lastError.ifBlank {
                                        t("qr_scan_failed", "Approval failed")
                                    }
                                }
                            },
                            colors = ButtonDefaults.buttonColors(
                                containerColor = MaterialTheme.colorScheme.primary
                            )
                        ) {
                            Text(t("qr_scan_approve", "Approve"))
                        }
                    }
                }
            }
        }

        Spacer(modifier = Modifier.weight(1f))
    }
}

@Composable
private fun PermissionNotice(title: String, body: String, onRetry: () -> Unit) {
    Card(
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Text(text = title, style = MaterialTheme.typography.titleMedium)
            Text(text = body, style = MaterialTheme.typography.bodyMedium)
            Button(onClick = onRetry) {
                Text(tr("qr_scan_retry", "Grant permission"))
            }
        }
    }
}

@Composable
private fun StatusBanner(message: String, icon: androidx.compose.ui.graphics.vector.ImageVector, tint: Color) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(tint.copy(alpha = 0.12f))
            .padding(horizontal = 12.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Icon(icon, contentDescription = null, tint = tint)
        Text(text = message, style = MaterialTheme.typography.bodyMedium)
    }
}

@Composable
private fun QrScannerView(
    lifecycleOwner: androidx.lifecycle.LifecycleOwner,
    restartKey: Int,
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
    var active by remember(restartKey) { mutableStateOf(true) }

    DisposableEffect(Unit) {
        onDispose {
            active = false
            scanner.close()
            executor.shutdown()
        }
    }

    LaunchedEffect(restartKey) {
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
            .clip(RoundedCornerShape(16.dp))
            .background(Color.Black)
    ) {
        androidx.compose.ui.viewinterop.AndroidView(
            factory = { previewView },
            modifier = Modifier.fillMaxSize()
        )
    }
}

private data class QrLoginPayload(
    val qrId: String,
    val secret: String,
    val deviceId: String?
)

private fun parseQrLoginPayload(raw: String): QrLoginPayload? {
    return try {
        val uri = Uri.parse(raw)
        if (uri.scheme != "mi_e2ee" || uri.host != "qr-login") return null
        val id = uri.getQueryParameter("id") ?: return null
        val secret = uri.getQueryParameter("s") ?: return null
        val deviceId = uri.getQueryParameter("d")
        QrLoginPayload(id, secret, deviceId)
    } catch (_: Exception) {
        null
    }
}

private fun hasPermissions(context: Context, permissions: Array<String>): Boolean {
    return permissions.all { perm ->
        ContextCompat.checkSelfPermission(context, perm) == PackageManager.PERMISSION_GRANTED
    }
}

@Composable
private fun rememberQrBitmap(payload: String, sizeDp: Int): androidx.compose.ui.graphics.ImageBitmap {
    val sizePx = with(androidx.compose.ui.platform.LocalDensity.current) { sizeDp.dp.roundToPx() }
    return remember(payload, sizePx) {
        val writer = QRCodeWriter()
        val matrix = writer.encode(payload, BarcodeFormat.QR_CODE, sizePx, sizePx)
        val bmp = Bitmap.createBitmap(sizePx, sizePx, Bitmap.Config.ARGB_8888)
        for (x in 0 until sizePx) {
            for (y in 0 until sizePx) {
                bmp.setPixel(x, y, if (matrix.get(x, y)) 0xFF000000.toInt() else 0xFFFFFFFF.toInt())
            }
        }
        bmp.asImageBitmap()
    }
}
