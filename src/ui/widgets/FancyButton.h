#pragma once

#include <QPixmap>
#include <QPushButton>

class QPropertyAnimation;

// Push button with an icon, an animated hover glow and a press "squish".
class FancyButton : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
    Q_PROPERTY(qreal press READ press WRITE setPress)
public:
    enum Variant { Primary, Secondary, Danger };

    FancyButton(const QString& text, const QString& icon, Variant variant,
                QWidget* parent = nullptr);

    QSize sizeHint() const override;

    qreal hover() const { return m_hover; }
    void setHover(qreal v);
    qreal press() const { return m_press; }
    void setPress(qreal v);

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void changeEvent(QEvent*) override;

private:
    static void animateTo(QPropertyAnimation* anim, qreal target);

    Variant m_variant;
    QPixmap m_icon;
    qreal m_hover = 0.0;
    qreal m_press = 0.0;
    QPropertyAnimation* m_hoverAnim;
    QPropertyAnimation* m_pressAnim;
};
