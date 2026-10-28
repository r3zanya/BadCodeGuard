#pragma once

#include <QColor>
#include <QLabel>

class QGraphicsOpacityEffect;
class QSequentialAnimationGroup;

// Label that cross-fades when its text (or colour) changes.
class FadeLabel : public QLabel {
    Q_OBJECT
public:
    explicit FadeLabel(QWidget* parent = nullptr);

    // An invalid colour keeps the colour from the stylesheet.
    void setTextAnimated(const QString& text, const QColor& color = QColor());

private:
    void apply(const QString& text, const QColor& color);

    QGraphicsOpacityEffect* m_effect;
    QSequentialAnimationGroup* m_group = nullptr;
    QString m_text;
    QColor m_color;
};
