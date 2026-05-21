pragma Singleton
import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

QtObject {
    function active_text_item(item) {
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

    function is_password_field(item) {
        return item && item.echoMode !== undefined && item.echoMode === TextInput.Password
    }

    function can_edit(item) {
        return !(item && item.readOnly === true)
    }

    function selection_range(item) {
        if (!item || item.selectionStart === undefined || item.selectionEnd === undefined) {
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

    function selected_text(item) {
        if (!item) {
            return ""
        }
        if (item.selectedText !== undefined) {
            return item.selectedText || ""
        }
        var range = selection_range(item)
        if (!range || item.text === undefined) {
            return ""
        }
        return item.text.slice(range.start, range.end)
    }

    function text_length(item) {
        if (!item) {
            return 0
        }
        if (item.text !== undefined && item.text !== null) {
            return item.text.length
        }
        if (item.length !== undefined) {
            return item.length
        }
        return 0
    }

    function replace_selection(item, text) {
        if (!item || !text) {
            return
        }
        var range = selection_range(item)
        if (range && item.remove) {
            item.remove(range.start, range.end)
            if (item.cursorPosition !== undefined) {
                item.cursorPosition = range.start
            }
        }
        if (item.insert && item.cursorPosition !== undefined) {
            item.insert(item.cursorPosition, text)
        }
    }

    function has_selection(item) {
        return selection_range(item) !== null
    }

    function newest_clipboard_text(client_bridge) {
        var internalText = Ui.ChatDisplayStore.internalClipboardText || ""
        var internalMs = Ui.ChatDisplayStore.internalClipboardMs || 0
        if (Date.now() - internalMs > 30000) {
            Ui.ChatDisplayStore.clearInternalClipboard()
            return ""
        }
        return internalText
    }

    function can_paste(client_bridge, isolated) {
        if (!isolated) {
            return true
        }
        return newest_clipboard_text(client_bridge).length > 0
    }

    function copy(item, cut, isolated) {
        if (!item) {
            return
        }
        if (is_password_field(item)) {
            return
        }
        if (!isolated) {
            if (cut && item.cut) {
                item.cut()
                return
            }
            if (!cut && item.copy) {
                item.copy()
                return
            }
        }
        var selected = selected_text(item)
        if (selected.length === 0) {
            return
        }
        Ui.ChatDisplayStore.setInternalClipboard(selected)
        if (cut && can_edit(item)) {
            var range = selection_range(item)
            if (range && item.remove) {
                item.remove(range.start, range.end)
                if (item.cursorPosition !== undefined) {
                    item.cursorPosition = range.start
                }
            }
        }
    }

    function paste(item, client_bridge, isolated) {
        if (!item) {
            return
        }
        if (!isolated) {
            if (item.paste) {
                item.paste()
            }
            return
        }
        replace_selection(item, newest_clipboard_text(client_bridge))
    }

    function copy_active(focus_item, cut, isolated) {
        copy(active_text_item(focus_item), cut, isolated)
    }

    function paste_active(focus_item, client_bridge, isolated) {
        paste(active_text_item(focus_item), client_bridge, isolated)
    }

    function select_all(item) {
        if (item && !is_password_field(item) && item.selectAll) {
            item.selectAll()
        }
    }
}
