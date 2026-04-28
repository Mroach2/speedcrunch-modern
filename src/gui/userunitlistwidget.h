// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef GUI_USERUNITLISTWIDGET_H
#define GUI_USERUNITLISTWIDGET_H

#include <QWidget>

class QEvent;
class QKeyEvent;
class QLabel;
class QLineEdit;
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
    void changeEvent(QEvent*);
    void keyPressEvent(QKeyEvent*);

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
};

#endif
