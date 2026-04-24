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
    property bool advancedExpanded: false
    readonly property bool smokeMode: typeof uiSmokeMode !== "undefined" ? !!uiSmokeMode : false
    readonly property bool compactStage: width <= 720 || height <= 760
    readonly property string heroEyebrow: waitingServerTrust
                                          ? (Ui.I18n.usesCjkLocale ? "需要确认服务器信任" : "Server trust review")
                                          : (Ui.I18n.usesCjkLocale ? "安全登录" : "Secure sign in")
    readonly property string heroSubtitle: waitingServerTrust
                                          ? (Ui.I18n.usesCjkLocale
                                             ? "继续之前，请确认当前网关指纹。"
                                             : "Confirm the current gateway fingerprint before continuing.")
                                          : (Ui.I18n.usesCjkLocale
                                             ? "使用账号继续，设备与传输状态会在本地校验。"
                                             : "Continue with your account and verify device and transport state locally.")

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

    function statusTitle() {
        if (errorText.length > 0) {
            return errorText
        }
        return ""
    }

    function statusLine() {
        return statusTitle()
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
            width: 280
            height: 280
            radius: 140
            anchors.top: parent.top
            anchors.topMargin: -72
            anchors.left: parent.left
            anchors.leftMargin: -64
            color: Qt.rgba(51 / 255, 144 / 255, 236 / 255, Ui.Style.isDark ? 0.16 : 0.12)
        }

        Rectangle {
            width: 220
            height: 220
            radius: 110
            anchors.bottom: parent.bottom
            anchors.bottomMargin: -84
            anchors.right: parent.right
            anchors.rightMargin: -48
            color: Qt.rgba(14 / 255, 165 / 255, 233 / 255, Ui.Style.isDark ? 0.10 : 0.08)
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
                radius: Ui.Style.radiusContinuous
                color: Ui.Style.authCardBg
                border.width: 1
                border.color: Ui.Style.authCardBorder
                antialiasing: true
                clip: true

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 108
                    radius: Ui.Style.radiusContinuous
                    color: Ui.Style.alpha(Ui.Style.accent, Ui.Style.isDark ? 0.10 : 0.08)
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: root.compactStage ? Ui.Style.paddingM : Ui.Style.paddingL
                    spacing: root.compactStage ? 8 : 10

                    Rectangle {
                        Layout.fillWidth: true
                        radius: Ui.Style.radiusContinuous
                        color: Ui.Style.authSurfaceStrong
                        border.width: 1
                        border.color: Ui.Style.authContextBorder
                        implicitHeight: heroColumn.implicitHeight + Ui.Style.paddingM * 2

                        ColumnLayout {
                            id: heroColumn
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingM
                            spacing: 8

                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 8

                                Rectangle {
                                    width: 34
                                    height: 34
                                    radius: 17
                                    color: Ui.Style.badgeSurfaceStrong
                                    border.width: 1
                                    border.color: Ui.Style.badgeBorder

                                    Image {
                                        anchors.centerIn: parent
                                        width: 16
                                        height: 16
                                        source: "qrc:/mi/e2ee/ui/icons/chat.svg"
                                        fillMode: Image.PreserveAspectFit
                                        smooth: true
                                        antialiasing: true
                                    }
                                }

                                Rectangle {
                                    radius: 10
                                    color: Ui.Style.sidebarMetaChipBg
                                    border.width: 1
                                    border.color: Ui.Style.sidebarMetaChipBorder
                                    implicitWidth: heroEyebrowLabel.implicitWidth + 14
                                    implicitHeight: 20

                                    Text {
                                        id: heroEyebrowLabel
                                        anchors.centerIn: parent
                                        text: root.heroEyebrow
                                        color: Ui.Style.textSecondary
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                        renderType: Text.NativeRendering
                                        antialiasing: true
                                    }
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: Ui.I18n.t("auth.login")
                                color: Ui.Style.textPrimary
                                font.pixelSize: root.compactStage ? 20 : 22
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                renderType: Text.NativeRendering
                                antialiasing: true
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.heroSubtitle
                                color: Ui.Style.textSecondary
                                font.pixelSize: 12
                                font.weight: Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                                renderType: Text.NativeRendering
                                antialiasing: true
                            }
                        }
                    }

                    RowLayout {
                        id: authModeTabs
                        Layout.fillWidth: true
                        Layout.preferredHeight: 0
                        visible: false
                        spacing: Ui.Style.paddingS

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            radius: 15
                            color: Ui.Style.dialogSelectedBg
                            border.width: 1
                            border.color: Ui.Style.tgActiveRowBorder

                            Image {
                                anchors.centerIn: parent
                                width: 14
                                height: 14
                                source: "qrc:/mi/e2ee/ui/icons/login.svg"
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                            }

                            MouseArea {
                                id: accountMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (qrPopup.visible) {
                                        qrPopup.close()
                                    }
                                    accountField.forceActiveFocus()
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            radius: 15
                            color: Ui.Style.topBarPillBg
                            border.width: 1
                            border.color: Ui.Style.topBarPillBorder

                            Image {
                                anchors.centerIn: parent
                                width: 14
                                height: 14
                                source: "qrc:/mi/e2ee/ui/icons/qrcode.svg"
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                            }

                            MouseArea {
                                id: qrModeMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: qrPopup.open()
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        visible: statusLine().length > 0
                        radius: 14
                        color: Ui.Style.authSurfaceStrong
                        border.width: 1
                        border.color: errorText.length > 0
                                      ? Ui.Style.alpha(Ui.Style.danger, 0.28)
                                      : Ui.Style.authContextBorder
                        implicitHeight: statusRow.implicitHeight + Ui.Style.paddingS * 2

                        RowLayout {
                            id: statusRow
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingS
                            spacing: Ui.Style.paddingS

                            Rectangle {
                                width: 20
                                height: 20
                                radius: 10
                                color: errorText.length > 0
                                       ? Ui.Style.alpha(Ui.Style.danger, 0.12)
                                       : Ui.Style.alpha(Ui.Style.accent, 0.10)

                                Text {
                                    anchors.centerIn: parent
                                    text: errorText.length > 0 ? "!" : "\u2022"
                                    color: errorText.length > 0 ? Ui.Style.danger : Ui.Style.accent
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                            }

                            Text {
                                Layout.fillWidth: true
                                text: statusTitle()
                                color: errorText.length > 0
                                       ? Ui.Style.danger
                                       : Ui.Style.textSecondary
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                wrapMode: Text.WordWrap
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        radius: Ui.Style.radiusContinuous
                        color: Ui.Style.authSurfaceStrong
                        border.width: 1
                        border.color: Ui.Style.authContextBorder
                        implicitHeight: loginPageLayout.implicitHeight + Ui.Style.paddingM * 2

                        ColumnLayout {
                            id: loginPageLayout
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingM
                            spacing: Ui.Style.paddingS

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
                                visible: advancedExpanded || rootCodeInput.length > 0
                                labelText: Ui.I18n.t("auth.placeholder.rootCode")
                                placeholderText: Ui.I18n.t("auth.placeholder.rootCode")
                                descriptionText: ""
                                onTextChanged: rootCodeInput = text
                            }

                            Components.PrimaryButton {
                                id: loginButton
                                Layout.fillWidth: true
                                Layout.preferredHeight: Ui.Style.authPrimaryButtonHeight
                                text: Ui.I18n.t("auth.login")
                                Accessible.name: Ui.I18n.t("auth.login")
                                enabled: accountInput.length > 0 && passwordInput.length > 0
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
                                Layout.fillWidth: true
                                spacing: Ui.Style.paddingS

                                Components.GhostButton {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
                                    text: Ui.I18n.t("auth.qrLogin")
                                    Accessible.name: Ui.I18n.t("auth.qrLogin")
                                    onClicked: qrPopup.open()
                                }

                                Components.GhostButton {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 34
                                    text: Ui.I18n.t("auth.register")
                                    Accessible.name: Ui.I18n.t("auth.register")
                                    onClicked: registerPopup.open()
                                }
                            }

                            Button {
                                id: advancedAccessButton
                                Layout.alignment: Qt.AlignHCenter
                                text: advancedExpanded
                                      ? Ui.I18n.t("auth.advanced.hide")
                                      : Ui.I18n.t("auth.advanced")
                                Accessible.name: text
                                background: Rectangle {
                                    radius: 10
                                    color: Ui.Style.sidebarMetaChipBg
                                    border.width: 1
                                    border.color: Ui.Style.sidebarMetaChipBorder
                                }
                                contentItem: Text {
                                    text: advancedAccessButton.text
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: advancedExpanded = !advancedExpanded
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
            radius: Ui.Style.radiusContinuous
            color: Ui.Style.authCardBg
            border.width: 1
            border.color: Ui.Style.authCardBorder
        }

        contentItem: ColumnLayout {
            spacing: Ui.Style.paddingS

            Text {
                Layout.fillWidth: true
                text: Ui.I18n.usesCjkLocale ? "创建新账号" : "Create an account"
                color: Ui.Style.textSecondary
                font.pixelSize: 10
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                maximumLineCount: 1
                elide: Text.ElideRight
                renderType: Text.NativeRendering
                antialiasing: true
            }

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
            radius: Ui.Style.radiusContinuous
            color: Ui.Style.authCardBg
            border.width: 1
            border.color: Ui.Style.authCardBorder
        }

        contentItem: ColumnLayout {
            spacing: Ui.Style.paddingS

            Text {
                Layout.fillWidth: true
                text: Ui.I18n.usesCjkLocale ? "从已登录设备扫描" : "Scan from a signed-in device"
                color: Ui.Style.textSecondary
                font.pixelSize: 10
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                maximumLineCount: 1
                elide: Text.ElideRight
                renderType: Text.NativeRendering
                antialiasing: true
            }

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 34
                height: 34
                radius: 17
                color: Ui.Style.badgeSurfaceStrong
                border.width: 1
                border.color: Ui.Style.badgeBorder

                Image {
                    anchors.centerIn: parent
                    width: 16
                    height: 16
                    source: "qrc:/mi/e2ee/ui/icons/qrcode.svg"
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    antialiasing: true
                }
            }

            Label {
                Layout.fillWidth: true
                text: Ui.I18n.t("auth.qrLogin")
                color: Ui.Style.textPrimary
                font.pixelSize: 18
                font.weight: Font.DemiBold
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                maximumLineCount: 1
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
