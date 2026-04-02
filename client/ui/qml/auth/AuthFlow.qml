import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root

    property string accountInput: ""
    property string passwordInput: ""
    property string rootCodeInput: ""
    property string registerAccount: ""
    property string registerPassword: ""
    property string registerConfirm: ""
    property int qrSeconds: 30
    property string errorText: ""
    property string lastLoginAccount: ""
    property string lastLoginPassword: ""
    property string lastLoginRootCode: ""
    property bool waitingServerTrust: false
    property bool qrActive: false

    signal authSucceeded()

    Timer {
        id: qrTimer
        interval: 1000
        repeat: true
        onTriggered: {
            if (qrSeconds > 0) {
                qrSeconds -= 1
            } else {
                qrTimer.stop()
                stopQrLogin()
            }
        }
    }

    Timer {
        id: qrPollTimer
        interval: 1500
        repeat: true
        onTriggered: pollQrLogin()
    }

    function completeAuth() {
        stopQrLogin()
        authSucceeded()
    }

    function attemptLogin(user, pass, rootCode, fromTrust) {
        if (!Ui.AuthDisplayStore.login(user, pass, rootCode)) {
            waitingServerTrust = Ui.AuthDisplayStore.waitingServerTrust
            errorText = Ui.AuthDisplayStore.errorText
            if (waitingServerTrust && !fromTrust && errorText.length === 0) {
                errorText = Ui.I18n.t("auth.error.login")
            }
            return false
        }
        waitingServerTrust = false
        errorText = ""
        completeAuth()
        return true
    }

    function startQrLogin() {
        if (!Ui.AuthDisplayStore.beginQrLogin(accountInput)) {
            errorText = Ui.AuthDisplayStore.errorText
            stopQrLogin()
            return false
        }
        errorText = ""
        qrActive = true
        resetQrTimer()
        qrPollTimer.restart()
        return true
    }

    function pollQrLogin() {
        if (!qrActive) {
            return
        }
        if (!Ui.AuthDisplayStore.pollQrLogin()) {
            errorText = Ui.AuthDisplayStore.errorText
            stopQrLogin()
            return
        }
        if (Ui.AuthDisplayStore.loggedIn) {
            errorText = ""
            stopQrLogin()
            completeAuth()
        }
    }

    function stopQrLogin() {
        Ui.AuthDisplayStore.cancelQrLogin()
        qrActive = false
        qrPollTimer.stop()
        qrTimer.stop()
    }

    function resetQrTimer() {
        qrSeconds = 30
        qrTimer.restart()
    }

    function statusTone() {
        return errorText.length > 0 ? "danger" : "neutral"
    }

    function statusTitle() {
        if (errorText.length > 0) {
            return errorText
        }
        return Ui.AuthDisplayStore.gatewayState.length > 0
                ? Ui.AuthDisplayStore.gatewayState
                : Ui.I18n.t("auth.hero.badge")
    }

    function statusDetail() {
        if (waitingServerTrust) {
            return Ui.I18n.t("dialog.securityCenter.trustReviewHint")
        }
        if (Ui.AuthDisplayStore.gatewayDetail.length > 0) {
            return Ui.AuthDisplayStore.gatewayDetail
        }
        return Ui.I18n.t("dialog.securityCenter.serverHint")
    }

    Rectangle {
        id: loginShell
        anchors.fill: parent
        color: "transparent"
        clip: true

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            gradient: Gradient {
                GradientStop { position: 0.0; color: Ui.Style.authBackdropTop }
                GradientStop { position: 1.0; color: Ui.Style.authBackdropBottom }
            }
        }

        Rectangle {
            width: parent.width * 0.54
            height: parent.height * 0.46
            anchors.centerIn: parent
            radius: Ui.Style.radiusXL * 2
            color: Ui.Style.authGlowPrimary
            opacity: 0.10
        }

        Item {
            id: authStage
            width: Math.min(parent.width - Ui.Style.paddingXL * 2, Ui.Style.authStageWidth)
            height: Math.min(parent.height - Ui.Style.paddingXL * 2, Ui.Style.authStageHeight)
            anchors.centerIn: parent

            Rectangle {
                id: authPanel
                width: Math.min(parent.width, Ui.Style.authPanelWidth)
                height: Math.min(parent.height, Ui.Style.authStageHeight)
                anchors.centerIn: parent
                radius: Ui.Style.radiusXL
                color: Ui.Style.authCardBg
                border.width: 1
                border.color: Ui.Style.authCardBorder
                antialiasing: true
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingL
                    spacing: 6

                    Label {
                        Layout.fillWidth: true
                        text: Ui.I18n.t("app.title")
                        color: Ui.Style.textMuted
                        font.pixelSize: Ui.Style.authMetaTextSize
                        font.weight: Font.Medium
                        wrapMode: Text.NoWrap
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: Ui.I18n.t("auth.title")
                        color: Ui.Style.textPrimary
                        font.pixelSize: 22
                        font.weight: Font.DemiBold
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: Ui.I18n.t("auth.subtitle")
                        color: Ui.Style.textSecondary
                        font.pixelSize: Ui.Style.authSubtitleTextSize
                        wrapMode: Text.WordWrap
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        radius: 15
                        border.width: 1
                        border.color: statusTone() === "danger"
                                      ? Ui.Style.authDangerBorder
                                      : Ui.Style.authContextBorder
                        color: statusTone() === "danger"
                               ? Ui.Style.authDangerBg
                               : Ui.Style.authContextBg

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Ui.Style.paddingS
                            anchors.rightMargin: Ui.Style.paddingS
                            spacing: 6

                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: statusTone() === "danger"
                                       ? Ui.Style.danger
                                       : Ui.Style.accent
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Text {
                                text: statusTitle()
                                Layout.preferredWidth: Math.min(108, implicitWidth)
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: Ui.Style.textPrimary
                                elide: Text.ElideRight
                            }

                            Text {
                                text: statusDetail()
                                Layout.fillWidth: true
                                font.pixelSize: 11
                                color: Ui.Style.textSecondary
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        implicitHeight: loginPageLayout.implicitHeight

                        ColumnLayout {
                            id: loginPageLayout
                            width: parent.width
                            spacing: Ui.Style.paddingS

                            Label {
                                text: Ui.I18n.t("auth.placeholder.account")
                                font.pixelSize: Ui.Style.authSubtitleTextSize
                                font.weight: Font.Medium
                                color: Ui.Style.authLabelText
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Components.SecureTextField {
                                id: accountField
                                Layout.fillWidth: true
                                Layout.preferredHeight: Ui.Style.authFieldHeight
                                placeholderText: Ui.I18n.t("auth.placeholder.account")
                                font.pixelSize: Ui.Style.authBodyTextSize
                                color: Ui.Style.textPrimary
                                placeholderTextColor: Ui.Style.authPlaceholderText
                                Accessible.name: Ui.I18n.t("auth.placeholder.account")
                                background: Rectangle {
                                    radius: Ui.Style.radiusMedium
                                    color: Ui.Style.authFieldBg
                                    border.width: 1
                                    border.color: accountField.activeFocus
                                                  ? Ui.Style.authFieldFocus
                                                  : Ui.Style.authFieldBorder
                                }
                                onTextChanged: accountInput = text
                            }

                            Label {
                                text: Ui.I18n.t("auth.placeholder.password")
                                font.pixelSize: Ui.Style.authSubtitleTextSize
                                font.weight: Font.Medium
                                color: Ui.Style.authLabelText
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            Components.SecureTextField {
                                id: passwordField
                                Layout.fillWidth: true
                                Layout.preferredHeight: Ui.Style.authFieldHeight
                                echoMode: TextInput.Password
                                placeholderText: Ui.I18n.t("auth.placeholder.password")
                                font.pixelSize: Ui.Style.authBodyTextSize
                                color: Ui.Style.textPrimary
                                placeholderTextColor: Ui.Style.authPlaceholderText
                                Accessible.name: Ui.I18n.t("auth.placeholder.password")
                                background: Rectangle {
                                    radius: Ui.Style.radiusMedium
                                    color: Ui.Style.authFieldBg
                                    border.width: 1
                                    border.color: passwordField.activeFocus
                                                  ? Ui.Style.authFieldFocus
                                                  : Ui.Style.authFieldBorder
                                }
                                onTextChanged: passwordInput = text
                            }

                            Components.RootAuthCodeCard {
                                id: rootCodeFieldCard
                                Layout.fillWidth: true
                                labelText: Ui.I18n.t("auth.placeholder.rootCode")
                                placeholderText: Ui.I18n.t("auth.placeholder.rootCode")
                                onTextChanged: rootCodeInput = text
                            }

                            Button {
                                id: loginButton
                                text: Ui.I18n.t("auth.login")
                                Layout.fillWidth: true
                                Layout.preferredHeight: Ui.Style.authPrimaryButtonHeight
                                Accessible.name: Ui.I18n.t("auth.login")
                                background: Rectangle {
                                    radius: Ui.Style.radiusMedium
                                    gradient: Gradient {
                                        GradientStop { position: 0.0; color: loginButton.down ? Ui.Style.accentPressed : Ui.Style.accentHover }
                                        GradientStop { position: 1.0; color: loginButton.down ? Ui.Style.accent : Ui.Style.accent }
                                    }
                                    border.width: 1
                                    border.color: Ui.Style.authBadgeBorder
                                }
                                contentItem: Text {
                                    text: Ui.I18n.t("auth.login")
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 16
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    if (accountInput.length === 0 || passwordInput.length === 0) {
                                        errorText = Ui.I18n.t("auth.error.login")
                                        return
                                    }
                                    errorText = ""
                                    lastLoginAccount = accountInput
                                    lastLoginPassword = passwordInput
                                    lastLoginRootCode = rootCodeInput
                                    attemptLogin(accountInput, passwordInput, rootCodeInput, false)
                                }
                            }

                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: Ui.Style.paddingS

                                Button {
                                    text: Ui.I18n.t("auth.registerAccount")
                                    flat: true
                                    Accessible.name: Ui.I18n.t("auth.registerAccount")
                                    onClicked: registerPopup.open()
                                    contentItem: Text {
                                        text: parent.text
                                        color: Ui.Style.link
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle { color: "transparent" }
                                }

                                Rectangle {
                                    width: 1
                                    height: 12
                                    radius: 1
                                    color: Ui.Style.borderSubtle
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                Button {
                                    text: Ui.I18n.t("auth.qrLogin")
                                    flat: true
                                    Accessible.name: Ui.I18n.t("auth.qrLogin")
                                    onClicked: qrPopup.open()
                                    contentItem: Text {
                                        text: parent.text
                                        color: Ui.Style.link
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle { color: "transparent" }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: registerPopup
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(root.width - Ui.Style.paddingXL * 2, 332)
        padding: Ui.Style.paddingL
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - implicitHeight) / 2)

        background: Rectangle {
            radius: Ui.Style.radiusLarge
            color: Ui.Style.authCardBg
            border.width: 1
            border.color: Ui.Style.authCardBorder
        }

        contentItem: ColumnLayout {
            spacing: Ui.Style.paddingS

            Label {
                Layout.fillWidth: true
                text: Ui.I18n.t("auth.registerAccount")
                color: Ui.Style.textPrimary
                font.pixelSize: 18
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }

            Components.SecureTextField {
                Layout.fillWidth: true
                Layout.preferredHeight: Ui.Style.authFieldHeight
                placeholderText: Ui.I18n.t("auth.register.placeholder.account")
                font.pixelSize: Ui.Style.authBodyTextSize
                color: Ui.Style.textPrimary
                placeholderTextColor: Ui.Style.authPlaceholderText
                Accessible.name: Ui.I18n.t("auth.register.placeholder.account")
                background: Rectangle {
                    radius: Ui.Style.radiusMedium
                    color: Ui.Style.authFieldBg
                    border.width: 1
                    border.color: Ui.Style.authFieldBorder
                }
                onTextChanged: registerAccount = text
            }

            Components.SecureTextField {
                Layout.fillWidth: true
                Layout.preferredHeight: Ui.Style.authFieldHeight
                echoMode: TextInput.Password
                placeholderText: Ui.I18n.t("auth.register.placeholder.password")
                font.pixelSize: Ui.Style.authBodyTextSize
                color: Ui.Style.textPrimary
                placeholderTextColor: Ui.Style.authPlaceholderText
                Accessible.name: Ui.I18n.t("auth.register.placeholder.password")
                background: Rectangle {
                    radius: Ui.Style.radiusMedium
                    color: Ui.Style.authFieldBg
                    border.width: 1
                    border.color: Ui.Style.authFieldBorder
                }
                onTextChanged: registerPassword = text
            }

            Components.SecureTextField {
                Layout.fillWidth: true
                Layout.preferredHeight: Ui.Style.authFieldHeight
                echoMode: TextInput.Password
                placeholderText: Ui.I18n.t("auth.register.placeholder.confirm")
                font.pixelSize: Ui.Style.authBodyTextSize
                color: Ui.Style.textPrimary
                placeholderTextColor: Ui.Style.authPlaceholderText
                Accessible.name: Ui.I18n.t("auth.register.placeholder.confirm")
                background: Rectangle {
                    radius: Ui.Style.radiusMedium
                    color: Ui.Style.authFieldBg
                    border.width: 1
                    border.color: Ui.Style.authFieldBorder
                }
                onTextChanged: registerConfirm = text
            }

            Button {
                text: Ui.I18n.t("auth.register")
                Layout.fillWidth: true
                Layout.preferredHeight: Ui.Style.authPrimaryButtonHeight
                Accessible.name: Ui.I18n.t("auth.register")
                background: Rectangle {
                    radius: Ui.Style.radiusMedium
                    color: Ui.Style.accent
                    border.width: 1
                    border.color: Ui.Style.authBadgeBorder
                }
                contentItem: Text {
                    text: Ui.I18n.t("auth.register")
                    color: Ui.Style.textPrimary
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (registerAccount.length === 0 || registerPassword.length === 0 || registerConfirm.length === 0) {
                        errorText = Ui.I18n.t("auth.error.registerIncomplete")
                        return
                    }
                    if (registerPassword !== registerConfirm) {
                        errorText = Ui.I18n.t("auth.error.passwordMismatch")
                        return
                    }
                    errorText = ""
                    if (!Ui.AuthDisplayStore.registerAccount(registerAccount, registerPassword)) {
                        errorText = Ui.AuthDisplayStore.errorText.length > 0
                            ? Ui.AuthDisplayStore.errorText
                            : Ui.I18n.t("auth.error.registerIncomplete")
                        return
                    }
                    errorText = ""
                    registerPopup.close()
                }
            }
        }
    }

    Popup {
        id: qrPopup
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(root.width - Ui.Style.paddingXL * 2, 300)
        padding: Ui.Style.paddingL
        x: Math.round((root.width - width) / 2)
        y: Math.round((root.height - implicitHeight) / 2)
        onOpened: {
            errorText = ""
            startQrLogin()
        }
        onClosed: stopQrLogin()

        background: Rectangle {
            radius: Ui.Style.radiusLarge
            color: Ui.Style.authCardBg
            border.width: 1
            border.color: Ui.Style.authCardBorder
        }

        contentItem: ColumnLayout {
            spacing: Ui.Style.paddingS

            Label {
                Layout.fillWidth: true
                text: Ui.I18n.t("auth.qrLogin")
                color: Ui.Style.textPrimary
                font.pixelSize: 18
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                id: qrBox
                Layout.alignment: Qt.AlignHCenter
                width: 196
                height: 196
                radius: Ui.Style.radiusMedium
                color: Ui.Style.authFieldBg
                border.width: 1
                border.color: Ui.Style.authFieldBorder

                Image {
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectFit
                    source: Ui.AuthDisplayStore.qrLoginPayload.length > 0
                            ? Ui.AuthDisplayStore.qrLoginImage(qrBox.width)
                            : ""
                    visible: source.length > 0
                }

                Text {
                    anchors.centerIn: parent
                    text: Ui.I18n.t("auth.qr.placeholder")
                    color: Ui.Style.textMuted
                    font.pixelSize: Ui.Style.authMetaTextSize
                    visible: !(Ui.AuthDisplayStore.qrLoginPayload.length > 0)
                }
            }

            Text {
                text: qrSeconds > 0
                      ? Ui.I18n.format("auth.qr.refreshIn", qrSeconds)
                      : Ui.I18n.t("auth.qr.expired")
                color: qrSeconds > 0 ? Ui.Style.textSecondary : Ui.Style.link
                font.pixelSize: Ui.Style.authSubtitleTextSize
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Button {
                text: Ui.I18n.t("auth.qr.refresh")
                flat: true
                Layout.alignment: Qt.AlignHCenter
                Accessible.name: Ui.I18n.t("auth.qr.refresh")
                onClicked: startQrLogin()
                contentItem: Text {
                    text: Ui.I18n.t("auth.qr.refresh")
                    color: Ui.Style.link
                    font.pixelSize: Ui.Style.authBodyTextSize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle { color: "transparent" }
            }
        }
    }

    Connections {
        target: Ui.AuthDisplayStore
        function onWaitingServerTrustChanged() {
            if (waitingServerTrust && !Ui.AuthDisplayStore.waitingServerTrust) {
                attemptLogin(lastLoginAccount, lastLoginPassword, lastLoginRootCode, true)
            }
        }
        function onErrorTextChanged() {
            if (Ui.AuthDisplayStore.errorText.length > 0) {
                errorText = Ui.AuthDisplayStore.errorText
            }
        }
    }
}
