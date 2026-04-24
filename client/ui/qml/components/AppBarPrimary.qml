import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string titleText: ""
    property string subtitleText: ""
    default property alias actionContent: actionRow.data
    readonly property bool hasActions: actionRow.children.length > 0

    radius: Ui.Style.radiusXL
    color: "transparent"
    border.width: 1
    border.color: Ui.Style.heroCardBorder
    clip: true
    implicitHeight: headerLayout.implicitHeight + Ui.Style.paddingL * 2

    gradient: Gradient {
        GradientStop { position: 0.0; color: Ui.Style.heroCardBg }
        GradientStop { position: 1.0; color: Ui.Style.heroCardBgAlt }
    }

    Rectangle {
        width: root.width * 0.52
        height: width
        x: root.width - width * 0.64
        y: -width * 0.34
        radius: width / 2
        color: Ui.Style.heroCardGlow
    }

    Rectangle {
        width: root.width * 0.26
        height: width
        x: -width * 0.24
        y: root.height - height * 0.54
        radius: width / 2
        color: Ui.Style.alpha(Ui.Style.accentSoft, Ui.Style.isDark ? 0.08 : 0.10)
    }

    Rectangle {
        x: 1
        y: 1
        width: root.width - 2
        height: 1
        color: Ui.Style.heroCardSheen
        opacity: Ui.Style.isDark ? 0.55 : 0.90
    }

    RowLayout {
        id: headerLayout
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingL
        spacing: Ui.Style.paddingL

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Rectangle {
                visible: root.titleText.length > 0 || root.subtitleText.length > 0
                Layout.preferredWidth: 44
                Layout.preferredHeight: 4
                radius: 2
                color: Ui.Style.accent
                opacity: Ui.Style.isDark ? 0.92 : 0.78
            }

            Components.UiText {
                Layout.fillWidth: true
                visible: text.length > 0
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

        Rectangle {
            visible: root.hasActions
            radius: Ui.Style.radiusLarge
            color: Ui.Style.iconWellBg
            border.width: 1
            border.color: Ui.Style.iconWellBorder
            Layout.alignment: Qt.AlignVCenter
            implicitHeight: actionRow.implicitHeight + Ui.Style.paddingS * 2
            implicitWidth: actionRow.implicitWidth + Ui.Style.paddingS * 2

            RowLayout {
                id: actionRow
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingS
                spacing: Ui.Style.paddingS
            }
        }
    }
}
