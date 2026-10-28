#pragma once

#include <QAbstractButton>
#include <QPixmap>

class QPropertyAnimation;

// Large clickable card that opens a section; lifts and slides its arrow on hover.
class SectionCard : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
    Q_PROPERTY(qreal press READ press WRITE setPress)
    Q_PROPERTY(qreal reveal READ reveal WRITE setReveal)
public:
    SectionCard(const QString& icon, const QString& title, const QString& description,
                QWidget* parent = nullptr);

    QSize sizeHint() const override { return {320, 172}; }

    qreal hover() const { return m_hover; }
    void setHover(qreal v);
    qreal press() const { return m_press; }
    void setPress(qreal v);
    qreal reveal() const { return m_reveal; }
    void setReveal(qreal v);

    // Hides the card, then fades and slides it in after `delayMs`.
    void playEntrance(int delayMs);

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    static void animateTo(QPropertyAnimation* anim, qreal target);

    QString m_title;
    QString m_description;
    QPixmap m_icon;
    QPixmap m_arrow;
    qreal m_hover = 0.0;
    qreal m_press = 0.0;
    qreal m_reveal = 1.0;
    QPropertyAnimation* m_revealAnim;
    QPropertyAnimation* m_hoverAnim;
    QPropertyAnimation* m_pressAnim;
};
