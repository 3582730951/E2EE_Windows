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
            return Ui.I18n.t("dialog.securityCenter.transportHealthyHint")
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
                                          ? Ui.I18n.t("dialog.securityCenter.trustReadyHint")
                                          : (gatewayInfo.length > 0
                                             ? gatewayInfo
                                             : Ui.I18n.t("dialog.securityCenter.trustReviewHint"))
    readonly property string serverDetail: smokeFixtureMode
                                           ? Ui.I18n.t("dialog.securityCenter.serverHint")
                                           : (gatewayInfo.length > 0
                                              ? gatewayInfo
                                              : Ui.I18n.t("dialog.securityCenter.serverHint"))
    readonly property string linkedDevicesSummary: Ui.I18n.t("dialog.securityCenter.devicesValue").arg(linkedDeviceCountValue)

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
        height: Ui.Style.topBarHeight
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
            anchors.margins: Ui.Style.paddingM

            ColumnLayout {
                spacing: 2

                Components.UiText {
                    text: root.title
                    textRole: "subtitle"
                }

                Components.UiText {
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
        anchors.margins: Ui.Style.paddingM
        anchors.topMargin: Ui.Style.topBarHeight + Ui.Style.paddingM
        clip: true

        ColumnLayout {
            width: root.width - Ui.Style.paddingM * 2
            spacing: Ui.Style.paddingS + 2

            Rectangle {
                Layout.fillWidth: true
                radius: Ui.Style.radiusMedium
                color: Ui.Style.panelBg
                border.color: Ui.Style.borderSubtle
                implicitHeight: summaryRow.implicitHeight + Ui.Style.paddingM * 2

                RowLayout {
                    id: summaryRow
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
                implicitHeight: Math.max(devicesCard.implicitHeight, statusCard.implicitHeight)
                Layout.fillWidth: true
                spacing: Ui.Style.paddingM

                Rectangle {
                    id: devicesCard
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Ui.Style.radiusMedium
                    color: Ui.Style.panelBg
                    border.color: Ui.Style.borderSubtle
                    implicitHeight: devicesColumn.implicitHeight + Ui.Style.paddingM * 2

                    ColumnLayout {
                        id: devicesColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: Ui.Style.paddingS

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Ui.Style.paddingS

                            Components.UiText {
                                text: Ui.I18n.t("dialog.securityCenter.devicesTitle")
                                textRole: "subtitle"
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
                            color: Ui.Style.panelBgAlt
                            border.color: Ui.Style.borderSubtle
                            implicitHeight: currentDeviceColumn.implicitHeight + Ui.Style.paddingM * 2

                            ColumnLayout {
                                id: currentDeviceColumn
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingM
                                spacing: 4

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
                                          ? "Primary endpoint · trusted this session"
                                          : Ui.I18n.t("dialog.securityCenter.transportHealthyHint")
                                    Layout.fillWidth: true
                                    textRole: "detail"
                                    roleColor: Ui.Style.textMuted
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: linkedDeviceCountValue > 0
                                                    ? Math.min(contentHeight, 184)
                                                    : 0
                            clip: true
                            interactive: false
                            visible: linkedDeviceCountValue > 0
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
                                implicitHeight: deviceRow.implicitHeight + Ui.Style.paddingM * 2
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.panelBgAlt
                                border.color: Ui.Style.borderSubtle

                                RowLayout {
                                    id: deviceRow
                                    anchors.fill: parent
                                    anchors.margins: Ui.Style.paddingM
                                    spacing: Ui.Style.paddingM

                                    Rectangle {
                                        width: 34
                                        height: 34
                                        radius: 17
                                        color: Ui.Style.dialogSelectedBg

                                        Image {
                                            anchors.centerIn: parent
                                            width: 16
                                            height: 16
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
                    Layout.preferredWidth: 264
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: Ui.Style.radiusMedium
                    color: Ui.Style.panelBg
                    border.color: Ui.Style.borderSubtle
                    implicitHeight: statusColumn.implicitHeight + Ui.Style.paddingM * 2

                    ColumnLayout {
                        id: statusColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: Ui.Style.paddingS

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Ui.Style.paddingS

                            Rectangle {
                                Layout.fillWidth: true
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.panelBgAlt
                                border.color: Ui.Style.borderSubtle
                                implicitHeight: trustMetricColumn.implicitHeight + Ui.Style.paddingS * 2

                                ColumnLayout {
                                    id: trustMetricColumn
                                    anchors.fill: parent
                                    anchors.margins: Ui.Style.paddingS
                                    spacing: 2

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
                                Layout.fillWidth: true
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.panelBgAlt
                                border.color: Ui.Style.borderSubtle
                                implicitHeight: deviceMetricColumn.implicitHeight + Ui.Style.paddingS * 2

                                ColumnLayout {
                                    id: deviceMetricColumn
                                    anchors.fill: parent
                                    anchors.margins: Ui.Style.paddingS
                                    spacing: 2

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
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

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
                                textRole: "detail"
                                roleColor: Ui.Style.textMuted
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

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
                                textRole: "detail"
                                roleColor: Ui.Style.textMuted
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Ui.Style.borderSubtle
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Components.UiText {
                                text: Ui.I18n.t("dialog.securityCenter.serverTitle")
                                textRole: "caption"
                                roleColor: Ui.Style.textSecondary
                            }

                            Components.UiText {
                                text: root.serverDetail
                                Layout.fillWidth: true
                                textRole: "detail"
                                roleColor: Ui.Style.textMuted
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                radius: Ui.Style.radiusMedium
                color: Ui.Style.panelBg
                border.color: Ui.Style.borderSubtle
                implicitHeight: actionRow.implicitHeight + Ui.Style.paddingM * 2

                RowLayout {
                    id: actionRow
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingM
                    spacing: Ui.Style.paddingM

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Components.UiText {
                            text: Ui.I18n.t("dialog.securityCenter.subtitle")
                            textRole: "subtitle"
                            roleColor: Ui.Style.textPrimary
                        }

                        Components.UiText {
                            text: Ui.I18n.t("dialog.securityCenter.devicesHint")
                            Layout.fillWidth: true
                            textRole: "detail"
                            roleColor: Ui.Style.textSecondary
                        }
                    }

                    Components.PrimaryButton {
                        text: Ui.I18n.t("dialog.securityCenter.manageDevices")
                        Accessible.name: Ui.I18n.t("dialog.securityCenter.manageDevices")
                        Layout.preferredWidth: 148
                        Layout.preferredHeight: 34
                        onClicked: root.requestManageDevices()
                    }
                }
            }
        }
    }
}
