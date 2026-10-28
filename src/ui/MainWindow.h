#pragma once

#include "engine/EngineSetup.h"
#include "engine/RealtimeMonitor.h"

#include <QElapsedTimer>
#include <QMainWindow>

#include <memory>

class QButtonGroup;
class QCloseEvent;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QShowEvent;
class QSystemTrayIcon;
class QVBoxLayout;
class QThread;
class FadeLabel;
class FadeStack;
class NavHighlight;
class QTimer;
class FancyButton;
class GlowProgress;
class LogoGlow;
class ToggleSwitch;
class ExclusionsPage;
class QuarantinePage;
class QPushButton;

class QLineEdit;
class QPlainTextEdit;
class HistoryPage;

class QComboBox;
class QTimeEdit;

namespace bcg {
class IScanner;
class ProcessMonitor;
class RealtimeMonitor;
class DatabaseUpdater;
class FeedUpdater;
class History;
class ReputationService;
struct ReputationResult;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool startMinimized(bool launchedByAutostart = false) const;

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onProgress(qint64 scanned, const QString& currentFile);
    void onThreatFound(const QString& path, const QString& name, int kind,
                       const QString& quarantineId, const QString& quarantineError);
    void onFinished(bool cancelled, qint64 scanned, qint64 threats, qint64 skipped);
    void onRealtimeThreat(const QString& path, const QString& name, int kind,
                          const QString& quarantineId, const QString& quarantineError);
    void onDatabaseUpdated(bool ok, const QString& message, int listed);
    void onFeedFinished(int kind, bool ok, const QString& message);
    void onProcessThreat(const QString& path, const QString& name, int kind,
                         const QString& quarantineId, const QString& quarantineError,
                         bool terminated, quint32 pid);
    void onTotal(qint64 total, bool final);
    void onReputation(const bcg::ReputationResult& result);

private:
    enum Page {
        PageDashboard,
        PageScan,
        PageQuarantine,
        PageHistory,
        PageExclusions,
        PageAdvanced,
        PageSettings
    };

    void buildUi();
    QWidget* buildSidebar();
    QWidget* buildDashboardPage();
    QWidget* buildScanPage();
    QWidget* buildQuarantinePage();
    QWidget* buildSettingsPage();
    void buildTray();
    void startEngineThread();
    void updateThreatButtons();
    void quarantineSelected();
    void excludeSelected();

    void showPage(Page page);
    void startScan(const QString& title, const QStringList& paths);
    void quickScan();
    void fullScan();
    void customScan();
    void setScanButtonsEnabled(bool enabled);
    void setStatus(const QString& text, const QColor& color);
    void updateLastScanLabel();
    void refreshProtectionRows();
    void applyRealtime(bool on);
    QString addThreatItem(const QString& path, const QString& name, int kind,
                          const QString& quarantineId, const QString& quarantineError);
    void updateDatabase(bool silent);
    void updateHashFeeds();
    void finishDatabaseUpdate();
    void autoUpdateDatabase();
    void refreshDbInfo();
    void refreshYaraInfo();
    // Folders for real-time protection, from the settings and the drives present now.
    QList<bcg::RealtimeMonitor::WatchFolder> watchList() const;
    void refreshWatchers(bool force);
    void applySchedule();
    void checkSchedule();
    void saveSchedule();
    QString etaText() const;
    void recordThreat(const QString& source, const QString& path, const QString& name, int kind,
                      const QString& quarantineId, const QString& quarantineError);
    void maybeLookup(const QString& path, int kind);
    void lookupSelected();
    QStringList feedUrls() const;
    void appendLog(const QString& line);
    void updateCounters(bool instant = false);
    void animateCount(QLabel* label, qint64 target, bool instant);
    void placeHighlight(bool animate);

    FadeStack* m_stack = nullptr;
    QButtonGroup* m_navGroup = nullptr;
    NavHighlight* m_highlight = nullptr;
    QTimer* m_clock = nullptr;
    QSystemTrayIcon* m_tray = nullptr;

    LogoGlow* m_glow = nullptr;
    FadeLabel* m_statusLabel = nullptr;
    QLabel* m_lastScanLabel = nullptr;
    QVBoxLayout* m_protectionRows = nullptr;
    FancyButton* m_quickBtn = nullptr;
    FancyButton* m_fullBtn = nullptr;
    FancyButton* m_customBtn = nullptr;

    FadeLabel* m_scanTitle = nullptr;
    QLabel* m_currentFile = nullptr;
    QLabel* m_filesValue = nullptr;
    QLabel* m_threatsValue = nullptr;
    QLabel* m_timeValue = nullptr;
    GlowProgress* m_progress = nullptr;
    FancyButton* m_cancelBtn = nullptr;
    QListWidget* m_threatList = nullptr;
    QPlainTextEdit* m_log = nullptr;

    ToggleSwitch* m_trayOnClose = nullptr;
    ToggleSwitch* m_startInTray = nullptr;
    ToggleSwitch* m_autostart = nullptr;
    ToggleSwitch* m_autoQuarantine = nullptr;
    ToggleSwitch* m_heuristics = nullptr;
    ToggleSwitch* m_realtime = nullptr;
    ToggleSwitch* m_archives = nullptr;
    ToggleSwitch* m_procControl = nullptr;
    ToggleSwitch* m_watchTemp = nullptr;
    ToggleSwitch* m_watchUsb = nullptr;
    ToggleSwitch* m_selfProtect = nullptr;
    ToggleSwitch* m_scheduleOn = nullptr;
    QComboBox* m_schedPeriod = nullptr;
    QComboBox* m_schedDay = nullptr;
    QComboBox* m_schedScan = nullptr;
    QTimeEdit* m_schedTime = nullptr;
    QLabel* m_schedInfo = nullptr;
    QListWidget* m_folderList = nullptr;
    QLabel* m_etaLabel = nullptr;
    bcg::ProcessMonitor* m_procMonitor = nullptr;
    QString m_watchSignature; // what the real-time monitor is watching now
    int m_watchCount = 0;
    qint64 m_total = 0;
    bool m_totalFinal = false;
    ToggleSwitch* m_vtAuto = nullptr;
    QLineEdit* m_dbKeyEdit = nullptr;
    QLineEdit* m_vtKeyEdit = nullptr;
    QPlainTextEdit* m_feedsEdit = nullptr;
    QLabel* m_dbInfoLabel = nullptr;
    QLabel* m_dbStatusLabel = nullptr;
    QLabel* m_yaraInfoLabel = nullptr;
    QLabel* m_yaraStatusLabel = nullptr;
    FancyButton* m_updateDbBtn = nullptr;
    FancyButton* m_yaraBtn = nullptr;
    FancyButton* m_vtBtn = nullptr;
    HistoryPage* m_historyPage = nullptr;
    bcg::RealtimeMonitor* m_monitor = nullptr;
    bcg::DatabaseUpdater* m_updater = nullptr;
    bcg::FeedUpdater* m_feeds = nullptr;
    bcg::ReputationService* m_reputation = nullptr;
    bcg::History* m_history = nullptr;
    bool m_silentUpdate = false;
    bool m_updateAnyOk = false;
    QStringList m_updateReport; // lines of the update in progress
    qint64 m_nextToken = 1;
    FancyButton* m_toQuarantineBtn = nullptr;
    FancyButton* m_excludeThreatBtn = nullptr;
    ExclusionsPage* m_exclusionsPage = nullptr;
    QuarantinePage* m_quarantinePage = nullptr;
    bcg::EngineLoadReport m_loadReport;
    bcg::ScanContext m_ctx;

    std::shared_ptr<bcg::ExclusionList> m_exclusions;
    bcg::IScanner* m_scanner = nullptr;
    QThread* m_thread = nullptr;
    bool m_scanning = false;
    bool m_quitting = false;
    bool m_fadedIn = false;
    qint64 m_scanned = 0;
    qint64 m_threats = 0;
    qint64 m_durationMs = -1; // last finished scan; -1 before the first one
    QElapsedTimer m_elapsed;
};
