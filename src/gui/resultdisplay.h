// This file is part of the SpeedCrunch project
// Copyright (C) 2009, 2011, 2013, 2014 @heldercorreia
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

#ifndef GUI_RESULTDISPLAY_H
#define GUI_RESULTDISPLAY_H

#include <QBasicTimer>
#include <QPlainTextEdit>
#include <QPair>
#include <QVector>

class Quantity;
class SyntaxHighlighter;
class HistoryEntry;
class Session;
class QContextMenuEvent;
class QPoint;
class QMouseEvent;
class QEvent;
class QPaintEvent;
class QRect;
class QResizeEvent;
class QToolButton;

class ResultDisplay : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit ResultDisplay(QWidget* parent = 0);

    void append(const QString& expr, Quantity& value,
                const QString& interpretedExpression = QString());
    void appendHistory(const QStringList& expressions, const QStringList& results);
    int count() const;
    bool isEmpty() const { return m_count==0; }
    QString exportHtml() const;
    void setHoverHighlightEnabled(bool enabled);
    void setEditingHistoryIndex(int index);
    QPair<int, int> viewportTopAnchor() const;
    void restoreViewportTopAnchor(const QPair<int, int>& anchor);

signals:
    void shiftWheelDown();
    void shiftWheelUp();
    void shiftControlWheelDown();
    void shiftControlWheelUp();
    void controlWheelDown();
    void controlWheelUp();
    void expressionSelected(const QString&);
    void editHistoryEntryRequested(int index);
    void editHistoryEntryContextRequested(int index);
    void cancelHistoryEditRequested();
    void removeHistoryEntryRequested(int index);
    void removeHistoryEntriesAboveRequested(int index);
    void removeHistoryEntriesBelowRequested(int index);

public slots:
    void clear();
    void clearHoverFeedback();
    void decreaseFontPointSize();
    void increaseFontPointSize();
    void rehighlight();
    void refresh();
    void refreshLastHistoryEntry();
    void scrollLines(int);
    void scrollLineUp();
    void scrollLineDown();
    void scrollPageUp();
    void scrollPageDown();
    void scrollToBottom();
    void scrollToTop();

protected:
    virtual void contextMenuEvent(QContextMenuEvent*);
    virtual bool eventFilter(QObject* watched, QEvent* event);
    virtual void leaveEvent(QEvent*);
    virtual void mouseDoubleClickEvent(QMouseEvent*);
    virtual void mousePressEvent(QMouseEvent*);
    virtual void mouseMoveEvent(QMouseEvent*);
    virtual void paintEvent(QPaintEvent*);
    virtual void resizeEvent(QResizeEvent*);
    virtual void scrollContentsBy(int dx, int dy);
    virtual void wheelEvent(QWheelEvent*);
    virtual void timerEvent(QTimerEvent*);
    void fullContentScrollEvent();
    float linesPerPage() const { return static_cast<float>(viewport()->height()) / fontMetrics().height(); }
    void pageScrollEvent();
    void scrollToDirection(int);
    void stopActiveScrollingAnimation();
    void repositionScrollToBottomButton();
    void updateScrollToBottomButtonVisibility();
    void updateScrollBarStyleSheet();
    int historyIndexAtPosition(const QPoint& pos) const;
    bool blockRangeForHistoryIndex(int historyIndex, int& startBlock, int& endBlock) const;
    QRect removeGlyphRectForHistoryIndex(int historyIndex) const;
    QRect removeGlyphBadgeRectForHistoryIndex(int historyIndex) const;
    QRect editGlyphRectForHistoryIndex(int historyIndex) const;
    QRect editGlyphBadgeRectForHistoryIndex(int historyIndex) const;
    QRect settingsGlyphRectForHistoryIndex(int historyIndex) const;
    QRect settingsGlyphBadgeRectForHistoryIndex(int historyIndex) const;
    QRect copyGlyphRectForHistoryIndex(int historyIndex) const;
    QRect copyGlyphBadgeRectForHistoryIndex(int historyIndex) const;
    QRect hoverActionRectForHistoryIndex(int historyIndex) const;
    QRect cancelGlyphBadgeRectForEditingIndex() const;
    void updateHoverHighlightSelection();
    void markHistoryBlockIndexCacheDirty();
    void ensureHistoryBlockIndexCache() const;

private:
    Q_DISABLE_COPY(ResultDisplay)

    SyntaxHighlighter* m_highlighter;
    QBasicTimer m_scrollTimer;
    int m_scrolledLines;
    int m_scrollDirection;
    bool m_isScrollingPageOnly;
    bool m_hoverHighlightEnabled;
    bool m_scrollBarHovered;
    mutable bool m_historyBlockIndexCacheDirty;
    mutable QVector<int> m_blockToHistoryIndex;
    mutable QVector<QPair<int, int>> m_historyBlockRanges;
    int m_hoveredHistoryIndex;
    int m_editingHistoryIndex;
    int m_count;
    QToolButton* m_scrollToBottomButton;
};

#endif
