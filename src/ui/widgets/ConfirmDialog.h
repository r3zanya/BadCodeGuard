#pragma once

#include <QDialog>

// Modal yes/no question in the app's style. Returns true when confirmed.
class ConfirmDialog : public QDialog {
    Q_OBJECT
public:
    static bool ask(QWidget* parent, const QString& title, const QString& text,
                    const QString& confirmText, bool danger = false);

protected:
    void showEvent(QShowEvent*) override;

private:
    ConfirmDialog(QWidget* parent, const QString& title, const QString& text,
                  const QString& confirmText, bool danger);
};
