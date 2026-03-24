pragma Singleton
import QtQuick 2.15

QtObject {
    id: callStore

    property bool incomingCallActive: false
    property string incomingCallPeer: ""
    property string incomingCallId: ""
    property bool incomingCallVideo: false

    function applyFromApp(active, peer, callId, video) {
        incomingCallActive = active === true
        incomingCallPeer = peer || ""
        incomingCallId = callId || ""
        incomingCallVideo = video === true
    }
}
