// SPDX-FileCopyrightText: 2007-2011, 2013-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


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
class QMenu;
class QPainter;
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
    void setLoadedSessionCount(int count);
    void setCloseSessionEnabled(bool enabled);
    void setSession(const Session* session);
    const Session* session() const { return m_session; }
    bool closeSessionEnabled() const { return m_closeSessionEnabled; }
    int loadedSessionCount() const { return m_loadedSessionCount; }
    QPair<int, int> viewportTopAnchor() const;
    void restoreViewportTopAnchor(const QPair<int, int>& anchor);
    void restoreScrollValue(int value);

signals:
    void clicked();
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
    void newSessionRequested();
    void openSessionRequested();
    void duplicateSessionRequested();
    void splitLeftRequested();
    void splitRightRequested();
    void splitUpRequested();
    void splitDownRequested();
    void renameSessionRequested();
    void clearSessionRequested();
    void closeSessionRequested();
    void closePaneRequested();
    void deleteSessionRequested();
    void loadedSessionsMenuRequested(const QPoint& globalPos);

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
    QMenu* createContextMenu(const QPoint& pos);
    void drawScrollEdgeGradients(QPainter* painter);
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
    bool historyBlockOverlapsSessionBadge(int historyIndex) const;
    bool historyBlockOverlapsScrollToBottomButton(int historyIndex) const;
    void updateHoverHighlightSelection();
    void markHistoryBlockIndexCacheDirty();
    void markSimplifiedExpressionBlock(int blockNumber);
    void markSimplifiedExpressionBlocks();
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
    int m_firstDisplayedHistoryIndex;
    int m_loadedSessionCount;
    bool m_closeSessionEnabled;
    const Session* m_session;
    QToolButton* m_scrollToBottomButton;
};

#endif
