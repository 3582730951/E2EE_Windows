import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
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
                Rectangle {
                    anchors.fill: parent
                    radius: Ui.Style.radiusMedium
                    color: Ui.Style.panelBg
                    border.color: Ui.Style.borderSubtle

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: 0

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            Text {
                                text: Ui.I18n.t("settings.theme")
                                color: Ui.Style.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                            Item { Layout.fillWidth: true }
                            ComboBox {
                                model: themeOptions
                                textRole: "label"
                                Layout.preferredWidth: 184
                                currentIndex: themeModeIndex(Ui.Style.themeMode)
                                onActivated: Ui.Style.themeMode = model[currentIndex].mode
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Ui.Style.borderSubtle }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            Text {
                                text: Ui.I18n.t("settings.language")
                                color: Ui.Style.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                            Item { Layout.fillWidth: true }
                            ComboBox {
                                model: localeOptions
                                textRole: "name"
                                Layout.preferredWidth: 184
                                currentIndex: localeModeIndex(Ui.I18n.localeMode)
                                onActivated: Ui.I18n.setLocaleMode(model[currentIndex].code)
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Ui.Style.borderSubtle }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            Text {
                                text: Ui.I18n.t("settings.fontSize")
                                color: Ui.Style.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                            Item { Layout.fillWidth: true }
                            Slider {
                                from: 12
                                to: 16
                                value: 13
                                Layout.preferredWidth: 184
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Ui.Style.borderSubtle }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            Text {
                                text: Ui.I18n.t("settings.messageDensity")
                                color: Ui.Style.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                            Item { Layout.fillWidth: true }
                            ComboBox {
                                model: [Ui.I18n.t("settings.density.normal"), Ui.I18n.t("settings.density.compact")]
                                Layout.preferredWidth: 184
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
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
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingM
                            spacing: 0

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 52

                                Text {
                                    text: Ui.I18n.t("settings.securityCenter.title")
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                }
                                Item { Layout.fillWidth: true }
                                Components.GhostButton {
                                    text: Ui.I18n.t("settings.securityCenter.open")
                                    Layout.preferredHeight: 32
                                    onClicked: root.requestSecurityCenter()
                                }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: Ui.Style.borderSubtle }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 52
                                spacing: Ui.Style.paddingM
                                Text {
                                    text: Ui.I18n.t("settings.privacy.clipboardIsolation")
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                }
                                Item { Layout.fillWidth: true }
                                Switch {
                                    checked: Ui.PreferenceStore.clipboardIsolationEnabled
                                    onToggled: Ui.PreferenceStore.setClipboardIsolationEnabled(checked)
                                }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: Ui.Style.borderSubtle }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 52
                                spacing: Ui.Style.paddingM
                                Text {
                                    text: Ui.I18n.t("settings.privacy.internalIme")
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                }
                                Item { Layout.fillWidth: true }
                                Switch {
                                    checked: Ui.PreferenceStore.internalImeEnabled
                                    onToggled: Ui.PreferenceStore.setInternalImeEnabled(checked)
                                }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: Ui.Style.borderSubtle }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 52
                                spacing: Ui.Style.paddingM
                                Text {
                                    text: Ui.I18n.t("settings.privacy.saveHistory")
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                }
                                Item { Layout.fillWidth: true }
                                Switch {
                                    checked: Ui.PreferenceStore.historySaveEnabled
                                    onToggled: Ui.PreferenceStore.setHistorySaveEnabled(checked)
                                }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: Ui.Style.borderSubtle }

                            RowLayout {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 52
                                spacing: Ui.Style.paddingM
                                Text {
                                    text: Ui.I18n.t("settings.privacy.aiEnhance")
                                    color: Ui.Style.textPrimary
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                }
                                Item { Layout.fillWidth: true }
                                Switch {
                                    checked: Ui.PreferenceStore.aiEnhanceEnabled
                                    onToggled: Ui.PreferenceStore.setAiEnhanceEnabled(checked)
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            Item {
                Rectangle {
                    anchors.fill: parent
                    radius: Ui.Style.radiusMedium
                    color: Ui.Style.panelBg
                    border.color: Ui.Style.borderSubtle

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 52
                            Text {
                                text: Ui.I18n.t("settings.privacy.aiEnhanceQuality")
                                color: Ui.Style.textPrimary
                                font.pixelSize: 13
                                font.weight: Font.Medium
                            }
                            Item { Layout.fillWidth: true }
                            ComboBox {
                                id: aiQualityCombo
                                model: aiQualityOptions
                                textRole: "label"
                                Layout.preferredWidth: 184
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
                            Layout.fillWidth: true
                        }

                        Text {
                            text: Ui.PreferenceStore.aiEnhanceGpuAvailable
                                  ? ""
                                  : Ui.I18n.t("settings.privacy.aiEnhanceGpuUnavailable")
                            visible: !Ui.PreferenceStore.aiEnhanceGpuAvailable
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Text {
                            text: Ui.I18n.t("settings.privacy.aiEnhanceRecommendPerf")
                                  .arg(Ui.PreferenceStore.aiEnhancePerfScale)
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Text {
                            text: Ui.I18n.t("settings.privacy.aiEnhanceRecommendQuality")
                                  .arg(Ui.PreferenceStore.aiEnhanceQualityScale)
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }

            Item {
                Rectangle {
                    anchors.fill: parent
                    radius: Ui.Style.radiusMedium
                    color: Ui.Style.panelBg
                    border.color: Ui.Style.borderSubtle

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: 8
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
                        Item { Layout.fillHeight: true }
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
