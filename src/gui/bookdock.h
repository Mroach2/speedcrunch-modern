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
signals:
    void paletteStyleChanged();
public slots:
    void setSource(const QUrl&) { }
protected:
    void changeEvent(QEvent*) override;
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
    void changeEvent(QEvent*) override;

private slots:
    void handleAnchorClick(const QUrl&);
    void refreshCurrentPage();

private:
    QString applyPaletteStyle(const QString& content) const;
    void updatePaletteStyle();

    Q_DISABLE_COPY(BookDock)
    Book* m_book;
    TextBrowser* m_browser;
    QString m_currentPage;
    bool m_refreshingPaletteStyle = false;
};

#endif // GUI_BOOKDOCK_H
