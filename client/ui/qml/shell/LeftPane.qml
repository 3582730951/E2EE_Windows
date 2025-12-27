import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: root
    signal requestNewChat()
    signal requestAddContact()
    signal requestCreateGroup()
    signal requestSettings()

    function focusSearch() {
        searchField.forceActiveFocus()
    }

    function clearSearch() {
        searchField.text = ""
        Ui.AppStore.setSearchQuery("")
    }

    Rectangle {
        anchors.fill: parent
        color: Ui.Style.panelBg
        border.color: Ui.Style.borderSubtle
        radius: Ui.Style.radiusLarge
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingS

        RowLayout {
            Layout.fillWidth: true
            spacing: Ui.Style.paddingS
            Button {
                text: "MENU"
                Layout.preferredWidth: 64
                Layout.preferredHeight: 32
            }
            TextField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "Search"
                onTextChanged: Ui.AppStore.setSearchQuery(text)
            }
            Button {
                id: addButton
                text: "+"
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                onClicked: {
                    var pos = addButton.mapToItem(null, 0, addButton.height)
                    menu.x = pos.x
                    menu.y = pos.y
                    menu.open()
                }
            }
            Menu {
                id: menu
                MenuItem { text: "New Chat"; onTriggered: root.requestNewChat() }
                MenuItem { text: "New Group"; onTriggered: root.requestCreateGroup() }
                MenuItem { text: "Add Contact"; onTriggered: root.requestAddContact() }
                MenuItem { text: "Settings"; onTriggered: root.requestSettings() }
            }
        }

        TabBar {
            id: tabBar
            Layout.fillWidth: true
            currentIndex: Ui.AppStore.currentLeftTab
            onCurrentIndexChanged: Ui.AppStore.setLeftTab(currentIndex)

            TabButton { text: "Chats" }
            TabButton { text: "Contacts" }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            ListView {
                id: dialogsList
                clip: true
                model: Ui.AppStore.filteredDialogsModel
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 160
                delegate: dialogDelegate
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
            }

            ListView {
                id: contactsList
                clip: true
                model: Ui.AppStore.filteredContactsModel
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 160
                delegate: contactDelegate
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
            }
        }
    }

    Component {
        id: dialogDelegate
        Item {
            width: ListView.view.width
            height: Ui.Style.dialogRowHeight
            property bool selected: chatId === Ui.AppStore.currentChatId

            Rectangle {
                anchors.fill: parent
                radius: Ui.Style.radiusMedium
                color: selected ? Ui.Style.dialogSelectedBg
                                : mouseArea.containsMouse ? Ui.Style.hoverBg : "transparent"
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingM
                spacing: Ui.Style.paddingM

                Rectangle {
                    width: Ui.Style.avatarSizeDialogRow
                    height: Ui.Style.avatarSizeDialogRow
                    radius: width / 2
                    color: Ui.Style.avatarColor(avatarKey)
                    Text {
                        anchors.centerIn: parent
                        text: title.length > 0 ? title.charAt(0).toUpperCase() : "?"
                        color: Ui.Style.textPrimary
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        text: title
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textPrimary
                        elide: Text.ElideRight
                    }
                    Text {
                        text: preview
                        font.pixelSize: 12
                        color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textMuted
                        elide: Text.ElideRight
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 72
                    Layout.alignment: Qt.AlignTop
                    spacing: 6
                    Text {
                        text: timeText
                        font.pixelSize: 11
                        color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textMuted
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        visible: unread > 0
                        radius: 10
                        color: Ui.Style.unreadBadgeBg
                        implicitWidth: Math.max(20, unreadText.paintedWidth + 12)
                        implicitHeight: 18
                        Text {
                            id: unreadText
                            anchors.centerIn: parent
                            text: unread
                            color: Ui.Style.unreadBadgeFg
                            font.pixelSize: 10
                        }
                    }
                    Text {
                        text: pinned ? "PIN" : ""
                        font.pixelSize: 10
                        color: Ui.Style.textMuted
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: Ui.AppStore.setCurrentChat(chatId)
                onPressed: {
                    if (mouse.button === Qt.RightButton) {
                        contextMenu.popup()
                    }
                }
            }

            Menu {
                id: contextMenu
                MenuItem {
                    text: pinned ? "Unpin" : "Pin"
                    onTriggered: Ui.AppStore.togglePin(chatId)
                }
                MenuItem {
                    text: "Mark as read"
                    onTriggered: Ui.AppStore.markDialogRead(chatId)
                }
                MenuItem { text: "Mute" }
                MenuItem {
                    text: "Delete chat"
                    onTriggered: Ui.AppStore.removeChat(chatId)
                }
            }
        }
    }

    Component {
        id: contactDelegate
        Item {
            width: ListView.view.width
            height: 56

            Rectangle {
                anchors.fill: parent
                radius: Ui.Style.radiusMedium
                color: mouseArea.containsMouse ? Ui.Style.hoverBg : "transparent"
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingM
                spacing: Ui.Style.paddingM

                Rectangle {
                    width: Ui.Style.avatarSizeDialogRow
                    height: Ui.Style.avatarSizeDialogRow
                    radius: width / 2
                    color: Ui.Style.avatarColor(avatarKey)
                    Text {
                        anchors.centerIn: parent
                        text: displayName.length > 0 ? displayName.charAt(0).toUpperCase() : "?"
                        color: Ui.Style.textPrimary
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Text {
                        text: displayName
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: Ui.Style.textPrimary
                        elide: Text.ElideRight
                    }
                    Text {
                        text: usernameOrPhone
                        font.pixelSize: 12
                        color: Ui.Style.textMuted
                        elide: Text.ElideRight
                    }
                }
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: Ui.AppStore.openChatFromContact(contactId)
            }
        }
    }
}
