// SPDX-FileCopyrightText: 2014, 2016, 2022, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/manualwindow.h"
#include "core/settings.h"
#include "core/manualserver.h"

#include <QtCore/QEvent>
#include <QKeyEvent>
#include <QtHelp/QHelpEngineCore>
#include <QDesktopServices>
#include <QIcon>


ManualWindow::ManualWindow(QWidget* parent)
    : QTextBrowser(parent), m_scrollUpdated(false)
{
    setWindowFlags(Qt::Window);
    setWindowIcon(QPixmap(":/speedcrunch.png"));
    setStyleSheet(QStringLiteral("QTextBrowser { background-color: #FFFFFF; }"));
    QFont f("Helvetica");
    f.setStyleHint(QFont::SansSerif);
    f.setPointSize(10);
    setFont(f);

    //Disable automatic opening of links. We handle them ourselves.
    this->setOpenLinks(false);
    connect(this, SIGNAL(anchorClicked(const QUrl&)), SLOT(handleAnchorClick(const QUrl&)));
    connect(this, SIGNAL(sourceChanged(const QUrl&)), SLOT(handleSourceChanged(const QUrl&)));

    m_server = ManualServer::instance();
    auto homePage = m_server->homePage();
    if (homePage.isValid()) {
        openPage(homePage);
    }
    retranslateText();
}

void ManualWindow::openPage(const QUrl& url)
{
    setSource(url);
}

void ManualWindow::retranslateText()
{
    QString docTitle = documentTitle();
    if (docTitle.isEmpty())
        setWindowTitle(tr("SpeedCrunch Manual"));
    else
        setWindowTitle(tr("%1 - SpeedCrunch Manual").arg(docTitle));
}


void ManualWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        m_server->ensureCorrectLanguage();
        this->reload();
        retranslateText();
    }
    else
        QTextBrowser::changeEvent(event);
}

void ManualWindow::keyPressEvent(QKeyEvent *ev)
{
    if(ev->key() == Qt::Key_Escape) {
        ev->accept();
        this->close();
    }
    else if(ev->key() == Qt::Key_Back
            || ev->key() == Qt::Key_Backspace) {
        ev->accept();
        this->backward();
    }
    else if(ev->key() == Qt::Key_Forward) {
        ev->accept();
        this->forward();
    }
    else if(ev->key() == Qt::Key_Home) {
        ev->accept();
        this->home();
    }
    else
        QTextBrowser::keyPressEvent(ev);
}

void ManualWindow::mouseReleaseEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::BackButton) {
        ev->accept();
        backward();
    }
    else if (ev->button() == Qt::ForwardButton) {
        ev->accept();
        forward();
    }
    else
        QTextBrowser::mouseReleaseEvent(ev);
}

void ManualWindow::closeEvent(QCloseEvent* event)
{
    emit windowClosed();
    QTextBrowser::closeEvent(event);
}

void ManualWindow::paintEvent(QPaintEvent* e)
{
    /* So the issue and reasoning here is:
     * - When opening the manual window via the context help feature *and it was maximized before*,
     *   it doesn't correctly scroll to the right anchor on the page.
     * - QTextBrowser keeps its scrolling offset in pixels; when the widget is resized, the text on
     *   the page is laid out again and the pixel offset of anchors may change. This means when
     *   resizing the widget, the formerly-scrolled-to anchor may not be at the top anymore.
     * - For maximized windows, the page is loaded first, -- including scrolling the widget to the
     *   anchor -- then there's a resizeEvent that sets the actual, maximized window size. This throws
     *   off the page position.
     * - We work around this here by updating the scroll position on the very first paintEvent, which
     *   happens after the window's maximized state has finally properly registered; late enough to fix
     *   the scrolling.
     *
     * Yes, it's rather hackish.
     */
    if (!m_scrollUpdated && source().isValid()) {
        scrollToAnchor(source().fragment());
        m_scrollUpdated = true;
    }
    QTextBrowser::paintEvent(e);
}

void ManualWindow::handleAnchorClick(const QUrl &url)
{
    if (url.scheme() == "qthelp")
        openPage(url);
    else
        QDesktopServices::openUrl(url);
}

void ManualWindow::handleSourceChanged(const QUrl&)
{
    // This updates the window title with the new document title.
    retranslateText();
}

QVariant ManualWindow::loadResource(int type, const QUrl &name)
{
    QByteArray ba;
    if (type < 4 && name.scheme()=="qthelp") {
        QUrl url(name);
        if (name.isRelative())
            url = source().resolved(name);
        ba = m_server->fileData(url);
    }
    return ba;
}
