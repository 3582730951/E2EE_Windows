import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

ToolButton {
    id: root
    property color baseColor: Ui.Style.textPrimary
    property color hoverColor: Ui.Style.textPrimary
    property color pressColor: Ui.Style.textPrimary
    property color bgColor: "transparent"
    property color hoverBg: Qt.rgba(1, 1, 1, 0.08)
    property color pressedBg: Qt.rgba(1, 1, 1, 0.16)
    property int buttonSize: 44
    property int iconSize: 20

    implicitWidth: buttonSize
    implicitHeight: buttonSize
    hoverEnabled: true

    icon.width: iconSize
    icon.height: iconSize
    icon.color: !enabled ? Ui.Style.textMuted
                         : (root.down ? root.pressColor : (root.hovered ? root.hoverColor : root.baseColor))
    opacity: enabled ? 1.0 : 0.5

    background: Rectangle {
        radius: root.buttonSize / 2
        color: root.down ? root.pressedBg : (root.hovered ? root.hoverBg : root.bgColor)
    }
}
