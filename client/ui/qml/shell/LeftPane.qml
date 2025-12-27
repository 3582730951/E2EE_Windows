import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root
    signal requestNewChat()
    signal requestAddContact()
    signal requestCreateGroup()
    signal requestSettings()

    function focusSearch() {
        searchField.focusInput()
    }

    function clearSearch() {
        searchField.text = ""
        Ui.AppStore.setSearchQuery("")
    }

    Rectangle {
        anchors.fill: parent
        color: Ui.Style.panelBg
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Ui.Style.paddingM
        spacing: Ui.Style.paddingS

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Ui.Style.topBarHeight
            spacing: Ui.Style.paddingS

            Components.IconButton {
                id: menuButton
                icon.source: "qrc:/mi/e2ee/ui/icons/menu-lines-dark.svg"
                buttonSize: Ui.Style.iconButtonSize
                iconSize: 16
                onClicked: menuPopup.popup(menuButton, 0, menuButton.height)
                ToolTip.visible: hovered
                ToolTip.text: "Menu"
            }

            Components.SearchField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: "Search"
                onTextEdited: Ui.AppStore.setSearchQuery(text)
            }

            Menu {
                id: menuPopup
                MenuItem { text: "Chats"; onTriggered: Ui.AppStore.setLeftTab(0) }
                MenuItem { text: "Contacts"; onTriggered: Ui.AppStore.setLeftTab(1) }
                MenuSeparator { }
                MenuItem { text: "New Chat"; onTriggered: root.requestNewChat() }
                MenuItem { text: "New Group"; onTriggered: root.requestCreateGroup() }
                MenuItem { text: "Add Contact"; onTriggered: root.requestAddContact() }
                MenuSeparator { }
                MenuItem { text: "Settings"; onTriggered: root.requestSettings() }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Ui.Style.borderSubtle
            opacity: 0.6
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: Ui.AppStore.currentLeftTab

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
                color: selected
                       ? Ui.Style.dialogSelectedBg
                       : (mouseArea.containsMouse ? Ui.Style.dialogHoverBg : "transparent")
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
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textPrimary
                        elide: Text.ElideRight
                    }
                    Text {
                        text: preview
                        font.pixelSize: 11
                        color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textMuted
                        elide: Text.ElideRight
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 70
                    Layout.alignment: Qt.AlignTop
                    spacing: 6
                    Text {
                        text: timeText
                        font.pixelSize: 10
                        color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textMuted
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        visible: unread > 0
                        radius: 9
                        color: Ui.Style.unreadBadgeBg
                        implicitWidth: Math.max(18, unreadText.paintedWidth + 10)
                        implicitHeight: 16
                        Text {
                            id: unreadText
                            anchors.centerIn: parent
                            text: unread
                            color: Ui.Style.unreadBadgeFg
                            font.pixelSize: 10
                        }
                    }
                    Image {
                        visible: pinned
                        source: "qrc:/mi/e2ee/ui/icons/star.svg"
                        width: 12
                        height: 12
                        opacity: 0.65
                        fillMode: Image.PreserveAspectFit
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
            height: Ui.Style.dialogRowHeight - 8

            Rectangle {
                anchors.fill: parent
                radius: Ui.Style.radiusMedium
                color: mouseArea.containsMouse ? Ui.Style.dialogHoverBg : "transparent"
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
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: Ui.Style.textPrimary
                        elide: Text.ElideRight
                    }
                    Text {
                        text: usernameOrPhone
                        font.pixelSize: 11
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
