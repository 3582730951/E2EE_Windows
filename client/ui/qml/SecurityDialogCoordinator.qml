import QtQuick 2.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/dialogs" as Dialogs

Item {
    id: root

    property var ownerWindow: null
    readonly property bool anyVisible: settingsDialog.visible ||
                                       securityCenterDialog.visible ||
                                       deviceManagerDialog.visible

    function openSettings() {
        Ui.SecurityDisplayStore.refresh()
        settingsDialog.open()
    }

    function openSecurityCenter() {
        Ui.SecurityDisplayStore.refresh()
        securityCenterDialog.open()
    }

    function openDeviceManager() {
        Ui.SecurityDisplayStore.refresh()
        deviceManagerDialog.open()
    }

    function handleEscape() {
        if (securityCenterDialog.visible) {
            securityCenterDialog.close()
            return true
        }
        if (settingsDialog.visible) {
            settingsDialog.close()
            return true
        }
        if (deviceManagerDialog.visible) {
            deviceManagerDialog.close()
            return true
        }
        return false
    }

    Dialogs.SettingsDialog {
        id: settingsDialog
        ownerWindow: root.ownerWindow
        onRequestSecurityCenter: {
            settingsDialog.close()
            root.openSecurityCenter()
        }
    }

    Dialogs.SecurityCenterDialog {
        id: securityCenterDialog
        ownerWindow: root.ownerWindow
        onRequestManageDevices: {
            securityCenterDialog.close()
            root.openDeviceManager()
        }
    }

    Dialogs.DeviceManagerDialog {
        id: deviceManagerDialog
        ownerWindow: root.ownerWindow
    }
}
