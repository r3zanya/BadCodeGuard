#include "HistoryPage.h"

#include "sys/History.h"
#include "ui/Theme.h"
#include "ui/widgets/ConfirmDialog.h"
#include "ui/widgets/FancyButton.h"
#include "ui/widgets/Icons.h"
#include "ui/widgets/SmoothScrollArea.h"
#include "ui/widgets/Toast.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace th = bcg::theme;

namespace {
constexpr int kShown = 300; // newest rows that are drawn; the file keeps more
}

HistoryPage::HistoryPage(bcg::History* history, QWidget* parent)
    : QWidget(parent)
    , m_history(history)
{
    setObjectName(QStringLiteral("page"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 32, 36, 28);
    root->setSpacing(14);

    auto* title = new QLabel(tr("Журнал"));
    title->setObjectName(QStringLiteral("title"));
    root->addWidget(title);
    auto* subtitle = new QLabel(tr("Найденные угрозы и итоги проверок. Записи хранятся на этом компьютере."));
    subtitle->setObjectName(QStringLiteral("muted"));
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);

    auto* bar = new QHBoxLayout;
    bar->setSpacing(10);
    m_count = new QLabel;
    m_count->setObjectName(QStringLiteral("muted"));
    bar->addWidget(m_count);
    bar->addStretch();
    m_export = new FancyButton(tr("Экспорт в CSV"), QStringLiteral("download"), FancyButton::Secondary);
    m_clear = new FancyButton(tr("Очистить журнал"), QStringLiteral("trash"), FancyButton::Danger);
    bar->addWidget(m_export);
    bar->addWidget(m_clear);
    root->addLayout(bar);
    connect(m_export, &QPushButton::clicked, this, &HistoryPage::exportCsv);
    connect(m_clear, &QPushButton::clicked, this, &HistoryPage::clearAll);

    auto* scroll = new SmoothScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("scrollBody"));
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 6, 0);
    contentLayout->setSpacing(8);
    m_rows = new QVBoxLayout;
    m_rows->setSpacing(8);
    contentLayout->addLayout(m_rows);

    m_empty = new QWidget;
    auto* emptyLayout = new QVBoxLayout(m_empty);
    emptyLayout->setSpacing(6);
    emptyLayout->addSpacing(40);
    auto* icon = new QLabel;
    icon->setPixmap(bcg::icons::pixmap(QStringLiteral("file"), th::kTrackOff, 72, devicePixelRatioF()));
    icon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(icon);
    auto* emptyTitle = new QLabel(tr("Журнал пуст"));
    emptyTitle->setObjectName(QStringLiteral("rowTitle"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);
    auto* emptyHint = new QLabel(tr("Здесь появятся итоги проверок и найденные угрозы."));
    emptyHint->setObjectName(QStringLiteral("muted"));
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyHint);
    contentLayout->addWidget(m_empty);
    contentLayout->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    connect(m_history, &bcg::History::changed, this, &HistoryPage::rebuild);
    rebuild();
}

void HistoryPage::rebuild()
{
    while (QLayoutItem* item = m_rows->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const QList<bcg::HistoryEntry>& all = m_history->entries();
    const int total = int(all.size());
    const int first = qMax(0, total - kShown);
    for (int i = total - 1; i >= first; --i) { // newest first
        const bcg::HistoryEntry& e = all.at(i);
        const bool threat = e.type == bcg::HistoryEntry::Threat;

        auto* row = new QFrame;
        row->setObjectName(QStringLiteral("tile"));
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(16, 10, 16, 10);
        layout->setSpacing(12);

        auto* icon = new QLabel;
        icon->setPixmap(bcg::icons::pixmap(threat ? QStringLiteral("warning") : QStringLiteral("check"),
                                           threat ? th::kDanger : th::kAccent, 24, devicePixelRatioF()));
        layout->addWidget(icon, 0, Qt::AlignTop);

        auto* texts = new QVBoxLayout;
        texts->setSpacing(2);
        auto* name = new QLabel(threat ? e.title.section(QStringLiteral(" — "), 0, 0) : e.title);
        name->setObjectName(QStringLiteral("rowTitle"));
        name->setToolTip(e.title);
        name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        texts->addWidget(name);
        if (!e.path.isEmpty()) {
            auto* path = new QLabel(QDir::toNativeSeparators(e.path));
            path->setObjectName(QStringLiteral("muted"));
            path->setToolTip(path->text());
            path->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
            texts->addWidget(path);
        }
        QStringList parts = {e.when.toString(QStringLiteral("dd.MM.yyyy HH:mm")), e.source};
        if (!e.action.isEmpty())
            parts << e.action;
        if (!e.note.isEmpty())
            parts << e.note;
        auto* details = new QLabel(parts.join(QStringLiteral(" · ")));
        details->setObjectName(QStringLiteral("muted"));
        details->setToolTip(details->text());
        details->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        texts->addWidget(details);
        layout->addLayout(texts, 1);
        m_rows->addWidget(row);
    }

    m_empty->setVisible(total == 0);
    m_export->setVisible(total > 0);
    m_clear->setVisible(total > 0);
    m_count->setText(total == 0 ? QString()
                     : total > kShown ? tr("Записей: %1 (показаны последние %2)").arg(total).arg(kShown)
                                      : tr("Записей: %1").arg(total));
}

void HistoryPage::exportCsv()
{
    const QString suggested =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/BadCodeGuard-журнал.csv");
    const QString path = QFileDialog::getSaveFileName(window(), tr("Экспорт журнала"), suggested,
                                                      tr("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    if (m_history->exportCsv(path))
        Toast::show(window(), tr("Журнал сохранён"), Toast::Kind::Success);
    else
        Toast::show(window(), tr("Не удалось сохранить файл"), Toast::Kind::Error);
}

void HistoryPage::clearAll()
{
    if (!ConfirmDialog::ask(window(), tr("Очистить журнал?"),
                            tr("Все записи (%1) будут удалены. Файлы в карантине это не затронет.")
                                .arg(m_history->entries().size()),
                            tr("Очистить"), true))
        return;
    m_history->clear();
    Toast::show(window(), tr("Журнал очищен"), Toast::Kind::Success);
}
