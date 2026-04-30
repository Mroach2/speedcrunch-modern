// SPDX-FileCopyrightText: 2009-2011, 2013-2016, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/variablelistwidget.h"

#include "core/evaluator.h"
#include "core/settings.h"
#include "core/numberformatter.h"

#include <QEvent>
#include <QTimer>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QShortcut>
#include <QTreeWidget>
#include <QVBoxLayout>

static QString formatValue(const Quantity &value);

VariableListWidget::VariableListWidget(QWidget* parent)
    : QWidget(parent)
    , m_filterTimer(new QTimer(this))
    , m_variables(new QTreeWidget(this))
    , m_noMatchLabel(new QLabel(m_variables))
    , m_searchFilter(new QLineEdit(this))
    , m_searchLabel(new QLabel(this))
    , m_pendingRefresh(false)
{
    m_filterTimer->setInterval(500);
    m_filterTimer->setSingleShot(true);

    m_variables->setAlternatingRowColors(true);
    m_variables->setAutoScroll(true);
    m_variables->setColumnCount(3);
    m_variables->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_variables->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_variables->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_variables->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_variables->setRootIsDecorated(false);
    m_variables->setSelectionBehavior(QTreeWidget::SelectRows);
    m_variables->setCursor(QCursor(Qt::PointingHandCursor));

    m_noMatchLabel->setAlignment(Qt::AlignCenter);
    m_noMatchLabel->adjustSize();
    m_noMatchLabel->hide();
    m_searchFilter->setClearButtonEnabled(true);

    QWidget* searchBox = new QWidget(this);
    QHBoxLayout* searchLayout = new QHBoxLayout;
    searchLayout->addWidget(m_searchLabel);
    searchLayout->addWidget(m_searchFilter);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchBox->setLayout(searchLayout);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins(3, 3, 3, 3);
    layout->addWidget(searchBox);
    layout->addWidget(m_variables);
    setLayout(layout);

    QMenu* contextMenu = new QMenu(m_variables);
    m_insertAction = new QAction("", contextMenu);
    m_editAction = new QAction("", contextMenu);
    m_deleteAction = new QAction("", contextMenu);
    m_deleteAllAction = new QAction("", contextMenu);
    m_variables->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_variables->addAction(m_insertAction);
    m_variables->addAction(m_editAction);
    m_variables->addAction(m_deleteAction);
    m_variables->addAction(m_deleteAllAction);

    QWidget::setTabOrder(m_searchFilter, m_variables);
    setFocusProxy(m_searchFilter);

    retranslateText();

    connect(m_filterTimer, SIGNAL(timeout()), SLOT(updateList()));
    connect(m_searchFilter, SIGNAL(textChanged(const QString&)), SLOT(triggerFilter()));
    connect(m_variables, SIGNAL(itemActivated(QTreeWidgetItem*, int)), SLOT(activateItem()));
    connect(m_insertAction, SIGNAL(triggered()), SLOT(activateItem()));
    connect(m_editAction, SIGNAL(triggered()), SLOT(editItem()));
    connect(m_deleteAction, SIGNAL(triggered()), SLOT(deleteItem()));
    connect(m_deleteAllAction, SIGNAL(triggered()), SLOT(deleteAllItems()));
    QShortcut* returnShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    returnShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(returnShortcut, &QShortcut::activated, this, [this]() { activateItem(); });
    QShortcut* enterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), this);
    enterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(enterShortcut, &QShortcut::activated, this, [this]() { activateItem(); });

    updateList();
}

VariableListWidget::~VariableListWidget()
{
    m_filterTimer->stop();
}

void VariableListWidget::updateList()
{
    if (!isVisible()) {
        m_pendingRefresh = true;
        return;
    }

    setUpdatesEnabled(false);

    m_filterTimer->stop();
    m_variables->clear();
    QString term = m_searchFilter->text();
    QList<Variable> variables = Evaluator::instance()->getUserDefinedVariables();

    for (int i = 0; i < variables.count(); ++i) {
        QString varName = variables.at(i).identifier();

        QStringList namesAndValues;
        namesAndValues << varName << formatValue(variables.at(i).value())
                       << variables.at(i).description();

        if (term.isEmpty()
            || namesAndValues.at(0).contains(term, Qt::CaseInsensitive)
            || namesAndValues.at(1).contains(term, Qt::CaseInsensitive)
            || namesAndValues.at(2).contains(term, Qt::CaseInsensitive))
        {
            QTreeWidgetItem* item = new QTreeWidgetItem(m_variables, namesAndValues);
            item->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
            item->setTextAlignment(1, Qt::AlignLeft | Qt::AlignVCenter);
            item->setTextAlignment(2, Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    m_variables->resizeColumnToContents(0);
    m_variables->resizeColumnToContents(1);
    m_variables->resizeColumnToContents(2);

    if (m_variables->topLevelItemCount() > 0) {
        m_noMatchLabel->hide();
        m_variables->sortItems(0, Qt::AscendingOrder);
    } else {
        m_noMatchLabel->setGeometry(m_variables->geometry());
        m_noMatchLabel->show();
        m_noMatchLabel->raise();
    }

    setUpdatesEnabled(true);
    m_pendingRefresh = false;
}

void VariableListWidget::retranslateText()
{
    QStringList titles;
    titles << tr("Name") << tr("Value") << tr("Description");
    m_variables->setHeaderLabels(titles);

    m_searchLabel->setText(tr("Search"));
    m_noMatchLabel->setText(tr("No match found"));

    m_insertAction->setText(tr("Insert"));
    m_editAction->setText(tr("Edit"));
    m_deleteAction->setText(tr("Delete"));
    m_deleteAllAction->setText(tr("Delete All"));

    QTimer::singleShot(0, this, SLOT(updateList()));
}

QTreeWidgetItem* VariableListWidget::currentItem() const
{
    return m_variables->currentItem();
}

QString VariableListWidget::searchText() const { return m_searchFilter->text(); }
void VariableListWidget::setSearchText(const QString& text) { m_searchFilter->setText(text); }

void VariableListWidget::activateItem()
{
    if (!currentItem() || m_variables->selectedItems().isEmpty())
        return;
    emit variableSelected(currentItem()->text(0));
}

void VariableListWidget::deleteItem()
{
    if (!currentItem() || m_variables->selectedItems().isEmpty())
        return;
    Evaluator::instance()->unsetVariable(currentItem()->text(0));
    updateList();
}

void VariableListWidget::editItem()
{
    if (!currentItem() || m_variables->selectedItems().isEmpty())
        return;
    QString editedExpression = currentItem()->text(0) + " = " + currentItem()->text(1);
    const QString description = currentItem()->text(2).trimmed();
    if (!description.isEmpty())
        editedExpression += " ? " + description;
    emit variableEdited(editedExpression);
}

void VariableListWidget::deleteAllItems()
{
    Evaluator::instance()->unsetAllUserDefinedVariables();
    updateList();
}

void VariableListWidget::triggerFilter()
{
    m_filterTimer->stop();
    m_filterTimer->start();
}

void VariableListWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        setLayoutDirection(Qt::LeftToRight);
        retranslateText();
        return;
    }
    QWidget::changeEvent(event);
}

void VariableListWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete) {
        deleteItem();
        event->accept();
        return;
    } else if (event->key() == Qt::Key_E) {
        editItem();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void VariableListWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_pendingRefresh)
        updateList();
}

static QString formatValue(const Quantity& value)
{
    return NumberFormatter::format(value);
}
