pragma Singleton
import QtQuick 2.15
import QtCore

QtObject {
    id: root

    Settings {
        id: localeSettings
        category: "ui_i18n"
        property string localeMode: "system"
    }

    readonly property string smokeLocale: typeof uiSmokeLocale !== "undefined"
                                          ? (uiSmokeLocale || "")
                                          : ""
    property string localeMode: {
        var mode = localeSettings.localeMode
        if (mode === "system" || mode === "zh-CN" || mode === "en-US") {
            return mode
        }
        return "system"
    }
    readonly property string effectiveLocale: resolvedLocale(localeMode)
    readonly property string currentLocale: effectiveLocale
    readonly property bool usesCjkLocale: effectiveLocale === "zh-CN"
    property var strings: ({})
    property bool loaded: false
    property var languages: [
        { code: "zh-CN", name: "简体中文", url: "qrc:/mi/e2ee/ui/qml/i18n/zh-CN.json" },
        { code: "en-US", name: "English", url: "qrc:/mi/e2ee/ui/qml/i18n/en-US.json" }
    ]

    signal localeChanged()

    function ensureLoaded() {
        if (!loaded) {
            loadCatalog(effectiveLocale)
        }
    }

    function t(key) {
        ensureLoaded()
        var value = strings[key]
        if (value === undefined || value === null) {
            return key
        }
        return value
    }

    function list(key) {
        ensureLoaded()
        var value = strings[key]
        return Array.isArray(value) ? value : []
    }

    function format(key) {
        var text = t(key)
        for (var i = 1; i < arguments.length; ++i) {
            text = text.replace("%" + i, arguments[i])
        }
        return text
    }

    function languageIndex(code) {
        for (var i = 0; i < languages.length; ++i) {
            if (languages[i].code === code) {
                return i
            }
        }
        return 0
    }

    function registerLanguage(code, name, url) {
        for (var i = 0; i < languages.length; ++i) {
            if (languages[i].code === code) {
                return
            }
        }
        var updated = languages.slice(0)
        updated.push({ code: code, name: name, url: url })
        languages = updated
    }

    function loadLanguagePack(url, code, name) {
        registerLanguage(code, name, url)
        setLocaleMode(code)
    }

    function systemLocaleMode() {
        var localeName = Qt.locale().name || ""
        var normalized = (localeName + "").replace("_", "-")
        return normalized.indexOf("zh") === 0 ? "zh-CN" : "en-US"
    }

    function resolvedLocale(mode) {
        if (smokeLocale === "zh-CN" || smokeLocale === "en-US") {
            return smokeLocale
        }
        if (mode === "zh-CN" || mode === "en-US") {
            return mode
        }
        return systemLocaleMode()
    }

    function setLocaleMode(mode) {
        if (!(mode === "system" || mode === "zh-CN" || mode === "en-US")) {
            return
        }
        localeMode = mode
        localeSettings.localeMode = mode
        loadCatalog(effectiveLocale)
    }

    function setLocale(code) {
        setLocaleMode(code)
    }

    function loadCatalog(code) {
        var entry = null
        for (var i = 0; i < languages.length; ++i) {
            if (languages[i].code === code) {
                entry = languages[i]
                break
            }
        }
        if (!entry) {
            return
        }
        var request = new XMLHttpRequest()
        request.open("GET", entry.url, false)
        request.send()
        if (request.status === 0 || request.status === 200) {
            try {
                strings = JSON.parse(request.responseText)
                loaded = true
                localeChanged()
            } catch (err) {
            }
        }
    }

    onLocaleModeChanged: {
        if (localeSettings.localeMode !== localeMode) {
            localeSettings.localeMode = localeMode
        }
    }

    onEffectiveLocaleChanged: {
        loaded = false
        loadCatalog(effectiveLocale)
    }

    Component.onCompleted: loadCatalog(effectiveLocale)
}
