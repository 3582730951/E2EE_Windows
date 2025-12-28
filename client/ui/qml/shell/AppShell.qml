import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/dialogs" as Dialogs
import "qrc:/mi/e2ee/ui/qml/shell" as Shell

Item {
    id: root

    property int windowWidth: 0
    property int leftWidth: Ui.Style.leftPaneWidthDefault

    SplitView {
        id: split
        anchors.fill: parent
        orientation: Qt.Horizontal
        handle: Rectangle {
            implicitWidth: 1
            color: Ui.Style.borderSubtle
            z: 5
        }

        Shell.LeftPane {
            id: leftPane
            SplitView.preferredWidth: root.leftWidth
            SplitView.minimumWidth: Ui.Style.leftPaneWidthMin
            onWidthChanged: {
                if (width > 80) {
                    root.leftWidth = width
                }
            }
            onRequestNewChat: newChatDialog.open()
            onRequestAddContact: addContactDialog.open()
            onRequestCreateGroup: createGroupWizard.open()
            onRequestSettings: settingsDialog.open()
            onRequestDeviceManager: deviceManagerDialog.open()
        }

        Shell.CenterPane {
            id: centerPane
            SplitView.fillWidth: true
        }
    }

    Dialogs.NewChatDialog { id: newChatDialog }
    Dialogs.AddContactDialog { id: addContactDialog }
    Dialogs.CreateGroupWizard { id: createGroupWizard }
    Dialogs.SettingsDialog { id: settingsDialog }
    Dialogs.DeviceManagerDialog { id: deviceManagerDialog }

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
        if (deviceManagerDialog.visible) {
            deviceManagerDialog.close()
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
    }
}
