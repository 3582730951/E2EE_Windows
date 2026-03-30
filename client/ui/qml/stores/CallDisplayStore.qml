pragma Singleton
import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

QtObject {
    id: store

    property var bridge: typeof clientBridge === "undefined" ? null : clientBridge

    readonly property bool incomingCallActive: Ui.CallStore.incomingCallActive
    readonly property string incomingCallPeer: Ui.CallStore.incomingCallPeer
    readonly property string incomingCallId: Ui.CallStore.incomingCallId
    readonly property bool incomingCallVideo: Ui.CallStore.incomingCallVideo
    readonly property string activeCallId: bridge ? bridge.activeCallId : ""
    readonly property string activeCallPeer: bridge ? bridge.activeCallPeer : ""
    readonly property bool activeCallVideo: bridge ? bridge.activeCallVideo : false
    readonly property bool groupCallActive: bridge ? bridge.groupCallActive : false
    readonly property string activeGroupCallId: bridge ? bridge.activeGroupCallId : ""
    readonly property string activeGroupCallGroup: bridge ? bridge.activeGroupCallGroup : ""
    readonly property bool activeGroupCallVideo: bridge ? bridge.activeGroupCallVideo : false

    function acceptIncomingCall() {
        return Ui.AppStore.acceptIncomingCall()
    }

    function declineIncomingCall() {
        Ui.AppStore.declineIncomingCall()
    }

    function endCall() {
        if (bridge && bridge.endCall) {
            bridge.endCall()
        }
    }
}
