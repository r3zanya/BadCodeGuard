#include "Schedule.h"

#include <QLocale>
#include <QSettings>

namespace bcg {

QDateTime Schedule::lastDue(const QDateTime& now) const
{
    QDate date = now.date();
    if (period == Weekly)
        date = date.addDays(-((date.dayOfWeek() - dayOfWeek + 7) % 7));
    QDateTime candidate(date, time);
    if (candidate > now)
        candidate = candidate.addDays(period == Weekly ? -7 : -1);
    return candidate;
}

QDateTime Schedule::nextDue(const QDateTime& now) const
{
    return lastDue(now).addDays(period == Weekly ? 7 : 1);
}

bool Schedule::isDue(const QDateTime& now, const QDateTime& lastRun) const
{
    if (!enabled || !lastRun.isValid())
        return false;
    return lastRun < lastDue(now);
}

QString Schedule::describe() const
{
    const QString at = time.toString(QStringLiteral("HH:mm"));
    if (period == Daily)
        return QStringLiteral("каждый день в %1").arg(at);
    static const char* const kDays[] = {"понедельникам", "вторникам", "средам", "четвергам",
                                        "пятницам", "субботам", "воскресеньям"};
    return QStringLiteral("по %1 в %2").arg(QString::fromUtf8(kDays[qBound(1, dayOfWeek, 7) - 1]), at);
}

Schedule Schedule::load(const QSettings& s)
{
    Schedule r;
    r.enabled = s.value(QStringLiteral("schedule/enabled"), false).toBool();
    r.period = s.value(QStringLiteral("schedule/period"), int(Weekly)).toInt() == int(Daily) ? Daily : Weekly;
    r.dayOfWeek = qBound(1, s.value(QStringLiteral("schedule/day"), 1).toInt(), 7);
    const QTime t = QTime::fromString(s.value(QStringLiteral("schedule/time"), QStringLiteral("03:00")).toString(),
                                      QStringLiteral("HH:mm"));
    r.time = t.isValid() ? t : QTime(3, 0);
    r.scan = s.value(QStringLiteral("schedule/scan"), int(Quick)).toInt() == int(Full) ? Full : Quick;
    return r;
}

void Schedule::save(QSettings& s) const
{
    s.setValue(QStringLiteral("schedule/enabled"), enabled);
    s.setValue(QStringLiteral("schedule/period"), int(period));
    s.setValue(QStringLiteral("schedule/day"), dayOfWeek);
    s.setValue(QStringLiteral("schedule/time"), time.toString(QStringLiteral("HH:mm")));
    s.setValue(QStringLiteral("schedule/scan"), int(scan));
}

} // namespace bcg
