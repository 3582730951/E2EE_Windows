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
    property bool advancedLoginExpanded: false

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
        Ui.AppStore.currentPage = 1
    }

    function attemptLogin(user, pass, rootCode, fromTrust) {
        if (!clientBridge) {
            errorText = Ui.I18n.t("auth.error.login")
            return false
        }
        if (!clientBridge.init("")) {
            errorText = clientBridge.lastError.length
                ? clientBridge.lastError
                : Ui.I18n.t("auth.error.login")
            return false
        }
        if (!clientBridge.loginWithRootCode(user, pass, rootCode)) {
            if (clientBridge.hasPendingServerTrust) {
                waitingServerTrust = true
                if (!fromTrust) {
                    errorText = "需信任服务器（TLS）"
                }
            } else if (clientBridge.lastError.length) {
                errorText = clientBridge.lastError
            } else {
                errorText = Ui.I18n.t("auth.error.login")
            }
            return false
        }
        waitingServerTrust = false
        errorText = ""
        Ui.AppStore.bootstrapAfterLogin()
        completeAuth()
        return true
    }

    function startQrLogin() {
        if (!clientBridge) {
            errorText = Ui.I18n.t("auth.error.login")
            return false
        }
        if (!clientBridge.init("")) {
            errorText = clientBridge.lastError.length
                ? clientBridge.lastError
                : Ui.I18n.t("auth.error.login")
            return false
        }
        var user = accountInput
        if (!clientBridge.beginQrLogin(user)) {
            errorText = clientBridge.lastError.length
                ? clientBridge.lastError
                : Ui.I18n.t("auth.error.login")
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
        if (!qrActive || !clientBridge) {
            return
        }
        if (!clientBridge.pollQrLogin()) {
            errorText = clientBridge.lastError.length
                ? clientBridge.lastError
                : Ui.I18n.t("auth.error.login")
            stopQrLogin()
            return
        }
        if (clientBridge.loggedIn) {
            errorText = ""
            stopQrLogin()
            Ui.AppStore.bootstrapAfterLogin()
            completeAuth()
        }
    }

    function stopQrLogin() {
        if (clientBridge) {
            clientBridge.cancelQrLogin()
        }
        qrActive = false
        qrPollTimer.stop()
        qrTimer.stop()
    }

    function resetQrTimer() {
        qrSeconds = 30
        qrTimer.restart()
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
            width: parent.width * 0.64
            height: parent.height * 0.82
            anchors.left: parent.left
            anchors.leftMargin: -parent.width * 0.10
            anchors.verticalCenter: parent.verticalCenter
            radius: width / 2
            color: Ui.Style.authGlowPrimary
            opacity: 0.55
        }

        Rectangle {
            width: parent.width * 0.44
            height: parent.height * 0.62
            anchors.right: parent.right
            anchors.rightMargin: -parent.width * 0.06
            anchors.top: parent.top
            anchors.topMargin: parent.height * 0.18
            radius: width / 2
            color: Ui.Style.authGlowSecondary
            opacity: 0.50
        }

        Item {
            id: authStage
            width: Math.min(parent.width - Ui.Style.paddingXL * 2, Ui.Style.authStageWidth)
            height: Math.min(parent.height - Ui.Style.paddingXL * 2, Ui.Style.authStageHeight)
            anchors.centerIn: parent

            RowLayout {
                anchors.fill: parent
                spacing: Ui.Style.paddingL

                Rectangle {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    Layout.preferredWidth: authStage.width * 0.50
                    radius: Ui.Style.radiusXL
                    color: Ui.Style.authContextBg
                    border.width: 1
                    border.color: Ui.Style.authContextBorder
                    antialiasing: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingXL
                        spacing: Ui.Style.paddingM

                        Rectangle {
                            Layout.preferredHeight: 28
                            radius: 14
                            color: Ui.Style.authBadgeBg
                            border.width: 1
                            border.color: Ui.Style.authBadgeBorder
                            Layout.alignment: Qt.AlignLeft
                            implicitWidth: contextBadge.implicitWidth + Ui.Style.paddingL * 2

                            Label {
                                id: contextBadge
                                anchors.centerIn: parent
                                text: Ui.I18n.t("auth.hero.badge")
                                color: Ui.Style.authBadgeText
                                font.pixelSize: Ui.Style.authMetaTextSize
                                font.weight: Font.DemiBold
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: Ui.I18n.t("auth.title")
                            color: Ui.Style.textPrimary
                            font.pixelSize: Ui.Style.authTitleTextSize
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: Ui.I18n.t("auth.subtitle")
                            color: Ui.Style.textSecondary
                            font.pixelSize: Ui.Style.authBodyTextSize
                            wrapMode: Text.Wrap
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: securityCopy.implicitHeight + Ui.Style.paddingL * 2
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.authSurfaceStrong
                            border.width: 1
                            border.color: Ui.Style.authCardBorder

                            Column {
                                id: securityCopy
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: Ui.Style.paddingL
                                spacing: Ui.Style.paddingS

                                Label {
                                    text: "安全基线"
                                    color: Ui.Style.authLabelText
                                    font.pixelSize: Ui.Style.authBodyTextSize
                                    font.weight: Font.DemiBold
                                }

                                Repeater {
                                    model: [
                                        "登录链路加密并校验服务器信任",
                                        "会话消息采用前向安全轮换机制",
                                        "设备与离线文件密钥隔离存储"
                                    ]

                                    delegate: Label {
                                        width: securityCopy.width
                                        text: "\u2022 " + modelData
                                        color: Ui.Style.textSecondary
                                        font.pixelSize: Ui.Style.authSubtitleTextSize
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        Label {
                            Layout.fillWidth: true
                            text: "登录后将进入安全会话主界面"
                            color: Ui.Style.textMuted
                            font.pixelSize: Ui.Style.authMetaTextSize
                            wrapMode: Text.Wrap
                        }
                    }
                }

                Rectangle {
                    id: authPanel
                    Layout.preferredWidth: Ui.Style.authPanelWidth
                    Layout.fillHeight: true
                    radius: Ui.Style.radiusXL
                    color: Ui.Style.authCardBg
                    border.width: 1
                    border.color: Ui.Style.authCardBorder
                    antialiasing: true
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingL
                        spacing: Ui.Style.paddingM

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 108
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.authPanelHeaderBg
                            border.width: 1
                            border.color: Ui.Style.authTitleBarBorder

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingL
                                spacing: Ui.Style.paddingXs

                                Label {
                                    text: Ui.I18n.t("auth.title")
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 24
                                    font.weight: Font.DemiBold
                                    Layout.fillWidth: true
                                }

                                Label {
                                    text: Ui.I18n.t("auth.subtitle")
                                    color: Ui.Style.textSecondary
                                    font.pixelSize: Ui.Style.authSubtitleTextSize
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
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
                                        }

                                        Components.SecureTextField {
                                            id: accountField
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: Ui.Style.authFieldHeight
                                            placeholderText: Ui.I18n.t("auth.placeholder.account")
                                            font.pixelSize: Ui.Style.authBodyTextSize
                                            color: Ui.Style.textPrimary
                                            placeholderTextColor: Ui.Style.authPlaceholderText
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

                                        Button {
                                            id: advancedToggleButton
                                            Layout.alignment: Qt.AlignLeft
                                            flat: true
                                            text: advancedLoginExpanded ? "收起高级选项" : "高级选项"
                                            onClicked: advancedLoginExpanded = !advancedLoginExpanded
                                            contentItem: Text {
                                                text: advancedToggleButton.text
                                                color: advancedToggleButton.hovered || advancedToggleButton.down
                                                       ? Ui.Style.textPrimary
                                                       : Ui.Style.authBadgeText
                                                font.pixelSize: Ui.Style.authSubtitleTextSize
                                                font.weight: Font.Medium
                                                horizontalAlignment: Text.AlignLeft
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                radius: Ui.Style.radiusSmall
                                                color: advancedToggleButton.down
                                                       ? Ui.Style.authInfoBorder
                                                       : (advancedToggleButton.hovered ? Ui.Style.authInfoBg : "transparent")
                                                border.width: advancedToggleButton.hovered || advancedToggleButton.down ? 1 : 0
                                                border.color: Ui.Style.authBadgeBorder
                                            }
                                        }

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: Ui.Style.paddingS
                                            visible: advancedLoginExpanded

                                            Label {
                                                text: Ui.I18n.t("auth.placeholder.rootCode")
                                                font.pixelSize: Ui.Style.authSubtitleTextSize
                                                font.weight: Font.Medium
                                                color: Ui.Style.authLabelText
                                                Layout.fillWidth: true
                                            }

                                            Components.SecureTextField {
                                                id: rootCodeField
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: Ui.Style.authFieldHeight
                                                echoMode: TextInput.Password
                                                placeholderText: Ui.I18n.t("auth.placeholder.rootCode")
                                                font.pixelSize: Ui.Style.authBodyTextSize
                                                color: Ui.Style.textPrimary
                                                placeholderTextColor: Ui.Style.authPlaceholderText
                                                background: Rectangle {
                                                    radius: Ui.Style.radiusMedium
                                                    color: Ui.Style.authFieldBg
                                                    border.width: 1
                                                    border.color: rootCodeField.activeFocus
                                                                  ? Ui.Style.authFieldFocus
                                                                  : Ui.Style.authFieldBorder
                                                }
                                                onTextChanged: rootCodeInput = text
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                Item { Layout.fillWidth: true }

                                                Button {
                                                    id: advancedQrLinkButton
                                                    text: Ui.I18n.t("auth.qrLogin")
                                                    flat: true
                                                    onClicked: loginStack.currentIndex = 2
                                                    contentItem: Text {
                                                        text: Ui.I18n.t("auth.qrLogin")
                                                        color: advancedQrLinkButton.hovered || advancedQrLinkButton.down
                                                               ? Ui.Style.textPrimary
                                                               : Ui.Style.authBadgeText
                                                        font.pixelSize: Ui.Style.authSubtitleTextSize
                                                        font.weight: Font.Medium
                                                        horizontalAlignment: Text.AlignHCenter
                                                        verticalAlignment: Text.AlignVCenter
                                                    }
                                                    background: Rectangle {
                                                        radius: Ui.Style.radiusMedium
                                                        color: advancedQrLinkButton.down ? Ui.Style.authInfoBorder
                                                                                         : (advancedQrLinkButton.hovered ? Ui.Style.authInfoBg : "transparent")
                                                        border.width: advancedQrLinkButton.hovered || advancedQrLinkButton.down ? 1 : 0
                                                        border.color: Ui.Style.authBadgeBorder
                                                    }
                                                }

                                                Item { Layout.fillWidth: true }
                                            }
                                        }

                                        Button {
                                            id: loginButton
                                            text: Ui.I18n.t("auth.login")
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: Ui.Style.authPrimaryButtonHeight
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
                                            Layout.fillWidth: true
                                            spacing: Ui.Style.paddingS
                                            Item { Layout.fillWidth: true }

                                            Button {
                                                id: registerLinkButton
                                                text: Ui.I18n.t("auth.registerAccount")
                                                flat: true
                                                onClicked: loginStack.currentIndex = 1
                                                contentItem: Text {
                                                    text: Ui.I18n.t("auth.registerAccount")
                                                    color: registerLinkButton.hovered || registerLinkButton.down
                                                           ? Ui.Style.textPrimary
                                                           : Ui.Style.authBadgeText
                                                    font.pixelSize: Ui.Style.authBodyTextSize
                                                    font.weight: Font.DemiBold
                                                    horizontalAlignment: Text.AlignHCenter
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                                background: Rectangle {
                                                    radius: Ui.Style.radiusMedium
                                                    color: registerLinkButton.down ? Ui.Style.authInfoBorder
                                                                                   : (registerLinkButton.hovered ? Ui.Style.authInfoBg : "transparent")
                                                    border.width: registerLinkButton.hovered || registerLinkButton.down ? 1 : 0
                                                    border.color: Ui.Style.authBadgeBorder
                                                }
                                            }

                                            Item { Layout.fillWidth: true }
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
                                                if (clientBridge && !clientBridge.init("")) {
                                                    errorText = clientBridge.lastError.length
                                                        ? clientBridge.lastError
                                                        : Ui.I18n.t("auth.error.registerIncomplete")
                                                    return
                                                }
                                                if (!clientBridge || !clientBridge.registerUser(registerAccount, registerPassword)) {
                                                    errorText = clientBridge && clientBridge.lastError.length
                                                        ? clientBridge.lastError
                                                        : Ui.I18n.t("auth.error.registerIncomplete")
                                                    return
                                                }
                                                errorText = "注册成功，请登录"
                                                loginStack.currentIndex = 0
                                            }
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Ui.Style.paddingS
                                            Item { Layout.fillWidth: true }

                                            Button {
                                                text: Ui.I18n.t("auth.register.backLogin")
                                                flat: true
                                                onClicked: loginStack.currentIndex = 0
                                                contentItem: Text {
                                                    text: Ui.I18n.t("auth.register.backLogin")
                                                    color: Ui.Style.link
                                                    font.pixelSize: Ui.Style.authBodyTextSize
                                                }
                                                background: Rectangle { color: "transparent" }
                                            }

                                            Button {
                                                text: Ui.I18n.t("auth.qrLogin")
                                                flat: true
                                                onClicked: loginStack.currentIndex = 2
                                                contentItem: Text {
                                                    text: Ui.I18n.t("auth.qrLogin")
                                                    color: Ui.Style.link
                                                    font.pixelSize: Ui.Style.authBodyTextSize
                                                }
                                                background: Rectangle { color: "transparent" }
                                            }

                                            Item { Layout.fillWidth: true }
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
                                                source: clientBridge && clientBridge.qrLoginPayload.length > 0
                                                        ? clientBridge.qrLoginImage(qrBox.width)
                                                        : ""
                                                visible: source.length > 0
                                            }

                                            Text {
                                                anchors.centerIn: parent
                                                text: Ui.I18n.t("auth.qr.placeholder")
                                                color: Ui.Style.textMuted
                                                font.pixelSize: Ui.Style.authMetaTextSize
                                                visible: !(clientBridge && clientBridge.qrLoginPayload.length > 0)
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
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Ui.Style.paddingS
                                            Item { Layout.fillWidth: true }

                                            Button {
                                                text: Ui.I18n.t("auth.qr.refresh")
                                                flat: true
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

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Ui.Style.paddingS
                                            Item { Layout.fillWidth: true }

                                            Button {
                                                text: Ui.I18n.t("auth.register.backLogin")
                                                flat: true
                                                onClicked: loginStack.currentIndex = 0
                                                contentItem: Text {
                                                    text: Ui.I18n.t("auth.register.backLogin")
                                                    color: Ui.Style.link
                                                    font.pixelSize: Ui.Style.authBodyTextSize
                                                }
                                                background: Rectangle { color: "transparent" }
                                            }

                                            Button {
                                                text: Ui.I18n.t("auth.registerAccount")
                                                flat: true
                                                onClicked: loginStack.currentIndex = 1
                                                contentItem: Text {
                                                    text: Ui.I18n.t("auth.registerAccount")
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

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 44
                            radius: Ui.Style.radiusMedium
                            color: Ui.Style.authPanelFooterBg
                            border.width: 1
                            border.color: errorText.length > 0 ? Ui.Style.authDangerBorder : Ui.Style.authTitleBarBorder

                            Label {
                                anchors.fill: parent
                                anchors.leftMargin: Ui.Style.paddingM
                                anchors.rightMargin: Ui.Style.paddingM
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignHCenter
                                text: errorText.length > 0 ? errorText : "连接后将自动同步会话安全状态"
                                color: errorText.length > 0 ? Ui.Style.danger : Ui.Style.textMuted
                                font.pixelSize: Ui.Style.authSubtitleTextSize
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: clientBridge
        function onTrustStateChanged() {
            if (waitingServerTrust && clientBridge && !clientBridge.hasPendingServerTrust) {
                attemptLogin(lastLoginAccount, lastLoginPassword, lastLoginRootCode, true)
            }
        }
        function onErrorChanged() {
            if (waitingServerTrust && clientBridge && clientBridge.hasPendingServerTrust) {
                if (clientBridge.lastError.length > 0) {
                    errorText = clientBridge.lastError
                }
            }
        }
    }
}
