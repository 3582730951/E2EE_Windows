import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    visible: false
    width: 680
    height: 460
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

    function aiQualityIndex(scale) {
        for (var i = 0; i < aiQualityOptions.length; ++i) {
            if (aiQualityOptions[i].scale === scale) {
                return i
            }
        }
        return 0
    }

    function requestAiQuality(scale) {
        if (scale === 4 && !Ui.AppStore.aiEnhanceX4Confirmed) {
            pendingAiQualityScale = scale
            aiX4Dialog.open()
            return
        }
        Ui.AppStore.setAiEnhanceQualityLevel(scale)
    }
    palette.buttonText: Ui.Style.textPrimary
    palette.highlight: Ui.Style.accent
    palette.highlightedText: Ui.Style.textPrimary

    function open() {
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
                    Text { text: Ui.I18n.t("settings.theme"); color: Ui.Style.textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        model: [Ui.I18n.t("settings.theme.dark"), Ui.I18n.t("settings.theme.light")]
                        Layout.preferredWidth: 220
                        currentIndex: Ui.Style.themeMode === "light" ? 1 : 0
                        onActivated: Ui.Style.themeMode = currentIndex === 1 ? "light" : "dark"
                    }
                    Text { text: Ui.I18n.t("settings.language"); color: Ui.Style.textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        model: Ui.I18n.languages
                        textRole: "name"
                        Layout.preferredWidth: 220
                        currentIndex: Ui.I18n.languageIndex(Ui.I18n.currentLocale)
                        onActivated: Ui.I18n.setLocale(model[currentIndex].code)
                    }
                    Text { text: Ui.I18n.t("settings.fontSize"); color: Ui.Style.textSecondary; font.pixelSize: 12 }
                    Slider { from: 12; to: 16; value: 13 }
                    Text { text: Ui.I18n.t("settings.messageDensity"); color: Ui.Style.textSecondary; font.pixelSize: 12 }
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

                    Text {
                        text: Ui.I18n.t("settings.privacy.clipboardIsolation")
                        color: Ui.Style.textSecondary
                        font.pixelSize: 12
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
                            checked: Ui.AppStore.clipboardIsolationEnabled
                            onToggled: Ui.AppStore.setClipboardIsolationEnabled(checked)
                        }
                    }

                    Text {
                        text: Ui.I18n.t("settings.privacy.internalIme")
                        color: Ui.Style.textSecondary
                        font.pixelSize: 12
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
                            checked: Ui.AppStore.internalImeEnabled
                            onToggled: Ui.AppStore.setInternalImeEnabled(checked)
                        }
                    }

                    Text {
                        text: Ui.I18n.t("settings.privacy.saveHistory")
                        color: Ui.Style.textSecondary
                        font.pixelSize: 12
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
                            checked: Ui.AppStore.historySaveEnabled
                            onToggled: Ui.AppStore.setHistorySaveEnabled(checked)
                        }
                    }

                    Text {
                        text: Ui.I18n.t("settings.privacy.aiEnhance")
                        color: Ui.Style.textSecondary
                        font.pixelSize: 12
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
                            checked: Ui.AppStore.aiEnhanceEnabled
                            onToggled: Ui.AppStore.setAiEnhanceEnabled(checked)
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
                        enabled: Ui.AppStore.aiEnhanceEnabled
                        opacity: Ui.AppStore.aiEnhanceEnabled ? 1.0 : 0.45
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
                                enabled: Ui.AppStore.aiEnhanceEnabled
                                currentIndex: aiQualityIndex(Ui.AppStore.aiEnhanceQualityLevel)
                                onActivated: requestAiQuality(model[currentIndex].scale)
                            }
                        }
                        Text {
                            text: Ui.AppStore.aiEnhanceGpuName.length > 0
                                  ? Ui.I18n.t("settings.privacy.aiEnhanceGpu").arg(
                                        Ui.AppStore.aiEnhanceGpuName +
                                        (Ui.AppStore.aiEnhanceGpuSeries > 0
                                         ? (" (" + Ui.AppStore.aiEnhanceGpuSeries + Ui.I18n.t("settings.privacy.aiEnhanceGpuSeriesSuffix") + ")")
                                         : ""))
                                  : ""
                            visible: Ui.AppStore.aiEnhanceGpuName.length > 0
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            text: Ui.AppStore.aiEnhanceGpuAvailable
                                  ? ""
                                  : Ui.I18n.t("settings.privacy.aiEnhanceGpuUnavailable")
                            visible: !Ui.AppStore.aiEnhanceGpuAvailable
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            text: Ui.I18n.t("settings.privacy.aiEnhanceRecommendPerf")
                                  .arg(Ui.AppStore.aiEnhancePerfScale)
                            color: Ui.Style.textMuted
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            text: Ui.I18n.t("settings.privacy.aiEnhanceRecommendQuality")
                                  .arg(Ui.AppStore.aiEnhanceQualityScale)
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
                        text: clientBridge ? clientBridge.serverInfo() : ""
                        color: Ui.Style.textMuted
                        font.pixelSize: 11
                    }
                    Text {
                        text: clientBridge ? clientBridge.version() : ""
                        color: Ui.Style.textMuted
                        font.pixelSize: 11
                    }
                    Text {
                        text: clientBridge
                              ? (clientBridge.remoteOk ? "在线" : ("离线：" + clientBridge.remoteError))
                              : ""
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
        standardButtons: Dialog.NoButton
        contentItem: Text {
            width: 320
            wrapMode: Text.WordWrap
            color: Ui.Style.textPrimary
            text: Ui.AppStore.aiEnhanceGpuAvailable
                  ? Ui.I18n.t("settings.privacy.aiEnhanceX4MessageGpu")
                  : Ui.I18n.t("settings.privacy.aiEnhanceX4MessageCpu")
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
            Ui.AppStore.setAiEnhanceX4Confirmed(true)
            Ui.AppStore.setAiEnhanceQualityLevel(targetScale)
            pendingAiQualityScale = 0
        }
        onRejected: {
            pendingAiQualityScale = 0
            aiQualityCombo.currentIndex =
                aiQualityIndex(Ui.AppStore.aiEnhanceQualityLevel)
        }
    }
}
