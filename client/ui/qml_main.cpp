#include <QCoreApplication>
#include <QGuiApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QPointer>
#include <QQuickStyle>
#include <QQuickWindow>

#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "quick_client.h"
#include "common/UiRuntimePaths.h"

namespace {

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

}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QML_XHR_ALLOW_FILE_READ", "1");
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    UiRuntimePaths::Prepare(argv[0]);
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MI"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("mi-e2ee.local"));
    QCoreApplication::setApplicationName(QStringLiteral("MI E2EE Client"));

    QQmlApplicationEngine engine;
    mi::client::ui::QuickClient client;
    engine.rootContext()->setContextProperty("clientBridge", &client);

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
    return app.exec();
}
