// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_RESULTLINEFORMATUTILS_H
#define GUI_RESULTLINEFORMATUTILS_H

#include "core/evaluator.h"
#include "core/mathdsl.h"
#include "core/numberformatter.h"
#include "core/regexpatterns.h"
#include "core/settings.h"
#include "core/unitdisplayformat.h"
#include "core/unicodechars.h"
#include "core/units.h"
#include "gui/displayformatutils.h"
#include "gui/simplifiedexpressionutils.h"
#include "math/rational.h"

#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>
#include <functional>

namespace ResultLineFormatUtils {

inline QString normalizeBracketedUnitTextForDisplay(const QString& text)
{
    QString output;
    output.reserve(text.size());
    int cursor = 0;
    QRegularExpressionMatchIterator matches =
        RegExpPatterns::unitBrackets().globalMatch(text);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        const int start = match.capturedStart();
        const int end = match.capturedEnd();
        output += text.mid(cursor, start - cursor);
        output += MathDsl::UnitStart;
        output += UnitDisplayFormat::normalizeUnitTextForDisplay(match.captured(1));
        output += MathDsl::UnitEnd;
        cursor = end;
    }
    output += text.mid(cursor);
    return output;
}

inline QString collapseValueUnitMultiplicationForDisplay(QString text)
{
    return text.replace(
        RegExpPatterns::valueTimesBracketedUnit(),
        QStringLiteral("\\1") + QString(MathDsl::QuantSp) + QStringLiteral("\\2"));
}

inline QString preserveExplicitBracketedSimpleUnitsFromSource(QString displayed,
                                                              const QString& sourceExpression)
{
    QRegularExpressionMatchIterator it =
        RegExpPatterns::bracketedSimpleUnitIdentifier().globalMatch(sourceExpression);
    QSet<QString> sourceSimpleUnits;
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        sourceSimpleUnits.insert(match.captured(1));
    }
    if (sourceSimpleUnits.isEmpty())
        return displayed;

    for (const QString& unitToken : sourceSimpleUnits) {
        const QString escapedToken = QRegularExpression::escape(unitToken);
        const QRegularExpression valueTimesUnit(
            QStringLiteral(R"((\d(?:[\d\.,]*)?)\s*[*·×]\s*(%1)(?![\p{L}\p{N}_]))")
                .arg(escapedToken));
        displayed.replace(
            valueTimesUnit,
            QStringLiteral("\\1")
                + QString(MathDsl::QuantSp)
                + QStringLiteral("[")
                + unitToken
                + QStringLiteral("]"));
    }
    return displayed;
}

inline QString preserveExplicitParenthesizedUnitDenominatorFromSource(
    QString displayed,
    const QString& sourceExpression)
{
    auto countExplicitGroupedUnitDenominators = [](const QString& text) {
        int count = 0;
        for (int i = 0; i < text.size(); ++i) {
            if (text.at(i) != MathDsl::DivOp)
                continue;
            int pos = i + 1;
            while (pos < text.size() && text.at(pos).isSpace())
                ++pos;
            if (pos >= text.size() || text.at(pos) != MathDsl::GroupStart)
                continue;

            int depth = 0;
            int closePos = -1;
            for (int j = pos; j < text.size(); ++j) {
                const QChar ch = text.at(j);
                if (ch == MathDsl::GroupStart) {
                    ++depth;
                } else if (ch == MathDsl::GroupEnd) {
                    --depth;
                    if (depth == 0) {
                        closePos = j;
                        break;
                    }
                    if (depth < 0)
                        break;
                }
            }
            if (closePos < 0)
                continue;
            const QString grouped = text.mid(pos + 1, closePos - pos - 1);
            if (grouped.contains(MathDsl::UnitStart) && grouped.contains(MathDsl::UnitEnd))
                ++count;
            i = closePos;
        }
        return count;
    };

    int sourceParenthesizedDenominatorCount =
        countExplicitGroupedUnitDenominators(sourceExpression);
    if (sourceParenthesizedDenominatorCount == 0)
        return displayed;

    int preservedCount = 0;
    for (int i = 0; i < displayed.size() && preservedCount < sourceParenthesizedDenominatorCount; ++i) {
        if (displayed.at(i) != MathDsl::DivOp)
            continue;
        int numStart = i + 1;
        while (numStart < displayed.size() && displayed.at(numStart).isSpace())
            ++numStart;
        if (numStart >= displayed.size())
            continue;
        if (displayed.at(numStart) == MathDsl::GroupStart)
            continue;

        int unitOpen = displayed.indexOf(MathDsl::UnitStart, numStart);
        int unitClose = (unitOpen >= 0) ? displayed.indexOf(MathDsl::UnitEnd, unitOpen + 1) : -1;
        if (unitOpen < 0 || unitClose < 0)
            continue;

        bool hasInnerDivOrAddSub = false;
        for (int j = numStart; j < unitOpen; ++j) {
            const QChar ch = displayed.at(j);
            if (ch == MathDsl::DivOp
                || MathDsl::isAdditionOperator(ch)
                || MathDsl::isSubtractionOperator(ch))
            {
                hasInnerDivOrAddSub = true;
                break;
            }
        }
        if (hasInnerDivOrAddSub)
            continue;

        displayed.insert(unitClose + 1, MathDsl::GroupEnd);
        displayed.insert(numStart, MathDsl::GroupStart);
        ++preservedCount;
    }
    return displayed;
}

inline QString preserveExplicitParenthesizedQuotientBeforeUnitFromSource(
    QString displayed,
    const QString& sourceExpression)
{
    auto countSourceGroupedQuotientsWithUnit = [](const QString& text) {
        int count = 0;
        for (int i = 0; i < text.size(); ++i) {
            if (text.at(i) != MathDsl::GroupStart)
                continue;
            int depth = 0;
            int close = -1;
            for (int j = i; j < text.size(); ++j) {
                const QChar ch = text.at(j);
                if (ch == MathDsl::GroupStart) {
                    ++depth;
                } else if (ch == MathDsl::GroupEnd) {
                    --depth;
                    if (depth == 0) {
                        close = j;
                        break;
                    }
                }
            }
            if (close < 0)
                continue;
            int next = close + 1;
            while (next < text.size() && text.at(next).isSpace())
                ++next;
            if (next < text.size()
                && text.at(next) == MathDsl::UnitStart
                && text.mid(i + 1, close - i - 1).contains(MathDsl::DivOp))
            {
                ++count;
            }
            i = close;
        }
        return count;
    };

    auto isNumericChar = [](QChar ch) {
        return ch.isDigit() || ch == MathDsl::DotSep || ch == MathDsl::CommaSep;
    };

    int preserveCount = countSourceGroupedQuotientsWithUnit(sourceExpression);
    if (preserveCount == 0)
        return displayed;

    for (int i = 0; i < displayed.size() && preserveCount > 0; ++i) {
        if (displayed.at(i) != MathDsl::UnitStart)
            continue;

        int rightEnd = i - 1;
        while (rightEnd >= 0 && displayed.at(rightEnd).isSpace())
            --rightEnd;
        if (rightEnd < 0)
            continue;
        if (!isNumericChar(displayed.at(rightEnd)))
            continue;

        int rightStart = rightEnd;
        while (rightStart >= 0 && isNumericChar(displayed.at(rightStart)))
            --rightStart;
        ++rightStart;

        int slashPos = rightStart - 1;
        while (slashPos >= 0 && displayed.at(slashPos).isSpace())
            --slashPos;
        if (slashPos < 0 || displayed.at(slashPos) != MathDsl::DivOp)
            continue;

        int leftEnd = slashPos - 1;
        while (leftEnd >= 0 && displayed.at(leftEnd).isSpace())
            --leftEnd;
        if (leftEnd < 0 || !isNumericChar(displayed.at(leftEnd)))
            continue;

        int leftStart = leftEnd;
        while (leftStart >= 0 && isNumericChar(displayed.at(leftStart)))
            --leftStart;
        ++leftStart;

        int beforeLeft = leftStart - 1;
        while (beforeLeft >= 0 && displayed.at(beforeLeft).isSpace())
            --beforeLeft;
        if (beforeLeft >= 0 && displayed.at(beforeLeft) == MathDsl::GroupStart)
            continue;

        displayed.insert(i, MathDsl::GroupEnd);
        displayed.insert(leftStart, MathDsl::GroupStart);
        ++i;
        --preserveCount;
    }
    return displayed;
}

inline QString collapseBracketedCompactAngleSuffixes(QString text)
{
    auto compactSuffixForToken = [](const QString& token) {
        const QString normalized = normalizeUnitName(
            UnicodeChars::normalizeUnitSymbolAliases(token.trimmed()));
        const UnitId id = unitId(normalized);
        if (id == UnitId::Degree) {
            return UnicodeChars::DegreeSign;
        }
        if (id == UnitId::Arcminute) {
            return UnicodeChars::Prime;
        }
        if (id == UnitId::Arcsecond) {
            return UnicodeChars::DoublePrime;
        }
        return QChar();
    };
    auto isCompactionSpace = [](QChar ch) {
        return ch.isSpace() || ch == MathDsl::QuantSp || ch == UnicodeChars::NoBreakSpace;
    };
    auto isCompactionLeftAnchor = [](QChar ch) {
        return ch.isLetterOrNumber()
            || ch == MathDsl::GroupEnd
            || ch == MathDsl::UnitEnd
            || ch == MathDsl::PercentOp
            || ch == UnicodeChars::DegreeSign
            || ch == UnicodeChars::Prime
            || ch == UnicodeChars::DoublePrime;
    };
    auto isInsideTrigCallArgument = [&](int index) {
        QVector<int> parenStack;
        parenStack.reserve(8);
        for (int i = 0; i < index && i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (ch == MathDsl::GroupStart) {
                parenStack.append(i);
            } else if (ch == MathDsl::GroupEnd) {
                if (!parenStack.isEmpty())
                    parenStack.removeLast();
            }
        }
        if (parenStack.isEmpty())
            return false;

        const int openParenPos = parenStack.last();
        int nameEnd = openParenPos - 1;
        while (nameEnd >= 0 && text.at(nameEnd).isSpace())
            --nameEnd;
        if (nameEnd < 0)
            return false;

        int nameStart = nameEnd;
        while (nameStart >= 0) {
            const QChar ch = text.at(nameStart);
            if (ch.isLetterOrNumber() || ch == UnicodeChars::LowLine) {
                --nameStart;
                continue;
            }
            break;
        }
        ++nameStart;
        if (nameStart > nameEnd)
            return false;

        return RegExpPatterns::isTrigFunctionIdentifier(
            QStringView(text).mid(nameStart, nameEnd - nameStart + 1));
    };

    QString output;
    output.reserve(text.size());
    int cursor = 0;
    QRegularExpressionMatchIterator it =
        RegExpPatterns::compactAngleTokenInBrackets().globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const int tokenStart = match.capturedStart();
        const int tokenEnd = match.capturedEnd();
        int left = tokenStart - 1;
        while (left >= cursor && isCompactionSpace(text.at(left)))
            --left;
        int right = tokenEnd;
        while (right < text.size() && isCompactionSpace(text.at(right)))
            ++right;
        if (left < cursor || !isCompactionLeftAnchor(text.at(left))) {
            output += text.mid(cursor, tokenEnd - cursor);
            cursor = tokenEnd;
            continue;
        }
        output += text.mid(cursor, left - cursor + 1);
        const QChar suffix = compactSuffixForToken(match.captured(1));
        if (suffix.isNull()) {
            output += match.captured(0);
        } else {
            if (suffix == UnicodeChars::DegreeSign
                && !isInsideTrigCallArgument(tokenStart)) {
                output += match.captured(0);
                cursor = tokenEnd;
                continue;
            }
            const QChar leftChar = text.at(left);
            const bool alreadyHasSameSuffix =
                leftChar == suffix
                || (suffix == UnicodeChars::DegreeSign
                    && (leftChar == UnicodeChars::MasculineOrdinalIndicator
                        || leftChar == UnicodeChars::RingAbove));
            if (!alreadyHasSameSuffix)
                output += suffix;
        }
        cursor = tokenEnd;
    }
    output += text.mid(cursor);
    return output;
}

inline bool containsExplicitBracketedAngleUnit(const QString& expression)
{
    QRegularExpressionMatchIterator matches =
        RegExpPatterns::unitBrackets().globalMatch(expression);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        if (Units::isExplicitAngleUnitName(match.captured(1)))
            return true;
    }
    return false;
}

inline bool containsExplicitSexagesimalAngleMarkers(const QString& expression)
{
    for (const QChar ch : expression) {
        if (ch == UnicodeChars::DegreeSign
            || ch == UnicodeChars::MasculineOrdinalIndicator
            || ch == UnicodeChars::RingOperator
            || ch == UnicodeChars::Prime
            || ch == UnicodeChars::DoublePrime
            || ch == UnicodeChars::Apostrophe
            || ch == UnicodeChars::QuotationMark) {
            return true;
        }
    }
    return false;
}

inline bool isStandaloneSexagesimalAngleLiteral(const QString& expression)
{
    if (!RegExpPatterns::standaloneSexagesimalAngleLiteralAllowed().match(expression).hasMatch())
        return false;

    if (!RegExpPatterns::anyDigit().match(expression).hasMatch())
        return false;

    return containsExplicitSexagesimalAngleMarkers(expression);
}

inline QString foldRepeatedAdditiveTermsForDisplay(const QString& text)
{
    if (text.isEmpty())
        return text;

    const Tokens tokens = Evaluator::instance()->scan(text);
    if (!tokens.valid() || tokens.isEmpty())
        return text;

    QVector<int> splitOps;
    int depth = 0;
    for (int i = 0; i < tokens.size(); ++i) {
        const Token::Operator op = tokens.at(i).asOperator();
        if (op == Token::AssociationStart) {
            ++depth;
            continue;
        }
        if (op == Token::AssociationEnd) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (depth == 0 && (op == Token::Addition || op == Token::Subtraction))
            splitOps.append(i);
    }
    if (splitOps.isEmpty())
        return text;

    auto tokenRangeText = [&text, &tokens](int startIndex, int endIndex) {
        const int startPos = tokens.at(startIndex).pos();
        const int endPos = tokens.at(endIndex).pos() + tokens.at(endIndex).size();
        return text.mid(startPos, endPos - startPos);
    };
    auto isMinusChar = [](const QChar& ch) {
        return ch == MathDsl::SubOp || ch == MathDsl::SubOpAl1;
    };
    auto formatCoeff = [](double value) {
        return QString::number(value, 'g', 15);
    };
    auto normalizeKey = [](QString s) {
        s.remove(QRegularExpression(QStringLiteral("\\s+")));
        return s;
    };
    auto isNumericPowerLiteralRange = [](const Tokens& parsed, int start, int end, const auto& self) -> bool {
        if (start < 0 || end >= parsed.size() || start > end)
            return false;

        // Strip unary prefix.
        while (start <= end) {
            const Token::Operator op = parsed.at(start).asOperator();
            if (op == Token::Addition || op == Token::Subtraction || op == Token::BitwiseLogicalNOT) {
                ++start;
                continue;
            }
            break;
        }
        // Strip postfix operators.
        while (start <= end) {
            const Token::Operator op = parsed.at(end).asOperator();
            if (op == Token::Percent || op == Token::Factorial) {
                --end;
                continue;
            }
            break;
        }
        if (start > end)
            return false;

        // Single numeric token (covers rendered superscript numbers too).
        if (start == end && parsed.at(start).isNumber())
            return true;

        // Unwrap one full parenthesized layer.
        if (parsed.at(start).asOperator() == Token::AssociationStart
            && parsed.at(end).asOperator() == Token::AssociationEnd) {
            int depth = 0;
            bool enclosesWholeRange = true;
            for (int i = start; i <= end; ++i) {
                const Token::Operator op = parsed.at(i).asOperator();
                if (op == Token::AssociationStart)
                    ++depth;
                else if (op == Token::AssociationEnd)
                    --depth;
                if (depth == 0 && i < end) {
                    enclosesWholeRange = false;
                    break;
                }
            }
            if (enclosesWholeRange)
                return self(parsed, start + 1, end - 1, self);
        }

        // Accept numeric powers recursively: a^b where both sides are numeric literals/powers.
        int exponentOp = -1;
        int depth = 0;
        for (int i = start; i <= end; ++i) {
            const Token::Operator op = parsed.at(i).asOperator();
            if (op == Token::AssociationStart) {
                ++depth;
                continue;
            }
            if (op == Token::AssociationEnd) {
                --depth;
                continue;
            }
            if (depth == 0 && op == Token::Exponentiation) {
                if (exponentOp >= 0)
                    return false;
                exponentOp = i;
            }
        }
        if (exponentOp <= start || exponentOp >= end)
            return false;

        return self(parsed, start, exponentOp - 1, self)
            && self(parsed, exponentOp + 1, end, self);
    };
    auto isNumericLiteralOrPowerLiteral = [&isNumericPowerLiteralRange](const QString& factorText) {
        auto isWholeWrappedByParentheses = [](const QString& text) {
            if (text.size() < 2 || text.at(0) != MathDsl::GroupStart || text.at(text.size() - 1) != MathDsl::GroupEnd)
                return false;
            int depth = 0;
            for (int i = 0; i < text.size(); ++i) {
                const QChar ch = text.at(i);
                if (ch == MathDsl::GroupStart)
                    ++depth;
                else if (ch == MathDsl::GroupEnd)
                    --depth;
                if (depth == 0 && i < text.size() - 1)
                    return false;
            }
            return depth == 0;
        };
        auto isDisplayedNumericPowerLiteralText = [&isWholeWrappedByParentheses](QString text) {
            text = text.trimmed();
            while (isWholeWrappedByParentheses(text))
                text = text.mid(1, text.size() - 2).trimmed();
            if (text.isEmpty())
                return false;
            bool hasDigit = false;
            for (const QChar& ch : text) {
                if (ch.isDigit()) {
                    hasDigit = true;
                    continue;
                }
                if (MathDsl::isSuperscriptPowerChar(ch))
                    continue;
                if (ch == MathDsl::PowOp
                    || ch == MathDsl::AddOp
                    || ch == MathDsl::SubOp
                    || ch == MathDsl::SubOpAl1
                    || ch == QLatin1Char('.')
                    || ch == QLatin1Char(','))
                {
                    continue;
                }
                return false;
            }
            return hasDigit;
        };

        if (isDisplayedNumericPowerLiteralText(factorText))
            return true;

        const Tokens parsed = Evaluator::instance()->scan(factorText);
        if (!parsed.valid() || parsed.isEmpty())
            return false;
        return isNumericPowerLiteralRange(parsed, 0, parsed.size() - 1, isNumericPowerLiteralRange);
    };

    QHash<QString, double> coeffByKey;
    QHash<QString, QString> displayByKey;
    QVector<QString> keyOrder;

    int termStart = 0;
    for (int part = 0; part <= splitOps.size(); ++part) {
        const int opIndex = (part < splitOps.size()) ? splitOps.at(part) : -1;
        const int termEnd = (opIndex >= 0) ? (opIndex - 1) : (tokens.size() - 1);
        if (termEnd < termStart) {
            termStart = opIndex + 1;
            continue;
        }

        const QString opText = (part == 0) ? QString(MathDsl::AddOp)
            : tokenRangeText(splitOps.at(part - 1), splitOps.at(part - 1));
        QString termText = tokenRangeText(termStart, termEnd).trimmed();
        termStart = opIndex + 1;
        if (termText.isEmpty())
            continue;

        bool negFromTerm = false;
        while (!termText.isEmpty() && (termText.at(0) == MathDsl::AddOp || isMinusChar(termText.at(0)))) {
            if (isMinusChar(termText.at(0)))
                negFromTerm = !negFromTerm;
            termText.remove(0, 1);
            termText = termText.trimmed();
        }
        if (termText.isEmpty())
            continue;

        bool ok = false;
        double numericFactor = 1.0;
        QString base = termText;
        const Tokens termTokens = Evaluator::instance()->scan(termText);
        if (termTokens.valid()
            && termTokens.size() >= 3
            && termTokens.at(0).isNumber()
            && termTokens.at(1).asOperator() == Token::Multiplication) {
            numericFactor = termTokens.at(0).text().toDouble(&ok);
            if (ok && numericFactor != 0.0) {
                const int baseStart = termTokens.at(2).pos();
                const int baseEnd = termTokens.at(termTokens.size() - 1).pos()
                    + termTokens.at(termTokens.size() - 1).size();
                if (baseStart >= 0 && baseEnd > baseStart && baseEnd <= termText.size())
                    base = termText.mid(baseStart, baseEnd - baseStart).trimmed();
            } else {
                ok = false;
            }
        } else {
            ok = true;
        }
        if (!ok || base.isEmpty())
            continue;

        const bool minusOp = (opText.size() == 1 && isMinusChar(opText.at(0)));
        const double signedFactor = (minusOp ? -1.0 : 1.0) * (negFromTerm ? -numericFactor : numericFactor);
        const QString key = normalizeKey(base);
        if (!coeffByKey.contains(key)) {
            keyOrder.append(key);
            displayByKey.insert(key, base);
        }
        coeffByKey[key] += signedFactor;
    }

    QString rebuilt;
    for (const QString& key : keyOrder) {
        const double coeff = coeffByKey.value(key, 0.0);
        if (std::abs(coeff) < 1e-12)
            continue;
        const QString base = displayByKey.value(key, key);
        const bool negative = coeff < 0.0;
        const double magnitude = std::abs(coeff);

        QString term = base;
        if (std::abs(magnitude - 1.0) >= 1e-12) {
            const QChar mulOp = isNumericLiteralOrPowerLiteral(base)
                ? MathDsl::MulCrossOp
                : MathDsl::MulDotOp;
            const QChar mulWrap = (mulOp == MathDsl::MulCrossOp)
                ? MathDsl::MulCrossWrapSp
                : MathDsl::MulDotWrapSp;
            // Keep coefficient*term rendering on the same DSL-wrapped spacing
            // path used by display formatting, so mixed-alias inputs do not
            // regress to compact "4·x" while other paths show "4 · x".
            term = formatCoeff(magnitude)
                + MathDsl::buildWrappedToken(mulOp, mulWrap)
                + base;
        }

        if (rebuilt.isEmpty()) {
            rebuilt = negative ? (QString(MathDsl::SubOpAl1) + term) : term;
        } else {
            rebuilt += negative ? QString(MathDsl::SubOpAl1) : QString(MathDsl::AddOp);
            rebuilt += term;
        }
    }

    return rebuilt.isEmpty() ? text : rebuilt;
}

inline QString simplifiedExpressionLineForDisplay(const QString& interpretedExpression,
                                                  const QString& sourceExpression,
                                                  bool simplifyResultExpressions,
                                                  const Evaluator* evaluator = nullptr)
{
    if (!simplifyResultExpressions)
        return QString();

    auto splitConversionSides = [](const QString& expression,
                                   QString* leftOut,
                                   QString* rightOut) -> bool {
        const QString asciiArrow = QString(MathDsl::SubOpAl1) + QString(MathDsl::GreaterThanOp);
        int splitPos = expression.lastIndexOf(MathDsl::TransOp);
        int splitWidth = 1;
        if (splitPos < 0) {
            splitPos = expression.lastIndexOf(asciiArrow);
            splitWidth = asciiArrow.size();
        }
        if (splitPos < 0)
            return false;

        if (leftOut)
            *leftOut = expression.left(splitPos).trimmed();
        if (rightOut)
            *rightOut = expression.mid(splitPos + splitWidth).trimmed();
        return true;
    };

    QString sourceLeft;
    QString sourceRight;
    const bool sourceHasConversionTarget =
        splitConversionSides(sourceExpression, &sourceLeft, &sourceRight);

    QString expressionForSimplification = interpretedExpression;
    if (sourceHasConversionTarget) {
        QString interpretedLeft;
        QString interpretedRight;
        if (splitConversionSides(interpretedExpression, &interpretedLeft, &interpretedRight)
            && !interpretedLeft.isEmpty())
        {
            expressionForSimplification = interpretedLeft;
        } else if (!sourceLeft.isEmpty()) {
            expressionForSimplification = sourceLeft;
        } else if (!sourceRight.isEmpty()) {
            expressionForSimplification = sourceRight;
        }
    }

    if (expressionForSimplification.isEmpty()) {
        if (!sourceLeft.isEmpty())
            expressionForSimplification = sourceLeft;
        else if (!sourceRight.isEmpty())
            expressionForSimplification = sourceRight;

        const bool sourceHasExplicitAngles =
            containsExplicitBracketedAngleUnit(sourceExpression)
            || containsExplicitSexagesimalAngleMarkers(sourceExpression);
        if (sourceHasExplicitAngles)
            expressionForSimplification = sourceExpression;
    }
    if (expressionForSimplification.isEmpty())
        return QString();

    QString interpretedDisplay = DisplayFormatUtils::applyDigitGroupingForDisplay(
        Evaluator::formatInterpretedExpressionForDisplay(expressionForSimplification, evaluator));
    QString simplifiedDisplay = DisplayFormatUtils::applyDigitGroupingForDisplay(
        Evaluator::formatInterpretedExpressionSimplifiedForDisplay(expressionForSimplification, evaluator));
    if (!sourceExpression.isEmpty()) {
        interpretedDisplay = DisplayFormatUtils::preserveConversionTargetBracketsForDisplay(
            interpretedDisplay, sourceExpression);
        simplifiedDisplay = DisplayFormatUtils::preserveConversionTargetBracketsForDisplay(
            simplifiedDisplay, sourceExpression);
    }
    interpretedDisplay = normalizeBracketedUnitTextForDisplay(interpretedDisplay);
    simplifiedDisplay = normalizeBracketedUnitTextForDisplay(simplifiedDisplay);
    interpretedDisplay = collapseValueUnitMultiplicationForDisplay(interpretedDisplay);
    simplifiedDisplay = collapseValueUnitMultiplicationForDisplay(simplifiedDisplay);
    interpretedDisplay = collapseBracketedCompactAngleSuffixes(interpretedDisplay);
    simplifiedDisplay = collapseBracketedCompactAngleSuffixes(simplifiedDisplay);
    if (simplifiedDisplay.isEmpty()
        || simplifiedDisplay == interpretedDisplay
        || simplifiedDisplay == SimplifiedExpressionUtils::expressionWithoutTopLevelComment(
            interpretedDisplay))
    {
        return QString();
    }

    if (SimplifiedExpressionUtils::shouldSuppressSimplifiedExpressionLine(
            interpretedDisplay, simplifiedDisplay))
        return QString();

    return foldRepeatedAdditiveTermsForDisplay(simplifiedDisplay);
}

inline bool splitConversionExpressionForDisplay(const QString& expression,
                                                QString* leftOut,
                                                QString* rightOut)
{
    const int asciiArrowPos = expression.lastIndexOf(QString(MathDsl::SubOpAl1) + MathDsl::GreaterThanOp);
    const int unicodeArrowPos = expression.lastIndexOf(MathDsl::TransOp);
    int textualInPos = -1;
    QRegularExpressionMatchIterator inMatches =
        QRegularExpression(QStringLiteral("\\bin\\b"),
                           QRegularExpression::CaseInsensitiveOption).globalMatch(expression);
    while (inMatches.hasNext())
        textualInPos = inMatches.next().capturedStart();

    int arrowPos = -1;
    int arrowWidth = 0;
    if (asciiArrowPos >= 0
        && asciiArrowPos >= unicodeArrowPos
        && asciiArrowPos >= textualInPos) {
        arrowPos = asciiArrowPos;
        arrowWidth = 2;
    } else if (unicodeArrowPos >= 0 && unicodeArrowPos >= textualInPos) {
        arrowPos = unicodeArrowPos;
        arrowWidth = 1;
    } else if (textualInPos >= 0) {
        arrowPos = textualInPos;
        arrowWidth = 2;
    }

    if (arrowPos < 0)
        return false;

    if (leftOut)
        *leftOut = expression.left(arrowPos).trimmed();
    if (rightOut)
        *rightOut = expression.mid(arrowPos + arrowWidth).trimmed();
    return true;
}

inline QString conversionTargetSuffixForDisplay(const QString& expression)
{
    QString target;
    if (!splitConversionExpressionForDisplay(expression, nullptr, &target))
        return QString();

    if (target.isEmpty())
        return QString();

    return QStringLiteral(" \u2192 ") + target;
}

inline QString formattedExpressionLineForDisplay(const QString& sourceExpression,
                                                 const QString& interpretedExpression,
                                                 const Evaluator* evaluator = nullptr)
{
    const QString sourceConversionTargetSuffix =
        conversionTargetSuffixForDisplay(sourceExpression);
    if (!sourceConversionTargetSuffix.isEmpty()) {
        QString sourceLeft;
        splitConversionExpressionForDisplay(sourceExpression, &sourceLeft, nullptr);
        const bool preserveConvertedStandaloneSexagesimalAngle =
            isStandaloneSexagesimalAngleLiteral(sourceLeft)
            && !sourceLeft.contains(MathDsl::UnitStart)
            && !sourceLeft.contains(MathDsl::UnitEnd);
        if (preserveConvertedStandaloneSexagesimalAngle) {
            return DisplayFormatUtils::applyDigitGroupingForDisplay(
                UnicodeChars::normalizePiForDisplay(sourceLeft))
                + sourceConversionTargetSuffix;
        }
    }

    const bool preserveStandaloneSexagesimalAngle =
        isStandaloneSexagesimalAngleLiteral(sourceExpression)
        && !sourceExpression.contains(MathDsl::UnitStart)
        && !sourceExpression.contains(MathDsl::UnitEnd)
        && !sourceExpression.contains(QString(MathDsl::SubOpAl1) + MathDsl::GreaterThanOp)
        && !sourceExpression.contains(MathDsl::TransOp);
    if (preserveStandaloneSexagesimalAngle) {
        return DisplayFormatUtils::applyDigitGroupingForDisplay(
            UnicodeChars::normalizePiForDisplay(sourceExpression.trimmed()));
    }

    const QString interpretedSource = interpretedExpression.isEmpty()
        ? sourceExpression
        : interpretedExpression;
    const QString displayed = DisplayFormatUtils::applyDigitGroupingForDisplay(
        UnicodeChars::normalizePiForDisplay(
            Evaluator::formatInterpretedExpressionForDisplay(interpretedSource, evaluator)));
    return collapseBracketedCompactAngleSuffixes(
        collapseValueUnitMultiplicationForDisplay(
            preserveExplicitParenthesizedUnitDenominatorFromSource(
                preserveExplicitParenthesizedQuotientBeforeUnitFromSource(
                preserveExplicitBracketedSimpleUnitsFromSource(
                    normalizeBracketedUnitTextForDisplay(
                        DisplayFormatUtils::preserveConversionTargetBracketsForDisplay(
                            displayed, sourceExpression)),
                    sourceExpression),
                sourceExpression),
                sourceExpression)));
}

inline QString trimTrailingFractionZeros(QString text)
{
    static const QRegularExpression trailingZerosAfterNonZero(
        QStringLiteral("([\\.,]\\d*?[1-9])0+$"));
    static const QRegularExpression onlyZeroFraction(
        QStringLiteral("[\\.,]0+$"));

    text.replace(trailingZerosAfterNonZero, QStringLiteral("\\1"));
    text.replace(onlyZeroFraction, QString());
    return text;
}

inline QString stripDisplayedUnitBrackets(QString text)
{
    const int openBracket = text.lastIndexOf(MathDsl::UnitStart);
    const int closeBracket = text.lastIndexOf(MathDsl::UnitEnd);
    if (openBracket >= 0
        && closeBracket > openBracket
        && closeBracket == text.size() - 1)
    {
        const QString normalizedUnit = UnitDisplayFormat::normalizeUnitTextForDisplay(
            text.mid(openBracket + 1, closeBracket - openBracket - 1));
        text = text.left(openBracket) + normalizedUnit;
    } else {
        text.replace(RegExpPatterns::unitBrackets(), QStringLiteral("\\1"));
    }
    auto endsWithPowerOfTenScientificNotation = [](const QString& prefix) {
        const QString trimmed = prefix.trimmed();
        return RegExpPatterns::trailingPowerOfTenScientificNotation().match(trimmed).hasMatch();
    };
    auto collapseTrailingCompactSuffix = [&text, &endsWithPowerOfTenScientificNotation](const QChar suffix) {
        const QString quantSpVariant = QString(MathDsl::QuantSp) + suffix;
        const QString asciiSpVariant = QStringLiteral(" ") + suffix;
        const QString compactVariant = QString(suffix);
        if (text.endsWith(quantSpVariant)) {
            if (endsWithPowerOfTenScientificNotation(
                    text.left(text.size() - quantSpVariant.size()))) {
                return;
            }
            text.chop(quantSpVariant.size());
            text += suffix;
        } else if (text.endsWith(asciiSpVariant)) {
            if (endsWithPowerOfTenScientificNotation(
                    text.left(text.size() - asciiSpVariant.size()))) {
                return;
            }
            text.chop(asciiSpVariant.size());
            text += suffix;
        } else if (text.endsWith(compactVariant)) {
            if (endsWithPowerOfTenScientificNotation(
                    text.left(text.size() - compactVariant.size()))) {
                text.insert(text.size() - compactVariant.size(), MathDsl::QuantSp);
            }
        }
    };
    collapseTrailingCompactSuffix(UnicodeChars::DegreeSign);
    collapseTrailingCompactSuffix(UnicodeChars::Prime);
    collapseTrailingCompactSuffix(UnicodeChars::DoublePrime);
    return text;
}

inline bool isPureTimeQuantity(const Quantity& value)
{
    if (!value.hasDimension())
        return false;

    const auto dimension = value.getDimensionByQuantity();
    if (dimension.count() != 1 || !dimension.contains(UnitQuantity::Time))
        return false;

    const auto it = dimension.constFind(UnitQuantity::Time);
    return it != dimension.constEnd()
        && it->numerator() == 1
        && it->denominator() == 1;
}

inline bool hasCompoundSexagesimalFields(const QString& expression)
{
    const QString trimmed = expression.trimmed();
    const QList<QChar> degreeMarkers = {
        UnicodeChars::DegreeSign,
        UnicodeChars::MasculineOrdinalIndicator,
        UnicodeChars::RingOperator
    };
    for (const QChar marker : degreeMarkers) {
        const int markerPos = trimmed.indexOf(marker);
        if (markerPos >= 0)
            return !trimmed.mid(markerPos + 1).trimmed().isEmpty();
    }

    int minuteSecondMarkerCount = 0;
    const QList<QChar> minuteSecondMarkers = {
        UnicodeChars::Prime,
        UnicodeChars::DoublePrime,
        UnicodeChars::Apostrophe,
        UnicodeChars::QuotationMark
    };
    for (const QChar ch : trimmed) {
        if (minuteSecondMarkers.contains(ch))
            ++minuteSecondMarkerCount;
    }

    return minuteSecondMarkerCount > 1;
}

inline bool shouldPreserveDetailedStandaloneSexagesimalAngle(
    const QString& sourceExpression,
    const Quantity& value,
    const Settings* settings)
{
    return value.isDimensionless()
        && settings->angleUnit == 'd'
        && isStandaloneSexagesimalAngleLiteral(sourceExpression)
        && hasCompoundSexagesimalFields(sourceExpression)
        && !sourceExpression.contains(QString(MathDsl::SubOpAl1) + MathDsl::GreaterThanOp)
        && !sourceExpression.contains(MathDsl::TransOp);
}

inline bool expressionUsesTrigFunction(const QString& sourceExpression,
                                       const QString& interpretedExpression,
                                       const Evaluator* evaluator = nullptr)
{
    const QString source = interpretedExpression.isEmpty()
        ? sourceExpression
        : interpretedExpression;
    if (RegExpPatterns::trigFunctionCall().match(source).hasMatch())
        return true;

    const QList<UserFunction> userFunctions =
        (evaluator ? evaluator : Evaluator::instance())->getUserFunctions();
    if (userFunctions.isEmpty())
        return false;

    QHash<QString, QString> bodiesByName;
    for (const UserFunction& function : userFunctions) {
        QString body = function.interpretedExpression().trimmed();
        if (body.isEmpty())
            body = function.expression().trimmed();
        if (!body.isEmpty())
            bodiesByName.insert(function.name(), body);
    }
    if (bodiesByName.isEmpty())
        return false;

    const QRegularExpression& anyFunctionCall = RegExpPatterns::anyFunctionCall();
    auto callsUserFunctionByName = [&bodiesByName, &anyFunctionCall](const QString& text) {
        QRegularExpressionMatchIterator it = anyFunctionCall.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            if (bodiesByName.contains(match.captured(1)))
                return true;
        }
        return false;
    };
    if (!callsUserFunctionByName(source))
        return false;

    QHash<QString, bool> memo;
    QSet<QString> visiting;
    std::function<bool(const QString&)> userFunctionUsesTrig = [&](const QString& functionName) -> bool {
        const auto memoIt = memo.constFind(functionName);
        if (memoIt != memo.constEnd())
            return memoIt.value();
        if (visiting.contains(functionName))
            return false;

        const QString body = bodiesByName.value(functionName);
        if (body.isEmpty())
            return false;

        visiting.insert(functionName);
        bool usesTrig = RegExpPatterns::trigFunctionCall().match(body).hasMatch();
        if (!usesTrig) {
            QRegularExpressionMatchIterator it = anyFunctionCall.globalMatch(body);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                const QString callee = match.captured(1);
                if (bodiesByName.contains(callee) && userFunctionUsesTrig(callee)) {
                    usesTrig = true;
                    break;
                }
            }
        }
        visiting.remove(functionName);
        memo.insert(functionName, usesTrig);
        return usesTrig;
    };

    QRegularExpressionMatchIterator sourceCalls = anyFunctionCall.globalMatch(source);
    while (sourceCalls.hasNext()) {
        const QRegularExpressionMatch match = sourceCalls.next();
        const QString called = match.captured(1);
        if (bodiesByName.contains(called) && userFunctionUsesTrig(called))
            return true;
    }

    return false;
}

inline bool expressionUsesPhasorOperator(const QString& sourceExpression,
                                         const QString& interpretedExpression)
{
    return sourceExpression.contains(MathDsl::PhasorOp)
        || interpretedExpression.contains(MathDsl::PhasorOp);
}

inline bool shouldShowAdditionalRationalForTrig(const Settings* settings,
                                                 const QString& sourceExpression,
                                                 const QString& interpretedExpression,
                                                 const Quantity& value,
                                                 const Evaluator* evaluator = nullptr)
{
    const bool hasRationalAlready =
        settings->resultFormat == 'r'
        || (settings->multipleResultLinesEnabled
            && settings->secondaryResultEnabled && settings->alternativeResultFormat == 'r')
        || (settings->multipleResultLinesEnabled
            && settings->tertiaryResultEnabled && settings->tertiaryResultFormat == 'r')
        || (settings->multipleResultLinesEnabled
            && settings->quaternaryResultEnabled && settings->quaternaryResultFormat == 'r')
        || (settings->multipleResultLinesEnabled
            && settings->quinaryResultEnabled && settings->quinaryResultFormat == 'r');
    if (hasRationalAlready)
        return false;

    if (!expressionUsesTrigFunction(sourceExpression, interpretedExpression, evaluator))
        return false;

    return !NumberFormatter::formatTrigSymbolic(value).isEmpty();
}

inline QString conversionTargetText(const QString& expression)
{
    const int asciiArrowPos = expression.lastIndexOf(QString(MathDsl::SubOpAl1) + MathDsl::GreaterThanOp);
    const int unicodeArrowPos = expression.lastIndexOf(MathDsl::TransOp);

    int arrowPos = -1;
    int arrowWidth = 0;
    if (asciiArrowPos >= 0 && asciiArrowPos >= unicodeArrowPos) {
        arrowPos = asciiArrowPos;
        arrowWidth = 2;
    } else if (unicodeArrowPos >= 0) {
        arrowPos = unicodeArrowPos;
        arrowWidth = 1;
    }

    if (arrowPos < 0)
        return QString();

    return expression.mid(arrowPos + arrowWidth).trimmed();
}

inline bool isRadianUnitText(QString text)
{
    text = text.trimmed();
    if (text.startsWith(MathDsl::UnitStart) && text.endsWith(MathDsl::UnitEnd) && text.size() > 2)
        text = text.mid(1, text.size() - 2).trimmed();
    text = normalizeUnitName(UnicodeChars::normalizeUnitSymbolAliases(text));
    return unitId(text) == UnitId::Radian;
}

inline bool isRadianResultContext(const Settings* settings,
                                  const QString& sourceExpression,
                                  const QString& interpretedExpression)
{
    if (settings->angleUnit == 'r')
        return true;

    const QString sourceTarget = conversionTargetText(sourceExpression);
    if (!sourceTarget.isEmpty() && isRadianUnitText(sourceTarget))
        return true;

    const QString interpretedTarget = conversionTargetText(interpretedExpression);
    return !interpretedTarget.isEmpty() && isRadianUnitText(interpretedTarget);
}

inline bool expressionUsesTrigOrExplicitAngleInput(const QString& sourceExpression,
                                                   const QString& interpretedExpression,
                                                   const Evaluator* evaluator = nullptr)
{
    if (expressionUsesTrigFunction(sourceExpression, interpretedExpression, evaluator))
        return true;

    return containsExplicitBracketedAngleUnit(sourceExpression)
        || containsExplicitBracketedAngleUnit(interpretedExpression)
        || containsExplicitSexagesimalAngleMarkers(sourceExpression)
        || containsExplicitSexagesimalAngleMarkers(interpretedExpression);
}

inline bool tryExtractRealRadiansFromResult(const Quantity& value, HNumber* radiansOut)
{
    if (!radiansOut || value.isNan())
        return false;

    Quantity scalar = value;
    if (scalar.hasUnit()) {
        CNumber displayNumber = scalar.numericValue();
        displayNumber /= scalar.unit();
        if (!displayNumber.isNearReal())
            return false;

        Quantity normalized(displayNumber.real);
        normalized.setDisplayUnit(CNumber(1), scalar.unitName());
        if (!Units::tryConvertExplicitAngleToRadians(&normalized))
            return false;

        scalar = normalized;
    }

    if (!scalar.isDimensionless())
        return false;

    const CNumber numeric = scalar.numericValue();
    if (!numeric.isNearReal())
        return false;

    *radiansOut = numeric.real;
    return true;
}

inline QString formatPiMultipleForRadians(const HNumber& radians)
{
    static const HNumber tolerance("1e-20");
    static const int maxDenominator = 3600;

    Rational ratio;
    if (!Rational::approximate(radians / HMath::pi(), maxDenominator, tolerance, &ratio))
        return QString();

    const int numerator = ratio.numerator();
    const int denominator = ratio.denominator();
    if (numerator == 0 || denominator <= 0)
        return QString();

    const HNumber expected = HMath::pi() * HNumber(numerator) / HNumber(denominator);
    const HNumber diff = HMath::abs(radians - expected);
    const HNumber scale = HMath::max(HMath::max(HMath::abs(radians), HMath::abs(expected)), HNumber(1));
    if (diff > tolerance * scale)
        return QString();

    const QString division = QString(MathDsl::DivWrap) + QString(MathDsl::DivOp) + QString(MathDsl::DivWrap);
    const QString multiplication = QString(MathDsl::MulDotWrapSp) + QString(MathDsl::MulDotOp)
        + QString(MathDsl::MulDotWrapSp);
    const QString pi = QString(UnicodeChars::Pi);
    const QString sign = (numerator < 0) ? QString(MathDsl::SubOp) : QString();
    const int absNumerator = qAbs(numerator);

    if (denominator == 1) {
        if (absNumerator == 1)
            return sign + pi;
        return sign + QString::number(absNumerator) + multiplication + pi;
    }

    if (absNumerator == 1)
        return sign + pi + division + QString::number(denominator);

    return sign
        + QString::number(absNumerator)
        + division
        + QString::number(denominator)
        + multiplication
        + pi;
}

inline QString formatPiRadianResultLineIfNeeded(const Settings* settings,
                                                const QString& sourceExpression,
                                                const QString& interpretedExpression,
                                                const Quantity& value,
                                                const Evaluator* evaluator = nullptr)
{
    if (expressionUsesPhasorOperator(sourceExpression, interpretedExpression))
        return QString();
    if (!expressionUsesTrigOrExplicitAngleInput(sourceExpression, interpretedExpression, evaluator))
        return QString();
    if (!isRadianResultContext(settings, sourceExpression, interpretedExpression))
        return QString();

    HNumber radians;
    if (!tryExtractRealRadiansFromResult(value, &radians))
        return QString();

    const QString piMultiple = formatPiMultipleForRadians(radians);
    if (piMultiple.isEmpty())
        return QString();

    return DisplayFormatUtils::applyDigitGroupingForDisplay(
        piMultiple + QString(MathDsl::QuantSp) + Units::angleModeUnitSymbol('r'));
}

inline QString normalizeResultLineForComparison(QString text)
{
    text.replace(MathDsl::QuantSp, QLatin1Char(' '));
    text.replace(UnicodeChars::NoBreakSpace, QLatin1Char(' '));
    text.replace(MathDsl::SubOp, MathDsl::SubOpAl1);
    return text;
}

inline QString appendAngleModeSuffixIfNeeded(const QString& formattedText,
                                             const QString& sourceExpression,
                                             const QString& interpretedExpression,
                                             const Quantity& value,
                                             char resultFormat,
                                             const Settings* settings,
                                             const Evaluator* evaluator = nullptr)
{
    if (resultFormat == 's')
        return formattedText;
    if (!value.isDimensionless())
        return formattedText;
    if (formattedText.contains(MathDsl::UnitStart) || formattedText.contains(MathDsl::UnitEnd))
        return formattedText;
    if (expressionUsesTrigFunction(sourceExpression, interpretedExpression, evaluator))
        return formattedText;
    if (expressionUsesPhasorOperator(sourceExpression, interpretedExpression))
        return formattedText;
    const bool hasExplicitBracketedAngleUnit =
        containsExplicitBracketedAngleUnit(sourceExpression)
        || containsExplicitBracketedAngleUnit(interpretedExpression);
    const bool hasExplicitSexagesimalAngleMarkers =
        containsExplicitSexagesimalAngleMarkers(sourceExpression)
        || containsExplicitSexagesimalAngleMarkers(interpretedExpression);
    if (!hasExplicitBracketedAngleUnit && !hasExplicitSexagesimalAngleMarkers)
        return formattedText;

    auto endsWithUnitToken = [&formattedText](const QString& token) {
        return formattedText.endsWith(token)
            || formattedText.endsWith(QString(MathDsl::QuantSp) + token)
            || formattedText.endsWith(QStringLiteral(" ") + token);
    };
    if (endsWithUnitToken(Units::angleModeUnitSymbol('r'))
        || endsWithUnitToken(Units::angleModeUnitSymbol('d'))
        || endsWithUnitToken(Units::angleModeUnitSymbol('g'))
        || endsWithUnitToken(Units::angleModeUnitSymbol('t'))
        || endsWithUnitToken(Units::angleModeUnitSymbol('v'))
        || formattedText.endsWith(UnicodeChars::Prime)
        || formattedText.endsWith(UnicodeChars::DoublePrime)) {
        return formattedText;
    }

    const QString symbol = Units::angleModeUnitSymbol(settings->angleUnit);
    const bool hasPowerOfTenScientificSuffix =
        RegExpPatterns::trailingPowerOfTenScientificNotation().match(
            formattedText.trimmed()).hasMatch();
    if (settings->angleUnit == 'd' && !hasPowerOfTenScientificSuffix)
        return formattedText + symbol;
    return formattedText + QString(MathDsl::QuantSp) + symbol;
}

inline QString formatNumericResultLine(const Quantity& value,
                                       const QString& sourceExpression,
                                       const QString& interpretedExpression,
                                       char resultFormat,
                                       int precision,
                                       bool complexNumbers,
                                       char complexFormat,
                                       bool stripUnitBrackets,
                                       const Settings* settings,
                                       const Evaluator* evaluator = nullptr)
{
    if (!expressionUsesTrigFunction(sourceExpression, interpretedExpression, evaluator)
        && shouldPreserveDetailedStandaloneSexagesimalAngle(
            sourceExpression, value, settings)) {
        resultFormat = 's';
    }

    Quantity formattedValue = value;
    if (resultFormat == 's' && isPureTimeQuantity(formattedValue))
        formattedValue.stripUnits();
    QString formattedText = DisplayFormatUtils::applyDigitGroupingForDisplay(
        NumberFormatter::format(formattedValue,
                                resultFormat,
                                precision,
                                complexNumbers,
                                complexFormat));
    formattedText = NumberFormatter::rewriteScientificNotationForDisplay(formattedText);
    formattedText.replace(QString::fromUtf8(" × 10⁰"), QString());
    formattedText = appendAngleModeSuffixIfNeeded(
        formattedText, sourceExpression, interpretedExpression, value, resultFormat, settings, evaluator);
    return stripUnitBrackets ? stripDisplayedUnitBrackets(formattedText) : formattedText;
}

inline QStringList formatResultLinesForDisplay(const QString& sourceExpression,
                                               const QString& interpretedExpression,
                                               const Quantity& value,
                                               bool includeExpressionLine,
                                               bool stripUnitBracketsInNumericLines,
                                               const Evaluator* evaluator = nullptr)
{
    const Settings* settings = Settings::instance();
    QStringList lines;
    auto appendUniqueLine = [&lines](const QString& line) {
        if (!line.isEmpty() && !lines.contains(line))
            lines.append(line);
    };

    if (includeExpressionLine)
        appendUniqueLine(formattedExpressionLineForDisplay(sourceExpression, interpretedExpression, evaluator));

    const QString simplifiedLine = simplifiedExpressionLineForDisplay(
        interpretedExpression, sourceExpression, settings->simplifyResultExpressions, evaluator);
    if (!simplifiedLine.isEmpty())
        appendUniqueLine(QStringLiteral("= ") + simplifiedLine);

    if (settings->simplifyResultExpressions
        && isPureTimeQuantity(value)
        && (SimplifiedExpressionUtils::isStandaloneSexagesimalTimeLiteral(sourceExpression)
            || SimplifiedExpressionUtils::containsSexagesimalTimeLiteral(sourceExpression))) {
        Quantity sexagesimalValue(value);
        sexagesimalValue.stripUnits();
        const QString normalizedSexagesimal = DisplayFormatUtils::applyDigitGroupingForDisplay(
            trimTrailingFractionZeros(
            NumberFormatter::format(sexagesimalValue,
                                    's',
                                    settings->resultPrecision,
                                    settings->complexNumbers,
                                    settings->resultComplexForm)));
        appendUniqueLine(QStringLiteral("= ")
            + normalizedSexagesimal
            + conversionTargetSuffixForDisplay(sourceExpression));
    }

    appendUniqueLine(QStringLiteral("= ") + formatNumericResultLine(
        value,
        sourceExpression,
        interpretedExpression,
        settings->resultFormat,
        settings->resultPrecision,
        settings->complexNumbers,
        settings->resultComplexForm,
        stripUnitBracketsInNumericLines,
        settings,
        evaluator));

    if (settings->multipleResultLinesEnabled && settings->secondaryResultEnabled
        && settings->alternativeResultFormat != '\0') {
        appendUniqueLine(QStringLiteral("= ") + formatNumericResultLine(
            value,
            sourceExpression,
            interpretedExpression,
            settings->alternativeResultFormat,
            settings->secondaryResultPrecision,
            settings->complexNumbers && settings->secondaryComplexNumbers,
            settings->secondaryResultComplexForm,
            stripUnitBracketsInNumericLines,
            settings,
            evaluator));
    }
    if (settings->multipleResultLinesEnabled && settings->tertiaryResultEnabled
        && settings->tertiaryResultFormat != '\0') {
        appendUniqueLine(QStringLiteral("= ") + formatNumericResultLine(
            value,
            sourceExpression,
            interpretedExpression,
            settings->tertiaryResultFormat,
            settings->tertiaryResultPrecision,
            settings->complexNumbers && settings->tertiaryComplexNumbers,
            settings->tertiaryResultComplexForm,
            stripUnitBracketsInNumericLines,
            settings,
            evaluator));
    }
    if (settings->multipleResultLinesEnabled && settings->quaternaryResultEnabled
        && settings->quaternaryResultFormat != '\0') {
        appendUniqueLine(QStringLiteral("= ") + formatNumericResultLine(
            value,
            sourceExpression,
            interpretedExpression,
            settings->quaternaryResultFormat,
            settings->quaternaryResultPrecision,
            settings->complexNumbers && settings->quaternaryComplexNumbers,
            settings->quaternaryResultComplexForm,
            stripUnitBracketsInNumericLines,
            settings,
            evaluator));
    }
    if (settings->multipleResultLinesEnabled && settings->quinaryResultEnabled
        && settings->quinaryResultFormat != '\0') {
        appendUniqueLine(QStringLiteral("= ") + formatNumericResultLine(
            value,
            sourceExpression,
            interpretedExpression,
            settings->quinaryResultFormat,
            settings->quinaryResultPrecision,
            settings->complexNumbers && settings->quinaryComplexNumbers,
            settings->quinaryResultComplexForm,
            stripUnitBracketsInNumericLines,
            settings,
            evaluator));
    }

    const QString piRadianResultLine = formatPiRadianResultLineIfNeeded(
        settings, sourceExpression, interpretedExpression, value, evaluator);
    if (!piRadianResultLine.isEmpty())
        appendUniqueLine(QStringLiteral("= ") + piRadianResultLine);

    if (shouldShowAdditionalRationalForTrig(
            settings, sourceExpression, interpretedExpression, value, evaluator)) {
        const QString trigSymbolicLine = DisplayFormatUtils::applyDigitGroupingForDisplay(
            NumberFormatter::formatTrigSymbolic(value));
        const QString trigSymbolicNormalizedPi = UnicodeChars::normalizePiForDisplay(trigSymbolicLine);
        if (trigSymbolicNormalizedPi.contains(UnicodeChars::Pi))
            return lines;
        const QString piRadianFromTrigLine = trigSymbolicLine
            + QString(MathDsl::QuantSp)
            + Units::angleModeUnitSymbol('r');
        if (piRadianResultLine.isEmpty()
            || normalizeResultLineForComparison(piRadianResultLine)
                != normalizeResultLineForComparison(piRadianFromTrigLine)) {
            appendUniqueLine(QStringLiteral("= ") + trigSymbolicLine);
        }
    }

    return lines;
}

} // namespace ResultLineFormatUtils

#endif
