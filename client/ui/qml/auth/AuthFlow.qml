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
            opacity: 0.16
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
                    spacing: Ui.Style.paddingS

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

                    Components.SecurityBadge {
                        labelText: Ui.AuthDisplayStore.gatewayState.length > 0
                                   ? Ui.AuthDisplayStore.gatewayState
                                   : Ui.I18n.t("auth.hero.badge")
                        detailText: Ui.AuthDisplayStore.gatewayDetail
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
                        Layout.preferredHeight: 1
                        color: Ui.Style.authTitleBarBorder
                    }

                    TabBar {
                        id: authTabBar
                        Layout.fillWidth: true
                        currentIndex: 0
                        spacing: Ui.Style.paddingXS
                        background: Rectangle {
                            radius: Ui.Style.radiusMedium
                            color: Ui.Style.authTabRail
                            border.width: 1
                            border.color: Ui.Style.authContextBorder
                        }

                        TabButton {
                            text: Ui.I18n.t("auth.login")
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: authTabBar.currentIndex === 0 ? Ui.Style.dialogSelectedBg : "transparent"
                            }
                            contentItem: Text {
                                text: parent.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: Ui.Style.authSubtitleTextSize
                                font.weight: Font.DemiBold
                                color: authTabBar.currentIndex === 0 ? Ui.Style.textPrimary : Ui.Style.textSecondary
                            }
                        }

                        TabButton {
                            text: Ui.I18n.t("auth.registerAccount")
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: authTabBar.currentIndex === 1 ? Ui.Style.dialogSelectedBg : "transparent"
                            }
                            contentItem: Text {
                                text: parent.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: Ui.Style.authSubtitleTextSize
                                font.weight: Font.DemiBold
                                color: authTabBar.currentIndex === 1 ? Ui.Style.textPrimary : Ui.Style.textSecondary
                            }
                        }

                        TabButton {
                            text: Ui.I18n.t("auth.qrLogin")
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: authTabBar.currentIndex === 2 ? Ui.Style.dialogSelectedBg : "transparent"
                            }
                            contentItem: Text {
                                text: parent.text
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                font.pixelSize: Ui.Style.authSubtitleTextSize
                                font.weight: Font.DemiBold
                                color: authTabBar.currentIndex === 2 ? Ui.Style.textPrimary : Ui.Style.textSecondary
                            }
                        }

                        onCurrentIndexChanged: {
                            if (loginStack.currentIndex !== currentIndex) {
                                loginStack.currentIndex = currentIndex
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        StackLayout {
                            id: loginStack
                            anchors.fill: parent
                            currentIndex: 0
                            onCurrentIndexChanged: {
                                errorText = ""
                                if (authTabBar.currentIndex !== currentIndex) {
                                    authTabBar.currentIndex = currentIndex
                                }
                                if (currentIndex === 2) {
                                    startQrLogin()
                                } else {
                                    stopQrLogin()
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

                                    }
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: registerPageLayout.implicitHeight

                                    ColumnLayout {
                                        id: registerPageLayout
                                        width: parent.width
                                        spacing: Ui.Style.paddingS

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
                                                loginStack.currentIndex = 0
                                            }
                                        }

                                    }
                                }

                                Item {
                                    Layout.fillWidth: true
                                    implicitHeight: qrPageLayout.implicitHeight

                                    ColumnLayout {
                                        id: qrPageLayout
                                        width: parent.width
                                        spacing: Ui.Style.paddingS

                                        Rectangle {
                                            id: qrBox
                                            Layout.alignment: Qt.AlignHCenter
                                            width: 200
                                            height: 200
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

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Ui.Style.paddingS
                                            Item { Layout.fillWidth: true }

                                            Button {
                                                text: Ui.I18n.t("auth.qr.refresh")
                                                flat: true
                                                Accessible.name: Ui.I18n.t("auth.qr.refresh")
                                                onClicked: startQrLogin()
                                                contentItem: Text {
                                                    text: Ui.I18n.t("auth.qr.refresh")
                                                    color: Ui.Style.link
                                                    font.pixelSize: Ui.Style.authBodyTextSize
                                                }
                                                background: Rectangle { color: "transparent" }
                                            }

                                            Item { Layout.fillWidth: true }
                                        }

                                    }
                                }
                            }
                        }

                    Components.StatusBanner {
                        Layout.fillWidth: true
                        tone: statusTone()
                        titleText: statusTitle()
                        detailText: statusDetail()
                    }
                }
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
