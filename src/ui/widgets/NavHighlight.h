#pragma once

#include <QWidget>

class QPropertyAnimation;

// Selection marker of the sidebar: one rounded plate with an accent bar that
// glides from entry to entry instead of each entry lighting up separately.
class NavHighlight : public QWidget {
    Q_OBJECT
public:
    explicit NavHighlight(QWidget* parent = nullptr);

    // Moves (animated) to cover `target`, given in parent coordinates.
    void moveTo(const QRect& target, bool animate = true);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QPropertyAnimation* m_anim;
    bool m_placed = false;
};
