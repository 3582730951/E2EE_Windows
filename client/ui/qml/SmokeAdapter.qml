import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/shell" as Shell

Item {
    id: root

    property int windowWidth: 0
    readonly property bool smokeMode: typeof uiSmokeMode !== "undefined" ? !!uiSmokeMode : false
    readonly property bool shellReady: appShell.shellReady

    function focusSearch() {
        appShell.focusSearch()
    }

    function showChatSearch() {
        appShell.showChatSearch()
    }

    function handleEscape() {
        appShell.handleEscape()
    }

    function openSecurityCenter() {
        appShell.openSecurityCenter()
    }

    function openSettings() {
        if (securityCoordinator) {
            securityCoordinator.openSettings()
        }
    }

    function applySceneDialogs() {
        if (!shellReady) {
            return
        }
        var scene = Ui.SmokeSceneStore.sceneName
        if (scene === "settings_home") {
            Qt.callLater(function() {
                openSettings()
            })
            return
        }
        if (scene === "security_center") {
            Qt.callLater(function() {
                openSecurityCenter()
            })
        }
    }

    function applySmokeOverrides() {
        if (!smokeMode) {
            return
        }
        if (Ui.SmokeSceneStore.forceLightTheme) {
            Ui.Style.themeMode = "light"
        } else if (Ui.SmokeSceneStore.forceDarkTheme) {
            Ui.Style.themeMode = "dark"
        }
        if (Ui.SmokeSceneStore.postLoginScene) {
            Ui.SmokeSceneStore.activatePreview()
            applySceneDialogs()
        }
    }

    Component.onCompleted: applySmokeOverrides()
    onShellReadyChanged: {
        if (shellReady) {
            applySceneDialogs()
        }
    }

    Ui.SecurityDialogCoordinator {
        id: securityCoordinator
        ownerWindow: root.Window.window
    }

    Shell.AppShell {
        id: appShell
        anchors.fill: parent
        windowWidth: root.windowWidth
        securityCoordinator: securityCoordinator
    }
}
