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

QString SmokeScene() {
    const QString raw = QString::fromUtf8(qgetenv("MI_E2EE_UI_SMOKE_SCENE")).trimmed().toLower();
    if (raw == QStringLiteral("auth_login")) {
        return QStringLiteral("login");
    }
    if (raw == QStringLiteral("chat_list") ||
        raw == QStringLiteral("chat_detail") ||
        raw == QStringLiteral("settings_home") ||
        raw == QStringLiteral("calls_home")) {
        return QStringLiteral("post_login");
    }
    if (raw == QStringLiteral("chat_list_light")) {
        return QStringLiteral("post_login_light");
    }
    if (raw == QStringLiteral("security_center")) {
        return QStringLiteral("security_center");
    }
    return raw;
}

QString SmokeLocale() {
    return QString::fromUtf8(qgetenv("MI_E2EE_UI_SMOKE_LOCALE")).trimmed();
}

QString SmokeTheme() {
    return QString::fromUtf8(qgetenv("MI_E2EE_UI_SMOKE_THEME")).trimmed().toLower();
}

double SmokeScale() {
    const QString value = QString::fromUtf8(qgetenv("MI_E2EE_UI_SMOKE_SCALE")).trimmed();
    bool ok = false;
    const double parsed = value.toDouble(&ok);
    if (ok && parsed > 0.0) {
        return parsed;
    }
    return 1.0;
}

QString SmokeCaptureNameForScene(const QString& scene) {
    if (scene == QStringLiteral("login")) {
        return QStringLiteral("login");
    }
    if (scene == QStringLiteral("security_center")) {
        return QStringLiteral("security-center");
    }
    if (scene == QStringLiteral("post_login_light")) {
        return QStringLiteral("post-login-light");
    }
    return QStringLiteral("post-login");
}

QString SmokeCapturePath(const QString& captureDir, const QString& name) {
    return QDir(captureDir).filePath(
        QFileInfo(name).completeBaseName() + QStringLiteral(".png"));
}

void AppendSmokeLog(const QString& captureDir, const QString& message);

bool SmokeCaptureExists(const QString& captureDir, const QString& name) {
    if (captureDir.isEmpty() || name.isEmpty()) {
        return false;
    }
    return QFileInfo::exists(SmokeCapturePath(captureDir, name));
}

int SmokeDurationFloorMs(const QString& scene) {
    if (scene == QStringLiteral("security_center")) {
        return 12000;
    }
    if (scene == QStringLiteral("post_login") ||
        scene == QStringLiteral("post_login_light")) {
        return 9000;
    }
    return 6500;
}

int SmokeCaptureExitCode(const QString& captureDir,
                         const QString& captureName,
                         bool saved,
                         bool requireInformative = false,
                         bool informative = true) {
    const bool captureExists = SmokeCaptureExists(captureDir, captureName);
    if (!captureExists && !captureDir.isEmpty() && !captureName.isEmpty()) {
        AppendSmokeLog(
            captureDir,
            QStringLiteral("UI smoke %1 capture file missing: %2")
                .arg(captureName, SmokeCapturePath(captureDir, captureName)));
    }
    if (requireInformative && !informative) {
        AppendSmokeLog(
            captureDir,
            QStringLiteral("UI smoke %1 capture not informative").arg(captureName));
    }
    return saved && captureExists && (!requireInformative || informative) ? 0 : 4;
}

QImage SmokeContentBoundsImage(const QImage& image);
bool IsInformativeSmokeImage(const QImage& image);

QSize SmokeViewportForScene(const QString& scene) {
    if (scene == QStringLiteral("login")) {
        return QSize(840, 620);
    }
    return QSize(900, 620);
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
    QScreen* screen = window->screen();
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return false;
    }
    const QString path = SmokeCapturePath(captureDir, name);
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

bool SaveSmokeImage(const QImage& image, const QString& captureDir, const QString& name) {
    if (image.isNull() || captureDir.isEmpty()) {
        return false;
    }
    QDir dir;
    if (!dir.mkpath(captureDir)) {
        return false;
    }
    return image.save(SmokeCapturePath(captureDir, name));
}

QImage SmokeContentBoundsImage(const QImage& image) {
    if (image.isNull()) {
        return {};
    }
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    if (argb.isNull()) {
        return {};
    }
    constexpr int kNearBlackThreshold = 8;
    int left = argb.width();
    int top = argb.height();
    int right = -1;
    int bottom = -1;
    for (int y = 0; y < argb.height(); ++y) {
        const QRgb* row =
            reinterpret_cast<const QRgb*>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x) {
            const QRgb pixel = row[x];
            if (qRed(pixel) <= kNearBlackThreshold &&
                qGreen(pixel) <= kNearBlackThreshold &&
                qBlue(pixel) <= kNearBlackThreshold) {
                continue;
            }
            left = std::min(left, x);
            top = std::min(top, y);
            right = std::max(right, x);
            bottom = std::max(bottom, y);
        }
    }
    if (right < left || bottom < top) {
        return {};
    }
    const QRect bounds(left, top, right - left + 1, bottom - top + 1);
    if (bounds.size() == argb.size()) {
        return argb;
    }
    return argb.copy(bounds);
}

bool IsInformativeSmokeImage(const QImage& image) {
    const QImage argb = SmokeContentBoundsImage(image);
    if (argb.isNull() || argb.width() < 64 || argb.height() < 64) {
        return false;
    }
    const int stepX = std::max(1, argb.width() / 24);
    const int stepY = std::max(1, argb.height() / 24);
    QSet<QRgb> samples;
    int minLuma = 255;
    int maxLuma = 0;
    for (int y = 0; y < argb.height(); y += stepY) {
        for (int x = 0; x < argb.width(); x += stepX) {
            const QRgb pixel = argb.pixel(x, y);
            samples.insert(pixel);
            const int luma = (qRed(pixel) * 30 + qGreen(pixel) * 59 + qBlue(pixel) * 11) / 100;
            minLuma = std::min(minLuma, luma);
            maxLuma = std::max(maxLuma, luma);
        }
    }
    return samples.size() >= 12 && (maxLuma - minLuma) >= 18;
}

void ForceSmokeViewport(QQuickWindow* window,
                        const QString& scene,
                        const QString& captureDir,
                        const QString& label) {
    if (!window) {
        return;
    }
    const QSize viewport = SmokeViewportForScene(scene);
    if (!viewport.isValid()) {
        return;
    }
    window->setProperty("width", viewport.width());
    window->setProperty("height", viewport.height());
    window->setProperty("minimumWidth", viewport.width());
    window->setProperty("minimumHeight", viewport.height());
    window->setProperty("maximumWidth", viewport.width());
    window->setProperty("maximumHeight", viewport.height());
    window->setMinimumSize(viewport);
    window->setMaximumSize(viewport);
    window->resize(viewport);
    AppendSmokeLog(
        captureDir,
        QStringLiteral("UI smoke viewport forced (%1=%2x%3)")
            .arg(label)
            .arg(viewport.width())
            .arg(viewport.height()));
}

#ifdef Q_OS_WIN
QImage CaptureSmokeWindowNative(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) {
        return {};
    }
    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return {};
    }
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (width <= 0 || height <= 0) {
        return {};
    }

    HDC screenDc = GetDC(nullptr);
    if (!screenDc) {
        return {};
    }
    HDC memoryDc = CreateCompatibleDC(screenDc);
    if (!memoryDc) {
        ReleaseDC(nullptr, screenDc);
        return {};
    }
    HBITMAP bitmap = CreateCompatibleBitmap(screenDc, width, height);
    if (!bitmap) {
        DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return {};
    }
    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
    HBRUSH backgroundBrush = CreateSolidBrush(RGB(248, 250, 252));
    const RECT paintRect{0, 0, width, height};

    const auto readBitmap = [&]() -> QImage {
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        std::vector<uchar> pixels(
            static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);
        const bool dibOk =
            GetDIBits(memoryDc, bitmap, 0, static_cast<UINT>(height), pixels.data(),
                      &bmi, DIB_RGB_COLORS) != 0;
        if (!dibOk) {
            return {};
        }
        for (size_t i = 0; i < pixels.size() / 4; ++i) {
            pixels[i * 4 + 3] = 0xFF;
        }
        QImage image(pixels.data(), width, height, QImage::Format_ARGB32);
        return image.copy();
    };

    auto prepareSurface = [&]() {
        FillRect(memoryDc, &paintRect, backgroundBrush);
    };

    QImage bestImage;

    prepareSurface();
    BOOL printOk = PrintWindow(hwnd, memoryDc, PW_RENDERFULLCONTENT);
    if (!printOk) {
        prepareSurface();
        printOk = PrintWindow(hwnd, memoryDc, 0);
    }
    if (printOk) {
        QImage printed = readBitmap();
        if (!printed.isNull()) {
            bestImage = printed;
            if (IsInformativeSmokeImage(printed)) {
                SelectObject(memoryDc, oldBitmap);
                DeleteObject(backgroundBrush);
                DeleteObject(bitmap);
                DeleteDC(memoryDc);
                ReleaseDC(nullptr, screenDc);
                return printed;
            }
        }
    }

    prepareSurface();
    const BOOL bltOk = BitBlt(
        memoryDc, 0, 0, width, height, screenDc, rect.left, rect.top,
        SRCCOPY | CAPTUREBLT);
    if (bltOk) {
        QImage fallback = readBitmap();
        if (!fallback.isNull()) {
            bestImage = fallback;
        }
    }

    SelectObject(memoryDc, oldBitmap);
    DeleteObject(backgroundBrush);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    return bestImage;
}

void ScheduleWindowsSmokeCaptureAndExit(HWND hwnd,
                                        const QString& captureDir,
                                        const QString& captureName,
                                        int delayMs) {
    std::thread([hwnd, captureDir, captureName, delayMs]() {
        constexpr int kAttempts = 4;
        QImage bestImage;
        bool informative = false;
        const QString label = captureName.isEmpty() ? QStringLiteral("post-login") : captureName;
        for (int attempt = 0; attempt < kAttempts; ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            AppendSmokeLog(
                captureDir,
                QStringLiteral("UI smoke %1 native capture attempt %2")
                    .arg(label)
                    .arg(attempt + 1));
            QImage image = CaptureSmokeWindowNative(hwnd);
            if (image.isNull()) {
                continue;
            }
            bestImage = image;
            informative = IsInformativeSmokeImage(image);
            if (informative) {
                break;
            }
        }
        const bool saved = SaveSmokeImage(bestImage, captureDir, label);
        AppendSmokeLog(captureDir,
                       saved ? QStringLiteral("UI smoke %1 native capture saved").arg(label)
                             : QStringLiteral("UI smoke %1 native capture failed").arg(label));
        const int exitCode =
            SmokeCaptureExitCode(captureDir, label, saved, true, informative);
        AppendSmokeLog(
            captureDir,
            exitCode == 0
                ? QStringLiteral("UI smoke %1 native capture complete; exiting 0")
                      .arg(label)
                : QStringLiteral("UI smoke %1 native capture failed; exiting %2")
                      .arg(label)
                      .arg(exitCode));
        ::ExitProcess(exitCode);
    }).detach();
}

QQuickWindow* FindVisibleTransientSmokeWindow(QQuickWindow* ownerWindow) {
    if (!ownerWindow) {
        return nullptr;
    }
    const auto windows = QGuiApplication::allWindows();
    for (QWindow* candidate : windows) {
        if (!candidate || candidate == ownerWindow || !candidate->isVisible()) {
            continue;
        }
        if (candidate->transientParent() != ownerWindow) {
            continue;
        }
        if (!(candidate->flags() & Qt::Window)) {
            continue;
        }
        if (auto* quickWindow = qobject_cast<QQuickWindow*>(candidate)) {
            return quickWindow;
        }
    }
    return nullptr;
}
#endif

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
    const QString smokeScene = SmokeScene();
    const QString smokeLocale = SmokeLocale();
    const QString smokeTheme = SmokeTheme();
    const double smokeScale = SmokeScale();
    QTimer smokeTimer;

    QQmlApplicationEngine engine;
    engine.setOutputWarningsToStandardError(true);
    mi::client::ui::QuickClient client;
    engine.rootContext()->setContextProperty("clientBridge", &client);
    engine.rootContext()->setContextProperty("uiSmokeMode", smokeMode);
    engine.rootContext()->setContextProperty("uiSmokeScene", smokeScene);
    engine.rootContext()->setContextProperty("uiSmokeLocale", smokeLocale);
    engine.rootContext()->setContextProperty("uiSmokeTheme", smokeTheme);
    engine.rootContext()->setContextProperty("uiSmokeScale", smokeScale);
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
    QObject* rootObject = engine.rootObjects().first();
    auto* window = qobject_cast<QQuickWindow*>(rootObject);
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
        const QString expectedCaptureName =
            smokeWithLogin ? SmokeCaptureNameForScene(smokeScene)
                           : QStringLiteral("window");
        if (!smokeCaptureDir.isEmpty() && !expectedCaptureName.isEmpty()) {
            QFile::remove(SmokeCapturePath(smokeCaptureDir, expectedCaptureName));
        }
        const int smokeDuration =
            (!smokeCaptureDir.isEmpty() && smokeWithLogin)
                ? qMax(requestedSmokeDuration, SmokeDurationFloorMs(smokeScene))
                : requestedSmokeDuration;
        smokeTimer.setSingleShot(true);
        smokeTimer.start(smokeDuration);
        QObject::connect(&smokeTimer, &QTimer::timeout, &app,
                         [smokeCaptureDir, expectedCaptureName]() {
            if (smokeCaptureDir.isEmpty() || expectedCaptureName.isEmpty()) {
                AppendSmokeLog(smokeCaptureDir,
                               QStringLiteral("UI smoke timer reached; quitting"));
                QCoreApplication::quit();
                return;
            }
            QString message =
                QStringLiteral("UI smoke timer reached before capture completion");
            message += QStringLiteral("; expected ")
                + SmokeCapturePath(smokeCaptureDir, expectedCaptureName);
            AppendSmokeLog(smokeCaptureDir, message);
            QCoreApplication::exit(5);
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
                                         rootObject, smokeScene,
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
                if (smokeScene == QStringLiteral("login")) {
                    const int loginOnlyDelayMs =
                        qMax(350, preCaptureDelayMs + 250);
                    QTimer::singleShot(loginOnlyDelayMs, &app, [&smokeTimer, smokeCaptureDir]() {
                        const int exitCode = smokeCaptureDir.isEmpty()
                            ? 0
                            : SmokeCaptureExitCode(
                                  smokeCaptureDir, QStringLiteral("login"),
                                  SmokeCaptureExists(
                                      smokeCaptureDir, QStringLiteral("login")));
                        smokeTimer.stop();
                        AppendSmokeLog(
                            smokeCaptureDir,
                            exitCode == 0
                                ? QStringLiteral("UI smoke login-only scene complete; exiting 0")
                                : QStringLiteral("UI smoke login-only capture missing or failed; exiting %1")
                                      .arg(exitCode));
                        QCoreApplication::exit(exitCode);
                    });
                    return;
                }
                QTimer::singleShot(loginDelayMs, &client, [&app, &client, &smokeTimer, smokeUser,
                                                           smokePass, smokeCaptureDir, window,
                                                           rootObject, smokeScene,
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
                        const bool securityCenterScene =
                            smokeScene == QStringLiteral("security_center");
                        const QString captureName = SmokeCaptureNameForScene(smokeScene);
                        const bool authMode =
                            smokeWindow ? smokeWindow->property("authMode").toBool() : true;
#ifdef Q_OS_WIN
                        const int maxPostLoginWaitMs =
                            qMin(4600, qMax(1800, postLoginCaptureDelayMs + 2200));
#else
                        const int maxPostLoginWaitMs =
                            qMin(2400, qMax(600, postLoginCaptureDelayMs + 700));
#endif
                        constexpr int kShellReadyPollMs = 120;
                        const int maxPostLoginPolls =
                            qMax(1, (maxPostLoginWaitMs + kShellReadyPollMs - 1) /
                                           kShellReadyPollMs);
                        auto postLoginDone = std::make_shared<bool>(false);
                        auto pollCount = std::make_shared<int>(0);
                        auto finishPostLoginCapture =
                            [smokeWindow, smokeCaptureDir, &smokeTimer, &app, rootObject,
                             postLoginDone, captureName, smokeScene,
                             postLoginCaptureDelayMs](
                                const QString& trigger, const bool shellReady) {
                                if (*postLoginDone) {
                                    return;
                                }
                                *postLoginDone = true;
                                AppendSmokeLog(
                                    smokeCaptureDir,
                                    QStringLiteral("UI smoke %1 capture begin "
                                                   "(trigger=%2, shellReady=%3)")
                                        .arg(captureName)
                                        .arg(trigger,
                                             shellReady ? QStringLiteral("true")
                                                        : QStringLiteral("false")));
#ifdef Q_OS_WIN
                                const bool securityCenterScene =
                                    smokeScene == QStringLiteral("security_center");
                                const HWND smokeHwnd =
                                    smokeWindow ? reinterpret_cast<HWND>(smokeWindow->winId())
                                                : nullptr;
                                if (securityCenterScene && smokeHwnd) {
                                    ForceSmokeViewport(
                                        smokeWindow.data(), smokeScene, smokeCaptureDir,
                                        QStringLiteral("main-window"));
                                    const bool opened = rootObject &&
                                        QMetaObject::invokeMethod(
                                            rootObject, "openShellSecurityCenter");
                                    AppendSmokeLog(
                                        smokeCaptureDir,
                                        opened
                                            ? QStringLiteral("UI smoke security center requested")
                                            : QStringLiteral("UI smoke security center request failed"));
                                    if (smokeWindow) {
                                        smokeWindow->update();
                                    }
                                    const int nativeCaptureDelayMs =
                                        qMin(3200, qMax(1200, postLoginCaptureDelayMs + 900));
                                    constexpr int kSecurityDialogPollMs = 120;
                                    const int maxSecurityDialogWaitMs =
                                        qMin(3600, qMax(1200, postLoginCaptureDelayMs + 1600));
                                    const int maxSecurityDialogPolls =
                                        qMax(1, (maxSecurityDialogWaitMs +
                                                 kSecurityDialogPollMs - 1) /
                                                        kSecurityDialogPollMs);
                                    auto dialogPollCount = std::make_shared<int>(0);
                                    auto* securityDialogTimer = new QTimer(&app);
                                    securityDialogTimer->setSingleShot(false);
                                    securityDialogTimer->setInterval(kSecurityDialogPollMs);
                                    QObject::connect(
                                        securityDialogTimer, &QTimer::timeout, &app,
                                        [smokeWindow, securityDialogTimer, dialogPollCount,
                                         maxSecurityDialogPolls, smokeCaptureDir,
                                         captureName, smokeScene, shellReady,
                                         nativeCaptureDelayMs,
                                         smokeHwnd, &smokeTimer]() mutable {
                                            *dialogPollCount += 1;
                                            QQuickWindow* dialogWindow =
                                                FindVisibleTransientSmokeWindow(
                                                    smokeWindow.data());
                                            if (!dialogWindow &&
                                                *dialogPollCount < maxSecurityDialogPolls) {
                                                return;
                                            }
                                            securityDialogTimer->stop();
                                            securityDialogTimer->deleteLater();
                                            const bool dialogVisible = dialogWindow != nullptr;
                                            if (dialogVisible) {
                                                ForceSmokeViewport(
                                                    dialogWindow, smokeScene,
                                                    smokeCaptureDir,
                                                    QStringLiteral("security-dialog"));
                                                dialogWindow->update();
                                                AppendSmokeLog(
                                                    smokeCaptureDir,
                                                    QStringLiteral("UI smoke security center "
                                                                   "dialog visible (poll=%1)")
                                                        .arg(*dialogPollCount));
                                            } else {
                                                AppendSmokeLog(
                                                    smokeCaptureDir,
                                                    QStringLiteral("UI smoke security center "
                                                                   "dialog not visible; using "
                                                                   "main window fallback"));
                                            }
                                            const HWND captureHwnd =
                                                dialogVisible
                                                    ? reinterpret_cast<HWND>(
                                                          dialogWindow->winId())
                                                    : smokeHwnd;
                                            smokeTimer.stop();
                                            AppendSmokeLog(
                                                smokeCaptureDir,
                                                QStringLiteral("UI smoke %1 native worker armed "
                                                               "(target=%2, shellReady=%3, "
                                                               "delayMs=%4)")
                                                    .arg(captureName)
                                                    .arg(dialogVisible
                                                             ? QStringLiteral(
                                                                   "security-dialog")
                                                             : QStringLiteral(
                                                                   "main-window-fallback"))
                                                    .arg(shellReady
                                                             ? QStringLiteral("true")
                                                             : QStringLiteral("false"))
                                                    .arg(nativeCaptureDelayMs));
                                            ScheduleWindowsSmokeCaptureAndExit(
                                                captureHwnd, smokeCaptureDir,
                                                captureName, nativeCaptureDelayMs);
                                        });
                                    securityDialogTimer->start();
                                    return;
                                }
                                if (smokeHwnd) {
                                    if (smokeWindow) {
                                        ForceSmokeViewport(
                                            smokeWindow.data(), smokeScene,
                                            smokeCaptureDir,
                                            QStringLiteral("main-window"));
                                        smokeWindow->update();
                                    }
                                    const int nativeCaptureDelayMs =
                                        qMin(3200, qMax(1500, postLoginCaptureDelayMs + 900));
                                    smokeTimer.stop();
                                    AppendSmokeLog(
                                        smokeCaptureDir,
                                        QStringLiteral("UI smoke %1 native worker armed "
                                                       "(shellReady=%2, delayMs=%3)")
                                            .arg(captureName)
                                            .arg(shellReady ? QStringLiteral("true")
                                                            : QStringLiteral("false"))
                                            .arg(nativeCaptureDelayMs));
                                    ScheduleWindowsSmokeCaptureAndExit(
                                        smokeHwnd, smokeCaptureDir, captureName,
                                        nativeCaptureDelayMs);
                                    return;
                                }
#endif
                                const auto saveAndQuit =
                                    [smokeWindow, smokeCaptureDir, &smokeTimer, captureName]() {
                                        const bool saved = SaveSmokeCapture(
                                            smokeWindow.data(), smokeCaptureDir, captureName);
                                        AppendSmokeLog(smokeCaptureDir,
                                                       saved
                                                           ? QStringLiteral("UI smoke %1 capture ok").arg(captureName)
                                                           : QStringLiteral("UI smoke %1 capture failed").arg(captureName));
                                        const int exitCode = SmokeCaptureExitCode(
                                            smokeCaptureDir, captureName, saved);
                                        smokeTimer.stop();
                                        AppendSmokeLog(
                                            smokeCaptureDir,
                                            exitCode == 0
                                                ? QStringLiteral("UI smoke %1 capture complete; exiting 0")
                                                      .arg(captureName)
                                                : QStringLiteral("UI smoke %1 capture missing or failed; exiting %2")
                                                      .arg(captureName)
                                                      .arg(exitCode));
                                        QCoreApplication::exit(exitCode);
                                    };
                                if (smokeScene == QStringLiteral("security_center")) {
                                    const bool opened = rootObject &&
                                        QMetaObject::invokeMethod(
                                            rootObject, "openShellSecurityCenter");
                                    AppendSmokeLog(
                                        smokeCaptureDir,
                                        opened
                                            ? QStringLiteral("UI smoke security center requested")
                                            : QStringLiteral("UI smoke security center request failed"));
                                    if (smokeWindow) {
                                        smokeWindow->update();
                                    }
                                    QTimer::singleShot(320, &app, saveAndQuit);
                                    return;
                                }
                                const bool saved = SaveSmokeCapture(
                                    smokeWindow.data(), smokeCaptureDir, captureName);
                                AppendSmokeLog(smokeCaptureDir,
                                               saved
                                                   ? QStringLiteral("UI smoke %1 capture ok").arg(captureName)
                                                   : QStringLiteral("UI smoke %1 capture failed").arg(captureName));
                                const int exitCode = SmokeCaptureExitCode(
                                    smokeCaptureDir, captureName, saved);
                                smokeTimer.stop();
                                AppendSmokeLog(
                                    smokeCaptureDir,
                                    exitCode == 0
                                        ? QStringLiteral("UI smoke %1 capture complete; exiting 0")
                                              .arg(captureName)
                                        : QStringLiteral("UI smoke %1 capture missing or failed; exiting %2")
                                              .arg(captureName)
                                              .arg(exitCode));
                                QCoreApplication::exit(exitCode);
                            };
                        AppendSmokeLog(smokeCaptureDir,
                                       QStringLiteral("UI smoke post-login ready wait "
                                                      "(authMode=%1, pollMs=%2, maxWaitMs=%3)")
                                           .arg(authMode ? QStringLiteral("true")
                                                         : QStringLiteral("false"))
                                           .arg(kShellReadyPollMs)
                                           .arg(maxPostLoginWaitMs));
                        auto* postLoginTimer = new QTimer(&app);
                        postLoginTimer->setSingleShot(false);
                        postLoginTimer->setInterval(kShellReadyPollMs);
                        QObject::connect(
                            postLoginTimer, &QTimer::timeout, &app,
                            [smokeWindow, postLoginTimer, pollCount, maxPostLoginPolls,
                             finishPostLoginCapture, postLoginDone,
                             smokeCaptureDir]() mutable {
                                if (*postLoginDone) {
                                    postLoginTimer->stop();
                                    postLoginTimer->deleteLater();
                                    return;
                                }
                                const bool shellReady =
                                    smokeWindow && smokeWindow->property("shellReady").toBool();
                                *pollCount += 1;
                                if (shellReady) {
                                    AppendSmokeLog(
                                        smokeCaptureDir,
                                        QStringLiteral("UI smoke post-login shell ready observed "
                                                       "(poll=%1); arming capture")
                                            .arg(*pollCount));
                                    postLoginTimer->stop();
                                    postLoginTimer->deleteLater();
                                    finishPostLoginCapture(QStringLiteral("shellReady"),
                                                           true);
                                    return;
                                }
                                if (*pollCount < maxPostLoginPolls) {
                                    return;
                                }
                                postLoginTimer->stop();
                                postLoginTimer->deleteLater();
                                finishPostLoginCapture(QStringLiteral("fallback"), false);
                            });
                        if (smokeWindow) {
                            smokeWindow->update();
                        }
                        postLoginTimer->start();
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
            QTimer::singleShot(captureDelayMs, window, [window, smokeCaptureDir, &smokeTimer]() {
                AppendSmokeLog(smokeCaptureDir, QStringLiteral("UI smoke window capture begin"));
                const bool saved = SaveSmokeCapture(window, smokeCaptureDir, QStringLiteral("window"));
                AppendSmokeLog(smokeCaptureDir,
                               saved
                                   ? QStringLiteral("UI smoke window capture ok")
                                   : QStringLiteral("UI smoke window capture failed"));
                const int exitCode = SmokeCaptureExitCode(
                    smokeCaptureDir, QStringLiteral("window"), saved);
                smokeTimer.stop();
                AppendSmokeLog(
                    smokeCaptureDir,
                    exitCode == 0
                        ? QStringLiteral("UI smoke window capture complete; exiting 0")
                        : QStringLiteral("UI smoke window capture missing or failed; exiting %1")
                              .arg(exitCode));
                QCoreApplication::exit(exitCode);
            });
        }
    }
    return app.exec();
}
