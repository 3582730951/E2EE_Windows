import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/auth" as Auth
import "qrc:/mi/e2ee/ui/qml/shell" as Shell

ApplicationWindow {
    id: root
    width: 1200
    height: 760
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: "MI E2EE Client"
    color: Ui.Style.windowBg
    font.family: Ui.Style.fontFamily
    palette.window: Ui.Style.windowBg
    palette.base: Ui.Style.panelBgAlt
    palette.button: Ui.Style.panelBgAlt
    palette.text: Ui.Style.textPrimary
    palette.buttonText: Ui.Style.textPrimary
    palette.highlight: Ui.Style.accent
    palette.highlightedText: Ui.Style.textPrimary

    StackLayout {
        id: rootStack
        anchors.fill: parent
        currentIndex: Ui.AppStore.currentPage === 0 ? 0 : 1

        Auth.AuthFlow {
            id: authFlow
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Shell.AppShell {
            id: appShell
            Layout.fillWidth: true
            Layout.fillHeight: true
            windowWidth: root.width
        }
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: appShell.focusSearch()
    }
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: appShell.showChatSearch()
    }
    Shortcut {
        sequence: "Ctrl+I"
        onActivated: Ui.AppStore.toggleRightPane()
    }
    Shortcut {
        sequence: "Ctrl+W"
        onActivated: Ui.AppStore.closeRightPane()
    }
    Shortcut {
        sequence: "Esc"
        onActivated: appShell.handleEscape()
    }
}
