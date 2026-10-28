#pragma once

#include <QFrame>

// Short message that slides up at the bottom of the window and fades away.
class Toast : public QFrame {
    Q_OBJECT
public:
    enum class Kind { Info, Success, Error };

    // Shows a toast on the window that contains `context`.
    static void show(QWidget* context, const QString& text, Kind kind = Kind::Info);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    Toast(QWidget* window, const QString& text, Kind kind);
    void dismiss();

    Kind m_kind;
    bool m_closing = false;
};
