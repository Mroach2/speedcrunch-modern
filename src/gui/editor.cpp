// SPDX-FileCopyrightText: 2007-2010, 2012-2019, 2021, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/editor.h"
#include "gui/displayformatutils.h"
#include "gui/editorutils.h"
#include "gui/functiontooltiputils.h"
#include "gui/resultlineformatutils.h"
#include "gui/syntaxhighlighter.h"
#include "core/constants.h"
#include "core/evaluator.h"
#include "core/functions.h"
#include "core/numberformatter.h"
#include "core/regexpatterns.h"
#include "core/settings.h"
#include "core/unitdisplayformat.h"
#include "core/session.h"
#include "core/unicodechars.h"
#include "core/units.h"
#include "core/mathdsl.h"

#include <QApplication>
#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScreen>
#include <QScrollBar>
#include <QRegularExpression>
#include <QStyle>
#include <QResizeEvent>
#include <QTimeLine>
#include <QTimer>
#include <QTextBlock>
#include <QTextLayout>
#include <QTreeWidget>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

static void moveCursorToEnd(Editor* editor)
{
    QTextCursor cursor = editor->textCursor();
    cursor.movePosition(QTextCursor::EndOfBlock);
    editor->setTextCursor(cursor);
}

static bool isOperatorOnlyIncompleteInput(const QString& expression)
{
    const QString trimmed = expression.trimmed();
    if (trimmed.isEmpty())
        return false;

    bool sawOperator = false;
    for (int i = 0; i < trimmed.size(); ++i) {
        const QChar ch = trimmed.at(i);
        if (ch.isSpace())
            continue;
        if (ch == MathDsl::AddOp || MathDsl::isSubtractionOperatorAlias(ch)) {
            sawOperator = true;
            continue;
        }
        return false;
    }

    return sawOperator;
}

static QString normalizeExpressionTypedInEditor(QString text)
{
    return EditorUtils::normalizeExpressionOperatorsForEditorInput(text);
}

static bool isInsideUnmatchedSquareBracketContext(const QString& text, int cursorPosition)
{
    int squareBracketDepth = 0;
    const int safeCursorPosition = qBound(0, cursorPosition, text.size());
    for (int i = 0; i < safeCursorPosition; ++i) {
        const QChar ch = text.at(i);
        if (ch == MathDsl::UnitStart) {
            ++squareBracketDepth;
            continue;
        }
        if (ch == MathDsl::UnitEnd && squareBracketDepth > 0)
            --squareBracketDepth;
    }
    return squareBracketDepth > 0;
}

static bool isTypedMultiplicationCharacter(const QChar& ch)
{
    return MathDsl::isMultiplicationOperator(ch)
           || MathDsl::isMultiplicationOperatorAlias(ch, true);
}

static bool isGroupedSpacedOperator(QChar leftSpace, QChar sign, QChar rightSpace)
{
    const auto matches = [leftSpace, sign, rightSpace](QChar expectedSpace, QChar expectedSign) {
        return leftSpace == expectedSpace
               && sign == expectedSign
               && rightSpace == expectedSpace;
    };

    return matches(MathDsl::MulDotWrapSp, MathDsl::MulDotOp)
           || matches(MathDsl::MulCrossWrapSp, MathDsl::MulCrossOp)
           || matches(MathDsl::AddWrap, MathDsl::AddOp)
           || matches(MathDsl::SubWrapSp, MathDsl::SubOp)
           || matches(MathDsl::DivWrap, MathDsl::DivOp)
           || matches(QLatin1Char(' '), MathDsl::Equals)
           || matches(MathDsl::SubWrapSp, MathDsl::TransOp)
           || matches(QLatin1Char(' '), MathDsl::CommentSep);
}

static QString wrappedShiftLeftToken()
{
    return MathDsl::buildWrappedToken(MathDsl::ShiftLeftOp);
}

static QString wrappedShiftRightToken()
{
    return MathDsl::buildWrappedToken(MathDsl::ShiftRightOp);
}

static bool isAfterGroupedSpacedOperator(const QString& text, int cursorPosition)
{
    if (cursorPosition > text.size())
        return false;
    const QString shiftLeft = wrappedShiftLeftToken();
    const QString shiftRight = wrappedShiftRightToken();
    if (cursorPosition >= shiftLeft.size()
        && text.mid(cursorPosition - shiftLeft.size(), shiftLeft.size()) == shiftLeft) {
        return true;
    }
    if (cursorPosition >= shiftRight.size()
        && text.mid(cursorPosition - shiftRight.size(), shiftRight.size()) == shiftRight) {
        return true;
    }
    if (cursorPosition < 3)
        return false;

    const QChar leftSpace = text.at(cursorPosition - 3);
    const QChar sign = text.at(cursorPosition - 2);
    const QChar rightSpace = text.at(cursorPosition - 1);

    // Keep display-formatted binary operators atomic for Backspace when the
    // cursor is exactly after "<space><operator><space>".
    return isGroupedSpacedOperator(leftSpace, sign, rightSpace);
}

static bool isBeforeGroupedSpacedOperator(const QString& text, int cursorPosition)
{
    if (cursorPosition < 0 || cursorPosition > text.size())
        return false;
    const QString shiftLeft = wrappedShiftLeftToken();
    const QString shiftRight = wrappedShiftRightToken();
    if (cursorPosition + shiftLeft.size() <= text.size()
        && text.mid(cursorPosition, shiftLeft.size()) == shiftLeft) {
        return true;
    }
    if (cursorPosition + shiftRight.size() <= text.size()
        && text.mid(cursorPosition, shiftRight.size()) == shiftRight) {
        return true;
    }
    if (cursorPosition + 3 > text.size())
        return false;

    const QChar leftSpace = text.at(cursorPosition);
    const QChar sign = text.at(cursorPosition + 1);
    const QChar rightSpace = text.at(cursorPosition + 2);

    return isGroupedSpacedOperator(leftSpace, sign, rightSpace);
}

static int groupedSpacedOperatorStartAround(const QString& text, int cursorPosition)
{
    // Detect whether the cursor is currently at/inside a grouped
    // "<space><operator><space>" triplet and return its first index.
    // We probe up to two chars to the left so positions on the sign or
    // right-space are still recognized as belonging to the same group.
    if (isBeforeGroupedSpacedOperator(text, cursorPosition))
        return cursorPosition;
    if (cursorPosition > 0 && isBeforeGroupedSpacedOperator(text, cursorPosition - 1))
        return cursorPosition - 1;
    if (cursorPosition > 1 && isBeforeGroupedSpacedOperator(text, cursorPosition - 2))
        return cursorPosition - 2;
    if (cursorPosition > 2 && isBeforeGroupedSpacedOperator(text, cursorPosition - 3))
        return cursorPosition - 3;
    return -1;
}

static bool isLeadingQuestionCommentToken(const QString& text)
{
    return text.size() >= 2
           && text.at(0) == MathDsl::CommentSep
           && text.at(1) == QLatin1Char(' ');
}

static int groupedTokenLengthBefore(const QString& text, int cursorPosition)
{
    const QString shiftLeft = wrappedShiftLeftToken();
    const QString shiftRight = wrappedShiftRightToken();
    if (cursorPosition >= shiftLeft.size()
        && cursorPosition <= text.size()
        && text.mid(cursorPosition - shiftLeft.size(), shiftLeft.size()) == shiftLeft) {
        return shiftLeft.size();
    }
    if (cursorPosition >= shiftRight.size()
        && cursorPosition <= text.size()
        && text.mid(cursorPosition - shiftRight.size(), shiftRight.size()) == shiftRight) {
        return shiftRight.size();
    }
    if (isAfterGroupedSpacedOperator(text, cursorPosition))
        return 3;
    if (cursorPosition >= 2
        && cursorPosition <= text.size()
        && cursorPosition - 2 == 0
        && isLeadingQuestionCommentToken(text)) {
        return 2;
    }
    return 0;
}

static int groupedTokenLengthAfter(const QString& text, int cursorPosition)
{
    const QString shiftLeft = wrappedShiftLeftToken();
    const QString shiftRight = wrappedShiftRightToken();
    if (cursorPosition >= 0
        && cursorPosition + shiftLeft.size() <= text.size()
        && text.mid(cursorPosition, shiftLeft.size()) == shiftLeft) {
        return shiftLeft.size();
    }
    if (cursorPosition >= 0
        && cursorPosition + shiftRight.size() <= text.size()
        && text.mid(cursorPosition, shiftRight.size()) == shiftRight) {
        return shiftRight.size();
    }
    if (isBeforeGroupedSpacedOperator(text, cursorPosition))
        return 3;
    if (cursorPosition == 0 && isLeadingQuestionCommentToken(text))
        return 2;
    return 0;
}

static int groupedTokenStartAround(const QString& text, int cursorPosition, int* tokenLength)
{
    const QString shiftLeft = wrappedShiftLeftToken();
    const QString shiftRight = wrappedShiftRightToken();
    for (int delta = 0; delta <= 3; ++delta) {
        const int start = cursorPosition - delta;
        if (start < 0)
            continue;
        if (start + shiftLeft.size() <= text.size()
            && text.mid(start, shiftLeft.size()) == shiftLeft) {
            if (tokenLength)
                *tokenLength = shiftLeft.size();
            return start;
        }
        if (start + shiftRight.size() <= text.size()
            && text.mid(start, shiftRight.size()) == shiftRight) {
            if (tokenLength)
                *tokenLength = shiftRight.size();
            return start;
        }
    }

    const int tripletStart = groupedSpacedOperatorStartAround(text, cursorPosition);
    if (tripletStart >= 0) {
        if (tokenLength)
            *tokenLength = 3;
        return tripletStart;
    }

    if (isLeadingQuestionCommentToken(text)
        && cursorPosition >= 0
        && cursorPosition <= 2) {
        if (tokenLength)
            *tokenLength = 2;
        return 0;
    }

    return -1;
}

static bool isRightOfOpeningSquareBracketWithOnlySpaces(const QString& text, int cursorPosition)
{
    int i = qBound(0, cursorPosition, text.size()) - 1;
    while (i >= 0 && text.at(i).isSpace())
        --i;
    return i >= 0 && text.at(i) == MathDsl::UnitStart;
}

static bool isCurrentUnitContextEmptyOrOnlySpaces(const QString& text, int cursorPosition)
{
    const int safeCursor = qBound(0, cursorPosition, text.size());
    int depth = 0;
    int openPos = -1;
    for (int i = 0; i < safeCursor; ++i) {
        const QChar ch = text.at(i);
        if (ch == MathDsl::UnitStart) {
            ++depth;
            openPos = i;
        } else if (ch == MathDsl::UnitEnd && depth > 0) {
            --depth;
            if (depth == 0)
                openPos = -1;
        }
    }

    if (depth <= 0 || openPos < 0)
        return false;

    for (int i = openPos + 1; i < safeCursor; ++i) {
        if (!text.at(i).isSpace())
            return false;
    }
    return true;
}

static bool hasOnlySpacesToRight(const QString& text, int cursorPosition)
{
    const int safeCursor = qBound(0, cursorPosition, text.size());
    for (int i = safeCursor; i < text.size(); ++i) {
        if (!text.at(i).isSpace())
            return false;
    }
    return true;
}

static bool isAnyOperatorKey(int key)
{
    return key == Qt::Key_Plus
           || key == Qt::Key_Minus
           || key == Qt::Key_Asterisk
           || key == Qt::Key_Slash
           || key == Qt::Key_division
           || key == Qt::Key_Percent
           || key == Qt::Key_AsciiCircum
           || key == Qt::Key_Ampersand
           || key == Qt::Key_Bar
           || key == Qt::Key_Equal
           || key == Qt::Key_Less
           || key == Qt::Key_Greater;
}

static bool isAnyMultiplicationOperator(const QChar& ch)
{
    return ch == MathDsl::MulDotOp
           || ch == MathDsl::MulCrossOp
           || MathDsl::isMultiplicationOperatorAlias(ch, true);
}

static bool isAnyAdditionOperator(const QChar& ch)
{
    // Treat both normalized '+' and accepted plus aliases as one operator
    // class for insertion guards (prevents consecutive plus insertion).
    return ch == MathDsl::AddOp
           || MathDsl::isAdditionOperatorAlias(ch);
}

static QChar normalizedTypedCharFromEvent(const QKeyEvent* event, const QString& normalizedEventText)
{
    if (normalizedEventText.size() == 1)
        return normalizedEventText.at(0);

    if (event->text().size() == 1) {
        const QChar raw = event->text().at(0);
        if (MathDsl::isAdditionOperatorAlias(raw))
            return MathDsl::AddOp;
        if (MathDsl::isSubtractionOperatorAlias(raw))
            return MathDsl::SubOp;
        if (MathDsl::isDivisionOperatorAlias(raw))
            return MathDsl::DivOp;
        if (MathDsl::isMultiplicationOperatorAlias(raw, true))
            return raw == MathDsl::MulDotOp ? MathDsl::MulDotOp : MathDsl::MulCrossOp;
        return raw;
    }

    switch (event->key()) {
    case Qt::Key_Plus: return MathDsl::AddOp;
    case Qt::Key_Equal:
        return (event->modifiers() & Qt::ShiftModifier) ? MathDsl::AddOp : MathDsl::Equals;
    case Qt::Key_Minus: return MathDsl::SubOp;
    case Qt::Key_Slash: return MathDsl::DivOp;
    case Qt::Key_Asterisk: return MathDsl::MulCrossOp;
    case Qt::Key_AsciiCircum: return MathDsl::PowOp;
    case Qt::Key_ParenLeft: return MathDsl::GroupStart;
    default: return QChar();
    }
}

static void replacePreviousOperatorAtCursorWithCaret(Editor* editor)
{
    QTextCursor cursor = editor->textCursor();
    const QString text = editor->text();
    const int pos = cursor.position();
    if (pos <= 0)
        return;

    int signPos = pos - 1;
    while (signPos >= 0 && text.at(signPos).isSpace())
        --signPos;
    if (signPos < 0 || !isAnyMultiplicationOperator(text.at(signPos)))
        return;

    int start = signPos;
    int end = signPos + 1;

    if (signPos > 0 && text.at(signPos - 1).isSpace())
        start = signPos - 1;
    if (signPos + 1 < pos && text.at(signPos + 1).isSpace())
        end = signPos + 2;

    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.insertText(QString::fromUtf8("^"));
    editor->setTextCursor(cursor);
}

static void replacePreviousSubtractionAtCursorWithUnitConversion(Editor* editor)
{
    QTextCursor cursor = editor->textCursor();
    const QString text = editor->text();
    const int pos = cursor.position();
    if (pos <= 0)
        return;

    int signPos = pos - 1;
    while (signPos >= 0 && text.at(signPos).isSpace())
        --signPos;
    if (signPos < 0 || !MathDsl::isSubtractionOperatorAlias(text.at(signPos)))
        return;

    int start = signPos;
    int end = signPos + 1;

    if (signPos > 0 && text.at(signPos - 1).isSpace())
        start = signPos - 1;
    if (signPos + 1 < pos && text.at(signPos + 1).isSpace())
        end = signPos + 2;

    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    const QString insertion =
        MathDsl::buildWrappedToken(MathDsl::TransOp, MathDsl::SubWrapSp)
        + QStringLiteral("[]");
    cursor.insertText(insertion);
    cursor.setPosition(start + insertion.size() - 1);
    editor->setTextCursor(cursor);
}

static bool canEndUnitConversionLeftOperand(const QChar& ch)
{
    return ch.isLetterOrNumber()
        || ch == MathDsl::GroupEnd
        || ch == MathDsl::UnitEnd
        || ch == MathDsl::Deg
        || ch == UnicodeChars::MasculineOrdinalIndicator
        || ch == UnicodeChars::Prime
        || ch == UnicodeChars::DoublePrime;
}

static bool isBlockingBinaryOperator(const QChar& ch)
{
    return isAnyAdditionOperator(ch)
        || MathDsl::isSubtractionOperatorAlias(ch)
        || MathDsl::isDivisionOperatorAlias(ch)
        || isAnyMultiplicationOperator(ch)
        || ch == MathDsl::PowOp
        || ch == MathDsl::PercentOp
        || ch == MathDsl::BitAndOp
        || ch == MathDsl::BitOrOp
        || ch == MathDsl::Equals
        || ch == MathDsl::LessThanOp
        || ch == MathDsl::GreaterThanOp;
}

static void insertUnitConversionAtCursorWithUnitPlaceholder(Editor* editor)
{
    QTextCursor cursor = editor->textCursor();
    const int start = cursor.position();
    QString insertion =
        MathDsl::buildWrappedToken(MathDsl::TransOp, MathDsl::SubWrapSp)
        + QStringLiteral("[]");
    if (start > 0 && editor->text().at(start - 1).isSpace() && insertion.at(0).isSpace())
        insertion.remove(0, 1);
    cursor.insertText(insertion);
    cursor.setPosition(start + insertion.size() - 1);
    editor->setTextCursor(cursor);
}

static bool textContainsOnlyAdditionAliases(const QString& text)
{
    if (text.isEmpty())
        return false;
    for (const QChar ch : text) {
        if (!MathDsl::isAdditionOperatorAlias(ch))
            return false;
    }
    return true;
}

static bool isCaretOperatorAlias(const QChar& ch)
{
    // Caret may arrive as ASCII '^' or as layout/IME-specific variants
    // (for example PT dead-key composition). Keep them equivalent here.
    switch (ch.unicode()) {
    case MathDsl::PowOp.unicode():
    case UnicodeChars::ModifierLetterCircumflexAccent.unicode():
    case UnicodeChars::Caret.unicode():
    case UnicodeChars::FullwidthCircumflexAccent.unicode():
        return true;
    default:
        return false;
    }
}

static bool textContainsOnlyCaretOperators(const QString& text)
{
    if (text.isEmpty())
        return false;
    for (const QChar ch : text) {
        if (!isCaretOperatorAlias(ch))
            return false;
    }
    return true;
}

static bool textContainsOnlyDivisionAliases(const QString& text)
{
    if (text.isEmpty())
        return false;
    for (const QChar ch : text) {
        if (!MathDsl::isDivisionOperatorAlias(ch))
            return false;
    }
    return true;
}

static bool textContainsOnlySubtractionAliases(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return false;
    for (const QChar ch : trimmed) {
        if (!MathDsl::isSubtractionOperatorAlias(ch))
            return false;
    }
    return true;
}

static bool isDeadKey(int key)
{
    return key >= Qt::Key_Dead_Grave && key <= Qt::Key_Dead_Small_Schwa;
}

static QChar previousNonSpaceChar(const QString& text, int cursorPosition)
{
    int i = qBound(0, cursorPosition, text.size()) - 1;
    while (i >= 0 && text.at(i).isSpace())
        --i;
    return i >= 0 ? text.at(i) : QChar();
}

static int previousNonSpaceIndex(const QString& text, int cursorPosition)
{
    int i = qBound(0, cursorPosition, text.size()) - 1;
    while (i >= 0 && text.at(i).isSpace())
        --i;
    return i;
}

static bool isExponentTailBeforeIndex(const QString& text, int index)
{
    if (index < 0 || index >= text.size())
        return false;

    const QChar ch = text.at(index);
    if (MathDsl::isSuperscriptPowerChar(ch))
        return true;

    if (!ch.isDigit())
        return false;

    int start = index;
    while (start >= 0 && text.at(start).isDigit())
        --start;
    const int beforeDigits = previousNonSpaceIndex(text, start + 1);
    if (beforeDigits < 0)
        return false;
    const QChar before = text.at(beforeDigits);
    return isCaretOperatorAlias(before) || before == MathDsl::PowNeg;
}

static bool isValidUnitExponentBase(const QChar& ch)
{
    return ch.isLetterOrNumber()
           || ch == MathDsl::GroupEnd
           || ch == MathDsl::UnitEnd;
}

static bool isInsideCommentFromQuestionMark(const QString& text, int cursorPosition)
{
    const int safeCursorPosition = qBound(0, cursorPosition, text.size());
    for (int i = safeCursorPosition - 1; i >= 0; --i) {
        const QChar ch = text.at(i);
        if (ch == MathDsl::CommentSep)
            return true;
        if (ch == QLatin1Char('\n') || ch == QLatin1Char('\r'))
            return false;
    }
    return false;
}

static bool hasOnlyWhitespaceToLeft(const QString& text, int cursorPosition)
{
    const int safeCursorPosition = qBound(0, cursorPosition, text.size());
    for (int i = 0; i < safeCursorPosition; ++i) {
        if (!text.at(i).isSpace())
            return false;
    }
    return true;
}

static bool hasAnyNonWhitespace(const QString& text)
{
    for (const QChar ch : text) {
        if (!ch.isSpace())
            return true;
    }
    return false;
}

static bool isCurrencySymbolChar(const QChar& ch)
{
    return ch.category() == QChar::Symbol_Currency;
}

static bool isUnitIdentifierCharInEditor(const QChar& ch)
{
    return ch.isLetterOrNumber()
           || ch == QLatin1Char('_')
           || ch == UnicodeChars::MicroSign
           || ch == UnicodeChars::GreekSmallLetterMu
           || ch == UnicodeChars::GreekCapitalOmega
           || ch == UnicodeChars::OhmSign
           || ch == MathDsl::Deg
           || ch == UnicodeChars::MasculineOrdinalIndicator
           || ch == MathDsl::ArcminOp
           || ch == MathDsl::ArcsecOp
           || ch == MathDsl::ArcminOpAl1
           || ch == MathDsl::ArcsecOpAl1;
}

static bool isAllowedUnitBracketChar(const QChar& ch)
{
    return isUnitIdentifierCharInEditor(ch)
           || ch == MathDsl::GroupStart
           || ch == MathDsl::GroupEnd
           || ch == MathDsl::UnitEnd
           || ch == MathDsl::PowOp
           || MathDsl::isDivisionOperator(ch)
           || MathDsl::isMultiplicationOperator(ch);
}

static QString superscriptFromAsciiExponent(const QString& asciiExponent)
{
    QString superscript;
    superscript.reserve(asciiExponent.size());
    for (const QChar ch : asciiExponent) {
        if (ch == MathDsl::AddOp) {
            superscript += MathDsl::PowPos;
            continue;
        }
        if (MathDsl::isSubtractionOperatorAlias(ch)) {
            superscript += MathDsl::PowNeg;
            continue;
        }
        const QChar superscriptDigit = MathDsl::asciiDigitToSuperscript(ch);
        if (superscriptDigit.isNull())
            return QString();
        superscript += superscriptDigit;
    }
    return superscript;
}

static bool rewriteTrailingAsciiUnitExponentToSuperscript(Editor* editor, const QString& suffix)
{
    QTextCursor cursor = editor->textCursor();
    const QString current = editor->text();
    const int cursorPos = cursor.position();
    int pos = qBound(0, cursorPos, current.size()) - 1;
    while (pos >= 0 && current.at(pos).isSpace())
        --pos;
    if (pos < 0 || !current.at(pos).isDigit())
        return false;

    int exponentStart = pos;
    while (exponentStart >= 0 && current.at(exponentStart).isDigit())
        --exponentStart;
    ++exponentStart;

    int caretPos = exponentStart - 1;
    if (caretPos >= 1 && MathDsl::isSubtractionOperatorAlias(current.at(caretPos))
        && current.at(caretPos - 1) == MathDsl::PowOp) {
        --caretPos;
    }
    if (caretPos < 0 || current.at(caretPos) != MathDsl::PowOp)
        return false;

    const QString asciiExponent = current.mid(caretPos + 1, pos - caretPos);
    const QString superscript = superscriptFromAsciiExponent(asciiExponent);
    if (superscript.isEmpty())
        return false;

    cursor.setPosition(caretPos);
    cursor.setPosition(pos + 1, QTextCursor::KeepAnchor);
    cursor.insertText(superscript + suffix);
    editor->setTextCursor(cursor);
    return true;
}

static bool appendSuffixAfterTrailingSuperscriptExponent(Editor* editor,
                                                         const QString& suffix,
                                                         const QString& numericBaseSuffix = QString())
{
    QTextCursor cursor = editor->textCursor();
    const QString current = editor->text();
    const int cursorPos = cursor.position();
    int pos = qBound(0, cursorPos, current.size()) - 1;
    while (pos >= 0 && current.at(pos).isSpace())
        --pos;
    if (pos < 0 || !MathDsl::isSuperscriptPowerChar(current.at(pos)))
        return false;

    const int exponentEnd = pos;
    while (pos >= 0 && MathDsl::isSuperscriptPowerChar(current.at(pos)))
        --pos;
    if (pos < 0)
        return false;

    const QChar base = current.at(pos);
    if (!base.isLetterOrNumber() && base != MathDsl::GroupEnd && base != MathDsl::UnitEnd)
        return false;

    const int insertPos = exponentEnd + 1;
    cursor.setPosition(insertPos);
    // Keep numeric-exponent products visually distinct: use the caller-provided
    // numeric suffix (typically " × ") when the exponent base is numeric.
    const QString chosenSuffix = (!numericBaseSuffix.isEmpty() && base.isDigit())
        ? numericBaseSuffix
        : suffix;
    cursor.insertText(chosenSuffix);
    editor->setTextCursor(cursor);
    return true;
}

static bool rewriteTrailingSuperscriptExponentToParenthesizedAscii(Editor* editor, const QString& suffix)
{
    QTextCursor cursor = editor->textCursor();
    const QString current = editor->text();
    const int cursorPos = cursor.position();
    int pos = qBound(0, cursorPos, current.size()) - 1;
    while (pos >= 0 && current.at(pos).isSpace())
        --pos;
    if (pos < 0 || !MathDsl::isSuperscriptPowerChar(current.at(pos)))
        return false;

    const int exponentEnd = pos;
    while (pos >= 0 && MathDsl::isSuperscriptPowerChar(current.at(pos)))
        --pos;
    if (pos < 0)
        return false;

    const QChar base = current.at(pos);
    if (!base.isLetterOrNumber() && base != MathDsl::GroupEnd && base != MathDsl::UnitEnd)
        return false;

    const int exponentStart = pos + 1;
    QString asciiExponent;
    asciiExponent.reserve(exponentEnd - exponentStart + 1);
    for (int i = exponentStart; i <= exponentEnd; ++i) {
        const QChar ch = current.at(i);
        if (MathDsl::isSuperscriptDigit(ch)) {
            const QChar asciiDigit = MathDsl::superscriptDigitToAscii(ch);
            if (asciiDigit.isNull())
                return false;
            asciiExponent += asciiDigit;
            continue;
        }
        if (ch == MathDsl::PowNeg) {
            asciiExponent += MathDsl::SubOpAl1;
            continue;
        }
        if (ch == MathDsl::PowPos) {
            asciiExponent += MathDsl::AddOp;
            continue;
        }
        return false;
    }
    if (asciiExponent.isEmpty())
        return false;

    const QString replacement = QStringLiteral("^(%1%2)").arg(asciiExponent, suffix);
    cursor.setPosition(exponentStart);
    cursor.setPosition(exponentEnd + 1, QTextCursor::KeepAnchor);
    cursor.insertText(replacement);
    cursor.setPosition(exponentStart + replacement.size() - 1);
    editor->setTextCursor(cursor);
    return true;
}

static QString renderUnitAsciiExponentsAsSuperscripts(const QString& unitText)
{
    static const QRegularExpression exponentPattern(
        QStringLiteral(R"(\^(?:\(([−-]?)(\d+)\)|([−-]?)(\d+)))"));

    QString output;
    output.reserve(unitText.size());
    int lastPos = 0;
    auto it = exponentPattern.globalMatch(unitText);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        output += unitText.mid(lastPos, match.capturedStart() - lastPos);

        QString sign = match.captured(1);
        QString digits = match.captured(2);
        if (digits.isEmpty()) {
            sign = match.captured(3);
            digits = match.captured(4);
        }

        if (!sign.isEmpty() && sign != QLatin1String("-") && sign != QString(MathDsl::SubOp)) {
            output += match.captured(0);
        } else {
            if (!sign.isEmpty())
                output += MathDsl::PowNeg;
            for (const QChar ch : digits) {
                const QChar superscript = MathDsl::asciiDigitToSuperscript(ch);
                output += superscript.isNull() ? ch : superscript;
            }
        }

        lastPos = match.capturedEnd();
    }
    output += unitText.mid(lastPos);
    return output;
}

static QString normalizeTypedTextForSquareBracketContext(const QString& surroundingText,
                                                         int cursorPosition,
                                                         QString text)
{
    QString normalized;
    normalized.reserve(text.size());
    const auto previousNonSpaceBefore = [](const QString& source, int fromExclusive) {
        int i = qBound(0, fromExclusive, source.size()) - 1;
        while (i >= 0 && source.at(i).isSpace())
            --i;
        return i >= 0 ? source.at(i) : QChar();
    };

    const int cursor = qBound(0, cursorPosition, surroundingText.size());
    QChar previous = previousNonSpaceBefore(surroundingText, cursor);
    int previousIndex = cursor - 1;
    while (previousIndex >= 0 && surroundingText.at(previousIndex).isSpace())
        --previousIndex;
    QChar previousPrevious = previousNonSpaceBefore(surroundingText, previousIndex);
    int previousPreviousIndex = previousIndex - 1;
    while (previousPreviousIndex >= 0 && surroundingText.at(previousPreviousIndex).isSpace())
        --previousPreviousIndex;
    QChar previousThird = previousNonSpaceBefore(surroundingText, previousPreviousIndex);
    const auto initialParenthesizedExponentDepth = [&]() {
        int depth = 0;
        const int end = qBound(0, cursorPosition, surroundingText.size());
        for (int idx = 0; idx < end; ++idx) {
            const QChar current = surroundingText.at(idx);
            if (current == MathDsl::GroupStart) {
                const QChar beforeOpen = previousNonSpaceBefore(surroundingText, idx);
                if (beforeOpen == MathDsl::PowOp) {
                    ++depth;
                } else if (depth > 0) {
                    ++depth;
                }
            } else if (current == MathDsl::GroupEnd && depth > 0) {
                --depth;
            }
        }
        return depth;
    };
    int parenthesizedExponentDepth = initialParenthesizedExponentDepth();

    for (int i = 0; i < text.size(); ++i) {
        QChar ch = text.at(i);

        if (MathDsl::isAdditionOperatorAlias(ch)) {
            return QString();
        }

        if (MathDsl::isDivisionOperatorAlias(ch))
            ch = MathDsl::DivOp;

        // Keep unit-symbol aliases normalized as users type inside [].
        if (ch == UnicodeChars::OhmSign)
            ch = UnicodeChars::GreekCapitalOmega;
        else if (ch == UnicodeChars::GreekSmallLetterMu)
            ch = UnicodeChars::MicroSign;
        else if (ch == UnicodeChars::MasculineOrdinalIndicator)
            ch = MathDsl::Deg;
        else if (ch == MathDsl::ArcminOpAl1)
            ch = MathDsl::ArcminOp;
        else if (ch == MathDsl::ArcsecOpAl1)
            ch = MathDsl::ArcsecOp;

        if (MathDsl::isSubtractionOperatorAlias(ch)) {
            const bool afterExponentStart =
                previous == MathDsl::PowOp
                || previous == MathDsl::PowNeg
                || (previous == MathDsl::GroupStart && previousPrevious == MathDsl::PowOp);
            if (!afterExponentStart)
                return QString();
            ch = MathDsl::SubOp;
        }

        if (isUnitIdentifierCharInEditor(ch) && !ch.isDigit()) {
            if (ch == QLatin1Char('e') || ch == QLatin1Char('E')) {
                const bool inExponentTypingPosition =
                    previous == MathDsl::PowOp
                    || parenthesizedExponentDepth > 0
                    || previousPrevious == MathDsl::PowOp;
                if (inExponentTypingPosition)
                    return QString();
            }

            const bool rightAfterExponentStart =
                previous == MathDsl::PowOp
                || (previous == MathDsl::GroupStart && previousPrevious == MathDsl::PowOp);
            if (rightAfterExponentStart)
                return QString();
        }

        if (previous == MathDsl::DivOp) {
            if (parenthesizedExponentDepth > 0) {
                const bool isValidExponentDenominatorDigit = ch.isDigit();
                if (!isValidExponentDenominatorDigit)
                    return QString();
            } else {
                const bool isValidUnitDenominator =
                    (isUnitIdentifierCharInEditor(ch) && !ch.isDigit())
                    || ch == MathDsl::GroupStart;
                if (!isValidUnitDenominator)
                    return QString();
            }
        }

        if (ch == MathDsl::DotSep || ch == MathDsl::CommaSep) {
            if (parenthesizedExponentDepth <= 0 || !previous.isDigit())
                return QString();

            int idx = normalized.size() - 1;
            while (idx >= 0 && normalized.at(idx).isDigit())
                --idx;
            if (idx >= 0 && normalized.at(idx) == MathDsl::DotSep)
                return QString();

            ch = MathDsl::DotSep;
        }

        if (MathDsl::isDivisionOperator(ch)) {
            if (MathDsl::isDivisionOperator(previous))
                return QString();
            if (MathDsl::isMultiplicationOperator(previous)) {
                return QString();
            }
            const bool afterExponentStart =
                previous == MathDsl::PowOp
                || previous == MathDsl::PowNeg
                || (previous == MathDsl::GroupStart && previousPrevious == MathDsl::PowOp);
            const bool afterSignedExponentStart =
                MathDsl::isSubtractionOperatorAlias(previous)
                && (previousPrevious == MathDsl::PowOp
                    || (previousPrevious == MathDsl::GroupStart && previousThird == MathDsl::PowOp));
            if (afterExponentStart || afterSignedExponentStart)
                return QString();
        }

        if (ch == MathDsl::PowOp) {
            const bool validExponentBase =
                previous.isLetterOrNumber()
                || previous == MathDsl::GroupEnd
                || previous == MathDsl::UnitEnd;
            if (!validExponentBase)
                return QString();
        }

        if (ch == QLatin1Char(' ')) {
            if (previous.isNull()
                || previous == MathDsl::PowOp
                || previous == MathDsl::GroupStart
                || MathDsl::isMultiplicationOperator(previous)) {
                return QString();
            }
            ch = MathDsl::MulDotOp;
        } else if (MathDsl::isMultiplicationOperatorAlias(ch, true)) {
            if (!MathDsl::isMultiplicationOperator(ch))
                ch = MathDsl::MulCrossOp;
        }

        if (!isAllowedUnitBracketChar(ch) && ch != MathDsl::SubOp)
            return QString();

        normalized += ch;
        if (!ch.isSpace()) {
            if (ch == MathDsl::GroupStart) {
                if (previous == MathDsl::PowOp)
                    ++parenthesizedExponentDepth;
                else if (parenthesizedExponentDepth > 0)
                    ++parenthesizedExponentDepth;
            } else if (ch == MathDsl::GroupEnd && parenthesizedExponentDepth > 0) {
                --parenthesizedExponentDepth;
            }
            previousThird = previousPrevious;
            previousPrevious = previous;
            previous = ch;
        }
    }

    return normalized;
}

static Tokens scanForCompletionContext(Evaluator* evaluator,
                                       const QString& text,
                                       bool squareBracketContext)
{
    const auto normalizeSuperscriptPowers = [&](QString source) {
        for (int i = 0; i < source.size(); ++i) {
            if (!MathDsl::isSuperscriptPowerChar(source.at(i)))
                continue;

            int j = i;
            while (j < source.size() && MathDsl::isSuperscriptPowerChar(source.at(j)))
                ++j;

            QString power;
            power.reserve(j - i + 3);
            bool negative = false;
            for (int k = i; k < j; ++k) {
                const QChar ch = source.at(k);
                if (MathDsl::isSuperscriptDigit(ch)) {
                    const QChar asciiDigit = MathDsl::superscriptDigitToAscii(ch);
                    if (!asciiDigit.isNull())
                        power += asciiDigit;
                    continue;
                }
                if (ch == MathDsl::PowNeg) {
                    if (power.isEmpty()) {
                        negative = true;
                        continue;
                    }
                    power += MathDsl::SubOpAl1;
                    continue;
                }
                if (ch == MathDsl::PowPos) {
                    if (power.isEmpty())
                        continue;
                    power += MathDsl::AddOp;
                }
            }

            if (power.isEmpty())
                continue;
            if (negative)
                power = QString(MathDsl::PowOp) + MathDsl::GroupStart + MathDsl::SubOpAl1 + power + MathDsl::GroupEnd;
            else
                power.prepend(MathDsl::PowOp);

            source.replace(i, j - i, power);
            i += power.size() - 1;
        }
        return source;
    };

    Tokens tokens = evaluator->scan(text);
    if (tokens.valid())
        return tokens;

    const QString normalized = normalizeSuperscriptPowers(text);
    if (normalized != text)
        tokens = evaluator->scan(normalized);
    if (tokens.valid())
        return tokens;

    if (squareBracketContext) {
        tokens = evaluator->scan(text + MathDsl::UnitEnd);
        if (!tokens.valid() && normalized != text)
            tokens = evaluator->scan(normalized + MathDsl::UnitEnd);
    }
    return tokens;
}

static int trailingIdentifierStart(const QString& text, int endPosition)
{
    const int safeEnd = qBound(0, endPosition, text.size());
    if (safeEnd <= 0)
        return -1;

    auto isIdentifierStartChar = [](const QChar& ch) {
        return isUnitIdentifierCharInEditor(ch)
               && !ch.isDigit()
               && !MathDsl::isSuperscriptPowerChar(ch);
    };
    auto isIdentifierContinueChar = [](const QChar& ch) {
        return isUnitIdentifierCharInEditor(ch)
               && !MathDsl::isSuperscriptPowerChar(ch);
    };

    int start = safeEnd;
    while (start > 0 && isIdentifierContinueChar(text.at(start - 1)))
        --start;
    while (start < safeEnd && !isIdentifierStartChar(text.at(start)))
        ++start;
    if (start == safeEnd)
        return -1;
    return start;
}

static QString formattedLiveResultWithAlternatives(const Quantity& quantity,
                                                   const QString& expression,
                                                   const QString& interpretedExpression,
                                                   const QString& simplifiedExpression = QString(),
                                                   const QString& sourceExpression = QString(),
                                                   const Evaluator* evaluator = nullptr)
{
    Q_UNUSED(expression);
    Q_UNUSED(simplifiedExpression);
    const QString source = sourceExpression.isEmpty() ? expression : sourceExpression;
    const QStringList lines = ResultLineFormatUtils::formatResultLinesForDisplay(
        source,
        interpretedExpression,
        quantity,
        true,
        true,
        evaluator);
    QStringList escapedLines;
    for (const QString& line : lines)
        escapedLines.append(line.toHtmlEscaped());
    return escapedLines.join(QStringLiteral("<br/>"));
}

static QString simplifiedExpressionLineForTooltip(const QString& interpretedExpression,
                                                  const QString& sourceExpression,
                                                  const Evaluator* evaluator)
{
    return ResultLineFormatUtils::simplifiedExpressionLineForDisplay(
        interpretedExpression,
        sourceExpression,
        Settings::instance()->simplifyResultExpressions,
        evaluator);
}

Editor::Editor(QWidget* parent)
    : QPlainTextEdit(parent)
{
    m_ownedSession.reset(new Session);
    m_session = m_ownedSession.get();
    m_evaluator = m_session->evaluator();
    m_currentHistoryIndex = 0;
    m_isAutoCompletionEnabled = true;
    m_completion = new EditorCompletion(this);
    m_constantCompletion = 0;
    m_completionTimer = new QTimer(this);
    m_isAutoCalcEnabled = true;
    m_highlighter = new SyntaxHighlighter(this);
    m_matchingTimer = new QTimer(this);
    m_customCursorVisible = true;
    m_shouldPaintCustomCursor = true;
    m_historyArrowNavigationEnabled = true;

    setViewportMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    setTabChangesFocus(true);
    setLineWrapMode(QPlainTextEdit::WidgetWidth);
    setWordWrapMode(QTextOption::WrapAnywhere);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setCursorWidth(0);

    connect(m_completion, &EditorCompletion::selectedCompletion,
            this, &Editor::autoComplete);
    connect(m_completionTimer, SIGNAL(timeout()), SLOT(triggerAutoComplete()));
    connect(m_matchingTimer, SIGNAL(timeout()), SLOT(doMatchingPar()));
    connect(this, &Editor::selectionChanged, this, &Editor::checkSelectionAutoCalc);
    connect(this, &Editor::textChanged, this, &Editor::checkAutoCalc);
    connect(this, &Editor::textChanged, this, &Editor::checkAutoComplete);
    connect(this, &Editor::textChanged, this, &Editor::checkMatching);
    connect(document()->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged,
            this, [this](const QSizeF&) { updateHeightForWrappedText(); });

    adjustSize();
    updateHeightForWrappedText();
}

Editor::~Editor() = default;

void Editor::setSession(Session* session)
{
    if (session == nullptr) {
        if (!m_ownedSession)
            m_ownedSession.reset(new Session);
        session = m_ownedSession.get();
    }

    m_session = session;
    m_evaluator = m_session->evaluator();
    m_highlighter->setEvaluator(m_evaluator);
    updateHistory();
}

void Editor::refreshAutoCalc()
{
    if (m_isAutoCalcEnabled) {
      if (!textCursor().selectedText().isEmpty())
          checkSelectionAutoCalc();
      else
          checkAutoCalc();
    }
}

QString Editor::text() const
{
    return toPlainText();
}

void Editor::setText(const QString& text)
{
    setPlainText(normalizeExpressionTypedInEditor(text));
}

void Editor::insert(const QString& text)
{
    QString normalized = normalizeExpressionTypedInEditor(text);
    const QString normalizedTrimmed = normalized.trimmed();
    const int pos = textCursor().position();
    const QString surroundingText = this->text();
    const bool squareBracketContext = isInsideUnmatchedSquareBracketContext(
        surroundingText,
        pos);

    if (squareBracketContext) {
        normalized = normalizeTypedTextForSquareBracketContext(
            surroundingText,
            pos,
            normalized);
        if (normalized.isEmpty())
            return;
    }

    // Reject repeated exponent operators when the nearest left non-space is
    // already a caret operator, regardless of how caret is encoded.
    if (textContainsOnlyCaretOperators(normalized)
        || textContainsOnlyCaretOperators(normalizedTrimmed)) {
        const QChar prev = previousNonSpaceChar(this->text(), pos);
        if (isCaretOperatorAlias(prev))
            return;
    }

    if (textContainsOnlyAdditionAliases(normalized)
        || textContainsOnlyAdditionAliases(normalizedTrimmed)) {
        const QChar prev = previousNonSpaceChar(this->text(), pos);
        const bool prevIsBlockingOperator =
            isAnyAdditionOperator(prev)
            || MathDsl::isSubtractionOperatorAlias(prev)
            || MathDsl::isDivisionOperatorAlias(prev)
            || isAnyMultiplicationOperator(prev)
            || prev == MathDsl::PowOp;
        if (prevIsBlockingOperator)
            return;

        normalized = EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            this->text(),
            pos,
            QString(MathDsl::AddOp));
    }

    insertPlainText(normalized);
}

void Editor::doBackspace()
{
    QTextCursor cursor = textCursor();

    const int groupedLength = !cursor.hasSelection()
        ? groupedTokenLengthBefore(toPlainText(), cursor.position())
        : 0;
    if (groupedLength > 0) {
        cursor.setPosition(cursor.position() - groupedLength, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        setTextCursor(cursor);
        return;
    }

    cursor.deletePreviousChar();
    setTextCursor(cursor);
}

void Editor::doDelete()
{
    QTextCursor cursor = textCursor();

    const int groupedLength = !cursor.hasSelection()
        ? groupedTokenLengthAfter(toPlainText(), cursor.position())
        : 0;
    if (groupedLength > 0) {
        cursor.setPosition(cursor.position() + groupedLength, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        setTextCursor(cursor);
        return;
    }

    cursor.deleteChar();
    setTextCursor(cursor);
}

char Editor::radixChar() const
{
    return Settings::instance()->radixCharacter();
}

int Editor::cursorPosition() const
{
    return textCursor().position();
}

void Editor::setCursorPosition(int position)
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(position);
    setTextCursor(cursor);
}

QSize Editor::sizeHint() const
{
    ensurePolished();
    const QFontMetrics metrics = fontMetrics();
    const int width = metrics.horizontalAdvance('x') * 10;
    const int height = metrics.lineSpacing() + 6;
    return QSize(width, height);
}

void Editor::clearHistory()
{
    m_history.clear();
    m_currentHistoryIndex = 0;
}

bool Editor::isAutoCompletionEnabled() const
{
    return m_isAutoCompletionEnabled;
}

void Editor::setAutoCompletionEnabled(bool enable)
{
    m_isAutoCompletionEnabled = enable;
}

bool Editor::isAutoCalcEnabled() const
{
    return m_isAutoCalcEnabled;
}

void Editor::setAutoCalcEnabled(bool enable)
{
    m_isAutoCalcEnabled = enable;
}

void Editor::setCustomCursorVisible(bool visible)
{
    setCursorWidth(0);

    if (m_customCursorVisible == visible)
        return;

    m_customCursorVisible = visible;
    m_shouldPaintCustomCursor = visible;
    viewport()->update();
}

void Editor::setHistoryArrowNavigationEnabled(bool enabled)
{
    m_historyArrowNavigationEnabled = enabled;
}

void Editor::checkAutoComplete()
{
    if (!m_isAutoCompletionEnabled)
        return;

    m_completionTimer->stop();
    m_completionTimer->setSingleShot(true);
    m_completionTimer->start();
}


void Editor::checkMatching()
{
    if (!Settings::instance()->syntaxHighlighting)
        return;

    m_matchingTimer->stop();
    m_matchingTimer->setSingleShot(true);
    m_matchingTimer->start();
}

void Editor::checkAutoCalc()
{
    if (m_isAutoCalcEnabled)
        autoCalc();
}

void Editor::doMatchingPar()
{
    // Clear previous.
    setExtraSelections(QList<QTextEdit::ExtraSelection>());

    if (!Settings::instance()->syntaxHighlighting)
        return;

    doMatchingLeft();
    doMatchingRight();
}

void Editor::checkSelectionAutoCalc()
{
    if (m_isAutoCalcEnabled)
        autoCalcSelection();
}

void Editor::doMatchingLeft()
{
    const int currentPosition = textCursor().position();
    if (currentPosition <= 0)
        return;
    const QString currentText = text();
    const int closePos = currentPosition - 1;
    if (closePos < 0 || closePos >= currentText.size())
        return;

    const auto isOpen = [](QChar ch) {
        return ch == MathDsl::GroupStart || ch == MathDsl::UnitStart || ch == MathDsl::ListStart;
    };
    const auto isClose = [](QChar ch) {
        return ch == MathDsl::GroupEnd || ch == MathDsl::UnitEnd || ch == MathDsl::ListEnd;
    };
    const auto matchingOpen = [](QChar ch) {
        if (ch == MathDsl::GroupEnd) return MathDsl::GroupStart;
        if (ch == MathDsl::UnitEnd) return MathDsl::UnitStart;
        if (ch == MathDsl::ListEnd) return MathDsl::ListStart;
        return QChar();
    };

    const QChar closing = currentText.at(closePos);
    if (!isClose(closing))
        return;

    QVector<QChar> expectedOpenStack;
    expectedOpenStack.append(matchingOpen(closing));
    int matchPos = -1;
    for (int i = closePos - 1; i >= 0; --i) {
        const QChar ch = currentText.at(i);
        if (isClose(ch)) {
            expectedOpenStack.append(matchingOpen(ch));
            continue;
        }
        if (!isOpen(ch))
            continue;
        if (expectedOpenStack.isEmpty())
            break;
        if (ch == expectedOpenStack.last()) {
            expectedOpenStack.removeLast();
            if (expectedOpenStack.isEmpty()) {
                matchPos = i;
                break;
            }
        } else {
            break;
        }
    }

    if (matchPos < 0)
        return;

    QTextEdit::ExtraSelection hilite1;
    hilite1.cursor = textCursor();
    hilite1.cursor.setPosition(matchPos);
    hilite1.cursor.setPosition(matchPos + 1, QTextCursor::KeepAnchor);
    hilite1.format.setBackground(m_highlighter->colorForRole(ColorScheme::Matched));

    QTextEdit::ExtraSelection hilite2;
    hilite2.cursor = textCursor();
    hilite2.cursor.setPosition(closePos);
    hilite2.cursor.setPosition(closePos + 1, QTextCursor::KeepAnchor);
    hilite2.format.setBackground(m_highlighter->colorForRole(ColorScheme::Matched));

    QList<QTextEdit::ExtraSelection> extras;
    extras << hilite1;
    extras << hilite2;
    setExtraSelections(extras);
}

void Editor::doMatchingRight()
{
    const int currentPosition = textCursor().position();
    const QString currentText = text();
    if (currentPosition < 0 || currentPosition >= currentText.size())
        return;
    const auto isOpen = [](QChar ch) {
        return ch == MathDsl::GroupStart || ch == MathDsl::UnitStart || ch == MathDsl::ListStart;
    };
    const auto isClose = [](QChar ch) {
        return ch == MathDsl::GroupEnd || ch == MathDsl::UnitEnd || ch == MathDsl::ListEnd;
    };
    const auto matchingClose = [](QChar ch) {
        if (ch == MathDsl::GroupStart) return MathDsl::GroupEnd;
        if (ch == MathDsl::UnitStart) return MathDsl::UnitEnd;
        if (ch == MathDsl::ListStart) return MathDsl::ListEnd;
        return QChar();
    };

    const int openPos = currentPosition;
    const QChar opening = currentText.at(openPos);
    if (!isOpen(opening))
        return;

    QVector<QChar> expectedCloseStack;
    expectedCloseStack.append(matchingClose(opening));
    int matchPos = -1;
    for (int i = openPos + 1; i < currentText.size(); ++i) {
        const QChar ch = currentText.at(i);
        if (isOpen(ch)) {
            expectedCloseStack.append(matchingClose(ch));
            continue;
        }
        if (!isClose(ch))
            continue;
        if (expectedCloseStack.isEmpty())
            break;
        if (ch == expectedCloseStack.last()) {
            expectedCloseStack.removeLast();
            if (expectedCloseStack.isEmpty()) {
                matchPos = i;
                break;
            }
        } else {
            break;
        }
    }

    if (matchPos < 0)
        return;

    QTextEdit::ExtraSelection hilite1;
    hilite1.cursor = textCursor();
    hilite1.cursor.setPosition(matchPos);
    hilite1.cursor.setPosition(matchPos + 1, QTextCursor::KeepAnchor);
    hilite1.format.setBackground(m_highlighter->colorForRole(ColorScheme::Matched));

    QTextEdit::ExtraSelection hilite2;
    hilite2.cursor = textCursor();
    hilite2.cursor.setPosition(openPos);
    hilite2.cursor.setPosition(openPos + 1, QTextCursor::KeepAnchor);
    hilite2.format.setBackground(m_highlighter->colorForRole(ColorScheme::Matched));

    QList<QTextEdit::ExtraSelection> extras;
    extras << hilite1;
    extras << hilite2;
    setExtraSelections(extras);
}


// Matches a list of built-in functions, units and variables to a fragment.
QStringList Editor::matchFragment(const QString& id, bool unitContext) const
{
    const Settings* settings = Settings::instance();

    QStringList choices;

    if (!unitContext && settings->autoCompletionBuiltInFunctions) {
        const auto fnames = FunctionRepo::instance()->getIdentifiers();
        for (int i = 0; i < fnames.count(); ++i) {
            if (fnames.at(i).startsWith(id, Qt::CaseSensitive)) {
                QString str = fnames.at(i);
                Function* f = FunctionRepo::instance()->find(str);
                if (f)
                    str.append(':').append(f->name());
                choices.append(str);
            }
        }
        choices.sort();
    }

    if (unitContext) {
        const auto unitNameMatchesFragment = [&id](const QString& unitName) -> bool {
            if (unitName.startsWith(id, Qt::CaseSensitive))
                return true;

            // Treat ASCII 'u' as a typing-friendly alias for leading micro sign.
            if (id.isEmpty()
                || id.at(0) != QLatin1Char('u')
                || !unitName.startsWith(UnicodeChars::MicroSign))
                return false;

            return unitName.mid(1).startsWith(id.mid(1), Qt::CaseSensitive);
        };

        QStringList unitChoices;
        QSet<QString> seenUnitNames;
        const QStringList allUnits = m_evaluator->allUnitIdentifiers();
        for (const QString& unitName : allUnits) {
            if (!unitNameMatchesFragment(unitName))
                continue;
            // Keep parsing support for ASCII microarcsecond alias, but avoid
            // showing it in completion where the preferred symbol is "µas".
            if (unitName == QStringLiteral("uas"))
                continue;
            if (seenUnitNames.contains(unitName))
                continue;
            seenUnitNames.insert(unitName);
            QString unitDescription = tr("Unit");
            if (const UserUnit* userUnit = m_evaluator->getUserUnit(unitName)) {
                const QString userDescription = userUnit->description().trimmed();
                unitDescription = userDescription.isEmpty()
                    ? tr("User unit")
                    : userDescription;
            } else {
                const QString localizedName = unitLocalizedIdentifierName(unitName);
                if (!localizedName.isEmpty())
                    unitDescription = tr(localizedName.toUtf8().constData());
            }
            unitChoices.append(unitName + QStringLiteral(":") + unitDescription);
        }
        unitChoices.sort();
        choices += unitChoices;
    }

    // Find matches in variable names.
    QStringList variableChoices;
    QSet<QString> seenVariableCompletionIds;
    QList<Variable> variables = m_evaluator->getVariables();
    for (int i = 0; i < variables.count(); ++i) {
        const Variable variable = variables.at(i);
        const bool isBuiltIn = variable.type() == Variable::BuiltIn;
        const bool includeVariable = !unitContext
            && ((isBuiltIn && settings->autoCompletionBuiltInVariables)
                || (!isBuiltIn && settings->autoCompletionUserVariables));
        if (!includeVariable)
            continue;

        if (variable.identifier().startsWith(id, Qt::CaseSensitive)) {
            const QString completionIdentifier = variable.identifier();
            if (seenVariableCompletionIds.contains(completionIdentifier))
                continue;
            seenVariableCompletionIds.insert(completionIdentifier);
            QString variableDescription;
            if (m_evaluator->isGlobalUserVariable(completionIdentifier))
                variableDescription = NumberFormatter::format(variable.value());
            else
                variableDescription = variable.description().trimmed().isEmpty()
                    ? NumberFormatter::format(variable.value())
                    : variable.description().trimmed();
            variableChoices.append(QString("%1:%2").arg(
                completionIdentifier,
                variableDescription));
        }
    }
    variableChoices.sort();
    choices += variableChoices;

    if (!unitContext && settings->autoCompletionUserFunctions) {
        QStringList ufchoices;
        auto userFunctions = m_evaluator->getUserFunctions();
        for (int i = 0; i < userFunctions.count(); ++i) {
            if (userFunctions.at(i).name().startsWith(id, Qt::CaseSensitive)) {
                const QString description = userFunctions.at(i).description().trimmed().isEmpty()
                    ? tr("User function")
                    : userFunctions.at(i).description().trimmed();
                ufchoices.append(QString("%1:%2").arg(
                    userFunctions.at(i).name(), description));
            }
        }
        ufchoices.sort();
        choices += ufchoices;
    }

    return choices;
}

QString Editor::getKeyword() const
{
    // Tokenize the expression.
    const int currentPosition = textCursor().position();
    const bool unitContextAtCursor = isInsideUnmatchedSquareBracketContext(text(), currentPosition);
    const Tokens tokens = scanForCompletionContext(
        m_evaluator,
        text(),
        unitContextAtCursor);

    // Find the token at the cursor.
    for (int i = tokens.size() - 1; i >= 0; --i) {
        const auto& token = tokens[i];
        if (token.pos() > currentPosition)
            continue;
        if (token.isIdentifier() || token.isUnitIdentifier()) {
            const QString tokenText = token.text();
            const auto matches = matchFragment(tokenText, unitContextAtCursor);

            // Prefer an exact identifier match under cursor; prefix matches
            // are only a fallback for partial identifiers.
            for (const auto& match : matches) {
                const QString identifier = match.split(":").first();
                if (identifier.compare(tokenText, Qt::CaseSensitive) == 0)
                    return identifier;
            }
            if (!matches.empty())
                return matches.first().split(":").first();
        }

        // Try further to the left.
        continue;
    }
    return "";
}

void Editor::triggerAutoComplete()
{
    if (m_shouldBlockAutoCompletionOnce) {
        m_shouldBlockAutoCompletionOnce = false;
        return;
    }
    if (!m_isAutoCompletionEnabled)
        return;

    // Tokenize the expression (this is very fast).
    const int currentPosition = textCursor().position();
    auto subtext = text().left(currentPosition);
    const bool unitContext = isInsideUnmatchedSquareBracketContext(text(), currentPosition);
    const auto tokens = scanForCompletionContext(m_evaluator, subtext, unitContext);
    if (!tokens.valid() || tokens.count() < 1)
        return;

    Token lastToken;
    bool foundIdentifierToken = false;
    for (int i = tokens.count() - 1; i >= 0; --i) {
        const Token candidate = tokens.at(i);
        if (!candidate.isIdentifier() && !candidate.isUnitIdentifier())
            continue;
        if (!candidate.size())
            continue;
        if (candidate.pos() > subtext.length())
            continue;
        lastToken = candidate;
        foundIdentifierToken = true;
        break;
    }
    if (!foundIdentifierToken)
        return;
    const int rawIdStart = trailingIdentifierStart(subtext, subtext.length());
    const QString id = rawIdStart >= 0
        ? subtext.mid(rawIdStart)
        : lastToken.text();
    if (id.length() < 1)
        return;

    // No space after identifier.
    const int rawIdEnd = rawIdStart >= 0 ? subtext.length() : (lastToken.pos() + lastToken.size());
    if (rawIdEnd < subtext.length())
        return;

    QStringList choices(matchFragment(id, unitContext));

    // If we are assigning a user function, find matches in its arguments names
    // and replace variables names that collide.
    if (m_evaluator->isUserFunctionAssign()) {
        for (int i=2; i<tokens.size(); ++i) {
            if (tokens[i].asOperator() == Token::ListSeparator)
                continue;
            if (tokens[i].asOperator() == Token::AssociationEnd
                && tokens[i].text() == QLatin1String(")"))
                break;
            if (tokens[i].isIdentifier()) {
                auto arg = tokens[i].text();
                if (!arg.startsWith(id, Qt::CaseSensitive))
                    continue;
                for (int j = 0; j < choices.size(); ++j) {
                    if (choices[j].split(":")[0] == arg) {
                        choices.removeAt(j);
                        j--;
                    }
                }
                choices.append(arg + ": " + tr("Argument"));
            }
        }
    }

    // No match, don't bother with completion.
    if (!choices.count())
        return;

    // Single perfect match, no need to give choices.
    if (choices.count() == 1)
        if (choices.at(0).split(":").first() == id)
            return;

    // Present the user with completion choices.
    m_completion->showCompletion(choices);
}

void Editor::autoComplete(const QString& item)
{
    if (!m_isAutoCompletionEnabled || item.isEmpty())
        return;
    // Accepting a completion edits text (and often inserts "()" for functions),
    // which emits textChanged and would immediately reopen completion.
    m_shouldBlockAutoCompletionOnce = true;

    const int currentPosition = textCursor().position();
    const bool unitContext = isInsideUnmatchedSquareBracketContext(text(), currentPosition);
    const auto subtext = text().left(currentPosition);
    const auto tokens = scanForCompletionContext(m_evaluator, subtext, unitContext);
    if (!tokens.valid() || tokens.count() < 1)
        return;

    Token lastToken;
    bool foundIdentifierToken = false;
    for (int i = tokens.count() - 1; i >= 0; --i) {
        const Token candidate = tokens.at(i);
        if (!candidate.isIdentifier() && !candidate.isUnitIdentifier())
            continue;
        if (!candidate.size())
            continue;
        if (candidate.pos() > subtext.length())
            continue;
        lastToken = candidate;
        foundIdentifierToken = true;
        break;
    }
    if (!foundIdentifierToken)
        return;

    const auto str = item.split(':');
    // Add leading space characters if any.
    auto newTokenText = str.at(0);
    if (unitContext) {
        newTokenText = UnicodeChars::normalizeUnitSymbolAliases(newTokenText);
        const QString preferredShortUnitText = UnitDisplayFormat::shortDisplayName(newTokenText);
        if (preferredShortUnitText != newTokenText) {
            // Some short unit symbols are not accepted by unit-context typing
            // normalization. Fall back to the long identifier so completion
            // still inserts a valid unit token instead of becoming a no-op.
            const QString normalizedShortUnitText = normalizeTypedTextForSquareBracketContext(
                text(),
                currentPosition,
                preferredShortUnitText);
            if (!normalizedShortUnitText.isEmpty())
                newTokenText = preferredShortUnitText;
        } else {
            newTokenText = preferredShortUnitText;
        }
        const UnitId completedUnitId =
            unitId(normalizeUnitName(UnicodeChars::normalizeUnitSymbolAliases(newTokenText)));
        if (completedUnitId != UnitId::Unknown) {
            const QString symbol = unitSymbol(completedUnitId);
            if (!symbol.isEmpty())
                newTokenText = symbol;
        }
    }
    if (newTokenText == QLatin1String("pi"))
        newTokenText = QString(UnicodeChars::Pi);
    const int rawIdStart = trailingIdentifierStart(subtext, subtext.length());
    const int replaceStart = rawIdStart >= 0 ? rawIdStart : lastToken.pos();
    const int replaceSize = rawIdStart >= 0 ? (subtext.length() - rawIdStart) : lastToken.size();
    const int leadingSpaces = rawIdStart >= 0
        ? 0
        : (lastToken.size() - lastToken.text().length());
    if (leadingSpaces > 0)
        newTokenText = newTokenText.rightJustified(
            leadingSpaces + newTokenText.length(), ' ');

    blockSignals(true);
    QTextCursor cursor = textCursor();
    cursor.setPosition(replaceStart);
    cursor.setPosition(replaceStart + replaceSize,
                       QTextCursor::KeepAnchor);
    setTextCursor(cursor);
    QPlainTextEdit::insertPlainText(newTokenText);
    blockSignals(false);

    cursor = textCursor();
    bool hasParensAlready = cursor.movePosition(QTextCursor::NextCharacter,
                                                QTextCursor::KeepAnchor);
    if (hasParensAlready) {
        auto nextChar = cursor.selectedText();
        hasParensAlready = (nextChar == "(");
    }
    bool isFunction = !unitContext
                      && (FunctionRepo::instance()->find(str.at(0))
                          || m_evaluator->hasUserFunction(str.at(0)));
    bool shouldAutoInsertParens = isFunction && !hasParensAlready;
    if (shouldAutoInsertParens) {
        insert(QString::fromLatin1("()"));
        cursor = textCursor();
        cursor.movePosition(QTextCursor::PreviousCharacter);
        setTextCursor(cursor);
    }

    checkAutoCalc();
}

void Editor::insertFromMimeData(const QMimeData* source)
{
    const QStringList expressions = EditorUtils::parsePastedExpressionsForEditorInput(source->text());

    if (expressions.isEmpty())
        return;

    auto normalizedPastedExpression = [](const QString& expression) {
        QString formattedNumericLiteral;
        if (NumberFormatter::tryFormatStandaloneNumericLiteralForDisplay(expression, &formattedNumericLiteral))
            return normalizeExpressionTypedInEditor(formattedNumericLiteral);
        return expression;
    };

    if (expressions.size() == 1) {
        // Insert text manually to make sure expression does not contain new line characters
        insert(normalizedPastedExpression(expressions.at(0)));
        return;
    }
    emit bulkEvaluationStarted();
    for (int i = 0; i < expressions.size(); ++i) {
        insert(normalizedPastedExpression(expressions.at(i)));
        evaluate();
    }
    emit bulkEvaluationFinished();
}

void Editor::autoCalc()
{
    if (!m_isAutoCalcEnabled)
        return;

    const auto str = m_evaluator->autoFix(text());
    if (str.isEmpty())
        return;

    // Same reason as above, do not update "ans".
    m_evaluator->setExpression(str);
    auto quantity = m_evaluator->evalNoAssign();

    if (m_evaluator->error().isEmpty()) {
        QString interpretedExpr = m_evaluator->interpretedExpression();
        QString simplifiedLine;
        if (!quantity.isNan() && !m_evaluator->isUserFunctionAssign()
            && !Evaluator::isCommentOnlyExpression(str)) {
            simplifiedLine = simplifiedExpressionLineForTooltip(interpretedExpr, text(), m_evaluator);
        }

        if (quantity.isNan() && (m_evaluator->isUserFunctionAssign()
            || Evaluator::isCommentOnlyExpression(str))) {
            // Result is not available for user function assignment and
            // comment-only expressions.
            emit autoCalcDisabled();
        } else {
            const auto formatted =
                formattedLiveResultWithAlternatives(
                    quantity, str, interpretedExpr, simplifiedLine, text(), m_evaluator);
            auto message = tr("Current result:<br/>%1").arg(formatted);
            emit autoCalcMessageAvailable(message);
            emit autoCalcQuantityAvailable(quantity);
        }
    } else {
        if (isOperatorOnlyIncompleteInput(str)) {
            emit autoCalcDisabled();
            return;
        }

        const QString usageTooltip = FunctionTooltipUtils::activeFunctionUsageTooltip(
            m_evaluator,
            text(),
            textCursor().position()
        );
        if (usageTooltip.isEmpty()) {
            QString baseExpression;
            if (EditorUtils::expressionWithoutIgnorableTrailingToken(str, &baseExpression)) {
                m_evaluator->setExpression(baseExpression);
                auto baseQuantity = m_evaluator->evalNoAssign();
                if (m_evaluator->error().isEmpty()) {
                    const QString interpretedExpr = m_evaluator->interpretedExpression();
                    const QString simplifiedLine = (!baseQuantity.isNan()
                        && !m_evaluator->isUserFunctionAssign()
                        && !Evaluator::isCommentOnlyExpression(baseExpression))
                        ? simplifiedExpressionLineForTooltip(interpretedExpr, text(), m_evaluator)
                        : QString();
                    if (baseQuantity.isNan() && (m_evaluator->isUserFunctionAssign()
                        || Evaluator::isCommentOnlyExpression(baseExpression))) {
                        emit autoCalcDisabled();
                    } else {
                        const auto formatted =
                            formattedLiveResultWithAlternatives(
                                baseQuantity, baseExpression, interpretedExpr, simplifiedLine, text(), m_evaluator);
                        auto message = tr("Current result:<br/>%1").arg(formatted);
                        emit autoCalcMessageAvailable(message);
                        emit autoCalcQuantityAvailable(baseQuantity);
                    }
                    return;
                }
            }
        }
        emit autoCalcMessageAvailable(
            usageTooltip.isEmpty() ? m_evaluator->error() : usageTooltip
        );
    }
}

void Editor::increaseFontPointSize()
{
    QFont newFont = font();
    const int newSize = newFont.pointSize() + 1;
    if (newSize > 96)
        return;
    newFont.setPointSize(newSize);
    setFont(newFont);
}

void Editor::decreaseFontPointSize()
{
    QFont newFont = font();
    const int newSize = newFont.pointSize() - 1;
    if (newSize < 8)
        return;
    newFont.setPointSize(newSize);
    setFont(newFont);
}

void Editor::autoCalcSelection(const QString& custom)
{
    if (!m_isAutoCalcEnabled)
        return;

    const QString rawSelection = custom.isNull() ? textCursor().selectedText() : custom;
    if (rawSelection.contains(RegExpPatterns::lineBreak())) {
        emit autoCalcDisabled();
        return;
    }

    auto str = rawSelection;
    str = m_evaluator->autoFix(str);
    if (str.isEmpty()) {
        emit autoCalcDisabled();
        return;
    }

    // Same reason as above, do not update "ans".
    m_evaluator->setExpression(str);
    auto quantity = m_evaluator->evalNoAssign();

    if (m_evaluator->error().isEmpty()) {
        const QString interpretedExpr = m_evaluator->interpretedExpression();
        const QString simplifiedLine = (!quantity.isNan() && !m_evaluator->isUserFunctionAssign()
            && !Evaluator::isCommentOnlyExpression(str))
            ? simplifiedExpressionLineForTooltip(interpretedExpr, rawSelection, m_evaluator)
            : QString();
        if (quantity.isNan() && (m_evaluator->isUserFunctionAssign()
            || Evaluator::isCommentOnlyExpression(str))) {
            // Result is not available for user function assignment and
            // comment-only expressions.
            auto message = tr("Selection result: n/a");
            emit autoCalcMessageAvailable(message);
        } else {
            const auto formatted =
                formattedLiveResultWithAlternatives(
                    quantity, str, interpretedExpr, simplifiedLine, rawSelection, m_evaluator);
            auto message = tr("Selection result:<br/>%1").arg(formatted);
            emit autoCalcMessageAvailable(message);
            emit autoCalcQuantityAvailable(quantity);
        }
    } else {
        if (isOperatorOnlyIncompleteInput(str)) {
            emit autoCalcDisabled();
            return;
        }

        QString baseExpression;
        if (EditorUtils::expressionWithoutIgnorableTrailingToken(str, &baseExpression)) {
            m_evaluator->setExpression(baseExpression);
            const auto baseQuantity = m_evaluator->evalNoAssign();
            if (m_evaluator->error().isEmpty()) {
                const QString interpretedExpr = m_evaluator->interpretedExpression();
                const QString simplifiedLine = (!baseQuantity.isNan() && !m_evaluator->isUserFunctionAssign()
                    && !Evaluator::isCommentOnlyExpression(baseExpression))
                    ? simplifiedExpressionLineForTooltip(interpretedExpr, rawSelection, m_evaluator)
                    : QString();
                if (baseQuantity.isNan() && (m_evaluator->isUserFunctionAssign()
                    || Evaluator::isCommentOnlyExpression(baseExpression))) {
                    auto message = tr("Selection result: n/a");
                    emit autoCalcMessageAvailable(message);
                } else {
                    const auto formatted =
                        formattedLiveResultWithAlternatives(
                            baseQuantity, baseExpression, interpretedExpr, simplifiedLine, rawSelection, m_evaluator);
                    auto message = tr("Selection result:<br/>%1").arg(formatted);
                    emit autoCalcMessageAvailable(message);
                    emit autoCalcQuantityAvailable(baseQuantity);
                }
                return;
            }
        }
        auto message = tr("Selection result: %1").arg(m_evaluator->error());
        emit autoCalcMessageAvailable(message);
    }
}

void Editor::insertConstant(const QString& constant)
{
    auto formattedConstant = constant;
    if (radixChar() == MathDsl::CommaSep)
        formattedConstant.replace(MathDsl::DotSep, MathDsl::CommaSep);
    formattedConstant = DisplayFormatUtils::applyValueUnitSpacingForDisplay(formattedConstant);
    if (!constant.isNull())
        insert(formattedConstant);
    if (m_constantCompletion) {
        disconnect(m_constantCompletion);
        m_constantCompletion->deleteLater();
        m_constantCompletion = 0;
    }
}

void Editor::cancelConstantCompletion()
{
    if (m_constantCompletion) {
        disconnect(m_constantCompletion);
        m_constantCompletion->deleteLater();
        m_constantCompletion = 0;
    }
}

void Editor::evaluate()
{
    triggerEnter();
}

void Editor::paintEvent(QPaintEvent* event)
{
    QPlainTextEdit::paintEvent(event);

    if (!m_customCursorVisible) {
        m_shouldPaintCustomCursor = false;
        return;
    }

    if (!m_shouldPaintCustomCursor) {
        m_shouldPaintCustomCursor = true;
        return;
    }
    m_shouldPaintCustomCursor = false;

    QRect cursor = cursorRect();
    cursor.setLeft(cursor.left() - 1);
    cursor.setRight(cursor.right() + 1);

    QPainter painter(viewport());
    painter.fillRect(cursor, m_highlighter->colorForRole(ColorScheme::Cursor));
}

void Editor::historyBack()
{
    if (!m_history.count())
        return;
    if (!m_currentHistoryIndex)
        return;

    m_shouldBlockAutoCompletionOnce = true;
    if (m_currentHistoryIndex == m_history.count())
        m_savedCurrentEditor = toPlainText();
    --m_currentHistoryIndex;
    setText(m_history.at(m_currentHistoryIndex).expr());
    moveCursorToEnd(this);
    ensureCursorVisible();
}

void Editor::historyForward()
{
    if (!m_history.count())
        return;
    if (m_currentHistoryIndex == m_history.count())
        return;

    m_shouldBlockAutoCompletionOnce = true;
    m_currentHistoryIndex++;
    if (m_currentHistoryIndex == m_history.count())
        setText(m_savedCurrentEditor);
    else
        setText(m_history.at(m_currentHistoryIndex).expr());
    moveCursorToEnd(this);
    ensureCursorVisible();
}

void Editor::triggerEnter()
{
    m_completionTimer->stop();
    m_matchingTimer->stop();
    m_currentHistoryIndex = m_history.count();
    emit returnPressed();
}

void Editor::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::FontChange)
        updateHeightForWrappedText();
    QPlainTextEdit::changeEvent(event);
}

void Editor::resizeEvent(QResizeEvent* event)
{
    QPlainTextEdit::resizeEvent(event);
    updateHeightForWrappedText();
}

void Editor::focusOutEvent(QFocusEvent* event)
{
    m_shouldPaintCustomCursor = false;
    QPlainTextEdit::focusOutEvent(event);
}

void Editor::inputMethodEvent(QInputMethodEvent* event)
{
    const QString normalizedCommit = normalizeExpressionTypedInEditor(event->commitString());
    const QString normalizedPreedit = normalizeExpressionTypedInEditor(event->preeditString());
    const int cursorPosition = textCursor().position();
    const bool squareBracketContext = isInsideUnmatchedSquareBracketContext(
        text(),
        cursorPosition);
    const QChar prev = previousNonSpaceChar(text(), cursorPosition);
    const bool autoAnsEnabled = Settings::instance()->autoAns;
    const bool atExpressionStart =
        !textCursor().hasSelection()
        && hasOnlyWhitespaceToLeft(text(), cursorPosition);

    if (atExpressionStart) {
        // Mirror keyPressEvent start-of-expression filtering for IME/dead-key
        // commits and preedit text; otherwise composed symbols can still appear.
        if (!normalizedCommit.isEmpty()) {
            const bool allAllowed = std::all_of(
                normalizedCommit.cbegin(),
                normalizedCommit.cend(),
                [autoAnsEnabled](const QChar& ch) {
                    return EditorUtils::isAllowedLeadingCharAtExpressionStart(ch, autoAnsEnabled);
                });
            if (!allAllowed) {
                event->accept();
                return;
            }
        }
        if (normalizedCommit.isEmpty() && !normalizedPreedit.isEmpty()) {
            const bool allAllowedPreedit = std::all_of(
                normalizedPreedit.cbegin(),
                normalizedPreedit.cend(),
                [autoAnsEnabled](const QChar& ch) {
                    return EditorUtils::isAllowedLeadingCharAtExpressionStart(ch, autoAnsEnabled);
                });
            if (!allAllowedPreedit) {
                QInputMethodEvent clearEvent;
                QPlainTextEdit::inputMethodEvent(&clearEvent);
                event->accept();
                return;
            }
        }
    }

    if (event->commitString().isEmpty()
        && textContainsOnlyCaretOperators(normalizedPreedit)) {
        if (isCaretOperatorAlias(prev)) {
            // Block dead-key/preedit caret echoes after an existing caret.
            // Send an empty IME update so any pending preedit is cleared.
            QInputMethodEvent clearEvent;
            QPlainTextEdit::inputMethodEvent(&clearEvent);
            event->accept();
            return;
        }
        // On layouts that use dead-caret preedit (e.g. PT), remember this
        // so the next committed digit/minus can be converted to superscript
        // even if preedit is not carried over to the commit event.
        if (squareBracketContext) {
            const bool validExponentBase =
                prev.isLetterOrNumber()
                || prev == MathDsl::GroupEnd
                || prev == MathDsl::UnitEnd;
            m_pendingDeadCaretPreedit = validExponentBase;
        } else {
            m_pendingDeadCaretPreedit = true;
        }
        event->accept();
        return;
    }

    if (textContainsOnlyCaretOperators(normalizedCommit)) {
        if (squareBracketContext && !isValidUnitExponentBase(prev)) {
            event->accept();
            return;
        }
        const bool prevIsBlockingOperator =
            isAnyAdditionOperator(prev)
            || MathDsl::isSubtractionOperatorAlias(prev)
            || MathDsl::isDivisionOperatorAlias(prev)
            || isAnyMultiplicationOperator(prev);
        if (prevIsBlockingOperator) {
            event->accept();
            return;
        }
        if (isCaretOperatorAlias(prev)
            || MathDsl::isSuperscriptPowerChar(prev)) {
            event->accept();
            return;
        }
    }

    if (normalizedCommit.size() == 1
        && !textCursor().hasSelection()
        && !(QApplication::keyboardModifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
        const QChar typed = normalizedCommit.at(0);
        const bool isTypedDigit = typed.isDigit();
        const bool isTypedMinus = MathDsl::isSubtractionOperatorAlias(typed)
                                  || typed == MathDsl::PowNeg;
        if (isTypedDigit || isTypedMinus) {
            const int cursorPos = textCursor().position();
            const int prevIndex = previousNonSpaceIndex(text(), cursorPos);
            const QChar prevChar = prevIndex >= 0 ? text().at(prevIndex) : QChar();
            const int baseIndex = previousNonSpaceIndex(text(), prevIndex);
            const QChar baseBeforeCaret = baseIndex >= 0 ? text().at(baseIndex) : QChar();
            const bool preeditCarriesCaret = textContainsOnlyCaretOperators(normalizedPreedit);
            const bool afterCaretRaw = isCaretOperatorAlias(prevChar);
            const bool afterCaretFromIme = preeditCarriesCaret || m_pendingDeadCaretPreedit;
            const bool afterCaret = afterCaretRaw || afterCaretFromIme;
            const bool continuingSuperscript =
                prevChar == MathDsl::PowNeg
                || MathDsl::isSuperscriptDigit(prevChar);
            const bool chainedAfterSuperscriptBase =
                afterCaretRaw && MathDsl::isSuperscriptPowerChar(baseBeforeCaret);

            if (squareBracketContext && afterCaretFromIme && !afterCaretRaw) {
                const bool validExponentBase =
                    prevChar.isLetterOrNumber()
                    || prevChar == MathDsl::GroupEnd
                    || prevChar == MathDsl::UnitEnd;
                if (!validExponentBase) {
                    event->accept();
                    return;
                }
            }

            const bool invalidSuperscriptMinus =
                isTypedMinus
                && (!afterCaret || chainedAfterSuperscriptBase || continuingSuperscript);

            if ((afterCaret || continuingSuperscript) && invalidSuperscriptMinus) {
                // Do not let raw IME minus/superscript-minus commit leak through
                // while inside an existing superscript exponent chain.
                event->accept();
                return;
            }

            if ((afterCaret || continuingSuperscript) && !invalidSuperscriptMinus) {
                const QChar superscript = isTypedDigit
                    ? MathDsl::asciiDigitToSuperscript(typed)
                    : MathDsl::PowNeg;
                if (!superscript.isNull()) {
                    QTextCursor cursor = textCursor();
                    if (isCaretOperatorAlias(prevChar)) {
                        cursor.setPosition(prevIndex);
                        cursor.setPosition(prevIndex + 1, QTextCursor::KeepAnchor);
                        cursor.insertText(QString(superscript));
                    } else {
                        cursor.insertText(QString(superscript));
                    }
                    setTextCursor(cursor);
                    m_pendingDeadCaretPreedit = false;
                    event->accept();
                    return;
                }
            }
        }
    }
    if (!normalizedCommit.isEmpty())
        m_pendingDeadCaretPreedit = false;

    if (textContainsOnlyAdditionAliases(normalizedCommit)) {
        if (squareBracketContext) {
            event->accept();
            return;
        }

        const bool prevIsBlockingOperator =
            isAnyAdditionOperator(prev)
            || MathDsl::isSubtractionOperatorAlias(prev)
            || MathDsl::isDivisionOperatorAlias(prev)
            || isAnyMultiplicationOperator(prev)
            || prev == MathDsl::PowOp;

        if (prevIsBlockingOperator) {
            event->accept();
            return;
        }

        const QString singlePlusAdjusted =
            EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
                text(),
                cursorPosition,
                QString(MathDsl::AddOp));
        if (singlePlusAdjusted.isEmpty()) {
            event->accept();
            return;
        }

        QInputMethodEvent normalizedEvent(event->preeditString(), event->attributes());
        normalizedEvent.setCommitString(singlePlusAdjusted,
                                        event->replacementStart(),
                                        event->replacementLength());
        QPlainTextEdit::inputMethodEvent(&normalizedEvent);
        return;
    }

    if (normalizedCommit.size() == 1
        && MathDsl::isSubtractionOperatorAlias(normalizedCommit.at(0))
        && !squareBracketContext) {
        if (!Settings::instance()->autoAns
            && textContainsOnlySubtractionAliases(text())) {
            event->accept();
            return;
        }
        const bool prevIsBlockingOperator =
            isAnyAdditionOperator(prev)
            || MathDsl::isSubtractionOperatorAlias(prev)
            || MathDsl::isDivisionOperatorAlias(prev)
            || isAnyMultiplicationOperator(prev)
            || prev == MathDsl::PowOp;
        if (prevIsBlockingOperator) {
            event->accept();
            return;
        }
    }

    if (squareBracketContext && !normalizedCommit.isEmpty()) {
        const QString adjusted = normalizeTypedTextForSquareBracketContext(
            text(),
            cursorPosition,
            normalizedCommit);
        if (adjusted.isEmpty()) {
            event->accept();
            return;
        }
        if (adjusted != event->commitString()) {
            QInputMethodEvent normalizedEvent(event->preeditString(), event->attributes());
            normalizedEvent.setCommitString(adjusted,
                                            event->replacementStart(),
                                            event->replacementLength());
            QPlainTextEdit::inputMethodEvent(&normalizedEvent);
            return;
        }
    }

    if (normalizedCommit == event->commitString()) {
        QPlainTextEdit::inputMethodEvent(event);
        return;
    }

    QInputMethodEvent normalizedEvent(event->preeditString(), event->attributes());
    normalizedEvent.setCommitString(normalizedCommit, event->replacementStart(), event->replacementLength());
    QPlainTextEdit::inputMethodEvent(&normalizedEvent);
}

void Editor::keyPressEvent(QKeyEvent* event)
{
    int key = event->key();
    const int cursorPosition = textCursor().position();
    const bool squareBracketContext = isInsideUnmatchedSquareBracketContext(
        text(),
        cursorPosition);
    const bool rightOfOpeningSquareBracket = squareBracketContext
        && isRightOfOpeningSquareBracketWithOnlySpaces(text(), cursorPosition);
    const bool emptyOrOnlySpacesUnitContext = squareBracketContext
        && isCurrentUnitContextEmptyOrOnlySpaces(text(), cursorPosition);

    if (squareBracketContext
        && isDeadKey(key)
        && key != Qt::Key_Dead_Circumflex
        && key != Qt::Key_Dead_Acute
        && key != Qt::Key_Dead_Diaeresis) {
        event->accept();
        return;
    }

    const auto implicitMulPrefixForTypedChar = [this](QChar typedChar) {
        const QString typedText(typedChar);
        const QString adjusted =
            EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
                text(),
                textCursor().position(),
                typedText);
        if (adjusted.size() > typedText.size() && adjusted.endsWith(typedText))
            return adjusted.left(adjusted.size() - typedText.size());
        return QString();
    };

    const QString normalizedEventText = normalizeExpressionTypedInEditor(event->text());
    const auto isOperatorLikeSingleCharInput = [](const QString& input) {
        if (input.size() != 1)
            return false;
        const QChar ch = input.at(0);
        return EditorUtils::isAnyOperator(ch)
               || MathDsl::isAdditionOperatorAlias(ch)
               || MathDsl::isSubtractionOperatorAlias(ch)
               || MathDsl::isDivisionOperatorAlias(ch)
               || MathDsl::isMultiplicationOperatorAlias(ch, true);
    };

    if (emptyOrOnlySpacesUnitContext
        && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        && (key == Qt::Key_Space
            || isAnyOperatorKey(key)
            || isOperatorLikeSingleCharInput(event->text())
            || isOperatorLikeSingleCharInput(normalizedEventText))) {
        event->accept();
        return;
    }

    const bool hasPrintableTextPayload =
        !event->text().isEmpty()
        && key < Qt::Key_Escape
        && std::all_of(event->text().cbegin(), event->text().cend(),
                       [](const QChar& ch) { return ch.isPrint(); });
    if (isInsideCommentFromQuestionMark(text(), cursorPosition)
        && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        && hasPrintableTextPayload) {
        QPlainTextEdit::keyPressEvent(event);
        return;
    }
    if (!textCursor().hasSelection()
        && hasOnlyWhitespaceToLeft(text(), cursorPosition)) {
        const bool autoAnsEnabledForStart = Settings::instance()->autoAns;
        // When "Auto-insert ans..." is off, treat expression start as a strict
        // allowlist gate. This blocks layout-specific AltGr/dead-key symbols
        // from bypassing leading-operator restrictions.
        //
        // When it is on, apply the same gate but allow +/*//^ starters too so
        // main-window auto-ans rewrite can still trigger.
        const QString typedCandidate =
            !normalizedEventText.isEmpty() ? normalizedEventText : event->text();
        if (!typedCandidate.isEmpty()) {
            const bool allAllowed = std::all_of(
                typedCandidate.cbegin(),
                typedCandidate.cend(),
                [autoAnsEnabledForStart](const QChar& ch) {
                    return ch.isSpace()
                           || ch == MathDsl::DotSep
                           || ch == MathDsl::CommaSep
                           || EditorUtils::isAllowedLeadingCharAtExpressionStart(ch, autoAnsEnabledForStart);
                });
            if (!allAllowed) {
                event->accept();
                return;
            }
        } else if (isDeadKey(key)
                   && key != Qt::Key_Dead_Tilde
                   && (key != Qt::Key_Dead_Circumflex || !autoAnsEnabledForStart)) {
            event->accept();
            return;
        }
    }

    const auto tryHandleSuperscriptExponentTyping = [&]() -> bool {
        if ((event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
            || textCursor().hasSelection()
            || normalizedEventText.size() != 1) {
            return false;
        }

        const QChar typed = normalizedEventText.at(0);
        const bool isTypedDigit = typed.isDigit();
        const bool isTypedMinus = MathDsl::isSubtractionOperatorAlias(typed)
                                  || typed == MathDsl::PowNeg;
        if (!isTypedDigit && !isTypedMinus)
            return false;

        const int cursorPos = textCursor().position();
        const int prevIndex = previousNonSpaceIndex(text(), cursorPos);
        if (prevIndex < 0)
            return false;
        const QChar prev = text().at(prevIndex);
        const int baseIndex = previousNonSpaceIndex(text(), prevIndex);
        const QChar baseBeforeCaret = baseIndex >= 0 ? text().at(baseIndex) : QChar();
        const bool afterCaret = isCaretOperatorAlias(prev);
        const bool continuingSuperscript = prev == MathDsl::PowNeg
                                           || MathDsl::isSuperscriptDigit(prev);
        const bool chainedAfterSuperscriptBase =
            afterCaret && MathDsl::isSuperscriptPowerChar(baseBeforeCaret);

        if (!afterCaret && !continuingSuperscript)
            return false;
        if (isTypedMinus && (!afterCaret || chainedAfterSuperscriptBase || continuingSuperscript))
            return false;

        const QChar superscript = isTypedDigit
            ? MathDsl::asciiDigitToSuperscript(typed)
            : MathDsl::PowNeg;
        if (superscript.isNull())
            return false;

        QTextCursor cursor = textCursor();
        if (afterCaret) {
            cursor.setPosition(prevIndex);
            cursor.setPosition(prevIndex + 1, QTextCursor::KeepAnchor);
            cursor.insertText(QString(superscript));
        } else {
            cursor.insertText(QString(superscript));
        }
        setTextCursor(cursor);
        event->accept();
        return true;
    };
    if (tryHandleSuperscriptExponentTyping())
        return;

    const auto tryHandleTypedGroupStart = [&]() -> bool {
        if (textCursor().hasSelection())
            return false;

        QTextCursor cursor = textCursor();
        const int position = cursor.position();
        const bool shouldAutoInsertGroupEnd = hasOnlySpacesToRight(text(), position);
        const QString insertedGroupStart = QString(MathDsl::GroupStart);
        const QString insertedGroupPair =
            QString(MathDsl::GroupStart) + QString(MathDsl::GroupEnd);
        cursor.insertText(shouldAutoInsertGroupEnd ? insertedGroupPair : insertedGroupStart);
        cursor.setPosition(position + 1);
        setTextCursor(cursor);
        event->accept();
        return true;
    };

    if (key == Qt::Key_Dead_Circumflex) {
        // PT and similar layouts emit a dead-circumflex key before commit.
        // Consume it when a caret is already on the left to avoid a second
        // pending caret from composition.
        const QChar prev = previousNonSpaceChar(text(), cursorPosition);
        if (squareBracketContext && !isValidUnitExponentBase(prev)) {
            event->accept();
            return;
        }
        if (isCaretOperatorAlias(prev)) {
            event->accept();
            return;
        }
        const bool prevIsBlockingOperator =
            isAnyAdditionOperator(prev)
            || MathDsl::isSubtractionOperatorAlias(prev)
            || MathDsl::isDivisionOperatorAlias(prev)
            || isAnyMultiplicationOperator(prev);
        if (prevIsBlockingOperator) {
            event->accept();
            return;
        }
        insert(QStringLiteral("^"));
        event->accept();
        return;
    }

    const bool isTypedTilde =
        key == Qt::Key_Dead_Tilde
        || key == Qt::Key_AsciiTilde
        || event->text() == QLatin1String("~")
        || normalizedEventText == QLatin1String("~");
    if (isTypedTilde) {
        if (squareBracketContext) {
            event->accept();
            return;
        }

        const QChar prev = previousNonSpaceChar(text(), cursorPosition);
        const bool prevAllowsUnaryTilde =
            prev.isNull()
            || prev == MathDsl::GroupStart
            || prev == MathDsl::UnitStart
            || isAnyAdditionOperator(prev)
            || MathDsl::isSubtractionOperatorAlias(prev)
            || MathDsl::isDivisionOperatorAlias(prev)
            || isAnyMultiplicationOperator(prev)
            || isCaretOperatorAlias(prev);
        if (prevAllowsUnaryTilde) {
            insert(QStringLiteral("~"));
        }
        event->accept();
        return;
    }

    if (textContainsOnlyAdditionAliases(event->text())
        || textContainsOnlyAdditionAliases(normalizedEventText)) {
        if (squareBracketContext) {
            event->accept();
            return;
        }

        const QChar prev = previousNonSpaceChar(text(), cursorPosition);
        const bool prevIsBlockingOperator =
            isAnyAdditionOperator(prev)
            || MathDsl::isSubtractionOperatorAlias(prev)
            || MathDsl::isDivisionOperatorAlias(prev)
            || isAnyMultiplicationOperator(prev)
            || prev == MathDsl::PowOp;
        if (prevIsBlockingOperator) {
            event->accept();
            return;
        }

        const QString singlePlusAdjusted =
            EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
                text(),
                textCursor().position(),
                QString(MathDsl::AddOp));
        if (!singlePlusAdjusted.isEmpty())
            insert(singlePlusAdjusted);
        event->accept();
        return;
    }

    const bool isTypedPlus =
        key == Qt::Key_Plus
        || (key == Qt::Key_Equal && (event->modifiers() & Qt::ShiftModifier))
        || textContainsOnlyAdditionAliases(event->text())
        || textContainsOnlyAdditionAliases(normalizedEventText);
    if (isTypedPlus) {
        if (squareBracketContext) {
            event->accept();
            return;
        }

        const QChar prev = previousNonSpaceChar(text(), cursorPosition);
        if (isAnyAdditionOperator(prev)
            || MathDsl::isSubtractionOperatorAlias(prev)
            || MathDsl::isDivisionOperatorAlias(prev)
            || isAnyMultiplicationOperator(prev)
            || prev == MathDsl::PowOp) {
            event->accept();
            return;
        }
    }

    const bool isTypedCaret =
        key == Qt::Key_AsciiCircum
        || textContainsOnlyCaretOperators(event->text())
        || textContainsOnlyCaretOperators(normalizedEventText);
    if (isTypedCaret) {
        const QChar prev = previousNonSpaceChar(text(), cursorPosition);
        if (squareBracketContext && !isValidUnitExponentBase(prev)) {
            event->accept();
            return;
        }
        const bool prevIsBlockingOperator =
            isAnyAdditionOperator(prev)
            || MathDsl::isSubtractionOperatorAlias(prev)
            || MathDsl::isDivisionOperatorAlias(prev)
            || isAnyMultiplicationOperator(prev);
        if (prevIsBlockingOperator) {
            event->accept();
            return;
        }
        if (isCaretOperatorAlias(prev)
            || MathDsl::isSuperscriptPowerChar(prev)) {
            event->accept();
            return;
        }
    }

    if (squareBracketContext
        && (textContainsOnlyDivisionAliases(event->text())
            || textContainsOnlyDivisionAliases(normalizedEventText))) {
        rewriteTrailingAsciiUnitExponentToSuperscript(this, QString());
        const int cursorPos = textCursor().position();
        const QChar previous = previousNonSpaceChar(text(), cursorPos);
        if (MathDsl::isDivisionOperatorAlias(previous)) {
            int previousIndex = qBound(0, cursorPos, text().size()) - 1;
            while (previousIndex >= 0 && text().at(previousIndex).isSpace())
                --previousIndex;
            if (previousIndex >= 1 && text().at(previousIndex - 1) == MathDsl::PowOp) {
                auto cursor = textCursor();
                cursor.setPosition(previousIndex + 1);
                cursor.deletePreviousChar();
                setTextCursor(cursor);
                event->accept();
                return;
            }
        }
        const QString adjusted = normalizeTypedTextForSquareBracketContext(
            text(),
            textCursor().position(),
            QString(MathDsl::DivOp));
        if (!adjusted.isEmpty())
            insert(adjusted);
        event->accept();
        return;
    }
    if (squareBracketContext
        && (event->text() == QLatin1String("]") || key == Qt::Key_BracketRight)
        && !textCursor().hasSelection()) {
        const QString currentText = text();
        const int cursorPos = textCursor().position();
        const int openPos = currentText.lastIndexOf(MathDsl::UnitStart, cursorPos - 1);
        if (openPos >= 0) {
            QTextCursor cursor = textCursor();
            const QString unitBody = currentText.mid(openPos + 1, cursorPos - (openPos + 1));
            const QString normalizedBody = renderUnitAsciiExponentsAsSuperscripts(unitBody);
            cursor.setPosition(openPos + 1);
            cursor.setPosition(cursorPos, QTextCursor::KeepAnchor);
            cursor.insertText(normalizedBody);
            cursor.insertText(QStringLiteral("]"));
            setTextCursor(cursor);
            event->accept();
            return;
        }
    }
    if (squareBracketContext
        && !normalizedEventText.isEmpty()
        && std::all_of(normalizedEventText.cbegin(), normalizedEventText.cend(), isTypedMultiplicationCharacter)) {
        rewriteTrailingAsciiUnitExponentToSuperscript(this, QString());
    }

    const bool isTypedGroupStart =
        key == Qt::Key_ParenLeft
        || event->text() == QLatin1String("(")
        || normalizedEventText == QLatin1String("(");
    if (isTypedGroupStart && tryHandleTypedGroupStart())
        return;

    const bool isTypedRadixSeparator =
        key == Qt::Key_Period
        || key == Qt::Key_Comma
        || event->text() == QLatin1String(".")
        || event->text() == QLatin1String(",")
        || normalizedEventText == QLatin1String(".")
        || normalizedEventText == QLatin1String(",");
    if (isTypedRadixSeparator) {
        if (!(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            const QString radix = QString(QChar(this->radixChar()));
            if (rewriteTrailingSuperscriptExponentToParenthesizedAscii(this, radix)) {
                event->accept();
                return;
            }
            const QChar prev = previousNonSpaceChar(text(), textCursor().position());
            if (prev == QChar(this->radixChar())
                || prev == MathDsl::DotSep
                || prev == MathDsl::CommaSep) {
                event->accept();
                return;
            }
            const bool startsNewDecimalAfterOperator =
                prev.isNull()
                || prev == MathDsl::AddOp
                || MathDsl::isSubtractionOperatorAlias(prev)
                || MathDsl::isMultiplicationOperator(prev)
                || MathDsl::isMultiplicationOperatorAlias(prev, true)
                || MathDsl::isDivisionOperator(prev)
                || MathDsl::isDivisionOperatorAlias(prev)
                || prev == MathDsl::PercentOp
                || prev == MathDsl::PowOp
                || prev == MathDsl::BitAndOp
                || prev == MathDsl::BitOrOp
                || prev == MathDsl::Equals
                || prev == MathDsl::LessThanOp
                || prev == MathDsl::GreaterThanOp;
            if (startsNewDecimalAfterOperator) {
                insert(QStringLiteral("0") + radix);
                event->accept();
                return;
            }
            if (prev == MathDsl::GroupEnd
                || prev == MathDsl::UnitEnd
                || prev.isLetter()) {
                QString prefix = MathDsl::buildWrappedToken(MathDsl::MulCrossOp, MathDsl::MulCrossWrapSp);
                const int pos = textCursor().position();
                if (pos > 0 && text().at(pos - 1).isSpace() && !prefix.isEmpty() && prefix.at(0).isSpace())
                    prefix.remove(0, 1);
                insert(prefix + QStringLiteral("0") + radix);
                event->accept();
                return;
            }
            if (!prev.isDigit()) {
                event->accept();
                return;
            }
        }
        if (event->modifiers() == Qt::KeypadModifier) {
            insert(QChar(this->radixChar()));
            event->accept();
            return;
        }
    }

    const QChar typedForRules = normalizedTypedCharFromEvent(event, normalizedEventText);
    if (key != Qt::Key_Enter
        && key != Qt::Key_Return
        && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        && !textCursor().hasSelection()
        && !typedForRules.isNull()) {
        const QChar prev = previousNonSpaceChar(text(), cursorPosition);
        const auto isGroupStartLetterDigitOrCurrency = [](const QChar& ch) {
            return ch == MathDsl::GroupStart
                || ch == MathDsl::ListStart
                || ch.isLetter()
                || ch.isDigit()
                || isCurrencySymbolChar(ch);
        };

        if (typedForRules == MathDsl::GreaterThanOp
            && MathDsl::isSubtractionOperatorAlias(prev)) {
            replacePreviousSubtractionAtCursorWithUnitConversion(this);
            event->accept();
            return;
        }
        if (typedForRules == MathDsl::SubOp
            && MathDsl::isSubtractionOperatorAlias(prev)) {
            const int prevIndex = previousNonSpaceIndex(text(), cursorPosition);
            const int beforePrevIndex = previousNonSpaceIndex(text(), prevIndex);
            if (beforePrevIndex < 0 || !canEndUnitConversionLeftOperand(text().at(beforePrevIndex))) {
                event->accept();
                return;
            }
            replacePreviousSubtractionAtCursorWithUnitConversion(this);
            event->accept();
            return;
        }
        if (typedForRules == MathDsl::TransOp && !squareBracketContext) {
            insertUnitConversionAtCursorWithUnitPlaceholder(this);
            event->accept();
            return;
        }
        if (typedForRules == MathDsl::LessThanOp || typedForRules == MathDsl::GreaterThanOp) {
            const int pos = textCursor().position();
            const QString wrappedShift =
                typedForRules == MathDsl::LessThanOp ? wrappedShiftLeftToken()
                                                     : wrappedShiftRightToken();
            if (pos >= wrappedShift.size()
                && text().mid(pos - wrappedShift.size(), wrappedShift.size()) == wrappedShift) {
                event->accept();
                return;
            }
            if (isBlockingBinaryOperator(prev)) {
                event->accept();
                return;
            }
        }

        if (MathDsl::isSubtractionOperatorAlias(prev)
            || isAnyAdditionOperator(prev)
            || MathDsl::isDivisionOperatorAlias(prev)) {
            const bool isRadixSeparator =
                typedForRules == MathDsl::DotSep || typedForRules == MathDsl::CommaSep;
            const bool isUnitConversionTail =
                MathDsl::isSubtractionOperatorAlias(prev)
                && typedForRules == MathDsl::GreaterThanOp;
            if (!isGroupStartLetterDigitOrCurrency(typedForRules)
                && !isRadixSeparator
                && !isUnitConversionTail) {
                event->accept();
                return;
            }
        } else if (isCaretOperatorAlias(prev)) {
            if (!(MathDsl::isSubtractionOperatorAlias(typedForRules)
                  || typedForRules == MathDsl::GroupStart
                  || typedForRules == MathDsl::ListStart
                  || typedForRules.isLetter()
                  || typedForRules.isDigit()
                  || isCurrencySymbolChar(typedForRules))) {
                event->accept();
                return;
            }
        } else if (isAnyMultiplicationOperator(prev)) {
            if (typedForRules == MathDsl::GroupStart
                || typedForRules == MathDsl::ListStart
                || typedForRules.isLetter()
                || isCurrencySymbolChar(typedForRules)
                || (typedForRules.isDigit() && !squareBracketContext)) {
                // Allowed as-is.
            } else if (isAnyMultiplicationOperator(typedForRules)) {
                const int prevIndex = previousNonSpaceIndex(text(), cursorPosition);
                const int beforePrevIndex = previousNonSpaceIndex(text(), prevIndex);
                if (beforePrevIndex >= 0 && isExponentTailBeforeIndex(text(), beforePrevIndex)) {
                    event->accept();
                    return;
                }
                replacePreviousOperatorAtCursorWithCaret(this);
                event->accept();
                return;
            } else {
                event->accept();
                return;
            }
        }
    }

    if (rightOfOpeningSquareBracket
        && (isAnyOperatorKey(key)
            || (event->text().size() == 1
                && EditorUtils::isAnyOperator(event->text().at(0)))
            || (normalizedEventText.size() == 1
                && EditorUtils::isAnyOperator(normalizedEventText.at(0))))) {
        event->accept();
        return;
    }

    if ((event->text() == QLatin1String("[") || key == Qt::Key_BracketLeft)
        && !textCursor().hasSelection()) {
        if (squareBracketContext) {
            event->accept();
            return;
        }
        const QChar previous = previousNonSpaceChar(text(), textCursor().position());
        if (previous == MathDsl::UnitEnd) {
            event->accept();
            return;
        }
        QTextCursor cursor = textCursor();
        const int position = cursor.position();
        const bool shouldInsertValueUnitSpace =
            previous == MathDsl::GroupEnd
            || !implicitMulPrefixForTypedChar(MathDsl::UnitStart).isEmpty();
        const QString prefix = shouldInsertValueUnitSpace
            ? QString(MathDsl::QuantSp)
            : QString();
        if (shouldInsertValueUnitSpace) {
            int left = position;
            while (left > 0 && text().at(left - 1).isSpace())
                --left;
            if (left < position) {
                cursor.setPosition(left);
                cursor.setPosition(position, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
        }
        const int insertionPosition = cursor.position();
        cursor.insertText(prefix + QStringLiteral("[]"));
        cursor.setPosition(insertionPosition + prefix.size() + 1);
        setTextCursor(cursor);
        event->accept();
        return;
    }

    switch (key) {
    case Qt::Key_Tab:
        // setTabChangesFocus() still allows entering a Tab character when
        // there's no other widgets to change focus to. To avoid that,
        // explicitly consume any Tabs that make it here.
        event->accept();
        return;

    case Qt::Key_Enter:
    case Qt::Key_Return:
        triggerEnter();
        event->accept();
        return;

    case Qt::Key_Escape:
        emit escapePressed();
        event->accept();
        return;

    case Qt::Key_Up:
        if (!m_historyArrowNavigationEnabled) {
            QPlainTextEdit::keyPressEvent(event);
            event->accept();
            return;
        }
        if (event->modifiers() & Qt::ShiftModifier)
            emit shiftUpPressed();
        else if (Settings::instance()->upDownArrowBehavior == Settings::UpDownArrowBehaviorAlways
                 || (Settings::instance()->upDownArrowBehavior == Settings::UpDownArrowBehaviorSingleLineOnly
                     && height() <= sizeHint().height()))
            historyBack();
        else
            QPlainTextEdit::keyPressEvent(event);
        event->accept();
        return;

    case Qt::Key_Down:
        if (!m_historyArrowNavigationEnabled) {
            QPlainTextEdit::keyPressEvent(event);
            event->accept();
            return;
        }
        if (event->modifiers() & Qt::ShiftModifier)
            emit shiftDownPressed();
        else if (Settings::instance()->upDownArrowBehavior == Settings::UpDownArrowBehaviorAlways
                 || (Settings::instance()->upDownArrowBehavior == Settings::UpDownArrowBehaviorSingleLineOnly
                     && height() <= sizeHint().height()))
            historyForward();
        else
            QPlainTextEdit::keyPressEvent(event);
        event->accept();
        return;

    case Qt::Key_PageUp:
        if (event->modifiers() & Qt::ShiftModifier)
            emit shiftPageUpPressed();
        else if (event->modifiers() & Qt::ControlModifier)
            emit controlPageUpPressed();
        else
            emit pageUpPressed();
        event->accept();
        return;

    case Qt::Key_PageDown:
        if (event->modifiers() & Qt::ShiftModifier)
            emit shiftPageDownPressed();
        else if (event->modifiers() & Qt::ControlModifier)
            emit controlPageDownPressed();
        else
            emit pageDownPressed();
        event->accept();
        return;

    case Qt::Key_Left:
    case Qt::Key_Right:
        {
            const bool isModifiedNavigation =
                event->modifiers() & (Qt::AltModifier | Qt::ControlModifier | Qt::MetaModifier);
            const bool keepAnchor = event->modifiers() & Qt::ShiftModifier;
            QTextCursor cursor = textCursor();
            if (!isModifiedNavigation && (keepAnchor || !cursor.hasSelection())) {
                // Plain left/right movement should keep grouped spaced
                // operators atomic, so single-step arrows do not stop
                // inside "<space><operator><space>".
                const int position = cursor.position();
                int newPosition = -1;
                if (key == Qt::Key_Left) {
                    const int groupedLength = groupedTokenLengthBefore(text(), position);
                    if (groupedLength > 0)
                        newPosition = position - groupedLength;
                    else {
                        const int groupedLengthAtNext = groupedTokenLengthBefore(text(), position + 1);
                        if (groupedLengthAtNext > 0)
                            newPosition = position - (groupedLengthAtNext - 1);
                    }
                } else if (key == Qt::Key_Right) {
                    const int groupedLength = groupedTokenLengthAfter(text(), position);
                    if (groupedLength > 0)
                        newPosition = position + groupedLength;
                    else if (position > 0) {
                        const int groupedLengthAtPrev = groupedTokenLengthAfter(text(), position - 1);
                        if (groupedLengthAtPrev > 0)
                            newPosition = position + (groupedLengthAtPrev - 1);
                    }
                }

                if (newPosition >= 0) {
                    cursor.setPosition(newPosition, keepAnchor ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
                    setTextCursor(cursor);
                    checkMatching();
                    if (textCursor().hasSelection())
                        checkSelectionAutoCalc();
                    else
                        checkAutoCalc();
                    event->accept();
                    return;
                }
            }
            const int oldPosition = textCursor().position();
            QPlainTextEdit::keyPressEvent(event);
            cursor = textCursor();
            int newPosition = cursor.position();
            if (isModifiedNavigation && newPosition == oldPosition) {
                // Some platforms/layouts may not move with Alt/Ctrl/Meta+arrows.
                // Still keep grouped spaced operators atomic when the cursor is
                // immediately before/after one.
                if (key == Qt::Key_Left) {
                    const int groupedLength = groupedTokenLengthBefore(text(), oldPosition);
                    if (groupedLength > 0)
                        newPosition = oldPosition - groupedLength;
                } else if (key == Qt::Key_Right) {
                    const int groupedLength = groupedTokenLengthAfter(text(), oldPosition);
                    if (groupedLength > 0)
                        newPosition = oldPosition + groupedLength;
                }
                if (newPosition != oldPosition) {
                    cursor.setPosition(newPosition, keepAnchor ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
                    setTextCursor(cursor);
                    checkMatching();
                    if (textCursor().hasSelection())
                        checkSelectionAutoCalc();
                    else
                        checkAutoCalc();
                    event->accept();
                    return;
                }
            }
            if (isModifiedNavigation && newPosition != oldPosition) {
                // Alt/Ctrl/Meta navigation uses native Qt word movement first.
                // Then, if that move lands at/inside a grouped spaced operator,
                // snap to the nearest valid side of the triplet to keep it
                // atomic without changing normal word-jump semantics.
                int groupLength = 0;
                const int groupStart = groupedTokenStartAround(text(), newPosition, &groupLength);
                if (groupStart >= 0) {
                    if (key == Qt::Key_Right && newPosition > oldPosition) {
                        // Use strict-side comparison so repeated Alt+Right does
                        // not get stuck at the same boundary.
                        newPosition = (oldPosition < groupStart)
                            ? groupStart
                            : groupStart + groupLength;
                    } else if (key == Qt::Key_Left && newPosition < oldPosition) {
                        // Use strict-side comparison so repeated Alt+Left keeps
                        // progressing past grouped operators.
                        newPosition = (oldPosition > groupStart + groupLength)
                            ? groupStart + groupLength
                            : groupStart;
                    }
                }
                if (newPosition != cursor.position()) {
                    cursor.setPosition(newPosition, keepAnchor ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
                    setTextCursor(cursor);
                }
            }
        }
        checkMatching();
        if (textCursor().hasSelection())
            checkSelectionAutoCalc();
        else
            checkAutoCalc();
        event->accept();
        return;

    case Qt::Key_Home:
    case Qt::Key_End:
        QPlainTextEdit::keyPressEvent(event);
        checkMatching();
        if (textCursor().hasSelection())
            checkSelectionAutoCalc();
        else
            checkAutoCalc();
        event->accept();
        return;

    case Qt::Key_Backspace:
        if (event->matches(QKeySequence::DeleteStartOfWord)) {
            // Preserve word-delete shortcuts (platform-dependent Alt/Ctrl+BS),
            // but still treat grouped spaced operators as a single unit.
            if (!textCursor().hasSelection()
                && isAfterGroupedSpacedOperator(text(), textCursor().position())) {
                doBackspace();
                event->accept();
                return;
            }
            QPlainTextEdit::keyPressEvent(event);
            event->accept();
            return;
        }
        doBackspace();
        event->accept();
        return;
    case Qt::Key_Delete:
        if (event->matches(QKeySequence::DeleteEndOfWord)) {
            // Preserve word-delete shortcuts (platform-dependent Alt/Ctrl+Del),
            // but still treat grouped spaced operators as a single unit.
            if (!textCursor().hasSelection()
                && isBeforeGroupedSpacedOperator(text(), textCursor().position())) {
                doDelete();
                event->accept();
                return;
            }
            QPlainTextEdit::keyPressEvent(event);
            event->accept();
            return;
        }
        doDelete();
        event->accept();
        return;

    case Qt::Key_Space:
        if (event->modifiers() == Qt::ControlModifier
            && !m_constantCompletion)
        {
            m_constantCompletion = new ConstantCompletion(this);
            connect(m_constantCompletion,
                    SIGNAL(selectedCompletion(const QString&)),
                    SLOT(insertConstant(const QString&)));
            connect(m_constantCompletion,
                    &ConstantCompletion::canceledCompletion,
                    this, &Editor::cancelConstantCompletion);
            m_constantCompletion->showCompletion();
            event->accept();
            return;
        }
        if (event->modifiers() == Qt::NoModifier) {
            const int pos = textCursor().position();
            if (pos > 0 && text().at(pos - 1).isSpace()) {
                event->accept();
                return;
            }
        }
        if (event->modifiers() == Qt::NoModifier
            && squareBracketContext) {
            if (appendSuffixAfterTrailingSuperscriptExponent(this, QString(MathDsl::MulDotOp))) {
                event->accept();
                return;
            }
            if (rewriteTrailingAsciiUnitExponentToSuperscript(this, QString(MathDsl::MulDotOp))) {
                event->accept();
                return;
            }
        }
        if (event->modifiers() == Qt::NoModifier && squareBracketContext) {
            const QString adjusted = normalizeTypedTextForSquareBracketContext(
                text(),
                textCursor().position(),
                QStringLiteral(" "));
            if (!adjusted.isEmpty())
                insert(adjusted);
            event->accept();
            return;
        }
        break;

    case Qt::Key_Asterisk: {
        if (squareBracketContext) {
            rewriteTrailingAsciiUnitExponentToSuperscript(this, QString());
            auto position = textCursor().position();
            const QChar prev = previousNonSpaceChar(text(), position);
            if (MathDsl::isSuperscriptPowerChar(prev)) {
                insert(QString(MathDsl::MulDotOp));
                event->accept();
                return;
            }
            const int opIndex = previousNonSpaceIndex(text(), position);
            const QChar op = opIndex >= 0 ? text().at(opIndex) : QChar();
            if (op == MathDsl::MulOpAl1
                || op == MathDsl::MulCrossOp
                || op == MathDsl::MulDotOp) {
                const int beforeOpIndex = previousNonSpaceIndex(text(), opIndex);
                if (beforeOpIndex >= 0
                    && isExponentTailBeforeIndex(text(), beforeOpIndex)) {
                    // Do not allow "**" to become "^" after superscript exponents.
                    event->accept();
                    return;
                }
                auto cursor = textCursor();
                cursor.removeSelectedText();
                cursor.deletePreviousChar();
                insert(QString::fromUtf8("^"));
            } else {
                insert(QString(MathDsl::MulDotOp));
            }
            event->accept();
            return;
        }
        auto position = textCursor().position();
        const QChar prev = previousNonSpaceChar(text(), position);
        if (MathDsl::isSuperscriptPowerChar(prev)) {
            insert(MathDsl::buildWrappedToken(MathDsl::MulDotOp, MathDsl::MulDotWrapSp));
            event->accept();
            return;
        }
        const int opIndex = previousNonSpaceIndex(text(), position);
        const QChar op = opIndex >= 0 ? text().at(opIndex) : QChar();
        if (op == MathDsl::MulOpAl1
            || op == MathDsl::MulCrossOp
            || op == MathDsl::MulDotOp) {
          const int beforeOpIndex = previousNonSpaceIndex(text(), opIndex);
          if (beforeOpIndex >= 0
              && isExponentTailBeforeIndex(text(), beforeOpIndex)) {
              event->accept();
              return;
          }
        }
        if (op == MathDsl::MulOpAl1 || op == MathDsl::MulCrossOp) {
          const int beforeOpIndex = previousNonSpaceIndex(text(), opIndex);
          if (beforeOpIndex >= 0
              && MathDsl::isSuperscriptPowerChar(text().at(beforeOpIndex))) {
              event->accept();
              return;
          }
          // Replace ×* by ^ operator
          auto cursor = textCursor();
          cursor.removeSelectedText();  // just in case some text is selected
          cursor.deletePreviousChar();
          insert(QString::fromUtf8("^"));
        } else {
          insert(EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
              text(),
              textCursor().position(),
              QString(MathDsl::MulCrossOp)));
        }
        event->accept();
        return;
    }

    case Qt::Key_Plus:
    case Qt::Key_Equal:
        if (key == Qt::Key_Equal && !(event->modifiers() & Qt::ShiftModifier)) {
            const QChar prev = previousNonSpaceChar(text(), textCursor().position());
            if (isAnyAdditionOperator(prev)
                || MathDsl::isSubtractionOperatorAlias(prev)
                || MathDsl::isDivisionOperatorAlias(prev)
                || isAnyMultiplicationOperator(prev)
                || prev == MathDsl::PowOp
                || prev == MathDsl::Equals) {
                event->accept();
                return;
            }
            insert(EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
                text(),
                textCursor().position(),
                QStringLiteral("=")));
            event->accept();
            return;
        }
        {
            const QChar prev = previousNonSpaceChar(text(), textCursor().position());
            if (isAnyAdditionOperator(prev)
                || MathDsl::isSubtractionOperatorAlias(prev)
                || MathDsl::isDivisionOperatorAlias(prev)
                || isAnyMultiplicationOperator(prev)
                || prev == MathDsl::PowOp) {
                event->accept();
                return;
            }
            insert(EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
                text(),
                textCursor().position(),
                QString(MathDsl::AddOp)));
            event->accept();
            return;
        }

    case Qt::Key_Minus:
        if (squareBracketContext) {
            const QString adjusted = normalizeTypedTextForSquareBracketContext(
                text(),
                textCursor().position(),
                QString(MathDsl::SubOp));
            if (!adjusted.isEmpty())
                insert(adjusted);
            event->accept();
            return;
        }
        if (!Settings::instance()->autoAns
            && textContainsOnlySubtractionAliases(text())) {
            event->accept();
            return;
        }
        {
            const QChar prev = previousNonSpaceChar(text(), textCursor().position());
            if (MathDsl::isSubtractionOperatorAlias(prev)
                || isAnyAdditionOperator(prev)
                || MathDsl::isDivisionOperatorAlias(prev)
                || isAnyMultiplicationOperator(prev)
                || prev == MathDsl::PowOp) {
                event->accept();
                return;
            }
        }
        insert(EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            text(),
            textCursor().position(),
            QString(MathDsl::SubOp)));
        event->accept();
        return;
    case Qt::Key_Slash:
        if (squareBracketContext) {
            const QString adjusted = normalizeTypedTextForSquareBracketContext(
                text(),
                textCursor().position(),
                QString(MathDsl::DivOp));
            if (!adjusted.isEmpty())
                insert(adjusted);
            event->accept();
            return;
        }
        break;
    case Qt::Key_At:
        insert(QString(MathDsl::Deg)); // U+00B0 ° DEGREE SIGN
        event->accept();
        return;
    case Qt::Key_ParenLeft:
        break;
    default:;
    }

    if (event->matches(QKeySequence::Copy)) {
        emit copySequencePressed();
        event->accept();
        return;
    }

    QString normalizedText = normalizedEventText;
    if (!(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        && normalizedText == QLatin1String("?")
        && !isInsideCommentFromQuestionMark(text(), textCursor().position())) {
        const int cursorPos = textCursor().position();
        const int prevIndex = cursorPos - 1;
        const bool needsLeftSpace = hasAnyNonWhitespace(text())
                                    && prevIndex >= 0
                                    && !text().at(prevIndex).isSpace();
        const bool needsRightSpace = cursorPos >= text().size() || !text().at(cursorPos).isSpace();

        normalizedText = QString();
        if (needsLeftSpace)
            normalizedText += QLatin1Char(' ');
        normalizedText += MathDsl::CommentSep;
        if (needsRightSpace)
            normalizedText += QLatin1Char(' ');
    }
    const QString implicitMulAdjustedText =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            text(),
            textCursor().position(),
            normalizedText);
    const QString contextAdjustedText = squareBracketContext
        ? normalizeTypedTextForSquareBracketContext(text(), textCursor().position(), implicitMulAdjustedText)
        : implicitMulAdjustedText;
    if (squareBracketContext && contextAdjustedText != implicitMulAdjustedText) {
        if (!contextAdjustedText.isEmpty())
            insert(contextAdjustedText);
        event->accept();
        return;
    }
    if (!contextAdjustedText.isEmpty() && contextAdjustedText != event->text()) {
        insert(contextAdjustedText);
        event->accept();
        return;
    }

    // For printable text input, always go through Editor::insert() so all
    // normalization and operator guards are consistently applied.
    if (!normalizedText.isEmpty()) {
        insert(contextAdjustedText.isEmpty() ? normalizedText : contextAdjustedText);
        event->accept();
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void Editor::scrollContentsBy(int dx, int dy)
{
    if (dy)
        return;
    QPlainTextEdit::scrollContentsBy(dx, dy);
    verticalScrollBar()->setMaximum(0);
    verticalScrollBar()->setMinimum(0);
}

void Editor::updateHeightForWrappedText()
{
    const int baseHeight = sizeHint().height();
    int visualLineCount = 0;

    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        const QTextLayout* layout = block.layout();
        if (!layout) {
            visualLineCount += 1;
            continue;
        }

        visualLineCount += std::max(1, layout->lineCount());
    }

    if (visualLineCount <= 0)
        visualLineCount = 1;

    const int clampedLines = std::max(1, std::min(5, visualLineCount));
    setFixedHeight(baseHeight * clampedLines);
}

void Editor::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() > 0)
        historyBack();
    else if (event->angleDelta().y() < 0)
        historyForward();
    event->accept();
}

void Editor::rehighlight()
{
    m_highlighter->update();
    auto color = m_highlighter->colorForRole(ColorScheme::EditorBackground);
    auto colorName = color.name();
    QPalette pal = palette();
    pal.setColor(QPalette::Active, QPalette::Base, color);
    pal.setColor(QPalette::Inactive, QPalette::Base, color);
    setPalette(pal);
    setStyleSheet(QString("QPlainTextEdit { background-color: %1; }").arg(colorName));
    viewport()->setStyleSheet(QString("background-color: %1;").arg(colorName));
    m_highlighter->rehighlight();
}

void Editor::updateHistory()
{
    const Session* session = m_session;
    if (session == nullptr)
        return;
    const int sessionHistoryCount = session->historySize();

    // Fast path for appending one new history entry.
    if (sessionHistoryCount == m_history.count() + 1) {
        m_history.append(session->historyEntryAtRef(sessionHistoryCount - 1));
        m_currentHistoryIndex = m_history.count();
        return;
    }

    // Fast path for history cap behavior: oldest entry dropped, newest appended.
    if (sessionHistoryCount == m_history.count() && sessionHistoryCount > 0) {
        const int count = sessionHistoryCount;
        if (count == 1) {
            m_history[0] = session->historyEntryAtRef(count - 1);
            m_currentHistoryIndex = m_history.count();
            return;
        }
        if (m_history.count() > 1
            && session->historyEntryAtRef(0).expr() == m_history.at(1).expr())
        {
            m_history.removeFirst();
            m_history.append(session->historyEntryAtRef(count - 1));
            m_currentHistoryIndex = m_history.count();
            return;
        }
    }

    m_history.clear();
    m_history.reserve(sessionHistoryCount);
    for (int i = 0; i < sessionHistoryCount; ++i)
        m_history.append(session->historyEntryAtRef(i));
    m_currentHistoryIndex = m_history.count();
}

void Editor::stopAutoCalc()
{
    emit autoCalcDisabled();
}

void Editor::stopAutoComplete()
{
    m_completionTimer->stop();
    m_completion->selectItem(QString());
    m_completion->doneCompletion();
    setFocus();
}

void Editor::wrapSelection()
{
    auto cursor = textCursor();
    if (cursor.hasSelection()) {
        const int selectionStart = cursor.selectionStart();
        const int selectionEnd = cursor.selectionEnd();
        cursor.setPosition(selectionStart);
        cursor.insertText(QString(MathDsl::GroupStart));
        cursor.setPosition(selectionEnd + 1);
        cursor.insertText(QString(MathDsl::GroupEnd));
    } else {
        cursor.movePosition(QTextCursor::Start);
        cursor.insertText(QString(MathDsl::GroupStart));
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(QString(MathDsl::GroupEnd));
    }
    setTextCursor(cursor);
}

EditorCompletion::EditorCompletion(Editor* editor)
    : QObject(editor)
{
    m_editor = editor;

    m_popup = new QTreeWidget();
    m_popup->setFrameShape(QFrame::NoFrame);
    m_popup->setColumnCount(3);
    m_popup->setRootIsDecorated(false);
    m_popup->header()->hide();
    m_popup->header()->setStretchLastSection(false);
    m_popup->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_popup->setSelectionBehavior(QTreeWidget::SelectRows);
    m_popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_popup->setMouseTracking(true);
    m_popup->installEventFilter(this);

    connect(m_popup, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
            SLOT(doneCompletion()));

    m_popup->hide();
    m_popup->setParent(editor->window(), Qt::Popup);
    m_popup->setFocusPolicy(Qt::NoFocus);
    m_popup->setFocusProxy(editor);
    m_popup->setFrameStyle(QFrame::Box | QFrame::Plain);
}

EditorCompletion::~EditorCompletion()
{
    // Popup ownership is handled by Qt parent-child deletion (its parent is
    // the window). Deleting it manually here can double-delete during shutdown.
}

bool EditorCompletion::eventFilter(QObject* object, QEvent* event)
{
    if (object != m_popup)
        return false;

    if (event->type() == QEvent::KeyPress) {
        int key = static_cast<QKeyEvent*>(event)->key();

        switch (key) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            if (m_popupInteracted) {
                doneCompletion();
                return true;
            }

            m_popup->hide();
            m_editor->setFocus();
            QMetaObject::invokeMethod(m_editor, "triggerEnter", Qt::DirectConnection);
            return true;

        case Qt::Key_Tab:
            doneCompletion();
            return true;

        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Home:
        case Qt::Key_End:
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            m_popupInteracted = true;
            return false;

        default:
            m_popup->hide();
            m_editor->setFocus();
            if (key != Qt::Key_Escape)
                QApplication::sendEvent(m_editor, event);
            return true;
        }
    }

    if (event->type() == QEvent::MouseButtonPress) {
        m_popup->hide();
        m_editor->setFocus();
        return true;
    }

    return false;
}

void EditorCompletion::doneCompletion()
{
    m_popup->hide();
    m_editor->setFocus();
    QTreeWidgetItem* item = m_popup->currentItem();
    emit selectedCompletion(item ? item->text(1) : QString());
}

void EditorCompletion::showCompletion(const QStringList& choices)
{
    if (!choices.count())
        return;
    m_popupInteracted = false;

    QFontMetrics metrics(m_editor->font());

    m_popup->setUpdatesEnabled(false);
    m_popup->clear();
    // Performance: compute these once per popup render (not per row) because
    // unit completion can contain many entries and this function runs often.
    Evaluator* evaluator = m_editor->evaluator();
    const QList<QString> builtInUnitKeys =
        Units::builtInUnitLookup(Settings::instance()->angleUnit).keys();
    const QSet<QString> builtInUnits(
        builtInUnitKeys.constBegin(),
        builtInUnitKeys.constEnd());
    const bool unitContextAtCursor = isInsideUnmatchedSquareBracketContext(
        m_editor->text(),
        m_editor->textCursor().position());

    for (int i = 0; i < choices.count(); ++i) {
        const auto pair = choices.at(i).split(':');
        if (pair.count() < 2)
            continue;

        const auto identifier = pair.at(0);
        const auto description = pair.mid(1).join(":");
        QString typeSymbol = QString::fromUtf8("👤 𝑥");

        if (FunctionRepo::instance()->find(identifier)) {
            typeSymbol = QString::fromUtf8("📚 ƒ");
        // Reuse local evaluator pointer to avoid repeated singleton lookups
        // on every completion row.
        } else if (evaluator->hasUserFunction(identifier)) {
            typeSymbol = QString::fromUtf8("👤 ƒ");
        } else if (!unitContextAtCursor && evaluator->hasVariable(identifier)) {
            const auto variable = evaluator->getVariable(identifier);
            if (variable.type() == Variable::BuiltIn)
                typeSymbol = QString::fromUtf8("📏 𝑘");
        } else if (evaluator->hasUserUnit(identifier)) {
            typeSymbol = QString::fromUtf8("👤 𝒖");
        // O(1) membership check against precomputed built-in unit identifiers.
        } else if (builtInUnits.contains(identifier)) {
            typeSymbol = QString::fromUtf8("📚 𝒖");
        }

        QStringList columns;
        columns << typeSymbol << identifier << description;
        QTreeWidgetItem* item = new QTreeWidgetItem(m_popup, columns);

        if (item && m_editor->layoutDirection() == Qt::RightToLeft)
            item->setTextAlignment(1, Qt::AlignRight);

        item->setTextAlignment(0, Qt::AlignCenter);
    }

    m_popup->sortItems(2, Qt::AscendingOrder);
    m_popup->sortItems(1, Qt::AscendingOrder);
    m_popup->setCurrentItem(m_popup->topLevelItem(0));

    // Size of the pop-up.
    m_popup->resizeColumnToContents(0);
    m_popup->setColumnWidth(0, m_popup->columnWidth(0) + 18);
    m_popup->resizeColumnToContents(1);
    m_popup->setColumnWidth(1, m_popup->columnWidth(1) + 25);
    m_popup->resizeColumnToContents(2);
    m_popup->setColumnWidth(2, m_popup->columnWidth(2) + 25);

    const int maxVisibleItems = 8;
    const int height =
        m_popup->sizeHintForRow(0) * qMin(maxVisibleItems, choices.count()) + 3;
    const int width = m_popup->columnWidth(0)
                      + m_popup->columnWidth(1)
                      + m_popup->columnWidth(2) + 1;

    // Position, reference is editor's cursor position in global coord.
    auto cursor = m_editor->textCursor();
    cursor.movePosition(QTextCursor::StartOfWord);
    const int pixelsOffset = metrics.horizontalAdvance(m_editor->text(), cursor.position());
    auto point = QPoint(pixelsOffset, m_editor->height());
    QPoint position = m_editor->mapToGlobal(point);

    // If popup is partially invisible, move to other position.
    auto screen = m_editor->screen()->availableGeometry();
    if (position.y() + height > screen.y() + screen.height())
        position.setY(position.y() - height - m_editor->height());
    if (position.x() + width > screen.x() + screen.width())
        position.setX(screen.x() + screen.width() - width);

    m_popup->setUpdatesEnabled(true);
    m_popup->setGeometry(QRect(position, QSize(width, height)));
    m_popup->show();
    m_popup->setFocus();
}

void EditorCompletion::selectItem(const QString& item)
{
    if (item.isNull()) {
        m_popup->setCurrentItem(0);
        return;
    }

    auto targets = m_popup->findItems(item, Qt::MatchExactly, 1);
    if (targets.count() > 0)
        m_popup->setCurrentItem(targets.at(0));
}

ConstantCompletion::ConstantCompletion(Editor* editor)
    : QObject(editor)
{
    m_editor = editor;

    m_popup = new QFrame;
    m_popup->setParent(editor->window(), Qt::Popup);
    m_popup->setFocusPolicy(Qt::NoFocus);
    m_popup->setFocusProxy(editor);
    m_popup->setFrameStyle(QFrame::Box | QFrame::Plain);

    m_categoryWidget = new QTreeWidget(m_popup);
    m_categoryWidget->setFrameShape(QFrame::NoFrame);
    m_categoryWidget->setColumnCount(1);
    m_categoryWidget->setRootIsDecorated(false);
    m_categoryWidget->header()->hide();
    m_categoryWidget->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_categoryWidget->setSelectionBehavior(QTreeWidget::SelectRows);
    m_categoryWidget->setMouseTracking(true);
    m_categoryWidget->installEventFilter(this);
    m_categoryWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    connect(m_categoryWidget, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
                              SLOT(showConstants()));

    m_constantWidget = new QTreeWidget(m_popup);
    m_constantWidget->setFrameShape(QFrame::NoFrame);
    m_constantWidget->setColumnCount(2);
    m_constantWidget->setColumnHidden(1, true);
    m_constantWidget->setRootIsDecorated(false);
    m_constantWidget->header()->hide();
    m_constantWidget->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_constantWidget->setSelectionBehavior(QTreeWidget::SelectRows);
    m_constantWidget->setMouseTracking(true);
    m_constantWidget->installEventFilter(this);
    m_constantWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    connect(m_constantWidget, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
                              SLOT(doneCompletion()));

    m_slider = new QTimeLine(100, m_popup);
    m_slider->setEasingCurve(QEasingCurve(QEasingCurve::Linear));
    connect(m_slider, SIGNAL(frameChanged(int)),
                      SLOT(setHorizontalPosition(int)));

    const Constants* constants = Constants::instance();
    m_constantList = constants->list();

    // Populate categories.
    QStringList domainList;
    domainList << tr("All");
    QTreeWidgetItem* all = new QTreeWidgetItem(m_categoryWidget, domainList);
    for (int i = 0; i < constants->domains().count(); ++i) {
        domainList.clear();
        domainList << constants->domains().at(i);
        new QTreeWidgetItem(m_categoryWidget, domainList);
    }
    m_categoryWidget->setCurrentItem(all);

    // Populate constants.
    m_lastDomain = tr("All");
    for (int i = 0; i < constants->list().count(); ++i) {
        QStringList names;
        names << constants->list().at(i).name;
        names << constants->list().at(i).name.toUpper();
        new QTreeWidgetItem(m_constantWidget, names);
    }
    m_constantWidget->sortItems(0, Qt::AscendingOrder);

    // Find size, the biggest between both.
    m_constantWidget->resizeColumnToContents(0);
    m_categoryWidget->resizeColumnToContents(0);
    int width = qMax(m_constantWidget->width(), m_categoryWidget->width());
    const int constantsHeight =
        m_constantWidget->sizeHintForRow(0)
            * qMin(7, m_constantList.count()) + 3;
    const int domainsHeight =
        m_categoryWidget->sizeHintForRow(0)
            * qMin(7, constants->domains().count()) + 3;
    const int height = qMax(constantsHeight, domainsHeight);
    width += 200; // Extra space (FIXME: scrollbar size?).

    // Adjust dimensions.
    m_popup->resize(width, height);
    m_constantWidget->resize(width, height);
    m_categoryWidget->resize(width, height);
}

ConstantCompletion::~ConstantCompletion()
{
    // Popup ownership is handled by Qt parent-child deletion (its parent is
    // the window). Deleting it manually here can double-delete during shutdown.
    m_editor->setFocus();
}

void ConstantCompletion::showCategory()
{
    m_slider->setFrameRange(m_popup->width(), 0);
    m_slider->stop();
    m_slider->start();
    m_categoryWidget->setFocus();
}

void ConstantCompletion::showConstants()
{
    m_slider->setFrameRange(0, m_popup->width());
    m_slider->stop();
    m_slider->start();
    m_constantWidget->setFocus();

    QString chosenDomain;
    if (m_categoryWidget->currentItem())
        chosenDomain = m_categoryWidget->currentItem()->text(0);

    if (m_lastDomain == chosenDomain)
        return;

    m_constantWidget->clear();

    for (int i = 0; i < m_constantList.count(); ++i) {
        QStringList names;
        names << m_constantList.at(i).name;
        names << m_constantList.at(i).name.toUpper();

        const bool include = (chosenDomain == tr("All")) ?
            true : (m_constantList.at(i).domain == chosenDomain);

        if (!include)
            continue;

        new QTreeWidgetItem(m_constantWidget, names);
    }

    m_constantWidget->sortItems(0, Qt::AscendingOrder);
    m_constantWidget->setCurrentItem(m_constantWidget->itemAt(0, 0));
    m_lastDomain = chosenDomain;
}

bool ConstantCompletion::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::Hide) {
        emit canceledCompletion();
        return true;
    }

    if (object == m_constantWidget) {

        if (event->type() == QEvent::KeyPress) {
            int key = static_cast<QKeyEvent*>(event)->key();

            switch (key) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Tab:
                doneCompletion();
                return true;

            case Qt::Key_Left:
                showCategory();
                return true;

            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
                return false;
            }

            if (key != Qt::Key_Escape)
                QApplication::sendEvent(m_editor, event);
            m_popup->hide();
            emit canceledCompletion();
            return true;
        }
    }

    if (object == m_categoryWidget) {

        if (event->type() == QEvent::KeyPress) {
            int key = static_cast<QKeyEvent*>(event)->key();

            switch (key) {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Right:
                showConstants();
                return true;

            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_PageUp:
            case Qt::Key_PageDown:
                return false;
            }

            if (key != Qt::Key_Escape)
                QApplication::sendEvent(m_editor, event);
            m_popup->hide();
            emit canceledCompletion();
            return true;
        }
    }

    return false;
}

void ConstantCompletion::doneCompletion()
{
    m_popup->hide();
    m_editor->setFocus();
    const auto* item = m_constantWidget->currentItem();
    if (!item) {
        emit selectedCompletion(QString());
        return;
    }

    auto found = std::find_if(m_constantList.begin(), m_constantList.end(),
        [&](const Constant& c) { return item->text(0) == c.name; }
    );
    if (found == m_constantList.end()) {
        emit selectedCompletion(QString());
        return;
    }

    QString normalizedUnit = found->unit;
    normalizedUnit.replace(UnicodeChars::MiddleDot, MathDsl::MulDotOp);
    const QString expression = found->unit.isEmpty()
        ? found->value
        : QStringLiteral("%1%2[%3]")
            .arg(found->value, QString(MathDsl::QuantSp), normalizedUnit);
    emit selectedCompletion(expression);
}

void ConstantCompletion::showCompletion()
{
    // Position, reference is editor's cursor position in global coord.
    QFontMetrics metrics(m_editor->font());
    const int currentPosition = m_editor->textCursor().position();
    const int pixelsOffset = metrics.horizontalAdvance(m_editor->text(), currentPosition);
    auto pos = m_editor->mapToGlobal(QPoint(pixelsOffset, m_editor->height()));

    const int height = m_popup->height();
    const int width = m_popup->width();

    // If popup is partially invisible, move to other position.
    const QRect screen = m_editor->screen()->availableGeometry();
    if (pos.y() + height > screen.y() + screen.height())
        pos.setY(pos.y() - height - m_editor->height());
    if (pos.x() + width > screen.x() + screen.width())
        pos.setX(screen.x() + screen.width() - width);

    // Start with category.
    m_categoryWidget->setFocus();
    setHorizontalPosition(0);

    m_popup->move(pos);
    m_popup->show();
}

void ConstantCompletion::setHorizontalPosition(int x)
{
    m_categoryWidget->move(-x, 0);
    m_constantWidget->move(m_popup->width() - x, 0);
}
