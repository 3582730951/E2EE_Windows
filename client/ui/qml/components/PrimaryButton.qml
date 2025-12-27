import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Button {
    id: root
    property color fill: Ui.Style.accent
    property color fillHover: Ui.Style.accentHover
    property color fillPressed: Ui.Style.accentPressed
    property color fillDisabled: Ui.Style.pressedBg
    property color textColor: Ui.Style.textPrimary

    background: Rectangle {
        radius: Ui.Style.radiusMedium
        color: root.enabled
               ? (root.down ? root.fillPressed : (root.hovered ? root.fillHover : root.fill))
               : root.fillDisabled
    }
    contentItem: Text {
        text: root.text
        color: root.textColor
        font.pixelSize: 13
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
