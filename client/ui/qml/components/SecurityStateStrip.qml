import QtQuick 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Rectangle {
    id: root

    property string titleText: ""
    property string valueText: ""
    property string detailText: ""
    property string tone: "healthy"
    readonly property color toneColor: {
        switch (tone) {
        case "healthy":
            return Ui.Style.success
        case "checking":
            return Ui.Style.warning
        case "review":
            return Ui.Style.accent
        case "blocked":
            return Ui.Style.danger
        default:
            return Ui.Style.textMuted
        }
    }
    readonly property color toneBg: Ui.Style.alpha(toneColor, Ui.Style.isDark ? 0.12 : 0.07)
    readonly property color toneBorder: Ui.Style.alpha(toneColor, Ui.Style.isDark ? 0.24 : 0.16)
    readonly property color toneWell: Ui.Style.alpha(toneColor, Ui.Style.isDark ? 0.18 : 0.11)

    radius: Ui.Style.radiusLarge
    color: Ui.Style.statusSurfaceAlt
    border.width: 1
    border.color: root.toneBorder
    clip: true
    implicitHeight: stripColumn.implicitHeight + Ui.Style.paddingM * 2

    Rectangle {
        width: root.width * 0.30
        height: width
        x: -width * 0.18
        y: -height * 0.22
        radius: width / 2
        color: root.toneBg
    }

    Rectangle {
        x: 1
        y: 1
        width: root.width - 2
        height: 1
        color: Ui.Style.heroCardSheen
        opacity: Ui.Style.isDark ? 0.40 : 0.76
    }

    ColumnLayout {
        id: stripColumn
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: Ui.Style.paddingS

            Rectangle {
                width: 34
                height: 34
                radius: 17
                color: root.toneWell
                border.width: 1
                border.color: root.toneBorder
                Layout.alignment: Qt.AlignVCenter

                Rectangle {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    radius: 9
                    visible: root.tone === "healthy"
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
                    width: 18
                    height: 18
                    visible: root.tone === "checking"

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
                        y: 4
                        width: 2
                        height: 6
                        radius: 1
                        color: root.toneColor
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: 3
                        anchors.verticalCenterOffset: 1
                        width: 5
                        height: 2
                        radius: 1
                        rotation: 35
                        color: root.toneColor
                    }
                }

                Item {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    visible: root.tone === "review" || (root.tone !== "healthy" && root.tone !== "checking" && root.tone !== "blocked")

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

                Item {
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    visible: root.tone === "blocked"

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
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Components.UiText {
                    text: root.titleText
                    textRole: "caption"
                    roleColor: Ui.Style.textSecondary
                }

                Components.UiText {
                    Layout.fillWidth: true
                    visible: root.detailText.length > 0
                    text: root.detailText
                    textRole: "supporting"
                    roleColor: Ui.Style.textMuted
                }
            }

            Rectangle {
                visible: root.valueText.length > 0
                radius: height / 2
                color: Ui.Style.statusValueBg
                border.width: 1
                border.color: Ui.Style.alpha(root.toneColor, Ui.Style.isDark ? 0.20 : 0.12)
                Layout.alignment: Qt.AlignTop
                implicitHeight: valueLabel.implicitHeight + 8
                implicitWidth: valueLabel.implicitWidth + 14

                Components.UiText {
                    id: valueLabel
                    anchors.centerIn: parent
                    text: root.valueText
                    textRole: "caption"
                    roleColor: root.toneColor
                }
            }
        }
    }
}
