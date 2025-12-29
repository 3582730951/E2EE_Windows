import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/mi/e2ee/ui/qml/components" as Components

TextField {
    id: field

    Components.InputContextMenu {
        id: contextMenu
        target: field
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onPressed: {
            field.forceActiveFocus()
            contextMenu.popup()
        }
    }
}
