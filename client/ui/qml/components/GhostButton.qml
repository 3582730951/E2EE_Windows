import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Button {
    id: root
    property color textColor: Ui.Style.textPrimary
    property color borderColor: Ui.Style.borderStrong
    property color hoverBg: Ui.Style.hoverBg
    property color pressedBg: Ui.Style.pressedBg

    background: Rectangle {
        radius: Ui.Style.radiusMedium
        border.color: root.borderColor
        border.width: 1
        color: root.down ? root.pressedBg : (root.hovered ? root.hoverBg : "transparent")
    }
    contentItem: Text {
        text: root.text
        color: root.textColor
        font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
