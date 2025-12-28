import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: Ui.Style.panelBg
    }

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: 1
        color: Ui.Style.borderSubtle
    }

    ScrollView {
        anchors.fill: parent
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            x: Ui.Style.paddingM
            y: Ui.Style.paddingM
            width: root.width - Ui.Style.paddingM * 2
            spacing: Ui.Style.paddingM

            Rectangle {
                Layout.fillWidth: true
                radius: Ui.Style.radiusLarge
                color: Ui.Style.panelBgAlt
                border.color: Ui.Style.borderSubtle

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingM
                    spacing: Ui.Style.paddingS

                    Rectangle {
                        width: 84
                        height: 84
                        radius: 42
                        color: Ui.Style.avatarColor(Ui.AppStore.currentChatTitle)
                        Layout.alignment: Qt.AlignHCenter
                        Text {
                            anchors.centerIn: parent
                            text: Ui.AppStore.currentChatTitle.length > 0
                                  ? Ui.AppStore.currentChatTitle.charAt(0).toUpperCase()
                                  : "?"
                            color: Ui.Style.textPrimary
                            font.pixelSize: 26
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: Ui.AppStore.currentChatTitle.length > 0
                              ? Ui.AppStore.currentChatTitle
                              : Ui.I18n.t("right.noChatSelected")
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

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Ui.Style.paddingS

                        Components.IconButton {
                            icon.source: "qrc:/mi/e2ee/ui/icons/bell.svg"
                            buttonSize: Ui.Style.iconButtonSmall
                            iconSize: 16
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("right.mute")
                        }
                        Components.IconButton {
                            icon.source: "qrc:/mi/e2ee/ui/icons/search.svg"
                            buttonSize: Ui.Style.iconButtonSmall
                            iconSize: 16
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("right.search")
                        }
                        Components.IconButton {
                            icon.source: "qrc:/mi/e2ee/ui/icons/more.svg"
                            buttonSize: Ui.Style.iconButtonSmall
                            iconSize: 16
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("chat.more")
                        }
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                currentIndex: Ui.AppStore.currentChatType === "group" ? 1 : 0

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Ui.Style.paddingS

                        Rectangle {
                            Layout.fillWidth: true
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.panelBgAlt
                            border.color: Ui.Style.borderSubtle
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingM
                                spacing: Ui.Style.paddingS

                                Text { text: Ui.I18n.t("right.profile"); color: Ui.Style.textSecondary; font.pixelSize: 11 }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text { text: Ui.I18n.t("right.username"); color: Ui.Style.textMuted; font.pixelSize: 10 }
                                    Text { text: "@demo"; color: Ui.Style.textPrimary; font.pixelSize: 12 }
                                    Text { text: Ui.I18n.t("right.phone"); color: Ui.Style.textMuted; font.pixelSize: 10 }
                                    Text { text: "+1 202-555-0101"; color: Ui.Style.textPrimary; font.pixelSize: 12 }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Ui.Style.paddingS
                                    Components.GhostButton {
                                        text: Ui.I18n.t("right.mute")
                                        Layout.fillWidth: true
                                    }
                                    Components.GhostButton {
                                        text: Ui.I18n.t("right.block")
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Ui.Style.paddingS

                        Rectangle {
                            Layout.fillWidth: true
                            radius: Ui.Style.radiusLarge
                            color: Ui.Style.panelBgAlt
                            border.color: Ui.Style.borderSubtle

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: Ui.Style.paddingM
                                spacing: Ui.Style.paddingS

                                Text { text: Ui.I18n.t("right.group"); color: Ui.Style.textSecondary; font.pixelSize: 11 }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Ui.Style.paddingS
                                    Components.GhostButton {
                                        text: Ui.I18n.t("right.add")
                                        Layout.fillWidth: true
                                    }
                                    Components.GhostButton {
                                        text: Ui.I18n.t("right.search")
                                        Layout.fillWidth: true
                                    }
                                    Components.GhostButton {
                                        text: Ui.I18n.t("right.notify")
                                        Layout.fillWidth: true
                                    }
                                }

                                TabBar {
                                    id: groupTabs
                                    Layout.fillWidth: true
                                    currentIndex: 0
                                    background: Rectangle {
                                        radius: Ui.Style.radiusMedium
                                        color: Ui.Style.panelBg
                                        border.color: Ui.Style.borderSubtle
                                    }
                                    TabButton {
                                        text: Ui.I18n.t("right.members")
                                        background: Rectangle {
                                            radius: Ui.Style.radiusMedium
                                            color: groupTabs.currentIndex === 0 ? Ui.Style.dialogSelectedBg : "transparent"
                                        }
                                        contentItem: Text {
                                            text: parent.text
                                            font.pixelSize: 11
                                            color: groupTabs.currentIndex === 0 ? Ui.Style.dialogSelectedFg : Ui.Style.textSecondary
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                    TabButton {
                                        text: Ui.I18n.t("right.media")
                                        background: Rectangle {
                                            radius: Ui.Style.radiusMedium
                                            color: groupTabs.currentIndex === 1 ? Ui.Style.dialogSelectedBg : "transparent"
                                        }
                                        contentItem: Text {
                                            text: parent.text
                                            font.pixelSize: 11
                                            color: groupTabs.currentIndex === 1 ? Ui.Style.dialogSelectedFg : Ui.Style.textSecondary
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                    TabButton {
                                        text: Ui.I18n.t("right.files")
                                        background: Rectangle {
                                            radius: Ui.Style.radiusMedium
                                            color: groupTabs.currentIndex === 2 ? Ui.Style.dialogSelectedBg : "transparent"
                                        }
                                        contentItem: Text {
                                            text: parent.text
                                            font.pixelSize: 11
                                            color: groupTabs.currentIndex === 2 ? Ui.Style.dialogSelectedFg : Ui.Style.textSecondary
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }

                                StackLayout {
                                    Layout.fillWidth: true
                                    currentIndex: groupTabs.currentIndex

                                    ListView {
                                        clip: true
                                        model: Ui.AppStore.membersModel
                                        delegate: Item {
                                            width: ListView.view.width
                                            height: 54
                                            Rectangle {
                                                anchors.fill: parent
                                                radius: Ui.Style.radiusMedium
                                                color: mouseArea.containsMouse ? Ui.Style.dialogHoverBg : "transparent"
                                            }
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
                                                        font.pixelSize: 12
                                                        color: Ui.Style.textPrimary
                                                        elide: Text.ElideRight
                                                    }
                                                    Text {
                                                        text: role
                                                        font.pixelSize: 10
                                                        color: Ui.Style.textMuted
                                                    }
                                                }
                                            }
                                            MouseArea {
                                                id: mouseArea
                                                anchors.fill: parent
                                                hoverEnabled: true
                                            }
                                        }
                                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                                        Layout.preferredHeight: 260
                                    }

                                    Item {
                                        Layout.preferredHeight: 180
                                        Text {
                                            anchors.centerIn: parent
                                            text: Ui.I18n.t("right.mediaPlaceholder")
                                            color: Ui.Style.textMuted
                                            font.pixelSize: 11
                                        }
                                    }

                                    Item {
                                        Layout.preferredHeight: 180
                                        Text {
                                            anchors.centerIn: parent
                                            text: Ui.I18n.t("right.filesPlaceholder")
                                            color: Ui.Style.textMuted
                                            font.pixelSize: 11
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
