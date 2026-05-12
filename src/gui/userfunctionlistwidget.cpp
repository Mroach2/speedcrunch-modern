// SPDX-FileCopyrightText: 2014-2016, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/userfunctionlistwidget.h"

#include "core/evaluator.h"
#include "gui/displayformatutils.h"
#include "gui/dockliststyle.h"
#include "core/settings.h"
#include "core/unicodechars.h"

#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QShortcut>
#include <QTreeWidget>
#include <QVBoxLayout>


UserFunctionListWidget::UserFunctionListWidget(QWidget* parent)
    : QWidget(parent)
    , m_filterTimer(new QTimer(this))
    , m_userFunctions(new QTreeWidget(this))
    , m_noMatchLabel(new QLabel(m_userFunctions))
    , m_searchFilter(new QLineEdit(this))
    , m_searchLabel(new QLabel(this))
    , m_evaluator(Evaluator::instance())
    , m_pendingRefresh(false)
{
    m_filterTimer->setInterval(500);
    m_filterTimer->setSingleShot(true);

    m_userFunctions->setAutoScroll(true);
    m_userFunctions->setColumnCount(3);
    m_userFunctions->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_userFunctions->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_userFunctions->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_userFunctions->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_userFunctions->setRootIsDecorated(false);
    m_userFunctions->setSelectionBehavior(QTreeWidget::SelectRows);
    DockListStyle::apply(m_userFunctions);

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
    layout->addWidget(m_userFunctions);
    setLayout(layout);

    QMenu* contextMenu = new QMenu(m_userFunctions);
    m_insertAction = new QAction("", contextMenu);
    m_editAction = new QAction("", contextMenu);
    m_deleteAction = new QAction("", contextMenu);
    m_deleteAllAction = new QAction("", contextMenu);
    m_userFunctions->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_userFunctions->addAction(m_insertAction);
    m_userFunctions->addAction(m_editAction);
    m_userFunctions->addAction(m_deleteAction);
    m_userFunctions->addAction(m_deleteAllAction);

    QWidget::setTabOrder(m_searchFilter, m_userFunctions);
    setFocusProxy(m_searchFilter);

    retranslateText();

    connect(m_filterTimer, SIGNAL(timeout()), SLOT(updateList()));
    connect(m_searchFilter, SIGNAL(textChanged(const QString&)), SLOT(triggerFilter()));
    connect(m_userFunctions, SIGNAL(itemActivated(QTreeWidgetItem*, int)), SLOT(activateItem()));
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

UserFunctionListWidget::~UserFunctionListWidget()
{
    m_filterTimer->stop();
}

void UserFunctionListWidget::updateList()
{
    if (!isVisible()) {
        m_pendingRefresh = true;
        return;
    }

    setUpdatesEnabled(false);

    m_filterTimer->stop();
    m_userFunctions->clear();
    QString term = m_searchFilter->text();
    QList<UserFunction> userFunctions = m_evaluator->getUserFunctions();

    for (int i = 0; i < userFunctions.count(); ++i) {
        QString fname = userFunctions.at(i).name() + "(" + userFunctions.at(i).arguments().join(";")  + ")";

        QStringList namesAndValues;
        namesAndValues << fname << userFunctions.at(i).expression() << userFunctions.at(i).description();

        if (term.isEmpty()
            || namesAndValues.at(0).contains(term, Qt::CaseInsensitive)
            || namesAndValues.at(1).contains(term, Qt::CaseInsensitive)
            || namesAndValues.at(2).contains(term, Qt::CaseInsensitive))
        {
            const UserFunction& userFunction = userFunctions.at(i);
            const QString rawExpression = userFunction.expression();
            const QString sourceExpression = userFunction.interpretedExpression().isEmpty()
                ? rawExpression
                : userFunction.interpretedExpression();
            const QString formattedExpression = DisplayFormatUtils::applyDigitGroupingForDisplay(
                UnicodeChars::normalizePiForDisplay(
                    Evaluator::formatInterpretedExpressionForDisplay(sourceExpression, m_evaluator)));
            namesAndValues[1] = formattedExpression;
            QTreeWidgetItem* item = new QTreeWidgetItem(m_userFunctions, namesAndValues);
            item->setData(1, Qt::UserRole, rawExpression);
            item->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
            item->setTextAlignment(1, Qt::AlignLeft | Qt::AlignVCenter);
            item->setTextAlignment(2, Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    m_userFunctions->resizeColumnToContents(0);
    m_userFunctions->resizeColumnToContents(1);
    m_userFunctions->resizeColumnToContents(2);

    if (m_userFunctions->topLevelItemCount() > 0) {
        m_noMatchLabel->hide();
        m_userFunctions->sortItems(0, Qt::AscendingOrder);
    } else {
        DockListStyle::showCenteredNoMatchLabel(m_userFunctions, m_noMatchLabel);
    }

    setUpdatesEnabled(true);
    m_pendingRefresh = false;
}

void UserFunctionListWidget::retranslateText()
{
    QStringList titles;
    titles << tr("Name") << tr("Value") << tr("Description");
    m_userFunctions->setHeaderLabels(titles);

    m_searchLabel->setText(tr("Search"));
    m_noMatchLabel->setText(tr("No match found"));

    m_insertAction->setText(tr("Insert"));
    m_editAction->setText(tr("Edit"));
    m_deleteAction->setText(tr("Delete"));
    m_deleteAllAction->setText(tr("Delete All"));

    QTimer::singleShot(0, this, SLOT(updateList()));
}

QTreeWidgetItem* UserFunctionListWidget::currentItem() const
{
    return m_userFunctions->currentItem();
}

QString UserFunctionListWidget::getUserFunctionName(const QTreeWidgetItem *item)
{
    return item->text(0).section("(", 0, 0);
}

QString UserFunctionListWidget::searchText() const { return m_searchFilter->text(); }
void UserFunctionListWidget::setEvaluator(Evaluator* evaluator) { m_evaluator = evaluator ? evaluator : Evaluator::instance(); }
void UserFunctionListWidget::setSearchText(const QString& text) { m_searchFilter->setText(text); }

void UserFunctionListWidget::activateItem()
{
    if (!currentItem() || m_userFunctions->selectedItems().isEmpty())
        return;
    emit userFunctionSelected(currentItem()->text(0));
}

void UserFunctionListWidget::editItem()
{
    if (!currentItem() || m_userFunctions->selectedItems().isEmpty())
        return;
    const QString rawExpression = currentItem()->data(1, Qt::UserRole).toString();
    QString editedExpression = currentItem()->text(0) + " = " + rawExpression;
    const QString description = currentItem()->text(2).trimmed();
    if (!description.isEmpty())
        editedExpression += " ? " + description;
    emit userFunctionEdited(editedExpression);
}

void UserFunctionListWidget::deleteItem()
{
    if (!currentItem() || m_userFunctions->selectedItems().isEmpty())
        return;
    m_evaluator->unsetUserFunction(getUserFunctionName(currentItem()));
    updateList();
}

void UserFunctionListWidget::deleteAllItems()
{
    m_evaluator->unsetAllUserFunctions();
    updateList();
}

void UserFunctionListWidget::triggerFilter()
{
    m_filterTimer->stop();
    m_filterTimer->start();
}

void UserFunctionListWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        setLayoutDirection(Qt::LeftToRight);
        retranslateText();
        return;
    }
    QWidget::changeEvent(event);
}

void UserFunctionListWidget::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete)
        deleteItem();
    else if (event->key() == Qt::Key_E)
        editItem();
    else {
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void UserFunctionListWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_pendingRefresh)
        updateList();
}
