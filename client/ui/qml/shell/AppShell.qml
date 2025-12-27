import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtCore
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/dialogs" as Dialogs
import "qrc:/mi/e2ee/ui/qml/shell" as Shell

Item {
    id: root

    property int windowWidth: 0
    property bool showRightPane: Ui.AppStore.rightPaneVisible && windowWidth >= 860

    Settings {
        id: paneSettings
        property int leftWidth: Ui.Style.leftPaneWidthDefault
        property int rightWidth: Ui.Style.rightPaneWidth
        property bool rightPaneVisible: false
    }

    Component.onCompleted: {
        Ui.AppStore.rightPaneVisible = paneSettings.rightPaneVisible
    }

    Connections {
        target: Ui.AppStore
        function onRightPaneVisibilityChanged(visible) {
            paneSettings.rightPaneVisible = visible
        }
    }

    SplitView {
        id: split
        anchors.fill: parent
        orientation: Qt.Horizontal
        handle: Rectangle {
            implicitWidth: 1
            color: Ui.Style.borderSubtle
        }

        Shell.LeftPane {
            id: leftPane
            SplitView.preferredWidth: paneSettings.leftWidth
            SplitView.minimumWidth: Ui.Style.leftPaneWidthMin
            onWidthChanged: {
                if (width > 80) {
                    paneSettings.leftWidth = width
                }
            }
            onRequestNewChat: newChatDialog.open()
            onRequestAddContact: addContactDialog.open()
            onRequestCreateGroup: createGroupWizard.open()
            onRequestSettings: settingsDialog.open()
        }

        Shell.CenterPane {
            id: centerPane
            SplitView.fillWidth: true
        }

        Shell.RightPane {
            id: rightPane
            visible: root.showRightPane
            SplitView.preferredWidth: root.showRightPane ? paneSettings.rightWidth : 0
            SplitView.minimumWidth: root.showRightPane ? 280 : 0
            SplitView.maximumWidth: root.showRightPane ? 520 : 0
            onWidthChanged: {
                if (root.showRightPane && width > 120) {
                    paneSettings.rightWidth = width
                }
            }
        }
    }

    Dialogs.NewChatDialog { id: newChatDialog }
    Dialogs.AddContactDialog { id: addContactDialog }
    Dialogs.CreateGroupWizard { id: createGroupWizard }
    Dialogs.SettingsDialog { id: settingsDialog }

    function focusSearch() {
        leftPane.focusSearch()
    }

    function showChatSearch() {
        centerPane.showSearch()
    }

    function handleEscape() {
        if (settingsDialog.visible) {
            settingsDialog.close()
            return
        }
        if (createGroupWizard.visible) {
            createGroupWizard.close()
            return
        }
        if (addContactDialog.visible) {
            addContactDialog.close()
            return
        }
        if (newChatDialog.visible) {
            newChatDialog.close()
            return
        }
        if (centerPane.clearChatSearch()) {
            return
        }
        if (Ui.AppStore.searchQuery.length > 0) {
            leftPane.clearSearch()
            return
        }
        Ui.AppStore.closeRightPane()
    }
}
