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
        radius: 18
        color: Qt.rgba(0.11, 0.14, 0.19, 0.89)
        border.color: Qt.rgba(1, 1, 1, 0.08)
        antialiasing: true
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Ui.Style.paddingL
            spacing: Ui.Style.paddingM

            Item {
                id: titleBar
                Layout.fillWidth: true
                Layout.preferredHeight: Ui.Style.topBarHeight

                RowLayout {
                    anchors.fill: parent
                    spacing: Ui.Style.paddingS

                    Item { Layout.fillWidth: true }
                    ToolButton {
                        id: menuButton
                        icon.source: "qrc:/mi/e2ee/ui/icons/menu-lines.svg"
                        icon.width: 16
                        icon.height: 16
                        onClicked: menuPopup.popup(menuButton, 0, menuButton.height)
                        background: Rectangle {
                            radius: 6
                            color: menuButton.down ? Ui.Style.pressedBg : "transparent"
                        }
                    }
                    ToolButton {
                        id: closeButton
                        icon.source: "qrc:/mi/e2ee/ui/icons/close-x.svg"
                        icon.width: 16
                        icon.height: 16
                        onClicked: Qt.quit()
                        background: Rectangle {
                            radius: 6
                            color: closeButton.down ? Ui.Style.pressedBg : "transparent"
                        }
                    }
                }
            }

            Menu {
                id: menuPopup
                property int sidePadding: 20
                property string textSettings: Ui.I18n.t("auth.menu.settings")
                property string textHelp: Ui.I18n.t("auth.menu.help")
                property string textAbout: Ui.I18n.t("auth.menu.about")
                readonly property real maxItemWidth: Math.max(metricsSettings.width,
                                                             metricsHelp.width,
                                                             metricsAbout.width)
                implicitWidth: Math.ceil(maxItemWidth + sidePadding * 2)

                TextMetrics {
                    id: metricsSettings
                    text: menuPopup.textSettings
                    font: menuPopup.font
                }
                TextMetrics {
                    id: metricsHelp
                    text: menuPopup.textHelp
                    font: menuPopup.font
                }
                TextMetrics {
                    id: metricsAbout
                    text: menuPopup.textAbout
                    font: menuPopup.font
                }

                MenuItem {
                    text: menuPopup.textSettings
                    implicitWidth: menuPopup.implicitWidth
                    leftPadding: menuPopup.sidePadding
                    rightPadding: menuPopup.sidePadding
                    contentItem: Text {
                        text: parent.text
                        color: Ui.Style.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.fill: parent
                    }
                }
                MenuItem {
                    text: menuPopup.textHelp
                    implicitWidth: menuPopup.implicitWidth
                    leftPadding: menuPopup.sidePadding
                    rightPadding: menuPopup.sidePadding
                    contentItem: Text {
                        text: parent.text
                        color: Ui.Style.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.fill: parent
                    }
                }
                MenuItem {
                    text: menuPopup.textAbout
                    implicitWidth: menuPopup.implicitWidth
                    leftPadding: menuPopup.sidePadding
                    rightPadding: menuPopup.sidePadding
                    contentItem: Text {
                        text: parent.text
                        color: Ui.Style.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        anchors.fill: parent
                    }
                }
            }

            Label {
                text: Ui.I18n.t("auth.title")
                font.pixelSize: 22
                font.weight: Font.DemiBold
                color: Ui.Style.textPrimary
            }
            Label {
                text: Ui.I18n.t("auth.subtitle")
                font.pixelSize: 14
                color: Ui.Style.textSecondary
            }

            StackLayout {
                id: loginStack
                Layout.fillWidth: true
                Layout.fillHeight: true
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
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Ui.Style.paddingM

                        Components.SecureTextField {
                            Layout.fillWidth: true
                            placeholderText: Ui.I18n.t("auth.placeholder.account")
                            font.pixelSize: 14
                            color: "#FFFFFF"
                            placeholderTextColor: Ui.Style.textSecondary
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: accountInput = text
                        }

                        Components.SecureTextField {
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: Ui.I18n.t("auth.placeholder.password")
                            font.pixelSize: 14
                            color: "#FFFFFF"
                            placeholderTextColor: Ui.Style.textSecondary
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: passwordInput = text
                        }

                        Components.SecureTextField {
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: Ui.I18n.t("auth.placeholder.rootCode")
                            font.pixelSize: 14
                            color: "#FFFFFF"
                            placeholderTextColor: Ui.Style.textSecondary
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: rootCodeInput = text
                        }

                        CheckBox {
                            text: Ui.I18n.t("auth.autoLogin")
                            font.pixelSize: 14
                        }

                        Button {
                            text: Ui.I18n.t("auth.login")
                            Layout.fillWidth: true
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.accent
                            }
                            contentItem: Text {
                                text: Ui.I18n.t("auth.login")
                                color: Ui.Style.textPrimary
                                font.pixelSize: 16
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
                            Item { Layout.fillWidth: true }
                            Button {
                                text: Ui.I18n.t("auth.registerAccount")
                                flat: true
                                onClicked: loginStack.currentIndex = 1
                                contentItem: Text {
                                    text: Ui.I18n.t("auth.registerAccount")
                                    color: Ui.Style.link
                                    font.pixelSize: 14
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
                                    font.pixelSize: 14
                                }
                                background: Rectangle { color: "transparent" }
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Ui.Style.paddingM

                        Components.SecureTextField {
                            Layout.fillWidth: true
                            placeholderText: Ui.I18n.t("auth.register.placeholder.account")
                            font.pixelSize: 14
                            color: "#FFFFFF"
                            placeholderTextColor: Ui.Style.textSecondary
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: registerAccount = text
                        }

                        Components.SecureTextField {
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: Ui.I18n.t("auth.register.placeholder.password")
                            font.pixelSize: 14
                            color: "#FFFFFF"
                            placeholderTextColor: Ui.Style.textSecondary
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: registerPassword = text
                        }

                        Components.SecureTextField {
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: Ui.I18n.t("auth.register.placeholder.confirm")
                            font.pixelSize: 14
                            color: "#FFFFFF"
                            placeholderTextColor: Ui.Style.textSecondary
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: registerConfirm = text
                        }

                        Button {
                            text: Ui.I18n.t("auth.register")
                            Layout.fillWidth: true
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.accent
                            }
                            contentItem: Text {
                                text: Ui.I18n.t("auth.register")
                                color: Ui.Style.textPrimary
                                font.pixelSize: 16
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
                            Item { Layout.fillWidth: true }
                            Button {
                                text: Ui.I18n.t("auth.register.backLogin")
                                flat: true
                                onClicked: loginStack.currentIndex = 0
                                contentItem: Text {
                                    text: Ui.I18n.t("auth.register.backLogin")
                                    color: Ui.Style.link
                                    font.pixelSize: 14
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
                                    font.pixelSize: 14
                                }
                                background: Rectangle { color: "transparent" }
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Ui.Style.paddingM

                        Rectangle {
                            id: qrBox
                            Layout.alignment: Qt.AlignHCenter
                            width: 200
                            height: 200
                            radius: Ui.Style.radiusMedium
                            color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                            border.color: Ui.Style.borderSubtle
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
                                font.pixelSize: 12
                                visible: !(clientBridge && clientBridge.qrLoginPayload.length > 0)
                            }
                        }

                        Text {
                            text: qrSeconds > 0
                                  ? Ui.I18n.format("auth.qr.refreshIn", qrSeconds)
                                  : Ui.I18n.t("auth.qr.expired")
                            color: qrSeconds > 0 ? Ui.Style.textSecondary : Ui.Style.link
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            Button {
                                text: Ui.I18n.t("auth.qr.refresh")
                                flat: true
                                onClicked: startQrLogin()
                                contentItem: Text {
                                    text: Ui.I18n.t("auth.qr.refresh")
                                    color: Ui.Style.link
                                    font.pixelSize: 14
                                }
                                background: Rectangle { color: "transparent" }
                            }
                            Item { Layout.fillWidth: true }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            Button {
                                text: Ui.I18n.t("auth.register.backLogin")
                                flat: true
                                onClicked: loginStack.currentIndex = 0
                                contentItem: Text {
                                    text: Ui.I18n.t("auth.register.backLogin")
                                    color: Ui.Style.link
                                    font.pixelSize: 14
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
                                    font.pixelSize: 14
                                }
                                background: Rectangle { color: "transparent" }
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            Text {
                text: errorText
                color: Ui.Style.danger
                font.pixelSize: 13
                visible: errorText.length > 0
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
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
