import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: root

    property string phoneInput: ""
    property string codeInput: ""
    property string passwordInput: ""
    property int resendSeconds: 25

    Timer {
        id: resendTimer
        interval: 1000
        repeat: true
        onTriggered: {
            if (resendSeconds > 0) {
                resendSeconds -= 1
            } else {
                resendTimer.stop()
            }
        }
    }

    function completeAuth() {
        Ui.AppStore.currentPage = 1
    }

    function resetResend() {
        resendSeconds = 25
        resendTimer.restart()
    }

    Rectangle {
        anchors.fill: parent
        color: Ui.Style.windowBg
    }

    StackLayout {
        id: stack
        anchors.fill: parent
        currentIndex: 0

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Rectangle {
                id: welcomeCard
                width: 420
                height: 360
                radius: Ui.Style.radiusLarge
                color: Ui.Style.panelBgAlt
                anchors.centerIn: parent

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingL
                    spacing: Ui.Style.paddingM

                    Label {
                        text: "MI E2EE"
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        color: Ui.Style.textPrimary
                    }
                    Label {
                        text: "Secure messaging demo"
                        font.pixelSize: 13
                        color: Ui.Style.textSecondary
                    }

                    Item { Layout.fillHeight: true }

                    Button {
                        text: "Start"
                        Layout.fillWidth: true
                        background: Rectangle {
                            radius: Ui.Style.radiusMedium
                            color: Ui.Style.accent
                        }
                        contentItem: Text {
                            text: "Start"
                            color: Ui.Style.textPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            stack.currentIndex = 1
                        }
                    }
                    Button {
                        text: "Use QR code"
                        Layout.fillWidth: true
                        onClicked: {
                            stack.currentIndex = 4
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Rectangle {
                width: 420
                height: 380
                radius: Ui.Style.radiusLarge
                color: Ui.Style.panelBgAlt
                anchors.centerIn: parent

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingL
                    spacing: Ui.Style.paddingM

                    Label {
                        text: "Enter your phone"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: Ui.Style.textPrimary
                    }
                    Label {
                        text: "We will send you a verification code."
                        font.pixelSize: 12
                        color: Ui.Style.textSecondary
                    }

                    ComboBox {
                        id: countryCombo
                        Layout.fillWidth: true
                        model: ["United States (+1)", "China (+86)", "United Kingdom (+44)", "Germany (+49)"]
                    }

                    TextField {
                        id: phoneField
                        Layout.fillWidth: true
                        placeholderText: "Phone number"
                        inputMethodHints: Qt.ImhDigitsOnly
                        onTextChanged: phoneInput = text
                    }

                    Text {
                        id: phoneError
                        text: "Please enter a valid phone number."
                        color: Ui.Style.danger
                        font.pixelSize: 11
                        visible: false
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingS
                        Button {
                            text: "Back"
                            Layout.fillWidth: true
                            onClicked: stack.currentIndex = 0
                        }
                        Button {
                            text: "Next"
                            Layout.fillWidth: true
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.accent
                            }
                            contentItem: Text {
                                text: "Next"
                                color: Ui.Style.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (phoneInput.length < 6) {
                                    phoneError.visible = true
                                    return
                                }
                                phoneError.visible = false
                                resetResend()
                                stack.currentIndex = 2
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Rectangle {
                width: 420
                height: 400
                radius: Ui.Style.radiusLarge
                color: Ui.Style.panelBgAlt
                anchors.centerIn: parent

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingL
                    spacing: Ui.Style.paddingM

                    Label {
                        text: "Enter code"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: Ui.Style.textPrimary
                    }
                    Label {
                        text: "We sent a 6-digit code to your phone."
                        font.pixelSize: 12
                        color: Ui.Style.textSecondary
                    }

                    TextField {
                        id: codeField
                        Layout.fillWidth: true
                        inputMask: "000000"
                        placeholderText: "000000"
                        onTextChanged: {
                            codeInput = text
                            if (codeInput.length === 6) {
                                if (codeInput === "000000") {
                                    stack.currentIndex = 3
                                } else {
                                    completeAuth()
                                }
                            }
                        }
                    }

                    Text {
                        text: resendSeconds > 0 ? "Resend in 0:" + (resendSeconds < 10 ? "0" + resendSeconds : resendSeconds)
                                                : "Resend code"
                        color: resendSeconds > 0 ? Ui.Style.textMuted : Ui.Style.link
                        font.pixelSize: 11
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingS
                        Button {
                            text: "Back"
                            Layout.fillWidth: true
                            onClicked: stack.currentIndex = 1
                        }
                        Button {
                            text: "Next"
                            Layout.fillWidth: true
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.accent
                            }
                            contentItem: Text {
                                text: "Next"
                                color: Ui.Style.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (codeInput.length === 6) {
                                    if (codeInput === "000000") {
                                        stack.currentIndex = 3
                                    } else {
                                        completeAuth()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Rectangle {
                width: 420
                height: 360
                radius: Ui.Style.radiusLarge
                color: Ui.Style.panelBgAlt
                anchors.centerIn: parent

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingL
                    spacing: Ui.Style.paddingM

                    Label {
                        text: "Two-step verification"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: Ui.Style.textPrimary
                    }
                    Label {
                        text: "Enter your password to continue."
                        font.pixelSize: 12
                        color: Ui.Style.textSecondary
                    }

                    TextField {
                        id: passwordField
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: "Password"
                        onTextChanged: passwordInput = text
                    }

                    Text {
                        text: "Forgot password?"
                        color: Ui.Style.link
                        font.pixelSize: 11
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingS
                        Button {
                            text: "Back"
                            Layout.fillWidth: true
                            onClicked: stack.currentIndex = 2
                        }
                        Button {
                            text: "Sign in"
                            Layout.fillWidth: true
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.accent
                            }
                            contentItem: Text {
                                text: "Sign in"
                                color: Ui.Style.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (passwordInput.length > 0) {
                                    completeAuth()
                                }
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Rectangle {
                width: 420
                height: 420
                radius: Ui.Style.radiusLarge
                color: Ui.Style.panelBgAlt
                anchors.centerIn: parent

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingL
                    spacing: Ui.Style.paddingM

                    Label {
                        text: "Scan QR code"
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        color: Ui.Style.textPrimary
                    }
                    Label {
                        text: "Open the app on your phone and scan."
                        font.pixelSize: 12
                        color: Ui.Style.textSecondary
                    }

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 200
                        height: 200
                        radius: Ui.Style.radiusMedium
                        color: Ui.Style.windowBg
                        border.color: Ui.Style.borderSubtle
                        Text {
                            anchors.centerIn: parent
                            text: "QR"
                            color: Ui.Style.textMuted
                            font.pixelSize: 24
                        }
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingS
                        Button {
                            text: "Back"
                            Layout.fillWidth: true
                            onClicked: stack.currentIndex = 0
                        }
                        Button {
                            text: "Simulate scan"
                            Layout.fillWidth: true
                            background: Rectangle {
                                radius: Ui.Style.radiusMedium
                                color: Ui.Style.accent
                            }
                            contentItem: Text {
                                text: "Simulate scan"
                                color: Ui.Style.textPrimary
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: completeAuth()
                        }
                    }
                }
            }
        }
    }
}
