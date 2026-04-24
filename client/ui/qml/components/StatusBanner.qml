import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string tone: "neutral"
    property string titleText: ""
    property string detailText: ""
    readonly property color toneColor: tone === "danger"
                                       ? Ui.Style.danger
                                       : (tone === "success"
                                          ? Ui.Style.success
                                          : Ui.Style.accent)
    readonly property color toneSurface: Ui.Style.alpha(toneColor, Ui.Style.isDark ? 0.16 : 0.09)
    readonly property color toneBorder: Ui.Style.alpha(toneColor, Ui.Style.isDark ? 0.24 : 0.16)

    visible: titleText.length > 0 || detailText.length > 0
    radius: Ui.Style.radiusLarge
    border.width: 1
    border.color: root.toneBorder
    color: Ui.Style.statusSurfaceAlt
    clip: true
    implicitHeight: bannerLayout.implicitHeight + Ui.Style.paddingM * 2

    Rectangle {
        width: root.width * 0.38
        height: width
        x: -width * 0.22
        y: -height * 0.28
        radius: width / 2
        color: root.toneSurface
    }

    Rectangle {
        x: 1
        y: 1
        width: root.width - 2
        height: 1
        color: Ui.Style.heroCardSheen
        opacity: Ui.Style.isDark ? 0.42 : 0.78
    }

    RowLayout {
        id: bannerLayout
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingM

        Rectangle {
            Layout.preferredWidth: 4
            Layout.fillHeight: true
            radius: 2
            color: root.toneColor
            opacity: Ui.Style.isDark ? 0.96 : 0.84
        }

        Rectangle {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            radius: 16
            color: root.toneSurface
            border.width: 1
            border.color: root.toneBorder
            Layout.alignment: Qt.AlignTop

            Rectangle {
                anchors.centerIn: parent
                width: 16
                height: 16
                radius: 8
                visible: root.tone === "success"
                color: root.toneColor

                Image {
                    anchors.centerIn: parent
                    width: 10
                    height: width
                    source: "qrc:/mi/e2ee/ui/icons/check.svg"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    antialiasing: true
                }
            }

            Item {
                anchors.centerIn: parent
                width: 16
                height: 16
                visible: root.tone === "danger"

                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 2
                    radius: 1
                    rotation: 45
                    color: root.toneColor
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 2
                    radius: 1
                    rotation: -45
                    color: root.toneColor
                }
            }

            Item {
                anchors.centerIn: parent
                width: 16
                height: 16
                visible: root.tone !== "danger" && root.tone !== "success"

                Rectangle {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    radius: 7
                    color: "transparent"
                    border.width: 1
                    border.color: root.toneColor
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: 2
                    width: 2
                    height: 5
                    radius: 1
                    color: root.toneColor
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 3
                    width: 3
                    height: 3
                    radius: 1.5
                    color: root.toneColor
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
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
}
