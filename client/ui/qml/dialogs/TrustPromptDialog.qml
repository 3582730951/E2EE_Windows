import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    visible: false
    width: 460
    height: 320
    flags: Qt.FramelessWindowHint | Qt.Window
    title: "信任确认"
    color: "transparent"
    font.family: Ui.Style.fontFamily
    palette.window: Ui.Style.windowBg
    palette.base: Ui.Style.panelBgAlt
    palette.button: Ui.Style.panelBgAlt
    palette.text: Ui.Style.textPrimary
    palette.buttonText: Ui.Style.textPrimary
    palette.highlight: Ui.Style.accent
    palette.highlightedText: Ui.Style.textPrimary

    property string mode: "server"
    property string fingerprint: ""
    property string pin: ""
    property string peerName: ""
    property string description: ""

    signal accepted(string pinText)

    function openWith(modeValue, fingerprintValue, pinValue, peerValue) {
        mode = modeValue
        fingerprint = fingerprintValue || ""
        pin = pinValue || ""
        peerName = peerValue || ""
        description = mode === "peer"
                       ? ("请确认对端身份指纹/验证码：" + peerName)
                       : "请确认服务器指纹/验证码"
        pinField.text = pin
        visible = true
        raise()
        requestActivate()
    }

    background: Rectangle {
        radius: Ui.Style.radiusLarge
        color: Ui.Style.panelBgAlt
        border.color: Ui.Style.borderSubtle
    }

    header: Rectangle {
        height: Ui.Style.topBarHeight
        color: Ui.Style.panelBgAlt
        border.color: Ui.Style.borderSubtle
        DragHandler {
            target: null
            acceptedButtons: Qt.LeftButton
            onActiveChanged: {
                if (active && root.startSystemMove) {
                    root.startSystemMove()
                }
            }
        }
        RowLayout {
            anchors.fill: parent
            anchors.margins: Ui.Style.paddingM
            Text {
                text: root.title
                color: Ui.Style.textPrimary
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }
            Item { Layout.fillWidth: true }
            Components.IconButton {
                icon.source: Ui.Style.isDark
                             ? "qrc:/mi/e2ee/ui/icons/close-x.svg"
                             : "qrc:/mi/e2ee/ui/icons/close-x-dark.svg"
                buttonSize: Ui.Style.iconButtonSmall
                iconSize: 14
                onClicked: root.close()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingS

        Text {
            text: description
            color: Ui.Style.textSecondary
            font.pixelSize: 12
        }

        Text {
            text: "指纹："
            color: Ui.Style.textMuted
            font.pixelSize: 11
        }
        TextArea {
            id: fingerprintField
            Layout.fillWidth: true
            Layout.preferredHeight: 70
            text: fingerprint
            wrapMode: TextEdit.Wrap
            readOnly: true
            color: Ui.Style.textPrimary
            background: Rectangle {
                radius: Ui.Style.radiusMedium
                color: Ui.Style.inputBg
                border.color: Ui.Style.borderSubtle
            }
        }

        Text {
            text: "验证码："
            color: Ui.Style.textMuted
            font.pixelSize: 11
        }
        TextField {
            id: pinField
            Layout.fillWidth: true
            placeholderText: "请输入验证码"
            font.pixelSize: 12
            color: Ui.Style.textPrimary
            placeholderTextColor: Ui.Style.textMuted
            background: Rectangle {
                radius: Ui.Style.radiusMedium
                color: Ui.Style.inputBg
                border.color: pinField.activeFocus ? Ui.Style.inputFocus : Ui.Style.inputBorder
            }
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: Ui.Style.paddingS
            Components.GhostButton {
                text: Ui.I18n.t("dialog.addContact.cancel")
                Layout.fillWidth: true
                onClicked: root.close()
            }
            Components.PrimaryButton {
                text: "信任"
                Layout.fillWidth: true
                onClicked: {
                    accepted(pinField.text)
                    root.close()
                }
            }
        }
    }
}
