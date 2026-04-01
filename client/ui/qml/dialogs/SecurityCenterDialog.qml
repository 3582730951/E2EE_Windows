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

    property var overviewCards: []
    property string currentDeviceDisplay: ""
    property string gatewayInfo: ""

    function refreshOverview() {
        Ui.SecurityDisplayStore.refresh()
        overviewCards = Ui.SecurityDisplayStore.overviewCards
        currentDeviceDisplay = Ui.SecurityDisplayStore.maskedCurrentDeviceId
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
            spacing: Ui.Style.paddingM

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
                        color: Ui.SecurityDisplayStore.transportHealthy
                               ? Qt.rgba(5 / 255, 150 / 255, 105 / 255, 0.14)
                               : Qt.rgba(220 / 255, 38 / 255, 38 / 255, 0.12)

                        Image {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            fillMode: Image.PreserveAspectFit
                            source: Ui.SecurityDisplayStore.transportHealthy
                                    ? "qrc:/mi/e2ee/ui/icons/check.svg"
                                    : "qrc:/mi/e2ee/ui/icons/info.svg"
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Components.UiText {
                            text: Ui.SecurityDisplayStore.transportHealthy
                                  ? Ui.I18n.t("dialog.securityCenter.transportHealthy")
                                  : Ui.I18n.t("dialog.securityCenter.transportNeedsAttention")
                            textRole: "subtitle"
                            roleColor: Ui.Style.textPrimary
                        }

                        Components.UiText {
                            text: Ui.SecurityDisplayStore.connectionSummary()
                            Layout.fillWidth: true
                            textRole: "detail"
                            roleColor: Ui.Style.textSecondary
                        }
                    }

                    Components.GhostButton {
                        text: Ui.I18n.t("dialog.securityCenter.manageDevices")
                        Accessible.name: Ui.I18n.t("dialog.securityCenter.manageDevices")
                        onClicked: root.requestManageDevices()
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: Ui.Style.paddingM
                columnSpacing: Ui.Style.paddingM

                Repeater {
                    model: root.overviewCards

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 108
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
                                    Layout.fillWidth: true
                                    textRole: "detail"
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

                            Components.UiText {
                                text: Ui.I18n.t("dialog.securityCenter.currentDevice")
                                textRole: "caption"
                                roleColor: Ui.Style.textSecondary
                            }

                            Components.UiText {
                                text: root.currentDeviceDisplay
                                textRole: "value_single"
                                roleColor: Ui.Style.textPrimary
                            }
                        }

                        Components.UiText {
                            text: gatewayInfo
                            textRole: "caption"
                            roleColor: Ui.Style.textMuted
                        }
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(96, Math.min(contentHeight, 220))
                        clip: true
                        model: Ui.SecurityDisplayStore.devicesModel
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
                            }
                        }
                    }

                    Components.UiText {
                        visible: Ui.SecurityDisplayStore.linkedDeviceCount === 0
                        text: Ui.I18n.t("dialog.securityCenter.noLinkedDevices")
                        textRole: "supporting"
                        roleColor: Ui.Style.textMuted
                    }
                }
            }
        }
    }
}
