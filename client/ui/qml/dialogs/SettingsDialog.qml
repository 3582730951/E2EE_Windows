import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Dialog {
    id: root
    modal: true
    width: 680
    height: 460
    title: "Settings"
    standardButtons: Dialog.NoButton

    background: Rectangle {
        radius: Ui.Style.radiusLarge
        color: Ui.Style.panelBgAlt
        border.color: Ui.Style.borderSubtle
    }

    header: Rectangle {
        height: Ui.Style.topBarHeight
        color: Ui.Style.panelBgAlt
        border.color: Ui.Style.borderSubtle
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
                icon.source: "qrc:/mi/e2ee/ui/icons/close-x-dark.svg"
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
            model: ["Appearance", "Notifications", "Privacy", "About"]
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
                    Text { text: "Theme"; color: Ui.Style.textSecondary; font.pixelSize: 12 }
                    ComboBox {
                        model: ["Dark Midnight"]
                        Layout.preferredWidth: 220
                    }
                    Text { text: "Font size"; color: Ui.Style.textSecondary; font.pixelSize: 12 }
                    Slider { from: 12; to: 16; value: 13 }
                    Text { text: "Message density"; color: Ui.Style.textSecondary; font.pixelSize: 12 }
                    ComboBox { model: ["Normal", "Compact"] }
                }
            }

            Item {
                Text {
                    anchors.centerIn: parent
                    text: "Notification settings placeholder"
                    color: Ui.Style.textMuted
                }
            }

            Item {
                Text {
                    anchors.centerIn: parent
                    text: "Privacy settings placeholder"
                    color: Ui.Style.textMuted
                }
            }

            Item {
                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Ui.Style.paddingS
                    Text { text: "MI E2EE Client"; color: Ui.Style.textPrimary; font.pixelSize: 16 }
                    Text { text: "Build demo UI"; color: Ui.Style.textMuted; font.pixelSize: 12 }
                }
            }
        }
    }
}
