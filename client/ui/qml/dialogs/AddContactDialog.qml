import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    property bool hasRequests: Ui.AppStore.friendRequestsModel.count > 0
    visible: false
    width: 360
    height: hasRequests ? 420 : 320
    flags: Qt.FramelessWindowHint | Qt.Window
    title: Ui.I18n.t("dialog.addContact.title")
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

    onVisibleChanged: {
        if (visible) {
            nameField.text = ""
            handleField.text = ""
            errorText.visible = false
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

        TextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: Ui.I18n.t("dialog.addContact.name")
            font.pixelSize: 13
            color: Ui.Style.textPrimary
            placeholderTextColor: Ui.Style.textMuted
            background: Rectangle {
                radius: Ui.Style.radiusMedium
                color: Ui.Style.inputBg
                border.color: nameField.activeFocus ? Ui.Style.inputFocus : Ui.Style.inputBorder
            }
        }
        TextField {
            id: handleField
            Layout.fillWidth: true
            placeholderText: Ui.I18n.t("dialog.addContact.handle")
            font.pixelSize: 13
            color: Ui.Style.textPrimary
            placeholderTextColor: Ui.Style.textMuted
            background: Rectangle {
                radius: Ui.Style.radiusMedium
                color: Ui.Style.inputBg
                border.color: handleField.activeFocus ? Ui.Style.inputFocus : Ui.Style.inputBorder
            }
        }
        Text {
            id: errorText
            text: Ui.I18n.t("dialog.addContact.errorName")
            color: Ui.Style.danger
            font.pixelSize: 11
            visible: false
        }

        ColumnLayout {
            id: requestsBlock
            Layout.fillWidth: true
            visible: Ui.AppStore.friendRequestsModel.count > 0
            spacing: Ui.Style.paddingS

            Text {
                text: Ui.I18n.t("dialog.addContact.requestsTitle")
                color: Ui.Style.textSecondary
                font.pixelSize: 12
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 140
                radius: Ui.Style.radiusMedium
                color: Ui.Style.inputBg
                border.color: Ui.Style.borderSubtle

                ListView {
                    id: requestsList
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingS
                    model: Ui.AppStore.friendRequestsModel
                    clip: true
                    spacing: Ui.Style.paddingS

                    delegate: RowLayout {
                        width: requestsList.width
                        spacing: Ui.Style.paddingS

                        Text {
                            text: username
                            color: Ui.Style.textPrimary
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Components.GhostButton {
                            text: Ui.I18n.t("dialog.addContact.reject")
                            Layout.preferredWidth: 52
                            height: 24
                            onClicked: Ui.AppStore.respondFriendRequest(username, false)
                        }

                        Components.PrimaryButton {
                            text: Ui.I18n.t("dialog.addContact.accept")
                            Layout.preferredWidth: 52
                            height: 24
                            onClicked: Ui.AppStore.respondFriendRequest(username, true)
                        }
                    }
                }
            }
        }
        Item { Layout.fillHeight: true }
        RowLayout {
            Layout.fillWidth: true
            spacing: Ui.Style.paddingS
            Components.GhostButton {
                text: Ui.I18n.t("dialog.addContact.cancel")
                Layout.fillWidth: true
                onClicked: root.close()
            }
            Components.PrimaryButton {
                text: Ui.I18n.t("dialog.addContact.add")
                Layout.fillWidth: true
                onClicked: {
                    if (Ui.AppStore.addContact(nameField.text, handleField.text)) {
                        nameField.text = ""
                        handleField.text = ""
                        errorText.visible = false
                        root.close()
                    } else {
                        errorText.visible = true
                    }
                }
            }
        }
    }
}
