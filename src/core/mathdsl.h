// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_MATHDSL_H
#define CORE_MATHDSL_H

#include "core/unicodechars.h"

#include <QChar>
#include <QString>

namespace MathDsl {

// Display/operator symbol constants consumed by DisplayFormatUtils.
// Spacing behavior is defined in gui/displayformatutils.h.
inline constexpr QChar MulDotOp = UnicodeChars::MiddleDot;
inline constexpr QChar MulCrossOp = UnicodeChars::MultiplicationSign;
inline constexpr QChar MulOpAl1 = UnicodeChars::Asterisk;
inline constexpr QChar MulOpAl2 = UnicodeChars::AsteriskOperator;
inline constexpr QChar MulOpAl3 = UnicodeChars::BulletOperator;
inline constexpr QChar MulOpAl4 = UnicodeChars::MultiplicationX;
inline constexpr QChar MulOpAl5 = UnicodeChars::HeavyMultiplicationX;
inline constexpr QChar MulOpAl6 = UnicodeChars::NAryTimesOperator;
inline constexpr QChar MulOpAl7 = UnicodeChars::VectorOrCrossProduct;
inline constexpr QChar DivOp = UnicodeChars::Solidus;
inline constexpr QChar TransOp = UnicodeChars::RightwardsArrow;
inline constexpr QChar PowOp = UnicodeChars::CircumflexAccent;
inline constexpr QChar TimeSep = UnicodeChars::Colon;
inline constexpr QChar FunArgSep = UnicodeChars::Semicolon;
inline constexpr QChar AddOp = UnicodeChars::PlusSign;
inline constexpr QChar Equals = UnicodeChars::EqualsSign;
inline constexpr QChar SubOp = UnicodeChars::MinusSign;
inline constexpr QChar SubOpAl1 = UnicodeChars::HyphenMinus;
inline constexpr QChar PercentOp = UnicodeChars::PercentSign;
inline constexpr QChar FactorOp = UnicodeChars::ExclamationMark;
inline constexpr QChar CommentSep = UnicodeChars::QuestionMark;
inline constexpr QChar DollarSign = UnicodeChars::DollarSign;
inline constexpr QChar BitAndOp = UnicodeChars::Ampersand;
inline constexpr QChar BitOrOp = UnicodeChars::VerticalLine;
inline constexpr QChar LessThanOp = UnicodeChars::LessThanSign;
inline constexpr QChar GreaterThanOp = UnicodeChars::GreaterThanSign;
inline constexpr QChar Deg = UnicodeChars::DegreeSign;
inline constexpr QChar CommaSep = UnicodeChars::Comma;
inline constexpr QChar DotSep = UnicodeChars::FullStop;
inline constexpr QChar ArcminOp = UnicodeChars::Prime;
inline constexpr QChar ArcminOpAl1 = UnicodeChars::Apostrophe;
inline constexpr QChar ArcsecOp = UnicodeChars::DoublePrime;
inline constexpr QChar ArcsecOpAl1 = UnicodeChars::QuotationMark;
inline constexpr QChar IntDivOp = UnicodeChars::ReverseSolidus;
inline constexpr QChar SubWrapSp = UnicodeChars::Space;
inline constexpr QChar AddWrap = UnicodeChars::Space;
inline constexpr QChar DivWrap = UnicodeChars::Space;
inline constexpr QChar MulDotWrapSp = UnicodeChars::Space;
inline constexpr QChar MulCrossWrapSp = UnicodeChars::Space;
inline constexpr QChar QuantSp = UnicodeChars::NoBreakSpace;
inline constexpr QChar Dig0 = UnicodeChars::DigitZero;
inline constexpr QChar Dig1 = UnicodeChars::DigitOne;
inline constexpr QChar Dig2 = UnicodeChars::DigitTwo;
inline constexpr QChar Dig3 = UnicodeChars::DigitThree;
inline constexpr QChar Dig4 = UnicodeChars::DigitFour;
inline constexpr QChar Dig5 = UnicodeChars::DigitFive;
inline constexpr QChar Dig6 = UnicodeChars::DigitSix;
inline constexpr QChar Dig7 = UnicodeChars::DigitSeven;
inline constexpr QChar Dig8 = UnicodeChars::DigitEight;
inline constexpr QChar Dig9 = UnicodeChars::DigitNine;
inline constexpr QChar PowPos = UnicodeChars::SuperscriptPlus;
inline constexpr QChar PowNeg = UnicodeChars::SuperscriptMinus;
inline constexpr QChar Pow0 = UnicodeChars::SuperscriptZero;
inline constexpr QChar Pow1 = UnicodeChars::SuperscriptOne;
inline constexpr QChar Pow2 = UnicodeChars::SuperscriptTwo;
inline constexpr QChar Pow3 = UnicodeChars::SuperscriptThree;
inline constexpr QChar Pow4 = UnicodeChars::SuperscriptFour;
inline constexpr QChar Pow5 = UnicodeChars::SuperscriptFive;
inline constexpr QChar Pow6 = UnicodeChars::SuperscriptSix;
inline constexpr QChar Pow7 = UnicodeChars::SuperscriptSeven;
inline constexpr QChar Pow8 = UnicodeChars::SuperscriptEight;
inline constexpr QChar Pow9 = UnicodeChars::SuperscriptNine;
inline constexpr QChar GroupStart = UnicodeChars::LeftParenthesis;
inline constexpr QChar GroupEnd = UnicodeChars::RightParenthesis;
inline constexpr QChar UnitStart = UnicodeChars::LeftSquareBracket;
inline constexpr QChar UnitEnd = UnicodeChars::RightSquareBracket;
inline constexpr QChar ListStart = UnicodeChars::LeftCurlyBracket;
inline constexpr QChar ListEnd = UnicodeChars::RightCurlyBracket;
inline const QString HexPrefix = QStringLiteral("0x");
inline const QString BinPrefix = QStringLiteral("0b");
inline const QString OctPrefix = QStringLiteral("0o");
inline const QString ShiftLeftOp = QString(LessThanOp) + QString(LessThanOp);
inline const QString ShiftRightOp = QString(GreaterThanOp) + QString(GreaterThanOp);
inline constexpr QChar HexPrefixAl1 = UnicodeChars::NumberSign;
inline const QString TransOpAl1 = QStringLiteral("in");
inline constexpr QChar Powers[] = { Pow0, Pow1, Pow2, Pow3, Pow4, Pow5, Pow6, Pow7, Pow8, Pow9 };
inline constexpr QChar Digits[] = { Dig0, Dig1, Dig2, Dig3, Dig4, Dig5, Dig6, Dig7, Dig8, Dig9 };

inline bool isAdditionOperator(const QChar& ch)
{
    return ch == AddOp;
}

inline bool isSubtractionOperator(const QChar& ch)
{
    return ch == SubOp || ch == SubOpAl1;
}

inline bool isMultiplicationOperator(const QChar& ch)
{
    return ch == MulDotOp || ch == MulCrossOp;
}

inline bool isDivisionOperator(const QChar& ch)
{
    return ch == DivOp;
}

bool isExpressionBoundaryOperatorOrSeparator(const QChar& ch);
bool isExpressionOperatorOrSeparator(const QChar& ch);
bool isMultiplicationOperatorAlias(const QChar& ch, bool keepDotOperator = false);
bool isDivisionOperatorAlias(const QChar& ch);
bool isAdditionOperatorAlias(const QChar& ch);
bool isSubtractionOperatorAlias(const QChar& ch);
QString buildWrappedToken(const QChar& op, const QChar& wrap = UnicodeChars::Space);
QString buildWrappedToken(const QChar& op, const QChar& leftWrap, const QChar& rightWrap);
QString buildWrappedToken(const QString& op, const QChar& wrap = UnicodeChars::Space);
QString buildWrappedToken(const QString& op, const QChar& leftWrap, const QChar& rightWrap);

inline bool isSuperscriptDigit(const QChar& ch)
{
    for (int i = 0; i < 10; ++i) {
        if (ch == Powers[i])
            return true;
    }
    return false;
}

inline bool isSuperscriptSign(const QChar& ch)
{
    return ch == PowNeg || ch == PowPos;
}

inline bool isSuperscriptPowerChar(const QChar& ch)
{
    return isSuperscriptDigit(ch) || isSuperscriptSign(ch);
}

inline QChar superscriptDigitToAscii(const QChar& ch)
{
    for (int i = 0; i < 10; ++i) {
        if (ch == Powers[i])
            return Digits[i];
    }
    return QChar();
}

inline QChar asciiDigitToSuperscript(const QChar& ch)
{
    const ushort code = ch.unicode();
    if (code < Dig0.unicode() || code > Dig9.unicode())
        return QChar();
    return Powers[code - Dig0.unicode()];
}

} // namespace MathDsl

#endif // CORE_MATHDSL_H
