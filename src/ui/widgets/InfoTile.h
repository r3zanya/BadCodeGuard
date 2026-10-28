#pragma once

#include <QColor>
#include <QFrame>

class QLabel;
class QPropertyAnimation;

// Summary tile: icon, caption, one prominent value and a small subtitle.
// Fades its text in whenever the content is replaced.
class InfoTile : public QFrame {
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
public:
    InfoTile(const QString& icon, const QString& caption, QWidget* parent = nullptr);

    void setContent(const QString& main, const QString& sub);
    void setLoading();
    void setMainColor(const QColor& color); // invalid colour restores the default

    qreal hover() const { return m_hover; }
    void setHover(qreal v);

protected:
    void paintEvent(QPaintEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    QLabel* m_main;
    QLabel* m_sub;
    QWidget* m_content;
    qreal m_hover = 0.0;
    QPropertyAnimation* m_hoverAnim;
};
