#pragma once

#include "ui/pages/SubPage.h"

#include <QList>

class ElevationBanner;
class FancyButton;
class SettingRow;
class ToggleSwitch;

// "Система и восстановление": SFC, DISM, TEMP cleanup and system toggles.
class SystemRecoveryPage : public SubPage {
    Q_OBJECT
public:
    explicit SystemRecoveryPage(QWidget* parent = nullptr);

    void activated() override;

private:
    bool requireAdmin();
    void runConsoleTool(FancyButton* button, const QString& title, const QString& program,
                        const QStringList& arguments, const QString& doneMessage);
    void refreshTempSize();
    void clearTemp();
    void onToggled(int index, bool on);

    ElevationBanner* m_banner;
    SettingRow* m_tempRow = nullptr;
    FancyButton* m_tempButton = nullptr;
    QList<ToggleSwitch*> m_toggles;
    QList<SettingRow*> m_rows;
    bool m_busy = false;
    bool m_reverting = false;
};
