#include "UwpPage.h"

#include "sys/Elevation.h"
#include "sys/ProcessRunner.h"
#include "ui/Theme.h"
#include "ui/widgets/Animations.h"
#include "ui/widgets/ConfirmDialog.h"
#include "ui/widgets/ElevationBanner.h"
#include "ui/widgets/FancyButton.h"
#include "ui/widgets/Icons.h"
#include "ui/widgets/SegmentedTabs.h"
#include "ui/widgets/SettingRow.h"
#include "ui/widgets/Toast.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QThreadPool>
#include <QVBoxLayout>

namespace sys = bcg::sys;
using sys::AppGroup;

namespace {

constexpr AppGroup kGroups[] = {AppGroup::Extra, AppGroup::Popular, AppGroup::Essential};

QString encodedCommand(const QString& script)
{
    const QByteArray utf16(reinterpret_cast<const char*>(script.utf16()), script.size() * 2);
    return QString::fromLatin1(utf16.toBase64());
}

} // namespace

UwpPage::UwpPage(QWidget* parent)
    : SubPage(tr("Удаление UWP-приложений"), parent)
{
    m_banner = new ElevationBanner;
    body()->addWidget(m_banner);

    m_tabs = new SegmentedTabs;
    m_tabs->addTab(tr("Лишние приложения"));
    m_tabs->addTab(tr("Популярные"));
    m_tabs->addTab(tr("Необходимые"));
    auto* tabRow = new QHBoxLayout;
    tabRow->addWidget(m_tabs);
    tabRow->addStretch();
    body()->addLayout(tabRow);

    for (int i = 0; i < 3; ++i) {
        m_groupCards[i] = buildGroup(kGroups[i]);
        body()->addWidget(m_groupCards[i]);
    }
    body()->addStretch();
    showGroup(0, false);
    connect(m_tabs, &SegmentedTabs::currentChanged, this, [this](int i) { showGroup(i, true); });
}

QFrame* UwpPage::buildGroup(AppGroup group)
{
    auto* card = new QFrame;
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    const QString note = group == AppGroup::Extra
        ? tr("Приложения, без которых система работает полностью. Их можно удалить, чтобы освободить место и убрать лишнее из меню «Пуск».")
        : group == AppGroup::Popular
        ? tr("Часто используемые приложения. Удаляйте только те, которыми не пользуетесь.")
        : tr("Внимание: эти приложения нужны многим программам и играм. Их удаление может нарушить работу Windows и сторонних приложений.");
    auto* hint = new QLabel(note);
    hint->setObjectName(group == AppGroup::Essential ? QStringLiteral("warnText") : QStringLiteral("muted"));
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* list = new QFrame;
    list->setObjectName(QStringLiteral("card"));
    auto* listLayout = new QVBoxLayout(list);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    const auto& apps = sys::appsIn(group);
    for (int i = 0; i < apps.size(); ++i) {
        const sys::AppEntry& entry = apps[i];
        auto* button = new FancyButton(tr("Удалить"), QStringLiteral("trash"), FancyButton::Secondary);
        auto* row = new SettingRow(entry.title, entry.description, button);
        row->setTag(tr("проверка…"), /*muted*/ true);
        button->setEnabled(false);

        if (i > 0) {
            auto* divider = new QFrame;
            divider->setObjectName(QStringLiteral("divider"));
            divider->setFixedHeight(1);
            listLayout->addWidget(divider);
        }
        listLayout->addWidget(row);

        const int itemIndex = m_items.size();
        m_items.append({&entry, row, button, group});
        connect(button, &QPushButton::clicked, this, [this, itemIndex] { removeApp(itemIndex); });
    }
    layout->addWidget(list);
    return card;
}

void UwpPage::showGroup(int index, bool animate)
{
    for (int i = 0; i < 3; ++i)
        m_groupCards[i]->setVisible(i == index);
    if (animate)
        bcg::fadeIn(m_groupCards[index], 300);
}

void UwpPage::activated()
{
    refreshInstalled();
}

void UwpPage::refreshInstalled()
{
    QPointer<UwpPage> self(this);
    QThreadPool::globalInstance()->start([self] {
        const QSet<QString> installed = sys::installedPackages();
        QMetaObject::invokeMethod(qApp, [self, installed] {
            if (!self)
                return;
            self->m_installed = installed;
            self->m_loaded = !installed.isEmpty();
            self->applyInstalled();
        }, Qt::QueuedConnection);
    });
}

void UwpPage::applyInstalled()
{
    for (const Item& item : m_items) {
        if (!m_loaded) {
            item.row->setTag(tr("не удалось определить"), true);
            item.button->setEnabled(!m_busy);
            continue;
        }
        const bool installed = sys::isInstalled(*item.entry, m_installed);
        item.row->setBadge(installed ? tr("Установлено") : QString());
        item.row->setTag(installed ? QString() : tr("Не установлено"), true);
        item.button->setEnabled(installed && !m_busy);
    }
}

void UwpPage::removeApp(int itemIndex)
{
    if (m_busy)
        return;
    const Item& item = m_items[itemIndex];
    if (!sys::isElevated()) {
        m_banner->pulse();
        Toast::show(this, tr("Нужны права администратора. Перезапустите программу от его имени."),
                    Toast::Kind::Error);
        return;
    }

    const bool essential = item.group == AppGroup::Essential;
    const QString text = essential
        ? tr("«%1» относится к необходимым приложениям. Её удаление может нарушить работу Windows "
             "и других программ, а вернуть её бывает непросто.\n\nУдалить?")
              .arg(item.entry->title)
        : tr("Приложение «%1» будет удалено для всех пользователей этого компьютера.")
              .arg(item.entry->title);
    if (!ConfirmDialog::ask(window(), tr("Удалить «%1»?").arg(item.entry->title), text, tr("Удалить"),
                            essential))
        return;

    m_busy = true;
    for (const Item& other : m_items)
        other.button->setEnabled(false);
    item.button->setText(tr("Удаление…"));
    item.button->updateGeometry();

    auto* runner = new sys::ProcessRunner(this);
    runner->setEncoding(sys::ProcessRunner::Encoding::Utf8);
    const QString title = item.entry->title;
    FancyButton* button = item.button;
    connect(runner, &sys::ProcessRunner::finished, this, [this, runner, title, button](int code, bool ok) {
        runner->deleteLater();
        m_busy = false;
        button->setText(tr("Удалить"));
        button->updateGeometry();
        const bool success = ok && code == 0;
        Toast::show(this, success ? tr("«%1» удалено").arg(title) : tr("Не удалось удалить «%1»").arg(title),
                    success ? Toast::Kind::Success : Toast::Kind::Error);
        refreshInstalled();
    });
    runner->start(QStringLiteral("powershell.exe"),
                  {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                   QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                   QStringLiteral("-EncodedCommand"), encodedCommand(sys::removalScript(*item.entry))});
}
