pragma Singleton
import QtQuick 2.15

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
    property var messagesByChatId: ({})
    property var membersByChatId: ({})
    property var pendingReply: null

    signal currentChatChanged(string chatId)
    signal leftTabChanged(int tab)
    signal rightPaneVisibilityChanged(bool visible)

    property ListModel dialogsModel: ListModel {}
    property ListModel contactsModel: ListModel {}
    property ListModel membersModel: ListModel {}
    property ListModel filteredDialogsModel: ListModel {}
    property ListModel filteredContactsModel: ListModel {}

    property Timer replyTimer: Timer {
        interval: 700
        repeat: false
        onTriggered: {
            if (!pendingReply) {
                return
            }
            var payload = pendingReply
            pendingReply = null
            var replyText = payload.text
            if (!replyText || replyText.length === 0) {
                replyText = "Got it."
            }
            var timeText = formatTime(new Date())
            var sender = payload.sender || "Echo"
            var message = {
                chatId: payload.chatId,
                msgId: nextMsgId(),
                kind: "in",
                senderName: sender,
                text: replyText,
                timeText: timeText,
                statusTicks: "none",
                edited: false
            }
            appendMessage(payload.chatId, message, payload.chatId !== currentChatId)
        }
    }

    function init() {
        if (initialized) {
            return
        }
        initialized = true
        seedContacts()
        seedDialogs()
        seedMessages()
        rebuildFiltered()
    }

    function seedContacts() {
        var contacts = [
            { id: "c_alice", name: "Alice", handle: "+1 202-555-0101" },
            { id: "c_ben", name: "Ben Carter", handle: "@ben" },
            { id: "c_chloe", name: "Chloe", handle: "@chloe" },
            { id: "c_diego", name: "Diego", handle: "+49 30 90210" },
            { id: "c_eva", name: "Eva", handle: "@eva" },
            { id: "c_felix", name: "Felix", handle: "+81 03 5555 1234" },
            { id: "c_grace", name: "Grace", handle: "@grace" },
            { id: "c_henry", name: "Henry", handle: "+44 20 7788 9900" }
        ]
        for (var i = 0; i < contacts.length; ++i) {
            contactsModel.append({
                contactId: contacts[i].id,
                displayName: contacts[i].name,
                usernameOrPhone: contacts[i].handle,
                avatarKey: contacts[i].name
            })
        }
    }

    function seedDialogs() {
        var dialogs = [
            { id: "c_alice", title: "Alice", preview: "Boarding now, talk soon.", time: "08:42", unread: 1, pinned: false, type: "private" },
            { id: "c_ben", title: "Ben Carter", preview: "Shipping the draft in 10.", time: "09:15", unread: 0, pinned: false, type: "private" },
            { id: "c_chloe", title: "Chloe", preview: "Call me when you are free.", time: "10:01", unread: 2, pinned: true, type: "private" },
            { id: "g_nightfall", title: "Project Nightfall", preview: "Standup moved to 10:30.", time: "11:20", unread: 4, pinned: false, type: "group", members: 6 }
        ]
        for (var i = 0; i < dialogs.length; ++i) {
            dialogsModel.append({
                chatId: dialogs[i].id,
                type: dialogs[i].type,
                title: dialogs[i].title,
                preview: dialogs[i].preview,
                timeText: dialogs[i].time,
                unread: dialogs[i].unread,
                pinned: dialogs[i].pinned,
                avatarKey: dialogs[i].title,
                memberCount: dialogs[i].members || 2
            })
        }
    }

    function seedMessages() {
        var privateChats = ["c_alice", "c_ben", "c_chloe"]
        for (var i = 0; i < privateChats.length; ++i) {
            var chatId = privateChats[i]
            var model = messagesModel(chatId)
            model.append({ chatId: chatId, msgId: nextMsgId(), kind: "date", text: "Today" })
            model.append({ chatId: chatId, msgId: nextMsgId(), kind: "system", text: "Secure session started" })
            for (var j = 0; j < 12; ++j) {
                var outgoing = j % 3 === 1
                model.append({
                    chatId: chatId,
                    msgId: nextMsgId(),
                    kind: outgoing ? "out" : "in",
                    senderName: outgoing ? "You" : "Contact",
                    text: sampleLine(chatId, j),
                    timeText: formatTimeOffset(j * 7 + 5),
                    statusTicks: outgoing ? "read" : "none",
                    edited: false
                })
            }
        }

        var groupId = "g_nightfall"
        var groupModel = messagesModel(groupId)
        groupModel.append({ chatId: groupId, msgId: nextMsgId(), kind: "date", text: "Today" })
        groupModel.append({ chatId: groupId, msgId: nextMsgId(), kind: "system", text: "You joined the group" })
        var groupSenders = ["Mia", "Oliver", "Liam", "Ava", "Noah", "Zoe"]
        for (var k = 0; k < 25; ++k) {
            var outgoingGroup = k % 4 === 0
            groupModel.append({
                chatId: groupId,
                msgId: nextMsgId(),
                kind: outgoingGroup ? "out" : "in",
                senderName: outgoingGroup ? "You" : groupSenders[k % groupSenders.length],
                text: groupLine(k),
                timeText: formatTimeOffset(k * 3 + 2),
                statusTicks: outgoingGroup ? "delivered" : "none",
                edited: false
            })
        }

        membersByChatId[groupId] = [
            { memberId: "m_mia", name: "Mia", role: "Owner" },
            { memberId: "m_oliver", name: "Oliver", role: "Admin" },
            { memberId: "m_liam", name: "Liam", role: "Member" },
            { memberId: "m_ava", name: "Ava", role: "Member" },
            { memberId: "m_noah", name: "Noah", role: "Member" },
            { memberId: "m_zoe", name: "Zoe", role: "Member" }
        ]
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
        currentChatMembers = dialog.memberCount || 2
        currentChatSubtitle = dialog.type === "group"
            ? currentChatMembers + " members"
            : "online"
        updateMembers(dialog.chatId)
    }

    function updateMembers(chatId) {
        membersModel.clear()
        var list = membersByChatId[chatId]
        if (!list) {
            return
        }
        for (var i = 0; i < list.length; ++i) {
            membersModel.append({
                memberId: list[i].memberId,
                displayName: list[i].name,
                role: list[i].role,
                avatarKey: list[i].name
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
            dialogsModel.insert(0, {
                chatId: contactId,
                type: "private",
                title: contact.displayName,
                preview: "",
                timeText: "",
                unread: 0,
                pinned: false,
                avatarKey: contact.displayName,
                memberCount: 2
            })
            rebuildFiltered()
        }
        setCurrentChat(contactId)
    }

    function sendMessage(text) {
        if (!currentChatId) {
            return
        }
        var trimmed = (text || "").trim()
        if (trimmed.length === 0) {
            return
        }
        var timeText = formatTime(new Date())
        var message = {
            chatId: currentChatId,
            msgId: nextMsgId(),
            kind: "out",
            senderName: "You",
            text: trimmed,
            timeText: timeText,
            statusTicks: "sent",
            edited: false
        }
        appendMessage(currentChatId, message, false)
        pendingReply = {
            chatId: currentChatId,
            sender: currentChatType === "group" ? "Mia" : "Contact",
            text: "Echo: " + trimmed
        }
        replyTimer.restart()
    }

    function appendMessage(chatId, message, markUnread) {
        var model = messagesModel(chatId)
        model.append(message)
        updateDialogPreview(chatId, message.text || "", message.timeText || "")
        if (markUnread) {
            bumpUnread(chatId)
        }
    }

    function updateDialogPreview(chatId, preview, timeText) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            return
        }
        dialogsModel.setProperty(idx, "preview", preview)
        dialogsModel.setProperty(idx, "timeText", timeText)
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
        if (displayName.length === 0) {
            return false
        }
        var contactId = "c_" + displayName.toLowerCase().replace(/\s+/g, "_")
        contactsModel.append({
            contactId: contactId,
            displayName: displayName,
            usernameOrPhone: handleText.length ? handleText : "@new",
            avatarKey: displayName
        })
        rebuildFiltered()
        return true
    }

    function createGroup(name, memberIds) {
        var title = (name || "").trim()
        if (title.length === 0) {
            return ""
        }
        var groupId = "g_" + title.toLowerCase().replace(/\s+/g, "_")
        dialogsModel.insert(0, {
            chatId: groupId,
            type: "group",
            title: title,
            preview: "Group created",
            timeText: formatTime(new Date()),
            unread: 0,
            pinned: false,
            avatarKey: title,
            memberCount: (memberIds ? memberIds.length : 1) + 1
        })
        membersByChatId[groupId] = []
        if (memberIds && memberIds.length) {
            for (var i = 0; i < memberIds.length; ++i) {
                var contact = findContact(memberIds[i])
                if (contact) {
                    membersByChatId[groupId].push({
                        memberId: contact.contactId,
                        name: contact.displayName,
                        role: "Member"
                    })
                }
            }
        }
        membersByChatId[groupId].unshift({
            memberId: "m_you",
            name: "You",
            role: "Owner"
        })
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

    function formatTimeOffset(minutesAgo) {
        var d = new Date()
        d.setMinutes(d.getMinutes() - minutesAgo)
        return formatTime(d)
    }

    function sampleLine(chatId, index) {
        var lines = [
            "Just landed, grabbing coffee now.",
            "Let me check the timeline.",
            "Draft is ready for review.",
            "Can we move the meeting?",
            "Pushing an update in 5.",
            "Looks good to me.",
            "Any blockers on your side?",
            "Ping me when ready."
        ]
        return lines[(index + chatId.length) % lines.length]
    }

    function groupLine(index) {
        var lines = [
            "Standup moved to 10:30, ok?",
            "I pushed the UI tweak.",
            "Need feedback on the API.",
            "Ship the build tonight?",
            "Reminder: review the spec.",
            "Sync after lunch.",
            "Metrics look stable.",
            "QA signed off."
        ]
        return lines[index % lines.length]
    }

    Component.onCompleted: init()
}
