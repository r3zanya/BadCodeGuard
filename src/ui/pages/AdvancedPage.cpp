#include "AdvancedPage.h"

#include "ui/pages/PcInfoPage.h"
#include "ui/pages/SystemRecoveryPage.h"
#include "ui/pages/UwpPage.h"
#include "ui/pages/WindowsInfoPage.h"
#include "ui/widgets/FadeStack.h"
#include "ui/widgets/SectionCard.h"

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

AdvancedPage::AdvancedPage(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("page"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);

    m_stack = new FadeStack;
    m_stack->addWidget(buildLanding());

    m_pages = {new SystemRecoveryPage, new UwpPage, new PcInfoPage, new WindowsInfoPage};
    for (SubPage* page : m_pages) {
        m_stack->addWidget(page);
        connect(page, &SubPage::backRequested, this, &AdvancedPage::backToLanding);
    }
    root->addWidget(m_stack);
}

QWidget* AdvancedPage::buildLanding()
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("page"));
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(36, 32, 36, 28);
    layout->setSpacing(6);

    auto* title = new QLabel(tr("Расширенные параметры системы"));
    title->setObjectName(QStringLiteral("title"));
    layout->addWidget(title);
    auto* subtitle = new QLabel(tr("Тонкая настройка Windows, восстановление, очистка и подробные сведения о компьютере."));
    subtitle->setObjectName(QStringLiteral("muted"));
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);
    layout->addSpacing(14);

    const struct {
        const char* icon;
        const char* title;
        const char* text;
    } sections[] = {
        {"recover", "Система и восстановление",
         "SFC и DISM, очистка TEMP, телеметрия, UAC, SmartScreen, гибернация и другие параметры."},
        {"package", "Удаление UWP-приложений",
         "Уберите лишние приложения Windows: Clipchamp, Новости, Погода и другие."},
        {"device", "Информация о ПК",
         "Процессор, видеокарта, память, накопители, оценка характеристик и экспорт в TXT."},
        {"window", "Информация о Windows",
         "Версия, сборка, активация, региональные параметры, виртуализация и защита."},
    };
    auto* grid = new QGridLayout;
    grid->setSpacing(16);
    for (int i = 0; i < 4; ++i) {
        auto* card = new SectionCard(QString::fromLatin1(sections[i].icon),
                                     QString::fromUtf8(sections[i].title),
                                     QString::fromUtf8(sections[i].text));
        grid->addWidget(card, i / 2, i % 2);
        m_cards.append(card);
        connect(card, &QAbstractButton::clicked, this, [this, i] { open(i); });
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);
    layout->addStretch();
    return page;
}

void AdvancedPage::open(int index)
{
    m_stack->setCurrentIndexAnimated(index + 1);
    m_pages[index]->activated();
}

void AdvancedPage::backToLanding()
{
    m_stack->setCurrentIndexAnimated(0);
    playLandingEntrance();
}

void AdvancedPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_stack->currentIndex() == 0)
        playLandingEntrance();
}

// The four cards rise in one after another.
void AdvancedPage::playLandingEntrance()
{
    for (int i = 0; i < m_cards.size(); ++i)
        m_cards[i]->playEntrance(120 + i * 90);
}
