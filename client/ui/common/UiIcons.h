// Shared icon rendering helpers (SVG -> tinted pixmap) for Widgets UI.
#pragma once

#include <QColor>
#include <QPixmap>
#include <QString>

namespace UiIcons {

QPixmap TintedSvg(const QString &resourcePath, int size, const QColor &color);
QPixmap TintedSvg(const QString &resourcePath, int size, const QColor &color, qreal dpr);

}  // namespace UiIcons
