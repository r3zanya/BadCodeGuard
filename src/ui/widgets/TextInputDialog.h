#pragma once

#include <QDialog>

class QLineEdit;

// Modal single-line text prompt in the app's style.
class TextInputDialog : public QDialog {
    Q_OBJECT
public:
    // Returns the entered text, or an empty string when cancelled.
    static QString getText(QWidget* parent, const QString& title, const QString& hint,
                           const QString& placeholder);

protected:
    void showEvent(QShowEvent*) override;

private:
    TextInputDialog(QWidget* parent, const QString& title, const QString& hint,
                    const QString& placeholder);

    QLineEdit* m_edit;
};
