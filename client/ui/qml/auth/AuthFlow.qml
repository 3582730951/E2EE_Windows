import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root

    property string accountInput: ""
    property string passwordInput: ""
    property string registerAccount: ""
    property string registerPassword: ""
    property string registerConfirm: ""
    property int qrSeconds: 30
    property string errorText: ""
    property string lastLoginAccount: ""
    property string lastLoginPassword: ""
    property bool waitingServerTrust: false

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
            }
        }
    }

    function completeAuth() {
        authSucceeded()
        Ui.AppStore.currentPage = 1
    }

    function attemptLogin(user, pass, fromTrust) {
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
        if (!clientBridge.login(user, pass)) {
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
                        resetQrTimer()
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
                            placeholderTextColor: Qt.rgba(1, 1, 1, 0.55)
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
                            placeholderTextColor: Qt.rgba(1, 1, 1, 0.55)
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: passwordInput = text
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
                                attemptLogin(accountInput, passwordInput, false)
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
                            placeholderTextColor: Qt.rgba(1, 1, 1, 0.55)
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
                            placeholderTextColor: Qt.rgba(1, 1, 1, 0.55)
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
                            placeholderTextColor: Qt.rgba(1, 1, 1, 0.55)
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
                            Canvas {
                                anchors.fill: parent
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.fillStyle = "#0b0b0c"
                                    var size = width
                                    var cells = 21
                                    var cell = Math.floor(size / cells)
                                    function drawMarker(x, y) {
                                        for (var iy = 0; iy < 7; ++iy) {
                                            for (var ix = 0; ix < 7; ++ix) {
                                                var border = ix === 0 || ix === 6 || iy === 0 || iy === 6
                                                var inner = ix >= 2 && ix <= 4 && iy >= 2 && iy <= 4
                                                if (border || inner) {
                                                    ctx.fillRect((x + ix) * cell, (y + iy) * cell, cell, cell)
                                                }
                                            }
                                        }
                                    }
                                    drawMarker(0, 0)
                                    drawMarker(cells - 7, 0)
                                    drawMarker(0, cells - 7)
                                    for (var y = 0; y < cells; ++y) {
                                        for (var x = 0; x < cells; ++x) {
                                            var inMarker = (x < 7 && y < 7) ||
                                                           (x >= cells - 7 && y < 7) ||
                                                           (x < 7 && y >= cells - 7)
                                            if (inMarker) {
                                                continue
                                            }
                                            if (((x * 7 + y * 11) % 13) < 5) {
                                                ctx.fillRect(x * cell, y * cell, cell, cell)
                                            }
                                        }
                                    }
                                }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    errorText = ""
                                    completeAuth()
                                }
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
                                onClicked: resetQrTimer()
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
                attemptLogin(lastLoginAccount, lastLoginPassword, true)
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
