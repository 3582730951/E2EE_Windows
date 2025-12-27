import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Item {
    id: root
    anchors.fill: parent

    property bool chatSearchVisible: false
    property bool stickToBottom: true

    function showSearch() {
        chatSearchVisible = true
        chatSearchField.forceActiveFocus()
    }

    function clearChatSearch() {
        if (!chatSearchVisible) {
            return false
        }
        chatSearchVisible = false
        chatSearchField.text = ""
        return true
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: Ui.Style.panelBgAlt
            border.color: Ui.Style.borderSubtle

            RowLayout {
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingM
                spacing: Ui.Style.paddingM

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

                TextField {
                    id: chatSearchField
                    visible: chatSearchVisible
                    Layout.preferredWidth: 180
                    placeholderText: "Search in chat"
                }

                Button {
                    text: "Find"
                    visible: !chatSearchVisible
                    onClicked: root.showSearch()
                }
                Button {
                    text: "Call"
                }
                Button {
                    text: "Video"
                }
                Button {
                    text: "Info"
                    onClicked: Ui.AppStore.toggleRightPane()
                }
                Button {
                    text: "More"
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Ui.Style.panelBg

            ListView {
                id: messageList
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingM
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

            Button {
                id: jumpButton
                text: "Jump to bottom"
                visible: !stickToBottom && messageList.count > 0
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Ui.Style.paddingM
                onClicked: messageList.positionViewAtEnd()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            color: Ui.Style.panelBgAlt
            border.color: Ui.Style.borderSubtle

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingM
                spacing: Ui.Style.paddingS

                Rectangle {
                    Layout.fillWidth: true
                    height: 0
                    visible: false
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Ui.Style.paddingS

                    Button {
                        text: "Attach"
                        Layout.preferredHeight: 32
                    }
                    Button {
                        text: "Emoji"
                        Layout.preferredHeight: 32
                    }

                    TextArea {
                        id: messageInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 44
                        wrapMode: TextEdit.Wrap
                        placeholderText: "Write a message"
                        selectByMouse: true
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

                    Button {
                        id: sendButton
                        text: "Send"
                        enabled: Ui.AppStore.currentChatId.length > 0 &&
                                 messageInput.text.trim().length > 0
                        Layout.preferredHeight: 36
                        background: Rectangle {
                            radius: Ui.Style.radiusMedium
                            color: sendButton.enabled ? Ui.Style.accent : Ui.Style.pressedBg
                        }
                        contentItem: Text {
                            text: "Send"
                            color: Ui.Style.textPrimary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: sendMessage()
                    }
                }
            }

            function inputHeight() {
                var minHeight = 44
                var maxHeight = 160
                var h = Math.min(maxHeight, Math.max(minHeight, messageInput.contentHeight + 16))
                messageInput.Layout.preferredHeight = h
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

            height: isDate || isSystem ? 34 : bubbleBlock.height + 8

            Rectangle {
                visible: isDate
                anchors.horizontalCenter: parent.horizontalCenter
                y: 6
                radius: 10
                color: Ui.Style.panelBgAlt
                border.color: Ui.Style.borderSubtle
                height: 22
                width: dateText.paintedWidth + 18
                Text {
                    id: dateText
                    anchors.centerIn: parent
                    text: model.text || ""
                    color: Ui.Style.textMuted
                    font.pixelSize: 11
                }
            }

            Text {
                visible: isSystem
                anchors.horizontalCenter: parent.horizontalCenter
                y: 8
                text: model.text || ""
                color: Ui.Style.textMuted
                font.pixelSize: 11
            }

            Item {
                id: bubbleBlock
                visible: isIncoming || isOutgoing
                property int hPadding: 12
                property int vPadding: 8
                property real maxBubbleWidth: Math.max(240, ListView.view.width * 0.65)
                property real bubbleWidth: Math.min(maxBubbleWidth,
                                                    Math.max(messageText.paintedWidth, metaRow.implicitWidth) + hPadding * 2)
                property real bubbleHeight: messageText.paintedHeight + metaRow.implicitHeight +
                                            vPadding * 2 + (senderLabel.visible ? senderLabel.implicitHeight + 4 : 0)

                width: bubbleWidth
                height: bubbleHeight
                x: isOutgoing ? parent.width - bubbleWidth - 12 : 12
                y: 4

                Rectangle {
                    anchors.fill: parent
                    radius: Ui.Style.radiusLarge
                    color: isOutgoing ? Ui.Style.bubbleOutBg : Ui.Style.bubbleInBg
                    border.color: Qt.darker(color, 1.1)

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
                                color: Ui.Style.bubbleMetaFg
                            }
                            Text {
                                visible: isOutgoing
                                text: tickText(statusTicks)
                                font.pixelSize: 10
                                color: Ui.Style.bubbleMetaFg
                            }
                        }
                    }
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
