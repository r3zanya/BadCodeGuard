#pragma once

#include <QFrame>

class QPropertyAnimation;

// Amber notice shown when the app runs without administrator rights, with a
// button that restarts it elevated. Hidden when already elevated.
class ElevationBanner : public QFrame {
    Q_OBJECT
    Q_PROPERTY(qreal flash READ flash WRITE setFlash)
public:
    explicit ElevationBanner(QWidget* parent = nullptr);

    // Briefly highlights the banner to draw attention to it.
    void pulse();

    qreal flash() const { return m_flash; }
    void setFlash(qreal v);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    qreal m_flash = 0.0;
    QPropertyAnimation* m_flashAnim;
};
