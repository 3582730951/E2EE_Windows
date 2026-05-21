#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QRect>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QSet>
#include <QSize>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include <vector>

#ifdef Q_OS_WIN
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <imm.h>
#ifdef _MSC_VER
#pragma comment(lib, "Imm32.lib")
#endif
#endif

#include "quick_client.h"
#include "common/UiRuntimePaths.h"

namespace {

struct WindowsUiRuntimeContract {
    int shellMinWidth;
    int compactTwoColumnMinWidth;
    int twoColumnDrawerMinWidth;
    int threeColumnMinWidth;
};

const WindowsUiRuntimeContract& ui_runtime_contract_spec() {
    static const WindowsUiRuntimeContract contract{
        760,
        980,
        1120,
        1360
    };
    return contract;
}

bool env_flag(const char* name) {
    const QByteArray value = qgetenv(name).trimmed().toLower();
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

QString env_string(const char* name, const QString& fallback = QString()) {
    const QByteArray value = qgetenv(name);
    if (value.isEmpty()) {
        return fallback;
    }
    return QString::fromUtf8(value);
}

int env_int(const char* name, int fallback) {
    bool ok = false;
    const int value = env_string(name).toInt(&ok);
    return ok ? value : fallback;
}

QSize ci_capture_viewport(const QString& scene) {
    if (scene == QStringLiteral("login")) {
        return QSize(840, 620);
    }
    return QSize(900, 620);
}

QString ci_capture_file_name(const QString& scene) {
    if (scene == QStringLiteral("login")) {
        return QStringLiteral("login.png");
    }
    if (scene == QStringLiteral("chat_detail")) {
        return QStringLiteral("chat-detail.png");
    }
    if (scene == QStringLiteral("calls_home")) {
        return QStringLiteral("calls-home.png");
    }
    if (scene == QStringLiteral("settings_home")) {
        return QStringLiteral("settings-home.png");
    }
    if (scene == QStringLiteral("security_center")) {
        return QStringLiteral("security-center.png");
    }
    if (scene == QStringLiteral("post_login_light")) {
        return QStringLiteral("post-login-light.png");
    }
    return QStringLiteral("post-login.png");
}

void apply_ci_capture_scene(QQmlApplicationEngine& engine,
                            const QString& scene,
                            const QString& locale,
                            const QString& theme) {
    QQmlComponent component(&engine);
    component.setData(R"(
import QtQuick 2.15
import "qrc:/mi/e2ee/ui/qml" as Ui
QtObject {
    function apply(scene, locale, theme) {
        if (locale === "zh-CN" || locale === "en-US") {
            Ui.I18n.setLocaleMode(locale)
        }
        if (theme === "dark" || theme === "light") {
            Ui.Style.themeMode = theme
        }
        Ui.AppStore.init()
        if (scene === "login") {
            Ui.AppStore.currentPage = 0
            Ui.AppStore.syncDomainStores()
            return
        }
        var chatId = "ci-runtime-chat"
        Ui.AppStore.currentPage = 1
        Ui.AppStore.ensureDialog(chatId, "private", Ui.I18n.t("app.title"), 2, Ui.I18n.t("app.title"))
        Ui.AppStore.setCurrentChat(chatId)
        var model = Ui.AppStore.messagesModel(chatId)
        if (model.count === 0) {
            Ui.AppStore.appendMessage(chatId, {
                chatId: chatId,
                msgId: "ci-runtime-in",
                kind: "in",
                contentKind: "text",
                senderName: Ui.I18n.t("app.title"),
                text: Ui.I18n.t("status.connected"),
                protectedTextId: "",
                timeText: "09:41",
                timestampMs: Date.now() - 60000,
                statusTicks: "none",
                edited: false,
                fileName: "",
                fileSize: 0,
                fileId: "",
                fileKey: "",
                fileUrl: "",
                downloadProgress: 0,
                imageEnhanced: false,
                stickerId: "",
                stickerUrl: "",
                stickerAnimated: false,
                previewUrl: "",
                contactUsername: "",
                contactDisplay: "",
                locationLabel: "",
                locationLat: 0,
                locationLon: 0,
                callId: "",
                callVideo: false,
                animateEmoji: false
            }, false)
            Ui.AppStore.appendMessage(chatId, {
                chatId: chatId,
                msgId: "ci-runtime-out",
                kind: "out",
                contentKind: "text",
                senderName: Ui.I18n.t("chat.you"),
                text: Ui.I18n.t("chat.writeMessage"),
                protectedTextId: "",
                timeText: "09:42",
                timestampMs: Date.now() - 30000,
                statusTicks: "sent",
                edited: false,
                fileName: "",
                fileSize: 0,
                fileId: "",
                fileKey: "",
                fileUrl: "",
                downloadProgress: 0,
                imageEnhanced: false,
                stickerId: "",
                stickerUrl: "",
                stickerAnimated: false,
                previewUrl: "",
                contactUsername: "",
                contactDisplay: "",
                locationLabel: "",
                locationLat: 0,
                locationLon: 0,
                callId: "",
                callVideo: false,
                animateEmoji: false
            }, false)
        }
        if (scene === "calls_home") {
            Ui.AppStore.setShellSurface("calls")
        } else if (scene === "settings_home") {
            Ui.AppStore.setShellSurface("settings")
        } else if (scene === "security_center") {
            Ui.AppStore.setShellSurface("security")
        } else {
            Ui.AppStore.setShellSurface("chat")
            if (scene === "chat_detail") {
                Ui.AppStore.rightPaneVisible = true
            }
        }
        Ui.AppStore.syncDomainStores()
    }
}
)", QUrl(QStringLiteral("qrc:/mi/e2ee/ui/qml/CiRuntimeCapture.qml")));
    std::unique_ptr<QObject> helper(component.create());
    if (!helper) {
        const auto errors = component.errors();
        for (const auto& error : errors) {
            qWarning() << error;
        }
        return;
    }
    QMetaObject::invokeMethod(helper.get(), "apply",
                              Q_ARG(QVariant, QVariant(scene)),
                              Q_ARG(QVariant, QVariant(locale)),
                              Q_ARG(QVariant, QVariant(theme)));
}

void schedule_ci_capture(QQuickWindow* window, const QString& scene) {
    if (!window) {
        return;
    }
    const QString capture_dir = env_string("MI_E2EE_CI_UI_CAPTURE_DIR");
    if (capture_dir.isEmpty()) {
        QTimer::singleShot(0, [] { QCoreApplication::exit(2); });
        return;
    }
    QDir().mkpath(capture_dir);
    const QString capture_path = QDir(capture_dir).filePath(ci_capture_file_name(scene));
    const QSize viewport = ci_capture_viewport(scene);
    window->setMinimumSize(viewport);
    window->setMaximumSize(viewport);
    window->resize(viewport);
    window->show();
    const int delay_ms = std::max(500, env_int("MI_E2EE_CI_UI_CAPTURE_MS", 1600));
    QTimer::singleShot(delay_ms, window, [window, capture_path] {
        const QImage image = window->grabWindow();
        if (image.isNull() || !image.save(capture_path)) {
            QCoreApplication::exit(3);
            return;
        }
        QCoreApplication::exit(0);
    });
}

class AuthWindowDragFilter : public QObject {
public:
    explicit AuthWindowDragFilter(QQuickWindow* window)
        : QObject(window), window_(window) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (!window_ || obj != window_) {
            return false;
        }
        const bool authMode = window_->property("authMode").toBool();
        if (!authMode) {
            pressed_ = false;
            dragging_ = false;
            return false;
        }

        switch (event->type()) {
            case QEvent::MouseButtonPress: {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    pressed_ = true;
                    dragging_ = false;
                    pressPos_ = mouseEvent->globalPosition();
                }
                break;
            }
            case QEvent::MouseMove: {
                if (!pressed_ || dragging_) {
                    break;
                }
                auto* mouseEvent = static_cast<QMouseEvent*>(event);
                const QPointF delta = mouseEvent->globalPosition() - pressPos_;
                if (std::abs(delta.x()) < kDragThreshold && std::abs(delta.y()) < kDragThreshold) {
                    break;
                }
                dragging_ = true;
                pressed_ = false;
                start_native_move();
                break;
            }
            case QEvent::MouseButtonRelease:
                pressed_ = false;
                dragging_ = false;
                break;
            default:
                break;
        }
        return false;
    }

private:
    void start_native_move() {
        if (!window_) {
            return;
        }
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(window_->winId());
        ReleaseCapture();
        SendMessageW(hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
#else
        window_->startSystemMove();
#endif
    }

    QPointer<QQuickWindow> window_;
    bool pressed_ = false;
    bool dragging_ = false;
    QPointF pressPos_;
    static constexpr qreal kDragThreshold = 4.0;
};

class WindowRoundFilter : public QObject {
public:
    explicit WindowRoundFilter(QQuickWindow* window)
        : QObject(window), window_(window) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (!window_ || obj != window_) {
            return false;
        }
        switch (event->type()) {
            case QEvent::Show:
            case QEvent::Resize:
            case QEvent::WindowStateChange:
                apply_rounded_region();
                break;
            default:
                break;
        }
        return false;
    }

private:
    void apply_rounded_region() {
#ifdef Q_OS_WIN
        const HWND hwnd = reinterpret_cast<HWND>(window_->winId());
        if (!hwnd) {
            return;
        }
        const bool maximized = (window_->windowState() & Qt::WindowMaximized) != 0;
        const bool fullscreen = window_->visibility() == QWindow::FullScreen;
        if (maximized || fullscreen) {
            SetWindowRgn(hwnd, nullptr, TRUE);
            return;
        }
        const int w = window_->width();
        const int h = window_->height();
        if (w <= 0 || h <= 0) {
            return;
        }
        const bool authMode = window_->property("authMode").toBool();
        const int radius = authMode ? 9 : 10;
        using create_round_rect_rgn_fn = HRGN(WINAPI*)(int, int, int, int, int, int);
        HMODULE gdi32 = GetModuleHandleW(L"gdi32.dll");
        if (!gdi32) {
            gdi32 = LoadLibraryW(L"gdi32.dll");
        }
        auto* round_region_proc = gdi32
            ? reinterpret_cast<create_round_rect_rgn_fn>(
                  GetProcAddress(gdi32, "CreateRoundRectRgn"))
            : nullptr;
        HRGN window_rgn = round_region_proc
            ? round_region_proc(0, 0, w + 1, h + 1, radius * 2, radius * 2)
            : CreateRectRgn(0, 0, w + 1, h + 1);
        if (window_rgn) {
            SetWindowRgn(hwnd, window_rgn, TRUE);
        }
#endif
    }

    QPointer<QQuickWindow> window_;
};

class SecureClipboardFilter : public QObject {
public:
    SecureClipboardFilter(QObject* root,
                          mi::client::ui::QuickClient* client,
                          QObject* parent = nullptr)
        : QObject(parent), root_(root), client_(client) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (!root_ || !client_ || !event) {
            return QObject::eventFilter(obj, event);
        }
        if (!client_->clipboardIsolation()) {
            return QObject::eventFilter(obj, event);
        }
        if (event->type() == QEvent::ShortcutOverride) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->matches(QKeySequence::Copy) ||
                keyEvent->matches(QKeySequence::Cut) ||
                keyEvent->matches(QKeySequence::Paste) ||
                keyEvent->matches(QKeySequence::SelectAll)) {
                event->accept();
                return true;
            }
            return QObject::eventFilter(obj, event);
        }
        if (event->type() != QEvent::KeyPress) {
            return QObject::eventFilter(obj, event);
        }
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->matches(QKeySequence::Copy)) {
            QMetaObject::invokeMethod(root_, "handleSecureCopy",
                                      Q_ARG(QVariant, false));
            return true;
        }
        if (keyEvent->matches(QKeySequence::Cut)) {
            QMetaObject::invokeMethod(root_, "handleSecureCopy",
                                      Q_ARG(QVariant, true));
            return true;
        }
        if (keyEvent->matches(QKeySequence::Paste)) {
            QMetaObject::invokeMethod(root_, "handleSecurePaste");
            return true;
        }
        if (keyEvent->matches(QKeySequence::SelectAll)) {
            QMetaObject::invokeMethod(root_, "handleSecureSelectAll");
            return true;
        }
        return QObject::eventFilter(obj, event);
    }

private:
    QPointer<QObject> root_;
    QPointer<mi::client::ui::QuickClient> client_;
};

class InputMethodBlocker : public QObject {
public:
    explicit InputMethodBlocker(mi::client::ui::QuickClient* client,
                                QObject* parent = nullptr)
        : QObject(parent), client_(client) {}

    void refresh() {
        if (!client_) {
            return;
        }
        const auto windows = QGuiApplication::allWindows();
        for (auto* window : windows) {
            apply_for_window(window);
        }
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (!client_ || !obj || !event) {
            return QObject::eventFilter(obj, event);
        }
        if (event->type() == QEvent::Show ||
            event->type() == QEvent::WindowActivate ||
            event->type() == QEvent::FocusIn) {
            if (auto* window = qobject_cast<QWindow*>(obj)) {
                apply_for_window(window);
            }
        }
        if (event->type() == QEvent::InputMethod) {
            if (client_->internalImeEnabled() && is_text_input(obj)) {
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }

private:
    static bool is_text_input(QObject* obj) {
        if (!obj) {
            return false;
        }
        return obj->inherits("QQuickTextInput") ||
               obj->inherits("QQuickTextEdit") ||
               obj->inherits("QQuickTextArea");
    }

    void apply_for_window(QWindow* window) {
#ifdef Q_OS_WIN
        if (!window || !client_) {
            return;
        }
        const HWND hwnd = reinterpret_cast<HWND>(window->winId());
        if (!hwnd) {
            return;
        }
        const bool enableInternal = client_->internalImeEnabled();
        if (enableInternal) {
            if (!saved_contexts_.contains(window->winId())) {
                HIMC previous = ImmAssociateContext(hwnd, nullptr);
                saved_contexts_.insert(window->winId(), previous);
            } else {
                ImmAssociateContext(hwnd, nullptr);
            }
        } else if (saved_contexts_.contains(window->winId())) {
            HIMC previous = saved_contexts_.take(window->winId());
            ImmAssociateContext(hwnd, previous);
        }
#else
        Q_UNUSED(window);
#endif
    }

    QPointer<mi::client::ui::QuickClient> client_;
#ifdef Q_OS_WIN
    QHash<WId, HIMC> saved_contexts_;
#endif
};

}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QML_XHR_ALLOW_FILE_READ", "1");
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    UiRuntimePaths::Prepare(argv[0]);
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MI"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("mi-e2ee.local"));
    QCoreApplication::setApplicationName(QStringLiteral("MI E2EE Client"));

    const auto& uiRuntimeContract = ui_runtime_contract_spec();

    QQmlApplicationEngine engine;
#if defined(MI_E2EE_PRIVACY_STRICT) && !defined(MI_E2EE_UI_TESTING)
    engine.setOutputWarningsToStandardError(false);
#else
    engine.setOutputWarningsToStandardError(true);
#endif
    mi::client::ui::QuickClient client;
    engine.rootContext()->setContextProperty("clientBridge", &client);
    engine.rootContext()->setContextProperty("uiShellMinWidth", uiRuntimeContract.shellMinWidth);
    engine.rootContext()->setContextProperty("uiCompactTwoColumnMinWidth",
                                             uiRuntimeContract.compactTwoColumnMinWidth);
    engine.rootContext()->setContextProperty("uiTwoColumnDrawerMinWidth",
                                             uiRuntimeContract.twoColumnDrawerMinWidth);
    engine.rootContext()->setContextProperty("uiThreeColumnMinWidth",
                                             uiRuntimeContract.threeColumnMinWidth);
    if (!QFile::exists(QStringLiteral(":/mi/e2ee/ui/qml/Main.qml"))) {
        return -1;
    }

    const QUrl url(QStringLiteral("qrc:/mi/e2ee/ui/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject* obj, const QUrl& objUrl) {
                         if (!obj && url == objUrl) {
                             QCoreApplication::exit(-1);
                         }
                     }, Qt::QueuedConnection);
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    QObject* rootObject = engine.rootObjects().first();
    auto* window = qobject_cast<QQuickWindow*>(rootObject);
    if (env_flag("MI_E2EE_CI_UI_CAPTURE")) {
        const QString scene = env_string("MI_E2EE_CI_UI_CAPTURE_SCENE",
                                         QStringLiteral("post_login"));
        const QString locale = env_string("MI_E2EE_CI_UI_CAPTURE_LOCALE",
                                          QStringLiteral("zh-CN"));
        const QString theme = env_string("MI_E2EE_CI_UI_CAPTURE_THEME",
                                         QStringLiteral("light"));
        apply_ci_capture_scene(engine, scene, locale, theme);
        schedule_ci_capture(window, scene);
    }
    if (window) {
        window->installEventFilter(new AuthWindowDragFilter(window));
        window->installEventFilter(new WindowRoundFilter(window));
    }
    app.installEventFilter(new SecureClipboardFilter(engine.rootObjects().first(),
                                                     &client, &app));
    auto* imeBlocker = new InputMethodBlocker(&client, &app);
    app.installEventFilter(imeBlocker);
    imeBlocker->refresh();
    return app.exec();
}
