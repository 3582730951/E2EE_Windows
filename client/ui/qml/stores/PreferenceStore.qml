pragma Singleton
import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

QtObject {
    id: preferenceStore

    property bool clipboardIsolationEnabled: true
    property bool internalImeEnabled: true
    property bool historySaveEnabled: false
    property bool aiEnhanceEnabled: false
    property int aiEnhanceQualityLevel: 2
    property bool aiEnhanceX4Confirmed: false
    readonly property bool aiEnhanceGpuAvailable: Ui.AppStore.aiEnhanceGpuAvailable
    readonly property string aiEnhanceGpuName: Ui.AppStore.aiEnhanceGpuName
    readonly property int aiEnhanceGpuSeries: Ui.AppStore.aiEnhanceGpuSeries
    readonly property int aiEnhancePerfScale: Ui.AppStore.aiEnhancePerfScale
    readonly property int aiEnhanceQualityScale: Ui.AppStore.aiEnhanceQualityScale

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

    function setClipboardIsolationEnabled(enabled) {
        Ui.AppStore.setClipboardIsolationEnabled(enabled)
    }

    function setInternalImeEnabled(enabled) {
        Ui.AppStore.setInternalImeEnabled(enabled)
    }

    function setHistorySaveEnabled(enabled) {
        Ui.AppStore.setHistorySaveEnabled(enabled)
    }

    function setAiEnhanceEnabled(enabled) {
        Ui.AppStore.setAiEnhanceEnabled(enabled)
    }

    function setAiEnhanceQualityLevel(level) {
        Ui.AppStore.setAiEnhanceQualityLevel(level)
    }

    function setAiEnhanceX4Confirmed(confirmed) {
        Ui.AppStore.setAiEnhanceX4Confirmed(confirmed)
    }
}
