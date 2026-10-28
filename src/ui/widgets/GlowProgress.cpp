#include "GlowProgress.h"

#include "ui/Theme.h"

#include <QEasingCurve>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>

namespace th = bcg::theme;

GlowProgress::GlowProgress(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(sizeHint().height());

    m_valueAnim = new QPropertyAnimation(this, "value", this);
    m_valueAnim->setDuration(450);
    m_valueAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_phaseAnim = new QPropertyAnimation(this, "phase", this);
    m_phaseAnim->setDuration(1500);
    m_phaseAnim->setStartValue(0.0);
    m_phaseAnim->setEndValue(1.0);
    m_phaseAnim->setEasingCurve(QEasingCurve::InOutSine);
    m_phaseAnim->setLoopCount(-1);
}

void GlowProgress::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    if (busy)
        m_phaseAnim->start();
    else
        m_phaseAnim->stop();
    update();
}

void GlowProgress::setValue(qreal fraction)
{
    m_valueAnim->stop();
    m_valueAnim->setStartValue(m_value);
    m_valueAnim->setEndValue(qBound(0.0, fraction, 1.0));
    m_valueAnim->start();
}

void GlowProgress::setValueRaw(qreal v)
{
    m_value = v;
    update();
}

void GlowProgress::setPhase(qreal v)
{
    m_phase = v;
    update();
}

void GlowProgress::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF track(0, (height() - 8) / 2.0, width(), 8);
    const qreal radius = track.height() / 2.0;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x1A, 0x1A, 0x20));
    p.drawRoundedRect(track, radius, radius);

    QRectF bar;
    if (m_busy) {
        const qreal segment = track.width() * 0.35;
        const qreal x = track.left() - segment + m_phase * (track.width() + segment);
        bar = QRectF(x, track.top(), segment, track.height());
    } else if (m_value > 0.0) {
        bar = QRectF(track.left(), track.top(), qMax(track.height(), track.width() * m_value),
                     track.height());
    } else {
        return;
    }

    // Glow underneath, then the bar itself, both clipped to the track.
    QPainterPath clip;
    clip.addRoundedRect(track, radius, radius);
    p.setClipPath(clip);

    QColor glow = th::kAccent;
    glow.setAlphaF(0.18f);
    p.setBrush(glow);
    p.drawRoundedRect(bar.adjusted(0, -3, 0, 3), radius + 3, radius + 3);

    QLinearGradient g(bar.topLeft(), bar.topRight());
    QColor edge = th::kAccent;
    if (m_busy)
        edge.setAlphaF(0.0);
    g.setColorAt(0.0, m_busy ? edge : th::kAccent);
    g.setColorAt(m_busy ? 0.5 : 1.0, th::kAccentLight);
    g.setColorAt(1.0, m_busy ? edge : th::kAccentLight);
    p.setBrush(g);
    p.drawRoundedRect(bar, radius, radius);
}
