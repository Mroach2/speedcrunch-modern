// This file is part of the SpeedCrunch project
// Copyright (C) 2009-2016 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; see the file COPYING.  If not, write to
// the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301, USA.

#include "gui/resultdisplay.h"

#include "gui/displayformatutils.h"
#include "gui/resultlineformatutils.h"
#include "core/functions.h"
#include "core/numberformatter.h"
#include "core/regexpatterns.h"
#include "core/settings.h"
#include "core/units.h"
#include "core/unicodechars.h"
#include "core/mathdsl.h"
#include "gui/simplifiedexpressionutils.h"
#include "gui/syntaxhighlighter.h"
#include "math/cmath.h"
#include "math/floatnum/floatconfig.h"
#include "core/evaluator.h"
#include "core/session.h"
#include "core/sessionhistory.h"

#include <QLatin1String>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStyle>
#include <QToolButton>
#include <QToolTip>

namespace {
QPointF badgeCenter(const QRect& rect)
{
    return QPointF(rect.x() + rect.width() * 0.5, rect.y() + rect.height() * 0.5);
}

QColor hoverColorForBackground(const QColor& background)
{
    const bool isLightBackground = background.lightnessF() >= 0.5;
    const QColor target = isLightBackground ? QColor(Qt::black) : QColor(Qt::white);
    const qreal blendFactor = 0.12; // subtle, but always visible even on pure black/white backgrounds

    auto mixChannel = [blendFactor](int from, int to) {
        return qBound(0, static_cast<int>(from + (to - from) * blendFactor), 255);
    };

    return QColor(mixChannel(background.red(), target.red()),
                  mixChannel(background.green(), target.green()),
                  mixChannel(background.blue(), target.blue()));
}

int maxRenderedHistoryEntries()
{
    static const int kDefaultMaxRenderedHistoryEntries = 800;
    const int configuredLimit = Settings::instance()->maxHistoryEntries;
    if (configuredLimit > 0)
        return qMin(configuredLimit, kDefaultMaxRenderedHistoryEntries);
    return kDefaultMaxRenderedHistoryEntries;
}

int firstDisplayedHistoryIndexForCount(int historyCount)
{
    return qMax(0, historyCount - maxRenderedHistoryEntries());
}

void cloneMenuActions(const QMenu* sourceMenu, QMenu* targetMenu)
{
    const QList<QAction*> sourceActions = sourceMenu->actions();
    for (QAction* sourceAction : sourceActions) {
        if (sourceAction->isSeparator()) {
            targetMenu->addSeparator();
            continue;
        }

        QMenu* sourceSubmenu = sourceAction->menu();
        if (sourceSubmenu != 0) {
            QMenu* clonedSubmenu = targetMenu->addMenu(sourceSubmenu->title());
            clonedSubmenu->setEnabled(sourceAction->isEnabled());
            cloneMenuActions(sourceSubmenu, clonedSubmenu);
            continue;
        }

        targetMenu->addAction(sourceAction);
    }
}

QString formatResultForClipboard(const Quantity& value)
{
    QString textToCopy = NumberFormatter::format(value);
    textToCopy.replace(
        RegExpPatterns::missingQuantSpBeforeUnit(),
        QStringLiteral("\\1") + QString(MathDsl::QuantSp) + QString(MathDsl::UnitStart));
    textToCopy.replace(UnicodeChars::MinusSign, MathDsl::SubOpAl1);
    return textToCopy;
}

QStringList formatResultLines(const HistoryEntry& entry)
{
    Settings* settings = Settings::instance();
    const bool oldComplexNumbers = settings->complexNumbers;
    const char oldImaginaryUnit = settings->imaginaryUnit;
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    const int oldResultPrecision = settings->resultPrecision;
    const char oldResultFormatComplex = settings->resultFormatComplex;
    const char oldUnitExp = settings->unitNegativeExponentStyle;
    const char oldRound = settings->resultRoundingMode;
    const bool oldMultiple = settings->multipleResultLinesEnabled;
    const bool oldSecondaryEnabled = settings->secondaryResultEnabled;
    const bool oldTertiaryEnabled = settings->tertiaryResultEnabled;
    const bool oldQuaternaryEnabled = settings->quaternaryResultEnabled;
    const bool oldQuinaryEnabled = settings->quinaryResultEnabled;
    const char oldSecondaryFormat = settings->alternativeResultFormat;
    const char oldTertiaryFormat = settings->tertiaryResultFormat;
    const char oldQuaternaryFormat = settings->quaternaryResultFormat;
    const char oldQuinaryFormat = settings->quinaryResultFormat;
    const int oldSecondaryPrecision = settings->secondaryResultPrecision;
    const int oldTertiaryPrecision = settings->tertiaryResultPrecision;
    const int oldQuaternaryPrecision = settings->quaternaryResultPrecision;
    const int oldQuinaryPrecision = settings->quinaryResultPrecision;
    const char oldSecondaryComplex = settings->secondaryResultFormatComplex;
    const char oldTertiaryComplex = settings->tertiaryResultFormatComplex;
    const char oldQuaternaryComplex = settings->quaternaryResultFormatComplex;
    const char oldQuinaryComplex = settings->quinaryResultFormatComplex;

    const EvaluationContext& ctx = entry.contextRef();
    settings->complexNumbers = ctx.complexOn;
    settings->imaginaryUnit = (ctx.unit == 'j') ? 'j' : 'i';
    settings->angleUnit = ctx.angle;
    settings->resultFormat = ctx.main.fmt;
    settings->resultPrecision = ctx.main.prec;
    settings->resultFormatComplex = ctx.main.cplx;
    settings->unitNegativeExponentStyle = isValidUnitNegativeExponentStyle(ctx.unitExp)
        ? ctx.unitExp
        : Settings::UnitNegativeExponentSuperscript;
    settings->resultRoundingMode = isValidResultRoundingMode(ctx.round)
        ? ctx.round
        : Settings::ResultRoundingHalfAwayFromZero;
    setRuntimeUnitNegativeExponentStyle(settings->unitNegativeExponentStyle);
    setRuntimeResultRoundingMode(settings->resultRoundingMode);

    settings->multipleResultLinesEnabled = !ctx.extras.isEmpty();
    settings->secondaryResultEnabled = false;
    settings->tertiaryResultEnabled = false;
    settings->quaternaryResultEnabled = false;
    settings->quinaryResultEnabled = false;
    if (ctx.extras.size() > 0) {
        settings->secondaryResultEnabled = true;
        settings->alternativeResultFormat = ctx.extras.at(0).fmt;
        settings->secondaryResultPrecision = ctx.extras.at(0).prec;
        settings->secondaryResultFormatComplex = ctx.extras.at(0).cplx;
    }
    if (ctx.extras.size() > 1) {
        settings->tertiaryResultEnabled = true;
        settings->tertiaryResultFormat = ctx.extras.at(1).fmt;
        settings->tertiaryResultPrecision = ctx.extras.at(1).prec;
        settings->tertiaryResultFormatComplex = ctx.extras.at(1).cplx;
    }
    if (ctx.extras.size() > 2) {
        settings->quaternaryResultEnabled = true;
        settings->quaternaryResultFormat = ctx.extras.at(2).fmt;
        settings->quaternaryResultPrecision = ctx.extras.at(2).prec;
        settings->quaternaryResultFormatComplex = ctx.extras.at(2).cplx;
    }
    if (ctx.extras.size() > 3) {
        settings->quinaryResultEnabled = true;
        settings->quinaryResultFormat = ctx.extras.at(3).fmt;
        settings->quinaryResultPrecision = ctx.extras.at(3).prec;
        settings->quinaryResultFormatComplex = ctx.extras.at(3).cplx;
    }

    const QStringList lines = ResultLineFormatUtils::formatResultLinesForDisplay(
        entry.expr(),
        entry.interpretedExpr(),
        entry.result(),
        false,
        true);

    settings->complexNumbers = oldComplexNumbers;
    settings->imaginaryUnit = oldImaginaryUnit;
    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    settings->resultPrecision = oldResultPrecision;
    settings->resultFormatComplex = oldResultFormatComplex;
    settings->unitNegativeExponentStyle = oldUnitExp;
    settings->resultRoundingMode = oldRound;
    settings->multipleResultLinesEnabled = oldMultiple;
    settings->secondaryResultEnabled = oldSecondaryEnabled;
    settings->tertiaryResultEnabled = oldTertiaryEnabled;
    settings->quaternaryResultEnabled = oldQuaternaryEnabled;
    settings->quinaryResultEnabled = oldQuinaryEnabled;
    settings->alternativeResultFormat = oldSecondaryFormat;
    settings->tertiaryResultFormat = oldTertiaryFormat;
    settings->quaternaryResultFormat = oldQuaternaryFormat;
    settings->quinaryResultFormat = oldQuinaryFormat;
    settings->secondaryResultPrecision = oldSecondaryPrecision;
    settings->tertiaryResultPrecision = oldTertiaryPrecision;
    settings->quaternaryResultPrecision = oldQuaternaryPrecision;
    settings->quinaryResultPrecision = oldQuinaryPrecision;
    settings->secondaryResultFormatComplex = oldSecondaryComplex;
    settings->tertiaryResultFormatComplex = oldTertiaryComplex;
    settings->quaternaryResultFormatComplex = oldQuaternaryComplex;
    settings->quinaryResultFormatComplex = oldQuinaryComplex;
    setRuntimeUnitNegativeExponentStyle(settings->unitNegativeExponentStyle);
    setRuntimeResultRoundingMode(settings->resultRoundingMode);
    return lines;
}

QString formattedExpressionForDisplay(const HistoryEntry& entry)
{
    return ResultLineFormatUtils::formattedExpressionLineForDisplay(
        entry.expr(),
        entry.interpretedExpr());
}

QStringList renderedHistoryLinesForDisplay(const HistoryEntry& entry)
{
    if (entry.hasRenderedLines())
        return entry.renderedLines();

    QStringList lines;
    lines.append(formattedExpressionForDisplay(entry));
    if (!entry.result().isNan())
        lines.append(formatResultLines(entry));
    return lines;
}

QString formattedExpressionForDisplay(const QString& expression,
                                     const QString& interpretedExpression)
{
    return ResultLineFormatUtils::formattedExpressionLineForDisplay(
        expression,
        interpretedExpression);
}

}

ResultDisplay::ResultDisplay(QWidget* parent)
    : QPlainTextEdit(parent)
    , m_highlighter(new SyntaxHighlighter(this))
    , m_scrolledLines(0)
    , m_scrollDirection(0)
    , m_isScrollingPageOnly(false)
    , m_hoverHighlightEnabled(true)
    , m_scrollBarHovered(false)
    , m_historyBlockIndexCacheDirty(true)
    , m_hoveredHistoryIndex(-1)
    , m_editingHistoryIndex(-1)
    , m_count(0)
    , m_firstDisplayedHistoryIndex(0)
    , m_scrollToBottomButton(new QToolButton(this))
{
    setViewportMargins(0, 0, 0, 0);
    setBackgroundRole(QPalette::Base);
    setLayoutDirection(Qt::LeftToRight);
    setMinimumWidth(150);
    setReadOnly(true);
    setFocusPolicy(Qt::NoFocus);
    setWordWrapMode(QTextOption::WrapAnywhere);
    setMouseTracking(true);

    QScrollBar* bar = verticalScrollBar();
    bar->setAttribute(Qt::WA_Hover, true);
    bar->setMouseTracking(true);
    bar->installEventFilter(this);
    connect(bar, &QScrollBar::valueChanged, this, [this]() {
        updateScrollToBottomButtonVisibility();
    });
    connect(bar, &QScrollBar::rangeChanged, this, [this]() {
        repositionScrollToBottomButton();
        updateScrollToBottomButtonVisibility();
    });

    m_scrollToBottomButton->setFocusPolicy(Qt::NoFocus);
    m_scrollToBottomButton->setCursor(Qt::PointingHandCursor);
    m_scrollToBottomButton->setToolTip(tr("Scroll to bottom"));
    m_scrollToBottomButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    m_scrollToBottomButton->setIconSize(QSize(16, 16));
    m_scrollToBottomButton->setFixedSize(30, 30);
    m_scrollToBottomButton->setStyleSheet(
        "QToolButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 15px;"
        "  background: palette(base);"
        "  color: palette(text);"
        "}"
        "QToolButton:hover {"
        "  background: palette(alternate-base);"
        "}");
    connect(m_scrollToBottomButton, &QToolButton::clicked, this, &ResultDisplay::scrollToBottom);
    m_scrollToBottomButton->hide();
    repositionScrollToBottomButton();
}

bool ResultDisplay::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == verticalScrollBar()) {
        if (event->type() == QEvent::Enter || event->type() == QEvent::HoverEnter) {
            if (!m_scrollBarHovered) {
                m_scrollBarHovered = true;
                updateScrollBarStyleSheet();
            }
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave) {
            if (m_scrollBarHovered) {
                m_scrollBarHovered = false;
                updateScrollBarStyleSheet();
            }
        }
    }

    return QPlainTextEdit::eventFilter(watched, event);
}

void ResultDisplay::setHoverHighlightEnabled(bool enabled)
{
    if (m_hoverHighlightEnabled == enabled)
        return;

    m_hoverHighlightEnabled = enabled;
    if (!m_hoverHighlightEnabled)
        m_hoveredHistoryIndex = -1;
    updateHoverHighlightSelection();
}

void ResultDisplay::setEditingHistoryIndex(int index)
{
    if (m_editingHistoryIndex == index)
        return;

    const int previousEditingHistoryIndex = m_editingHistoryIndex;
    m_editingHistoryIndex = index;

    if (m_editingHistoryIndex >= 0 && m_hoveredHistoryIndex >= 0) {
        const int previousHoveredHistoryIndex = m_hoveredHistoryIndex;
        m_hoveredHistoryIndex = -1;
        updateHoverHighlightSelection();
        viewport()->update(hoverActionRectForHistoryIndex(previousHoveredHistoryIndex));
    }
    if (m_editingHistoryIndex >= 0)
        viewport()->unsetCursor();

    updateHoverHighlightSelection();

    if (previousEditingHistoryIndex >= 0)
        viewport()->update(viewport()->rect());
    if (m_editingHistoryIndex >= 0)
        viewport()->update(viewport()->rect());
}

void ResultDisplay::append(const QString& expression, Quantity& value,
                           const QString& interpretedExpression)
{
    ++m_count;

    appendPlainText(formattedExpressionForDisplay(expression, interpretedExpression));
    if (!value.isNan()) {
        const Settings* settings = Settings::instance();
        EvaluationContext ctx;
        ctx.main.fmt = settings->resultFormat;
        ctx.main.prec = settings->resultPrecision;
        ctx.main.cplx = settings->resultFormatComplex;
        ctx.complexOn = settings->complexNumbers;
        ctx.unit = settings->imaginaryUnit;
        ctx.angle = settings->angleUnit;
        ctx.unitExp = settings->unitNegativeExponentStyle;
        ctx.round = settings->resultRoundingMode;
        const HistoryEntry entry(expression, value, interpretedExpression, ctx);
        const QStringList resultLines = formatResultLines(entry);
        for (const QString& line : resultLines)
            appendPlainText(line);
    }
    appendPlainText(QLatin1String(""));
    markHistoryBlockIndexCacheDirty();
}

int ResultDisplay::count() const
{
    return m_count;
}

QPair<int, int> ResultDisplay::viewportTopAnchor() const
{
    const QTextBlock block = firstVisibleBlock();
    if (!block.isValid())
        return qMakePair(-1, 0);

    const QRectF blockRect = blockBoundingGeometry(block).translated(contentOffset());
    const int offsetInBlock = qMax(0, qRound(-blockRect.top()));
    return qMakePair(block.blockNumber(), offsetInBlock);
}

void ResultDisplay::restoreViewportTopAnchor(const QPair<int, int>& anchor)
{
    if (anchor.first < 0 || document()->blockCount() <= 0)
        return;

    const int blockNumber = qBound(0, anchor.first, document()->blockCount() - 1);
    const QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid())
        return;

    QScrollBar* bar = verticalScrollBar();
    const int targetValue = qRound(blockBoundingGeometry(block).top()) + anchor.second;
    bar->setValue(qBound(bar->minimum(), targetValue, bar->maximum()));
}

QString ResultDisplay::exportHtml() const
{
    QString str;
    m_highlighter->asHtml(str);
    return str;
}

void ResultDisplay::rehighlight()
{
    m_highlighter->update();
    updateScrollBarStyleSheet();
}


void ResultDisplay::clear()
{
    m_count = 0;
    m_firstDisplayedHistoryIndex = 0;
    setPlainText(QLatin1String(""));
    markHistoryBlockIndexCacheDirty();
    clearHoverFeedback();
    updateScrollToBottomButtonVisibility();
}

void ResultDisplay::clearHoverFeedback()
{
    const int previousHoveredHistoryIndex = m_hoveredHistoryIndex;
    m_hoveredHistoryIndex = -1;
    QToolTip::hideText();
    updateHoverHighlightSelection();
    if (previousHoveredHistoryIndex >= 0)
        viewport()->update(hoverActionRectForHistoryIndex(previousHoveredHistoryIndex));
}


void ResultDisplay::refresh()
{
    const Session* session = Evaluator::instance()->session();
    const int historyCount = session->historySize();
    const int previousScrollValue = verticalScrollBar()->value();
    const int firstDisplayedHistoryIndex = firstDisplayedHistoryIndexForCount(historyCount);

    // Fast path for the common "new evaluation added one history entry" case.
    if (historyCount == m_count + 1
        && historyCount > 0
        && firstDisplayedHistoryIndex == m_firstDisplayedHistoryIndex) {
        clearHoverFeedback();
        const HistoryEntry& lastEntry = session->historyEntryAtRef(historyCount - 1);
        const QStringList renderedLines = renderedHistoryLinesForDisplay(lastEntry);
        for (const QString& line : renderedLines)
            appendPlainText(line);
        appendPlainText(QLatin1String(""));
        m_count = historyCount;
        m_firstDisplayedHistoryIndex = firstDisplayedHistoryIndex;
        markHistoryBlockIndexCacheDirty();
        updateHoverHighlightSelection();
        updateScrollToBottomButtonVisibility();
        return;
    }

    clearHoverFeedback();
    m_count = historyCount;
    m_firstDisplayedHistoryIndex = firstDisplayedHistoryIndex;

    QStringList allLines;
    allLines.reserve(qMax(1, (m_count - m_firstDisplayedHistoryIndex) * 3));
    for (int i = m_firstDisplayedHistoryIndex; i < m_count; ++i) {
        const HistoryEntry& historyEntry = session->historyEntryAtRef(i);
        allLines.append(renderedHistoryLinesForDisplay(historyEntry));
        allLines.append(QLatin1String(""));
    }

    setPlainText(allLines.join(QLatin1String("\n")));
    verticalScrollBar()->setValue(previousScrollValue);

    markHistoryBlockIndexCacheDirty();
    updateHoverHighlightSelection();
    updateScrollToBottomButtonVisibility();
}

void ResultDisplay::refreshLastHistoryEntry()
{
    const Session* session = Evaluator::instance()->session();
    const int historyCount = session->historySize();
    const int firstDisplayedHistoryIndex = firstDisplayedHistoryIndexForCount(historyCount);
    if (historyCount == 0) {
        clear();
        return;
    }

    if (m_count != historyCount
        || m_firstDisplayedHistoryIndex != firstDisplayedHistoryIndex
        || blockCount() <= 0) {
        refresh();
        return;
    }

    QTextBlock endBlock = document()->lastBlock();
    while (endBlock.isValid() && endBlock.text().isEmpty() && endBlock.previous().isValid())
        endBlock = endBlock.previous();

    if (!endBlock.isValid()) {
        refresh();
        return;
    }

    QTextBlock startBlock = endBlock;
    while (startBlock.previous().isValid() && !startBlock.previous().text().isEmpty())
        startBlock = startBlock.previous();

    const HistoryEntry& lastEntry = session->historyEntryAtRef(historyCount - 1);
    QStringList updatedLines = renderedHistoryLinesForDisplay(lastEntry);
    updatedLines.append(QLatin1String(""));

    clearHoverFeedback();

    QTextCursor cursor(document());
    cursor.setPosition(startBlock.position());
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.insertText(updatedLines.join(QLatin1String("\n")));
    markHistoryBlockIndexCacheDirty();
    updateHoverHighlightSelection();
    updateScrollToBottomButtonVisibility();
}

void ResultDisplay::scrollLines(int numberOfLines)
{
    QScrollBar* bar = verticalScrollBar();
    bar->setValue(bar->value() + numberOfLines);
}

void ResultDisplay::scrollLineUp()
{
    if (m_scrollDirection != 0) {
        stopActiveScrollingAnimation();
        return;
    }

    scrollLines(-1);
}

void ResultDisplay::scrollLineDown()
{
    if (m_scrollDirection != 0) {
        stopActiveScrollingAnimation();
        return;
    }

    scrollLines(1);
}

void ResultDisplay::scrollToDirection(int direction)
{
    m_scrolledLines = 0;
    bool mustStartTimer = (m_scrollDirection == 0);
    m_scrollDirection = direction;
    if (mustStartTimer)
        m_scrollTimer.start(16, this);
}

void ResultDisplay::scrollPageUp()
{
    if (verticalScrollBar()->value() == 0)
        return;

    m_isScrollingPageOnly = true;
    scrollToDirection(-1);
}

void ResultDisplay::scrollPageDown()
{
    if (verticalScrollBar()->value() == verticalScrollBar()->maximum())
        return;

    m_isScrollingPageOnly = true;
    scrollToDirection(1);
}

void ResultDisplay::scrollToTop()
{
    if (verticalScrollBar()->value() == 0)
        return;

    m_isScrollingPageOnly = false;
    scrollToDirection(-1);
}


void ResultDisplay::scrollToBottom()
{
    if (verticalScrollBar()->value() == verticalScrollBar()->maximum())
        return;

    m_isScrollingPageOnly = false;
    scrollToDirection(1);
}

void ResultDisplay::increaseFontPointSize()
{
    QFont newFont = font();
    const int newSize = newFont.pointSize() + 1;
    if (newSize > 96)
        return;
    newFont.setPointSize(newSize);
    setFont(newFont);
}

void ResultDisplay::decreaseFontPointSize()
{
    QFont newFont = font();
    const int newSize = newFont.pointSize() - 1;
    if (newSize < 8)
        return;
    newFont.setPointSize(newSize);
    setFont(newFont);
}

void ResultDisplay::mouseDoubleClickEvent(QMouseEvent* event)
{
    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::StartOfBlock);
    cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    QString text = cursor.selectedText();
    QString resultMarker = QLatin1String("= ");
    if (text.startsWith(resultMarker)) {
        // Display lines strip unit brackets for readability, but editor input
        // requires canonical expression syntax ("value[unit]") with QuantSp.
        // For result lines (all blocks after the expression block in the same
        // history entry), rehydrate from the underlying Quantity instead of
        // copying the visually simplified text.
        const int historyIndex = historyIndexAtPosition(event->pos());
        if (historyIndex >= 0) {
            int startBlock = -1;
            int endBlock = -1;
            if (blockRangeForHistoryIndex(historyIndex, startBlock, endBlock)
                && cursor.blockNumber() > startBlock)
            {
                const Session* session = Evaluator::instance()->session();
                if (historyIndex < session->historySize()) {
                    const Quantity value = session->historyEntryAtRef(historyIndex).result();
                    if (!value.isNan()) {
                        QString clipboardText = formatResultForClipboard(value);
                        const QString displayedResultLine = text.mid(resultMarker.size());
                        if (!clipboardText.contains(MathDsl::UnitStart)
                            && !clipboardText.endsWith(MathDsl::Deg)
                            && !clipboardText.endsWith(UnicodeChars::Prime)
                            && !clipboardText.endsWith(UnicodeChars::DoublePrime)) {
                            if (displayedResultLine.endsWith(MathDsl::Deg))
                                clipboardText += MathDsl::Deg;
                            else if (displayedResultLine.endsWith(UnicodeChars::Prime))
                                clipboardText += UnicodeChars::Prime;
                            else if (displayedResultLine.endsWith(UnicodeChars::DoublePrime))
                                clipboardText += UnicodeChars::DoublePrime;
                        }
                        emit expressionSelected(clipboardText);
                        return;
                    }
                }
            }
        }
        text.remove(resultMarker);
    }
    emit expressionSelected(text);
}

void ResultDisplay::mousePressEvent(QMouseEvent* event)
{
    emit clicked();

    if (m_editingHistoryIndex >= 0) {
        const QRect cancelRect = cancelGlyphBadgeRectForEditingIndex();
        if (event->button() == Qt::LeftButton && cancelRect.isValid() && cancelRect.contains(event->pos())) {
            emit cancelHistoryEditRequested();
            event->accept();
            return;
        }
        QPlainTextEdit::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton && m_hoverHighlightEnabled && m_hoveredHistoryIndex >= 0) {
        const QRect copyRect = copyGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
        if (copyRect.contains(event->pos())) {
            const Session* session = Evaluator::instance()->session();
            if (m_hoveredHistoryIndex >= 0 && m_hoveredHistoryIndex < session->historySize()) {
                const Quantity value = session->historyEntryAtRef(m_hoveredHistoryIndex).result();
                if (!value.isNan())
                    QApplication::clipboard()->setText(formatResultForClipboard(value), QClipboard::Clipboard);
            }
            event->accept();
            return;
        }

        const QRect editRect = editGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
        if (editRect.contains(event->pos())) {
            emit editHistoryEntryRequested(m_hoveredHistoryIndex);
            event->accept();
            return;
        }

        const QRect settingsRect = settingsGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
        if (settingsRect.contains(event->pos())) {
            emit editHistoryEntryContextRequested(m_hoveredHistoryIndex);
            event->accept();
            return;
        }

        const QRect removeRect = removeGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
        if (removeRect.contains(event->pos())) {
            emit removeHistoryEntryRequested(m_hoveredHistoryIndex);
            event->accept();
            return;
        }
    }

    QPlainTextEdit::mousePressEvent(event);
}

void ResultDisplay::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = createStandardContextMenu();
    const int historyIndex = historyIndexAtPosition(event->pos());
    if (historyIndex >= 0) {
        const Session* session = Evaluator::instance()->session();
        menu->addSeparator();
        QAction* copyExpressionAction = menu->addAction(tr("Copy Expression"));
        connect(copyExpressionAction, &QAction::triggered, this, [session, historyIndex]() {
            if (historyIndex < 0 || historyIndex >= session->historySize())
                return;

            QApplication::clipboard()->setText(session->historyEntryAtRef(historyIndex).expr(), QClipboard::Clipboard);
        });
        QAction* copyResultAction = menu->addAction(tr("Copy Result"));
        const bool canCopyResult = historyIndex >= 0
            && historyIndex < session->historySize()
            && !session->historyEntryAtRef(historyIndex).result().isNan();
        copyResultAction->setEnabled(canCopyResult);
        connect(copyResultAction, &QAction::triggered, this, [session, historyIndex]() {
            if (historyIndex < 0 || historyIndex >= session->historySize())
                return;

            const Quantity value = session->historyEntryAtRef(historyIndex).result();
            if (value.isNan())
                return;

            QApplication::clipboard()->setText(formatResultForClipboard(value), QClipboard::Clipboard);
        });
        menu->addSeparator();
        QAction* editAction = menu->addAction(tr("Edit Expression"));
        connect(editAction, &QAction::triggered, this, [this, historyIndex]() {
            emit editHistoryEntryRequested(historyIndex);
        });
        QAction* editSettingsAction = menu->addAction(tr("Change Settings"));
        connect(editSettingsAction, &QAction::triggered, this, [this, historyIndex]() {
            emit editHistoryEntryContextRequested(historyIndex);
        });
        QAction* removeAction = menu->addAction(tr("Remove Calculation"));
        connect(removeAction, &QAction::triggered, this, [this, historyIndex]() {
            emit removeHistoryEntryRequested(historyIndex);
        });
        menu->addSeparator();
        QAction* removeAboveAction = menu->addAction(tr("Remove All Calculations Above"));
        connect(removeAboveAction, &QAction::triggered, this, [this, historyIndex]() {
            emit removeHistoryEntriesAboveRequested(historyIndex);
        });
        QAction* removeBelowAction = menu->addAction(tr("Remove All Calculations Below"));
        connect(removeBelowAction, &QAction::triggered, this, [this, historyIndex]() {
            emit removeHistoryEntriesBelowRequested(historyIndex);
        });
    }

    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(window());
    if (mainWindow != 0 && mainWindow->menuBar() != 0) {
        menu->addSeparator();
        QMenu* mainMenu = menu->addMenu(tr("Main Menu"));
        const QList<QAction*> topLevelActions = mainWindow->menuBar()->actions();
        for (QAction* topLevelAction : topLevelActions) {
            if (topLevelAction->isSeparator()) {
                mainMenu->addSeparator();
                continue;
            }

            QMenu* sourceSubmenu = topLevelAction->menu();
            if (sourceSubmenu != 0) {
                QMenu* clonedTopLevelSubmenu = mainMenu->addMenu(sourceSubmenu->title());
                clonedTopLevelSubmenu->setEnabled(topLevelAction->isEnabled());
                cloneMenuActions(sourceSubmenu, clonedTopLevelSubmenu);
            }
        }
    }

    menu->exec(event->globalPos());
    delete menu;
}

void ResultDisplay::leaveEvent(QEvent* event)
{
    QPlainTextEdit::leaveEvent(event);
    viewport()->unsetCursor();
    if (m_hoveredHistoryIndex >= 0) {
        const int previousHoveredHistoryIndex = m_hoveredHistoryIndex;
        m_hoveredHistoryIndex = -1;
        updateHoverHighlightSelection();
        viewport()->update(hoverActionRectForHistoryIndex(previousHoveredHistoryIndex));
    }
}

void ResultDisplay::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != m_scrollTimer.timerId()) {
        QWidget::timerEvent(event);
        return;
    }

    if (m_isScrollingPageOnly)
        pageScrollEvent();
    else
        fullContentScrollEvent();
}

void ResultDisplay::pageScrollEvent()
{
    if (m_scrolledLines >= linesPerPage()) {
        stopActiveScrollingAnimation();
        return;
    }

    scrollLines(m_scrollDirection * 2);
    m_scrolledLines += 2;
}

void ResultDisplay::fullContentScrollEvent()
{
    QScrollBar* bar = verticalScrollBar();
    int value = bar->value();
    bool shouldStop = (m_scrollDirection == -1 && value <= 0) || (m_scrollDirection == 1 && value >= bar->maximum());

    if (shouldStop && m_scrollDirection != 0) {
        stopActiveScrollingAnimation();
        return;
    }

    scrollLines(m_scrollDirection * 10);
}

void ResultDisplay::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
        if (event->angleDelta().y() > 0)
            emit shiftControlWheelUp();
        else
            emit shiftControlWheelDown();
    } else if (event->modifiers() == Qt::ShiftModifier) {
        if (event->angleDelta().y() > 0)
            emit shiftWheelUp();
        else
            emit shiftWheelDown();
    } else if (event->modifiers() == Qt::ControlModifier) {
        if (event->angleDelta().y() > 0)
            emit controlWheelUp();
        else
            emit controlWheelDown();
    } else {
        QPlainTextEdit::wheelEvent(event);
        return;
    }

    event->accept();
}

void ResultDisplay::mouseMoveEvent(QMouseEvent* event)
{
    QPlainTextEdit::mouseMoveEvent(event);

    if (event->buttons() & Qt::LeftButton) {
        QToolTip::hideText();
        viewport()->unsetCursor();
        return;
    }

    if (m_editingHistoryIndex >= 0) {
        const QRect cancelRect = cancelGlyphBadgeRectForEditingIndex();
        const bool overCancelGlyph = cancelRect.isValid() && cancelRect.contains(event->pos());
        if (overCancelGlyph)
            viewport()->setCursor(Qt::PointingHandCursor);
        else
            viewport()->unsetCursor();

        if (overCancelGlyph)
            QToolTip::showText(QCursor::pos(), tr("Cancel editing"), this);
        else
            QToolTip::hideText();
        return;
    }

    if (!m_hoverHighlightEnabled) {
        if (m_hoveredHistoryIndex >= 0) {
            const int previousHoveredHistoryIndex = m_hoveredHistoryIndex;
            m_hoveredHistoryIndex = -1;
            updateHoverHighlightSelection();
            viewport()->update(hoverActionRectForHistoryIndex(previousHoveredHistoryIndex));
        }
        QToolTip::hideText();
        viewport()->unsetCursor();
        return;
    }

    const int hoveredHistoryIndex = historyIndexAtPosition(event->pos());
    if (hoveredHistoryIndex != m_hoveredHistoryIndex) {
        const int previousHoveredHistoryIndex = m_hoveredHistoryIndex;
        m_hoveredHistoryIndex = hoveredHistoryIndex;
        updateHoverHighlightSelection();
        if (previousHoveredHistoryIndex >= 0)
            viewport()->update(hoverActionRectForHistoryIndex(previousHoveredHistoryIndex));
        if (m_hoveredHistoryIndex >= 0)
            viewport()->update(hoverActionRectForHistoryIndex(m_hoveredHistoryIndex));
    }

    QRect copyRect;
    QRect editRect;
    QRect settingsRect;
    QRect removeRect;
    if (m_hoveredHistoryIndex >= 0) {
        copyRect = copyGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
        editRect = editGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
        settingsRect = settingsGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
        removeRect = removeGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
    }

    const bool overCopyGlyph = copyRect.contains(event->pos());
    const bool overEditGlyph = editRect.contains(event->pos());
    const bool overSettingsGlyph = settingsRect.contains(event->pos());
    const bool overRemoveGlyph = removeRect.contains(event->pos());
    const bool overActionGlyph = overCopyGlyph || overEditGlyph || overSettingsGlyph || overRemoveGlyph;
    if (overActionGlyph)
        viewport()->setCursor(Qt::PointingHandCursor);
    else
        viewport()->unsetCursor();

    if (overCopyGlyph) {
        QToolTip::showText(QCursor::pos(), tr("Copy result"), this);
    } else if (overEditGlyph) {
        QToolTip::showText(QCursor::pos(), tr("Edit expression"), this);
    } else if (overSettingsGlyph) {
        QToolTip::showText(QCursor::pos(), tr("Change settings"), this);
    } else if (overRemoveGlyph) {
        QToolTip::showText(QCursor::pos(), tr("Remove calculation"), this);
    } else {
        QToolTip::hideText();
    }
}

void ResultDisplay::paintEvent(QPaintEvent* event)
{
    QPlainTextEdit::paintEvent(event);

    QPainter painter(viewport());

    if (m_editingHistoryIndex >= 0) {
        const QRect cancelRect = cancelGlyphBadgeRectForEditingIndex();
        if (!cancelRect.isValid())
            return;

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::white);
        painter.drawEllipse(cancelRect);
        painter.setPen(QPen(QColor(200, 50, 0, 255), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF center = badgeCenter(cancelRect);
        const qreal side = cancelRect.width() * 0.22;
        painter.drawRect(QRectF(center.x() - side, center.y() - side, side * 2.0, side * 2.0));
        return;
    }

    if (!m_hoverHighlightEnabled || m_hoveredHistoryIndex < 0)
        return;

    const QRect copyRect = copyGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
    const QRect removeRect = removeGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
    const QRect editRect = editGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
    const QRect settingsRect = settingsGlyphBadgeRectForHistoryIndex(m_hoveredHistoryIndex);
    if (!copyRect.isValid() || !removeRect.isValid() || !editRect.isValid() || !settingsRect.isValid())
        return;

    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(copyRect);
    painter.setPen(QPen(QColor(80, 80, 80, 255), 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF copyCenter = badgeCenter(copyRect);
    const qreal pageSize = copyRect.width() * 0.33;
    const QRectF backPage(copyCenter.x() - pageSize * 0.72,
                          copyCenter.y() - pageSize * 0.72,
                          pageSize,
                          pageSize);
    const QRectF frontPage(copyCenter.x() - pageSize * 0.28,
                           copyCenter.y() - pageSize * 0.28,
                           pageSize,
                           pageSize);
    const qreal radius = qMax(0.8, copyRect.width() * 0.07);
    painter.drawRoundedRect(backPage, radius, radius);
    painter.drawRoundedRect(frontPage, radius, radius);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(editRect);
    painter.setPen(QPen(QColor(40, 90, 180, 255), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF editCenter = badgeCenter(editRect);
    const qreal editHalf = editRect.width() * 0.20;
    painter.drawLine(QPointF(editCenter.x() - editHalf, editCenter.y() + editHalf),
                     QPointF(editCenter.x() + editHalf, editCenter.y() - editHalf));
    painter.setPen(QPen(QColor(40, 90, 180, 255), 1.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(editCenter.x() + editHalf * 0.62, editCenter.y() - editHalf * 0.62),
                     QPointF(editCenter.x() + editHalf * 1.08, editCenter.y() - editHalf * 1.08));

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(settingsRect);
    painter.setPen(QPen(QColor(20, 120, 120, 255), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const QPointF settingsCenter = badgeCenter(settingsRect);
    const qreal gear = settingsRect.width() * 0.16;
    painter.drawEllipse(QRectF(settingsCenter.x() - gear, settingsCenter.y() - gear, gear * 2.0, gear * 2.0));
    painter.drawLine(QPointF(settingsCenter.x() - gear * 1.9, settingsCenter.y()),
                     QPointF(settingsCenter.x() - gear * 1.2, settingsCenter.y()));
    painter.drawLine(QPointF(settingsCenter.x() + gear * 1.2, settingsCenter.y()),
                     QPointF(settingsCenter.x() + gear * 1.9, settingsCenter.y()));
    painter.drawLine(QPointF(settingsCenter.x(), settingsCenter.y() - gear * 1.9),
                     QPointF(settingsCenter.x(), settingsCenter.y() - gear * 1.2));
    painter.drawLine(QPointF(settingsCenter.x(), settingsCenter.y() + gear * 1.2),
                     QPointF(settingsCenter.x(), settingsCenter.y() + gear * 1.9));

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(removeRect);
    painter.setPen(QPen(QColor(220, 0, 0, 255), 1.8, Qt::SolidLine, Qt::RoundCap));
    const QPointF center = badgeCenter(removeRect);
    const qreal half = removeRect.width() * 0.22;
    painter.drawLine(QPointF(center.x() - half, center.y() - half),
                     QPointF(center.x() + half, center.y() + half));
    painter.drawLine(QPointF(center.x() - half, center.y() + half),
                     QPointF(center.x() + half, center.y() - half));
}

void ResultDisplay::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    repositionScrollToBottomButton();
}

void ResultDisplay::scrollContentsBy(int dx, int dy)
{
    QPlainTextEdit::scrollContentsBy(dx, dy);
    updateScrollToBottomButtonVisibility();
}

void ResultDisplay::stopActiveScrollingAnimation()
{
    m_scrollTimer.stop();
    m_scrolledLines = 0;
    m_scrollDirection = 0;
    updateScrollToBottomButtonVisibility();
}

void ResultDisplay::repositionScrollToBottomButton()
{
    if (!m_scrollToBottomButton)
        return;

    const int margin = 10;
    const QRect contentRect = contentsRect();
    const int x = contentRect.left()
        + qMax(0, (contentRect.width() - m_scrollToBottomButton->width()) / 2);
    const int y = contentRect.top()
        + qMax(0, contentRect.height() - m_scrollToBottomButton->height() - margin);
    m_scrollToBottomButton->move(x, y);
    m_scrollToBottomButton->raise();
}

void ResultDisplay::updateScrollToBottomButtonVisibility()
{
    if (!m_scrollToBottomButton)
        return;

    QScrollBar* bar = verticalScrollBar();
    const bool hasScrollableContent = bar->maximum() > bar->minimum();
    const bool nearBottom = bar->value() >= bar->maximum();
    m_scrollToBottomButton->setVisible(hasScrollableContent && !nearBottom);
}

void ResultDisplay::updateScrollBarStyleSheet()
{
    static const int kBaseScrollBarWidthAt96Dpi = 5;
    static const qreal kReferenceDpi = 96.0;
    const int baseScrollBarWidth = qMax(
        1,
        qRound(kBaseScrollBarWidthAt96Dpi * (logicalDpiX() / kReferenceDpi)));
    const int scrollBarWidth = m_scrollBarHovered
        ? baseScrollBarWidth * 2
        : baseScrollBarWidth;

    verticalScrollBar()->setStyleSheet(QString(
        "QScrollBar:vertical {"
        "   border: 0;"
        "   margin: 0 0 0 0;"
        "   background: %1;"
        "   width: %3px;"
        "}"
        "QScrollBar::handle:vertical {"
        "   background: %2;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "   border: 0;"
        "   width: 0;"
        "   height: 0;"
        "   background: %1;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "   background: %1;"
        "}"
    ).arg(m_highlighter->colorForRole(ColorScheme::Background).name(),
          m_highlighter->colorForRole(ColorScheme::ScrollBar).name())
      .arg(scrollBarWidth));
}

int ResultDisplay::historyIndexAtPosition(const QPoint& pos) const
{
    const QTextCursor cursor = cursorForPosition(pos);
    const int blockNumber = cursor.blockNumber();
    if (blockNumber < 0)
        return -1;

    ensureHistoryBlockIndexCache();
    if (blockNumber >= m_blockToHistoryIndex.size())
        return -1;

    const int historyIndex = m_blockToHistoryIndex.at(blockNumber);
    const int localHistoryIndex = historyIndex - m_firstDisplayedHistoryIndex;
    if (localHistoryIndex < 0 || localHistoryIndex >= m_historyBlockRanges.size())
        return -1;

    const QPair<int, int> range = m_historyBlockRanges.at(localHistoryIndex);
    // Separator blank lines are intentionally mapped in the cache; keep them
    // non-interactive while still allowing genuine blank lines inside entries.
    if (blockNumber < range.first || blockNumber > range.second)
        return -1;

    return historyIndex;
}

bool ResultDisplay::blockRangeForHistoryIndex(int historyIndex, int& startBlock, int& endBlock) const
{
    startBlock = -1;
    endBlock = -1;
    const int localHistoryIndex = historyIndex - m_firstDisplayedHistoryIndex;
    if (localHistoryIndex < 0)
        return false;

    ensureHistoryBlockIndexCache();
    if (localHistoryIndex >= m_historyBlockRanges.size())
        return false;

    const QPair<int, int> range = m_historyBlockRanges.at(localHistoryIndex);
    startBlock = range.first;
    endBlock = range.second;
    return startBlock >= 0 && endBlock >= startBlock;
}

QRect ResultDisplay::removeGlyphRectForHistoryIndex(int historyIndex) const
{
    int startBlock = -1;
    int endBlock = -1;
    if (!blockRangeForHistoryIndex(historyIndex, startBlock, endBlock))
        return QRect();

    QTextBlock start = document()->findBlockByNumber(startBlock);
    QTextBlock end = document()->findBlockByNumber(endBlock);
    if (!start.isValid() || !end.isValid())
        return QRect();

    const QRectF startRect = blockBoundingGeometry(start).translated(contentOffset());
    const QRectF endRect = blockBoundingGeometry(end).translated(contentOffset());
    const int top = qRound(startRect.top());
    const int bottom = qRound(endRect.bottom());
    if (bottom <= top)
        return QRect();

    const int side = qMax(12, fontMetrics().height());
    const int spacing = 4;
    const int rightPadding = 6;
    const int left = viewport()->width() - side - rightPadding;
    const int minLeft = side + spacing;
    if (left < minLeft)
        return QRect();

    return QRect(left, top, side, bottom - top);
}

QRect ResultDisplay::removeGlyphBadgeRectForHistoryIndex(int historyIndex) const
{
    const QRect removeLaneRect = removeGlyphRectForHistoryIndex(historyIndex);
    if (!removeLaneRect.isValid())
        return QRect();

    const int diameter = qMin(removeLaneRect.width(), qMax(12, fontMetrics().height() - 2));
    const int left = removeLaneRect.left() + (removeLaneRect.width() - diameter) / 2;
    const int top = removeLaneRect.top() + (removeLaneRect.height() - diameter) / 2;
    return QRect(left, top, diameter, diameter);
}

QRect ResultDisplay::editGlyphRectForHistoryIndex(int historyIndex) const
{
    const QRect removeRect = removeGlyphRectForHistoryIndex(historyIndex);
    if (!removeRect.isValid())
        return QRect();

    const int spacing = 4;
    const int left = removeRect.left() - removeRect.width() - spacing;
    if (left < 0)
        return QRect();

    return QRect(left, removeRect.top(), removeRect.width(), removeRect.height());
}

QRect ResultDisplay::editGlyphBadgeRectForHistoryIndex(int historyIndex) const
{
    const QRect editLaneRect = editGlyphRectForHistoryIndex(historyIndex);
    if (!editLaneRect.isValid())
        return QRect();

    const int diameter = qMin(editLaneRect.width(), qMax(12, fontMetrics().height() - 2));
    const int left = editLaneRect.left() + (editLaneRect.width() - diameter) / 2;
    const int top = editLaneRect.top() + (editLaneRect.height() - diameter) / 2;
    return QRect(left, top, diameter, diameter);
}

QRect ResultDisplay::copyGlyphRectForHistoryIndex(int historyIndex) const
{
    const QRect settingsRect = settingsGlyphRectForHistoryIndex(historyIndex);
    if (!settingsRect.isValid())
        return QRect();

    const int spacing = 4;
    const int left = settingsRect.left() - settingsRect.width() - spacing;
    if (left < 0)
        return QRect();

    return QRect(left, settingsRect.top(), settingsRect.width(), settingsRect.height());
}

QRect ResultDisplay::copyGlyphBadgeRectForHistoryIndex(int historyIndex) const
{
    const QRect copyLaneRect = copyGlyphRectForHistoryIndex(historyIndex);
    if (!copyLaneRect.isValid())
        return QRect();

    const int diameter = qMin(copyLaneRect.width(), qMax(12, fontMetrics().height() - 2));
    const int left = copyLaneRect.left() + (copyLaneRect.width() - diameter) / 2;
    const int top = copyLaneRect.top() + (copyLaneRect.height() - diameter) / 2;
    return QRect(left, top, diameter, diameter);
}

QRect ResultDisplay::settingsGlyphRectForHistoryIndex(int historyIndex) const
{
    const QRect editRect = editGlyphRectForHistoryIndex(historyIndex);
    if (!editRect.isValid())
        return QRect();

    const int spacing = 4;
    const int left = editRect.left() - editRect.width() - spacing;
    if (left < 0)
        return QRect();

    return QRect(left, editRect.top(), editRect.width(), editRect.height());
}

QRect ResultDisplay::settingsGlyphBadgeRectForHistoryIndex(int historyIndex) const
{
    const QRect settingsLaneRect = settingsGlyphRectForHistoryIndex(historyIndex);
    if (!settingsLaneRect.isValid())
        return QRect();

    const int diameter = qMin(settingsLaneRect.width(), qMax(12, fontMetrics().height() - 2));
    const int left = settingsLaneRect.left() + (settingsLaneRect.width() - diameter) / 2;
    const int top = settingsLaneRect.top() + (settingsLaneRect.height() - diameter) / 2;
    return QRect(left, top, diameter, diameter);
}

QRect ResultDisplay::hoverActionRectForHistoryIndex(int historyIndex) const
{
    return copyGlyphRectForHistoryIndex(historyIndex)
        .united(settingsGlyphRectForHistoryIndex(historyIndex))
        .united(editGlyphRectForHistoryIndex(historyIndex))
        .united(removeGlyphRectForHistoryIndex(historyIndex));
}

QRect ResultDisplay::cancelGlyphBadgeRectForEditingIndex() const
{
    if (m_editingHistoryIndex < 0)
        return QRect();
    return removeGlyphBadgeRectForHistoryIndex(m_editingHistoryIndex);
}

void ResultDisplay::updateHoverHighlightSelection()
{
    QList<QTextEdit::ExtraSelection> selections;
    if (m_hoveredHistoryIndex < 0 && m_editingHistoryIndex < 0) {
        setExtraSelections(selections);
        return;
    }

    const QColor hoverColor = hoverColorForBackground(
        m_highlighter->colorForRole(ColorScheme::Background));

    auto appendSelectionForHistoryIndex = [this, &selections, &hoverColor](int historyIndex) {
        int startBlock = -1;
        int endBlock = -1;
        if (!blockRangeForHistoryIndex(historyIndex, startBlock, endBlock))
            return;

        for (int blockNumber = startBlock; blockNumber <= endBlock; ++blockNumber) {
            QTextBlock block = document()->findBlockByNumber(blockNumber);
            if (!block.isValid())
                continue;

            QTextLayout* layout = block.layout();
            if (layout == nullptr)
                continue;

            const int lineCount = layout->lineCount();
            if (lineCount <= 0)
                continue;

            const int blockPosition = block.position();
            for (int lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
                const QTextLine line = layout->lineAt(lineIndex);
                QTextEdit::ExtraSelection selection;
                selection.cursor = QTextCursor(document());
                selection.cursor.setPosition(blockPosition + line.textStart());
                selection.format.setProperty(QTextFormat::FullWidthSelection, true);
                selection.format.setBackground(hoverColor);
                selections.append(selection);
            }
        }
    };

    if (m_hoverHighlightEnabled && m_hoveredHistoryIndex >= 0)
        appendSelectionForHistoryIndex(m_hoveredHistoryIndex);
    if (m_editingHistoryIndex >= 0 && (!m_hoverHighlightEnabled || m_editingHistoryIndex != m_hoveredHistoryIndex))
        appendSelectionForHistoryIndex(m_editingHistoryIndex);

    setExtraSelections(selections);
}

void ResultDisplay::markHistoryBlockIndexCacheDirty()
{
    m_historyBlockIndexCacheDirty = true;
}

void ResultDisplay::ensureHistoryBlockIndexCache() const
{
    if (!m_historyBlockIndexCacheDirty)
        return;

    m_blockToHistoryIndex.clear();
    m_historyBlockRanges.clear();

    const Session* session = Evaluator::instance()->session();
    const int historySize = session->historySize();
    const int firstDisplayedHistoryIndex = firstDisplayedHistoryIndexForCount(historySize);
    const int displayedHistoryCount = historySize - firstDisplayedHistoryIndex;
    m_historyBlockRanges.reserve(qMax(0, displayedHistoryCount));

    QTextBlock block = document()->firstBlock();
    for (int i = firstDisplayedHistoryIndex; i < historySize; ++i) {
        const int startBlock = block.isValid() ? block.blockNumber() : -1;
        int endBlock = -1;

        // Reconstruct ranges from the currently displayed text blocks so hover
        // hit-testing always matches what is visible, even after format mode
        // changes that intentionally do not rewrite history lines.
        while (block.isValid()) {
            const int blockNumber = block.blockNumber();
            if (block.text().isEmpty()) {
                // Preserve existing behavior: separator blocks map to this
                // history entry, but are excluded from highlight ranges.
                m_blockToHistoryIndex.append(i);
                block = block.next();
                break;
            }

            m_blockToHistoryIndex.append(i);
            endBlock = blockNumber;
            block = block.next();
        }

        m_historyBlockRanges.append(qMakePair(startBlock, endBlock));
    }

    // Keep the cache shape aligned with the rendered document even if there
    // are stale trailing blocks.
    while (block.isValid()) {
        m_blockToHistoryIndex.append(historySize > 0 ? historySize - 1 : -1);
        block = block.next();
    }

    m_historyBlockIndexCacheDirty = false;
}
