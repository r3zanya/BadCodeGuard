#include "NavHighlight.h"

#include "ui/Theme.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPropertyAnimation>

namespace th = bcg::theme;

NavHighlight::NavHighlight(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_anim = new QPropertyAnimation(this, "geometry", this);
    m_anim->setDuration(480);
    m_anim->setEasingCurve(QEasingCurve::OutQuart);
    hide();
}

void NavHighlight::moveTo(const QRect& target, bool animate)
{
    const QRect rect = target.adjusted(0, 2, 0, -2);
    m_anim->stop();
    if (!m_placed || !animate) {
        setGeometry(rect);
        m_placed = true;
        show();
        lower();
        return;
    }
    m_anim->setStartValue(geometry());
    m_anim->setEndValue(rect);
    m_anim->start();
}

void NavHighlight::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(th::kCardHover);
    p.drawRoundedRect(QRectF(rect()), 10, 10);

    p.setBrush(th::kAccent);
    p.drawRoundedRect(QRectF(0, (height() - 20) / 2.0, 3, 20), 1.5, 1.5);
}
