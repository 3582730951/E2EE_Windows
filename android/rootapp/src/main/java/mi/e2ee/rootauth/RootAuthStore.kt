package mi.e2ee.rootauth

import android.content.Context
import android.os.Build
import android.util.Base64
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import java.security.KeyFactory
import java.security.KeyPair
import java.security.KeyPairGenerator
import java.security.MessageDigest
import java.security.PublicKey
import java.security.Signature
import java.security.spec.PKCS8EncodedKeySpec
import java.security.spec.X509EncodedKeySpec

class RootAuthStore(context: Context) {
    private val prefs = buildEncryptedPrefs(context)
    private var cachedKeyPair: KeyPair? = null

    fun publicKeyHex(): String {
        val kp = loadOrCreateKeyPair() ?: return ""
        val raw = rawPublicKey(kp.public) ?: return ""
        return bytesToHexLower(raw)
    }

    fun regenerateKeyPair(): String {
        val kp = generateKeyPair()
        saveKeyPair(kp)
        cachedKeyPair = kp
        return publicKeyHex()
    }

    fun currentCode(stepSec: Long = 5L, digits: Int = 6): TotpResult {
        val nowSec = System.currentTimeMillis() / 1000L
        val counter = nowSec / stepSec
        val remaining = (stepSec - (nowSec % stepSec)).toInt()
        val kp = loadOrCreateKeyPair() ?: return TotpResult("------", remaining)
        val pub = rawPublicKey(kp.public) ?: return TotpResult("------", remaining)
        val code = deriveCode(pub, counter, digits)
        return TotpResult(code, remaining)
    }

    fun currentProof(deviceId: String, context: String, stepSec: Long = 5L): String? {
        if (deviceId.isBlank()) {
            return null
        }
        val kp = loadOrCreateKeyPair() ?: return null
        val counter = (System.currentTimeMillis() / 1000L) / stepSec
        return signProof(kp, deviceId, context, counter)
    }

    fun currentAuthString(
        deviceId: String,
        context: String,
        stepSec: Long = 5L,
        digits: Int = 6
    ): String? {
        if (deviceId.isBlank()) {
            return null
        }
        val kp = loadOrCreateKeyPair() ?: return null
        val pub = rawPublicKey(kp.public) ?: return null
        val counter = (System.currentTimeMillis() / 1000L) / stepSec
        val code = deriveCode(pub, counter, digits)
        val proof = signProof(kp, deviceId, context, counter) ?: return null
        return "$code:$proof"
    }

    private fun loadOrCreateKeyPair(): KeyPair? {
        cachedKeyPair?.let { return it }
        loadKeyPair()?.let {
            cachedKeyPair = it
            return it
        }
        val kp = generateKeyPair()
        saveKeyPair(kp)
        cachedKeyPair = kp
        return kp
    }

    private fun loadKeyPair(): KeyPair? {
        val privB64 = prefs.getString(KEY_PRIV, null) ?: return null
        val pubB64 = prefs.getString(KEY_PUB, null) ?: return null
        return try {
            val privBytes = Base64.decode(privB64, Base64.DEFAULT)
            val pubBytes = Base64.decode(pubB64, Base64.DEFAULT)
            val kf = KeyFactory.getInstance("Ed25519")
            val priv = kf.generatePrivate(PKCS8EncodedKeySpec(privBytes))
            val pub = kf.generatePublic(X509EncodedKeySpec(pubBytes))
            KeyPair(pub, priv)
        } catch (_: Exception) {
            null
        }
    }

    private fun saveKeyPair(keyPair: KeyPair) {
        val privB64 = Base64.encodeToString(keyPair.private.encoded, Base64.NO_WRAP)
        val pubB64 = Base64.encodeToString(keyPair.public.encoded, Base64.NO_WRAP)
        prefs.edit()
            .putString(KEY_PRIV, privB64)
            .putString(KEY_PUB, pubB64)
            .apply()
    }

    private fun generateKeyPair(): KeyPair {
        val gen = KeyPairGenerator.getInstance("Ed25519")
        return gen.generateKeyPair()
    }

    private fun rawPublicKey(pub: PublicKey): ByteArray? {
        val encoded = pub.encoded ?: return null
        if (encoded.size < 32) {
            return null
        }
        return encoded.copyOfRange(encoded.size - 32, encoded.size)
    }

    private fun deriveCode(pubkey: ByteArray, counter: Long, digits: Int): String {
        val label = ROOT_CODE_LABEL.toByteArray(Charsets.UTF_8)
        val msg = ByteArray(label.size + 1 + pubkey.size + 1 + 8)
        var offset = 0
        System.arraycopy(label, 0, msg, offset, label.size)
        offset += label.size
        msg[offset++] = 0
        System.arraycopy(pubkey, 0, msg, offset, pubkey.size)
        offset += pubkey.size
        msg[offset++] = 0
        var value = counter
        for (i in 7 downTo 0) {
            msg[offset + i] = (value and 0xFF).toByte()
            value = value shr 8
        }
        val digest = MessageDigest.getInstance("SHA-256").digest(msg)
        val bin = ((digest[0].toInt() and 0xFF) shl 24) or
            ((digest[1].toInt() and 0xFF) shl 16) or
            ((digest[2].toInt() and 0xFF) shl 8) or
            (digest[3].toInt() and 0xFF)
        val mod = if (digits == 8) 100_000_000 else 1_000_000
        val code = bin % mod
        return code.toString().padStart(digits, '0')
    }

    private fun buildProofMessage(deviceId: String, context: String, counter: Long): ByteArray {
        val label = ROOT_PROOF_LABEL.toByteArray(Charsets.UTF_8)
        val deviceBytes = deviceId.toByteArray(Charsets.UTF_8)
        val ctxBytes = context.toByteArray(Charsets.UTF_8)
        val msg = ByteArray(label.size + 1 + deviceBytes.size + 1 + ctxBytes.size + 1 + 8)
        var offset = 0
        System.arraycopy(label, 0, msg, offset, label.size)
        offset += label.size
        msg[offset++] = 0
        System.arraycopy(deviceBytes, 0, msg, offset, deviceBytes.size)
        offset += deviceBytes.size
        msg[offset++] = 0
        System.arraycopy(ctxBytes, 0, msg, offset, ctxBytes.size)
        offset += ctxBytes.size
        msg[offset++] = 0
        var value = counter
        for (i in 7 downTo 0) {
            msg[offset + i] = (value and 0xFF).toByte()
            value = value shr 8
        }
        return msg
    }

    private fun signProof(
        keyPair: KeyPair,
        deviceId: String,
        context: String,
        counter: Long
    ): String? {
        val msg = buildProofMessage(deviceId, context, counter)
        val sig = Signature.getInstance("Ed25519")
        sig.initSign(keyPair.private)
        sig.update(msg)
        val signature = sig.sign()
        return bytesToHexLower(signature)
    }

    private fun bytesToHexLower(bytes: ByteArray): String {
        val out = StringBuilder(bytes.size * 2)
        for (b in bytes) {
            val v = b.toInt() and 0xFF
            out.append(HEX[v ushr 4])
            out.append(HEX[v and 0x0F])
        }
        return out.toString()
    }

    private fun buildEncryptedPrefs(context: Context) = run {
        val masterKey = try {
            val builder = MasterKey.Builder(context)
                .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
            requestStrongBox(builder)
            builder.build()
        } catch (e: Exception) {
            MasterKey.Builder(context)
                .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
                .build()
        }
        EncryptedSharedPreferences.create(
            context,
            PREFS_NAME,
            masterKey,
            EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
            EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
        )
    }

    companion object {
        private const val PREFS_NAME = "root_auth_prefs"
        private const val KEY_PRIV = "root_priv_b64"
        private const val KEY_PUB = "root_pub_b64"
        private const val ROOT_CODE_LABEL = "mi_e2ee_root_code_v1"
        private const val ROOT_PROOF_LABEL = "mi_e2ee_root_proof_v2"
        private val HEX = "0123456789abcdef".toCharArray()
    }

    private fun requestStrongBox(builder: MasterKey.Builder) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return
        }
        try {
            val method = MasterKey.Builder::class.java.getMethod(
                "setRequestStrongBoxBacked",
                Boolean::class.javaPrimitiveType
            )
            method.invoke(builder, true)
        } catch (_: Exception) {
            // Best-effort only.
        }
    }
}

data class TotpResult(
    val code: String,
    val remaining: Int
)
