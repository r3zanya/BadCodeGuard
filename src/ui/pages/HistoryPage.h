#pragma once

#include <QWidget>

#include <memory>

class QLabel;
class QVBoxLayout;
class FancyButton;

namespace bcg {
class History;
}

// Journal of findings and scan runs, newest first.
class HistoryPage : public QWidget {
    Q_OBJECT
public:
    explicit HistoryPage(bcg::History* history, QWidget* parent = nullptr);

private:
    void rebuild();
    void exportCsv();
    void clearAll();

    bcg::History* m_history;
    QVBoxLayout* m_rows = nullptr;
    QWidget* m_empty = nullptr;
    QLabel* m_count = nullptr;
    FancyButton* m_export = nullptr;
    FancyButton* m_clear = nullptr;
};
