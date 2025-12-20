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

namespace UiIcons {

QPixmap TintedSvg(const QString &resourcePath, int size, const QColor &color) {
    EnsureUiResources();
    const QString resolvedPath = ResolveSvgPath(resourcePath);
    if (!QFile::exists(resolvedPath)) {
        return {};
    }
    qreal dpr = 1.0;
    if (auto *screen = QGuiApplication::primaryScreen()) {
        dpr = screen->devicePixelRatio();
    }
    const int pixelSize = qMax(1, static_cast<int>(std::lround(size * dpr)));
    static QHash<QString, QPixmap> cache;
    const QString key =
        resolvedPath + QStringLiteral(":") + QString::number(size) + QStringLiteral(":") +
        QString::number(static_cast<quint32>(color.rgba()), 16) + QStringLiteral(":") +
        QString::number(pixelSize);
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
    pm.fill(Qt::transparent);
    pm.setDevicePixelRatio(dpr);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const qreal padRatio = pixelSize <= 20 ? 0.14 : (pixelSize <= 28 ? 0.10 : 0.06);
        const qreal pad = qMax(1.0, pixelSize * padRatio);
        const qreal side = qMax<qreal>(1.0, pixelSize - pad * 2.0);
        renderer.render(&p, QRectF(pad, pad, side, side));
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pm.rect(), color);
    }
    cache.insert(key, pm);
    return pm;
}

}  // namespace UiIcons
