import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: root

    property string kind: "chat"
    property int size: 72
    readonly property color toneColor: {
        switch (kind) {
        case "calls":
            return Ui.Style.warning
        case "settings":
            return Ui.Style.textSecondary
        case "security":
            return Ui.Style.accent
        default:
            return Ui.Style.accentSoft
        }
    }
    readonly property color toneBorder: Ui.Style.alpha(toneColor, Ui.Style.isDark ? 0.32 : 0.18)
    readonly property color toneSurface: Ui.Style.alpha(toneColor, Ui.Style.isDark ? 0.16 : 0.09)
    readonly property color toneGlow: Ui.Style.alpha(Qt.lighter(toneColor, 1.14), Ui.Style.isDark ? 0.26 : 0.16)

    readonly property string iconSource: {
        switch (kind) {
        case "calls":
            return "qrc:/mi/e2ee/ui/icons/phone.svg"
        case "settings":
            return "qrc:/mi/e2ee/ui/icons/settings.svg"
        case "security":
            return "qrc:/mi/e2ee/ui/icons/device.svg"
        default:
            return "qrc:/mi/e2ee/ui/icons/chat.svg"
        }
    }

    width: size
    height: size

    Rectangle {
        width: root.size
        height: root.size
        radius: root.size / 2
        color: root.toneSurface
        border.width: 1
        border.color: root.toneBorder
    }

    Rectangle {
        width: root.size * 0.70
        height: width
        radius: width / 2
        x: root.size * 0.05
        y: root.size * 0.10
        color: root.toneGlow
    }

    Rectangle {
        width: root.size * 0.18
        height: width
        radius: width / 2
        x: root.size * 0.68
        y: root.size * 0.18
        color: Ui.Style.alpha(root.toneColor, Ui.Style.isDark ? 0.34 : 0.22)
    }

    Rectangle {
        width: root.size * 0.12
        height: width
        radius: width / 2
        x: root.size * 0.16
        y: root.size * 0.68
        color: Ui.Style.alpha(root.toneColor, Ui.Style.isDark ? 0.22 : 0.14)
    }

    Rectangle {
        width: root.size * 0.44
        height: width
        radius: root.size * 0.16
        x: root.size * 0.18
        y: root.size * 0.22
        rotation: -12
        color: root.toneSurface
        border.width: 1
        border.color: root.toneBorder
        transformOrigin: Item.Center
    }

    Rectangle {
        width: root.size * 0.50
        height: width
        radius: root.size * 0.18
        x: root.size * 0.28
        y: root.size * 0.24
        color: Ui.Style.iconWellBg
        border.width: 1
        border.color: Ui.Style.iconWellBorder

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Math.max(0, parent.radius - 1)
            color: Ui.Style.iconWellOverlay
        }

        Rectangle {
            width: parent.width * 0.48
            height: width
            radius: width / 2
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: parent.height * 0.16
            color: Ui.Style.alpha(root.toneColor, Ui.Style.isDark ? 0.22 : 0.12)
        }
    }

    Image {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.size * 0.02
        width: root.size * 0.22
        height: width
        source: root.iconSource
        fillMode: Image.PreserveAspectFit
        smooth: true
        antialiasing: true
    }
}
