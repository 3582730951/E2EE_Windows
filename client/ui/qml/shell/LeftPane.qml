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
    signal requestNotifications()
    signal requestSettings()
    signal requestDeviceManager()

    function focusSearch() {
        searchField.focusInput()
    }

    function clearSearch() {
        searchField.text = ""
        Ui.ChatDisplayStore.setSearchQuery("")
    }

    Rectangle {
        anchors.fill: parent
        color: Ui.Style.railBg
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

        Rectangle {
            Layout.fillWidth: true
            radius: Ui.Style.radiusXL
            color: Ui.Style.railHeaderBg
            border.width: 1
            border.color: Ui.Style.borderSubtle
            implicitHeight: railHeaderColumn.implicitHeight + Ui.Style.paddingL * 2

            ColumnLayout {
                id: railHeaderColumn
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingL
                spacing: Ui.Style.paddingM

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Ui.Style.paddingS

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: Ui.I18n.t("app.title")
                            color: Ui.Style.textPrimary
                            font.pixelSize: 18
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: Ui.I18n.t("auth.hero.badge")
                            color: Ui.Style.textSecondary
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }
                    }

                    Components.IconButton {
                        id: menuButton
                        icon.source: Ui.Style.isDark
                                     ? "qrc:/mi/e2ee/ui/icons/menu-lines.svg"
                                     : "qrc:/mi/e2ee/ui/icons/menu-lines-dark.svg"
                        buttonSize: Ui.Style.iconButtonSize
                        iconSize: 16
                        bgColor: Ui.Style.topBarPillBg
                        hoverBg: Ui.Style.hoverBg
                        pressedBg: Ui.Style.pressedBg
                        onClicked: menuPopup.popup(menuButton, 0, menuButton.height)
                        ToolTip.visible: hovered && !menuPopup.visible
                        ToolTip.text: Ui.I18n.t("left.menu")
                    }

                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Ui.Style.paddingS

                    Rectangle {
                        Layout.preferredHeight: 24
                        radius: 12
                        color: Ui.Style.railAccentBg
                        border.width: 1
                        border.color: Ui.Style.railAccentBorder
                        implicitWidth: securePillText.implicitWidth + 18

                        Text {
                            id: securePillText
                            anchors.centerIn: parent
                            text: Ui.I18n.t("chat.secureSession")
                            color: Ui.Style.accentSoft
                            font.pixelSize: Math.max(12, Ui.Style.microTextSize - 1)
                            font.weight: Font.DemiBold
                        }
                    }

                    Item { Layout.fillWidth: true }

                    Item {
                        Layout.preferredWidth: Ui.Style.iconButtonSize
                        Layout.preferredHeight: Ui.Style.iconButtonSize
                        Layout.minimumWidth: Ui.Style.iconButtonSize
                        Layout.minimumHeight: Ui.Style.iconButtonSize
                        Layout.maximumWidth: Ui.Style.iconButtonSize
                        Layout.maximumHeight: Ui.Style.iconButtonSize
                        clip: false

                        Components.IconButton {
                            id: notificationsButton
                            anchors.fill: parent
                            icon.source: "qrc:/mi/e2ee/ui/icons/bell.svg"
                            buttonSize: Ui.Style.iconButtonSize
                            iconSize: 16
                            bgColor: Ui.Style.topBarPillBg
                            hoverBg: Ui.Style.hoverBg
                            pressedBg: Ui.Style.pressedBg
                            onClicked: root.requestNotifications()
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("left.notifications")
                        }

                        Rectangle {
                            id: notificationsBadge
                            visible: Ui.ChatDisplayStore.notificationCount > 0
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.rightMargin: -2
                            anchors.topMargin: -2
                            radius: 9
                            color: Ui.Style.danger
                            width: Math.max(18, badgeText.paintedWidth + 10)
                            height: 18
                            z: 3
                            Text {
                                id: badgeText
                                anchors.centerIn: parent
                                text: Ui.ChatDisplayStore.notificationCount > 99 ? "99+" : Ui.ChatDisplayStore.notificationCount
                                color: Ui.Style.unreadBadgeFg
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Ui.Style.paddingS

                    Components.PrimaryButton {
                        id: quickNewChatButton
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        Accessible.name: Ui.I18n.t("left.newChat")
                        onClicked: root.requestNewChat()
                        contentItem: RowLayout {
                            anchors.centerIn: parent
                            spacing: 6
                            Image {
                                source: "qrc:/mi/e2ee/ui/icons/chat.svg"
                                width: 13
                                height: 13
                                fillMode: Image.PreserveAspectFit
                            }
                            Text {
                                text: Ui.I18n.t("left.newChat")
                                color: "#FFFFFF"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                        }
                    }

                    Components.IconButton {
                        id: quickComposeButton
                        Accessible.name: Ui.I18n.t("chat.more")
                        icon.source: "qrc:/mi/e2ee/ui/icons/plus.svg"
                        buttonSize: 34
                        iconSize: 15
                        bgColor: Ui.Style.topBarPillBg
                        hoverBg: Ui.Style.hoverBg
                        pressedBg: Ui.Style.pressedBg
                        onClicked: composePopup.popup(quickComposeButton, 0, quickComposeButton.height + 4)
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.more")
                    }
                }

                Components.SearchField {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 120
                    placeholderText: Ui.I18n.t("left.search")
                    text: Ui.ChatDisplayStore.searchQuery
                    onTextEdited: Ui.ChatDisplayStore.setSearchQuery(text)
                }
            }
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

        Menu {
            id: composePopup
            width: 170

            MenuItem {
                text: Ui.I18n.t("left.newGroup")
                onTriggered: root.requestCreateGroup()
            }
            MenuItem {
                text: Ui.I18n.t("left.addContact")
                onTriggered: root.requestAddContact()
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
            model: Ui.ChatDisplayStore.filteredDialogsModel
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 160
            spacing: 2
            delegate: dialogDelegate
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: Ui.ChatDisplayStore.filteredDialogsModel.count === 0

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 12, 248)
                radius: Ui.Style.radiusXL
                color: Ui.Style.panelBgRaised
                border.width: 1
                border.color: Ui.Style.borderSubtle
                implicitHeight: emptyCardLayout.implicitHeight + 28

                ColumnLayout {
                    id: emptyCardLayout
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                        radius: 22
                        color: Qt.rgba(59 / 255, 125 / 255, 216 / 255, 0.18)

                        Rectangle {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            radius: 7
                            color: Ui.Style.accent
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: Ui.I18n.t("left.emptyTitle")
                        color: Ui.Style.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        text: Ui.I18n.t("left.emptyBody")
                        color: Ui.Style.textSecondary
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }

                    Components.PrimaryButton {
                        text: Ui.I18n.t("left.newChat")
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        onClicked: root.requestNewChat()
                    }

                    Components.GhostButton {
                        text: Ui.I18n.t("left.addContact")
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        onClicked: root.requestAddContact()
                    }
                }
            }
        }
    }

    Component {
        id: dialogDelegate
        Item {
            width: ListView.view.width
            height: Ui.Style.dialogRowHeight
            property bool selected: chatId === Ui.ChatDisplayStore.currentChatId
            function handlePressed(mouse) {
                if (mouse.button === Qt.RightButton) {
                    contextMenu.popup()
                }
            }

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: parent.height - 14
                    radius: 2
                    visible: selected
                    color: Ui.Style.tgUnreadBadge
                }

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 2
                    radius: Ui.Style.radiusLarge
                    color: selected
                           ? Ui.Style.tgActiveRowBg
                           : (mouseArea.containsMouse ? Ui.Style.dialogHoverBg : "transparent")
                    border.width: selected || mouseArea.containsMouse ? 1 : 0
                    border.color: selected ? Ui.Style.tgActiveRowBorder : Ui.Style.borderSubtle
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingS
                    anchors.rightMargin: Ui.Style.paddingS + Ui.Style.paddingXS
                    spacing: Ui.Style.paddingS

                Rectangle {
                    width: Ui.Style.avatarSizeDialogRow
                    height: Ui.Style.avatarSizeDialogRow
                    radius: width / 2
                    color: Ui.Style.avatarColor(avatarKey)
                    border.width: 1
                    border.color: selected ? Ui.Style.railAccentBorder : Qt.rgba(1, 1, 1, 0.08)
                    Text {
                        anchors.centerIn: parent
                        text: title.length > 0 ? title.charAt(0).toUpperCase() : ""
                        color: Ui.Style.textPrimary
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }

                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: 1
                        anchors.bottomMargin: 1
                        visible: unread > 0
                        color: Ui.Style.tgOnlineDot
                        border.width: 2
                        border.color: Ui.Style.railBg
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.rightMargin: Ui.Style.paddingXS
                    spacing: 2
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
                            color: selected ? Ui.Style.dialogSelectedFg : Ui.Style.textSecondary
                            elide: Text.ElideRight
                        }
                    }
                }

                Item {
                    id: metaColumn
                    Layout.preferredWidth: 52
                    Layout.minimumWidth: 52
                    Layout.maximumWidth: 52
                    Layout.fillHeight: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 1

                        Text {
                            id: timeLabel
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignRight | Qt.AlignTop
                            text: timeText
                            font.pixelSize: 11
                            font.weight: unread > 0 ? Font.DemiBold : Font.Medium
                            color: unread > 0 ? Ui.Style.tgUnreadBadge : (selected ? Ui.Style.dialogSelectedFg : Ui.Style.textMuted)
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
                            radius: 10
                            color: muted ? Ui.Style.tgMutedBadge : Ui.Style.tgUnreadBadge
                            implicitWidth: Math.max(22, unreadText.paintedWidth + 12)
                            implicitHeight: 20
                            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
                            Text {
                                id: unreadText
                                anchors.centerIn: parent
                                text: unread > 99 ? "99+" : unread
                                color: muted ? Ui.Style.unreadBadgeMutedFg : Ui.Style.unreadBadgeFg
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: Ui.ChatDisplayStore.setCurrentChat(chatId)
                onPressed: handlePressed
            }

            Menu {
                id: contextMenu
                MenuItem {
                    text: pinned ? Ui.I18n.t("left.context.unpin") : Ui.I18n.t("left.context.pin")
                    onTriggered: Ui.ChatDisplayStore.togglePin(chatId)
                }
                MenuItem {
                    text: Ui.I18n.t("left.context.markRead")
                    onTriggered: Ui.ChatDisplayStore.markDialogRead(chatId)
                }
                MenuItem { text: Ui.I18n.t("left.context.mute") }
                MenuItem {
                    text: Ui.I18n.t("left.context.delete")
                    onTriggered: Ui.ChatDisplayStore.removeChat(chatId)
                }
            }
        }
    }

}
