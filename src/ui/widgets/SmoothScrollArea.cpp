#include "SmoothScrollArea.h"

#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>

namespace {
constexpr double kPixelsPerDegree = 0.9; // one 120-unit notch scrolls ~108 px
constexpr int kDurationMs = 320;
} // namespace

SmoothScrollArea::SmoothScrollArea(QWidget* parent)
    : QScrollArea(parent)
{
    m_anim = new QPropertyAnimation(verticalScrollBar(), "value", this);
    m_anim->setDuration(kDurationMs);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    // Dragging the scrollbar handle takes over from a running animation.
    connect(verticalScrollBar(), &QScrollBar::sliderPressed, m_anim, &QPropertyAnimation::stop);
}

void SmoothScrollArea::wheelEvent(QWheelEvent* event)
{
    QScrollBar* bar = verticalScrollBar();
    // Touchpads already deliver smooth pixel deltas; only animate wheel notches.
    if (!event->pixelDelta().isNull() || event->angleDelta().y() == 0 || bar->maximum() == 0) {
        QScrollArea::wheelEvent(event);
        return;
    }

    const int start = m_anim->state() == QAbstractAnimation::Running ? m_target : bar->value();
    m_target = qBound(bar->minimum(),
                      start - static_cast<int>(event->angleDelta().y() * kPixelsPerDegree),
                      bar->maximum());
    m_anim->stop();
    m_anim->setStartValue(bar->value());
    m_anim->setEndValue(m_target);
    m_anim->start();
    event->accept();
}
