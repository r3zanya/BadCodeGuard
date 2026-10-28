#pragma once

#include <QWidget>

class QHBoxLayout;
class QVBoxLayout;

// Base of the "advanced system parameters" sub-pages: a header with a back
// button, a title and optional actions, above a scrollable body.
class SubPage : public QWidget {
    Q_OBJECT
public:
    explicit SubPage(const QString& title, QWidget* parent = nullptr);

    // Called every time the page is opened.
    virtual void activated() {}

signals:
    void backRequested();

protected:
    QVBoxLayout* body() const { return m_body; }
    void addHeaderAction(QWidget* widget);

private:
    QVBoxLayout* m_body;
    QHBoxLayout* m_header;
};
