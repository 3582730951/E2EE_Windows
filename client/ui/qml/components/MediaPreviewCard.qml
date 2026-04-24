import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string kind: "photo"
    property string titleText: ""
    property string detailText: ""
    property bool compact: false

    readonly property color toneAccent: {
        switch (kind) {
        case "photo":
            return Ui.Style.accent
        case "video":
            return "#3B82F6"
        case "voice":
            return Ui.Style.success
        case "link":
            return "#0EA5E9"
        default:
            return Ui.Style.textSecondary
        }
    }
    readonly property color toneBg: {
        switch (kind) {
        case "photo":
            return Qt.rgba(37 / 255, 99 / 255, 235 / 255, Ui.Style.isDark ? 0.18 : 0.09)
        case "video":
            return Qt.rgba(59 / 255, 130 / 255, 246 / 255, Ui.Style.isDark ? 0.18 : 0.09)
        case "voice":
            return Qt.rgba(5 / 255, 150 / 255, 105 / 255, Ui.Style.isDark ? 0.18 : 0.09)
        case "link":
            return Qt.rgba(14 / 255, 165 / 255, 233 / 255, Ui.Style.isDark ? 0.18 : 0.09)
        default:
            return Qt.rgba(100 / 255, 116 / 255, 139 / 255, Ui.Style.isDark ? 0.18 : 0.08)
        }
    }
    readonly property color toneBorder: {
        switch (kind) {
        case "photo":
            return Qt.rgba(37 / 255, 99 / 255, 235 / 255, Ui.Style.isDark ? 0.30 : 0.18)
        case "video":
            return Qt.rgba(59 / 255, 130 / 255, 246 / 255, Ui.Style.isDark ? 0.30 : 0.18)
        case "voice":
            return Qt.rgba(5 / 255, 150 / 255, 105 / 255, Ui.Style.isDark ? 0.30 : 0.18)
        case "link":
            return Qt.rgba(14 / 255, 165 / 255, 233 / 255, Ui.Style.isDark ? 0.30 : 0.18)
        default:
            return Ui.Style.borderSubtle
        }
    }
    readonly property string iconSource: {
        switch (kind) {
        case "photo":
            return "qrc:/mi/e2ee/ui/icons/image.svg"
        case "video":
            return "qrc:/mi/e2ee/ui/icons/video.svg"
        case "voice":
            return "qrc:/mi/e2ee/ui/icons/mic.svg"
        case "link":
            return "qrc:/mi/e2ee/ui/icons/info.svg"
        default:
            return "qrc:/mi/e2ee/ui/icons/file.svg"
        }
    }

    radius: Ui.Style.radiusLarge
    color: Ui.Style.mediaCardBg
    border.width: 1
    border.color: root.toneBorder
    clip: true
    implicitHeight: root.compact ? 58 : 72

    Rectangle {
        width: root.width * 0.28
        height: width
        x: -width * 0.18
        y: -height * 0.16
        radius: width / 2
        color: Ui.Style.alpha(root.toneAccent, Ui.Style.isDark ? 0.12 : 0.08)
    }

    Rectangle {
        x: 1
        y: 1
        width: root.width - 2
        height: 1
        color: Ui.Style.heroCardSheen
        opacity: Ui.Style.isDark ? 0.30 : 0.70
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: root.compact ? Ui.Style.paddingS : Ui.Style.paddingM
        spacing: root.compact ? Ui.Style.paddingS : Ui.Style.paddingM

        Rectangle {
            Layout.preferredWidth: root.compact ? 34 : 42
            Layout.preferredHeight: root.compact ? 34 : 42
            radius: Ui.Style.radiusMedium
            color: root.toneBg
            border.width: 1
            border.color: root.toneBorder
            clip: true

            Rectangle {
                width: parent.width * 0.70
                height: width
                x: -parent.width * 0.12
                y: -parent.height * 0.18
                radius: width / 2
                color: Ui.Style.alpha(root.toneAccent, Ui.Style.isDark ? 0.18 : 0.12)
            }

            Rectangle {
                anchors.centerIn: parent
                width: parent.width - 12
                height: width
                radius: Ui.Style.radiusSmall
                color: Ui.Style.iconWellBg
                border.width: 1
                border.color: Ui.Style.iconWellBorder

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    radius: Math.max(0, parent.radius - 1)
                    color: Ui.Style.mediaPreviewOverlay
                }
            }

            Image {
                anchors.centerIn: parent
                width: root.compact ? 15 : 18
                height: width
                source: root.iconSource
                fillMode: Image.PreserveAspectFit
                smooth: true
                antialiasing: true
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Components.UiText {
                Layout.fillWidth: true
                text: root.titleText
                textRole: "caption"
                roleColor: Ui.Style.textPrimary
            }

            Components.UiText {
                Layout.fillWidth: true
                visible: root.detailText.length > 0
                text: root.detailText
                textRole: "supporting"
                roleColor: Ui.Style.textMuted
            }
        }
    }
}
