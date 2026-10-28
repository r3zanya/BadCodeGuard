#include "ToggleSwitch.h"

#include "ui/Theme.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPropertyAnimation>
#include <QSignalBlocker>

namespace th = bcg::theme;

ToggleSwitch::ToggleSwitch(QWidget* parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setFixedSize(sizeHint());

    m_positionAnim = new QPropertyAnimation(this, "position", this);
    m_positionAnim->setDuration(240);
    m_positionAnim->setEasingCurve(QEasingCurve::OutBack);
    m_hoverAnim = new QPropertyAnimation(this, "hover", this);
    m_hoverAnim->setDuration(160);

    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        m_positionAnim->stop();
        m_positionAnim->setStartValue(m_position);
        m_positionAnim->setEndValue(on ? 1.0 : 0.0);
        m_positionAnim->start();
    });
}

void ToggleSwitch::setCheckedInstant(bool on)
{
    m_positionAnim->stop();
    {
        QSignalBlocker blocker(this);
        setChecked(on);
    }
    setPosition(on ? 1.0 : 0.0);
}

void ToggleSwitch::setPosition(qreal v)
{
    m_position = v;
    update();
}

void ToggleSwitch::setHover(qreal v)
{
    m_hover = v;
    update();
}

void ToggleSwitch::enterEvent(QEnterEvent* e)
{
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hover);
    m_hoverAnim->setEndValue(1.0);
    m_hoverAnim->start();
    QAbstractButton::enterEvent(e);
}

void ToggleSwitch::leaveEvent(QEvent* e)
{
    m_hoverAnim->stop();
    m_hoverAnim->setStartValue(m_hover);
    m_hoverAnim->setEndValue(0.0);
    m_hoverAnim->start();
    QAbstractButton::leaveEvent(e);
}

void ToggleSwitch::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // OutBack overshoots past 1.0; colours must stay in range.
    const qreal t = qBound(0.0, m_position, 1.0);
    const QColor off = th::mix(th::kTrackOff, th::kCardHover, m_hover);
    const QRectF track(1, 1, width() - 2, height() - 2);
    p.setPen(Qt::NoPen);
    p.setBrush(th::mix(off, th::kAccent, t));
    p.drawRoundedRect(track, track.height() / 2, track.height() / 2);

    // The knob stretches slightly while pressed.
    const qreal d = 20.0;
    const qreal stretch = isDown() ? 4.0 : 0.0;
    const qreal travel = width() - 8.0 - d;
    const qreal x = 4.0 + m_position * travel - stretch * t;
    p.setBrush(th::mix(QColor(0xB8, 0xB8, 0xC2), th::kAccentDark, t));
    p.drawRoundedRect(QRectF(x, 4.0, d + stretch, d), d / 2, d / 2);
}
