#pragma once

#include <QAbstractButton>
#include <QPixmap>

class QPropertyAnimation;

// Square button that shows only an icon, with animated hover and press.
class IconButton : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
    Q_PROPERTY(qreal press READ press WRITE setPress)
public:
    IconButton(const QString& icon, const QString& toolTip, QWidget* parent = nullptr);

    QSize sizeHint() const override { return {42, 42}; }

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

    QPixmap m_muted;
    QPixmap m_accent;
    qreal m_hover = 0.0;
    qreal m_press = 0.0;
    QPropertyAnimation* m_hoverAnim;
    QPropertyAnimation* m_pressAnim;
};
