import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Dialog {
    id: root
    modal: true
    width: 640
    height: 420
    title: "Settings"
    standardButtons: Dialog.Close

    RowLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingM

        ListView {
            id: sectionList
            Layout.preferredWidth: 160
            Layout.fillHeight: true
            model: ["Appearance", "Notifications", "Privacy", "About"]
            currentIndex: 0
            delegate: Item {
                width: ListView.view.width
                height: 40
                Rectangle {
                    anchors.fill: parent
                    color: ListView.isCurrentItem ? Ui.Style.panelBgAlt : "transparent"
                    radius: Ui.Style.radiusSmall
                }
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: Ui.Style.textPrimary
                    font.pixelSize: 12
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: sectionList.currentIndex = index
                }
            }
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
                        Layout.preferredWidth: 200
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
