import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string labelText: ""
    property string detailText: ""

    radius: 14
    color: Ui.Style.authBadgeBg
    border.width: 1
    border.color: Ui.Style.authBadgeBorder
    implicitHeight: badgeRow.implicitHeight + 10
    implicitWidth: badgeRow.implicitWidth + 18

    RowLayout {
        id: badgeRow
        anchors.centerIn: parent
        spacing: 6

        Rectangle {
            width: 8
            height: 8
            radius: 4
            color: Ui.Style.success
        }

        Components.UiText {
            text: root.labelText
            textRole: "caption"
            roleColor: Ui.Style.authBadgeText
        }

        Components.UiText {
            visible: text.length > 0
            text: root.detailText
            textRole: "caption"
            roleColor: Ui.Style.textMuted
        }
    }
}
