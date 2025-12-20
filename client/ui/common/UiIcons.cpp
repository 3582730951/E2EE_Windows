#include "UiIcons.h"

#include <QHash>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIODevice>
#include <QPainter>
#include <QResource>
#include <QScreen>
#include <QSvgRenderer>
#include <QWindow>

#include <cmath>
#include <mutex>

static void InitUiResources() {
    Q_INIT_RESOURCE(ui_resources);
}

static void EnsureUiResources() {
    static std::once_flag once;
    std::call_once(once, InitUiResources);
}

static QString ResolveSvgPath(const QString &resourcePath) {
    const QString trimmed = resourcePath.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    if (QFile::exists(trimmed)) {
        return trimmed;
    }

    const QFileInfo info(trimmed);
    const QString fileName = info.fileName();
    if (fileName.isEmpty()) {
        return trimmed;
    }
    const QString baseDir = QCoreApplication::applicationDirPath();
    const QString sameDir = baseDir + QStringLiteral("/") + fileName;
    if (QFile::exists(sameDir)) {
        return sameDir;
    }
    const QString iconsDir = baseDir + QStringLiteral("/icons/") + fileName;
    if (QFile::exists(iconsDir)) {
        return iconsDir;
    }
    return trimmed;
}

static qreal DefaultDpr() {
    if (auto *window = QGuiApplication::focusWindow()) {
        if (auto *screen = window->screen()) {
            return screen->devicePixelRatio();
        }
    }
    if (auto *screen = QGuiApplication::primaryScreen()) {
        return screen->devicePixelRatio();
    }
    return 1.0;
}

static qreal NormalizeDpr(qreal dpr) {
    if (std::isfinite(dpr) && dpr > 0.0) {
        return dpr;
    }
    return DefaultDpr();
}

static int DprKey(qreal dpr) {
    return static_cast<int>(std::lround(dpr * 1000.0));
}

namespace UiIcons {

QPixmap TintedSvg(const QString &resourcePath, int size, const QColor &color, qreal dpr) {
    EnsureUiResources();
    const QString resolvedPath = ResolveSvgPath(resourcePath);
    if (!QFile::exists(resolvedPath)) {
        return {};
    }
    const qreal effectiveDpr = NormalizeDpr(dpr);
    const int logicalSize = qMax(1, size);
    const int pixelSize =
        qMax(1, static_cast<int>(std::ceil(logicalSize * effectiveDpr)));
    const qreal renderSize = pixelSize / effectiveDpr;
    static QHash<QString, QPixmap> cache;
    const QString key =
        resolvedPath + QStringLiteral(":") + QString::number(size) + QStringLiteral(":") +
        QString::number(static_cast<quint32>(color.rgba()), 16) + QStringLiteral(":") +
        QString::number(pixelSize) + QStringLiteral(":") + QString::number(DprKey(effectiveDpr));
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    QFile svgFile(resolvedPath);
    if (!svgFile.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QByteArray svgData = svgFile.readAll();

    QSvgRenderer renderer(svgData);
    if (!renderer.isValid()) {
        return {};
    }
    QPixmap pm(pixelSize, pixelSize);
    pm.setDevicePixelRatio(effectiveDpr);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const qreal padRatio =
            logicalSize <= 20 ? 0.04 : (logicalSize <= 28 ? 0.03 : 0.02);
        const qreal pad = qMax(1.0, logicalSize * padRatio);
        const qreal side = qMax<qreal>(1.0, logicalSize - pad * 2.0);
        const qreal extra = qMax<qreal>(0.0, (renderSize - logicalSize) * 0.5);
        renderer.render(&p, QRectF(extra + pad, extra + pad, side, side));
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pm.rect(), color);
    }
    cache.insert(key, pm);
    return pm;
}

QPixmap TintedSvg(const QString &resourcePath, int size, const QColor &color) {
    return TintedSvg(resourcePath, size, color, 0.0);
}

}  // namespace UiIcons
