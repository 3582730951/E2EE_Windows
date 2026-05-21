pragma Singleton
import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

QtObject {
    function format_call_duration(total_sec) {
        var sec = Math.max(0, total_sec || 0)
        var hours = Math.floor(sec / 3600)
        var minutes = Math.floor((sec % 3600) / 60)
        var seconds = sec % 60
        var hh = hours > 0 ? (hours < 10 ? "0" + hours : "" + hours) : ""
        var mm = minutes < 10 ? "0" + minutes : "" + minutes
        var ss = seconds < 10 ? "0" + seconds : "" + seconds
        return hours > 0 ? (hh + ":" + mm + ":" + ss) : (mm + ":" + ss)
    }

    function preview_icon_for(kind) {
        switch (kind) {
        case "photo":
            return "qrc:/mi/e2ee/ui/icons/image.svg"
        case "video":
            return "qrc:/mi/e2ee/ui/icons/video.svg"
        case "voice":
            return "qrc:/mi/e2ee/ui/icons/mic.svg"
        case "link":
            return "qrc:/mi/e2ee/ui/icons/info.svg"
        default:
            return "qrc:/mi/e2ee/ui/icons/file.svg"
        }
    }

    function preview_accent_for(kind) {
        switch (kind) {
        case "photo":
            return Ui.Style.accent
        case "video":
            return "#3B82F6"
        case "voice":
            return Ui.Style.success
        case "link":
            return "#0EA5E9"
        default:
            return Ui.Style.textSecondary
        }
    }

    function preview_tint_for(kind) {
        switch (kind) {
        case "photo":
            return Qt.rgba(37 / 255, 99 / 255, 235 / 255, Ui.Style.isDark ? 0.20 : 0.12)
        case "video":
            return Qt.rgba(59 / 255, 130 / 255, 246 / 255, Ui.Style.isDark ? 0.20 : 0.12)
        case "voice":
            return Qt.rgba(5 / 255, 150 / 255, 105 / 255, Ui.Style.isDark ? 0.20 : 0.12)
        case "link":
            return Qt.rgba(14 / 255, 165 / 255, 233 / 255, Ui.Style.isDark ? 0.20 : 0.12)
        default:
            return Qt.rgba(100 / 255, 116 / 255, 139 / 255, Ui.Style.isDark ? 0.16 : 0.10)
        }
    }

    function preview_border_for(kind) {
        switch (kind) {
        case "photo":
            return Qt.rgba(37 / 255, 99 / 255, 235 / 255, Ui.Style.isDark ? 0.30 : 0.18)
        case "video":
            return Qt.rgba(59 / 255, 130 / 255, 246 / 255, Ui.Style.isDark ? 0.30 : 0.18)
        case "voice":
            return Qt.rgba(5 / 255, 150 / 255, 105 / 255, Ui.Style.isDark ? 0.30 : 0.18)
        case "link":
            return Qt.rgba(14 / 255, 165 / 255, 233 / 255, Ui.Style.isDark ? 0.30 : 0.18)
        default:
            return Ui.Style.borderSubtle
        }
    }
}
