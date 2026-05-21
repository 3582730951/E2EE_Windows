pragma Singleton
import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: store

    readonly property var client: typeof clientBridge === "undefined" ? null : clientBridge
    readonly property var devicesModel: devicesModelObject
    readonly property int linkedDeviceCount: devicesModelObject.count
    readonly property string maskedCurrentDeviceId: currentDeviceDisplay
    readonly property bool transportHealthy: client ? client.remoteOk === true : false
    readonly property string versionText: client && client.version ? client.version() : ""
    readonly property string transportHeadline: transportHealthy
                                                ? Ui.I18n.t("dialog.securityCenter.transportHealthy")
                                                : Ui.I18n.t("dialog.securityCenter.transportNeedsAttention")
    readonly property string transportTone: transportHealthy ? "healthy" : "blocked"
    readonly property string trustHeadline: gatewayDisplayState.length > 0
                                            ? gatewayDisplayState
                                            : Ui.I18n.t("dialog.securityCenter.trustReview")
    readonly property string trustDetail: gatewayDisplayDetail.length > 0
                                          ? gatewayDisplayDetail
                                          : Ui.I18n.t("dialog.securityCenter.trustReviewHint")
    readonly property string trustTone: "review"
    readonly property string linkedDevicesSummary: Ui.I18n.t("dialog.securityCenter.devicesValue").arg(linkedDeviceCount)

    property var overviewCards: []
    property string currentDeviceDisplay: ""
    property string currentDeviceCopyValue: ""
    property string gatewayDisplayState: ""
    property string gatewayDisplayDetail: ""

    readonly property var themeOptions: [
        { label: Ui.I18n.t("settings.theme.system"), mode: "system" },
        { label: Ui.I18n.t("settings.theme.light"), mode: "light" },
        { label: Ui.I18n.t("settings.theme.dark"), mode: "dark" }
    ]
    readonly property var localeOptions: [
        { label: Ui.I18n.t("settings.theme.system"), code: "system" }
    ].concat(Ui.I18n.languages.map(function(entry) {
        return { label: entry.name, code: entry.code }
    }))
    readonly property bool clipboardIsolationEnabled: Ui.PreferenceStore.clipboardIsolationEnabled
    readonly property bool internalImeEnabled: Ui.PreferenceStore.internalImeEnabled
    readonly property bool historySaveEnabled: Ui.PreferenceStore.historySaveEnabled
    readonly property bool aiEnhanceEnabled: Ui.PreferenceStore.aiEnhanceEnabled
    readonly property int aiEnhanceQualityLevel: Ui.PreferenceStore.aiEnhanceQualityLevel
    readonly property bool aiEnhanceX4Confirmed: Ui.PreferenceStore.aiEnhanceX4Confirmed
    readonly property bool aiEnhanceGpuAvailable: Ui.PreferenceStore.aiEnhanceGpuAvailable
    readonly property string aiEnhanceGpuName: Ui.PreferenceStore.aiEnhanceGpuName
    readonly property int aiEnhanceGpuSeries: Ui.PreferenceStore.aiEnhanceGpuSeries
    readonly property int aiEnhancePerfScale: Ui.PreferenceStore.aiEnhancePerfScale
    readonly property int aiEnhanceQualityScale: Ui.PreferenceStore.aiEnhanceQualityScale

    property ListModel devicesModelObject: ListModel {}

    function themeModeIndex(mode) {
        for (var i = 0; i < themeOptions.length; ++i) {
            if (themeOptions[i].mode === mode) {
                return i
            }
        }
        return 0
    }

    function localeModeIndex(mode) {
        for (var i = 0; i < localeOptions.length; ++i) {
            if (localeOptions[i].code === mode) {
                return i
            }
        }
        return 0
    }

    function setThemeMode(mode) {
        Ui.Style.themeMode = mode
    }

    function setLocaleMode(mode) {
        Ui.I18n.setLocaleMode(mode)
    }

    function rebuildOverviewCards() {
        overviewCards = [
            {
                icon: "qrc:/mi/e2ee/ui/icons/check.svg",
                title: Ui.I18n.t("dialog.securityCenter.transportTitle"),
                value: transportHeadline,
                detail: connectionSummary(),
                tone: transportTone
            },
            {
                icon: "qrc:/mi/e2ee/ui/icons/info.svg",
                title: Ui.I18n.t("dialog.securityCenter.trustTitle"),
                value: trustHeadline,
                detail: trustDetail,
                tone: trustTone
            },
            {
                icon: "qrc:/mi/e2ee/ui/icons/device.svg",
                title: Ui.I18n.t("dialog.securityCenter.devicesTitle"),
                value: linkedDevicesSummary,
                detail: Ui.I18n.t("dialog.securityCenter.devicesHint"),
                tone: linkedDeviceCount > 0 ? "healthy" : "review"
            },
            {
                icon: "qrc:/mi/e2ee/ui/icons/clock.svg",
                title: Ui.I18n.t("dialog.securityCenter.serverTitle"),
                value: gatewayDisplayDetail.length > 0 ? gatewayDisplayDetail : gatewayDisplayState,
                detail: Ui.I18n.t("dialog.securityCenter.serverHint"),
                tone: "checking"
            }
        ]
    }

    function refresh() {
        devicesModelObject.clear()
        gatewayDisplayState = client && client.gatewayDisplayState ? client.gatewayDisplayState() : ""
        gatewayDisplayDetail = client && client.gatewayDisplayDetail ? client.gatewayDisplayDetail() : ""
        currentDeviceDisplay = client && client.maskedCurrentDeviceId ? client.maskedCurrentDeviceId() : ""
        currentDeviceCopyValue = client
                ? ((client.deviceDisplayId && client.deviceDisplayId.length > 0)
                   ? client.deviceDisplayId
                   : (client.deviceId || ""))
                : ""

        var devices = client && client.listDevicesDisplay ? client.listDevicesDisplay() : []
        for (var i = 0; i < devices.length; ++i) {
            var device = devices[i]
            if (device.isCurrent === true) {
                continue
            }
            devicesModelObject.append({
                deviceId: device.deviceId || "",
                displayId: device.copyValue || "",
                maskedDeviceDisplayId: device.maskedDeviceDisplayId || "",
                maskedDeviceId: device.maskedDeviceDisplayId || "",
                lastSeenSec: device.lastSeenSec || 0,
                lastSeenDisplay: device.lastSeenDisplay || "",
                copyValue: device.copyValue || ""
            })
        }

        rebuildOverviewCards()
    }

    function kickDevice(deviceId) {
        if (!client || !client.kickDevice) {
            return false
        }
        var ok = client.kickDevice(deviceId)
        if (ok) {
            refresh()
        }
        return ok
    }

    function connectionSummary() {
        if (transportHealthy) {
            return Ui.I18n.t("dialog.securityCenter.transportHealthyHint")
        }
        if (client && client.remoteError && client.remoteError.length > 0) {
            return client.remoteError
        }
        return Ui.I18n.t("dialog.securityCenter.transportNeedsAttentionHint")
    }

    function requestAiQuality(scale) {
        if (scale === 4 && !Ui.PreferenceStore.aiEnhanceX4Confirmed) {
            return false
        }
        Ui.PreferenceStore.setAiEnhanceQualityLevel(scale)
        return true
    }

    function copyToInternalClipboard(text) {
        Ui.AppStore.setInternalClipboard(text)
    }

    Connections {
        target: store.client
        ignoreUnknownSignals: true

        function onDeviceChanged() {
            store.refresh()
        }

        function onConnectionChanged() {
            store.refresh()
        }

        function onAuthStateChanged() {
            store.refresh()
        }
    }

    Connections {
        target: Ui.I18n

        function onLocaleChanged() {
            store.refresh()
        }
    }

    Component.onCompleted: refresh()
}
