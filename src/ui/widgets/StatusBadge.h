#pragma once

#include "sys/SystemInfo.h"

#include <QWidget>

// Pill with a coloured dot and text, used for on/off style states.
class StatusBadge : public QWidget {
    Q_OBJECT
public:
    StatusBadge(const QString& text, bcg::sys::Status status, QWidget* parent = nullptr);

    QSize sizeHint() const override;

    static QColor colorFor(bcg::sys::Status status);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString m_text;
    bcg::sys::Status m_status;
};
