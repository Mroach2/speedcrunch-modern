// This file is part of the SpeedCrunch project
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef GUI_EDITORUTILS_H
#define GUI_EDITORUTILS_H

#include "core/unicodechars.h"
#include "core/mathdsl.h"

#include <QString>
#include <QStringList>

namespace EditorUtils {

enum AutoAnsRewriteMode {
    AutoAnsNoRewrite = 0,
    AutoAnsPrependAns = 1,
    AutoAnsAppendAns = 2
};

inline QString normalizeMultiplicationOperators(QString text, bool keepDotOperator = false)
{
    for (QChar& ch : text) {
        if (MathDsl::isMultiplicationOperatorAlias(ch, keepDotOperator))
            ch = MathDsl::MulCrossOp;
    }
    return text;
}

inline QString normalizeDivisionOperators(QString text)
{
    for (QChar& ch : text) {
        if (MathDsl::isDivisionOperatorAlias(ch))
            ch = UnicodeChars::BigSolidus; // ⧸ BIG SOLIDUS
    }
    return text;
}

inline QString normalizeDivisionOperatorsForEditorInput(QString text)
{
    for (QChar& ch : text) {
        if (MathDsl::isDivisionOperatorAlias(ch) || ch == UnicodeChars::BigSolidus)
            ch = MathDsl::DivOp; // / SOLIDUS
    }
    return text;
}

inline QString normalizeAdditionOperators(QString text)
{
    for (QChar& ch : text) {
        if (MathDsl::isAdditionOperatorAlias(ch))
            ch = MathDsl::AddOp; // + PLUS SIGN
    }
    return text;
}

inline QString normalizeSubtractionOperators(QString text)
{
    for (QChar& ch : text) {
        if (MathDsl::isSubtractionOperatorAlias(ch))
            ch = UnicodeChars::MinusSign;
    }
    return text;
}

inline QString normalizeExpressionOperators(QString text)
{
    text = UnicodeChars::normalizeUnitSymbolAliases(text);
    text = UnicodeChars::normalizeSexagesimalSymbolAliases(text);
    text = normalizeMultiplicationOperators(text);
    text = normalizeDivisionOperators(text);
    text = normalizeAdditionOperators(text);
    text = normalizeSubtractionOperators(text);
    return text;
}

inline QString normalizeExpressionOperatorsForEditorInput(QString text)
{
    text = UnicodeChars::normalizeUnitSymbolAliases(text);
    text = UnicodeChars::normalizeSexagesimalSymbolAliases(text);
    text = UnicodeChars::normalizePiAliasesToPi(text);
    text = UnicodeChars::normalizeRootFunctionAliasesForDisplay(text);
    text = normalizeMultiplicationOperators(text, true);
    text = normalizeDivisionOperatorsForEditorInput(text);
    text = normalizeAdditionOperators(text);
    text = normalizeSubtractionOperators(text);
    return text;
}

inline bool shouldIgnoreTypedSpaceAfterDigit(const QString& text, int cursorPosition)
{
    if (cursorPosition <= 0 || cursorPosition > text.size())
        return false;

    int i = cursorPosition - 1;
    while (i >= 0 && text.at(i).isSpace())
        --i;
    if (i < 0)
        return false;

    const QChar prevNonSpace = text.at(i);
    return prevNonSpace.isDigit()
           || prevNonSpace.isLetter()
           || MathDsl::isSuperscriptDigit(prevNonSpace)
           || prevNonSpace == MathDsl::GroupEnd
           || prevNonSpace == MathDsl::UnitEnd;
}

inline QString adjustedTypedTextForImplicitMultiplicationAfterDigit(
    const QString& text, int cursorPosition, const QString& typedText)
{
    if (typedText.size() != 1)
        return typedText;
    if (cursorPosition <= 0 || cursorPosition > text.size())
        return typedText;

    const QChar typed = typedText.at(0);
    const auto isScientificExponentSignContext = [&]() {
        int i = cursorPosition - 1;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        if (i < 0)
            return false;
        const QChar marker = text.at(i);
        if (marker != QLatin1Char('e') && marker != QLatin1Char('E'))
            return false;

        --i;
        if (i < 0)
            return false;

        bool sawDigit = false;
        while (i >= 0) {
            const QChar ch = text.at(i);
            if (ch.isDigit()) {
                sawDigit = true;
                --i;
                continue;
            }
            if (ch == MathDsl::DotSep || ch == MathDsl::CommaSep) {
                --i;
                continue;
            }
            if (MathDsl::isSubtractionOperatorAlias(ch)
                || MathDsl::isAdditionOperatorAlias(ch)) {
                const int boundaryIndex = i - 1;
                if (boundaryIndex < 0)
                    return sawDigit;
                const QChar boundary = text.at(boundaryIndex);
                return sawDigit
                       && (boundary.isSpace()
                           || MathDsl::isExpressionBoundaryOperatorOrSeparator(boundary)
                           || boundary == MathDsl::GroupStart
                           || boundary == MathDsl::UnitStart);
            }
            return sawDigit
                   && (ch.isSpace()
                       || MathDsl::isExpressionBoundaryOperatorOrSeparator(ch)
                       || ch == MathDsl::GroupStart
                       || ch == MathDsl::UnitStart);
        }
        return sawDigit;
    };
    const auto isSingleZeroTermToLeft = [&]() {
        int i = cursorPosition - 1;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        if (i < 0 || text.at(i) != MathDsl::Dig0)
            return false;
        --i;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        if (i < 0)
            return true;
        const QChar left = text.at(i);
        return MathDsl::isExpressionBoundaryOperatorOrSeparator(left)
               || left == MathDsl::GroupStart
               || left == MathDsl::UnitStart;
    };
    QString operatorPrefix;
    const auto leftNonSpaceSupportsOperatorInsertion = [&]() {
        int i = cursorPosition - 1;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        if (i < 0)
            return false;
        const QChar prevNonSpace = text.at(i);
        return prevNonSpace.isDigit()
            || prevNonSpace.isLetter()
            || MathDsl::isSuperscriptDigit(prevNonSpace)
            || prevNonSpace == MathDsl::GroupEnd
            || prevNonSpace == MathDsl::UnitEnd;
    };
    if (typed.isLetter()) {
        if (typed == QLatin1Char('e') || typed == QLatin1Char('E'))
            return typedText;
        if (typed == QLatin1Char('i') || typed == QLatin1Char('I')) {
            int i = cursorPosition - 1;
            while (i >= 0 && text.at(i).isSpace())
                --i;
            if (i < 0)
                return typedText;
            const QChar prevNonSpace = text.at(i);
            const bool shouldExpandToIn =
                prevNonSpace.isDigit()
                || MathDsl::isSuperscriptDigit(prevNonSpace)
                || prevNonSpace == MathDsl::GroupEnd
                || prevNonSpace == MathDsl::UnitEnd;
            if (!shouldExpandToIn)
                return typedText;
            return (i == cursorPosition - 1) ? (QStringLiteral(" ") + typedText) : typedText;
        }
        if ((typed == QLatin1Char('b') || typed == QLatin1Char('B')
             || typed == QLatin1Char('o') || typed == QLatin1Char('O')
             || typed == QLatin1Char('x') || typed == QLatin1Char('X'))
            && isSingleZeroTermToLeft()) {
            return typedText;
        }
        return typedText;
    } else if (typed == MathDsl::GroupStart || typed == MathDsl::UnitStart) {
        int i = cursorPosition - 1;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        if (i < 0)
            return typedText;

        const QChar prevNonSpace = text.at(i);
        if (!prevNonSpace.isDigit()
            && !MathDsl::isSuperscriptDigit(prevNonSpace)
            && !prevNonSpace.isLetter()) {
            return typedText;
        }
    } else if (typed == MathDsl::AddOp || MathDsl::isAdditionOperatorAlias(typed)) {
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        operatorPrefix = MathDsl::buildWrappedToken(MathDsl::AddOp, MathDsl::AddWrap);
    } else if (MathDsl::isSubtractionOperatorAlias(typed)) {
        if (isScientificExponentSignContext())
            return typedText;
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        operatorPrefix = MathDsl::buildWrappedToken(MathDsl::SubOp, MathDsl::SubWrapSp);
    } else if (typed == MathDsl::DivOp || MathDsl::isDivisionOperatorAlias(typed)) {
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        operatorPrefix = MathDsl::buildWrappedToken(MathDsl::DivOp, MathDsl::DivWrap);
    } else if (MathDsl::isMultiplicationOperator(typed)
               || MathDsl::isMultiplicationOperatorAlias(typed, true)) {
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        const bool useDotSign = typed == MathDsl::MulDotOp;
        const QChar sign = useDotSign ? MathDsl::MulDotOp : MathDsl::MulCrossOp;
        const QChar space = useDotSign ? MathDsl::MulDotWrapSp : MathDsl::MulCrossWrapSp;
        operatorPrefix = MathDsl::buildWrappedToken(sign, space);
    } else if (typed == MathDsl::Equals) {
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        operatorPrefix = MathDsl::buildWrappedToken(MathDsl::Equals, UnicodeChars::Space);
    } else if (typed == MathDsl::LessThanOp) {
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        operatorPrefix = MathDsl::buildWrappedToken(MathDsl::ShiftLeftOp);
    } else if (typed == MathDsl::GreaterThanOp) {
        if (!leftNonSpaceSupportsOperatorInsertion())
            return typedText;
        operatorPrefix = MathDsl::buildWrappedToken(MathDsl::ShiftRightOp);
    } else {
        return typedText;
    }

    if (!operatorPrefix.isEmpty()) {
        // Avoid producing double spaces when the cursor is already after a
        // space and we are about to insert a grouped " <op> " sequence.
        if (cursorPosition > 0 && text.at(cursorPosition - 1).isSpace()
            && operatorPrefix.size() >= 3
            && operatorPrefix.at(0).isSpace()) {
            return operatorPrefix.mid(1);
        }
        return operatorPrefix;
    }

    if (typed == MathDsl::GroupStart) {
        int i = cursorPosition - 1;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        if (i >= 0) {
            const QChar prevNonSpace = text.at(i);
            bool numericLeftTerm = prevNonSpace.isDigit()
                                   && !MathDsl::isSuperscriptDigit(prevNonSpace);
            if (!numericLeftTerm && MathDsl::isSuperscriptDigit(prevNonSpace)) {
                int baseIndex = i;
                while (baseIndex >= 0 && MathDsl::isSuperscriptPowerChar(text.at(baseIndex)))
                    --baseIndex;
                numericLeftTerm = baseIndex >= 0 && text.at(baseIndex).isDigit();
            }
            if (numericLeftTerm) {
                return MathDsl::buildWrappedToken(MathDsl::MulCrossOp, MathDsl::MulCrossWrapSp)
                       + typedText;
            }
        }
    }

    return MathDsl::buildWrappedToken(MathDsl::MulDotOp, MathDsl::MulDotWrapSp)
           + typedText;
}

inline bool isAnyOperator(const QChar& ch)
{
    return ch == MathDsl::AddOp
           || ch == MathDsl::SubOpAl1
           || ch == MathDsl::MulOpAl1
           || ch == QLatin1Char('/')
           || ch == MathDsl::PercentOp
           || ch == MathDsl::PowOp
           || ch == MathDsl::BitAndOp
           || ch == MathDsl::BitOrOp
           || ch == MathDsl::Equals
           || ch == MathDsl::LessThanOp
           || ch == MathDsl::GreaterThanOp
           || MathDsl::isAdditionOperatorAlias(ch)
           || MathDsl::isSubtractionOperatorAlias(ch)
           || MathDsl::isDivisionOperatorAlias(ch)
           || MathDsl::isMultiplicationOperatorAlias(ch, true);
}

inline bool endsWithIncompleteExpressionToken(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return false;

    const QChar last = trimmed.at(trimmed.size() - 1);
    return last == MathDsl::GroupStart || MathDsl::isExpressionBoundaryOperatorOrSeparator(last);
}

inline bool expressionWithoutIgnorableTrailingToken(const QString& text, QString* out)
{
    if (!out)
        return false;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return false;

    int i = trimmed.size() - 1;
    const QChar last = trimmed.at(i);
    const bool isPlusMinusTail =
        (last == MathDsl::AddOp || MathDsl::isSubtractionOperatorAlias(last));
    const bool isMultiplicationTail =
        (MathDsl::isMultiplicationOperator(last)
         || MathDsl::isMultiplicationOperatorAlias(last, true));

    if (last != MathDsl::GroupStart
        && !isPlusMinusTail
        && !isMultiplicationTail
        && last != MathDsl::FunArgSep
        && last != QLatin1Char('/')
        && last != MathDsl::PowOp
        && last != MathDsl::IntDivOp)
        return false;

    // Allow safe trailing suffixes:
    // - one or more '+'
    // - one or more '−'
    // - one or more '('
    // - one or more '\'
    // - one '/' only
    if (last == QLatin1Char('/')) {
        --i;
        if (i >= 0 && trimmed.at(i) == QLatin1Char('/'))
            return false;
    } else if (last == MathDsl::PowOp) {
        --i;
        if (i >= 0 && trimmed.at(i) == MathDsl::PowOp)
            return false;
    } else if (isPlusMinusTail) {
        while (i >= 0 && (trimmed.at(i) == MathDsl::AddOp
                          || MathDsl::isSubtractionOperatorAlias(trimmed.at(i))))
            --i;
    } else if (isMultiplicationTail) {
        --i;
        if (i >= 0) {
            const QChar prev = trimmed.at(i);
            const bool prevIsMultiplication =
                (MathDsl::isMultiplicationOperator(prev)
                 || MathDsl::isMultiplicationOperatorAlias(prev, true));
            if (prevIsMultiplication) {
                --i;
                if (i >= 0) {
                    const QChar prevPrev = trimmed.at(i);
                    const bool prevPrevIsMultiplication =
                        (MathDsl::isMultiplicationOperator(prevPrev)
                         || MathDsl::isMultiplicationOperatorAlias(prevPrev, true));
                    if (prevPrevIsMultiplication)
                        return false;
                }
            }
        }
    } else {
        while (i >= 0 && trimmed.at(i) == last)
            --i;
    }

    const QString prefix = trimmed.left(i + 1).trimmed();
    if (prefix.isEmpty())
        return false;

    const QChar prefixLast = prefix.at(prefix.size() - 1);
    if (prefixLast == MathDsl::GroupStart
        || MathDsl::isExpressionBoundaryOperatorOrSeparator(prefixLast)
        || prefixLast == MathDsl::IntDivOp)
        return false;

    *out = prefix;
    return true;
}

template <typename NormalizeExpression>
inline QStringList parsePastedExpressionsImpl(const QString& text, NormalizeExpression normalizeExpression)
{
    QStringList expressions;
    const QStringList candidates =
        text.split('\n', Qt::KeepEmptyParts, Qt::CaseSensitive);
    expressions.reserve(candidates.size());

    for (const auto& candidate : candidates) {
        const auto expression = normalizeExpression(candidate.trimmed());
        if (!expression.isEmpty())
            expressions.append(expression);
    }

    return expressions;
}

inline QStringList parsePastedExpressions(const QString& text)
{
    return parsePastedExpressionsImpl(text, normalizeExpressionOperators);
}

inline QStringList parsePastedExpressionsForEditorInput(const QString& text)
{
    return parsePastedExpressionsImpl(text, normalizeExpressionOperatorsForEditorInput);
}

inline AutoAnsRewriteMode autoAnsRewriteModeForLeadingOperator(const QString& operatorText)
{
    if (operatorText.size() != 1)
        return AutoAnsNoRewrite;

    const QChar op = normalizeExpressionOperatorsForEditorInput(operatorText).at(0);
    if (op == QLatin1Char('~'))
        return AutoAnsAppendAns;
    if (op == MathDsl::AddOp
        || MathDsl::isSubtractionOperatorAlias(op)
        || MathDsl::isMultiplicationOperator(op)
        || MathDsl::isDivisionOperator(op)
        || op == MathDsl::PowOp
        || op == MathDsl::FactorOp) {
        return AutoAnsPrependAns;
    }
    return AutoAnsNoRewrite;
}

inline QString applyAutoAnsRewrite(const QString& expression, AutoAnsRewriteMode mode)
{
    if (mode == AutoAnsAppendAns)
        return expression + QStringLiteral("ans");
    if (mode == AutoAnsPrependAns)
        return QStringLiteral("ans") + expression;
    return expression;
}

inline bool isAllowedLeadingCharAtExpressionStart(const QChar& ch, bool autoAnsEnabled)
{
    if (ch.isDigit())
        return true;
    if (ch == UnicodeChars::LowLine
        || ch == MathDsl::DollarSign
        || ch == UnicodeChars::Summation
        || ch == UnicodeChars::SquareRoot
        || ch == UnicodeChars::CubeRoot) {
        return true;
    }
    if (ch.isLetter()) {
        // Exclude ordinal indicators that behave like symbols on PT/US layouts
        // and are not intended to be valid leading tokens.
        if (ch == UnicodeChars::FeminineOrdinalIndicator || ch == UnicodeChars::MasculineOrdinalIndicator)
            return false;
        return true;
    }
    if (ch.category() == QChar::Symbol_Currency)
        return true;
    if (MathDsl::isSubtractionOperatorAlias(ch))
        return true;
    if (ch == QLatin1Char('~')
        || ch == MathDsl::GroupStart
        || ch == MathDsl::UnitStart
        || ch == MathDsl::HexPrefixAl1
        || ch == MathDsl::CommentSep) {
        return true;
    }
    if (!autoAnsEnabled)
        return false;

    // When auto-ans is enabled, allow leading operator starts that can be
    // rewritten as "ans <op> ..." or "~ans".
    return ch == MathDsl::AddOp
           || MathDsl::isDivisionOperator(ch)
           || MathDsl::isMultiplicationOperator(ch)
           || ch == MathDsl::FactorOp
           || MathDsl::isAdditionOperatorAlias(ch)
           || MathDsl::isDivisionOperatorAlias(ch)
           || MathDsl::isMultiplicationOperatorAlias(ch, true)
           || ch == MathDsl::PowOp;
}

}

#endif
