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
    signal requestDeviceManager()

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

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: Ui.Style.borderSubtle
        z: 10
        visible: false
        enabled: false
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
                icon.source: Ui.Style.isDark
                             ? "qrc:/mi/e2ee/ui/icons/menu-lines.svg"
                             : "qrc:/mi/e2ee/ui/icons/menu-lines-dark.svg"
                buttonSize: Ui.Style.iconButtonSize
                iconSize: 16
                onClicked: menuPopup.popup(menuButton, 0, menuButton.height)
                ToolTip.visible: hovered && !menuPopup.visible
                ToolTip.text: Ui.I18n.t("left.menu")
            }

            Components.IconButton {
                id: deviceButton
                icon.source: "qrc:/mi/e2ee/ui/icons/device.svg"
                buttonSize: Ui.Style.iconButtonSize
                iconSize: 16
                onClicked: root.requestDeviceManager()
                ToolTip.visible: hovered
                ToolTip.text: Ui.I18n.t("left.deviceManager")
            }

            Components.SearchField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: Ui.I18n.t("left.search")
                onTextEdited: Ui.AppStore.setSearchQuery(text)
            }

            Menu {
                id: menuPopup
                property int compactWidth: 180
                property int compactFontSize: 13
                property int compactPadding: 3
                property int compactSpacing: 3
                property int compactItemHeight: Math.round(compactFontSize + compactPadding * 2 + 4)
                padding: 3
                implicitWidth: compactWidth
                width: compactWidth
                MenuItem {
                    id: menuNewChat
                    text: Ui.I18n.t("left.newChat")
                    implicitHeight: menuPopup.compactItemHeight
                    height: menuPopup.compactItemHeight
                    padding: menuPopup.compactPadding
                    spacing: menuPopup.compactSpacing
                    onTriggered: root.requestNewChat()
                    contentItem: Text {
                        anchors.fill: parent
                        text: menuNewChat.text
                        color: menuNewChat.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                        font.pixelSize: menuPopup.compactFontSize
                        font.family: Ui.Style.fontFamily
                        renderType: Text.NativeRendering
                        antialiasing: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
                MenuItem {
                    id: menuNewGroup
                    text: Ui.I18n.t("left.newGroup")
                    implicitHeight: menuPopup.compactItemHeight
                    height: menuPopup.compactItemHeight
                    padding: menuPopup.compactPadding
                    spacing: menuPopup.compactSpacing
                    onTriggered: root.requestCreateGroup()
                    contentItem: Text {
                        anchors.fill: parent
                        text: menuNewGroup.text
                        color: menuNewGroup.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                        font.pixelSize: menuPopup.compactFontSize
                        font.family: Ui.Style.fontFamily
                        renderType: Text.NativeRendering
                        antialiasing: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
                MenuItem {
                    id: menuAddContact
                    text: Ui.I18n.t("left.addContact")
                    implicitHeight: menuPopup.compactItemHeight
                    height: menuPopup.compactItemHeight
                    padding: menuPopup.compactPadding
                    spacing: menuPopup.compactSpacing
                    onTriggered: root.requestAddContact()
                    contentItem: Text {
                        anchors.fill: parent
                        text: menuAddContact.text
                        color: menuAddContact.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                        font.pixelSize: menuPopup.compactFontSize
                        font.family: Ui.Style.fontFamily
                        renderType: Text.NativeRendering
                        antialiasing: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
                MenuSeparator { }
                MenuItem {
                    id: menuDeviceManager
                    text: Ui.I18n.t("left.deviceManager")
                    implicitHeight: menuPopup.compactItemHeight
                    height: menuPopup.compactItemHeight
                    padding: menuPopup.compactPadding
                    spacing: menuPopup.compactSpacing
                    onTriggered: root.requestDeviceManager()
                    contentItem: Text {
                        anchors.fill: parent
                        text: menuDeviceManager.text
                        color: menuDeviceManager.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                        font.pixelSize: menuPopup.compactFontSize
                        font.family: Ui.Style.fontFamily
                        renderType: Text.NativeRendering
                        antialiasing: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
                MenuSeparator { }
                MenuItem {
                    id: menuSettings
                    text: Ui.I18n.t("left.settings")
                    implicitHeight: menuPopup.compactItemHeight
                    height: menuPopup.compactItemHeight
                    padding: menuPopup.compactPadding
                    spacing: menuPopup.compactSpacing
                    onTriggered: root.requestSettings()
                    contentItem: Text {
                        anchors.fill: parent
                        text: menuSettings.text
                        color: menuSettings.enabled ? Ui.Style.textPrimary : Ui.Style.textMuted
                        font.pixelSize: menuPopup.compactFontSize
                        font.family: Ui.Style.fontFamily
                        renderType: Text.NativeRendering
                        antialiasing: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Ui.Style.borderSubtle
            opacity: 0.6
        }

        ListView {
            id: dialogsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Ui.AppStore.filteredDialogsModel
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 160
            delegate: dialogDelegate
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
        }
    }

    Component {
        id: dialogDelegate
        Item {
            width: ListView.view.width
            height: Ui.Style.dialogRowHeight
            property bool selected: chatId === Ui.AppStore.currentChatId
            function handlePressed(mouse) {
                if (mouse.button === Qt.RightButton) {
                    contextMenu.popup()
                }
            }

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
                anchors.rightMargin: Ui.Style.paddingM + Ui.Style.paddingS
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
                    Layout.rightMargin: Ui.Style.paddingS
                    spacing: 4
                    Text {
                        text: title
                        Layout.fillWidth: true
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textPrimary
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: (type === "group" && (lastSenderName || "").length > 0) ? 6 : 0
                        Rectangle {
                            visible: type === "group" && (lastSenderName || "").length > 0
                            width: 16
                            height: 16
                            radius: 8
                            color: Ui.Style.avatarColor(lastSenderAvatarKey || lastSenderName)
                            Text {
                                anchors.centerIn: parent
                                text: (lastSenderName || "").length > 0
                                      ? lastSenderName.charAt(0).toUpperCase()
                                      : ""
                                color: Ui.Style.textPrimary
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            text: preview
                            Layout.fillWidth: true
                            font.pixelSize: 11
                            color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textMuted
                            elide: Text.ElideRight
                        }
                    }
                }

                Item {
                    id: metaColumn
                    Layout.preferredWidth: 60
                    Layout.minimumWidth: 60
                    Layout.maximumWidth: 60
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 2

                        Text {
                            id: timeLabel
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignRight | Qt.AlignTop
                            text: timeText
                            font.pixelSize: 10
                            color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textMuted
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                        }

                        Image {
                            id: pinnedIcon
                            visible: pinned
                            source: "qrc:/mi/e2ee/ui/icons/star.svg"
                            width: 12
                            height: 12
                            opacity: 0.65
                            fillMode: Image.PreserveAspectFit
                            Layout.alignment: Qt.AlignRight | Qt.AlignTop
                        }

                        Item { Layout.fillHeight: true }

                        Rectangle {
                            id: unreadBadge
                            visible: unread > 0
                            radius: 9
                            color: muted ? Ui.Style.unreadBadgeMutedBg : Ui.Style.unreadBadgeBg
                            implicitWidth: Math.max(18, unreadText.paintedWidth + 10)
                            implicitHeight: 16
                            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                            Text {
                                id: unreadText
                                anchors.centerIn: parent
                                text: unread > 99 ? "99+" : unread
                                color: muted ? Ui.Style.unreadBadgeMutedFg : Ui.Style.unreadBadgeFg
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: Ui.AppStore.setCurrentChat(chatId)
                onPressed: handlePressed
            }

            Menu {
                id: contextMenu
                MenuItem {
                    text: pinned ? Ui.I18n.t("left.context.unpin") : Ui.I18n.t("left.context.pin")
                    onTriggered: Ui.AppStore.togglePin(chatId)
                }
                MenuItem {
                    text: Ui.I18n.t("left.context.markRead")
                    onTriggered: Ui.AppStore.markDialogRead(chatId)
                }
                MenuItem { text: Ui.I18n.t("left.context.mute") }
                MenuItem {
                    text: Ui.I18n.t("left.context.delete")
                    onTriggered: Ui.AppStore.removeChat(chatId)
                }
            }
        }
    }

}
