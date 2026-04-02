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

    property string currentDeviceDisplay: ""
    property string gatewayState: ""
    property string gatewayInfo: ""

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

    readonly property bool effectiveTransportHealthy: smokeFixtureMode
                                                     ? true
                                                     : Ui.SecurityDisplayStore.transportHealthy
    readonly property string smokeTransportHint: Ui.I18n.usesCjkLocale
                                                 ? "TLS、会话密钥与轮换状态正常。"
                                                 : "TLS, session keys, and rotation healthy."
    readonly property string smokeTrustHint: Ui.I18n.usesCjkLocale
                                             ? "根信任已就绪，可用于敏感操作。"
                                             : "Root trust ready for sensitive actions."
    readonly property string smokeGatewayStatus: Ui.I18n.usesCjkLocale
                                                 ? "已固定指纹"
                                                 : "Pinned fingerprint"
    readonly property string smokeGatewayHint: Ui.I18n.usesCjkLocale
                                               ? "网关访问维持在安全传输之上。"
                                               : "Gateway access stays on secure transport."
    readonly property string smokeCurrentDeviceHint: Ui.I18n.usesCjkLocale
                                                     ? "当前会话的主端点。"
                                                     : "Primary endpoint for this session."
    readonly property string smokeDevicesSectionTitle: Ui.I18n.usesCjkLocale
                                                       ? "设备与会话"
                                                       : "Devices & session"
    readonly property string smokeDevicesHint: Ui.I18n.usesCjkLocale
                                               ? "审查、移除或追加可信设备。"
                                               : "Review, remove, or link trusted devices."
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
    readonly property var effectiveDevicesModel: smokeFixtureMode
                                                 ? smokeLinkedDevices
                                                 : Ui.SecurityDisplayStore.devicesModel
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
    readonly property string serverHeadline: smokeFixtureMode
                                             ? root.smokeGatewayStatus
                                             : (gatewayState.length > 0
                                                ? gatewayState
                                                : (gatewayInfo.length > 0
                                                   ? gatewayInfo
                                                   : Ui.I18n.t("dialog.securityCenter.serverTitle")))
    readonly property string serverDetail: smokeFixtureMode
                                           ? root.smokeGatewayHint
                                           : (gatewayInfo.length > 0
                                              ? gatewayInfo
                                              : Ui.I18n.t("dialog.securityCenter.serverHint"))
    readonly property string linkedDevicesSummary: Ui.I18n.t("dialog.securityCenter.devicesValue").arg(linkedDeviceCountValue)
    readonly property var statusRows: [
        {
            title: Ui.I18n.t("dialog.securityCenter.transportTitle"),
            value: transportHeadline,
            detail: transportDetail
        },
        {
            title: Ui.I18n.t("dialog.securityCenter.trustTitle"),
            value: trustHeadline,
            detail: trustDetail
        },
        {
            title: Ui.I18n.t("dialog.securityCenter.serverTitle"),
            value: serverHeadline,
            detail: serverDetail
        }
    ]

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
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: root.width - Ui.Style.paddingM * 2
            spacing: Ui.Style.paddingS + 2

            Rectangle {
                Layout.fillWidth: true
                radius: Ui.Style.radiusMedium
                color: Qt.rgba(5 / 255, 150 / 255, 105 / 255, 0.08)
                border.color: Qt.rgba(5 / 255, 150 / 255, 105 / 255, 0.24)
                implicitHeight: 40

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Ui.Style.paddingM
                    anchors.rightMargin: Ui.Style.paddingM
                    spacing: 8

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: root.effectiveTransportHealthy ? Ui.Style.success : Ui.Style.danger
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Components.UiText {
                        text: Ui.I18n.t("dialog.securityCenter.transportTitle")
                        textRole: "caption"
                        roleColor: Ui.Style.textSecondary
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Components.UiText {
                        text: root.transportHeadline
                        textRole: "value_single"
                        roleColor: Ui.Style.textPrimary
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Item { Layout.fillWidth: true }

                    Components.UiText {
                        text: root.effectiveCurrentDeviceDisplay
                        textRole: "caption"
                        roleColor: Ui.Style.textSecondary
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Components.UiText {
                        text: root.linkedDevicesSummary
                        textRole: "caption"
                        roleColor: Ui.Style.textSecondary
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(devicesCard.implicitHeight, sidePanel.implicitHeight)
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
                                text: smokeFixtureMode
                                      ? root.smokeDevicesSectionTitle
                                      : Ui.I18n.t("dialog.securityCenter.devicesTitle")
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
                                    textRole: "detail"
                                    roleColor: Ui.Style.textMuted
                                }
                            }
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: linkedDeviceCountValue > 0
                                                    ? Math.min(contentHeight, 186)
                                                    : 0
                            clip: true
                            interactive: false
                            model: root.effectiveDevicesModel
                            spacing: Ui.Style.paddingS
                            visible: linkedDeviceCountValue > 0

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
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.panelBgAlt
                                border.color: Ui.Style.borderSubtle
                                implicitHeight: 56

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Ui.Style.paddingM
                                    anchors.rightMargin: Ui.Style.paddingM
                                    spacing: Ui.Style.paddingM

                                    Rectangle {
                                        width: 30
                                        height: 30
                                        radius: 15
                                        color: Ui.Style.railAccentBg

                                        Image {
                                            anchors.centerIn: parent
                                            width: 14
                                            height: 14
                                            fillMode: Image.PreserveAspectFit
                                            source: "qrc:/mi/e2ee/ui/icons/device.svg"
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

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
                    id: sidePanel
                    Layout.preferredWidth: 272
                    Layout.fillHeight: true
                    radius: Ui.Style.radiusMedium
                    color: Ui.Style.panelBg
                    border.color: Ui.Style.borderSubtle
                    implicitHeight: sidePanelColumn.implicitHeight + Ui.Style.paddingM * 2

                    ColumnLayout {
                        id: sidePanelColumn
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: 0

                        Repeater {
                            model: root.statusRows

                            delegate: Item {
                                Layout.fillWidth: true
                                implicitHeight: 68

                                ColumnLayout {
                                    anchors.fill: parent
                                    spacing: 2

                                    Item { Layout.fillHeight: true }

                                    Components.UiText {
                                        text: modelData.title
                                        textRole: "caption"
                                        roleColor: Ui.Style.textSecondary
                                    }

                                    Components.UiText {
                                        text: modelData.value
                                        textRole: "value_single"
                                        roleColor: Ui.Style.textPrimary
                                    }

                                    Components.UiText {
                                        text: modelData.detail
                                        textRole: "caption"
                                        roleColor: Ui.Style.textMuted
                                        Layout.fillWidth: true
                                    }

                                    Item { Layout.fillHeight: true }
                                }
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
                            text: Ui.I18n.t("dialog.securityCenter.manageDevices")
                            textRole: "subtitle"
                            roleColor: Ui.Style.textPrimary
                        }

                        Components.UiText {
                            text: smokeFixtureMode
                                  ? root.smokeDevicesHint
                                  : Ui.I18n.t("dialog.securityCenter.devicesHint")
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
