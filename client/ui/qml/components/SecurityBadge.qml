import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string labelText: ""
    property string detailText: ""
    readonly property real maxBadgeWidth: parent && parent.width > 0
                                         ? parent.width * Ui.Style.badgeMaxWidthRatio
                                         : (badgeRow.implicitWidth + 20)

    radius: height / 2
    color: Ui.Style.badgeSurface
    border.width: 1
    border.color: Ui.Style.badgeBorder
    clip: true
    implicitHeight: badgeRow.implicitHeight + 10
    implicitWidth: Math.min(badgeRow.implicitWidth + 20, maxBadgeWidth)
    width: implicitWidth

    Rectangle {
        x: 1
        y: 1
        width: root.width - 2
        height: 1
        color: Ui.Style.heroCardSheen
        opacity: Ui.Style.isDark ? 0.35 : 0.72
    }

    RowLayout {
        id: badgeRow
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 6

        Rectangle {
            width: 18
            height: 18
            radius: 9
            color: Ui.Style.badgeSurfaceStrong
            border.width: 1
            border.color: Ui.Style.alpha(Ui.Style.accent, Ui.Style.isDark ? 0.26 : 0.18)

            Rectangle {
                anchors.centerIn: parent
                width: 12
                height: 12
                radius: 6
                color: Ui.Style.accent
            }

            Image {
                anchors.centerIn: parent
                width: 9
                height: width
                source: "qrc:/mi/e2ee/ui/icons/check.svg"
                fillMode: Image.PreserveAspectFit
                smooth: true
                antialiasing: true
            }
        }

        Components.UiText {
            Layout.fillWidth: root.detailText.length === 0
            Layout.maximumWidth: root.detailText.length === 0 ? -1 : Math.max(36, root.width * 0.56)
            text: root.labelText
            textRole: "caption"
            roleColor: Ui.Style.badgeTextPrimary
            wrapMode: Text.NoWrap
            maximumLineCount: 1
            elide: Text.ElideRight
        }

        Rectangle {
            visible: root.detailText.length > 0
            width: 4
            height: 4
            radius: 2
            color: Ui.Style.badgeTextSecondary
            opacity: 0.7
        }

        Components.UiText {
            visible: text.length > 0
            Layout.maximumWidth: Math.max(28, root.width * 0.34)
            text: root.detailText
            textRole: "caption"
            roleColor: Ui.Style.badgeTextSecondary
            wrapMode: Text.NoWrap
            maximumLineCount: 1
            elide: Text.ElideRight
        }
    }
}
