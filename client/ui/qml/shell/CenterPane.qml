import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root
    anchors.fill: parent

    property bool chatSearchVisible: false
    property bool stickToBottom: true
    property bool hasChat: Ui.AppStore.currentChatId.length > 0

    function showSearch() {
        if (!hasChat) {
            return
        }
        chatSearchVisible = true
        chatSearchField.focusInput()
    }

    function clearChatSearch() {
        if (!chatSearchVisible) {
            return false
        }
        chatSearchVisible = false
        chatSearchField.text = ""
        return true
    }
    onHasChatChanged: {
        if (!hasChat) {
            clearChatSearch()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: hasChat ? Ui.Style.topBarHeight : 0
            Layout.minimumHeight: hasChat ? Ui.Style.topBarHeight : 0
            Layout.maximumHeight: hasChat ? Ui.Style.topBarHeight : 0
            visible: hasChat
            color: Ui.Style.panelBg

            RowLayout {
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingM
                spacing: Ui.Style.paddingS

                Rectangle {
                    width: Ui.Style.avatarSizeTopBar
                    height: Ui.Style.avatarSizeTopBar
                    radius: width / 2
                    color: Ui.Style.avatarColor(Ui.AppStore.currentChatTitle)
                    Text {
                        anchors.centerIn: parent
                        text: Ui.AppStore.currentChatTitle.length > 0
                              ? Ui.AppStore.currentChatTitle.charAt(0).toUpperCase()
                              : "?"
                        color: Ui.Style.textPrimary
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: Ui.AppStore.currentChatTitle.length > 0
                              ? Ui.AppStore.currentChatTitle
                              : "Select a chat"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: Ui.Style.textPrimary
                        elide: Text.ElideRight
                    }
                    Text {
                        text: Ui.AppStore.currentChatSubtitle
                        font.pixelSize: 11
                        color: Ui.Style.textMuted
                        elide: Text.ElideRight
                    }
                }

                Components.SearchField {
                    id: chatSearchField
                    visible: chatSearchVisible
                    Layout.preferredWidth: 160
                    placeholderText: "Search in chat"
                }

                Components.IconButton {
                    icon.source: "qrc:/mi/e2ee/ui/icons/search.svg"
                    buttonSize: Ui.Style.iconButtonSmall
                    iconSize: 15
                    visible: !chatSearchVisible
                    onClicked: root.showSearch()
                    ToolTip.visible: hovered
                    ToolTip.text: "Find"
                }
                Components.IconButton {
                    icon.source: "qrc:/mi/e2ee/ui/icons/phone.svg"
                    buttonSize: Ui.Style.iconButtonSmall
                    iconSize: 15
                    ToolTip.visible: hovered
                    ToolTip.text: "Call"
                }
                Components.IconButton {
                    icon.source: "qrc:/mi/e2ee/ui/icons/video.svg"
                    buttonSize: Ui.Style.iconButtonSmall
                    iconSize: 15
                    ToolTip.visible: hovered
                    ToolTip.text: "Video"
                }
                Components.IconButton {
                    icon.source: "qrc:/mi/e2ee/ui/icons/info.svg"
                    buttonSize: Ui.Style.iconButtonSmall
                    iconSize: 15
                    onClicked: Ui.AppStore.toggleRightPane()
                    ToolTip.visible: hovered
                    ToolTip.text: "Info"
                }
                Components.IconButton {
                    icon.source: "qrc:/mi/e2ee/ui/icons/more.svg"
                    buttonSize: Ui.Style.iconButtonSmall
                    iconSize: 15
                    ToolTip.visible: hovered
                    ToolTip.text: "More"
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Ui.Style.borderSubtle
            }
        }

        Rectangle {
            id: messageArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Ui.Style.messageBg
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#DCEFD2" }
                GradientStop { position: 1.0; color: "#C7E7B6" }
            }

            Image {
                anchors.fill: parent
                source: "qrc:/mi/e2ee/ui/qml/assets/wallpaper_tile.svg"
                fillMode: Image.Tile
                opacity: 0.28
                smooth: true
            }

            ListView {
                id: messageList
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingL
                clip: true
                model: Ui.AppStore.currentChatId.length > 0
                       ? Ui.AppStore.messagesModel(Ui.AppStore.currentChatId)
                       : null
                boundsBehavior: Flickable.StopAtBounds
                cacheBuffer: 320
                delegate: messageDelegate
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }

                onContentYChanged: {
                    var atBottom = contentY + height >= contentHeight - 40
                    stickToBottom = atBottom
                }
                onCountChanged: {
                    if (stickToBottom) {
                        positionViewAtEnd()
                    }
                }
            }

            Item {
                anchors.fill: parent
                visible: Ui.AppStore.currentChatId.length === 0

                Column {
                    anchors.centerIn: parent
                    spacing: 10
                    Rectangle {
                        radius: 14
                        color: Qt.rgba(0.28, 0.45, 0.33, 0.35)
                        border.color: Qt.rgba(1, 1, 1, 0.4)
                        implicitWidth: emptyText.paintedWidth + 24
                        implicitHeight: 28
                        Text {
                            id: emptyText
                            anchors.centerIn: parent
                            text: "Select a chat to start messaging"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                        }
                    }
                }
            }

            Components.IconButton {
                id: jumpButton
                visible: !stickToBottom && messageList.count > 0
                icon.source: "qrc:/mi/e2ee/ui/icons/chevron-down.svg"
                buttonSize: 30
                iconSize: 14
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Ui.Style.paddingM
                bgColor: Ui.Style.panelBg
                hoverBg: Ui.Style.hoverBg
                pressedBg: Ui.Style.pressedBg
                onClicked: messageList.positionViewAtEnd()
                ToolTip.visible: hovered
                ToolTip.text: "Jump to bottom"
            }
        }

        Rectangle {
            id: inputBar
            Layout.fillWidth: true
            Layout.preferredHeight: hasChat ? implicitHeight : 0
            Layout.minimumHeight: hasChat ? implicitHeight : 0
            Layout.maximumHeight: hasChat ? implicitHeight : 0
            visible: hasChat
            color: Ui.Style.panelBg
            implicitHeight: inputColumn.implicitHeight + Ui.Style.paddingM * 2

            ColumnLayout {
                id: inputColumn
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingM
                spacing: Ui.Style.paddingS

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Ui.Style.paddingS

                    Components.IconButton {
                        icon.source: "qrc:/mi/e2ee/ui/icons/paperclip.svg"
                        buttonSize: Ui.Style.iconButtonSmall
                        iconSize: 16
                        ToolTip.visible: hovered
                        ToolTip.text: "Attach"
                    }

                    Rectangle {
                        id: inputField
                        Layout.fillWidth: true
                        radius: Ui.Style.radiusXL
                        color: Ui.Style.inputBg
                        border.color: messageInput.activeFocus ? Ui.Style.inputFocus : Ui.Style.inputBorder
                        border.width: 1
                        height: messageInput.implicitHeight + Ui.Style.paddingS * 2

                        TextArea {
                            id: messageInput
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingS
                            wrapMode: TextEdit.Wrap
                            placeholderText: "Write a message"
                            color: Ui.Style.textPrimary
                            selectByMouse: true
                            background: Rectangle { color: "transparent" }
                            enabled: Ui.AppStore.currentChatId.length > 0
                            Keys.onPressed: {
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                    if (event.modifiers & Qt.ShiftModifier) {
                                        return
                                    }
                                    event.accepted = true
                                    sendMessage()
                                }
                            }
                            onTextChanged: inputHeight()
                        }
                    }

                    Components.IconButton {
                        icon.source: "qrc:/mi/e2ee/ui/icons/emoji.svg"
                        buttonSize: Ui.Style.iconButtonSmall
                        iconSize: 16
                        ToolTip.visible: hovered
                        ToolTip.text: "Emoji"
                    }

                    Components.IconButton {
                        id: sendButton
                        icon.source: "qrc:/mi/e2ee/ui/icons/send.svg"
                        buttonSize: 34
                        iconSize: 18
                        bgColor: Ui.Style.accent
                        hoverBg: Ui.Style.accentHover
                        pressedBg: Ui.Style.accentPressed
                        baseColor: Ui.Style.textPrimary
                        hoverColor: Ui.Style.textPrimary
                        pressColor: Ui.Style.textPrimary
                        enabled: Ui.AppStore.currentChatId.length > 0 &&
                                 messageInput.text.trim().length > 0
                        onClicked: sendMessage()
                        ToolTip.visible: hovered
                        ToolTip.text: "Send"
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: Ui.Style.borderSubtle
            }

            function inputHeight() {
                var minHeight = 40
                var maxHeight = 160
                var h = Math.min(maxHeight, Math.max(minHeight, messageInput.contentHeight + 16))
                messageInput.implicitHeight = h
            }

            function sendMessage() {
                if (messageInput.text.trim().length === 0) {
                    return
                }
                Ui.AppStore.sendMessage(messageInput.text)
                messageInput.text = ""
            }
        }
    }

    Component {
        id: messageDelegate
        Item {
            width: ListView.view.width
            property bool isDate: kind === "date"
            property bool isSystem: kind === "system"
            property bool isIncoming: kind === "in"
            property bool isOutgoing: kind === "out"
            property bool showSender: isIncoming && Ui.AppStore.currentChatType === "group"

            height: isDate || isSystem ? 32 : bubbleBlock.height + 10

            Rectangle {
                visible: isDate
                anchors.horizontalCenter: parent.horizontalCenter
                y: 6
                radius: 10
                color: Qt.rgba(1, 1, 1, 0.65)
                border.color: Qt.rgba(0, 0, 0, 0.06)
                height: 20
                width: dateText.paintedWidth + 16
                Text {
                    id: dateText
                    anchors.centerIn: parent
                    text: model.text || ""
                    color: Ui.Style.textMuted
                    font.pixelSize: 10
                }
            }

            Text {
                visible: isSystem
                anchors.horizontalCenter: parent.horizontalCenter
                y: 8
                text: model.text || ""
                color: Ui.Style.textMuted
                font.pixelSize: 10
            }

            Item {
                id: bubbleBlock
                visible: isIncoming || isOutgoing
                property int hPadding: 12
                property int vPadding: 8
                property real maxBubbleWidth: Math.max(260, (ListView.view ? ListView.view.width : root.width) * 0.62)
                property real bubbleWidth: Math.min(maxBubbleWidth,
                                                    Math.max(messageText.paintedWidth, metaRow.implicitWidth) + hPadding * 2)
                property real bubbleHeight: messageText.paintedHeight + metaRow.implicitHeight +
                                            vPadding * 2 + (senderLabel.visible ? senderLabel.implicitHeight + 4 : 0)

                width: bubbleWidth
                height: bubbleHeight
                x: isOutgoing ? parent.width - bubbleWidth - Ui.Style.paddingL : Ui.Style.paddingL
                y: 4

                Rectangle {
                    id: bubble
                    anchors.fill: parent
                    radius: 12
                    color: isOutgoing ? Ui.Style.bubbleOutBg : Ui.Style.bubbleInBg
                    border.width: 0

                    Column {
                        anchors.fill: parent
                        anchors.margins: bubbleBlock.hPadding
                        spacing: 4

                        Text {
                            id: senderLabel
                            visible: showSender
                            text: senderName
                            font.pixelSize: 11
                            color: Ui.Style.link
                        }

                        Text {
                            id: messageText
                            text: model.text || ""
                            width: bubbleBlock.maxBubbleWidth - bubbleBlock.hPadding * 2
                            wrapMode: Text.Wrap
                            color: isOutgoing ? Ui.Style.bubbleOutFg : Ui.Style.bubbleInFg
                            font.pixelSize: 13
                        }

                        Row {
                            id: metaRow
                            spacing: 6
                            anchors.right: parent.right
                            Text {
                                text: timeText || ""
                                font.pixelSize: 10
                                color: isOutgoing ? Ui.Style.bubbleMetaOutFg : Ui.Style.bubbleMetaInFg
                            }
                            Text {
                                visible: isOutgoing
                                text: tickText(statusTicks)
                                font.pixelSize: 10
                                color: Ui.Style.bubbleMetaOutFg
                            }
                        }
                    }
                }

                Rectangle {
                    width: 8
                    height: 8
                    radius: 2
                    color: bubble.color
                    rotation: 45
                    transformOrigin: Item.Center
                    anchors.bottom: bubble.bottom
                    anchors.bottomMargin: 8
                    anchors.left: bubble.left
                    anchors.leftMargin: -4
                    visible: isIncoming
                }

                Rectangle {
                    width: 8
                    height: 8
                    radius: 2
                    color: bubble.color
                    rotation: 45
                    transformOrigin: Item.Center
                    anchors.bottom: bubble.bottom
                    anchors.bottomMargin: 8
                    anchors.right: bubble.right
                    anchors.rightMargin: -4
                    visible: isOutgoing
                }
            }

            function tickText(status) {
                if (status === "read") {
                    return "OK"
                }
                if (status === "delivered") {
                    return "DL"
                }
                if (status === "sent") {
                    return "SN"
                }
                return ""
            }
        }
    }
}
