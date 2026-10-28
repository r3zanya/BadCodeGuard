#include "SystemRecoveryPage.h"

#include "sys/Elevation.h"
#include "sys/SystemInfo.h"
#include "sys/TempCleaner.h"
#include "sys/Tweaks.h"
#include "ui/widgets/ConfirmDialog.h"
#include "ui/widgets/ElevationBanner.h"
#include "ui/widgets/FancyButton.h"
#include "ui/widgets/RunDialog.h"
#include "ui/widgets/SettingRow.h"
#include "ui/widgets/Toast.h"
#include "ui/widgets/ToggleSwitch.h"

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QPointer>
#include <QThreadPool>
#include <QVBoxLayout>

namespace sys = bcg::sys;

namespace {

QFrame* makeCard()
{
    auto* card = new QFrame;
    card->setObjectName(QStringLiteral("card"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    return card;
}

void addRow(QFrame* card, QWidget* row)
{
    auto* layout = static_cast<QVBoxLayout*>(card->layout());
    if (layout->count() > 0) {
        auto* divider = new QFrame;
        divider->setObjectName(QStringLiteral("divider"));
        divider->setFixedHeight(1);
        layout->addWidget(divider);
    }
    layout->addWidget(row);
}

QLabel* makeSectionLabel(const QString& text)
{
    auto* label = new QLabel(text);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

} // namespace

SystemRecoveryPage::SystemRecoveryPage(QWidget* parent)
    : SubPage(tr("Система и восстановление"), parent)
{
    m_banner = new ElevationBanner;
    body()->addWidget(m_banner);

    // ---- Repair and cleanup ----
    body()->addWidget(makeSectionLabel(tr("Восстановление и очистка")));
    QFrame* repair = makeCard();

    auto* sfc = new FancyButton(tr("Восстановить"), QStringLiteral("recover"), FancyButton::Secondary);
    addRow(repair, new SettingRow(tr("Восстановление системы с помощью SFC"),
                                  tr("Проверяет системные файлы и заменяет повреждённые (sfc /scannow). "
                                     "Занимает 10–20 минут."),
                                  sfc));
    connect(sfc, &QPushButton::clicked, this, [this, sfc] {
        runConsoleTool(sfc, tr("Восстановление системы (SFC)"), QStringLiteral("sfc"),
                       {QStringLiteral("/scannow")}, tr("Проверка SFC завершена"));
    });

    auto* dism = new FancyButton(tr("Восстановить"), QStringLiteral("recover"), FancyButton::Secondary);
    addRow(repair, new SettingRow(tr("Восстановление системы с помощью DISM"),
                                  tr("Восстанавливает хранилище компонентов Windows "
                                     "(DISM /RestoreHealth). Нужен доступ в интернет."),
                                  dism));
    connect(dism, &QPushButton::clicked, this, [this, dism] {
        runConsoleTool(dism, tr("Восстановление системы (DISM)"), QStringLiteral("DISM"),
                       {QStringLiteral("/Online"), QStringLiteral("/Cleanup-Image"),
                        QStringLiteral("/RestoreHealth")},
                       tr("Восстановление DISM завершено"));
    });

    m_tempButton = new FancyButton(tr("Очистить"), QStringLiteral("trash"), FancyButton::Secondary);
    m_tempRow = new SettingRow(tr("Очистить папку TEMP"),
                               tr("Удаляет временные файлы пользователя. Файлы, которые сейчас "
                                  "используются, пропускаются."),
                               m_tempButton);
    m_tempRow->setBadge(tr("считаем…"));
    addRow(repair, m_tempRow);
    connect(m_tempButton, &QPushButton::clicked, this, &SystemRecoveryPage::clearTemp);
    body()->addWidget(repair);

    // ---- System parameters ----
    body()->addWidget(makeSectionLabel(tr("Параметры системы")));
    QFrame* params = makeCard();
    const auto& tweaks = sys::tweaks();
    for (int i = 0; i < tweaks.size(); ++i) {
        const sys::Tweak& tweak = tweaks[i];
        auto* toggle = new ToggleSwitch;
        toggle->setCheckedInstant(tweak.isApplied());
        auto* row = new SettingRow(tweak.title, tweak.description, toggle);
        if (tweak.needsReboot)
            row->setTag(tr("Нужна перезагрузка"));
        addRow(params, row);
        m_toggles.append(toggle);
        m_rows.append(row);
        connect(toggle, &QAbstractButton::toggled, this, [this, i](bool on) { onToggled(i, on); });
    }
    body()->addWidget(params);
    body()->addStretch();
}

void SystemRecoveryPage::activated()
{
    refreshTempSize();

    // Settings may have been changed elsewhere (Windows settings, other tools).
    const auto& tweaks = sys::tweaks();
    m_reverting = true;
    for (int i = 0; i < tweaks.size(); ++i) {
        const bool applied = tweaks[i].isApplied();
        if (m_toggles[i]->isChecked() != applied)
            m_toggles[i]->setChecked(applied);
    }
    m_reverting = false;
}

bool SystemRecoveryPage::requireAdmin()
{
    if (sys::isElevated())
        return true;
    m_banner->pulse();
    Toast::show(this, tr("Нужны права администратора. Перезапустите программу от его имени."),
                Toast::Kind::Error);
    return false;
}

void SystemRecoveryPage::runConsoleTool(FancyButton* button, const QString& title,
                                        const QString& program, const QStringList& arguments,
                                        const QString& doneMessage)
{
    if (!requireAdmin())
        return;
    button->setEnabled(false);
    auto* dialog = new RunDialog(window(), title, program, arguments);
    connect(dialog, &QObject::destroyed, button, [button] { button->setEnabled(true); });
    connect(dialog, &RunDialog::completed, this, [this, doneMessage](bool success) {
        Toast::show(this, success ? doneMessage : tr("Операция завершилась с ошибкой"),
                    success ? Toast::Kind::Success : Toast::Kind::Error);
    });
    dialog->show();
}

void SystemRecoveryPage::refreshTempSize()
{
    m_tempRow->setBadge(tr("считаем…"));
    QPointer<SystemRecoveryPage> self(this);
    QThreadPool::globalInstance()->start([self] {
        const qint64 size = sys::folderSize(sys::tempFolder());
        QMetaObject::invokeMethod(qApp, [self, size] {
            if (self)
                self->m_tempRow->setBadge(sys::info::formatSize(static_cast<double>(size)));
        }, Qt::QueuedConnection);
    });
}

void SystemRecoveryPage::clearTemp()
{
    const QString folder = sys::tempFolder();
    if (!ConfirmDialog::ask(window(), tr("Очистить папку TEMP?"),
                            tr("Будут удалены временные файлы из:\n%1\n\nФайлы, которые сейчас "
                               "используются программами, будут пропущены.")
                                .arg(folder),
                            tr("Очистить")))
        return;

    m_tempButton->setEnabled(false);
    m_tempRow->setBadge(tr("очистка…"));
    QPointer<SystemRecoveryPage> self(this);
    QThreadPool::globalInstance()->start([self, folder] {
        const sys::CleanResult result = sys::clearFolder(folder);
        const qint64 left = sys::folderSize(folder);
        QMetaObject::invokeMethod(qApp, [self, result, left] {
            if (!self)
                return;
            self->m_tempButton->setEnabled(true);
            self->m_tempRow->setBadge(sys::info::formatSize(static_cast<double>(left)));
            QString message = tr("Освобождено %1").arg(sys::info::formatSize(static_cast<double>(result.freedBytes)));
            if (result.skippedFiles > 0)
                message += tr(" · пропущено файлов: %1").arg(result.skippedFiles);
            Toast::show(self, message, Toast::Kind::Success);
        }, Qt::QueuedConnection);
    });
}

void SystemRecoveryPage::onToggled(int index, bool on)
{
    if (m_reverting)
        return;
    const sys::Tweak& tweak = sys::tweaks()[index];
    ToggleSwitch* toggle = m_toggles[index];

    auto revert = [this, toggle, on] {
        m_reverting = true;
        toggle->setChecked(!on);
        m_reverting = false;
    };

    if (m_busy) {
        revert();
        return;
    }
    if (tweak.needsAdmin && !requireAdmin()) {
        revert();
        return;
    }
    if (on && tweak.risky
        && !ConfirmDialog::ask(window(), tweak.title + QLatin1Char('?'),
                               tweak.description + tr("\n\nВы уверены, что хотите продолжить?"),
                               tr("Применить"), /*danger*/ true)) {
        revert();
        return;
    }

    m_busy = true;
    for (ToggleSwitch* t : m_toggles)
        t->setEnabled(false);

    QPointer<SystemRecoveryPage> self(this);
    QThreadPool::globalInstance()->start([self, index, on] {
        const sys::Tweak& t = sys::tweaks()[index];
        const bool ok = t.apply(on);
        const bool applied = t.isApplied();
        QMetaObject::invokeMethod(qApp, [self, index, on, ok, applied] {
            if (!self)
                return;
            const sys::Tweak& t = sys::tweaks()[index];
            self->m_busy = false;
            for (ToggleSwitch* toggle : self->m_toggles)
                toggle->setEnabled(true);

            // Show the real state, which may differ from what was requested.
            self->m_reverting = true;
            self->m_toggles[index]->setChecked(applied);
            self->m_reverting = false;

            if (!ok) {
                Toast::show(self, tr("Не удалось изменить параметр. Проверьте права доступа."),
                            Toast::Kind::Error);
            } else if (t.needsReboot) {
                Toast::show(self, tr("Готово. Перезагрузите компьютер, чтобы изменения вступили в силу."),
                            Toast::Kind::Success);
            } else {
                Toast::show(self, on ? tr("Параметр применён") : tr("Параметр возвращён"),
                            Toast::Kind::Success);
            }
        }, Qt::QueuedConnection);
    });
}
