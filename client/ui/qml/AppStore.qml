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
    property int sendErrorTimeoutMs: 4500
    property bool clipboardIsolationEnabled: true
    property bool internalImeEnabled: true
    property bool historySaveEnabled: true
    property bool aiEnhanceEnabled: false
    property int aiEnhanceQualityLevel: 2
    property bool aiEnhanceX4Confirmed: false
    property bool aiEnhanceGpuAvailable: false
    property string aiEnhanceGpuName: ""
    property int aiEnhanceGpuSeries: 0
    property int aiEnhancePerfScale: 2
    property int aiEnhanceQualityScale: 2
    property string internalClipboardText: ""
    property double internalClipboardMs: 0
    property var messagesByChatId: ({})
    property var membersByChatId: ({})
    property var typingByChatId: ({})
    property var presenceByChatId: ({})
    property var downloadProgressByFileId: ({})
    property bool incomingCallActive: false
    property string incomingCallPeer: ""
    property string incomingCallId: ""
    property bool incomingCallVideo: false
    property string currentChatBackgroundUrl: ""
    property var recalledMessageIdsByChat: ({})
    property int recallWindowMs: 5 * 60 * 1000

    signal currentChatChanged(string chatId)
    signal leftTabChanged(int tab)
    signal rightPaneVisibilityChanged(bool visible)

    property ListModel dialogsModel: ListModel {}
    property ListModel contactsModel: ListModel {}
    property ListModel membersModel: ListModel {}
    property ListModel filteredDialogsModel: ListModel {}
    property ListModel filteredContactsModel: ListModel {}
    property ListModel friendRequestsModel: ListModel {}
    property ListModel groupInvitesModel: ListModel {}
    property ListModel noticesModel: ListModel {}
    property int notificationCount: friendRequestsModel.count + groupInvitesModel.count + noticesModel.count
    property var knownFriendIds: ({})
    property bool friendIdsInitialized: false

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
            if (clientBridge.historySaveEnabled) {
                historySaveEnabled = clientBridge.historySaveEnabled()
            }
            if (clientBridge.aiEnhanceRecommendations) {
                var rec = clientBridge.aiEnhanceRecommendations()
                if (rec) {
                    aiEnhanceGpuAvailable = rec.gpuAvailable === true
                    aiEnhanceGpuName = rec.gpuName || ""
                    aiEnhanceGpuSeries = rec.gpuSeries || 0
                    aiEnhancePerfScale = rec.perfScale || 2
                    aiEnhanceQualityScale = rec.qualityScale || 2
                }
            } else if (clientBridge.aiEnhanceGpuAvailable) {
                aiEnhanceGpuAvailable = clientBridge.aiEnhanceGpuAvailable()
            }
            if (clientBridge.aiEnhanceEnabled) {
                aiEnhanceEnabled = clientBridge.aiEnhanceEnabled()
            }
            if (clientBridge.aiEnhanceQualityLevel) {
                aiEnhanceQualityLevel = clientBridge.aiEnhanceQualityLevel()
            }
            if (clientBridge.aiEnhanceX4Confirmed) {
                aiEnhanceX4Confirmed = clientBridge.aiEnhanceX4Confirmed()
            }
        }
        rebuildFiltered()
    }

    function isEmojiBase(code) {
        return (code >= 0x1F300 && code <= 0x1FAFF) ||
               (code >= 0x2600 && code <= 0x27BF)
    }
    function isEmojiComponent(code) {
        if (code === 0x200D || code === 0xFE0F || code === 0x20E3) {
            return true
        }
        return code >= 0x1F3FB && code <= 0x1F3FF
    }
    function isSingleEmoji(text) {
        var value = (text || "").trim()
        if (value.length === 0) {
            return false
        }
        var count = 0
        for (var i = 0; i < value.length; ) {
            var code = value.codePointAt(i)
            var step = code > 0xFFFF ? 2 : 1
            if (code <= 0xFFFF) {
                var ch = value.charAt(i)
                if (/\\s/.test(ch)) {
                    i += step
                    continue
                }
            }
            if (isEmojiBase(code)) {
                count += 1
                if (count > 1) {
                    return false
                }
            } else if (!isEmojiComponent(code)) {
                return false
            }
            i += step
        }
        return count === 1
    }

    function detectFileKind(fileName) {
        var name = (fileName || "").toLowerCase()
        var dot = name.lastIndexOf(".")
        var ext = dot >= 0 ? name.slice(dot + 1) : ""
        if (ext === "gif") {
            return "gif"
        }
        if (ext === "png" || ext === "jpg" || ext === "jpeg" ||
            ext === "webp" || ext === "bmp") {
            return "image"
        }
        if (ext === "mp4" || ext === "mov" || ext === "mkv" ||
            ext === "webm" || ext === "avi") {
            return "video"
        }
        return "file"
    }

    function parseLocationText(text) {
        var value = (text || "").trim()
        var pattern = /^(?:【位置】|\\[位置\\])(.*)\\s+lat:([-\\d\\.]+),\\s*lon:([-\\d\\.]+)$/
        var match = pattern.exec(value)
        if (!match || match.length < 4) {
            return null
        }
        var label = (match[1] || "").trim()
        var lat = parseFloat(match[2])
        var lon = parseFloat(match[3])
        if (isNaN(lat) || isNaN(lon)) {
            return null
        }
        return { label: label, lat: lat, lon: lon }
    }

    function parseCallInviteText(text) {
        var value = (text || "").trim()
        var voicePrefix = "[call]voice:"
        var videoPrefix = "[call]video:"
        var callId = ""
        var video = false
        if (value.indexOf(voicePrefix) === 0) {
            callId = value.slice(voicePrefix.length).trim()
        } else if (value.indexOf(videoPrefix) === 0) {
            callId = value.slice(videoPrefix.length).trim()
            video = true
        } else {
            return null
        }
        if (!/^[0-9a-fA-F]{32}$/.test(callId)) {
            return null
        }
        return { callId: callId, video: video }
    }

    function parseCallEndId(text) {
        var value = (text || "").trim()
        var endPrefix = "[call]end:"
        if (value.indexOf(endPrefix) !== 0) {
            return ""
        }
        return value.slice(endPrefix.length).trim()
    }

    function parseRecallTarget(text) {
        var value = (text || "").trim()
        var prefix = "[recall]:"
        if (value.indexOf(prefix) !== 0) {
            return ""
        }
        return value.slice(prefix.length).trim()
    }

    function clearIncomingCall() {
        incomingCallActive = false
        incomingCallPeer = ""
        incomingCallId = ""
        incomingCallVideo = false
    }

    function refreshChatBackground(chatId) {
        var target = (chatId && chatId.length > 0) ? chatId : currentChatId
        if (!target || !clientBridge || !clientBridge.chatBackground) {
            if (target === currentChatId) {
                currentChatBackgroundUrl = ""
            }
            return
        }
        var url = clientBridge.chatBackground(target)
        if (target === currentChatId) {
            currentChatBackgroundUrl = url && url.toString ? url.toString()
                                                          : (url ? "" + url : "")
        }
    }

    function ensureRecallMap(chatId) {
        if (!recalledMessageIdsByChat[chatId]) {
            recalledMessageIdsByChat[chatId] = {}
        }
        return recalledMessageIdsByChat[chatId]
    }

    function markMessageRecalled(chatId, messageId) {
        if (!chatId || !messageId) {
            return
        }
        var map = ensureRecallMap(chatId)
        map[messageId] = true
    }

    function isMessageRecalled(chatId, messageId) {
        if (!chatId || !messageId) {
            return false
        }
        var map = recalledMessageIdsByChat[chatId]
        return map && map[messageId] === true
    }

    function refreshDialogPreviewFromModel(chatId) {
        var idx = findDialogIndex(chatId)
        if (idx < 0) {
            return
        }
        var model = messagesModel(chatId)
        if (!model || model.count === 0) {
            updateDialogPreview(chatId, "", "", "")
            return
        }
        var lastIndex = model.count - 1
        var entry = null
        while (lastIndex >= 0) {
            var candidate = model.get(lastIndex)
            if (candidate && candidate.kind !== "system") {
                entry = candidate
                break
            }
            lastIndex -= 1
        }
        if (!entry) {
            updateDialogPreview(chatId, "", "", "")
            return
        }
        updateDialogPreview(chatId,
                            entry.text || "",
                            entry.timeText || "",
                            entry.senderName || "")
    }

    function removeMessageFromChat(chatId, messageId) {
        var idx = findMessageIndex(chatId, messageId)
        if (idx < 0) {
            return false
        }
        var model = messagesModel(chatId)
        var removed = model.get(idx)
        model.remove(idx)
        if (removed && removed.kind === "in" && chatId !== currentChatId) {
            var dialogIdx = findDialogIndex(chatId)
            if (dialogIdx >= 0) {
                var unread = dialogsModel.get(dialogIdx).unread || 0
                if (unread > 0) {
                    dialogsModel.setProperty(dialogIdx, "unread", unread - 1)
                }
            }
        }
        refreshDialogPreviewFromModel(chatId)
        return true
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
        var nextFriendIds = {}
        var friendNameMap = {}

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
            nextFriendIds[username] = true
            friendNameMap[username] = display
        }
        if (clientBridge && clientBridge.loggedIn) {
            if (friendIdsInitialized) {
                for (var addId in nextFriendIds) {
                    if (!knownFriendIds[addId]) {
                        var displayName = friendNameMap[addId] || addId
                        addNotice("friend_added|" + addId,
                                  Ui.I18n.t("notice.friendAddedTitle"),
                                  Ui.I18n.format("notice.friendAddedDetail", displayName))
                    }
                }
                for (var rmId in knownFriendIds) {
                    if (!nextFriendIds[rmId]) {
                        addNotice("friend_removed|" + rmId,
                                  Ui.I18n.t("notice.friendRemovedTitle"),
                                  Ui.I18n.format("notice.friendRemovedDetail", rmId))
                    }
                }
            }
            knownFriendIds = nextFriendIds
            friendIdsInitialized = true
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
        var nowMs = Date.now()
        for (var i = 0; i < requests.length; ++i) {
            var r = requests[i]
            var name = r.username || ""
            friendRequestsModel.append({
                username: name,
                remark: r.remark || "",
                receivedMs: nowMs
            })
        }
    }

    function buildInviteKey(groupId, fromUser, messageId) {
        var gid = (groupId || "").trim()
        var from = (fromUser || "").trim()
        var mid = (messageId || "").trim()
        return gid + "|" + (mid.length > 0 ? mid : from)
    }

    function addGroupInvite(groupId, fromUser, messageId) {
        var gid = (groupId || "").trim()
        if (gid.length === 0) {
            return
        }
        var key = buildInviteKey(gid, fromUser, messageId)
        for (var i = 0; i < groupInvitesModel.count; ++i) {
            if (groupInvitesModel.get(i).key === key) {
                return
            }
        }
        groupInvitesModel.append({
            key: key,
            groupId: gid,
            fromUser: fromUser || "",
            messageId: messageId || "",
            receivedMs: Date.now()
        })
    }

    function removeGroupInviteByKey(key) {
        for (var i = groupInvitesModel.count - 1; i >= 0; --i) {
            if (groupInvitesModel.get(i).key === key) {
                groupInvitesModel.remove(i)
            }
        }
    }

    function joinGroupInvite(key) {
        if (!clientBridge) {
            return false
        }
        for (var i = 0; i < groupInvitesModel.count; ++i) {
            var inv = groupInvitesModel.get(i)
            if (inv.key === key) {
                var ok = clientBridge.joinGroup(inv.groupId)
                if (ok) {
                    removeGroupInviteByKey(key)
                }
                return ok
            }
        }
        return false
    }

    function ignoreGroupInvite(key) {
        removeGroupInviteByKey(key)
    }

    function copyGroupInviteId(groupId) {
        var gid = (groupId || "").trim()
        if (gid.length === 0) {
            return
        }
        setInternalClipboard(gid)
    }

    function addNotice(key, title, detail) {
        var k = (key || "").trim()
        if (k.length === 0) {
            return
        }
        for (var i = 0; i < noticesModel.count; ++i) {
            if (noticesModel.get(i).key === k) {
                return
            }
        }
        noticesModel.append({
            key: k,
            title: title || "",
            detail: detail || "",
            receivedMs: Date.now()
        })
    }

    function dismissNotice(key) {
        for (var i = noticesModel.count - 1; i >= 0; --i) {
            if (noticesModel.get(i).key === key) {
                noticesModel.remove(i)
            }
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
        var isGroup = message.isGroup === true
        var outgoing = message.outgoing === true

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
        if (kind === "call_end") {
            if (incomingCallActive && incomingCallId === message.callId) {
                clearIncomingCall()
            }
            return
        }
        if (kind === "recall") {
            var targetId = message.targetMessageId || ""
            if (targetId.length > 0) {
                applyRecallToChat(convId, targetId, outgoing)
            }
            return
        }

        var sender = message.sender || ""
        var msgId = message.messageId || nextMsgId()
        var timeText = message.time || formatTime(new Date())
        if (isMessageRecalled(convId, msgId)) {
            return
        }
        if (kind === "group_invite" && !outgoing) {
            addGroupInvite(convId, sender, msgId)
        }
        if (kind === "notice") {
            var noticeKind = message.noticeKind || 0
            var noticeTarget = message.noticeTarget || ""
            if (noticeKind === 3 && clientBridge && noticeTarget === clientBridge.username) {
                var actor = message.noticeActor || ""
                var detail = actor.length > 0
                             ? Ui.I18n.format("notice.groupKickedDetail", actor, convId)
                             : Ui.I18n.format("notice.groupKickedDetailNoActor", convId)
                addNotice("group_kick|" + convId + "|" + actor,
                          Ui.I18n.t("notice.groupKickedTitle"),
                          detail)
            }
        }

        var text = message.text || ""
        if (kind === "file") {
            var name = message.fileName || ""
            text = name.length > 0 ? "[文件] " + name : "[文件]"
        } else if (kind === "sticker") {
            var sticker = message.stickerId || ""
            text = sticker.length > 0 ? "[贴纸] " + sticker : "[贴纸]"
        } else if (kind === "location") {
            var label = message.locationLabel || ""
            text = label.length > 0 ? "[位置] " + label : "[位置]"
        } else if (kind === "call_invite") {
            text = message.video ? Ui.I18n.t("chat.callIncomingVideo")
                                 : Ui.I18n.t("chat.callIncomingVoice")
        } else if (kind === "group_invite") {
            text = "邀请加入群聊"
        }

        var contentKind = "text"
        var locationLabel = ""
        var locationLat = 0
        var locationLon = 0
        var callId = message.callId || ""
        var callVideo = message.video === true
        var parsedCallInvite = null
        if (kind === "location") {
            contentKind = "location"
            locationLabel = message.locationLabel || ""
            locationLat = message.locationLat || 0
            locationLon = message.locationLon || 0
        } else if (kind === "sticker") {
            contentKind = "sticker"
        } else if (kind === "file") {
            contentKind = detectFileKind(message.fileName || "")
            if (!message.fileUrl && !message.filePath) {
                contentKind = "file"
            }
        } else if (kind === "call_invite") {
            contentKind = "call"
        } else if (kind === "text") {
            var invite = parseCallInviteText(text)
            if (invite) {
                contentKind = "call"
                callId = invite.callId
                callVideo = invite.video
                parsedCallInvite = invite
                if (!outgoing) {
                    text = invite.video ? Ui.I18n.t("chat.callIncomingVideo")
                                        : Ui.I18n.t("chat.callIncomingVoice")
                }
            } else {
                var loc = parseLocationText(text)
                if (loc) {
                    contentKind = "location"
                    locationLabel = loc.label
                    locationLat = loc.lat
                    locationLon = loc.lon
                } else if (isSingleEmoji(text)) {
                    contentKind = "emoji"
                }
            }
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
        var fileId = message.fileId || ""
        var progressValue = downloadProgressByFileId[fileId]
        if (progressValue === undefined || progressValue === null) {
            progressValue = message.downloadProgress || 0
        }
        var fileUrlValue = message.fileUrl || message.filePath || ""
        var fileUrlText = fileUrlValue && fileUrlValue.toString
                          ? fileUrlValue.toString()
                          : (fileUrlValue ? "" + fileUrlValue : "")
        if (progressValue <= 0 &&
            (fileUrlText.length > 0 || (outgoing && kind === "file"))) {
            progressValue = 1
        }
        var timestampSec = message.timestampSec || 0
        var timestampMs = timestampSec > 0 ? timestampSec * 1000 : Date.now()

        var entry = {
            chatId: convId,
            msgId: msgId,
            kind: entryKind,
            contentKind: contentKind,
            senderName: displaySender,
            text: text,
            timeText: timeText,
            timestampMs: timestampMs,
            statusTicks: outgoing ? "sent" : "none",
            edited: false,
            fileName: message.fileName || "",
            fileSize: message.fileSize || 0,
            fileId: fileId,
            fileKey: message.fileKey || "",
            fileUrl: fileUrlValue,
            downloadProgress: progressValue,
            imageEnhanced: message.imageEnhanced === true,
            stickerId: message.stickerId || "",
            stickerUrl: message.stickerUrl || "",
            stickerAnimated: message.stickerAnimated || false,
            previewUrl: message.previewUrl || "",
            locationLabel: locationLabel,
            locationLat: locationLat,
            locationLon: locationLon,
            callId: callId,
            callVideo: callVideo,
            animateEmoji: contentKind === "emoji"
        }

        if (msgId && mergeMessageEntry(convId, msgId, entry)) {
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
        if ((kind === "call_invite" || parsedCallInvite) && !outgoing && !isGroup) {
            incomingCallActive = true
            incomingCallPeer = convId
            incomingCallId = callId
            incomingCallVideo = callVideo
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
        clearSendError()
        currentChatId = chatId
        updateCurrentChatDetails()
        loadHistoryForChat(chatId)
        refreshChatBackground(chatId)
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
        var recallIds = {}
        for (var r = 0; r < history.length; ++r) {
            var rh = history[r]
            if (rh.kind !== "text") {
                continue
            }
            var rtext = rh.text || ""
            var rid = parseRecallTarget(rtext)
            if (rid.length > 0) {
                recallIds[rid] = true
            }
        }
        recalledMessageIdsByChat[chatId] = recallIds
        var lastPreviewText = ""
        var lastPreviewTime = ""
        var lastPreviewSender = ""
        for (var i = 0; i < history.length; ++i) {
            var h = history[i]
            var entryKind = "in"
            if (h.kind === "system") {
                entryKind = "system"
            } else if (h.outgoing === true) {
                entryKind = "out"
            }

            var rawText = h.text || ""
            if (h.kind === "text") {
                var recallTarget = parseRecallTarget(rawText)
                if (recallTarget.length > 0) {
                    continue
                }
                if (parseCallEndId(rawText).length > 0) {
                    continue
                }
            }
            var historyId = h.messageId || ""
            if (historyId.length > 0 && recallIds[historyId]) {
                continue
            }

            var text = rawText
            if (h.kind === "file") {
                text = h.fileName ? "[文件] " + h.fileName : "[文件]"
            } else if (h.kind === "sticker") {
                text = h.stickerId ? "[贴纸] " + h.stickerId : "[贴纸]"
            }

            var contentKind = "text"
            var locationLabel = ""
            var locationLat = 0
            var locationLon = 0
            if (h.kind === "sticker") {
                contentKind = "sticker"
            } else if (h.kind === "file") {
                contentKind = detectFileKind(h.fileName || "")
                if (!h.fileUrl) {
                    contentKind = "file"
                }
            } else if (h.kind === "text") {
                var loc = parseLocationText(text)
                if (loc) {
                    contentKind = "location"
                    locationLabel = loc.label
                    locationLat = loc.lat
                    locationLon = loc.lon
                } else if (isSingleEmoji(text)) {
                    contentKind = "emoji"
                }
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

            var fileId = h.fileId || ""
            var progressValue = downloadProgressByFileId[fileId]
            if (progressValue === undefined || progressValue === null) {
                progressValue = h.downloadProgress || 0
            }
            var fileUrlValue = h.fileUrl || ""
            var fileUrlText = fileUrlValue && fileUrlValue.toString
                              ? fileUrlValue.toString()
                              : (fileUrlValue ? "" + fileUrlValue : "")
            if (progressValue <= 0 &&
                (fileUrlText.length > 0 || (h.outgoing === true && h.kind === "file"))) {
                progressValue = 1
            }
            var timestampSec = h.timestampSec || 0
            var timestampMs = timestampSec > 0 ? timestampSec * 1000 : Date.now()
            var displaySender = h.outgoing ? Ui.I18n.t("chat.you") : (h.sender || "")

            model.append({
                chatId: chatId,
                msgId: historyId || nextMsgId(),
                kind: entryKind,
                contentKind: contentKind,
                senderName: displaySender,
                text: text,
                timeText: h.time || "",
                timestampMs: timestampMs,
                statusTicks: ticks,
                edited: false,
                fileName: h.fileName || "",
                fileSize: h.fileSize || 0,
                fileId: fileId,
                fileKey: h.fileKey || "",
                fileUrl: fileUrlValue,
                downloadProgress: progressValue,
                imageEnhanced: h.imageEnhanced === true,
                stickerId: h.stickerId || "",
                stickerUrl: h.stickerUrl || "",
                stickerAnimated: h.stickerAnimated || false,
                previewUrl: h.previewUrl || "",
                locationLabel: locationLabel,
                locationLat: locationLat,
                locationLon: locationLon,
                animateEmoji: false
            })
            if (entryKind !== "system") {
                lastPreviewText = text
                lastPreviewTime = h.time || ""
                lastPreviewSender = displaySender
            }
        }
        if (lastPreviewText.length > 0 || lastPreviewTime.length > 0) {
            updateDialogPreview(chatId, lastPreviewText, lastPreviewTime, lastPreviewSender)
        } else {
            updateDialogPreview(chatId, "", "", "")
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

    function sendSticker(stickerId) {
        sendErrorMessage = ""
        if (!currentChatId || !clientBridge) {
            sendErrorMessage = Ui.I18n.t("chat.sendFailed")
            return false
        }
        var sid = (stickerId || "").trim()
        if (sid.length === 0) {
            return false
        }
        var ok = clientBridge.sendSticker(currentChatId, sid, currentChatType === "group")
        if (!ok) {
            var err = clientBridge.lastError || ""
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
        if (path && path.toString) {
            path = path.toString()
        }
        if (!path || path.length === 0) {
            sendErrorMessage = Ui.I18n.t("chat.sendFailed")
            return false
        }
        var ok = clientBridge.sendFile(currentChatId, path, currentChatType === "group")
        if (!ok) {
            var err = clientBridge.lastError || ""
            sendErrorMessage = err.length > 0 ? err : Ui.I18n.t("chat.sendFailed")
        }
        return ok
    }

    function sendLocation(lat, lon, label) {
        sendErrorMessage = ""
        if (!currentChatId || !clientBridge) {
            sendErrorMessage = Ui.I18n.t("chat.sendFailed")
            return false
        }
        var latNum = parseFloat(lat)
        var lonNum = parseFloat(lon)
        if (isNaN(latNum) || isNaN(lonNum)) {
            sendErrorMessage = Ui.I18n.t("chat.sendFailed")
            return false
        }
        var ok = clientBridge.sendLocation(currentChatId, latNum, lonNum, label || "",
                                           currentChatType === "group")
        if (!ok) {
            var err = clientBridge.lastError || ""
            sendErrorMessage = err.length > 0 ? err : Ui.I18n.t("chat.sendFailed")
        }
        return ok
    }

    function setChatBackgroundForCurrentChat(url) {
        if (!currentChatId || !clientBridge || !clientBridge.setChatBackground) {
            return false
        }
        var ok = clientBridge.setChatBackground(currentChatId, url)
        if (!ok) {
            var err = clientBridge.lastError || ""
            sendErrorMessage = err.length > 0 ? err : Ui.I18n.t("chat.sendFailed")
            return false
        }
        refreshChatBackground(currentChatId)
        return true
    }

    function startCall(video) {
        if (!currentChatId || !clientBridge) {
            return false
        }
        if (currentChatType === "group") {
            return startGroupCall(video)
        }
        var callId = video ? clientBridge.startVideoCall(currentChatId)
                           : clientBridge.startVoiceCall(currentChatId)
        return callId && callId.length > 0
    }

    function groupCallInfo(chatId) {
        if (!chatId || !clientBridge || !clientBridge.groupCallRooms) {
            return null
        }
        var rooms = clientBridge.groupCallRooms || []
        for (var i = 0; i < rooms.length; ++i) {
            if (rooms[i].groupId === chatId) {
                return rooms[i]
            }
        }
        return null
    }

    function startGroupCall(video) {
        if (!currentChatId || !clientBridge || !clientBridge.startGroupCall) {
            return false
        }
        var callId = clientBridge.startGroupCall(currentChatId, video === true)
        if (!callId || callId.length === 0) {
            var err = clientBridge.lastError || ""
            sendErrorMessage = err.length > 0 ? err : Ui.I18n.t("chat.sendFailed")
            return false
        }
        return true
    }

    function joinGroupCall(video) {
        var info = groupCallInfo(currentChatId)
        if (!info || !info.callId || !clientBridge || !clientBridge.joinGroupCall) {
            return false
        }
        var ok = clientBridge.joinGroupCall(currentChatId, info.callId, video === true)
        if (!ok) {
            var err = clientBridge.lastError || ""
            sendErrorMessage = err.length > 0 ? err : Ui.I18n.t("chat.sendFailed")
        }
        return ok
    }

    function leaveGroupCall() {
        if (clientBridge && clientBridge.leaveGroupCall) {
            clientBridge.leaveGroupCall()
        }
    }

    function endGroupCall() {
        if (clientBridge && clientBridge.endGroupCall) {
            clientBridge.endGroupCall()
        }
    }

    function handleCallAction(video) {
        if (currentChatType === "group") {
            var info = groupCallInfo(currentChatId)
            if (info && info.callId) {
                return joinGroupCall(video)
            }
            return startGroupCall(video)
        }
        return startCall(video)
    }

    function acceptIncomingCall() {
        if (!incomingCallActive || !clientBridge) {
            return false
        }
        var ok = clientBridge.joinCall(incomingCallPeer, incomingCallId, incomingCallVideo)
        if (ok) {
            clearIncomingCall()
        }
        return ok
    }

    function declineIncomingCall() {
        if (incomingCallActive && clientBridge && clientBridge.sendCallEnd) {
            clientBridge.sendCallEnd(incomingCallPeer, incomingCallId)
        }
        clearIncomingCall()
    }

    function applyRecallToChat(chatId, messageId, outgoing) {
        markMessageRecalled(chatId, messageId)
        return removeMessageFromChat(chatId, messageId)
    }

    function requestRecallMessage(chatId, messageId, timestampMs, isGroup) {
        if (!chatId || !messageId || !clientBridge || !clientBridge.recallMessage) {
            return false
        }
        var nowMs = Date.now()
        if (timestampMs && (nowMs - timestampMs) > recallWindowMs) {
            sendErrorMessage = Ui.I18n.t("chat.recallExpired")
            return false
        }
        var ok = clientBridge.recallMessage(chatId, messageId, isGroup === true)
        if (ok) {
            applyRecallToChat(chatId, messageId, true)
        } else {
            var err = clientBridge.lastError || ""
            sendErrorMessage = err.length > 0 ? err : Ui.I18n.t("chat.sendFailed")
        }
        return ok
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

    function setHistorySaveEnabled(enabled) {
        historySaveEnabled = enabled === true
        if (clientBridge && clientBridge.setHistorySaveEnabled) {
            clientBridge.setHistorySaveEnabled(historySaveEnabled)
        }
        if (!historySaveEnabled) {
            clearAllMessages()
        }
    }

    function setAiEnhanceEnabled(enabled) {
        aiEnhanceEnabled = enabled === true
        if (clientBridge && clientBridge.setAiEnhanceEnabled) {
            clientBridge.setAiEnhanceEnabled(aiEnhanceEnabled)
        }
    }

    function setAiEnhanceQualityLevel(level) {
        aiEnhanceQualityLevel = level || 2
        if (clientBridge && clientBridge.setAiEnhanceQualityLevel) {
            clientBridge.setAiEnhanceQualityLevel(aiEnhanceQualityLevel)
        }
    }

    function setAiEnhanceX4Confirmed(confirmed) {
        aiEnhanceX4Confirmed = confirmed === true
        if (clientBridge && clientBridge.setAiEnhanceX4Confirmed) {
            clientBridge.setAiEnhanceX4Confirmed(aiEnhanceX4Confirmed)
        }
    }

    function setInternalClipboard(text) {
        internalClipboardText = text || ""
        internalClipboardMs = Date.now()
    }

    function clearAllMessages() {
        for (var chatId in messagesByChatId) {
            if (!messagesByChatId.hasOwnProperty(chatId)) {
                continue
            }
            var model = messagesByChatId[chatId]
            if (model && model.clear) {
                model.clear()
            }
        }
        for (var i = 0; i < dialogsModel.count; ++i) {
            dialogsModel.setProperty(i, "preview", "")
            dialogsModel.setProperty(i, "timeText", "")
            dialogsModel.setProperty(i, "subtitle", "")
        }
        recalledMessageIdsByChat = ({})
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
                refreshChatBackground("")
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

    function findMessageIndex(chatId, messageId) {
        if (!messageId) {
            return -1
        }
        var model = messagesModel(chatId)
        for (var i = model.count - 1; i >= 0; --i) {
            if (model.get(i).msgId === messageId) {
                return i
            }
        }
        return -1
    }

    function mergeMessageEntry(chatId, messageId, incoming) {
        var idx = findMessageIndex(chatId, messageId)
        if (idx < 0) {
            return false
        }
        var model = messagesModel(chatId)
        var updated = false
        function setString(prop, value) {
            if (!value || typeof value !== "string" || value.length === 0) {
                return
            }
            if (model.get(idx)[prop] !== value) {
                model.setProperty(idx, prop, value)
                updated = true
            }
        }
        function setContentKind(value) {
            if (!value || typeof value !== "string" || value.length === 0) {
                return
            }
            var currentKind = model.get(idx).contentKind || ""
            if (value === "file" && currentKind !== "" && currentKind !== "file") {
                return
            }
            if (currentKind !== value) {
                model.setProperty(idx, "contentKind", value)
                updated = true
            }
        }
        function setNumber(prop, value) {
            if (typeof value !== "number" || isNaN(value)) {
                return
            }
            if (model.get(idx)[prop] !== value) {
                model.setProperty(idx, prop, value)
                updated = true
            }
        }
        function setBoolTrue(prop, value) {
            if (value !== true) {
                return
            }
            if (model.get(idx)[prop] !== true) {
                model.setProperty(idx, prop, true)
                updated = true
            }
        }
        function setUrl(prop, value) {
            if (value === undefined || value === null) {
                return
            }
            var text = value.toString ? value.toString() : "" + value
            if (text.length === 0) {
                return
            }
            var current = model.get(idx)[prop]
            var currentText = current && current.toString ? current.toString()
                                                         : (current !== undefined ? "" + current : "")
            if (currentText !== text) {
                model.setProperty(idx, prop, value)
                updated = true
            }
        }

        if (incoming) {
            setContentKind(incoming.contentKind)
            setString("text", incoming.text)
            setString("senderName", incoming.senderName)
            setString("fileName", incoming.fileName)
            setNumber("fileSize", incoming.fileSize)
            setString("fileId", incoming.fileId)
            setString("fileKey", incoming.fileKey)
            setUrl("fileUrl", incoming.fileUrl)
            setUrl("previewUrl", incoming.previewUrl)
            setBoolTrue("imageEnhanced", incoming.imageEnhanced)
            setString("stickerId", incoming.stickerId)
            setUrl("stickerUrl", incoming.stickerUrl)
            setBoolTrue("stickerAnimated", incoming.stickerAnimated)
            setString("locationLabel", incoming.locationLabel)
            setNumber("locationLat", incoming.locationLat)
            setNumber("locationLon", incoming.locationLon)
            setString("callId", incoming.callId)
            setBoolTrue("callVideo", incoming.callVideo)
        }
        return true
    }

    function updateMessageStatus(chatId, messageId, status) {
        var idx = findMessageIndex(chatId, messageId)
        if (idx < 0) {
            return
        }
        var model = messagesModel(chatId)
        if (status === "read") {
            model.setProperty(idx, "statusTicks", "read")
        } else if (status === "delivery") {
            model.setProperty(idx, "statusTicks", "delivered")
        }
    }

    function applyAttachmentCache(fileId, fileUrl, previewUrl) {
        if (!fileId) {
            return
        }
        for (var chatId in messagesByChatId) {
            if (!messagesByChatId.hasOwnProperty(chatId)) {
                continue
            }
            var model = messagesByChatId[chatId]
            if (!model) {
                continue
            }
            for (var i = model.count - 1; i >= 0; --i) {
                var entry = model.get(i)
                if (entry.fileId !== fileId) {
                    continue
                }
                if (fileUrl) {
                    model.setProperty(i, "fileUrl", fileUrl)
                    var detected = detectFileKind(entry.fileName || "")
                    if (detected && detected !== "file") {
                        model.setProperty(i, "contentKind", detected)
                    }
                }
                if (previewUrl) {
                    model.setProperty(i, "previewUrl", previewUrl)
                }
            }
        }
        updateDownloadProgress(fileId, 1)
    }

    function applyImageEnhance(messageId, outputUrl) {
        if (!messageId) {
            return
        }
        var urlText = outputUrl && outputUrl.toString
                      ? outputUrl.toString()
                      : (outputUrl ? "" + outputUrl : "")
        if (urlText.length === 0) {
            return
        }
        for (var chatId in messagesByChatId) {
            if (!messagesByChatId.hasOwnProperty(chatId)) {
                continue
            }
            var model = messagesByChatId[chatId]
            if (!model) {
                continue
            }
            for (var i = model.count - 1; i >= 0; --i) {
                if (model.get(i).msgId === messageId) {
                    model.setProperty(i, "fileUrl", outputUrl)
                    model.setProperty(i, "contentKind", "image")
                    model.setProperty(i, "imageEnhanced", true)
                }
            }
        }
    }

    function updateDownloadProgress(fileId, progress) {
        if (!fileId) {
            return
        }
        var value = progress
        if (typeof value !== "number" || isNaN(value)) {
            value = 0
        }
        value = Math.max(0, Math.min(1, value))
        downloadProgressByFileId[fileId] = value
        for (var chatId in messagesByChatId) {
            if (!messagesByChatId.hasOwnProperty(chatId)) {
                continue
            }
            var model = messagesByChatId[chatId]
            if (!model) {
                continue
            }
            for (var i = model.count - 1; i >= 0; --i) {
                if (model.get(i).fileId === fileId) {
                    model.setProperty(i, "downloadProgress", value)
                }
            }
        }
    }

    function hasMessageId(chatId, messageId) {
        return findMessageIndex(chatId, messageId) >= 0
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
        function onConnectionChanged() {
            if (clientBridge && clientBridge.remoteOk) {
                clearSendError()
            }
        }
        function onCallStateChanged() {
            if (clientBridge && clientBridge.activeCallId.length === 0) {
                clearIncomingCall()
            }
        }
        function onAttachmentCacheReady(fileId, fileUrl, previewUrl, error) {
            if (error && error.length > 0) {
                return
            }
            applyAttachmentCache(fileId, fileUrl, previewUrl)
        }
        function onAttachmentDownloadProgress(fileId, savePath, progress) {
            updateDownloadProgress(fileId, progress)
        }
        function onAttachmentDownloadFinished(fileId, savePath, ok, error) {
            if (ok) {
                updateDownloadProgress(fileId, 1)
            } else {
                updateDownloadProgress(fileId, 0)
            }
        }
        function onImageEnhanceFinished(messageId, sourceUrl, outputUrl, ok, error) {
            if (ok && outputUrl) {
                applyImageEnhance(messageId, outputUrl)
            }
        }
    }

    onSendErrorMessageChanged: {
        if (sendErrorMessage.length > 0) {
            sendErrorTimer.restart()
        } else {
            sendErrorTimer.stop()
        }
    }

    Timer {
        id: sendErrorTimer
        interval: sendErrorTimeoutMs
        repeat: false
        onTriggered: sendErrorMessage = ""
    }

    Component.onCompleted: init()
}
