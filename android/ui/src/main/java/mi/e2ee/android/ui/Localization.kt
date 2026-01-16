package mi.e2ee.android.ui

import android.content.Context
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.platform.LocalContext
import org.json.JSONObject

private const val LANGUAGE_PREFS = "mi_chat_prefs"
private const val KEY_LANGUAGE = "lang_code"
private const val DEFAULT_LANGUAGE = "zh-CN"
private const val LANG_ASSET_DIR = "lang"

data class LanguagePack(
    val code: String,
    val label: String,
    val strings: Map<String, String>
)

data class UiStrings(val entries: Map<String, String>) {
    fun get(key: String, fallback: String): String = entries[key] ?: fallback
}

data class LanguageController(
    val current: LanguagePack,
    val packs: List<LanguagePack>,
    val setLanguage: (String) -> Unit
)

val LocalStrings = staticCompositionLocalOf { UiStrings(emptyMap()) }
val LocalLanguageController = staticCompositionLocalOf<LanguageController?> { null }

@Composable
fun ProvideLocalization(content: @Composable () -> Unit) {
    val context = LocalContext.current
    val prefs = remember(context) {
        context.getSharedPreferences(LANGUAGE_PREFS, Context.MODE_PRIVATE)
    }
    val savedCode = prefs.getString(KEY_LANGUAGE, null)
    var selectedCode by rememberSaveable { mutableStateOf(savedCode ?: DEFAULT_LANGUAGE) }
    val packs = remember(context) { loadLanguagePacks(context) }
    val resolvedPack = remember(selectedCode, packs) {
        packs.firstOrNull { it.code == selectedCode }
            ?: packs.firstOrNull { it.code == DEFAULT_LANGUAGE }
            ?: packs.firstOrNull()
            ?: LanguagePack(DEFAULT_LANGUAGE, DEFAULT_LANGUAGE, emptyMap())
    }

    LaunchedEffect(selectedCode) {
        prefs.edit().putString(KEY_LANGUAGE, selectedCode).apply()
    }

    val controller = remember(resolvedPack, packs) {
        LanguageController(
            current = resolvedPack,
            packs = packs,
            setLanguage = { code -> selectedCode = code }
        )
    }

    androidx.compose.runtime.CompositionLocalProvider(
        LocalStrings provides UiStrings(resolvedPack.strings),
        LocalLanguageController provides controller
    ) {
        content()
    }
}

@Composable
fun tr(key: String, fallback: String): String {
    return LocalStrings.current.get(key, fallback)
}

private fun loadLanguagePacks(context: Context): List<LanguagePack> {
    val assetManager = context.assets
    val files = assetManager.list(LANG_ASSET_DIR) ?: return emptyList()
    val packs = mutableListOf<LanguagePack>()
    for (file in files) {
        if (!file.endsWith(".json")) {
            continue
        }
        val json = runCatching {
            assetManager.open("$LANG_ASSET_DIR/$file")
                .bufferedReader()
                .use { it.readText() }
        }.getOrNull() ?: continue
        val code = file.removeSuffix(".json")
        parseLanguagePack(json, code)?.let { packs.add(it) }
    }
    return packs.sortedBy { it.code }
}

private fun parseLanguagePack(json: String, fallbackCode: String): LanguagePack? {
    return runCatching {
        val root = JSONObject(json)
        val code = root.optString("code", fallbackCode)
        val label = root.optString("label", code)
        val strings = mutableMapOf<String, String>()
        val data = root.optJSONObject("strings")
        if (data != null) {
            val keys = data.keys()
            while (keys.hasNext()) {
                val key = keys.next()
                strings[key] = data.optString(key, "")
            }
        }
        LanguagePack(code, label, strings)
    }.getOrNull()
}
