#pragma once

#include <QColor>
#include <QPixmap>
#include <QString>

namespace bcg::icons {

// Renders a built-in line icon ("shield", "scan", "quarantine", "settings",
// "bolt", "drive", "folder", "cancel") as a crisp pixmap of size x size
// logical pixels in the given colour.
QPixmap pixmap(const QString& name, const QColor& color, int size, qreal dpr = 1.0);

} // namespace bcg::icons
