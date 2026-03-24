pragma Singleton
import QtQuick 2.15

QtObject {
    id: sessionStore

    property int currentPage: 0
    property bool initialized: false
    property string statusMessage: ""
    readonly property bool loggedIn: currentPage !== 0

    function applyFromApp(page, initValue, status) {
        currentPage = page
        initialized = initValue === true
        statusMessage = status || ""
    }
}
