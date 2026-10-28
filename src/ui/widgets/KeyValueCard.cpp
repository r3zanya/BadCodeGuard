#include "KeyValueCard.h"

#include "ui/widgets/StatusBadge.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
constexpr int kKeyWidth = 250;

void clear(QLayout* layout)
{
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        if (QLayout* child = item->layout()) {
            clear(child);
            delete child;
        }
        delete item;
    }
}
} // namespace

KeyValueCard::KeyValueCard(const QString& title, QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("card"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 10);
    root->setSpacing(4);

    m_title = new QLabel(title);
    m_title->setObjectName(QStringLiteral("sectionTitle"));
    root->addWidget(m_title);

    m_rows = new QVBoxLayout;
    m_rows->setSpacing(0);
    root->addLayout(m_rows);
}

void KeyValueCard::setTitle(const QString& title)
{
    m_title->setText(title);
}

void KeyValueCard::setRows(const QList<bcg::sys::InfoRow>& rows)
{
    // Rebuilding identical rows would only cause a visible flicker.
    bool same = rows.size() == m_current.size();
    for (int i = 0; same && i < rows.size(); ++i) {
        same = rows[i].key == m_current[i].key && rows[i].value == m_current[i].value
            && rows[i].status == m_current[i].status;
    }
    if (same)
        return;
    m_current = rows;
    clear(m_rows);
    for (int i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        if (i > 0) {
            auto* divider = new QFrame;
            divider->setObjectName(QStringLiteral("divider"));
            divider->setFixedHeight(1);
            m_rows->addWidget(divider);
        }
        auto* line = new QWidget;
        auto* layout = new QHBoxLayout(line);
        layout->setContentsMargins(0, 9, 0, 9);
        layout->setSpacing(16);

        auto* key = new QLabel(row.key);
        key->setObjectName(QStringLiteral("muted"));
        key->setFixedWidth(kKeyWidth);
        key->setWordWrap(true);
        layout->addWidget(key, 0, Qt::AlignTop);

        if (row.status != bcg::sys::Status::None) {
            layout->addWidget(new StatusBadge(row.value, row.status), 0, Qt::AlignVCenter);
            layout->addStretch();
        } else {
            auto* value = new QLabel(row.value.isEmpty() ? QStringLiteral("—") : row.value);
            value->setWordWrap(true);
            value->setTextInteractionFlags(Qt::TextSelectableByMouse);
            layout->addWidget(value, 1, Qt::AlignTop);
        }
        m_rows->addWidget(line);
    }
}
