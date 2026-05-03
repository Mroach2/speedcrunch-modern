// SPDX-FileCopyrightText: 2009-2010, 2013-2016, 2018, 2021, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/syntaxhighlighter.h"

#include "core/evaluator.h"
#include "core/functions.h"
#include "core/mathdsl.h"
#include "core/settings.h"

#include <QtCore/QDir>
#include <QtCore/QJsonObject>
#include <QLatin1String>
#include <QApplication>
#include <QPalette>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QTextDocumentFragment>

static const constexpr auto COLOR_SCHEME_EXTENSION = "json";
static const constexpr auto DefaultColorSchemeName = "Terminal";

static QString textNormalizedForHighlighting(QString text)
{
    static const QHash<QChar, QChar> superscriptToAscii {
        {MathDsl::PowNeg, MathDsl::SubOpAl1}, // ⁻ SUPERSCRIPT MINUS.
        {MathDsl::Pow0, QLatin1Char('0')}, // ⁰
        {MathDsl::Pow1, QLatin1Char('1')}, // ¹
        {MathDsl::Pow2, QLatin1Char('2')}, // ²
        {MathDsl::Pow3, QLatin1Char('3')}, // ³
        {MathDsl::Pow4, QLatin1Char('4')}, // ⁴
        {MathDsl::Pow5, QLatin1Char('5')}, // ⁵
        {MathDsl::Pow6, QLatin1Char('6')}, // ⁶
        {MathDsl::Pow7, QLatin1Char('7')}, // ⁷
        {MathDsl::Pow8, QLatin1Char('8')}, // ⁸
        {MathDsl::Pow9, QLatin1Char('9')}, // ⁹
    };

    for (QChar& ch : text) {
        const QChar replacement = superscriptToAscii.value(ch, QChar::Null);
        if (!replacement.isNull())
            ch = replacement;
    }
    return text;
}

static bool isSuperscriptExponentChar(QChar ch)
{
    return ch == MathDsl::PowNeg || MathDsl::isSuperscriptDigit(ch);
}

static bool hasSuperscriptExponent(const QString& tokenText)
{
    for (const QChar ch : tokenText) {
        if (isSuperscriptExponentChar(ch))
            return true;
    }
    return false;
}

static QString stripTrailingAsciiDigits(QString text)
{
    while (!text.isEmpty() && text.at(text.size() - 1).isDigit())
        text.chop(1);
    return text;
}

static const QVector<QString> colorSchemeSearchPaths()
{
    static QVector<QString> searchPaths;
    if (searchPaths.isEmpty()) {
        // By only populating the paths in a function when they're used, we ensure all the QApplication
        // fields that are used by QStandardPaths are set.
        searchPaths.append(QString("%1/color-schemes").arg(Settings::getDataPath()));
        searchPaths.append(QStringLiteral(":/color-schemes"));
    }
    return searchPaths;
}

QColor getFallbackColor(ColorScheme::Role role)
{
    switch (role) {
    case ColorScheme::Background:
    case ColorScheme::EditorBackground:
        return QApplication::palette().color(QPalette::Base);
    default:
        return QApplication::palette().color(QPalette::Text);
    }
}

ColorScheme::ColorScheme(const QJsonDocument& doc)
    : m_valid(false)
{
    if (!doc.isObject())
        return;
    auto obj = doc.object();
    const auto roleEntries = roleNames();
    for (const auto& role : roleEntries) {
        auto v = obj.value(role.first);
        if (v.isUndefined())
            // Having a key missing is fine...
            continue;
        auto color = QColor(v.toString());
        if (!color.isValid())
            // ...having one that's not a color is not.
            return;
        m_colors.insert(role.second, color);
    }
    m_valid = true;
}

QColor ColorScheme::colorForRole(Role role) const
{
    QColor color = m_colors[role];
    if (!color.isValid())
        return getFallbackColor(role);
    else
        return color;
}

QStringList ColorScheme::enumerate()
{
    QMap<QString, void*> colorSchemes;
    for (auto& searchPath : colorSchemeSearchPaths()) {
        QDir dir(searchPath);
        dir.setFilter(QDir::Files | QDir::Readable);
        dir.setNameFilters({ QString("*.%1").arg(COLOR_SCHEME_EXTENSION) });
        const auto infoList = dir.entryInfoList();
        for (auto& info : infoList) // TODO: Use Qt 5.7's qAsConst().
            colorSchemes.insert(info.completeBaseName(), nullptr);
    }
    // Since this is a QMap, the keys are already sorted in ascending order.
    return colorSchemes.keys();
}

ColorScheme ColorScheme::loadFromFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return ColorScheme();
    // TODO: Better error handling.
    return ColorScheme(QJsonDocument::fromJson(file.readAll()));
}

ColorScheme ColorScheme::loadByName(const QString& name)
{
    for (auto& path : colorSchemeSearchPaths()) {
        auto fileName = QString("%1/%2.%3").arg(path, name, COLOR_SCHEME_EXTENSION);
        auto colorScheme = loadFromFile(fileName);
        if (colorScheme.isValid())
            return colorScheme;
    }
    return ColorScheme();
}

QJsonObject ColorScheme::toJsonObject() const
{
    QJsonObject object;
    const auto roleEntries = roleNames();
    for (const auto& roleEntry : roleEntries)
        object.insert(roleEntry.first, colorForRole(roleEntry.second).name());
    return object;
}

ColorScheme ColorScheme::fromJsonObject(const QJsonObject& object)
{
    return ColorScheme(QJsonDocument(object));
}

QVector<QPair<QString, ColorScheme::Role>> ColorScheme::roleNames()
{
    return {
        { QStringLiteral("cursor"), ColorScheme::Cursor },
        { QStringLiteral("number"), ColorScheme::Number },
        { QStringLiteral("parens"), ColorScheme::Parens },
        { QStringLiteral("result"), ColorScheme::Result },
        { QStringLiteral("comment"), ColorScheme::Comment },
        { QStringLiteral("matched"), ColorScheme::Matched },
        { QStringLiteral("function"), ColorScheme::Function },
        { QStringLiteral("operator"), ColorScheme::Operator },
        { QStringLiteral("variable"), ColorScheme::Variable },
        { QStringLiteral("scrollbar"), ColorScheme::ScrollBar },
        { QStringLiteral("separator"), ColorScheme::Separator },
        { QStringLiteral("background"), ColorScheme::Background },
        { QStringLiteral("editorbackground"), ColorScheme::EditorBackground },
    };
}


SyntaxHighlighter::SyntaxHighlighter(QPlainTextEdit* edit)
    : QSyntaxHighlighter(edit)
{
    setDocument(edit->document());
    update();
}

void SyntaxHighlighter::setColorScheme(ColorScheme&& colorScheme) {
    m_colorScheme = colorScheme;
}

void SyntaxHighlighter::highlightBlock(const QString& text)
{
    // Default color for the text
    setFormat(0, text.length(), colorForRole(ColorScheme::Number));

    if (!Settings::instance()->syntaxHighlighting)
        return;

    if (text.startsWith(QLatin1String("="))) {
        setFormat(0, 1, colorForRole(ColorScheme::Operator));
        setFormat(1, text.length(), colorForRole(ColorScheme::Result));
        if (Settings::instance()->digitGrouping > 0) {
            // Use token-based grouping for result lines as well, so
            // integer-only grouping works when lexer splits around radix chars.
            const Tokens tokens = Evaluator::instance()->scan(text);
            for (int i = 0; i < tokens.count(); ++i) {
                const Token& token = tokens.at(i);
                if (token.type() != Token::stxNumber)
                    continue;

                if (Settings::instance()->digitGroupingIntegerPartOnly
                        && i > 0
                        && !tokens.at(i - 1).text().isEmpty()
                        && (tokens.at(i - 1).type() == Token::stxSep
                            || tokens.at(i - 1).type() == Token::stxOperator)
                        && Evaluator::isRadixChar(tokens.at(i - 1).text().at(0).unicode())) {
                    continue;
                }

                groupDigits(text, token.pos(), token.size());
            }
        }
        return;
    }

    int questionMarkIndex = text.indexOf(MathDsl::CommentSep);
    if (questionMarkIndex != -1)
        setFormat(questionMarkIndex, text.length(), colorForRole(ColorScheme::Comment));

    const QString normalizedText = textNormalizedForHighlighting(text);
    Tokens tokens = Evaluator::instance()->scan(normalizedText);
    int unitBracketDepth = 0;

    for (int i = 0; i < tokens.count(); ++i) {
        const Token& token = tokens.at(i);
        const QString tokenText = token.text().toLower();
        QStringList functionNames = FunctionRepo::instance()->getIdentifiers();
        QColor color;
        const bool insideUnitBrackets = unitBracketDepth > 0;

        switch (token.type()) {
        case Token::stxNumber:
        case Token::stxUnknown:
            color = colorForRole(ColorScheme::Number);
            if (insideUnitBrackets)
                color = colorForRole(ColorScheme::Parens);
            // TODO: color thousand separators differently? It might help troubleshooting issues
            break;

        case Token::stxOperator:
            color = colorForRole(ColorScheme::Operator);
            if (insideUnitBrackets)
                color = colorForRole(ColorScheme::Parens);
            break;

        case Token::stxSep:
            color = colorForRole(ColorScheme::Separator);
            if (insideUnitBrackets)
                color = colorForRole(ColorScheme::Parens);
            break;

        case Token::stxOpenPar:
        case Token::stxClosePar:
            color = colorForRole(ColorScheme::Parens);
            break;

        case Token::stxIdentifier:
            color = colorForRole(ColorScheme::Variable);
            if (Evaluator::instance()->hasUserFunction(token.text())
                || functionNames.contains(tokenText, Qt::CaseInsensitive))
                color = colorForRole(ColorScheme::Function);
            else if (i + 1 < tokens.count()
                     && tokens.at(i + 1).type() == Token::stxOpenPar
                     && token.pos() >= 0
                     && token.pos() + token.size() <= text.size()) {
                const QString originalTokenText = text.mid(token.pos(), token.size());
                if (hasSuperscriptExponent(originalTokenText)) {
                    const QString baseName = stripTrailingAsciiDigits(token.text());
                    if (Evaluator::instance()->hasUserFunction(baseName)
                        || functionNames.contains(baseName.toLower(), Qt::CaseInsensitive)) {
                        color = colorForRole(ColorScheme::Function);
                    }
                }
            }
            break;

        case Token::stxUnitIdentifier:
            color = colorForRole(ColorScheme::Parens);
            break;

        default:
            break;
        };

        setFormat(token.pos(), token.size(), color);
        if ((token.type() == Token::stxIdentifier
             || token.type() == Token::stxUnitIdentifier)
                && token.pos() >= 0
                && token.pos() + token.size() <= text.size()) {
            const QString originalTokenText = text.mid(token.pos(), token.size());
            for (int j = 0; j < originalTokenText.size(); ++j) {
                if (isSuperscriptExponentChar(originalTokenText.at(j)))
                    setFormat(token.pos() + j, 1, token.type() == Token::stxUnitIdentifier
                        ? colorForRole(ColorScheme::Parens)
                        : colorForRole(ColorScheme::Number));
            }
        }

        if (token.type() == Token::stxOpenPar && token.text() == QLatin1String("["))
            ++unitBracketDepth;
        else if (token.type() == Token::stxClosePar && token.text() == QLatin1String("]"))
            unitBracketDepth = qMax(0, unitBracketDepth - 1);

        if (token.type() == Token::stxNumber && Settings::instance()->digitGrouping > 0) {
            // If the lexer split a decimal number around the radix character,
            // avoid grouping the fractional token when integer-only grouping is enabled.
            if (Settings::instance()->digitGroupingIntegerPartOnly
                    && i > 0
                    && !tokens.at(i - 1).text().isEmpty()
                    && (tokens.at(i - 1).type() == Token::stxSep
                        || tokens.at(i - 1).type() == Token::stxOperator)
                    && Evaluator::isRadixChar(tokens.at(i - 1).text().at(0).unicode())) {
                continue;
            }
            groupDigits(text, token.pos(), token.size());
        }
    }

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == MathDsl::ListStart || ch == MathDsl::ListEnd)
            setFormat(i, 1, colorForRole(ColorScheme::Parens));
    }
}

void SyntaxHighlighter::update()
{
    const Settings* settings = Settings::instance();
    const QString name = settings->colorScheme;
    if (name == QLatin1String("Custom")) {
        const QJsonDocument customDoc = QJsonDocument::fromJson(settings->customColorSchemeJson.toUtf8());
        ColorScheme customScheme(customDoc);
        if (customScheme.isValid())
            setColorScheme(std::move(customScheme));
        else
            setColorScheme(ColorScheme::loadByName(DefaultColorSchemeName));
    } else {
        setColorScheme(ColorScheme::loadByName(name));
    }

    QColor backgroundColor = colorForRole(ColorScheme::Background);
    QWidget* parentWidget = static_cast<QWidget*>(parent());
    QPalette pal = parentWidget->palette();
    pal.setColor(QPalette::Active, QPalette::Base, backgroundColor);
    pal.setColor(QPalette::Inactive, QPalette::Base, backgroundColor);
    parentWidget->setPalette(pal);

    rehighlight();
}

void SyntaxHighlighter::formatDigitsGroup(const QString& text, int start, int end, bool invert, int size)
{
    Q_ASSERT(start <= end);
    Q_ASSERT(size > 0);

    qreal spacing = 100; // Size of the space between groups (100 means no space).
    spacing += 40 * Settings::instance()->digitGrouping;
    int inc = !invert ? -1 : 1;
    if(!invert)
    {
        int tmp = start;
        start = end - 1;
        end = tmp - 1;

        // Skip the first digit so that we add the spacing to the first digit of the next group.
        while (start != end && Evaluator::isSeparatorChar(text[start].unicode()))
            --start;
        if (start == end)
            return; // Bug ?
        --start;
    }

    for (int count = 0 ; start != end ; start += inc)
    {
        // When there are separators in the number, we must not count them as part of the group.
        if (!Evaluator::isSeparatorChar(text[start].unicode()))
        {
            ++count;
            if (count == size)
            {
                // Only change the letter spacing from the format and keep the other properties.
                QTextCharFormat fmt = format(start);
                fmt.setFontLetterSpacing(spacing);
                setFormat(start, 1, fmt);
                count = 0; // Reset
                // TODO: if the next character is a separator, do not add spacing?
            }
        }
    }
}

void SyntaxHighlighter::groupDigits(const QString& text, int pos, int length)
{
    // Used to find out which characters belong to which radixes.
    static int charType[128] = { 0 };
    static const int BIN_CHAR = (1 << 0);
    static const int OCT_CHAR = (1 << 1);
    static const int DEC_CHAR = (1 << 2);
    static const int HEX_CHAR = (1 << 3);

    if (charType[int('0')] == 0) { // Initialize the table on first call (not thread-safe!).
        charType[int('0')] = HEX_CHAR | DEC_CHAR | OCT_CHAR | BIN_CHAR;
        charType[int('1')] = HEX_CHAR | DEC_CHAR | OCT_CHAR | BIN_CHAR;
        charType[int('2')] = HEX_CHAR | DEC_CHAR | OCT_CHAR;
        charType[int('3')] = HEX_CHAR | DEC_CHAR | OCT_CHAR;
        charType[int('4')] = HEX_CHAR | DEC_CHAR | OCT_CHAR;
        charType[int('5')] = HEX_CHAR | DEC_CHAR | OCT_CHAR;
        charType[int('6')] = HEX_CHAR | DEC_CHAR | OCT_CHAR;
        charType[int('7')] = HEX_CHAR | DEC_CHAR | OCT_CHAR;
        charType[int('8')] = HEX_CHAR | DEC_CHAR;
        charType[int('9')] = HEX_CHAR | DEC_CHAR;
        charType[int('a')] = HEX_CHAR;
        charType[int('b')] = HEX_CHAR;
        charType[int('c')] = HEX_CHAR;
        charType[int('d')] = HEX_CHAR;
        charType[int('e')] = HEX_CHAR;
        charType[int('f')] = HEX_CHAR;
        charType[int('A')] = HEX_CHAR;
        charType[int('B')] = HEX_CHAR;
        charType[int('C')] = HEX_CHAR;
        charType[int('D')] = HEX_CHAR;
        charType[int('E')] = HEX_CHAR;
        charType[int('F')] = HEX_CHAR;
    }

    int s = -1; // Index of the first digit (most significant).
    int radixPos = -1; // Index of radix char for the current number, if any.
    bool invertGroup = false; // If true, group digits from the most significant digit.
    int groupSize = 3; // Number of digits to group (depends on the radix).
    int allowedChars = DEC_CHAR; // Allowed characters for the radix of the current number being parsed.

    int endPos = pos + length;
    if (endPos > text.length())
        endPos = text.length();
    for (int i = pos; i < endPos; ++i) {
        ushort c = text[i].unicode();
        bool isDigit = c < 128 && (charType[c] & allowedChars);

        if (s >= 0) {
            if (!isDigit) {
                bool endOfNumber = true;
                if (Evaluator::isRadixChar(c) && i < endPos - 1) {
                    // Radix characters keep integer and fractional digits in the same number.
                    ushort nextC = text[i + 1].unicode();
                    if (nextC < 128 && (charType[nextC] & allowedChars))
                        endOfNumber = false;
                }
                // If this is a separator and next character is a digit or a separator,
                // the next character is part of the same number expression
                if (Evaluator::isSeparatorChar(c) && i<endPos-1) {
                    ushort nextC = text[i+1].unicode();
                    if ((nextC < 128 && (charType[nextC] & allowedChars))
                         || Evaluator::isSeparatorChar(nextC))
                        endOfNumber = false;
                }

                if (c == MathDsl::TimeSep || c == MathDsl::Deg
                        || c == MathDsl::ArcminOp || c == MathDsl::ArcsecOp
                        || c == MathDsl::ArcminOpAl1 || c == MathDsl::ArcsecOpAl1)
                    endOfNumber = true;

                if (endOfNumber) {
                    // End of current number found, start grouping the digits.
                    if (radixPos >= 0 && Settings::instance()->digitGroupingIntegerPartOnly)
                        formatDigitsGroup(text, s, radixPos, false, groupSize);
                    else
                        formatDigitsGroup(text, s, i, invertGroup, groupSize);
                    s = -1; // Reset.
                    radixPos = -1; // Reset.
                }
            }
        } else {
            if (isDigit) { // Start of number found.
                s = i;
                radixPos = -1;
            }
        }

        if (!isDigit) {
            if (Evaluator::isRadixChar(c)) {
                if (Settings::instance()->digitGroupingIntegerPartOnly)
                    radixPos = i;
                else
                    invertGroup = true; // Invert the grouping for the fractional part.
            } else if (!Evaluator::isSeparatorChar(c)){
                // Look for a radix prefix.
                invertGroup = false;
                radixPos = -1;
                if (i > 0 && text[i - 1] == '0') {
                    if (c == 'x') {
                        groupSize = 4;
                        allowedChars = HEX_CHAR;
                    } else if (c == 'o') {
                        groupSize = 3;
                        allowedChars = OCT_CHAR;
                    } else if (c == 'b') {
                        groupSize = 4;
                        allowedChars = BIN_CHAR;
                    } else {
                        groupSize = 3;
                        allowedChars = DEC_CHAR;
                    }
                } else {
                    groupSize = 3;
                    allowedChars = DEC_CHAR;
                }
            }
        }
    }

    // Group the last digits if the string finishes with the number.
    if (s >= 0) {
        if (radixPos >= 0 && Settings::instance()->digitGroupingIntegerPartOnly)
            formatDigitsGroup(text, s, radixPos, false, groupSize);
        else
            formatDigitsGroup(text, s, endPos, invertGroup, groupSize);
    }
}


// Original code snippet from StackOverflow.
// http://stackoverflow.com/questions/15280452/how-can-i-get-highlighted-text-from-a-qsyntaxhighlighter-into-an-html-string
void SyntaxHighlighter::asHtml(QString& html)
{
    // Create a new document from all the selected text document.
    QTextCursor cursor(document());
    cursor.select(QTextCursor::Document);
    QTextDocument* tempDocument(new QTextDocument);
    Q_ASSERT(tempDocument);
    QTextCursor tempCursor(tempDocument);

    tempCursor.insertFragment(cursor.selection());
    tempCursor.select(QTextCursor::Document);
    // Set the default foreground for the inserted characters.
    QTextCharFormat textfmt = tempCursor.charFormat();
    textfmt.setForeground(Qt::gray);
    tempCursor.setCharFormat(textfmt);

    // Apply the additional formats set by the syntax highlighter
    QTextBlock start = document()->findBlock(cursor.selectionStart());
    QTextBlock end = document()->findBlock(cursor.selectionEnd());
    end = end.next();
    const int selectionStart = cursor.selectionStart();
    const int endOfDocument = tempDocument->characterCount() - 1;
    for(QTextBlock current = start; current.isValid() && current != end; current = current.next()) {
        const QTextLayout* layout(current.layout());

        foreach(const QTextLayout::FormatRange &range, layout->formats()) {
            const int start = current.position() + range.start - selectionStart;
            const int end = start + range.length;
            if(end <= 0 || start >= endOfDocument)
                continue;
            tempCursor.setPosition(qMax(start, 0));
            tempCursor.setPosition(qMin(end, endOfDocument), QTextCursor::KeepAnchor);
            tempCursor.setCharFormat(range.format);
        }
    }

    // Reset the user states since they are not interesting
    for(QTextBlock block = tempDocument->begin(); block.isValid(); block = block.next())
        block.setUserState(-1);

    // Make sure the text appears pre-formatted, and set the background we want.
    tempCursor.select(QTextCursor::Document);
    QTextBlockFormat blockFormat = tempCursor.blockFormat();
    blockFormat.setNonBreakableLines(true);
    blockFormat.setBackground(colorForRole(ColorScheme::Background));
    tempCursor.setBlockFormat(blockFormat);

    // Finally retreive the syntax higlighted and formatted html.
    html = tempCursor.selection().toHtml();
    delete tempDocument;

    // Inject CSS, so to avoid a white margin
    html.replace("<head>", QString("<head> <style> body {background-color: %1;}</style>")
                                    .arg(colorForRole(ColorScheme::Background).name()));
}
