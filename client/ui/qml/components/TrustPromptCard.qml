import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string titleText: ""
    property string descriptionText: ""
    property string badgeLabelText: ""
    property string badgeDetailText: ""
    property string fingerprintLabelText: Ui.I18n.t("dialog.securityCenter.serverTitle")
    property string fingerprintText: ""
    property string fingerprintDetailText: ""

    radius: Ui.Style.radiusLarge
    color: Ui.Style.panelBg
    border.width: 1
    border.color: Ui.Style.borderSubtle
    clip: true

    Rectangle {
        x: 1
        y: 1
        width: root.width - 2
        height: 1
        color: Ui.Style.heroCardSheen
        opacity: Ui.Style.isDark ? 0.38 : 0.78
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingS

        Components.SecurityBadge {
            visible: root.badgeLabelText.length > 0 || root.badgeDetailText.length > 0
            labelText: root.badgeLabelText
            detailText: root.badgeDetailText
        }

        Components.UiText {
            Layout.fillWidth: true
            text: root.titleText
            textRole: "title"
        }

        Components.UiText {
            Layout.fillWidth: true
            text: root.descriptionText
            textRole: "detail"
            roleColor: Ui.Style.textSecondary
        }

        Components.DeviceFingerprintRow {
            Layout.fillWidth: true
            labelText: root.fingerprintLabelText
            valueText: root.fingerprintText
            detailText: root.fingerprintDetailText
            copyValue: root.fingerprintText
        }
    }
}
