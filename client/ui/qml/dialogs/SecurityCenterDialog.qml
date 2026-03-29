import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    property var ownerWindow: null
    signal requestManageDevices()
    visible: false
    width: 720
    height: 540
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

    property var overviewCards: []
    property ListModel devicesModel: ListModel {}
    property string currentDeviceDisplay: "N/A"
    property string gatewayInfo: ""

    function refreshOverview() {
        devicesModel.clear()

        var linkedCount = 0
        var currentId = clientBridge ? clientBridge.deviceId : ""
        currentDeviceDisplay = (clientBridge && clientBridge.deviceDisplayId.length > 0)
            ? clientBridge.deviceDisplayId
            : "N/A"
        gatewayInfo = clientBridge ? clientBridge.serverInfo() : ""

        var devices = clientBridge ? clientBridge.listDevices() : []
        for (var i = 0; i < devices.length; ++i) {
            var device = devices[i]
            if (currentId && device.deviceId === currentId) {
                continue
            }
            linkedCount += 1
            devicesModel.append({
                deviceDisplayId: device.deviceDisplayId,
                lastSeenSec: device.lastSeenSec || 0
            })
        }

        var transportHealthy = clientBridge ? clientBridge.remoteOk : false
        overviewCards = [
            {
                icon: "qrc:/mi/e2ee/ui/icons/check.svg",
                title: Ui.I18n.t("dialog.securityCenter.transportTitle"),
                value: transportHealthy
                    ? Ui.I18n.t("dialog.securityCenter.transportHealthy")
                    : Ui.I18n.t("dialog.securityCenter.transportNeedsAttention"),
                detail: transportHealthy
                    ? Ui.I18n.t("dialog.securityCenter.transportHealthyHint")
                    : Ui.I18n.t("dialog.securityCenter.transportNeedsAttentionHint")
            },
            {
                icon: "qrc:/mi/e2ee/ui/icons/info.svg",
                title: Ui.I18n.t("dialog.securityCenter.trustTitle"),
                value: transportHealthy
                    ? Ui.I18n.t("dialog.securityCenter.trustReady")
                    : Ui.I18n.t("dialog.securityCenter.trustReview"),
                detail: transportHealthy
                    ? Ui.I18n.t("dialog.securityCenter.trustReadyHint")
                    : Ui.I18n.t("dialog.securityCenter.trustReviewHint")
            },
            {
                icon: "qrc:/mi/e2ee/ui/icons/device.svg",
                title: Ui.I18n.t("dialog.securityCenter.devicesTitle"),
                value: Ui.I18n.t("dialog.securityCenter.devicesValue").arg(linkedCount),
                detail: Ui.I18n.t("dialog.securityCenter.devicesHint")
            },
            {
                icon: "qrc:/mi/e2ee/ui/icons/clock.svg",
                title: Ui.I18n.t("dialog.securityCenter.serverTitle"),
                value: gatewayInfo.length > 0 ? gatewayInfo : "127.0.0.1",
                detail: Ui.I18n.t("dialog.securityCenter.serverHint")
            }
        ]
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

                Text {
                    text: root.title
                    color: Ui.Style.textPrimary
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }

                Text {
                    text: Ui.I18n.t("dialog.securityCenter.subtitle")
                    color: Ui.Style.textMuted
                    font.pixelSize: 11
                }
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

    ScrollView {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        anchors.topMargin: Ui.Style.topBarHeight + Ui.Style.paddingM
        clip: true

        ColumnLayout {
            width: root.width - Ui.Style.paddingM * 2
            spacing: Ui.Style.paddingM

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: Ui.Style.paddingM
                columnSpacing: Ui.Style.paddingM

                Repeater {
                    model: root.overviewCards

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 122
                        radius: Ui.Style.radiusMedium
                        color: Ui.Style.panelBg
                        border.color: Ui.Style.borderSubtle

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingM
                            spacing: Ui.Style.paddingM

                            Rectangle {
                                width: 40
                                height: 40
                                radius: 20
                                color: Ui.Style.dialogSelectedBg

                                Image {
                                    anchors.centerIn: parent
                                    width: 18
                                    height: 18
                                    fillMode: Image.PreserveAspectFit
                                    source: modelData.icon
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3

                                Text {
                                    text: modelData.title
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 11
                                }

                                Text {
                                    text: modelData.value
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                Text {
                                    text: modelData.detail
                                    color: Ui.Style.textMuted
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
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

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingM
                    spacing: Ui.Style.paddingM

                    RowLayout {
                        Layout.fillWidth: true

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: Ui.I18n.t("dialog.securityCenter.currentDevice")
                                color: Ui.Style.textSecondary
                                font.pixelSize: 11
                            }

                            Text {
                                text: root.currentDeviceDisplay
                                color: Ui.Style.textPrimary
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                        }

                        Components.GhostButton {
                            text: Ui.I18n.t("dialog.securityCenter.manageDevices")
                            onClicked: root.requestManageDevices()
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(96, Math.min(contentHeight, 220))
                        clip: true
                        model: devicesModel
                        spacing: Ui.Style.paddingS

                        delegate: Rectangle {
                            width: ListView.view.width
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

                                    Text {
                                        text: deviceDisplayId.length > 0 ? deviceDisplayId : "N/A"
                                        color: Ui.Style.textPrimary
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: lastSeenSec > 0
                                              ? ("Last seen " + lastSeenSec + "s")
                                              : Ui.I18n.t("dialog.securityCenter.transportHealthy")
                                        color: Ui.Style.textMuted
                                        font.pixelSize: 11
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        visible: devicesModel.count === 0
                        text: Ui.I18n.t("dialog.securityCenter.noLinkedDevices")
                        color: Ui.Style.textMuted
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
