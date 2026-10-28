#pragma once

#include "sys/Scoring.h"

#include <QFrame>

class GlowProgress;
class QLabel;

// "System rating" card: four component ratings with animated bars and a
// large final score. Slides open the first time it is shown.
class ScoreCard : public QFrame {
    Q_OBJECT
public:
    explicit ScoreCard(QWidget* parent = nullptr);

    void showLoading();
    void showScores(const bcg::sys::Scores& scores);

private:
    struct Row {
        QLabel* value = nullptr;
        GlowProgress* bar = nullptr;
    };
    Row addRow(class QVBoxLayout* layout, const QString& title, const QString& hint);
    void reveal();
    void animateTo(QLabel* label, double target, int duration);

    Row m_cpu;
    Row m_gpu;
    Row m_capacity;
    Row m_speed;
    QLabel* m_hint;
    QLabel* m_total;
    bool m_revealed = false;
};
