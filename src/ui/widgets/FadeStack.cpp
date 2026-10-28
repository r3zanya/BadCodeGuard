#include "FadeStack.h"

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>

namespace {
constexpr int kFadeInMs = 380;
constexpr int kFadeOutMs = 340;
constexpr int kSlideMs = 460;
constexpr int kSlideIn = 18;   // the new page rises from this far below
constexpr int kSlideOut = -10; // the old page drifts slightly upwards
} // namespace

void FadeStack::setCurrentIndexAnimated(int index)
{
    if (index == currentIndex() || index < 0 || index >= count())
        return;

    finishAnimation();

    // Snapshot of the page being left; it fades out on top of the new one.
    QPixmap snapshot;
    if (isVisible() && currentWidget() && currentWidget()->isVisible())
        snapshot = currentWidget()->grab();

    setCurrentIndex(index);
    QWidget* page = currentWidget();

    m_group = new QParallelAnimationGroup(this);

    // With a snapshot on top the new page stays opaque underneath, so the
    // overlay fading out is a true cross-fade without a dip in brightness.
    if (snapshot.isNull()) {
        auto* effect = new QGraphicsOpacityEffect(page);
        effect->setOpacity(0.0);
        page->setGraphicsEffect(effect);
        auto* fadeIn = new QPropertyAnimation(effect, "opacity");
        fadeIn->setDuration(kFadeInMs);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->setEasingCurve(QEasingCurve::InOutCubic);
        m_group->addAnimation(fadeIn);
    }

    auto* slide = new QPropertyAnimation(page, "pos");
    slide->setDuration(kSlideMs);
    slide->setStartValue(QPoint(0, kSlideIn));
    slide->setEndValue(QPoint(0, 0));
    slide->setEasingCurve(QEasingCurve::OutQuart);
    m_group->addAnimation(slide);

    if (!snapshot.isNull()) {
        auto* overlay = new QLabel(this);
        overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
        overlay->setPixmap(snapshot);
        overlay->setGeometry(0, 0, width(), height());
        overlay->show();
        overlay->raise();
        m_overlay = overlay;

        auto* overlayEffect = new QGraphicsOpacityEffect(overlay);
        overlayEffect->setOpacity(1.0);
        overlay->setGraphicsEffect(overlayEffect);
        auto* fadeOut = new QPropertyAnimation(overlayEffect, "opacity");
        fadeOut->setDuration(kFadeOutMs);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);
        fadeOut->setEasingCurve(QEasingCurve::InOutCubic);
        m_group->addAnimation(fadeOut);

        auto* drift = new QPropertyAnimation(overlay, "pos");
        drift->setDuration(kSlideMs);
        drift->setStartValue(QPoint(0, 0));
        drift->setEndValue(QPoint(0, kSlideOut));
        drift->setEasingCurve(QEasingCurve::OutQuart);
        m_group->addAnimation(drift);
    }

    m_page = page;
    connect(m_group, &QAbstractAnimation::finished, this, &FadeStack::finishAnimation);
    m_group->start();
}

// Stops a running transition and restores the stack to its normal state; the
// graphics effect must not linger because it makes text rendering slower.
void FadeStack::finishAnimation()
{
    QParallelAnimationGroup* group = m_group;
    m_group.clear();
    if (group) {
        group->stop();
        group->deleteLater();
    }
    if (m_overlay) {
        m_overlay->deleteLater();
        m_overlay.clear();
    }
    if (m_page) {
        m_page->setGraphicsEffect(nullptr);
        m_page->move(0, 0);
        m_page.clear();
    }
}
