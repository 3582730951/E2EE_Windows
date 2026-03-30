pragma Singleton
import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui

QtObject {
    id: store

    readonly property bool smokeMode: typeof uiSmokeMode !== "undefined" ? !!uiSmokeMode : false
    readonly property string sceneName: typeof uiSmokeScene !== "undefined"
                                        ? ((uiSmokeScene || "").toLowerCase())
                                        : ""
    readonly property string localeOverride: typeof uiSmokeLocale !== "undefined"
                                             ? (uiSmokeLocale || "")
                                             : ""
    readonly property string themeOverride: typeof uiSmokeTheme !== "undefined"
                                            ? ((uiSmokeTheme || "").toLowerCase())
                                            : ""
    readonly property double scaleOverride: typeof uiSmokeScale !== "undefined"
                                            ? Number(uiSmokeScale || 1.0)
                                            : 1.0
    readonly property string normalizedScene: {
        if (sceneName === "login" ||
                sceneName === "security_center" ||
                sceneName === "post_login_light" ||
                sceneName === "post_login") {
            return sceneName
        }
        return "post_login"
    }
    readonly property bool loginScene: normalizedScene === "login"
    readonly property bool securityCenterScene: normalizedScene === "security_center"
    readonly property bool postLoginLightScene: normalizedScene === "post_login_light"
    readonly property bool postLoginScene: normalizedScene !== "login"
    readonly property bool forceLightTheme: postLoginLightScene || themeOverride === "light"
    readonly property bool forceDarkTheme: themeOverride === "dark"

    function activatePreview() {
        if (!smokeMode || !postLoginScene || Ui.SessionStore.currentPage !== 0) {
            return
        }
        Ui.AppStore.enterSmokeShellPreview(normalizedScene)
    }

    function viewportWidth(includeAuthFallback) {
        if (loginScene || includeAuthFallback === true) {
            return 840
        }
        return 900
    }

    function viewportHeight() {
        return 620
    }

    function captureName() {
        if (normalizedScene === "login") {
            return "login"
        }
        if (normalizedScene === "security_center") {
            return "security-center"
        }
        if (normalizedScene === "post_login_light") {
            return "post-login-light"
        }
        return "post-login"
    }
}
