import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root
    readonly property bool smokeMode: typeof uiSmokeMode !== "undefined" ? !!uiSmokeMode : false
    readonly property string smokeRailBadgeText: {
        if (!smokeMode) {
            return Ui.I18n.t("chat.secureSession")
        }
        if (Ui.SmokeSceneStore.postLoginLightScene) {
            return Ui.I18n.usesCjkLocale ? "平台发布" : "Platform rollout"
        }
        if (Ui.SmokeSceneStore.normalizedScene === "post_login") {
            return Ui.I18n.usesCjkLocale ? "设计评审" : "Design review"
        }
        return Ui.I18n.t("chat.secureSession")
    }
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
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.topMargin: 0
        anchors.bottomMargin: 12
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 132
            Layout.minimumHeight: 132
            Layout.maximumHeight: 132

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 12
                anchors.leftMargin: 0
                anchors.rightMargin: 0
                anchors.bottomMargin: 20
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 52
                    spacing: 6

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 1

                        Text {
                            text: Ui.I18n.t("app.title")
                            color: Ui.Style.textPrimary
                            font.pixelSize: 15
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Text {
                            text: root.smokeRailBadgeText
                            color: Ui.Style.accentSoft
                            font.pixelSize: 10
                            font.weight: Font.Medium
                            elide: Text.ElideRight
                        }
                    }

                    Item {
                        Layout.preferredWidth: 30
                        Layout.preferredHeight: 30
                        Layout.alignment: Qt.AlignVCenter
                        clip: false

                        Components.IconButton {
                            id: notificationsButton
                            anchors.fill: parent
                            icon.source: "qrc:/mi/e2ee/ui/icons/bell.svg"
                            buttonSize: 30
                            iconSize: 14
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
                            radius: 8
                            color: Ui.Style.danger
                            width: Math.max(16, badgeText.paintedWidth + 8)
                            height: 16
                            z: 3

                            Text {
                                id: badgeText
                                anchors.centerIn: parent
                                text: Ui.ChatDisplayStore.notificationCount > 99 ? "99+" : Ui.ChatDisplayStore.notificationCount
                                color: Ui.Style.unreadBadgeFg
                                font.pixelSize: 9
                                font.weight: Font.DemiBold
                            }
                        }
                    }

                    Components.IconButton {
                        id: menuButton
                        Layout.alignment: Qt.AlignVCenter
                        icon.source: Ui.Style.isDark
                                     ? "qrc:/mi/e2ee/ui/icons/menu-lines.svg"
                                     : "qrc:/mi/e2ee/ui/icons/menu-lines-dark.svg"
                        buttonSize: 30
                        iconSize: 14
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
                    Layout.preferredHeight: 36
                    spacing: 6

                    Components.SearchField {
                        id: searchField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 120
                        Layout.preferredHeight: 36
                        placeholderText: Ui.I18n.t("left.search")
                        text: Ui.ChatDisplayStore.searchQuery
                        onTextEdited: Ui.ChatDisplayStore.setSearchQuery(text)
                    }

                    Components.GhostButton {
                        id: quickNewChatButton
                        Layout.preferredWidth: 84
                        Layout.preferredHeight: 36
                        text: Ui.I18n.t("left.newChat")
                        Accessible.name: Ui.I18n.t("left.newChat")
                        onClicked: root.requestNewChat()
                    }

                    Components.IconButton {
                        id: quickComposeButton
                        Accessible.name: Ui.I18n.t("chat.more")
                        icon.source: "qrc:/mi/e2ee/ui/icons/plus.svg"
                        buttonSize: 36
                        iconSize: 14
                        bgColor: Ui.Style.topBarPillBg
                        hoverBg: Ui.Style.hoverBg
                        pressedBg: Ui.Style.pressedBg
                        onClicked: composePopup.popup(quickComposeButton, 0, quickComposeButton.height + 4)
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.more")
                    }
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
            opacity: 0.32
        }

        ListView {
            id: dialogsList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Ui.ChatDisplayStore.filteredDialogsModel
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 160
            spacing: 1
            delegate: dialogDelegate
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: Ui.ChatDisplayStore.filteredDialogsModel.count === 0

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(parent.width - 24, 248)
                spacing: 10

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
                    Layout.preferredHeight: 36
                    onClicked: root.requestNewChat()
                }

                Components.GhostButton {
                    text: Ui.I18n.t("left.addContact")
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    onClicked: root.requestAddContact()
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
                    anchors.leftMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    width: 3
                    height: parent.height - 12
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
                    anchors.margins: 6
                    anchors.rightMargin: 7
                    spacing: 6

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
                    Layout.preferredWidth: 48
                    Layout.minimumWidth: 48
                    Layout.maximumWidth: 48
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
                            radius: 9
                            color: muted ? Ui.Style.tgMutedBadge : Ui.Style.tgUnreadBadge
                            implicitWidth: Math.max(20, unreadText.paintedWidth + 10)
                            implicitHeight: 18
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
