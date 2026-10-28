#pragma once

#include <QScrollArea>

class QPropertyAnimation;

// Scroll area whose mouse-wheel scrolling glides to its target instead of
// jumping in steps; consecutive wheel notches add up.
class SmoothScrollArea : public QScrollArea {
    Q_OBJECT
public:
    explicit SmoothScrollArea(QWidget* parent = nullptr);

protected:
    void wheelEvent(QWheelEvent*) override;

private:
    QPropertyAnimation* m_anim;
    int m_target = 0;
};
