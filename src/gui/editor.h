// SPDX-FileCopyrightText: 2007-2010, 2013-2017, 2019-2020, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_EDITOR_H
#define GUI_EDITOR_H

#include "core/colorscheme.h"
#include "core/sessionhistory.h"

#include <QColor>
#include <QPlainTextEdit>
#include <memory>
#include <optional>

struct Constant;
class ConstantCompletion;
class EditorCompletion;
class Evaluator;
class Session;
class CNumber;
class SyntaxHighlighter;

class QEvent;
class QFocusEvent;
class QInputMethodEvent;
class QKeyEvent;
class QMimeData;
class QTimeLine;
class QTimer;
class QTreeWidget;
class QWheelEvent;
class QWidget;
class QResizeEvent;

class Editor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit Editor(QWidget* parent = nullptr);
    ~Editor() override;

    bool isAutoCalcEnabled() const;
    bool isAutoCompletionEnabled() const;
    Evaluator* evaluator() const { return m_evaluator; }
    static Editor* completionMouseSelectionOwner();
    void clearHistory();
    QColor cursorColor() const { return m_themePrimaryColor; }
    int cursorPosition() const;
    void doBackspace();
    void doDelete();
    char radixChar() const;
    void setAutoCalcEnabled(bool);
    void setAutoCompletionEnabled(bool);
    void setCustomCursorVisible(bool visible);
    void setThemePrimaryColor(const QColor& color, bool usePrimaryOutline);
    void setThemePreviewColorScheme(const ColorScheme& scheme);
    void setThemeCompletionColors(const QColor& background,
                                  const QColor& foreground,
                                  const QColor& scrollbarThumb,
                                  const QColor& scrollbarThumbForeground,
                                  const QColor& selectedRow,
                                  const QColor& selectedRowForeground,
                                  const QColor& outline,
                                  int cornerRadius);
    void setThemeSurfaceColor(const QColor& color, const QColor& outerColor = QColor());
    void setSession(Session* session);
    void setHistoryArrowNavigationEnabled(bool enabled);
    void setCursorPosition(int pos);
    void setText(const QString&);
    void stopAutoCalc();
    void stopAutoComplete();
    void wrapSelection();
    QString text() const;
    QStringList matchFragment(const QString&, bool unitContext = false) const;
    QString getKeyword() const;

signals:
    void autoCalcMessageAvailable(const QString&);
    void autoCalcQuantityAvailable(const Quantity&);
    void autoCalcDisabled();
    void controlPageDownPressed();
    void controlPageUpPressed();
    void copySequencePressed();
    void pageDownPressed();
    void pageUpPressed();
    void returnPressed();
    void escapePressed();
    void shiftDownPressed();
    void shiftUpPressed();
    void shiftPageDownPressed();
    void shiftPageUpPressed();
    void bulkEvaluationStarted();
    void bulkEvaluationFinished();

public slots:
    void autoCalcSelection(const QString& custom = QString());
    void cancelConstantCompletion();
    void evaluate();
    void decreaseFontPointSize();
    void increaseFontPointSize();
    void insert(const QString&);
    void insertConstant(const QString&);
    void rehighlight();
    void updateHistory();
    void refreshAutoCalc();

protected slots:
    void insertFromMimeData(const QMimeData*) override;
    void autoCalc();
    void autoComplete(const QString&);
    void checkAutoCalc();
    void checkAutoComplete();
    void checkMatching();
    void checkSelectionAutoCalc();
    void doMatchingLeft();
    void doMatchingPar();
    void doMatchingRight();
    void historyBack();
    void historyForward();
    void triggerAutoComplete();
    void triggerEnter();

protected:
    void changeEvent(QEvent*) override;
    bool event(QEvent*) override;
    void focusInEvent(QFocusEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    void inputMethodEvent(QInputMethodEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void scrollContentsBy(int, int) override;
    QSize sizeHint() const override;
    void wheelEvent(QWheelEvent*) override;

private:
    Q_DISABLE_COPY(Editor)
    friend class EditorCompletion;

    bool m_isAutoCalcEnabled;
    bool m_shouldBlockAutoCompletionOnce = false;
    bool m_isAutoCompletionEnabled;
    EditorCompletion* m_completion;
    QTimer* m_completionTimer;
    QString m_suppressedCompletionText;
    int m_suppressedCompletionPosition = -1;
    ConstantCompletion* m_constantCompletion;
    Evaluator* m_evaluator;
    SyntaxHighlighter* m_highlighter;
    QList<HistoryEntry> m_history;
    QString m_savedCurrentEditor;
    int m_currentHistoryIndex;
    QTimer* m_matchingTimer;
    QTimer* m_cursorBlinkTimer;
    bool m_customCursorVisible;
    bool m_themedCursorVisible = false;
    bool m_historyArrowNavigationEnabled;
    bool m_canScrollWrappedText = false;
    bool m_pendingDeadCaretPreedit = false;
    QColor m_themeSurfaceColor;
    QColor m_themeOuterSurfaceColor;
    QColor m_themePrimaryColor;
    QColor m_completionBackgroundColor;
    QColor m_completionForegroundColor;
    QColor m_completionScrollbarThumbColor;
    QColor m_completionScrollbarThumbForegroundColor;
    QColor m_completionSelectedRowColor;
    QColor m_completionSelectedRowForegroundColor;
    QColor m_completionOutlineColor;
    int m_completionCornerRadius = 0;
    std::optional<ColorScheme> m_themePreviewColorScheme;
    bool m_usePrimaryOutline = false;
    std::unique_ptr<Session> m_ownedSession;
    Session* m_session;

    void updateHeightForWrappedText();
    void updateHeightAndEnsureCursorVisible();
    void showThemedCursorAndRestartBlink();
    void hideThemedCursorAndStopBlink();
    bool shouldPaintThemedCursor() const;
    QRect themedCursorRect() const;
};

class EditorCompletion : public QObject {
    Q_OBJECT

public:
    EditorCompletion(Editor*);
    ~EditorCompletion();

    bool eventFilter(QObject*, QEvent*);
    bool isVisible() const;
    bool handleEditorKeyPress(QKeyEvent* event);
    bool handleEditorWheelEvent(QWheelEvent* event);
    void setThemeColors(const QColor& background,
                        const QColor& foreground,
                        const QColor& scrollbarThumb,
                        const QColor& scrollbarThumbForeground,
                        const QColor& selectedRow,
                        const QColor& selectedRowForeground,
                        const QColor& outline,
                        int cornerRadius);
    void showCompletion(const QStringList&);

signals:
    void selectedCompletion(const QString&);

public slots:
    void doneCompletion();
    void selectItem(const QString&);

private:
    Q_DISABLE_COPY(EditorCompletion)

    Editor* m_editor;
    QTreeWidget* m_popup;
    bool m_popupInteracted = false;
    QColor m_backgroundColor;
    QColor m_foregroundColor;
    QColor m_scrollbarThumbColor;
    QColor m_scrollbarThumbForegroundColor;
    QColor m_selectedRowColor;
    QColor m_selectedRowForegroundColor;
    QColor m_outlineColor;
    int m_cornerRadius = 0;

    bool handleCompletionKey(QKeyEvent* event);
    void applyThemeColors();
    void restoreEditorFocus();
};

class ConstantCompletion : public QObject {
    Q_OBJECT

public:
    ConstantCompletion(Editor*);
    ~ConstantCompletion();

    bool eventFilter(QObject*, QEvent*);
    void showCompletion();

signals:
    void canceledCompletion();
    void selectedCompletion(const QString&);

public slots:
    void doneCompletion();

protected slots:
    void setHorizontalPosition(int);
    void showCategory();
    void showConstants();

private:
    Q_DISABLE_COPY(ConstantCompletion)

    QTreeWidget* m_categoryWidget;
    QList<Constant> m_constantList;
    Editor* m_editor;
    QString m_lastDomain;
    QTreeWidget* m_constantWidget;
    QFrame* m_popup;
    QTimeLine* m_slider;
};

#endif
