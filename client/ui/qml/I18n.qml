pragma Singleton
import QtQuick 2.15

QtObject {
    id: root

    property string currentLocale: "zh-CN"
    property var strings: ({})
    property bool loaded: false
    property var languages: [
        { code: "zh-CN", name: "简体中文", url: "qrc:/mi/e2ee/ui/qml/i18n/zh-CN.json" },
        { code: "en-US", name: "English", url: "qrc:/mi/e2ee/ui/qml/i18n/en-US.json" }
    ]

    signal localeChanged()

    function ensureLoaded() {
        if (!loaded) {
            setLocale(currentLocale)
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
        setLocale(code)
    }

    function setLocale(code) {
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
                currentLocale = code
                loaded = true
                localeChanged()
            } catch (err) {
            }
        }
    }

    Component.onCompleted: setLocale(currentLocale)
}
