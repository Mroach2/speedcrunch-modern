// SPDX-FileCopyrightText: 2009-2011, 2013-2015, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_HISTORYWIDGET_H
#define GUI_HISTORYWIDGET_H

#include <QWidget>

class QListWidget;
class QListWidgetItem;
class HistoryEntry;
class QPoint;
class Session;

class HistoryWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryWidget(QWidget *parent = 0);
    void setSession(const Session* session);

public slots:
    void updateHistory();

signals:
    void expressionSelected(const QString &);
    void removeHistoryEntryRequested(int index);
    void removeHistoryEntriesAboveRequested(int index);
    void removeHistoryEntriesBelowRequested(int index);

protected slots:
    void handleItem(QListWidgetItem *);
    void handleContextMenuRequested(const QPoint &position);

protected:
    void changeEvent(QEvent *);

private:
    Q_DISABLE_COPY(HistoryWidget)

    void appendHistoryItem(int index);
    void rebuildHistory();

    QListWidget *m_list;
    const Session* m_session;
};

#endif
