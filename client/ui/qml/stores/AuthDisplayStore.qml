pragma Singleton
import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: store

    readonly property var client: typeof clientBridge === "undefined" ? null : clientBridge
    readonly property bool loggedIn: client ? client.loggedIn : false
    readonly property string qrLoginPayload: client ? (client.qrLoginPayload || "") : ""

    property string errorText: ""
    property bool waitingServerTrust: false
    property string gatewayState: ""
    property string gatewayDetail: ""
    property string maskedCurrentDeviceId: ""

    signal authSucceeded()
    signal serverTrustPromptRequested(string fingerprint, string pin)
    signal peerTrustPromptRequested(string peer, string fingerprint, string pin)

    function syncDisplayState() {
        gatewayState = client && client.gatewayDisplayState ? client.gatewayDisplayState() : ""
        gatewayDetail = client && client.gatewayDisplayDetail ? client.gatewayDisplayDetail() : ""
        maskedCurrentDeviceId = client && client.maskedCurrentDeviceId ? client.maskedCurrentDeviceId() : ""
    }

    function syncTrustState() {
        waitingServerTrust = client ? client.hasPendingServerTrust === true : false
    }

    function syncState() {
        syncDisplayState()
        syncTrustState()
    }

    function completeAuth() {
        cancelQrLogin()
        Ui.AppStore.currentPage = 1
        if (Ui.AppStore.syncDomainStores) {
            Ui.AppStore.syncDomainStores()
        }
        authSucceeded()
    }

    function initClient() {
        if (!client) {
            errorText = Ui.I18n.t("auth.error.login")
            return false
        }
        if (!client.init("")) {
            errorText = client.lastError && client.lastError.length > 0
                        ? client.lastError
                        : Ui.I18n.t("auth.error.login")
            syncState()
            return false
        }
        syncState()
        return true
    }

    function login(user, pass, rootCode) {
        if (!initClient()) {
            return false
        }
        if (!client.loginWithRootCode(user, pass, rootCode)) {
            syncState()
            if (waitingServerTrust) {
                errorText = client.lastError && client.lastError.length > 0
                            ? client.lastError
                            : Ui.I18n.t("dialog.securityCenter.trustReviewHint")
            } else {
                errorText = client.lastError && client.lastError.length > 0
                            ? client.lastError
                            : Ui.I18n.t("auth.error.login")
            }
            return false
        }
        errorText = ""
        syncState()
        Ui.AppStore.bootstrapAfterLogin()
        completeAuth()
        return true
    }

    function beginQrLogin(user) {
        if (!initClient()) {
            return false
        }
        if (!client.beginQrLogin(user)) {
            errorText = client.lastError && client.lastError.length > 0
                        ? client.lastError
                        : Ui.I18n.t("auth.error.login")
            syncState()
            return false
        }
        errorText = ""
        syncState()
        return true
    }

    function pollQrLogin() {
        if (!client) {
            errorText = Ui.I18n.t("auth.error.login")
            return false
        }
        if (!client.pollQrLogin()) {
            errorText = client.lastError && client.lastError.length > 0
                        ? client.lastError
                        : Ui.I18n.t("auth.error.login")
            syncState()
            return false
        }
        syncState()
        if (loggedIn) {
            errorText = ""
            Ui.AppStore.bootstrapAfterLogin()
            completeAuth()
        }
        return true
    }

    function cancelQrLogin() {
        if (client) {
            client.cancelQrLogin()
        }
        syncState()
    }

    function qrLoginImage(size) {
        return client ? client.qrLoginImage(size) : ""
    }

    function registerAccount(account, password) {
        if (!initClient()) {
            return false
        }
        if (!client.registerUser(account, password)) {
            errorText = client.lastError && client.lastError.length > 0
                        ? client.lastError
                        : Ui.I18n.t("auth.error.registerIncomplete")
            syncState()
            return false
        }
        errorText = ""
        syncState()
        return true
    }

    function approveTrust(mode, pin) {
        if (!client) {
            errorText = Ui.I18n.t("auth.error.login")
            return false
        }
        var ok = false
        if (mode === "peer") {
            ok = client.trustPendingPeer(pin)
        } else {
            ok = client.trustPendingServer(pin)
        }
        syncState()
        if (!ok) {
            errorText = client.lastError && client.lastError.length > 0
                        ? client.lastError
                        : Ui.I18n.t("dialog.securityCenter.trustReviewHint")
            return false
        }
        errorText = ""
        return true
    }

    Connections {
        target: store.client
        ignoreUnknownSignals: true

        function onTokenChanged() {
            store.syncState()
        }

        function onTrustStateChanged() {
            store.syncState()
        }

        function onServerTrustRequired(fingerprint, pin) {
            store.syncState()
            store.serverTrustPromptRequested(fingerprint, pin)
        }

        function onPeerTrustRequired(peer, fingerprint, pin) {
            store.syncState()
            store.peerTrustPromptRequested(peer, fingerprint, pin)
        }

        function onQrLoginChanged() {
            store.syncState()
        }

        function onConnectionChanged() {
            store.syncDisplayState()
        }

        function onDeviceChanged() {
            store.syncDisplayState()
        }
    }

    Component.onCompleted: syncState()
}
