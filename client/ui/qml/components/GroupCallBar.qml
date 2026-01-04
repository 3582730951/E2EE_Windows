import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root
    property bool videoEnabled: false
    property bool micEnabled: true
    property bool cameraEnabled: true

    signal leaveRequested()
    signal micToggled(bool enabled)
    signal cameraToggled(bool enabled)

    implicitHeight: row.implicitHeight
    implicitWidth: row.implicitWidth

    Row {
        id: row
        spacing: 18
        Components.RoundIconButton {
            icon.source: "qrc:/mi/e2ee/ui/icons/mic.svg"
            buttonSize: 46
            iconSize: 20
            baseColor: root.micEnabled ? Ui.Style.textPrimary : Ui.Style.textMuted
            hoverColor: Ui.Style.textPrimary
            pressColor: Ui.Style.textPrimary
            bgColor: "transparent"
            onClicked: {
                var next = !root.micEnabled
                root.micToggled(next)
            }
        }
        Components.RoundIconButton {
            icon.source: "qrc:/mi/e2ee/ui/icons/phone.svg"
            buttonSize: 56
            iconSize: 22
            baseColor: Ui.Style.danger
            hoverColor: Ui.Style.danger
            pressColor: Ui.Style.danger
            bgColor: "transparent"
            hoverBg: Qt.rgba(1, 1, 1, 0.08)
            pressedBg: Qt.rgba(1, 1, 1, 0.16)
            onClicked: root.leaveRequested()
        }
        Components.RoundIconButton {
            icon.source: "qrc:/mi/e2ee/ui/icons/video.svg"
            buttonSize: 46
            iconSize: 20
            baseColor: root.cameraEnabled ? Ui.Style.textPrimary : Ui.Style.textMuted
            hoverColor: Ui.Style.textPrimary
            pressColor: Ui.Style.textPrimary
            bgColor: "transparent"
            visible: root.videoEnabled
            onClicked: {
                var next = !root.cameraEnabled
                root.cameraToggled(next)
            }
        }
    }
}
