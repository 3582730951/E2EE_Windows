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
    height: 372
    transientParent: ownerWindow
    flags: Qt.FramelessWindowHint | Qt.Window
    title: mode === "peer" && peerName.length > 0
           ? peerName
           : Ui.I18n.t("dialog.securityCenter.trustTitle")
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
    readonly property string promptBadgeLabel: mode === "peer"
                                               ? (Ui.I18n.usesCjkLocale ? "联系人审批" : "Peer approval")
                                               : (Ui.I18n.usesCjkLocale ? "网关信任" : "Gateway trust")
    readonly property string promptBadgeDetail: mode === "peer"
                                                ? (peerName.length > 0 ? peerName : (Ui.I18n.usesCjkLocale ? "待审批设备" : "Pending device"))
                                                : (Ui.I18n.usesCjkLocale ? "手动审批" : "Manual approval")
    readonly property string fingerprintHint: mode === "peer"
                                              ? (Ui.I18n.usesCjkLocale
                                                 ? "核对联系人指纹后再批准。"
                                                 : "Compare the peer fingerprint before approval.")
                                              : (Ui.I18n.usesCjkLocale
                                                 ? "核对固定网关指纹后再继续。"
                                                 : "Compare the pinned gateway fingerprint before continuing.")
    readonly property string rootCodeHint: mode === "peer"
                                           ? (Ui.I18n.usesCjkLocale
                                              ? "输入根审批码完成本次手动信任。"
                                              : "Enter the root approval code for this manual trust action.")
                                           : (Ui.I18n.usesCjkLocale
                                              ? "输入根审批码继续本次网关信任。"
                                              : "Enter the root approval code to continue this gateway trust review.")

    signal accepted(string pinText)

    function openWith(modeValue, fingerprintValue, pinValue, peerValue) {
        mode = modeValue
        fingerprint = fingerprintValue || ""
        pin = pinValue || ""
        peerName = peerValue || ""
        description = mode === "peer"
                       ? (Ui.I18n.usesCjkLocale
                          ? "请先确认联系人身份与指纹，再执行本次审批。"
                          : "Verify the peer identity and fingerprint before approving this trust request.")
                       : (Ui.I18n.usesCjkLocale
                          ? "请先确认网关指纹与信任来源，再执行本次审批。"
                          : "Verify the gateway fingerprint and trusted source before approving this request.")
        Ui.AuthDisplayStore.clearError()
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
            ColumnLayout {
                spacing: 2

                Components.UiText {
                    text: root.title
                    textRole: "subtitle"
                }

                Components.UiText {
                    text: root.promptBadgeLabel
                    textRole: "caption"
                    roleColor: Ui.Style.textMuted
                }
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
            badgeLabelText: root.promptBadgeLabel
            badgeDetailText: root.promptBadgeDetail
            fingerprintLabelText: Ui.I18n.t("dialog.securityCenter.serverTitle")
            fingerprintText: fingerprint
            fingerprintDetailText: root.fingerprintHint
        }

        Components.RootAuthCodeCard {
            id: pinCard
            Layout.fillWidth: true
            labelText: Ui.I18n.t("auth.placeholder.rootCode")
            placeholderText: Ui.I18n.t("auth.placeholder.rootCode")
            descriptionText: root.rootCodeHint
            text: pin
        }

        Components.UiText {
            Layout.fillWidth: true
            visible: Ui.AuthDisplayStore.errorText.length > 0
            text: Ui.AuthDisplayStore.errorText
            textRole: "supporting"
            roleColor: Ui.Style.danger
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
                enabled: pinCard.text.trim().length > 0
                Accessible.name: Ui.I18n.t("dialog.securityCenter.trustTitle")
                onClicked: accepted(pinCard.text.trim())
            }
        }
    }
}
