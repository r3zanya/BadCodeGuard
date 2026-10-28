#include "MainWindow.h"

#include "engine/EngineSetup.h"
#include "engine/Log.h"
#include "engine/ProcessMonitor.h"
#include "engine/RealtimeMonitor.h"
#include "sys/Autostart.h"
#include "sys/Drives.h"
#include "sys/Schedule.h"
#include "sys/SecureStore.h"
#include "sys/SelfProtect.h"
#include "sys/DatabaseUpdater.h"
#include "sys/FeedUpdater.h"
#include "sys/History.h"
#include "sys/ReputationService.h"
#include "ui/pages/HistoryPage.h"
#include "ui/Theme.h"
#include "ui/pages/AdvancedPage.h"
#include "ui/pages/ExclusionsPage.h"
#include "ui/pages/QuarantinePage.h"
#include "ui/widgets/ConfirmDialog.h"
#include "ui/widgets/FadeStack.h"
#include "ui/widgets/FadeLabel.h"
#include "ui/widgets/FancyButton.h"
#include "ui/widgets/GlowProgress.h"
#include "ui/widgets/Icons.h"
#include "ui/widgets/LogoGlow.h"
#include "ui/widgets/NavButton.h"
#include "ui/widgets/NavHighlight.h"
#include "ui/widgets/SmoothScrollArea.h"
#include "ui/widgets/StatusBadge.h"
#include "ui/widgets/Toast.h"
#include "ui/widgets/ToggleSwitch.h"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QTimeEdit>
#include <QColor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEasingCurve>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QSignalBlocker>
#include <QSettings>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QThread>
#include <QTimer>
#include <QVariantAnimation>
#include <QUrl>
#include <QVBoxLayout>

namespace th = bcg::theme;

namespace {

constexpr const char* kKeyTrayOnClose = "trayOnClose";
constexpr const char* kKeyStartInTray = "startInTray";
constexpr const char* kKeyLastScan = "lastScan";
constexpr const char* kKeyAutoQuarantine = "autoQuarantine";
constexpr const char* kKeyHeuristics = "heuristics";
constexpr const char* kKeyRealtime = "realtime";
constexpr const char* kKeyDbKey = "dbKey";
constexpr const char* kKeyDbUpdated = "dbUpdated";
constexpr const char* kKeyArchives = "archives";
constexpr const char* kKeyVtKey = "vtKey";
constexpr const char* kKeyVtAuto = "vtAuto";
constexpr const char* kKeyFeeds = "feeds";
constexpr const char* kKeyProcMon = "procMonitor";
constexpr const char* kKeySelfProtect = "selfProtect";
constexpr const char* kKeyWatchTemp = "watchTemp";
constexpr const char* kKeyWatchUsb = "watchUsb";
constexpr const char* kKeyWatchFolders = "watchFolders";
constexpr const char* kKeyScheduleLast = "schedule/last";
constexpr qint64 kDbMaxAgeSecs = 24 * 60 * 60;

constexpr int kRoleQuarantineId = Qt::UserRole + 1;
constexpr int kRolePath = Qt::UserRole + 2;
constexpr int kRoleName = Qt::UserRole + 3;
constexpr int kRoleKind = Qt::UserRole + 4;
constexpr int kRoleBaseText = Qt::UserRole + 5;

// API keys are kept encrypted with the Windows user's key (DPAPI).
QString secret(const char* key)
{
    QSettings s;
    return bcg::secure::readSecret(s, QString::fromLatin1(key));
}

void setSecret(const char* key, const QString& value)
{
    QSettings s;
    bcg::secure::writeSecret(s, QString::fromLatin1(key), value);
}

QWidget* makePage(QVBoxLayout*& layout)
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("page"));
    layout = new QVBoxLayout(page);
    layout->setContentsMargins(36, 32, 36, 28);
    layout->setSpacing(18);
    return page;
}

QLabel* makeLabel(const QString& text, const char* objectName)
{
    auto* label = new QLabel(text);
    label->setObjectName(QString::fromLatin1(objectName));
    return label;
}

QFrame* makeCard()
{
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("card"));
    return card;
}

QWidget* makeStat(const QString& title, QLabel*& value)
{
    auto* tile = new QFrame;
    tile->setObjectName(QStringLiteral("tile"));
    auto* layout = new QVBoxLayout(tile);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(2);
    layout->addWidget(makeLabel(title, "muted"));
    value = makeLabel(QStringLiteral("0"), "stat");
    layout->addWidget(value);
    return tile;
}

QWidget* makeSettingRow(const QString& title, const QString& description, ToggleSwitch* toggle)
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(20, 16, 20, 16);
    auto* texts = new QVBoxLayout;
    texts->setSpacing(2);
    texts->addWidget(makeLabel(title, "rowTitle"));
    auto* descriptionLabel = makeLabel(description, "muted");
    descriptionLabel->setWordWrap(true);
    texts->addWidget(descriptionLabel);
    layout->addLayout(texts, 1);
    layout->addWidget(toggle);
    return row;
}

void addDivider(QVBoxLayout* layout)
{
    auto* divider = new QFrame;
    divider->setObjectName(QStringLiteral("divider"));
    divider->setFixedHeight(1);
    layout->addWidget(divider);
}

QString formatDuration(qint64 ms)
{
    const qint64 s = ms / 1000;
    return QStringLiteral("%1:%2")
        .arg(s / 60, 2, 10, QLatin1Char('0'))
        .arg(s % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("BadCode Guard"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    resize(1040, 680);
    setMinimumSize(900, 600);

    m_exclusions = std::make_shared<bcg::ExclusionList>();
    ExclusionsPage::load(*m_exclusions);
    m_ctx = bcg::buildContext(m_exclusions, &m_loadReport);
    {
        QSettings settings;
        m_ctx.options->autoQuarantine = settings.value(kKeyAutoQuarantine, true).toBool();
        m_ctx.options->heuristics = settings.value(kKeyHeuristics, true).toBool();
        m_ctx.options->archives = settings.value(kKeyArchives, true).toBool();
    }

    for (const QString& e : std::as_const(m_loadReport.errors))
        bcg::log::warn(QStringLiteral("Файл правил не загружен: %1").arg(e));
    if (m_loadReport.yaraFailed > 0)
        bcg::log::warn(QStringLiteral("Файлов правил YARA не скомпилировалось: %1").arg(m_loadReport.yaraFailed));

    m_history = new bcg::History(bcg::dataFolder() + QStringLiteral("/history.json"), this);
    m_reputation = new bcg::ReputationService(this);
    m_reputation->setApiKey(secret(kKeyVtKey));
    connect(m_reputation, &bcg::ReputationService::result, this, &MainWindow::onReputation);
    m_feeds = new bcg::FeedUpdater(this);
    connect(m_feeds, &bcg::FeedUpdater::finished, this, &MainWindow::onFeedFinished);

    m_monitor = new bcg::RealtimeMonitor(m_ctx, this);
    connect(m_monitor, &bcg::RealtimeMonitor::threatFound, this, &MainWindow::onRealtimeThreat);
    m_updater = new bcg::DatabaseUpdater(this);
    connect(m_updater, &bcg::DatabaseUpdater::finished, this, &MainWindow::onDatabaseUpdated);
    m_procMonitor = new bcg::ProcessMonitor(m_ctx, this);
    connect(m_procMonitor, &bcg::ProcessMonitor::threatFound, this, &MainWindow::onProcessThreat);

    buildUi();
    applyRealtime(QSettings().value(kKeyRealtime, true).toBool());
    if (QSettings().value(kKeyProcMon, true).toBool())
        m_procMonitor->start();
    if (QSettings().value(kKeySelfProtect, false).toBool())
        bcg::selfprotect::enable();
    refreshProtectionRows();
    buildTray();
    startEngineThread();
    updateLastScanLabel();
    bcg::log::info(QStringLiteral("Программа запущена, версия %1").arg(QApplication::applicationVersion()));

    // Keep the hash list fresh: check shortly after start, then every hour.
    QTimer::singleShot(8000, this, &MainWindow::autoUpdateDatabase);
    auto* dbTimer = new QTimer(this);
    dbTimer->setInterval(60 * 60 * 1000);
    connect(dbTimer, &QTimer::timeout, this, &MainWindow::autoUpdateDatabase);
    dbTimer->start();

    // Watchdog for real-time folders: picks up USB sticks that come and go and
    // restarts a listener that died (a folder that was deleted and recreated).
    auto* watchTimer = new QTimer(this);
    watchTimer->setInterval(10 * 1000);
    connect(watchTimer, &QTimer::timeout, this, [this] { refreshWatchers(false); });
    watchTimer->start();

    // Scheduled scans: look at the clock twice a minute, and shortly after start
    // for a run that was missed while the computer was off.
    auto* scheduleTimer = new QTimer(this);
    scheduleTimer->setInterval(30 * 1000);
    connect(scheduleTimer, &QTimer::timeout, this, &MainWindow::checkSchedule);
    scheduleTimer->start();
    QTimer::singleShot(20 * 1000, this, &MainWindow::checkSchedule);
}

MainWindow::~MainWindow()
{
    m_monitor->stop();
    m_procMonitor->stop();
    if (m_scanner)
        m_scanner->cancel();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
    }
}

bool MainWindow::startMinimized(bool launchedByAutostart) const
{
    return QSystemTrayIcon::isSystemTrayAvailable()
        && (launchedByAutostart || QSettings().value(kKeyStartInTray, false).toBool());
}

void MainWindow::startEngineThread()
{
    m_scanner = new bcg::FileScanner(m_ctx);
    m_thread = new QThread(this);
    m_scanner->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_scanner, &QObject::deleteLater);

    connect(m_scanner, &bcg::IScanner::progress, this, &MainWindow::onProgress);
    connect(m_scanner, &bcg::IScanner::threatFound, this, &MainWindow::onThreatFound);
    connect(m_scanner, &bcg::IScanner::finished, this, &MainWindow::onFinished);
    connect(m_scanner, &bcg::IScanner::totalChanged, this, &MainWindow::onTotal);
    connect(m_scanner, &bcg::IScanner::note, this, [this](const QString& text) {
        appendLog(text);
        bcg::log::info(text);
    });
    m_thread->start();
}

void MainWindow::buildUi()
{
    auto* central = new QWidget;
    auto* root = new QHBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_stack = new FadeStack;
    m_stack->addWidget(buildDashboardPage());
    m_stack->addWidget(buildScanPage());
    m_stack->addWidget(buildQuarantinePage());
    m_historyPage = new HistoryPage(m_history);
    m_stack->addWidget(m_historyPage);
    m_exclusionsPage = new ExclusionsPage(m_exclusions);
    m_stack->addWidget(m_exclusionsPage);
    connect(m_quarantinePage, &QuarantinePage::excludeRequested, this, [this](const QString& path) {
        m_exclusionsPage->addExclusion({bcg::Exclusion::File, path});
    });
    m_stack->addWidget(new AdvancedPage);
    m_stack->addWidget(buildSettingsPage());

    root->addWidget(buildSidebar());
    root->addWidget(m_stack, 1);
    setCentralWidget(central);
}

QWidget* MainWindow::buildSidebar()
{
    auto* sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(232);
    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(16, 26, 16, 18);
    layout->setSpacing(4);

    auto* brandRow = new QHBoxLayout;
    brandRow->setContentsMargins(6, 0, 0, 0);
    brandRow->setSpacing(10);
    auto* logo = new QLabel;
    logo->setPixmap(QPixmap(QStringLiteral(":/icons/symbol.png"))
                        .scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brandRow->addWidget(logo);
    brandRow->addWidget(makeLabel(QStringLiteral("BadCode\nGuard"), "brand"));
    brandRow->addStretch();
    layout->addLayout(brandRow);
    layout->addSpacing(26);

    m_highlight = new NavHighlight(sidebar);
    m_navGroup = new QButtonGroup(this);
    m_navGroup->setExclusive(true);
    const struct {
        Page page;
        const char* text;
        const char* icon;
    } items[] = {
        {PageDashboard, "Защита", "shield"},
        {PageScan, "Проверка", "scan"},
        {PageQuarantine, "Карантин", "quarantine"},
        {PageHistory, "Журнал", "file"},
        {PageExclusions, "Исключения", "exclude"},
        {PageAdvanced, "Параметры системы", "chip"},
        {PageSettings, "Настройки", "settings"},
    };
    for (const auto& item : items) {
        auto* button = new NavButton(QString::fromUtf8(item.text), QString::fromLatin1(item.icon));
        m_navGroup->addButton(button, item.page);
        layout->addWidget(button);
        connect(button, &QAbstractButton::toggled, this, [this](bool on) {
            if (on)
                placeHighlight(true);
        });
    }
    m_navGroup->button(PageDashboard)->setChecked(true);
    connect(m_navGroup, &QButtonGroup::idClicked, this,
            [this](int id) { showPage(static_cast<Page>(id)); });

    layout->addStretch();
    auto* version = makeLabel(QStringLiteral("Версия %1").arg(QApplication::applicationVersion()),
                              "version");
    version->setContentsMargins(8, 0, 0, 0);
    layout->addWidget(version);
    return sidebar;
}

QWidget* MainWindow::buildDashboardPage()
{
    QVBoxLayout* layout = nullptr;
    QWidget* page = makePage(layout);

    layout->addWidget(makeLabel(QStringLiteral("Защита"), "title"));

    QFrame* card = makeCard();
    auto* cardLayout = new QHBoxLayout(card);
    cardLayout->setContentsMargins(24, 8, 28, 8);
    cardLayout->setSpacing(20);
    card->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_glow = new LogoGlow;
    cardLayout->addWidget(m_glow);

    auto* text = new QVBoxLayout;
    text->setSpacing(6);
    text->addStretch();
    m_statusLabel = new FadeLabel;
    m_statusLabel->setObjectName(QStringLiteral("status"));
    text->addWidget(m_statusLabel);
    m_lastScanLabel = makeLabel(QString(), "muted");
    text->addWidget(m_lastScanLabel);
    text->addStretch();
    cardLayout->addLayout(text, 1);
    layout->addWidget(card);
    setStatus(QStringLiteral("Готов к проверке"), th::kAccent);

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(12);
    m_quickBtn = new FancyButton(QStringLiteral("Быстрая проверка"), QStringLiteral("bolt"),
                                 FancyButton::Primary);
    m_fullBtn = new FancyButton(QStringLiteral("Полная проверка"), QStringLiteral("drive"),
                                FancyButton::Secondary);
    m_customBtn = new FancyButton(QStringLiteral("Выбрать папку…"), QStringLiteral("folder"),
                                  FancyButton::Secondary);
    for (auto* b : {m_quickBtn, m_fullBtn, m_customBtn})
        buttons->addWidget(b);
    buttons->addStretch();
    layout->addLayout(buttons);

    layout->addSpacing(6);
    layout->addWidget(makeLabel(QStringLiteral("Состояние защиты"), "sectionTitle"));
    QFrame* protection = makeCard();
    m_protectionRows = new QVBoxLayout(protection);
    m_protectionRows->setContentsMargins(0, 0, 0, 0);
    m_protectionRows->setSpacing(0);
    layout->addWidget(protection);
    layout->addStretch();

    connect(m_quickBtn, &QPushButton::clicked, this, &MainWindow::quickScan);
    connect(m_fullBtn, &QPushButton::clicked, this, &MainWindow::fullScan);
    connect(m_customBtn, &QPushButton::clicked, this, &MainWindow::customScan);

    // The list of protection rows is long: let the page scroll in a small window.
    auto* scroll = new SmoothScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(page);
    return scroll;
}

// Rebuilds the dashboard's list of what is actually on and what is not.
void MainWindow::refreshProtectionRows()
{
    using bcg::sys::Status;
    while (QLayoutItem* item = m_protectionRows->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    struct Row {
        QString title;
        QString detail;
        QString badge;
        Status status;
    };
    const bool realtime = m_monitor->isRunning();
    const bool procOn = m_procMonitor->isRunning();
    const bcg::Schedule sched = bcg::Schedule::load(QSettings());
    const bool heuristics = m_ctx.options->heuristics;
    const bool archives = m_ctx.options->archives;
    const bool autoQuarantine = m_ctx.options->autoQuarantine;
    const bool yaraNone = m_loadReport.yaraRules == 0;
    const bool vtReady = m_reputation->hasKey();
    const bool dbThin = m_loadReport.hashes < 100;
    const QList<Row> rows = {
        {QStringLiteral("Защита в реальном времени"),
         realtime ? QStringLiteral("Слежение за папками: %1.").arg(m_watchCount)
                  : QStringLiteral("Файлы проверяются только по запросу."),
         realtime ? QStringLiteral("Работает") : QStringLiteral("Выключена"),
         realtime ? Status::Good : Status::Warn},
        {QStringLiteral("Базы сигнатур"),
         dbThin ? QStringLiteral("Хэшей: %1, правил: %2. Обновите базу в Настройках.")
                      .arg(m_loadReport.hashes)
                      .arg(m_loadReport.rules)
                : QStringLiteral("Хэшей: %1, правил: %2.")
                      .arg(m_loadReport.hashes)
                      .arg(m_loadReport.rules),
         dbThin ? QStringLiteral("Мало данных") : QStringLiteral("Загружены"),
         dbThin ? Status::Warn : Status::Good},
        {QStringLiteral("Контроль запуска программ"),
         procOn ? QStringLiteral("Новые программы проверяются в течение секунды после старта.")
                : QStringLiteral("Запускаемые программы не проверяются."),
         procOn ? QStringLiteral("Работает") : QStringLiteral("Выключен"),
         procOn ? Status::Good : Status::Warn},
        {QStringLiteral("Плановая проверка"),
         sched.enabled ? QStringLiteral("Следующая: %1.")
                               .arg(sched.nextDue(QDateTime::currentDateTime())
                                        .toString(QStringLiteral("dd.MM.yyyy HH:mm")))
                       : QStringLiteral("Не настроена. Можно включить в Настройках."),
         sched.enabled ? QStringLiteral("Включена") : QStringLiteral("Выключена"),
         sched.enabled ? Status::Good : Status::Neutral},
        {QStringLiteral("Правила YARA"),
         yaraNone ? QStringLiteral("Правил нет. Скачайте набор в Настройках.")
                  : QStringLiteral("Правил загружено: %1.").arg(m_loadReport.yaraRules),
         yaraNone ? QStringLiteral("Нет правил") : QStringLiteral("Загружены"),
         yaraNone ? Status::Warn : Status::Good},
        {QStringLiteral("Эвристический анализ"),
         QStringLiteral("Исполняемые файлы, скрипты, ярлыки, PDF и документы Office."),
         heuristics ? QStringLiteral("Включён") : QStringLiteral("Выключен"),
         heuristics ? Status::Good : Status::Warn},
        {QStringLiteral("Проверка архивов"),
         QStringLiteral("Заглядывает внутрь zip, 7z, rar, tar и документов Office."),
         archives ? QStringLiteral("Включена") : QStringLiteral("Выключена"),
         archives ? Status::Good : Status::Warn},
        {QStringLiteral("Репутация (VirusTotal)"),
         vtReady ? QStringLiteral("Подозрительные файлы можно сверить по хэшу.")
                 : QStringLiteral("Нужен ключ VirusTotal в Настройках."),
         vtReady ? QStringLiteral("Подключена") : QStringLiteral("Не настроена"),
         vtReady ? Status::Good : Status::Neutral},
        {QStringLiteral("Автоматический карантин"),
         QStringLiteral("Угрозы из базы и правил изолируются сразу."),
         autoQuarantine ? QStringLiteral("Включён") : QStringLiteral("Выключен"),
         autoQuarantine ? Status::Good : Status::Warn},
        {QStringLiteral("Карантин"), QStringLiteral("Изолированные файлы можно восстановить."),
         QStringLiteral("Файлов: %1").arg(m_ctx.quarantine->entries().size()), Status::Neutral},
    };

    for (int i = 0; i < rows.size(); ++i) {
        if (i > 0) {
            auto* divider = new QFrame;
            divider->setObjectName(QStringLiteral("divider"));
            divider->setFixedHeight(1);
            m_protectionRows->addWidget(divider);
        }
        auto* row = new QWidget;
        auto* rowLayout = new QHBoxLayout(row);
        row->setMinimumHeight(56);
        rowLayout->setContentsMargins(20, 8, 20, 8);
        rowLayout->setSpacing(16);
        auto* texts = new QVBoxLayout;
        texts->setSpacing(3);
        texts->addWidget(makeLabel(rows[i].title, "rowTitle"));
        auto* detail = makeLabel(rows[i].detail, "muted");
        detail->setWordWrap(true);
        texts->addWidget(detail);
        rowLayout->addLayout(texts, 1);
        rowLayout->addWidget(new StatusBadge(rows[i].badge, rows[i].status), 0, Qt::AlignVCenter);
        m_protectionRows->addWidget(row);
    }
}

QWidget* MainWindow::buildScanPage()
{
    QVBoxLayout* layout = nullptr;
    QWidget* page = makePage(layout);

    m_scanTitle = new FadeLabel;
    m_scanTitle->setObjectName(QStringLiteral("title"));
    m_scanTitle->setTextAnimated(QStringLiteral("Проверка"));
    layout->addWidget(m_scanTitle);

    QFrame* card = makeCard();
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 18, 20, 18);
    cardLayout->setSpacing(12);

    m_currentFile = makeLabel(QStringLiteral("Проверка не запущена"), "muted");
    m_currentFile->setMinimumWidth(0);
    m_currentFile->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_progress = new GlowProgress;

    auto* stats = new QHBoxLayout;
    stats->setSpacing(12);
    stats->addWidget(makeStat(QStringLiteral("Файлов"), m_filesValue), 1);
    stats->addWidget(makeStat(QStringLiteral("Угроз"), m_threatsValue), 1);
    stats->addWidget(makeStat(QStringLiteral("Время"), m_timeValue), 1);
    m_cancelBtn = new FancyButton(QStringLiteral("Отменить"), QStringLiteral("cancel"),
                                  FancyButton::Secondary);
    m_cancelBtn->setEnabled(false);
    stats->addWidget(m_cancelBtn, 0, Qt::AlignVCenter);

    m_etaLabel = makeLabel(QString(), "muted");
    cardLayout->addWidget(m_currentFile);
    cardLayout->addWidget(m_progress);
    cardLayout->addWidget(m_etaLabel);
    cardLayout->addLayout(stats);
    layout->addWidget(card);

    layout->addWidget(makeLabel(QStringLiteral("Найденные угрозы"), "muted"));
    m_threatList = new QListWidget;
    m_threatList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_threatList, 2);

    auto* threatActions = new QHBoxLayout;
    threatActions->setSpacing(10);
    m_toQuarantineBtn = new FancyButton(QStringLiteral("В карантин"), QStringLiteral("quarantine"),
                                        FancyButton::Secondary);
    m_excludeThreatBtn = new FancyButton(QStringLiteral("Добавить в исключения"),
                                         QStringLiteral("exclude"), FancyButton::Secondary);
    m_vtBtn = new FancyButton(QStringLiteral("Проверить в VirusTotal"), QStringLiteral("scan"),
                              FancyButton::Secondary);
    m_vtBtn->setToolTip(QStringLiteral("Отправляется только хэш файла, сам файл остаётся на компьютере"));
    threatActions->addWidget(m_toQuarantineBtn);
    threatActions->addWidget(m_excludeThreatBtn);
    threatActions->addWidget(m_vtBtn);
    threatActions->addStretch();
    layout->addLayout(threatActions);
    m_toQuarantineBtn->setEnabled(false);
    m_excludeThreatBtn->setEnabled(false);
    m_vtBtn->setEnabled(false);
    connect(m_vtBtn, &QPushButton::clicked, this, &MainWindow::lookupSelected);
    connect(m_threatList, &QListWidget::itemSelectionChanged, this, &MainWindow::updateThreatButtons);
    connect(m_toQuarantineBtn, &QPushButton::clicked, this, &MainWindow::quarantineSelected);
    connect(m_excludeThreatBtn, &QPushButton::clicked, this, &MainWindow::excludeSelected);

    layout->addWidget(makeLabel(QStringLiteral("Журнал"), "muted"));
    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    layout->addWidget(m_log, 1);

    connect(m_cancelBtn, &QPushButton::clicked, this, [this] {
        m_cancelBtn->setEnabled(false);
        appendLog(QStringLiteral("Отмена…"));
        m_scanner->cancel();
    });
    m_clock = new QTimer(this);
    m_clock->setInterval(250);
    connect(m_clock, &QTimer::timeout, this, [this] { updateCounters(); });
    updateCounters(true);
    return page;
}

QWidget* MainWindow::buildQuarantinePage()
{
    m_quarantinePage = new QuarantinePage(m_ctx.quarantine);
    return m_quarantinePage;
}

QWidget* MainWindow::buildSettingsPage()
{
    QVBoxLayout* layout = nullptr;
    QWidget* page = makePage(layout);

    layout->addWidget(makeLabel(QStringLiteral("Настройки"), "title"));

    QSettings settings;
    m_trayOnClose = new ToggleSwitch;
    m_trayOnClose->setCheckedInstant(settings.value(kKeyTrayOnClose, true).toBool());
    m_startInTray = new ToggleSwitch;
    m_startInTray->setCheckedInstant(settings.value(kKeyStartInTray, false).toBool());
    m_autostart = new ToggleSwitch;
    m_autostart->setCheckedInstant(bcg::autostart::isEnabled());

    QFrame* card = makeCard();
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);
    cardLayout->addWidget(makeSettingRow(
        QStringLiteral("Запускать вместе с Windows"),
        QStringLiteral("Программа стартует при входе в систему и сразу уходит в трей."),
        m_autostart));
    auto* autostartDivider = new QFrame;
    autostartDivider->setObjectName(QStringLiteral("divider"));
    autostartDivider->setFixedHeight(1);
    cardLayout->addWidget(autostartDivider);
    cardLayout->addWidget(makeSettingRow(
        QStringLiteral("Сворачивать в трей"),
        QStringLiteral("Закрытие окна не завершает программу, она остаётся в трее."),
        m_trayOnClose));
    auto* divider = new QFrame;
    divider->setObjectName(QStringLiteral("divider"));
    divider->setFixedHeight(1);
    cardLayout->addWidget(divider);
    cardLayout->addWidget(makeSettingRow(QStringLiteral("Запускать в трее"),
                                         QStringLiteral("При старте окно не показывается."),
                                         m_startInTray));
    layout->addWidget(card);

    m_autoQuarantine = new ToggleSwitch;
    m_autoQuarantine->setCheckedInstant(m_ctx.options->autoQuarantine);
    m_heuristics = new ToggleSwitch;
    m_heuristics->setCheckedInstant(m_ctx.options->heuristics);
    QFrame* engineCard = makeCard();
    auto* engineLayout = new QVBoxLayout(engineCard);
    engineLayout->setContentsMargins(0, 0, 0, 0);
    engineLayout->setSpacing(0);
    m_realtime = new ToggleSwitch;
    m_realtime->setCheckedInstant(QSettings().value(kKeyRealtime, true).toBool());
    engineLayout->addWidget(makeSettingRow(
        QStringLiteral("Защита в реальном времени"),
        QStringLiteral("Следит за «Загрузками» и «Рабочим столом»: новые и изменённые файлы "
                       "проверяются сразу."),
        m_realtime));
    addDivider(engineLayout);

    // More folders for real-time protection.
    m_watchTemp = new ToggleSwitch;
    m_watchTemp->setCheckedInstant(QSettings().value(kKeyWatchTemp, true).toBool());
    engineLayout->addWidget(makeSettingRow(
        QStringLiteral("Следить за папкой Temp"),
        QStringLiteral("Туда попадают распакованные установщики и вложения. Из-за большого числа "
                       "временных файлов проверяются только программы, скрипты, архивы и документы."),
        m_watchTemp));
    addDivider(engineLayout);
    m_watchUsb = new ToggleSwitch;
    m_watchUsb->setCheckedInstant(QSettings().value(kKeyWatchUsb, true).toBool());
    engineLayout->addWidget(makeSettingRow(
        QStringLiteral("Следить за съёмными дисками"),
        QStringLiteral("Флешки и карты памяти подхватываются сами, как только их вставили."),
        m_watchUsb));
    addDivider(engineLayout);
    auto* foldersRow = new QWidget;
    auto* foldersLayout = new QVBoxLayout(foldersRow);
    foldersLayout->setContentsMargins(20, 16, 20, 16);
    foldersLayout->setSpacing(8);
    foldersLayout->addWidget(makeLabel(QStringLiteral("Свои папки для слежения"), "rowTitle"));
    auto* foldersHint = makeLabel(
        QStringLiteral("Файлы в этих папках (и вложенных) проверяются сразу после появления."),
        "muted");
    foldersHint->setWordWrap(true);
    foldersLayout->addWidget(foldersHint);
    m_folderList = new QListWidget;
    m_folderList->setFixedHeight(96);
    m_folderList->addItems(QSettings().value(kKeyWatchFolders).toStringList());
    foldersLayout->addWidget(m_folderList);
    auto* folderButtons = new QHBoxLayout;
    folderButtons->setSpacing(10);
    auto* addFolder = new FancyButton(QStringLiteral("Добавить папку…"), QStringLiteral("plus"),
                                      FancyButton::Secondary);
    auto* removeFolder = new FancyButton(QStringLiteral("Убрать"), QStringLiteral("trash"),
                                         FancyButton::Secondary);
    folderButtons->addWidget(addFolder);
    folderButtons->addWidget(removeFolder);
    folderButtons->addStretch();
    foldersLayout->addLayout(folderButtons);
    engineLayout->addWidget(foldersRow);
    addDivider(engineLayout);
    m_procControl = new ToggleSwitch;
    m_procControl->setCheckedInstant(QSettings().value(kKeyProcMon, true).toBool());
    engineLayout->addWidget(makeSettingRow(
        QStringLiteral("Контроль запуска программ"),
        QStringLiteral("Каждая запущенная программа проверяется в течение секунды. Угроза из базы, "
                       "правил или YARA завершается и изолируется, подозрительная только "
                       "показывается. Совсем короткие программы могут успеть отработать."),
        m_procControl));
    addDivider(engineLayout);
    connect(m_watchTemp, &QAbstractButton::toggled, this, [this](bool on) {
        QSettings().setValue(kKeyWatchTemp, on);
        refreshWatchers(false);
    });
    connect(m_watchUsb, &QAbstractButton::toggled, this, [this](bool on) {
        QSettings().setValue(kKeyWatchUsb, on);
        refreshWatchers(false);
    });
    connect(addFolder, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Выберите папку"));
        if (dir.isEmpty())
            return;
        const QString native = QDir::toNativeSeparators(dir);
        for (int i = 0; i < m_folderList->count(); ++i) {
            if (m_folderList->item(i)->text().compare(native, Qt::CaseInsensitive) == 0)
                return;
        }
        m_folderList->addItem(native);
        QStringList all;
        for (int i = 0; i < m_folderList->count(); ++i)
            all << m_folderList->item(i)->text();
        QSettings().setValue(kKeyWatchFolders, all);
        refreshWatchers(false);
    });
    connect(removeFolder, &QPushButton::clicked, this, [this] {
        qDeleteAll(m_folderList->selectedItems());
        QStringList all;
        for (int i = 0; i < m_folderList->count(); ++i)
            all << m_folderList->item(i)->text();
        QSettings().setValue(kKeyWatchFolders, all);
        refreshWatchers(false);
    });
    connect(m_procControl, &QAbstractButton::toggled, this, [this](bool on) {
        QSettings().setValue(kKeyProcMon, on);
        if (on)
            m_procMonitor->start();
        else
            m_procMonitor->stop();
        refreshProtectionRows();
    });
    engineLayout->addWidget(makeSettingRow(
        QStringLiteral("Автоматически в карантин"),
        QStringLiteral("Угрозы, найденные по базе и правилам, изолируются сразу. Эвристические "
                       "находки только показываются."),
        m_autoQuarantine));
    auto* engineDivider = new QFrame;
    engineDivider->setObjectName(QStringLiteral("divider"));
    engineDivider->setFixedHeight(1);
    engineLayout->addWidget(engineDivider);
    engineLayout->addWidget(makeSettingRow(
        QStringLiteral("Эвристический анализ"),
        QStringLiteral("Поиск подозрительных программ, скриптов, ярлыков, PDF и документов Office. "
                       "Подписанные программы и файлы Windows не трогаются."),
        m_heuristics));
    auto* archivesDivider = new QFrame;
    archivesDivider->setObjectName(QStringLiteral("divider"));
    archivesDivider->setFixedHeight(1);
    engineLayout->addWidget(archivesDivider);
    m_archives = new ToggleSwitch;
    m_archives->setCheckedInstant(m_ctx.options->archives);
    engineLayout->addWidget(makeSettingRow(
        QStringLiteral("Проверять архивы"),
        QStringLiteral("Заглядывает внутрь zip, 7z, rar, tar и документов Office. Вложенность до 3 "
                       "уровней, распаковка ограничена по размеру. Архивы с паролем не проверяются."),
        m_archives));
    auto* dbDivider = new QFrame;
    dbDivider->setObjectName(QStringLiteral("divider"));
    dbDivider->setFixedHeight(1);
    engineLayout->addWidget(dbDivider);
    auto* dbRow = new QWidget;
    auto* dbLayout = new QHBoxLayout(dbRow);
    dbLayout->setContentsMargins(20, 16, 20, 16);
    auto* dbTexts = new QVBoxLayout;
    dbTexts->setSpacing(2);
    dbTexts->addWidget(makeLabel(QStringLiteral("Базы сигнатур"), "rowTitle"));
    m_dbInfoLabel = makeLabel(QString(), "muted");
    m_dbInfoLabel->setWordWrap(true);
    dbTexts->addWidget(m_dbInfoLabel);
    refreshDbInfo();
    for (const QString& error : m_loadReport.errors) {
        auto* warn = makeLabel(error, "warnText");
        warn->setWordWrap(true);
        dbTexts->addWidget(warn);
    }
    dbLayout->addLayout(dbTexts, 1);
    auto* openDb = new FancyButton(QStringLiteral("Открыть папку"), QStringLiteral("folder"),
                                   FancyButton::Secondary);
    dbLayout->addWidget(openDb);
    engineLayout->addWidget(dbRow);

    auto* updateDivider = new QFrame;
    updateDivider->setObjectName(QStringLiteral("divider"));
    updateDivider->setFixedHeight(1);
    engineLayout->addWidget(updateDivider);
    auto* updateRow = new QWidget;
    auto* updateLayout = new QVBoxLayout(updateRow);
    updateLayout->setContentsMargins(20, 16, 20, 16);
    updateLayout->setSpacing(8);
    updateLayout->addWidget(makeLabel(QStringLiteral("Обновление базы (MalwareBazaar)"), "rowTitle"));
    auto* updateHint = makeLabel(
        QStringLiteral("Вставьте ключ abuse.ch или ссылку на экспорт. Ключ хранится только на этом "
                       "компьютере. Список обновляется сам раз в сутки."),
        "muted");
    updateHint->setWordWrap(true);
    updateLayout->addWidget(updateHint);
    auto* keyRow = new QHBoxLayout;
    keyRow->setSpacing(10);
    m_dbKeyEdit = new QLineEdit;
    m_dbKeyEdit->setEchoMode(QLineEdit::Password);
    m_dbKeyEdit->setPlaceholderText(QStringLiteral("Ключ abuse.ch или ссылка"));
    m_dbKeyEdit->setText(secret(kKeyDbKey));
    keyRow->addWidget(m_dbKeyEdit, 1);
    m_updateDbBtn = new FancyButton(QStringLiteral("Обновить базу"), QStringLiteral("download"),
                                    FancyButton::Primary);
    keyRow->addWidget(m_updateDbBtn);
    updateLayout->addLayout(keyRow);
    updateLayout->addWidget(makeLabel(
        QStringLiteral("Другие списки хэшей SHA-256: ссылки https, по одной в строке"), "muted"));
    m_feedsEdit = new QPlainTextEdit;
    m_feedsEdit->setFixedHeight(76);
    m_feedsEdit->setPlaceholderText(QStringLiteral("https://…/hashes.txt"));
    m_feedsEdit->setPlainText(QSettings().value(kKeyFeeds).toStringList().join(QLatin1Char('\n')));
    updateLayout->addWidget(m_feedsEdit);
    m_dbStatusLabel = makeLabel(QString(), "muted");
    m_dbStatusLabel->setWordWrap(true);
    updateLayout->addWidget(m_dbStatusLabel);
    engineLayout->addWidget(updateRow);

    // YARA rules.
    auto* yaraDivider = new QFrame;
    yaraDivider->setObjectName(QStringLiteral("divider"));
    yaraDivider->setFixedHeight(1);
    engineLayout->addWidget(yaraDivider);
    auto* yaraRow = new QWidget;
    auto* yaraLayout = new QVBoxLayout(yaraRow);
    yaraLayout->setContentsMargins(20, 16, 20, 16);
    yaraLayout->setSpacing(8);
    yaraLayout->addWidget(makeLabel(QStringLiteral("Правила YARA"), "rowTitle"));
    auto* yaraHint = makeLabel(
        QStringLiteral("Свои файлы *.yar кладите в папку правил. Набор сообщества Yara-Rules "
                       "(раздел malware) скачивается с GitHub. Чужие правила иногда срабатывают на "
                       "безопасные файлы: проверяйте находки перед удалением."),
        "muted");
    yaraHint->setWordWrap(true);
    yaraLayout->addWidget(yaraHint);
    m_yaraInfoLabel = makeLabel(QString(), "muted");
    refreshYaraInfo();
    yaraLayout->addWidget(m_yaraInfoLabel);
    auto* yaraButtons = new QHBoxLayout;
    yaraButtons->setSpacing(10);
    m_yaraBtn = new FancyButton(QStringLiteral("Скачать набор правил"), QStringLiteral("download"),
                                FancyButton::Primary);
    auto* openYara = new FancyButton(QStringLiteral("Открыть папку"), QStringLiteral("folder"),
                                     FancyButton::Secondary);
    yaraButtons->addWidget(m_yaraBtn);
    yaraButtons->addWidget(openYara);
    yaraButtons->addStretch();
    yaraLayout->addLayout(yaraButtons);
    m_yaraStatusLabel = makeLabel(QString(), "muted");
    m_yaraStatusLabel->setWordWrap(true);
    yaraLayout->addWidget(m_yaraStatusLabel);
    engineLayout->addWidget(yaraRow);
    connect(m_yaraBtn, &QPushButton::clicked, this, [this] {
        if (m_feeds->busy())
            return;
        m_yaraBtn->setEnabled(false);
        m_yaraStatusLabel->setStyleSheet(QString());
        m_yaraStatusLabel->setText(QStringLiteral("Загрузка…"));
        m_feeds->updateYaraRules();
    });
    connect(openYara, &QPushButton::clicked, this, [] {
        QDir().mkpath(bcg::yaraFolder());
        QDesktopServices::openUrl(QUrl::fromLocalFile(bcg::yaraFolder()));
    });

    // Reputation.
    auto* vtDivider = new QFrame;
    vtDivider->setObjectName(QStringLiteral("divider"));
    vtDivider->setFixedHeight(1);
    engineLayout->addWidget(vtDivider);
    auto* vtRow = new QWidget;
    auto* vtLayout = new QVBoxLayout(vtRow);
    vtLayout->setContentsMargins(20, 16, 20, 16);
    vtLayout->setSpacing(8);
    vtLayout->addWidget(makeLabel(QStringLiteral("Репутация файлов (VirusTotal)"), "rowTitle"));
    auto* vtHint = makeLabel(
        QStringLiteral("Подозрительный файл можно сверить с базой VirusTotal. Отправляется только "
                       "хэш файла, сам файл остаётся на компьютере. Бесплатный ключ даёт 4 запроса "
                       "в минуту."),
        "muted");
    vtHint->setWordWrap(true);
    vtLayout->addWidget(vtHint);
    m_vtKeyEdit = new QLineEdit;
    m_vtKeyEdit->setEchoMode(QLineEdit::Password);
    m_vtKeyEdit->setPlaceholderText(QStringLiteral("Ключ API VirusTotal"));
    m_vtKeyEdit->setText(secret(kKeyVtKey));
    vtLayout->addWidget(m_vtKeyEdit);
    engineLayout->addWidget(vtRow);
    auto* vtAutoDivider = new QFrame;
    vtAutoDivider->setObjectName(QStringLiteral("divider"));
    vtAutoDivider->setFixedHeight(1);
    engineLayout->addWidget(vtAutoDivider);
    m_vtAuto = new ToggleSwitch;
    m_vtAuto->setCheckedInstant(QSettings().value(kKeyVtAuto, false).toBool());
    engineLayout->addWidget(makeSettingRow(
        QStringLiteral("Сверять подозрительные файлы автоматически"),
        QStringLiteral("Хэш каждого файла, найденного эвристикой, уходит в VirusTotal. Выключено по "
                       "умолчанию."),
        m_vtAuto));
    connect(m_vtKeyEdit, &QLineEdit::editingFinished, this, [this] {
        const QString key = m_vtKeyEdit->text().trimmed();
        setSecret(kKeyVtKey, key);
        m_reputation->setApiKey(key);
        refreshProtectionRows();
    });
    connect(m_vtAuto, &QAbstractButton::toggled, this,
            [](bool on) { QSettings().setValue(kKeyVtAuto, on); });
    connect(m_archives, &QAbstractButton::toggled, this, [this](bool on) {
        m_ctx.options->archives = on;
        QSettings().setValue(kKeyArchives, on);
        refreshProtectionRows();
    });
    connect(m_updateDbBtn, &QPushButton::clicked, this, [this] { updateDatabase(false); });
    connect(m_dbKeyEdit, &QLineEdit::returnPressed, this, [this] { updateDatabase(false); });
    layout->addWidget(engineCard);

    // Schedule, self-protection and the program's own log.
    QFrame* careCard = makeCard();
    auto* careLayout = new QVBoxLayout(careCard);
    careLayout->setContentsMargins(0, 0, 0, 0);
    careLayout->setSpacing(0);
    const bcg::Schedule schedule = bcg::Schedule::load(QSettings());
    m_scheduleOn = new ToggleSwitch;
    m_scheduleOn->setCheckedInstant(schedule.enabled);
    careLayout->addWidget(makeSettingRow(
        QStringLiteral("Плановая проверка"),
        QStringLiteral("Запускается сама в выбранное время. Если компьютер был выключен, проверка "
                       "пройдёт при следующем включении."),
        m_scheduleOn));
    auto* schedRow = new QWidget;
    auto* schedLayout = new QVBoxLayout(schedRow);
    schedLayout->setContentsMargins(20, 0, 20, 16);
    schedLayout->setSpacing(8);
    auto* schedControls = new QHBoxLayout;
    schedControls->setSpacing(10);
    m_schedPeriod = new QComboBox;
    m_schedPeriod->addItems({QStringLiteral("Каждый день"), QStringLiteral("Раз в неделю")});
    m_schedPeriod->setCurrentIndex(schedule.period == bcg::Schedule::Daily ? 0 : 1);
    m_schedDay = new QComboBox;
    m_schedDay->addItems({QStringLiteral("Понедельник"), QStringLiteral("Вторник"),
                          QStringLiteral("Среда"), QStringLiteral("Четверг"), QStringLiteral("Пятница"),
                          QStringLiteral("Суббота"), QStringLiteral("Воскресенье")});
    m_schedDay->setCurrentIndex(schedule.dayOfWeek - 1);
    m_schedTime = new QTimeEdit(schedule.time);
    m_schedTime->setDisplayFormat(QStringLiteral("HH:mm"));
    m_schedScan = new QComboBox;
    m_schedScan->addItems({QStringLiteral("Быстрая проверка"), QStringLiteral("Полная проверка")});
    m_schedScan->setCurrentIndex(schedule.scan == bcg::Schedule::Quick ? 0 : 1);
    for (QWidget* w : std::initializer_list<QWidget*>{m_schedPeriod, m_schedDay, m_schedTime, m_schedScan})
        schedControls->addWidget(w);
    schedControls->addStretch();
    schedLayout->addLayout(schedControls);
    m_schedInfo = makeLabel(QString(), "muted");
    m_schedInfo->setWordWrap(true);
    schedLayout->addWidget(m_schedInfo);
    careLayout->addWidget(schedRow);
    addDivider(careLayout);
    connect(m_scheduleOn, &QAbstractButton::toggled, this, [this] { saveSchedule(); });
    connect(m_schedPeriod, &QComboBox::currentIndexChanged, this, [this] { saveSchedule(); });
    connect(m_schedDay, &QComboBox::currentIndexChanged, this, [this] { saveSchedule(); });
    connect(m_schedScan, &QComboBox::currentIndexChanged, this, [this] { saveSchedule(); });
    connect(m_schedTime, &QTimeEdit::editingFinished, this, [this] { saveSchedule(); });

    m_selfProtect = new ToggleSwitch;
    m_selfProtect->setCheckedInstant(QSettings().value(kKeySelfProtect, false).toBool());
    careLayout->addWidget(makeSettingRow(
        QStringLiteral("Защита программы от остановки"),
        QStringLiteral("Другие программы не смогут завершить BadCode Guard или внедриться в него. "
                       "Пока включено, «Снять задачу» в диспетчере задач не работает: закрывайте "
                       "программу через «Выход» в меню значка в трее. Выключено по умолчанию."),
        m_selfProtect));
    addDivider(careLayout);
    connect(m_selfProtect, &QAbstractButton::toggled, this, [this](bool on) {
        if (on) {
            if (!ConfirmDialog::ask(window(), QStringLiteral("Включить защиту от остановки?"),
                                    QStringLiteral("Диспетчер задач не сможет завершить программу. "
                                                   "Закрывать её нужно через «Выход» в меню значка "
                                                   "в трее. Если программа зависнет, поможет "
                                                   "перезагрузка или запуск диспетчера от "
                                                   "администратора."),
                                    QStringLiteral("Включить"), true)) {
                QSignalBlocker blocker(m_selfProtect);
                m_selfProtect->setChecked(false);
                return;
            }
            if (!bcg::selfprotect::enable()) {
                QSignalBlocker blocker(m_selfProtect);
                m_selfProtect->setChecked(false);
                Toast::show(this, QStringLiteral("Не удалось включить защиту"), Toast::Kind::Error);
                bcg::log::warn(QStringLiteral("Защита от остановки не включилась"));
                return;
            }
        } else {
            bcg::selfprotect::disable();
        }
        QSettings().setValue(kKeySelfProtect, on);
    });

    auto* logRow = new QWidget;
    auto* logLayout = new QHBoxLayout(logRow);
    logLayout->setContentsMargins(20, 16, 20, 16);
    auto* logTexts = new QVBoxLayout;
    logTexts->setSpacing(2);
    logTexts->addWidget(makeLabel(QStringLiteral("Журнал работы программы"), "rowTitle"));
    auto* logHint = makeLabel(
        QStringLiteral("Текстовый файл с ошибками и событиями: неудачные обновления, проблемы с "
                       "карантином, остановившиеся наблюдатели."),
        "muted");
    logHint->setWordWrap(true);
    logTexts->addWidget(logHint);
    logLayout->addLayout(logTexts, 1);
    auto* openLog = new FancyButton(QStringLiteral("Открыть папку"), QStringLiteral("folder"),
                                    FancyButton::Secondary);
    logLayout->addWidget(openLog);
    careLayout->addWidget(logRow);
    layout->addWidget(careCard);
    connect(openLog, &QPushButton::clicked, this, [] {
        const QString dir = QFileInfo(bcg::log::file()).absolutePath();
        QDir().mkpath(dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    applySchedule();

    connect(m_realtime, &QAbstractButton::toggled, this, [this](bool on) {
        QSettings().setValue(kKeyRealtime, on);
        applyRealtime(on);
        refreshProtectionRows();
    });
    connect(openDb, &QPushButton::clicked, this, [] {
        QDir().mkpath(bcg::signaturesFolder());
        QDesktopServices::openUrl(QUrl::fromLocalFile(bcg::signaturesFolder()));
    });
    connect(m_autoQuarantine, &QAbstractButton::toggled, this, [this](bool on) {
        m_ctx.options->autoQuarantine = on;
        QSettings().setValue(kKeyAutoQuarantine, on);
        refreshProtectionRows();
    });
    connect(m_heuristics, &QAbstractButton::toggled, this, [this](bool on) {
        m_ctx.options->heuristics = on;
        QSettings().setValue(kKeyHeuristics, on);
        refreshProtectionRows();
    });
    connect(m_ctx.quarantine.get(), &bcg::Quarantine::changed, this, [this] { refreshProtectionRows(); });
    layout->addStretch();

    connect(m_autostart, &QAbstractButton::toggled, this, [this](bool on) {
        if (bcg::autostart::setEnabled(on))
            return;
        // Registry write failed: put the switch back and say why.
        QSignalBlocker blocker(m_autostart);
        m_autostart->setChecked(!on);
        Toast::show(this, QStringLiteral("Не удалось изменить автозапуск"), Toast::Kind::Error);
    });
    connect(m_trayOnClose, &QAbstractButton::toggled, this,
            [](bool on) { QSettings().setValue(kKeyTrayOnClose, on); });
    connect(m_startInTray, &QAbstractButton::toggled, this,
            [](bool on) { QSettings().setValue(kKeyStartInTray, on); });

    // More rows than a small window can show: let the page scroll.
    auto* scroll = new SmoothScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(page);
    return scroll;
}

void MainWindow::buildTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(windowIcon(), this);
    m_tray->setToolTip(QStringLiteral("BadCode Guard"));

    auto* menu = new QMenu(this);
    auto showWindow = [this] {
        showNormal();
        raise();
        activateWindow();
    };
    menu->addAction(QStringLiteral("Открыть"), this, showWindow);
    menu->addAction(QStringLiteral("Быстрая проверка"), this, [this, showWindow] {
        showWindow();
        quickScan();
    });
    menu->addSeparator();
    menu->addAction(QStringLiteral("Выход"), this, [this] {
        m_quitting = true;
        close();
        qApp->quit();
    });
    m_tray->setContextMenu(menu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [showWindow](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger
                    || reason == QSystemTrayIcon::DoubleClick)
                    showWindow();
            });
    m_tray->show();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!m_quitting && m_tray && m_tray->isVisible() && m_trayOnClose->isChecked()) {
        hide();
        m_tray->showMessage(QStringLiteral("BadCode Guard"),
                            QStringLiteral("Программа продолжает работать в трее."),
                            QSystemTrayIcon::Information, 3000);
        event->ignore();
        return;
    }
    event->accept();
    qApp->quit();
}

// The window fades in the first time it appears.
void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    // Layout is final after the first event-loop turn: place the highlight then.
    QTimer::singleShot(0, this, [this] { placeHighlight(false); });
    if (m_fadedIn)
        return;
    m_fadedIn = true;
    setWindowOpacity(0.0);
    auto* fade = new QPropertyAnimation(this, "windowOpacity", this);
    fade->setDuration(320);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::showPage(Page page)
{
    m_stack->setCurrentIndexAnimated(page);
    m_navGroup->button(page)->setChecked(true);
}

void MainWindow::setScanButtonsEnabled(bool enabled)
{
    for (auto* b : {m_quickBtn, m_fullBtn, m_customBtn})
        b->setEnabled(enabled);
}

void MainWindow::setStatus(const QString& text, const QColor& color)
{
    m_statusLabel->setTextAnimated(text, color);
}

void MainWindow::quickScan()
{
    QStringList paths;
    for (auto loc : {QStandardPaths::DownloadLocation, QStandardPaths::DesktopLocation,
                     QStandardPaths::TempLocation}) {
        for (const QString& p : QStandardPaths::standardLocations(loc)) {
            if (QDir(p).exists() && !paths.contains(p))
                paths << p;
        }
    }
    startScan(QStringLiteral("Быстрая проверка"), paths);
}

void MainWindow::fullScan()
{
    QStringList paths;
    for (const QFileInfo& drive : QDir::drives())
        paths << drive.absoluteFilePath();
    startScan(QStringLiteral("Полная проверка"), paths);
}

void MainWindow::customScan()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Выберите папку"));
    if (!dir.isEmpty())
        startScan(QStringLiteral("Проверка папки"), {dir});
}

void MainWindow::startScan(const QString& title, const QStringList& paths)
{
    if (m_scanning) {
        showPage(PageScan);
        return;
    }
    if (paths.isEmpty())
        return;

    m_scanning = true;
    m_scanned = 0;
    m_threats = 0;
    m_threatList->clear();
    m_log->clear();
    m_scanTitle->setTextAnimated(title);
    m_progress->setValue(0.0);
    m_progress->setBusy(true); // the total is unknown until the background count finishes
    m_total = 0;
    m_totalFinal = false;
    m_cancelBtn->setEnabled(true);
    setScanButtonsEnabled(false);
    m_glow->setActive(true);
    setStatus(QStringLiteral("Идёт проверка…"), th::kAccent);
    m_elapsed.start();
    m_clock->start();
    updateCounters(true);
    showPage(PageScan);

    appendLog(QStringLiteral("Начало: %1").arg(title));
    for (const QString& p : paths)
        appendLog(QStringLiteral("  • %1").arg(QDir::toNativeSeparators(p)));

    bcg::IScanner* scanner = m_scanner;
    QMetaObject::invokeMethod(scanner, [scanner, paths] { scanner->start(paths); },
                              Qt::QueuedConnection);
}

void MainWindow::onProgress(qint64 scanned, const QString& currentFile)
{
    m_scanned = scanned;
    if (!currentFile.isEmpty()) {
        const QString native = QDir::toNativeSeparators(currentFile);
        m_currentFile->setText(m_currentFile->fontMetrics().elidedText(
            native, Qt::ElideMiddle, m_currentFile->width()));
    }
    updateCounters();
}

void MainWindow::onThreatFound(const QString& path, const QString& name, int kind,
                               const QString& quarantineId, const QString& quarantineError)
{
    ++m_threats;
    const QString status = addThreatItem(path, name, kind, quarantineId, quarantineError);
    appendLog(QStringLiteral("Угроза: %1 (%2)%3")
                  .arg(name, QDir::toNativeSeparators(path), status));
    recordThreat(QStringLiteral("Проверка"), path, name, kind, quarantineId, quarantineError);
    maybeLookup(path, kind);
    updateCounters();
}

// Writes a finding to the journal.
void MainWindow::recordThreat(const QString& source, const QString& path, const QString& name,
                              int kind, const QString& quarantineId, const QString& quarantineError)
{
    bcg::HistoryEntry e;
    e.type = bcg::HistoryEntry::Threat;
    e.source = source;
    e.title = name;
    e.path = path;
    e.kind = kind;
    if (!quarantineId.isEmpty())
        e.action = QStringLiteral("в карантине");
    else if (!quarantineError.isEmpty())
        e.action = QStringLiteral("не удалось изолировать");
    else if (kind == static_cast<int>(bcg::ThreatKind::Heuristic))
        e.action = QStringLiteral("подозрительный, оставлен на месте");
    else
        e.action = QStringLiteral("оставлен на месте");
    m_history->add(e);
}

// Suspicious (not proven) files can be checked against VirusTotal automatically.
void MainWindow::maybeLookup(const QString& path, int kind)
{
    if (kind != static_cast<int>(bcg::ThreatKind::Heuristic) || !m_vtAuto || !m_vtAuto->isChecked()
        || !m_reputation->hasKey())
        return;
    m_reputation->lookupFile(path, m_nextToken++);
}

// Adds a row to the threat list and returns its status suffix for the log.
QString MainWindow::addThreatItem(const QString& path, const QString& name, int kind,
                                  const QString& quarantineId, const QString& quarantineError)
{
    const QString native = QDir::toNativeSeparators(path);
    const bool heuristic = kind == static_cast<int>(bcg::ThreatKind::Heuristic);
    QString status;
    QColor color = th::kDanger;
    if (!quarantineId.isEmpty()) {
        status = QStringLiteral(" [в карантине]");
        color = th::kAccent;
    } else if (!quarantineError.isEmpty()) {
        status = QStringLiteral(" [не удалось изолировать: %1]").arg(quarantineError);
        bcg::log::warn(QStringLiteral("Не удалось изолировать %1: %2").arg(native, quarantineError));
    } else if (heuristic) {
        status = QStringLiteral(" [подозрительный, проверьте вручную]");
        color = QColor(0xFF, 0xB8, 0x4C);
    }
    auto* item = new QListWidgetItem(QStringLiteral("%1 — %2%3").arg(name, native, status));
    item->setForeground(color);
    item->setData(kRoleQuarantineId, quarantineId);
    item->setData(kRolePath, path);
    item->setData(kRoleName, name);
    item->setData(kRoleKind, kind);
    item->setData(kRoleBaseText, item->text());
    m_threatList->addItem(item);
    return status;
}

// A file that appeared in a watched folder turned out to be a threat.
void MainWindow::onRealtimeThreat(const QString& path, const QString& name, int kind,
                                  const QString& quarantineId, const QString& quarantineError)
{
    const QString status = addThreatItem(path, name, kind, quarantineId, quarantineError);
    const QString native = QDir::toNativeSeparators(path);
    appendLog(QStringLiteral("Реальное время: %1 (%2)%3").arg(name, native, status));
    recordThreat(QStringLiteral("Реальное время"), path, name, kind, quarantineId, quarantineError);
    maybeLookup(path, kind);
    if (!m_scanning)
        setStatus(QStringLiteral("Обнаружена угроза"), th::kDanger);

    const QString file = QFileInfo(path).fileName();
    if (isVisible() && !isMinimized()) {
        Toast::show(this,
                    quarantineId.isEmpty() ? QStringLiteral("Угроза: %1").arg(file)
                                           : QStringLiteral("Угроза изолирована: %1").arg(file),
                    Toast::Kind::Error);
    } else if (m_tray) {
        m_tray->showMessage(QStringLiteral("BadCode Guard"),
                            QStringLiteral("%1: %2%3").arg(name, file, status),
                            QSystemTrayIcon::Warning);
    }
}

void MainWindow::applyRealtime(bool on)
{
    if (!on) {
        m_monitor->stop();
        m_watchSignature.clear();
        m_watchCount = 0;
        return;
    }
    refreshWatchers(true);
}

QList<bcg::RealtimeMonitor::WatchFolder> MainWindow::watchList() const
{
    QList<bcg::RealtimeMonitor::WatchFolder> list;
    QStringList seen;
    auto add = [&](const QString& path, bool onlyRisky) {
        const QString k = QDir::fromNativeSeparators(path).toLower();
        if (path.isEmpty() || seen.contains(k) || !QDir(path).exists())
            return;
        seen << k;
        list.append({path, onlyRisky});
    };

    for (auto loc : {QStandardPaths::DownloadLocation, QStandardPaths::DesktopLocation}) {
        for (const QString& p : QStandardPaths::standardLocations(loc))
            add(p, false);
    }
    const QSettings settings;
    // Temp changes constantly, so only file types that can do harm are checked there.
    if (settings.value(kKeyWatchTemp, true).toBool())
        add(QStandardPaths::writableLocation(QStandardPaths::TempLocation), true);
    if (settings.value(kKeyWatchUsb, true).toBool()) {
        for (const QString& root : bcg::drives::removableRoots())
            add(root, false);
    }
    for (const QString& p : settings.value(kKeyWatchFolders).toStringList())
        add(p, false);
    return list;
}

// Starts or restarts real-time watching when the set of folders changed, or when
// a listener has died.
void MainWindow::refreshWatchers(bool force)
{
    if (!QSettings().value(kKeyRealtime, true).toBool())
        return;
    const auto list = watchList();
    QStringList parts;
    for (const auto& f : list)
        parts << f.path + (f.onlyRisky ? QStringLiteral("|r") : QString());
    const QString signature = parts.join(QLatin1Char(';'));

    const bool dead = m_monitor->isRunning() && m_monitor->aliveWatchers() < m_watchCount;
    if (!force && !dead && signature == m_watchSignature)
        return;
    if (dead)
        bcg::log::warn(QStringLiteral("Слежение за папками перезапущено: часть наблюдателей остановилась"));
    m_watchCount = m_monitor->start(list);
    m_watchSignature = signature;
    refreshProtectionRows();
}

// A program that has just started turned out to be a threat.
void MainWindow::onProcessThreat(const QString& path, const QString& name, int kind,
                                 const QString& quarantineId, const QString& quarantineError,
                                 bool terminated, quint32 pid)
{
    Q_UNUSED(pid)
    const QString status = addThreatItem(path, name, kind, quarantineId, quarantineError);
    const QString suffix = terminated ? QStringLiteral(", процесс завершён") : QString();
    appendLog(QStringLiteral("Запуск программы: %1 (%2)%3%4")
                  .arg(name, QDir::toNativeSeparators(path), status, suffix));

    bcg::HistoryEntry e;
    e.type = bcg::HistoryEntry::Threat;
    e.source = QStringLiteral("Запуск программы");
    e.title = name;
    e.path = path;
    e.kind = kind;
    QStringList actions;
    if (terminated)
        actions << QStringLiteral("процесс завершён");
    if (!quarantineId.isEmpty())
        actions << QStringLiteral("в карантине");
    else if (!quarantineError.isEmpty())
        actions << QStringLiteral("не удалось изолировать");
    else
        actions << QStringLiteral("оставлен на месте");
    e.action = actions.join(QStringLiteral(", "));
    m_history->add(e);
    maybeLookup(path, kind);

    if (!m_scanning)
        setStatus(QStringLiteral("Обнаружена угроза"), th::kDanger);
    const QString file = QFileInfo(path).fileName();
    const QString text = terminated ? QStringLiteral("Остановлена угроза: %1").arg(file)
                                    : QStringLiteral("Запущена подозрительная программа: %1").arg(file);
    if (isVisible() && !isMinimized())
        Toast::show(this, text, Toast::Kind::Error);
    else if (m_tray)
        m_tray->showMessage(QStringLiteral("BadCode Guard"), QStringLiteral("%1 (%2)").arg(text, name),
                            QSystemTrayIcon::Warning);
}

void MainWindow::onTotal(qint64 total, bool final)
{
    m_total = total;
    m_totalFinal = final;
}

// "Проверено 1 200 из 48 000 · осталось ≈ 04:12" once the count is exact.
QString MainWindow::etaText() const
{
    if (!m_scanning)
        return QString();
    const QLocale loc;
    if (!m_totalFinal || m_total <= 0)
        return QStringLiteral("Подсчёт файлов… проверено %1 (уже найдено %2)")
            .arg(loc.toString(m_scanned), loc.toString(m_total));
    QString text = QStringLiteral("Проверено %1 из %2").arg(loc.toString(m_scanned), loc.toString(m_total));
    const qint64 ms = m_elapsed.elapsed();
    if (m_scanned >= 200 && ms > 3000 && m_scanned < m_total) {
        const qint64 left = ms * (m_total - m_scanned) / m_scanned;
        text += QStringLiteral(" · осталось ≈ %1").arg(formatDuration(left));
    }
    return text;
}

void MainWindow::saveSchedule()
{
    bcg::Schedule s;
    s.enabled = m_scheduleOn->isChecked();
    s.period = m_schedPeriod->currentIndex() == 0 ? bcg::Schedule::Daily : bcg::Schedule::Weekly;
    s.dayOfWeek = m_schedDay->currentIndex() + 1;
    s.time = m_schedTime->time();
    s.scan = m_schedScan->currentIndex() == 0 ? bcg::Schedule::Quick : bcg::Schedule::Full;
    QSettings settings;
    s.save(settings);
    // Switching it on must not fire a run for a moment that is already past.
    if (s.enabled && !QDateTime::fromString(settings.value(kKeyScheduleLast).toString(), Qt::ISODate).isValid())
        settings.setValue(kKeyScheduleLast, QDateTime::currentDateTime().toString(Qt::ISODate));
    applySchedule();
}

// Shows the current schedule in the settings and enables only the controls that matter.
void MainWindow::applySchedule()
{
    const bcg::Schedule s = bcg::Schedule::load(QSettings());
    m_schedDay->setEnabled(s.period == bcg::Schedule::Weekly);
    m_schedInfo->setText(s.enabled
                             ? QStringLiteral("Следующая проверка: %1 (%2).")
                                   .arg(s.nextDue(QDateTime::currentDateTime())
                                            .toString(QStringLiteral("dd.MM.yyyy HH:mm")),
                                        s.describe())
                             : QStringLiteral("Плановая проверка выключена."));
    refreshProtectionRows();
}

void MainWindow::checkSchedule()
{
    const bcg::Schedule s = bcg::Schedule::load(QSettings());
    if (!s.enabled || m_scanning)
        return;
    QSettings settings;
    const QDateTime last = QDateTime::fromString(settings.value(kKeyScheduleLast).toString(), Qt::ISODate);
    const QDateTime now = QDateTime::currentDateTime();
    if (!s.isDue(now, last))
        return;
    settings.setValue(kKeyScheduleLast, now.toString(Qt::ISODate));
    bcg::log::info(QStringLiteral("Плановая проверка запущена (%1)").arg(s.describe()));
    if (s.scan == bcg::Schedule::Full)
        fullScan();
    else
        quickScan();
    appendLog(QStringLiteral("Запуск по расписанию (%1)").arg(s.describe()));
    applySchedule();
}

void MainWindow::refreshDbInfo()
{
    const QString updated = QSettings().value(kKeyDbUpdated).toString();
    const QDateTime when = QDateTime::fromString(updated, Qt::ISODate);
    m_dbInfoLabel->setText(
        QStringLiteral("Загружено хэшей: %1, правил: %2. %3 Свои файлы (*.txt, *.sha256, *.bcg) "
                       "можно класть в папку баз.")
            .arg(m_loadReport.hashes)
            .arg(m_loadReport.rules)
            .arg(when.isValid() ? QStringLiteral("Обновлено: %1.")
                                        .arg(when.toLocalTime().toString(QStringLiteral("dd.MM.yyyy HH:mm")))
                                : QStringLiteral("Автообновление не настроено.")));
}

QStringList MainWindow::feedUrls() const
{
    QStringList urls;
    for (const QString& line : m_feedsEdit->toPlainText().split(QLatin1Char('\n'))) {
        const QString t = line.trimmed();
        if (!t.isEmpty() && !urls.contains(t))
            urls << t;
    }
    return urls;
}

// Updates in order: MalwareBazaar (when a key is known), then the extra hash
// lists, then the engine picks up whatever arrived.
void MainWindow::updateDatabase(bool silent)
{
    if (m_updater->busy() || m_feeds->busy())
        return;
    QString text = m_dbKeyEdit->text();
    if (text.trimmed().isEmpty() && silent)
        text = secret(kKeyDbKey);
    m_silentUpdate = silent;
    m_updateReport.clear();
    m_updateAnyOk = false;
    m_updateDbBtn->setEnabled(false);
    m_dbStatusLabel->setStyleSheet(QString());
    m_dbStatusLabel->setText(QStringLiteral("Загрузка…"));
    QSettings().setValue(kKeyFeeds, feedUrls());

    if (text.trimmed().isEmpty() && feedUrls().isEmpty()) {
        m_updateReport << QStringLiteral("Введите ключ abuse.ch или ссылки на списки хэшей.");
        finishDatabaseUpdate();
        return;
    }
    if (text.trimmed().isEmpty())
        updateHashFeeds();
    else
        m_updater->update(text);
}

void MainWindow::updateHashFeeds()
{
    m_feeds->updateHashLists(feedUrls());
}

// Runs from a timer: refreshes the lists when a source is set up and they are old.
void MainWindow::autoUpdateDatabase()
{
    const QSettings settings;
    const bool configured = !secret(kKeyDbKey).isEmpty()
        || !settings.value(kKeyFeeds).toStringList().isEmpty();
    if (!configured || m_scanning)
        return;
    const QDateTime last = QDateTime::fromString(settings.value(kKeyDbUpdated).toString(), Qt::ISODate);
    if (!last.isValid() || last.secsTo(QDateTime::currentDateTimeUtc()) >= kDbMaxAgeSecs)
        updateDatabase(true);
}

void MainWindow::onDatabaseUpdated(bool ok, const QString& message, int listed)
{
    Q_UNUSED(listed)
    m_updateReport << message;
    if (!ok)
        bcg::log::warn(QStringLiteral("Обновление базы MalwareBazaar: %1").arg(message));
    if (ok) {
        m_updateAnyOk = true;
        // The key worked: remember it (never the whole link).
        const QString key = bcg::DatabaseUpdater::extractKey(m_dbKeyEdit->text());
        if (!key.isEmpty())
            setSecret(kKeyDbKey, key);
    }
    updateHashFeeds();
}

void MainWindow::onFeedFinished(int kind, bool ok, const QString& message)
{
    if (!ok)
        bcg::log::warn(QStringLiteral("Загрузка данных (%1): %2")
                           .arg(kind == bcg::FeedUpdater::HashLists ? QStringLiteral("списки хэшей")
                                                                    : QStringLiteral("правила YARA"),
                                message));
    if (kind == bcg::FeedUpdater::HashLists) {
        if (!message.startsWith(QStringLiteral("Дополнительных")))
            m_updateReport << message;
        m_updateAnyOk |= ok && !message.startsWith(QStringLiteral("Дополнительных"));
        finishDatabaseUpdate();
        return;
    }

    // YARA rule set.
    m_yaraBtn->setEnabled(true);
    if (!ok) {
        m_yaraStatusLabel->setStyleSheet(QStringLiteral("color: #FFB84C;"));
        m_yaraStatusLabel->setText(message);
        Toast::show(this, QStringLiteral("Не удалось скачать правила YARA"), Toast::Kind::Error);
        return;
    }
    const bcg::YaraLoadReport report = bcg::reloadYara(m_ctx);
    m_loadReport.yaraRules = report.rules;
    m_loadReport.yaraFailed = report.failed;
    refreshYaraInfo();
    refreshProtectionRows();
    m_yaraStatusLabel->setStyleSheet(QStringLiteral("color: #2EE6B0;"));
    m_yaraStatusLabel->setText(
        QStringLiteral("%1 Загружено правил: %2, файлов не подошло: %3.")
            .arg(message)
            .arg(report.rules)
            .arg(report.failed));
    Toast::show(this, QStringLiteral("Правила YARA обновлены"), Toast::Kind::Success);
}

void MainWindow::finishDatabaseUpdate()
{
    if (m_updateAnyOk) {
        QSettings().setValue(kKeyDbUpdated, QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        m_loadReport.hashes = bcg::reloadSignatures(m_ctx);
        refreshDbInfo();
        refreshProtectionRows();
    }
    m_updateDbBtn->setEnabled(true);
    m_dbStatusLabel->setStyleSheet(
        QString::fromLatin1(m_updateAnyOk ? "color: #2EE6B0;" : "color: #FFB84C;"));
    QString text = m_updateReport.join(QLatin1Char(' '));
    if (m_updateAnyOk)
        text += QStringLiteral(" Всего в базе: %1.").arg(m_loadReport.hashes);
    m_dbStatusLabel->setText(text);
    if (!m_silentUpdate)
        Toast::show(this,
                    m_updateAnyOk ? QStringLiteral("База обновлена")
                                  : QStringLiteral("Не удалось обновить базу"),
                    m_updateAnyOk ? Toast::Kind::Success : Toast::Kind::Error);
}

void MainWindow::refreshYaraInfo()
{
    m_yaraInfoLabel->setText(
        m_loadReport.yaraRules == 0
            ? QStringLiteral("Правил пока нет.")
            : QStringLiteral("Загружено правил: %1%2.")
                  .arg(m_loadReport.yaraRules)
                  .arg(m_loadReport.yaraFailed > 0
                           ? QStringLiteral(", файлов пропущено: %1").arg(m_loadReport.yaraFailed)
                           : QString()));
}

void MainWindow::updateThreatButtons()
{
    const QList<QListWidgetItem*> selected = m_threatList->selectedItems();
    bool canQuarantine = false;
    for (QListWidgetItem* item : selected)
        canQuarantine |= item->data(kRoleQuarantineId).toString().isEmpty();
    m_toQuarantineBtn->setEnabled(canQuarantine && !m_scanning);
    m_excludeThreatBtn->setEnabled(!selected.isEmpty());
    m_vtBtn->setEnabled(!selected.isEmpty());
}

// Asks VirusTotal about the selected rows. Only the file's hash is sent.
void MainWindow::lookupSelected()
{
    if (!m_reputation->hasKey()) {
        Toast::show(this, QStringLiteral("Сначала введите ключ VirusTotal в Настройках"),
                    Toast::Kind::Error);
        return;
    }
    int queued = 0;
    for (QListWidgetItem* item : m_threatList->selectedItems()) {
        const QString path = item->data(kRolePath).toString();
        // A quarantined file is no longer at its path; its hash cannot be read.
        if (!item->data(kRoleQuarantineId).toString().isEmpty() || !QFileInfo::exists(path))
            continue;
        item->setText(item->data(kRoleBaseText).toString() + QStringLiteral(" · VirusTotal: запрос…"));
        m_reputation->lookupFile(path, m_nextToken++);
        ++queued;
    }
    if (queued == 0)
        Toast::show(this, QStringLiteral("Для файлов в карантине проверка недоступна"),
                    Toast::Kind::Info);
    else if (queued > 1)
        Toast::show(this, QStringLiteral("Запросов в очереди: %1 (VirusTotal: не чаще 4 в минуту)")
                              .arg(queued),
                    Toast::Kind::Info);
}

void MainWindow::onReputation(const bcg::ReputationResult& r)
{
    const QString text = bcg::ReputationService::summary(r);
    for (int i = 0; i < m_threatList->count(); ++i) {
        QListWidgetItem* item = m_threatList->item(i);
        if (item->data(kRolePath).toString() != r.path)
            continue;
        item->setText(item->data(kRoleBaseText).toString() + QStringLiteral(" · ") + text);
        item->setToolTip(text);
    }
    appendLog(QStringLiteral("%1 (%2)").arg(text, QDir::toNativeSeparators(r.path)));

    if (r.state != bcg::ReputationResult::Error) {
        // Record the verdict next to the finding it belongs to.
        QList<bcg::HistoryEntry> copy = m_history->entries();
        for (int i = int(copy.size()) - 1; i >= 0; --i) {
            if (copy.at(i).type == bcg::HistoryEntry::Threat && copy.at(i).path == r.path) {
                m_history->update(i, copy.at(i).action, text);
                break;
            }
        }
    } else if (!m_vtAuto || !m_vtAuto->isChecked()) {
        Toast::show(this, text, Toast::Kind::Error);
    }
}

void MainWindow::quarantineSelected()
{
    int done = 0;
    QString lastError;
    for (QListWidgetItem* item : m_threatList->selectedItems()) {
        if (!item->data(kRoleQuarantineId).toString().isEmpty())
            continue;
        const QString path = item->data(kRolePath).toString();
        const QString name = item->data(kRoleName).toString();
        const bcg::QuarantineResult r =
            m_ctx.quarantine->add(path, name, item->data(kRoleKind).toInt());
        if (!r.ok) {
            lastError = r.error;
            continue;
        }
        ++done;
        item->setData(kRoleQuarantineId, r.entry.id);
        item->setText(QStringLiteral("%1 — %2 [в карантине]")
                          .arg(name, QDir::toNativeSeparators(path)));
        item->setForeground(th::kAccent);
        appendLog(QStringLiteral("В карантин: %1").arg(QDir::toNativeSeparators(path)));
    }
    updateThreatButtons();
    if (!lastError.isEmpty())
        Toast::show(this, QStringLiteral("Не удалось изолировать: %1").arg(lastError),
                    Toast::Kind::Error);
    else if (done > 0)
        Toast::show(this, QStringLiteral("Помещено в карантин: %1").arg(done), Toast::Kind::Success);
}

void MainWindow::excludeSelected()
{
    for (QListWidgetItem* item : m_threatList->selectedItems())
        m_exclusionsPage->addExclusion({bcg::Exclusion::File, item->data(kRolePath).toString()});
}

void MainWindow::onFinished(bool cancelled, qint64 scanned, qint64 threats, qint64 skipped)
{
    m_scanning = false;
    m_clock->stop();
    m_durationMs = m_elapsed.elapsed();
    m_scanned = scanned;
    m_threats = threats;
    m_progress->setBusy(false);
    m_progress->setValue(cancelled ? 0.0 : 1.0);
    m_cancelBtn->setEnabled(false);
    setScanButtonsEnabled(true);
    m_glow->setActive(false);
    m_currentFile->setText(cancelled ? QStringLiteral("Проверка отменена")
                                     : QStringLiteral("Проверка завершена"));
    if (threats > 0)
        setStatus(QStringLiteral("Обнаружены угрозы: %1").arg(threats), th::kDanger);
    else
        setStatus(cancelled ? QStringLiteral("Проверка отменена")
                            : QStringLiteral("Угроз не найдено"),
                  th::kAccent);
    updateCounters();

    const QString summary = QStringLiteral("%1: файлов %2, угроз %3, время %4")
                                .arg(cancelled ? QStringLiteral("Отменено")
                                               : QStringLiteral("Завершено"))
                                .arg(scanned)
                                .arg(threats)
                                .arg(formatDuration(m_durationMs));
    appendLog(summary);
    if (skipped > 0)
        appendLog(QStringLiteral("Пропущено по исключениям: %1").arg(skipped));

    bcg::HistoryEntry run;
    run.type = bcg::HistoryEntry::Scan;
    run.source = QStringLiteral("Проверка");
    run.title = summary;
    run.action = cancelled ? QStringLiteral("отменена") : QStringLiteral("завершена");
    m_history->add(run);

    if (!cancelled) {
        QSettings().setValue(
            kKeyLastScan,
            QStringLiteral("%1 — файлов %2, угроз %3")
                .arg(QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm")))
                .arg(scanned)
                .arg(threats));
        updateLastScanLabel();
    }
    if (m_tray && !isVisible())
        m_tray->showMessage(QStringLiteral("BadCode Guard"), summary,
                            threats ? QSystemTrayIcon::Warning : QSystemTrayIcon::Information);
}

void MainWindow::updateLastScanLabel()
{
    const QString last = QSettings().value(kKeyLastScan).toString();
    m_lastScanLabel->setText(last.isEmpty() ? QStringLiteral("Проверок ещё не было")
                                            : QStringLiteral("Последняя проверка: %1").arg(last));
}

// Numbers glide to their new value instead of jumping.
void MainWindow::animateCount(QLabel* label, qint64 target, bool instant)
{
    auto* anim = label->findChild<QVariantAnimation*>();
    if (!anim) {
        anim = new QVariantAnimation(label);
        anim->setDuration(260);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QVariantAnimation::valueChanged, label, [label](const QVariant& v) {
            label->setText(QLocale().toString(v.toLongLong()));
        });
    }
    const qint64 current = anim->currentValue().isValid() ? anim->currentValue().toLongLong() : 0;
    if (!instant && anim->endValue().isValid() && anim->endValue().toLongLong() == target)
        return;
    anim->stop();
    if (instant) {
        anim->setStartValue(int(target));
        anim->setEndValue(int(target));
        label->setText(QLocale().toString(target));
        return;
    }
    anim->setStartValue(int(current));
    anim->setEndValue(int(target));
    anim->start();
}

void MainWindow::updateCounters(bool instant)
{
    animateCount(m_filesValue, m_scanned, instant);
    animateCount(m_threatsValue, m_threats, instant);
    m_threatsValue->setStyleSheet(
        m_threats > 0 ? QStringLiteral("color: %1;").arg(th::kDanger.name()) : QString());

    const qint64 ms = m_scanning ? m_elapsed.elapsed() : m_durationMs;
    m_timeValue->setText(ms < 0 ? QStringLiteral("—") : formatDuration(ms));

    // Once the files are counted the bar is exact; until then it only pulses.
    if (m_scanning) {
        if (m_totalFinal && m_total > 0) {
            m_progress->setBusy(false);
            m_progress->setValue(qMin(1.0, double(m_scanned) / double(m_total)));
        } else {
            m_progress->setBusy(true);
        }
    }
    m_etaLabel->setText(etaText());
}

// Puts the sidebar highlight under the selected entry.
void MainWindow::placeHighlight(bool animate)
{
    if (QAbstractButton* checked = m_navGroup->checkedButton())
        m_highlight->moveTo(checked->geometry(), animate);
}

void MainWindow::appendLog(const QString& line)
{
    m_log->appendPlainText(
        QStringLiteral("[%1] %2")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")), line));
}
