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
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QTextStream>
#include <QTimer>

#include <cmath>
#include <memory>

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

QString SmokeCaptureDir() {
    return QString::fromUtf8(qgetenv("MI_E2EE_UI_SMOKE_CAPTURE_DIR")).trimmed();
}

void AppendSmokeLog(const QString& captureDir, const QString& message) {
    qCritical().noquote() << message;
    if (captureDir.isEmpty()) {
        return;
    }
    QDir dir;
    if (!dir.mkpath(captureDir)) {
        return;
    }
    QFile file(QDir(captureDir).filePath(QStringLiteral("ui-smoke.log")));
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream stream(&file);
    stream << message << Qt::endl;
}

bool SaveSmokeCapture(QQuickWindow* window, const QString& captureDir, const QString& name) {
    if (!window || captureDir.isEmpty()) {
        return false;
    }
    QDir dir;
    if (!dir.mkpath(captureDir)) {
        return false;
    }
    const QString fileName = QFileInfo(name).completeBaseName() + QStringLiteral(".png");
    QScreen* screen = window->screen();
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return false;
    }
    const QString path = QDir(captureDir).filePath(fileName);
    const QImage windowCapture = window->grabWindow();
    if (!windowCapture.isNull() && windowCapture.save(path)) {
        return true;
    }
    const QPixmap screenCapture = screen->grabWindow(window->winId());
    if (screenCapture.isNull()) {
        return false;
    }
    return screenCapture.save(path);
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
    const QString smokeCaptureDir = SmokeCaptureDir();
    QTimer smokeTimer;

    QQmlApplicationEngine engine;
    engine.setOutputWarningsToStandardError(true);
    mi::client::ui::QuickClient client;
    engine.rootContext()->setContextProperty("clientBridge", &client);
    QObject::connect(&engine, &QQmlEngine::warnings, &app,
                     [smokeCaptureDir](const QList<QQmlError>& warnings) {
                         for (const auto& warning : warnings) {
                             AppendSmokeLog(smokeCaptureDir, warning.toString());
                         }
                     });
    if (!QFile::exists(QStringLiteral(":/mi/e2ee/ui/qml/Main.qml"))) {
        AppendSmokeLog(smokeCaptureDir,
                       QStringLiteral("Missing QML resource: qrc:/mi/e2ee/ui/qml/Main.qml"));
        return -1;
    }

    const QUrl url(QStringLiteral("qrc:/mi/e2ee/ui/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url, smokeCaptureDir](QObject* obj, const QUrl& objUrl) {
                         if (!obj && url == objUrl) {
                             AppendSmokeLog(smokeCaptureDir,
                                             QStringLiteral("Failed to create root object for %1")
                                                .arg(objUrl.toString()));
                             QCoreApplication::exit(-1);
                         }
                     }, Qt::QueuedConnection);
    AppendSmokeLog(smokeCaptureDir,
                   QStringLiteral("About to load QML root: %1").arg(url.toString()));
    engine.load(url);
    AppendSmokeLog(smokeCaptureDir,
                   QStringLiteral("QML root load returned for %1").arg(url.toString()));

    if (engine.rootObjects().isEmpty()) {
        AppendSmokeLog(smokeCaptureDir, QStringLiteral("QML rootObjects is empty after load"));
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
        AppendSmokeLog(smokeCaptureDir, QStringLiteral("UI smoke start"));
        const int requestedSmokeDuration = SmokeDurationMs();
        const bool smokeWithLogin = !smokeUser.isEmpty() && !smokePass.isEmpty();
        const int smokeDuration =
            (!smokeCaptureDir.isEmpty() && smokeWithLogin)
                ? qMax(requestedSmokeDuration, 6500)
                : requestedSmokeDuration;
        smokeTimer.setSingleShot(true);
        smokeTimer.start(smokeDuration);
        QObject::connect(&smokeTimer, &QTimer::timeout, &app, [&app, smokeCaptureDir]() {
            AppendSmokeLog(smokeCaptureDir, QStringLiteral("UI smoke timer reached; quitting"));
            QCoreApplication::quit();
        });
        if (smokeWithLogin) {
            const int preCaptureDelayMs = smokeCaptureDir.isEmpty()
                ? 0
                : qMin(1200, qMax(450, smokeDuration / 4));
            const int loginDelayMs = smokeCaptureDir.isEmpty()
                ? 0
                : preCaptureDelayMs + qMin(600, qMax(220, smokeDuration / 8));
            const int postLoginCaptureDelayMs = smokeCaptureDir.isEmpty()
                ? 0
                : qMin(1200, qMax(500, smokeDuration / 4));
            QTimer::singleShot(0, &app, [&app, &client, &smokeTimer, smokeUser, smokePass,
                                         smokeConfig, smokeCaptureDir, window,
                                         loginDelayMs, preCaptureDelayMs,
                                         postLoginCaptureDelayMs]() {
                AppendSmokeLog(smokeCaptureDir,
                               QStringLiteral("UI smoke config: %1 (exists=%2)")
                                   .arg(smokeConfig,
                                        QFileInfo::exists(smokeConfig)
                                            ? QStringLiteral("yes")
                                            : QStringLiteral("no")));
                AppendSmokeLog(smokeCaptureDir, QStringLiteral("UI smoke init begin"));
                if (!client.init(smokeConfig)) {
                    const QString initError = client.lastError().trimmed();
                    AppendSmokeLog(smokeCaptureDir,
                                   QStringLiteral("UI smoke client init failed: %1")
                                       .arg(initError.isEmpty()
                                                ? QStringLiteral("unknown error")
                                                : initError));
                    smokeTimer.stop();
                    QCoreApplication::exit(2);
                    return;
                }
                AppendSmokeLog(smokeCaptureDir, QStringLiteral("UI smoke init ok"));
                if (!smokeCaptureDir.isEmpty() && window) {
                    QTimer::singleShot(preCaptureDelayMs, window, [window, smokeCaptureDir]() {
                        AppendSmokeLog(smokeCaptureDir, QStringLiteral("UI smoke login capture begin"));
                        const bool saved =
                            SaveSmokeCapture(window, smokeCaptureDir, QStringLiteral("login"));
                        AppendSmokeLog(smokeCaptureDir,
                                       saved
                                           ? QStringLiteral("UI smoke login capture ok")
                                           : QStringLiteral("UI smoke login capture failed"));
                    });
                }
                QTimer::singleShot(loginDelayMs, &client, [&app, &client, &smokeTimer, smokeUser,
                                                           smokePass, smokeCaptureDir, window,
                                                           postLoginCaptureDelayMs]() {
                    AppendSmokeLog(smokeCaptureDir, QStringLiteral("UI smoke login begin"));
                    if (!client.login(smokeUser, smokePass)) {
                        const QString loginError = client.lastError().trimmed();
                        AppendSmokeLog(smokeCaptureDir,
                                       QStringLiteral("UI smoke login failed: %1")
                                           .arg(loginError.isEmpty()
                                                    ? QStringLiteral("unknown error")
                                                    : loginError));
                        smokeTimer.stop();
                        QCoreApplication::exit(3);
                        return;
                    }
                    AppendSmokeLog(smokeCaptureDir, QStringLiteral("UI smoke login ok"));
                    if (!smokeCaptureDir.isEmpty() && window) {
                        QPointer<QQuickWindow> smokeWindow(window);
                        const bool authMode =
                            smokeWindow ? smokeWindow->property("authMode").toBool() : true;
                        const int fallbackCaptureDelayMs =
                            qMin(1500, qMax(250, postLoginCaptureDelayMs));
                        auto postLoginDone = std::make_shared<bool>(false);
                        auto finishPostLoginCapture =
                            [smokeWindow, smokeCaptureDir, &smokeTimer, postLoginDone](
                                const QString& trigger) {
                                if (*postLoginDone) {
                                    return;
                                }
                                *postLoginDone = true;
                                AppendSmokeLog(smokeCaptureDir,
                                               QStringLiteral("UI smoke post-login capture begin "
                                                              "(trigger=%1)")
                                                   .arg(trigger));
                                const bool saved = SaveSmokeCapture(
                                    smokeWindow.data(), smokeCaptureDir,
                                    QStringLiteral("post-login"));
                                AppendSmokeLog(smokeCaptureDir,
                                               saved
                                                   ? QStringLiteral("UI smoke post-login capture ok")
                                                   : QStringLiteral("UI smoke post-login capture failed"));
                                smokeTimer.stop();
                                AppendSmokeLog(smokeCaptureDir,
                                               QStringLiteral("UI smoke login success; quitting"));
                                QCoreApplication::exit(0);
                            };
                        AppendSmokeLog(smokeCaptureDir,
                                       QStringLiteral("UI smoke post-login render wait "
                                                      "(authMode=%1, fallbackMs=%2)")
                                           .arg(authMode ? QStringLiteral("true")
                                                         : QStringLiteral("false"))
                                           .arg(fallbackCaptureDelayMs));
                        QMetaObject::Connection frameConnection;
                        frameConnection = QObject::connect(
                            window, &QQuickWindow::frameSwapped, &app,
                            [finishPostLoginCapture, &frameConnection]() mutable {
                                QObject::disconnect(frameConnection);
                                finishPostLoginCapture(QStringLiteral("frameSwapped"));
                            },
                            Qt::QueuedConnection);
                        QTimer::singleShot(fallbackCaptureDelayMs, &app,
                                           [finishPostLoginCapture, &frameConnection, postLoginDone]() mutable {
                            if (*postLoginDone) {
                                return;
                            }
                            QObject::disconnect(frameConnection);
                            finishPostLoginCapture(QStringLiteral("fallback"));
                        });
                        window->update();
                        return;
                    }
                    smokeTimer.stop();
                    AppendSmokeLog(smokeCaptureDir,
                                   QStringLiteral("UI smoke login success; quitting"));
                    QCoreApplication::exit(0);
                });
            });
        } else if (!smokeCaptureDir.isEmpty() && window) {
            const int captureDelayMs = qMin(300, qMax(100, smokeDuration / 6));
            QTimer::singleShot(captureDelayMs, window, [window, smokeCaptureDir]() {
                AppendSmokeLog(smokeCaptureDir, QStringLiteral("UI smoke window capture begin"));
                const bool saved = SaveSmokeCapture(window, smokeCaptureDir, QStringLiteral("window"));
                AppendSmokeLog(smokeCaptureDir,
                               saved
                                   ? QStringLiteral("UI smoke window capture ok")
                                   : QStringLiteral("UI smoke window capture failed"));
            });
        }
    }
    return app.exec();
}
