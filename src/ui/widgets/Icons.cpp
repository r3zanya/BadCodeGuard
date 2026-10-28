#include "Icons.h"

#include <QHash>
#include <QPainter>
#include <QSvgRenderer>

namespace bcg::icons {

namespace {

// Shapes live on a 24x24 grid and are drawn as strokes.
const QHash<QString, QString>& shapes()
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("shield"),
         QStringLiteral("<path d='M12 3 L19.5 6 V11.5 C19.5 16 16.3 19.4 12 21 C7.7 19.4 4.5 16 4.5 "
                        "11.5 V6 Z'/><path d='M9 12 L11.2 14.2 L15.2 10'/>")},
        {QStringLiteral("scan"),
         QStringLiteral("<circle cx='11' cy='11' r='6.5'/><path d='M16 16 L20.5 20.5'/>")},
        {QStringLiteral("quarantine"),
         QStringLiteral("<rect x='3.5' y='4' width='17' height='4.5' rx='1.2'/>"
                        "<path d='M5 8.5 V18 A1.5 1.5 0 0 0 6.5 19.5 H17.5 A1.5 1.5 0 0 0 19 18 "
                        "V8.5'/><path d='M10 12.5 H14'/>")},
        {QStringLiteral("settings"),
         QStringLiteral("<path d='M4 7 H13 M17 7 H20 M4 12 H7 M11 12 H20 M4 17 H15 M19 17 H20'/>"
                        "<circle cx='15' cy='7' r='2'/><circle cx='9' cy='12' r='2'/>"
                        "<circle cx='17' cy='17' r='2'/>")},
        {QStringLiteral("bolt"),
         QStringLiteral("<path d='M13 3 L5.5 13.5 H11.5 L10.5 21 L18.5 10 H12.5 Z'/>")},
        {QStringLiteral("drive"),
         QStringLiteral("<rect x='3.5' y='13' width='17' height='6.5' rx='2'/>"
                        "<path d='M5.5 13 L7.5 5 H16.5 L18.5 13'/><path d='M7.5 16.2 H7.6'/>")},
        {QStringLiteral("folder"),
         QStringLiteral("<path d='M3.5 7 A1.5 1.5 0 0 1 5 5.5 H9.2 L11.2 7.7 H19 A1.5 1.5 0 0 1 "
                        "20.5 9.2 V17.5 A1.5 1.5 0 0 1 19 19 H5 A1.5 1.5 0 0 1 3.5 17.5 Z'/>")},
        {QStringLiteral("chip"),
         QStringLiteral("<rect x='6' y='6' width='12' height='12' rx='2'/><rect x='9.5' y='9.5' width='5' height='5' rx='.6'/>"
                        "<path d='M9 3 V6 M15 3 V6 M9 18 V21 M15 18 V21 M3 9 H6 M3 15 H6 M18 9 H21 M18 15 H21'/>")},
        {QStringLiteral("gpu"),
         QStringLiteral("<rect x='2.5' y='6.5' width='19' height='9.5' rx='1.6'/><circle cx='9' cy='11.2' r='2.6'/>"
                        "<path d='M14.5 9.5 H18.5 M14.5 12.5 H18.5 M5.5 16 V19 M9 16 V19'/>")},
        {QStringLiteral("ram"),
         QStringLiteral("<rect x='2.5' y='7.5' width='19' height='8.5' rx='1.3'/><path d='M6.5 11 V13 M10.5 11 V13 M14.5 11 V13 M18 11 V13 "
                        "M5.5 16 V19 M9.5 16 V19 M14.5 16 V19 M18.5 16 V19'/>")},
        {QStringLiteral("vram"),
         QStringLiteral("<path d='M12 3.5 L21 8 L12 12.5 L3 8 Z'/><path d='M3 12 L12 16.5 L21 12'/><path d='M3 16 L12 20.5 L21 16'/>")},
        {QStringLiteral("storage"),
         QStringLiteral("<ellipse cx='12' cy='6' rx='8' ry='3'/><path d='M4 6 V18 C4 19.7 7.6 21 12 21 C16.4 21 20 19.7 20 18 V6'/>"
                        "<path d='M4 12 C4 13.7 7.6 15 12 15 C16.4 15 20 13.7 20 12'/>")},
        {QStringLiteral("device"),
         QStringLiteral("<rect x='3' y='4' width='18' height='12' rx='1.8'/><path d='M8 20 H16 M12 16 V20'/>")},
        {QStringLiteral("download"),
         QStringLiteral("<path d='M12 4 V15 M7.5 10.5 L12 15 L16.5 10.5 M4.5 19.5 H19.5'/>")},
        {QStringLiteral("copy"),
         QStringLiteral("<rect x='8.5' y='8.5' width='11' height='11' rx='2'/>"
                        "<path d='M15.5 8.5 V6.5 A2 2 0 0 0 13.5 4.5 H6.5 A2 2 0 0 0 4.5 6.5 V13.5 A2 2 0 0 0 6.5 15.5 H8.5'/>")},
        {QStringLiteral("gauge"),
         QStringLiteral("<path d='M4.6 17 A8.5 8.5 0 1 1 19.4 17'/><path d='M12 13 L16 8.5'/><circle cx='12' cy='13' r='1'/>")},
        {QStringLiteral("trash"),
         QStringLiteral("<path d='M4.5 7 H19.5 M9.5 7 V4.5 H14.5 V7 M6.5 7 L7.5 19.5 H16.5 L17.5 7 M10 11 V16 M14 11 V16'/>")},
        {QStringLiteral("recover"),
         QStringLiteral("<path d='M20 11 A8 8 0 1 0 18.2 16.6'/><path d='M20.5 5 V11 H14.5'/>")},
        {QStringLiteral("package"),
         QStringLiteral("<path d='M12 3 L20 7.5 V16.5 L12 21 L4 16.5 V7.5 Z'/><path d='M4 7.5 L12 12 L20 7.5 M12 12 V21'/>")},
        {QStringLiteral("info"),
         QStringLiteral("<circle cx='12' cy='12' r='8.5'/><path d='M12 11 V16.5 M12 7.9 H12.01'/>")},
        {QStringLiteral("window"),
         QStringLiteral("<rect x='3.5' y='3.5' width='7.5' height='7.5' rx='.9'/><rect x='13' y='3.5' width='7.5' height='7.5' rx='.9'/>"
                        "<rect x='3.5' y='13' width='7.5' height='7.5' rx='.9'/><rect x='13' y='13' width='7.5' height='7.5' rx='.9'/>")},
        {QStringLiteral("lock"),
         QStringLiteral("<rect x='5' y='10.5' width='14' height='10' rx='2'/><path d='M8 10.5 V7.5 A4 4 0 0 1 16 7.5 V10.5'/>")},
        {QStringLiteral("warning"),
         QStringLiteral("<path d='M12 4 L21 19.5 H3 Z'/><path d='M12 10 V14.5 M12 17 H12.01'/>")},
        {QStringLiteral("back"), QStringLiteral("<path d='M19 12 H5 M11 6 L5 12 L11 18'/>")},
        {QStringLiteral("arrow"), QStringLiteral("<path d='M5 12 H19 M13 6 L19 12 L13 18'/>")},
        {QStringLiteral("check"), QStringLiteral("<path d='M5 12.5 L10 17.5 L19 7'/>")},
        {QStringLiteral("calendar"),
         QStringLiteral("<rect x='3.5' y='5' width='17' height='15' rx='2'/><path d='M3.5 10 H20.5 M8 3 V7 M16 3 V7'/>")},
        {QStringLiteral("key"),
         QStringLiteral("<circle cx='8' cy='15' r='3.5'/><path d='M10.5 12.5 L19 4 M15.5 7.5 L18 10 M17.5 5.5 L19.5 7.5'/>")},
        {QStringLiteral("hash"),
         QStringLiteral("<path d='M9.5 4 L7.5 20 M16.5 4 L14.5 20 M4 9 H20 M3.5 15 H19.5'/>")},
        {QStringLiteral("exclude"),
         QStringLiteral("<path d='M12 3 L19.5 6 V11.5 C19.5 16 16.3 19.4 12 21 C7.7 19.4 4.5 16 4.5 "
                        "11.5 V6 Z'/><path d='M8.2 15.8 L15.8 8.2'/>")},
        {QStringLiteral("plus"), QStringLiteral("<path d='M12 5 V19 M5 12 H19'/>")},
        {QStringLiteral("file"),
         QStringLiteral("<path d='M6.5 3.5 H13.5 L18 8 V19 A1.5 1.5 0 0 1 16.5 20.5 H7.5 A1.5 1.5 0 0 1 "
                        "6 19 V5 A1.5 1.5 0 0 1 7.5 3.5 Z'/><path d='M13.5 3.5 V8 H18'/>")},
        {QStringLiteral("cancel"), QStringLiteral("<path d='M6.5 6.5 L17.5 17.5 M17.5 6.5 L6.5 17.5'/>")},
    };
    return map;
}

} // namespace

QPixmap pixmap(const QString& name, const QColor& color, int size, qreal dpr)
{
    const QString svg =
        QStringLiteral("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' "
                       "stroke='%1' stroke-width='1.8' stroke-linecap='round' "
                       "stroke-linejoin='round'>%2</svg>")
            .arg(color.name(QColor::HexRgb), shapes().value(name));

    QSvgRenderer renderer(svg.toUtf8());
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    renderer.render(&p, QRectF(0, 0, size, size));
    return pm;
}

} // namespace bcg::icons
