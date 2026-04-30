// SPDX-FileCopyrightText: 2009-2011, 2013-2014, 2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_CONSTANTSWIDGET_H
#define GUI_CONSTANTSWIDGET_H

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class ConstantsWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConstantsWidget(QWidget* parent = nullptr);
    ~ConstantsWidget();
    QString selectedDomain() const;
    QString selectedSubdomain() const;
    QString searchText() const;
    void restoreState(const QString& domain, const QString& subdomain, const QString& searchText);

signals:
    void constantSelected(const QString&);

public slots:
    void handleRadixCharacterChange();

protected slots:
    void filter();
    void handleDomainChanged();
    void handleItem(QTreeWidgetItem*);
    void refreshSubdomains();
    void retranslateText();
    void triggerFilter();
    void updateList();

protected:
    virtual void changeEvent(QEvent*);

private:
    Q_DISABLE_COPY(ConstantsWidget)

    QComboBox* m_domain;
    QLabel* m_domainLabel;
    QComboBox* m_subdomain;
    QLabel* m_subdomainLabel;
    QLineEdit* m_filter;
    QTimer* m_filterTimer;
    QLabel* m_label;
    QTreeWidget* m_list;
    QLabel* m_noMatchLabel;
};

#endif
