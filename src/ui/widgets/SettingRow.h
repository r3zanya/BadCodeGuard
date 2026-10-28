#pragma once

#include <QWidget>

class QLabel;

// One line of a settings card: title (with optional badge and tag), a
// description and a control (toggle or button) on the right.
class SettingRow : public QWidget {
    Q_OBJECT
public:
    SettingRow(const QString& title, const QString& description, QWidget* control,
               QWidget* parent = nullptr);

    void setBadge(const QString& text);   // accent pill after the title, e.g. a size
    void setTag(const QString& text, bool muted = false); // amber pill (grey if muted)
    void setDescription(const QString& text);

private:
    QLabel* m_description;
    QLabel* m_badge;
    QLabel* m_tag;
};
