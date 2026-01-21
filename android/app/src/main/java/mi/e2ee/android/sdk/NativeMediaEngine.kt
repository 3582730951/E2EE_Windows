package mi.e2ee.android.sdk

object NativeMediaEngine {
    private const val BACKEND_LIB_NAME = "mi_e2ee_backend"
    private const val JNI_LIB_NAME = "mi_e2ee_jni"

    val available: Boolean = runCatching {
        System.loadLibrary(BACKEND_LIB_NAME)
        System.loadLibrary(JNI_LIB_NAME)
        true
    }.getOrDefault(false)

    external fun createPeerEngine(
        clientHandle: Long,
        peerUsername: String,
        callId: ByteArray,
        initiator: Boolean,
        enableVideo: Boolean,
        sampleRate: Int,
        channels: Int,
        frameMs: Int,
        videoWidth: Int,
        videoHeight: Int,
        videoFps: Int
    ): Long

    external fun createGroupEngine(
        clientHandle: Long,
        groupId: String,
        callId: ByteArray,
        keyId: Int,
        enableVideo: Boolean,
        sampleRate: Int,
        channels: Int,
        frameMs: Int,
        videoWidth: Int,
        videoHeight: Int,
        videoFps: Int
    ): Long

    external fun destroyEngine(engineHandle: Long)
    external fun lastError(engineHandle: Long): String
    external fun getAudioFrameSamples(engineHandle: Long): Int
    external fun poll(engineHandle: Long, maxPackets: Int, waitMs: Int): Boolean
    external fun sendPcm(engineHandle: Long, samples: ShortArray): Boolean
    external fun sendNv12(
        engineHandle: Long,
        data: ByteArray,
        width: Int,
        height: Int,
        stride: Int
    ): Boolean

    external fun popAudio(engineHandle: Long): ShortArray?
    external fun popVideo(engineHandle: Long): MediaVideoFrame?
}
