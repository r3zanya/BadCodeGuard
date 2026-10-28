#include "SegmentedTabs.h"

#include "ui/Theme.h"

#include <QEasingCurve>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>

namespace th = bcg::theme;

namespace {
constexpr int kHeight = 44;
constexpr int kPad = 4;
constexpr int kTextPad = 22;
} // namespace

SegmentedTabs::SegmentedTabs(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(kHeight);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_pillAnim = new QPropertyAnimation(this, "pill", this);
    m_pillAnim->setDuration(300);
    m_pillAnim->setEasingCurve(QEasingCurve::OutCubic);
}

void SegmentedTabs::addTab(const QString& text)
{
    m_tabs << text;
    m_pill = tabRect(m_current);
    updateGeometry();
    update();
}

QSize SegmentedTabs::sizeHint() const
{
    qreal width = 2 * kPad;
    for (int i = 0; i < m_tabs.size(); ++i)
        width += fontMetrics().horizontalAdvance(m_tabs[i]) + 2 * kTextPad;
    return {static_cast<int>(width), kHeight};
}

QRectF SegmentedTabs::tabRect(int index) const
{
    qreal x = kPad;
    for (int i = 0; i < m_tabs.size(); ++i) {
        const qreal w = fontMetrics().horizontalAdvance(m_tabs[i]) + 2 * kTextPad;
        if (i == index)
            return QRectF(x, kPad, w, kHeight - 2 * kPad);
        x += w;
    }
    return {};
}

int SegmentedTabs::tabAt(const QPoint& pos) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (tabRect(i).contains(pos))
            return i;
    }
    return -1;
}

void SegmentedTabs::setPill(const QRectF& r)
{
    m_pill = r;
    update();
}

void SegmentedTabs::setCurrentIndex(int index, bool animate)
{
    if (index < 0 || index >= m_tabs.size() || index == m_current)
        return;
    m_current = index;
    m_pillAnim->stop();
    if (animate) {
        m_pillAnim->setStartValue(m_pill);
        m_pillAnim->setEndValue(tabRect(index));
        m_pillAnim->start();
    } else {
        setPill(tabRect(index));
    }
    emit currentChanged(index);
}

void SegmentedTabs::mouseMoveEvent(QMouseEvent* e)
{
    const int index = tabAt(e->position().toPoint());
    if (index != m_hover) {
        m_hover = index;
        update();
    }
}

void SegmentedTabs::leaveEvent(QEvent*)
{
    m_hover = -1;
    update();
}

void SegmentedTabs::mousePressEvent(QMouseEvent* e)
{
    const int index = tabAt(e->position().toPoint());
    if (index >= 0)
        setCurrentIndex(index);
}

void SegmentedTabs::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.setPen(QPen(th::kBorder, 1.0));
    p.setBrush(th::kCard);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 13, 13);

    QColor pill = th::kAccent;
    pill.setAlphaF(0.16f);
    p.setPen(QPen(th::mix(th::kBorder, th::kAccent, 0.55), 1.0));
    p.setBrush(pill);
    p.drawRoundedRect(m_pill, 10, 10);

    for (int i = 0; i < m_tabs.size(); ++i) {
        const QRectF r = tabRect(i);
        // Colour follows how much of the pill covers this tab.
        const qreal overlap = qBound(0.0, r.intersected(m_pill).width() / r.width(), 1.0);
        QColor c = th::mix(th::kMuted, th::kText, i == m_hover ? 1.0 : 0.0);
        c = th::mix(c, th::kAccent, overlap);
        p.setPen(c);
        p.drawText(r, Qt::AlignCenter, m_tabs[i]);
    }
}
