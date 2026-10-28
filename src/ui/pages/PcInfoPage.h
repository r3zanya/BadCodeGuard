#pragma once

#include "ui/pages/SubPage.h"

#include <QList>

class FancyButton;
class InfoTile;
class IconButton;
class KeyValueCard;
class QVBoxLayout;
class ScoreCard;

// "Информация о ПК": six hardware tiles, an optional rating and the full details.
class PcInfoPage : public SubPage {
    Q_OBJECT
public:
    explicit PcInfoPage(QWidget* parent = nullptr);

    void activated() override;

private:
    void populate();
    void runRating();
    void saveReport();
    void copySummary();

    InfoTile* m_cpu;
    InfoTile* m_gpu;
    InfoTile* m_ram;
    InfoTile* m_vram;
    InfoTile* m_storage;
    InfoTile* m_device;
    ScoreCard* m_score;
    QVBoxLayout* m_details;
    QList<KeyValueCard*> m_cards;
    FancyButton* m_rateButton;
    bool m_populated = false;
};
