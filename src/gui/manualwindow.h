// SPDX-FileCopyrightText: 2014, 2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_MANUALWINDOW_H
#define GUI_MANUALWINDOW_H

#include <QTextBrowser>

class QCloseEvent;
class QEvent;
class QUrl;
class ManualServer;

class ManualWindow : public QTextBrowser {
    Q_OBJECT

public:
    ManualWindow(QWidget* parent = 0);

signals:
    void windowClosed();

public slots:
    void openPage(const QUrl&);
    void retranslateText();

protected:
    virtual void changeEvent(QEvent*);
    virtual void keyPressEvent(QKeyEvent * ev);
    virtual void mouseReleaseEvent(QMouseEvent* ev);
	virtual void closeEvent(QCloseEvent*);
    virtual void paintEvent(QPaintEvent* e);
private slots:
    void handleAnchorClick(const QUrl&url);
    void handleSourceChanged(const QUrl& url);

private:
    Q_DISABLE_COPY(ManualWindow)

    QVariant loadResource(int type, const QUrl &name);
    ManualServer *m_server;
    bool m_scrollUpdated;
};

#endif // GUI_MANUALWINDOW_H
