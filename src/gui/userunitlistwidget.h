// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_USERUNITLISTWIDGET_H
#define GUI_USERUNITLISTWIDGET_H

#include <QWidget>

class QEvent;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QShowEvent;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

class UserUnitListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit UserUnitListWidget(QWidget* parent = 0);
    ~UserUnitListWidget();

    QTreeWidgetItem* currentItem() const;
    QString getUserUnitName(const QTreeWidgetItem*);
    QString searchText() const;
    void setSearchText(const QString& text);

signals:
    // Emitted immediately before updateList() reads Evaluator::instance().
    void aboutToUpdateList();
    void userUnitSelected(const QString&);
    void userUnitEdited(const QString&);

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
    Q_DISABLE_COPY(UserUnitListWidget)

    QTimer* m_filterTimer;
    QTreeWidget* m_userUnits;
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
