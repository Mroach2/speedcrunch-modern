// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/userunitlistwidget.h"

#include "core/evaluator.h"
#include "core/mathdsl.h"
#include "core/numberformatter.h"
#include "gui/dockliststyle.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QShortcut>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QRegularExpression>

UserUnitListWidget::UserUnitListWidget(QWidget* parent)
    : QWidget(parent)
    , m_filterTimer(new QTimer(this))
    , m_userUnits(new QTreeWidget(this))
    , m_noMatchLabel(new QLabel(m_userUnits))
    , m_searchFilter(new QLineEdit(this))
    , m_searchLabel(new QLabel(this))
    , m_pendingRefresh(false)
{
    m_filterTimer->setInterval(500);
    m_filterTimer->setSingleShot(true);

    m_userUnits->setAutoScroll(true);
    m_userUnits->setColumnCount(3);
    m_userUnits->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_userUnits->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_userUnits->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_userUnits->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_userUnits->setRootIsDecorated(false);
    m_userUnits->setSelectionBehavior(QTreeWidget::SelectRows);
    DockListStyle::apply(m_userUnits);

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
    layout->addWidget(m_userUnits);
    setLayout(layout);

    QMenu* contextMenu = new QMenu(m_userUnits);
    m_insertAction = new QAction("", contextMenu);
    m_editAction = new QAction("", contextMenu);
    m_deleteAction = new QAction("", contextMenu);
    m_deleteAllAction = new QAction("", contextMenu);
    m_userUnits->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_userUnits->addAction(m_insertAction);
    m_userUnits->addAction(m_editAction);
    m_userUnits->addAction(m_deleteAction);
    m_userUnits->addAction(m_deleteAllAction);

    QWidget::setTabOrder(m_searchFilter, m_userUnits);
    setFocusProxy(m_searchFilter);

    retranslateText();

    connect(m_filterTimer, SIGNAL(timeout()), SLOT(updateList()));
    connect(m_searchFilter, SIGNAL(textChanged(const QString&)), SLOT(triggerFilter()));
    connect(m_userUnits, SIGNAL(itemActivated(QTreeWidgetItem*, int)), SLOT(activateItem()));
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

UserUnitListWidget::~UserUnitListWidget()
{
    m_filterTimer->stop();
}

void UserUnitListWidget::updateList()
{
    if (!isVisible()) {
        m_pendingRefresh = true;
        return;
    }

    setUpdatesEnabled(false);

    m_filterTimer->stop();
    m_userUnits->clear();
    const QString term = m_searchFilter->text();
    const QList<UserUnit> userUnits = Evaluator::instance()->getUserUnits();

    for (int i = 0; i < userUnits.count(); ++i) {
        const UserUnit& userUnit = userUnits.at(i);
        QString displayExpression = userUnit.interpretedExpression().isEmpty()
            ? userUnit.expression()
            : userUnit.interpretedExpression();
        const int commentPos = displayExpression.lastIndexOf(MathDsl::CommentSep);
        if (commentPos >= 0)
            displayExpression = displayExpression.left(commentPos).trimmed();
        // Keep only the right-hand-side expression in the Value column:
        // turn "[unit] = expr" (or legacy "1 [unit] = expr") into "expr".
        displayExpression.remove(
            QRegularExpression(QStringLiteral(R"(^\s*(?:1\s+)?\[[^\]]+\]\s*=\s*)")));
        QStringList row;
        row << userUnit.name()
            << (displayExpression.isEmpty() ? NumberFormatter::format(userUnit.value()) : displayExpression)
            << userUnit.description();

        if (term.isEmpty()
            || row.at(0).contains(term, Qt::CaseInsensitive)
            || row.at(1).contains(term, Qt::CaseInsensitive)
            || row.at(2).contains(term, Qt::CaseInsensitive))
        {
            QTreeWidgetItem* item = new QTreeWidgetItem(m_userUnits, row);
            item->setData(1, Qt::UserRole, userUnit.expression());
            item->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter);
            item->setTextAlignment(1, Qt::AlignLeft | Qt::AlignVCenter);
            item->setTextAlignment(2, Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    m_userUnits->resizeColumnToContents(0);
    m_userUnits->resizeColumnToContents(1);
    m_userUnits->resizeColumnToContents(2);

    if (m_userUnits->topLevelItemCount() > 0) {
        m_noMatchLabel->hide();
        m_userUnits->sortItems(0, Qt::AscendingOrder);
    } else {
        DockListStyle::showCenteredNoMatchLabel(m_userUnits, m_noMatchLabel);
    }

    setUpdatesEnabled(true);
    m_pendingRefresh = false;
}

void UserUnitListWidget::retranslateText()
{
    QStringList titles;
    titles << tr("Name") << tr("Value") << tr("Description");
    m_userUnits->setHeaderLabels(titles);

    m_searchLabel->setText(tr("Search"));
    m_noMatchLabel->setText(tr("No match found"));

    m_insertAction->setText(tr("Insert"));
    m_editAction->setText(tr("Edit"));
    m_deleteAction->setText(tr("Delete"));
    m_deleteAllAction->setText(tr("Delete All"));

    QTimer::singleShot(0, this, SLOT(updateList()));
}

QTreeWidgetItem* UserUnitListWidget::currentItem() const
{
    return m_userUnits->currentItem();
}

QString UserUnitListWidget::getUserUnitName(const QTreeWidgetItem* item)
{
    return item->text(0);
}

QString UserUnitListWidget::searchText() const { return m_searchFilter->text(); }
void UserUnitListWidget::setSearchText(const QString& text) { m_searchFilter->setText(text); }

void UserUnitListWidget::activateItem()
{
    if (!currentItem() || m_userUnits->selectedItems().isEmpty())
        return;
    emit userUnitSelected(QStringLiteral("[%1]").arg(currentItem()->text(0)));
}

void UserUnitListWidget::editItem()
{
    if (!currentItem() || m_userUnits->selectedItems().isEmpty())
        return;
    const QString expression = currentItem()->data(1, Qt::UserRole).toString();
    QString editedExpression = QStringLiteral("[%1] = %2").arg(currentItem()->text(0), expression);
    const QString description = currentItem()->text(2).trimmed();
    if (!description.isEmpty())
        editedExpression += QStringLiteral(" ? ") + description;
    emit userUnitEdited(editedExpression);
}

void UserUnitListWidget::deleteItem()
{
    if (!currentItem() || m_userUnits->selectedItems().isEmpty())
        return;
    Evaluator::instance()->unsetUserUnit(getUserUnitName(currentItem()));
    updateList();
}

void UserUnitListWidget::deleteAllItems()
{
    Evaluator::instance()->unsetAllUserUnits();
    updateList();
}

void UserUnitListWidget::triggerFilter()
{
    m_filterTimer->stop();
    m_filterTimer->start();
}

void UserUnitListWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        setLayoutDirection(Qt::LeftToRight);
        retranslateText();
        return;
    }
    QWidget::changeEvent(event);
}

void UserUnitListWidget::keyPressEvent(QKeyEvent* event)
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

void UserUnitListWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (m_pendingRefresh)
        updateList();
}
