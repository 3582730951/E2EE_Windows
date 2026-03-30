import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
import "qrc:/mi/e2ee/ui/qml/auth" as Auth
import "qrc:/mi/e2ee/ui/qml/components" as Components

ApplicationWindow {
    id: root
    property bool authMode: Ui.SessionStore.currentPage === 0
    property bool smokeMode: typeof uiSmokeMode !== "undefined" ? !!uiSmokeMode : false
    property string smokeScene: typeof uiSmokeScene !== "undefined" ? (uiSmokeScene || "") : ""
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

    width: authMode
           ? authWidth
           : (smokeMode ? Ui.SmokeSceneStore.viewportWidth(false) : 1200)
    height: authMode
            ? authHeight
            : (smokeMode ? Ui.SmokeSceneStore.viewportHeight() : 760)
    minimumWidth: authMode ? authWidth : width
    minimumHeight: authMode ? authHeight : height
    maximumWidth: smokeMode ? width : (authMode ? authWidth : 16384)
    maximumHeight: smokeMode ? height : (authMode ? authHeight : 16384)
    flags: Qt.FramelessWindowHint | Qt.Window
    visible: true
    title: Ui.I18n.t("app.title")
    color: smokeMode
           ? (authMode ? Ui.Style.authBackdropBottom : Ui.Style.windowBg)
           : "transparent"
    font.family: Ui.Style.fontFamily
    palette.window: Ui.Style.windowBg
    palette.base: Ui.Style.panelBgAlt
    palette.button: Ui.Style.panelBgAlt
    palette.text: Ui.Style.textPrimary
    palette.buttonText: Ui.Style.textPrimary
    palette.highlight: Ui.Style.accent
    palette.highlightedText: Ui.Style.textPrimary

    function activeTextItem() {
        var item = root.activeFocusItem
        if (!item) {
            return null
        }
        if (item.readOnly !== undefined && item.readOnly) {
            return null
        }
        if (item.selectedText === undefined && item.insert === undefined) {
            return null
        }
        return item
    }

    function selectionRange(item) {
        if (!item) {
            return null
        }
        if (item.selectionStart === undefined || item.selectionEnd === undefined) {
            return null
        }
        var start = item.selectionStart
        var end = item.selectionEnd
        if (start === end) {
            return null
        }
        if (start > end) {
            var tmp = start
            start = end
            end = tmp
        }
        return { start: start, end: end }
    }

    function handleSecureCopy(cut) {
        var item = activeTextItem()
        if (!item || item.selectedText === undefined) {
            return
        }
        var text = item.selectedText || ""
        if (text.length === 0) {
            return
        }
        Ui.ChatDisplayStore.setInternalClipboard(text)
        if (cut && item.remove !== undefined) {
            var range = selectionRange(item)
            if (range) {
                item.remove(range.start, range.end)
                if (item.cursorPosition !== undefined) {
                    item.cursorPosition = range.start
                }
            }
        }
    }

    function handleSecurePaste() {
        var item = activeTextItem()
        if (!item) {
            return
        }
        var internalText = Ui.ChatDisplayStore.internalClipboardText || ""
        var internalMs = Ui.ChatDisplayStore.internalClipboardMs || 0
        var systemText = clientBridge ? clientBridge.systemClipboardText() : ""
        var systemMs = clientBridge ? clientBridge.systemClipboardTimestamp() : 0
        var text = internalText
        if (systemText.length > 0 && systemMs > internalMs) {
            text = systemText
        }
        if (text.length === 0) {
            return
        }
        if (item.insert !== undefined && item.cursorPosition !== undefined) {
            item.insert(item.cursorPosition, text)
        }
    }

    function handleSecureSelectAll() {
        var item = activeTextItem()
        if (item && item.selectAll !== undefined) {
            item.selectAll()
        }
    }

    function toggleMaximize() {
        if (root.visibility === Window.Maximized) {
            root.showNormal()
        } else {
            root.showMaximized()
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

            function focusSearch() {
                smokeAdapter.focusSearch()
            }

            function showChatSearch() {
                smokeAdapter.showChatSearch()
            }

            function handleEscape() {
                smokeAdapter.handleEscape()
            }

            function openSecurityCenter() {
                smokeAdapter.openSecurityCenter()
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

                Ui.SmokeAdapter {
                    id: smokeAdapter
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    windowWidth: root.width
                }
            }
        }
    }

    Rectangle {
        id: windowFrame
        anchors.fill: parent
        color: authMode
               ? (smokeMode ? Ui.Style.authBackdropBottom : "transparent")
               : Ui.Style.windowBg
        radius: authMode ? 18 : 20
        border.color: Ui.Style.borderSubtle
        border.width: 1
        antialiasing: !(smokeMode && authMode)
        clip: true
        layer.enabled: !(smokeMode && authMode)
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

                Components.UiText {
                    text: Ui.I18n.t("auth.subtitle")
                    textRole: "caption"
                    roleColor: Ui.Style.textMuted
                    font.pixelSize: Ui.Style.authMetaTextSize
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
                asynchronous: !root.smokeMode
                visible: status === Loader.Ready
                sourceComponent: authFlowComponent
            }

            Loader {
                id: shellLoader
                anchors.fill: parent
                active: Ui.SessionStore.currentPage !== 0
                asynchronous: !root.smokeMode
                visible: status === Loader.Ready
                sourceComponent: shellComponent
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: focusShellSearch()
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
        onActivated: handleSecureCopy(false)
    }
    Shortcut {
        sequences: [StandardKey.Cut]
        context: Qt.ApplicationShortcut
        enabled: Ui.PreferenceStore.clipboardIsolationEnabled
        onActivated: handleSecureCopy(true)
    }
    Shortcut {
        sequences: [StandardKey.Paste]
        context: Qt.ApplicationShortcut
        enabled: Ui.PreferenceStore.clipboardIsolationEnabled
        onActivated: handleSecurePaste()
    }
    Shortcut {
        sequences: [StandardKey.SelectAll]
        context: Qt.ApplicationShortcut
        enabled: Ui.PreferenceStore.clipboardIsolationEnabled
        onActivated: handleSecureSelectAll()
    }

    Ui.TrustFlowCoordinator {
        ownerWindow: root
    }
}
