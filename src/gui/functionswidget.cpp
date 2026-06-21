// SPDX-FileCopyrightText: 2009-2011, 2013-2014, 2016, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/functionswidget.h"

#include "core/functions.h"
#include "core/settings.h"
#include "gui/dockliststyle.h"
#include "gui/themedlineedit.h"

#include <QEvent>
#include <QString>
#include <QTimer>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QShortcut>
#include <QTreeWidget>
#include <QVBoxLayout>

FunctionsWidget::FunctionsWidget(QWidget* parent)
    : QWidget(parent)
    , m_filterTimer(new QTimer(this))
    , m_domain(new QComboBox(this))
    , m_domainLabel(new QLabel(this))
    , m_functions(new QTreeWidget(this))
    , m_noMatchLabel(new QLabel(m_functions))
    , m_searchFilter(new ThemedLineEdit(this))
    , m_searchLabel(new QLabel(this))
{
    constexpr int kControlRowHorizontalPadding = 8;
    constexpr int kControlRowVerticalPadding = 6;

    m_filterTimer->setInterval(500);
    m_filterTimer->setSingleShot(true);

    m_functions->setAutoScroll(true);
    m_functions->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_functions->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_functions->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_functions->setColumnCount(2);
    m_functions->setRootIsDecorated(false);
    m_functions->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_functions->setSelectionBehavior(QTreeWidget::SelectRows);
    DockListStyle::apply(m_functions);

    m_noMatchLabel->setAlignment(Qt::AlignCenter);
    m_noMatchLabel->adjustSize();
    m_noMatchLabel->hide();
    m_searchFilter->setClearButtonEnabled(true);

    m_domain->setEditable(false);
    m_domain->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    QWidget* domainBox = new QWidget(this);
    QHBoxLayout* domainLayout = new QHBoxLayout;
    domainLayout->addWidget(m_domainLabel);
    domainLayout->addWidget(m_domain);
    domainLayout->setContentsMargins(kControlRowHorizontalPadding,
                                     kControlRowVerticalPadding,
                                     kControlRowHorizontalPadding,
                                     kControlRowVerticalPadding);
    domainBox->setLayout(domainLayout);

    QWidget* searchBox = new QWidget(this);
    QHBoxLayout* searchLayout = new QHBoxLayout;
    searchLayout->addWidget(m_searchLabel);
    searchLayout->addWidget(m_searchFilter);
    searchLayout->setContentsMargins(kControlRowHorizontalPadding,
                                     kControlRowVerticalPadding,
                                     kControlRowHorizontalPadding,
                                     kControlRowVerticalPadding);
    searchBox->setLayout(searchLayout);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(domainBox);
    layout->addWidget(searchBox);
    layout->addWidget(m_functions);
    setLayout(layout);

    QWidget::setTabOrder(m_searchFilter, m_functions);
    setFocusProxy(m_searchFilter);

    retranslateText();

    connect(m_filterTimer, SIGNAL(timeout()), SLOT(updateList()));
    connect(m_domain, SIGNAL(activated(int)), SLOT(updateList()));
    connect(m_functions, SIGNAL(itemActivated(QTreeWidgetItem*, int)), SLOT(handleItemActivated(QTreeWidgetItem*, int)));
    connect(m_searchFilter, SIGNAL(textChanged(const QString &)), SLOT(triggerFilter()));
    QShortcut* returnShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    returnShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(returnShortcut, &QShortcut::activated, this, [this]() {
        if (const QTreeWidgetItem* current = m_functions->currentItem())
            emit functionSelected(current->text(0));
    });
    QShortcut* enterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), this);
    enterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(enterShortcut, &QShortcut::activated, this, [this]() {
        if (const QTreeWidgetItem* current = m_functions->currentItem())
            emit functionSelected(current->text(0));
    });

    updateList();
}

FunctionsWidget::~FunctionsWidget()
{
    m_filterTimer->stop();
}

void FunctionsWidget::updateList()
{
    setUpdatesEnabled(false);

    m_filterTimer->stop();
    m_functions->clear();
    QString term = m_searchFilter->text();
    const QString selectedDomain = m_domain->currentText();
    QStringList functionNames = FunctionRepo::instance()->getIdentifiers();
    FunctionRepo::instance()->retranslateText();

    for (int k = 0; k < functionNames.count(); ++k) {
        const QString identifier = functionNames.at(k);
        Function* f = FunctionRepo::instance()->find(identifier);
        if (!f)
            continue;

        QStringList str;
        str << identifier << f->name();

        const bool domainMatches = selectedDomain == tr("All") || f->domain() == selectedDomain;
        if (term.isEmpty()
            || str.at(0).contains(term, Qt::CaseInsensitive)
            || str.at(1).contains(term, Qt::CaseInsensitive))
        {
            if (domainMatches) {
                QTreeWidgetItem* item = new QTreeWidgetItem(m_functions, str);
                if (layoutDirection() == Qt::LeftToRight) {
                    item->setTextAlignment(0, Qt::AlignLeft);
                    item->setTextAlignment(1, Qt::AlignLeft);
                } else {
                    item->setTextAlignment(0, Qt::AlignRight);
                    item->setTextAlignment(1, Qt::AlignLeft);
                }
            }
        }
    }

    m_functions->resizeColumnToContents(0);
    m_functions->resizeColumnToContents(1);

    if (m_functions->topLevelItemCount() > 0) {
        m_noMatchLabel->hide();
        m_functions->sortItems(0, Qt::AscendingOrder);
    } else {
        DockListStyle::showCenteredNoMatchLabel(m_functions, m_noMatchLabel);
    }

    setUpdatesEnabled(true);
}

void FunctionsWidget::retranslateText()
{
    QStringList titles;
    const QString identifier = tr("Identifier");
    const QString name = tr("Name");
    titles << identifier << name;
    m_functions->setHeaderLabels(titles);

    m_searchLabel->setText(tr("Search"));
    m_domainLabel->setText(tr("Domain"));
    const QString selectedDomain = m_domain->currentText();
    m_domain->clear();
    m_domain->addItem(tr("All"));
    m_domain->addItems(FunctionRepo::instance()->domains());
    const int selectedIndex = m_domain->findText(selectedDomain);
    m_domain->setCurrentIndex(selectedIndex >= 0 ? selectedIndex : 0);
    m_noMatchLabel->setText(tr("No match found"));

    updateList();
}

QList<QTreeWidgetItem*> FunctionsWidget::selectedItems() const
{
    return m_functions->selectedItems();
}

const QTreeWidgetItem* FunctionsWidget::currentItem() const
{
    return m_functions->currentItem();
}

QString FunctionsWidget::searchText() const { return m_searchFilter->text(); }
void FunctionsWidget::setSearchText(const QString& text) { m_searchFilter->setText(text); }
QString FunctionsWidget::selectedDomain() const { return m_domain->currentText(); }
void FunctionsWidget::setSelectedDomain(const QString& domain)
{
    const int index = m_domain->findText(domain);
    if (index >= 0) {
        m_domain->setCurrentIndex(index);
        updateList();
    }
}

void FunctionsWidget::handleItemActivated(QTreeWidgetItem* item, int /*column*/)
{
    emit functionSelected(item->text(0));
}

void FunctionsWidget::clearSelection(QTreeWidgetItem*)
{
    m_functions->clearSelection();
}

void FunctionsWidget::triggerFilter()
{
    m_filterTimer->stop();
    m_filterTimer->start();
}

void FunctionsWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        FunctionRepo::instance()->retranslateText();
        retranslateText();
    } else
        QWidget::changeEvent(event);
}
