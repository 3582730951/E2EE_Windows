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
    property string currentDeviceCopyValue: ""
    property string gatewayState: ""
    property string gatewayInfo: ""

    signal requestManageDevices()

    visible: false
    width: smokeFixtureMode ? Ui.SmokeSceneStore.viewportWidth(false) : 700
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
                                                 ? "TLS、会话密钥、轮换正常。"
                                                 : "TLS, keys, rotation healthy."
    readonly property string smokeTrustHint: Ui.I18n.usesCjkLocale
                                             ? "根信任已就绪。"
                                             : "Root trust ready."
    readonly property string smokeGatewayStatus: Ui.I18n.usesCjkLocale
                                                 ? "已固定指纹"
                                                 : "Pinned fingerprint"
    readonly property string smokeGatewayHint: Ui.I18n.usesCjkLocale
                                               ? "网关走安全传输。"
                                               : "Gateway on secure transport."
    readonly property string smokeCurrentDeviceHint: Ui.I18n.usesCjkLocale
                                                     ? "当前会话的主端点。"
                                                     : "Primary endpoint for this session."
    readonly property string smokeDevicesSectionTitle: Ui.I18n.usesCjkLocale
                                                       ? "设备与会话"
                                                       : "Devices & session"
    readonly property string smokeDevicesHint: Ui.I18n.usesCjkLocale
                                               ? "查看、移除或关联可信设备。"
                                               : "Review, remove, or link trusted devices."
    readonly property string currentDeviceDetailText: smokeFixtureMode
                                                      ? smokeCurrentDeviceHint
                                                      : (Ui.I18n.usesCjkLocale
                                                         ? "与已绑定设备分开。"
                                                         : "Separate from linked devices.")
    readonly property string linkedDevicesHintText: smokeFixtureMode
                                                    ? smokeDevicesHint
                                                    : (Ui.I18n.usesCjkLocale
                                                       ? "已绑定设备单独列出，解绑需确认。"
                                                       : "Listed separately. Unlink requires confirmation.")
    readonly property string manageDevicesHintText: Ui.I18n.usesCjkLocale
                                                    ? "复制标识或确认解绑。"
                                                    : "Copy identifiers or confirm unlink."
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
                                             : (gatewayInfo.length > 0
                                                ? gatewayInfo
                                                : Ui.I18n.t("dialog.securityCenter.serverTitle"))
    readonly property string serverDetail: smokeFixtureMode
                                           ? root.smokeGatewayHint
                                           : (gatewayState.length > 0
                                              ? gatewayState
                                              : Ui.I18n.t("dialog.securityCenter.serverHint"))
    readonly property string linkedDevicesSummary: Ui.I18n.t("dialog.securityCenter.devicesValue").arg(linkedDeviceCountValue)
    readonly property var statusRows: [
        {
            title: Ui.I18n.t("dialog.securityCenter.transportTitle"),
            value: transportHeadline,
            detail: transportDetail,
            tone: root.effectiveTransportHealthy ? "healthy" : "blocked"
        },
        {
            title: Ui.I18n.t("dialog.securityCenter.trustTitle"),
            value: trustHeadline,
            detail: trustDetail,
            tone: smokeFixtureMode ? "healthy" : "review"
        },
        {
            title: Ui.I18n.t("dialog.securityCenter.serverTitle"),
            value: serverHeadline,
            detail: serverDetail,
            tone: smokeFixtureMode ? "healthy" : "checking"
        }
    ]

    function refreshOverview() {
        if (smokeFixtureMode) {
            currentDeviceDisplay = ""
            currentDeviceCopyValue = ""
            gatewayState = ""
            gatewayInfo = ""
            return
        }
        Ui.SecurityDisplayStore.refresh()
        currentDeviceDisplay = Ui.SecurityDisplayStore.maskedCurrentDeviceId
        currentDeviceCopyValue = Ui.SecurityDisplayStore.currentDeviceCopyValue
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
                    text: Ui.I18n.usesCjkLocale ? "信任、传输、设备" : "Trust, transport, devices"
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
                radius: Ui.Style.radiusLarge
                color: Ui.Style.statusSurfaceAlt
                border.width: 1
                border.color: Ui.Style.alpha(Ui.Style.accent, Ui.Style.isDark ? 0.24 : 0.14)
                implicitHeight: securityHero.implicitHeight + Ui.Style.paddingM * 2
                clip: true

                Rectangle {
                    width: parent.width * 0.42
                    height: width
                    x: -width * 0.16
                    y: -height * 0.24
                    radius: width / 2
                    color: Ui.Style.alpha(Ui.Style.accent, Ui.Style.isDark ? 0.14 : 0.09)
                }

                Rectangle {
                    x: 1
                    y: 1
                    width: parent.width - 2
                    height: 1
                    color: Ui.Style.heroCardSheen
                    opacity: Ui.Style.isDark ? 0.38 : 0.78
                }

                RowLayout {
                    id: securityHero
                    anchors.fill: parent
                    anchors.leftMargin: Ui.Style.paddingM
                    anchors.rightMargin: Ui.Style.paddingM
                    anchors.topMargin: Ui.Style.paddingM
                    anchors.bottomMargin: Ui.Style.paddingM
                    spacing: Ui.Style.paddingM

                    Components.EmptyStateIllustration {
                        kind: "security"
                        size: 54
                        Layout.alignment: Qt.AlignTop
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Components.UiText {
                            text: Ui.I18n.usesCjkLocale ? "安全概览" : "Trust overview"
                            textRole: "subtitle"
                            roleColor: Ui.Style.textPrimary
                        }

                        Components.UiText {
                            Layout.fillWidth: true
                            text: Ui.I18n.usesCjkLocale
                                  ? "传输、信任、设备分开显示。"
                                  : "Transport, trust, and devices stay separate."
                            textRole: "supporting"
                            roleColor: Ui.Style.textSecondary
                        }

                        Flow {
                            Layout.fillWidth: true
                            spacing: Ui.Style.paddingS

                            Components.SecurityBadge {
                                labelText: root.transportHeadline
                                detailText: Ui.I18n.t("dialog.securityCenter.transportTitle")
                            }

                            Components.SecurityBadge {
                                labelText: root.trustHeadline
                                detailText: Ui.I18n.t("dialog.securityCenter.trustTitle")
                            }

                            Components.SecurityBadge {
                                labelText: root.linkedDevicesSummary
                                detailText: Ui.I18n.t("dialog.securityCenter.devicesTitle")
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Ui.Style.paddingS + 2

                Rectangle {
                    id: devicesCard
                    Layout.fillWidth: true
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

                        Components.UiText {
                            Layout.fillWidth: true
                            text: root.linkedDevicesHintText
                            textRole: "supporting"
                            roleColor: Ui.Style.textSecondary
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
                                spacing: Ui.Style.paddingS

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Ui.Style.paddingM

                                    Components.IdentityAvatar {
                                        size: 36
                                        titleText: root.effectiveCurrentDeviceDisplay
                                        seedText: root.effectiveCurrentDeviceDisplay
                                        mode: "device"
                                        presenceState: root.effectiveTransportHealthy ? "secure" : "busy"
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

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
                                    }
                                }

                                Components.DeviceFingerprintRow {
                                    Layout.fillWidth: true
                                    labelText: Ui.I18n.usesCjkLocale ? "设备标识" : "Device identifier"
                                    valueText: root.effectiveCurrentDeviceDisplay
                                    detailText: root.currentDeviceDetailText
                                    copyValue: smokeFixtureMode ? "" : root.currentDeviceCopyValue
                                }
                            }
                        }

                        Components.UiText {
                            visible: linkedDeviceCountValue > 0
                            text: Ui.I18n.t("dialog.deviceManager.linkedDevices")
                            textRole: "caption"
                            roleColor: Ui.Style.textSecondary
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: linkedDeviceCountValue > 0
                                                    ? Math.min(contentHeight, 172)
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
                                implicitHeight: linkedDeviceRow.implicitHeight + Ui.Style.paddingM * 2

                                RowLayout {
                                    id: linkedDeviceRow
                                    anchors.fill: parent
                                    anchors.leftMargin: Ui.Style.paddingM
                                    anchors.rightMargin: Ui.Style.paddingM
                                    spacing: Ui.Style.paddingM

                                    Rectangle {
                                        width: 30
                                        height: 30
                                        radius: 15
                                        color: "transparent"

                                        Components.IdentityAvatar {
                                            anchors.fill: parent
                                            size: 30
                                            titleText: deviceDisplayId
                                            seedText: deviceDisplayId
                                            mode: "device"
                                            presenceState: "secure"
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

                                    Components.SecurityBadge {
                                        labelText: Ui.I18n.usesCjkLocale ? "已绑定" : "Linked"
                                        detailText: deviceSeenText
                                    }
                                }
                            }
                        }

                        Rectangle {
                            visible: linkedDeviceCountValue === 0
                            Layout.fillWidth: true
                            radius: Ui.Style.radiusMedium
                            color: Ui.Style.railAccentBg
                            border.width: 1
                            border.color: Ui.Style.railAccentBorder
                            implicitHeight: emptyDevicesRow.implicitHeight + Ui.Style.paddingM * 2

                            RowLayout {
                                id: emptyDevicesRow
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingM
                                spacing: Ui.Style.paddingM

                                Components.EmptyStateIllustration {
                                    kind: "security"
                                    size: 40
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Components.UiText {
                                        text: Ui.I18n.t("dialog.securityCenter.noLinkedDevices")
                                        textRole: "value_single"
                                        roleColor: Ui.Style.textPrimary
                                    }

                                    Components.UiText {
                                        text: root.smokeFixtureMode
                                              ? root.smokeDevicesHint
                                              : (Ui.I18n.usesCjkLocale
                                                 ? "新设备会在这里出现。"
                                                 : "New trusted devices appear here.")
                                        textRole: "detail"
                                        roleColor: Ui.Style.textMuted
                                    }
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    id: sidePanel
                    Layout.fillWidth: true
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

                            delegate: Components.SecurityStateStrip {
                                Layout.fillWidth: true
                                Layout.bottomMargin: Ui.Style.paddingS
                                titleText: modelData.title
                                valueText: modelData.value
                                detailText: modelData.detail
                                tone: modelData.tone
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
                    spacing: Ui.Style.paddingS

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Components.UiText {
                            text: Ui.I18n.t("dialog.securityCenter.manageDevices")
                            textRole: "subtitle"
                            roleColor: Ui.Style.textPrimary
                        }

                        Components.UiText {
                            Layout.fillWidth: true
                            text: root.manageDevicesHintText
                            textRole: "supporting"
                            roleColor: Ui.Style.textSecondary
                        }
                    }

                    Components.PrimaryButton {
                        text: Ui.I18n.t("dialog.securityCenter.manageDevices")
                        Accessible.name: Ui.I18n.t("dialog.securityCenter.manageDevices")
                        Layout.preferredWidth: 136
                        Layout.preferredHeight: 34
                        onClicked: root.requestManageDevices()
                    }
                }
            }
        }
    }
}
