import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

ToolButton {
    id: root
    property string accessibleName: ""
    property color baseColor: Ui.Style.iconMuted
    property color hoverColor: Ui.Style.textPrimary
    property color pressColor: Ui.Style.textPrimary
    property color bgColor: "transparent"
    property color hoverBg: Ui.Style.hoverBg
    property color pressedBg: Ui.Style.pressedBg
    property int buttonSize: 36
    property int iconSize: 18
    signal rightClicked

    implicitWidth: buttonSize
    implicitHeight: buttonSize
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.role: Accessible.Button
    Accessible.name: root.accessibleName.length > 0
                     ? root.accessibleName
                     : (ToolTip.text ? ToolTip.text : "")
    Accessible.description: ToolTip.text ? ToolTip.text : ""

    icon.width: iconSize
    icon.height: iconSize
    icon.color: !enabled ? Ui.Style.textMuted
                         : (root.down ? root.pressColor : (root.hovered ? root.hoverColor : root.baseColor))
    opacity: enabled ? 1.0 : 0.5

    background: Rectangle {
        radius: Ui.Style.radiusMedium
        color: root.down ? root.pressedBg
                         : (root.activeFocus ? root.hoverBg
                                             : (root.hovered ? root.hoverBg : root.bgColor))
        border.width: root.activeFocus ? 1 : 0
        border.color: Ui.Style.accent
        Behavior on color {
            ColorAnimation { duration: Ui.Style.motionFast }
        }
    }

    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        hoverEnabled: true
        onClicked: root.rightClicked()
    }
}
