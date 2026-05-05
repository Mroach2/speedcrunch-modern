// SPDX-FileCopyrightText: 2009-2010, 2013-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_SYNTAXHIGHLIGHTER_H
#define GUI_SYNTAXHIGHLIGHTER_H

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QSyntaxHighlighter>
#include <QTextBlockUserData>

class QPlainTextEdit;

class SyntaxHighlightBlockData : public QTextBlockUserData {
public:
    bool highlightResultExpressionSyntax = false;
};

class ColorScheme {
public:
    enum Role {
        Cursor,
        Number,
        Parens,
        List,
        Unit,
        Result,
        Comment,
        Matched,
        Function,
        Operator,
        Variable,
        ScrollBar,
        Separator,
        Background,
        EditorBackground
    };

    ColorScheme() : m_valid(false) { }
    ColorScheme(const QJsonDocument& doc);
    bool isValid() const { return m_valid; }
    QColor colorForRole(Role role) const;
    QJsonObject toJsonObject() const;

    static QStringList enumerate();
    static bool isBuiltInName(const QString& name);
    static QVector<QString> fileSystemSearchPaths();
    static QString filePathForName(const QString& name);
    static ColorScheme loadFromFile(const QString& path);
    static ColorScheme loadByName(const QString& name);
    static ColorScheme fromJsonObject(const QJsonObject& object);
    static QVector<QPair<QString, Role>> roleNames();

private:
    bool m_valid;
    QHash<Role, QColor> m_colors;
};

class SyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit SyntaxHighlighter(QPlainTextEdit*);

    void setColorScheme(ColorScheme&&);
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

    ColorScheme m_colorScheme;
};

#endif
