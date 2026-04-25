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
import androidx.camera.core.ImageProxy
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
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
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
import com.google.zxing.BarcodeFormat
import com.google.zxing.BinaryBitmap
import com.google.zxing.DecodeHintType
import com.google.zxing.MultiFormatReader
import com.google.zxing.PlanarYUVLuminanceSource
import com.google.zxing.common.HybridBinarizer
import com.google.zxing.qrcode.QRCodeWriter
import java.util.concurrent.Executors

@Composable
fun QrLoginDisplayScreen(
    sdk: SdkBridge,
    username: String,
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
        payload = sdk.beginQrLogin(username)
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
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = onBack) {
                Icon(MiOwnedIcons.ArrowBack, contentDescription = "Back")
            }
            Text(
                text = t("qr_login_title", "QR login"),
                style = MaterialTheme.typography.titleLarge
            )
        }
        SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(12.dp),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                if (!error.isNullOrBlank()) {
                    StatusBanner(
                        message = error ?: "",
                        icon = MiOwnedIcons.Alert,
                        tint = MaterialTheme.colorScheme.error
                    )
                } else {
                    StatusBanner(
                        message = status,
                        icon = MiOwnedIcons.QrCode,
                        tint = MaterialTheme.colorScheme.primary
                    )
                }
                if (!payload.isNullOrBlank()) {
                    val qrBitmap = rememberQrBitmap(payload ?: "", 208)
                    Image(
                        bitmap = qrBitmap,
                        contentDescription = "QR",
                        modifier = Modifier
                            .size(208.dp)
                            .clip(RoundedCornerShape(14.dp))
                    )
                } else {
                    Box(
                        modifier = Modifier
                            .size(208.dp)
                            .clip(RoundedCornerShape(14.dp))
                            .background(MaterialTheme.colorScheme.surfaceVariant),
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = t("qr_login_empty", "QR unavailable"),
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                }
                TextButton(onClick = onBack) {
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
                    Text(t("qr_login_root_body", "Enter the auth string (code:signature) from the Root Auth app."))
                    OutlinedTextField(
                        value = rootCodeInput,
                        onValueChange = { value ->
                            val cleaned = value.trim().filter {
                                it.isLetterOrDigit() || it == ':' || it == '|'
                            }
                            rootCodeInput = cleaned.take(160)
                        },
                        placeholder = { Text(t("qr_login_root_hint", "Auth string (code:signature)")) },
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Ascii,
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
                .padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = t("qr_scan_login_required", "Please sign in to approve login."),
                style = MaterialTheme.typography.titleMedium
            )
            TextButton(onClick = onBack) { Text(t("qr_scan_back", "Back")) }
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
    var rootCodeInput by remember { mutableStateOf("") }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            IconButton(onClick = onBack) {
                Icon(MiOwnedIcons.ArrowBack, contentDescription = "Back")
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

        SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                if (scanError != null) {
                    StatusBanner(
                        message = scanError ?: "",
                        icon = MiOwnedIcons.Alert,
                        tint = MaterialTheme.colorScheme.error
                    )
                } else if (approved) {
                    StatusBanner(
                        message = t("qr_scan_approved", "Login authorized"),
                        icon = MiOwnedIcons.CheckCircle,
                        tint = MaterialTheme.colorScheme.primary
                    )
                } else {
                    StatusBanner(
                        message = t("qr_scan_hint", "Align the QR code within the frame."),
                        icon = MiOwnedIcons.QrCode,
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
                        TextButton(onClick = {
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
            SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
                Column(
                    modifier = Modifier.fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text(
                        text = t("qr_scan_confirm", "Authorize new device login?"),
                        style = MaterialTheme.typography.titleMedium
                    )
                    payload.username?.takeIf { it.isNotBlank() }?.let { user ->
                        Text(
                            text = t("qr_scan_user", "Account: %s").format(user),
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                    Text(
                        text = t("qr_scan_device", "Device: %s").format(deviceLabel),
                        style = MaterialTheme.typography.bodyMedium
                    )
                    OutlinedTextField(
                        value = rootCodeInput,
                        onValueChange = { value ->
                            val cleaned = value.trim().filter {
                                it.isLetterOrDigit() || it == ':' || it == '|'
                            }
                            rootCodeInput = cleaned.take(160)
                        },
                        modifier = Modifier.fillMaxWidth(),
                        placeholder = { Text(t("qr_scan_root_hint", "Root auth string (code:signature) (optional)")) },
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Ascii,
                            imeAction = ImeAction.Done
                        ),
                        singleLine = true
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        SecondaryButton(
                            label = t("qr_scan_rescan", "Rescan"),
                            modifier = Modifier.weight(1f),
                            fillMaxWidth = false,
                            onClick = {
                                scanned = null
                                scanError = null
                                approved = false
                                rootCodeInput = ""
                                scanKey += 1
                            }
                        )
                        PrimaryButton(
                            label = t("qr_scan_approve", "Approve"),
                            modifier = Modifier.weight(1f),
                            fillMaxWidth = false,
                            onClick = {
                                val code = rootCodeInput.trim().ifBlank { null }
                                val ok = sdk.approveQrLogin(payload.qrId, payload.secret, code)
                                if (ok) {
                                    approved = true
                                    scanned = null
                                    rootCodeInput = ""
                                } else {
                                    scanError = sdk.lastError.ifBlank {
                                        t("qr_scan_failed", "Approval failed")
                                    }
                                }
                            }
                        )
                    }
                }
            }
        }

        Spacer(modifier = Modifier.weight(1f))
    }
}

@Composable
private fun PermissionNotice(title: String, body: String, onRetry: () -> Unit) {
    SurfaceSectionCard(modifier = Modifier.fillMaxWidth()) {
        Column(
            modifier = Modifier
                .fillMaxWidth(),
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Text(text = title, style = MaterialTheme.typography.titleMedium)
            Text(text = body, style = MaterialTheme.typography.bodyMedium)
            TextButton(onClick = onRetry) { Text(tr("qr_scan_retry", "Grant permission")) }
        }
    }
}

@Composable
private fun StatusBanner(message: String, icon: androidx.compose.ui.graphics.vector.ImageVector, tint: Color) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(999.dp))
            .background(tint.copy(alpha = 0.08f))
            .padding(horizontal = 10.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Icon(icon, contentDescription = null, tint = tint)
        Text(
            text = message,
            style = MaterialTheme.typography.labelLarge,
            color = MaterialTheme.colorScheme.onSurface,
            maxLines = 1
        )
    }
}

private fun decodeQrFromImageProxy(imageProxy: ImageProxy): String? {
    val plane = imageProxy.planes.firstOrNull() ?: return null
    val width = imageProxy.width
    val height = imageProxy.height
    if (width <= 0 || height <= 0) return null
    val buffer = plane.buffer.duplicate()
    val rowStride = plane.rowStride
    val pixelStride = plane.pixelStride
    val luma = ByteArray(width * height)
    var out = 0
    for (row in 0 until height) {
        val rowOffset = row * rowStride
        for (col in 0 until width) {
            val index = rowOffset + col * pixelStride
            if (index >= buffer.limit()) return null
            luma[out++] = buffer.get(index)
        }
    }
    val source = PlanarYUVLuminanceSource(luma, width, height, 0, 0, width, height, false)
    val bitmap = BinaryBitmap(HybridBinarizer(source))
    val reader = MultiFormatReader().apply {
        setHints(mapOf(DecodeHintType.POSSIBLE_FORMATS to listOf(BarcodeFormat.QR_CODE)))
    }
    return try {
        reader.decodeWithState(bitmap).text
    } catch (_: Exception) {
        null
    } finally {
        reader.reset()
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
    val mainHandler = remember { Handler(Looper.getMainLooper()) }
    var active by remember(restartKey) { mutableStateOf(true) }

    DisposableEffect(Unit) {
        onDispose {
            active = false
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
                val raw = decodeQrFromImageProxy(imageProxy)
                imageProxy.close()
                if (!active) return@setAnalyzer
                if (!raw.isNullOrBlank()) {
                    active = false
                    mainHandler.post { onQrFound(raw) }
                }
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
    val deviceId: String?,
    val username: String?
)

private fun parseQrLoginPayload(raw: String): QrLoginPayload? {
    return try {
        val uri = Uri.parse(raw)
        if (uri.scheme != "mi_e2ee" || uri.host != "qr-login") return null
        val id = uri.getQueryParameter("id") ?: return null
        val secret = uri.getQueryParameter("s") ?: return null
        val deviceId = uri.getQueryParameter("d")
        val username = uri.getQueryParameter("u")
        QrLoginPayload(id, secret, deviceId, username)
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
