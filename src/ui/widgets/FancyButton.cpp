#include "FancyButton.h"

#include "ui/Theme.h"
#include "ui/widgets/Icons.h"

#include <QEasingCurve>
#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPropertyAnimation>

namespace th = bcg::theme;

namespace {
constexpr int kIconSize = 18;
constexpr int kHeight = 44;
constexpr int kPadding = 20;
constexpr int kGap = 10;
} // namespace

FancyButton::FancyButton(const QString& text, const QString& icon, Variant variant,
                         QWidget* parent)
    : QPushButton(text, parent)
    , m_variant(variant)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    if (!icon.isEmpty()) {
        const QColor color = variant == Secondary ? th::kAccent : th::kAccentDark;
        m_icon = bcg::icons::pixmap(icon, color, kIconSize, devicePixelRatioF());
    }

    m_hoverAnim = new QPropertyAnimation(this, "hover", this);
    m_hoverAnim->setDuration(180);
    m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_pressAnim = new QPropertyAnimation(this, "press", this);
    m_pressAnim->setDuration(120);
    m_pressAnim->setEasingCurve(QEasingCurve::OutCubic);
}

QSize FancyButton::sizeHint() const
{
    int w = fontMetrics().horizontalAdvance(text()) + 2 * kPadding;
    if (!m_icon.isNull())
        w += kIconSize + kGap;
    return {w, kHeight};
}

void FancyButton::setHover(qreal v)
{
    m_hover = v;
    update();
}

void FancyButton::setPress(qreal v)
{
    m_press = v;
    update();
}

void FancyButton::animateTo(QPropertyAnimation* anim, qreal target)
{
    anim->stop();
    anim->setStartValue(anim->targetObject()->property(anim->propertyName()));
    anim->setEndValue(target);
    anim->start();
}

void FancyButton::enterEvent(QEnterEvent* e)
{
    animateTo(m_hoverAnim, 1.0);
    QPushButton::enterEvent(e);
}

void FancyButton::leaveEvent(QEvent* e)
{
    animateTo(m_hoverAnim, 0.0);
    QPushButton::leaveEvent(e);
}

void FancyButton::mousePressEvent(QMouseEvent* e)
{
    QPushButton::mousePressEvent(e);
    animateTo(m_pressAnim, 1.0);
}

void FancyButton::mouseReleaseEvent(QMouseEvent* e)
{
    QPushButton::mouseReleaseEvent(e);
    animateTo(m_pressAnim, 0.0);
}

void FancyButton::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::EnabledChange)
        update();
    QPushButton::changeEvent(e);
}

void FancyButton::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    if (!isEnabled())
        p.setOpacity(0.4);

    const qreal scale = 1.0 - 0.03 * m_press;
    p.translate(width() / 2.0, height() / 2.0);
    p.scale(scale, scale);
    p.translate(-width() / 2.0, -height() / 2.0);

    const QRectF body = QRectF(rect()).adjusted(3, 3, -3, -3);
    QColor textColor;
    p.setPen(Qt::NoPen);

    if (m_variant != Secondary) {
        const QColor base = m_variant == Danger ? th::kDanger : th::kAccent;
        const QColor light = m_variant == Danger ? QColor(0xFF, 0x85, 0x92) : th::kAccentLight;
        if (isEnabled()) {
            QColor glow = base;
            glow.setAlphaF(0.22f * static_cast<float>(m_hover));
            p.setBrush(glow);
            p.drawRoundedRect(body.adjusted(-3, -3, 3, 3), 13, 13);
        }
        p.setBrush(th::mix(base, light, m_hover));
        p.drawRoundedRect(body, 10, 10);
        textColor = th::kAccentDark;
    } else {
        p.setBrush(th::mix(th::kCard, th::kCardHover, m_hover));
        p.setPen(QPen(th::mix(th::kBorder, th::kAccent, m_hover), 1.2));
        p.drawRoundedRect(body, 10, 10);
        textColor = th::kText;
    }

    const int textWidth = fontMetrics().horizontalAdvance(text());
    const int iconWidth = m_icon.isNull() ? 0 : kIconSize + kGap;
    qreal x = (width() - (iconWidth + textWidth)) / 2.0;
    if (!m_icon.isNull()) {
        p.drawPixmap(QPointF(x, (height() - kIconSize) / 2.0), m_icon);
        x += iconWidth;
    }
    QFont f = font();
    f.setWeight(m_variant == Secondary ? QFont::Medium : QFont::DemiBold);
    p.setFont(f);
    p.setPen(textColor);
    p.drawText(QRectF(x, 0, textWidth + 2, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
}
