// SPDX-FileCopyrightText: 2008-2010, 2013-2014, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_BOOKDOCK_H
#define GUI_BOOKDOCK_H

#include "core/book.h"

#include <QDockWidget>
#include <QTextBrowser>

class QUrl;

class TextBrowser : public QTextBrowser {
    Q_OBJECT
public:
    TextBrowser(QWidget* parent) : QTextBrowser(parent) { }
public slots:
    virtual void setSource(const QUrl&) { }
};

class BookDock : public QDockWidget {
    Q_OBJECT

public:
    BookDock(QWidget* parent = 0);

signals:
    void expressionSelected(const QString&);

public slots:
    void openPage(const QUrl&);
    void retranslateText();
    QString currentPage() const;

protected:
    virtual void changeEvent(QEvent*);

private slots:
    void handleAnchorClick(const QUrl&);

private:
    Q_DISABLE_COPY(BookDock)
    Book* m_book;
    TextBrowser* m_browser;
    QString m_currentPage;
};

#endif // GUI_BOOKDOCK_H
