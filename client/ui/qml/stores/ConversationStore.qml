pragma Singleton
import QtQuick 2.15

QtObject {
    id: conversationStore

    property string searchQuery: ""
    property int currentLeftTab: 0
    property bool rightPaneVisible: false
    property int notificationCount: 0

    function applyFromApp(query, leftTab, paneVisible, badgeCount) {
        searchQuery = query || ""
        currentLeftTab = leftTab || 0
        rightPaneVisible = paneVisible === true
        notificationCount = badgeCount || 0
    }
}
