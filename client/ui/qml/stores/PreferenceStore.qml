pragma Singleton
import QtQuick 2.15

QtObject {
    id: preferenceStore

    property bool clipboardIsolationEnabled: true
    property bool internalImeEnabled: true
    property bool historySaveEnabled: true
    property bool aiEnhanceEnabled: false
    property int aiEnhanceQualityLevel: 2
    property bool aiEnhanceX4Confirmed: false

    function applyFromApp(clipboardIsolation,
                          internalIme,
                          historySave,
                          aiEnhance,
                          aiQuality,
                          aiX4Confirmed) {
        clipboardIsolationEnabled = clipboardIsolation === true
        internalImeEnabled = internalIme === true
        historySaveEnabled = historySave === true
        aiEnhanceEnabled = aiEnhance === true
        aiEnhanceQualityLevel = aiQuality || 2
        aiEnhanceX4Confirmed = aiX4Confirmed === true
    }
}
