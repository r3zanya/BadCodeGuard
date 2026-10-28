#include "FadeLabel.h"

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>

FadeLabel::FadeLabel(QWidget* parent)
    : QLabel(parent)
{
    m_effect = new QGraphicsOpacityEffect(this);
    m_effect->setOpacity(1.0);
    setGraphicsEffect(m_effect);
}

void FadeLabel::apply(const QString& text, const QColor& color)
{
    setText(text);
    setStyleSheet(color.isValid() ? QStringLiteral("color: %1;").arg(color.name()) : QString());
}

void FadeLabel::setTextAnimated(const QString& text, const QColor& color)
{
    if (text == m_text && color == m_color)
        return;
    const bool first = m_text.isNull();
    m_text = text;
    m_color = color;

    if (m_group) {
        m_group->stop();
        m_group->deleteLater();
        m_group = nullptr;
    }
    if (first || !isVisible()) {
        m_effect->setOpacity(1.0);
        apply(text, color);
        return;
    }

    m_group = new QSequentialAnimationGroup(this);
    auto* out = new QPropertyAnimation(m_effect, "opacity");
    out->setDuration(140);
    out->setEndValue(0.0);
    out->setEasingCurve(QEasingCurve::InCubic);
    auto* in = new QPropertyAnimation(m_effect, "opacity");
    in->setDuration(320);
    in->setEndValue(1.0);
    in->setEasingCurve(QEasingCurve::OutCubic);
    m_group->addAnimation(out);
    m_group->addAnimation(in);
    connect(out, &QPropertyAnimation::finished, this, [this, text, color] { apply(text, color); });
    m_group->start();
}
