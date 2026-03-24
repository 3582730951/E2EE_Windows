import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
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

    property ListModel devicesModel: ListModel {}
    property string pendingKickId: ""
    property string pendingKickDisplayId: ""

    function refreshDevices() {
        devicesModel.clear()
        if (!clientBridge) {
            return
        }
        var list = clientBridge.listDevices()
        var currentId = clientBridge.deviceId
        for (var i = 0; i < list.length; ++i) {
            var d = list[i]
            if (currentId && d.deviceId === currentId) {
                continue
            }
            devicesModel.append({
                deviceId: d.deviceId,
                deviceDisplayId: d.deviceDisplayId,
                lastSeenSec: d.lastSeenSec || 0
            })
        }
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
            }
            Item { Layout.fillWidth: true }
            Components.IconButton {
                icon.source: Ui.Style.isDark
                             ? "qrc:/mi/e2ee/ui/icons/close-x.svg"
                             : "qrc:/mi/e2ee/ui/icons/close-x-dark.svg"
                buttonSize: Ui.Style.iconButtonSmall
                iconSize: 14
                onClicked: root.close()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingM

        Text {
            text: Ui.I18n.t("dialog.deviceManager.currentDevice")
            color: Ui.Style.textSecondary
            font.pixelSize: 12
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
                    Text {
                        anchors.centerIn: parent
                        text: "PC"
                        color: Ui.Style.textPrimary
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: Ui.I18n.t("dialog.deviceManager.thisDevice")
                        color: Ui.Style.textPrimary
                        font.pixelSize: 13
                    }
                    Text {
                        text: (clientBridge && clientBridge.deviceDisplayId.length > 0)
                              ? ("ID: " + clientBridge.deviceDisplayId)
                              : Ui.I18n.t("dialog.deviceManager.deviceOnline")
                        color: Ui.Style.textMuted
                        font.pixelSize: 11
                    }
                }
            }
        }

        Text {
            text: Ui.I18n.t("dialog.deviceManager.linkedDevices")
            color: Ui.Style.textSecondary
            font.pixelSize: 12
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Ui.Style.paddingS

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                clip: true
                model: devicesModel
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
                            color: Ui.Style.avatarColor(deviceDisplayId.length > 0
                                                       ? deviceDisplayId
                                                       : "device")
                            Text {
                                anchors.centerIn: parent
                                text: "LD"
                                color: Ui.Style.textPrimary
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: deviceDisplayId.length > 0 ? deviceDisplayId : "N/A"
                                color: Ui.Style.textPrimary
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                            Text {
                                text: lastSeenSec > 0
                                      ? ("Last seen " + lastSeenSec + "s")
                                      : Ui.I18n.t("dialog.deviceManager.deviceOnline")
                                color: Ui.Style.textMuted
                                font.pixelSize: 11
                            }
                        }
                        Components.GhostButton {
                            text: Ui.I18n.t("dialog.deviceManager.unlink")
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: {
                                pendingKickId = deviceId
                                pendingKickDisplayId =
                                    deviceDisplayId.length > 0 ? deviceDisplayId : "N/A"
                                kickConfirm.open()
                            }
                        }
                    }
                }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
            }

            Text {
                visible: devicesModel.count === 0
                text: Ui.I18n.t("chat.empty")
                color: Ui.Style.textMuted
                font.pixelSize: 12
            }
        }

        Item { Layout.fillHeight: true }
    }

    Dialog {
        id: kickConfirm
        modal: true
        focus: true
        title: Ui.I18n.t("dialog.deviceManager.kickTitle")
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: {
            var target = pendingKickId
            pendingKickId = ""
            pendingKickDisplayId = ""
            if (clientBridge && target.length > 0) {
                if (clientBridge.kickDevice(target)) {
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
        contentItem: ColumnLayout {
            spacing: Ui.Style.paddingS
            Text {
                text: Ui.I18n.format("dialog.deviceManager.kickConfirm",
                                     pendingKickDisplayId.length > 0
                                     ? pendingKickDisplayId
                                     : "N/A")
                color: Ui.Style.textPrimary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
