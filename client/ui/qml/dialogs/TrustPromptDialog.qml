import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    property var ownerWindow: null
    visible: false
    width: 460
    height: 320
    transientParent: ownerWindow
    flags: Qt.FramelessWindowHint | Qt.Window
    title: Ui.I18n.t("dialog.securityCenter.trustTitle")
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
                       ? (Ui.I18n.t("dialog.securityCenter.trustReviewHint") + " " + peerName)
                       : Ui.I18n.t("dialog.securityCenter.transportNeedsAttentionHint")
        pinCard.text = pin
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
            Components.UiText {
                text: root.title
                textRole: "subtitle"
            }
            Item { Layout.fillWidth: true }
            Components.IconButton {
                icon.source: Ui.Style.isDark
                             ? "qrc:/mi/e2ee/ui/icons/close-x.svg"
                             : "qrc:/mi/e2ee/ui/icons/close-x-dark.svg"
                buttonSize: Ui.Style.iconButtonSmall
                iconSize: 14
                Accessible.name: Ui.I18n.t("dialog.addContact.cancel")
                onClicked: root.close()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingS

        Components.TrustPromptCard {
            Layout.fillWidth: true
            titleText: mode === "peer" && peerName.length > 0
                       ? peerName
                       : Ui.I18n.t("dialog.securityCenter.trustTitle")
            descriptionText: description
            fingerprintLabelText: Ui.I18n.t("dialog.securityCenter.serverTitle")
            fingerprintText: fingerprint
        }

        Components.RootAuthCodeCard {
            id: pinCard
            Layout.fillWidth: true
            labelText: Ui.I18n.t("auth.placeholder.rootCode")
            placeholderText: Ui.I18n.t("auth.placeholder.rootCode")
            text: pin
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: Ui.Style.paddingS
            Components.GhostButton {
                text: Ui.I18n.t("dialog.addContact.cancel")
                Layout.fillWidth: true
                Accessible.name: Ui.I18n.t("dialog.addContact.cancel")
                onClicked: root.close()
            }
            Components.PrimaryButton {
                text: Ui.I18n.t("dialog.securityCenter.trustTitle")
                Layout.fillWidth: true
                Accessible.name: Ui.I18n.t("dialog.securityCenter.trustTitle")
                onClicked: {
                    accepted(pinCard.text)
                    root.close()
                }
            }
        }
    }
}
