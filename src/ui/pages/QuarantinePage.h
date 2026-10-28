#pragma once

#include "engine/Quarantine.h"

#include <QHash>
#include <QWidget>

#include <memory>

class QLabel;
class QVBoxLayout;
class FancyButton;

// "Карантин": isolated files with restore and permanent delete.
class QuarantinePage : public QWidget {
    Q_OBJECT
public:
    explicit QuarantinePage(std::shared_ptr<bcg::Quarantine> quarantine, QWidget* parent = nullptr);

signals:
    // The user asked to keep a restored file out of future scans.
    void excludeRequested(const QString& path);

private:
    void sync();
    QWidget* makeRow(const bcg::QuarantineEntry& entry);
    void dropRow(QWidget* row);
    void restore(const bcg::QuarantineEntry& entry);
    void erase(const bcg::QuarantineEntry& entry);
    void eraseAll();

    std::shared_ptr<bcg::Quarantine> m_quarantine;
    QVBoxLayout* m_rows = nullptr;
    QWidget* m_empty = nullptr;
    QLabel* m_count = nullptr;
    FancyButton* m_clearAll = nullptr;
    QHash<QString, QWidget*> m_rowById;
};
