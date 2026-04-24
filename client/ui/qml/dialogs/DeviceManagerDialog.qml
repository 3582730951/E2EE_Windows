import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    property var ownerWindow: null
    readonly property string currentDeviceMaskedId: Ui.SecurityDisplayStore.maskedCurrentDeviceId.length > 0
                                                    ? Ui.SecurityDisplayStore.maskedCurrentDeviceId
                                                    : Ui.I18n.t("dialog.deviceManager.deviceOnline")
    readonly property string currentDeviceCopyValue: Ui.SecurityDisplayStore.currentDeviceCopyValue
    readonly property string linkedDevicesSummary: Ui.SecurityDisplayStore.linkedDevicesSummary
    visible: false
    width: 520
    height: 420
    transientParent: ownerWindow
    flags: Qt.FramelessWindowHint | Qt.Window
    title: Ui.I18n.t("dialog.deviceManager.title")
    color: "transparent"
    font.family: Ui.Style.fontFamily
    palette.window: Ui.Style.windowBg
    palette.base: Ui.Style.panelBgAlt
    palette.button: Ui.Style.panelBgAlt
    palette.text: Ui.Style.textPrimary
    palette.buttonText: Ui.Style.textPrimary
    palette.highlight: Ui.Style.accent
    palette.highlightedText: Ui.Style.textPrimary

    function open() {
        visible = true
        raise()
        requestActivate()
    }

    property string pendingKickId: ""
    property string pendingKickDisplayId: ""

    function refreshDevices() {
        Ui.SecurityDisplayStore.refresh()
    }

    onVisibleChanged: {
        if (visible) {
            refreshDevices()
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
            Text {
                text: root.title
                color: Ui.Style.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
                elide: Text.ElideRight
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
            spacing: Ui.Style.paddingM

            Rectangle {
                Layout.fillWidth: true
                radius: Ui.Style.radiusLarge
                color: Ui.Style.statusSurfaceAlt
                border.width: 1
                border.color: Ui.Style.alpha(Ui.Style.accent, Ui.Style.isDark ? 0.24 : 0.14)
                implicitHeight: deviceHeroColumn.implicitHeight + Ui.Style.paddingM * 2
                clip: true

                Rectangle {
                    x: 1
                    y: 1
                    width: parent.width - 2
                    height: 1
                    color: Ui.Style.heroCardSheen
                    opacity: Ui.Style.isDark ? 0.38 : 0.78
                }

                ColumnLayout {
                    id: deviceHeroColumn
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingM
                    spacing: Ui.Style.paddingS

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingM

                        Components.EmptyStateIllustration {
                            kind: "security"
                            size: 48
                            Layout.alignment: Qt.AlignTop
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Components.UiText {
                                text: root.title
                                textRole: "subtitle"
                                roleColor: Ui.Style.textPrimary
                            }

                            Components.UiText {
                                Layout.fillWidth: true
                                text: Ui.I18n.usesCjkLocale
                                      ? "当前设备与已绑定设备分开，解绑需确认。"
                                      : "Current and linked devices stay separate. Unlink requires confirmation."
                                textRole: "supporting"
                                roleColor: Ui.Style.textSecondary
                            }
                        }
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingS

                        Components.SecurityBadge {
                            labelText: root.currentDeviceMaskedId
                            detailText: Ui.I18n.usesCjkLocale ? "当前设备" : "Current device"
                        }

                        Components.SecurityBadge {
                            labelText: root.linkedDevicesSummary
                            detailText: Ui.I18n.t("dialog.deviceManager.linkedDevices")
                        }
                    }
                }
            }

            Components.UiText {
                text: Ui.I18n.t("dialog.deviceManager.currentDevice")
                textRole: "caption"
                roleColor: Ui.Style.textSecondary
            }

            Rectangle {
                id: currentDeviceCard
                Layout.fillWidth: true
                implicitHeight: currentDeviceColumn.implicitHeight + Ui.Style.paddingM * 2
                radius: Ui.Style.radiusLarge
                color: Ui.Style.panelBg
                border.width: 1
                border.color: Ui.Style.borderSubtle
                clip: true

                Rectangle {
                    x: 1
                    y: 1
                    width: parent.width - 2
                    height: 1
                    color: Ui.Style.heroCardSheen
                    opacity: Ui.Style.isDark ? 0.34 : 0.70
                }

                ColumnLayout {
                    id: currentDeviceColumn
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingM
                    spacing: Ui.Style.paddingS

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingM

                        Components.IdentityAvatar {
                            size: 40
                            titleText: root.currentDeviceMaskedId
                            seedText: root.currentDeviceMaskedId
                            mode: "device"
                            presenceState: "secure"
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Components.UiText {
                                text: Ui.I18n.t("dialog.deviceManager.thisDevice")
                                textRole: "subtitle"
                                roleColor: Ui.Style.textPrimary
                            }

                            Components.UiText {
                                Layout.fillWidth: true
                                text: Ui.I18n.usesCjkLocale
                                      ? "不显示在下方的已绑定设备列表中。"
                                      : "Not shown in the linked-device list below."
                                textRole: "supporting"
                                roleColor: Ui.Style.textMuted
                            }
                        }
                    }

                    Components.DeviceFingerprintRow {
                        Layout.fillWidth: true
                        labelText: Ui.I18n.usesCjkLocale ? "设备标识" : "Device identifier"
                        valueText: root.currentDeviceMaskedId
                        detailText: Ui.I18n.usesCjkLocale
                                    ? "屏幕显示掩码，复制为完整标识。"
                                    : "Masked on screen. Copy gets the full identifier."
                        copyValue: root.currentDeviceCopyValue
                    }
                }
            }

            Components.UiText {
                text: Ui.I18n.t("dialog.deviceManager.linkedDevices")
                textRole: "caption"
                roleColor: Ui.Style.textSecondary
            }

            Rectangle {
                Layout.fillWidth: true
                radius: Ui.Style.radiusLarge
                color: Ui.Style.panelBg
                border.width: 1
                border.color: Ui.Style.borderSubtle
                implicitHeight: linkedDevicesColumn.implicitHeight + Ui.Style.paddingM * 2

                ColumnLayout {
                    id: linkedDevicesColumn
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingM
                    spacing: Ui.Style.paddingS

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingM

                        Components.UiText {
                            text: root.linkedDevicesSummary
                            textRole: "subtitle"
                            roleColor: Ui.Style.textPrimary
                        }

                        Item { Layout.fillWidth: true }

                        Components.SecurityBadge {
                            labelText: Ui.I18n.usesCjkLocale ? "解绑前确认" : "Confirm before unlink"
                            detailText: Ui.I18n.t("dialog.deviceManager.unlink")
                        }
                    }

                    Components.UiText {
                        Layout.fillWidth: true
                        text: Ui.I18n.usesCjkLocale
                              ? "已绑定设备单独列出；复制与解绑分开。"
                              : "Listed separately; copy and unlink are separate."
                        textRole: "supporting"
                        roleColor: Ui.Style.textSecondary
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Ui.SecurityDisplayStore.linkedDeviceCount > 0
                                                ? Math.min(contentHeight, 236)
                                                : 0
                        clip: true
                        spacing: Ui.Style.paddingS
                        model: Ui.SecurityDisplayStore.devicesModel

                        delegate: Rectangle {
                            width: ListView.view.width
                            implicitHeight: linkedDeviceColumn.implicitHeight + Ui.Style.paddingM * 2
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.panelBgAlt
                            border.width: 1
                            border.color: Ui.Style.borderSubtle
                            clip: true

                            ColumnLayout {
                                id: linkedDeviceColumn
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingM
                                spacing: Ui.Style.paddingS

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Ui.Style.paddingM

                                    Components.IdentityAvatar {
                                        size: 36
                                        titleText: maskedDeviceDisplayId
                                        seedText: maskedDeviceDisplayId.length > 0 ? maskedDeviceDisplayId : "device"
                                        mode: "device"
                                        presenceState: "secure"
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Components.UiText {
                                            text: maskedDeviceDisplayId
                                            textRole: "value_single"
                                            roleColor: Ui.Style.textPrimary
                                        }

                                        Components.UiText {
                                            text: lastSeenDisplay
                                            textRole: "caption"
                                            roleColor: Ui.Style.textMuted
                                        }
                                    }

                                    Components.SecurityBadge {
                                        labelText: Ui.I18n.usesCjkLocale ? "已绑定" : "Linked"
                                        detailText: lastSeenDisplay
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Ui.Style.paddingS

                                    Item { Layout.fillWidth: true }

                                    Components.GhostButton {
                                        text: Ui.I18n.t("dialog.notifications.copyId")
                                        Layout.preferredWidth: 86
                                        onClicked: Ui.SecurityDisplayStore.copyToInternalClipboard(copyValue)
                                    }

                                    Components.GhostButton {
                                        text: Ui.I18n.t("dialog.deviceManager.unlink")
                                        Layout.preferredWidth: 86
                                        Accessible.name: Ui.I18n.t("dialog.deviceManager.unlink")
                                        onClicked: {
                                            pendingKickId = deviceId
                                            pendingKickDisplayId = maskedDeviceDisplayId
                                            kickConfirm.open()
                                        }
                                    }
                                }
                            }
                        }
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                    }

                    Rectangle {
                        visible: Ui.SecurityDisplayStore.linkedDeviceCount === 0
                        Layout.fillWidth: true
                        radius: Ui.Style.radiusLarge
                        color: Ui.Style.railAccentBg
                        border.width: 1
                        border.color: Ui.Style.railAccentBorder
                        implicitHeight: emptyStateRow.implicitHeight + Ui.Style.paddingM * 2

                        RowLayout {
                            id: emptyStateRow
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingM
                            spacing: Ui.Style.paddingM

                            Components.EmptyStateIllustration {
                                kind: "security"
                                size: 44
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
                                    Layout.fillWidth: true
                                    text: Ui.I18n.usesCjkLocale
                                          ? "新绑定设备会显示在这里。"
                                          : "New linked devices appear here."
                                    textRole: "supporting"
                                    roleColor: Ui.Style.textMuted
                                }
                            }
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }

    Dialog {
        id: kickConfirm
        modal: true
        focus: true
        title: Ui.I18n.t("dialog.deviceManager.kickTitle")
        width: 360
        implicitWidth: 360
        contentWidth: 320
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: {
            var target = pendingKickId
            pendingKickId = ""
            pendingKickDisplayId = ""
            if (target.length > 0) {
                if (Ui.SecurityDisplayStore.kickDevice(target)) {
                    root.refreshDevices()
                }
            }
        }
        onRejected: {
            pendingKickId = ""
            pendingKickDisplayId = ""
        }
        background: Rectangle {
            radius: Ui.Style.radiusMedium
            color: Ui.Style.panelBgAlt
            border.color: Ui.Style.borderSubtle
        }
        contentItem: Item {
            implicitWidth: 320
            implicitHeight: kickConfirmText.implicitHeight
            width: 320
            Text {
                id: kickConfirmText
                anchors.left: parent.left
                anchors.right: parent.right
                text: Ui.I18n.format("dialog.deviceManager.kickConfirm",
                                     pendingKickDisplayId.length > 0
                                     ? pendingKickDisplayId
                                     : Ui.SecurityDisplayStore.maskedCurrentDeviceId)
                color: Ui.Style.textPrimary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }
    }
}
