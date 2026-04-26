package mi.e2ee.android.ui

import android.util.Base64
import androidx.compose.ui.platform.ClipboardManager
import androidx.compose.ui.text.AnnotatedString
import java.security.SecureRandom
import java.util.concurrent.ConcurrentHashMap

object SecureClipboard {
    private const val TokenPrefix = "zerox://clip/"
    private const val TtlMs = 60_000L
    private val rng = SecureRandom()
    private val entries = ConcurrentHashMap<String, Entry>()

    fun copyProtectedText(clipboard: ClipboardManager, plaintext: String) {
        purgeExpired(System.currentTimeMillis())
        val token = newToken()
        entries[token] = Entry(plaintext, System.currentTimeMillis() + TtlMs)
        clipboard.setText(AnnotatedString(TokenPrefix + token))
    }

    fun resolveProtectedText(value: String?): String? {
        if (value.isNullOrBlank() || !value.startsWith(TokenPrefix)) {
            return null
        }
        purgeExpired(System.currentTimeMillis())
        val token = value.removePrefix(TokenPrefix)
        val entry = entries[token] ?: return null
        if (entry.expiresAtMs <= System.currentTimeMillis()) {
            entries.remove(token)
            return null
        }
        return entry.value
    }

    fun clear() {
        entries.clear()
    }

    private fun newToken(): String {
        val bytes = ByteArray(24)
        rng.nextBytes(bytes)
        return Base64.encodeToString(
            bytes,
            Base64.URL_SAFE or Base64.NO_WRAP or Base64.NO_PADDING
        )
    }

    private fun purgeExpired(nowMs: Long) {
        entries.entries.removeIf { it.value.expiresAtMs <= nowMs }
    }

    private data class Entry(
        val value: String,
        val expiresAtMs: Long
    )
}
