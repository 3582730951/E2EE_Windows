import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: root

    property string titleText: ""
    property string seedText: titleText
    property string mode: "person"
    property string presenceState: ""
    property int size: 40
    property bool selected: false

    readonly property color baseColor: Ui.Style.avatarColor((seedText || titleText || mode || "avatar") + ":" + mode)
    readonly property color accentColor: Qt.lighter(baseColor, Ui.Style.isDark ? 1.12 : 1.2)
    readonly property color textColor: {
        var c = root.baseColor
        var luminance = 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b
        return luminance > 0.56 ? "#0F172A" : "#F8FBFF"
    }
    readonly property color badgeColor: {
        switch (presenceState) {
        case "online":
            return Ui.Style.tgOnlineDot
        case "typing":
            return Ui.Style.warning
        case "muted":
            return Ui.Style.tgMutedBadge
        case "busy":
            return Ui.Style.danger
        case "secure":
            return Ui.Style.accent
        default:
            return "transparent"
        }
    }

    function initials() {
        var source = (titleText || "").trim()
        if (source.length === 0) {
            return ""
        }
        var parts = source.split(/\s+/)
        if (parts.length > 1) {
            return (parts[0].charAt(0) + parts[1].charAt(0)).toUpperCase()
        }
        return source.slice(0, Math.min(2, source.length)).toUpperCase()
    }

    width: size
    height: size
    implicitWidth: size
    implicitHeight: size

    Rectangle {
        id: avatarShell
        anchors.fill: parent
        radius: root.mode === "group" ? Ui.Style.radiusMedium : width / 2
        border.width: 1
        border.color: root.selected
                      ? Ui.Style.alpha(Ui.Style.accent, Ui.Style.isDark ? 0.50 : 0.34)
                      : Ui.Style.avatarBorder
        clip: true
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.lighter(root.baseColor, Ui.Style.isDark ? 1.06 : 1.16)
            }
            GradientStop {
                position: 1.0
                color: Qt.darker(root.baseColor, Ui.Style.isDark ? 1.08 : 1.10)
            }
        }

        Rectangle {
            width: root.size * 0.74
            height: width
            x: -root.size * 0.10
            y: -root.size * 0.22
            radius: width / 2
            color: Ui.Style.avatarHalo
        }

        Rectangle {
            width: root.size * 0.42
            height: width
            x: root.size * 0.54
            y: root.size * 0.54
            radius: width / 2
            color: Ui.Style.alpha(root.accentColor, Ui.Style.isDark ? 0.22 : 0.16)
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 2
            radius: Math.max(0, parent.radius - 2)
            color: Ui.Style.avatarInset
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Qt.rgba(1, 1, 1, Ui.Style.isDark ? 0.04 : 0.08)
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            border.width: 1
            border.color: Ui.Style.alpha(Qt.lighter(root.baseColor, 1.12), Ui.Style.isDark ? 0.16 : 0.10)
            color: "transparent"
        }
    }

    Item {
        anchors.fill: parent
        visible: root.mode === "group"

        Rectangle {
            width: root.size * 0.44
            height: width
            radius: width / 2
            x: root.size * 0.12
            y: root.size * 0.18
            color: Qt.lighter(root.baseColor, 1.24)
            border.width: 1
            border.color: Ui.Style.alpha(Qt.lighter(root.baseColor, 1.4), Ui.Style.isDark ? 0.18 : 0.12)
        }

        Rectangle {
            width: root.size * 0.34
            height: width
            radius: width / 2
            x: root.size * 0.44
            y: root.size * 0.22
            color: Qt.darker(root.baseColor, 1.10)
            border.width: 1
            border.color: Ui.Style.alpha(Qt.darker(root.baseColor, 1.16), Ui.Style.isDark ? 0.18 : 0.12)
        }

        Rectangle {
            width: root.size * 0.30
            height: width
            radius: width / 2
            x: root.size * 0.30
            y: root.size * 0.48
            color: Qt.lighter(root.baseColor, 1.32)
            border.width: 1
            border.color: Ui.Style.alpha(Qt.lighter(root.baseColor, 1.4), Ui.Style.isDark ? 0.18 : 0.12)
        }

        Rectangle {
            width: root.size * 0.28
            height: width
            radius: width / 2
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: root.size * 0.08
            anchors.bottomMargin: root.size * 0.08
            color: Ui.Style.iconWellBg
            border.width: 1
            border.color: Ui.Style.iconWellBorder

            Image {
                anchors.centerIn: parent
                width: parent.width * 0.52
                height: width
                source: "qrc:/mi/e2ee/ui/icons/group.svg"
                fillMode: Image.PreserveAspectFit
                smooth: true
                antialiasing: true
            }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: root.size * 0.52
        height: width
        radius: width / 2
        visible: root.mode === "device" || root.mode === "system"
        color: Ui.Style.iconWellBg
        border.width: 1
        border.color: Ui.Style.iconWellBorder

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            radius: Math.max(0, parent.radius - 1)
            color: Ui.Style.iconWellOverlay
        }

        Image {
            anchors.centerIn: parent
            width: root.size * 0.24
            height: width
            source: root.mode === "device"
                    ? "qrc:/mi/e2ee/ui/icons/device.svg"
                    : "qrc:/mi/e2ee/ui/icons/info.svg"
            fillMode: Image.PreserveAspectFit
            smooth: true
            antialiasing: true
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.mode !== "group" && root.mode !== "device" && root.mode !== "system"
        text: root.initials()
        color: root.textColor
        font.family: Ui.Style.fontFamily
        font.pixelSize: Math.round(root.size * 0.34)
        font.weight: Font.DemiBold
        renderType: Text.NativeRendering
        antialiasing: true
    }

    Rectangle {
        visible: root.presenceState.length > 0
        width: Math.max(10, root.size * 0.24)
        height: width
        radius: width / 2
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: root.mode === "group" ? 1 : 0
        anchors.bottomMargin: root.mode === "group" ? 1 : 0
        color: root.badgeColor
        border.width: 2
        border.color: Ui.Style.panelBg

        Rectangle {
            width: parent.width * 0.44
            height: width
            radius: width / 2
            x: parent.width * 0.18
            y: parent.height * 0.14
            color: Qt.rgba(1, 1, 1, Ui.Style.isDark ? 0.16 : 0.26)
        }
    }
}
