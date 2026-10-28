#include "Animations.h"

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QMetaObject>
#include <QPointer>
#include <QPropertyAnimation>
#include <QWidget>

namespace bcg {

void fadeIn(QWidget* widget, int durationMs)
{
    auto* effect = new QGraphicsOpacityEffect(widget);
    effect->setOpacity(0.0);
    widget->setGraphicsEffect(effect);

    auto* anim = new QPropertyAnimation(effect, "opacity", effect);
    anim->setDuration(durationMs);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    QPointer<QWidget> target(widget);
    QObject::connect(anim, &QPropertyAnimation::finished, widget, [target] {
        if (!target)
            return;
        // Drop the effect once the animation has fully stopped.
        QMetaObject::invokeMethod(target, [target] {
            if (target)
                target->setGraphicsEffect(nullptr);
        }, Qt::QueuedConnection);
    });
    anim->start();
}

} // namespace bcg
