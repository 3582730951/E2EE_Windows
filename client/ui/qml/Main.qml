import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/auth" as Auth
import "qrc:/mi/e2ee/ui/qml/components" as Components
import "qrc:/mi/e2ee/ui/qml/shell" as Shell

ApplicationWindow {
    id: root
    property bool authMode: Ui.SessionStore.currentPage === 0
    property bool authReady: !!(authLoader
                                && authLoader.active
                                && authLoader.status === Loader.Ready)
    property bool shellReady: !!(shellLoader
                                 && shellLoader.active
                                 && shellLoader.status === Loader.Ready
                                 && shellLoader.item
                                 && shellLoader.item.shellReady === true)
    property int authWidth: 840
    property int authHeight: 620

    width: authMode ? authWidth : 1200
    height: authMode ? authHeight : 760
    minimumWidth: authMode ? authWidth : Ui.Style.shellMinWidth
    minimumHeight: authMode ? authHeight : Ui.Style.shellMinHeight
    maximumWidth: authMode ? authWidth : 16384
    maximumHeight: authMode ? authHeight : 16384
    flags: Qt.FramelessWindowHint | Qt.Window
    visible: true
    title: Ui.I18n.t("app.title")
    color: "transparent"
    font.family: Ui.Style.fontFamily
    palette.window: Ui.Style.windowBg
    palette.base: Ui.Style.panelBgAlt
    palette.button: Ui.Style.panelBgAlt
    palette.text: Ui.Style.textPrimary
    palette.buttonText: Ui.Style.textPrimary
    palette.highlight: Ui.Style.accent
    palette.highlightedText: Ui.Style.textPrimary

    function toggleMaximize() {
        if (root.visibility === Window.Maximized) {
            root.showNormal()
        } else {
            root.showMaximized()
        }
    }

    function begin_resize(edges) {
        if (root.visibility === Window.Maximized) {
            return
        }
        if (root.startSystemResize) {
            root.startSystemResize(edges)
        }
    }

    function activeShellItem() {
        return shellLoader && shellLoader.item ? shellLoader.item : null
    }

    function focusShellSearch() {
        var shell = activeShellItem()
        if (shell && shell.focusSearch) {
            shell.focusSearch()
        }
    }

    function showShellChatSearch() {
        var shell = activeShellItem()
        if (shell && shell.showChatSearch) {
            shell.showChatSearch()
        }
    }

    function openShellNewChat() {
        var shell = activeShellItem()
        if (shell && shell.openNewChat) {
            shell.openNewChat()
        }
    }

    function handleShellEscape() {
        var shell = activeShellItem()
        if (shell && shell.handleEscape) {
            shell.handleEscape()
        }
    }

    function openShellSecurityCenter() {
        var shell = activeShellItem()
        if (shell && shell.openSecurityCenter) {
            shell.openSecurityCenter()
        }
    }

    component ResizeHandle: MouseArea {
        property int edges: 0
        property int resizeCursor: Qt.ArrowCursor
        enabled: true
        visible: true
        z: 1000
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        cursorShape: resizeCursor
        preventStealing: true
        onPressed: root.begin_resize(edges)
    }

    Component {
        id: authFlowComponent

        Auth.AuthFlow {
            anchors.fill: parent
        }
    }

    Component {
        id: shellComponent

        Item {
            id: shellRoot
            anchors.fill: parent
            readonly property bool shellReady: appShell.shellReady

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

            Ui.SecurityDialogCoordinator {
                id: securityCoordinator
                ownerWindow: root
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    id: appTitleBar
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    Layout.minimumHeight: 26
                    Layout.maximumHeight: 26
                    color: Ui.Style.panelBg
                    property bool dragging: false
                    property point pressPos: Qt.point(0, 0)
                    property point windowPos: Qt.point(0, 0)

                    function hitButton(button, x, y) {
                        var p = button.mapToItem(appTitleBar, 0, 0)
                        return x >= p.x && x <= p.x + button.width &&
                               y >= p.y && y <= p.y + button.height
                    }

                    DragHandler {
                        id: appDrag
                        target: null
                        acceptedButtons: Qt.LeftButton
                        grabPermissions: PointerHandler.CanTakeOverFromItems
                        property bool manualDrag: false
                        property point windowPos: Qt.point(0, 0)

                        onActiveChanged: {
                            if (!active) {
                                manualDrag = false
                                return
                            }
                            manualDrag = true
                            if (root.startSystemMove && root.startSystemMove()) {
                                manualDrag = false
                            } else {
                                windowPos = Qt.point(root.x, root.y)
                            }
                        }

                        onTranslationChanged: {
                            if (!manualDrag) {
                                return
                            }
                            root.x = windowPos.x + translation.x
                            root.y = windowPos.y + translation.y
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        propagateComposedEvents: true
                        onDoubleClicked: root.toggleMaximize()
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Ui.Style.borderSubtle
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 6
                        z: 1

                        Item { Layout.fillWidth: true }

                        ToolButton {
                            id: minButton
                            hoverEnabled: true
                            implicitWidth: 28
                            implicitHeight: 18
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("window.minimize")
                            Accessible.name: ToolTip.text
                            onClicked: root.showMinimized()
                            background: Rectangle {
                                radius: 6
                                color: minButton.down ? Ui.Style.pressedBg
                                                      : (minButton.hovered ? Ui.Style.hoverBg : "transparent")
                            }
                            contentItem: Item {
                                width: 12
                                height: 12
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 10
                                    height: 2
                                    radius: 1
                                    color: minButton.hovered ? Ui.Style.textPrimary : Ui.Style.textSecondary
                                }
                            }
                        }

                        ToolButton {
                            id: maxButton
                            hoverEnabled: true
                            implicitWidth: 28
                            implicitHeight: 18
                            ToolTip.visible: hovered
                            ToolTip.text: root.visibility === Window.Maximized
                                          ? Ui.I18n.t("window.restore")
                                          : Ui.I18n.t("window.maximize")
                            Accessible.name: ToolTip.text
                            onClicked: root.toggleMaximize()
                            background: Rectangle {
                                radius: 6
                                color: maxButton.down ? Ui.Style.pressedBg
                                                      : (maxButton.hovered ? Ui.Style.hoverBg : "transparent")
                            }
                            contentItem: Item {
                                width: 12
                                height: 12
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 10
                                    height: 8
                                    color: "transparent"
                                    border.width: 1
                                    border.color: maxButton.hovered ? Ui.Style.textPrimary : Ui.Style.textSecondary
                                }
                            }
                        }

                        ToolButton {
                            id: closeButton
                            hoverEnabled: true
                            implicitWidth: 28
                            implicitHeight: 18
                            ToolTip.visible: hovered
                            ToolTip.text: Ui.I18n.t("window.close")
                            Accessible.name: ToolTip.text
                            onClicked: root.close()
                            background: Rectangle {
                                radius: 6
                                color: closeButton.down ? Ui.Style.pressedBg
                                                        : (closeButton.hovered ? Ui.Style.hoverBg : "transparent")
                            }
                            contentItem: Item {
                                width: 12
                                height: 12
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 10
                                    height: 2
                                    radius: 1
                                    rotation: 45
                                    color: closeButton.hovered ? Ui.Style.textPrimary : Ui.Style.textSecondary
                                }
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 10
                                    height: 2
                                    radius: 1
                                    rotation: -45
                                    color: closeButton.hovered ? Ui.Style.textPrimary : Ui.Style.textSecondary
                                }
                            }
                        }
                    }
                }

                Shell.AppShell {
                    id: appShell
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    windowWidth: root.width
                    securityCoordinator: securityCoordinator
                }
            }
        }
    }

    Rectangle {
        id: windowFrame
        anchors.fill: parent
        color: authMode
               ? "transparent"
               : Ui.Style.windowBg
        radius: authMode ? 18 : 20
        border.color: Ui.Style.borderSubtle
        border.width: 1
        antialiasing: true
        clip: true
        layer.enabled: true
        layer.smooth: true

        Rectangle {
            id: authTitleBar
            visible: authMode
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Ui.Style.authWindowTitleBarHeight
            color: Ui.Style.authTitleBarBg

            DragHandler {
                id: authDrag
                target: null
                acceptedButtons: Qt.LeftButton
                grabPermissions: PointerHandler.CanTakeOverFromItems
                property bool manualDrag: false
                property point windowPos: Qt.point(0, 0)

                onActiveChanged: {
                    if (!active) {
                        manualDrag = false
                        return
                    }
                    manualDrag = true
                    if (root.startSystemMove && root.startSystemMove()) {
                        manualDrag = false
                    } else {
                        windowPos = Qt.point(root.x, root.y)
                    }
                }

                onTranslationChanged: {
                    if (!manualDrag) {
                        return
                    }
                    root.x = windowPos.x + translation.x
                    root.y = windowPos.y + translation.y
                }
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 6

                Components.UiText {
                    text: Ui.I18n.t("app.title")
                    textRole: "caption"
                    roleColor: Ui.Style.authTitleBarText
                    font.pixelSize: Ui.Style.authWindowTitleTextSize
                }

                Item { Layout.fillWidth: true }

                ToolButton {
                    id: authMinButton
                    hoverEnabled: true
                    implicitWidth: 28
                    implicitHeight: 18
                    ToolTip.visible: hovered
                    ToolTip.text: Ui.I18n.t("window.minimize")
                    Accessible.name: ToolTip.text
                    onClicked: root.showMinimized()
                    background: Rectangle {
                        radius: 6
                        color: authMinButton.down ? Ui.Style.pressedBg
                                                  : (authMinButton.hovered ? Ui.Style.hoverBg : "transparent")
                    }
                    contentItem: Item {
                        width: 12
                        height: 12
                        Rectangle {
                            anchors.centerIn: parent
                            width: 10
                            height: 2
                            radius: 1
                            color: authMinButton.hovered ? Ui.Style.textPrimary : Ui.Style.textSecondary
                        }
                    }
                }

                ToolButton {
                    id: authCloseButton
                    hoverEnabled: true
                    implicitWidth: 28
                    implicitHeight: 18
                    ToolTip.visible: hovered
                    ToolTip.text: Ui.I18n.t("window.close")
                    Accessible.name: ToolTip.text
                    onClicked: root.close()
                    background: Rectangle {
                        radius: 6
                        color: authCloseButton.down ? Ui.Style.pressedBg
                                                    : (authCloseButton.hovered ? Ui.Style.hoverBg : "transparent")
                    }
                    contentItem: Item {
                        width: 12
                        height: 12
                        Rectangle {
                            anchors.centerIn: parent
                            width: 10
                            height: 2
                            radius: 1
                            rotation: 45
                            color: authCloseButton.hovered ? Ui.Style.textPrimary : Ui.Style.textSecondary
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 10
                            height: 2
                            radius: 1
                            rotation: -45
                            color: authCloseButton.hovered ? Ui.Style.textPrimary : Ui.Style.textSecondary
                        }
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Ui.Style.authTitleBarBorder
            }
        }

        Item {
            id: rootStack
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: authMode ? authTitleBar.bottom : parent.top
            Loader {
                id: authLoader
                anchors.fill: parent
                active: Ui.SessionStore.currentPage === 0
                asynchronous: true
                visible: status === Loader.Ready
                sourceComponent: authFlowComponent
            }

            Loader {
                id: shellLoader
                anchors.fill: parent
                active: Ui.SessionStore.currentPage !== 0
                asynchronous: true
                visible: status === Loader.Ready
                sourceComponent: shellComponent
            }
        }
    }

    ResizeHandle {
        edges: Qt.LeftEdge
        resizeCursor: Qt.SizeHorCursor
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
    }

    ResizeHandle {
        edges: Qt.RightEdge
        resizeCursor: Qt.SizeHorCursor
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
    }

    ResizeHandle {
        edges: Qt.TopEdge
        resizeCursor: Qt.SizeVerCursor
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 6
    }

    ResizeHandle {
        edges: Qt.BottomEdge
        resizeCursor: Qt.SizeVerCursor
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 6
    }

    ResizeHandle {
        edges: Qt.LeftEdge | Qt.TopEdge
        resizeCursor: Qt.SizeFDiagCursor
        anchors.left: parent.left
        anchors.top: parent.top
        width: 12
        height: 12
    }

    ResizeHandle {
        edges: Qt.RightEdge | Qt.BottomEdge
        resizeCursor: Qt.SizeFDiagCursor
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 12
        height: 12
    }

    ResizeHandle {
        edges: Qt.RightEdge | Qt.TopEdge
        resizeCursor: Qt.SizeBDiagCursor
        anchors.right: parent.right
        anchors.top: parent.top
        width: 12
        height: 12
    }

    ResizeHandle {
        edges: Qt.LeftEdge | Qt.BottomEdge
        resizeCursor: Qt.SizeBDiagCursor
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: 12
        height: 12
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: focusShellSearch()
    }
    Shortcut {
        sequence: "Ctrl+N"
        onActivated: openShellNewChat()
    }
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: showShellChatSearch()
    }
    Shortcut {
        sequence: "Esc"
        onActivated: handleShellEscape()
    }
    Shortcut {
        sequences: [StandardKey.Copy]
        context: Qt.ApplicationShortcut
        enabled: Ui.PreferenceStore.clipboardIsolationEnabled
        onActivated: Ui.ClipboardUtil.copy_active(root.activeFocusItem, false, true)
    }
    Shortcut {
        sequences: [StandardKey.Cut]
        context: Qt.ApplicationShortcut
        enabled: Ui.PreferenceStore.clipboardIsolationEnabled
        onActivated: Ui.ClipboardUtil.copy_active(root.activeFocusItem, true, true)
    }
    Shortcut {
        sequences: [StandardKey.Paste]
        context: Qt.ApplicationShortcut
        enabled: Ui.PreferenceStore.clipboardIsolationEnabled
        onActivated: Ui.ClipboardUtil.paste_active(root.activeFocusItem, clientBridge, true)
    }
    Shortcut {
        sequences: [StandardKey.SelectAll]
        context: Qt.ApplicationShortcut
        enabled: Ui.PreferenceStore.clipboardIsolationEnabled
        onActivated: Ui.ClipboardUtil.select_all(Ui.ClipboardUtil.active_text_item(root.activeFocusItem))
    }

    Ui.TrustFlowCoordinator {
        ownerWindow: root
    }
}
