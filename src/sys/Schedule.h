#pragma once

#include <QDateTime>
#include <QString>
#include <QTime>

class QSettings;

namespace bcg {

// When to run an unattended scan: every day or once a week, at a fixed time.
// A run missed because the computer was off happens at the next opportunity.
struct Schedule {
    enum Period { Daily, Weekly };
    enum ScanKind { Quick, Full };

    bool enabled = false;
    Period period = Weekly;
    int dayOfWeek = 1; // Qt: 1 = Monday ... 7 = Sunday (used for Weekly)
    QTime time{3, 0};
    ScanKind scan = Quick;

    // The latest scheduled moment that is not after `now`.
    QDateTime lastDue(const QDateTime& now) const;
    // The next scheduled moment after `now`.
    QDateTime nextDue(const QDateTime& now) const;
    // True when a scheduled moment has passed since `lastRun` (an invalid
    // `lastRun` counts as "never": not due, see QSettings handling in the UI).
    bool isDue(const QDateTime& now, const QDateTime& lastRun) const;

    QString describe() const; // "каждый день в 03:00", "по понедельникам в 03:00"

    static Schedule load(const QSettings& settings);
    void save(QSettings& settings) const;
};

} // namespace bcg
