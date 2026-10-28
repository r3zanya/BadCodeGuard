#include "History.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace bcg {

namespace {
constexpr int kMaxEntries = 2000;

QString csvField(const QString& s)
{
    QString v = s;
    v.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    // Stop spreadsheet programs from running a cell that starts like a formula.
    if (!v.isEmpty() && QStringLiteral("=+-@").contains(v.at(0)))
        v.prepend(QLatin1Char('\''));
    return QLatin1Char('"') + v + QLatin1Char('"');
}
} // namespace

History::History(const QString& file, QObject* parent)
    : QObject(parent)
    , m_file(file)
{
    load();
}

void History::load()
{
    QFile f(m_file);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonArray array = QJsonDocument::fromJson(f.readAll()).array();
    for (const QJsonValue& v : array) {
        const QJsonObject o = v.toObject();
        HistoryEntry e;
        e.type = o[QLatin1String("type")].toInt() == int(HistoryEntry::Scan) ? HistoryEntry::Scan
                                                                              : HistoryEntry::Threat;
        e.when = QDateTime::fromString(o[QLatin1String("when")].toString(), Qt::ISODate);
        e.source = o[QLatin1String("source")].toString();
        e.title = o[QLatin1String("title")].toString();
        e.path = o[QLatin1String("path")].toString();
        e.action = o[QLatin1String("action")].toString();
        e.kind = o[QLatin1String("kind")].toInt();
        e.note = o[QLatin1String("note")].toString();
        m_entries.append(e);
    }
}

void History::save() const
{
    QJsonArray array;
    for (const HistoryEntry& e : m_entries) {
        QJsonObject o;
        o[QLatin1String("type")] = int(e.type);
        o[QLatin1String("when")] = e.when.toString(Qt::ISODate);
        o[QLatin1String("source")] = e.source;
        o[QLatin1String("title")] = e.title;
        o[QLatin1String("path")] = e.path;
        o[QLatin1String("action")] = e.action;
        o[QLatin1String("kind")] = e.kind;
        o[QLatin1String("note")] = e.note;
        array.append(o);
    }
    QDir().mkpath(QFileInfo(m_file).absolutePath());
    QSaveFile f(m_file);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

int History::add(const HistoryEntry& entry)
{
    HistoryEntry e = entry;
    if (!e.when.isValid())
        e.when = QDateTime::currentDateTime();
    m_entries.append(e);
    while (m_entries.size() > kMaxEntries)
        m_entries.removeFirst();
    save();
    emit changed();
    return int(m_entries.size()) - 1;
}

void History::update(int index, const QString& action, const QString& note)
{
    if (index < 0 || index >= m_entries.size())
        return;
    m_entries[index].action = action;
    m_entries[index].note = note;
    save();
    emit changed();
}

void History::clear()
{
    m_entries.clear();
    save();
    emit changed();
}

bool History::exportCsv(const QString& path) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write("\xEF\xBB\xBF");
    f.write("Дата;Источник;Событие;Путь;Действие;Примечание\r\n");
    for (const HistoryEntry& e : m_entries) {
        const QStringList cols = {e.when.toString(QStringLiteral("dd.MM.yyyy HH:mm:ss")), e.source,
                                  e.title, e.path, e.action, e.note};
        QStringList quoted;
        for (const QString& c : cols)
            quoted << csvField(c);
        f.write((quoted.join(QLatin1Char(';')) + QStringLiteral("\r\n")).toUtf8());
    }
    return true;
}

} // namespace bcg
