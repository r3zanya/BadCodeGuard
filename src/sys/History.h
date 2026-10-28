#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

namespace bcg {

struct HistoryEntry {
    enum Type { Threat, Scan };

    Type type = Threat;
    QDateTime when;
    QString source;  // "Проверка", "Реальное время"
    QString title;   // threat name, or the scan summary
    QString path;    // empty for scan summaries
    QString action;  // what happened to the file
    int kind = 0;    // ThreatKind, for threats
    QString note;    // extra: VirusTotal verdict, ...
};

// Journal of findings and scan runs, saved to history.json in the data folder.
// Used from the UI thread only.
class History : public QObject {
    Q_OBJECT
public:
    explicit History(const QString& file, QObject* parent = nullptr);

    const QList<HistoryEntry>& entries() const { return m_entries; }
    // Returns the index of the new entry (newest entries are last).
    int add(const HistoryEntry& entry);
    // Changes action / note of an existing entry (for example after a lookup).
    void update(int index, const QString& action, const QString& note);
    void clear();
    // Writes a CSV file (UTF-8 with BOM, opens correctly in Excel).
    bool exportCsv(const QString& path) const;

signals:
    void changed();

private:
    void load();
    void save() const;

    QString m_file;
    QList<HistoryEntry> m_entries;
};

} // namespace bcg
