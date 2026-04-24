import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/dialogs" as Dialogs

Item {
    id: root

    property var ownerWindow: null

    Dialogs.TrustPromptDialog {
        id: trustDialog
        ownerWindow: root.ownerWindow
        onAccepted: function(pinText) {
            if (Ui.AuthDisplayStore.approveTrust(mode, pinText)) {
                trustDialog.close()
            }
        }
    }

    Connections {
        target: Ui.AuthDisplayStore

        function onServerTrustPromptRequested(fingerprint, pin) {
            trustDialog.openWith("server", fingerprint, pin, "")
        }

        function onPeerTrustPromptRequested(peer, fingerprint, pin) {
            trustDialog.openWith("peer", fingerprint, pin, peer)
        }
    }
}
