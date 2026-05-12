// SPDX-FileCopyrightText: 2009-2011, 2014-2016, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/historywidget.h"
#include "core/sessionhistory.h"
#include "core/numberformatter.h"
#include "core/session.h"
#include "gui/dockliststyle.h"

#include <QAction>
#include <QEvent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QShortcut>
#include <QVBoxLayout>

namespace {
QString groupedExpressionForHistory(const QString& input)
{
    return NumberFormatter::formatNumericLiteralForDisplay(input);
}
}

HistoryWidget::HistoryWidget(QWidget *parent)
    : QWidget(parent)
    , m_list(new QListWidget(this))
    , m_session(nullptr)
{
    m_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    DockListStyle::apply(m_list);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(3, 3, 3, 3);
    layout->addWidget(m_list);
    setLayout(layout);

    connect(m_list, SIGNAL(itemActivated(QListWidgetItem *)), SLOT(handleItem(QListWidgetItem *)));
    connect(m_list, SIGNAL(customContextMenuRequested(const QPoint &)),
            SLOT(handleContextMenuRequested(const QPoint &)));
    QShortcut* returnShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    returnShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(returnShortcut, &QShortcut::activated, this, [this]() {
        if (QListWidgetItem* current = m_list->currentItem())
            handleItem(current);
    });
    QShortcut* enterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), this);
    enterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(enterShortcut, &QShortcut::activated, this, [this]() {
        if (QListWidgetItem* current = m_list->currentItem())
            handleItem(current);
    });

    updateHistory();
}

void HistoryWidget::updateHistory()
{
    const int historySize = m_session != nullptr ? m_session->historySize() : 0;

    m_list->clear();
    m_list->clearSelection();

    for (int i = 0; i < historySize; ++i) {
        const QString expression = m_session->historyEntryAtRef(i).expr();
        QListWidgetItem* item = new QListWidgetItem(groupedExpressionForHistory(expression));
        item->setData(Qt::UserRole, expression);
        m_list->addItem(item);
    }
    m_list->scrollToBottom();
}

void HistoryWidget::setSession(const Session* session)
{
    if (m_session == session)
        return;

    m_session = session;
    updateHistory();
}

void HistoryWidget::handleItem(QListWidgetItem *item)
{
    m_list->clearSelection();
    emit expressionSelected(item->data(Qt::UserRole).toString());
}

void HistoryWidget::handleContextMenuRequested(const QPoint &position)
{
    const int historyIndex = m_list->indexAt(position).row();
    if (historyIndex < 0)
        return;

    QMenu menu(m_list);
    QAction *removeAboveAction = menu.addAction(tr("Remove All Calculations Above"));
    connect(removeAboveAction, &QAction::triggered, this, [this, historyIndex]() {
        emit removeHistoryEntriesAboveRequested(historyIndex);
    });
    QAction *removeAction = menu.addAction(tr("Remove This Calculation"));
    connect(removeAction, &QAction::triggered, this, [this, historyIndex]() {
        emit removeHistoryEntryRequested(historyIndex);
    });
    QAction *removeBelowAction = menu.addAction(tr("Remove All Calculations Below"));
    connect(removeBelowAction, &QAction::triggered, this, [this, historyIndex]() {
        emit removeHistoryEntriesBelowRequested(historyIndex);
    });
    menu.exec(m_list->viewport()->mapToGlobal(position));
}

void HistoryWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
        setLayoutDirection(Qt::LeftToRight);
    else
        QWidget::changeEvent(e);
}
