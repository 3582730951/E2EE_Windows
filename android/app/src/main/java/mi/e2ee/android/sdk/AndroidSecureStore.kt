package mi.e2ee.android.sdk

import android.os.Build
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

object AndroidSecureStore {
    private const val KEY_ALIAS = "mi_e2ee_secure_store_key_v1"
    private const val MAGIC = "MI_E2EE_SECURE_STORE_KS1"
    private const val MAX_IV_BYTES = 32
    private val lastError = ThreadLocal<String?>()

    @JvmStatic
    fun isSupported(): Boolean {
        return Build.VERSION.SDK_INT >= 23
    }

    @JvmStatic
    fun lastError(): String {
        return lastError.get() ?: ""
    }

    private fun setError(msg: String): ByteArray? {
        lastError.set(msg)
        return null
    }

    private fun loadKeyStore(): KeyStore {
        val ks = KeyStore.getInstance("AndroidKeyStore")
        ks.load(null)
        return ks
    }

    private fun buildSpec(strongBox: Boolean): KeyGenParameterSpec {
        val builder = KeyGenParameterSpec.Builder(
            KEY_ALIAS,
            KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
        ).setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setRandomizedEncryptionRequired(true)
            .setKeySize(256)
        if (strongBox && Build.VERSION.SDK_INT >= 28) {
            builder.setIsStrongBoxBacked(true)
        }
        return builder.build()
    }

    private fun createKey(): SecretKey? {
        return try {
            val keyGen = KeyGenerator.getInstance(
                KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore"
            )
            if (Build.VERSION.SDK_INT >= 28) {
                try {
                    keyGen.init(buildSpec(true))
                    return keyGen.generateKey()
                } catch (_: Exception) {
                    // fall through to non-strongbox key
                }
            }
            keyGen.init(buildSpec(false))
            keyGen.generateKey()
        } catch (_: Exception) {
            null
        }
    }

    private fun getOrCreateKey(): SecretKey? {
        return try {
            val existing = loadKeyStore().getKey(KEY_ALIAS, null) as? SecretKey
            existing ?: createKey()
        } catch (_: Exception) {
            null
        }
    }

    @JvmStatic
    fun encrypt(plain: ByteArray, entropy: ByteArray?): ByteArray? {
        lastError.set(null)
        if (plain.isEmpty()) return setError("secure store plain empty")
        if (!isSupported()) return setError("secure store unsupported")
        val key = getOrCreateKey() ?: return setError("secure store key unavailable")
        return try {
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.ENCRYPT_MODE, key)
            if (entropy != null && entropy.isNotEmpty()) {
                cipher.updateAAD(entropy)
            }
            val iv = cipher.iv
            if (iv.isEmpty() || iv.size > MAX_IV_BYTES) {
                return setError("secure store iv invalid")
            }
            val cipherText = cipher.doFinal(plain)
            val magicBytes = MAGIC.toByteArray(Charsets.US_ASCII)
            val out = ByteArray(magicBytes.size + 1 + iv.size + cipherText.size)
            System.arraycopy(magicBytes, 0, out, 0, magicBytes.size)
            out[magicBytes.size] = iv.size.toByte()
            System.arraycopy(iv, 0, out, magicBytes.size + 1, iv.size)
            System.arraycopy(
                cipherText, 0, out, magicBytes.size + 1 + iv.size, cipherText.size
            )
            out
        } catch (_: Exception) {
            setError("secure store encrypt failed")
        }
    }

    @JvmStatic
    fun decrypt(blob: ByteArray, entropy: ByteArray?): ByteArray? {
        lastError.set(null)
        if (blob.isEmpty()) return setError("secure store blob empty")
        if (!isSupported()) return setError("secure store unsupported")
        val key = getOrCreateKey() ?: return setError("secure store key unavailable")
        val magicBytes = MAGIC.toByteArray(Charsets.US_ASCII)
        if (blob.size < magicBytes.size + 1) {
            return setError("secure store blob invalid")
        }
        val head = blob.copyOfRange(0, magicBytes.size)
        if (!head.contentEquals(magicBytes)) {
            return setError("secure store blob invalid")
        }
        val ivLen = blob[magicBytes.size].toInt() and 0xFF
        if (ivLen <= 0 || ivLen > MAX_IV_BYTES) {
            return setError("secure store blob invalid")
        }
        val offset = magicBytes.size + 1
        if (offset + ivLen >= blob.size) {
            return setError("secure store blob invalid")
        }
        val iv = blob.copyOfRange(offset, offset + ivLen)
        val cipherText = blob.copyOfRange(offset + ivLen, blob.size)
        return try {
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            val spec = GCMParameterSpec(128, iv)
            cipher.init(Cipher.DECRYPT_MODE, key, spec)
            if (entropy != null && entropy.isNotEmpty()) {
                cipher.updateAAD(entropy)
            }
            cipher.doFinal(cipherText)
        } catch (_: Exception) {
            setError("secure store decrypt failed")
        }
    }
}
