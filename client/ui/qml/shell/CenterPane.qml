import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 6.2
import QtQuick.Window 2.15
import QtMultimedia 6.2
import QtPositioning 6.2
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root
    property var bridge: typeof clientBridge === "undefined" ? null : clientBridge
    property bool smokeMode: typeof uiSmokeMode !== "undefined" ? !!uiSmokeMode : false
    readonly property Window hostWindow: root.Window.window
    readonly property bool smokePostLoginScene: smokeMode && Ui.SmokeSceneStore.normalizedScene === "post_login"
    readonly property bool smokePostLoginLightScene: smokeMode && Ui.SmokeSceneStore.postLoginLightScene
    readonly property string shellSurface: Ui.AppStore.currentShellSurface || "chat"
    readonly property bool showingChatSurface: shellSurface === "chat"
    readonly property bool showingContactsSurface: shellSurface === "contacts"
    readonly property bool showingCallsSurface: shellSurface === "calls"
    readonly property bool showingSettingsSurface: shellSurface === "settings"
    readonly property bool showingSecuritySurface: shellSurface === "security"
    readonly property int currentThemeOptionIndex: Math.max(0, Ui.SecurityDisplayStore.themeModeIndex(Ui.Style.themeMode))
    readonly property int currentLocaleOptionIndex: Math.max(0, Ui.SecurityDisplayStore.localeModeIndex(Ui.I18n.localeMode))
    readonly property var currentThemeOption: Ui.SecurityDisplayStore.themeOptions.length > 0
                                              ? Ui.SecurityDisplayStore.themeOptions[Math.min(currentThemeOptionIndex,
                                                                                              Ui.SecurityDisplayStore.themeOptions.length - 1)]
                                              : null
    readonly property var currentLocaleOption: Ui.SecurityDisplayStore.localeOptions.length > 0
                                               ? Ui.SecurityDisplayStore.localeOptions[Math.min(currentLocaleOptionIndex,
                                                                                               Ui.SecurityDisplayStore.localeOptions.length - 1)]
                                               : null

    property bool chatSearchVisible: false
    property bool stickToBottom: true
    property bool hasChat: Ui.ChatDisplayStore.currentChatId.length > 0
    readonly property bool adaptiveThreeColumn: (hostWindow ? hostWindow.width : width) >= Ui.Style.threeColumnMinWidth
    readonly property bool detailsPaneActive: hasChat && (adaptiveThreeColumn || Ui.ChatDisplayStore.rightPaneVisible)
    readonly property bool drawerDetailActive: hasChat &&
                                              Ui.ChatDisplayStore.rightPaneVisible &&
                                              !adaptiveThreeColumn
    readonly property int drawerReserveWidth: drawerDetailActive
                                              ? (((hostWindow ? hostWindow.width : width) >= Ui.Style.twoColumnDrawerMinWidth)
                                                 ? Ui.Style.rightPaneDrawerCompactWidth
                                                 : Ui.Style.rightPaneWidthDrawerNarrow)
                                              : 0
    readonly property int chatColumnMaxWidth: 860
    readonly property int utilityColumnMaxWidth: 920
    readonly property int centeredChatColumnWidth: Math.max(320,
                                                            Math.min(chatColumnMaxWidth,
                                                                     width - Ui.Style.paddingL * 2 - drawerReserveWidth))
    readonly property int centeredUtilityColumnWidth: Math.max(320,
                                                               Math.min(utilityColumnMaxWidth,
                                                                        width - Ui.Style.paddingL * 2 - drawerReserveWidth))
    readonly property real contentCenterOffset: drawerReserveWidth > 0 ? -drawerReserveWidth / 2 : 0
    property real actionScale: 1.0
    property real topBarScale: 1.0
    property int actionButtonSize: 30
    property int actionIconSize: 15
    property int inputButtonSize: 30
    property int inputIconSize: 16
    property int composerCornerSafeInset: Ui.Style.paddingS
    property int actionTopBarHeight: Ui.Style.topBarHeight
    property bool emojiLoaded: false
    property int emojiPopupWidth: 280
    property int emojiPopupHeight: 220
    property int emojiCellSize: 28
    property int stickerCellSize: 54
    property int stickerSize: 120
    property int emojiTabIndex: 0
    property string pendingDownloadId: ""
    property string pendingDownloadKey: ""
    property string pendingDownloadName: ""
    property int pendingDownloadSize: 0
    property bool imeChineseMode: true
    property bool imeComposing: false
    property bool imeShiftPressed: false
    property bool imeShiftUsed: false
    property int imeStartPos: 0
    property int imeLength: 0
    property string imeBuffer: ""
    property var imeCandidates: []
    property int imeCandidateIndex: 0
    property string imePreedit: ""
    property bool internalImeReady: !!(!smokeMode &&
                                       Ui.PreferenceStore.internalImeEnabled &&
                                       bridge &&
                                       bridge.imeAvailable &&
                                       bridge.imeAvailable())
    property bool imePopupVisible: internalImeReady && imeComposing &&
                                   imeCandidates && imeCandidates.length > 0
    property int callDurationSec: 0
    property double callStartMs: 0
    property bool callMicEnabled: true
    property bool callCameraEnabled: true
    property int contextMenuPadding: 10

    FontMetrics {
        id: contextMenuMetrics
        font.family: Ui.Style.fontFamily
        font.pixelSize: 12
    }

    function formatCallDuration(totalSec) {
        var sec = Math.max(0, totalSec || 0)
        var hours = Math.floor(sec / 3600)
        var minutes = Math.floor((sec % 3600) / 60)
        var seconds = sec % 60
        var hh = hours > 0 ? (hours < 10 ? "0" + hours : "" + hours) : ""
        var mm = minutes < 10 ? "0" + minutes : "" + minutes
        var ss = seconds < 10 ? "0" + seconds : "" + seconds
        return hours > 0 ? (hh + ":" + mm + ":" + ss) : (mm + ":" + ss)
    }

    function utilityFriendlyThemeDetail() {
        return currentThemeOption && currentThemeOption.label
                ? currentThemeOption.label
                : Ui.I18n.t("settings.theme.system")
    }

    function utilityFriendlyLocaleDetail() {
        return currentLocaleOption && currentLocaleOption.label
                ? currentLocaleOption.label
                : Ui.I18n.t("settings.language")
    }

    function utilityFriendlySecurityDetail() {
        if (Ui.SecurityDisplayStore.transportHealthy) {
            return Ui.I18n.usesCjkLocale ? "已加密并保持连接" : "Encrypted and connected"
        }
        return Ui.I18n.usesCjkLocale ? "需要重新检查会话" : "Session needs review"
    }

    function utilityFriendlyTrustDetail() {
        var stateText = (Ui.SecurityDisplayStore.gatewayDisplayState || "").toLowerCase()
        if (stateText.indexOf("pin") !== -1 || stateText.indexOf("固定") !== -1 ||
                stateText.indexOf("local") !== -1 || stateText.indexOf("本地") !== -1) {
            return Ui.I18n.usesCjkLocale ? "已验证" : "Verified"
        }
        return Ui.I18n.usesCjkLocale ? "待确认" : "Pending"
    }

    function utilityFriendlyGatewayDetail() {
        if (Ui.SecurityDisplayStore.gatewayDisplayDetail.length > 0 ||
                Ui.SecurityDisplayStore.gatewayDisplayState.length > 0) {
            return Ui.I18n.usesCjkLocale ? "已连接受信网络" : "Connected over trusted transport"
        }
        return Ui.I18n.usesCjkLocale ? "等待连接" : "Waiting for connection"
    }

    function utilityFriendlyDeviceName(displayId, index, isCurrent) {
        if (isCurrent === true) {
            return Ui.I18n.usesCjkLocale ? "这台设备" : "This device"
        }
        var lowered = (displayId || "").toLowerCase()
        if (lowered.indexOf("pad") !== -1 || lowered.indexOf("tab") !== -1) {
            return Ui.I18n.usesCjkLocale ? "平板" : "Tablet"
        }
        if (lowered.indexOf("desk") !== -1 || lowered.indexOf("lap") !== -1 || lowered.indexOf("pc") !== -1) {
            return Ui.I18n.usesCjkLocale ? "桌面端" : "Desktop"
        }
        return Ui.I18n.usesCjkLocale
                ? ("已连接设备 " + (index + 1))
                : ("Linked device " + (index + 1))
    }

    function utilityCallTargetId() {
        if (Ui.ChatDisplayStore.currentChatId.length > 0) {
            return Ui.ChatDisplayStore.currentChatId
        }
        if (Ui.ChatDisplayStore.filteredDialogsModel.count > 0) {
            return Ui.ChatDisplayStore.filteredDialogsModel.get(0).chatId || ""
        }
        return ""
    }

    function utilityCallTargetTitle() {
        if (Ui.ChatDisplayStore.currentChatId.length > 0) {
            return Ui.ChatDisplayStore.currentChatTitle
        }
        if (Ui.ChatDisplayStore.filteredDialogsModel.count > 0) {
            return Ui.ChatDisplayStore.filteredDialogsModel.get(0).title || ""
        }
        return ""
    }

    function utilityCallTargetAvatarSeed() {
        if (Ui.ChatDisplayStore.currentChatId.length > 0) {
            return Ui.ChatDisplayStore.currentChatId
        }
        if (Ui.ChatDisplayStore.filteredDialogsModel.count > 0) {
            var dialogEntry = Ui.ChatDisplayStore.filteredDialogsModel.get(0)
            return dialogEntry.avatarKey || dialogEntry.title || ""
        }
        return ""
    }

    function utilityCallTargetAvatarMode() {
        if (Ui.ChatDisplayStore.currentChatId.length > 0) {
            return Ui.ChatDisplayStore.currentChatType === "group" ? "group" : "person"
        }
        if (Ui.ChatDisplayStore.filteredDialogsModel.count > 0) {
            return Ui.ChatDisplayStore.filteredDialogsModel.get(0).avatarMode || ""
        }
        return ""
    }

    function previewIconFor(kind) {
        switch (kind) {
        case "photo":
            return "qrc:/mi/e2ee/ui/icons/image.svg"
        case "video":
            return "qrc:/mi/e2ee/ui/icons/video.svg"
        case "voice":
            return "qrc:/mi/e2ee/ui/icons/mic.svg"
        case "link":
            return "qrc:/mi/e2ee/ui/icons/location.svg"
        default:
            return "qrc:/mi/e2ee/ui/icons/file.svg"
        }
    }

    function previewTintFor(kind) {
        switch (kind) {
        case "photo":
            return Qt.rgba(37 / 255, 99 / 255, 235 / 255, Ui.Style.isDark ? 0.20 : 0.12)
        case "video":
            return Qt.rgba(59 / 255, 130 / 255, 246 / 255, Ui.Style.isDark ? 0.20 : 0.12)
        case "voice":
            return Qt.rgba(5 / 255, 150 / 255, 105 / 255, Ui.Style.isDark ? 0.20 : 0.12)
        case "link":
            return Qt.rgba(14 / 255, 165 / 255, 233 / 255, Ui.Style.isDark ? 0.20 : 0.12)
        default:
            return Qt.rgba(100 / 255, 116 / 255, 139 / 255, Ui.Style.isDark ? 0.16 : 0.10)
        }
    }

    component UtilityNavRow: Item {
        id: utilityNavRow
        property string iconSource: ""
        property color iconBg: Ui.Style.railAccentBg
        property color iconBorder: Ui.Style.railAccentBorder
        property string titleText: ""
        property string detailText: ""
        property string trailingText: ""
        signal clicked()

        implicitHeight: trailingText.length > 0 || detailText.length === 0 ? 48 : 54

        Rectangle {
            id: utilityNavIcon
            width: 34
            height: 34
            radius: 17
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: utilityNavRow.iconBg
            border.width: 1
            border.color: utilityNavRow.iconBorder

            Image {
                anchors.centerIn: parent
                width: 14
                height: 14
                fillMode: Image.PreserveAspectFit
                source: utilityNavRow.iconSource
                smooth: true
                antialiasing: true
            }
        }

        Image {
            id: utilityNavChevron
            width: 12
            height: 12
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
            source: "qrc:/mi/e2ee/ui/icons/chevron-right.svg"
            smooth: true
            antialiasing: true
        }

        Text {
            id: utilityNavTrailing
            visible: utilityNavRow.trailingText.length > 0
            width: visible ? 78 : 0
            anchors.right: utilityNavChevron.left
            anchors.rightMargin: visible ? Ui.Style.paddingS : 0
            anchors.verticalCenter: parent.verticalCenter
            text: utilityNavRow.trailingText
            maximumLineCount: 1
            elide: Text.ElideRight
            color: Ui.Style.textSecondary
            font.pixelSize: 11
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignRight
        }

        Column {
            anchors.left: utilityNavIcon.right
            anchors.right: utilityNavTrailing.visible ? utilityNavTrailing.left : utilityNavChevron.left
            anchors.leftMargin: Ui.Style.paddingM
            anchors.rightMargin: Ui.Style.paddingS
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: utilityNavRow.titleText
                color: Ui.Style.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            Text {
                visible: utilityNavRow.detailText.length > 0 && !utilityNavTrailing.visible
                width: parent.width
                text: utilityNavRow.detailText
                color: Ui.Style.textSecondary
                font.pixelSize: 11
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: utilityNavRow.clicked()
        }

    }

    component UtilityToggleRow: Item {
        id: utilityToggleRow
        property string iconSource: ""
        property color iconBg: Ui.Style.topBarPillBg
        property color iconBorder: Ui.Style.topBarPillBorder
        property string titleText: ""
        property string detailText: ""
        property bool checked: false
        signal toggled(bool checked)

        implicitHeight: 72

        Rectangle {
            id: utilityToggleIcon
            width: 34
            height: 34
            radius: 17
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: utilityToggleRow.iconBg
            border.width: 1
            border.color: utilityToggleRow.iconBorder

            Image {
                anchors.centerIn: parent
                width: 14
                height: 14
                fillMode: Image.PreserveAspectFit
                source: utilityToggleRow.iconSource
                smooth: true
                antialiasing: true
            }
        }

        Components.InlineSwitch {
            id: utilityToggleSwitch
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            checked: utilityToggleRow.checked
            onToggled: utilityToggleRow.toggled(checked)
        }

        Column {
            anchors.left: utilityToggleIcon.right
            anchors.right: utilityToggleSwitch.left
            anchors.leftMargin: Ui.Style.paddingM
            anchors.rightMargin: Ui.Style.paddingM
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: utilityToggleRow.titleText
                color: Ui.Style.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            Text {
                visible: utilityToggleRow.detailText.length > 0
                width: parent.width
                text: utilityToggleRow.detailText
                color: Ui.Style.textSecondary
                font.pixelSize: 11
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

    }

    component UtilitySummaryRow: Item {
        id: utilitySummaryRow
        property string iconSource: ""
        property color iconBg: Ui.Style.railAccentBg
        property color iconBorder: Ui.Style.railAccentBorder
        property string labelText: ""
        property string valueText: ""

        implicitHeight: Ui.Style.settingsRowMinHeight

        Rectangle {
            id: utilitySummaryIcon
            width: 34
            height: 34
            radius: 17
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: utilitySummaryRow.iconBg
            border.width: 1
            border.color: utilitySummaryRow.iconBorder

            Image {
                anchors.centerIn: parent
                width: 14
                height: 14
                fillMode: Image.PreserveAspectFit
                source: utilitySummaryRow.iconSource
                smooth: true
                antialiasing: true
            }
        }

        Text {
            id: utilitySummaryValue
            width: 168
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: utilitySummaryRow.valueText
            color: Ui.Style.textSecondary
            font.pixelSize: 11
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignRight
            maximumLineCount: 1
            elide: Text.ElideRight
        }

        Text {
            anchors.left: utilitySummaryIcon.right
            anchors.right: utilitySummaryValue.left
            anchors.leftMargin: Ui.Style.paddingM
            anchors.rightMargin: Ui.Style.paddingM
            anchors.verticalCenter: parent.verticalCenter
            text: utilitySummaryRow.labelText
            color: Ui.Style.textPrimary
            font.pixelSize: 13
            font.weight: Font.DemiBold
            maximumLineCount: 1
            elide: Text.ElideRight
        }

    }

    component UtilityDeviceRow: Item {
        id: utilityDeviceRow
        property string titleText: ""
        property string detailText: ""

        implicitHeight: 40

        Rectangle {
            id: utilityDeviceIcon
            width: 34
            height: 34
            radius: 17
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: Ui.Style.railAccentBg
            border.width: 1
            border.color: Ui.Style.railAccentBorder

            Image {
                anchors.centerIn: parent
                width: 14
                height: 14
                fillMode: Image.PreserveAspectFit
                source: "qrc:/mi/e2ee/ui/icons/device.svg"
            }
        }

        Column {
            anchors.left: utilityDeviceIcon.right
            anchors.right: parent.right
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: utilityDeviceRow.titleText
                color: Ui.Style.textPrimary
                font.pixelSize: 12
                font.weight: Font.DemiBold
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: utilityDeviceRow.detailText
                color: Ui.Style.textSecondary
                font.pixelSize: 11
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

    }

    component UtilityCallRow: Item {
        id: utilityCallRow
        property string titleText: ""
        property string subtitleText: ""
        property string timeText: ""
        property string avatarTitle: ""
        property string avatarSeed: ""
        property string avatarMode: ""
        signal clicked()
        signal actionClicked()

        readonly property string detailLine: {
            if (utilityCallRow.subtitleText.length > 0 && utilityCallRow.timeText.length > 0) {
                return utilityCallRow.subtitleText + " · " + utilityCallRow.timeText
            }
            if (utilityCallRow.subtitleText.length > 0) {
                return utilityCallRow.subtitleText
            }
            return utilityCallRow.timeText
        }

        implicitHeight: detailLine.length > 0 ? 54 : 48

        Components.IdentityAvatar {
            id: utilityCallAvatar
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            size: 38
            titleText: utilityCallRow.avatarTitle
            seedText: utilityCallRow.avatarSeed
            mode: utilityCallRow.avatarMode
            presenceState: "idle"
        }

        Components.IconButton {
            id: utilityCallAction
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            accessibleName: Ui.I18n.t("chat.call")
            icon.source: "qrc:/mi/e2ee/ui/icons/phone.svg"
            buttonSize: 34
            iconSize: 16
            onClicked: utilityCallRow.actionClicked()
        }

        Column {
            anchors.left: utilityCallAvatar.right
            anchors.right: utilityCallAction.left
            anchors.leftMargin: Ui.Style.paddingM
            anchors.rightMargin: Ui.Style.paddingM
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                width: parent.width
                text: utilityCallRow.titleText
                color: Ui.Style.textPrimary
                font.pixelSize: 13
                font.weight: Font.DemiBold
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            Text {
                visible: detailLine.length > 0
                width: parent.width
                text: detailLine
                color: Ui.Style.textSecondary
                font.pixelSize: 11
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }

        MouseArea {
            anchors.fill: parent
            anchors.rightMargin: 42
            cursorShape: Qt.PointingHandCursor
            onClicked: utilityCallRow.clicked()
        }

    }

    function contextMenuWidth(labels) {
        var maxWidth = 0
        for (var i = 0; i < labels.length; ++i) {
            var label = labels[i] || ""
            var w = contextMenuMetrics.advanceWidth(label)
            if (w > maxWidth) {
                maxWidth = w
            }
        }
        return Math.ceil(maxWidth) + contextMenuPadding * 2
    }

    function resetCallControls() {
        callMicEnabled = true
        callCameraEnabled = true
        if (clientBridge && clientBridge.setCallMicEnabled) {
            clientBridge.setCallMicEnabled(true)
        }
        if (clientBridge && clientBridge.setCallCameraEnabled) {
            clientBridge.setCallCameraEnabled(true)
        }
    }

    function cycleThemeOption() {
        var options = Ui.SecurityDisplayStore.themeOptions || []
        if (options.length === 0) {
            return
        }
        var nextIndex = (Ui.SecurityDisplayStore.themeModeIndex(Ui.Style.themeMode) + 1 + options.length) % options.length
        Ui.SecurityDisplayStore.setThemeMode(options[nextIndex].mode)
    }

    function cycleLocaleOption() {
        var options = Ui.SecurityDisplayStore.localeOptions || []
        if (options.length === 0) {
            return
        }
        var nextIndex = (Ui.SecurityDisplayStore.localeModeIndex(Ui.I18n.localeMode) + 1 + options.length) % options.length
        Ui.SecurityDisplayStore.setLocaleMode(options[nextIndex].code)
    }

    function showSearch() {
        if (!showingChatSurface || !hasChat) {
            return
        }
        chatSearchVisible = true
        chatSearchField.focusInput()
    }
    function toggleDetailsPane() {
        if (!hasChat) {
            return
        }
        if (adaptiveThreeColumn) {
            return
        }
        Ui.ChatDisplayStore.toggleRightPane()
    }

    function clearChatSearch() {
        if (!chatSearchVisible) {
            return false
        }
        chatSearchVisible = false
        chatSearchField.text = ""
        return true
    }

    function handleEscape() {
        if (chatSearchVisible) {
            return clearChatSearch()
        }
        return false
    }
    function showAttachPopup() {
        if (!hasChat) {
            return
        }
        var pos = attachButton.mapToItem(root, 0, 0)
        var iconLeft = pos.x + (attachButton.width - inputIconSize) / 2
        var desiredX = iconLeft - attachPopup.contentLeftPadding
        var minX = Ui.Style.paddingS
        var maxX = root.width - attachPopup.implicitWidth - Ui.Style.paddingS
        attachPopup.x = Math.max(minX, Math.min(maxX, desiredX))
        var desiredY = pos.y - attachPopup.implicitHeight - 8
        attachPopup.y = Math.max(Ui.Style.paddingS, desiredY)
        attachPopup.open()
    }
    function resolveDialogUrl(dialog) {
        if (!dialog) {
            return ""
        }
        var url = dialog.selectedFile
        if (!url && dialog.selectedFiles && dialog.selectedFiles.length > 0) {
            url = dialog.selectedFiles[0]
        }
        if (!url && dialog.fileUrl !== undefined) {
            url = dialog.fileUrl
        }
        if (!url) {
            return ""
        }
        return url.toString ? url.toString() : ("" + url)
    }
    function promptFileDownload(fileId, fileKey, fileName, fileSize) {
        pendingDownloadId = fileId || ""
        pendingDownloadKey = fileKey || ""
        pendingDownloadName = fileName && fileName.length > 0 ? fileName : (fileId || "")
        pendingDownloadSize = fileSize || 0
        if (pendingDownloadId.length === 0 || pendingDownloadKey.length === 0) {
            return
        }
        if (arguments.length > 4 && arguments[4] === true) {
            openDownloadSaveDialog()
        } else {
            downloadConfirm.open()
        }
    }
    function openDownloadSaveDialog() {
        if (clientBridge && clientBridge.defaultDownloadFileUrl) {
            var url = clientBridge.defaultDownloadFileUrl(pendingDownloadName)
            if (url) {
                downloadSaveDialog.selectedFile = url
            }
        }
        downloadSaveDialog.open()
    }
    function openImagePreview(url, name) {
        var resolved = url && url.toString ? url.toString() : (url ? "" + url : "")
        if (!resolved || resolved.length === 0) {
            return
        }
        imageViewer.openWith(resolved, name || "")
    }
    function requestImageEnhanceForMessage(messageId, url, name) {
        if (!Ui.ChatDisplayStore.aiEnhanceEnabled) {
            return
        }
        if (!clientBridge || !clientBridge.requestImageEnhanceForMessage) {
            return
        }
        if (!messageId || messageId.length === 0) {
            return
        }
        var targetUrl = url && url.toString ? url.toString() : (url ? "" + url : "")
        if (!targetUrl || targetUrl.length === 0) {
            return
        }
        clientBridge.requestImageEnhanceForMessage(messageId, targetUrl, name || "")
    }
    function loadEmoji() {
        if (emojiLoaded) {
            return
        }
        emojiLoaded = true
        emojiModel.clear()
        var request = new XMLHttpRequest()
        request.onreadystatechange = function() {
            if (request.readyState !== XMLHttpRequest.DONE) {
                return
            }
            if (request.status === 0 || request.status === 200) {
                try {
                    var codes = JSON.parse(request.responseText)
                    var limit = Math.min(codes.length, 160)
                    for (var i = 0; i < limit; ++i) {
                        var code = parseInt(codes[i], 16)
                        if (!isNaN(code)) {
                            emojiModel.append({ value: String.fromCodePoint(code) })
                        }
                    }
                } catch (err) {
                    emojiLoaded = false
                    emojiModel.clear()
                }
            } else {
                emojiLoaded = false
                emojiModel.clear()
            }
        }
        request.open("GET", "qrc:/mi/e2ee/ui/emoji/emoji.json")
        request.send()
    }
    function loadStickers() {
        stickerModel.clear()
        if (!clientBridge || !clientBridge.stickerItems) {
            return
        }
        var items = clientBridge.stickerItems()
        for (var i = 0; i < items.length; ++i) {
            var item = items[i]
            stickerModel.append({
                stickerId: item.id,
                title: item.title,
                animated: item.animated,
                path: item.path
            })
        }
    }
    function showStickerImport() {
        if (!hasChat) {
            return
        }
        stickerPicker.open()
    }
    function showEmojiPopup() {
        if (!hasChat) {
            return
        }
        if (emojiPopup.visible) {
            emojiPopup.close()
            return
        }
        loadEmoji()
        loadStickers()
        var pos = emojiButton.mapToItem(root, 0, 0)
        var desiredX = pos.x + emojiButton.width - emojiPopup.width
        var minX = Ui.Style.paddingS
        var maxX = root.width - emojiPopup.width - Ui.Style.paddingS
        emojiPopup.x = Math.max(minX, Math.min(maxX, desiredX))
        var desiredY = pos.y - emojiPopup.height - 8
        emojiPopup.y = Math.max(Ui.Style.paddingS, desiredY)
        emojiPopup.open()
    }
    function insertEmoji(value) {
        if (!value || !messageInput) {
            return
        }
        messageInput.insert(messageInput.cursorPosition, value)
        messageInput.forceActiveFocus()
    }
    function selectedRange() {
        if (!messageInput) {
            return null
        }
        var start = messageInput.selectionStart
        var end = messageInput.selectionEnd
        if (start === undefined || end === undefined) {
            return null
        }
        if (start === end) {
            return null
        }
        if (start > end) {
            var tmp = start
            start = end
            end = tmp
        }
        return { start: start, end: end }
    }
    function replaceSelectionWith(text) {
        if (!text || !messageInput) {
            return
        }
        var range = selectedRange()
        if (range) {
            messageInput.remove(range.start, range.end)
            messageInput.cursorPosition = range.start
        }
        messageInput.insert(messageInput.cursorPosition, text)
    }
    function contextCopy(cut) {
        var selected = messageInput.selectedText || ""
        if (selected.length === 0) {
            return
        }
        if (Ui.PreferenceStore.clipboardIsolationEnabled) {
            Ui.ChatDisplayStore.setInternalClipboard(selected)
            if (cut) {
                var range = selectedRange()
                if (range) {
                    messageInput.remove(range.start, range.end)
                    messageInput.cursorPosition = range.start
                }
            }
            return
        }
        if (cut) {
            messageInput.cut()
        } else {
            messageInput.copy()
        }
    }
    function contextPaste() {
        if (!messageInput) {
            return
        }
        if (!Ui.PreferenceStore.clipboardIsolationEnabled) {
            messageInput.paste()
            return
        }
        var internalText = Ui.ChatDisplayStore.internalClipboardText || ""
        var internalMs = Ui.ChatDisplayStore.internalClipboardMs || 0
        var systemText = clientBridge ? clientBridge.systemClipboardText() : ""
        var systemMs = clientBridge ? clientBridge.systemClipboardTimestamp() : 0
        var text = internalText
        if (systemText.length > 0 && systemMs > internalMs) {
            text = systemText
        }
        if (text.length === 0) {
            return
        }
        replaceSelectionWith(text)
    }
    function contextSelectAll() {
        if (messageInput) {
            messageInput.selectAll()
        }
    }
    function contextCanPaste() {
        if (!Ui.PreferenceStore.clipboardIsolationEnabled) {
            return true
        }
        var internalText = Ui.ChatDisplayStore.internalClipboardText || ""
        var systemText = clientBridge ? clientBridge.systemClipboardText() : ""
        return internalText.length > 0 || systemText.length > 0
    }
    function externalImeActive() {
        if (internalImeReady) {
            return false
        }
        if (messageInput && messageInput.inputMethodComposing) {
            return true
        }
        return Qt.inputMethod && Qt.inputMethod.visible
    }
    function externalImeChineseMode() {
        if (!Qt.inputMethod || !Qt.inputMethod.locale) {
            return false
        }
        var name = (Qt.inputMethod.locale.name || "").toLowerCase()
        return name.indexOf("zh") === 0 || name.indexOf("zh_") === 0 || name.indexOf("zh-") === 0
    }
    function internalImeRimeAvailable() {
        return internalImeReady && clientBridge && clientBridge.imeRimeAvailable &&
               clientBridge.imeRimeAvailable()
    }
    function imeStatusText() {
        var source = internalImeReady
                     ? (internalImeRimeAvailable()
                        ? Ui.I18n.t("ime.source.rime")
                        : Ui.I18n.t("ime.source.custom"))
                     : Ui.I18n.t("ime.source.thirdParty")
        var lang = internalImeReady
                   ? (imeChineseMode ? Ui.I18n.t("ime.lang.zh") : Ui.I18n.t("ime.lang.en"))
                   : (externalImeChineseMode() ? Ui.I18n.t("ime.lang.zh") : Ui.I18n.t("ime.lang.en"))
        return Ui.I18n.t("ime.status.label") + ":" + source + " :" + lang
    }
    function resetImeState() {
        imeComposing = false
        imeShiftPressed = false
        imeShiftUsed = false
        imeStartPos = 0
        imeLength = 0
        imeBuffer = ""
        imeCandidates = []
        imeCandidateIndex = 0
        imePreedit = ""
    }
    function requestCurrentLocation() {
        locationDialog.errorText = ""
        locationDialog.locationBusy = true
        locationSourceLoader.active = !smokeMode
        if (smokeMode) {
            locationDialog.errorText = Ui.I18n.t("attach.locationUnavailable")
            locationDialog.locationBusy = false
            return
        }
        if (locationSourceLoader.status === Loader.Ready
                && locationSourceLoader.item
                && locationSourceLoader.item.update) {
            locationSourceLoader.item.update()
        }
    }
    function cancelImeComposition(keepText) {
        if (!imeComposing) {
            return
        }
        if (!keepText && imeLength > 0) {
            messageInput.remove(imeStartPos, imeStartPos + imeLength)
            messageInput.cursorPosition = imeStartPos
        }
        if (clientBridge && clientBridge.imeClear) {
            clientBridge.imeClear()
        }
        resetImeState()
    }
    function updateImeCandidates() {
        var list = []
        if (clientBridge && clientBridge.imeCandidates) {
            list = clientBridge.imeCandidates(imeBuffer, 5)
        }
        if (!list || list.length === 0) {
            list = [imeBuffer]
        }
        imeCandidates = list
        if (imeCandidateIndex >= imeCandidates.length) {
            imeCandidateIndex = 0
        }
        var preeditText = ""
        if (clientBridge && clientBridge.imePreedit) {
            preeditText = clientBridge.imePreedit()
        }
        imePreedit = preeditText.length > 0 ? preeditText : imeBuffer
    }
    function updateImeComposition() {
        if (!imeComposing) {
            return
        }
        if (imeLength > 0) {
            messageInput.remove(imeStartPos, imeStartPos + imeLength)
        }
        messageInput.insert(imeStartPos, imeBuffer)
        imeLength = imeBuffer.length
        messageInput.cursorPosition = imeStartPos + imeLength
        updateImeCandidates()
    }
    function startImeComposition(ch) {
        if (!messageInput) {
            return
        }
        if (!imeComposing) {
            var selStart = Math.min(messageInput.selectionStart, messageInput.selectionEnd)
            var selEnd = Math.max(messageInput.selectionStart, messageInput.selectionEnd)
            if (!isNaN(selStart) && !isNaN(selEnd) && selEnd > selStart) {
                messageInput.remove(selStart, selEnd)
                messageInput.cursorPosition = selStart
            }
            imeComposing = true
            imeStartPos = messageInput.cursorPosition
            imeLength = 0
            imeBuffer = ""
            imeCandidateIndex = 0
            imeCandidates = []
            imePreedit = ""
        }
        imeBuffer += ch
        updateImeComposition()
    }
    function commitImeCandidate(index) {
        if (!imeComposing) {
            return
        }
        if (!imeCandidates || imeCandidates.length === 0) {
            cancelImeComposition(true)
            return
        }
        var safeIndex = Math.max(0, Math.min(index, imeCandidates.length - 1))
        var candidate = imeCandidates[safeIndex]
        if (imeLength > 0) {
            messageInput.remove(imeStartPos, imeStartPos + imeLength)
        }
        messageInput.insert(imeStartPos, candidate)
        messageInput.cursorPosition = imeStartPos + candidate.length
        if (clientBridge && clientBridge.imeCommit) {
            clientBridge.imeCommit(safeIndex)
        }
        resetImeState()
    }
    function handleImeKey(event) {
        if (!internalImeReady || !imeChineseMode || externalImeActive()) {
            return false
        }
        if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) {
            return false
        }
        var key = event.key
        var text = event.text || ""
        if (text.length === 0 && key >= Qt.Key_A && key <= Qt.Key_Z) {
            text = String.fromCharCode(key)
        }
        if (!imeComposing) {
            if (text.length === 1 && /[a-zA-Z]/.test(text)) {
                startImeComposition(text.toLowerCase())
                event.accepted = true
                return true
            }
            return false
        }
        if (key === Qt.Key_Backspace) {
            if (imeBuffer.length > 0) {
                imeBuffer = imeBuffer.slice(0, -1)
                if (imeBuffer.length === 0) {
                    cancelImeComposition(false)
                } else {
                    updateImeComposition()
                }
            } else {
                cancelImeComposition(false)
            }
            event.accepted = true
            return true
        }
        if (key === Qt.Key_Space || key === Qt.Key_Return || key === Qt.Key_Enter) {
            commitImeCandidate(imeCandidateIndex)
            event.accepted = true
            return true
        }
        if (key >= Qt.Key_1 && key <= Qt.Key_5) {
            commitImeCandidate(key - Qt.Key_1)
            event.accepted = true
            return true
        }
        if (key === Qt.Key_Left) {
            imeCandidateIndex = Math.max(0, imeCandidateIndex - 1)
            updateImeCandidates()
            event.accepted = true
            return true
        }
        if (key === Qt.Key_Right) {
            imeCandidateIndex = Math.min(imeCandidateIndex + 1, imeCandidates.length - 1)
            updateImeCandidates()
            event.accepted = true
            return true
        }
        if (key === Qt.Key_Up || key === Qt.Key_Down) {
            event.accepted = true
            return true
        }
        if (key === Qt.Key_Escape) {
            cancelImeComposition(false)
            event.accepted = true
            return true
        }
        if (text.length === 1 && /[a-zA-Z]/.test(text)) {
            imeBuffer += text.toLowerCase()
            updateImeComposition()
            event.accepted = true
            return true
        }
        if (text.length === 1 && !/[a-zA-Z\\s]/.test(text)) {
            commitImeCandidate(imeCandidateIndex)
            messageInput.insert(messageInput.cursorPosition, text)
            event.accepted = true
            return true
        }
        return false
    }
    onHasChatChanged: {
        if (!hasChat) {
            clearChatSearch()
        }
    }

    Connections {
        target: Ui.PreferenceStore
        function onInternalImeEnabledChanged() {
            if (!Ui.PreferenceStore.internalImeEnabled) {
                cancelImeComposition(true)
                if (clientBridge && clientBridge.imeReset) {
                    clientBridge.imeReset()
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: showingChatSurface && hasChat ? actionTopBarHeight : 0
            Layout.minimumHeight: showingChatSurface && hasChat ? actionTopBarHeight : 0
            Layout.maximumHeight: showingChatSurface && hasChat ? actionTopBarHeight : 0
            visible: showingChatSurface && hasChat
            color: "transparent"

            Rectangle {
                id: topBarCard
                width: root.centeredChatColumnWidth
                height: parent.height - 10
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: root.contentCenterOffset
                anchors.verticalCenter: parent.verticalCenter
                radius: Ui.Style.radiusContinuous
                color: Ui.Style.topBarBg
                border.width: 1
                border.color: Ui.Style.topBarPillBorder

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: Math.max(0, parent.radius - 1)
                    color: Ui.Style.alpha(Ui.Style.sidebarHairline, Ui.Style.isDark ? 0.03 : 0.20)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Ui.Style.paddingM
                    anchors.rightMargin: Ui.Style.paddingM
                    anchors.topMargin: 8
                    anchors.bottomMargin: 8
                    spacing: Ui.Style.paddingM

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingM

                        Components.IdentityAvatar {
                            Layout.preferredWidth: 36
                            Layout.preferredHeight: 36
                            size: 36
                            titleText: Ui.ChatDisplayStore.currentChatTitle
                            seedText: Ui.ChatDisplayStore.currentChatId
                            mode: Ui.ChatDisplayStore.currentChatType === "group" ? "group" : "person"
                            presenceState: Ui.ChatDisplayStore.currentChatType === "group" ? "secure" : "online"
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                text: Ui.ChatDisplayStore.currentChatTitle.length > 0
                                      ? Ui.ChatDisplayStore.currentChatTitle
                                      : Ui.I18n.t("chat.selectChat")
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                color: Ui.Style.textPrimary
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                renderType: Text.NativeRendering
                                antialiasing: true
                            }

                            RowLayout {
                                id: chatHeaderStateChips
                                Layout.fillWidth: true
                                spacing: 6

                                Rectangle {
                                    visible: Ui.ChatDisplayStore.currentChatId.length > 0
                                    radius: 9
                                    color: Ui.Style.sidebarMetaChipBg
                                    border.width: 1
                                    border.color: Ui.Style.sidebarMetaChipBorder
                                    implicitWidth: chatPrimaryStatusText.implicitWidth + 14
                                    implicitHeight: 18

                                    Text {
                                        id: chatPrimaryStatusText
                                        anchors.centerIn: parent
                                        text: Ui.ChatDisplayStore.currentChatType === "group"
                                              ? Ui.I18n.format("chat.members", Ui.ChatDisplayStore.currentChatMembers)
                                              : (Ui.ChatDisplayStore.currentChatSubtitle.length > 0
                                                 ? Ui.ChatDisplayStore.currentChatSubtitle
                                                 : Ui.I18n.t("chat.online"))
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                        color: Ui.Style.textSecondary
                                        renderType: Text.NativeRendering
                                        antialiasing: true
                                    }
                                }

                                Rectangle {
                                    visible: Ui.ChatDisplayStore.currentChatId.length > 0 &&
                                             Ui.ChatDisplayStore.isChatMuted(Ui.ChatDisplayStore.currentChatId)
                                    width: 18
                                    height: 18
                                    radius: 9
                                    color: Ui.Style.sidebarMetaChipBg
                                    border.width: 1
                                    border.color: Ui.Style.sidebarMetaChipBorder
                                    Accessible.ignored: true

                                    Image {
                                        anchors.centerIn: parent
                                        width: 10
                                        height: 10
                                        source: "qrc:/mi/e2ee/ui/icons/bell.svg"
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        antialiasing: true
                                    }
                                }

                                Rectangle {
                                    visible: Ui.ChatDisplayStore.currentChatId.length > 0 &&
                                             Ui.ChatDisplayStore.isChatStealth(Ui.ChatDisplayStore.currentChatId)
                                    width: 18
                                    height: 18
                                    radius: 9
                                    color: Ui.Style.railAccentBg
                                    border.width: 1
                                    border.color: Ui.Style.railAccentBorder
                                    Accessible.ignored: true

                                    Image {
                                        anchors.centerIn: parent
                                        width: 10
                                        height: 10
                                        source: "qrc:/mi/e2ee/ui/icons/offline.svg"
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        antialiasing: true
                                    }
                                }

                                Rectangle {
                                    visible: Ui.ChatDisplayStore.currentChatId.length > 0 &&
                                             Ui.ChatDisplayStore.isChatBlocked(Ui.ChatDisplayStore.currentChatId)
                                    width: 18
                                    height: 18
                                    radius: 9
                                    color: Qt.rgba(220 / 255, 38 / 255, 38 / 255, Ui.Style.isDark ? 0.20 : 0.10)
                                    border.width: 1
                                    border.color: Qt.rgba(220 / 255, 38 / 255, 38 / 255, Ui.Style.isDark ? 0.35 : 0.18)
                                    Accessible.ignored: true

                                    Image {
                                        anchors.centerIn: parent
                                        width: 10
                                        height: 10
                                        source: "qrc:/mi/e2ee/ui/icons/close.svg"
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        antialiasing: true
                                    }
                                }

                                Item {
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }

                    RowLayout {
                        id: actionRow
                        spacing: 2
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                        Components.SearchField {
                            id: chatSearchField
                            visible: chatSearchVisible
                            Layout.preferredWidth: 148
                            placeholderText: Ui.I18n.t("chat.find")
                            onInputActiveFocusChanged: {
                                if (!inputActiveFocus && text.length === 0) {
                                    root.clearChatSearch()
                                }
                            }
                        }

                        Components.IconButton {
                            icon.source: "qrc:/mi/e2ee/ui/icons/search.svg"
                            buttonSize: actionButtonSize
                            iconSize: actionIconSize
                            bgColor: Ui.Style.topBarPillBg
                            hoverBg: Ui.Style.hoverBg
                            pressedBg: Ui.Style.pressedBg
                            visible: !chatSearchVisible
                            onClicked: root.showSearch()
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("chat.find")
                        }
                        Components.IconButton {
                            icon.source: "qrc:/mi/e2ee/ui/icons/phone.svg"
                            buttonSize: actionButtonSize
                            iconSize: actionIconSize
                            bgColor: Ui.Style.topBarPillBg
                            hoverBg: Ui.Style.hoverBg
                            pressedBg: Ui.Style.pressedBg
                            enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("chat.call")
                            onClicked: Ui.ChatDisplayStore.handleCallAction(false)
                        }
                        Components.IconButton {
                            icon.source: "qrc:/mi/e2ee/ui/icons/video.svg"
                            buttonSize: actionButtonSize
                            iconSize: actionIconSize
                            bgColor: Ui.Style.topBarPillBg
                            hoverBg: Ui.Style.hoverBg
                            pressedBg: Ui.Style.pressedBg
                            enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("chat.video")
                            onClicked: Ui.ChatDisplayStore.handleCallAction(true)
                        }
                        Components.IconButton {
                            id: detailsPaneButton
                            icon.source: "qrc:/mi/e2ee/ui/icons/info.svg"
                            buttonSize: actionButtonSize
                            iconSize: actionIconSize
                            bgColor: detailsPaneActive ? Ui.Style.dialogSelectedBg : Ui.Style.topBarPillBg
                            hoverBg: detailsPaneActive ? Ui.Style.dialogSelectedBg : Ui.Style.hoverBg
                            pressedBg: detailsPaneActive ? Ui.Style.dialogSelectedBg : Ui.Style.pressedBg
                            baseColor: detailsPaneActive ? Ui.Style.iconActive : Ui.Style.iconMuted
                            hoverColor: Ui.Style.iconActive
                            pressColor: Ui.Style.iconActive
                            enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("chat.details")
                            onClicked: root.toggleDetailsPane()
                        }
                        Components.IconButton {
                            id: chatMoreButton
                            icon.source: "qrc:/mi/e2ee/ui/icons/more-vert.svg"
                            buttonSize: actionButtonSize
                            iconSize: actionIconSize
                            bgColor: Ui.Style.topBarPillBg
                            hoverBg: Ui.Style.hoverBg
                            pressedBg: Ui.Style.pressedBg
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("chat.more")
                            onClicked: chatMoreMenu.popup(chatMoreButton, 0, chatMoreButton.height + 4)
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Ui.Style.borderSubtle
                    opacity: 0.9
                }
            }

            Text {
                id: imeStatusLabel
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                text: ""
                color: Ui.Style.textSecondary
                font.pixelSize: Ui.Style.microTextSize
                font.weight: Font.Medium
                elide: Text.ElideRight
                width: Math.min(parent.width * 0.4, 300)
                horizontalAlignment: Text.AlignHCenter
                visible: false
            }

            Menu {
                id: chatMoreMenu
                property int compactWidth: 180
                implicitWidth: compactWidth
                width: compactWidth
                MenuItem {
                    text: Ui.I18n.t("chat.stealth")
                    checkable: true
                    checked: Ui.ChatDisplayStore.isChatStealth(Ui.ChatDisplayStore.currentChatId)
                    enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                    onTriggered: Ui.ChatDisplayStore.toggleChatStealth(Ui.ChatDisplayStore.currentChatId)
                }
                MenuItem {
                    text: Ui.I18n.t("chat.mute")
                    checkable: true
                    checked: Ui.ChatDisplayStore.isChatMuted(Ui.ChatDisplayStore.currentChatId)
                    enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                    onTriggered: Ui.ChatDisplayStore.toggleChatMuted(Ui.ChatDisplayStore.currentChatId)
                }
                MenuItem {
                    text: Ui.ChatDisplayStore.isChatBlocked(Ui.ChatDisplayStore.currentChatId)
                          ? Ui.I18n.t("chat.unblock")
                          : Ui.I18n.t("right.block")
                    enabled: Ui.ChatDisplayStore.currentChatId.length > 0 &&
                             Ui.ChatDisplayStore.currentChatType === "private"
                    onTriggered: Ui.ChatDisplayStore.toggleChatBlocked(Ui.ChatDisplayStore.currentChatId)
                }
            }

        }

        Rectangle {
            id: messageArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            property bool hasChatBackground: Ui.ChatDisplayStore.currentChatBackgroundUrl.length > 0
            color: Ui.Style.messageBg
            gradient: Gradient {
                GradientStop { position: 0.0; color: Ui.Style.messageGradientStart }
                GradientStop { position: 1.0; color: Ui.Style.messageGradientEnd }
            }

            Image {
                anchors.fill: parent
                source: Ui.ChatDisplayStore.currentChatBackgroundUrl
                fillMode: Image.PreserveAspectCrop
                smooth: true
                antialiasing: true
                mipmap: true
                cache: true
                asynchronous: true
                opacity: 0.10
                visible: root.showingChatSurface && messageArea.hasChatBackground
            }

            Image {
                anchors.fill: parent
                source: "qrc:/mi/e2ee/ui/qml/assets/wallpaper_tile.svg"
                fillMode: Image.Tile
                opacity: Ui.Style.isDark ? 0.04 : 0.02
                smooth: true
                visible: root.showingChatSurface && !messageArea.hasChatBackground
            }

            Rectangle {
                id: groupCallBanner
                property var callInfo: Ui.ChatDisplayStore.groupCallInfo(Ui.ChatDisplayStore.currentChatId)
                visible: root.showingChatSurface &&
                         Ui.ChatDisplayStore.currentChatType === "group" &&
                         callInfo
                height: visible ? 44 : 0
                anchors.top: parent.top
                width: root.centeredChatColumnWidth
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: root.contentCenterOffset
                anchors.topMargin: Ui.Style.paddingM
                radius: 12
                color: Ui.Style.panelBgAlt
                border.color: Ui.Style.borderSubtle

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Ui.Style.paddingS
                    anchors.rightMargin: Ui.Style.paddingS
                    anchors.topMargin: 6
                    anchors.bottomMargin: 6
                    spacing: Ui.Style.paddingS

                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: Ui.Style.railAccentBg
                        border.width: 1
                        border.color: Ui.Style.railAccentBorder
                        Layout.alignment: Qt.AlignVCenter

                        Image {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            fillMode: Image.PreserveAspectFit
                            source: groupCallBanner.callInfo && groupCallBanner.callInfo.video
                                    ? "qrc:/mi/e2ee/ui/icons/video.svg"
                                    : "qrc:/mi/e2ee/ui/icons/phone.svg"
                        }
                    }

                    Text {
                        text: groupCallBanner.callInfo && groupCallBanner.callInfo.video
                              ? Ui.I18n.t("chat.groupCallActiveVideo")
                              : Ui.I18n.t("chat.groupCallActiveVoice")
                        color: Ui.Style.textPrimary
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        Layout.alignment: Qt.AlignVCenter
                        elide: Text.ElideRight
                    }

                    Text {
                        text: Ui.I18n.t("chat.callDuration")
                              .arg(formatCallDuration(callDurationSec))
                        color: Ui.Style.textSecondary
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        Layout.alignment: Qt.AlignVCenter
                        elide: Text.ElideRight
                    }

                    Item { Layout.fillWidth: true }
                    Components.GhostButton {
                        text: Ui.I18n.t("chat.groupCallJoin")
                        Layout.preferredHeight: 32
                        visible: !Ui.CallDisplayStore.groupCallActive
                        onClicked: Ui.ChatDisplayStore.joinGroupCall(
                                       groupCallBanner.callInfo &&
                                       groupCallBanner.callInfo.video)
                    }
                    Components.PrimaryButton {
                        text: Ui.I18n.t("chat.groupCallLeave")
                        Layout.preferredHeight: 32
                        visible: Ui.CallDisplayStore.groupCallActive &&
                                 Ui.CallDisplayStore.activeGroupCallGroup === Ui.ChatDisplayStore.currentChatId
                        onClicked: Ui.ChatDisplayStore.leaveGroupCall()
                    }
                }
            }

            ListView {
                id: messageList
                width: root.centeredChatColumnWidth
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: root.contentCenterOffset
                // anchors.rightMargin: Ui.Style.paddingL + root.drawerReserveWidth
                anchors.bottomMargin: Ui.Style.paddingS + 2
                anchors.topMargin: Ui.Style.paddingM +
                                   (groupCallBanner.visible
                                    ? groupCallBanner.height + Ui.Style.paddingS
                                    : 0)
                clip: true
                visible: root.showingChatSurface
                model: root.showingChatSurface && Ui.ChatDisplayStore.currentChatId.length > 0
                       ? Ui.ChatDisplayStore.messagesModel(Ui.ChatDisplayStore.currentChatId)
                       : null
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 320
                delegate: messageDelegate
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }

                onContentYChanged: {
                    var atBottom = contentY + height >= contentHeight - 40
                    stickToBottom = atBottom
                }
                onCountChanged: {
                    if (stickToBottom) {
                        positionViewAtEnd()
                    }
                }
            }

            Item {
                anchors.fill: parent
                visible: root.showingChatSurface && Ui.ChatDisplayStore.currentChatId.length === 0

                Rectangle {
                    id: postLoginEmptyStateCard
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 64, 468)
                    radius: Ui.Style.radiusXL
                    color: Ui.Style.panelBgAlt
                    border.color: Ui.Style.borderSubtle
                    border.width: 1
                    implicitHeight: emptyStateColumn.implicitHeight + 48

                    ColumnLayout {
                        id: emptyStateColumn
                        anchors.fill: parent
                        anchors.margins: 24
                        spacing: 14

                        Components.EmptyStateIllustration {
                            Layout.alignment: Qt.AlignHCenter
                            kind: "chat"
                            size: 68
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            visible: false
                            spacing: 8

                            Rectangle {
                                radius: 10
                                color: Ui.Style.topBarPillBg
                                border.width: 1
                                border.color: Ui.Style.topBarPillBorder
                                implicitWidth: emptyStateSessionLabel.implicitWidth + 14
                                implicitHeight: 22

                                Text {
                                    id: emptyStateSessionLabel
                                    anchors.centerIn: parent
                                    text: Ui.SecurityDisplayStore.transportHealthy
                                          ? (Ui.I18n.usesCjkLocale ? "加密会话已就绪" : "Secure session ready")
                                          : Ui.I18n.t("dialog.securityCenter.transportNeedsAttention")
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }
                            }

                            Rectangle {
                                visible: Ui.SecurityDisplayStore.gatewayDisplayState.length > 0
                                radius: 10
                                color: Ui.Style.railAccentBg
                                border.width: 1
                                border.color: Ui.Style.railAccentBorder
                                implicitWidth: emptyStateTrustLabel.implicitWidth + 14
                                implicitHeight: 22

                                Text {
                                    id: emptyStateTrustLabel
                                    anchors.centerIn: parent
                                    text: Ui.SecurityDisplayStore.gatewayDisplayState
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: true
                            text: Ui.I18n.t("left.emptyTitle")
                            color: Ui.Style.textPrimary
                            wrapMode: Text.NoWrap
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                            maximumLineCount: 1
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: true
                            text: Ui.I18n.usesCjkLocale
                                  ? "从左侧选择会话开始消息"
                                  : "Choose a conversation from the left"
                            color: Ui.Style.textSecondary
                            wrapMode: Text.NoWrap
                            horizontalAlignment: Text.AlignHCenter
                            lineHeight: 1.2
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            maximumLineCount: 1
                        }

                        RowLayout {
                            id: postLoginEmptyStateActions
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 8

                            Components.IconButton {
                                id: chatEmptySecondaryAction
                                accessibleName: Ui.I18n.t("left.contacts")
                                icon.source: "qrc:/mi/e2ee/ui/icons/group.svg"
                                buttonSize: 42
                                iconSize: 18
                                bgColor: Ui.Style.topBarPillBg
                                hoverBg: Ui.Style.hoverBg
                                pressedBg: Ui.Style.pressedBg
                                onClicked: Ui.AppStore.setShellSurface("contacts")
                                ToolTip.visible: hovered
                                ToolTip.text: accessibleName
                            }

                            Components.IconButton {
                                id: chatEmptyPrimaryAction
                                accessibleName: Ui.I18n.t("left.newChat")
                                icon.source: "qrc:/mi/e2ee/ui/icons/plus.svg"
                                buttonSize: 42
                                iconSize: 18
                                bgColor: Ui.Style.accent
                                baseColor: "#F6FAFF"
                                hoverColor: "#F6FAFF"
                                pressColor: "#F6FAFF"
                                hoverBg: Ui.Style.accentHover
                                pressedBg: Ui.Style.accentPressed
                                onClicked: Ui.AppStore.openChatFromContact(Ui.ChatDisplayStore.contactsModel.count > 0
                                                                          ? Ui.ChatDisplayStore.contactsModel.get(0).contactId
                                                                          : "")
                                ToolTip.visible: hovered
                                ToolTip.text: accessibleName
                            }
                        }

                        RowLayout {
                            id: postLoginEmptyStateInsights
                            visible: false
                            Layout.fillWidth: true
                            spacing: Ui.Style.paddingS

                            Repeater {
                                model: [
                                    {
                                        icon: "qrc:/mi/e2ee/ui/icons/device.svg",
                                        detail: Ui.SecurityDisplayStore.maskedCurrentDeviceId.length > 0
                                                ? Ui.SecurityDisplayStore.maskedCurrentDeviceId
                                                : Ui.I18n.t("dialog.deviceManager.thisDevice")
                                    },
                                    {
                                        icon: "qrc:/mi/e2ee/ui/icons/info.svg",
                                        detail: Ui.SecurityDisplayStore.gatewayDisplayDetail.length > 0
                                                ? Ui.SecurityDisplayStore.gatewayDisplayDetail
                                                : Ui.I18n.t("dialog.securityCenter.serverHint")
                                    }
                                ]

                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    radius: Ui.Style.radiusLarge
                                    color: Ui.Style.panelBg
                                    border.width: 1
                                    border.color: Ui.Style.borderSubtle
                                    implicitHeight: insightRow.implicitHeight + Ui.Style.paddingM * 2

                                    RowLayout {
                                        id: insightRow
                                        anchors.fill: parent
                                        anchors.margins: Ui.Style.paddingM
                                        spacing: 8

                                        Rectangle {
                                            width: 26
                                            height: 26
                                            radius: 13
                                            color: Ui.Style.railAccentBg
                                            border.width: 1
                                            border.color: Ui.Style.railAccentBorder

                                            Image {
                                                anchors.centerIn: parent
                                                width: 12
                                                height: 12
                                                source: modelData.icon
                                                fillMode: Image.PreserveAspectFit
                                                smooth: true
                                                antialiasing: true
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.detail
                                            color: Ui.Style.textPrimary
                                            font.pixelSize: 12
                                            font.weight: Font.Medium
                                            wrapMode: Text.NoWrap
                                            maximumLineCount: 1
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Components.IconButton {
                id: jumpButton
                visible: root.showingChatSurface && !stickToBottom && messageList.count > 0
                accessibleName: Ui.I18n.t("chat.jumpBottom")
                icon.source: "qrc:/mi/e2ee/ui/icons/chevron-down.svg"
                buttonSize: 30
                iconSize: 14
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.rightMargin: Ui.Style.paddingM + root.drawerReserveWidth
                anchors.bottomMargin: Ui.Style.paddingM
                bgColor: Ui.Style.panelBg
                hoverBg: Ui.Style.hoverBg
                pressedBg: Ui.Style.pressedBg
                onClicked: messageList.positionViewAtEnd()
                ToolTip.visible: hovered
                ToolTip.text: Ui.I18n.t("chat.jumpBottom")
            }

            Item {
                id: contactsHub
                anchors.fill: parent
                visible: root.showingContactsSurface

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingL
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        width: Math.max(0, contactsHub.width - Ui.Style.paddingL * 2)
                        spacing: Ui.Style.paddingM

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                Layout.fillWidth: true
                                text: Ui.I18n.t("contacts.title")
                                color: Ui.Style.textPrimary
                                font.pixelSize: 22
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Components.UiText {
                                Layout.fillWidth: true
                                text: Ui.I18n.usesCjkLocale
                                      ? "联系人是一等入口，支持直接发起私聊、建群和查看身份。"
                                      : "Contacts are a first-class surface for starting chats, groups, and identity checks."
                                textRole: "supporting"
                                roleColor: Ui.Style.textSecondary
                                wrapMode: Text.WordWrap
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.panelBg
                            border.width: 1
                            border.color: Ui.Style.borderSubtle
                            implicitHeight: contactsHeroRow.implicitHeight + Ui.Style.paddingL * 2

                            RowLayout {
                                id: contactsHeroRow
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingL
                                spacing: Ui.Style.paddingM

                                Components.EmptyStateIllustration {
                                    kind: "chat"
                                    size: 52
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        Layout.fillWidth: true
                                        text: Ui.I18n.usesCjkLocale ? "快速发起聊天" : "Start a chat fast"
                                        color: Ui.Style.textPrimary
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: Ui.I18n.t("contacts.emptyHint")
                                        color: Ui.Style.textSecondary
                                        font.pixelSize: 11
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                Components.PrimaryButton {
                                    Layout.alignment: Qt.AlignVCenter
                                    text: Ui.I18n.t("contacts.startChat")
                                    onClicked: {
                                        if (Ui.ChatDisplayStore.contactsModel.count > 0) {
                                            Ui.ChatDisplayStore.openChatFromContact(Ui.ChatDisplayStore.contactsModel.get(0).contactId)
                                        }
                                    }
                                }
                            }
                        }

                        Repeater {
                            model: Ui.ChatDisplayStore.filteredContactsModel

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                radius: Ui.Style.radiusLarge
                                color: contactsCardMouse.containsMouse ? Ui.Style.dialogHoverBg : Ui.Style.panelBg
                                border.width: 1
                                border.color: Ui.Style.borderSubtle
                                implicitHeight: contactsCardRow.implicitHeight + Ui.Style.paddingM * 2

                                RowLayout {
                                    id: contactsCardRow
                                    anchors.fill: parent
                                    anchors.margins: Ui.Style.paddingM
                                    spacing: Ui.Style.paddingM

                                    Components.IdentityAvatar {
                                        size: 44
                                        titleText: displayName
                                        seedText: avatarKey || displayName
                                        mode: "person"
                                        presenceState: "online"
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3

                                        Text {
                                            Layout.fillWidth: true
                                            text: displayName
                                            color: Ui.Style.textPrimary
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: usernameOrPhone
                                            color: Ui.Style.textSecondary
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Components.GhostButton {
                                        Layout.alignment: Qt.AlignVCenter
                                        text: Ui.I18n.t("contacts.startChat")
                                        Layout.preferredHeight: 32
                                        onClicked: Ui.ChatDisplayStore.openChatFromContact(contactId)
                                    }
                                }

                                MouseArea {
                                    id: contactsCardMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: Ui.ChatDisplayStore.openChatFromContact(contactId)
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            visible: !Ui.ChatDisplayStore.filteredContactsModel ||
                                     Ui.ChatDisplayStore.filteredContactsModel.count === 0
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.panelBg
                            border.width: 1
                            border.color: Ui.Style.borderSubtle
                            implicitHeight: 108

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingL
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: Ui.I18n.t("contacts.emptyTitle")
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: Ui.I18n.t("contacts.emptyHint")
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            Item {
                id: utilitySurface
                anchors.fill: parent
                visible: root.showingCallsSurface || root.showingSettingsSurface || root.showingSecuritySurface

                ScrollView {
                    id: utilityScroll
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingL
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded

                    ColumnLayout {
                        readonly property int utilityCardWidth: Math.min(width, Ui.Style.utilitySurfaceMaxWidth)
                        width: Math.max(0, utilityScroll.availableWidth)
                        spacing: Ui.Style.paddingL

                        Item {
                            id: utilityPageHeader
                            Layout.preferredWidth: parent.utilityCardWidth
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredHeight: 92
                            readonly property bool hideBackAction: utilityScroll.availableWidth >= 700

                            Rectangle {
                                id: utilityBackButton
                                width: 28
                                height: 28
                                radius: 14
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                visible: !utilityPageHeader.hideBackAction
                                color: Ui.Style.topBarPillBg
                                border.width: 1
                                border.color: Ui.Style.topBarPillBorder

                                Image {
                                    anchors.centerIn: parent
                                    width: 12
                                    height: 12
                                    source: "qrc:/mi/e2ee/ui/icons/chevron-right.svg"
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    antialiasing: true
                                    rotation: 180
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (root.showingSecuritySurface) {
                                            Ui.AppStore.setShellSurface("settings")
                                        } else {
                                            Ui.AppStore.setShellSurface("chat")
                                        }
                                    }
                                }
                            }

                            Column {
                                anchors.left: utilityBackButton.visible ? utilityBackButton.right : parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: utilityBackButton.visible ? Ui.Style.paddingM : 0
                                anchors.rightMargin: 0
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 6

                                Text {
                                    id: utilityPageTitle
                                    width: parent.width
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                    text: root.showingCallsSurface
                                          ? Ui.I18n.t("chat.call")
                                          : (root.showingSecuritySurface
                                             ? Ui.I18n.t("dialog.securityCenter.title")
                                             : Ui.I18n.t("settings.title"))
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 28
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: utilityBackButton.visible ? Text.AlignLeft : Text.AlignHCenter
                                    renderType: Text.NativeRendering
                                    antialiasing: true
                                }

                                Text {
                                    width: parent.width
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                    text: root.showingCallsSurface
                                          ? (Ui.I18n.usesCjkLocale ? "最近记录与快速发起" : "Recent history and quick launch")
                                          : (root.showingSecuritySurface
                                             ? (Ui.I18n.usesCjkLocale ? "信任、设备与传输状态" : "Trust, devices, and transport status")
                                             : (Ui.I18n.usesCjkLocale ? "外观、语言与隐私偏好" : "Appearance, language, and privacy"))
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 13
                                    horizontalAlignment: utilityBackButton.visible ? Text.AlignLeft : Text.AlignHCenter
                                    renderType: Text.NativeRendering
                                    antialiasing: true
                                }
                            }
                        }

                        Rectangle {
                            id: settingsPrimaryList
                            Layout.preferredWidth: parent.utilityCardWidth
                            Layout.alignment: Qt.AlignHCenter
                            visible: root.showingSettingsSurface
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.panelBg
                            border.width: 1
                            border.color: Ui.Style.borderSubtle
                            implicitHeight: settingsPrimaryColumn.implicitHeight + Ui.Style.paddingM * 2

                            ColumnLayout {
                                id: settingsPrimaryColumn
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingS
                                spacing: 0

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: settingsAccountRow.implicitHeight

                                UtilityNavRow {
                                    id: settingsAccountRow
                                    anchors.fill: parent
                                    iconSource: "qrc:/mi/e2ee/ui/icons/device.svg"
                                    iconBg: Ui.Style.topBarPillBg
                                    iconBorder: Ui.Style.topBarPillBorder
                                    titleText: Ui.I18n.t("auth.brand")
                                    trailingText: Ui.I18n.usesCjkLocale ? "账号" : "Account"
                                }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: themeSettingsRow.implicitHeight

                                    UtilityNavRow {
                                    id: themeSettingsRow
                                    anchors.fill: parent
                                    iconSource: "qrc:/mi/e2ee/ui/icons/palette.svg"
                                    iconBg: Ui.Style.railAccentBg
                                    iconBorder: Ui.Style.railAccentBorder
                                    titleText: Ui.I18n.t("settings.theme")
                                        detailText: root.utilityFriendlyThemeDetail()
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.cycleThemeOption()
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: localeSettingsRow.implicitHeight

                                    UtilityNavRow {
                                    id: localeSettingsRow
                                    anchors.fill: parent
                                    iconSource: "qrc:/mi/e2ee/ui/icons/language.svg"
                                    iconBg: Ui.Style.topBarPillBg
                                    iconBorder: Ui.Style.topBarPillBorder
                                    titleText: Ui.I18n.t("settings.language")
                                        detailText: root.utilityFriendlyLocaleDetail()
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.cycleLocaleOption()
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: securitySettingsRow.implicitHeight

                                    UtilityNavRow {
                                    id: securitySettingsRow
                                    anchors.fill: parent
                                    iconSource: "qrc:/mi/e2ee/ui/icons/check.svg"
                                    iconBg: Ui.Style.railAccentBg
                                    iconBorder: Ui.Style.railAccentBorder
                                    titleText: Ui.I18n.t("settings.securityCenter.title")
                                        detailText: root.utilityFriendlySecurityDetail()
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Ui.AppStore.setShellSurface("security")
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: privacySettingsRow.implicitHeight

                                    UtilityNavRow {
                                        id: privacySettingsRow
                                        anchors.fill: parent
                                        iconSource: "qrc:/mi/e2ee/ui/icons/file.svg"
                                        iconBg: Ui.Style.topBarPillBg
                                        iconBorder: Ui.Style.topBarPillBorder
                                        titleText: Ui.I18n.t("settings.section.privacy")
                                        detailText: ""
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: notificationsSettingsRow.implicitHeight

                                    UtilityNavRow {
                                        id: notificationsSettingsRow
                                        anchors.fill: parent
                                        iconSource: "qrc:/mi/e2ee/ui/icons/bell.svg"
                                        iconBg: Ui.Style.railAccentBg
                                        iconBorder: Ui.Style.railAccentBorder
                                        titleText: Ui.I18n.t("settings.section.notifications")
                                        detailText: ""
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: devicesSettingsRow.implicitHeight

                                    UtilityNavRow {
                                        id: devicesSettingsRow
                                        anchors.fill: parent
                                        iconSource: "qrc:/mi/e2ee/ui/icons/group.svg"
                                        iconBg: Ui.Style.topBarPillBg
                                        iconBorder: Ui.Style.topBarPillBorder
                                        titleText: Ui.I18n.t("dialog.deviceManager.linkedDevices")
                                        trailingText: Ui.SecurityDisplayStore.devicesModel.count > 0
                                                      ? ("" + Ui.SecurityDisplayStore.devicesModel.count)
                                                      : "0"
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Ui.AppStore.setShellSurface("security")
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: densitySettingsRow.implicitHeight

                                    UtilityNavRow {
                                        id: densitySettingsRow
                                        anchors.fill: parent
                                        iconSource: "qrc:/mi/e2ee/ui/icons/chat.svg"
                                        iconBg: Ui.Style.railAccentBg
                                        iconBorder: Ui.Style.railAccentBorder
                                        titleText: Ui.I18n.t("settings.messageDensity")
                                        detailText: Ui.I18n.t("settings.density.normal")
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: fontSizeSettingsRow.implicitHeight

                                    UtilityNavRow {
                                        id: fontSizeSettingsRow
                                        anchors.fill: parent
                                        iconSource: "qrc:/mi/e2ee/ui/icons/language.svg"
                                        iconBg: Ui.Style.topBarPillBg
                                        iconBorder: Ui.Style.topBarPillBorder
                                        titleText: Ui.I18n.t("settings.fontSize")
                                        detailText: "16 px"
                                    }
                                }

                            }
                        }

                        Rectangle {
                            id: securitySummaryList
                            Layout.preferredWidth: parent.utilityCardWidth
                            Layout.alignment: Qt.AlignHCenter
                            visible: root.showingSecuritySurface
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.panelBg
                            border.width: 1
                            border.color: Ui.Style.borderSubtle
                            implicitHeight: securitySummaryColumn.implicitHeight + Ui.Style.paddingM * 2

                            ColumnLayout {
                                id: securitySummaryColumn
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingS
                                spacing: 0

                                UtilityNavRow {
                                    Layout.fillWidth: true
                                    iconSource: "qrc:/mi/e2ee/ui/icons/check.svg"
                                    iconBg: Ui.Style.railAccentBg
                                    iconBorder: Ui.Style.railAccentBorder
                                    titleText: Ui.I18n.t("dialog.securityCenter.transportTitle")
                                    trailingText: root.utilityFriendlySecurityDetail()
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                UtilityNavRow {
                                    Layout.fillWidth: true
                                    iconSource: "qrc:/mi/e2ee/ui/icons/group.svg"
                                    iconBg: Ui.Style.topBarPillBg
                                    iconBorder: Ui.Style.topBarPillBorder
                                    titleText: Ui.I18n.t("dialog.securityCenter.devicesTitle")
                                    trailingText: Ui.SecurityDisplayStore.devicesModel.count > 0
                                                  ? (Ui.I18n.usesCjkLocale
                                                     ? ("" + Ui.SecurityDisplayStore.devicesModel.count)
                                                     : ("" + Ui.SecurityDisplayStore.devicesModel.count))
                                                  : "0"
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                UtilityNavRow {
                                    Layout.fillWidth: true
                                    iconSource: "qrc:/mi/e2ee/ui/icons/info.svg"
                                    iconBg: Ui.Style.topBarPillBg
                                    iconBorder: Ui.Style.topBarPillBorder
                                    titleText: Ui.I18n.t("dialog.securityCenter.trustTitle")
                                    trailingText: root.utilityFriendlyTrustDetail()
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                UtilityNavRow {
                                    Layout.fillWidth: true
                                    iconSource: "qrc:/mi/e2ee/ui/icons/clock.svg"
                                    iconBg: Ui.Style.topBarPillBg
                                    iconBorder: Ui.Style.topBarPillBorder
                                    titleText: Ui.I18n.t("dialog.securityCenter.serverTitle")
                                    trailingText: root.utilityFriendlyGatewayDetail()
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                UtilityNavRow {
                                    Layout.fillWidth: true
                                    iconSource: "qrc:/mi/e2ee/ui/icons/device.svg"
                                    iconBg: Ui.Style.topBarPillBg
                                    iconBorder: Ui.Style.topBarPillBorder
                                    titleText: Ui.I18n.t("dialog.deviceManager.currentDevice")
                                    trailingText: Ui.SecurityDisplayStore.maskedCurrentDeviceId.length > 0
                                                  ? Ui.SecurityDisplayStore.maskedCurrentDeviceId
                                                  : (Ui.I18n.usesCjkLocale ? "本机" : "This device")
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                UtilityNavRow {
                                    Layout.fillWidth: true
                                    iconSource: "qrc:/mi/e2ee/ui/icons/info.svg"
                                    iconBg: Ui.Style.topBarPillBg
                                    iconBorder: Ui.Style.topBarPillBorder
                                    titleText: Ui.I18n.usesCjkLocale ? "版本" : "Version"
                                    trailingText: Ui.SecurityDisplayStore.versionText.length > 0
                                                  ? Ui.SecurityDisplayStore.versionText
                                                  : (Ui.I18n.usesCjkLocale ? "桌面版" : "Desktop")
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                UtilityNavRow {
                                    Layout.fillWidth: true
                                    iconSource: "qrc:/mi/e2ee/ui/icons/file.svg"
                                    iconBg: Ui.Style.topBarPillBg
                                    iconBorder: Ui.Style.topBarPillBorder
                                    titleText: Ui.I18n.usesCjkLocale ? "剪贴板隔离" : "Clipboard isolation"
                                    trailingText: Ui.SecurityDisplayStore.clipboardIsolationEnabled
                                                  ? (Ui.I18n.usesCjkLocale ? "已开启" : "On")
                                                  : (Ui.I18n.usesCjkLocale ? "关闭" : "Off")
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                UtilityNavRow {
                                    Layout.fillWidth: true
                                    iconSource: "qrc:/mi/e2ee/ui/icons/group.svg"
                                    iconBg: Ui.Style.railAccentBg
                                    iconBorder: Ui.Style.railAccentBorder
                                    titleText: Ui.I18n.t("dialog.securityCenter.manageDevices")
                                    trailingText: ""
                                }
                            }
                        }

                        Rectangle {
                            id: securityDevicesCard
                            Layout.preferredWidth: parent.utilityCardWidth
                            Layout.alignment: Qt.AlignHCenter
                            visible: root.showingSecuritySurface &&
                                     Ui.SecurityDisplayStore.devicesModel.count > 0
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.panelBg
                            border.width: 1
                            border.color: Ui.Style.borderSubtle
                            implicitHeight: devicesColumn.implicitHeight + Ui.Style.paddingL * 2

                            ColumnLayout {
                                id: devicesColumn
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingL
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    text: Ui.I18n.t("dialog.securityCenter.devicesTitle")
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }

                                Repeater {
                                    model: Ui.SecurityDisplayStore.devicesModel

                                    delegate: UtilityDeviceRow {
                                        Layout.fillWidth: true
                                        titleText: root.utilityFriendlyDeviceName(maskedDeviceDisplayId, index, false)
                                        detailText: lastSeenDisplay
                                    }
                                }

                                Text {
                                    visible: Ui.SecurityDisplayStore.devicesModel.count === 0
                                    Layout.fillWidth: true
                                    text: Ui.I18n.t("dialog.securityCenter.noLinkedDevices")
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                }

                            }
                        }

                        Rectangle {
                            id: callsCurrentCard
                            Layout.preferredWidth: parent.utilityCardWidth
                            Layout.alignment: Qt.AlignHCenter
                            visible: root.showingCallsSurface &&
                                     (Ui.CallDisplayStore.activeCallId.length > 0 ||
                                      Ui.CallDisplayStore.incomingCallActive)
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.panelBg
                            border.width: 1
                            border.color: Ui.Style.borderSubtle
                            implicitHeight: callsCurrentColumn.implicitHeight + Ui.Style.paddingL * 2

                            ColumnLayout {
                                id: callsCurrentColumn
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingM
                                spacing: Ui.Style.paddingM

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Ui.Style.paddingM

                                    Rectangle {
                                        width: 40
                                        height: 40
                                        radius: 20
                                        color: Ui.Style.railAccentBg
                                        border.width: 1
                                        border.color: Ui.Style.railAccentBorder

                                        Image {
                                            anchors.centerIn: parent
                                            width: 16
                                            height: 16
                                            fillMode: Image.PreserveAspectFit
                                            source: (Ui.CallDisplayStore.activeCallId.length > 0 || Ui.CallDisplayStore.incomingCallActive)
                                                    ? (Ui.CallDisplayStore.activeCallVideo || Ui.CallDisplayStore.incomingCallVideo
                                                       ? "qrc:/mi/e2ee/ui/icons/video.svg"
                                                       : "qrc:/mi/e2ee/ui/icons/phone.svg")
                                                    : "qrc:/mi/e2ee/ui/icons/phone.svg"
                                            smooth: true
                                            antialiasing: true
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            Layout.fillWidth: true
                                            maximumLineCount: 1
                                            elide: Text.ElideRight
                                            text: Ui.CallDisplayStore.activeCallId.length > 0
                                                  ? (Ui.CallDisplayStore.activeCallVideo
                                                     ? Ui.I18n.t("chat.callActiveVideo")
                                                     : Ui.I18n.t("chat.callActiveVoice"))
                                                  : (Ui.CallDisplayStore.incomingCallActive
                                                     ? (Ui.CallDisplayStore.incomingCallVideo
                                                        ? Ui.I18n.t("chat.callIncomingVideo")
                                                        : Ui.I18n.t("chat.callIncomingVoice"))
                                                     : Ui.I18n.t("calls.ready"))
                                            color: Ui.Style.textPrimary
                                            font.pixelSize: 14
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: (Ui.CallDisplayStore.activeCallId.length > 0 || Ui.CallDisplayStore.incomingCallActive)
                                                  ? Ui.ChatDisplayStore.resolveTitle(Ui.CallDisplayStore.activeCallPeer.length > 0
                                                                                    ? Ui.CallDisplayStore.activeCallPeer
                                                                                    : Ui.CallDisplayStore.incomingCallPeer)
                                                  : (Ui.ChatDisplayStore.currentChatTitle.length > 0
                                                     ? Ui.ChatDisplayStore.currentChatTitle
                                                     : Ui.I18n.t("calls.pickContact"))
                                            color: Ui.Style.textSecondary
                                            font.pixelSize: 12
                                            maximumLineCount: 1
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Text {
                                        visible: Ui.CallDisplayStore.activeCallId.length > 0
                                        text: Ui.I18n.t("chat.callDuration").arg(formatCallDuration(callDurationSec))
                                        color: Ui.Style.textMuted
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignRight
                                        elide: Text.ElideRight
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Components.GhostButton {
                                        visible: Ui.CallDisplayStore.incomingCallActive &&
                                                 Ui.CallDisplayStore.activeCallId.length === 0
                                        text: Ui.I18n.t("chat.callDecline")
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 36
                                        onClicked: Ui.CallDisplayStore.declineIncomingCall()
                                    }

                                    Components.PrimaryButton {
                                        visible: Ui.CallDisplayStore.incomingCallActive &&
                                                 Ui.CallDisplayStore.activeCallId.length === 0
                                        text: Ui.I18n.t("chat.callAccept")
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 36
                                        onClicked: Ui.CallDisplayStore.acceptIncomingCall()
                                    }

                                    Components.GhostButton {
                                        visible: Ui.CallDisplayStore.activeCallId.length > 0
                                        text: Ui.I18n.t("chat.callHangup")
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 36
                                        onClicked: Ui.CallDisplayStore.endCall()
                                    }

                                    Components.PrimaryButton {
                                        visible: !Ui.CallDisplayStore.incomingCallActive &&
                                                 Ui.CallDisplayStore.activeCallId.length === 0
                                        text: Ui.I18n.t("chat.call")
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 36
                                        enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                                        onClicked: Ui.ChatDisplayStore.handleCallAction(false)
                                    }

                                    Components.GhostButton {
                                        visible: !Ui.CallDisplayStore.incomingCallActive &&
                                                 Ui.CallDisplayStore.activeCallId.length === 0
                                        text: Ui.I18n.t("chat.video")
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 36
                                        enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                                        onClicked: Ui.ChatDisplayStore.handleCallAction(true)
                                    }
                                }
                            }
                        }

                        Rectangle {
                            id: callsRecentCard
                            Layout.preferredWidth: parent.utilityCardWidth
                            Layout.alignment: Qt.AlignHCenter
                            visible: root.showingCallsSurface
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.panelBg
                            border.width: 1
                            border.color: Ui.Style.borderSubtle
                            implicitHeight: callsRecentColumn.implicitHeight + Ui.Style.paddingL * 2

                            ColumnLayout {
                                id: callsRecentColumn
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingM
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: Ui.CallDisplayStore.activeCallId.length === 0 &&
                                             !Ui.CallDisplayStore.incomingCallActive &&
                                             root.utilityCallTargetId().length > 0
                                    spacing: 10

                                    Components.IdentityAvatar {
                                        size: 42
                                        titleText: root.utilityCallTargetTitle()
                                        seedText: root.utilityCallTargetAvatarSeed()
                                        mode: root.utilityCallTargetAvatarMode()
                                        presenceState: "online"
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            Layout.fillWidth: true
                                            text: root.utilityCallTargetTitle()
                                            color: Ui.Style.textPrimary
                                            font.pixelSize: 14
                                            font.weight: Font.DemiBold
                                            maximumLineCount: 1
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: Ui.I18n.usesCjkLocale ? "快速发起通话" : "Quick call"
                                            color: Ui.Style.textSecondary
                                            font.pixelSize: 12
                                            maximumLineCount: 1
                                            elide: Text.ElideRight
                                        }
                                    }

                                    Components.IconButton {
                                        accessibleName: Ui.I18n.t("chat.call")
                                        icon.source: "qrc:/mi/e2ee/ui/icons/phone.svg"
                                        buttonSize: 36
                                        iconSize: 16
                                        onClicked: {
                                            Ui.ChatDisplayStore.setCurrentChat(root.utilityCallTargetId())
                                            Ui.ChatDisplayStore.handleCallAction(false)
                                        }
                                    }

                                    Components.IconButton {
                                        accessibleName: Ui.I18n.t("chat.video")
                                        icon.source: "qrc:/mi/e2ee/ui/icons/video.svg"
                                        buttonSize: 36
                                        iconSize: 16
                                        onClicked: {
                                            Ui.ChatDisplayStore.setCurrentChat(root.utilityCallTargetId())
                                            Ui.ChatDisplayStore.handleCallAction(true)
                                        }
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    visible: Ui.CallDisplayStore.activeCallId.length === 0 &&
                                             !Ui.CallDisplayStore.incomingCallActive &&
                                             root.utilityCallTargetId().length > 0
                                    height: 1
                                    color: Ui.Style.borderSubtle
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: Ui.I18n.t("calls.recent")
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    maximumLineCount: 1
                                    elide: Text.ElideRight
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    Rectangle {
                                        Layout.preferredWidth: 52
                                        Layout.preferredHeight: 26
                                        radius: 13
                                        color: Ui.Style.railAccentBg
                                        border.width: 1
                                        border.color: Ui.Style.railAccentBorder

                                        Text {
                                            anchors.centerIn: parent
                                            text: Ui.I18n.usesCjkLocale ? "全部" : "All"
                                            color: Ui.Style.textPrimary
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                        }
                                    }

                                    Rectangle {
                                        Layout.preferredWidth: 60
                                        Layout.preferredHeight: 26
                                        radius: 13
                                        color: Ui.Style.topBarPillBg
                                        border.width: 1
                                        border.color: Ui.Style.topBarPillBorder

                                        Text {
                                            anchors.centerIn: parent
                                            text: Ui.I18n.usesCjkLocale ? "未接" : "Missed"
                                            color: Ui.Style.textSecondary
                                            font.pixelSize: 11
                                            font.weight: Font.Medium
                                        }
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }
                                }

                                Repeater {
                                    model: root.showingCallsSurface
                                           ? Math.min(10, Ui.ChatDisplayStore.filteredDialogsModel.count)
                                           : 0

                                    delegate: Item {
                                        property var dialogEntry: Ui.ChatDisplayStore.filteredDialogsModel.get(index)
                                        Layout.fillWidth: true
                                        implicitHeight: recentCallRow.implicitHeight

                                        UtilityCallRow {
                                            id: recentCallRow
                                            anchors.fill: parent
                                            avatarTitle: dialogEntry.title || ""
                                            avatarSeed: dialogEntry.avatarKey || dialogEntry.title || ""
                                            avatarMode: dialogEntry.avatarMode || ""
                                            titleText: dialogEntry.title || ""
                                            subtitleText: (Ui.CallDisplayStore.activeCallPeer === (dialogEntry.chatId || "") ||
                                                           Ui.CallDisplayStore.incomingCallPeer === (dialogEntry.chatId || ""))
                                                          ? (Ui.CallDisplayStore.activeCallVideo || Ui.CallDisplayStore.incomingCallVideo
                                                             ? Ui.I18n.t("chat.callActiveVideo")
                                                             : Ui.I18n.t("chat.callActiveVoice"))
                                                          : Ui.I18n.t("chat.call")
                                            timeText: dialogEntry.timeText || ""
                                            onClicked: Ui.ChatDisplayStore.setCurrentChat(dialogEntry.chatId || "")
                                            onActionClicked: {
                                                Ui.ChatDisplayStore.setCurrentChat(dialogEntry.chatId || "")
                                                Ui.ChatDisplayStore.handleCallAction(false)
                                            }
                                        }

                                    }
                                }

                                Rectangle {
                                    visible: Ui.ChatDisplayStore.filteredDialogsModel.count === 0
                                    Layout.fillWidth: true
                                    radius: Ui.Style.radiusMedium
                                    color: Ui.Style.panelBgAlt
                                    border.width: 1
                                    border.color: Ui.Style.borderSubtle
                                    implicitHeight: noRecentCallsLabel.implicitHeight + Ui.Style.paddingM * 2

                                    Text {
                                        id: noRecentCallsLabel
                                        anchors.centerIn: parent
                                        text: Ui.I18n.t("calls.noRecent")
                                        color: Ui.Style.textSecondary
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                    }
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Ui.Style.paddingL
                        }
                    }
                }
            }

            Item {
                id: callOverlay
                anchors.fill: parent
                z: 5
                visible: root.showingChatSurface &&
                         Ui.CallDisplayStore.incomingCallActive &&
                         Ui.CallDisplayStore.activeCallId.length === 0
                property bool callVideo: Ui.CallDisplayStore.incomingCallVideo
                property string callPeer: Ui.CallDisplayStore.incomingCallPeer

                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(0, 0, 0, 0.55)
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                }

                Rectangle {
                    id: incomingPanel
                    visible: root.showingChatSurface &&
                             Ui.CallDisplayStore.incomingCallActive &&
                             Ui.CallDisplayStore.activeCallId.length === 0
                    width: 320
                    height: 210
                    radius: 14
                    color: Ui.Style.panelBgRaised
                    border.color: Ui.Style.borderSubtle
                    anchors.centerIn: parent

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: Ui.Style.paddingS
                        Text {
                            text: Ui.CallDisplayStore.incomingCallVideo
                                  ? Ui.I18n.t("chat.callIncomingVideo")
                                  : Ui.I18n.t("chat.callIncomingVoice")
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: Ui.Style.textPrimary
                        }
                        Text {
                            text: Ui.ChatDisplayStore.resolveTitle(Ui.CallDisplayStore.incomingCallPeer)
                            font.pixelSize: 12
                            color: Ui.Style.textSecondary
                            elide: Text.ElideRight
                        }
                        Item { Layout.fillHeight: true }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Ui.Style.paddingS
                            Components.GhostButton {
                                text: Ui.I18n.t("chat.callDecline")
                                Layout.fillWidth: true
                                onClicked: Ui.CallDisplayStore.declineIncomingCall()
                            }
                            Components.PrimaryButton {
                                text: Ui.I18n.t("chat.callAccept")
                                Layout.fillWidth: true
                                onClicked: Ui.CallDisplayStore.acceptIncomingCall()
                            }
                        }
                    }
                }

            }
        }

        function syncCallState() {
            if (clientBridge &&
                (Ui.CallDisplayStore.activeCallId.length > 0 || Ui.CallDisplayStore.groupCallActive)) {
                callStartMs = Date.now()
                callDurationSec = 0
                resetCallControls()
            } else {
                callStartMs = 0
                callDurationSec = 0
            }
        }

        Connections {
            target: root.bridge
            function onCallStateChanged() { syncCallState() }
            function onGroupCallStateChanged() { syncCallState() }
        }

        Timer {
            id: callDurationTimer
            interval: 1000
            repeat: true
            running: !!(root.bridge &&
                        (Ui.CallDisplayStore.activeCallId.length > 0 || Ui.CallDisplayStore.groupCallActive))
            onTriggered: {
                if (!callStartMs || callStartMs <= 0) {
                    callStartMs = Date.now()
                }
                callDurationSec = Math.max(0,
                                           Math.floor((Date.now() - callStartMs) / 1000))
            }
        }

        Window {
            id: voiceCallWindow
            visible: Ui.CallDisplayStore.activeCallId.length > 0 &&
                     !Ui.CallDisplayStore.activeCallVideo
            flags: Qt.Window | Qt.FramelessWindowHint
            transientParent: root.hostWindow
            color: "transparent"
            width: 520
            height: 360
            minimumWidth: 520
            maximumWidth: 520
            minimumHeight: 360
            maximumHeight: 360

            function centerWindow() {
                if (root.hostWindow) {
                    x = Math.round(root.hostWindow.x + (root.hostWindow.width - width) / 2)
                    y = Math.round(root.hostWindow.y + (root.hostWindow.height - height) / 2)
                    return
                }
                x = Screen.virtualX + (Screen.width - width) / 2
                y = Screen.virtualY + (Screen.height - height) / 2
            }

            onVisibleChanged: {
                if (visible) {
                    centerWindow()
                }
            }
            onWidthChanged: if (visible) { centerWindow() }
            onHeightChanged: if (visible) { centerWindow() }
            onClosing: {
                if (clientBridge && clientBridge.activeCallId.length > 0) {
                    clientBridge.endCall()
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: 16
                color: Ui.Style.panelBgRaised
                border.color: Ui.Style.borderSubtle
            }

            DragHandler {
                target: null
                acceptedButtons: Qt.LeftButton
                onActiveChanged: {
                    if (active && voiceCallWindow.startSystemMove) {
                        voiceCallWindow.startSystemMove()
                    }
                }
            }

            Column {
                anchors.centerIn: parent
                spacing: 8
                Text {
                    text: Ui.I18n.t("chat.callActiveVoice")
                    color: Ui.Style.textPrimary
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                Text {
                    text: Ui.ChatDisplayStore.resolveTitle(Ui.CallDisplayStore.activeCallPeer)
                    color: Ui.Style.textSecondary
                    font.pixelSize: 12
                }
                Text {
                    text: Ui.I18n.t("chat.callDuration")
                          .arg(formatCallDuration(callDurationSec))
                    color: Ui.Style.textMuted
                    font.pixelSize: 11
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 18
                Components.RoundIconButton {
                    accessibleName: Ui.I18n.t("chat.endCall")
                    icon.source: "qrc:/mi/e2ee/ui/icons/phone.svg"
                    buttonSize: 50
                    iconSize: 22
                    baseColor: Ui.Style.danger
                    hoverColor: Ui.Style.danger
                    pressColor: Ui.Style.danger
                    bgColor: "transparent"
                    hoverBg: Qt.rgba(1, 1, 1, 0.08)
                    pressedBg: Qt.rgba(1, 1, 1, 0.16)
                    onClicked: {
                        if (clientBridge) {
                            clientBridge.endCall()
                        }
                    }
                }
            }
        }

        Window {
            id: videoCallWindow
            visible: Ui.CallDisplayStore.activeCallId.length > 0 &&
                     Ui.CallDisplayStore.activeCallVideo
            flags: Qt.Window | Qt.FramelessWindowHint
            transientParent: root.hostWindow
            color: "transparent"
            width: 760
            height: 520
            minimumWidth: 760
            maximumWidth: 760
            minimumHeight: 520
            maximumHeight: 520

            function centerWindow() {
                if (root.hostWindow) {
                    x = Math.round(root.hostWindow.x + (root.hostWindow.width - width) / 2)
                    y = Math.round(root.hostWindow.y + (root.hostWindow.height - height) / 2)
                    return
                }
                x = Screen.virtualX + (Screen.width - width) / 2
                y = Screen.virtualY + (Screen.height - height) / 2
            }

            function bindVideoSinks() {
                if (!clientBridge) {
                    return
                }
                if (clientBridge.bindRemoteVideoSink) {
                    clientBridge.bindRemoteVideoSink(remoteVideo.videoSink)
                }
                if (clientBridge.bindLocalVideoSink) {
                    clientBridge.bindLocalVideoSink(localVideo.videoSink)
                }
            }

            onVisibleChanged: {
                if (visible) {
                    centerWindow()
                    bindVideoSinks()
                }
            }
            Component.onCompleted: bindVideoSinks()
            onWidthChanged: if (visible) { centerWindow() }
            onHeightChanged: if (visible) { centerWindow() }
            onClosing: {
                if (clientBridge && clientBridge.activeCallId.length > 0) {
                    clientBridge.endCall()
                }
            }

            Rectangle {
                anchors.fill: parent
                radius: 16
                color: Ui.Style.panelBgRaised
                border.color: Ui.Style.borderSubtle
            }

            DragHandler {
                target: null
                acceptedButtons: Qt.LeftButton
                onActiveChanged: {
                    if (active && videoCallWindow.startSystemMove) {
                        videoCallWindow.startSystemMove()
                    }
                }
            }

            Item {
                id: videoStage
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingM

                Rectangle {
                    anchors.fill: parent
                    radius: 12
                    color: Ui.Style.panelBgAlt
                    border.color: Ui.Style.borderSubtle
                }

                VideoOutput {
                    id: remoteVideo
                    anchors.fill: parent
                    fillMode: VideoOutput.PreserveAspectFit
                }

                VideoOutput {
                    id: localVideo
                    width: 180
                    height: 110
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: Ui.Style.paddingS
                    fillMode: VideoOutput.PreserveAspectFit
                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: "transparent"
                        border.color: Ui.Style.borderSubtle
                    }
                }

                Column {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: Ui.Style.paddingS
                    spacing: 4
                    Text {
                        text: Ui.I18n.t("chat.callActiveVideo")
                        color: Ui.Style.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: Ui.ChatDisplayStore.resolveTitle(Ui.CallDisplayStore.activeCallPeer)
                        color: Ui.Style.textSecondary
                        font.pixelSize: 11
                    }
                    Text {
                        text: Ui.I18n.t("chat.callDuration")
                              .arg(formatCallDuration(callDurationSec))
                        color: Ui.Style.textMuted
                        font.pixelSize: 10
                    }
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 18
                    spacing: 18
                    Components.RoundIconButton {
                        accessibleName: Ui.I18n.t("chat.toggleMic")
                        icon.source: "qrc:/mi/e2ee/ui/icons/mic.svg"
                        buttonSize: 46
                        iconSize: 20
                        baseColor: callMicEnabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                        hoverColor: Ui.Style.textPrimary
                        pressColor: Ui.Style.textPrimary
                        bgColor: "transparent"
                        onClicked: {
                            callMicEnabled = !callMicEnabled
                            if (clientBridge && clientBridge.setCallMicEnabled) {
                                clientBridge.setCallMicEnabled(callMicEnabled)
                            }
                        }
                    }
                    Components.RoundIconButton {
                        accessibleName: Ui.I18n.t("chat.endCall")
                        icon.source: "qrc:/mi/e2ee/ui/icons/phone.svg"
                        buttonSize: 56
                        iconSize: 22
                        baseColor: Ui.Style.danger
                        hoverColor: Ui.Style.danger
                        pressColor: Ui.Style.danger
                        bgColor: "transparent"
                        hoverBg: Qt.rgba(1, 1, 1, 0.08)
                        pressedBg: Qt.rgba(1, 1, 1, 0.16)
                        onClicked: {
                            if (clientBridge) {
                                clientBridge.endCall()
                            }
                        }
                    }
                    Components.RoundIconButton {
                        accessibleName: Ui.I18n.t("chat.toggleCamera")
                        icon.source: "qrc:/mi/e2ee/ui/icons/video.svg"
                        buttonSize: 46
                        iconSize: 20
                        baseColor: callCameraEnabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                        hoverColor: Ui.Style.textPrimary
                        pressColor: Ui.Style.textPrimary
                        bgColor: "transparent"
                        onClicked: {
                            callCameraEnabled = !callCameraEnabled
                            if (clientBridge && clientBridge.setCallCameraEnabled) {
                                clientBridge.setCallCameraEnabled(callCameraEnabled)
                            }
                        }
                    }
                }
            }
        }

        GroupCallWindow {
            id: groupCallWindow
            visible: !!(root.bridge && root.bridge.groupCallActive)
            ownerWindow: root.hostWindow
            clientBridge: root.bridge
            participants: root.bridge ? root.bridge.groupCallParticipants : []
            videoEnabled: !!(root.bridge && root.bridge.activeGroupCallVideo)
            durationSec: callDurationSec
            micEnabled: callMicEnabled
            cameraEnabled: callCameraEnabled
            onLeaveRequested: Ui.ChatDisplayStore.leaveGroupCall()
            onMicToggled: {
                callMicEnabled = enabled
                if (clientBridge && clientBridge.setCallMicEnabled) {
                    clientBridge.setCallMicEnabled(callMicEnabled)
                }
            }
            onCameraToggled: {
                callCameraEnabled = enabled
                if (clientBridge && clientBridge.setCallCameraEnabled) {
                    clientBridge.setCallCameraEnabled(callCameraEnabled)
                }
            }
        }

        Rectangle {
            id: inputBar
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.centeredChatColumnWidth
            Layout.maximumWidth: root.centeredChatColumnWidth
            Layout.bottomMargin: Ui.Style.paddingM
            Layout.preferredHeight: showingChatSurface && hasChat ? implicitHeight : 0
            Layout.minimumHeight: showingChatSurface && hasChat ? implicitHeight : 0
            Layout.maximumHeight: showingChatSurface && hasChat ? implicitHeight : 0
            visible: showingChatSurface && hasChat
            color: Ui.Style.tgGlassSurface
            radius: Ui.Style.radiusContinuous
            border.width: 1
            border.color: Ui.Style.tgCardBorder
            implicitHeight: 64
            property int inputFieldHeight: 40

            ColumnLayout {
                id: inputColumn
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                Text {
                    visible: Ui.ChatDisplayStore.sendErrorMessage.length > 0
                    text: Ui.ChatDisplayStore.sendErrorMessage
                    color: Ui.Style.danger
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    maximumLineCount: 1
                }

                RowLayout {
                    Layout.fillWidth: true
                    // Layout.rightMargin: root.drawerReserveWidth
                    Layout.bottomMargin: 0
                    spacing: Ui.Style.paddingS

                    Components.IconButton {
                        id: attachButton
                        icon.source: "qrc:/mi/e2ee/ui/icons/paperclip.svg"
                        buttonSize: inputButtonSize
                        iconSize: inputIconSize
                        bgColor: Ui.Style.sidebarMetaChipBg
                        hoverBg: Ui.Style.hoverBg
                        pressedBg: Ui.Style.pressedBg
                        onClicked: root.showAttachPopup()
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.attach")
                    }

                    Rectangle {
                        id: inputField
                        Layout.fillWidth: true
                        Layout.preferredHeight: inputBar.inputFieldHeight
                        implicitHeight: inputBar.inputFieldHeight
                        radius: Ui.Style.radiusXL
                        color: Ui.Style.inputBg
                        border.color: messageInput.activeFocus ? Ui.Style.inputFocus : Ui.Style.inputBorder
                        border.width: 1

                        Flickable {
                            id: inputFlick
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingS
                            clip: true
                            interactive: true
                            flickableDirection: Flickable.VerticalFlick
                            boundsBehavior: Flickable.StopAtBounds
                            contentWidth: messageInput.width
                            contentHeight: messageInput.height

                            TextArea {
                                id: messageInput
                                width: inputFlick.width
                                height: Math.max(inputFlick.height, implicitHeight)
                                wrapMode: TextEdit.Wrap
                                placeholderText: Ui.I18n.t("chat.writeMessage")
                                placeholderTextColor: Ui.Style.textSecondary
                                color: Ui.Style.textPrimary
                                selectByMouse: true
                                cursorVisible: true
                                cursorDelegate: Rectangle {
                                    width: 2
                                    radius: 1
                                    color: Ui.Style.accent
                                }
                                background: Rectangle { color: "transparent" }
                                enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                                Keys.onPressed: function(event) {
                                    var allowInternalIme = internalImeReady && !externalImeActive()
                                    if (allowInternalIme) {
                                        if (event.key === Qt.Key_Shift && !event.isAutoRepeat) {
                                            imeShiftPressed = true
                                            imeShiftUsed = false
                                            event.accepted = true
                                            return
                                        }
                                        if (imeShiftPressed && event.key !== Qt.Key_Shift) {
                                            imeShiftUsed = true
                                        }
                                        if (handleImeKey(event)) {
                                            return
                                        }
                                    }
                                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                        if (event.modifiers & Qt.ShiftModifier) {
                                            return
                                        }
                                        event.accepted = true
                                        inputBar.sendMessage()
                                    }
                                }
                                Keys.onReleased: function(event) {
                                    if (!internalImeReady || externalImeActive()) {
                                        return
                                    }
                                    if (event.key === Qt.Key_Shift && !event.isAutoRepeat) {
                                        var toggle = imeShiftPressed && !imeShiftUsed
                                        imeShiftPressed = false
                                        imeShiftUsed = false
                                        if (toggle) {
                                            imeChineseMode = !imeChineseMode
                                            if (!imeChineseMode) {
                                                cancelImeComposition(true)
                                            }
                                            event.accepted = true
                                        }
                                    }
                                }
                                onTextChanged: inputBar.updateInputHeight()
                                onContentHeightChanged: inputBar.updateInputHeight()
                                Component.onCompleted: inputBar.updateInputHeight()
                                onCursorPositionChanged: inputBar.ensureCursorVisible()
                                onCursorRectangleChanged: inputBar.ensureCursorVisible()
                                onActiveFocusChanged: {
                                    if (!activeFocus) {
                                        cancelImeComposition(true)
                                        return
                                    }
                                    if (internalImeReady && Qt.inputMethod && Qt.inputMethod.reset) {
                                        Qt.inputMethod.reset()
                                        if (Qt.inputMethod.hide) {
                                            Qt.inputMethod.hide()
                                        }
                                    }
                                }
                                inputMethodHints: internalImeReady
                                                    ? (Qt.ImhNoPredictiveText | Qt.ImhPreferLatin)
                                                    : Qt.ImhNone
                            }
                        }

                        MouseArea {
                            id: inputContextArea
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            hoverEnabled: true
                            onPressed: {
                                if (!hasChat) {
                                    return
                                }
                                messageInput.forceActiveFocus()
                                inputContextMenu.popup()
                            }
                        }
                    }

                    Components.IconButton {
                        id: emojiButton
                        icon.source: "qrc:/mi/e2ee/ui/icons/emoji.svg"
                        buttonSize: inputButtonSize
                        iconSize: inputIconSize
                        bgColor: Ui.Style.sidebarMetaChipBg
                        hoverBg: Ui.Style.hoverBg
                        pressedBg: Ui.Style.pressedBg
                        onClicked: root.showEmojiPopup()
                        onRightClicked: root.showStickerImport()
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.emoji")
                    }

                    Components.IconButton {
                        id: micButton
                        icon.source: "qrc:/mi/e2ee/ui/icons/mic.svg"
                        accessibleName: Ui.I18n.usesCjkLocale ? "语音" : "Voice"
                        buttonSize: inputButtonSize
                        iconSize: inputIconSize
                        bgColor: Ui.Style.sidebarMetaChipBg
                        hoverBg: Ui.Style.hoverBg
                        pressedBg: Ui.Style.pressedBg
                        enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                        ToolTip.visible: hovered
                        ToolTip.text: accessibleName
                    }

                    Components.IconButton {
                        id: sendButton
                        property bool hasDraft: messageInput.text.trim().length > 0
                        Layout.preferredWidth: inputButtonSize
                        Layout.preferredHeight: inputButtonSize
                        Layout.alignment: Qt.AlignVCenter
                        accessibleName: Ui.I18n.t("chat.send")
                        icon.source: "qrc:/mi/e2ee/ui/icons/send.svg"
                        iconSize: inputIconSize
                        buttonSize: inputButtonSize
                        enabled: Ui.ChatDisplayStore.currentChatId.length > 0
                        baseColor: hasDraft ? "#F6FAFF" : Ui.Style.textMuted
                        hoverColor: "#F6FAFF"
                        pressColor: "#F6FAFF"
                        bgColor: hasDraft ? Ui.Style.accent : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.34)
                        hoverBg: hasDraft ? Ui.Style.accentHover : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.42)
                        pressedBg: hasDraft ? Ui.Style.accentPressed : Qt.rgba(75 / 255, 137 / 255, 255 / 255, 0.42)
                        onClicked: {
                            if (!hasDraft) {
                                messageInput.forceActiveFocus()
                                return
                            }
                            inputBar.sendMessage()
                        }
                        ToolTip.visible: hovered
                        ToolTip.text: accessibleName
                    }
                }
            }

            Rectangle {
                id: imePopup
                visible: imePopupVisible
                radius: 8
                color: Ui.Style.panelBgRaised
                border.color: Ui.Style.borderSubtle
                x: inputField.x
                y: inputField.y - height - 6
                width: Math.min(inputField.width, 420)
                implicitHeight: imePopupColumn.implicitHeight + Ui.Style.paddingS * 2
                z: 4

                Column {
                    id: imePopupColumn
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingS
                    spacing: 6
                    Text {
                        text: imePreedit
                        visible: imePreedit.length > 0
                        color: Ui.Style.textPrimary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Flow {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: imeCandidates
                            delegate: Rectangle {
                                radius: 6
                                color: index === imeCandidateIndex ? Ui.Style.hoverBg : "transparent"
                                border.color: index === imeCandidateIndex ? Ui.Style.accent : "transparent"
                                border.width: 1
                                height: 26
                                width: candidateRow.implicitWidth + 12
                                Row {
                                    id: candidateRow
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text {
                                        text: (index + 1) + "."
                                        color: Ui.Style.textMuted
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: modelData
                                        color: Ui.Style.textPrimary
                                        font.pixelSize: 12
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: commitImeCandidate(index)
                                }
                            }
                        }
                    }
                }
            }

            Menu {
                id: inputContextMenu
                MenuItem {
                    text: Ui.I18n.t("input.context.cut")
                    enabled: messageInput.selectedText.length > 0
                    onTriggered: contextCopy(true)
                }
                MenuItem {
                    text: Ui.I18n.t("input.context.copy")
                    enabled: messageInput.selectedText.length > 0
                    onTriggered: contextCopy(false)
                }
                MenuItem {
                    text: Ui.I18n.t("input.context.paste")
                    enabled: contextCanPaste()
                    onTriggered: contextPaste()
                }
                MenuItem {
                    text: Ui.I18n.t("input.context.selectAll")
                    enabled: messageInput.length > 0
                    onTriggered: contextSelectAll()
                }
            }

            Connections {
                target: Ui.ChatDisplayStore
                function onSendErrorMessageChanged() {
                    if (Ui.ChatDisplayStore.sendErrorMessage.length > 0) {
                        sendErrorTimer.restart()
                    }
                }
            }

            Timer {
                id: sendErrorTimer
                interval: 3000
                repeat: false
                onTriggered: Ui.ChatDisplayStore.clearSendError()
            }

            function updateInputHeight() {
                inputFieldHeight = 40
                Qt.callLater(ensureCursorVisible)
            }

            function ensureCursorVisible() {
                var rect = messageInput.positionToRectangle(messageInput.cursorPosition)
                if (!rect || inputFlick.height <= 0) {
                    return
                }
                var padding = 4
                var viewTop = inputFlick.contentY
                var viewBottom = inputFlick.contentY + inputFlick.height
                var rectTop = rect.y - padding
                var rectBottom = rect.y + rect.height + padding
                var maxY = Math.max(0, inputFlick.contentHeight - inputFlick.height)
                if (rectBottom > viewBottom) {
                    inputFlick.contentY = Math.min(maxY, rectBottom - inputFlick.height)
                } else if (rectTop < viewTop) {
                    inputFlick.contentY = Math.max(0, rectTop)
                }
            }

            function sendMessage() {
                if (internalImeReady && imeComposing) {
                    commitImeCandidate(imeCandidateIndex)
                }
                if (messageInput.text.trim().length === 0) {
                    return
                }
                var ok = Ui.ChatDisplayStore.sendMessage(messageInput.text)
                if (ok) {
                    messageInput.text = ""
                }
            }
        }
    }

    FileDialog {
        id: filePicker
        title: Ui.I18n.t("attach.document")
        fileMode: FileDialog.OpenFile
        onAccepted: {
            Ui.ChatDisplayStore.sendFile(resolveDialogUrl(filePicker))
        }
    }
    FileDialog {
        id: mediaPicker
        title: Ui.I18n.t("attach.photoVideo")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "媒体文件 (*.png *.jpg *.jpeg *.gif *.webp *.bmp *.mp4 *.mov *.mkv *.webm *.avi)"
        ]
        onAccepted: {
            Ui.ChatDisplayStore.sendFile(resolveDialogUrl(mediaPicker))
        }
    }
    FileDialog {
        id: stickerPicker
        title: Ui.I18n.t("chat.importSticker")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "媒体文件 (*.gif *.png *.jpg *.jpeg *.webp *.bmp *.mp4 *.mov *.mkv *.webm *.avi)"
        ]
        onAccepted: {
            if (clientBridge && clientBridge.importSticker) {
                var result = clientBridge.importSticker(resolveDialogUrl(stickerPicker))
                if (result && result.ok) {
                    loadStickers()
                    emojiTabIndex = 1
                    showEmojiPopup()
                }
            }
        }
    }
    Popup {
        id: downloadConfirm
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape
        width: 320

        background: Rectangle {
            radius: 12
            color: Ui.Style.panelBgRaised
            border.color: Ui.Style.borderSubtle
        }

        contentItem: ColumnLayout {
            anchors.margins: Ui.Style.paddingM
            spacing: Ui.Style.paddingS
            Text {
                text: Ui.I18n.t("chat.fileDownloadTitle")
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: Ui.Style.textPrimary
            }
            Text {
                text: Ui.I18n.format("chat.fileDownloadPrompt", pendingDownloadName)
                font.pixelSize: 12
                color: Ui.Style.textSecondary
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Ui.Style.paddingS
                Components.GhostButton {
                    text: Ui.I18n.t("chat.fileDownloadCancel")
                    Layout.fillWidth: true
                    onClicked: downloadConfirm.close()
                }
                    Components.PrimaryButton {
                        text: Ui.I18n.t("chat.fileDownloadConfirm")
                        Layout.fillWidth: true
                        onClicked: {
                            downloadConfirm.close()
                            openDownloadSaveDialog()
                        }
                    }
            }
        }
    }
    FileDialog {
        id: downloadSaveDialog
        title: Ui.I18n.t("chat.fileDownloadPick")
        fileMode: FileDialog.SaveFile
        onAccepted: {
            if (!clientBridge || !clientBridge.requestAttachmentDownload) {
                return
            }
            var path = resolveDialogUrl(downloadSaveDialog)
            if (!path || path.length === 0) {
                return
            }
            clientBridge.requestAttachmentDownload(
                        pendingDownloadId,
                        pendingDownloadKey,
                        pendingDownloadName,
                        pendingDownloadSize,
                        path)
        }
    }
    Window {
        id: imageViewer
        visible: false
        flags: Qt.Window | Qt.FramelessWindowHint
        modality: Qt.ApplicationModal
        color: "transparent"
        width: Screen.width
        height: Screen.height
        x: Screen.virtualX
        y: Screen.virtualY

        property string sourceUrl: ""
        property string imageName: ""
        property real zoom: 1.0
        property real minZoom: 0.2
        property real maxZoom: 6.0
        property real imageBaseWidth: 0
        property real imageBaseHeight: 0
        property real panX: 0
        property real panY: 0
        property int edgeMargin: 48

        function clampZoom(value) {
            return Math.max(minZoom, Math.min(maxZoom, value))
        }

        function clampPan() {
            var maxX = Math.max(0, (imageBaseWidth * zoom - viewerLayer.width) / 2)
            var maxY = Math.max(0, (imageBaseHeight * zoom - viewerLayer.height) / 2)
            panX = Math.max(-maxX, Math.min(maxX, panX))
            panY = Math.max(-maxY, Math.min(maxY, panY))
        }

        function updateBaseSize() {
            if (imageItem.implicitWidth <= 0 || imageItem.implicitHeight <= 0) {
                return
            }
            var availableW = Math.max(1, width - edgeMargin * 2)
            var availableH = Math.max(1, height - edgeMargin * 2)
            var scale = Math.min(availableW / imageItem.implicitWidth,
                                 availableH / imageItem.implicitHeight)
            imageBaseWidth = imageItem.implicitWidth * scale
            imageBaseHeight = imageItem.implicitHeight * scale
            clampPan()
        }

        function openWith(url, name) {
            sourceUrl = url
            imageName = name || ""
            zoom = 1.0
            panX = 0
            panY = 0
            updateBaseSize()
            visible = true
            raise()
            requestActivate()
        }

        function closeViewer() {
            close()
        }

        onClosing: {
            sourceUrl = ""
            imageName = ""
            zoom = 1.0
            panX = 0
            panY = 0
        }
        onWidthChanged: updateBaseSize()
        onHeightChanged: updateBaseSize()
        onZoomChanged: clampPan()

        Shortcut {
            sequence: "Esc"
            context: Qt.WindowShortcut
            onActivated: imageViewer.closeViewer()
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.76)
        }

        Item {
            id: viewerLayer
            anchors.fill: parent

            WheelHandler {
                target: viewerLayer
                onWheel: {
                    var factor = wheel.angleDelta.y < 0 ? 1.1 : (1.0 / 1.1)
                    imageViewer.zoom = imageViewer.clampZoom(imageViewer.zoom * factor)
                    wheel.accepted = true
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true
                property real startX: 0
                property real startY: 0
                property real startPanX: 0
                property real startPanY: 0
                onPressed: {
                    startX = mouse.x
                    startY = mouse.y
                    startPanX = imageViewer.panX
                    startPanY = imageViewer.panY
                }
                onPositionChanged: {
                    if (!(mouse.buttons & Qt.LeftButton)) {
                        return
                    }
                    imageViewer.panX = startPanX + (mouse.x - startX)
                    imageViewer.panY = startPanY + (mouse.y - startY)
                    imageViewer.clampPan()
                }
                onDoubleClicked: {
                    imageViewer.zoom = 1.0
                    imageViewer.panX = 0
                    imageViewer.panY = 0
                }
            }

            Image {
                id: imageItem
                width: imageViewer.imageBaseWidth
                height: imageViewer.imageBaseHeight
                x: (parent.width - width) / 2 + imageViewer.panX
                y: (parent.height - height) / 2 + imageViewer.panY
                source: imageViewer.sourceUrl
                fillMode: Image.PreserveAspectFit
                smooth: true
                antialiasing: true
                cache: true
                transformOrigin: Item.Center
                scale: imageViewer.zoom
                onStatusChanged: {
                    if (status === Image.Ready) {
                        imageViewer.updateBaseSize()
                    }
                }
            }

            Components.GhostButton {
                text: Ui.I18n.t("image.preview.close")
                width: 60
                height: 28
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 16
                onClicked: imageViewer.closeViewer()
            }
        }
    }

    ListModel {
        id: emojiModel
    }
    ListModel {
        id: stickerModel
    }

    Popup {
        id: emojiPopup
        modal: false
        focus: true
        padding: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: emojiPopupWidth
        height: emojiPopupHeight

        background: Rectangle {
            radius: 10
            color: Ui.Style.panelBgRaised
            border.color: Ui.Style.borderSubtle
        }

        contentItem: ColumnLayout {
            anchors.fill: parent
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Rectangle {
                    Layout.fillWidth: true
                    height: 26
                    radius: 8
                    color: emojiTabIndex === 0 ? Ui.Style.accent : Ui.Style.panelBg
                    border.color: Ui.Style.borderSubtle
                    Text {
                        anchors.centerIn: parent
                        text: Ui.I18n.t("chat.emoji")
                        font.pixelSize: 11
                        color: emojiTabIndex === 0 ? Ui.Style.textPrimary : Ui.Style.textMuted
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: emojiTabIndex = 0
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 26
                    radius: 8
                    color: emojiTabIndex === 1 ? Ui.Style.accent : Ui.Style.panelBg
                    border.color: Ui.Style.borderSubtle
                    Text {
                        anchors.centerIn: parent
                        text: Ui.I18n.t("chat.sticker")
                        font.pixelSize: 11
                        color: emojiTabIndex === 1 ? Ui.Style.textPrimary : Ui.Style.textMuted
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: emojiTabIndex = 1
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: emojiTabIndex

                GridView {
                    id: emojiGrid
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    cellWidth: emojiCellSize
                    cellHeight: emojiCellSize
                    model: emojiModel
                    clip: true
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                    delegate: Item {
                        width: emojiGrid.cellWidth
                        height: emojiGrid.cellHeight
                        Text {
                            anchors.centerIn: parent
                            text: value
                            font.pixelSize: 18
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: root.insertEmoji(value)
                        }
                    }
                }

                GridView {
                    id: stickerGrid
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    cellWidth: stickerCellSize
                    cellHeight: stickerCellSize
                    model: stickerModel
                    clip: true
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                    delegate: Item {
                        width: stickerGrid.cellWidth
                        height: stickerGrid.cellHeight
                        AnimatedImage {
                            anchors.centerIn: parent
                            width: stickerGrid.cellWidth - 10
                            height: stickerGrid.cellHeight - 10
                            visible: animated
                            source: path
                            playing: true
                            cache: true
                            fillMode: Image.PreserveAspectFit
                        }
                        Image {
                            anchors.centerIn: parent
                            width: stickerGrid.cellWidth - 10
                            height: stickerGrid.cellHeight - 10
                            visible: !animated
                            source: path
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            antialiasing: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                Ui.ChatDisplayStore.sendSticker(stickerId)
                                emojiPopup.close()
                            }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: attachPopup
        modal: false
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property int itemHeight: 36
        property int iconSize: 18
        property int iconGap: 6
        property int contentLeftPadding: 5
        property int contentRightPadding: 5
        readonly property int iconBlockWidth: iconSize + iconGap
        property int fontSize: 12
        property string textPhoto: Ui.I18n.t("attach.photoVideo")
        property string textDocument: Ui.I18n.t("attach.document")
        property string textContact: Ui.I18n.t("attach.contact")
        property string textLocation: Ui.I18n.t("attach.location")
        property var items: [
            { kind: "photo", label: Ui.I18n.t("attach.photoVideo"), iconSource: "qrc:/mi/e2ee/ui/icons/image.svg" },
            { kind: "document", label: Ui.I18n.t("attach.document"), iconSource: "qrc:/mi/e2ee/ui/icons/file.svg" },
            { kind: "contact", label: Ui.I18n.t("attach.contact"), iconSource: "qrc:/mi/e2ee/ui/icons/info.svg" },
            { kind: "location", label: Ui.I18n.t("attach.location"), iconSource: "qrc:/mi/e2ee/ui/icons/location.svg" }
        ]
        readonly property real maxTextWidth: Math.max(metricsPhoto.width,
                                                     metricsDocument.width,
                                                     metricsContact.width,
                                                     metricsLocation.width)
        implicitWidth: Math.ceil(contentLeftPadding + contentRightPadding + iconBlockWidth + maxTextWidth + 7)
        implicitHeight: Math.ceil(itemHeight * 4)

        background: Rectangle {
            radius: 10
            color: Ui.Style.panelBgRaised
            border.color: Ui.Style.borderSubtle
        }

        contentItem: ColumnLayout {
            spacing: 0
            Repeater {
                model: attachPopup.items

                delegate: Item {
                    width: attachPopup.implicitWidth
                    height: attachPopup.itemHeight
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: mouseArea.containsMouse ? Ui.Style.hoverBg : "transparent"
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: attachPopup.contentLeftPadding
                        anchors.rightMargin: attachPopup.contentRightPadding
                        spacing: 0
                        Item {
                            width: attachPopup.iconBlockWidth
                            height: attachPopup.iconSize
                            Image {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: attachPopup.iconSize
                                height: attachPopup.iconSize
                                source: modelData.iconSource
                                sourceSize.width: attachPopup.iconSize
                                sourceSize.height: attachPopup.iconSize
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                            }
                        }
                        Text {
                            text: modelData.label
                            color: Ui.Style.textPrimary
                            font.pixelSize: attachPopup.fontSize
                            font.family: Ui.Style.fontFamily
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }
                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            attachPopup.close()
                            if (modelData.kind === "photo") {
                                mediaPicker.open()
                            } else if (modelData.kind === "document") {
                                filePicker.open()
                            } else if (modelData.kind === "contact") {
                                contactDialog.open()
                            } else if (modelData.kind === "location") {
                                locationDialog.open()
                            }
                        }
                    }
                }
            }
        }

        TextMetrics {
            id: metricsPhoto
            text: attachPopup.textPhoto
            font.pixelSize: attachPopup.fontSize
            font.family: Ui.Style.fontFamily
        }
        TextMetrics {
            id: metricsDocument
            text: attachPopup.textDocument
            font.pixelSize: attachPopup.fontSize
            font.family: Ui.Style.fontFamily
        }
        TextMetrics {
            id: metricsContact
            text: attachPopup.textContact
            font.pixelSize: attachPopup.fontSize
            font.family: Ui.Style.fontFamily
        }
        TextMetrics {
            id: metricsLocation
            text: attachPopup.textLocation
            font.pixelSize: attachPopup.fontSize
            font.family: Ui.Style.fontFamily
        }
    }

    Popup {
        id: contactDialog
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape
        width: 360
        property string errorText: ""

        onOpened: {
            errorText = ""
            contactUsernameField.text = ""
            contactDisplayField.text = ""
        }

        background: Rectangle {
            radius: 12
            color: Ui.Style.panelBgRaised
            border.color: Ui.Style.borderSubtle
        }

        contentItem: ColumnLayout {
            anchors.margins: Ui.Style.paddingM
            spacing: Ui.Style.paddingS

            Text {
                text: Ui.I18n.t("attach.contactTitle")
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: Ui.Style.textPrimary
                elide: Text.ElideRight
            }
            Text {
                text: Ui.I18n.t("attach.contactHint")
                color: Ui.Style.textMuted
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Components.SecureTextField {
                id: contactUsernameField
                Layout.fillWidth: true
                placeholderText: Ui.I18n.t("attach.contactUsername")
                font.pixelSize: 12
            }
            Components.SecureTextField {
                id: contactDisplayField
                Layout.fillWidth: true
                placeholderText: Ui.I18n.t("attach.contactDisplay")
                font.pixelSize: 12
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 140
                visible: Ui.ChatDisplayStore.contactsModel.count > 0
                radius: 10
                color: Ui.Style.panelBgAlt
                border.color: Ui.Style.borderSubtle

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingS
                    spacing: Ui.Style.paddingXS

                    Text {
                        text: Ui.I18n.t("attach.contactSelect")
                        color: Ui.Style.textSecondary
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: Ui.ChatDisplayStore.contactsModel
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 38
                            radius: 8
                            color: pickerArea.containsMouse ? Ui.Style.hoverBg : "transparent"

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingS
                                spacing: Ui.Style.paddingS

                                Rectangle {
                                    width: 24
                                    height: 24
                                    radius: 12
                                    color: Ui.Style.avatarColor(displayName || contactId || "")
                                    Text {
                                        anchors.centerIn: parent
                                        text: (displayName || contactId || "?").charAt(0).toUpperCase()
                                        color: Ui.Style.textPrimary
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Text {
                                        text: displayName || contactId || ""
                                        color: Ui.Style.textPrimary
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: contactId || ""
                                        color: Ui.Style.textMuted
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            MouseArea {
                                id: pickerArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    contactUsernameField.text = contactId || ""
                                    contactDisplayField.text = displayName || ""
                                }
                            }
                        }
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                    }
                }
            }
            Text {
                visible: contactDialog.errorText.length > 0
                text: contactDialog.errorText
                color: Ui.Style.danger
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Ui.Style.paddingS
                Components.GhostButton {
                    text: Ui.I18n.t("attach.contactCancel")
                    Layout.fillWidth: true
                    onClicked: contactDialog.close()
                }
                Components.PrimaryButton {
                    text: Ui.I18n.t("attach.contactSend")
                    Layout.fillWidth: true
                    onClicked: {
                        var ok = Ui.ChatDisplayStore.sendContactCard(contactUsernameField.text,
                                                             contactDisplayField.text)
                        if (ok) {
                            contactDialog.close()
                        } else {
                            contactDialog.errorText = Ui.ChatDisplayStore.sendErrorMessage
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: locationDialog
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape
        width: 320
        property string errorText: ""
        property bool locationBusy: false
        onOpened: {
            errorText = ""
            locationBusy = false
            locationLabelField.text = ""
            locationLatField.text = ""
            locationLonField.text = ""
        }
        onClosed: locationSourceLoader.active = false

        background: Rectangle {
            radius: 12
            color: Ui.Style.panelBgRaised
            border.color: Ui.Style.borderSubtle
        }

        contentItem: ColumnLayout {
            anchors.margins: Ui.Style.paddingM
            spacing: Ui.Style.paddingS
            Text {
                text: Ui.I18n.t("attach.locationTitle")
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: Ui.Style.textPrimary
                elide: Text.ElideRight
            }
            Components.SecureTextField {
                id: locationLabelField
                Layout.fillWidth: true
                placeholderText: Ui.I18n.t("attach.locationLabel")
                font.pixelSize: 12
            }
            Components.SecureTextField {
                id: locationLatField
                Layout.fillWidth: true
                placeholderText: Ui.I18n.t("attach.locationLat")
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                font.pixelSize: 12
            }
            Components.SecureTextField {
                id: locationLonField
                Layout.fillWidth: true
                placeholderText: Ui.I18n.t("attach.locationLon")
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                font.pixelSize: 12
            }
            Components.GhostButton {
                text: locationDialog.locationBusy
                      ? Ui.I18n.t("attach.locationFetching")
                      : Ui.I18n.t("attach.locationCurrent")
                Layout.fillWidth: true
                enabled: !locationDialog.locationBusy
                onClicked: requestCurrentLocation()
            }
            Text {
                visible: locationDialog.errorText.length > 0
                text: locationDialog.errorText
                color: Ui.Style.danger
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Ui.Style.paddingS
                Components.GhostButton {
                    text: Ui.I18n.t("attach.locationCancel")
                    Layout.fillWidth: true
                    onClicked: locationDialog.close()
                }
                Components.PrimaryButton {
                    text: Ui.I18n.t("attach.locationSend")
                    Layout.fillWidth: true
                    onClicked: {
                        var lat = parseFloat(locationLatField.text)
                        var lon = parseFloat(locationLonField.text)
                        if (isNaN(lat) || isNaN(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180) {
                            locationDialog.errorText = Ui.I18n.t("attach.locationInvalid")
                            return
                        }
                        var ok = Ui.ChatDisplayStore.sendLocation(lat, lon, locationLabelField.text)
                        if (ok) {
                            locationDialog.close()
                        } else {
                            locationDialog.errorText = Ui.ChatDisplayStore.sendErrorMessage
                        }
                    }
                }
            }
        }
    }

    Loader {
        id: locationSourceLoader
        active: false
        sourceComponent: locationSourceComponent
        onLoaded: {
            if (locationDialog.locationBusy
                    && item
                    && item.update) {
                item.update()
            }
        }
    }

    Component {
        id: locationSourceComponent
        PositionSource {
            active: false
            updateInterval: 0
            onPositionChanged: {
                if (!position || !position.coordinate || !position.coordinate.isValid) {
                    locationDialog.errorText = Ui.I18n.t("attach.locationUnavailable")
                    locationDialog.locationBusy = false
                    return
                }
                locationLatField.text = position.coordinate.latitude.toFixed(6)
                locationLonField.text = position.coordinate.longitude.toFixed(6)
                if (locationLabelField.text.trim().length === 0) {
                    locationLabelField.text = Ui.I18n.t("attach.locationCurrentLabel")
                }
                locationDialog.errorText = ""
                locationDialog.locationBusy = false
            }
            onSourceErrorChanged: {
                if (sourceError !== PositionSource.NoError) {
                    locationDialog.errorText = Ui.I18n.t("attach.locationUnavailable")
                    locationDialog.locationBusy = false
                }
            }
        }
    }

    Component {
        id: messageDelegate
        Item {
            width: ListView.view.width
            property bool isDate: kind === "date"
            property bool isSystem: kind === "system"
            property bool isIncoming: kind === "in"
            property bool isOutgoing: kind === "out"
            property bool showSender: isIncoming && Ui.ChatDisplayStore.currentChatType === "group"
            property string contentKind: model.contentKind || "text"
            property bool isEmoji: contentKind === "emoji"
            property bool isSticker: contentKind === "sticker"
            property bool isImage: contentKind === "image"
            property bool isGif: contentKind === "gif"
            property bool isVideo: contentKind === "video"
            property bool isFile: contentKind === "file"
            property bool isLocation: contentKind === "location"
            property bool isContact: contentKind === "contact"
            property bool isCall: contentKind === "call"
            property string msgId: model.msgId || ""
            property double timestampMs: model.timestampMs || 0
            property string fileName: model.fileName || ""
            property string fileId: model.fileId || ""
            property string fileKey: model.fileKey || ""
            property var fileUrl: model.fileUrl || ""
            property int fileSize: model.fileSize || 0
            property string contactUsername: model.contactUsername || ""
            property string contactDisplay: model.contactDisplay || ""
            property bool imageEnhanced: model.imageEnhanced === true
            property bool attachmentRequested: false
            property int senderAvatarSize: 26
            property int senderAvatarGap: 8
            property int senderLeftInset: showSender
                                            ? Ui.Style.paddingL + senderAvatarSize + senderAvatarGap
                                            : Ui.Style.paddingL
            property bool recallEligible: isOutgoing && msgId.length > 0 &&
                                          timestampMs > 0 &&
                                          (Date.now() - timestampMs) <= Ui.ChatDisplayStore.recallWindowMs

            height: isDate || isSystem ? 32 : bubbleBlock.height + 10

            function hasLocalUrl(value) {
                if (!value) {
                    return false
                }
                if (value.toString) {
                    return value.toString().length > 0
                }
                return ("" + value).length > 0
            }

            function requestAttachmentCache() {
                if (attachmentRequested) {
                    return
                }
                var expectedKind = Ui.ChatDisplayStore.detectFileKind(fileName || "")
                var bridge = (typeof clientBridge !== "undefined") ? clientBridge : null
                if (!fileId || !fileKey || hasLocalUrl(fileUrl)) {
                    return
                }
                if (!bridge || !bridge.ensureAttachmentCached) {
                    return
                }
                attachmentRequested = true
                Qt.callLater(function() {
                    var result = bridge.ensureAttachmentCached(fileId, fileKey, fileName, fileSize)
                    if (result && result.ok && ListView.view && ListView.view.model) {
                        if (result.fileUrl) {
                            ListView.view.model.setProperty(index, "fileUrl", result.fileUrl)
                            if (expectedKind && expectedKind !== "file") {
                                ListView.view.model.setProperty(index, "contentKind", expectedKind)
                            }
                        }
                        if (result.previewUrl) {
                            ListView.view.model.setProperty(index, "previewUrl", result.previewUrl)
                        }
                    }
                })
            }

            Component.onCompleted: requestAttachmentCache()
            onFileUrlChanged: requestAttachmentCache()
            onFileIdChanged: requestAttachmentCache()
            onFileKeyChanged: requestAttachmentCache()
            onFileNameChanged: requestAttachmentCache()

            Rectangle {
                visible: isDate
                anchors.horizontalCenter: parent.horizontalCenter
                y: 6
                radius: 10
                color: Qt.rgba(1, 1, 1, 0.65)
                border.color: Qt.rgba(0, 0, 0, 0.06)
                height: 20
                width: dateText.paintedWidth + 16
                Text {
                    id: dateText
                    anchors.centerIn: parent
                    text: model.text || ""
                    color: Ui.Style.textMuted
                    font.pixelSize: Ui.Style.microTextSize
                    font.weight: Font.Medium
                }
            }

            Text {
                visible: isSystem
                anchors.horizontalCenter: parent.horizontalCenter
                y: 8
                text: model.text || ""
                color: Ui.Style.textMuted
                font.pixelSize: Ui.Style.microTextSize
                font.weight: Font.Medium
            }

            Rectangle {
                id: senderAvatar
                visible: showSender
                width: senderAvatarSize
                height: senderAvatarSize
                anchors.left: parent.left
                anchors.leftMargin: Ui.Style.paddingL
                anchors.top: bubbleBlock.top
                anchors.topMargin: 2
                color: "transparent"

                Components.IdentityAvatar {
                    anchors.fill: parent
                    size: senderAvatarSize
                    titleText: senderName || ""
                    seedText: senderName || ""
                    mode: "person"
                    presenceState: "online"
                }
            }

            Item {
                id: bubbleBlock
                visible: isIncoming || isOutgoing
                property bool transparentBubble: isSticker || isEmoji
                property bool usesFullWidth: !isSticker && !isImage && !isGif &&
                                             !isVideo && !isFile && !isLocation &&
                                             !isContact &&
                                             !isEmoji
                property int hPadding: transparentBubble ? (isEmoji ? 4 : 6) : 12
                property int vPadding: transparentBubble ? (isEmoji ? 4 : 6) : 8
                property int outgoingMetaSafeInset: isOutgoing ? 14 : 0
                property int outgoingMetaBottomInset: isOutgoing ? 12 : 0
                property int outgoingMetaExtraPad: isOutgoing ? Math.max(0, outgoingMetaSafeInset + 4 - hPadding) : 0
                property int outgoingMetaRightPad: isOutgoing && usesFullWidth
                                                   ? Math.max(18, outgoingMetaSafeInset + 6)
                                                   : 0
                property real bubbleEdgeInset: transparentBubble
                                               ? Ui.Style.paddingL
                                               : (isOutgoing
                                                  ? Ui.Style.paddingXL + 14
                                                  : Ui.Style.paddingXL + 4)
                property real availableBubbleWidth: Math.max(220,
                                                             (ListView.view ? ListView.view.width : root.width)
                                                             - bubbleEdgeInset * 2)
                property real maxBubbleWidth: Math.min(Math.max(isOutgoing ? 204 : 212,
                                                                availableBubbleWidth * (isOutgoing ? 0.66 : 0.62)),
                                                       Math.max(208,
                                                                availableBubbleWidth - (transparentBubble
                                                                                        ? 0
                                                                                        : (isOutgoing ? 24 : 16))))
                property real contentWidth: contentLoader.item
                                             ? Math.min(maxBubbleWidth - hPadding * 2,
                                                        contentLoader.item.implicitWidth)
                                             : 0
                property real contentHeight: contentLoader.item ? contentLoader.item.implicitHeight : 0
                property real bubbleWidth: usesFullWidth
                                           ? Math.min(maxBubbleWidth + outgoingMetaRightPad,
                                                      availableBubbleWidth)
                                           : Math.min(maxBubbleWidth,
                                                      Math.max(contentWidth, metaBadge.implicitWidth + outgoingMetaExtraPad) + hPadding * 2)
                property real bubbleHeight: contentHeight + metaBadge.implicitHeight +
                                            vPadding * 2 + (senderLabel.visible ? senderLabel.implicitHeight + 4 : 0)

                width: bubbleWidth
                height: bubbleHeight
                x: isOutgoing
                   ? Math.max(Ui.Style.paddingL,
                              parent.width - bubbleWidth - bubbleEdgeInset - (transparentBubble ? 0 : 12))
                   : senderLeftInset
                y: 4

                Component {
                    id: textContent
                    Item {
                        implicitWidth: textBlock.paintedWidth
                        implicitHeight: textBlock.paintedHeight
                        width: bubbleBlock.maxBubbleWidth - bubbleBlock.hPadding * 2
                        Text {
                            id: textBlock
                            text: model.text || ""
                            width: parent.width
                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                            color: isOutgoing ? Ui.Style.bubbleOutFg : Ui.Style.bubbleInFg
                            font.pixelSize: 13
                        }
                    }
                }

                Component {
                    id: emojiContent
                    Item {
                        implicitWidth: emojiText.paintedWidth
                        implicitHeight: emojiText.paintedHeight
                        width: implicitWidth
                        height: implicitHeight
                        Text {
                            id: emojiText
                            text: model.text || ""
                            font.pixelSize: 40
                            color: isOutgoing ? Ui.Style.bubbleOutFg : Ui.Style.bubbleInFg
                            transformOrigin: Item.Center
                            anchors.centerIn: parent
                        }
                        SequentialAnimation {
                            running: isEmoji && (model.animateEmoji === true)
                            loops: 1
                            ParallelAnimation {
                                NumberAnimation { target: emojiText; property: "scale"; from: 0.6; to: 1.2; duration: 200; easing.type: Easing.OutBack }
                                NumberAnimation { target: emojiText; property: "opacity"; from: 0.4; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
                            }
                            NumberAnimation { target: emojiText; property: "scale"; from: 1.2; to: 1.0; duration: 160; easing.type: Easing.OutCubic }
                            onStopped: {
                                if (model.animateEmoji === true && ListView.view && ListView.view.model) {
                                    ListView.view.model.setProperty(index, "animateEmoji", false)
                                }
                            }
                        }
                    }
                }

                Component {
                    id: stickerContent
                    Item {
                        implicitWidth: stickerSize
                        implicitHeight: stickerSize
                        AnimatedImage {
                            anchors.centerIn: parent
                            width: stickerSize
                            height: stickerSize
                            visible: stickerAnimated
                            source: stickerUrl
                            playing: true
                            cache: true
                            fillMode: Image.PreserveAspectFit
                        }
                        Image {
                            anchors.centerIn: parent
                            width: stickerSize
                            height: stickerSize
                            visible: !stickerAnimated
                            source: stickerUrl
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            antialiasing: true
                        }
                    }
                }

                Component {
                    id: imageContent
                    Item {
                        implicitWidth: 240
                        implicitHeight: 180
                        Image {
                            anchors.centerIn: parent
                            width: 240
                            height: 180
                            source: fileUrl
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            antialiasing: true
                        }
                        Menu {
                            id: imageContextMenu
                            property int compactWidth: root.contextMenuWidth([
                                Ui.I18n.t("image.context.enhanced"),
                                Ui.I18n.t("image.context.enhance"),
                                Ui.I18n.t("image.context.setBackground"),
                                Ui.I18n.t("chat.recall")
                            ])
                            implicitWidth: compactWidth
                            width: compactWidth
                            MenuItem {
                                text: imageEnhanced
                                      ? Ui.I18n.t("image.context.enhanced")
                                      : Ui.I18n.t("image.context.enhance")
                                enabled: !imageEnhanced &&
                                         Ui.ChatDisplayStore.aiEnhanceEnabled &&
                                         hasLocalUrl(fileUrl)
                                width: imageContextMenu.compactWidth
                                implicitWidth: imageContextMenu.compactWidth
                                height: 28
                                contentItem: Text {
                                    text: parent.text
                                    anchors.fill: parent
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 12
                                    color: parent.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                                }
                                onTriggered: root.requestImageEnhanceForMessage(
                                                 msgId, fileUrl, fileName)
                            }
                            MenuItem {
                                text: Ui.I18n.t("image.context.setBackground")
                                enabled: hasLocalUrl(fileUrl)
                                width: imageContextMenu.compactWidth
                                implicitWidth: imageContextMenu.compactWidth
                                height: 28
                                contentItem: Text {
                                    text: parent.text
                                    anchors.fill: parent
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 12
                                    color: parent.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                                }
                                onTriggered: Ui.ChatDisplayStore.setChatBackgroundForCurrentChat(fileUrl)
                            }
                            MenuItem {
                                text: Ui.I18n.t("chat.recall")
                                visible: isOutgoing
                                enabled: true
                                width: imageContextMenu.compactWidth
                                implicitWidth: imageContextMenu.compactWidth
                                height: 28
                                contentItem: Text {
                                    text: parent.text
                                    anchors.fill: parent
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 12
                                    color: recallEligible ? Ui.Style.textPrimary : Ui.Style.textMuted
                                }
                                onTriggered: Ui.ChatDisplayStore.requestRecallMessage(
                                                 Ui.ChatDisplayStore.currentChatId,
                                                 msgId,
                                                 timestampMs,
                                                 Ui.ChatDisplayStore.currentChatType === "group")
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.RightButton) {
                                    var pos = mapToItem(root, mouse.x, mouse.y)
                                    imageContextMenu.popup(root, pos.x, pos.y)
                                    return
                                }
                                root.openImagePreview(fileUrl, fileName)
                            }
                        }
                    }
                }

                Component {
                    id: gifContent
                    Item {
                        implicitWidth: 240
                        implicitHeight: 180
                        AnimatedImage {
                            anchors.centerIn: parent
                            width: 240
                            height: 180
                            source: fileUrl
                            playing: true
                            cache: true
                            fillMode: Image.PreserveAspectFit
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.openImagePreview(fileUrl, fileName)
                        }
                    }
                }

                Component {
                    id: videoContent
                    Item {
                        id: videoItem
                        implicitWidth: 260
                        implicitHeight: 180
                        property bool hasSource: fileUrl && fileUrl.toString ? fileUrl.toString().length > 0
                                               : (fileUrl && ("" + fileUrl).length > 0)
                        property bool previewAvailable: previewUrl && previewUrl.toString
                                                        ? previewUrl.toString().length > 0
                                                        : (previewUrl && ("" + previewUrl).length > 0)

                        Rectangle {
                            anchors.fill: parent
                            radius: 12
                            color: Ui.Style.panelBgAlt
                            border.color: Ui.Style.borderSubtle
                        }

                        MediaPlayer {
                            id: videoPlayer
                            source: fileUrl
                            videoOutput: videoView
                            audioOutput: AudioOutput {
                                volume: 1.0
                            }
                        }

                        VideoOutput {
                            id: videoView
                            anchors.fill: parent
                            fillMode: VideoOutput.PreserveAspectFit
                            visible: videoItem.hasSource
                        }

                        Image {
                            anchors.fill: parent
                            source: previewUrl || ""
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            antialiasing: true
                            visible: !videoItem.hasSource
                                     ? true
                                     : (videoPlayer.playbackState !== MediaPlayer.PlayingState &&
                                        videoItem.previewAvailable)
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 48
                            height: 48
                            radius: 24
                            color: Qt.rgba(0, 0, 0, 0.45)
                            visible: videoItem.hasSource &&
                                     videoPlayer.playbackState !== MediaPlayer.PlayingState
                            Image {
                                anchors.centerIn: parent
                                source: "qrc:/mi/e2ee/ui/icons/video.svg"
                                width: 20
                                height: 20
                                opacity: 0.9
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (!videoItem.hasSource) {
                                    return
                                }
                                if (videoPlayer.playbackState === MediaPlayer.PlayingState) {
                                    videoPlayer.pause()
                                } else {
                                    videoPlayer.play()
                                }
                            }
                        }

                        onVisibleChanged: {
                            if (!visible && videoPlayer.playbackState === MediaPlayer.PlayingState) {
                                videoPlayer.pause()
                            }
                        }
                    }
                }

                Component {
                    id: fileContent
                    Item {
                        implicitWidth: 248
                        implicitHeight: 92
                        property real progressValue: (downloadProgress !== undefined
                                                       && downloadProgress !== null)
                                                      ? downloadProgress : 0
                        readonly property string previewKind: {
                            var detectedKind = Ui.ChatDisplayStore.detectFileKind(fileName || "")
                            if (detectedKind === "image" || detectedKind === "gif") {
                                return "photo"
                            }
                            if (detectedKind === "video") {
                                return "video"
                            }
                            return "file"
                        }
                        Rectangle {
                            anchors.fill: parent
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.heroCardBgAlt
                            border.width: 1
                            border.color: Ui.Style.heroCardBorder

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                height: 34
                                radius: Ui.Style.radiusLarge
                                color: root.previewTintFor(fileContent.previewKind)
                                opacity: Ui.Style.isDark ? 0.34 : 0.56
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 10

                                Rectangle {
                                    Layout.preferredWidth: 42
                                    Layout.preferredHeight: 42
                                    radius: 16
                                    color: Ui.Style.badgeSurfaceStrong
                                    border.width: 1
                                    border.color: Ui.Style.badgeBorder

                                    Image {
                                        anchors.centerIn: parent
                                        width: 18
                                        height: 18
                                        source: root.previewIconFor(fileContent.previewKind)
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        antialiasing: true
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        Layout.fillWidth: true
                                        text: fileName || ""
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        color: Ui.Style.textPrimary
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                        renderType: Text.NativeRendering
                                        antialiasing: true
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: fileSize > 0
                                              ? (Math.round(fileSize / 1024) + " KB")
                                              : Ui.I18n.t("right.files")
                                        font.pixelSize: 10
                                        color: Ui.Style.textSecondary
                                        elide: Text.ElideRight
                                        maximumLineCount: 1
                                        renderType: Text.NativeRendering
                                        antialiasing: true
                                    }

                                    Rectangle {
                                        Layout.alignment: Qt.AlignLeft
                                        radius: 9
                                        color: Ui.Style.sidebarMetaChipBg
                                        border.width: 1
                                        border.color: Ui.Style.sidebarMetaChipBorder
                                        implicitWidth: fileMetaLabel.implicitWidth + 12
                                        implicitHeight: 18

                                        Text {
                                            id: fileMetaLabel
                                            anchors.centerIn: parent
                                            text: hasLocalUrl(fileUrl)
                                                  ? (Ui.I18n.usesCjkLocale ? "已缓存" : "Cached")
                                                  : (Ui.I18n.usesCjkLocale ? "点击下载" : "Tap to download")
                                            font.pixelSize: 10
                                            font.weight: Font.DemiBold
                                            color: Ui.Style.textSecondary
                                            renderType: Text.NativeRendering
                                            antialiasing: true
                                        }
                                    }
                                }
                            }
                        }
                        Canvas {
                            id: downloadRing
                            width: 18
                            height: 18
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 6
                            property real progress: Math.max(0, Math.min(1, fileContent.progressValue))
                            onProgressChanged: requestPaint()
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                var cx = width / 2
                                var cy = height / 2
                                var r = Math.min(width, height) / 2 - 1.5
                                ctx.lineWidth = 3
                                ctx.lineCap = "round"
                                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.25)
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, 0, Math.PI * 2)
                                ctx.stroke()
                                if (progress > 0) {
                                    ctx.strokeStyle = Ui.Style.success
                                    ctx.beginPath()
                                    ctx.arc(cx, cy, r,
                                            -Math.PI / 2,
                                            -Math.PI / 2 + progress * Math.PI * 2)
                                    ctx.stroke()
                                }
                            }
                        }
                        Menu {
                            id: fileContextMenu
                            property int compactWidth: root.contextMenuWidth([
                                Ui.I18n.t("chat.fileDownloadConfirm"),
                                Ui.I18n.t("chat.recall")
                            ])
                            implicitWidth: compactWidth
                            width: compactWidth
                            MenuItem {
                                text: Ui.I18n.t("chat.fileDownloadConfirm")
                                width: fileContextMenu.compactWidth
                                implicitWidth: fileContextMenu.compactWidth
                                height: 28
                                contentItem: Text {
                                    text: parent.text
                                    anchors.fill: parent
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 12
                                    color: parent.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                                }
                                onTriggered: root.promptFileDownload(fileId, fileKey, fileName, fileSize, true)
                            }
                            MenuItem {
                                text: Ui.I18n.t("chat.recall")
                                visible: isOutgoing
                                enabled: true
                                width: fileContextMenu.compactWidth
                                implicitWidth: fileContextMenu.compactWidth
                                height: 28
                                contentItem: Text {
                                    text: parent.text
                                    anchors.fill: parent
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 12
                                    color: recallEligible ? Ui.Style.textPrimary : Ui.Style.textMuted
                                }
                                onTriggered: Ui.ChatDisplayStore.requestRecallMessage(
                                                 Ui.ChatDisplayStore.currentChatId,
                                                 msgId,
                                                 timestampMs,
                                                 Ui.ChatDisplayStore.currentChatType === "group")
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.RightButton) {
                                    var pos = mapToItem(root, mouse.x, mouse.y)
                                    fileContextMenu.popup(root, pos.x, pos.y)
                                    return
                                }
                                root.promptFileDownload(fileId, fileKey, fileName, fileSize)
                            }
                        }
                    }
                }

                Component {
                    id: locationContent
                    Item {
                        implicitWidth: 248
                        implicitHeight: 108
                        Rectangle {
                            anchors.fill: parent
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.heroCardBgAlt
                            border.width: 1
                            border.color: Ui.Style.heroCardBorder
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: 44
                            radius: Ui.Style.radiusLarge
                            color: Qt.rgba(14 / 255, 165 / 255, 233 / 255, Ui.Style.isDark ? 0.20 : 0.14)
                            border.width: 1
                            border.color: Qt.rgba(14 / 255, 165 / 255, 233 / 255, Ui.Style.isDark ? 0.24 : 0.12)

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8

                                Rectangle {
                                    width: 28
                                    height: 28
                                    radius: 14
                                    color: Ui.Style.badgeSurfaceStrong
                                    border.width: 1
                                    border.color: Ui.Style.badgeBorder

                                    Image {
                                        anchors.centerIn: parent
                                        width: 14
                                        height: 14
                                        source: "qrc:/mi/e2ee/ui/icons/location.svg"
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        antialiasing: true
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: (locationLabel && locationLabel.length > 0)
                                          ? locationLabel
                                          : Ui.I18n.t("attach.location")
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: Ui.Style.textPrimary
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                    renderType: Text.NativeRendering
                                    antialiasing: true
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 10
                            anchors.topMargin: 54
                            spacing: 6

                            Text {
                                Layout.fillWidth: true
                                text: Ui.I18n.t("attach.locationLat") + ": " +
                                      Number(locationLat).toFixed(5) + "   " +
                                      Ui.I18n.t("attach.locationLon") + ": " +
                                      Number(locationLon).toFixed(5)
                                font.pixelSize: 10
                                color: Ui.Style.textSecondary
                                wrapMode: Text.Wrap
                                maximumLineCount: 2
                                renderType: Text.NativeRendering
                                antialiasing: true
                            }

                            Rectangle {
                                Layout.alignment: Qt.AlignLeft
                                radius: 9
                                color: Ui.Style.sidebarMetaChipBg
                                border.width: 1
                                border.color: Ui.Style.sidebarMetaChipBorder
                                implicitWidth: locationMetaLabel.implicitWidth + 12
                                implicitHeight: 18

                                Text {
                                    id: locationMetaLabel
                                    anchors.centerIn: parent
                                    text: Ui.I18n.usesCjkLocale ? "共享位置卡片" : "Shared location card"
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    color: Ui.Style.textSecondary
                                    renderType: Text.NativeRendering
                                    antialiasing: true
                                }
                            }
                        }
                    }
                }

                Component {
                    id: contactContent
                    Item {
                        implicitWidth: 248
                        implicitHeight: 126

                        Rectangle {
                            anchors.fill: parent
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.heroCardBgAlt
                            border.width: 1
                            border.color: Ui.Style.heroCardBorder
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: 42
                            radius: Ui.Style.radiusLarge
                            color: Qt.rgba(51 / 255, 144 / 255, 236 / 255, Ui.Style.isDark ? 0.20 : 0.12)
                            border.width: 1
                            border.color: Qt.rgba(51 / 255, 144 / 255, 236 / 255, Ui.Style.isDark ? 0.24 : 0.14)
                        }

                        ColumnLayout {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.margins: 10
                            anchors.topMargin: 8
                            spacing: 6

                            RowLayout {
                                spacing: 8
                                Rectangle {
                                    width: 38
                                    height: 38
                                    radius: 19
                                    color: Ui.Style.alpha(Ui.Style.avatarColor(contactDisplay || contactUsername || ""),
                                                          Ui.Style.isDark ? 0.92 : 1.0)
                                    border.width: 1
                                    border.color: Ui.Style.badgeBorder
                                    Text {
                                        anchors.centerIn: parent
                                        text: (contactDisplay || contactUsername || "?").charAt(0).toUpperCase()
                                        color: Ui.Style.textPrimary
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Text {
                                        text: contactDisplay.length > 0
                                              ? contactDisplay
                                              : (contactUsername.length > 0
                                                 ? contactUsername
                                                 : Ui.I18n.t("chat.contact"))
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        color: Ui.Style.textPrimary
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: contactUsername
                                        visible: contactUsername.length > 0
                                        font.pixelSize: 10
                                        color: Ui.Style.textSecondary
                                        elide: Text.ElideRight
                                    }
                                }

                                Rectangle {
                                    Layout.alignment: Qt.AlignVCenter
                                    radius: 9
                                    color: Ui.Style.sidebarMetaChipBg
                                    border.width: 1
                                    border.color: Ui.Style.sidebarMetaChipBorder
                                    implicitWidth: contactMetaLabel.implicitWidth + 12
                                    implicitHeight: 18

                                    Text {
                                        id: contactMetaLabel
                                        anchors.centerIn: parent
                                        text: Ui.I18n.usesCjkLocale ? "联系人卡片" : "Contact card"
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                        color: Ui.Style.textSecondary
                                        renderType: Text.NativeRendering
                                        antialiasing: true
                                    }
                                }
                            }

                            Text {
                                text: model.text || ""
                                font.pixelSize: 10
                                color: Ui.Style.textSecondary
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                                maximumLineCount: 3
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6
                                Components.GhostButton {
                                    text: Ui.I18n.t("chat.contactCopy")
                                    Layout.fillWidth: true
                                    onClicked: Ui.ChatDisplayStore.setInternalClipboard(contactUsername)
                                }
                                Components.PrimaryButton {
                                    text: Ui.I18n.t("chat.contactOpen")
                                    Layout.fillWidth: true
                                    enabled: contactUsername.length > 0
                                    onClicked: Ui.ChatDisplayStore.openChatFromContact(contactUsername)
                                }
                            }
                        }
                    }
                }

                Menu {
                    id: messageContextMenu
                    property int compactWidth: root.contextMenuWidth([
                        Ui.I18n.t("chat.recall")
                    ])
                    implicitWidth: compactWidth
                    width: compactWidth
                    MenuItem {
                        text: Ui.I18n.t("chat.recall")
                        visible: isOutgoing
                        enabled: true
                        width: messageContextMenu.compactWidth
                        implicitWidth: messageContextMenu.compactWidth
                        height: 28
                        contentItem: Text {
                            text: parent.text
                            anchors.fill: parent
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: 12
                            color: recallEligible ? Ui.Style.textPrimary : Ui.Style.textMuted
                        }
                        onTriggered: Ui.ChatDisplayStore.requestRecallMessage(
                                         Ui.ChatDisplayStore.currentChatId,
                                         msgId,
                                         timestampMs,
                                         Ui.ChatDisplayStore.currentChatType === "group")
                    }
                }

                Rectangle {
                    id: bubble
                    anchors.fill: parent
                    radius: 12
                    color: bubbleBlock.transparentBubble
                           ? "transparent"
                           : (isOutgoing ? Ui.Style.bubbleOutBg : Ui.Style.bubbleInBg)
                    border.width: 0

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        hoverEnabled: true
                        propagateComposedEvents: true
                        onClicked: {
                            if (mouse.button !== Qt.RightButton) {
                                return
                            }
                            if (!isOutgoing) {
                                return
                            }
                            var pos = mapToItem(root, mouse.x, mouse.y)
                            messageContextMenu.popup(root, pos.x, pos.y)
                        }
                    }

                    Column {
                        anchors.fill: parent
                        anchors.leftMargin: bubbleBlock.hPadding
                        anchors.rightMargin: isOutgoing ? Math.max(bubbleBlock.outgoingMetaSafeInset, bubbleBlock.hPadding) : bubbleBlock.hPadding
                        anchors.topMargin: bubbleBlock.vPadding
                        anchors.bottomMargin: isOutgoing ? Math.max(bubbleBlock.outgoingMetaBottomInset, bubbleBlock.vPadding) : bubbleBlock.vPadding
                        spacing: 4

                        Text {
                            id: senderLabel
                            visible: showSender
                            text: senderName
                            font.pixelSize: 11
                            color: Ui.Style.link
                        }

                        Loader {
                            id: contentLoader
                            width: bubbleBlock.usesFullWidth
                                   ? bubbleBlock.maxBubbleWidth - bubbleBlock.hPadding * 2
                                   : bubbleBlock.contentWidth
                            sourceComponent: isSticker ? stickerContent
                                            : (isGif ? gifContent
                                            : (isImage ? imageContent
                                            : (isVideo ? videoContent
                                            : (isFile ? fileContent
                                            : (isLocation ? locationContent
                                            : (isContact ? contactContent
                                            : (isEmoji ? emojiContent : textContent)))))))
                        }

                        Rectangle {
                            id: metaBadge
                            anchors.right: parent.right
                            anchors.rightMargin: isOutgoing ? Math.max(10, bubbleBlock.outgoingMetaSafeInset - 2) : 0
                            radius: 6
                            color: bubbleBlock.transparentBubble
                                   ? Qt.rgba(7 / 255, 12 / 255, 18 / 255, 0.78)
                                   : (isOutgoing
                                      ? Qt.rgba(8 / 255, 26 / 255, 20 / 255, 0.34)
                                      : Qt.rgba(8 / 255, 14 / 255, 21 / 255, 0.30))
                            border.width: 0
                            implicitWidth: metaRow.implicitWidth + 10
                            implicitHeight: metaRow.implicitHeight + 2

                            Row {
                                id: metaRow
                                anchors.centerIn: parent
                                spacing: 4
                                Text {
                                    text: timeText || ""
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: isOutgoing
                                           ? Qt.lighter(Ui.Style.bubbleMetaOutFg, 1.08)
                                           : Qt.lighter(Ui.Style.bubbleMetaInFg, 1.08)
                                    renderType: Text.NativeRendering
                                    antialiasing: true
                                }
                                Text {
                                    visible: isOutgoing
                                    text: tickText(statusTicks)
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    color: Qt.lighter(Ui.Style.bubbleMetaOutFg, 1.08)
                                    renderType: Text.NativeRendering
                                    antialiasing: true
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    width: 8
                    height: 8
                    radius: 2
                    color: bubble.color
                    rotation: 45
                    transformOrigin: Item.Center
                    anchors.bottom: bubble.bottom
                    anchors.bottomMargin: 8
                    anchors.left: bubble.left
                    anchors.leftMargin: -4
                    visible: isIncoming && !bubbleBlock.transparentBubble
                }

                Rectangle {
                    width: 8
                    height: 8
                    radius: 2
                    color: bubble.color
                    rotation: 45
                    transformOrigin: Item.Center
                    anchors.bottom: bubble.bottom
                    anchors.bottomMargin: 8
                    anchors.right: bubble.right
                    anchors.rightMargin: 0
                    visible: isOutgoing && !bubbleBlock.transparentBubble
                }
            }

            function tickText(status) {
                if (status === "read") {
                    return "OK"
                }
                if (status === "delivered") {
                    return "DL"
                }
                if (status === "sent") {
                    return "SN"
                }
                return ""
            }
        }
    }
}
