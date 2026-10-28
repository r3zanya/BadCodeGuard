#pragma once

#include "engine/ExclusionList.h"

#include <QWidget>

#include <memory>

class QLabel;
class QVBoxLayout;

// "Исключения": files, folders, extensions and masks the engine skips.
class ExclusionsPage : public QWidget {
    Q_OBJECT
public:
    explicit ExclusionsPage(std::shared_ptr<bcg::ExclusionList> list, QWidget* parent = nullptr);

    // Reads the saved list from QSettings (call once at start-up).
    static void load(bcg::ExclusionList& list);

    // Adds an entry programmatically (e.g. from the quarantine page).
    void addExclusion(const bcg::Exclusion& item) { add(item); }

protected:
    void showEvent(QShowEvent*) override;

private:
    void addFile();
    void addFolder();
    void addExtension();
    void addMask();
    void add(const bcg::Exclusion& item);
    QWidget* makeRow(const bcg::Exclusion& item);
    void removeRow(QWidget* row, const bcg::Exclusion& item);
    void refreshEmptyState();
    void save() const;

    std::shared_ptr<bcg::ExclusionList> m_list;
    QVBoxLayout* m_rows = nullptr;
    QWidget* m_empty = nullptr;
    QLabel* m_count = nullptr;
};
