#include "UiIcons.h"

#include <QHash>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QPainter>
#include <QResource>
#include <QSvgRenderer>

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
    static QHash<QString, QPixmap> cache;
    const QString key =
        resolvedPath + QStringLiteral(":") + QString::number(size) + QStringLiteral(":") +
        QString::number(static_cast<quint32>(color.rgba()), 16);
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
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    {
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&p);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pm.rect(), color);
    }
    cache.insert(key, pm);
    return pm;
}

}  // namespace UiIcons
