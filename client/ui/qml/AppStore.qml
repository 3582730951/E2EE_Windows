pragma Singleton
import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

QtObject {
    id: store

    property int currentPage: 0
    property string currentChatId: ""
    property string currentChatTitle: ""
    property string currentChatSubtitle: ""
    property string currentChatType: "private"
    property int currentChatMembers: 0
    property int currentLeftTab: 0
    property bool rightPaneVisible: false
    property string searchQuery: ""
    property bool initialized: false
    property string statusMessage: ""
    property string sendErrorMessage: ""
    property bool clipboardIsolationEnabled: true
    property bool internalImeEnabled: true
    property string internalClipboardText: ""
    property double internalClipboardMs: 0
    property var messagesByChatId: ({})
    property var membersByChatId: ({})
    property var typingByChatId: ({})
    property var presenceByChatId: ({})

    signal currentChatChanged(string chatId)
    signal leftTabChanged(int tab)
    signal rightPaneVisibilityChanged(bool visible)

    property ListModel dialogsModel: ListModel {}
    property ListModel contactsModel: ListModel {}
    property ListModel membersModel: ListModel {}
    property ListModel filteredDialogsModel: ListModel {}
    property ListModel filteredContactsModel: ListModel {}
    property ListModel friendRequestsModel: ListModel {}

    function init() {
        if (initialized) {
            return
        }
        initialized = true
        if (clientBridge) {
            clientBridge.init("")
            if (clientBridge.setClipboardIsolation) {
                clientBridge.setClipboardIsolation(clipboardIsolationEnabled)
            }
            if (clientBridge.setInternalImeEnabled) {
                clientBridge.setInternalImeEnabled(internalImeEnabled)
            }
        }
        rebuildFiltered()
    }

    function bootstrapAfterLogin() {
        rebuildDialogs()
        refreshFriendRequests()
        rebuildFiltered()
        if (dialogsModel.count > 0 && currentChatId.length === 0) {
            setCurrentChat(dialogsModel.get(0).chatId)
        }
    }

    function rebuildDialogs() {
        contactsModel.clear()
        var targets = {}

        var friends = clientBridge ? clientBridge.friends : []
        for (var i = 0; i < friends.length; ++i) {
            var f = friends[i]
            var username = (f.username || "").trim()
            if (username.length === 0) {
                continue
            }
            var display = (f.remark && f.remark.length > 0) ? f.remark : username
            contactsModel.append({
                contactId: username,
                displayName: display,
                usernameOrPhone: username,
                avatarKey: display
            })
            targets[username] = {
                type: "private",
                title: display,
                memberCount: 2,
                avatarKey: display
            }
        }

        var groups = clientBridge ? clientBridge.groups : []
        for (var g = 0; g < groups.length; ++g) {
            var group = groups[g]
            var groupId = (group.id || "").trim()
            if (groupId.length === 0) {
                continue
            }
            var groupTitle = (group.name && group.name.length > 0) ? group.name : groupId
            targets[groupId] = {
                type: "group",
                title: groupTitle,
                memberCount: group.memberCount || 0,
                avatarKey: groupTitle
            }
        }

        for (var key in targets) {
            if (!targets.hasOwnProperty(key)) {
                continue
            }
            var entry = targets[key]
            ensureDialog(key, entry.type, entry.title, entry.memberCount, entry.avatarKey)
        }

        for (var j = dialogsModel.count - 1; j >= 0; --j) {
            var id = dialogsModel.get(j).chatId
            if (!targets.hasOwnProperty(id)) {
                dialogsModel.remove(j)
            }
        }
        rebuildFiltered()
        updateCurrentChatDetails()
    }

    function refreshFriendRequests() {
        friendRequestsModel.clear()
        var requests = clientBridge ? clientBridge.friendRequests : []
        for (var i = 0; i < requests.length; ++i) {
            var r = requests[i]
            var name = r.username || ""
            friendRequestsModel.append({
                username: name,
                remark: r.remark || ""
            })
        }
    }

    function ensureDialog(chatId, type, title, memberCount, avatarKey) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            dialogsModel.append({
                chatId: chatId,
                type: type,
                title: title,
                preview: "",
                timeText: "",
                unread: 0,
                pinned: false,
                muted: false,
                stealth: false,
                avatarKey: avatarKey || title,
                lastSenderName: "",
                lastSenderAvatarKey: "",
                memberCount: memberCount || (type === "group" ? 0 : 2)
            })
            return
        }
        if (dialogsModel.get(idx).title !== title) {
            dialogsModel.setProperty(idx, "title", title)
        }
        if (dialogsModel.get(idx).type !== type) {
            dialogsModel.setProperty(idx, "type", type)
        }
        if (avatarKey) {
            dialogsModel.setProperty(idx, "avatarKey", avatarKey)
        }
        if (dialogsModel.get(idx).muted === undefined) {
            dialogsModel.setProperty(idx, "muted", false)
        }
        if (dialogsModel.get(idx).stealth === undefined) {
            dialogsModel.setProperty(idx, "stealth", false)
        }
        if (memberCount && memberCount > 0) {
            dialogsModel.setProperty(idx, "memberCount", memberCount)
        }
    }

    function handleMessageEvent(message) {
        if (!message || !message.kind) {
            return
        }
        var kind = message.kind
        var convId = (message.convId || "").trim()
        if (convId.length === 0) {
            return
        }

        if (kind === "delivery" || kind === "read") {
            updateMessageStatus(convId, message.messageId || "", kind)
            return
        }
        if (kind === "typing") {
            typingByChatId[convId] = message.typing === true
            if (convId === currentChatId) {
                updateCurrentChatDetails()
            }
            return
        }
        if (kind === "presence") {
            presenceByChatId[convId] = message.online === true
            if (convId === currentChatId) {
                updateCurrentChatDetails()
            }
            return
        }

        var isGroup = message.isGroup === true
        var outgoing = message.outgoing === true
        var sender = message.sender || ""
        var msgId = message.messageId || nextMsgId()
        var timeText = message.time || formatTime(new Date())

        var text = message.text || ""
        if (kind === "file") {
            var name = message.fileName || ""
            text = name.length > 0 ? "[文件] " + name : "[文件]"
        } else if (kind === "sticker") {
            var sticker = message.stickerId || ""
            text = sticker.length > 0 ? "[贴纸] " + sticker : "[贴纸]"
        } else if (kind === "call_invite") {
            text = message.video ? "视频通话邀请" : "语音通话邀请"
        } else if (kind === "group_invite") {
            text = "邀请加入群聊"
        }

        ensureDialog(convId, isGroup ? "group" : "private",
                     resolveTitle(convId),
                     isGroup ? currentChatMembers : 2,
                     resolveTitle(convId))

        var entryKind = "in"
        if (kind === "notice" || kind === "group_invite") {
            entryKind = "system"
        } else if (outgoing) {
            entryKind = "out"
        }

        var displaySender = outgoing ? Ui.I18n.t("chat.you") : (sender || resolveTitle(convId))
        var entry = {
            chatId: convId,
            msgId: msgId,
            kind: entryKind,
            senderName: displaySender,
            text: text,
            timeText: timeText,
            statusTicks: outgoing ? "sent" : "none",
            edited: false
        }

        if (msgId && hasMessageId(convId, msgId)) {
            return
        }
        appendMessage(convId, entry, convId !== currentChatId)
        if (isGroup && entryKind !== "system") {
            updateDialogPreview(convId, text, timeText, displaySender)
        } else {
            updateDialogPreview(convId, text, timeText, "")
        }

        if (convId === currentChatId && !outgoing && !isGroup && entryKind === "in") {
            if (clientBridge) {
                clientBridge.sendReadReceipt(convId, msgId)
            }
        }
    }

    function messagesModel(chatId) {
        if (!messagesByChatId[chatId]) {
            var model = Qt.createQmlObject("import QtQuick 2.15; ListModel {}", store)
            messagesByChatId[chatId] = model
        }
        return messagesByChatId[chatId]
    }

    function setSearchQuery(value) {
        searchQuery = value
        rebuildFiltered()
    }

    function rebuildFiltered() {
        filteredDialogsModel.clear()
        filteredContactsModel.clear()
        var query = (searchQuery || "").toLowerCase()
        for (var i = 0; i < dialogsModel.count; ++i) {
            var dialog = dialogsModel.get(i)
            var titleText = dialog.title || ""
            var previewText = dialog.preview || ""
            if (query.length === 0 ||
                titleText.toLowerCase().indexOf(query) !== -1 ||
                previewText.toLowerCase().indexOf(query) !== -1) {
                filteredDialogsModel.append(dialog)
            }
        }
        for (var j = 0; j < contactsModel.count; ++j) {
            var contact = contactsModel.get(j)
            var contactName = contact.displayName || ""
            var contactHandle = contact.usernameOrPhone || ""
            if (query.length === 0 ||
                contactName.toLowerCase().indexOf(query) !== -1 ||
                contactHandle.toLowerCase().indexOf(query) !== -1) {
                filteredContactsModel.append(contact)
            }
        }
    }

    function setCurrentChat(chatId) {
        if (!chatId) {
            return
        }
        currentChatId = chatId
        updateCurrentChatDetails()
        loadHistoryForChat(chatId)
        markDialogRead(chatId)
        currentChatChanged(chatId)
    }

    function updateCurrentChatDetails() {
        var idx = findDialogIndex(currentChatId)
        if (idx < 0) {
            currentChatTitle = ""
            currentChatSubtitle = ""
            currentChatType = "private"
            currentChatMembers = 0
            membersModel.clear()
            return
        }
        var dialog = dialogsModel.get(idx)
        currentChatTitle = dialog.title
        currentChatType = dialog.type
        currentChatMembers = dialog.memberCount || 0
        if (currentChatType === "group") {
            currentChatSubtitle = Ui.I18n.format("chat.members", currentChatMembers)
        } else {
            var typing = typingByChatId[currentChatId] === true
            if (typing) {
                currentChatSubtitle = "正在输入..."
            } else {
                currentChatSubtitle = Ui.I18n.t("chat.online")
            }
        }
        updateMembers(dialog.chatId)
    }

    function updateMembers(chatId) {
        membersModel.clear()
        if (currentChatType !== "group") {
            return
        }
        var list = clientBridge ? clientBridge.listGroupMembersInfo(chatId) : []
        for (var i = 0; i < list.length; ++i) {
            var entry = list[i]
            var roleText = Ui.I18n.t("role.member")
            if (entry.role === 0) {
                roleText = Ui.I18n.t("role.owner")
            } else if (entry.role === 1) {
                roleText = Ui.I18n.t("role.admin")
            }
            membersModel.append({
                memberId: entry.username,
                displayName: entry.username,
                role: roleText,
                avatarKey: entry.username
            })
        }
        currentChatMembers = membersModel.count
    }

    function loadHistoryForChat(chatId) {
        var model = messagesModel(chatId)
        model.clear()
        if (!clientBridge) {
            return
        }
        var isGroup = currentChatType === "group"
        var history = clientBridge.loadHistory(chatId, isGroup)
        for (var i = 0; i < history.length; ++i) {
            var h = history[i]
            var entryKind = "in"
            if (h.kind === "system") {
                entryKind = "system"
            } else if (h.outgoing === true) {
                entryKind = "out"
            }

            var text = h.text || ""
            if (h.kind === "file") {
                text = h.fileName ? "[文件] " + h.fileName : "[文件]"
            } else if (h.kind === "sticker") {
                text = h.stickerId ? "[贴纸] " + h.stickerId : "[贴纸]"
            }

            var status = h.status || "sent"
            var ticks = "none"
            if (h.outgoing === true) {
                if (status === "read") {
                    ticks = "read"
                } else if (status === "delivered") {
                    ticks = "delivered"
                } else if (status === "sent") {
                    ticks = "sent"
                }
            }

            model.append({
                chatId: chatId,
                msgId: h.messageId || nextMsgId(),
                kind: entryKind,
                senderName: h.outgoing ? Ui.I18n.t("chat.you") : (h.sender || ""),
                text: text,
                timeText: h.time || "",
                statusTicks: ticks,
                edited: false
            })
        }
    }

    function setLeftTab(tabIndex) {
        if (currentLeftTab === tabIndex) {
            return
        }
        currentLeftTab = tabIndex
        leftTabChanged(tabIndex)
    }

    function toggleRightPane() {
        rightPaneVisible = !rightPaneVisible
        rightPaneVisibilityChanged(rightPaneVisible)
    }

    function closeRightPane() {
        if (!rightPaneVisible) {
            return
        }
        rightPaneVisible = false
        rightPaneVisibilityChanged(rightPaneVisible)
    }

    function openChatFromContact(contactId) {
        var dialogIdx = findDialogIndex(contactId)
        if (dialogIdx < 0) {
            var contact = findContact(contactId)
            if (!contact) {
                return
            }
            ensureDialog(contactId, "private", contact.displayName, 2, contact.displayName)
            rebuildFiltered()
        }
        setCurrentChat(contactId)
    }

    function sendMessage(text) {
        sendErrorMessage = ""
        if (!currentChatId || !clientBridge) {
            sendErrorMessage = Ui.I18n.t("chat.sendFailed")
            return false
        }
        var trimmed = (text || "").trim()
        if (trimmed.length === 0) {
            return false
        }
        var ok = clientBridge.sendText(currentChatId, trimmed, currentChatType === "group")
        if (!ok) {
            var err = clientBridge.lastError || ""
            if (err.length === 0 && clientBridge.remoteOk === false) {
                err = clientBridge.remoteError || ""
            }
            sendErrorMessage = err.length > 0 ? err : Ui.I18n.t("chat.sendFailed")
        }
        return ok
    }

    function sendFile(path) {
        sendErrorMessage = ""
        if (!currentChatId || !clientBridge) {
            sendErrorMessage = Ui.I18n.t("chat.sendFailed")
            return false
        }
        if (!path || path.length === 0) {
            return false
        }
        var ok = clientBridge.sendFile(currentChatId, path, currentChatType === "group")
        if (!ok) {
            var err = clientBridge.lastError || ""
            sendErrorMessage = err.length > 0 ? err : Ui.I18n.t("chat.sendFailed")
        }
        return ok
    }

    function sendLocation() {
        return sendMessage("[" + Ui.I18n.t("attach.location") + "]")
    }

    function appendMessage(chatId, message, markUnread) {
        var model = messagesModel(chatId)
        model.append(message)
        updateDialogPreview(chatId, message.text || "", message.timeText || "", message.senderName || "")
        if (markUnread) {
            bumpUnread(chatId)
        }
    }

    function updateDialogPreview(chatId, preview, timeText, senderName) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            return
        }
        dialogsModel.setProperty(idx, "preview", preview)
        dialogsModel.setProperty(idx, "timeText", timeText)
        if (senderName && dialogsModel.get(idx).type === "group") {
            dialogsModel.setProperty(idx, "lastSenderName", senderName)
            dialogsModel.setProperty(idx, "lastSenderAvatarKey", senderName)
        }
        rebuildFiltered()
    }

    function bumpUnread(chatId) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            return
        }
        var count = dialogsModel.get(idx).unread || 0
        dialogsModel.setProperty(idx, "unread", count + 1)
        rebuildFiltered()
    }

    function markDialogRead(chatId) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            return
        }
        dialogsModel.setProperty(idx, "unread", 0)
        rebuildFiltered()
    }

    function togglePin(chatId) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            return
        }
        var pinned = dialogsModel.get(idx).pinned
        dialogsModel.setProperty(idx, "pinned", !pinned)
        if (!pinned) {
            dialogsModel.move(idx, 0, 1)
        }
        rebuildFiltered()
    }

    function isChatMuted(chatId) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            return false
        }
        return dialogsModel.get(idx).muted === true
    }

    function isChatStealth(chatId) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            return false
        }
        return dialogsModel.get(idx).stealth === true
    }

    function setChatMuted(chatId, muted) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            return
        }
        dialogsModel.setProperty(idx, "muted", muted === true)
        rebuildFiltered()
    }

    function setChatStealth(chatId, stealth) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            return
        }
        dialogsModel.setProperty(idx, "stealth", stealth === true)
        rebuildFiltered()
    }

    function toggleChatMuted(chatId) {
        setChatMuted(chatId, !isChatMuted(chatId))
    }

    function toggleChatStealth(chatId) {
        setChatStealth(chatId, !isChatStealth(chatId))
    }

    function setClipboardIsolationEnabled(enabled) {
        clipboardIsolationEnabled = enabled === true
        if (!clipboardIsolationEnabled) {
            internalClipboardText = ""
            internalClipboardMs = 0
        }
        if (clientBridge && clientBridge.setClipboardIsolation) {
            clientBridge.setClipboardIsolation(clipboardIsolationEnabled)
        }
    }

    function setInternalImeEnabled(enabled) {
        internalImeEnabled = enabled === true
        if (!internalImeEnabled && clientBridge && clientBridge.imeReset) {
            clientBridge.imeReset()
        }
        if (clientBridge && clientBridge.setInternalImeEnabled) {
            clientBridge.setInternalImeEnabled(internalImeEnabled)
        }
    }

    function setInternalClipboard(text) {
        internalClipboardText = text || ""
        internalClipboardMs = Date.now()
    }

    function clearSendError() {
        sendErrorMessage = ""
    }

    function removeChat(chatId) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            return
        }
        dialogsModel.remove(idx)
        rebuildFiltered()
        if (currentChatId === chatId) {
            if (dialogsModel.count > 0) {
                setCurrentChat(dialogsModel.get(0).chatId)
            } else {
                currentChatId = ""
                updateCurrentChatDetails()
            }
        }
    }

    function addContact(name, handle) {
        var displayName = (name || "").trim()
        var handleText = (handle || "").trim()
        var target = handleText.length ? handleText : displayName
        if (!clientBridge || target.length === 0) {
            return false
        }
        return clientBridge.sendFriendRequest(target, displayName)
    }

    function respondFriendRequest(username, accept) {
        var who = (username || "").trim()
        if (!clientBridge || who.length === 0) {
            return false
        }
        var ok = clientBridge.respondFriendRequest(who, accept === true)
        if (ok) {
            refreshFriendRequests()
            rebuildDialogs()
        }
        return ok
    }

    function createGroup(name, memberIds) {
        if (!clientBridge) {
            return ""
        }
        var groupId = clientBridge.createGroup()
        if (!groupId || groupId.length === 0) {
            return ""
        }
        if (memberIds && memberIds.length) {
            for (var i = 0; i < memberIds.length; ++i) {
                clientBridge.sendGroupInvite(groupId, memberIds[i])
            }
        }
        ensureDialog(groupId, "group", name.length ? name : groupId, memberIds ? memberIds.length + 1 : 1, name.length ? name : groupId)
        rebuildFiltered()
        setCurrentChat(groupId)
        return groupId
    }

    function findDialogIndex(chatId) {
        for (var i = 0; i < dialogsModel.count; ++i) {
            if (dialogsModel.get(i).chatId === chatId) {
                return i
            }
        }
        return -1
    }

    function findContact(contactId) {
        for (var i = 0; i < contactsModel.count; ++i) {
            var contact = contactsModel.get(i)
            if (contact.contactId === contactId) {
                return contact
            }
        }
        return null
    }

    function resolveTitle(chatId) {
        var idx = findDialogIndex(chatId)
        if (idx >= 0) {
            return dialogsModel.get(idx).title
        }
        var contact = findContact(chatId)
        if (contact) {
            return contact.displayName
        }
        return chatId
    }

    function updateMessageStatus(chatId, messageId, status) {
        if (!messageId) {
            return
        }
        var model = messagesModel(chatId)
        for (var i = model.count - 1; i >= 0; --i) {
            var entry = model.get(i)
            if (entry.msgId === messageId) {
                if (status === "read") {
                    model.setProperty(i, "statusTicks", "read")
                } else if (status === "delivery") {
                    model.setProperty(i, "statusTicks", "delivered")
                }
                break
            }
        }
    }

    function hasMessageId(chatId, messageId) {
        if (!messageId) {
            return false
        }
        var model = messagesModel(chatId)
        for (var i = model.count - 1; i >= 0; --i) {
            if (model.get(i).msgId === messageId) {
                return true
            }
        }
        return false
    }

    function nextMsgId() {
        return "m_" + Math.floor(Math.random() * 1e9)
    }

    function formatTime(date) {
        var hours = date.getHours()
        var minutes = date.getMinutes()
        var hh = hours < 10 ? "0" + hours : "" + hours
        var mm = minutes < 10 ? "0" + minutes : "" + minutes
        return hh + ":" + mm
    }

    property var bridgeConnections: Connections {
        target: clientBridge
        function onFriendsChanged() {
            rebuildDialogs()
        }
        function onGroupsChanged() {
            rebuildDialogs()
        }
        function onFriendRequestsChanged() {
            refreshFriendRequests()
        }
        function onMessageEvent(message) {
            handleMessageEvent(message)
        }
        function onStatus(message) {
            statusMessage = message
        }
        function onTokenChanged() {
            if (clientBridge && clientBridge.loggedIn) {
                bootstrapAfterLogin()
            }
        }
    }

    Component.onCompleted: init()
}
