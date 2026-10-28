#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

namespace bcg::sys {

class ProcessRunner;

// Collects hardware and Windows data with PowerShell (two parallel scripts,
// because several WMI queries are slow) and keeps the latest result.
class SystemInfoService : public QObject {
    Q_OBJECT
public:
    static SystemInfoService& instance();

    // Starts loading unless a load is already running.
    void refresh();
    bool isLoading() const { return m_pending > 0; }
    bool hasHardware() const { return !m_hardware.isEmpty(); }
    bool hasWindows() const { return !m_windows.isEmpty(); }
    const QJsonObject& hardware() const { return m_hardware; }
    const QJsonObject& windows() const { return m_windows; }
    QDateTime lastRefresh() const { return m_lastRefresh; }

signals:
    void hardwareChanged();
    void windowsChanged();
    void loadFinished();

private:
    SystemInfoService();
    void load(const QString& part, ProcessRunner* runner);

    QJsonObject m_hardware;
    QJsonObject m_windows;
    QDateTime m_lastRefresh;
    QString m_hardwareText;
    QString m_windowsText;
    ProcessRunner* m_hardwareRunner;
    ProcessRunner* m_windowsRunner;
    int m_pending = 0;
};

// ---- Presentation of the collected data ----

enum class Status { None, Good, Warn, Bad, Neutral };

struct InfoRow {
    QString key;
    QString value;
    Status status = Status::None;
};

struct InfoGroup {
    QString title;
    QList<InfoRow> rows;
};

namespace info {

// "32 GB", "15,92 GB": binary gigabytes, Russian decimal comma.
QString formatGb(double bytes);
// "812 MB", "1,23 GB": picks the unit by magnitude.
QString formatSize(double bytes);
// "2 TB", "512 GB": decimal units as printed on drive boxes.
QString formatDriveSize(double bytes);

// The four lines of the "copy" format, e.g.
//   Процессор: AMD Ryzen 5 7500F 6-Core Processor (6 / 12)
QString cpuLine(const QJsonObject& hw);
QString ramLine(const QJsonObject& hw);
QString gpuLine(const QJsonObject& hw);
QString deviceLine(const QJsonObject& hw);
QString copyText(const QJsonObject& hw);

// Values of the six summary tiles.
struct Tile {
    QString main;
    QString sub;
};
Tile cpuTile(const QJsonObject& hw);
Tile gpuTile(const QJsonObject& hw);
Tile ramTile(const QJsonObject& hw);
Tile vramTile(const QJsonObject& hw);
Tile storageTile(const QJsonObject& hw);
Tile deviceTile(const QJsonObject& hw);

QList<InfoGroup> hardwareGroups(const QJsonObject& hw);

QList<InfoRow> windowsOverview(const QJsonObject& win);
QList<InfoGroup> windowsGroups(const QJsonObject& win);
// Extra Windows facts that only the text report includes.
QList<InfoRow> windowsExtras(const QJsonObject& win);

QString fullReport(const QJsonObject& hw, const QJsonObject& win);

} // namespace info
} // namespace bcg::sys
