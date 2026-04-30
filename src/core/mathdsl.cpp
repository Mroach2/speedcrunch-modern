// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/mathdsl.h"

namespace MathDsl {

bool isMultiplicationOperatorAlias(const QChar& ch, bool keepDotOperator)
{
    switch (ch.unicode()) {
    case MathDsl::MulOpAl1.unicode():
    case MathDsl::MulOpAl2.unicode():
    case MathDsl::MulOpAl3.unicode():
    case MathDsl::MulOpAl4.unicode():
    case MathDsl::MulOpAl5.unicode():
    case MathDsl::MulOpAl6.unicode():
    case MathDsl::MulOpAl7.unicode():
        return true;
    case MulDotOp.unicode():
        return !keepDotOperator;
    default:
        return false;
    }
}

bool isDivisionOperatorAlias(const QChar& ch)
{
    switch (ch.unicode()) {
    case UnicodeChars::DivisionSign.unicode():
        return true;
    default:
        return isDivisionOperator(ch);
    }
}

bool isAdditionOperatorAlias(const QChar& ch)
{
    return ch.unicode() == UnicodeChars::FullwidthPlusSign.unicode();
}

bool isSubtractionOperatorAlias(const QChar& ch)
{
    switch (ch.unicode()) {
    case UnicodeChars::MinusSign.unicode():
    case UnicodeChars::HyphenMinus.unicode():
    case UnicodeChars::Hyphen.unicode():
    case UnicodeChars::NonBreakingHyphen.unicode():
    case UnicodeChars::EnDash.unicode():
    case UnicodeChars::EmDash.unicode():
    case UnicodeChars::HorizontalBar.unicode():
    case UnicodeChars::HyphenBullet.unicode():
    case UnicodeChars::SmallHyphenMinus.unicode():
    case UnicodeChars::FullwidthHyphenMinus.unicode():
        return true;
    default:
        return false;
    }
}

bool isExpressionBoundaryOperatorOrSeparator(const QChar& ch)
{
    return ch == AddOp
           || ch == SubOp
           || isMultiplicationOperator(ch)
           || isDivisionOperator(ch)
           || ch == PercentOp
           || ch == FactorOp
           || ch == PowOp
           || ch == BitAndOp
           || ch == BitOrOp
           || ch == Equals
           || ch == GreaterThanOp
           || ch == LessThanOp
           || ch == FunArgSep
           || ch == CommaSep;
}

bool isExpressionOperatorOrSeparator(const QChar& ch)
{
    return isExpressionBoundaryOperatorOrSeparator(ch);
}

QString buildWrappedToken(const QChar& op, const QChar& wrap)
{
    return buildWrappedToken(op, wrap, wrap);
}

QString buildWrappedToken(const QChar& op, const QChar& leftWrap, const QChar& rightWrap)
{
    QString out;
    out.reserve(3);
    out += leftWrap;
    out += op;
    out += rightWrap;
    return out;
}

QString buildWrappedToken(const QString& op, const QChar& wrap)
{
    return buildWrappedToken(op, wrap, wrap);
}

QString buildWrappedToken(const QString& op, const QChar& leftWrap, const QChar& rightWrap)
{
    QString out;
    out.reserve(op.size() + 2);
    out += leftWrap;
    out += op;
    out += rightWrap;
    return out;
}

} // namespace MathDsl
