import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: root
    property var clientBridge
    property string username: ""
    property bool isLocal: false
    property bool videoEnabled: false

    implicitWidth: 220
    implicitHeight: 150

    function bindSink() {
        if (!clientBridge || !videoEnabled) {
            return
        }
        if (isLocal && clientBridge.bindLocalVideoSink) {
            clientBridge.bindLocalVideoSink(videoOutput.videoSink)
            return
        }
        if (!isLocal && clientBridge.bindGroupCallVideoSink) {
            clientBridge.bindGroupCallVideoSink(username, videoOutput.videoSink)
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: Ui.Style.panelBgAlt
        border.color: Ui.Style.borderSubtle
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        visible: videoEnabled
    }

    Rectangle {
        anchors.fill: parent
        visible: !videoEnabled
        radius: 12
        color: Ui.Style.panelBg
        border.color: Ui.Style.borderSubtle
        Text {
            anchors.centerIn: parent
            text: username.length > 0 ? username.charAt(0).toUpperCase() : "?"
            color: Ui.Style.textPrimary
            font.pixelSize: 28
            font.weight: Font.DemiBold
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 28
        radius: 8
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        text: isLocal ? Ui.I18n.t("chat.groupCallYou") : username
        color: Ui.Style.textPrimary
        font.pixelSize: 11
        elide: Text.ElideRight
        width: parent.width - 20
    }

    Component.onCompleted: bindSink()
    onUsernameChanged: bindSink()
    onVideoEnabledChanged: bindSink()
}
