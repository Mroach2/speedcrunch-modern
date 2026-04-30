// SPDX-FileCopyrightText: 2007-2010, 2013-2017, 2019-2020, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_EDITOR_H
#define GUI_EDITOR_H

#include "core/sessionhistory.h"

#include <QPlainTextEdit>

struct Constant;
class ConstantCompletion;
class EditorCompletion;
class Evaluator;
class Session;
class CNumber;
class SyntaxHighlighter;

class QEvent;
class QInputMethodEvent;
class QKeyEvent;
class QMimeData;
class QTimeLine;
class QTreeWidget;
class QWheelEvent;
class QWidget;
class QResizeEvent;

class Editor : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit Editor(QWidget* parent = nullptr);

    bool isAutoCalcEnabled() const;
    bool isAutoCompletionEnabled() const;
    void clearHistory();
    int cursorPosition() const;
    void doBackspace();
    void doDelete();
    char radixChar() const;
    void setAutoCalcEnabled(bool);
    void setAutoCompletionEnabled(bool);
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

    bool m_isAutoCalcEnabled;
    bool m_shouldBlockAutoCompletionOnce = false;
    bool m_isAutoCompletionEnabled;
    EditorCompletion* m_completion;
    QTimer* m_completionTimer;
    ConstantCompletion* m_constantCompletion;
    Evaluator* m_evaluator;
    SyntaxHighlighter* m_highlighter;
    QList<HistoryEntry> m_history;
    QString m_savedCurrentEditor;
    int m_currentHistoryIndex;
    QTimer* m_matchingTimer;
    bool m_shouldPaintCustomCursor;
    bool m_historyArrowNavigationEnabled;
    bool m_pendingDeadCaretPreedit = false;
    const Session * m_session;

    void updateHeightForWrappedText();
};

class EditorCompletion : public QObject {
    Q_OBJECT

public:
    EditorCompletion(Editor*);
    ~EditorCompletion();

    bool eventFilter(QObject*, QEvent*);
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
