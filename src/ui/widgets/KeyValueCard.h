#pragma once

#include "sys/SystemInfo.h"

#include <QFrame>

class QVBoxLayout;

// Card with a title and a list of "key: value" rows; rows with a status show
// the value as a coloured badge.
class KeyValueCard : public QFrame {
    Q_OBJECT
public:
    explicit KeyValueCard(const QString& title, QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setRows(const QList<bcg::sys::InfoRow>& rows);

private:
    QVBoxLayout* m_rows;
    QList<bcg::sys::InfoRow> m_current;
    class QLabel* m_title;
};
