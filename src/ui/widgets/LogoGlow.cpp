#include "LogoGlow.h"

#include "ui/Theme.h"

#include <QEasingCurve>
#include <QPainter>
#include <QPropertyAnimation>
#include <QRadialGradient>
#include <QtMath>

namespace th = bcg::theme;

namespace {
constexpr qreal kLogoSize = 92.0;
constexpr qreal kIdleIntensity = 0.5;
constexpr int kIdleCycleMs = 3600;
constexpr int kActiveCycleMs = 1800;
} // namespace

LogoGlow::LogoGlow(QWidget* parent)
    : QWidget(parent)
    , m_logo(QStringLiteral(":/icons/symbol.png"))
{
    setFixedSize(sizeHint());

    m_phaseAnim = new QPropertyAnimation(this, "phase", this);
    m_phaseAnim->setStartValue(0.0);
    m_phaseAnim->setEndValue(1.0);
    m_phaseAnim->setDuration(kIdleCycleMs);
    m_phaseAnim->setLoopCount(-1);

    m_intensityAnim = new QPropertyAnimation(this, "intensity", this);
    m_intensityAnim->setDuration(600);
    m_intensityAnim->setEasingCurve(QEasingCurve::OutCubic);
}

void LogoGlow::setActive(bool active)
{
    m_intensityAnim->stop();
    m_intensityAnim->setStartValue(m_intensity);
    m_intensityAnim->setEndValue(active ? 1.0 : kIdleIntensity);
    m_intensityAnim->start();
    m_phaseAnim->setDuration(active ? kActiveCycleMs : kIdleCycleMs);
}

void LogoGlow::setPhase(qreal v)
{
    m_phase = v;
    update();
}

void LogoGlow::setIntensity(qreal v)
{
    m_intensity = v;
    update();
}

// Animate only while visible: no repaints for hidden pages.
void LogoGlow::showEvent(QShowEvent*)
{
    m_phaseAnim->start();
}

void LogoGlow::hideEvent(QHideEvent*)
{
    m_phaseAnim->stop();
}

void LogoGlow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QPointF c(width() / 2.0, height() / 2.0);
    const qreal breathe = 0.5 + 0.5 * qSin(m_phase * 2.0 * M_PI);

    // Soft halo behind the shield.
    const qreal haloRadius = 64.0 + 6.0 * breathe;
    QRadialGradient halo(c, haloRadius);
    QColor core = th::kAccent;
    core.setAlphaF((0.10 + 0.10 * breathe) * m_intensity);
    QColor rim = th::kAccent;
    rim.setAlphaF(0.0);
    halo.setColorAt(0.0, core);
    halo.setColorAt(1.0, rim);
    p.setPen(Qt::NoPen);
    p.setBrush(halo);
    p.drawEllipse(c, haloRadius, haloRadius);

    // Two rings expanding outwards, half a cycle apart.
    for (int k = 0; k < 2; ++k) {
        const qreal t = std::fmod(m_phase + 0.5 * k, 1.0);
        const qreal fade = (1.0 - t) * (1.0 - t);
        QColor ring = th::kAccent;
        ring.setAlphaF(0.45 * fade * m_intensity);
        p.setPen(QPen(ring, 1.6));
        p.setBrush(Qt::NoBrush);
        const qreal r = 54.0 + t * 32.0;
        p.drawEllipse(c, r, r);
    }

    const qreal size = kLogoSize * (1.0 + 0.02 * breathe * m_intensity);
    p.drawPixmap(QRectF(c.x() - size / 2, c.y() - size / 2, size, size), m_logo,
                 QRectF(m_logo.rect()));
}
