// SPDX-FileCopyrightText: 2009-2010, 2013-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_SYNTAXHIGHLIGHTER_H
#define GUI_SYNTAXHIGHLIGHTER_H

#include "core/colorscheme.h"
#include <QSyntaxHighlighter>
#include <QTextBlockUserData>

class QPlainTextEdit;
class Evaluator;

class SyntaxHighlightBlockData : public QTextBlockUserData {
public:
    bool highlightResultExpressionSyntax = false;
};

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit SyntaxHighlighter(QPlainTextEdit*);

    void setColorScheme(ColorScheme&&);
    void setEvaluator(const Evaluator* evaluator);
    QColor colorForRole(ColorScheme::Role role) const { return m_colorScheme.colorForRole(role); }

    void update();
    virtual void highlightBlock(const QString&);
    void asHtml(QString& html);

private:
    Q_DISABLE_COPY(SyntaxHighlighter)
    SyntaxHighlighter();
    SyntaxHighlighter(QObject*);
    SyntaxHighlighter(QTextDocument*);
    void groupDigits(const QString& text, int pos, int length);
    void formatDigitsGroup(const QString& text, int start, int end, bool invert, int size);
    const Evaluator* evaluator() const;

    ColorScheme m_colorScheme;
    const Evaluator* m_evaluator;
};

#endif
