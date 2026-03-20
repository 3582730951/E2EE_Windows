package mi.e2ee.rootauth

import java.io.ByteArrayOutputStream
import java.io.IOException
import java.io.InputStream
import java.net.InetSocketAddress
import java.net.Socket
import java.security.MessageDigest
import java.security.SecureRandom
import java.security.cert.X509Certificate
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocket

data class QrLoginApproveResult(
    val success: Boolean,
    val error: String? = null
)

object QrLoginApproveClient {
    private const val FRAME_MAGIC = 0x4D495746
    private const val FRAME_VERSION = 1
    private const val FRAME_HEADER_SIZE = 12
    private const val MAX_PAYLOAD_BYTES = 16 * 1024 * 1024
    private const val TYPE_QR_LOGIN_APPROVE_ROOT = 61
    private const val TIMEOUT_MS = 5000

    fun approve(
        username: String,
        qrId: String,
        secretHex: String,
        deviceId: String,
        rootCode: String,
        host: String,
        port: Int,
        useTls: Boolean,
        fingerprint: String?
    ): QrLoginApproveResult {
        if (username.isBlank() || qrId.isBlank() || secretHex.isBlank() ||
            deviceId.isBlank() || rootCode.isBlank()) {
            return QrLoginApproveResult(false, "invalid approve payload")
        }
        if (host.isBlank() || port <= 0 || port > 65535) {
            return QrLoginApproveResult(false, "invalid server endpoint")
        }
        val payload = ByteArrayOutputStream()
        if (!writeString(username, payload) ||
            !writeString(qrId, payload) ||
            !writeString(secretHex, payload) ||
            !writeString(deviceId, payload) ||
            !writeString(rootCode, payload)) {
            return QrLoginApproveResult(false, "payload encode failed")
        }
        val frame = encodeFrame(TYPE_QR_LOGIN_APPROVE_ROOT, payload.toByteArray())
        return try {
            openSocket(host, port, useTls, fingerprint).use { socket ->
                val output = socket.getOutputStream()
                output.write(frame)
                output.flush()
                val resp = readFrame(socket.getInputStream())
                    ?: return QrLoginApproveResult(false, "response decode failed")
                if (resp.type != TYPE_QR_LOGIN_APPROVE_ROOT) {
                    return QrLoginApproveResult(false, "response type mismatch")
                }
                decodeApproveResult(resp.payload)
            }
        } catch (e: Exception) {
            QrLoginApproveResult(false, e.message ?: "network error")
        }
    }

    private fun openSocket(
        host: String,
        port: Int,
        useTls: Boolean,
        fingerprint: String?
    ): Socket {
        val socket = Socket()
        socket.connect(InetSocketAddress(host, port), TIMEOUT_MS)
        socket.soTimeout = TIMEOUT_MS
        if (!useTls) {
            return socket
        }
        val sslSocket = createSslSocket(socket, host, port, fingerprint)
        sslSocket.soTimeout = TIMEOUT_MS
        return sslSocket
    }

    private fun createSslSocket(
        base: Socket,
        host: String,
        port: Int,
        fingerprint: String?
    ): SSLSocket {
        val normalized = fingerprint?.trim()?.lowercase()
        val factory = SSLContext.getInstance("TLS").apply {
            init(null, null, SecureRandom())
        }.socketFactory
        val sslSocket = factory.createSocket(base, host, port, true) as SSLSocket
        val params = sslSocket.sslParameters
        params.endpointIdentificationAlgorithm = "HTTPS"
        sslSocket.sslParameters = params
        sslSocket.startHandshake()
        if (!normalized.isNullOrBlank()) {
            if (!verifyFingerprint(sslSocket, normalized)) {
                sslSocket.close()
                throw IOException("tls fingerprint mismatch")
            }
        }
        return sslSocket
    }

    private fun verifyFingerprint(socket: SSLSocket, expected: String): Boolean {
        val certs = socket.session.peerCertificates
        val cert = certs.firstOrNull() as? X509Certificate ?: return false
        val digest = MessageDigest.getInstance("SHA-256").digest(cert.encoded)
        val actual = digest.joinToString("") { "%02x".format(it) }
        return actual == expected
    }

    private data class Frame(val type: Int, val payload: ByteArray)

    private fun readFrame(input: InputStream): Frame? {
        val header = ByteArray(FRAME_HEADER_SIZE)
        if (!readFully(input, header, FRAME_HEADER_SIZE)) {
            return null
        }
        val magic = readUint32Le(header, 0)
        if (magic != FRAME_MAGIC) {
            return null
        }
        val version = readUint16Le(header, 4)
        if (version != FRAME_VERSION) {
            return null
        }
        val type = readUint16Le(header, 6)
        val payloadLen = readUint32Le(header, 8)
        if (payloadLen < 0 || payloadLen > MAX_PAYLOAD_BYTES) {
            return null
        }
        val payload = ByteArray(payloadLen)
        if (payloadLen > 0 && !readFully(input, payload, payloadLen)) {
            return null
        }
        return Frame(type, payload)
    }

    private fun decodeApproveResult(payload: ByteArray): QrLoginApproveResult {
        if (payload.isEmpty()) {
            return QrLoginApproveResult(false, "response empty")
        }
        val ok = payload[0].toInt() != 0
        if (ok) {
            return QrLoginApproveResult(true)
        }
        val err = readString(payload, 1) ?: "approve failed"
        return QrLoginApproveResult(false, err)
    }

    private fun writeString(value: String, out: ByteArrayOutputStream): Boolean {
        val bytes = value.toByteArray(Charsets.UTF_8)
        if (bytes.size > 0xFFFF) {
            return false
        }
        out.write(bytes.size and 0xFF)
        out.write((bytes.size shr 8) and 0xFF)
        out.write(bytes)
        return true
    }

    private fun readString(payload: ByteArray, offset: Int): String? {
        if (offset + 2 > payload.size) {
            return null
        }
        val len = (payload[offset].toInt() and 0xFF) or
            ((payload[offset + 1].toInt() and 0xFF) shl 8)
        val start = offset + 2
        val end = start + len
        if (end > payload.size) {
            return null
        }
        return payload.copyOfRange(start, end).toString(Charsets.UTF_8)
    }

    private fun encodeFrame(type: Int, payload: ByteArray): ByteArray {
        val out = ByteArray(FRAME_HEADER_SIZE + payload.size)
        writeUint32Le(FRAME_MAGIC, out, 0)
        writeUint16Le(FRAME_VERSION, out, 4)
        writeUint16Le(type, out, 6)
        writeUint32Le(payload.size, out, 8)
        if (payload.isNotEmpty()) {
            System.arraycopy(payload, 0, out, FRAME_HEADER_SIZE, payload.size)
        }
        return out
    }

    private fun writeUint16Le(value: Int, out: ByteArray, offset: Int) {
        out[offset] = (value and 0xFF).toByte()
        out[offset + 1] = ((value shr 8) and 0xFF).toByte()
    }

    private fun writeUint32Le(value: Int, out: ByteArray, offset: Int) {
        out[offset] = (value and 0xFF).toByte()
        out[offset + 1] = ((value shr 8) and 0xFF).toByte()
        out[offset + 2] = ((value shr 16) and 0xFF).toByte()
        out[offset + 3] = ((value shr 24) and 0xFF).toByte()
    }

    private fun readUint16Le(buf: ByteArray, offset: Int): Int {
        return (buf[offset].toInt() and 0xFF) or
            ((buf[offset + 1].toInt() and 0xFF) shl 8)
    }

    private fun readUint32Le(buf: ByteArray, offset: Int): Int {
        return (buf[offset].toInt() and 0xFF) or
            ((buf[offset + 1].toInt() and 0xFF) shl 8) or
            ((buf[offset + 2].toInt() and 0xFF) shl 16) or
            ((buf[offset + 3].toInt() and 0xFF) shl 24)
    }

    private fun readFully(input: InputStream, buffer: ByteArray, len: Int): Boolean {
        var offset = 0
        var remaining = len
        while (remaining > 0) {
            val read = input.read(buffer, offset, remaining)
            if (read < 0) {
                return false
            }
            offset += read
            remaining -= read
        }
        return true
    }
}
