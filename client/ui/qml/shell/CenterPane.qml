import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 6.2
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/components" as Components

Item {
    id: root

    property bool chatSearchVisible: false
    property bool stickToBottom: true
    property bool hasChat: Ui.AppStore.currentChatId.length > 0
    property real actionScale: 1.32
    property real topBarScale: 0.72
    property int actionButtonSize: Math.round(Ui.Style.iconButtonSmall * actionScale)
    property int actionIconSize: Math.round(15 * actionScale)
    property int inputButtonSize: Math.round(Ui.Style.iconButtonSmall * actionScale)
    property int inputIconSize: Math.round(16 * actionScale)
    property int actionTopBarHeight: Math.round(Math.max(Ui.Style.topBarHeight * topBarScale,
                                                        actionButtonSize + Ui.Style.paddingS * 2 * topBarScale))
    property bool emojiLoaded: false
    property int emojiPopupWidth: 280
    property int emojiPopupHeight: 220
    property int emojiCellSize: 28
    property bool imeChineseMode: true
    property bool imeComposing: false
    property bool imeShiftPressed: false
    property bool imeShiftUsed: false
    property int imeStartPos: 0
    property int imeLength: 0
    property string imeBuffer: ""
    property var imeCandidates: []
    property int imeCandidateIndex: 0
    property string imePreedit: ""
    property bool internalImeReady: Ui.AppStore.internalImeEnabled &&
                                    clientBridge && clientBridge.imeAvailable &&
                                    clientBridge.imeAvailable()
    property bool imePopupVisible: internalImeReady && imeComposing &&
                                   imeCandidates && imeCandidates.length > 0

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
    function showAttachPopup() {
        if (!hasChat) {
            return
        }
        var pos = attachButton.mapToItem(root, 0, 0)
        var iconLeft = pos.x + (attachButton.width - inputIconSize) / 2
        var desiredX = iconLeft - attachPopup.contentLeftPadding
        var minX = Ui.Style.paddingS
        var maxX = root.width - attachPopup.implicitWidth - Ui.Style.paddingS
        attachPopup.x = Math.max(minX, Math.min(maxX, desiredX))
        var desiredY = pos.y - attachPopup.implicitHeight - 8
        attachPopup.y = Math.max(Ui.Style.paddingS, desiredY)
        attachPopup.open()
    }
    function loadEmoji() {
        if (emojiLoaded) {
            return
        }
        emojiLoaded = true
        var request = new XMLHttpRequest()
        request.open("GET", "qrc:/mi/e2ee/ui/emoji/emoji.json", false)
        request.send()
        if (request.status === 0 || request.status === 200) {
            try {
                var codes = JSON.parse(request.responseText)
                var limit = Math.min(codes.length, 160)
                for (var i = 0; i < limit; ++i) {
                    var code = parseInt(codes[i], 16)
                    if (!isNaN(code)) {
                        emojiModel.append({ value: String.fromCodePoint(code) })
                    }
                }
            } catch (err) {
                emojiLoaded = false
            }
        } else {
            emojiLoaded = false
        }
    }
    function showEmojiPopup() {
        if (!hasChat) {
            return
        }
        if (emojiPopup.visible) {
            emojiPopup.close()
            return
        }
        loadEmoji()
        var pos = emojiButton.mapToItem(root, 0, 0)
        var desiredX = pos.x + emojiButton.width - emojiPopup.width
        var minX = Ui.Style.paddingS
        var maxX = root.width - emojiPopup.width - Ui.Style.paddingS
        emojiPopup.x = Math.max(minX, Math.min(maxX, desiredX))
        var desiredY = pos.y - emojiPopup.height - 8
        emojiPopup.y = Math.max(Ui.Style.paddingS, desiredY)
        emojiPopup.open()
    }
    function insertEmoji(value) {
        if (!value || !messageInput) {
            return
        }
        messageInput.insert(messageInput.cursorPosition, value)
        messageInput.forceActiveFocus()
    }
    function selectedRange() {
        if (!messageInput) {
            return null
        }
        var start = messageInput.selectionStart
        var end = messageInput.selectionEnd
        if (start === undefined || end === undefined) {
            return null
        }
        if (start === end) {
            return null
        }
        if (start > end) {
            var tmp = start
            start = end
            end = tmp
        }
        return { start: start, end: end }
    }
    function replaceSelectionWith(text) {
        if (!text || !messageInput) {
            return
        }
        var range = selectedRange()
        if (range) {
            messageInput.remove(range.start, range.end)
            messageInput.cursorPosition = range.start
        }
        messageInput.insert(messageInput.cursorPosition, text)
    }
    function contextCopy(cut) {
        var selected = messageInput.selectedText || ""
        if (selected.length === 0) {
            return
        }
        if (Ui.AppStore.clipboardIsolationEnabled) {
            Ui.AppStore.setInternalClipboard(selected)
            if (cut) {
                var range = selectedRange()
                if (range) {
                    messageInput.remove(range.start, range.end)
                    messageInput.cursorPosition = range.start
                }
            }
            return
        }
        if (cut) {
            messageInput.cut()
        } else {
            messageInput.copy()
        }
    }
    function contextPaste() {
        if (!messageInput) {
            return
        }
        if (!Ui.AppStore.clipboardIsolationEnabled) {
            messageInput.paste()
            return
        }
        var internalText = Ui.AppStore.internalClipboardText || ""
        var internalMs = Ui.AppStore.internalClipboardMs || 0
        var systemText = clientBridge ? clientBridge.systemClipboardText() : ""
        var systemMs = clientBridge ? clientBridge.systemClipboardTimestamp() : 0
        var text = internalText
        if (systemText.length > 0 && systemMs > internalMs) {
            text = systemText
        }
        if (text.length === 0) {
            return
        }
        replaceSelectionWith(text)
    }
    function contextSelectAll() {
        if (messageInput) {
            messageInput.selectAll()
        }
    }
    function contextCanPaste() {
        if (!Ui.AppStore.clipboardIsolationEnabled) {
            return true
        }
        var internalText = Ui.AppStore.internalClipboardText || ""
        var systemText = clientBridge ? clientBridge.systemClipboardText() : ""
        return internalText.length > 0 || systemText.length > 0
    }
    function externalImeActive() {
        if (messageInput && messageInput.inputMethodComposing) {
            return true
        }
        return Qt.inputMethod && Qt.inputMethod.visible
    }
    function resetImeState() {
        imeComposing = false
        imeShiftPressed = false
        imeShiftUsed = false
        imeStartPos = 0
        imeLength = 0
        imeBuffer = ""
        imeCandidates = []
        imeCandidateIndex = 0
        imePreedit = ""
    }
    function cancelImeComposition(keepText) {
        if (!imeComposing) {
            return
        }
        if (!keepText && imeLength > 0) {
            messageInput.remove(imeStartPos, imeStartPos + imeLength)
            messageInput.cursorPosition = imeStartPos
        }
        if (clientBridge && clientBridge.imeClear) {
            clientBridge.imeClear()
        }
        resetImeState()
    }
    function updateImeCandidates() {
        var list = []
        if (clientBridge && clientBridge.imeCandidates) {
            list = clientBridge.imeCandidates(imeBuffer, 5)
        }
        if (!list || list.length === 0) {
            list = [imeBuffer]
        }
        imeCandidates = list
        if (imeCandidateIndex >= imeCandidates.length) {
            imeCandidateIndex = 0
        }
        var preeditText = ""
        if (clientBridge && clientBridge.imePreedit) {
            preeditText = clientBridge.imePreedit()
        }
        imePreedit = preeditText.length > 0 ? preeditText : imeBuffer
    }
    function updateImeComposition() {
        if (!imeComposing) {
            return
        }
        if (imeLength > 0) {
            messageInput.remove(imeStartPos, imeStartPos + imeLength)
        }
        messageInput.insert(imeStartPos, imeBuffer)
        imeLength = imeBuffer.length
        messageInput.cursorPosition = imeStartPos + imeLength
        updateImeCandidates()
    }
    function startImeComposition(ch) {
        if (!messageInput) {
            return
        }
        if (!imeComposing) {
            var selStart = Math.min(messageInput.selectionStart, messageInput.selectionEnd)
            var selEnd = Math.max(messageInput.selectionStart, messageInput.selectionEnd)
            if (!isNaN(selStart) && !isNaN(selEnd) && selEnd > selStart) {
                messageInput.remove(selStart, selEnd)
                messageInput.cursorPosition = selStart
            }
            imeComposing = true
            imeStartPos = messageInput.cursorPosition
            imeLength = 0
            imeBuffer = ""
            imeCandidateIndex = 0
            imeCandidates = []
            imePreedit = ""
        }
        imeBuffer += ch
        updateImeComposition()
    }
    function commitImeCandidate(index) {
        if (!imeComposing) {
            return
        }
        if (!imeCandidates || imeCandidates.length === 0) {
            cancelImeComposition(true)
            return
        }
        var safeIndex = Math.max(0, Math.min(index, imeCandidates.length - 1))
        var candidate = imeCandidates[safeIndex]
        if (imeLength > 0) {
            messageInput.remove(imeStartPos, imeStartPos + imeLength)
        }
        messageInput.insert(imeStartPos, candidate)
        messageInput.cursorPosition = imeStartPos + candidate.length
        if (clientBridge && clientBridge.imeCommit) {
            clientBridge.imeCommit(safeIndex)
        }
        resetImeState()
    }
    function handleImeKey(event) {
        if (!internalImeReady || !imeChineseMode || externalImeActive()) {
            return false
        }
        if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) {
            return false
        }
        var key = event.key
        var text = event.text || ""
        if (text.length === 0 && key >= Qt.Key_A && key <= Qt.Key_Z) {
            text = String.fromCharCode(key)
        }
        if (!imeComposing) {
            if (text.length === 1 && /[a-zA-Z]/.test(text)) {
                startImeComposition(text.toLowerCase())
                event.accepted = true
                return true
            }
            return false
        }
        if (key === Qt.Key_Backspace) {
            if (imeBuffer.length > 0) {
                imeBuffer = imeBuffer.slice(0, -1)
                if (imeBuffer.length === 0) {
                    cancelImeComposition(false)
                } else {
                    updateImeComposition()
                }
            } else {
                cancelImeComposition(false)
            }
            event.accepted = true
            return true
        }
        if (key === Qt.Key_Space || key === Qt.Key_Return || key === Qt.Key_Enter) {
            commitImeCandidate(imeCandidateIndex)
            event.accepted = true
            return true
        }
        if (key >= Qt.Key_1 && key <= Qt.Key_5) {
            commitImeCandidate(key - Qt.Key_1)
            event.accepted = true
            return true
        }
        if (key === Qt.Key_Left) {
            imeCandidateIndex = Math.max(0, imeCandidateIndex - 1)
            updateImeCandidates()
            event.accepted = true
            return true
        }
        if (key === Qt.Key_Right) {
            imeCandidateIndex = Math.min(imeCandidateIndex + 1, imeCandidates.length - 1)
            updateImeCandidates()
            event.accepted = true
            return true
        }
        if (key === Qt.Key_Up || key === Qt.Key_Down) {
            event.accepted = true
            return true
        }
        if (key === Qt.Key_Escape) {
            cancelImeComposition(false)
            event.accepted = true
            return true
        }
        if (text.length === 1 && /[a-zA-Z]/.test(text)) {
            imeBuffer += text.toLowerCase()
            updateImeComposition()
            event.accepted = true
            return true
        }
        if (text.length === 1 && !/[a-zA-Z\\s]/.test(text)) {
            commitImeCandidate(imeCandidateIndex)
            messageInput.insert(messageInput.cursorPosition, text)
            event.accepted = true
            return true
        }
        return false
    }
    onHasChatChanged: {
        if (!hasChat) {
            clearChatSearch()
        }
    }

    Connections {
        target: Ui.AppStore
        function onInternalImeEnabledChanged() {
            if (!Ui.AppStore.internalImeEnabled) {
                cancelImeComposition(true)
                if (clientBridge && clientBridge.imeReset) {
                    clientBridge.imeReset()
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: topBar
            Layout.fillWidth: true
            Layout.preferredHeight: hasChat ? actionTopBarHeight : 0
            Layout.minimumHeight: hasChat ? actionTopBarHeight : 0
            Layout.maximumHeight: hasChat ? actionTopBarHeight : 0
            visible: hasChat
            color: Ui.Style.panelBg

            RowLayout {
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingS * topBarScale
                spacing: Ui.Style.paddingS
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Ui.Style.paddingS

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                        text: Ui.AppStore.currentChatTitle.length > 0
                              ? Ui.AppStore.currentChatTitle
                              : Ui.I18n.t("chat.selectChat")
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
                }

                RowLayout {
                    id: actionRow
                    spacing: 0.5
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter

                    Components.SearchField {
                        id: chatSearchField
                        visible: chatSearchVisible
                        Layout.preferredWidth: 160
                        placeholderText: Ui.I18n.t("chat.searchInChat")
                        onInputActiveFocusChanged: {
                            if (!inputActiveFocus && text.length === 0) {
                                root.clearChatSearch()
                            }
                        }
                    }

                    Components.IconButton {
                        icon.source: "qrc:/mi/e2ee/ui/icons/search.svg"
                        buttonSize: actionButtonSize
                        iconSize: actionIconSize
                        visible: !chatSearchVisible
                        onClicked: root.showSearch()
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.find")
                    }
                    Components.IconButton {
                        icon.source: "qrc:/mi/e2ee/ui/icons/phone.svg"
                        buttonSize: actionButtonSize
                        iconSize: actionIconSize
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.call")
                    }
                    Components.IconButton {
                        icon.source: "qrc:/mi/e2ee/ui/icons/video.svg"
                        buttonSize: actionButtonSize
                        iconSize: actionIconSize
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.video")
                    }
                    Components.IconButton {
                        id: chatMoreButton
                        icon.source: "qrc:/mi/e2ee/ui/icons/more-vert.svg"
                        buttonSize: actionButtonSize
                        iconSize: actionIconSize
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.more")
                        onClicked: chatMoreMenu.popup(chatMoreButton, 0, chatMoreButton.height + 4)
                    }
                }
            }

            Menu {
                id: chatMoreMenu
                property int compactWidth: 180
                implicitWidth: compactWidth
                width: compactWidth
                MenuItem {
                    text: Ui.I18n.t("chat.stealth")
                    checkable: true
                    checked: Ui.AppStore.isChatStealth(Ui.AppStore.currentChatId)
                    enabled: Ui.AppStore.currentChatId.length > 0
                    onTriggered: Ui.AppStore.toggleChatStealth(Ui.AppStore.currentChatId)
                }
                MenuItem {
                    text: Ui.I18n.t("chat.mute")
                    checkable: true
                    checked: Ui.AppStore.isChatMuted(Ui.AppStore.currentChatId)
                    enabled: Ui.AppStore.currentChatId.length > 0
                    onTriggered: Ui.AppStore.toggleChatMuted(Ui.AppStore.currentChatId)
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
                GradientStop { position: 0.0; color: Ui.Style.messageGradientStart }
                GradientStop { position: 1.0; color: Ui.Style.messageGradientEnd }
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
                            text: Ui.I18n.t("chat.empty")
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
                ToolTip.text: Ui.I18n.t("chat.jumpBottom")
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
            property int inputFieldHeight: 0

            ColumnLayout {
                id: inputColumn
                anchors.fill: parent
                anchors.margins: Ui.Style.paddingM
                spacing: Ui.Style.paddingS

                Text {
                    visible: Ui.AppStore.sendErrorMessage.length > 0
                    text: Ui.AppStore.sendErrorMessage
                    color: Ui.Style.danger
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Ui.Style.paddingS

                    Components.IconButton {
                        id: attachButton
                        icon.source: "qrc:/mi/e2ee/ui/icons/paperclip.svg"
                        buttonSize: inputButtonSize
                        iconSize: inputIconSize
                        onClicked: root.showAttachPopup()
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.attach")
                    }

                    Rectangle {
                        id: inputField
                        Layout.fillWidth: true
                        Layout.preferredHeight: inputBar.inputFieldHeight
                        implicitHeight: inputBar.inputFieldHeight
                        radius: Ui.Style.radiusXL
                        color: Ui.Style.inputBg
                        border.color: messageInput.activeFocus ? Ui.Style.inputFocus : Ui.Style.inputBorder
                        border.width: 1

                        Flickable {
                            id: inputFlick
                            anchors.fill: parent
                            anchors.margins: Ui.Style.paddingS
                            clip: true
                            interactive: true
                            flickableDirection: Flickable.VerticalFlick
                            boundsBehavior: Flickable.StopAtBounds
                            contentWidth: messageInput.width
                            contentHeight: messageInput.height

                            TextArea {
                                id: messageInput
                                width: inputFlick.width
                                height: Math.max(inputFlick.height, implicitHeight)
                                wrapMode: TextEdit.Wrap
                                placeholderText: Ui.I18n.t("chat.writeMessage")
                                color: Ui.Style.textPrimary
                                selectByMouse: true
                                cursorVisible: true
                                cursorDelegate: Rectangle {
                                    width: 2
                                    radius: 1
                                    color: Ui.Style.accent
                                }
                                background: Rectangle { color: "transparent" }
                                enabled: Ui.AppStore.currentChatId.length > 0
                                Keys.onPressed: function(event) {
                                    var allowInternalIme = internalImeReady && !externalImeActive()
                                    if (allowInternalIme) {
                                        if (event.key === Qt.Key_Shift && !event.isAutoRepeat) {
                                            imeShiftPressed = true
                                            imeShiftUsed = false
                                            event.accepted = true
                                            return
                                        }
                                        if (imeShiftPressed && event.key !== Qt.Key_Shift) {
                                            imeShiftUsed = true
                                        }
                                        if (handleImeKey(event)) {
                                            return
                                        }
                                    }
                                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                        if (event.modifiers & Qt.ShiftModifier) {
                                            return
                                        }
                                        event.accepted = true
                                        inputBar.sendMessage()
                                    }
                                }
                                Keys.onReleased: function(event) {
                                    if (!internalImeReady || externalImeActive()) {
                                        return
                                    }
                                    if (event.key === Qt.Key_Shift && !event.isAutoRepeat) {
                                        var toggle = imeShiftPressed && !imeShiftUsed
                                        imeShiftPressed = false
                                        imeShiftUsed = false
                                        if (toggle) {
                                            imeChineseMode = !imeChineseMode
                                            if (!imeChineseMode) {
                                                cancelImeComposition(true)
                                            }
                                            event.accepted = true
                                        }
                                    }
                                }
                                onTextChanged: inputBar.updateInputHeight()
                                onContentHeightChanged: inputBar.updateInputHeight()
                                Component.onCompleted: inputBar.updateInputHeight()
                                onCursorPositionChanged: inputBar.ensureCursorVisible()
                                onCursorRectangleChanged: inputBar.ensureCursorVisible()
                                onActiveFocusChanged: {
                                    if (!activeFocus) {
                                        cancelImeComposition(true)
                                        return
                                    }
                                    if (internalImeReady && Qt.inputMethod && Qt.inputMethod.reset) {
                                        Qt.inputMethod.reset()
                                        if (Qt.inputMethod.hide) {
                                            Qt.inputMethod.hide()
                                        }
                                    }
                                }
                                inputMethodHints: internalImeReady
                                                    ? (Qt.ImhNoPredictiveText | Qt.ImhPreferLatin)
                                                    : Qt.ImhNone
                            }
                        }
                    }

                    MouseArea {
                        id: inputContextArea
                        anchors.fill: inputField
                        acceptedButtons: Qt.RightButton
                        hoverEnabled: true
                        onPressed: {
                            if (!hasChat) {
                                return
                            }
                            messageInput.forceActiveFocus()
                            inputContextMenu.popup()
                        }
                    }

                    Components.IconButton {
                        id: emojiButton
                        icon.source: "qrc:/mi/e2ee/ui/icons/emoji.svg"
                        buttonSize: inputButtonSize
                        iconSize: inputIconSize
                        onClicked: root.showEmojiPopup()
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.emoji")
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
                        onClicked: inputBar.sendMessage()
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.send")
                    }
                }
            }

            Rectangle {
                id: imePopup
                visible: imePopupVisible
                radius: 8
                color: Ui.Style.panelBgRaised
                border.color: Ui.Style.borderSubtle
                anchors.left: inputField.left
                anchors.bottom: inputField.top
                anchors.bottomMargin: 6
                width: Math.min(inputField.width, 420)
                implicitHeight: imePopupColumn.implicitHeight + Ui.Style.paddingS * 2
                z: 4

                Column {
                    id: imePopupColumn
                    anchors.fill: parent
                    anchors.margins: Ui.Style.paddingS
                    spacing: 6
                    Text {
                        text: imePreedit
                        visible: imePreedit.length > 0
                        color: Ui.Style.textPrimary
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    Flow {
                        width: parent.width
                        spacing: 6
                        Repeater {
                            model: imeCandidates
                            delegate: Rectangle {
                                radius: 6
                                color: index === imeCandidateIndex ? Ui.Style.hoverBg : "transparent"
                                border.color: index === imeCandidateIndex ? Ui.Style.accent : "transparent"
                                border.width: 1
                                height: 26
                                width: candidateRow.implicitWidth + 12
                                Row {
                                    id: candidateRow
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Text {
                                        text: (index + 1) + "."
                                        color: Ui.Style.textMuted
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: modelData
                                        color: Ui.Style.textPrimary
                                        font.pixelSize: 12
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: commitImeCandidate(index)
                                }
                            }
                        }
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

            Menu {
                id: inputContextMenu
                MenuItem {
                    text: Ui.I18n.t("input.context.cut")
                    enabled: messageInput.selectedText.length > 0
                    onTriggered: contextCopy(true)
                }
                MenuItem {
                    text: Ui.I18n.t("input.context.copy")
                    enabled: messageInput.selectedText.length > 0
                    onTriggered: contextCopy(false)
                }
                MenuItem {
                    text: Ui.I18n.t("input.context.paste")
                    enabled: contextCanPaste()
                    onTriggered: contextPaste()
                }
                MenuItem {
                    text: Ui.I18n.t("input.context.selectAll")
                    enabled: messageInput.length > 0
                    onTriggered: contextSelectAll()
                }
            }

            Connections {
                target: Ui.AppStore
                function onSendErrorMessageChanged() {
                    if (Ui.AppStore.sendErrorMessage.length > 0) {
                        sendErrorTimer.restart()
                    }
                }
            }

            Timer {
                id: sendErrorTimer
                interval: 3000
                repeat: false
                onTriggered: Ui.AppStore.clearSendError()
            }

            function updateInputHeight() {
                var minHeight = 40
                var maxHeight = 160
                var textHeight = Math.min(maxHeight, Math.max(minHeight, messageInput.implicitHeight))
                inputFieldHeight = textHeight + Ui.Style.paddingS * 2
                Qt.callLater(ensureCursorVisible)
            }

            function ensureCursorVisible() {
                var rect = messageInput.positionToRectangle(messageInput.cursorPosition)
                if (!rect || inputFlick.height <= 0) {
                    return
                }
                var padding = 4
                var viewTop = inputFlick.contentY
                var viewBottom = inputFlick.contentY + inputFlick.height
                var rectTop = rect.y - padding
                var rectBottom = rect.y + rect.height + padding
                var maxY = Math.max(0, inputFlick.contentHeight - inputFlick.height)
                if (rectBottom > viewBottom) {
                    inputFlick.contentY = Math.min(maxY, rectBottom - inputFlick.height)
                } else if (rectTop < viewTop) {
                    inputFlick.contentY = Math.max(0, rectTop)
                }
            }

            function sendMessage() {
                if (internalImeReady && imeComposing) {
                    commitImeCandidate(imeCandidateIndex)
                }
                if (messageInput.text.trim().length === 0) {
                    return
                }
                var ok = Ui.AppStore.sendMessage(messageInput.text)
                if (ok) {
                    messageInput.text = ""
                }
            }
        }
    }

    FileDialog {
        id: filePicker
        title: Ui.I18n.t("attach.document")
        onAccepted: {
            Ui.AppStore.sendFile(fileUrl.toString())
        }
    }

    ListModel {
        id: emojiModel
    }

    Popup {
        id: emojiPopup
        modal: false
        focus: true
        padding: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: emojiPopupWidth
        height: emojiPopupHeight

        background: Rectangle {
            radius: 10
            color: Ui.Style.panelBgRaised
            border.color: Ui.Style.borderSubtle
        }

        contentItem: GridView {
            id: emojiGrid
            anchors.fill: parent
            cellWidth: emojiCellSize
            cellHeight: emojiCellSize
            model: emojiModel
            clip: true
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
            delegate: Item {
                width: emojiGrid.cellWidth
                height: emojiGrid.cellHeight
                Text {
                    anchors.centerIn: parent
                    text: value
                    font.pixelSize: 18
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.insertEmoji(value)
                }
            }
        }
    }

    Popup {
        id: attachPopup
        modal: false
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        property int itemHeight: 36
        property int iconSize: 18
        property int iconGap: 6
        property int contentLeftPadding: 5
        property int contentRightPadding: 5
        readonly property int iconBlockWidth: iconSize + iconGap
        property int fontSize: 12
        property string textPhoto: Ui.I18n.t("attach.photoVideo")
        property string textDocument: Ui.I18n.t("attach.document")
        property string textLocation: Ui.I18n.t("attach.location")
        property var items: [
            { kind: "photo", label: Ui.I18n.t("attach.photoVideo"), iconSource: "qrc:/mi/e2ee/ui/icons/image.svg" },
            { kind: "document", label: Ui.I18n.t("attach.document"), iconSource: "qrc:/mi/e2ee/ui/icons/file.svg" },
            { kind: "location", label: Ui.I18n.t("attach.location"), iconSource: "qrc:/mi/e2ee/ui/icons/location.svg" }
        ]
        readonly property real maxTextWidth: Math.max(metricsPhoto.width,
                                                     metricsDocument.width,
                                                     metricsLocation.width)
        implicitWidth: Math.ceil(contentLeftPadding + contentRightPadding + iconBlockWidth + maxTextWidth + 7)
        implicitHeight: Math.ceil(itemHeight * 3)

        background: Rectangle {
            radius: 10
            color: Ui.Style.panelBgRaised
            border.color: Ui.Style.borderSubtle
        }

        contentItem: ColumnLayout {
            spacing: 0
            Repeater {
                model: attachPopup.items

                delegate: Item {
                    width: attachPopup.implicitWidth
                    height: attachPopup.itemHeight
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: mouseArea.containsMouse ? Ui.Style.hoverBg : "transparent"
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: attachPopup.contentLeftPadding
                        anchors.rightMargin: attachPopup.contentRightPadding
                        spacing: 0
                        Item {
                            width: attachPopup.iconBlockWidth
                            height: attachPopup.iconSize
                            Image {
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: attachPopup.iconSize
                                height: attachPopup.iconSize
                                source: modelData.iconSource
                                sourceSize.width: attachPopup.iconSize
                                sourceSize.height: attachPopup.iconSize
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                            }
                        }
                        Text {
                            text: modelData.label
                            color: Ui.Style.textPrimary
                            font.pixelSize: attachPopup.fontSize
                            font.family: Ui.Style.fontFamily
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            Layout.fillWidth: true
                        }
                    }
                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            attachPopup.close()
                            if (modelData.kind === "document" || modelData.kind === "photo") {
                                filePicker.open()
                            } else if (modelData.kind === "location") {
                                Ui.AppStore.sendLocation()
                            }
                        }
                    }
                }
            }
        }

        TextMetrics {
            id: metricsPhoto
            text: attachPopup.textPhoto
            font.pixelSize: attachPopup.fontSize
            font.family: Ui.Style.fontFamily
        }
        TextMetrics {
            id: metricsDocument
            text: attachPopup.textDocument
            font.pixelSize: attachPopup.fontSize
            font.family: Ui.Style.fontFamily
        }
        TextMetrics {
            id: metricsLocation
            text: attachPopup.textLocation
            font.pixelSize: attachPopup.fontSize
            font.family: Ui.Style.fontFamily
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
            property int senderAvatarSize: 26
            property int senderAvatarGap: 8
            property int senderLeftInset: showSender
                                            ? Ui.Style.paddingL + senderAvatarSize + senderAvatarGap
                                            : Ui.Style.paddingL

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

            Rectangle {
                id: senderAvatar
                visible: showSender
                width: senderAvatarSize
                height: senderAvatarSize
                radius: width / 2
                color: Ui.Style.avatarColor(senderName || "")
                anchors.left: parent.left
                anchors.leftMargin: Ui.Style.paddingL
                anchors.top: bubbleBlock.top
                anchors.topMargin: 2
                Text {
                    anchors.centerIn: parent
                    text: (senderName || "").length > 0
                          ? senderName.charAt(0).toUpperCase()
                          : "?"
                    color: Ui.Style.textPrimary
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                }
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
                x: isOutgoing ? parent.width - bubbleWidth - Ui.Style.paddingL : senderLeftInset
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
