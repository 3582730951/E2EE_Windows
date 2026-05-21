import QtQuick 2.15
import QtQuick.Controls 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

Menu {
    id: menu
    property var target

    function is_password_field() {
        return Ui.ClipboardUtil.is_password_field(target)
    }

    function can_edit() {
        return Ui.ClipboardUtil.can_edit(target)
    }

    function has_selection() {
        return Ui.ClipboardUtil.has_selection(target)
    }

    function target_text_length() {
        return Ui.ClipboardUtil.text_length(target)
    }

    function can_paste() {
        return Ui.ClipboardUtil.can_paste(clientBridge, Ui.AppStore.clipboardIsolationEnabled)
    }

    function copy_selection(cut) {
        Ui.ClipboardUtil.copy(target, cut, Ui.AppStore.clipboardIsolationEnabled)
    }

    function paste_selection() {
        Ui.ClipboardUtil.paste(target, clientBridge, Ui.AppStore.clipboardIsolationEnabled)
    }

    function select_all_text() {
        Ui.ClipboardUtil.select_all(target)
    }

    MenuItem {
        text: Ui.I18n.t("input.context.cut")
        enabled: has_selection() && can_edit() && !is_password_field()
        onTriggered: copy_selection(true)
    }
    MenuItem {
        text: Ui.I18n.t("input.context.copy")
        enabled: has_selection() && !is_password_field()
        onTriggered: copy_selection(false)
    }
    MenuItem {
        text: Ui.I18n.t("input.context.paste")
        enabled: can_paste() && can_edit()
        onTriggered: paste_selection()
    }
    MenuItem {
        text: Ui.I18n.t("input.context.selectAll")
        enabled: target_text_length() > 0
        onTriggered: select_all_text()
    }
}
