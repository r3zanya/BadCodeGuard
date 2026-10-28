#pragma once

#include "ui/pages/SubPage.h"

#include <QList>

class InfoTile;
class KeyValueCard;
class QVBoxLayout;

// "Информация о Windows": overview tiles and grouped system/protection state.
class WindowsInfoPage : public SubPage {
    Q_OBJECT
public:
    explicit WindowsInfoPage(QWidget* parent = nullptr);

    void activated() override;

private:
    void populate();

    QList<InfoTile*> m_tiles;
    QVBoxLayout* m_groups;
    QList<KeyValueCard*> m_cards;
    bool m_populated = false;
};
