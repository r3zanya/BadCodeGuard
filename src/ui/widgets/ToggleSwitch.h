#pragma once

#include <QAbstractButton>

class QPropertyAnimation;

// iOS-style on/off switch whose knob and colour glide between states.
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal position READ position WRITE setPosition)
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    QSize sizeHint() const override { return {50, 28}; }

    // Sets the state without animating or emitting toggled().
    void setCheckedInstant(bool on);

    qreal position() const { return m_position; }
    void setPosition(qreal v);
    qreal hover() const { return m_hover; }
    void setHover(qreal v);

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    qreal m_position = 0.0;
    qreal m_hover = 0.0;
    QPropertyAnimation* m_positionAnim;
    QPropertyAnimation* m_hoverAnim;
};
