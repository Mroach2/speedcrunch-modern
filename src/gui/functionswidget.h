// SPDX-FileCopyrightText: 2009-2011, 2013-2014, 2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_FUNCTIONSWIDGET_H
#define GUI_FUNCTIONSWIDGET_H

#include <QList>
#include <QWidget>

class QEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class FunctionsWidget : public QWidget {
    Q_OBJECT

public:
    explicit FunctionsWidget(QWidget* parent = 0);
    ~FunctionsWidget();

    const QTreeWidgetItem* currentItem() const;
    QList<QTreeWidgetItem*> selectedItems() const;
    QString searchText() const;
    void setSearchText(const QString& text);
    QString selectedDomain() const;
    void setSelectedDomain(const QString& domain);

signals:
    void functionSelected(const QString&);

protected slots:
    void handleItemActivated(QTreeWidgetItem*, int);
    virtual void changeEvent(QEvent*);
    void clearSelection(QTreeWidgetItem*);
    void updateList();
    void retranslateText();
    void triggerFilter();

private:
    Q_DISABLE_COPY(FunctionsWidget)

    QTimer* m_filterTimer;
    QComboBox* m_domain;
    QLabel* m_domainLabel;
    QTreeWidget* m_functions;
    bool m_insertAllItems;
    QLabel* m_noMatchLabel;
    QLineEdit* m_searchFilter;
    QLabel* m_searchLabel;
};

#endif // GUI_FUNCTIONSWIDGET_H
