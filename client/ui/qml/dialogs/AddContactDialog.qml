import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Dialog {
    id: root
    modal: true
    width: 360
    height: 320
    title: "Add Contact"
    standardButtons: Dialog.Close

    onOpened: {
        nameField.text = ""
        handleField.text = ""
        errorText.visible = false
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingM

        TextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: "Name"
        }
        TextField {
            id: handleField
            Layout.fillWidth: true
            placeholderText: "Phone or username"
        }
        Text {
            id: errorText
            text: "Name is required."
            color: Ui.Style.danger
            font.pixelSize: 11
            visible: false
        }
        Item { Layout.fillHeight: true }
        Button {
            text: "Add"
            Layout.fillWidth: true
            background: Rectangle {
                radius: Ui.Style.radiusMedium
                color: Ui.Style.accent
            }
            contentItem: Text {
                text: "Add"
                color: Ui.Style.textPrimary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
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
