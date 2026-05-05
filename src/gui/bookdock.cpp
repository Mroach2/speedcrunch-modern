// SPDX-FileCopyrightText: 2008-2015, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/bookdock.h"

#include "core/book.h"
#include "core/evaluator.h"
#include "core/settings.h"
#include "gui/editorutils.h"
#include "core/mathdsl.h"

#include <QEvent>
#include <QPalette>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {
QString toSuperscriptDigits(const QString& text)
{
    QString out;
    out.reserve(text.size());

    for (const QChar ch : text) {
        const QChar superscript = MathDsl::asciiDigitToSuperscript(ch);
        out += superscript.isNull() ? ch : superscript;
    }

    return out;
}

QString formatFormulaForEditorInsertion(const QString& text)
{
    QString formatted = text;

    // Convert exponent forms such as ^2 or ^(12) to superscripts.
    static const QRegularExpression s_powerPattern(
        QStringLiteral(R"(\^(?:\(([0-9]+)\)|([0-9]+)))"));
    int offset = 0;
    auto it = s_powerPattern.globalMatch(formatted);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString exponentDigits = match.captured(1).isEmpty()
            ? match.captured(2)
            : match.captured(1);
        const QString superscript = toSuperscriptDigits(exponentDigits);
        const int start = match.capturedStart(0) + offset;
        const int length = match.capturedLength(0);
        formatted.replace(start, length, superscript);
        offset += superscript.size() - length;
    }

    // Use regular spaces around the main binary operators.
    const QString ms(MathDsl::AddWrap);
    const QString replacement = ms + QStringLiteral("\\1") + ms;
    static const QRegularExpression s_binaryOps(
        QStringLiteral(R"(\s*([=+\-−·×/*])\s*)"));
    formatted.replace(s_binaryOps, replacement);

    return formatted;
}

} // namespace

void TextBrowser::changeEvent(QEvent* event)
{
    QTextBrowser::changeEvent(event);
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange
        || event->type() == QEvent::StyleChange) {
        emit paletteStyleChanged();
    }
}

BookDock::BookDock(QWidget* parent)
    : QDockWidget(parent)
    , m_book(new Book(this))
{
    QWidget* widget = new QWidget(this);
    QVBoxLayout* bookLayout = new QVBoxLayout;

    m_browser = new TextBrowser(this);
    m_browser->setLineWrapMode(QTextEdit::NoWrap);
    m_browser->setOpenLinks(false);
    m_browser->setOpenExternalLinks(false);
    m_browser->setAutoFillBackground(true);
    updatePaletteStyle();

    connect(m_browser, SIGNAL(anchorClicked(const QUrl&)), SLOT(handleAnchorClick(const QUrl&)));
    connect(m_browser, SIGNAL(paletteStyleChanged()), SLOT(refreshCurrentPage()));

    bookLayout->addWidget(m_browser);
    widget->setLayout(bookLayout);
    setWidget(widget);

    retranslateText();
    openPage(QUrl("index"));
}

void BookDock::handleAnchorClick(const QUrl& url)
{
    if (url.toString().startsWith("formula:")) {
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
        QString expression = url.toString(QUrl::DecodeReserved).mid(8);
#else
        QString expression = url.toString().mid(8);
#endif
        expression = EditorUtils::normalizeExpressionOperatorsForEditorInput(expression);

        Evaluator* evaluator = Evaluator::instance();
        evaluator->setExpression(expression);
        if (evaluator->isValid()) {
            const QString interpreted = evaluator->interpretedExpression();
            if (!interpreted.isEmpty())
                expression = Evaluator::formatInterpretedExpressionForDisplay(interpreted);
        }

        expression.replace(MathDsl::MulCrossOp, MathDsl::MulDotOp);
        expression = formatFormulaForEditorInsertion(expression);
        emit expressionSelected(expression);
    } else
        openPage(url);
}

void BookDock::openPage(const QUrl& url)
{
    const QString page = url.toString().isEmpty() ? QStringLiteral("index") : url.toString();
    QString content = m_book->getPageContent(page);
    if (!content.isNull())
        m_browser->setHtml(applyPaletteStyle(content));
    m_currentPage = page;
}

void BookDock::retranslateText()
{
    setWindowTitle(tr("Formula Book"));
    QString content = m_book->getCurrentPageContent();
    if (!content.isNull())
        m_browser->setHtml(applyPaletteStyle(content));
}

void BookDock::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateText();
    } else if (event->type() == QEvent::PaletteChange
               || event->type() == QEvent::ApplicationPaletteChange
               || event->type() == QEvent::StyleChange) {
        refreshCurrentPage();
    } else {
        QDockWidget::changeEvent(event);
    }
}

QString BookDock::currentPage() const
{
    return m_currentPage.isEmpty() ? QStringLiteral("index") : m_currentPage;
}

void BookDock::refreshCurrentPage()
{
    if (m_refreshingPaletteStyle)
        return;

    m_refreshingPaletteStyle = true;
    updatePaletteStyle();
    const QString content = m_book->getCurrentPageContent();
    if (!content.isNull())
        m_browser->setHtml(applyPaletteStyle(content));
    m_refreshingPaletteStyle = false;
}

QString BookDock::applyPaletteStyle(const QString& content) const
{
    const QPalette palette = m_browser->palette();
    const QString style = QStringLiteral(
        "<style>"
        "body { background-color: %1; color: %2; }"
        "a:link, a:visited { color: %3; }"
        ".page-link a:link, .page-link a:visited { color: %2; }"
        ".formula a:link, .formula a:visited { color: %3; }"
        "</style>")
        .arg(palette.color(QPalette::Base).name(),
             palette.color(QPalette::Text).name(),
             palette.color(QPalette::Link).name());

    QString styledContent = content;
    const int headEnd = styledContent.indexOf(QStringLiteral("</head>"), 0, Qt::CaseInsensitive);
    if (headEnd >= 0)
        styledContent.insert(headEnd, style);
    return styledContent;
}

void BookDock::updatePaletteStyle()
{
    QPalette palette = this->palette();
    palette.setColor(QPalette::Base, palette.color(QPalette::Window));
    palette.setColor(QPalette::Text, palette.color(QPalette::WindowText));
    m_browser->setPalette(palette);
    m_browser->viewport()->setPalette(palette);
}
