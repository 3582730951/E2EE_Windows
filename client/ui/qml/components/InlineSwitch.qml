import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Control {
    id: root

    property bool checked: false
    signal toggled(bool checked)

    implicitWidth: 42
    implicitHeight: 26
    hoverEnabled: true

    background: Rectangle {
        radius: height / 2
        color: root.checked ? Ui.Style.accent : Ui.Style.topBarPillBg
        border.width: 1
        border.color: root.checked
                      ? Ui.Style.alpha(Ui.Style.accentPressed, 0.35)
                      : Ui.Style.topBarPillBorder

        Rectangle {
            width: 18
            height: 18
            radius: 9
            y: 3
            x: root.checked ? parent.width - width - 3 : 3
            color: "#FFFFFF"
            border.width: 1
            border.color: root.checked
                          ? Ui.Style.alpha(Ui.Style.accentPressed, 0.18)
                          : Ui.Style.alpha(Ui.Style.textMuted, 0.16)

            Behavior on x {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }

    contentItem: Item {}

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
