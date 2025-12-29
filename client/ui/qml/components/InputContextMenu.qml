import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Menu {
    id: menu
    property var target

    function isPasswordField() {
        return target && target.echoMode !== undefined &&
                target.echoMode === TextInput.Password
    }

    function canEdit() {
        return !(target && target.readOnly === true)
    }

    function selectedRange() {
        if (!target || target.selectionStart === undefined ||
                target.selectionEnd === undefined) {
            return null
        }
        var start = target.selectionStart
        var end = target.selectionEnd
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

    function selectionText() {
        if (!target) {
            return ""
        }
        if (target.selectedText !== undefined) {
            return target.selectedText || ""
        }
        var range = selectedRange()
        if (!range || target.text === undefined) {
            return ""
        }
        return target.text.slice(range.start, range.end)
    }

    function targetTextLength() {
        if (!target) {
            return 0
        }
        if (target.text !== undefined && target.text !== null) {
            return target.text.length
        }
        if (target.length !== undefined) {
            return target.length
        }
        return 0
    }

    function replaceSelection(text) {
        if (!target || !text) {
            return
        }
        var range = selectedRange()
        if (range && target.remove) {
            target.remove(range.start, range.end)
            if (target.cursorPosition !== undefined) {
                target.cursorPosition = range.start
            }
        }
        if (target.insert && target.cursorPosition !== undefined) {
            target.insert(target.cursorPosition, text)
        }
    }

    function hasSelection() {
        return selectedRange() !== null
    }

    function canPaste() {
        if (!Ui.AppStore.clipboardIsolationEnabled) {
            return true
        }
        var internalText = Ui.AppStore.internalClipboardText || ""
        var systemText = clientBridge ? clientBridge.systemClipboardText() : ""
        return internalText.length > 0 || systemText.length > 0
    }

    function copy(cut) {
        if (!target) {
            return
        }
        if (!Ui.AppStore.clipboardIsolationEnabled) {
            if (cut && target.cut) {
                target.cut()
                return
            }
            if (!cut && target.copy) {
                target.copy()
                return
            }
        }
        var selected = selectionText()
        if (selected.length === 0) {
            return
        }
        Ui.AppStore.setInternalClipboard(selected)
        if (cut && canEdit()) {
            var range = selectedRange()
            if (range && target.remove) {
                target.remove(range.start, range.end)
                if (target.cursorPosition !== undefined) {
                    target.cursorPosition = range.start
                }
            }
        }
    }

    function paste() {
        if (!target) {
            return
        }
        if (!Ui.AppStore.clipboardIsolationEnabled) {
            if (target.paste) {
                target.paste()
                return
            }
        }
        var internalText = Ui.AppStore.internalClipboardText || ""
        var internalMs = Ui.AppStore.internalClipboardMs || 0
        var systemText = clientBridge ? clientBridge.systemClipboardText() : ""
        var systemMs = clientBridge ? clientBridge.systemClipboardTimestamp() : 0
        var text = internalText
        if (systemText.length > 0 && systemMs > internalMs) {
            text = systemText
        }
        if (text.length === 0) {
            return
        }
        replaceSelection(text)
    }

    function selectAll() {
        if (target && target.selectAll) {
            target.selectAll()
        }
    }

    MenuItem {
        text: Ui.I18n.t("input.context.cut")
        enabled: hasSelection() && canEdit() && !isPasswordField()
        onTriggered: copy(true)
    }
    MenuItem {
        text: Ui.I18n.t("input.context.copy")
        enabled: hasSelection() && !isPasswordField()
        onTriggered: copy(false)
    }
    MenuItem {
        text: Ui.I18n.t("input.context.paste")
        enabled: canPaste() && canEdit()
        onTriggered: paste()
    }
    MenuItem {
        text: Ui.I18n.t("input.context.selectAll")
        enabled: targetTextLength() > 0
        onTriggered: selectAll()
    }
}
