#pragma once

#include <QWidget>

class FadeStack;
class SubPage;

// "Расширенные параметры системы": a landing page with four section cards
// that open the sub-pages (and a back button to return).
class AdvancedPage : public QWidget {
    Q_OBJECT
public:
    explicit AdvancedPage(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent*) override;

private:
    void playLandingEntrance();
    void open(int index);
    void backToLanding();
    QWidget* buildLanding();

    FadeStack* m_stack;
    QList<class SectionCard*> m_cards;
    QList<SubPage*> m_pages;
};
