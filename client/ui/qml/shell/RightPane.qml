import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: root
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: Ui.Style.panelBg
        border.color: Ui.Style.borderSubtle
        radius: Ui.Style.radiusLarge
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingM

        Rectangle {
            width: 80
            height: 80
            radius: 40
            color: Ui.Style.avatarColor(Ui.AppStore.currentChatTitle)
            Layout.alignment: Qt.AlignHCenter
            Text {
                anchors.centerIn: parent
                text: Ui.AppStore.currentChatTitle.length > 0
                      ? Ui.AppStore.currentChatTitle.charAt(0).toUpperCase()
                      : "?"
                color: Ui.Style.textPrimary
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: Ui.AppStore.currentChatTitle.length > 0
                  ? Ui.AppStore.currentChatTitle
                  : "No chat selected"
            font.pixelSize: 16
            font.weight: Font.DemiBold
            color: Ui.Style.textPrimary
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: Ui.AppStore.currentChatSubtitle
            font.pixelSize: 12
            color: Ui.Style.textMuted
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: Ui.AppStore.currentChatType === "group" ? 1 : 0

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Ui.Style.paddingS

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingS
                        Button { text: "Mute"; Layout.fillWidth: true }
                        Button { text: "Block"; Layout.fillWidth: true }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Ui.Style.borderSubtle
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingS
                        Text { text: "Username"; color: Ui.Style.textMuted; font.pixelSize: 11 }
                        Text { text: "@demo"; color: Ui.Style.textPrimary; font.pixelSize: 13 }
                        Text { text: "Phone"; color: Ui.Style.textMuted; font.pixelSize: 11 }
                        Text { text: "+1 202-555-0101"; color: Ui.Style.textPrimary; font.pixelSize: 13 }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: Ui.Style.paddingS

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Ui.Style.paddingS
                        Button { text: "Add"; Layout.fillWidth: true }
                        Button { text: "Search"; Layout.fillWidth: true }
                        Button { text: "Notify"; Layout.fillWidth: true }
                    }

                    TabBar {
                        id: groupTabs
                        Layout.fillWidth: true
                        TabButton { text: "Members" }
                        TabButton { text: "Media" }
                        TabButton { text: "Files" }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: groupTabs.currentIndex

                        ListView {
                            clip: true
                            model: Ui.AppStore.membersModel
                            delegate: Item {
                                width: ListView.view.width
                                height: 54
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: Ui.Style.paddingS
                                    spacing: Ui.Style.paddingM
                                    Rectangle {
                                        width: 36
                                        height: 36
                                        radius: 18
                                        color: Ui.Style.avatarColor(avatarKey)
                                        Text {
                                            anchors.centerIn: parent
                                            text: displayName.length > 0 ? displayName.charAt(0).toUpperCase() : "?"
                                            color: Ui.Style.textPrimary
                                            font.pixelSize: 13
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text {
                                            text: displayName
                                            font.pixelSize: 13
                                            color: Ui.Style.textPrimary
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            text: role
                                            font.pixelSize: 11
                                            color: Ui.Style.textMuted
                                        }
                                    }
                                }
                            }
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                        }

                        Item {
                            Text {
                                anchors.centerIn: parent
                                text: "Media placeholder"
                                color: Ui.Style.textMuted
                            }
                        }

                        Item {
                            Text {
                                anchors.centerIn: parent
                                text: "Files placeholder"
                                color: Ui.Style.textMuted
                            }
                        }
                    }
                }
            }
        }
    }
}
