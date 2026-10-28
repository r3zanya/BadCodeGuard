#include "AppxCatalog.h"

#include "sys/ProcessRunner.h"

#include <QObject>

namespace bcg::sys {

namespace {

QList<AppEntry> extraApps()
{
    return {
        {QObject::tr("Clipchamp"), QObject::tr("Видеоредактор от Microsoft"), {QStringLiteral("Clipchamp.Clipchamp")}},
        {QObject::tr("Новости"), QObject::tr("Лента новостей Microsoft Start"), {QStringLiteral("Microsoft.BingNews")}},
        {QObject::tr("Погода"), QObject::tr("Прогноз погоды"), {QStringLiteral("Microsoft.BingWeather")}},
        {QObject::tr("Solitaire Collection"), QObject::tr("Карточные игры с рекламой"), {QStringLiteral("Microsoft.MicrosoftSolitaireCollection")}},
        {QObject::tr("Центр отзывов"), QObject::tr("Отправка отзывов в Microsoft"), {QStringLiteral("Microsoft.WindowsFeedbackHub")}},
        {QObject::tr("Советы"), QObject::tr("Подсказки по работе с Windows"), {QStringLiteral("Microsoft.Getstarted")}},
        {QObject::tr("Microsoft 365 (Office)"), QObject::tr("Ярлык-приложение для Office"), {QStringLiteral("Microsoft.MicrosoftOfficeHub")}},
        {QObject::tr("Получить помощь"), QObject::tr("Справка и поддержка Microsoft"), {QStringLiteral("Microsoft.GetHelp")}},
        {QObject::tr("Cortana"), QObject::tr("Голосовой помощник"), {QStringLiteral("Microsoft.549981C3F5F10")}},
        {QObject::tr("Люди"), QObject::tr("Контакты"), {QStringLiteral("Microsoft.People")}},
        {QObject::tr("Почта и Календарь"), QObject::tr("Встроенный почтовый клиент"), {QStringLiteral("microsoft.windowscommunicationsapps")}},
        {QObject::tr("Карты"), QObject::tr("Офлайн-карты Windows"), {QStringLiteral("Microsoft.WindowsMaps")}},
        {QObject::tr("Записки"), QObject::tr("Sticky Notes"), {QStringLiteral("Microsoft.MicrosoftStickyNotes")}},
        {QObject::tr("Microsoft To Do"), QObject::tr("Список задач"), {QStringLiteral("Microsoft.Todos")}},
        {QObject::tr("Power Automate"), QObject::tr("Автоматизация задач"), {QStringLiteral("Microsoft.PowerAutomateDesktop")}},
        {QObject::tr("Dev Home"), QObject::tr("Панель для разработчиков"), {QStringLiteral("Microsoft.Windows.DevHome")}},
        {QObject::tr("Смешанная реальность"), QObject::tr("Портал смешанной реальности"), {QStringLiteral("Microsoft.MixedReality.Portal")}},
        {QObject::tr("Средство 3D-просмотра"), QObject::tr("Просмотр 3D-моделей"), {QStringLiteral("Microsoft.Microsoft3DViewer")}},
        {QObject::tr("OneNote"), QObject::tr("Приложение для заметок"), {QStringLiteral("Microsoft.Office.OneNote")}},
        {QObject::tr("Skype"), QObject::tr("Видеозвонки"), {QStringLiteral("Microsoft.SkypeApp")}},
        {QObject::tr("Copilot"), QObject::tr("ИИ-помощник Microsoft"), {QStringLiteral("Microsoft.Copilot")}},
        {QObject::tr("Outlook (новый)"), QObject::tr("Новая почта Outlook"), {QStringLiteral("Microsoft.OutlookForWindows")}},
        {QObject::tr("Быстрая помощь"), QObject::tr("Удалённая помощь Microsoft"), {QStringLiteral("MicrosoftCorporationII.QuickAssist")}},
    };
}

QList<AppEntry> popularApps()
{
    return {
        {QObject::tr("Paint"), QObject::tr("Простой графический редактор"), {QStringLiteral("Microsoft.Paint")}},
        {QObject::tr("Камера"), QObject::tr("Съёмка фото и видео с веб-камеры"), {QStringLiteral("Microsoft.WindowsCamera")}},
        {QObject::tr("Ваш телефон"), QObject::tr("Связь с телефоном (Phone Link)"), {QStringLiteral("Microsoft.YourPhone")}},
    };
}

QList<AppEntry> essentialApps()
{
    return {
        {QObject::tr("Microsoft Store"), QObject::tr("Магазин приложений. Без него не установить игры и программы из Store"),
         {QStringLiteral("Microsoft.WindowsStore")}},
        {QObject::tr("Xbox"), QObject::tr("Xbox, игровая панель и службы входа для игр"),
         {QStringLiteral("Microsoft.GamingApp"), QStringLiteral("Microsoft.XboxGamingOverlay"),
          QStringLiteral("Microsoft.XboxIdentityProvider"), QStringLiteral("Microsoft.XboxSpeechToTextOverlay"),
          QStringLiteral("Microsoft.Xbox.TCUI")}},
        {QObject::tr("Блокнот"), QObject::tr("Стандартный текстовый редактор"), {QStringLiteral("Microsoft.WindowsNotepad")}},
        {QObject::tr("Калькулятор"), QObject::tr("Стандартный калькулятор"), {QStringLiteral("Microsoft.WindowsCalculator")}},
    };
}

} // namespace

const QList<AppEntry>& appsIn(AppGroup group)
{
    static const QList<AppEntry> extra = extraApps();
    static const QList<AppEntry> popular = popularApps();
    static const QList<AppEntry> essential = essentialApps();
    switch (group) {
    case AppGroup::Extra: return extra;
    case AppGroup::Popular: return popular;
    case AppGroup::Essential: return essential;
    }
    return extra;
}

QSet<QString> installedPackages()
{
    QString output;
    const int code = runHidden(
        QStringLiteral("powershell.exe"),
        {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-Command"),
         QStringLiteral("Get-AppxPackage | ForEach-Object { $_.Name }")},
        60000, &output);
    QSet<QString> names;
    if (code != 0)
        return names;
    for (const QString& line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        names.insert(line.trimmed().toLower());
    return names;
}

bool isInstalled(const AppEntry& entry, const QSet<QString>& installed)
{
    for (const QString& package : entry.packages) {
        if (installed.contains(package.toLower()))
            return true;
    }
    return false;
}

QString removalScript(const AppEntry& entry)
{
    QString script = QStringLiteral("[Console]::OutputEncoding = [Text.Encoding]::UTF8\n"
                                    "$ErrorActionPreference = 'Continue'\n");
    for (const QString& package : entry.packages) {
        script += QStringLiteral(
                      "Write-Output 'Удаление пакета %1'\n"
                      "Get-AppxPackage -AllUsers -Name '%1' | Remove-AppxPackage -AllUsers\n"
                      "Get-AppxProvisionedPackage -Online | Where-Object { $_.DisplayName -eq '%1' } | "
                      "Remove-AppxProvisionedPackage -Online | Out-Null\n")
                      .arg(package);
    }
    script += QStringLiteral("Write-Output 'Готово'\n");
    return script;
}

} // namespace bcg::sys
