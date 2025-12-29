import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/mi/e2ee/ui/qml/components" as Components

TextArea {
    id: area

    Components.InputContextMenu {
        id: contextMenu
        target: area
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onPressed: {
            area.forceActiveFocus()
            contextMenu.popup()
        }
    }
}
