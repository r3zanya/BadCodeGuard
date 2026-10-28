#pragma once

#include <QWidget>

class QPropertyAnimation;

// Thin progress bar with a soft glow. Either shows a smoothly animated
// fraction, or a sweeping segment while the total is unknown (busy).
class GlowProgress : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal value READ value WRITE setValueRaw)
    Q_PROPERTY(qreal phase READ phase WRITE setPhase)
public:
    explicit GlowProgress(QWidget* parent = nullptr);

    QSize sizeHint() const override { return {200, 16}; }

    void setBusy(bool busy);
    void setValue(qreal fraction); // animated, 0..1

    qreal value() const { return m_value; }
    void setValueRaw(qreal v);
    qreal phase() const { return m_phase; }
    void setPhase(qreal v);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    qreal m_value = 0.0;
    qreal m_phase = 0.0;
    bool m_busy = false;
    QPropertyAnimation* m_valueAnim;
    QPropertyAnimation* m_phaseAnim;
};
