#pragma once

#include <QAbstractButton>
#include <QPixmap>

class QPropertyAnimation;

// Sidebar entry: line icon + text, with animated hover and selection states.
class NavButton : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
    Q_PROPERTY(qreal selection READ selection WRITE setSelection)
public:
    NavButton(const QString& text, const QString& icon, QWidget* parent = nullptr);

    QSize sizeHint() const override { return {198, 44}; }

    qreal hover() const { return m_hover; }
    void setHover(qreal v);
    qreal selection() const { return m_selection; }
    void setSelection(qreal v);

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    static void animateTo(QPropertyAnimation* anim, qreal target);

    QPixmap m_iconMuted;
    QPixmap m_iconAccent;
    qreal m_hover = 0.0;
    qreal m_selection = 0.0;
    QPropertyAnimation* m_hoverAnim;
    QPropertyAnimation* m_selectionAnim;
};
