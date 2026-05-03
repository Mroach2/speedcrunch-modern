// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_UNICODECHARS_H
#define CORE_UNICODECHARS_H

#include "core/regexpatterns.h"

#include <QChar>
#include <QString>

namespace UnicodeChars {

inline constexpr QChar Space(0x0020); // ' '
inline constexpr QChar DigitZero(0x0030); // '0'
inline constexpr QChar DigitOne(0x0031); // '1'
inline constexpr QChar DigitTwo(0x0032); // '2'
inline constexpr QChar DigitThree(0x0033); // '3'
inline constexpr QChar DigitFour(0x0034); // '4'
inline constexpr QChar DigitFive(0x0035); // '5'
inline constexpr QChar DigitSix(0x0036); // '6'
inline constexpr QChar DigitSeven(0x0037); // '7'
inline constexpr QChar DigitEight(0x0038); // '8'
inline constexpr QChar DigitNine(0x0039); // '9'
inline constexpr QChar LeftToRightMark(0x200E); // LRM
inline constexpr QChar NoBreakSpace(0x00A0); // ' '
inline constexpr QChar MediumMathematicalSpace(0x205F); // ' '
inline constexpr QChar NarrowNoBreakSpace(0x202F); // ' '
inline constexpr QChar LeftParenthesis(0x0028); // '('
inline constexpr QChar RightParenthesis(0x0029); // ')'
inline constexpr QChar LeftSquareBracket(0x005B); // '['
inline constexpr QChar RightSquareBracket(0x005D); // ']'
inline constexpr QChar LeftCurlyBracket(0x007B); // '{'
inline constexpr QChar RightCurlyBracket(0x007D); // '}'
inline constexpr QChar PlusSign(0x002B); // '+'
inline constexpr QChar FullwidthPlusSign(0xFF0B); // '＋'
inline constexpr QChar Colon(0x003A); // ':'
inline constexpr QChar Semicolon(0x003B); // ';'
inline constexpr QChar EqualsSign(0x003D); // '='
inline constexpr QChar LessThanSign(0x003C); // '<'
inline constexpr QChar GreaterThanSign(0x003E); // '>'
inline constexpr QChar Ampersand(0x0026); // '&'
inline constexpr QChar VerticalLine(0x007C); // '|'
inline constexpr QChar Comma(0x002C); // ','
inline constexpr QChar FullStop(0x002E); // '.'
inline constexpr QChar Tilde(0x007E); // '~'
inline constexpr QChar Apostrophe(0x0027); // '''
inline constexpr QChar NumberSign(0x0023); // '#'
inline constexpr QChar DollarSign(0x0024); // '$'
inline constexpr QChar ExclamationMark(0x0021); // '!'
inline constexpr QChar PercentSign(0x0025); // '%'
inline constexpr QChar QuestionMark(0x003F); // '?'
inline constexpr QChar CircumflexAccent(0x005E); // '^'
inline constexpr QChar ModifierLetterCircumflexAccent(0x02C6); // 'ˆ'
inline constexpr QChar Caret(0x2038); // '‸'
inline constexpr QChar FullwidthCircumflexAccent(0xFF3E); // '＾'
inline constexpr QChar Solidus(0x002F); // '/'
inline constexpr QChar ReverseSolidus(0x005C); // '\'
inline constexpr QChar HyphenMinus(0x002D); // '-'
inline constexpr QChar Hyphen(0x2010); // '‐'
inline constexpr QChar NonBreakingHyphen(0x2011); // '‑'
inline constexpr QChar EnDash(0x2013); // '–'
inline constexpr QChar EmDash(0x2014); // '—'
inline constexpr QChar HorizontalBar(0x2015); // '―'
inline constexpr QChar HyphenBullet(0x2043); // '⁃'
inline constexpr QChar SmallHyphenMinus(0xFE63); // '﹣'
inline constexpr QChar FullwidthHyphenMinus(0xFF0D); // '－'
inline constexpr QChar MultiplicationSign(0x00D7); // '×'
inline constexpr QChar MinusSign(0x2212); // '−'
inline constexpr QChar DotOperator(0x22C5); // '⋅'
inline constexpr QChar Asterisk(0x002A); // '*'
inline constexpr QChar LowLine(0x005F); // '_'
inline constexpr QChar QuotationMark(0x0022); // '"'
inline constexpr QChar LatinSmallLetterI(0x0069); // 'i'
inline constexpr QChar LatinSmallLetterP(0x0070); // 'p'
inline constexpr QChar MiddleDot(0x00B7); // '·'
inline constexpr QChar AsteriskOperator(0x2217); // '∗'
inline constexpr QChar BulletOperator(0x2219); // '∙'
inline constexpr QChar MultiplicationX(0x2715); // '✕'
inline constexpr QChar HeavyMultiplicationX(0x2716); // '✖'
inline constexpr QChar NAryTimesOperator(0x2A09); // '⨉'
inline constexpr QChar VectorOrCrossProduct(0x2A2F); // '⨯'
inline constexpr QChar GreekCapitalOmega(0x03A9); // 'Ω'
inline constexpr QChar OhmSign(0x2126); // 'Ω'
inline constexpr QChar MicroSign(0x00B5); // 'µ'
inline constexpr QChar GreekSmallLetterMu(0x03BC); // 'μ'
inline constexpr QChar Pi(0x03C0); // 'π'
inline constexpr QChar Summation(0x2211); // '∑'
inline constexpr QChar SquareRoot(0x221A); // '√'
inline constexpr QChar CubeRoot(0x221B); // '∛'
inline constexpr QChar RightwardsArrow(0x2192); // '→'
inline constexpr QChar Prime(0x2032); // '′'
inline constexpr QChar DoublePrime(0x2033); // '″'
inline constexpr QChar DegreeSign(0x00B0); // '°'
inline constexpr QChar FeminineOrdinalIndicator(0x00AA); // 'ª'
inline constexpr QChar MasculineOrdinalIndicator(0x00BA); // 'º'
inline constexpr QChar RingAbove(0x02DA); // '˚'
inline constexpr QChar RingOperator(0x2218); // '∘'
inline constexpr QChar DivisionSign(0x00F7); // '÷'
inline constexpr QChar DivisionSlash(0x2215); // '∕'
inline constexpr QChar BigSolidus(0x29F8); // '⧸'
inline constexpr QChar SuperscriptPlus(0x207A); // '⁺'
inline constexpr QChar SuperscriptMinus(0x207B); // '⁻'
inline constexpr QChar SuperscriptZero(0x2070); // '⁰'
inline constexpr QChar SuperscriptOne(0x00B9); // '¹'
inline constexpr QChar SuperscriptTwo(0x00B2); // '²'
inline constexpr QChar SuperscriptThree(0x00B3); // '³'
inline constexpr QChar SuperscriptFour(0x2074); // '⁴'
inline constexpr QChar SuperscriptFive(0x2075); // '⁵'
inline constexpr QChar SuperscriptSix(0x2076); // '⁶'
inline constexpr QChar SuperscriptSeven(0x2077); // '⁷'
inline constexpr QChar SuperscriptEight(0x2078); // '⁸'
inline constexpr QChar SuperscriptNine(0x2079); // '⁹'
inline constexpr QChar SubscriptZero(0x2080); // '₀'
inline constexpr QChar SubscriptOne(0x2081); // '₁'
inline constexpr QChar SubscriptTwo(0x2082); // '₂'
inline constexpr QChar SubscriptThree(0x2083); // '₃'
inline constexpr QChar SubscriptFour(0x2084); // '₄'
inline constexpr QChar SubscriptFive(0x2085); // '₅'
inline constexpr QChar SubscriptSix(0x2086); // '₆'
inline constexpr QChar SubscriptSeven(0x2087); // '₇'
inline constexpr QChar SubscriptEight(0x2088); // '₈'
inline constexpr QChar SubscriptNine(0x2089); // '₉'
inline constexpr QChar LatinSubscriptSmallLetterA(0x2090); // 'ₐ'
inline constexpr QChar LatinSubscriptSmallLetterE(0x2091); // 'ₑ'
inline constexpr QChar LatinSubscriptSmallLetterO(0x2092); // 'ₒ'
inline constexpr QChar LatinSubscriptSmallLetterX(0x2093); // 'ₓ'
inline constexpr QChar LatinSubscriptSmallLetterSchwa(0x2094); // 'ₔ'
inline constexpr QChar LatinSubscriptSmallLetterH(0x2095); // 'ₕ'
inline constexpr QChar LatinSubscriptSmallLetterK(0x2096); // 'ₖ'
inline constexpr QChar LatinSubscriptSmallLetterL(0x2097); // 'ₗ'
inline constexpr QChar LatinSubscriptSmallLetterM(0x2098); // 'ₘ'
inline constexpr QChar LatinSubscriptSmallLetterN(0x2099); // 'ₙ'
inline constexpr QChar LatinSubscriptSmallLetterP(0x209A); // 'ₚ'
inline constexpr QChar LatinSubscriptSmallLetterS(0x209B); // 'ₛ'
inline constexpr QChar LatinSubscriptSmallLetterT(0x209C); // 'ₜ'
inline constexpr QChar LatinSubscriptSmallLetterI(0x1D62); // 'ᵢ'
inline constexpr QChar LatinSubscriptSmallLetterR(0x1D63); // 'ᵣ'
inline constexpr QChar LatinSubscriptSmallLetterU(0x1D64); // 'ᵤ'
inline constexpr QChar LatinSubscriptSmallLetterV(0x1D65); // 'ᵥ'
inline constexpr QChar GreekSubscriptSmallLetterBeta(0x1D66); // 'ᵦ'
inline constexpr QChar GreekSubscriptSmallLetterGamma(0x1D67); // 'ᵧ'
inline constexpr QChar GreekSubscriptSmallLetterRho(0x1D68); // 'ᵨ'
inline constexpr QChar GreekSubscriptSmallLetterPhi(0x1D69); // 'ᵩ'
inline constexpr QChar GreekSubscriptSmallLetterChi(0x1D6A); // 'ᵪ'
// These pi variants are non-BMP Unicode code points (> U+FFFF), so they
// cannot be represented as a single QChar (UTF-16 code unit). We keep them
// as char32_t and feed them to QString::fromUcs4 when normalizing aliases.
inline constexpr char32_t MathematicalBoldSmallPiCodePoint = 0x1D6D1;
inline constexpr char32_t MathematicalItalicSmallPiCodePoint = 0x1D70B;
inline constexpr char32_t MathematicalSansSerifBoldSmallPiCodePoint = 0x1D745;
inline constexpr char32_t MathematicalSansSerifBoldItalicSmallPiCodePoint = 0x1D7B9;

// Returns true if `ch` is a superscript decimal digit (0-9).
// Example input/output: '²' -> true, '2' -> false.
inline bool isSuperscriptDigit(QChar ch)
{
    return ch == SuperscriptOne || ch == SuperscriptTwo || ch == SuperscriptThree
        || (ch.unicode() >= SuperscriptZero.unicode() && ch.unicode() <= SuperscriptNine.unicode());
}

// Returns true if `ch` is a superscript sign used in exponents.
// Example input/output: '⁻' -> true, '-' -> false.
inline bool isSuperscriptSign(QChar ch)
{
    return ch == SuperscriptMinus || ch == SuperscriptPlus;
}

// Returns true when `ch` is accepted inside a simple unit identifier.
// Example input/output: 'Ω' -> true, '/' -> false.
inline bool isUnitIdentifierChar(QChar ch)
{
    return ch.isLetterOrNumber() || ch == LowLine
        || ch == MicroSign
        || ch == GreekCapitalOmega
        || ch == DegreeSign
        || ch == MasculineOrdinalIndicator
        || ch == RingAbove;
}

// Replaces mathematical styled pi symbols with plain 'π'.
// Example input/output: "𝛑 + π" -> "π + π".
inline QString normalizePiAliasesToPi(QString text)
{
    const QString piSymbol(Pi);
    text.replace(QString::fromUcs4(&MathematicalBoldSmallPiCodePoint, 1), piSymbol);
    text.replace(QString::fromUcs4(&MathematicalItalicSmallPiCodePoint, 1), piSymbol);
    text.replace(QString::fromUcs4(&MathematicalSansSerifBoldSmallPiCodePoint, 1), piSymbol);
    text.replace(QString::fromUcs4(&MathematicalSansSerifBoldItalicSmallPiCodePoint, 1), piSymbol);
    return text;
}

// Normalizes unit-symbol aliases to canonical forms.
// Example input/output:
// "Ω(U+2126) μ(U+03BC) º(U+00BA) ˚(U+02DA)" -> "Ω(U+03A9) µ(U+00B5) °(U+00B0) °(U+00B0)".
inline QString normalizeUnitSymbolAliases(QString text)
{
    text.replace(OhmSign, GreekCapitalOmega);
    text.replace(GreekSmallLetterMu, MicroSign);
    text.replace(MasculineOrdinalIndicator, DegreeSign);
    text.replace(RingAbove, DegreeSign);
    return text;
}

// Normalizes ASCII sexagesimal marks to Unicode prime symbols.
// Example input/output: "12'34\"" -> "12′34″".
inline QString normalizeSexagesimalSymbolAliases(QString text)
{
    text.replace(Apostrophe, Prime);
    text.replace(QuotationMark, DoublePrime);
    return text;
}

// Normalizes pi text for display while preserving identifiers (e.g. "pixel").
// Example input/output: "2*pi + 𝛑 + pixel" -> "2*π + π + pixel".
inline QString normalizePiForDisplay(QString text)
{
    text = normalizePiAliasesToPi(text);

    auto isIdentifierChar = [](QChar ch) {
        return ch.isLetter() || ch.isDigit() || ch == LowLine;
    };

    for (int i = 0; i + 1 < text.size();) {
        if (text.at(i) == LatinSmallLetterP && text.at(i + 1) == LatinSmallLetterI) {
            const bool hasLeft = i > 0;
            const bool hasRight = i + 2 < text.size();
            const bool leftIsIdentifier = hasLeft && isIdentifierChar(text.at(i - 1));
            const bool rightIsIdentifier = hasRight && isIdentifierChar(text.at(i + 2));
            if (!leftIsIdentifier && !rightIsIdentifier) {
                text.replace(i, 2, QString(Pi));
                ++i;
                continue;
            }
        }
        ++i;
    }

    return text;
}

// Rewrites root/summation function words to display symbols.
// Example input/output: "sqrt(x)+cbrt(y)+summation(k)" -> "√(x)+∛(y)+∑(k)".
inline QString normalizeRootFunctionAliasesForDisplay(QString text)
{
    text.replace(RegExpPatterns::summationWord(), QString(Summation));
    text.replace(RegExpPatterns::sqrtWord(), QString(SquareRoot));
    text.replace(RegExpPatterns::cbrtWord(), QString(CubeRoot));
    return text;
}

} // namespace UnicodeChars

#endif
