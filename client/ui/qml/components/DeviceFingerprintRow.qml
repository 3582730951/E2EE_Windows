import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string labelText: ""
    property string valueText: ""
    property string copyValue: ""

    radius: Ui.Style.radiusMedium
    color: Ui.Style.panelBgAlt
    border.color: Ui.Style.borderSubtle
    implicitHeight: content.implicitHeight + Ui.Style.paddingS * 2

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingS
        spacing: Ui.Style.paddingS

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Components.UiText {
                Layout.fillWidth: true
                text: root.labelText
                textRole: "caption"
                foreground: Ui.Style.textMuted
            }

            Components.UiText {
                Layout.fillWidth: true
                text: root.valueText
                textRole: "code_inline"
            }
        }

        Components.GhostButton {
            text: Ui.I18n.t("dialog.notifications.copyId")
            visible: root.copyValue.length > 0
            onClicked: Ui.SecurityDisplayStore.copyToInternalClipboard(root.copyValue)
        }
    }
}
