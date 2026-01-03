import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs 6.2
import QtMultimedia 6.2
import QtPositioning 6.2
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
    property int stickerCellSize: 54
    property int stickerSize: 120
    property int emojiTabIndex: 0
    property string pendingDownloadId: ""
    property string pendingDownloadKey: ""
    property string pendingDownloadName: ""
    property int pendingDownloadSize: 0
    property string previewImageUrl: ""
    property string previewImageName: ""
    property bool previewSuggestEnhance: false
    property string previewEnhanceHint: ""
    property bool previewEnhancing: false
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
    function resolveDialogUrl(dialog) {
        if (!dialog) {
            return ""
        }
        var url = dialog.selectedFile
        if (!url && dialog.selectedFiles && dialog.selectedFiles.length > 0) {
            url = dialog.selectedFiles[0]
        }
        if (!url && dialog.fileUrl !== undefined) {
            url = dialog.fileUrl
        }
        if (!url) {
            return ""
        }
        return url.toString ? url.toString() : ("" + url)
    }
    function promptFileDownload(fileId, fileKey, fileName, fileSize) {
        pendingDownloadId = fileId || ""
        pendingDownloadKey = fileKey || ""
        pendingDownloadName = fileName && fileName.length > 0 ? fileName : (fileId || "")
        pendingDownloadSize = fileSize || 0
        if (pendingDownloadId.length === 0 || pendingDownloadKey.length === 0) {
            return
        }
        if (arguments.length > 4 && arguments[4] === true) {
            openDownloadSaveDialog()
        } else {
            downloadConfirm.open()
        }
    }
    function openDownloadSaveDialog() {
        if (clientBridge && clientBridge.defaultDownloadFileUrl) {
            var url = clientBridge.defaultDownloadFileUrl(pendingDownloadName)
            if (url) {
                downloadSaveDialog.selectedFile = url
            }
        }
        downloadSaveDialog.open()
    }
    function openImagePreview(url, name) {
        var resolved = url && url.toString ? url.toString() : (url ? "" + url : "")
        if (!resolved || resolved.length === 0) {
            return
        }
        previewImageUrl = resolved
        previewImageName = name || ""
        previewSuggestEnhance = false
        previewEnhanceHint = ""
        imagePreview.open()
    }
    function requestImageEnhance() {
        previewEnhanceHint = ""
        if (!clientBridge || !clientBridge.requestImageEnhance) {
            previewEnhanceHint = "AI超清暂未接入"
            return
        }
        if (previewEnhancing) {
            return
        }
        previewEnhancing = true
        previewEnhanceHint = "正在进行超清优化..."
        var ok = clientBridge.requestImageEnhance(previewImageUrl, previewImageName)
        if (!ok) {
            var err = clientBridge.lastError || ""
            previewEnhanceHint = err.length > 0 ? err : "AI超清暂未接入"
            previewEnhancing = false
        } else {
            previewEnhanceHint = "已提交超清优化请求"
        }
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
    function loadStickers() {
        stickerModel.clear()
        if (!clientBridge || !clientBridge.stickerItems) {
            return
        }
        var items = clientBridge.stickerItems()
        for (var i = 0; i < items.length; ++i) {
            var item = items[i]
            stickerModel.append({
                stickerId: item.id,
                title: item.title,
                animated: item.animated,
                path: item.path
            })
        }
    }
    function showStickerImport() {
        if (!hasChat) {
            return
        }
        stickerPicker.open()
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
        loadStickers()
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
        if (internalImeReady) {
            return false
        }
        if (messageInput && messageInput.inputMethodComposing) {
            return true
        }
        return Qt.inputMethod && Qt.inputMethod.visible
    }
    function externalImeChineseMode() {
        if (!Qt.inputMethod || !Qt.inputMethod.locale) {
            return false
        }
        var name = (Qt.inputMethod.locale.name || "").toLowerCase()
        return name.indexOf("zh") === 0 || name.indexOf("zh_") === 0 || name.indexOf("zh-") === 0
    }
    function internalImeRimeAvailable() {
        return internalImeReady && clientBridge && clientBridge.imeRimeAvailable &&
               clientBridge.imeRimeAvailable()
    }
    function imeStatusText() {
        var source = internalImeReady
                     ? (internalImeRimeAvailable()
                        ? Ui.I18n.t("ime.source.rime")
                        : Ui.I18n.t("ime.source.custom"))
                     : Ui.I18n.t("ime.source.thirdParty")
        var lang = internalImeReady
                   ? (imeChineseMode ? Ui.I18n.t("ime.lang.zh") : Ui.I18n.t("ime.lang.en"))
                   : (externalImeChineseMode() ? Ui.I18n.t("ime.lang.zh") : Ui.I18n.t("ime.lang.en"))
        return Ui.I18n.t("ime.status.label") + ":" + source + " :" + lang
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
                        enabled: Ui.AppStore.currentChatType === "private"
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.call")
                        onClicked: Ui.AppStore.startCall(false)
                    }
                    Components.IconButton {
                        icon.source: "qrc:/mi/e2ee/ui/icons/video.svg"
                        buttonSize: actionButtonSize
                        iconSize: actionIconSize
                        enabled: Ui.AppStore.currentChatType === "private"
                        ToolTip.visible: hovered
                        ToolTip.text: Ui.I18n.t("chat.video")
                        onClicked: Ui.AppStore.startCall(true)
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

            Text {
                id: imeStatusLabel
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                text: imeStatusText()
                color: Ui.Style.textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
                width: Math.min(parent.width * 0.4, 300)
                horizontalAlignment: Text.AlignHCenter
                visible: hasChat
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

            Item {
                id: callOverlay
                anchors.fill: parent
                z: 5
                visible: Ui.AppStore.incomingCallActive ||
                         (clientBridge && clientBridge.activeCallId.length > 0)
                property bool callVideo: clientBridge && clientBridge.activeCallVideo
                property string callPeer: clientBridge ? clientBridge.activeCallPeer : ""

                Rectangle {
                    anchors.fill: parent
                    color: Qt.rgba(0, 0, 0, 0.55)
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                }

                Rectangle {
                    id: incomingPanel
                    visible: Ui.AppStore.incomingCallActive &&
                             (!clientBridge || clientBridge.activeCallId.length === 0)
                    width: 320
                    height: 210
                    radius: 14
                    color: Ui.Style.panelBgRaised
                    border.color: Ui.Style.borderSubtle
                    anchors.centerIn: parent

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: Ui.Style.paddingS
                        Text {
                            text: Ui.AppStore.incomingCallVideo
                                  ? Ui.I18n.t("chat.callIncomingVideo")
                                  : Ui.I18n.t("chat.callIncomingVoice")
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: Ui.Style.textPrimary
                        }
                        Text {
                            text: Ui.AppStore.resolveTitle(Ui.AppStore.incomingCallPeer)
                            font.pixelSize: 12
                            color: Ui.Style.textSecondary
                            elide: Text.ElideRight
                        }
                        Item { Layout.fillHeight: true }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Ui.Style.paddingS
                            Components.GhostButton {
                                text: Ui.I18n.t("chat.callDecline")
                                Layout.fillWidth: true
                                onClicked: Ui.AppStore.declineIncomingCall()
                            }
                            Components.PrimaryButton {
                                text: Ui.I18n.t("chat.callAccept")
                                Layout.fillWidth: true
                                onClicked: Ui.AppStore.acceptIncomingCall()
                            }
                        }
                    }
                }

                Rectangle {
                    id: activeCallPanel
                    visible: clientBridge && clientBridge.activeCallId.length > 0
                    width: Math.min(parent.width * 0.78, 760)
                    height: Math.min(parent.height * 0.78, 520)
                    radius: 14
                    color: Ui.Style.panelBgRaised
                    border.color: Ui.Style.borderSubtle
                    anchors.centerIn: parent

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Ui.Style.paddingM
                        spacing: Ui.Style.paddingS
                        Text {
                            text: callOverlay.callVideo
                                  ? Ui.I18n.t("chat.callActiveVideo")
                                  : Ui.I18n.t("chat.callActiveVoice")
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: Ui.Style.textPrimary
                        }
                        Text {
                            text: Ui.AppStore.resolveTitle(callOverlay.callPeer || "")
                            font.pixelSize: 12
                            color: Ui.Style.textSecondary
                            elide: Text.ElideRight
                        }
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: callOverlay.callVideo

                            Rectangle {
                                anchors.fill: parent
                                radius: 12
                                color: Ui.Style.panelBgAlt
                                border.color: Ui.Style.borderSubtle
                            }

                            VideoOutput {
                                id: remoteView
                                anchors.fill: parent
                                fillMode: VideoOutput.PreserveAspectFit
                                Component.onCompleted: {
                                    if (clientBridge && clientBridge.bindRemoteVideoSink) {
                                        clientBridge.bindRemoteVideoSink(remoteView.videoSink)
                                    }
                                }
                            }
                            VideoOutput {
                                id: localView
                                width: 160
                                height: 100
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: Ui.Style.paddingS
                                fillMode: VideoOutput.PreserveAspectFit
                                Component.onCompleted: {
                                    if (clientBridge && clientBridge.bindLocalVideoSink) {
                                        clientBridge.bindLocalVideoSink(localView.videoSink)
                                    }
                                }
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 10
                                    color: "transparent"
                                    border.color: Ui.Style.borderSubtle
                                }
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            visible: !callOverlay.callVideo
                            Rectangle {
                                anchors.fill: parent
                                radius: 12
                                color: Ui.Style.panelBgAlt
                                border.color: Ui.Style.borderSubtle
                                Text {
                                    anchors.centerIn: parent
                                    text: Ui.I18n.t("chat.callActiveVoice")
                                    font.pixelSize: 12
                                    color: Ui.Style.textMuted
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Ui.Style.paddingS
                            Item { Layout.fillWidth: true }
                            Components.PrimaryButton {
                                text: Ui.I18n.t("chat.callHangup")
                                onClicked: {
                                    if (clientBridge) {
                                        clientBridge.endCall()
                                    }
                                }
                            }
                        }
                    }
                }
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
                        onRightClicked: root.showStickerImport()
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
        fileMode: FileDialog.OpenFile
        onAccepted: {
            Ui.AppStore.sendFile(resolveDialogUrl(filePicker))
        }
    }
    FileDialog {
        id: mediaPicker
        title: Ui.I18n.t("attach.photoVideo")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "媒体文件 (*.png *.jpg *.jpeg *.gif *.webp *.bmp *.mp4 *.mov *.mkv *.webm *.avi)"
        ]
        onAccepted: {
            Ui.AppStore.sendFile(resolveDialogUrl(mediaPicker))
        }
    }
    FileDialog {
        id: stickerPicker
        title: Ui.I18n.t("chat.importSticker")
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "媒体文件 (*.gif *.png *.jpg *.jpeg *.webp *.bmp *.mp4 *.mov *.mkv *.webm *.avi)"
        ]
        onAccepted: {
            if (clientBridge && clientBridge.importSticker) {
                var result = clientBridge.importSticker(resolveDialogUrl(stickerPicker))
                if (result && result.ok) {
                    loadStickers()
                    emojiTabIndex = 1
                    showEmojiPopup()
                }
            }
        }
    }
    Popup {
        id: downloadConfirm
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape
        width: 320

        background: Rectangle {
            radius: 12
            color: Ui.Style.panelBgRaised
            border.color: Ui.Style.borderSubtle
        }

        contentItem: ColumnLayout {
            anchors.margins: Ui.Style.paddingM
            spacing: Ui.Style.paddingS
            Text {
                text: Ui.I18n.t("chat.fileDownloadTitle")
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: Ui.Style.textPrimary
            }
            Text {
                text: Ui.I18n.format("chat.fileDownloadPrompt", pendingDownloadName)
                font.pixelSize: 12
                color: Ui.Style.textSecondary
                wrapMode: Text.Wrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Ui.Style.paddingS
                Components.GhostButton {
                    text: Ui.I18n.t("chat.fileDownloadCancel")
                    Layout.fillWidth: true
                    onClicked: downloadConfirm.close()
                }
                    Components.PrimaryButton {
                        text: Ui.I18n.t("chat.fileDownloadConfirm")
                        Layout.fillWidth: true
                        onClicked: {
                            downloadConfirm.close()
                            openDownloadSaveDialog()
                        }
                    }
            }
        }
    }
    FileDialog {
        id: downloadSaveDialog
        title: Ui.I18n.t("chat.fileDownloadPick")
        fileMode: FileDialog.SaveFile
        onAccepted: {
            if (!clientBridge || !clientBridge.requestAttachmentDownload) {
                return
            }
            var path = resolveDialogUrl(downloadSaveDialog)
            if (!path || path.length === 0) {
                return
            }
            clientBridge.requestAttachmentDownload(
                        pendingDownloadId,
                        pendingDownloadKey,
                        pendingDownloadName,
                        pendingDownloadSize,
                        path)
        }
    }
    Popup {
        id: imagePreview
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        x: 0
        y: 0
        width: root.width
        height: root.height
        onClosed: {
            previewImageUrl = ""
            previewImageName = ""
            previewSuggestEnhance = false
            previewEnhanceHint = ""
            previewEnhancing = false
        }

        background: Rectangle {
            color: Qt.rgba(0, 0, 0, 0.72)
        }

        contentItem: Item {
            anchors.fill: parent

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    var p = mapToItem(previewImage, mouse.x, mouse.y)
                    if (p.x < 0 || p.y < 0 ||
                        p.x > previewImage.width || p.y > previewImage.height) {
                        imagePreview.close()
                    }
                }
            }

            Image {
                id: previewImage
                anchors.fill: parent
                anchors.margins: 48
                source: previewImageUrl
                fillMode: Image.PreserveAspectFit
                smooth: true
                antialiasing: true
                cache: true
                onStatusChanged: {
                    if (status === Image.Ready) {
                        var w = sourceSize.width
                        var h = sourceSize.height
                        if (w > 0 && h > 0) {
                            previewSuggestEnhance = Math.min(w, h) < 900
                        } else {
                            previewSuggestEnhance = true
                        }
                    } else {
                        previewSuggestEnhance = false
                    }
                }
            }

            Rectangle {
                id: enhanceBanner
                visible: previewSuggestEnhance
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 16
                width: Math.min(parent.width - 80, 540)
                height: 36
                radius: 18
                color: Ui.Style.panelBgRaised
                border.color: Ui.Style.borderSubtle

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 8
                    Text {
                        text: "图片不够清晰？试试 AI 超清优化"
                        font.pixelSize: 12
                        color: Ui.Style.textPrimary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Components.GhostButton {
                        text: "稍后"
                        height: 24
                        onClicked: previewSuggestEnhance = false
                    }
                    Components.PrimaryButton {
                        text: previewEnhancing ? "处理中" : "立即优化"
                        height: 24
                        enabled: !previewEnhancing
                        onClicked: requestImageEnhance()
                    }
                }
            }

            Text {
                visible: previewEnhanceHint.length > 0
                anchors.top: enhanceBanner.bottom
                anchors.topMargin: 8
                anchors.horizontalCenter: parent.horizontalCenter
                text: previewEnhanceHint
                font.pixelSize: 12
                color: Ui.Style.textSecondary
            }

            Components.GhostButton {
                text: "关闭"
                width: 60
                height: 28
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 16
                onClicked: imagePreview.close()
            }
        }
    }

    Connections {
        target: clientBridge
        function onImageEnhanceFinished(sourceUrl, outputUrl, ok, error) {
            if (!imagePreview.visible) {
                return
            }
            var src = sourceUrl && sourceUrl.toString ? sourceUrl.toString() : (sourceUrl ? "" + sourceUrl : "")
            var current = previewImageUrl && previewImageUrl.toString ? previewImageUrl.toString()
                                                                      : (previewImageUrl ? "" + previewImageUrl : "")
            if (src.length === 0 || current.length === 0 || src !== current) {
                return
            }
            previewEnhancing = false
            if (ok && outputUrl && outputUrl.length > 0) {
                previewImageUrl = outputUrl
                previewEnhanceHint = "超清优化完成"
                previewSuggestEnhance = false
            } else {
                previewEnhanceHint = error && error.length > 0 ? error : "超清优化失败"
            }
        }
    }

    ListModel {
        id: emojiModel
    }
    ListModel {
        id: stickerModel
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

        contentItem: ColumnLayout {
            anchors.fill: parent
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Rectangle {
                    Layout.fillWidth: true
                    height: 26
                    radius: 8
                    color: emojiTabIndex === 0 ? Ui.Style.accent : Ui.Style.panelBg
                    border.color: Ui.Style.borderSubtle
                    Text {
                        anchors.centerIn: parent
                        text: Ui.I18n.t("chat.emoji")
                        font.pixelSize: 11
                        color: emojiTabIndex === 0 ? Ui.Style.textPrimary : Ui.Style.textMuted
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: emojiTabIndex = 0
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    height: 26
                    radius: 8
                    color: emojiTabIndex === 1 ? Ui.Style.accent : Ui.Style.panelBg
                    border.color: Ui.Style.borderSubtle
                    Text {
                        anchors.centerIn: parent
                        text: Ui.I18n.t("chat.sticker")
                        font.pixelSize: 11
                        color: emojiTabIndex === 1 ? Ui.Style.textPrimary : Ui.Style.textMuted
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: emojiTabIndex = 1
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: emojiTabIndex

                GridView {
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

                GridView {
                    id: stickerGrid
                    anchors.fill: parent
                    cellWidth: stickerCellSize
                    cellHeight: stickerCellSize
                    model: stickerModel
                    clip: true
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded; width: 6 }
                    delegate: Item {
                        width: stickerGrid.cellWidth
                        height: stickerGrid.cellHeight
                        AnimatedImage {
                            anchors.centerIn: parent
                            width: stickerGrid.cellWidth - 10
                            height: stickerGrid.cellHeight - 10
                            visible: animated
                            source: path
                            playing: true
                            cache: true
                            fillMode: Image.PreserveAspectFit
                        }
                        Image {
                            anchors.centerIn: parent
                            width: stickerGrid.cellWidth - 10
                            height: stickerGrid.cellHeight - 10
                            visible: !animated
                            source: path
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            antialiasing: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                Ui.AppStore.sendSticker(stickerId)
                                emojiPopup.close()
                            }
                        }
                    }
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
                            if (modelData.kind === "photo") {
                                mediaPicker.open()
                            } else if (modelData.kind === "document") {
                                filePicker.open()
                            } else if (modelData.kind === "location") {
                                locationDialog.open()
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

    Popup {
        id: locationDialog
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape
        width: 320
        property string errorText: ""
        property bool locationBusy: false
        onOpened: {
            errorText = ""
            locationBusy = false
            locationLabelField.text = ""
            locationLatField.text = ""
            locationLonField.text = ""
        }

        background: Rectangle {
            radius: 12
            color: Ui.Style.panelBgRaised
            border.color: Ui.Style.borderSubtle
        }

        contentItem: ColumnLayout {
            anchors.margins: Ui.Style.paddingM
            spacing: Ui.Style.paddingS
            Text {
                text: Ui.I18n.t("attach.locationTitle")
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: Ui.Style.textPrimary
            }
            Components.SecureTextField {
                id: locationLabelField
                Layout.fillWidth: true
                placeholderText: Ui.I18n.t("attach.locationLabel")
                font.pixelSize: 12
            }
            Components.SecureTextField {
                id: locationLatField
                Layout.fillWidth: true
                placeholderText: Ui.I18n.t("attach.locationLat")
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                font.pixelSize: 12
            }
            Components.SecureTextField {
                id: locationLonField
                Layout.fillWidth: true
                placeholderText: Ui.I18n.t("attach.locationLon")
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                font.pixelSize: 12
            }
            Components.GhostButton {
                text: locationDialog.locationBusy
                      ? Ui.I18n.t("attach.locationFetching")
                      : Ui.I18n.t("attach.locationCurrent")
                Layout.fillWidth: true
                enabled: !locationDialog.locationBusy
                onClicked: {
                    locationDialog.errorText = ""
                    locationDialog.locationBusy = true
                    locationSource.update()
                }
            }
            Text {
                visible: locationDialog.errorText.length > 0
                text: locationDialog.errorText
                color: Ui.Style.danger
                font.pixelSize: 11
                elide: Text.ElideRight
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Ui.Style.paddingS
                Components.GhostButton {
                    text: Ui.I18n.t("attach.locationCancel")
                    Layout.fillWidth: true
                    onClicked: locationDialog.close()
                }
                Components.PrimaryButton {
                    text: Ui.I18n.t("attach.locationSend")
                    Layout.fillWidth: true
                    onClicked: {
                        var lat = parseFloat(locationLatField.text)
                        var lon = parseFloat(locationLonField.text)
                        if (isNaN(lat) || isNaN(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180) {
                            locationDialog.errorText = Ui.I18n.t("attach.locationInvalid")
                            return
                        }
                        var ok = Ui.AppStore.sendLocation(lat, lon, locationLabelField.text)
                        if (ok) {
                            locationDialog.close()
                        } else {
                            locationDialog.errorText = Ui.AppStore.sendErrorMessage
                        }
                    }
                }
            }
        }
    }

    PositionSource {
        id: locationSource
        active: false
        updateInterval: 0
        onPositionChanged: {
            if (!position || !position.coordinate || !position.coordinate.isValid) {
                locationDialog.errorText = Ui.I18n.t("attach.locationUnavailable")
                locationDialog.locationBusy = false
                return
            }
            locationLatField.text = position.coordinate.latitude.toFixed(6)
            locationLonField.text = position.coordinate.longitude.toFixed(6)
            if (locationLabelField.text.trim().length === 0) {
                locationLabelField.text = Ui.I18n.t("attach.locationCurrentLabel")
            }
            locationDialog.errorText = ""
            locationDialog.locationBusy = false
        }
        onSourceErrorChanged: {
            if (sourceError !== PositionSource.NoError) {
                locationDialog.errorText = Ui.I18n.t("attach.locationUnavailable")
                locationDialog.locationBusy = false
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
            property string contentKind: model.contentKind || "text"
            property bool isEmoji: contentKind === "emoji"
            property bool isSticker: contentKind === "sticker"
            property bool isImage: contentKind === "image"
            property bool isGif: contentKind === "gif"
            property bool isVideo: contentKind === "video"
            property bool isFile: contentKind === "file"
            property bool isLocation: contentKind === "location"
            property bool isCall: contentKind === "call"
            property string fileName: model.fileName || ""
            property string fileId: model.fileId || ""
            property string fileKey: model.fileKey || ""
            property var fileUrl: model.fileUrl || ""
            property int fileSize: model.fileSize || 0
            property bool attachmentRequested: false
            property int senderAvatarSize: 26
            property int senderAvatarGap: 8
            property int senderLeftInset: showSender
                                            ? Ui.Style.paddingL + senderAvatarSize + senderAvatarGap
                                            : Ui.Style.paddingL

            height: isDate || isSystem ? 32 : bubbleBlock.height + 10

            function hasLocalUrl(value) {
                if (!value) {
                    return false
                }
                if (value.toString) {
                    return value.toString().length > 0
                }
                return ("" + value).length > 0
            }

            function requestAttachmentCache() {
                if (attachmentRequested) {
                    return
                }
                var expectedKind = Ui.AppStore.detectFileKind(fileName || "")
                if (!fileId || !fileKey || hasLocalUrl(fileUrl)) {
                    return
                }
                if (!clientBridge || !clientBridge.ensureAttachmentCached) {
                    return
                }
                attachmentRequested = true
                Qt.callLater(function() {
                    var result = clientBridge.ensureAttachmentCached(fileId, fileKey, fileName, fileSize)
                    if (result && result.ok && ListView.view && ListView.view.model) {
                        if (result.fileUrl) {
                            ListView.view.model.setProperty(index, "fileUrl", result.fileUrl)
                            if (expectedKind && expectedKind !== "file") {
                                ListView.view.model.setProperty(index, "contentKind", expectedKind)
                            }
                        }
                        if (result.previewUrl) {
                            ListView.view.model.setProperty(index, "previewUrl", result.previewUrl)
                        }
                    }
                })
            }

            Component.onCompleted: requestAttachmentCache()
            onFileUrlChanged: requestAttachmentCache()
            onFileIdChanged: requestAttachmentCache()
            onFileKeyChanged: requestAttachmentCache()
            onFileNameChanged: requestAttachmentCache()

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
                property bool transparentBubble: isSticker || isEmoji
                property bool usesFullWidth: !isSticker && !isImage && !isGif &&
                                             !isVideo && !isFile && !isLocation &&
                                             !isEmoji
                property int hPadding: transparentBubble ? (isEmoji ? 4 : 6) : 12
                property int vPadding: transparentBubble ? (isEmoji ? 4 : 6) : 8
                property real maxBubbleWidth: Math.max(260, (ListView.view ? ListView.view.width : root.width) * 0.62)
                property real contentWidth: contentLoader.item
                                             ? Math.min(maxBubbleWidth - hPadding * 2,
                                                        contentLoader.item.implicitWidth)
                                             : 0
                property real contentHeight: contentLoader.item ? contentLoader.item.implicitHeight : 0
                property real bubbleWidth: Math.min(maxBubbleWidth,
                                                    Math.max(contentWidth, metaRow.implicitWidth) + hPadding * 2)
                property real bubbleHeight: contentHeight + metaRow.implicitHeight +
                                            vPadding * 2 + (senderLabel.visible ? senderLabel.implicitHeight + 4 : 0)

                width: bubbleWidth
                height: bubbleHeight
                x: isOutgoing ? parent.width - bubbleWidth - Ui.Style.paddingL : senderLeftInset
                y: 4

                Component {
                    id: textContent
                    Item {
                        implicitWidth: textBlock.paintedWidth
                        implicitHeight: textBlock.paintedHeight
                        width: bubbleBlock.maxBubbleWidth - bubbleBlock.hPadding * 2
                        Text {
                            id: textBlock
                            text: model.text || ""
                            width: parent.width
                            wrapMode: Text.Wrap
                            color: isOutgoing ? Ui.Style.bubbleOutFg : Ui.Style.bubbleInFg
                            font.pixelSize: 13
                        }
                    }
                }

                Component {
                    id: emojiContent
                    Item {
                        implicitWidth: emojiText.paintedWidth
                        implicitHeight: emojiText.paintedHeight
                        width: implicitWidth
                        height: implicitHeight
                        Text {
                            id: emojiText
                            text: model.text || ""
                            font.pixelSize: 40
                            color: isOutgoing ? Ui.Style.bubbleOutFg : Ui.Style.bubbleInFg
                            transformOrigin: Item.Center
                            anchors.centerIn: parent
                        }
                        SequentialAnimation {
                            running: isEmoji && (model.animateEmoji === true)
                            loops: 1
                            ParallelAnimation {
                                NumberAnimation { target: emojiText; property: "scale"; from: 0.6; to: 1.2; duration: 200; easing.type: Easing.OutBack }
                                NumberAnimation { target: emojiText; property: "opacity"; from: 0.4; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
                            }
                            NumberAnimation { target: emojiText; property: "scale"; from: 1.2; to: 1.0; duration: 160; easing.type: Easing.OutCubic }
                            onStopped: {
                                if (model.animateEmoji === true && ListView.view && ListView.view.model) {
                                    ListView.view.model.setProperty(index, "animateEmoji", false)
                                }
                            }
                        }
                    }
                }

                Component {
                    id: stickerContent
                    Item {
                        implicitWidth: stickerSize
                        implicitHeight: stickerSize
                        AnimatedImage {
                            anchors.centerIn: parent
                            width: stickerSize
                            height: stickerSize
                            visible: stickerAnimated
                            source: stickerUrl
                            playing: true
                            cache: true
                            fillMode: Image.PreserveAspectFit
                        }
                        Image {
                            anchors.centerIn: parent
                            width: stickerSize
                            height: stickerSize
                            visible: !stickerAnimated
                            source: stickerUrl
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            antialiasing: true
                        }
                    }
                }

                Component {
                    id: imageContent
                    Item {
                        implicitWidth: 240
                        implicitHeight: 180
                        Image {
                            anchors.centerIn: parent
                            width: 240
                            height: 180
                            source: fileUrl
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            antialiasing: true
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.openImagePreview(fileUrl, fileName)
                        }
                    }
                }

                Component {
                    id: gifContent
                    Item {
                        implicitWidth: 240
                        implicitHeight: 180
                        AnimatedImage {
                            anchors.centerIn: parent
                            width: 240
                            height: 180
                            source: fileUrl
                            playing: true
                            cache: true
                            fillMode: Image.PreserveAspectFit
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.openImagePreview(fileUrl, fileName)
                        }
                    }
                }

                Component {
                    id: videoContent
                    Item {
                        id: videoItem
                        implicitWidth: 260
                        implicitHeight: 180
                        property bool hasSource: fileUrl && fileUrl.toString ? fileUrl.toString().length > 0
                                               : (fileUrl && ("" + fileUrl).length > 0)
                        property bool previewAvailable: previewUrl && previewUrl.toString
                                                        ? previewUrl.toString().length > 0
                                                        : (previewUrl && ("" + previewUrl).length > 0)

                        Rectangle {
                            anchors.fill: parent
                            radius: 12
                            color: Ui.Style.panelBgAlt
                            border.color: Ui.Style.borderSubtle
                        }

                        MediaPlayer {
                            id: videoPlayer
                            source: fileUrl
                            videoOutput: videoView
                            audioOutput: AudioOutput {
                                volume: 1.0
                            }
                        }

                        VideoOutput {
                            id: videoView
                            anchors.fill: parent
                            fillMode: VideoOutput.PreserveAspectFit
                            visible: videoItem.hasSource
                        }

                        Image {
                            anchors.fill: parent
                            source: previewUrl || ""
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            antialiasing: true
                            visible: !videoItem.hasSource
                                     ? true
                                     : (videoPlayer.playbackState !== MediaPlayer.PlayingState &&
                                        videoItem.previewAvailable)
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 48
                            height: 48
                            radius: 24
                            color: Qt.rgba(0, 0, 0, 0.45)
                            visible: videoItem.hasSource &&
                                     videoPlayer.playbackState !== MediaPlayer.PlayingState
                            Image {
                                anchors.centerIn: parent
                                source: "qrc:/mi/e2ee/ui/icons/video.svg"
                                width: 20
                                height: 20
                                opacity: 0.9
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                if (!videoItem.hasSource) {
                                    return
                                }
                                if (videoPlayer.playbackState === MediaPlayer.PlayingState) {
                                    videoPlayer.pause()
                                } else {
                                    videoPlayer.play()
                                }
                            }
                        }

                        onVisibleChanged: {
                            if (!visible && videoPlayer.playbackState === MediaPlayer.PlayingState) {
                                videoPlayer.pause()
                            }
                        }
                    }
                }

                Component {
                    id: fileContent
                    Item {
                        implicitWidth: 220
                        implicitHeight: 60
                        property real progressValue: (downloadProgress !== undefined
                                                       && downloadProgress !== null)
                                                      ? downloadProgress : 0
                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: Ui.Style.panelBgAlt
                            border.color: Ui.Style.borderSubtle
                        }
                        Row {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8
                            Rectangle {
                                width: 32
                                height: 32
                                radius: 8
                                color: Ui.Style.hoverBg
                                Text {
                                    anchors.centerIn: parent
                                    text: "FILE"
                                    font.pixelSize: 9
                                    color: Ui.Style.textMuted
                                }
                            }
                            Column {
                                spacing: 2
                                width: parent.width - 48
                                Text {
                                    text: fileName || ""
                                    font.pixelSize: 11
                                    color: Ui.Style.textPrimary
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: fileSize > 0 ? (Math.round(fileSize / 1024) + " KB") : ""
                                    font.pixelSize: 10
                                    color: Ui.Style.textMuted
                                }
                            }
                        }
                        Canvas {
                            id: downloadRing
                            width: 18
                            height: 18
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: 6
                            property real progress: Math.max(0, Math.min(1, fileContent.progressValue))
                            onProgressChanged: requestPaint()
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.clearRect(0, 0, width, height)
                                var cx = width / 2
                                var cy = height / 2
                                var r = Math.min(width, height) / 2 - 1.5
                                ctx.lineWidth = 3
                                ctx.lineCap = "round"
                                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.25)
                                ctx.beginPath()
                                ctx.arc(cx, cy, r, 0, Math.PI * 2)
                                ctx.stroke()
                                if (progress > 0) {
                                    ctx.strokeStyle = Ui.Style.success
                                    ctx.beginPath()
                                    ctx.arc(cx, cy, r,
                                            -Math.PI / 2,
                                            -Math.PI / 2 + progress * Math.PI * 2)
                                    ctx.stroke()
                                }
                            }
                        }
                        Menu {
                            id: fileContextMenu
                            MenuItem {
                                text: Ui.I18n.t("chat.fileDownloadConfirm")
                                onTriggered: root.promptFileDownload(fileId, fileKey, fileName, fileSize, true)
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: {
                                if (mouse.button === Qt.RightButton) {
                                    var pos = mapToItem(root, mouse.x, mouse.y)
                                    fileContextMenu.popup(root, pos.x, pos.y)
                                    return
                                }
                                root.promptFileDownload(fileId, fileKey, fileName, fileSize)
                            }
                        }
                    }
                }

                Component {
                    id: locationContent
                    Item {
                        implicitWidth: 220
                        implicitHeight: 72
                        Rectangle {
                            anchors.fill: parent
                            radius: 10
                            color: Ui.Style.panelBgAlt
                            border.color: Ui.Style.borderSubtle
                        }
                        Column {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4
                            Text {
                                text: (locationLabel && locationLabel.length > 0)
                                      ? locationLabel
                                      : Ui.I18n.t("attach.location")
                                font.pixelSize: 12
                                color: Ui.Style.textPrimary
                                elide: Text.ElideRight
                            }
                            Text {
                                text: "lat:" + Number(locationLat).toFixed(5) + ", lon:" + Number(locationLon).toFixed(5)
                                font.pixelSize: 10
                                color: Ui.Style.textMuted
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Rectangle {
                    id: bubble
                    anchors.fill: parent
                    radius: 12
                    color: bubbleBlock.transparentBubble
                           ? "transparent"
                           : (isOutgoing ? Ui.Style.bubbleOutBg : Ui.Style.bubbleInBg)
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

                        Loader {
                            id: contentLoader
                            width: bubbleBlock.usesFullWidth
                                   ? bubbleBlock.maxBubbleWidth - bubbleBlock.hPadding * 2
                                   : bubbleBlock.contentWidth
                            sourceComponent: isSticker ? stickerContent
                                            : (isGif ? gifContent
                                            : (isImage ? imageContent
                                            : (isVideo ? videoContent
                                            : (isFile ? fileContent
                                            : (isLocation ? locationContent
                                            : (isEmoji ? emojiContent : textContent))))))
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
                    visible: isIncoming && !bubbleBlock.transparentBubble
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
                    visible: isOutgoing && !bubbleBlock.transparentBubble
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
