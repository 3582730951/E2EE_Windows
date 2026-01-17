package mi.e2ee.android.ui

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.ImageFormat
import android.graphics.Rect
import android.graphics.YuvImage
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.AudioTrack
import android.media.MediaRecorder
import android.os.SystemClock
import androidx.camera.core.CameraSelector
import androidx.camera.core.ImageAnalysis
import androidx.camera.core.ImageProxy
import androidx.camera.core.Preview
import androidx.camera.lifecycle.ProcessCameraProvider
import androidx.camera.view.PreviewView
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.content.ContextCompat
import androidx.lifecycle.LifecycleOwner
import java.io.ByteArrayOutputStream
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors
import kotlin.math.max
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import mi.e2ee.android.sdk.MediaConfig
import mi.e2ee.android.sdk.MediaVideoFrame
import mi.e2ee.android.sdk.NativeMediaEngine

class CallMediaController(
    private val context: Context,
    private val sdk: SdkBridge
) {
    var lastError by mutableStateOf("")
        private set
    var remoteVideo by mutableStateOf<Bitmap?>(null)
        private set
    var running by mutableStateOf(false)
        private set

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var engineHandle: Long = 0L
    private var currentCallKey: String? = null
    private var audioRecord: AudioRecord? = null
    private var audioTrack: AudioTrack? = null
    private var pollJob: Job? = null
    private var captureJob: Job? = null
    private var playbackJob: Job? = null
    private var videoJob: Job? = null
    private var cameraProvider: ProcessCameraProvider? = null
    private var preview: Preview? = null
    private var analysis: ImageAnalysis? = null
    private var analysisExecutor: ExecutorService? = null
    private var frameSamples: Int = 0
    private var videoEnabled: Boolean = false
    private var lastVideoNs: Long = 0L
    private var maxPackets: Int = 32
    private var waitMs: Int = 0
    private var videoWidth: Int = 640
    private var videoHeight: Int = 360
    private var videoFps: Int = 24
    private var nv12Buffer: ByteArray? = null

    fun startPeerCall(call: PeerCallState, lifecycleOwner: LifecycleOwner, previewView: PreviewView?) {
        val callKey = "peer:${call.peerUsername}:${call.callIdHex}"
        if (!ensureStart(callKey, call.video, isGroup = false)) {
            return
        }
        val clientHandle = sdk.clientHandle()
        if (clientHandle == 0L) {
            lastError = "SDK not initialized"
            return
        }
        val handle = NativeMediaEngine.createPeerEngine(
            clientHandle,
            call.peerUsername,
            call.callId,
            call.initiator,
            call.video,
            48000,
            1,
            20,
            videoWidth,
            videoHeight,
            videoFps
        )
        if (!startEngine(handle, call.video, lifecycleOwner, previewView)) {
            return
        }
    }

    fun startGroupCall(call: GroupCallState, lifecycleOwner: LifecycleOwner, previewView: PreviewView?) {
        val callKey = "group:${call.groupId}:${call.callIdHex}:${call.keyId}"
        if (!ensureStart(callKey, call.video, isGroup = true)) {
            return
        }
        val clientHandle = sdk.clientHandle()
        if (clientHandle == 0L) {
            lastError = "SDK not initialized"
            return
        }
        val handle = NativeMediaEngine.createGroupEngine(
            clientHandle,
            call.groupId,
            call.callId,
            call.keyId,
            call.video,
            48000,
            1,
            20,
            videoWidth,
            videoHeight,
            videoFps
        )
        if (!startEngine(handle, call.video, lifecycleOwner, previewView)) {
            return
        }
    }

    fun stop() {
        running = false
        pollJob?.cancel()
        pollJob = null
        captureJob?.cancel()
        captureJob = null
        playbackJob?.cancel()
        playbackJob = null
        videoJob?.cancel()
        videoJob = null
        audioRecord?.run {
            runCatching { stop() }
            release()
        }
        audioRecord = null
        audioTrack?.run {
            runCatching { pause() }
            runCatching { flush() }
            release()
        }
        audioTrack = null
        stopCamera()
        if (engineHandle != 0L) {
            NativeMediaEngine.destroyEngine(engineHandle)
        }
        engineHandle = 0L
        currentCallKey = null
        remoteVideo = null
        if (sdk.clientHandle() != 0L) {
            sdk.clearMediaSubscriptions()
        }
    }

    private fun ensureStart(callKey: String, enableVideo: Boolean, isGroup: Boolean): Boolean {
        if (!NativeMediaEngine.available) {
            lastError = "Native media engine not available"
            return false
        }
        if (running && currentCallKey == callKey) {
            return false
        }
        stop()
        currentCallKey = callKey
        videoEnabled = enableVideo
        lastError = ""
        lastVideoNs = 0L
        val config = sdk.getMediaConfig()
        applyMediaConfig(config, isGroup = isGroup)
        return true
    }

    private fun applyMediaConfig(config: MediaConfig?, isGroup: Boolean) {
        if (config == null) {
            maxPackets = 32
            waitMs = 0
            return
        }
        if (isGroup) {
            maxPackets = if (config.groupPullMaxPackets > 0) config.groupPullMaxPackets else 32
            waitMs = if (config.groupPullWaitMs >= 0) config.groupPullWaitMs else 0
        } else {
            maxPackets = if (config.pullMaxPackets > 0) config.pullMaxPackets else 32
            waitMs = if (config.pullWaitMs >= 0) config.pullWaitMs else 0
        }
    }

    private fun startEngine(
        handle: Long,
        enableVideo: Boolean,
        lifecycleOwner: LifecycleOwner,
        previewView: PreviewView?
    ): Boolean {
        if (handle == 0L) {
            lastError = "Media engine init failed"
            currentCallKey = null
            return false
        }
        engineHandle = handle
        frameSamples = NativeMediaEngine.getAudioFrameSamples(handle).takeIf { it > 0 } ?: 960
        if (!startAudio()) {
            NativeMediaEngine.destroyEngine(handle)
            engineHandle = 0L
            currentCallKey = null
            return false
        }
        running = true
        startPollLoop()
        startPlaybackLoop()
        if (enableVideo) {
            if (previewView != null) {
                startCamera(lifecycleOwner, previewView)
            }
            startVideoLoop()
        }
        return true
    }

    private fun startAudio(): Boolean {
        val sampleRate = 48000
        val channelIn = AudioFormat.CHANNEL_IN_MONO
        val channelOut = AudioFormat.CHANNEL_OUT_MONO
        val encoding = AudioFormat.ENCODING_PCM_16BIT
        val minIn = AudioRecord.getMinBufferSize(sampleRate, channelIn, encoding)
        val frameBytes = frameSamples * 2
        val inBuffer = max(minIn, frameBytes * 2)
        val record = AudioRecord.Builder()
            .setAudioSource(MediaRecorder.AudioSource.VOICE_COMMUNICATION)
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(encoding)
                    .setSampleRate(sampleRate)
                    .setChannelMask(channelIn)
                    .build()
            )
            .setBufferSizeInBytes(inBuffer)
            .build()
        if (record.state != AudioRecord.STATE_INITIALIZED) {
            lastError = "AudioRecord init failed"
            return false
        }
        val minOut = AudioTrack.getMinBufferSize(sampleRate, channelOut, encoding)
        val outBuffer = max(minOut, frameBytes * 2)
        val track = AudioTrack.Builder()
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                    .build()
            )
            .setAudioFormat(
                AudioFormat.Builder()
                    .setEncoding(encoding)
                    .setSampleRate(sampleRate)
                    .setChannelMask(channelOut)
                    .build()
            )
            .setBufferSizeInBytes(outBuffer)
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
        if (track.state != AudioTrack.STATE_INITIALIZED) {
            lastError = "AudioTrack init failed"
            record.release()
            return false
        }
        audioRecord = record
        audioTrack = track
        record.startRecording()
        track.play()
        startCaptureLoop()
        return true
    }

    private fun startCaptureLoop() {
        val record = audioRecord ?: return
        val handle = engineHandle
        val buffer = ShortArray(frameSamples)
        captureJob = scope.launch {
            while (isActive && running) {
                var offset = 0
                while (offset < buffer.size && isActive && running) {
                    val read = record.read(buffer, offset, buffer.size - offset)
                    if (read > 0) {
                        offset += read
                    } else {
                        break
                    }
                }
                if (offset == buffer.size) {
                    NativeMediaEngine.sendPcm(handle, buffer)
                } else {
                    delay(5)
                }
            }
        }
    }

    private fun startPlaybackLoop() {
        val track = audioTrack ?: return
        val handle = engineHandle
        playbackJob = scope.launch {
            while (isActive && running) {
                val frame = NativeMediaEngine.popAudio(handle)
                if (frame != null && frame.isNotEmpty()) {
                    track.write(frame, 0, frame.size)
                } else {
                    delay(10)
                }
            }
        }
    }

    private fun startPollLoop() {
        val handle = engineHandle
        pollJob = scope.launch {
            while (isActive && running) {
                val ok = NativeMediaEngine.poll(handle, maxPackets, waitMs)
                if (!ok) {
                    val err = NativeMediaEngine.lastError(handle)
                    if (err.isNotBlank()) {
                        withContext(Dispatchers.Main) {
                            lastError = err
                        }
                    }
                }
                if (waitMs == 0) {
                    delay(5)
                }
            }
        }
    }

    private fun startVideoLoop() {
        val handle = engineHandle
        videoJob = scope.launch {
            while (isActive && running && videoEnabled) {
                val frame = NativeMediaEngine.popVideo(handle)
                if (frame != null) {
                    val bitmap = nv12ToBitmap(frame)
                    if (bitmap != null) {
                        withContext(Dispatchers.Main) {
                            remoteVideo = bitmap
                        }
                    }
                } else {
                    delay(10)
                }
            }
        }
    }

    private fun startCamera(lifecycleOwner: LifecycleOwner, previewView: PreviewView) {
        val executor = Executors.newSingleThreadExecutor()
        analysisExecutor = executor
        val providerFuture = ProcessCameraProvider.getInstance(context)
        providerFuture.addListener(
            {
                if (!running || engineHandle == 0L || !videoEnabled) {
                    return@addListener
                }
                val provider = providerFuture.get()
                val selector = if (runCatching { provider.hasCamera(CameraSelector.DEFAULT_FRONT_CAMERA) }.getOrDefault(false)) {
                    CameraSelector.DEFAULT_FRONT_CAMERA
                } else {
                    CameraSelector.DEFAULT_BACK_CAMERA
                }
                val previewUseCase = Preview.Builder().build().apply {
                    setSurfaceProvider(previewView.surfaceProvider)
                }
                val analysisUseCase = ImageAnalysis.Builder()
                    .setTargetResolution(android.util.Size(videoWidth, videoHeight))
                    .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                    .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_YUV_420_888)
                    .build()
                analysisUseCase.setAnalyzer(executor) { image ->
                    handleVideoFrame(image)
                }
                provider.unbindAll()
                runCatching {
                    provider.bindToLifecycle(lifecycleOwner, selector, previewUseCase, analysisUseCase)
                }.onFailure {
                    lastError = "Camera bind failed"
                }
                cameraProvider = provider
                preview = previewUseCase
                analysis = analysisUseCase
            },
            ContextCompat.getMainExecutor(context)
        )
    }

    private fun stopCamera() {
        analysis?.clearAnalyzer()
        analysis = null
        preview = null
        cameraProvider?.unbindAll()
        cameraProvider = null
        analysisExecutor?.shutdown()
        analysisExecutor = null
    }

    private fun handleVideoFrame(image: ImageProxy) {
        if (!running || engineHandle == 0L) {
            image.close()
            return
        }
        val nowNs = SystemClock.elapsedRealtimeNanos()
        val frameIntervalNs = 1_000_000_000L / max(1, videoFps)
        if (nowNs - lastVideoNs < frameIntervalNs) {
            image.close()
            return
        }
        lastVideoNs = nowNs
        val nv12 = yuv420ToNv12(image)
        if (nv12 != null) {
            NativeMediaEngine.sendNv12(engineHandle, nv12, image.width, image.height, image.width)
        }
        image.close()
    }

    private fun yuv420ToNv12(image: ImageProxy): ByteArray? {
        val width = image.width
        val height = image.height
        val ySize = width * height
        val uvSize = ySize / 2
        val total = ySize + uvSize
        val out = if (nv12Buffer?.size == total) {
            nv12Buffer
        } else {
            ByteArray(total).also { nv12Buffer = it }
        } ?: return null
        val yPlane = image.planes[0]
        val uPlane = image.planes[1]
        val vPlane = image.planes[2]
        val yBuffer = yPlane.buffer
        val yRowStride = yPlane.rowStride
        val yPos = yBuffer.position()
        for (row in 0 until height) {
            val rowStart = row * yRowStride
            yBuffer.position(rowStart)
            yBuffer.get(out, row * width, width)
        }
        yBuffer.position(yPos)
        val uBuffer = uPlane.buffer
        val vBuffer = vPlane.buffer
        val uRowStride = uPlane.rowStride
        val vRowStride = vPlane.rowStride
        val uPixelStride = uPlane.pixelStride
        val vPixelStride = vPlane.pixelStride
        var outPos = ySize
        val chromaHeight = height / 2
        for (row in 0 until chromaHeight) {
            val uRowStart = row * uRowStride
            val vRowStart = row * vRowStride
            var col = 0
            while (col < width / 2) {
                val uIndex = uRowStart + col * uPixelStride
                val vIndex = vRowStart + col * vPixelStride
                out[outPos++] = uBuffer.get(uIndex)
                out[outPos++] = vBuffer.get(vIndex)
                col += 1
            }
        }
        return out
    }

    private fun nv12ToBitmap(frame: MediaVideoFrame): Bitmap? {
        val width = frame.width
        val height = frame.height
        if (width <= 0 || height <= 0) {
            return null
        }
        val stride = max(frame.stride, width)
        val expected = stride * height * 3 / 2
        if (frame.data.size < expected) {
            return null
        }
        val nv21 = ByteArray(width * height * 3 / 2)
        for (row in 0 until height) {
            val srcOffset = row * stride
            val dstOffset = row * width
            System.arraycopy(frame.data, srcOffset, nv21, dstOffset, width)
        }
        val ySize = stride * height
        var outPos = width * height
        val chromaHeight = height / 2
        for (row in 0 until chromaHeight) {
            val rowStart = ySize + row * stride
            var col = 0
            while (col < width) {
                val u = frame.data[rowStart + col]
                val v = frame.data[rowStart + col + 1]
                nv21[outPos++] = v
                nv21[outPos++] = u
                col += 2
            }
        }
        val yuv = YuvImage(nv21, ImageFormat.NV21, width, height, null)
        val stream = ByteArrayOutputStream()
        if (!yuv.compressToJpeg(Rect(0, 0, width, height), 70, stream)) {
            return null
        }
        val bytes = stream.toByteArray()
        return BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
    }
}
