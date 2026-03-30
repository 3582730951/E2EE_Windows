import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string titleText: ""
    property string subtitleText: ""
    default property alias actionContent: actionRow.data

    radius: Ui.Style.radiusLarge
    color: Ui.Style.panelBgAlt
    border.color: Ui.Style.borderSubtle
    implicitHeight: headerLayout.implicitHeight + Ui.Style.paddingM * 2

    RowLayout {
        id: headerLayout
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingM

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Components.UiText {
                Layout.fillWidth: true
                text: root.titleText
                textRole: "title"
                roleColor: Ui.Style.textPrimary
            }

            Components.UiText {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.subtitleText
                textRole: "subtitle"
                roleColor: Ui.Style.textMuted
            }
        }

        RowLayout {
            id: actionRow
            spacing: Ui.Style.paddingS
        }
    }
}
