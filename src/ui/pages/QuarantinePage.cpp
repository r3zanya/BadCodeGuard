#include "QuarantinePage.h"

#include "engine/IScanner.h"
#include "ui/Theme.h"
#include "ui/widgets/Animations.h"
#include "ui/widgets/ConfirmDialog.h"
#include "ui/widgets/FancyButton.h"
#include "ui/widgets/IconButton.h"
#include "ui/widgets/Icons.h"
#include "ui/widgets/SmoothScrollArea.h"
#include "ui/widgets/Toast.h"

#include <QDir>
#include <QEasingCurve>
#include <QFileInfo>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QPropertyAnimation>
#include <QVBoxLayout>

namespace th = bcg::theme;
using bcg::QuarantineEntry;

namespace {

QString kindText(int kind)
{
    switch (static_cast<bcg::ThreatKind>(kind)) {
    case bcg::ThreatKind::Signature: return QStringLiteral("по базе хэшей");
    case bcg::ThreatKind::Rule: return QStringLiteral("по правилу");
    case bcg::ThreatKind::Heuristic: return QStringLiteral("эвристика");
    case bcg::ThreatKind::Yara: return QStringLiteral("по правилу YARA");
    }
    return {};
}

QString sizeText(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

} // namespace

QuarantinePage::QuarantinePage(std::shared_ptr<bcg::Quarantine> quarantine, QWidget* parent)
    : QWidget(parent)
    , m_quarantine(std::move(quarantine))
{
    setObjectName(QStringLiteral("page"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 32, 36, 28);
    root->setSpacing(14);

    auto* title = new QLabel(tr("Карантин"));
    title->setObjectName(QStringLiteral("title"));
    root->addWidget(title);
    auto* subtitle = new QLabel(tr("Угрозы изолированы и не могут запуститься. Файл можно восстановить "
                                   "на прежнее место или удалить навсегда."));
    subtitle->setObjectName(QStringLiteral("muted"));
    subtitle->setWordWrap(true);
    root->addWidget(subtitle);

    auto* bar = new QHBoxLayout;
    m_count = new QLabel;
    m_count->setObjectName(QStringLiteral("muted"));
    bar->addWidget(m_count);
    bar->addStretch();
    m_clearAll = new FancyButton(tr("Очистить карантин"), QStringLiteral("trash"), FancyButton::Danger);
    bar->addWidget(m_clearAll);
    root->addLayout(bar);
    connect(m_clearAll, &QPushButton::clicked, this, &QuarantinePage::eraseAll);

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
    icon->setPixmap(bcg::icons::pixmap(QStringLiteral("quarantine"), th::kTrackOff, 72,
                                       devicePixelRatioF()));
    icon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(icon);
    auto* emptyTitle = new QLabel(tr("Карантин пуст"));
    emptyTitle->setObjectName(QStringLiteral("rowTitle"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);
    auto* emptyHint = new QLabel(tr("Сюда попадают найденные угрозы."));
    emptyHint->setObjectName(QStringLiteral("muted"));
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyHint);
    contentLayout->addWidget(m_empty);
    contentLayout->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    connect(m_quarantine.get(), &bcg::Quarantine::changed, this, &QuarantinePage::sync);
    sync();
}

// Brings the rows in line with the vault: new entries fade in at the top,
// vanished ones collapse away.
void QuarantinePage::sync()
{
    QList<QuarantineEntry> entries = m_quarantine->entries();
    std::sort(entries.begin(), entries.end(),
              [](const QuarantineEntry& a, const QuarantineEntry& b) { return a.when > b.when; });

    QSet<QString> alive;
    for (const QuarantineEntry& e : std::as_const(entries))
        alive.insert(e.id);

    for (auto it = m_rowById.begin(); it != m_rowById.end();) {
        if (alive.contains(it.key())) {
            ++it;
        } else {
            dropRow(it.value());
            it = m_rowById.erase(it);
        }
    }
    // Insert missing rows at their sorted position.
    int position = 0;
    for (const QuarantineEntry& e : std::as_const(entries)) {
        if (!m_rowById.contains(e.id)) {
            QWidget* row = makeRow(e);
            m_rows->insertWidget(position, row);
            m_rowById.insert(e.id, row);
            if (isVisible())
                bcg::fadeIn(row, 320);
        }
        ++position;
    }

    const int n = int(entries.size());
    m_empty->setVisible(n == 0);
    m_clearAll->setVisible(n > 0);
    m_count->setText(n == 0 ? QString() : tr("В карантине: %1").arg(n));
}

QWidget* QuarantinePage::makeRow(const QuarantineEntry& entry)
{
    auto* row = new QFrame;
    row->setObjectName(QStringLiteral("tile"));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 10, 8, 10);
    layout->setSpacing(12);

    auto* icon = new QLabel;
    icon->setPixmap(bcg::icons::pixmap(QStringLiteral("warning"), th::kDanger, 24, devicePixelRatioF()));
    layout->addWidget(icon, 0, Qt::AlignTop);

    auto* texts = new QVBoxLayout;
    texts->setSpacing(2);
    auto* name = new QLabel(QFileInfo(entry.originalPath).fileName());
    name->setObjectName(QStringLiteral("rowTitle"));
    name->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    texts->addWidget(name);
    auto* path = new QLabel(QDir::toNativeSeparators(entry.originalPath));
    path->setObjectName(QStringLiteral("muted"));
    path->setToolTip(path->text());
    path->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    texts->addWidget(path);
    auto* details = new QLabel(QStringLiteral("%1 · %2 · %3 · %4")
                                   .arg(entry.threat.section(QStringLiteral(" — "), 0, 0),
                                        kindText(entry.kind), sizeText(entry.size),
                                        entry.when.toString(QStringLiteral("dd.MM.yyyy HH:mm"))));
    details->setObjectName(QStringLiteral("muted"));
    details->setToolTip(entry.threat);
    details->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    texts->addWidget(details);
    layout->addLayout(texts, 1);

    auto* restoreBtn = new FancyButton(tr("Восстановить"), QStringLiteral("recover"), FancyButton::Secondary);
    layout->addWidget(restoreBtn, 0, Qt::AlignVCenter);
    auto* eraseBtn = new IconButton(QStringLiteral("trash"), tr("Удалить навсегда"));
    layout->addWidget(eraseBtn, 0, Qt::AlignVCenter);
    connect(restoreBtn, &QPushButton::clicked, this, [this, entry] { restore(entry); });
    connect(eraseBtn, &QAbstractButton::clicked, this, [this, entry] { erase(entry); });
    return row;
}

void QuarantinePage::dropRow(QWidget* row)
{
    row->setEnabled(false);
    auto* effect = new QGraphicsOpacityEffect(row);
    row->setGraphicsEffect(effect);
    auto* group = new QParallelAnimationGroup(row);
    auto* fade = new QPropertyAnimation(effect, "opacity", group);
    fade->setDuration(200);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    auto* collapse = new QPropertyAnimation(row, "maximumHeight", group);
    collapse->setDuration(280);
    collapse->setStartValue(row->height());
    collapse->setEndValue(0);
    collapse->setEasingCurve(QEasingCurve::InOutCubic);
    group->addAnimation(fade);
    group->addAnimation(collapse);
    connect(group, &QAbstractAnimation::finished, row, [row] { row->deleteLater(); });
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void QuarantinePage::restore(const QuarantineEntry& entry)
{
    const QString name = QFileInfo(entry.originalPath).fileName();
    if (!ConfirmDialog::ask(window(), tr("Восстановить «%1»?").arg(name),
                            tr("Файл вернётся в %1.\nУгроза: %2.\nВосстанавливайте его, только если "
                               "уверены, что он безопасен.")
                                .arg(QDir::toNativeSeparators(entry.originalPath), entry.threat),
                            tr("Восстановить"), true))
        return;

    bcg::QuarantineResult r = m_quarantine->restore(entry.id);
    if (r.targetExists) {
        if (!ConfirmDialog::ask(window(), tr("Файл уже существует"),
                                tr("По пути %1 уже лежит другой файл. Заменить его?")
                                    .arg(QDir::toNativeSeparators(entry.originalPath)),
                                tr("Заменить"), true))
            return;
        r = m_quarantine->restore(entry.id, true);
    }
    if (!r.ok) {
        Toast::show(window(), tr("Не удалось восстановить: %1").arg(r.error), Toast::Kind::Error);
        return;
    }
    Toast::show(window(), tr("Файл восстановлен"), Toast::Kind::Success);
    if (ConfirmDialog::ask(window(), tr("Добавить в исключения?"),
                           tr("Тогда «%1» не будет снова попадать в карантин при проверках.").arg(name),
                           tr("Добавить")))
        emit excludeRequested(r.entry.originalPath);
}

void QuarantinePage::erase(const QuarantineEntry& entry)
{
    if (!ConfirmDialog::ask(window(), tr("Удалить навсегда?"),
                            tr("«%1» будет удалён без возможности восстановления.")
                                .arg(QFileInfo(entry.originalPath).fileName()),
                            tr("Удалить"), true))
        return;
    m_quarantine->remove(entry.id);
}

void QuarantinePage::eraseAll()
{
    const int n = int(m_quarantine->entries().size());
    if (n == 0)
        return;
    if (!ConfirmDialog::ask(window(), tr("Очистить карантин?"),
                            tr("Все файлы (%1) будут удалены навсегда.").arg(n), tr("Очистить"), true))
        return;
    m_quarantine->removeAll();
    Toast::show(window(), tr("Карантин очищен"), Toast::Kind::Success);
}
