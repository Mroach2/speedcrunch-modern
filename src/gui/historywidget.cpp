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
    m_list->setUniformItemSizes(true);
    DockListStyle::apply(m_list);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
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

    if (historySize == 0) {
        m_list->clear();
        m_list->clearSelection();
        return;
    }

    if (historySize == m_list->count() + 1) {
        appendHistoryItem(historySize - 1);
        m_list->scrollToBottom();
        return;
    }

    if (historySize == m_list->count()) {
        const int count = historySize;
        if (count == 1) {
            const QString expression = m_session->historyEntryAtRef(0).expr();
            QListWidgetItem* item = m_list->item(0);
            if (item != nullptr && item->data(Qt::UserRole).toString() != expression) {
                item->setText(groupedExpressionForHistory(expression));
                item->setData(Qt::UserRole, expression);
            }
            m_list->scrollToBottom();
            return;
        }

        QListWidgetItem* secondItem = m_list->item(1);
        if (secondItem != nullptr
            && secondItem->data(Qt::UserRole).toString() == m_session->historyEntryAtRef(0).expr()) {
            delete m_list->takeItem(0);
            appendHistoryItem(historySize - 1);
            m_list->scrollToBottom();
            return;
        }
    }

    rebuildHistory();
}

void HistoryWidget::appendHistoryItem(int index)
{
    if (m_session == nullptr || index < 0 || index >= m_session->historySize())
        return;

    const QString expression = m_session->historyEntryAtRef(index).expr();
    QListWidgetItem* item = new QListWidgetItem(groupedExpressionForHistory(expression));
    item->setData(Qt::UserRole, expression);
    m_list->addItem(item);
}

void HistoryWidget::rebuildHistory()
{
    const int historySize = m_session != nullptr ? m_session->historySize() : 0;

    m_list->setUpdatesEnabled(false);
    m_list->clear();
    m_list->clearSelection();

    for (int i = 0; i < historySize; ++i)
        appendHistoryItem(i);

    m_list->setUpdatesEnabled(true);
    m_list->scrollToBottom();
}

void HistoryWidget::setSession(const Session* session)
{
    if (m_session == session)
        return;

    m_session = session;
    m_list->clear();
    m_list->clearSelection();
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
