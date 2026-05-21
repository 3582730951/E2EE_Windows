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

    readonly property color toneAccent: Ui.UiUtil.preview_accent_for(kind)
    readonly property color toneBg: Ui.UiUtil.preview_tint_for(kind)
    readonly property color toneBorder: Ui.UiUtil.preview_border_for(kind)
    readonly property string iconSource: Ui.UiUtil.preview_icon_for(kind)

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
