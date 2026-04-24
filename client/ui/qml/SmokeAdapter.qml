import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/shell" as Shell

Item {
    id: root

    property int windowWidth: 0
    property bool previewActivationScheduled: false
    readonly property bool smokeMode: typeof uiSmokeMode !== "undefined" ? !!uiSmokeMode : false
    readonly property bool shellReady: appShell.shellReady

    function activatePreview() {
        if (!smokeMode || !Ui.SmokeSceneStore.postLoginScene) {
            return
        }
        Ui.SmokeSceneStore.activatePreview()
        if (shellReady) {
            applySceneDialogs()
        }
    }

    function schedulePreviewActivation() {
        if (!smokeMode || !Ui.SmokeSceneStore.postLoginScene || previewActivationScheduled) {
            return
        }
        previewActivationScheduled = true
        Qt.callLater(function() {
            previewActivationScheduled = false
            activatePreview()
        })
    }

    function focusSearch() {
        appShell.focusSearch()
    }

    function showChatSearch() {
        appShell.showChatSearch()
    }

    function openNewChat() {
        appShell.openNewChat()
    }

    function handleEscape() {
        appShell.handleEscape()
    }

    function openSecurityCenter() {
        appShell.openSecurityCenter()
    }

    function openSettings() {
        if (appShell.openSettingsSurface) {
            appShell.openSettingsSurface()
        }
    }

    function applySceneDialogs() {
        if (!shellReady) {
            return
        }
        var scene = Ui.SmokeSceneStore.sceneName
        if (scene === "settings_home") {
            Ui.ChatDisplayStore.closeRightPane()
            Qt.callLater(function() {
                openSettings()
            })
            return
        }
        if (scene === "security_center") {
            Ui.ChatDisplayStore.closeRightPane()
            Qt.callLater(function() {
                openSecurityCenter()
            })
            return
        }
        if (scene === "calls_home") {
            Ui.ChatDisplayStore.closeRightPane()
            Ui.AppStore.setShellSurface("calls")
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
            schedulePreviewActivation()
        }
    }

    Component.onCompleted: applySmokeOverrides()
    onShellReadyChanged: {
        if (shellReady) {
            schedulePreviewActivation()
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
