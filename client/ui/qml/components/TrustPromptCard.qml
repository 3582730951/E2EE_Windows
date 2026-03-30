import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string titleText: ""
    property string descriptionText: ""
    property string fingerprintLabelText: Ui.I18n.t("dialog.securityCenter.serverTitle")
    property string fingerprintText: ""

    radius: Ui.Style.radiusLarge
    color: Ui.Style.panelBg
    border.color: Ui.Style.borderSubtle

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingS

        Components.UiText {
            Layout.fillWidth: true
            text: root.titleText
            textRole: "title"
        }

        Components.UiText {
            Layout.fillWidth: true
            text: root.descriptionText
            textRole: "detail"
            foreground: Ui.Style.textSecondary
        }

        Components.DeviceFingerprintRow {
            Layout.fillWidth: true
            labelText: root.fingerprintLabelText
            valueText: root.fingerprintText
            copyValue: root.fingerprintText
        }
    }
}
