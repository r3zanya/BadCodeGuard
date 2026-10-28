#pragma once

#include "sys/AppxCatalog.h"
#include "ui/pages/SubPage.h"

#include <QList>
#include <QSet>

class ElevationBanner;
class FancyButton;
class QFrame;
class SegmentedTabs;
class SettingRow;

// "Удаление UWP-приложений": installed Store apps split into three groups.
class UwpPage : public SubPage {
    Q_OBJECT
public:
    explicit UwpPage(QWidget* parent = nullptr);

    void activated() override;

private:
    struct Item {
        const bcg::sys::AppEntry* entry;
        SettingRow* row;
        FancyButton* button;
        bcg::sys::AppGroup group;
    };

    QFrame* buildGroup(bcg::sys::AppGroup group);
    void showGroup(int index, bool animate);
    void refreshInstalled();
    void applyInstalled();
    void removeApp(int itemIndex);

    ElevationBanner* m_banner;
    SegmentedTabs* m_tabs;
    QFrame* m_groupCards[3] = {};
    QList<Item> m_items;
    QSet<QString> m_installed;
    bool m_loaded = false;
    bool m_busy = false;
};
