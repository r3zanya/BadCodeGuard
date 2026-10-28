#include "PcInfoPage.h"

#include "sys/Scoring.h"
#include "sys/SystemInfo.h"
#include "ui/widgets/Animations.h"
#include "ui/widgets/FancyButton.h"
#include "ui/widgets/IconButton.h"
#include "ui/widgets/InfoTile.h"
#include "ui/widgets/KeyValueCard.h"
#include "ui/widgets/ScoreCard.h"
#include "ui/widgets/Toast.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QPointer>
#include <QStandardPaths>
#include <QThreadPool>
#include <QVBoxLayout>

namespace sys = bcg::sys;

namespace {

constexpr qint64 kStaleSeconds = 60;

// Desktop first, Downloads when there is no desktop folder.
QString reportPath()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (dir.isEmpty() || !QDir(dir).exists())
        dir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return QDir(dir).filePath(QStringLiteral("BadCode Guard — Информация о ПК.txt"));
}

} // namespace

PcInfoPage::PcInfoPage(QWidget* parent)
    : SubPage(tr("Информация о ПК"), parent)
{
    m_rateButton = new FancyButton(tr("Оценка характеристик"), QStringLiteral("gauge"), FancyButton::Primary);
    auto* download = new IconButton(QStringLiteral("download"), tr("Сохранить информацию в TXT-файл"));
    auto* copy = new IconButton(QStringLiteral("copy"), tr("Скопировать краткую информацию"));
    addHeaderAction(m_rateButton);
    addHeaderAction(download);
    addHeaderAction(copy);
    connect(m_rateButton, &QPushButton::clicked, this, &PcInfoPage::runRating);
    connect(download, &QAbstractButton::clicked, this, &PcInfoPage::saveReport);
    connect(copy, &QAbstractButton::clicked, this, &PcInfoPage::copySummary);

    m_cpu = new InfoTile(QStringLiteral("chip"), tr("Процессор"));
    m_gpu = new InfoTile(QStringLiteral("gpu"), tr("Видеокарта"));
    m_ram = new InfoTile(QStringLiteral("ram"), tr("Оперативная память"));
    m_vram = new InfoTile(QStringLiteral("vram"), tr("Всего видеопамяти"));
    m_storage = new InfoTile(QStringLiteral("storage"), tr("Объём всех накопителей"));
    m_device = new InfoTile(QStringLiteral("device"), tr("Модель устройства"));

    auto* grid = new QGridLayout;
    grid->setSpacing(14);
    const QList<InfoTile*> tiles = {m_cpu, m_gpu, m_ram, m_vram, m_storage, m_device};
    for (int i = 0; i < tiles.size(); ++i)
        grid->addWidget(tiles[i], i / 3, i % 3);
    for (int column = 0; column < 3; ++column)
        grid->setColumnStretch(column, 1);
    body()->addLayout(grid);

    m_score = new ScoreCard;
    body()->addWidget(m_score);

    m_details = new QVBoxLayout;
    m_details->setSpacing(14);
    body()->addLayout(m_details);
    body()->addStretch();

    auto& service = sys::SystemInfoService::instance();
    connect(&service, &sys::SystemInfoService::hardwareChanged, this, &PcInfoPage::populate);
}

void PcInfoPage::activated()
{
    auto& service = sys::SystemInfoService::instance();
    if (service.hasHardware())
        populate();
    const bool stale = !service.lastRefresh().isValid()
        || service.lastRefresh().secsTo(QDateTime::currentDateTime()) > kStaleSeconds;
    if (!service.hasHardware() || stale)
        service.refresh();
}

void PcInfoPage::populate()
{
    const QJsonObject& hw = sys::SystemInfoService::instance().hardware();
    if (hw.isEmpty())
        return;

    namespace info = sys::info;
    auto apply = [](InfoTile* tile, const info::Tile& t) { tile->setContent(t.main, t.sub); };
    apply(m_cpu, info::cpuTile(hw));
    apply(m_gpu, info::gpuTile(hw));
    apply(m_ram, info::ramTile(hw));
    apply(m_vram, info::vramTile(hw));
    apply(m_storage, info::storageTile(hw));
    apply(m_device, info::deviceTile(hw));

    const QList<sys::InfoGroup> groups = info::hardwareGroups(hw);
    // Reuse the cards when the layout of the data did not change.
    while (m_cards.size() > groups.size())
        delete m_cards.takeLast();
    while (m_cards.size() < groups.size()) {
        auto* card = new KeyValueCard(QString());
        m_cards.append(card);
        m_details->addWidget(card);
        if (!m_populated)
            bcg::fadeIn(card, 420);
    }
    for (int i = 0; i < groups.size(); ++i) {
        m_cards[i]->setTitle(groups[i].title);
        m_cards[i]->setRows(groups[i].rows);
    }
    m_populated = true;
}

void PcInfoPage::runRating()
{
    auto& service = sys::SystemInfoService::instance();
    if (!service.hasHardware()) {
        Toast::show(this, tr("Данные о ПК ещё загружаются, подождите пару секунд."));
        return;
    }
    m_rateButton->setEnabled(false);
    m_score->showLoading();

    const QJsonObject hw = service.hardware();
    QPointer<PcInfoPage> self(this);
    QThreadPool::globalInstance()->start([self, hw] {
        const sys::Scores scores = sys::computeScores(hw);
        QMetaObject::invokeMethod(qApp, [self, scores] {
            if (!self)
                return;
            self->m_score->showScores(scores);
            self->m_rateButton->setEnabled(true);
        }, Qt::QueuedConnection);
    });
}

void PcInfoPage::saveReport()
{
    auto& service = sys::SystemInfoService::instance();
    if (!service.hasHardware()) {
        Toast::show(this, tr("Данные о ПК ещё загружаются, подождите пару секунд."));
        return;
    }
    const QString text = sys::info::fullReport(service.hardware(), service.windows());
    const QString path = reportPath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Toast::show(this, tr("Не удалось сохранить файл: %1").arg(file.errorString()), Toast::Kind::Error);
        return;
    }
    // UTF-8 with a BOM so that every Windows editor shows Cyrillic correctly;
    // CRLF line endings for Notepad.
    QString crlf = text;
    crlf.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
    file.write("\xEF\xBB\xBF");
    file.write(crlf.toUtf8());
    file.close();

    const bool onDesktop = QFileInfo(path).dir().path()
        == QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    Toast::show(this, onDesktop ? tr("Сохранено на рабочем столе") : tr("Сохранено в папке «Загрузки»"),
                Toast::Kind::Success);
}

void PcInfoPage::copySummary()
{
    auto& service = sys::SystemInfoService::instance();
    if (!service.hasHardware()) {
        Toast::show(this, tr("Данные о ПК ещё загружаются, подождите пару секунд."));
        return;
    }
    QApplication::clipboard()->setText(sys::info::copyText(service.hardware()));
    Toast::show(this, tr("Информация о ПК скопирована"), Toast::Kind::Success);
}
