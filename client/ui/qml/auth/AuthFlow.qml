import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: root

    property string accountInput: ""
    property string passwordInput: ""
    property string registerAccount: ""
    property string registerPassword: ""
    property string registerConfirm: ""
    property int qrSeconds: 30
    property string errorText: ""

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

        MouseArea {
            id: dragArea
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onPressed: function(mouse) {
                if (root.window && root.window.startSystemMove) {
                    root.window.startSystemMove()
                }
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Ui.Style.paddingL
            spacing: Ui.Style.paddingM

            RowLayout {
                Layout.fillWidth: true
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

            Menu {
                id: menuPopup
                MenuItem { text: "设置" }
                MenuItem { text: "帮助" }
                MenuItem { text: "关于" }
            }

            Label {
                text: "账号登录"
                font.pixelSize: 22
                font.weight: Font.DemiBold
                color: Ui.Style.textPrimary
            }
            Label {
                text: "安全登录，开启加密会话"
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

                        TextField {
                            Layout.fillWidth: true
                            placeholderText: "账号/手机号/邮箱"
                            font.pixelSize: 14
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: accountInput = text
                        }

                        TextField {
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: "密码"
                            font.pixelSize: 14
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: passwordInput = text
                        }

                        CheckBox {
                            text: "自动登录"
                            font.pixelSize: 14
                        }

                        Button {
                            text: "登录"
                            Layout.fillWidth: true
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.accent
                            }
                            contentItem: Text {
                                text: "登录"
                                color: Ui.Style.textPrimary
                                font.pixelSize: 16
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (accountInput.length === 0 || passwordInput.length === 0) {
                                    errorText = "请输入账号和密码"
                                    return
                                }
                                errorText = ""
                                completeAuth()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            Button {
                                text: "注册账号"
                                flat: true
                                onClicked: loginStack.currentIndex = 1
                                contentItem: Text {
                                    text: "注册账号"
                                    color: Ui.Style.link
                                    font.pixelSize: 14
                                }
                                background: Rectangle { color: "transparent" }
                            }
                            Button {
                                text: "扫码登录"
                                flat: true
                                onClicked: loginStack.currentIndex = 2
                                contentItem: Text {
                                    text: "扫码登录"
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

                        TextField {
                            Layout.fillWidth: true
                            placeholderText: "用户名/手机号/邮箱"
                            font.pixelSize: 14
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: registerAccount = text
                        }

                        TextField {
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: "密码"
                            font.pixelSize: 14
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: registerPassword = text
                        }

                        TextField {
                            Layout.fillWidth: true
                            echoMode: TextInput.Password
                            placeholderText: "确认密码"
                            font.pixelSize: 14
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Qt.rgba(0.08, 0.1, 0.14, 0.9)
                                border.color: Ui.Style.borderSubtle
                            }
                            onTextChanged: registerConfirm = text
                        }

                        Button {
                            text: "注册"
                            Layout.fillWidth: true
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.accent
                            }
                            contentItem: Text {
                                text: "注册"
                                color: Ui.Style.textPrimary
                                font.pixelSize: 16
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (registerAccount.length === 0 || registerPassword.length === 0 || registerConfirm.length === 0) {
                                    errorText = "请填写完整注册信息"
                                    return
                                }
                                if (registerPassword !== registerConfirm) {
                                    errorText = "两次密码不一致"
                                    return
                                }
                                errorText = ""
                                completeAuth()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            Button {
                                text: "返回登录"
                                flat: true
                                onClicked: loginStack.currentIndex = 0
                                contentItem: Text {
                                    text: "返回登录"
                                    color: Ui.Style.link
                                    font.pixelSize: 14
                                }
                                background: Rectangle { color: "transparent" }
                            }
                            Button {
                                text: "扫码登录"
                                flat: true
                                onClicked: loginStack.currentIndex = 2
                                contentItem: Text {
                                    text: "扫码登录"
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
                            text: qrSeconds > 0 ? ("二维码将在 " + qrSeconds + " 秒后刷新") : "二维码已过期，请刷新"
                            color: qrSeconds > 0 ? Ui.Style.textSecondary : Ui.Style.link
                            font.pixelSize: 13
                            horizontalAlignment: Text.AlignHCenter
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Item { Layout.fillWidth: true }
                            Button {
                                text: "刷新"
                                flat: true
                                onClicked: resetQrTimer()
                                contentItem: Text {
                                    text: "刷新"
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
                                text: "返回登录"
                                flat: true
                                onClicked: loginStack.currentIndex = 0
                                contentItem: Text {
                                    text: "返回登录"
                                    color: Ui.Style.link
                                    font.pixelSize: 14
                                }
                                background: Rectangle { color: "transparent" }
                            }
                            Button {
                                text: "注册账号"
                                flat: true
                                onClicked: loginStack.currentIndex = 1
                                contentItem: Text {
                                    text: "注册账号"
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
}
