#include "WindowsInfoPage.h"

#include "sys/SystemInfo.h"
#include "ui/widgets/Animations.h"
#include "ui/widgets/IconButton.h"
#include "ui/widgets/InfoTile.h"
#include "ui/widgets/KeyValueCard.h"
#include "ui/widgets/StatusBadge.h"

#include <QDateTime>
#include <QGridLayout>
#include <QVBoxLayout>

namespace sys = bcg::sys;

namespace {
constexpr qint64 kStaleSeconds = 60;
}

WindowsInfoPage::WindowsInfoPage(QWidget* parent)
    : SubPage(tr("Информация о Windows"), parent)
{
    auto* refresh = new IconButton(QStringLiteral("recover"), tr("Обновить данные"));
    addHeaderAction(refresh);
    connect(refresh, &QAbstractButton::clicked, this, [] { sys::SystemInfoService::instance().refresh(); });

    const struct {
        const char* icon;
        const char* caption;
    } tiles[] = {
        {"window", "Операционная система"}, {"info", "Версия"},         {"package", "Редакция"},
        {"hash", "Сборка"},                  {"calendar", "Дата установки"}, {"key", "Состояние активации"},
    };
    auto* grid = new QGridLayout;
    grid->setSpacing(14);
    int i = 0;
    for (const auto& t : tiles) {
        auto* tile = new InfoTile(QString::fromLatin1(t.icon), QString::fromUtf8(t.caption));
        m_tiles.append(tile);
        grid->addWidget(tile, i / 3, i % 3);
        ++i;
    }
    for (int column = 0; column < 3; ++column)
        grid->setColumnStretch(column, 1);
    body()->addLayout(grid);

    m_groups = new QVBoxLayout;
    m_groups->setSpacing(14);
    body()->addLayout(m_groups);
    body()->addStretch();

    connect(&sys::SystemInfoService::instance(), &sys::SystemInfoService::windowsChanged, this,
            &WindowsInfoPage::populate);
}

void WindowsInfoPage::activated()
{
    auto& service = sys::SystemInfoService::instance();
    if (service.hasWindows())
        populate(); // also re-reads the settings that are checked live
    const bool stale = !service.lastRefresh().isValid()
        || service.lastRefresh().secsTo(QDateTime::currentDateTime()) > kStaleSeconds;
    if (!service.hasWindows() || stale)
        service.refresh();
}

void WindowsInfoPage::populate()
{
    const QJsonObject& win = sys::SystemInfoService::instance().windows();
    if (win.isEmpty())
        return;

    const QList<sys::InfoRow> overview = sys::info::windowsOverview(win);
    for (int i = 0; i < m_tiles.size() && i < overview.size(); ++i) {
        const sys::InfoRow& row = overview[i];
        // Skip the fade when the value is unchanged (periodic re-population).
        m_tiles[i]->setContent(row.value.isEmpty() ? QStringLiteral("—") : row.value, QString());
        m_tiles[i]->setMainColor(row.status == sys::Status::None ? QColor() : StatusBadge::colorFor(row.status));
    }

    const QList<sys::InfoGroup> groups = sys::info::windowsGroups(win);
    while (m_cards.size() > groups.size())
        delete m_cards.takeLast();
    while (m_cards.size() < groups.size()) {
        auto* card = new KeyValueCard(QString());
        m_cards.append(card);
        m_groups->addWidget(card);
        if (!m_populated)
            bcg::fadeIn(card, 420);
    }
    for (int i = 0; i < groups.size(); ++i) {
        m_cards[i]->setTitle(groups[i].title);
        m_cards[i]->setRows(groups[i].rows);
    }
    m_populated = true;
}
