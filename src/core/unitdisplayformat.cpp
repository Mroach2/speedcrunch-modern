// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/unitdisplayformat.h"

#include "core/mathdsl.h"
#include "core/unicodechars.h"
#include "core/units.h"
#include <QHash>
#include <QVector>

namespace UnitDisplayFormat {

namespace {

// Returns the index where a trailing superscript exponent suffix starts.
// Supports ¹²³ and U+2070..U+2079 digits plus superscript plus/minus.
int superscriptSuffixStart(const QString& text)
{
    int suffixStart = text.size();
    while (suffixStart > 0) {
        const QChar ch = text.at(suffixStart - 1);
        if (!UnicodeChars::isSuperscriptDigit(ch) && !UnicodeChars::isSuperscriptSign(ch))
            break;
        --suffixStart;
    }
    return suffixStart;
}

// Builds an identifier->symbol cache from unit metadata, used to collapse
// long names (and aliases) to short display symbols.
const QHash<QString, QString>& shortNamesByIdentifier()
{
    static const QHash<QString, QString> shortNames = [] {
        QHash<QString, QString> map;
        const auto& values = Units::builtInUnitValues();
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        const QString& identifier = it.key();
        UnitId id = unitId(normalizeUnitName(identifier));
        if (id == UnitId::Unknown)
            continue;
            const QString symbol = unitSymbol(id);
            if (!symbol.isEmpty())
                map.insert(identifier, symbol);
        }
        return map;
    }();
    return shortNames;
}

QString compositeAngleAlias(UnitId id)
{
    if (id == UnitId::Degree)
        return Units::degreeAliasSymbol();
    if (id == UnitId::Arcminute)
        return Units::arcminuteAliasSymbol();
    if (id == UnitId::Arcsecond)
        return Units::arcsecondAliasSymbol();
    return QString();
}

UnitId unitIdForDisplayTokenBase(const QString& tokenBase)
{
    const UnitId id = unitId(normalizeUnitName(tokenBase));
    if (id != UnitId::Unknown)
        return id;

    if (tokenBase == unitSymbol(UnitId::Degree))
        return UnitId::Degree;
    if (tokenBase == unitSymbol(UnitId::Arcminute))
        return UnitId::Arcminute;
    if (tokenBase == unitSymbol(UnitId::Arcsecond))
        return UnitId::Arcsecond;
    return UnitId::Unknown;
}

bool isDisplayUnitTokenChar(QChar ch)
{
    return UnicodeChars::isUnitIdentifierChar(ch)
        || ch == UnicodeChars::Prime
        || ch == UnicodeChars::DoublePrime
        || UnicodeChars::isSuperscriptDigit(ch)
        || UnicodeChars::isSuperscriptSign(ch);
}

QString superscriptFromPositiveExponent(int exponent)
{
    if (exponent <= 1)
        return QString();

    QString superscript;
    const QString digits = QString::number(exponent);
    superscript.reserve(digits.size());
    for (const QChar& digit : digits)
        superscript.append(MathDsl::asciiDigitToSuperscript(digit));
    return superscript;
}

int parseTokenExponent(const QString& token, int* baseEnd, bool* ok)
{
    *ok = false;
    *baseEnd = token.size();
    if (token.isEmpty())
        return 0;

    const int suffixStart = superscriptSuffixStart(token);
    if (suffixStart >= token.size())
        return 1;

    bool negative = false;
    int pos = suffixStart;
    if (token.at(pos) == MathDsl::PowNeg || token.at(pos) == MathDsl::PowPos) {
        negative = token.at(pos) == MathDsl::PowNeg;
        ++pos;
    }
    if (pos >= token.size())
        return 0;

    QString asciiDigits;
    asciiDigits.reserve(token.size() - pos);
    for (; pos < token.size(); ++pos) {
        const QChar asciiDigit = MathDsl::superscriptDigitToAscii(token.at(pos));
        if (asciiDigit.isNull())
            return 0;
        asciiDigits.append(asciiDigit);
    }

    bool intOk = false;
    const int exponent = asciiDigits.toInt(&intOk);
    if (!intOk)
        return 0;

    *ok = true;
    *baseEnd = suffixStart;
    return negative ? -exponent : exponent;
}

QString simplifyCompositeUnitText(const QString& text)
{
    if (text.isEmpty()
        || text.contains(MathDsl::GroupStart)
        || text.contains(MathDsl::GroupEnd))
    {
        return text;
    }

    struct UnitFactor {
        QString base;
        int exponent = 0;
    };

    QVector<UnitFactor> factors;
    QHash<QString, int> factorIndexByBase;
    bool hasRepeatedBase = false;

    auto appendFactor = [&](const QString& token, bool inDenominator) {
        if (token.isEmpty())
            return;

        int baseEnd = token.size();
        bool hasExplicitExponent = false;
        int exponent = parseTokenExponent(token, &baseEnd, &hasExplicitExponent);
        if (!hasExplicitExponent)
            exponent = 1;

        const QString base = token.left(baseEnd);
        if (base.isEmpty())
            return;

        if (inDenominator)
            exponent = -exponent;

        if (!factorIndexByBase.contains(base)) {
            UnitFactor factor;
            factor.base = base;
            factor.exponent = exponent;
            factors.append(factor);
            factorIndexByBase.insert(base, factors.size() - 1);
            return;
        }

        hasRepeatedBase = true;
        UnitFactor& existing = factors[factorIndexByBase.value(base)];
        existing.exponent += exponent;
    };

    QString currentToken;
    bool denominatorForNextToken = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        const bool isSpaceSep = ch.isSpace();
        const bool isMul =
            ch == MathDsl::MulOpAl1
            || ch == MathDsl::MulDotOp
            || ch == MathDsl::MulCrossOp;
        const bool isDiv = ch == MathDsl::DivOp;
        if (!isSpaceSep && !isMul && !isDiv) {
            currentToken.append(ch);
            continue;
        }

        appendFactor(currentToken.trimmed(), denominatorForNextToken);
        currentToken.clear();
        // '/' only affects the immediately following factor; multiplication
        // separators start a new factor in numerator context.
        denominatorForNextToken = isDiv;
    }
    appendFactor(currentToken.trimmed(), denominatorForNextToken);
    if (!hasRepeatedBase)
        return text;

    QStringList numerators;
    QStringList denominators;
    numerators.reserve(factors.size());
    denominators.reserve(factors.size());
    for (const UnitFactor& factor : factors) {
        if (factor.exponent == 0)
            continue;

        if (factor.exponent > 0) {
            numerators.append(factor.base + superscriptFromPositiveExponent(factor.exponent));
        } else {
            denominators.append(factor.base + superscriptFromPositiveExponent(-factor.exponent));
        }
    }

    if (numerators.isEmpty() && denominators.isEmpty())
        return QStringLiteral("1");

    const QString mulOp = QString(MathDsl::MulDotOp);
    if (denominators.isEmpty())
        return numerators.join(mulOp);

    QString result = numerators.isEmpty() ? QStringLiteral("1") : numerators.join(mulOp);
    if (denominators.size() == 1) {
        result += QString(MathDsl::DivOp) + denominators.first();
    } else {
        result += QString(MathDsl::DivOp)
            + QString(MathDsl::GroupStart)
            + denominators.join(mulOp)
            + QString(MathDsl::GroupEnd);
    }
    return result;
}

} // namespace

// Collapses a unit token to a compact display form:
// canonical symbol when known, preserving superscript exponent suffixes.
// Examples:
// - "metre" -> "m"
// - "kilometre" -> "km"
// - "quebibyte" -> "QiB"
// - "metre⁻²" -> "m⁻²"
QString shortDisplayName(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return trimmed;

    const int suffixStart = superscriptSuffixStart(trimmed);
    const QString baseName = trimmed.left(suffixStart);
    const QString exponentSuffix = trimmed.mid(suffixStart);

    const auto& shortNames = shortNamesByIdentifier();
    if (shortNames.contains(baseName))
        return shortNames.value(baseName) + exponentSuffix;

    for (const PrefixId id : siPrefixIds()) {
        const QString prefixLong = prefixName(id);
        if (!baseName.startsWith(prefixLong))
            continue;
        const QString base = baseName.mid(prefixLong.size());
        if (shortNames.contains(base))
            return prefixSymbol(id) + shortNames.value(base) + exponentSuffix;
    }

    for (const PrefixId id : binaryPrefixIds()) {
        const QString prefixLong = prefixName(id);
        if (!baseName.startsWith(prefixLong))
            continue;
        const QString base = baseName.mid(prefixLong.size());
        if (shortNames.contains(base))
            return prefixSymbol(id) + shortNames.value(base) + exponentSuffix;
    }

    return trimmed;
}

// Normalizes a full unit-expression string for display by tokenizing around
// separators/operators and applying token-level formatting.
// Examples:
// - "metre second⁻¹" -> "m s⁻¹"
// - "joule/(mole kelvin)" -> "J/(mol K)"
// - "kilogram·metre/second²" -> "kg·m/s²"
QString normalizeUnitTextForDisplay(const QString& text)
{
    if (text.isEmpty())
        return text;

    const bool hasCompositeUnitOperators =
        text.contains(MathDsl::MulOpAl1)
        || text.contains(MathDsl::MulDotOp)
        || text.contains(MathDsl::MulCrossOp)
        || text.contains(MathDsl::DivOp);
    QString displayInput = text;
    if (hasCompositeUnitOperators) {
        displayInput.replace(UnicodeChars::DegreeSign, Units::degreeAliasSymbol());
        displayInput.replace(UnicodeChars::Prime, Units::arcminuteAliasSymbol());
        displayInput.replace(UnicodeChars::DoublePrime, Units::arcsecondAliasSymbol());
    }

    QString normalized;
    normalized.reserve(displayInput.size());

    for (int i = 0; i < displayInput.size();) {
        const QChar ch = displayInput.at(i);
        if (!isDisplayUnitTokenChar(ch)) {
            normalized.append(ch);
            ++i;
            continue;
        }

        int j = i + 1;
        while (j < displayInput.size() && isDisplayUnitTokenChar(displayInput.at(j)))
            ++j;

        const QString token = displayInput.mid(i, j - i);
        const int tokenSuffixStart = superscriptSuffixStart(token);
        const QString tokenBase = token.left(tokenSuffixStart);
        const bool hasExponentSuffix = tokenSuffixStart < token.size();
        QString displayToken = shortDisplayName(token);
        const UnitId tokenId = unitIdForDisplayTokenBase(tokenBase);
        const QString preferredAlias = compositeAngleAlias(tokenId);
        const bool shouldPreferAlias =
            !preferredAlias.isEmpty() && (hasCompositeUnitOperators || hasExponentSuffix);
        if (shouldPreferAlias) {
            QString suffix;
            const QString tokenSymbol = unitSymbol(tokenId);
            if (!displayToken.isEmpty() && displayToken.startsWith(tokenSymbol)) {
                suffix = displayToken.mid(tokenSymbol.size());
            } else if (displayToken.startsWith(preferredAlias)) {
                suffix = displayToken.mid(preferredAlias.size());
            }
            displayToken = preferredAlias + suffix;
        }
        normalized.append(displayToken);
        i = j;
    }
    if (hasCompositeUnitOperators)
        return simplifyCompositeUnitText(normalized);
    return normalized;
}

} // namespace UnitDisplayFormat
