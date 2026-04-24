import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string labelText: ""
    property string valueText: ""
    property string detailText: ""
    property string copyValue: ""
    property string copyButtonText: Ui.I18n.t("dialog.notifications.copyId")

    radius: Ui.Style.radiusLarge
    color: Ui.Style.statusSurfaceAlt
    border.width: 1
    border.color: Ui.Style.borderSubtle
    implicitHeight: content.implicitHeight + Ui.Style.paddingM * 2
    clip: true

    Rectangle {
        x: 1
        y: 1
        width: root.width - 2
        height: 1
        color: Ui.Style.heroCardSheen
        opacity: Ui.Style.isDark ? 0.36 : 0.72
    }

    RowLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingM

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Components.UiText {
                Layout.fillWidth: true
                text: root.labelText
                textRole: "caption"
                roleColor: Ui.Style.textMuted
            }

            Components.UiText {
                Layout.fillWidth: true
                text: root.valueText
                textRole: "code_inline"
            }

            Components.UiText {
                Layout.fillWidth: true
                visible: root.detailText.length > 0
                text: root.detailText
                textRole: "supporting"
                roleColor: Ui.Style.textSecondary
            }
        }

        Components.GhostButton {
            text: root.copyButtonText
            visible: root.copyValue.length > 0
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 84
            onClicked: Ui.SecurityDisplayStore.copyToInternalClipboard(root.copyValue)
        }
    }
}
