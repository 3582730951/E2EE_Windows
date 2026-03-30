import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Accessibility 1.0
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    property var ownerWindow: null
    signal requestSecurityCenter()
    visible: false
    width: 680
    height: 460
    transientParent: ownerWindow
    flags: Qt.FramelessWindowHint | Qt.Window
    title: Ui.I18n.t("settings.title")
    color: "transparent"
    font.family: Ui.Style.fontFamily
    palette.window: Ui.Style.windowBg
    palette.base: Ui.Style.panelBgAlt
    palette.button: Ui.Style.panelBgAlt
    palette.text: Ui.Style.textPrimary

    property int pendingAiQualityScale: 0
    property var aiQualityOptions: [
        { label: Ui.I18n.t("settings.privacy.aiEnhanceQualityX2"), scale: 2 },
        { label: Ui.I18n.t("settings.privacy.aiEnhanceQualityX4"), scale: 4 }
    ]
    property var themeOptions: [
        { label: Ui.I18n.t("settings.theme.system"), mode: "system" },
        { label: Ui.I18n.t("settings.theme.light"), mode: "light" },
        { label: Ui.I18n.t("settings.theme.dark"), mode: "dark" }
    ]
    property var localeOptions: [
        { name: Ui.I18n.t("settings.theme.system"), code: "system" }
    ].concat(Ui.I18n.languages)

    function aiQualityIndex(scale) {
        for (var i = 0; i < aiQualityOptions.length; ++i) {
            if (aiQualityOptions[i].scale === scale) {
                return i
            }
        }
        return 0
    }

    function themeModeIndex(mode) {
        for (var i = 0; i < themeOptions.length; ++i) {
            if (themeOptions[i].mode === mode) {
                return i
            }
        }
        return 0
    }

    function requestAiQuality(scale) {
        if (scale === 4 && !Ui.PreferenceStore.aiEnhanceX4Confirmed) {
            pendingAiQualityScale = scale
            aiX4Dialog.open()
            return
        }
        Ui.PreferenceStore.setAiEnhanceQualityLevel(scale)
    }

    function localeModeIndex(mode) {
        for (var i = 0; i < localeOptions.length; ++i) {
            if (localeOptions[i].code === mode) {
                return i
            }
        }
        return 0
    }
    palette.buttonText: Ui.Style.textPrimary
    palette.highlight: Ui.Style.accent
    palette.highlightedText: Ui.Style.textPrimary

    function open() {
        Ui.SecurityDisplayStore.refresh()
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
                elide: Text.ElideRight
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

    RowLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingM

        ListView {
            id: sectionList
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            model: [Ui.I18n.t("settings.section.appearance"),
                    Ui.I18n.t("settings.section.notifications"),
                    Ui.I18n.t("settings.section.privacy"),
                    Ui.I18n.t("settings.section.aiModel"),
                    Ui.I18n.t("settings.section.about")]
            currentIndex: 0
            delegate: Item {
                width: ListView.view.width
                height: 42
                Rectangle {
                    anchors.fill: parent
                    radius: Ui.Style.radiusMedium
                    color: ListView.isCurrentItem
                           ? Ui.Style.dialogSelectedBg
                           : (mouseArea.containsMouse ? Ui.Style.dialogHoverBg : "transparent")
                }
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: ListView.isCurrentItem ? Ui.Style.dialogSelectedFg : Ui.Style.textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: sectionList.currentIndex = index
                }
            }
        }

        Rectangle {
            width: 1
            Layout.fillHeight: true
            color: Ui.Style.borderSubtle
            opacity: 0.6
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sectionList.currentIndex

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Ui.Style.paddingS
                    Text { text: Ui.I18n.t("settings.theme"); color: Ui.Style.textSecondary; font.pixelSize: 12; elide: Text.ElideRight }
                    ComboBox {
                        model: themeOptions
                        textRole: "label"
                        Layout.preferredWidth: 220
                        currentIndex: themeModeIndex(Ui.Style.themeMode)
                        onActivated: Ui.Style.themeMode = model[currentIndex].mode
                    }
                    Text { text: Ui.I18n.t("settings.language"); color: Ui.Style.textSecondary; font.pixelSize: 12; elide: Text.ElideRight }
                    ComboBox {
                        model: localeOptions
                        textRole: "name"
                        Layout.preferredWidth: 220
                        currentIndex: localeModeIndex(Ui.I18n.localeMode)
                        onActivated: Ui.I18n.setLocaleMode(model[currentIndex].code)
                    }
                    Text { text: Ui.I18n.t("settings.fontSize"); color: Ui.Style.textSecondary; font.pixelSize: 12; elide: Text.ElideRight }
                    Slider { from: 12; to: 16; value: 13 }
                    Text { text: Ui.I18n.t("settings.messageDensity"); color: Ui.Style.textSecondary; font.pixelSize: 12; elide: Text.ElideRight }
                    ComboBox { model: [Ui.I18n.t("settings.density.normal"), Ui.I18n.t("settings.density.compact")] }
                }
            }

            Item {
                Text {
                    anchors.centerIn: parent
                    text: Ui.I18n.t("settings.section.notifications")
                    color: Ui.Style.textMuted
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Ui.Style.paddingM

                    Rectangle {
                        Layout.fillWidth: true
                        radius: Ui.Style.radiusMedium
                        color: Ui.Style.panelBg
                        border.color: Ui.Style.borderSubtle

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingM
                            spacing: Ui.Style.paddingM

                            Rectangle {
                                width: 38
                                height: 38
                                radius: 19
                                color: Ui.Style.dialogSelectedBg

                                Image {
                                    anchors.centerIn: parent
                                    width: 18
                                    height: 18
                                    fillMode: Image.PreserveAspectFit
                                    source: "qrc:/mi/e2ee/ui/icons/check.svg"
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: Ui.I18n.t("settings.securityCenter.title")
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: Ui.I18n.t("settings.securityCenter.detail")
                                    color: Ui.Style.textMuted
                                    font.pixelSize: 11
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                            }

                            Components.GhostButton {
                                text: Ui.I18n.t("settings.securityCenter.open")
                                Layout.alignment: Qt.AlignVCenter
                                onClicked: root.requestSecurityCenter()
                            }
                        }
                    }

                    Text {
                        text: Ui.I18n.t("settings.privacy.clipboardIsolation")
                        color: Ui.Style.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: Ui.I18n.t("settings.privacy.clipboardIsolationHint")
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        Switch {
                            checked: Ui.PreferenceStore.clipboardIsolationEnabled
                            onToggled: Ui.PreferenceStore.setClipboardIsolationEnabled(checked)
                        }
                    }

                    Text {
                        text: Ui.I18n.t("settings.privacy.internalIme")
                        color: Ui.Style.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: Ui.I18n.t("settings.privacy.internalImeHint")
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        Switch {
                            checked: Ui.PreferenceStore.internalImeEnabled
                            onToggled: Ui.PreferenceStore.setInternalImeEnabled(checked)
                        }
                    }

                    Text {
                        text: Ui.I18n.t("settings.privacy.saveHistory")
                        color: Ui.Style.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: Ui.I18n.t("settings.privacy.saveHistoryHint")
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        Switch {
                            checked: Ui.PreferenceStore.historySaveEnabled
                            onToggled: Ui.PreferenceStore.setHistorySaveEnabled(checked)
                        }
                    }

                    Text {
                        text: Ui.I18n.t("settings.privacy.aiEnhance")
                        color: Ui.Style.textSecondary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: Ui.I18n.t("settings.privacy.aiEnhanceHint")
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        Switch {
                            checked: Ui.PreferenceStore.aiEnhanceEnabled
                            onToggled: Ui.PreferenceStore.setAiEnhanceEnabled(checked)
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Ui.Style.paddingM

                    Text {
                        text: Ui.I18n.t("settings.privacy.aiEnhance")
                        color: Ui.Style.textSecondary
                        font.pixelSize: 12
                    }
                    Text {
                        text: Ui.I18n.t("settings.privacy.aiEnhanceHint")
                        color: Ui.Style.textMuted
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 16
                        spacing: Ui.Style.paddingXS
                        enabled: Ui.PreferenceStore.aiEnhanceEnabled
                        opacity: Ui.PreferenceStore.aiEnhanceEnabled ? 1.0 : 0.45
                        Text {
                            text: Ui.I18n.t("settings.privacy.aiEnhanceQuality")
                            color: Ui.Style.textSecondary
                            font.pixelSize: 12
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            ComboBox {
                                id: aiQualityCombo
                                model: aiQualityOptions
                                textRole: "label"
                                Layout.preferredWidth: 220
                                enabled: Ui.PreferenceStore.aiEnhanceEnabled
                                currentIndex: aiQualityIndex(Ui.PreferenceStore.aiEnhanceQualityLevel)
                                onActivated: requestAiQuality(model[currentIndex].scale)
                            }
                        }
                        Text {
                            text: Ui.PreferenceStore.aiEnhanceGpuName.length > 0
                                  ? Ui.I18n.t("settings.privacy.aiEnhanceGpu").arg(
                                        Ui.PreferenceStore.aiEnhanceGpuName +
                                        (Ui.PreferenceStore.aiEnhanceGpuSeries > 0
                                         ? (" (" + Ui.PreferenceStore.aiEnhanceGpuSeries + Ui.I18n.t("settings.privacy.aiEnhanceGpuSeriesSuffix") + ")")
                                         : ""))
                                  : ""
                            visible: Ui.PreferenceStore.aiEnhanceGpuName.length > 0
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            text: Ui.PreferenceStore.aiEnhanceGpuAvailable
                                  ? ""
                                  : Ui.I18n.t("settings.privacy.aiEnhanceGpuUnavailable")
                            visible: !Ui.PreferenceStore.aiEnhanceGpuAvailable
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            text: Ui.I18n.t("settings.privacy.aiEnhanceRecommendPerf")
                                  .arg(Ui.PreferenceStore.aiEnhancePerfScale)
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            text: Ui.I18n.t("settings.privacy.aiEnhanceRecommendQuality")
                                  .arg(Ui.PreferenceStore.aiEnhanceQualityScale)
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                    Item { Layout.fillHeight: true }
                }
            }

            Item {
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Ui.Style.paddingS
                    Text { text: Ui.I18n.t("settings.about.appName"); color: Ui.Style.textPrimary; font.pixelSize: 16 }
                    Text { text: Ui.I18n.t("settings.about.build"); color: Ui.Style.textMuted; font.pixelSize: 12 }
                    Text {
                        text: Ui.SecurityDisplayStore.gatewayDisplayDetail
                        color: Ui.Style.textMuted
                        font.pixelSize: 11
                    }
                    Text {
                        text: Ui.SecurityDisplayStore.versionText
                        color: Ui.Style.textMuted
                        font.pixelSize: 11
                    }
                    Text {
                        text: Ui.SecurityDisplayStore.connectionSummary()
                        color: Ui.Style.textMuted
                        font.pixelSize: 11
                    }
                }
            }
        }
    }

    Dialog {
        id: aiX4Dialog
        modal: true
        title: Ui.I18n.t("settings.privacy.aiEnhanceX4Title")
        width: 360
        implicitWidth: 360
        contentWidth: 320
        standardButtons: Dialog.NoButton
        contentItem: Item {
            implicitWidth: 320
            implicitHeight: aiX4Message.implicitHeight
            width: 320
            Text {
                id: aiX4Message
                anchors.left: parent.left
                anchors.right: parent.right
                wrapMode: Text.WordWrap
                color: Ui.Style.textPrimary
                text: Ui.PreferenceStore.aiEnhanceGpuAvailable
                      ? Ui.I18n.t("settings.privacy.aiEnhanceX4MessageGpu")
                      : Ui.I18n.t("settings.privacy.aiEnhanceX4MessageCpu")
            }
        }
        footer: DialogButtonBox {
            Button {
                text: Ui.I18n.t("settings.privacy.aiEnhanceX4Cancel")
                onClicked: aiX4Dialog.reject()
            }
            Button {
                text: Ui.I18n.t("settings.privacy.aiEnhanceX4Confirm")
                onClicked: aiX4Dialog.accept()
            }
        }
        onAccepted: {
            var targetScale = pendingAiQualityScale || 4
            Ui.PreferenceStore.setAiEnhanceX4Confirmed(true)
            Ui.PreferenceStore.setAiEnhanceQualityLevel(targetScale)
            pendingAiQualityScale = 0
        }
        onRejected: {
            pendingAiQualityScale = 0
            aiQualityCombo.currentIndex =
                aiQualityIndex(Ui.PreferenceStore.aiEnhanceQualityLevel)
        }
    }
}
