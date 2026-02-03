package mi.e2ee.rootauth

import android.content.Context
import android.os.Build
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import java.util.Locale
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec

class RootAuthStore(context: Context) {
    private val prefs = buildEncryptedPrefs(context)

    fun loadSecret(): String? {
        return prefs.getString(KEY_SECRET, null)
    }

    fun saveSecret(hex: String): Boolean {
        val cleaned = hex.trim().lowercase(Locale.US)
        if (!isValidSecret(cleaned)) {
            return false
        }
        prefs.edit().putString(KEY_SECRET, cleaned).apply()
        return true
    }

    fun clearSecret() {
        prefs.edit().remove(KEY_SECRET).apply()
    }

    fun currentCode(secretHex: String, stepSec: Long = 5L, digits: Int = 6): TotpResult {
        val nowSec = System.currentTimeMillis() / 1000L
        val counter = nowSec / stepSec
        val remaining = (stepSec - (nowSec % stepSec)).toInt()
        val secret = hexToBytes(secretHex) ?: return TotpResult("------", remaining)
        val code = totp(secret, counter, digits)
        return TotpResult(code, remaining)
    }

    private fun totp(secret: ByteArray, counter: Long, digits: Int): String {
        val msg = ByteArray(8)
        var value = counter
        for (i in 7 downTo 0) {
            msg[i] = (value and 0xFF).toByte()
            value = value shr 8
        }
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(secret, "HmacSHA256"))
        val digest = mac.doFinal(msg)
        val offset = digest[digest.size - 1].toInt() and 0x0F
        val bin = ((digest[offset].toInt() and 0x7F) shl 24) or
            ((digest[offset + 1].toInt() and 0xFF) shl 16) or
            ((digest[offset + 2].toInt() and 0xFF) shl 8) or
            (digest[offset + 3].toInt() and 0xFF)
        val mod = if (digits == 8) 100_000_000 else 1_000_000
        val code = (bin % mod).toString()
        return code.padStart(digits, '0')
    }

    private fun hexToBytes(hex: String): ByteArray? {
        val cleaned = hex.trim().lowercase(Locale.US)
        if (!isValidSecret(cleaned)) {
            return null
        }
        val out = ByteArray(cleaned.length / 2)
        var idx = 0
        while (idx < cleaned.length) {
            val byte = cleaned.substring(idx, idx + 2).toIntOrNull(16) ?: return null
            out[idx / 2] = byte.toByte()
            idx += 2
        }
        return out
    }

    private fun isValidSecret(value: String): Boolean {
        if (value.length != 64) {
            return false
        }
        for (c in value) {
            if (c !in '0'..'9' && c !in 'a'..'f') {
                return false
            }
        }
        return true
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
        private const val KEY_SECRET = "root_secret_hex"
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
