#include <QCoreApplication>
#include <QGuiApplication>
#include <QEvent>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QPointer>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#include <imm.h>
#ifdef _MSC_VER
#pragma comment(lib, "Imm32.lib")
#endif
#endif

#include "quick_client.h"
#include "common/UiRuntimePaths.h"

namespace {

bool EnvFlagEnabled(const char *name) {
    const QByteArray value = qgetenv(name).trimmed().toLower();
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

int SmokeDurationMs() {
    bool ok = false;
    const int value = qEnvironmentVariableIntValue("MI_E2EE_UI_SMOKE_MS", &ok);
    if (ok && value > 0) {
        return value;
    }
    return 2000;
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
                startNativeMove();
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
    void startNativeMove() {
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
                applyRoundedRegion();
                break;
            default:
                break;
        }
        return false;
    }

private:
    void applyRoundedRegion() {
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
        HRGN region = CreateRoundRectRgn(0, 0, w + 1, h + 1, radius * 2, radius * 2);
        if (region) {
            SetWindowRgn(hwnd, region, TRUE);
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
            applyForWindow(window);
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
                applyForWindow(window);
            }
        }
        if (event->type() == QEvent::InputMethod) {
            if (client_->internalImeEnabled() && isTextInput(obj)) {
                return true;
            }
        }
        return QObject::eventFilter(obj, event);
    }

private:
    static bool isTextInput(QObject* obj) {
        if (!obj) {
            return false;
        }
        return obj->inherits("QQuickTextInput") ||
               obj->inherits("QQuickTextEdit") ||
               obj->inherits("QQuickTextArea");
    }

    void applyForWindow(QWindow* window) {
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

    const bool smokeMode = EnvFlagEnabled("MI_E2EE_UI_SMOKE");
    const QString smokeUser = QString::fromUtf8(qgetenv("MI_E2EE_UI_SMOKE_USER"));
    const QString smokePass = QString::fromUtf8(qgetenv("MI_E2EE_UI_SMOKE_PASS"));
    const QString smokeConfig = QString::fromUtf8(qgetenv("MI_E2EE_UI_SMOKE_CONFIG"));
    QTimer smokeTimer;

    QQmlApplicationEngine engine;
    mi::client::ui::QuickClient client;
    engine.rootContext()->setContextProperty("clientBridge", &client);
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
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
        window->installEventFilter(new AuthWindowDragFilter(window));
        window->installEventFilter(new WindowRoundFilter(window));
    }
    app.installEventFilter(new SecureClipboardFilter(engine.rootObjects().first(),
                                                     &client, &app));
    auto* imeBlocker = new InputMethodBlocker(&client, &app);
    app.installEventFilter(imeBlocker);
    imeBlocker->refresh();
    if (smokeMode) {
        smokeTimer.setSingleShot(true);
        smokeTimer.start(SmokeDurationMs());
        QObject::connect(&smokeTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        if (!smokeUser.isEmpty() && !smokePass.isEmpty()) {
            QTimer::singleShot(0, &app, [&client, &smokeTimer, smokeUser, smokePass, smokeConfig]() {
                if (!client.init(smokeConfig)) {
                    smokeTimer.stop();
                    QCoreApplication::exit(2);
                    return;
                }
                if (!client.login(smokeUser, smokePass)) {
                    smokeTimer.stop();
                    QCoreApplication::exit(3);
                    return;
                }
            });
        }
    }
    return app.exec();
}
