import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Window {
    id: root
    property var clientBridge
    property var participants: []
    property bool videoEnabled: false
    property int durationSec: 0
    property bool micEnabled: true
    property bool cameraEnabled: true

    signal leaveRequested()
    signal micToggled(bool enabled)
    signal cameraToggled(bool enabled)

    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    width: 820
    height: 560

    function centerWindow() {
        x = Screen.virtualX + (Screen.width - width) / 2
        y = Screen.virtualY + (Screen.height - height) / 2
    }

    function formatCallDuration(totalSec) {
        var minutes = Math.floor(totalSec / 60)
        var seconds = totalSec % 60
        var mm = minutes < 10 ? "0" + minutes : "" + minutes
        var ss = seconds < 10 ? "0" + seconds : "" + seconds
        return mm + ":" + ss
    }

    function columnsFor(count) {
        if (count <= 1) return 1
        if (count <= 2) return 2
        if (count <= 4) return 2
        if (count <= 6) return 3
        return 4
    }

    onVisibleChanged: {
        if (visible) {
            centerWindow()
        }
    }
    onWidthChanged: if (visible) { centerWindow() }
    onHeightChanged: if (visible) { centerWindow() }
    onClosing: {
        leaveRequested()
    }

    Rectangle {
        anchors.fill: parent
        radius: 16
        color: Ui.Style.panelBgRaised
        border.color: Ui.Style.borderSubtle
    }

    DragHandler {
        target: null
        acceptedButtons: Qt.LeftButton
        onActiveChanged: {
            if (active && root.startSystemMove) {
                root.startSystemMove()
            }
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM

        Rectangle {
            anchors.fill: parent
            radius: 12
            color: Ui.Style.panelBgAlt
            border.color: Ui.Style.borderSubtle
        }

        GridLayout {
            id: grid
            anchors.fill: parent
            anchors.margins: Ui.Style.paddingM
            columns: columnsFor(participants ? participants.length : 0)
            rowSpacing: Ui.Style.paddingS
            columnSpacing: Ui.Style.paddingS

            Repeater {
                model: participants || []
                delegate: Components.GroupCallTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clientBridge: root.clientBridge
                    username: modelData.username || ""
                    isLocal: root.clientBridge &&
                             root.clientBridge.username === (modelData.username || "")
                    videoEnabled: root.videoEnabled
                }
            }
        }

        Column {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: Ui.Style.paddingS
            spacing: 4
            Text {
                text: root.videoEnabled
                      ? Ui.I18n.t("chat.groupCallActiveVideo")
                      : Ui.I18n.t("chat.groupCallActiveVoice")
                color: Ui.Style.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            Text {
                text: Ui.I18n.t("chat.callDuration")
                      .arg(formatCallDuration(root.durationSec))
                color: Ui.Style.textMuted
                font.pixelSize: 10
            }
        }

        Components.GroupCallBar {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            videoEnabled: root.videoEnabled
            micEnabled: root.micEnabled
            cameraEnabled: root.cameraEnabled
            onLeaveRequested: root.leaveRequested()
            onMicToggled: {
                root.micEnabled = enabled
                root.micToggled(enabled)
            }
            onCameraToggled: {
                root.cameraEnabled = enabled
                root.cameraToggled(enabled)
            }
        }
    }
}
