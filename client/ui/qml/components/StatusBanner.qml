import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string tone: "neutral"
    property string titleText: ""
    property string detailText: ""

    visible: titleText.length > 0 || detailText.length > 0
    radius: Ui.Style.radiusMedium
    border.width: 1
    border.color: tone === "danger"
                  ? Ui.Style.authDangerBorder
                  : (tone === "success"
                     ? Ui.Style.authSuccessBorder
                     : Ui.Style.authInfoBorder)
    color: tone === "danger"
           ? Ui.Style.authDangerBg
           : (tone === "success"
              ? Ui.Style.authSuccessBg
              : Ui.Style.authInfoBg)
    implicitHeight: bannerLayout.implicitHeight + Ui.Style.paddingM * 2

    ColumnLayout {
        id: bannerLayout
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: 4

        Components.UiText {
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.titleText
            textRole: "subtitle"
            roleColor: Ui.Style.textPrimary
        }

        Components.UiText {
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.detailText
            textRole: "detail"
            roleColor: Ui.Style.textSecondary
        }
    }
}
