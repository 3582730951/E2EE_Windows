pragma Singleton
import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

QtObject {
    id: store

    readonly property string currentChatId: Ui.ChatStore.currentChatId
    readonly property string currentChatTitle: Ui.ChatStore.currentChatTitle
    readonly property string currentChatSubtitle: Ui.ChatStore.currentChatSubtitle
    readonly property string currentChatType: Ui.ChatStore.currentChatType
    readonly property int currentChatMembers: Ui.ChatStore.currentChatMembers
    readonly property string currentChatBackgroundUrl: Ui.ChatStore.currentChatBackgroundUrl
    readonly property string sendErrorMessage: Ui.ChatStore.sendErrorMessage
    readonly property string searchQuery: Ui.ConversationStore.searchQuery
    readonly property bool rightPaneVisible: Ui.ConversationStore.rightPaneVisible
    readonly property int notificationCount: Ui.ConversationStore.notificationCount
    readonly property bool aiEnhanceEnabled: Ui.PreferenceStore.aiEnhanceEnabled
    readonly property string internalClipboardText: Ui.AppStore.internalClipboardText
    readonly property double internalClipboardMs: Ui.AppStore.internalClipboardMs
    readonly property var filteredDialogsModel: Ui.AppStore.filteredDialogsModel
    readonly property var contactsModel: Ui.AppStore.contactsModel
    readonly property var membersModel: Ui.AppStore.membersModel
    readonly property var sharedMediaModel: Ui.AppStore.sharedMediaModel
    readonly property var sharedFilesModel: Ui.AppStore.sharedFilesModel
    readonly property var sharedLinksModel: Ui.AppStore.sharedLinksModel
    readonly property int recallWindowMs: Ui.AppStore.recallWindowMs

    function messagesModel(chatId) {
        return Ui.AppStore.messagesModel(chatId)
    }

    function messageText(entry) {
        return Ui.AppStore.messageText(entry)
    }

    function setSearchQuery(value) {
        Ui.AppStore.setSearchQuery(value)
    }

    function setCurrentChat(chatId) {
        Ui.AppStore.setCurrentChat(chatId)
    }

    function markDialogRead(chatId) {
        Ui.AppStore.markDialogRead(chatId)
    }

    function togglePin(chatId) {
        Ui.AppStore.togglePin(chatId)
    }

    function removeChat(chatId) {
        Ui.AppStore.removeChat(chatId)
    }

    function toggleRightPane() {
        Ui.AppStore.toggleRightPane()
    }

    function closeRightPane() {
        Ui.AppStore.closeRightPane()
    }

    function groupCallInfo(chatId) {
        return Ui.AppStore.groupCallInfo(chatId)
    }

    function sendMessage(text) {
        return Ui.AppStore.sendMessage(text)
    }

    function sendSticker(stickerId) {
        return Ui.AppStore.sendSticker(stickerId)
    }

    function sendFile(path) {
        return Ui.AppStore.sendFile(path)
    }

    function sendLocation(lat, lon, label) {
        return Ui.AppStore.sendLocation(lat, lon, label)
    }

    function sendContactCard(username, displayName) {
        return Ui.AppStore.sendContactCard(username, displayName)
    }

    function setChatBackgroundForCurrentChat(url) {
        return Ui.AppStore.setChatBackgroundForCurrentChat(url)
    }

    function handleCallAction(video) {
        return Ui.AppStore.handleCallAction(video)
    }

    function joinGroupCall(video) {
        return Ui.AppStore.joinGroupCall(video)
    }

    function leaveGroupCall() {
        Ui.AppStore.leaveGroupCall()
    }

    function isChatMuted(chatId) {
        return Ui.AppStore.isChatMuted(chatId)
    }

    function isChatStealth(chatId) {
        return Ui.AppStore.isChatStealth(chatId)
    }

    function isChatBlocked(chatId) {
        return Ui.AppStore.isChatBlocked(chatId)
    }

    function toggleChatMuted(chatId) {
        Ui.AppStore.toggleChatMuted(chatId)
    }

    function toggleChatStealth(chatId) {
        Ui.AppStore.toggleChatStealth(chatId)
    }

    function toggleChatBlocked(chatId) {
        return Ui.AppStore.toggleChatBlocked(chatId)
    }

    function clearSendError() {
        Ui.AppStore.clearSendError()
    }

    function requestRecallMessage(chatId, messageId, timestampMs, isGroup) {
        return Ui.AppStore.requestRecallMessage(chatId, messageId, timestampMs, isGroup)
    }

    function openChatFromContact(contactId) {
        Ui.AppStore.openChatFromContact(contactId)
    }

    function setInternalClipboard(text) {
        Ui.AppStore.setInternalClipboard(text)
    }

    function resolveTitle(chatId) {
        return Ui.AppStore.resolveTitle(chatId)
    }

    function detectFileKind(fileName) {
        return Ui.AppStore.detectFileKind(fileName)
    }
}
