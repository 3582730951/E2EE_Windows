import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/dialogs" as Dialogs
import "qrc:/mi/e2ee/ui/qml/shell" as Shell

Item {
    id: root

    property int windowWidth: 0
    property int leftWidth: Ui.Style.leftPaneWidthDefault
    property int rightWidth: Ui.Style.rightPaneWidth
    property var securityCoordinator: null
    readonly property bool shellReady: true
    readonly property var layoutContract: Ui.Style.shellLayoutContract
    readonly property bool hasActiveChat: Ui.ChatDisplayStore.currentChatId.length > 0
    readonly property string shellSurface: Ui.AppStore.currentShellSurface || "chat"
    readonly property bool canUseThreeColumn: root.windowWidth >= layoutContract.threeColumnMinWidth
    readonly property bool canUseUtilityRail: root.windowWidth >= 820
    readonly property bool canUseDrawerTwoColumn: root.windowWidth >= layoutContract.twoColumnDrawerMinWidth
    readonly property bool canUseCompactTwoColumn: root.windowWidth >= layoutContract.compactTwoColumnMinWidth
    readonly property bool isCompactShell: canUseCompactTwoColumn && !canUseDrawerTwoColumn
    readonly property bool immersiveUtilitySurface: shellSurface === "settings" ||
                                                    shellSurface === "security" ||
                                                    shellSurface === "calls"
    readonly property bool tightChatColumns: shellSurface === "chat" &&
                                             hasActiveChat &&
                                             Ui.ChatDisplayStore.rightPaneVisible &&
                                             canUseThreeColumn &&
                                             // root.windowWidth < 1400
                                             root.windowWidth < 1300
    readonly property bool rightPaneMounted: hasActiveChat &&
                                             canUseThreeColumn &&
                                             Ui.ChatDisplayStore.rightPaneVisible
    readonly property bool rightPaneDrawerVisible: hasActiveChat &&
                                                   !canUseThreeColumn &&
                                                   Ui.ChatDisplayStore.rightPaneVisible &&
                                                   shellSurface === "chat"
    readonly property bool drawerTightChatColumns: shellSurface === "chat" &&
                                                   hasActiveChat &&
                                                   rightPaneDrawerVisible
    readonly property int rightPaneDrawerWidth: canUseDrawerTwoColumn
                                                ? Math.min(rightWidth, Ui.Style.rightPaneWidthMax)
                                                : Ui.Style.rightPaneWidthDrawerNarrow
    readonly property Window hostWindow: root.Window.window

    onWindowWidthChanged: {
        var leftMaxWidth = windowWidth < 1160
                           ? Ui.Style.leftPaneWidthDefault
                           : Math.max(Ui.Style.leftPaneWidthDefault, Math.floor(windowWidth * 0.30))
        var rightMaxWidth = windowWidth < 1160
                            ? Ui.Style.rightPaneWidth
                            : Math.min(Ui.Style.rightPaneWidthMax, Math.floor(windowWidth * 0.30))
        if (isCompactShell) {
            leftWidth = Math.max(Ui.Style.leftPaneWidthMin,
                                 Math.min(Ui.Style.leftPaneWidthCompact, leftMaxWidth))
        } else {
            leftWidth = Math.max(Ui.Style.leftPaneWidthMin,
                                 Math.min(leftWidth, leftMaxWidth))
        }
        rightWidth = Math.max(Ui.Style.rightPaneWidthMin,
                              Math.min(rightWidth, rightMaxWidth))
        if (!canUseCompactTwoColumn) {
            Ui.ChatDisplayStore.closeRightPane()
        }
    }

    onHasActiveChatChanged: {
        if (!hasActiveChat) {
            Ui.ChatDisplayStore.closeRightPane()
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Ui.Style.tgCloudTop }
            GradientStop { position: 1.0; color: Ui.Style.tgCloudBottom }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: 10
            radius: 24
            color: Ui.Style.tgGlassSurface
            border.width: 1
            border.color: Ui.Style.tgCardBorder
            clip: true

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                width: parent.width * 0.44
                height: 120
                color: Ui.Style.tgCardHighlight
                radius: 60
                x: -24
                y: -42
            }

            SplitView {
                id: split
                anchors.fill: parent
                orientation: Qt.Horizontal
                handle: Rectangle {
                    implicitWidth: 1
                    color: Ui.Style.borderSubtle
                    z: 5
                }

                Loader {
                    id: leftPaneLoader
                    active: !root.immersiveUtilitySurface || root.canUseUtilityRail
                    visible: active
                    SplitView.preferredWidth: root.immersiveUtilitySurface
                                              ? Ui.Style.leftPaneWidthUtilityRail
                                              : (root.tightChatColumns
                                              ? Ui.Style.leftPaneWidthDetailTight
                                              : (root.drawerTightChatColumns
                                              ? Ui.Style.leftPaneWidthDrawerTight
                                              : (root.isCompactShell
                                              ? Ui.Style.leftPaneWidthCompact
                                              : root.leftWidth)))
                    SplitView.minimumWidth: root.immersiveUtilitySurface
                                            ? Ui.Style.leftPaneWidthUtilityRail
                                            : (root.tightChatColumns
                                            ? Ui.Style.leftPaneWidthDetailTight
                                            : (root.drawerTightChatColumns
                                            ? Ui.Style.leftPaneWidthDrawerTight
                                            : Ui.Style.leftPaneWidthMin))
                    onWidthChanged: {
                        if (width > 80 &&
                                !root.isCompactShell &&
                                !root.tightChatColumns &&
                                !root.drawerTightChatColumns) {
                            root.leftWidth = width
                        }
                    }
                    sourceComponent: Shell.LeftPane {
                        compactShell: root.isCompactShell
                        onRequestNewChat: newChatDialog.open()
                        onRequestAddContact: addContactDialog.open()
                        onRequestCreateGroup: createGroupWizard.open()
                        onRequestNotifications: notificationDialog.open()
                        onRequestSettings: Ui.AppStore.setShellSurface("settings")
                        onRequestDeviceManager: Ui.AppStore.setShellSurface("security")
                    }
                }

                Shell.CenterPane {
                    id: centerPane
                    SplitView.fillWidth: true
                    SplitView.minimumWidth: Ui.Style.centerPaneWidthMin
                }

                Loader {
                    id: rightPaneLoader
                    active: root.rightPaneMounted
                    visible: active
                    SplitView.preferredWidth: root.tightChatColumns
                                              ? Ui.Style.rightPaneWidthTight
                                              : root.rightWidth
                    SplitView.minimumWidth: root.tightChatColumns
                                            ? Ui.Style.rightPaneWidthTight
                                            : Ui.Style.rightPaneWidthMin
                    SplitView.maximumWidth: Ui.Style.rightPaneWidthMax
                    onWidthChanged: {
                        if (active && width > Ui.Style.rightPaneWidthMin && !root.tightChatColumns) {
                            root.rightWidth = width
                        }
                    }
                    sourceComponent: Shell.RightPane {
                    }
                }
            }

            Loader {
                id: rightPaneDrawer
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: root.rightPaneDrawerWidth
                active: root.rightPaneDrawerVisible
                visible: active
                z: 30
                sourceComponent: Item {
                    anchors.fill: parent

                    Rectangle {
                        anchors.fill: parent
                        color: Ui.Style.panelBgAlt
                        border.width: 1
                        border.color: Ui.Style.borderSubtle
                    }

                    Shell.RightPane {
                        anchors.fill: parent
                    }
                }
            }

            MouseArea {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: rightPaneDrawer.left
                visible: root.rightPaneDrawerVisible
                enabled: visible
                z: 20
                acceptedButtons: Qt.LeftButton
                onClicked: Ui.ChatDisplayStore.closeRightPane()
            }
        }
    }

    Dialogs.NewChatDialog {
        id: newChatDialog
        ownerWindow: root.hostWindow
    }
    Dialogs.AddContactDialog {
        id: addContactDialog
        ownerWindow: root.hostWindow
    }
    Dialogs.CreateGroupWizard {
        id: createGroupWizard
        ownerWindow: root.hostWindow
    }
    Dialogs.NotificationCenterDialog {
        id: notificationDialog
        ownerWindow: root.hostWindow
    }
    function focusSearch() {
        if (leftPaneLoader.item && leftPaneLoader.item.focusSearch) {
            leftPaneLoader.item.focusSearch()
        }
    }

    function openNewChat() {
        Ui.AppStore.setShellSurface("chat")
        newChatDialog.open()
    }

    function showChatSearch() {
        if (shellSurface === "chat") {
            centerPane.showSearch()
        }
    }

    function openSettingsSurface() {
        if (Ui.SecurityDisplayStore && Ui.SecurityDisplayStore.refresh) {
            Ui.SecurityDisplayStore.refresh()
        }
        Ui.AppStore.setShellSurface("settings")
    }

    function openSecurityCenter() {
        if (Ui.SecurityDisplayStore && Ui.SecurityDisplayStore.refresh) {
            Ui.SecurityDisplayStore.refresh()
        }
        Ui.AppStore.setShellSurface("security")
    }

    function handleEscape() {
        if (root.securityCoordinator && root.securityCoordinator.handleEscape()) {
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
        if (notificationDialog.visible) {
            notificationDialog.close()
            return
        }
        if (centerPane.handleEscape()) {
            return
        }
        if (leftPaneLoader.item && leftPaneLoader.item.handleEscape && leftPaneLoader.item.handleEscape()) {
            return
        }
        if (root.rightPaneDrawerVisible) {
            Ui.ChatDisplayStore.closeRightPane()
            return
        }
        if (shellSurface === "security") {
            Ui.AppStore.setShellSurface("settings")
            return
        }
        if (shellSurface !== "chat") {
            Ui.AppStore.setShellSurface("chat")
        }
    }
}
