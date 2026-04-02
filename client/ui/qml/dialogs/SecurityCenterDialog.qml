import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    property var ownerWindow: null
    readonly property bool smokeMode: typeof uiSmokeMode !== "undefined" ? !!uiSmokeMode : false
    readonly property bool smokeFixtureMode: smokeMode && Ui.SmokeSceneStore.securityCenterScene
    signal requestManageDevices()
    visible: false
    width: smokeFixtureMode ? Ui.SmokeSceneStore.viewportWidth(false) : 720
    height: smokeFixtureMode ? Ui.SmokeSceneStore.viewportHeight() : 540
    minimumWidth: width
    minimumHeight: height
    maximumWidth: width
    maximumHeight: height
    transientParent: ownerWindow
    flags: Qt.FramelessWindowHint | Qt.Window
    title: Ui.I18n.t("dialog.securityCenter.title")
    color: "transparent"
    font.family: Ui.Style.fontFamily
    palette.window: Ui.Style.windowBg
    palette.base: Ui.Style.panelBgAlt
    palette.button: Ui.Style.panelBgAlt
    palette.text: Ui.Style.textPrimary
    palette.buttonText: Ui.Style.textPrimary
    palette.highlight: Ui.Style.accent
    palette.highlightedText: Ui.Style.textPrimary

    property string currentDeviceDisplay: ""
    property string gatewayState: ""
    property string gatewayInfo: ""
    readonly property bool effectiveTransportHealthy: smokeFixtureMode
                                                     ? true
                                                     : Ui.SecurityDisplayStore.transportHealthy
    readonly property int smokeOuterMargin: Ui.Style.paddingS + 2
    readonly property int smokeCardPadding: Ui.Style.paddingS
    readonly property int smokeGap: Ui.Style.paddingXS + 2
    readonly property string smokeTransportHint: "TLS, session keys, and rotation healthy."
    readonly property string smokeTrustHint: "Root trust ready for sensitive actions."
    readonly property string smokeGatewayStatus: "Pinned fingerprint"
    readonly property string smokeGatewayHint: "Gateway access stays on secure transport."
    readonly property string smokeCurrentDeviceHint: "Primary endpoint for this session."
    readonly property string smokeDevicesHint: "Review, remove, or link trusted devices."
    readonly property var smokeLinkedDevices: [
        {
            maskedDeviceDisplayId: "win-23..ac91",
            lastSeenDisplay: "Windows desktop · 2m ago"
        },
        {
            maskedDeviceDisplayId: "ipad-b2..77fe",
            lastSeenDisplay: "iPad Pro · 18m ago"
        },
        {
            maskedDeviceDisplayId: "mac-51..1d42",
            lastSeenDisplay: "MacBook Air · Yesterday"
        },
        {
            maskedDeviceDisplayId: "ios-88..9b31",
            lastSeenDisplay: "iPhone 15 Pro · Yesterday"
        }
    ]
    readonly property string fallbackCurrentDeviceDisplay: "eb7f..0c0e"
    readonly property int linkedDeviceCountValue: smokeFixtureMode
                                                ? smokeLinkedDevices.length
                                                : Ui.SecurityDisplayStore.linkedDeviceCount
    readonly property string effectiveCurrentDeviceDisplay: currentDeviceDisplay.length > 0
                                                            ? currentDeviceDisplay
                                                            : fallbackCurrentDeviceDisplay
    readonly property string transportHeadline: effectiveTransportHealthy
                                               ? Ui.I18n.t("dialog.securityCenter.transportHealthy")
                                               : Ui.I18n.t("dialog.securityCenter.transportNeedsAttention")
    readonly property string transportDetail: {
        if (smokeFixtureMode) {
            return root.smokeTransportHint
        }
        var detail = Ui.SecurityDisplayStore.connectionSummary()
        if (detail.length > 0) {
            return detail
        }
        return effectiveTransportHealthy
             ? Ui.I18n.t("dialog.securityCenter.transportHealthyHint")
             : Ui.I18n.t("dialog.securityCenter.transportNeedsAttentionHint")
    }
    readonly property string trustHeadline: smokeFixtureMode
                                            ? Ui.I18n.t("dialog.securityCenter.trustReady")
                                            : (gatewayState.length > 0
                                               ? gatewayState
                                               : Ui.I18n.t("dialog.securityCenter.trustReview"))
    readonly property string trustDetail: smokeFixtureMode
                                          ? root.smokeTrustHint
                                          : (gatewayInfo.length > 0
                                             ? gatewayInfo
                                             : Ui.I18n.t("dialog.securityCenter.trustReviewHint"))
    readonly property string serverDetail: smokeFixtureMode
                                           ? root.smokeGatewayHint
                                           : (gatewayInfo.length > 0
                                              ? gatewayInfo
                                              : Ui.I18n.t("dialog.securityCenter.serverHint"))
    readonly property string serverHeadline: smokeFixtureMode
                                             ? root.smokeGatewayStatus
                                             : (gatewayState.length > 0
                                                ? gatewayState
                                                : (gatewayInfo.length > 0
                                                   ? gatewayInfo
                                                   : Ui.I18n.t("dialog.securityCenter.serverTitle")))
    readonly property string linkedDevicesSummary: Ui.I18n.t("dialog.securityCenter.devicesValue").arg(linkedDeviceCountValue)
    readonly property color smokeSummaryBg: Qt.rgba(5 / 255, 150 / 255, 105 / 255, 0.08)
    readonly property color smokeSummaryBorder: Qt.rgba(5 / 255, 150 / 255, 105 / 255, 0.24)
    readonly property color smokeDeviceBg: Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.08)
    readonly property color smokeDeviceBorder: Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.24)
    readonly property color smokeTrustBg: Qt.rgba(245 / 255, 158 / 255, 11 / 255, 0.10)
    readonly property color smokeTrustBorder: Qt.rgba(245 / 255, 158 / 255, 11 / 255, 0.22)
    readonly property color smokeServerBg: Qt.rgba(99 / 255, 102 / 255, 241 / 255, 0.08)
    readonly property color smokeServerBorder: Qt.rgba(99 / 255, 102 / 255, 241 / 255, 0.20)

    function refreshOverview() {
        if (smokeFixtureMode) {
            currentDeviceDisplay = ""
            gatewayState = ""
            gatewayInfo = ""
            return
        }
        Ui.SecurityDisplayStore.refresh()
        currentDeviceDisplay = Ui.SecurityDisplayStore.maskedCurrentDeviceId
        gatewayState = Ui.SecurityDisplayStore.gatewayDisplayState
        gatewayInfo = Ui.SecurityDisplayStore.gatewayDisplayDetail
    }

    function open() {
        refreshOverview()
        visible = true
        raise()
        requestActivate()
    }

    onVisibleChanged: {
        if (visible) {
            refreshOverview()
        }
    }

    background: Rectangle {
        radius: Ui.Style.radiusLarge
        color: Ui.Style.panelBgAlt
        border.color: Ui.Style.borderSubtle
    }

    header: Rectangle {
        height: smokeFixtureMode ? 44 : Ui.Style.topBarHeight
        color: Ui.Style.panelBgAlt
        border.color: Ui.Style.borderSubtle

        DragHandler {
            target: null
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (active && root.startSystemMove) {
                    root.startSystemMove()
                }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: smokeFixtureMode ? root.smokeOuterMargin : Ui.Style.paddingM

            ColumnLayout {
                spacing: smokeFixtureMode ? 0 : 2

                Components.UiText {
                    text: root.title
                    textRole: "subtitle"
                }

                Components.UiText {
                    visible: !smokeFixtureMode
                    text: Ui.I18n.t("dialog.securityCenter.subtitle")
                    textRole: "caption"
                    roleColor: Ui.Style.textMuted
                }
            }

            Item { Layout.fillWidth: true }

            Components.IconButton {
                icon.source: Ui.Style.isDark
                             ? "qrc:/mi/e2ee/ui/icons/close-x.svg"
                             : "qrc:/mi/e2ee/ui/icons/close-x-dark.svg"
                buttonSize: Ui.Style.iconButtonSmall
                iconSize: 14
                Accessible.name: Ui.I18n.t("dialog.addContact.cancel")
                onClicked: root.close()
            }
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: smokeFixtureMode ? root.smokeOuterMargin : Ui.Style.paddingM
        anchors.topMargin: smokeFixtureMode ? root.smokeOuterMargin : Ui.Style.paddingM
        clip: true
        ScrollBar.vertical.policy: smokeFixtureMode ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded

        ColumnLayout {
            width: root.width - (smokeFixtureMode
                                 ? root.smokeOuterMargin * 2
                                 : Ui.Style.paddingM * 2)
            spacing: smokeFixtureMode ? root.smokeGap : Ui.Style.paddingS + 2

            Rectangle {
                Layout.fillWidth: true
                radius: Ui.Style.radiusMedium
                color: Ui.Style.panelBg
                border.color: smokeFixtureMode ? root.smokeSummaryBorder : Ui.Style.borderSubtle
                implicitHeight: (smokeFixtureMode
                                 ? smokeSummaryRow.implicitHeight + root.smokeCardPadding * 2
                                 : summaryRow.implicitHeight + Ui.Style.paddingM * 2)

                RowLayout {
                    id: smokeSummaryRow
                    visible: smokeFixtureMode
                    anchors.fill: parent
                    anchors.margins: root.smokeCardPadding
                    spacing: root.smokeGap

                    Rectangle {
                        Layout.fillWidth: true
                        radius: Ui.Style.radiusMedium
                        color: root.smokeSummaryBg
                        border.color: root.smokeSummaryBorder
                        implicitHeight: smokeTransportColumn.implicitHeight + root.smokeCardPadding * 2

                        ColumnLayout {
                            id: smokeTransportColumn
                            anchors.fill: parent
                            anchors.margins: root.smokeCardPadding
                            spacing: 1

                            Components.UiText {
                                text: Ui.I18n.t("dialog.securityCenter.transportTitle")
                                textRole: "caption"
                                roleColor: Ui.Style.textSecondary
                            }

                            Components.UiText {
                                text: root.transportHeadline
                                textRole: "value_single"
                                roleColor: Ui.Style.textPrimary
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 176
                        radius: Ui.Style.radiusMedium
                        color: root.smokeDeviceBg
                        border.color: root.smokeDeviceBorder
                        implicitHeight: smokeCurrentColumn.implicitHeight + root.smokeCardPadding * 2

                        ColumnLayout {
                            id: smokeCurrentColumn
                            anchors.fill: parent
                            anchors.margins: root.smokeCardPadding
                            spacing: 1

                            Components.UiText {
                                text: Ui.I18n.t("dialog.securityCenter.currentDevice")
                                textRole: "caption"
                                roleColor: Ui.Style.textSecondary
                            }

                            Components.UiText {
                                text: root.effectiveCurrentDeviceDisplay
                                textRole: "value_single"
                                roleColor: Ui.Style.textPrimary
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 132
                        radius: Ui.Style.radiusMedium
                        color: root.smokeTrustBg
                        border.color: root.smokeTrustBorder
                        implicitHeight: smokeCountColumn.implicitHeight + root.smokeCardPadding * 2

                        ColumnLayout {
                            id: smokeCountColumn
                            anchors.fill: parent
                            anchors.margins: root.smokeCardPadding
                            spacing: 1

                            Components.UiText {
                                text: Ui.I18n.t("dialog.securityCenter.devicesTitle")
                                textRole: "caption"
                                roleColor: Ui.Style.textSecondary
                            }

                            Components.UiText {
                                text: root.linkedDevicesSummary
                                textRole: "value_single"
                                roleColor: Ui.Style.textPrimary
                            }
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 156
                        radius: Ui.Style.radiusMedium
                        color: root.smokeServerBg
                        border.color: root.smokeServerBorder
                        implicitHeight: smokeGatewayColumn.implicitHeight + root.smokeCardPadding * 2

                        ColumnLayout {
                            id: smokeGatewayColumn
                            anchors.fill: parent
                            anchors.margins: root.smokeCardPadding
                            spacing: 1

                            Components.UiText {
                                text: Ui.I18n.t("dialog.securityCenter.serverTitle")
                                textRole: "caption"
                                roleColor: Ui.Style.textSecondary
                            }

                            Components.UiText {
                                text: root.smokeGatewayStatus
                                textRole: "value_single"
                                roleColor: Ui.Style.textPrimary
                            }
                        }
                    }
                }

                RowLayout {
                    id: summaryRow
                    visible: !smokeFixtureMode
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingM
                    spacing: Ui.Style.paddingM

                    Rectangle {
                        width: 34
                        height: 34
                        radius: 17
                        color: root.effectiveTransportHealthy
                               ? Qt.rgba(5 / 255, 150 / 255, 105 / 255, 0.14)
                               : Qt.rgba(220 / 255, 38 / 255, 38 / 255, 0.12)

                        Image {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            fillMode: Image.PreserveAspectFit
                            source: root.effectiveTransportHealthy
                                    ? "qrc:/mi/e2ee/ui/icons/check.svg"
                                    : "qrc:/mi/e2ee/ui/icons/info.svg"
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Components.UiText {
                            text: root.transportHeadline
                            textRole: "subtitle"
                            roleColor: Ui.Style.textPrimary
                        }

                        Components.UiText {
                            text: root.transportDetail
                            Layout.fillWidth: true
                            textRole: "detail"
                            roleColor: Ui.Style.textSecondary
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Rectangle {
                                implicitHeight: 20
                                radius: 10
                                color: Ui.Style.railAccentBg
                                border.width: 1
                                border.color: Ui.Style.railAccentBorder
                                implicitWidth: summaryDeviceText.implicitWidth + 12

                                Text {
                                    id: summaryDeviceText
                                    anchors.centerIn: parent
                                    text: root.effectiveCurrentDeviceDisplay
                                    color: Ui.Style.accentSoft
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }
                            }

                            Rectangle {
                                implicitHeight: 20
                                radius: 10
                                color: Ui.Style.topBarPillBg
                                border.width: 1
                                border.color: Ui.Style.topBarPillBorder
                                implicitWidth: linkedDeviceSummaryText.implicitWidth + 12

                                Text {
                                    id: linkedDeviceSummaryText
                                    anchors.centerIn: parent
                                    text: root.linkedDevicesSummary
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 10
                                    font.weight: Font.Medium
                                }
                            }
                        }
                    }
                }
            }

            RowLayout {
                implicitHeight: smokeFixtureMode
                                ? 280
                                : Math.max(devicesCard.implicitHeight, statusCard.implicitHeight)
                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight
                spacing: smokeFixtureMode ? root.smokeGap : Ui.Style.paddingM

                Rectangle {
                    id: devicesCard
                    Layout.preferredWidth: smokeFixtureMode ? 526 : -1
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Ui.Style.radiusMedium
                    color: smokeFixtureMode ? Qt.rgba(239 / 255, 246 / 255, 255 / 255, 0.96) : Ui.Style.panelBg
                    border.color: smokeFixtureMode ? root.smokeDeviceBorder : Ui.Style.borderSubtle
                    implicitHeight: devicesColumn.implicitHeight + (smokeFixtureMode
                                                                    ? root.smokeCardPadding * 2
                                                                    : Ui.Style.paddingM * 2)

                    ColumnLayout {
                        id: devicesColumn
                        anchors.fill: parent
                        anchors.margins: smokeFixtureMode ? root.smokeCardPadding : Ui.Style.paddingM
                        spacing: smokeFixtureMode ? root.smokeGap : Ui.Style.paddingS

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: smokeFixtureMode ? root.smokeGap : Ui.Style.paddingS

                            Components.UiText {
                                text: smokeFixtureMode
                                      ? "Devices and session"
                                      : Ui.I18n.t("dialog.securityCenter.devicesTitle")
                                textRole: smokeFixtureMode ? "value_single" : "subtitle"
                                roleColor: Ui.Style.textPrimary
                            }

                            Item { Layout.fillWidth: true }

                            Rectangle {
                                implicitHeight: 20
                                radius: 10
                                color: Ui.Style.topBarPillBg
                                border.width: 1
                                border.color: Ui.Style.topBarPillBorder
                                implicitWidth: deviceCountText.implicitWidth + 12

                                Text {
                                    id: deviceCountText
                                    anchors.centerIn: parent
                                    text: root.linkedDevicesSummary
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 10
                                    font.weight: Font.Medium
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: Ui.Style.radiusMedium
                            color: smokeFixtureMode ? Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.14) : Ui.Style.panelBgAlt
                            border.color: smokeFixtureMode ? Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.28) : Ui.Style.borderSubtle
                            implicitHeight: currentDeviceColumn.implicitHeight + (smokeFixtureMode
                                                                                  ? root.smokeCardPadding * 2
                                                                                  : Ui.Style.paddingM * 2)

                            ColumnLayout {
                                id: currentDeviceColumn
                                anchors.fill: parent
                                anchors.margins: smokeFixtureMode ? root.smokeCardPadding : Ui.Style.paddingM
                                spacing: 3

                                Components.UiText {
                                    text: Ui.I18n.t("dialog.securityCenter.currentDevice")
                                    textRole: "caption"
                                    roleColor: Ui.Style.textSecondary
                                }

                                Components.UiText {
                                    text: root.effectiveCurrentDeviceDisplay
                                    Layout.fillWidth: true
                                    textRole: "value_single"
                                    roleColor: Ui.Style.textPrimary
                                }

                                Components.UiText {
                                    text: smokeFixtureMode
                                          ? root.smokeCurrentDeviceHint
                                          : Ui.I18n.t("dialog.securityCenter.transportHealthyHint")
                                    Layout.fillWidth: true
                                    textRole: smokeFixtureMode ? "caption" : "detail"
                                    roleColor: Ui.Style.textMuted
                                }
                            }
                        }

                        Rectangle {
                            visible: !smokeFixtureMode
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        Components.UiText {
                            visible: smokeFixtureMode
                            text: "Linked devices"
                            textRole: "caption"
                            roleColor: Ui.Style.textSecondary
                        }

                        Repeater {
                            model: smokeFixtureMode ? Math.min(root.smokeLinkedDevices.length, 3) : 0

                            delegate: Rectangle {
                                Layout.fillWidth: true
                                property var deviceEntry: root.smokeLinkedDevices[index]
                                radius: Ui.Style.radiusMedium
                                implicitHeight: smokeLinkedRow.implicitHeight + root.smokeCardPadding * 2
                                color: index % 2 === 0
                                       ? Qt.rgba(59 / 255, 130 / 255, 246 / 255, 0.10)
                                       : Qt.rgba(14 / 255, 165 / 255, 233 / 255, 0.08)
                                border.color: index % 2 === 0
                                              ? Qt.rgba(59 / 255, 130 / 255, 246 / 255, 0.24)
                                              : Qt.rgba(14 / 255, 165 / 255, 233 / 255, 0.22)

                                RowLayout {
                                    id: smokeLinkedRow
                                    anchors.fill: parent
                                    anchors.margins: root.smokeCardPadding
                                    spacing: root.smokeGap

                                    Rectangle {
                                        width: 28
                                        height: 28
                                        radius: 14
                                        color: index % 2 === 0
                                               ? Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.18)
                                               : Qt.rgba(6 / 255, 182 / 255, 212 / 255, 0.18)

                                        Image {
                                            anchors.centerIn: parent
                                            width: 13
                                            height: 13
                                            fillMode: Image.PreserveAspectFit
                                            source: "qrc:/mi/e2ee/ui/icons/device.svg"
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        Components.UiText {
                                            text: deviceEntry.maskedDeviceDisplayId
                                            textRole: "value_single"
                                            roleColor: Ui.Style.textPrimary
                                        }

                                        Components.UiText {
                                            text: deviceEntry.lastSeenDisplay
                                            textRole: "caption"
                                            roleColor: Ui.Style.textMuted
                                        }
                                    }
                                }
                            }
                        }

                        ListView {
                            visible: !smokeFixtureMode && linkedDeviceCountValue > 0
                            Layout.fillWidth: true
                            Layout.preferredHeight: linkedDeviceCountValue > 0
                                                    ? (smokeFixtureMode
                                                       ? Math.min(contentHeight, 132)
                                                       : Math.min(contentHeight, 184))
                                                    : 0
                            clip: true
                            interactive: false
                            model: smokeFixtureMode
                                   ? root.smokeLinkedDevices
                                   : Ui.SecurityDisplayStore.devicesModel
                            spacing: Ui.Style.paddingS

                            delegate: Rectangle {
                                width: ListView.view.width
                                property var deviceEntry: root.smokeFixtureMode
                                                          ? ((typeof modelData !== "undefined" && modelData)
                                                             ? modelData
                                                             : ({}))
                                                          : model
                                property string deviceDisplayId: deviceEntry &&
                                                                 deviceEntry.maskedDeviceDisplayId
                                                                 ? deviceEntry.maskedDeviceDisplayId
                                                                 : ""
                                property string deviceSeenText: deviceEntry &&
                                                                deviceEntry.lastSeenDisplay
                                                                ? deviceEntry.lastSeenDisplay
                                                                : ""
                                implicitHeight: deviceRow.implicitHeight + (root.smokeFixtureMode
                                                                            ? Ui.Style.paddingS * 2
                                                                            : Ui.Style.paddingM * 2)
                                radius: Ui.Style.radiusMedium
                                color: root.smokeFixtureMode
                                       ? (index % 2 === 0
                                          ? Qt.rgba(59 / 255, 130 / 255, 246 / 255, 0.10)
                                          : Qt.rgba(14 / 255, 165 / 255, 233 / 255, 0.08))
                                       : Ui.Style.panelBgAlt
                                border.color: root.smokeFixtureMode
                                              ? (index % 2 === 0
                                                 ? Qt.rgba(59 / 255, 130 / 255, 246 / 255, 0.24)
                                                 : Qt.rgba(14 / 255, 165 / 255, 233 / 255, 0.22))
                                              : Ui.Style.borderSubtle

                                RowLayout {
                                    id: deviceRow
                                    anchors.fill: parent
                                    anchors.margins: root.smokeFixtureMode ? Ui.Style.paddingS + 2 : Ui.Style.paddingM
                                    spacing: root.smokeFixtureMode ? Ui.Style.paddingS + 2 : Ui.Style.paddingM

                                    Rectangle {
                                        width: root.smokeFixtureMode ? 30 : 34
                                        height: root.smokeFixtureMode ? 30 : 34
                                        radius: width / 2
                                        color: root.smokeFixtureMode
                                               ? (index % 2 === 0
                                                  ? Qt.rgba(37 / 255, 99 / 255, 235 / 255, 0.18)
                                                  : Qt.rgba(6 / 255, 182 / 255, 212 / 255, 0.18))
                                               : Ui.Style.dialogSelectedBg

                                        Image {
                                            anchors.centerIn: parent
                                            width: root.smokeFixtureMode ? 14 : 16
                                            height: root.smokeFixtureMode ? 14 : 16
                                            fillMode: Image.PreserveAspectFit
                                            source: "qrc:/mi/e2ee/ui/icons/device.svg"
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Components.UiText {
                                            text: deviceDisplayId
                                            textRole: "value_single"
                                            roleColor: Ui.Style.textPrimary
                                        }

                                        Components.UiText {
                                            text: deviceSeenText
                                            textRole: "caption"
                                            roleColor: Ui.Style.textMuted
                                        }
                                    }
                                }
                            }
                        }

                        Components.UiText {
                            visible: linkedDeviceCountValue === 0
                            text: Ui.I18n.t("dialog.securityCenter.noLinkedDevices")
                            textRole: "supporting"
                            roleColor: Ui.Style.textMuted
                        }
                    }
                }

                Rectangle {
                    id: statusCard
                    Layout.preferredWidth: smokeFixtureMode ? 314 : 264
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Ui.Style.radiusMedium
                    color: smokeFixtureMode ? Qt.rgba(248 / 255, 250 / 255, 252 / 255, 0.98) : Ui.Style.panelBg
                    border.color: smokeFixtureMode ? Qt.rgba(203 / 255, 213 / 255, 225 / 255, 0.85) : Ui.Style.borderSubtle
                    implicitHeight: statusColumn.implicitHeight + (smokeFixtureMode
                                                                   ? root.smokeCardPadding * 2
                                                                   : Ui.Style.paddingM * 2)

                    ColumnLayout {
                        id: statusColumn
                        anchors.fill: parent
                        anchors.margins: smokeFixtureMode ? root.smokeCardPadding : Ui.Style.paddingM
                        spacing: smokeFixtureMode ? root.smokeGap : Ui.Style.paddingS

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Ui.Style.paddingS

                            Components.UiText {
                                text: smokeFixtureMode
                                      ? (Ui.I18n.usesCjkLocale ? "状态摘要" : "Status summary")
                                      : (Ui.I18n.usesCjkLocale ? "状态摘要" : "Status summary")
                                textRole: smokeFixtureMode ? "caption" : "subtitle"
                                roleColor: Ui.Style.textPrimary
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Rectangle {
                            visible: true
                            Layout.fillWidth: true
                            radius: Ui.Style.radiusMedium
                            color: smokeFixtureMode ? root.smokeSummaryBg : Ui.Style.panelBgAlt
                            border.color: smokeFixtureMode ? root.smokeSummaryBorder : Ui.Style.borderSubtle
                            implicitHeight: smokeTransportInfoColumn.implicitHeight + (smokeFixtureMode
                                                                                        ? root.smokeCardPadding * 2
                                                                                        : Ui.Style.paddingS * 2)

                            ColumnLayout {
                                id: smokeTransportInfoColumn
                                anchors.fill: parent
                                anchors.margins: smokeFixtureMode ? root.smokeCardPadding : Ui.Style.paddingS
                                spacing: smokeFixtureMode ? 2 : 3

                                Components.UiText {
                                    text: Ui.I18n.t("dialog.securityCenter.transportTitle")
                                    textRole: "caption"
                                    roleColor: Ui.Style.textSecondary
                                }

                                Components.UiText {
                                    text: root.transportHeadline
                                    textRole: "value_single"
                                    roleColor: Ui.Style.textPrimary
                                }

                                Components.UiText {
                                    text: root.transportDetail
                                    Layout.fillWidth: true
                                    textRole: smokeFixtureMode ? "caption" : "detail"
                                    roleColor: Ui.Style.textMuted
                                }
                            }
                        }

                        Rectangle {
                            visible: true
                            Layout.fillWidth: true
                            radius: Ui.Style.radiusMedium
                            color: smokeFixtureMode ? root.smokeTrustBg : Ui.Style.panelBgAlt
                            border.color: smokeFixtureMode ? root.smokeTrustBorder : Ui.Style.borderSubtle
                            implicitHeight: smokeTrustColumn.implicitHeight + (smokeFixtureMode
                                                                               ? root.smokeCardPadding * 2
                                                                               : Ui.Style.paddingS * 2)

                            ColumnLayout {
                                id: smokeTrustColumn
                                anchors.fill: parent
                                anchors.margins: smokeFixtureMode ? root.smokeCardPadding : Ui.Style.paddingS
                                spacing: smokeFixtureMode ? 2 : 3

                                Components.UiText {
                                    text: Ui.I18n.t("dialog.securityCenter.trustTitle")
                                    textRole: "caption"
                                    roleColor: Ui.Style.textSecondary
                                }

                                Components.UiText {
                                    text: root.trustHeadline
                                    textRole: "value_single"
                                    roleColor: Ui.Style.textPrimary
                                }

                                Components.UiText {
                                    text: root.trustDetail
                                    Layout.fillWidth: true
                                    textRole: smokeFixtureMode ? "caption" : "detail"
                                    roleColor: Ui.Style.textMuted
                                }
                            }
                        }

                        Rectangle {
                            visible: true
                            Layout.fillWidth: true
                            radius: Ui.Style.radiusMedium
                            color: smokeFixtureMode ? root.smokeServerBg : Ui.Style.panelBgAlt
                            border.color: smokeFixtureMode ? root.smokeServerBorder : Ui.Style.borderSubtle
                            implicitHeight: smokeServerColumn.implicitHeight + (smokeFixtureMode
                                                                                ? root.smokeCardPadding * 2
                                                                                : Ui.Style.paddingS * 2)

                            ColumnLayout {
                                id: smokeServerColumn
                                anchors.fill: parent
                                anchors.margins: smokeFixtureMode ? root.smokeCardPadding : Ui.Style.paddingS
                                spacing: smokeFixtureMode ? 2 : 3

                                Components.UiText {
                                    text: Ui.I18n.t("dialog.securityCenter.serverTitle")
                                    textRole: "caption"
                                    roleColor: Ui.Style.textSecondary
                                }

                                Components.UiText {
                                    text: root.serverHeadline
                                    textRole: "value_single"
                                    roleColor: Ui.Style.textPrimary
                                }

                                Components.UiText {
                                    text: root.serverDetail
                                    Layout.fillWidth: true
                                    textRole: smokeFixtureMode ? "caption" : "detail"
                                    roleColor: Ui.Style.textMuted
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: Ui.Style.radiusMedium
                color: smokeFixtureMode ? Qt.rgba(224 / 255, 242 / 255, 254 / 255, 0.94) : Ui.Style.panelBg
                border.color: smokeFixtureMode ? root.smokeDeviceBorder : Ui.Style.borderSubtle
                implicitHeight: actionRow.implicitHeight + (smokeFixtureMode
                                                            ? root.smokeCardPadding * 2
                                                            : Ui.Style.paddingM * 2)

                RowLayout {
                    id: actionRow
                    anchors.fill: parent
                    anchors.margins: smokeFixtureMode ? root.smokeCardPadding : Ui.Style.paddingM
                    spacing: smokeFixtureMode ? root.smokeGap : Ui.Style.paddingM

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Components.UiText {
                            text: smokeFixtureMode
                                  ? Ui.I18n.t("dialog.securityCenter.manageDevices")
                                  : Ui.I18n.t("dialog.securityCenter.subtitle")
                            textRole: smokeFixtureMode ? "value_single" : "subtitle"
                            roleColor: Ui.Style.textPrimary
                        }

                        Components.UiText {
                            text: smokeFixtureMode
                                  ? root.smokeDevicesHint
                                  : Ui.I18n.t("dialog.securityCenter.devicesHint")
                            Layout.fillWidth: true
                            textRole: smokeFixtureMode ? "caption" : "detail"
                            roleColor: Ui.Style.textSecondary
                        }
                    }

                    Components.PrimaryButton {
                        text: Ui.I18n.t("dialog.securityCenter.manageDevices")
                        Accessible.name: Ui.I18n.t("dialog.securityCenter.manageDevices")
                        Layout.preferredWidth: smokeFixtureMode ? 150 : 148
                        Layout.preferredHeight: smokeFixtureMode ? 30 : 34
                        onClicked: root.requestManageDevices()
                    }
                }
            }
        }
    }
}
