import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Accessibility 1.0
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    property var ownerWindow: null
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

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingM

        Components.UiText {
            text: Ui.I18n.t("dialog.deviceManager.currentDevice")
            textRole: "caption"
            roleColor: Ui.Style.textSecondary
        }

        Rectangle {
            id: currentDeviceCard
            Layout.fillWidth: true
            implicitHeight: currentDeviceRow.implicitHeight + Ui.Style.paddingM * 2
            radius: Ui.Style.radiusMedium
            color: Ui.Style.panelBg
            border.color: Ui.Style.borderSubtle
            RowLayout {
                id: currentDeviceRow
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingM
                spacing: Ui.Style.paddingM
                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    color: Ui.Style.avatarColor("device")
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
                        text: Ui.I18n.t("dialog.deviceManager.thisDevice")
                        textRole: "subtitle"
                        roleColor: Ui.Style.textPrimary
                    }
                    Components.UiText {
                        text: Ui.SecurityDisplayStore.maskedCurrentDeviceId.length > 0
                              ? Ui.SecurityDisplayStore.maskedCurrentDeviceId
                              : Ui.I18n.t("dialog.deviceManager.deviceOnline")
                        textRole: "caption"
                        roleColor: Ui.Style.textMuted
                    }
                }
            }
        }

        Components.UiText {
            text: Ui.I18n.t("dialog.deviceManager.linkedDevices")
            textRole: "caption"
            roleColor: Ui.Style.textSecondary
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Ui.Style.paddingS

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                clip: true
                model: Ui.SecurityDisplayStore.devicesModel
                delegate: Rectangle {
                    width: ListView.view.width
                    implicitHeight: linkedDeviceRow.implicitHeight + Ui.Style.paddingM * 2
                    radius: Ui.Style.radiusMedium
                    color: Ui.Style.panelBg
                    border.color: Ui.Style.borderSubtle
                    RowLayout {
                        id: linkedDeviceRow
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: Ui.Style.paddingM
                        Rectangle {
                            width: 36
                            height: 36
                            radius: 18
                            color: Ui.Style.avatarColor(maskedDeviceDisplayId.length > 0
                                                       ? maskedDeviceDisplayId
                                                       : "device")
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
                        Components.GhostButton {
                            text: Ui.I18n.t("dialog.deviceManager.unlink")
                            Layout.alignment: Qt.AlignVCenter
                            Accessible.name: Ui.I18n.t("dialog.deviceManager.unlink")
                            onClicked: {
                                pendingKickId = deviceId
                                pendingKickDisplayId = maskedDeviceDisplayId
                                kickConfirm.open()
                            }
                        }
                    }
                }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
            }

            Components.UiText {
                visible: Ui.SecurityDisplayStore.linkedDeviceCount === 0
                text: Ui.I18n.t("dialog.securityCenter.noLinkedDevices")
                textRole: "supporting"
                roleColor: Ui.Style.textMuted
            }
        }

        Item { Layout.fillHeight: true }
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
