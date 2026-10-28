#include "SectionCard.h"

#include "ui/widgets/CardPaint.h"
#include "ui/widgets/Icons.h"

#include <QEasingCurve>
#include <QFont>
#include <QPropertyAnimation>
#include <QTimer>

namespace th = bcg::theme;

namespace {
constexpr int kPad = 22;
constexpr int kBadge = 46;
constexpr int kIconSize = 24;
constexpr int kArrowSize = 20;
} // namespace

SectionCard::SectionCard(const QString& icon, const QString& title, const QString& description,
                         QWidget* parent)
    : QAbstractButton(parent)
    , m_title(title)
    , m_description(description)
{
    setAccessibleName(title);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setMinimumHeight(sizeHint().height());
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const qreal dpr = devicePixelRatioF();
    m_icon = bcg::icons::pixmap(icon, th::kAccent, kIconSize, dpr);
    m_arrow = bcg::icons::pixmap(QStringLiteral("arrow"), th::kAccent, kArrowSize, dpr);

    m_hoverAnim = new QPropertyAnimation(this, "hover", this);
    m_hoverAnim->setDuration(220);
    m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_pressAnim = new QPropertyAnimation(this, "press", this);
    m_pressAnim->setDuration(120);
    m_pressAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_revealAnim = new QPropertyAnimation(this, "reveal", this);
    m_revealAnim->setDuration(620);
    m_revealAnim->setEasingCurve(QEasingCurve::OutQuart);
}

void SectionCard::setReveal(qreal v)
{
    m_reveal = v;
    update();
}

void SectionCard::playEntrance(int delayMs)
{
    m_revealAnim->stop();
    setReveal(0.0);
    m_revealAnim->setStartValue(0.0);
    m_revealAnim->setEndValue(1.0);
    QTimer::singleShot(delayMs, this, [this] { m_revealAnim->start(); });
}

void SectionCard::setHover(qreal v)
{
    m_hover = v;
    update();
}

void SectionCard::setPress(qreal v)
{
    m_press = v;
    update();
}

void SectionCard::animateTo(QPropertyAnimation* anim, qreal target)
{
    anim->stop();
    anim->setStartValue(anim->targetObject()->property(anim->propertyName()));
    anim->setEndValue(target);
    anim->start();
}

void SectionCard::enterEvent(QEnterEvent* e)
{
    animateTo(m_hoverAnim, 1.0);
    QAbstractButton::enterEvent(e);
}

void SectionCard::leaveEvent(QEvent* e)
{
    animateTo(m_hoverAnim, 0.0);
    QAbstractButton::leaveEvent(e);
}

void SectionCard::mousePressEvent(QMouseEvent* e)
{
    QAbstractButton::mousePressEvent(e);
    animateTo(m_pressAnim, 1.0);
}

void SectionCard::mouseReleaseEvent(QMouseEvent* e)
{
    QAbstractButton::mouseReleaseEvent(e);
    animateTo(m_pressAnim, 0.0);
}

void SectionCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setOpacity(m_reveal);
    p.translate(0, (1.0 - m_reveal) * 22.0);

    // The card lifts a few pixels on hover and squishes slightly when pressed.
    const qreal scale = 1.0 - 0.015 * m_press;
    p.translate(width() / 2.0, height() / 2.0 - 3.0 * m_hover);
    p.scale(scale, scale);
    p.translate(-width() / 2.0, -height() / 2.0);

    const QRectF body = QRectF(rect()).adjusted(2, 4, -2, -4);
    bcg::paintCard(p, body, m_hover, 16.0);

    QColor badge = th::kAccent;
    badge.setAlphaF(static_cast<float>(0.12 + 0.10 * m_hover));
    p.setPen(Qt::NoPen);
    p.setBrush(badge);
    const QRectF badgeRect(body.left() + kPad, body.top() + kPad, kBadge, kBadge);
    p.drawRoundedRect(badgeRect, 13, 13);
    p.drawPixmap(QPointF(badgeRect.center().x() - kIconSize / 2.0,
                         badgeRect.center().y() - kIconSize / 2.0),
                 m_icon);

    // Arrow slides to the right and fades in.
    p.setOpacity(0.35 + 0.65 * m_hover);
    p.drawPixmap(QPointF(body.right() - kPad - kArrowSize + 6.0 * m_hover, body.top() + kPad + 13),
                 m_arrow);
    p.setOpacity(1.0);

    QFont titleFont = font();
    titleFont.setPixelSize(17);
    titleFont.setWeight(QFont::DemiBold);
    p.setFont(titleFont);
    p.setPen(th::kText);
    const qreal textLeft = body.left() + kPad;
    const qreal titleTop = badgeRect.bottom() + 14;
    p.drawText(QRectF(textLeft, titleTop, body.width() - 2 * kPad, 24), Qt::AlignLeft | Qt::AlignVCenter,
               m_title);

    QFont descFont = font();
    descFont.setPixelSize(13);
    p.setFont(descFont);
    p.setPen(th::kMuted);
    p.drawText(QRectF(textLeft, titleTop + 26, body.width() - 2 * kPad, body.bottom() - titleTop - 30),
               Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_description);
}
