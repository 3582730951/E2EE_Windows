#include "UiIcons.h"

#include <QHash>
#include <QPainter>
#include <QSvgRenderer>

namespace UiIcons {

QPixmap TintedSvg(const QString &resourcePath, int size, const QColor &color) {
    static QHash<QString, QPixmap> cache;
    const QString key =
        resourcePath + QStringLiteral(":") + QString::number(size) + QStringLiteral(":") +
        QString::number(static_cast<quint32>(color.rgba()), 16);
    const auto it = cache.constFind(key);
    if (it != cache.constEnd()) {
        return it.value();
    }

    QSvgRenderer renderer(resourcePath);
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

