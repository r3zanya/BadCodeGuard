#include "IconButton.h"

#include "ui/widgets/CardPaint.h"
#include "ui/widgets/Icons.h"

#include <QEasingCurve>
#include <QEvent>
#include <QPainter>
#include <QPropertyAnimation>

namespace th = bcg::theme;

namespace {
constexpr int kIconSize = 20;
}

IconButton::IconButton(const QString& icon, const QString& toolTip, QWidget* parent)
    : QAbstractButton(parent)
{
    setToolTip(toolTip);
    setAccessibleName(toolTip);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setFixedSize(sizeHint());

    const qreal dpr = devicePixelRatioF();
    m_muted = bcg::icons::pixmap(icon, th::kText, kIconSize, dpr);
    m_accent = bcg::icons::pixmap(icon, th::kAccent, kIconSize, dpr);

    m_hoverAnim = new QPropertyAnimation(this, "hover", this);
    m_hoverAnim->setDuration(170);
    m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_pressAnim = new QPropertyAnimation(this, "press", this);
    m_pressAnim->setDuration(110);
    m_pressAnim->setEasingCurve(QEasingCurve::OutCubic);
}

void IconButton::setHover(qreal v)
{
    m_hover = v;
    update();
}

void IconButton::setPress(qreal v)
{
    m_press = v;
    update();
}

void IconButton::animateTo(QPropertyAnimation* anim, qreal target)
{
    anim->stop();
    anim->setStartValue(anim->targetObject()->property(anim->propertyName()));
    anim->setEndValue(target);
    anim->start();
}

void IconButton::enterEvent(QEnterEvent* e)
{
    animateTo(m_hoverAnim, 1.0);
    QAbstractButton::enterEvent(e);
}

void IconButton::leaveEvent(QEvent* e)
{
    animateTo(m_hoverAnim, 0.0);
    QAbstractButton::leaveEvent(e);
}

void IconButton::mousePressEvent(QMouseEvent* e)
{
    QAbstractButton::mousePressEvent(e);
    animateTo(m_pressAnim, 1.0);
}

void IconButton::mouseReleaseEvent(QMouseEvent* e)
{
    QAbstractButton::mouseReleaseEvent(e);
    animateTo(m_pressAnim, 0.0);
}

void IconButton::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::EnabledChange)
        update();
    QAbstractButton::changeEvent(e);
}

void IconButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    if (!isEnabled())
        p.setOpacity(0.4);

    const qreal scale = 1.0 - 0.06 * m_press;
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(scale, scale);
    p.translate(-width() / 2.0, -height() / 2.0);

    bcg::paintCard(p, QRectF(rect()).adjusted(2, 2, -2, -2), m_hover, 11.0);

    const QPointF pos((width() - kIconSize) / 2.0, (height() - kIconSize) / 2.0);
    p.setOpacity(p.opacity() * (1.0 - m_hover));
    p.drawPixmap(pos, m_muted);
    p.setOpacity(isEnabled() ? m_hover : m_hover * 0.4);
    p.drawPixmap(pos, m_accent);
}
