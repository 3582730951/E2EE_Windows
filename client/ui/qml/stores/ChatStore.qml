pragma Singleton
import QtQuick 2.15

QtObject {
    id: chatStore

    property string currentChatId: ""
    property string currentChatTitle: ""
    property string currentChatSubtitle: ""
    property string currentChatType: "private"
    property int currentChatMembers: 0
    property string currentChatBackgroundUrl: ""
    property string sendErrorMessage: ""

    function applyFromApp(chatId, title, subtitle, chatType, members, backgroundUrl, sendError) {
        currentChatId = chatId || ""
        currentChatTitle = title || ""
        currentChatSubtitle = subtitle || ""
        currentChatType = chatType || "private"
        currentChatMembers = members || 0
        currentChatBackgroundUrl = backgroundUrl || ""
        sendErrorMessage = sendError || ""
    }
}
