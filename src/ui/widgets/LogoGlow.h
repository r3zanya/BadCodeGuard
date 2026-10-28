#pragma once

#include <QPixmap>
#include <QWidget>

class QPropertyAnimation;

// The brand shield with a gentle "breathing" glow and radar-like pulse rings.
// While active (scanning) the pulse is stronger and faster.
class LogoGlow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal phase READ phase WRITE setPhase)
    Q_PROPERTY(qreal intensity READ intensity WRITE setIntensity)
public:
    explicit LogoGlow(QWidget* parent = nullptr);

    QSize sizeHint() const override { return {176, 176}; }

    void setActive(bool active);

    qreal phase() const { return m_phase; }
    void setPhase(qreal v);
    qreal intensity() const { return m_intensity; }
    void setIntensity(qreal v);

protected:
    void paintEvent(QPaintEvent*) override;
    void showEvent(QShowEvent*) override;
    void hideEvent(QHideEvent*) override;

private:
    QPixmap m_logo;
    qreal m_phase = 0.0;
    qreal m_intensity = 0.5;
    QPropertyAnimation* m_phaseAnim;
    QPropertyAnimation* m_intensityAnim;
};
