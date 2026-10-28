#include "NavButton.h"

#include "ui/Theme.h"
#include "ui/widgets/Icons.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPropertyAnimation>

namespace th = bcg::theme;

namespace {
constexpr int kIconSize = 20;
}

NavButton::NavButton(const QString& text, const QString& icon, QWidget* parent)
    : QAbstractButton(parent)
{
    setText(text);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);

    const qreal dpr = devicePixelRatioF();
    m_iconMuted = bcg::icons::pixmap(icon, th::kMuted, kIconSize, dpr);
    m_iconAccent = bcg::icons::pixmap(icon, th::kAccent, kIconSize, dpr);

    m_hoverAnim = new QPropertyAnimation(this, "hover", this);
    m_hoverAnim->setDuration(180);
    m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_selectionAnim = new QPropertyAnimation(this, "selection", this);
    m_selectionAnim->setDuration(320);
    m_selectionAnim->setEasingCurve(QEasingCurve::OutCubic);

    connect(this, &QAbstractButton::toggled, this,
            [this](bool on) { animateTo(m_selectionAnim, on ? 1.0 : 0.0); });
}

void NavButton::setHover(qreal v)
{
    m_hover = v;
    update();
}

void NavButton::setSelection(qreal v)
{
    m_selection = v;
    update();
}

void NavButton::animateTo(QPropertyAnimation* anim, qreal target)
{
    anim->stop();
    anim->setStartValue(anim->targetObject()->property(anim->propertyName()));
    anim->setEndValue(target);
    anim->start();
}

void NavButton::enterEvent(QEnterEvent* e)
{
    animateTo(m_hoverAnim, 1.0);
    QAbstractButton::enterEvent(e);
}

void NavButton::leaveEvent(QEvent* e)
{
    animateTo(m_hoverAnim, 0.0);
    QAbstractButton::leaveEvent(e);
}

void NavButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // The selected plate and accent bar belong to NavHighlight; only the
    // hover glow is drawn here, fading out as the entry becomes selected.
    QColor bg = th::kCardHover;
    bg.setAlphaF(static_cast<float>(m_hover * 0.55 * (1.0 - m_selection)));
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(rect()).adjusted(0, 2, 0, -2), 10, 10);

    const QPointF iconPos(16, (height() - kIconSize) / 2.0);
    p.setOpacity(1.0 - m_selection);
    p.drawPixmap(iconPos, m_iconMuted);
    p.setOpacity(m_selection);
    p.drawPixmap(iconPos, m_iconAccent);
    p.setOpacity(1.0);

    p.setPen(th::mix(th::mix(th::kMuted, th::kText, m_hover), th::kAccent, m_selection));
    p.drawText(QRectF(48, 0, width() - 56, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
}
