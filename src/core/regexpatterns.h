// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_REGEXPATTERNS_H
#define CORE_REGEXPATTERNS_H

#include <QRegularExpression>
#include <QStringView>

namespace RegExpPatterns {

// Matches one standalone numeric token (decimal, hex, octal, or binary).
// Example input/output: "x = 0xFF" -> match "0xFF"; "abc123def" -> no standalone match.
inline const QRegularExpression& numericToken()
{
    static const QRegularExpression pattern(
        QStringLiteral("(?<![\\p{L}\\p{N}])(?:0[xX][0-9A-Fa-f]+(?:[\\.,][0-9A-Fa-f]+)?|0[oO][0-7]+(?:[\\.,][0-7]+)?|0[bB][01]+(?:[\\.,][01]+)?|\\d+(?:[\\.,]\\d+)?(?:[eE][+\\-−]?\\d+)?)(?![\\p{L}\\p{N}])"));
    return pattern;
}

// Matches a full string that is a signed numeric literal with optional spaces.
// Example input/output: "  - 1.23e4  " -> match; "1 + 2" -> no match.
inline const QRegularExpression& signedNumericLiteralWithSpaces()
{
    static const QRegularExpression pattern(
        QStringLiteral("^\\s*[+\\-−]?\\s*(?:0[xX][0-9A-Fa-f]+(?:[\\.,][0-9A-Fa-f]+)?|0[oO][0-7]+(?:[\\.,][0-7]+)?|0[bB][01]+(?:[\\.,][01]+)?|\\d+(?:[\\.,]\\d+)?(?:[eE][+\\-−]?\\d+)?)\\s*$"));
    return pattern;
}

// Matches an exponent suffix at the end of a decimal literal.
// Example input/output: "1.2e-3" -> match "e-3"; "1.2e" -> no match.
inline const QRegularExpression& decimalExponentSuffix()
{
    static const QRegularExpression pattern(QStringLiteral("[eE][+\\-−]?\\d+$"));
    return pattern;
}

// Matches radix separators accepted by parser helpers ('.' or ',').
// Example input/output: "12,34" -> match ","; "1234" -> no match.
inline const QRegularExpression& radixSeparator()
{
    static const QRegularExpression pattern(QStringLiteral("[\\.,]"));
    return pattern;
}

// Matches any Unicode line-break character handled by the editor/parser.
// Example input/output: "a\nb" -> match "\n"; "ab" -> no match.
inline const QRegularExpression& lineBreak()
{
    static const QRegularExpression pattern(
        QStringLiteral("[\\n\\r\\x{0085}\\x{2028}\\x{2029}]")
    );
    return pattern;
}

// Matches a token separator (characters not allowed in expression tokens).
// Example input/output: "a@b" -> match "@"; "a+b" -> no match.
inline const QRegularExpression& separatorToken()
{
    static const QRegularExpression pattern(
        // Match everything that is not alphanumeric or an expression operator.
        QStringLiteral(R"([^a-zA-Z0-9\+\-\−\*\×·÷\/⧸\^;\(\)\[\]%!=\\&\|<>\?#→\x0000])")
    );
    return pattern;
}

// Matches a trigonometric function call start, case-insensitive.
// Example input/output: "Sin (x)" -> match "Sin ("; "sinh(x)" -> no match.
inline const QRegularExpression& trigFunctionCall()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(\b(?:sin|cos|tan|cot|sec|csc|arcsin|arccos|arctan|arctan2|radians|degrees|gradians)\s*\()"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// Matches a generic function-call start and captures function identifier.
// Example input/output: "foo(x)" -> captures "foo"; "1+2" -> no match.
inline const QRegularExpression& anyFunctionCall()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(\b([\p{L}_][\p{L}\p{N}_]*)\s*\()"));
    return pattern;
}

// Returns true when identifier text names a trigonometric function.
// Example input/output: "tan" -> true; "round" -> false.
inline bool isTrigFunctionIdentifier(QStringView identifier)
{
    return identifier.compare(QStringLiteral("sin"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("cos"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("tan"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("cot"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("sec"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("csc"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("arcsin"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("arccos"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("arctan"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("arctan2"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("degrees"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("radians"), Qt::CaseInsensitive) == 0
        || identifier.compare(QStringLiteral("gradians"), Qt::CaseInsensitive) == 0;
}

// Matches a full expression that is one plain function call.
// Example input/output: " f(x+1) " -> match; "f(x)+g(x)" -> no match.
inline const QRegularExpression& trivialSingleFunctionCall()
{
    static const QRegularExpression pattern(
        // Keep this intentionally strict (ASCII identifier only) so we don't
        // hide meaningful simplifications like "f²(x)".
        QStringLiteral("^\\s*[+\\-−]?\\s*[A-Za-z_][A-Za-z0-9_]*\\s*\\(.*\\)\\s*$"));
    return pattern;
}

// Matches a full unsigned decimal integer.
// Example input/output: "12345" -> match; "-123" -> no match.
inline const QRegularExpression& unsignedDecimalInteger()
{
    static const QRegularExpression pattern(QStringLiteral(R"(^\d+$)"));
    return pattern;
}

// Matches a full unsigned decimal number (integer or fixed-point).
// Example input/output: "12.34" -> match; "+12.34" -> no match.
inline const QRegularExpression& unsignedDecimalNumber()
{
    static const QRegularExpression pattern(QStringLiteral(R"(^\d+(?:\.\d+)?$)"));
    return pattern;
}

// Matches a full signed decimal number.
// Example input/output: "-12.34" -> match; "12e3" -> no match.
inline const QRegularExpression& signedDecimalNumber()
{
    static const QRegularExpression pattern(QStringLiteral(R"(^[+\-]?\d+(?:\.\d+)?$)"));
    return pattern;
}

// Matches decimals equivalent to integers, capturing whole and fractional parts.
// Example input/output: "42.000" -> match groups ("42","000"); "42.01" -> match but not integer-equivalent by itself.
inline const QRegularExpression& unsignedIntegerEquivalentDecimal()
{
    static const QRegularExpression pattern(QStringLiteral(R"(^(\d+)\.(\d*)$)"));
    return pattern;
}

// Matches a simple unit identifier used by unit-token parsing.
// Example input/output: "m2" -> match; "m/s" -> no match.
inline const QRegularExpression& simpleUnitIdentifier()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[A-Za-z_µμΩ°º˚][A-Za-z0-9_µμΩ°º˚]*$"));
    return pattern;
}

// Matches a simple quotient whose denominator is a parenthesized expression.
// Example input/output: "2/(m s^-1)" -> match; "2/m/s" -> no match.
inline const QRegularExpression& simpleParenthesizedDenominatorQuotient()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^\s*[+\-−]?\d+(?:\.\d+)?\s*/\s*\([^()]+\)\s*$)"));
    return pattern;
}

// Matches one bracketed unit block like "[kg]" and captures inner text.
// Example input/output: "1[kg]" -> match "[kg]" with group "kg".
inline const QRegularExpression& unitBrackets()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(\[([^\[\]]+)\])"));
    return pattern;
}

// Matches one bracketed simple unit identifier and captures the identifier.
// Example input/output: "[rev]" -> match with group "rev"; "[kg·m]" -> no match.
inline const QRegularExpression& bracketedSimpleUnitIdentifier()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(\[\s*([A-Za-z_µμΩ°º˚][A-Za-z0-9_µμΩ°º˚]*)\s*\])"));
    return pattern;
}

// Matches bracketed compact-angle unit tokens and captures the inner token.
// Example input/output: "1 [arcmin]" -> captures "arcmin"; "1 [′]" -> captures "′".
inline const QRegularExpression& compactAngleTokenInBrackets()
{
    static const QRegularExpression pattern(
        QStringLiteral("\\[\\s*([\\x{00B0}\\x{00BA}\\x{02DA}\\x{2032}\\x{2033}]|deg(?:ree)?|arcmin(?:ute)?|arcsec(?:ond)?)\\s*\\]"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// Matches a missing quantity-space before unit bracket.
// Example input/output: "1[kg]" -> match "1[" with group "1".
inline const QRegularExpression& missingQuantSpBeforeUnit()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"((\S)\[)"));
    return pattern;
}

// Matches value×[unit] style text to normalize as value [unit] in display.
// Example input/output: "750 · [h]" -> groups ("0","[h]"); "a · [h]" -> no match.
inline const QRegularExpression& valueTimesBracketedUnit()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"((\d)\s*[*·×]\s*(\[[^\[\]]+\]))"));
    return pattern;
}

// Matches text ending in power-of-ten scientific notation:
// "*10^n", "× 10^n", or "× 10ⁿ".
// Example input/output: "1.2 × 10⁻³" -> match; "1.2e-3" -> no match.
inline const QRegularExpression& trailingPowerOfTenScientificNotation()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(.*(?:\*|×)\s*10(?:\^[−-]?\d+|[⁰¹²³⁴⁵⁶⁷⁸⁹⁻⁺]+)$)"));
    return pattern;
}

// Matches the standalone word "summation", case-insensitive.
// Example input/output: "summation(k)" -> match; "presummation" -> no match.
inline const QRegularExpression& summationWord()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"((?<![\p{L}\p{N}_$])summation(?![\p{L}\p{N}_$]))"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// Matches the standalone word "sqrt", case-insensitive.
// Example input/output: "sqrt(x)" -> match; "asqrtb" -> no match.
inline const QRegularExpression& sqrtWord()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"((?<![\p{L}\p{N}_$])sqrt(?![\p{L}\p{N}_$]))"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// Matches the standalone word "cbrt", case-insensitive.
// Example input/output: "cbrt(x)" -> match; "mycbrtfn" -> no match.
inline const QRegularExpression& cbrtWord()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"((?<![\p{L}\p{N}_$])cbrt(?![\p{L}\p{N}_$]))"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// Matches characters allowed in a standalone sexagesimal angle literal.
// Example input/output: "1°2′3″" -> full match; "cos(1°)" -> no full match.
inline const QRegularExpression& standaloneSexagesimalAngleLiteralAllowed()
{
    static const QRegularExpression pattern(
        QStringLiteral("^[\\s\\p{Zs}+\\-−0-9\\.,°º˚'′\"″]+$"));
    return pattern;
}

// Matches a decimal digit anywhere in the input.
// Example input/output: "°′″" -> no match; "1°" -> match.
inline const QRegularExpression& anyDigit()
{
    static const QRegularExpression pattern(QStringLiteral("\\d"));
    return pattern;
}

} // namespace RegExpPatterns

#endif
