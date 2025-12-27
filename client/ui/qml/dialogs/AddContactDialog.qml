import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Dialog {
    id: root
    modal: true
    width: 360
    height: 320
    title: "Add Contact"
    standardButtons: Dialog.NoButton

    onOpened: {
        nameField.text = ""
        handleField.text = ""
        errorText.visible = false
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
                icon.source: "qrc:/mi/e2ee/ui/icons/close-x-dark.svg"
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
            placeholderText: "Name"
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
            placeholderText: "Phone or username"
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
            text: "Name is required."
            color: Ui.Style.danger
            font.pixelSize: 11
            visible: false
        }
        Item { Layout.fillHeight: true }
        RowLayout {
            Layout.fillWidth: true
            spacing: Ui.Style.paddingS
            Components.GhostButton {
                text: "Cancel"
                Layout.fillWidth: true
                onClicked: root.close()
            }
            Components.PrimaryButton {
                text: "Add"
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
