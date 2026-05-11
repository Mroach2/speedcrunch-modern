// SPDX-FileCopyrightText: 2009, 2011, 2013-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_VARIABLELISTWIDGET_H
#define GUI_VARIABLELISTWIDGET_H

#include <QList>
#include <QWidget>

class QEvent;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QShowEvent;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class Variable;

class VariableListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VariableListWidget(QWidget* parent = 0);
    ~VariableListWidget();

    QTreeWidgetItem* currentItem() const;
    QString searchText() const;
    void setSearchText(const QString& text);

signals:
    // Emitted immediately before updateList() reads Evaluator::instance().
    void aboutToUpdateList();
    void variableSelected(const QString&);
    void variableEdited(const QString&);

public slots:
    void updateList();
    void retranslateText();

protected slots:
    void activateItem();
    void editItem();
    void deleteItem();
    void deleteAllItems();
    void triggerFilter();

protected:
    void changeEvent(QEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    Q_DISABLE_COPY(VariableListWidget)

    QTimer* m_filterTimer;
    QTreeWidget* m_variables;
    QAction* m_insertAction;
    QAction* m_editAction;
    QAction* m_deleteAction;
    QAction* m_deleteAllAction;
    QLabel* m_noMatchLabel;
    QLineEdit* m_searchFilter;
    QLabel* m_searchLabel;
    bool m_pendingRefresh;
};

#endif
