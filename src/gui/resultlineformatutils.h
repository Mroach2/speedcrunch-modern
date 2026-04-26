// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef GUI_RESULTLINEFORMATUTILS_H
#define GUI_RESULTLINEFORMATUTILS_H

#include "core/evaluator.h"
#include "core/mathdsl.h"
#include "core/numberformatter.h"
#include "core/regexpatterns.h"
#include "core/settings.h"
#include "core/unicodechars.h"
#include "core/units.h"
#include "gui/displayformatutils.h"
#include "gui/simplifiedexpressionutils.h"

#include <QRegularExpression>
#include <QString>
#include <QVector>

namespace ResultLineFormatUtils {

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
            // Keep coefficient*term rendering on the same DSL-wrapped spacing
            // path used by display formatting, so mixed-alias inputs do not
            // regress to compact "4·x" while other paths show "4 · x".
            term = formatCoeff(magnitude)
                + MathDsl::buildWrappedToken(MathDsl::MulDotOp, MathDsl::MulDotWrapSp)
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
                                                  bool simplifyResultExpressions)
{
    if (!simplifyResultExpressions)
        return QString();

    QString expressionForSimplification = interpretedExpression;
    if (expressionForSimplification.isEmpty()) {
        const bool sourceHasExplicitAngles =
            containsExplicitBracketedAngleUnit(sourceExpression)
            || containsExplicitSexagesimalAngleMarkers(sourceExpression);
        if (sourceHasExplicitAngles)
            expressionForSimplification = sourceExpression;
    }
    if (expressionForSimplification.isEmpty())
        return QString();

    QString interpretedDisplay = DisplayFormatUtils::applyDigitGroupingForDisplay(
        Evaluator::formatInterpretedExpressionForDisplay(expressionForSimplification));
    QString simplifiedDisplay = DisplayFormatUtils::applyDigitGroupingForDisplay(
        Evaluator::formatInterpretedExpressionSimplifiedForDisplay(expressionForSimplification));
    if (!sourceExpression.isEmpty()) {
        interpretedDisplay = DisplayFormatUtils::preserveConversionTargetBracketsForDisplay(
            interpretedDisplay, sourceExpression);
        simplifiedDisplay = DisplayFormatUtils::preserveConversionTargetBracketsForDisplay(
            simplifiedDisplay, sourceExpression);
    }
    if (simplifiedDisplay.isEmpty() || simplifiedDisplay == interpretedDisplay)
        return QString();

    if (SimplifiedExpressionUtils::shouldSuppressSimplifiedExpressionLine(
            interpretedDisplay, simplifiedDisplay))
        return QString();

    return foldRepeatedAdditiveTermsForDisplay(simplifiedDisplay);
}

inline QString formattedExpressionLineForDisplay(const QString& sourceExpression,
                                                 const QString& interpretedExpression)
{
    const bool preserveStandaloneSexagesimalAngle =
        isStandaloneSexagesimalAngleLiteral(sourceExpression)
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
            Evaluator::formatInterpretedExpressionForDisplay(interpretedSource)));
    return DisplayFormatUtils::preserveConversionTargetBracketsForDisplay(
        displayed, sourceExpression);
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
    text.replace(RegExpPatterns::unitBrackets(), QStringLiteral("\\1"));
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

inline QString conversionTargetSuffixForDisplay(const QString& expression)
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

    const QString target = expression.mid(arrowPos + arrowWidth).trimmed();
    if (target.isEmpty())
        return QString();

    return QStringLiteral(" \u2192 ") + target;
}

inline bool shouldPreserveStandaloneSexagesimalAngle(const QString& sourceExpression,
                                                      const Quantity& value,
                                                      const Settings* settings)
{
    return value.isDimensionless()
        && settings->angleUnit == 'd'
        && isStandaloneSexagesimalAngleLiteral(sourceExpression)
        && !sourceExpression.contains(QString(MathDsl::SubOpAl1) + MathDsl::GreaterThanOp)
        && !sourceExpression.contains(MathDsl::TransOp);
}

inline bool expressionUsesTrigFunction(const QString& sourceExpression,
                                       const QString& interpretedExpression)
{
    const QString source = interpretedExpression.isEmpty()
        ? sourceExpression
        : interpretedExpression;
    return RegExpPatterns::trigFunctionCall().match(source).hasMatch();
}

inline bool shouldShowAdditionalRationalForTrig(const Settings* settings,
                                                 const QString& sourceExpression,
                                                 const QString& interpretedExpression,
                                                 const Quantity& value)
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

    if (!expressionUsesTrigFunction(sourceExpression, interpretedExpression))
        return false;

    return !NumberFormatter::formatTrigSymbolic(value).isEmpty();
}

inline QString appendAngleModeSuffixIfNeeded(const QString& formattedText,
                                             const QString& sourceExpression,
                                             const QString& interpretedExpression,
                                             const Quantity& value,
                                             char resultFormat,
                                             const Settings* settings)
{
    if (resultFormat == 's')
        return formattedText;
    if (!value.isDimensionless())
        return formattedText;
    if (formattedText.contains(MathDsl::UnitStart) || formattedText.contains(MathDsl::UnitEnd))
        return formattedText;
    if (expressionUsesTrigFunction(sourceExpression, interpretedExpression))
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
                                       const Settings* settings)
{
    if (!expressionUsesTrigFunction(sourceExpression, interpretedExpression)
        && shouldPreserveStandaloneSexagesimalAngle(
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
    formattedText = appendAngleModeSuffixIfNeeded(
        formattedText, sourceExpression, interpretedExpression, value, resultFormat, settings);
    return stripUnitBrackets ? stripDisplayedUnitBrackets(formattedText) : formattedText;
}

inline QStringList formatResultLinesForDisplay(const QString& sourceExpression,
                                               const QString& interpretedExpression,
                                               const Quantity& value,
                                               bool includeExpressionLine,
                                               bool stripUnitBracketsInNumericLines)
{
    const Settings* settings = Settings::instance();
    QStringList lines;
    auto appendUniqueLine = [&lines](const QString& line) {
        if (!line.isEmpty() && !lines.contains(line))
            lines.append(line);
    };

    if (includeExpressionLine)
        appendUniqueLine(formattedExpressionLineForDisplay(sourceExpression, interpretedExpression));

    const QString simplifiedLine = simplifiedExpressionLineForDisplay(
        interpretedExpression, sourceExpression, settings->simplifyResultExpressions);
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
                                    settings->resultFormatComplex)));
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
        settings->resultFormatComplex,
        stripUnitBracketsInNumericLines,
        settings));

    if (settings->multipleResultLinesEnabled && settings->secondaryResultEnabled
        && settings->alternativeResultFormat != '\0') {
        appendUniqueLine(QStringLiteral("= ") + formatNumericResultLine(
            value,
            sourceExpression,
            interpretedExpression,
            settings->alternativeResultFormat,
            settings->secondaryResultPrecision,
            settings->complexNumbers && settings->secondaryComplexNumbers,
            settings->secondaryResultFormatComplex,
            stripUnitBracketsInNumericLines,
            settings));
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
            settings->tertiaryResultFormatComplex,
            stripUnitBracketsInNumericLines,
            settings));
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
            settings->quaternaryResultFormatComplex,
            stripUnitBracketsInNumericLines,
            settings));
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
            settings->quinaryResultFormatComplex,
            stripUnitBracketsInNumericLines,
            settings));
    }

    if (shouldShowAdditionalRationalForTrig(
            settings, sourceExpression, interpretedExpression, value)) {
        appendUniqueLine(QStringLiteral("= ") + DisplayFormatUtils::applyDigitGroupingForDisplay(
            NumberFormatter::formatTrigSymbolic(value)));
    }

    return lines;
}

} // namespace ResultLineFormatUtils

#endif
