// SPDX-FileCopyrightText: 2007-2011, 2013-2018, 2020, 2022, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/evaluator.h"
#include "core/constants.h"
#include "core/regexpatterns.h"
#include "core/session.h"
#include "core/settings.h"
#include "core/unitdisplayformat.h"
#include "core/unicodechars.h"
#include "core/mathdsl.h"
#include "core/numberformatter.h"
#include "math/rational.h"
#include "core/units.h"

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>
#include <QStack>
#include <cmath>
#include <limits>

#define ALLOW_IMPLICIT_MULT

static constexpr int MAX_PRECEDENCE = INT_MAX;
static constexpr int INVALID_PRECEDENCE = INT_MIN;

static bool isSubscriptDigit(QChar ch);
static bool isSubscriptLetter(QChar ch);
static bool isIdentifierStart(QChar ch);
static bool isIdentifierContinue(QChar ch);
static QString superscriptDigitsToAscii(const QString& text);
static bool normalizeUnsignedIntegerEquivalentDecimalText(QString& text);
static QString rewriteListBracesToInternalCalls(const QString& text);

#ifdef EVALUATOR_DEBUG
#include <QDebug>
#include <QFile>
#include <QTextStream>

QTextStream& operator<<(QTextStream& s, Quantity num)
{
    s << DMath::format(num, Quantity::Format::Fixed());
    return s;
}
#endif // EVALUATOR_DEBUG

static Evaluator* s_evaluatorInstance = 0;

static void s_deleteEvaluator()
{
    delete s_evaluatorInstance;
}

static bool s_isSummationFunctionIdentifier(const QString& identifier)
{
    return FunctionRepo::instance()->isIdentifierAliasOf(identifier, QStringLiteral("summation"));
}

static bool s_isMolmassFunctionIdentifier(const QString& identifier)
{
    return FunctionRepo::instance()->isIdentifierAliasOf(identifier, QStringLiteral("molmass"));
}

static bool s_isMassFunctionIdentifier(const QString& identifier)
{
    return FunctionRepo::instance()->isIdentifierAliasOf(identifier, QStringLiteral("mass"));
}

static bool s_isUpperAsciiLetter(QChar ch)
{
    return ch >= QLatin1Char('A') && ch <= QLatin1Char('Z');
}

static bool s_isLowerAsciiLetter(QChar ch)
{
    return ch >= QLatin1Char('a') && ch <= QLatin1Char('z');
}

static bool s_isAsciiDigit(QChar ch)
{
    return ch >= QLatin1Char('0') && ch <= QLatin1Char('9');
}

static const QHash<QString, HNumber>& s_molmassElementWeights()
{
    static const QHash<QString, HNumber> weights = [] {
        struct Entry {
            const char* symbol;
            const char* mass;
        };
        static const Entry entries[] = {
            {"H", "1.008"}, {"He", "4.0026"}, {"Li", "6.94"}, {"Be", "9.0122"}, {"B", "10.81"},
            {"C", "12.011"}, {"N", "14.007"}, {"O", "15.999"}, {"F", "18.998"}, {"Ne", "20.180"},
            {"Na", "22.990"}, {"Mg", "24.305"}, {"Al", "26.982"}, {"Si", "28.085"}, {"P", "30.974"},
            {"S", "32.06"}, {"Cl", "35.45"}, {"Ar", "39.948"}, {"K", "39.098"}, {"Ca", "40.078"},
            {"Sc", "44.956"}, {"Ti", "47.867"}, {"V", "50.942"}, {"Cr", "51.996"}, {"Mn", "54.938"},
            {"Fe", "55.845"}, {"Co", "58.933"}, {"Ni", "58.693"}, {"Cu", "63.546"}, {"Zn", "65.38"},
            {"Ga", "69.723"}, {"Ge", "72.630"}, {"As", "74.922"}, {"Se", "78.971"}, {"Br", "79.904"},
            {"Kr", "83.798"}, {"Rb", "85.468"}, {"Sr", "87.62"}, {"Y", "88.906"}, {"Zr", "91.224"},
            {"Nb", "92.906"}, {"Mo", "95.95"}, {"Tc", "98"}, {"Ru", "101.07"}, {"Rh", "102.91"},
            {"Pd", "106.42"}, {"Ag", "107.87"}, {"Cd", "112.41"}, {"In", "114.82"}, {"Sn", "118.71"},
            {"Sb", "121.76"}, {"Te", "127.60"}, {"I", "126.90"}, {"Xe", "131.29"}, {"Cs", "132.91"},
            {"Ba", "137.33"}, {"La", "138.91"}, {"Ce", "140.12"}, {"Pr", "140.91"}, {"Nd", "144.24"},
            {"Pm", "145"}, {"Sm", "150.36"}, {"Eu", "151.96"}, {"Gd", "157.25"}, {"Tb", "158.93"},
            {"Dy", "162.50"}, {"Ho", "164.93"}, {"Er", "167.26"}, {"Tm", "168.93"}, {"Yb", "173.05"},
            {"Lu", "174.97"}, {"Hf", "178.49"}, {"Ta", "180.95"}, {"W", "183.84"}, {"Re", "186.21"},
            {"Os", "190.23"}, {"Ir", "192.22"}, {"Pt", "195.08"}, {"Au", "196.97"}, {"Hg", "200.59"},
            {"Tl", "204.38"}, {"Pb", "207.2"}, {"Bi", "208.98"}, {"Po", "209"}, {"At", "210"},
            {"Rn", "222"}, {"Fr", "223"}, {"Ra", "226"}, {"Ac", "227"}, {"Th", "232.04"},
            {"Pa", "231.04"}, {"U", "238.03"}, {"Np", "237"}, {"Pu", "244"}, {"Am", "243"},
            {"Cm", "247"}, {"Bk", "247"}, {"Cf", "251"}, {"Es", "252"}, {"Fm", "257"},
            {"Md", "258"}, {"No", "259"}, {"Lr", "266"}, {"Rf", "267"}, {"Db", "268"},
            {"Sg", "269"}, {"Bh", "270"}, {"Hs", "277"}, {"Mt", "278"}, {"Ds", "281"},
            {"Rg", "282"}, {"Cn", "285"}, {"Nh", "286"}, {"Fl", "289"}, {"Mc", "290"},
            {"Lv", "293"}, {"Ts", "294"}, {"Og", "294"}
        };

        QHash<QString, HNumber> table;
        table.reserve(static_cast<int>(sizeof(entries) / sizeof(entries[0])));
        for (const Entry& entry : entries)
            table.insert(QString::fromLatin1(entry.symbol), HNumber(entry.mass));
        return table;
    }();
    return weights;
}

static bool s_tryParseMolarMassFormula(const QString& formulaText, Quantity* outMass)
{
    if (!outMass)
        return false;

    QString formula = formulaText.trimmed();
    if (formula.isEmpty())
        return false;

    for (int i = 0; i < formula.size(); ++i) {
        const QChar ch = formula.at(i);
        if (s_isUpperAsciiLetter(ch)
            || s_isLowerAsciiLetter(ch)
            || s_isAsciiDigit(ch)) {
            continue;
        }
        if (isSubscriptDigit(ch)) {
            formula[i] = QChar(MathDsl::Dig0.unicode()
                               + (ch.unicode() - UnicodeChars::SubscriptZero.unicode()));
            continue;
        }
        return false;
    }

    const auto& weights = s_molmassElementWeights();
    HNumber totalMass(0);

    int pos = 0;
    while (pos < formula.size()) {
        if (!s_isUpperAsciiLetter(formula.at(pos)))
            return false;

        QString symbol;
        symbol += formula.at(pos);
        ++pos;
        if (pos < formula.size() && s_isLowerAsciiLetter(formula.at(pos))) {
            symbol += formula.at(pos);
            ++pos;
        }

        const auto symbolIt = weights.constFind(symbol);
        if (symbolIt == weights.constEnd())
            return false;

        QString countDigits;
        while (pos < formula.size() && s_isAsciiDigit(formula.at(pos))) {
            countDigits += formula.at(pos);
            ++pos;
        }
        if (!countDigits.isEmpty() && countDigits.startsWith(QLatin1Char('0')))
            return false;

        const HNumber count = countDigits.isEmpty()
                              ? HNumber(1)
                              : HNumber(countDigits.toLatin1().constData());
        totalMass += symbolIt.value() * count;
    }

    const Quantity gramsPerMole = Units::gram() / Units::mole();
    Quantity mass = Quantity(totalMass) * gramsPerMole;
    QString gramsPerMoleSymbol;
    if (runtimeUnitNegativeExponentStyle() == Settings::UnitNegativeExponentSuperscript) {
        gramsPerMoleSymbol = QString(::unitSymbol(UnitId::Gram))
            + QLatin1Char(' ')
            + QString(::unitSymbol(UnitId::Mole))
            + QString(MathDsl::PowNeg)
            + QString(MathDsl::Pow1);
    } else {
        gramsPerMoleSymbol = QString(::unitSymbol(UnitId::Gram))
            + MathDsl::DivOp
            + QString(::unitSymbol(UnitId::Mole));
    }
    mass.setDisplayUnit(gramsPerMole.numericValue(), gramsPerMoleSymbol);
    *outMass = mass;
    return true;
}

static QString s_toSuperscriptExponent(int exponent)
{
    QString result;
    if (exponent < 0)
        result += MathDsl::PowNeg;
    for (const QChar& digit : QString::number(qAbs(exponent)))
        result += MathDsl::asciiDigitToSuperscript(digit);
    return result;
}

static QString s_renderUnitExponentsAsSuperscripts(QString text)
{
    static const QRegularExpression caretExponentRe(
        QStringLiteral(R"(\^\(([−-]?\d+)\)|\^([−-]?\d+))"));

    int offset = 0;
    while (true) {
        const QRegularExpressionMatch match = caretExponentRe.match(text, offset);
        if (!match.hasMatch())
            break;

        QString expText = match.captured(1);
        if (expText.isEmpty())
            expText = match.captured(2);
        expText.replace(MathDsl::SubOp, MathDsl::SubOpAl1); // Unicode minus.
        bool ok = false;
        const int exponent = expText.toInt(&ok);
        if (!ok) {
            offset = match.capturedEnd();
            continue;
        }

        text.replace(match.capturedStart(), match.capturedLength(),
                     s_toSuperscriptExponent(exponent));
        offset = match.capturedStart() + 1;
    }
    return text;
}

static int s_parseSuperscriptExponent(const QString& exponentText, bool* ok)
{
    *ok = false;
    if (exponentText.isEmpty())
        return 0;

    bool negative = false;
    int pos = 0;
    if (exponentText.at(0) == MathDsl::PowNeg || exponentText.at(0) == MathDsl::PowPos) {
        negative = exponentText.at(0) == MathDsl::PowNeg;
        pos = 1;
    }
    if (pos >= exponentText.size())
        return 0;

    QString ascii;
    ascii.reserve(exponentText.size() - pos);
    for (; pos < exponentText.size(); ++pos) {
        const QChar ch = exponentText.at(pos);
        const QChar asciiDigit = MathDsl::superscriptDigitToAscii(ch);
        if (asciiDigit.isNull())
            return 0;
        ascii += asciiDigit;
    }

    bool intOk = false;
    const int value = ascii.toInt(&intOk);
    if (!intOk)
        return 0;
    *ok = true;
    return negative ? -value : value;
}

static QString s_negateUnitFactorExponent(QString factor)
{
    factor = factor.trimmed();
    if (factor.isEmpty())
        return factor;

    static const QRegularExpression caretExponentRe(
        QStringLiteral(R"(^(.*?)(?:\^\(([−-]?\d+)\)|\^([−-]?\d+))$)"));
    const QRegularExpressionMatch caretMatch = caretExponentRe.match(factor);
    if (caretMatch.hasMatch()) {
        QString expText = caretMatch.captured(2);
        if (expText.isEmpty())
            expText = caretMatch.captured(3);
        expText.replace(MathDsl::SubOp, MathDsl::SubOpAl1);
        bool ok = false;
        const int exponent = expText.toInt(&ok);
        if (ok)
            return caretMatch.captured(1) + MathDsl::PowOp + QString::number(-exponent);
    }

    int suffixStart = factor.size();
    while (suffixStart > 0) {
        const QChar ch = factor.at(suffixStart - 1);
        const bool isSuperscriptDigit = MathDsl::isSuperscriptDigit(ch);
        const bool isSuperscriptSign = MathDsl::isSuperscriptSign(ch);
        if (!isSuperscriptDigit && !isSuperscriptSign)
            break;
        --suffixStart;
    }
    if (suffixStart < factor.size()) {
        bool ok = false;
        const int exponent = s_parseSuperscriptExponent(factor.mid(suffixStart), &ok);
        if (ok)
            return factor.left(suffixStart) + MathDsl::PowOp + QString::number(-exponent);
    }

    return factor + QLatin1String("^-1");
}

static QString s_applySuperscriptStyleToExplicitUnitText(QString unitText)
{
    if (!unitText.contains(MathDsl::DivOp))
        return s_renderUnitExponentsAsSuperscripts(unitText);

    QStringList parts = unitText.split(MathDsl::DivOp);
    if (parts.isEmpty())
        return s_renderUnitExponentsAsSuperscripts(unitText);

    QString normalized = parts.takeFirst().trimmed();
    for (QString denominator : parts) {
        denominator = denominator.trimmed();
        if (denominator.startsWith(MathDsl::GroupStart) && denominator.endsWith(MathDsl::GroupEnd))
            denominator = denominator.mid(1, denominator.size() - 2).trimmed();
        denominator.replace(MathDsl::MulDotOp, MathDsl::MulOpAl1);

        const QStringList factors = denominator.split(MathDsl::MulOpAl1, Qt::SkipEmptyParts);
        for (const QString& rawFactor : factors) {
            const QString negated = s_negateUnitFactorExponent(rawFactor);
            if (negated.isEmpty())
                continue;
            if (!normalized.isEmpty())
                normalized += MathDsl::MulOpAl1;
            normalized += negated;
        }
    }

    QString rendered = s_renderUnitExponentsAsSuperscripts(normalized);
    rendered.replace(MathDsl::MulOpAl1, MathDsl::MulDotOp);
    return rendered;
}

static bool s_isOpeningUnitBracketToken(const Token& token)
{
    return token.asOperator() == Token::AssociationStart
           && token.text() == QString(MathDsl::UnitStart);
}

static bool s_isClosingUnitBracketToken(const Token& token)
{
    return token.asOperator() == Token::AssociationEnd
           && token.text() == QString(MathDsl::UnitEnd);
}

static bool s_isOpeningListBraceToken(const Token& token)
{
    return token.asOperator() == Token::AssociationStart
           && token.text() == QLatin1String("{");
}

static bool s_isClosingListBraceToken(const Token& token)
{
    return token.asOperator() == Token::AssociationEnd
           && token.text() == QLatin1String("}");
}

static bool s_tokenSourceContainsExplicitUnitAttachment(const Token& token,
                                                        const QString& expression)
{
    if (!token.isOperand())
        return false;
    if (token.pos() < 0 || token.size() <= 0)
        return false;
    if (token.pos() + token.size() > expression.size())
        return false;

    const QString source = expression.mid(token.pos(), token.size());
    return source.contains(MathDsl::UnitStart) && source.contains(MathDsl::UnitEnd);
}

static QHash<QString, Quantity> s_buildBuiltInUnitLookup(char angleMode)
{
    return Units::builtInUnitLookup(angleMode);
}

static const QHash<QString, Quantity>& s_builtInUnitLookup()
{
    static char cachedAngleMode = '\0';
    static QHash<QString, Quantity> cache;

    const char angleMode = Settings::instance()->angleUnit;
    if (cache.isEmpty() || cachedAngleMode != angleMode) {
        cache = s_buildBuiltInUnitLookup(angleMode);
        cachedAngleMode = angleMode;
    }

    return cache;
}

static bool s_tryGetBuiltInUnitQuantity(const QString& identifier, Quantity* valueOut)
{
    const auto& lookup = s_builtInUnitLookup();
    const auto it = lookup.constFind(identifier);
    if (it == lookup.constEnd())
        return false;
    if (valueOut)
        *valueOut = it.value();
    return true;
}

static bool s_isBuiltInUnitIdentifier(const QString& identifier)
{
    return s_builtInUnitLookup().contains(identifier);
}

static QString rewriteListBracesToInternalCalls(const QString& text)
{
    QString result;
    result.reserve(text.size());
    bool inComment = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == MathDsl::CommentSep)
            inComment = true;
        if (!inComment && ch == MathDsl::ListStart) {
            result += QStringLiteral("list(");
        } else if (!inComment && ch == MathDsl::ListEnd) {
            result += MathDsl::GroupEnd;
        } else {
            result += ch;
        }
    }
    return result;
}

static bool s_isUserUnitLhsTokens(const Tokens& tokens)
{
    return tokens.count() > 4
        && tokens.at(0).asOperator() == Token::AssociationStart
        && tokens.at(0).text() == QString(MathDsl::UnitStart)
        && tokens.at(1).isUnitIdentifier()
        && tokens.at(2).asOperator() == Token::AssociationEnd
        && tokens.at(2).text() == QString(MathDsl::UnitEnd)
        && tokens.at(3).asOperator() == Token::Assignment;
}

static bool s_isBracketedExpression(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.size() < 2
        || trimmed.at(0) != MathDsl::UnitStart
        || trimmed.at(trimmed.size() - 1) != MathDsl::UnitEnd)
    {
        return false;
    }

    int depth = 0;
    for (int i = 0; i < trimmed.size(); ++i) {
        const QChar ch = trimmed.at(i);
        if (ch == MathDsl::UnitStart) {
            ++depth;
        } else if (ch == MathDsl::UnitEnd) {
            --depth;
            if (depth < 0)
                return false;
            if (depth == 0 && i != trimmed.size() - 1)
                return false;
        }
    }
    return depth == 0;
}

static bool splitUserFunctionDescription(const QString& expression,
                                         QString* expressionWithoutDescription,
                                         QString* description)
{
    const int equalsPos = expression.indexOf(MathDsl::Equals);
    if (equalsPos < 0)
        return false;

    const QString leftSide = expression.left(equalsPos);
    if (!leftSide.contains('(') || !leftSide.contains(')'))
        return false;

    int depth = 0;
    const int n = expression.size();
    for (int i = equalsPos + 1; i < n; ++i) {
        const QChar ch = expression.at(i);
        if (ch == '(') {
            ++depth;
        } else if (ch == ')' && depth > 0) {
            --depth;
        } else if (ch == MathDsl::CommentSep && depth == 0) {
            *expressionWithoutDescription = expression.left(i).trimmed();
            *description = expression.mid(i + 1).trimmed();
            return true;
        }
    }

    return false;
}

static bool splitVariableDescription(const QString& expression,
                                     QString* expressionWithoutDescription,
                                     QString* description)
{
    const int equalsPos = expression.indexOf(MathDsl::Equals);
    if (equalsPos < 0)
        return false;

    const QString leftSide = expression.left(equalsPos).trimmed();
    if (leftSide.contains('(') || leftSide.contains(')'))
        return false;

    int depth = 0;
    const int n = expression.size();
    for (int i = equalsPos + 1; i < n; ++i) {
        const QChar ch = expression.at(i);
        if (ch == '(') {
            ++depth;
        } else if (ch == ')' && depth > 0) {
            --depth;
        } else if (ch == MathDsl::CommentSep && depth == 0) {
            *expressionWithoutDescription = expression.left(i).trimmed();
            *description = expression.mid(i + 1).trimmed();
            return true;
        }
    }

    return false;
}

static int findTopLevelCommentDelimiterPos(const QString& text)
{
    int depth = 0;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == MathDsl::GroupStart) {
            ++depth;
            continue;
        }
        if (ch == MathDsl::GroupEnd) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (ch != MathDsl::CommentSep || depth != 0)
            continue;

        const bool hasLeftWhitespace = i > 0 && text.at(i - 1).isSpace();
        const bool hasRightWhitespace = i + 1 >= text.size() || text.at(i + 1).isSpace();
        if (hasLeftWhitespace && hasRightWhitespace)
            return i;
    }
    return -1;
}

static bool s_isSubtractionOperatorAlias(const QChar& ch)
{
    return ch == MathDsl::SubOpAl1 || ch == MathDsl::SubOp;
}

static bool s_isMultiplicationOperatorAlias(const QChar& ch, bool keepDotOperator = false)
{
    switch (ch.unicode()) {
    case MathDsl::MulOpAl1.unicode():
    case MathDsl::MulCrossOp.unicode():
        return true;
    case MathDsl::MulDotOp.unicode():
        return !keepDotOperator;
    default:
        return false;
    }
}

static bool s_isExpressionOperatorOrSeparator(const QChar& ch)
{
    return ch == MathDsl::AddOp
           || ch == MathDsl::SubOp
           || ch == MathDsl::MulDotOp
           || ch == MathDsl::DivOp
           || ch == MathDsl::PercentOp
           || ch == MathDsl::PowOp
           || ch == MathDsl::BitAndOp
           || ch == MathDsl::BitOrOp
           || ch == MathDsl::Equals
           || ch == MathDsl::GreaterThanOp
           || ch == MathDsl::LessThanOp
           || ch == MathDsl::UnitStart
           || ch == MathDsl::UnitEnd
           || ch == MathDsl::FunArgSep
           || ch == MathDsl::CommaSep;
}

static bool s_expressionWithoutIgnorableTrailingToken(const QString& text, QString* out)
{
    if (!out)
        return false;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return false;

    int i = trimmed.size() - 1;
    const QChar last = trimmed.at(i);
    const bool isPlusMinusTail =
        (last == MathDsl::AddOp || s_isSubtractionOperatorAlias(last));
    const bool isMultiplicationTail =
        (last == MathDsl::MulDotOp || s_isMultiplicationOperatorAlias(last, true));

    if (last != MathDsl::GroupStart
        && last != MathDsl::UnitStart
        && !isPlusMinusTail
        && !isMultiplicationTail
        && last != QLatin1Char(';')
        && last != MathDsl::DivOp
        && last != MathDsl::PowOp
        && last != MathDsl::IntDivOp)
        return false;

    if (last == MathDsl::DivOp) {
        --i;
        if (i >= 0 && trimmed.at(i) == MathDsl::DivOp)
            return false;
    } else if (last == MathDsl::PowOp) {
        --i;
        if (i >= 0 && trimmed.at(i) == MathDsl::PowOp)
            return false;
    } else if (isPlusMinusTail) {
        while (i >= 0 && (trimmed.at(i) == MathDsl::AddOp
                          || s_isSubtractionOperatorAlias(trimmed.at(i))))
            --i;
    } else if (isMultiplicationTail) {
        --i;
        if (i >= 0) {
            const QChar prev = trimmed.at(i);
            const bool prevIsMultiplication =
                (prev == MathDsl::MulDotOp
                 || s_isMultiplicationOperatorAlias(prev, true));
            if (prevIsMultiplication) {
                --i;
                if (i >= 0) {
                    const QChar prevPrev = trimmed.at(i);
                    const bool prevPrevIsMultiplication =
                        (prevPrev == MathDsl::MulDotOp
                         || s_isMultiplicationOperatorAlias(prevPrev, true));
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
        || prefixLast == MathDsl::UnitStart
        || s_isExpressionOperatorOrSeparator(prefixLast)
        || prefixLast == MathDsl::IntDivOp)
        return false;

    *out = prefix;
    return true;
}

bool isMinus(const QChar& ch)
{
    return ch == MathDsl::SubOpAl1 || ch == MathDsl::SubOp;
}

static bool isDegreeSign(const QChar& ch)
{
    // Accept both DEGREE SIGN (U+00B0) and MASCULINE ORDINAL INDICATOR
    // (U+00BA), plus other degree-like symbols emitted by some layouts/IMEs.
    return ch == MathDsl::Deg  // ° DEGREE SIGN
           || ch == UnicodeChars::MasculineOrdinalIndicator // º MASCULINE ORDINAL INDICATOR
           || ch == UnicodeChars::RingAbove // ˚ RING ABOVE
           || ch == UnicodeChars::RingOperator; // ∘ RING OPERATOR
}

bool isExponent(const QChar& ch, int base)
{
    switch (base) {
        case 2:
            return ch == QLatin1Char('b') || ch == QLatin1Char('B');
        case 8:
            return ch == QLatin1Char('o') || ch == QLatin1Char('O') || ch == QLatin1Char('C');
        case 10:
            return ch == QLatin1Char('e') || ch == QLatin1Char('E');
        case 16:
            return ch == QLatin1Char('h') || ch == QLatin1Char('H');
        default:
            return false;
    }
}

const Quantity& Evaluator::checkOperatorResult(const Quantity& n)
{
    switch (n.error()) {
    case Success: break;
    case NoOperand:
        if (!m_assignFunc)
            // The arguments are still NaN, so ignore this error.
            m_error = Evaluator::tr("cannot operate on a NaN");
        break;
    case Underflow:
        m_error = Evaluator::tr("underflow - tiny result is out "
                                "of SpeedCrunch's number range");
        break;
    case Overflow:
        m_error = Evaluator::tr("overflow - huge result is out of "
                                "SpeedCrunch's number range");
        break;
    case ZeroDivide:
        m_error = Evaluator::tr("division by zero");
        break;
    case OutOfLogicRange:
        if (!(m_assignFunc && !m_assignArg.isEmpty())) {
            m_error = Evaluator::tr("overflow - logic result exceeds "
                                    "maximum of 256 bits");
        }
        break;
    case OutOfIntegerRange:
        if (!(m_assignFunc && !m_assignArg.isEmpty())) {
            m_error = Evaluator::tr("overflow - integer result exceeds "
                                    "maximum limit for integers");
        }
        break;
    case TooExpensive:
        m_error = Evaluator::tr("too time consuming - "
                                "computation was rejected");
        break;
    case DimensionMismatch:
        // We cannot make any assumptions about the dimension of the arguments,
        // so ignore this error when assigning a function.
        if (!m_assignFunc)
            m_error = Evaluator::tr("dimension mismatch - quantities with "
                                    "different dimensions cannot be "
                                    "compared, added, etc.");
        break;
    case InvalidDimension:
        m_error = Evaluator::tr("invalid dimension - operation might "
                                "require dimensionless arguments");
        break;
    case NotImplemented:
        m_error = Evaluator::tr("operation is not defined for these arguments");
        break;
    case EvalUnstable:
        m_error = Evaluator::tr("Computation aborted - encountered "
                                "numerical instability");
        break;
    default:;
    }

    return n;
}

QString Evaluator::stringFromFunctionError(Function* function)
{
    if (!function->error())
        return QString();

    QString result = QString::fromLatin1("<b>%1</b>: ");

    switch (function->error()) {
    case Success: break;
    case InvalidParamCount:
        result += Evaluator::tr("wrong number of arguments");
        break;
    case NoOperand:
        result += Evaluator::tr("does not take NaN as an argument");
        break;
    case Overflow:
        result += Evaluator::tr("overflow - huge result is out of "
                                "SpeedCrunch's number range");
        break;
    case Underflow:
        result += Evaluator::tr("underflow - tiny result is out of "
                                "SpeedCrunch's number range");
        break;
    case OutOfLogicRange:
        result += Evaluator::tr("overflow - logic result exceeds "
                                "maximum of 256 bits");
        break;
    case OutOfIntegerRange:
        result += Evaluator::tr("result out of range");
        break;
    case ZeroDivide:
        result += Evaluator::tr("division by zero");
        break;
    case EvalUnstable:
        result += Evaluator::tr("Computation aborted - "
                                "encountered numerical instability");
        break;
    case OutOfDomain:
        result += Evaluator::tr("undefined for argument domain");
        break;
    case TooExpensive:
        result += Evaluator::tr("computation too expensive");
        break;
    case InvalidDimension:
        result += Evaluator::tr("invalid dimension - function "
                                "might require dimensionless arguments");
        break;
    case DimensionMismatch:
        result += Evaluator::tr("dimension mismatch - quantities with "
                                "different dimensions cannot be compared, "
                                "added, etc.");
        break;
    case IONoBase:
    case BadLiteral:
    case InvalidPrecision:
    case InvalidParam:
        result += Evaluator::tr("internal error, please report a bug");
        break;
    default:
        result += Evaluator::tr("error");
        break;
    };

    return result.arg(function->identifier());
}

class TokenStack : public QVector<Token> {
public:
    TokenStack();
    bool isEmpty() const;
    unsigned itemCount() const;
    Token pop();
    void push(const Token& token);
    const Token& top();
    const Token& top(unsigned index);
    bool hasError() const { return !m_error.isEmpty(); }
    QString error() const { return m_error; }
    void reduce(int count, int minPrecedence = INVALID_PRECEDENCE);
    void reduce(int count, Token&& top, int minPrecedence = INVALID_PRECEDENCE);
    void reduce(QList<Token> tokens, Token&& top,
                int minPrecedence = INVALID_PRECEDENCE);
private:
    void ensureSpace();
    int topIndex;
    QString m_error;
};

const Token Token::null;

static Token::Operator matchOperator(const QString& text)
{
    Token::Operator result = Token::Invalid;

    if (text.length() == 1) {
        QChar p = text.at(0);
        switch(p.unicode()) {
        case '+':
            result = Token::Addition;
            break;
        case MathDsl::SubOp.unicode():
        case MathDsl::SubOpAl1.unicode():
            result = Token::Subtraction;
            break;
        case MathDsl::MulCrossOp.unicode():
        case MathDsl::MulDotOp.unicode():
        case MathDsl::MulOpAl1.unicode():
            result = Token::Multiplication;
            break;
        case UnicodeChars::DivisionSign.unicode():
        case UnicodeChars::BigSolidus.unicode():
        case '/':
            result = Token::Division;
            break;
        case MathDsl::PowOp.unicode():
            result = Token::Exponentiation;
            break;
        case MathDsl::FunArgSep.unicode():
            result = Token::ListSeparator;
            break;
        case MathDsl::GroupStart.unicode():
            result = Token::AssociationStart;
            break;
        case MathDsl::GroupEnd.unicode():
            result = Token::AssociationEnd;
            break;
        case MathDsl::UnitStart.unicode():
            result = Token::AssociationStart;
            break;
        case MathDsl::UnitEnd.unicode():
            result = Token::AssociationEnd;
            break;
        case MathDsl::ListStart.unicode():
            result = Token::AssociationStart;
            break;
        case MathDsl::ListEnd.unicode():
            result = Token::AssociationEnd;
            break;
        case MathDsl::FactorOp.unicode():
            result = Token::Factorial;
            break;
        case MathDsl::PercentOp.unicode():
            result = Token::Percent;
            break;
        case MathDsl::Equals.unicode():
            result = Token::Assignment;
            break;
        case '\\':
            result = Token::IntegerDivision;
            break;
        case '~':
            result = Token::BitwiseLogicalNOT;
            break;
        case MathDsl::BitAndOp.unicode():
            result = Token::BitwiseLogicalAND;
            break;
        case MathDsl::BitOrOp.unicode():
            result = Token::BitwiseLogicalOR;
            break;
        case MathDsl::TransOp.unicode():
            result = Token::UnitConversion;
            break;
        default:
            result = Token::Invalid;
        }

    } else if (text.length() == 2) {
        if (text == "**")
            result = Token::Exponentiation;
        else if (text == "<<")
          result = Token::ArithmeticLeftShift;
        else if (text == ">>")
          result = Token::ArithmeticRightShift;
        else if (text == "->"
                 || (text.at(0) == MathDsl::SubOp && text.at(1) == UnicodeChars::GreaterThanSign)
                 || text.compare(MathDsl::TransOpAl1, Qt::CaseInsensitive) == 0)
            result = Token::UnitConversion;
    }

   return result;
}

// Helper function: give operator precedence e.g. "+" is 300 while "*" is 500.
static int opPrecedence(Token::Operator op)
{
    int prec;
    switch (op) {
    case Token::Factorial:
    case Token::Percent:
        prec = 800;
        break;
    case Token::Exponentiation:
        prec = 700;
        break;
    case Token::Function:
        // Not really operator but needed for managing shift/reduce conflicts.
        prec = 600;
        break;
    case Token::Multiplication:
    case Token::Division:
        prec = 500;
        break;
    case Token::Modulo:
    case Token::IntegerDivision:
        prec = 600;
        break;
    case Token::Addition:
    case Token::Subtraction:
        prec = 300;
        break;
    case Token::ArithmeticLeftShift:
    case Token::ArithmeticRightShift:
        prec = 200;
        break;
    case Token::BitwiseLogicalNOT:
        prec = 150;
        break;
    case Token::BitwiseLogicalAND:
        prec = 100;
        break;
    case Token::BitwiseLogicalOR:
        prec = 50;
        break;
    case Token::UnitConversion:
        prec = 0;
        break;
    case Token::AssociationEnd:
    case Token::ListSeparator:
        prec = -100;
        break;
    case Token::AssociationStart:
        prec = -200;
        break;
    default:
        prec = -200;
        break;
    }
    return prec;
}

static int opcodePrecedence(Opcode::Type opcodeType)
{
    switch (opcodeType) {
    case Opcode::Add:
    case Opcode::Sub:
        return opPrecedence(Token::Addition);
    case Opcode::Mul:
    case Opcode::Div:
        return opPrecedence(Token::Multiplication);
    case Opcode::Unit:
        return opPrecedence(Token::Factorial) + 1;
    case Opcode::Pow:
        return opPrecedence(Token::Exponentiation);
    case Opcode::Fact:
        return opPrecedence(Token::Factorial);
    case Opcode::Percent:
        return opPrecedence(Token::Percent);
    case Opcode::Modulo:
    case Opcode::IntDiv:
        return opPrecedence(Token::Modulo);
    case Opcode::LSh:
    case Opcode::RSh:
        return opPrecedence(Token::ArithmeticLeftShift);
    case Opcode::BAnd:
        return opPrecedence(Token::BitwiseLogicalAND);
    case Opcode::BOr:
        return opPrecedence(Token::BitwiseLogicalOR);
    case Opcode::Conv:
        return opPrecedence(Token::UnitConversion);
    case Opcode::Neg:
    case Opcode::BNot:
        return opPrecedence(Token::Multiplication);
    default:
        return MAX_PRECEDENCE;
    }
}

static bool isRightAssociativeOpcode(Opcode::Type opcodeType)
{
    return opcodeType == Opcode::Pow;
}

static QString opcodeToInfixSymbol(Opcode::Type opcodeType, bool implicitMultiplication = false)
{
    switch (opcodeType) {
    case Opcode::Add: return "+";
    case Opcode::Sub: return "-";
    case Opcode::Mul:
        return implicitMultiplication
            ? QString(MathDsl::MulDotOp)
            : QString(MathDsl::MulCrossOp);
    case Opcode::Unit: return QString();
    case Opcode::Div: return "/";
    case Opcode::Pow: return "^";
    case Opcode::Percent: return "%";
    case Opcode::Modulo: return "mod";
    case Opcode::IntDiv: return "\\";
    case Opcode::LSh: return "<<";
    case Opcode::RSh: return ">>";
    case Opcode::BAnd: return "&";
    case Opcode::BOr: return "|";
    case Opcode::Conv: return QString(MathDsl::TransOp);
    default: return QString();
    }
}

Token::Token(Type type, const QString& text, int pos, int size)
{
    m_type = type;
    m_text = text;
    m_pos = pos;
    m_size = size;
    m_minPrecedence = MAX_PRECEDENCE;
}

Token::Token(const Token& token)
{
    m_type = token.m_type;
    m_text = token.m_text;
    m_pos = token.m_pos;
    m_size = token.m_size;
    m_minPrecedence = token.m_minPrecedence;
}

Token& Token::operator=(const Token& token)
{
    m_type = token.m_type;
    m_text = token.m_text;
    m_pos = token.m_pos;
    m_size = token.m_size;
    m_minPrecedence = token.m_minPrecedence;
    return*this;
}

Quantity Token::asNumber() const
{
    QString text = m_text;
    return isNumber() ? Quantity(CNumber((const char*)text.toLatin1()))
                      : Quantity(0);
}

Token::Operator Token::asOperator() const
{
    return isOperator() ? matchOperator(m_text) : Invalid;
}

QString Token::description() const
{
    QString desc;

    switch (m_type) {
    case stxNumber:
        desc = "Number";
        break;
    case stxIdentifier:
        desc = "Identifier";
        break;
    case stxUnitIdentifier:
        desc = "Unit Identifier";
        break;
    case stxOpenPar:
    case stxClosePar:
    case stxSep:
    case stxOperator:
        desc = "Operator";
        break;
    default:
        desc = "Unknown";
        break;
    }

    while (desc.length() < 10)
        desc.prepend(' ');

    QString header;
    header.append(QString::number(m_pos) + ","
                  + QString::number(m_pos + m_size - 1));
    header.append("," + (m_minPrecedence == MAX_PRECEDENCE ?
                             "MAX" : QString::number(m_minPrecedence)));
    header.append("  ");

    while (header.length() < 10)
        header.append(' ');

    desc.prepend(header);
    desc.append(" : ").append(m_text);

    return desc;
}

static bool tokenPositionCompare(const Token& a, const Token& b)
{
    return (a.pos() < b.pos());
}

static bool tokenCanEndOperandForDisplaySpacing(const Token& token)
{
    return token.isOperand()
        || token.asOperator() == Token::AssociationEnd
        || token.asOperator() == Token::Factorial
        || token.asOperator() == Token::Percent;
}

static bool tokenCanStartOperandForDisplaySpacing(const Token& token)
{
    return token.isOperand()
        || token.asOperator() == Token::AssociationStart
        || token.asOperator() == Token::Addition
        || token.asOperator() == Token::Subtraction
        || token.asOperator() == Token::BitwiseLogicalNOT;
}

static bool isImaginaryUnitTokenForDisplay(const Token& token)
{
    if (!token.isIdentifier())
        return false;

    const QString text = token.text();
    if (text == QLatin1String("i") || text == QLatin1String("j"))
        return true;

    const QChar configuredImaginaryUnit(Settings::instance()->imaginaryUnit);
    return text.size() == 1 && text.at(0) == configuredImaginaryUnit;
}

static bool startsWithImaginaryLiteralForDisplay(const Tokens& tokens, int startIndex)
{
    if (startIndex < 0 || startIndex >= tokens.size())
        return false;

    if (isImaginaryUnitTokenForDisplay(tokens.at(startIndex)))
        return true;

    if (startIndex + 2 >= tokens.size())
        return false;

    const Token::Operator mulOp = tokens.at(startIndex + 1).asOperator();
    return tokens.at(startIndex).isNumber()
        && mulOp == Token::Multiplication
        && isImaginaryUnitTokenForDisplay(tokens.at(startIndex + 2));
}

static bool isStandaloneNumericLiteralOperandForDisplay(const Tokens& tokens,
                                                        int startIndex,
                                                        int endIndex)
{
    if (startIndex < 0 || endIndex >= tokens.size() || startIndex > endIndex)
        return false;

    int start = startIndex;
    int end = endIndex;

    while (start <= end) {
        const Token::Operator op = tokens.at(start).asOperator();
        if (op == Token::Addition
            || op == Token::Subtraction
            || op == Token::BitwiseLogicalNOT) {
            ++start;
            continue;
        }
        break;
    }
    while (start <= end) {
        const Token::Operator op = tokens.at(end).asOperator();
        if (op == Token::Percent || op == Token::Factorial) {
            --end;
            continue;
        }
        break;
    }
    if (start > end)
        return false;

    if (tokens.at(end).text() == QString(MathDsl::UnitEnd)) {
        int depth = 0;
        int unitStart = -1;
        for (int i = end; i >= start; --i) {
            const QString text = tokens.at(i).text();
            if (text == QString(MathDsl::UnitEnd)) {
                ++depth;
                continue;
            }
            if (text == QString(MathDsl::UnitStart)) {
                --depth;
                if (depth == 0) {
                    unitStart = i;
                    break;
                }
            }
        }
        if (unitStart < 0)
            return false;
        end = unitStart - 1;
        if (start > end)
            return false;
    }

    if (start == end && tokens.at(start).isNumber())
        return true;

    if (tokens.at(start).asOperator() == Token::AssociationStart
        && tokens.at(end).asOperator() == Token::AssociationEnd) {
        int depth = 0;
        for (int i = start; i <= end; ++i) {
            const Token::Operator op = tokens.at(i).asOperator();
            if (op == Token::AssociationStart)
                ++depth;
            else if (op == Token::AssociationEnd)
                --depth;
            if (depth == 0 && i < end)
                return false;
        }
        return isStandaloneNumericLiteralOperandForDisplay(tokens, start + 1, end - 1);
    }

    return false;
}

static bool isUnsignedIntegerDigitsText(const QString& text)
{
    if (text.isEmpty())
        return false;
    for (const QChar& ch : text) {
        if (!ch.isDigit())
            return false;
    }
    return true;
}

static bool isIntegerNumericLiteralOperandForDisplay(const Tokens& tokens,
                                                     int rangeStart,
                                                     int rangeEnd)
{
    if (!isStandaloneNumericLiteralOperandForDisplay(tokens, rangeStart, rangeEnd))
        return false;

    if (rangeStart == rangeEnd && tokens.at(rangeStart).isNumber()) {
        QString text = tokens.at(rangeStart).text();
        return normalizeUnsignedIntegerEquivalentDecimalText(text)
            || isUnsignedIntegerDigitsText(text);
    }

    if (tokens.at(rangeStart).asOperator() == Token::AssociationStart
        && tokens.at(rangeEnd).asOperator() == Token::AssociationEnd) {
        int innerStart = rangeStart + 1;
        int innerEnd = rangeEnd - 1;
        if (innerStart > innerEnd)
            return false;
        if (tokens.at(innerStart).asOperator() == Token::Addition
            || tokens.at(innerStart).asOperator() == Token::Subtraction) {
            ++innerStart;
        }
        if (innerStart != innerEnd || !tokens.at(innerStart).isNumber())
            return false;
        QString text = tokens.at(innerStart).text();
        return normalizeUnsignedIntegerEquivalentDecimalText(text)
            || isUnsignedIntegerDigitsText(text);
    }

    return false;
}

static bool isNumericPowerLiteralOperandForDisplay(const Tokens& tokens,
                                                   int rangeStart,
                                                   int rangeEnd)
{
    if (rangeStart < 0 || rangeEnd >= tokens.size() || rangeStart > rangeEnd)
        return false;

    int start = rangeStart;
    int end = rangeEnd;

    if (tokens.at(start).asOperator() == Token::AssociationStart
        && tokens.at(end).asOperator() == Token::AssociationEnd) {
        int depth = 0;
        bool enclosesWholeRange = true;
        for (int i = start; i <= end; ++i) {
            const Token::Operator op = tokens.at(i).asOperator();
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
            return isNumericPowerLiteralOperandForDisplay(tokens, start + 1, end - 1);
    }

    int exponentOp = -1;
    int depth = 0;
    for (int i = start; i <= end; ++i) {
        const Token::Operator op = tokens.at(i).asOperator();
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

    const bool baseIsNumericLiteral =
        isStandaloneNumericLiteralOperandForDisplay(tokens, start, exponentOp - 1)
        || isNumericPowerLiteralOperandForDisplay(tokens, start, exponentOp - 1);
    const bool exponentIsNumericLiteral =
        isStandaloneNumericLiteralOperandForDisplay(tokens, exponentOp + 1, end)
        || isNumericPowerLiteralOperandForDisplay(tokens, exponentOp + 1, end);
    return baseIsNumericLiteral && exponentIsNumericLiteral;
}

static bool isHardcodedNumericLiteralOperandForDisplay(const Tokens& tokens,
                                                       int rangeStart,
                                                       int rangeEnd)
{
    return isStandaloneNumericLiteralOperandForDisplay(tokens, rangeStart, rangeEnd)
        || isNumericPowerLiteralOperandForDisplay(tokens, rangeStart, rangeEnd);
}

static bool isHardcodedNumericOperandOnLeftOfMultiplicationForDisplay(const Tokens& tokens,
                                                                      int mulIndex,
                                                                      bool includeNumericPowers)
{
    if (mulIndex <= 0 || mulIndex >= tokens.size())
        return false;

    int end = mulIndex - 1;
    int start = end;

    if (tokens.at(end).text() == QString(MathDsl::UnitEnd)) {
        int depth = 0;
        int unitStart = -1;
        for (int i = end; i >= 0; --i) {
            const QString text = tokens.at(i).text();
            if (text == QString(MathDsl::UnitEnd)) {
                ++depth;
                continue;
            }
            if (text == QString(MathDsl::UnitStart)) {
                --depth;
                if (depth == 0) {
                    unitStart = i;
                    break;
                }
            }
        }
        if (unitStart < 1)
            return false;
        start = unitStart - 1;
    }

    if (tokens.at(start).asOperator() == Token::AssociationEnd) {
        int depth = 0;
        int groupStart = -1;
        for (int i = start; i >= 0; --i) {
            const Token::Operator op = tokens.at(i).asOperator();
            if (op == Token::AssociationEnd) {
                ++depth;
                continue;
            }
            if (op == Token::AssociationStart) {
                --depth;
                if (depth == 0) {
                    groupStart = i;
                    break;
                }
            }
        }
        if (groupStart < 0)
            return false;
        // "(...)" immediately preceded by an identifier is a function call
        // operand (e.g. "sin(...)"), not a hardcoded numeric literal.
        if (groupStart > 0 && tokens.at(groupStart - 1).isIdentifier())
            return false;
        start = groupStart;
    }

    if (start >= 2
        && tokens.at(start - 1).asOperator() == Token::Exponentiation
        && tokens.at(start - 2).isNumber())
    {
        QString baseText = tokens.at(start - 2).text();
        normalizeUnsignedIntegerEquivalentDecimalText(baseText);
        const bool hasIntegerExponent = isIntegerNumericLiteralOperandForDisplay(tokens, start, end);
        const bool canTreatAsNumericPower =
            includeNumericPowers
            ? isHardcodedNumericLiteralOperandForDisplay(tokens, start, end)
            : hasIntegerExponent;
        if ((baseText == QLatin1String("10") && hasIntegerExponent)
            || (includeNumericPowers && canTreatAsNumericPower)) {
            start -= 2;
        }
    }

    if (start > 0) {
        const Token& previous = tokens.at(start - 1);
        const Token::Operator previousOp = previous.asOperator();
        if (tokenCanEndOperandForDisplaySpacing(previous)
            || previousOp == Token::Exponentiation) {
            return false;
        }
    }

    return includeNumericPowers
        ? isHardcodedNumericLiteralOperandForDisplay(tokens, start, end)
        : isStandaloneNumericLiteralOperandForDisplay(tokens, start, end);
}

static bool isHardcodedNumericOperandOnRightOfMultiplicationForDisplay(const Tokens& tokens,
                                                                       int mulIndex,
                                                                       bool includeNumericPowers)
{
    if (mulIndex < 0 || mulIndex >= tokens.size() - 1)
        return false;

    int start = mulIndex + 1;
    int end = start;

    const Token::Operator leadingOp = tokens.at(start).asOperator();
    if (leadingOp == Token::Addition
        || leadingOp == Token::Subtraction
        || leadingOp == Token::BitwiseLogicalNOT) {
        ++end;
        if (end >= tokens.size())
            return false;
    }

    if (tokens.at(end).asOperator() == Token::AssociationStart) {
        int depth = 0;
        int groupEnd = -1;
        for (int i = end; i < tokens.size(); ++i) {
            const Token::Operator op = tokens.at(i).asOperator();
            if (op == Token::AssociationStart) {
                ++depth;
                continue;
            }
            if (op == Token::AssociationEnd) {
                --depth;
                if (depth == 0) {
                    groupEnd = i;
                    break;
                }
            }
        }
        if (groupEnd < 0)
            return false;
        end = groupEnd;
    } else if (tokens.at(end).isNumber()) {
        if (end + 1 < tokens.size() && tokens.at(end + 1).text() == QString(MathDsl::UnitStart)) {
            int depth = 0;
            int unitEnd = -1;
            for (int i = end + 1; i < tokens.size(); ++i) {
                const QString text = tokens.at(i).text();
                if (text == QString(MathDsl::UnitStart)) {
                    ++depth;
                    continue;
                }
                if (text == QString(MathDsl::UnitEnd)) {
                    --depth;
                    if (depth == 0) {
                        unitEnd = i;
                        break;
                    }
                }
            }
            if (unitEnd < 0)
                return false;
            end = unitEnd;
        }
    }

    while (end + 1 < tokens.size()) {
        const Token::Operator op = tokens.at(end + 1).asOperator();
        if (op == Token::Percent || op == Token::Factorial) {
            ++end;
            continue;
        }
        break;
    }

    if (end + 1 < tokens.size() && tokens.at(end + 1).asOperator() == Token::Exponentiation) {
        if (start != end || !tokens.at(start).isNumber())
            return false;

        const int exponentStart = end + 2;
        if (exponentStart >= tokens.size())
            return false;
        int exponentEnd = exponentStart;
        if (tokens.at(exponentStart).asOperator() == Token::AssociationStart) {
            int depth = 0;
            int groupEnd = -1;
            for (int i = exponentStart; i < tokens.size(); ++i) {
                const Token::Operator op = tokens.at(i).asOperator();
                if (op == Token::AssociationStart) {
                    ++depth;
                    continue;
                }
                if (op == Token::AssociationEnd) {
                    --depth;
                    if (depth == 0) {
                        groupEnd = i;
                        break;
                    }
                }
            }
            if (groupEnd < 0)
                return false;
            exponentEnd = groupEnd;
        }

        const bool hasIntegerExponent =
            isIntegerNumericLiteralOperandForDisplay(tokens, exponentStart, exponentEnd);
        if (includeNumericPowers)
            return isHardcodedNumericLiteralOperandForDisplay(tokens, start, exponentEnd);

        QString baseText = tokens.at(start).text();
        normalizeUnsignedIntegerEquivalentDecimalText(baseText);
        if (baseText != QLatin1String("10"))
            return false;
        return hasIntegerExponent;
    }

    return includeNumericPowers
        ? isHardcodedNumericLiteralOperandForDisplay(tokens, start, end)
        : isStandaloneNumericLiteralOperandForDisplay(tokens, start, end);
}

static bool isUnsignedDecimalIntegerText(const QString& text)
{
    return RegExpPatterns::unsignedDecimalInteger().match(text).hasMatch();
}

static bool isUnsignedDecimalNumberText(const QString& text)
{
    return RegExpPatterns::unsignedDecimalNumber().match(text).hasMatch();
}

static bool isSignedDecimalNumberText(const QString& text)
{
    return RegExpPatterns::signedDecimalNumber().match(text).hasMatch();
}

static bool isSafeForDoubleArithmetic(const QString& text)
{
    if (!isSignedDecimalNumberText(text))
        return false;

    int digitCount = 0;
    for (const QChar& ch : text) {
        if (ch.isDigit())
            ++digitCount;
    }

    // Keep folding conservative to avoid precision regressions when using
    // temporary C++ doubles in the display simplifier.
    return digitCount <= 15;
}

static QString formatSimplifiedDecimal(double value)
{
    const double rounded = std::round(value);
    if (std::abs(value - rounded) < 1e-12)
        return QString::number(static_cast<long long>(rounded));

    QString text = QString::number(value, 'g', 15);
    if (text.contains(MathDsl::DotSep)) {
        while (text.endsWith(QLatin1Char('0')))
            text.chop(1);
        if (text.endsWith(MathDsl::DotSep))
            text.chop(1);
    }
    return text;
}

static bool normalizeUnsignedIntegerEquivalentDecimalText(QString& text)
{
    const QRegularExpressionMatch match = RegExpPatterns::unsignedIntegerEquivalentDecimal().match(text);
    if (!match.hasMatch())
        return false;

    const QString fractionalPart = match.captured(2);
    for (const QChar& ch : fractionalPart) {
        if (ch != QLatin1Char('0'))
            return false;
    }

    text = match.captured(1);
    return true;
}

static QString renderIntegerPowersAsSuperscriptsForDisplay(
    const QString& expression,
    bool collapseRightAssociativeIntegerPowerChains = false)
{
    auto collapseRightAssociativeIntegerPowerChainsFn = [](QString text) {
        auto checkedPow = [](long long base, long long exponent, long long* out) {
            if (exponent < 0)
                return false;
            long long result = 1;
            for (long long i = 0; i < exponent; ++i) {
                if (base != 0 && result > std::numeric_limits<long long>::max() / base)
                    return false;
                result *= base;
                if (result > 1000000)
                    return false;
            }
            *out = result;
            return true;
        };

        auto collapseIntegerExponentChainText = [&checkedPow](const QString& chain, long long* out) {
            QVector<long long> exponents;
            int pos = 0;
            while (pos < chain.size()) {
                bool negative = false;
                if (chain.at(pos) == MathDsl::SubOpAl1 || chain.at(pos) == MathDsl::SubOp) {
                    negative = true;
                    ++pos;
                }
                if (negative || pos >= chain.size() || !chain.at(pos).isDigit())
                    return false;

                long long value = 0;
                while (pos < chain.size() && chain.at(pos).isDigit()) {
                    const int digit = chain.at(pos).digitValue();
                    if (value > 1000000)
                        return false;
                    value = value * 10 + digit;
                    ++pos;
                }
                exponents.append(value);

                if (pos >= chain.size())
                    break;
                if (chain.at(pos) != MathDsl::PowOp)
                    return false;
                ++pos;
            }

            if (exponents.size() < 2)
                return false;

            long long collapsed = exponents.last();
            for (int idx = exponents.size() - 2; idx >= 0; --idx) {
                if (!checkedPow(exponents.at(idx), collapsed, &collapsed))
                    return false;
            }
            *out = collapsed;
            return true;
        };

        auto parsePositiveIntegerToken = [](const QString& input, int start, long long* valueOut, int* endOut) {
            int pos = start;
            bool parenthesized = false;
            if (pos < input.size() && input.at(pos) == MathDsl::GroupStart) {
                parenthesized = true;
                ++pos;
            }
            if (pos >= input.size() || !input.at(pos).isDigit())
                return false;
            long long value = 0;
            while (pos < input.size() && input.at(pos).isDigit()) {
                const int digit = input.at(pos).digitValue();
                if (value > 1000000)
                    return false;
                value = value * 10 + digit;
                ++pos;
            }
            if (parenthesized) {
                if (pos >= input.size() || input.at(pos) != MathDsl::GroupEnd)
                    return false;
                ++pos;
            }
            *valueOut = value;
            *endOut = pos;
            return true;
        };

        // Fold nested integer powers: (a^m)^n -> a^(m*n).
        while (true) {
            bool changed = false;
            for (int i = 1; i < text.size(); ++i) {
                if (text.at(i) != MathDsl::PowOp || text.at(i - 1) != MathDsl::GroupEnd)
                    continue;

                long long outerExp = 0;
                int outerEnd = i + 1;
                if (!parsePositiveIntegerToken(text, i + 1, &outerExp, &outerEnd))
                    continue;

                int depth = 0;
                int open = -1;
                for (int j = i - 1; j >= 0; --j) {
                    const QChar ch = text.at(j);
                    if (ch == MathDsl::GroupEnd)
                        ++depth;
                    else if (ch == MathDsl::GroupStart) {
                        --depth;
                        if (depth == 0) {
                            open = j;
                            break;
                        }
                    }
                }
                if (open < 0)
                    continue;

                const QString inner = text.mid(open + 1, i - open - 2);
                int innerDepth = 0;
                int innerPowPos = -1;
                for (int j = 0; j < inner.size(); ++j) {
                    const QChar ch = inner.at(j);
                    if (ch == MathDsl::GroupStart)
                        ++innerDepth;
                    else if (ch == MathDsl::GroupEnd) {
                        if (innerDepth > 0)
                            --innerDepth;
                    } else if (ch == MathDsl::PowOp && innerDepth == 0) {
                        innerPowPos = j;
                    }
                }
                if (innerPowPos <= 0)
                    continue;

                long long innerExp = 0;
                int innerExpEnd = innerPowPos + 1;
                if (!parsePositiveIntegerToken(inner, innerPowPos + 1, &innerExp, &innerExpEnd))
                    continue;
                if (innerExpEnd != inner.size())
                    continue;

                const QString innerBase = inner.left(innerPowPos);
                long long mergedExp = 0;
                if (innerExp != 0 && outerExp > 1000000 / innerExp)
                    continue;
                mergedExp = innerExp * outerExp;
                if (mergedExp > 1000000)
                    continue;

                text.replace(open, outerEnd - open, innerBase + MathDsl::PowOp + QString::number(mergedExp));
                changed = true;
                break;
            }
            if (!changed)
                break;
        }

        auto parseIntegerExponentToken = [&text, &collapseIntegerExponentChainText](int start, long long* valueOut, int* endOut) {
            int pos = start;
            if (pos < text.size() && text.at(pos) == MathDsl::GroupStart) {
                int depth = 0;
                int close = -1;
                for (int i = pos; i < text.size(); ++i) {
                    const QChar ch = text.at(i);
                    if (ch == MathDsl::GroupStart)
                        ++depth;
                    else if (ch == MathDsl::GroupEnd) {
                        --depth;
                        if (depth == 0) {
                            close = i;
                            break;
                        }
                    }
                }
                if (close < 0)
                    return false;
                const QString inner = text.mid(pos + 1, close - pos - 1).trimmed();
                long long collapsed = 0;
                if (!collapseIntegerExponentChainText(inner, &collapsed))
                    return false;
                *valueOut = collapsed;
                *endOut = close + 1;
                return true;
            }

            bool negative = false;
            if (pos < text.size() && (text.at(pos) == MathDsl::SubOpAl1 || text.at(pos) == MathDsl::SubOp)) {
                negative = true;
                ++pos;
            }

            if (pos >= text.size() || !text.at(pos).isDigit())
                return false;

            long long value = 0;
            while (pos < text.size() && text.at(pos).isDigit()) {
                const int digit = text.at(pos).digitValue();
                if (value > 1000000) // avoid overflow-heavy chains in display-only rewrite
                    return false;
                value = value * 10 + digit;
                ++pos;
            }

            *valueOut = negative ? -value : value;
            *endOut = pos;
            return true;
        };

        for (int i = 0; i < text.size(); ++i) {
            if (text.at(i) != MathDsl::PowOp)
                continue;

            QVector<long long> exponents;
            const bool startsWithParenthesizedExponentChain =
                (i + 1 < text.size() && text.at(i + 1) == MathDsl::GroupStart);
            int tokenPos = i + 1;
            while (tokenPos < text.size()) {
                long long value = 0;
                int tokenEnd = tokenPos;
                if (!parseIntegerExponentToken(tokenPos, &value, &tokenEnd))
                    break;
                exponents.append(value);
                tokenPos = tokenEnd;
                if (tokenPos >= text.size() || text.at(tokenPos) != MathDsl::PowOp)
                    break;
                ++tokenPos;
            }

            if (exponents.isEmpty())
                continue;
            if (exponents.size() < 2 && !startsWithParenthesizedExponentChain)
                continue;
            if (std::any_of(exponents.constBegin(), exponents.constEnd(),
                    [](long long v) { return v < 0; })) {
                continue;
            }

            long long collapsedExponent = exponents.last();
            bool ok = true;
            for (int idx = exponents.size() - 2; idx >= 0; --idx) {
                if (!checkedPow(exponents.at(idx), collapsedExponent, &collapsedExponent)) {
                    ok = false;
                    break;
                }
            }
            if (!ok)
                continue;

            text.replace(i + 1, tokenPos - (i + 1), QString::number(collapsedExponent));
        }
        return text;
    };

    QString normalizedExpression = expression;
    if (collapseRightAssociativeIntegerPowerChains)
        normalizedExpression = collapseRightAssociativeIntegerPowerChainsFn(expression);

    static const QRegularExpression powerRE(
        QStringLiteral(R"(\^(?:\(([−-]?)(\d+)\)|(\d+))(?![\p{L}\p{N}\.,_!]))"));
    QString rendered;
    rendered.reserve(normalizedExpression.size());
    int lastPos = 0;
    auto it = powerRE.globalMatch(normalizedExpression);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        rendered += normalizedExpression.mid(lastPos, match.capturedStart() - lastPos);

        const QString sign = match.captured(1);
        QString digits = match.captured(2);
        if (digits.isEmpty())
            digits = match.captured(3);

        QString superscript;
        superscript.reserve(digits.size() + 1);
        if (!sign.isEmpty())
            superscript += MathDsl::PowNeg;
        for (const QChar& digit : digits)
            superscript += MathDsl::asciiDigitToSuperscript(digit);
        rendered += superscript;

        lastPos = match.capturedEnd();
    }
    rendered += normalizedExpression.mid(lastPos);

    auto isIdentifierChar = [](const QChar& ch) {
        return isIdentifierContinue(ch);
    };
    auto isIdentifierStartChar = [](const QChar& ch) {
        return isIdentifierStart(ch);
    };
    auto isSuperscriptPowerChar = [](const QChar& ch) {
        return MathDsl::isSuperscriptPowerChar(ch);
    };

    // Convert function-call powers from "f(x)²" to "f²(x)".
    int i = 0;
    while (i < rendered.size()) {
        if (rendered.at(i) != MathDsl::GroupEnd) {
            ++i;
            continue;
        }

        int superscriptEnd = i + 1;
        while (superscriptEnd < rendered.size() && isSuperscriptPowerChar(rendered.at(superscriptEnd)))
            ++superscriptEnd;
        if (superscriptEnd == i + 1) {
            ++i;
            continue;
        }

        int depth = 0;
        int openParen = -1;
        for (int j = i; j >= 0; --j) {
            const QChar ch = rendered.at(j);
            if (ch == MathDsl::GroupEnd) {
                ++depth;
            } else if (ch == MathDsl::GroupStart) {
                --depth;
                if (depth == 0) {
                    openParen = j;
                    break;
                }
            }
        }
        if (openParen <= 0) {
            i = superscriptEnd;
            continue;
        }

        int nameStart = openParen - 1;
        while (nameStart >= 0 && isIdentifierChar(rendered.at(nameStart)))
            --nameStart;
        ++nameStart;
        if (nameStart >= openParen || !isIdentifierStartChar(rendered.at(nameStart))) {
            i = superscriptEnd;
            continue;
        }

        if (nameStart > 0 && isIdentifierChar(rendered.at(nameStart - 1))) {
            i = superscriptEnd;
            continue;
        }

        const QString funcName = rendered.mid(nameStart, openParen - nameStart);
        const QString callArgs = rendered.mid(openParen, i - openParen + 1);
        const QString superscript = rendered.mid(i + 1, superscriptEnd - (i + 1));
        const QString replacement = funcName + superscript + callArgs;
        rendered.replace(nameStart, superscriptEnd - nameStart, replacement);
        i = nameStart + replacement.size();
    }

    return rendered;
}

static bool isWrappedInOuterParentheses(const QString& text)
{
    if (text.size() < 2 || text.front() != MathDsl::GroupStart || text.back() != MathDsl::GroupEnd)
        return false;

    int depth = 0;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == MathDsl::GroupStart) {
            ++depth;
        } else if (ch == MathDsl::GroupEnd) {
            --depth;
            if (depth < 0)
                return false;
            if (depth == 0 && i != text.size() - 1)
                return false;
        }
    }

    return depth == 0;
}

static bool termNeedsGroupingForDisplay(const Tokens& tokens, int firstIndex, int lastIndex)
{
    if (firstIndex < 0 || lastIndex >= tokens.size() || firstIndex >= lastIndex)
        return false;

    const int additivePrecedence = opPrecedence(Token::Addition);
    int depth = 0;
    for (int i = firstIndex; i <= lastIndex; ++i) {
        const Token& token = tokens.at(i);
        const Token::Operator op = token.asOperator();
        if (op == Token::AssociationStart) {
            ++depth;
            continue;
        }
        if (op == Token::AssociationEnd) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (depth != 0 || !token.isOperator())
            continue;

        const bool hasLeftOperand = i > firstIndex
            && tokenCanEndOperandForDisplaySpacing(tokens.at(i - 1));
        const bool hasRightOperand = i < lastIndex
            && tokenCanStartOperandForDisplaySpacing(tokens.at(i + 1));
        if (!hasLeftOperand || !hasRightOperand)
            continue;

        if (opPrecedence(op) > additivePrecedence) {
            // Additive precedence already disambiguates top-level arithmetic
            // terms, so avoid injecting grouping parentheses for them.
            if (op == Token::Exponentiation
                || op == Token::Multiplication
                || op == Token::Division
                || op == Token::IntegerDivision
                || op == Token::Modulo)
            {
                continue;
            }
            return true;
        }
    }

    return false;
}

static QString groupHighPrecedenceAdditiveTermsForDisplay(const QString& expression,
                                                          const Tokens& tokens)
{
    if (tokens.isEmpty())
        return expression;

    QVector<int> topLevelBinaryAddSubTokenIndices;
    int depth = 0;
    for (int i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens.at(i);
        const Token::Operator op = token.asOperator();
        if (op == Token::AssociationStart) {
            ++depth;
            continue;
        }
        if (op == Token::AssociationEnd) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (depth != 0 || (op != Token::Addition && op != Token::Subtraction))
            continue;

        const bool hasLeftOperand = i > 0
            && tokenCanEndOperandForDisplaySpacing(tokens.at(i - 1));
        const bool hasRightOperand = i + 1 < tokens.size()
            && tokenCanStartOperandForDisplaySpacing(tokens.at(i + 1));
        if (hasLeftOperand && hasRightOperand)
            topLevelBinaryAddSubTokenIndices.append(i);
    }

    if (topLevelBinaryAddSubTokenIndices.isEmpty())
        return expression;

    auto tokenRangeText = [&expression, &tokens](int startIndex, int endIndex) {
        const int startPos = tokens.at(startIndex).pos();
        const int endPos = tokens.at(endIndex).pos() + tokens.at(endIndex).size();
        return expression.mid(startPos, endPos - startPos);
    };

    QString grouped;
    grouped.reserve(expression.size() + topLevelBinaryAddSubTokenIndices.size() * 2);

    int termStartIndex = 0;
    for (int splitPos = 0; splitPos < topLevelBinaryAddSubTokenIndices.size(); ++splitPos) {
        const int operatorIndex = topLevelBinaryAddSubTokenIndices.at(splitPos);
        const int termEndIndex = operatorIndex - 1;
        if (termEndIndex >= termStartIndex) {
            QString termText = tokenRangeText(termStartIndex, termEndIndex);
            if (termNeedsGroupingForDisplay(tokens, termStartIndex, termEndIndex)
                && !isWrappedInOuterParentheses(termText))
            {
                termText = QStringLiteral("(%1)").arg(termText);
            }
            grouped += termText;
        }

        grouped += tokenRangeText(operatorIndex, operatorIndex);
        termStartIndex = operatorIndex + 1;
    }

    if (termStartIndex < tokens.size()) {
        QString lastTermText = tokenRangeText(termStartIndex, tokens.size() - 1);
        if (termNeedsGroupingForDisplay(tokens, termStartIndex, tokens.size() - 1)
            && !isWrappedInOuterParentheses(lastTermText))
        {
            lastTermText = QStringLiteral("(%1)").arg(lastTermText);
        }
        grouped += lastTermText;
    }

    return grouped;
}

static QString simplifyRepeatedMultiplicativeBasesForDisplay(const QString& expression,
                                                             const Tokens& tokens);
static bool approximateRationalFromDouble(double value, int maxDenominator,
                                          qint64* outNumerator, qint64* outDenominator);

static bool parseSimplifiablePowerFactor(const QString& factorText,
                                         QString* baseOut,
                                         int* exponentOut)
{
    static const QRegularExpression identifierRE(
        QStringLiteral(R"(^(?:[\p{L}_][\p{L}\p{N}_]*)$)"));
    static const QRegularExpression numberRE(
        QStringLiteral(R"(^(?:\d+(?:\.\d+)?)$)"));

    auto isFunctionCallBase = [](const QString& text) {
        const int firstParen = text.indexOf(MathDsl::GroupStart);
        if (firstParen <= 0 || !text.endsWith(MathDsl::GroupEnd))
            return false;

        const QString functionName = text.left(firstParen);
        if (!identifierRE.match(functionName).hasMatch())
            return false;

        int depth = 0;
        for (int i = firstParen; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (ch == MathDsl::GroupStart) {
                ++depth;
            } else if (ch == MathDsl::GroupEnd) {
                --depth;
                if (depth < 0)
                    return false;
                if (depth == 0 && i != text.size() - 1)
                    return false;
            }
        }
        return depth == 0;
    };

    auto isSimplifiableBase = [&isFunctionCallBase](const QString& text) {
        return identifierRE.match(text).hasMatch()
            || numberRE.match(text).hasMatch()
            || isFunctionCallBase(text);
    };

    const QString factor = factorText.trimmed();
    QString base = factor;
    int exponent = 1;

    int depth = 0;
    int topLevelPowerPos = -1;
    for (int i = 0; i < factor.size(); ++i) {
        const QChar ch = factor.at(i);
        if (ch == MathDsl::GroupStart) {
            ++depth;
        } else if (ch == MathDsl::GroupEnd) {
            if (depth > 0)
                --depth;
        } else if (ch == MathDsl::PowOp && depth == 0) {
            topLevelPowerPos = i;
            break;
        }
    }

    if (topLevelPowerPos >= 0) {
        base = factor.left(topLevelPowerPos);
        QString exponentText = factor.mid(topLevelPowerPos + 1);
        if (base.isEmpty() || exponentText.isEmpty())
            return false;

        if (exponentText.startsWith(MathDsl::GroupStart)
            && exponentText.endsWith(MathDsl::GroupEnd))
        {
            exponentText = exponentText.mid(1, exponentText.size() - 2);
        }

        if (!normalizeUnsignedIntegerEquivalentDecimalText(exponentText)
            && !isUnsignedDecimalIntegerText(exponentText))
        {
            return false;
        }

        bool ok = false;
        exponent = exponentText.toInt(&ok);
        if (!ok || exponent <= 0)
            return false;
    }

    base = base.trimmed();
    if (!isSimplifiableBase(base))
        return false;

    *baseOut = base;
    *exponentOut = exponent;
    return true;
}

static bool approximateRationalFromDouble(double value, int maxDenominator,
                                          qint64* outNumerator, qint64* outDenominator)
{
    if (!outNumerator || !outDenominator || maxDenominator <= 0 || !std::isfinite(value))
        return false;

    const double absValue = std::abs(value);
    qint64 n0 = 0, d0 = 1;
    qint64 n1 = 1, d1 = 0;
    double x = absValue;

    for (int iter = 0; iter < 64; ++iter) {
        const qint64 a = static_cast<qint64>(std::floor(x));
        const qint64 n2 = a * n1 + n0;
        const qint64 d2 = a * d1 + d0;
        if (d2 > maxDenominator || d2 <= 0)
            break;

        n0 = n1; d0 = d1;
        n1 = n2; d1 = d2;

        const double frac = x - static_cast<double>(a);
        if (std::abs(frac) < 1e-15)
            break;
        x = 1.0 / frac;
    }

    if (d1 <= 0)
        return false;

    const qint64 signedNumerator = (value < 0.0) ? -n1 : n1;
    const double reconstructed = static_cast<double>(signedNumerator) / static_cast<double>(d1);
    if (std::abs(reconstructed - value) > 1e-12)
        return false;

    *outNumerator = signedNumerator;
    *outDenominator = d1;
    return true;
}

static QString simplifyRepeatedBasesInMultiplicativeTermForDisplay(const QString& termText)
{
    if (termText.isEmpty())
        return termText;

    QString workingTerm = termText;
    int negativeUnitFactorCount = 0;
    {
        static const QRegularExpression s_leadingNegativeUnitMultiplier(
            QStringLiteral(R"(^\s*[−-]1\s*(?:\*|·|×)\s*(.+)$)"));
        const QRegularExpressionMatch match = s_leadingNegativeUnitMultiplier.match(workingTerm);
        if (match.hasMatch()) {
            workingTerm = match.captured(1);
            ++negativeUnitFactorCount;
        }

        static const QRegularExpression s_trailingDivisionNegativeOne(
            QStringLiteral(R"(\s*(?:/|÷|⧸)\s*(?:[−-]1|\([−-]1\))\s*$)"));
        static const QRegularExpression s_trailingMultiplicationNegativeOne(
            QStringLiteral(R"(\s*(?:\*|·|×)\s*(?:[−-]1|\([−-]1\))\s*$)"));
        static const QRegularExpression s_trailingNeutralOne(
            QStringLiteral(R"(\s*(?:/|÷|⧸|\*|·|×)\s*(?:\+?1|\(\+?1\))\s*$)"));
        while (true) {
            QString updated = workingTerm;
            updated.replace(s_trailingDivisionNegativeOne, QString());
            if (updated != workingTerm) {
                workingTerm = updated;
                ++negativeUnitFactorCount;
                continue;
            }

            updated = workingTerm;
            updated.replace(s_trailingMultiplicationNegativeOne, QString());
            if (updated != workingTerm) {
                workingTerm = updated;
                ++negativeUnitFactorCount;
                continue;
            }

            updated = workingTerm;
            updated.replace(s_trailingNeutralOne, QString());
            if (updated == workingTerm)
                break;
            workingTerm = updated;
        }
    }

    auto simplifyMultiplicationOnlySegment = [](const QString& segmentText) {
        if (segmentText.isEmpty())
            return segmentText;

        QVector<QString> factors;
        QVector<QString> operators;
        int depth = 0;
        int partStart = 0;
        for (int i = 0; i < segmentText.size(); ++i) {
            const QChar ch = segmentText.at(i);
            if (ch == MathDsl::GroupStart) {
                ++depth;
                continue;
            }
            if (ch == MathDsl::GroupEnd) {
                if (depth > 0)
                    --depth;
                continue;
            }
            if (depth == 0
                && (ch == MathDsl::MulOpAl1
                    || ch == MathDsl::MulDotOp
                    || ch == MathDsl::MulCrossOp))
            {
                factors.append(segmentText.mid(partStart, i - partStart));
                operators.append(QString(ch));
                partStart = i + 1;
            }
        }
        factors.append(segmentText.mid(partStart));

        if (factors.size() < 2)
            return segmentText;

        QHash<QString, int> totalExponentsByBase;
        for (int i = 0; i < factors.size(); ++i) {
            QString base;
            int exponent = 0;
            if (!parseSimplifiablePowerFactor(factors.at(i), &base, &exponent)) {
                continue;
            }
            totalExponentsByBase[base] += exponent;
        }

        QString simplified;
        QSet<QString> emittedBases;
        bool hasOutput = false;
        int i = 0;
        while (i < factors.size()) {
            QString base;
            int exponent = 0;
            const bool isSimplifiableFactor =
                parseSimplifiablePowerFactor(factors.at(i), &base, &exponent);

            if (!isSimplifiableFactor) {
                if (hasOutput && i > 0)
                    simplified += operators.at(i - 1);
                simplified += factors.at(i);
                hasOutput = true;
                ++i;
                continue;
            }

            if (emittedBases.contains(base)) {
                ++i;
                continue;
            }
            emittedBases.insert(base);

            if (hasOutput && i > 0)
                simplified += operators.at(i - 1);
            const int totalExponent = totalExponentsByBase.value(base, 0);
            if (totalExponent > 1) {
                simplified += base;
                simplified += QStringLiteral("^%1").arg(totalExponent);
            } else {
                simplified += factors.at(i);
            }
            hasOutput = true;
            ++i;
        }

        QVector<QString> finalFactors;
        QVector<QString> finalOperators;
        int finalDepth = 0;
        int finalStart = 0;
        for (int i = 0; i < simplified.size(); ++i) {
            const QChar ch = simplified.at(i);
            if (ch == MathDsl::GroupStart) {
                ++finalDepth;
                continue;
            }
            if (ch == MathDsl::GroupEnd) {
                if (finalDepth > 0)
                    --finalDepth;
                continue;
            }
            if (finalDepth == 0
                && (ch == MathDsl::MulOpAl1
                    || ch == MathDsl::MulDotOp
                    || ch == MathDsl::MulCrossOp))
            {
                finalFactors.append(simplified.mid(finalStart, i - finalStart));
                finalOperators.append(QString(ch));
                finalStart = i + 1;
            }
        }
        finalFactors.append(simplified.mid(finalStart));

        int firstNumericIndex = -1;
        double numericProduct = 1.0;
        int numericCount = 0;
        bool hasSignedNumericFactor = false;
        auto isParenthesizedSignedDecimalFactor = [](const QString& factorText) {
            QString text = factorText.trimmed();
            if (!(text.startsWith(MathDsl::GroupStart) && text.endsWith(MathDsl::GroupEnd)))
                return false;
            text = text.mid(1, text.size() - 2).trimmed();
            return isSignedDecimalNumberText(text) && !isUnsignedDecimalNumberText(text);
        };
        for (int i = 0; i < finalFactors.size(); ++i) {
            const QString factor = finalFactors.at(i);
            if (isSignedDecimalNumberText(factor) && !isUnsignedDecimalNumberText(factor))
                hasSignedNumericFactor = true;
            if (isParenthesizedSignedDecimalFactor(factor))
                hasSignedNumericFactor = true;
            if (!isUnsignedDecimalNumberText(finalFactors.at(i))
                || !isSafeForDoubleArithmetic(finalFactors.at(i)))
                continue;
            if (firstNumericIndex < 0)
                firstNumericIndex = i;
            bool ok = false;
            const double value = finalFactors.at(i).toDouble(&ok);
            if (!ok)
                continue;
            numericProduct *= value;
            ++numericCount;
        }
        if (hasSignedNumericFactor)
            return simplified;
        if (numericCount > 1) {
            finalFactors[firstNumericIndex] = formatSimplifiedDecimal(numericProduct);
            for (int i = finalFactors.size() - 1; i >= 0; --i) {
                if (i == firstNumericIndex
                    || !isUnsignedDecimalNumberText(finalFactors.at(i))
                    || !isSafeForDoubleArithmetic(finalFactors.at(i)))
                    continue;
                finalFactors.removeAt(i);
                finalOperators.removeAt(i - 1);
            }
            QString rebuiltNumeric = finalFactors.at(0);
            for (int i = 1; i < finalFactors.size(); ++i) {
                rebuiltNumeric += finalOperators.at(i - 1);
                rebuiltNumeric += finalFactors.at(i);
            }
            return rebuiltNumeric;
        }

        return simplified;
    };
    auto simplifyPostDivisionSegment = [&simplifyMultiplicationOnlySegment](const QString& segmentText) {
        if (segmentText.isEmpty())
            return segmentText;

        QVector<QString> factors;
        QVector<QString> operators;
        int depth = 0;
        int partStart = 0;
        for (int i = 0; i < segmentText.size(); ++i) {
            const QChar ch = segmentText.at(i);
            if (ch == MathDsl::GroupStart) {
                ++depth;
                continue;
            }
            if (ch == MathDsl::GroupEnd) {
                if (depth > 0)
                    --depth;
                continue;
            }
            if (depth == 0
                && (ch == MathDsl::MulOpAl1
                    || ch == MathDsl::MulDotOp
                    || ch == MathDsl::MulCrossOp))
            {
                factors.append(segmentText.mid(partStart, i - partStart));
                operators.append(QString(ch));
                partStart = i + 1;
            }
        }
        factors.append(segmentText.mid(partStart));

        if (factors.size() <= 1)
            return segmentText;

        QString tail;
        tail += factors.at(1);
        for (int i = 2; i < factors.size(); ++i) {
            tail += operators.at(i - 1);
            tail += factors.at(i);
        }
        tail = simplifyMultiplicationOnlySegment(tail);

        QString rebuilt = factors.at(0);
        rebuilt += operators.at(0);
        rebuilt += tail;
        return rebuilt;
    };

    QVector<QString> divisionSegments;
    QVector<QString> divisionOperators;
    int depth = 0;
    int segmentStart = 0;
    for (int i = 0; i < workingTerm.size(); ++i) {
        const QChar ch = workingTerm.at(i);
        if (ch == MathDsl::GroupStart) {
            ++depth;
            continue;
        }
        if (ch == MathDsl::GroupEnd) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (depth == 0 && (ch == MathDsl::DivOp || ch == MathDsl::IntDivOp)) {
            divisionSegments.append(workingTerm.mid(segmentStart, i - segmentStart));
            divisionOperators.append(QString(ch));
            segmentStart = i + 1;
        }
    }
    divisionSegments.append(workingTerm.mid(segmentStart));

    auto foldNumericDenominatorIntoPreviousSegment =
        [](QString* previousSegment, const QString& denominatorText) {
            if (!isUnsignedDecimalNumberText(denominatorText)
                || !isSafeForDoubleArithmetic(denominatorText))
                return false;

            bool okDen = false;
            const double den = denominatorText.toDouble(&okDen);
            if (!okDen || den == 0.0)
                return false;

            QString prev = *previousSegment;
            QVector<QString> factors;
            QVector<QString> operators;
            int depth = 0;
            int partStart = 0;
            for (int i = 0; i < prev.size(); ++i) {
                const QChar ch = prev.at(i);
                if (ch == MathDsl::GroupStart) {
                    ++depth;
                    continue;
                }
                if (ch == MathDsl::GroupEnd) {
                    if (depth > 0)
                        --depth;
                    continue;
                }
                if (depth == 0
                    && (ch == MathDsl::MulOpAl1
                        || ch == MathDsl::MulDotOp
                        || ch == MathDsl::MulCrossOp))
                {
                    factors.append(prev.mid(partStart, i - partStart));
                    operators.append(QString(ch));
                    partStart = i + 1;
                }
            }
            factors.append(prev.mid(partStart));

            if (factors.size() <= 1)
                return false;

            for (int i = 1; i < factors.size(); ++i) {
                if (!isUnsignedDecimalNumberText(factors.at(i))
                    || !isSafeForDoubleArithmetic(factors.at(i)))
                    continue;

                bool okNum = false;
                const double num = factors.at(i).toDouble(&okNum);
                if (!okNum)
                    continue;

                factors[i] = formatSimplifiedDecimal(num / den);
                QString rebuilt = factors.at(0);
                for (int j = 1; j < factors.size(); ++j) {
                    rebuilt += operators.at(j - 1);
                    rebuilt += factors.at(j);
                }
                *previousSegment = rebuilt;
                return true;
            }

            return false;
        };

    QString rebuilt;
    if (divisionOperators.isEmpty()) {
        QString simplified = simplifyMultiplicationOnlySegment(workingTerm);
        if ((negativeUnitFactorCount % 2) != 0 && !simplified.isEmpty()) {
            if (simplified == QLatin1String("1"))
                return QStringLiteral("-1");
            return QStringLiteral("-") + simplified;
        }
        return simplified;
    } else {
        QVector<QString> simplifiedSegments;
        simplifiedSegments.reserve(divisionSegments.size());
        for (int i = 0; i < divisionSegments.size(); ++i) {
            const bool followsDivisionOperator =
                i > 0 && (divisionOperators.at(i - 1) == QLatin1String("/")
                          || divisionOperators.at(i - 1) == QLatin1String("\\"));
            simplifiedSegments.append(followsDivisionOperator
                ? simplifyPostDivisionSegment(divisionSegments.at(i))
                : simplifyMultiplicationOnlySegment(divisionSegments.at(i)));
        }

        for (int i = 1; i < simplifiedSegments.size(); ++i) {
            if (divisionOperators.at(i - 1) != QLatin1String("/"))
                continue;
            if (i <= 1 || divisionOperators.at(i - 2) != QLatin1String("/"))
                continue;
            QString previous = simplifiedSegments.at(i - 1);
            if (!foldNumericDenominatorIntoPreviousSegment(&previous, simplifiedSegments.at(i)))
                continue;
            simplifiedSegments[i - 1] = previous;
            simplifiedSegments.removeAt(i);
            divisionOperators.removeAt(i - 1);
            --i;
        }

        for (int i = 0; i < simplifiedSegments.size(); ++i) {
            rebuilt += simplifiedSegments.at(i);
            if (i < divisionOperators.size())
                rebuilt += divisionOperators.at(i);
        }
    }

    auto simplifyParenthesizedDenominators = [&simplifyMultiplicationOnlySegment](const QString& text) {
        QString out;
        out.reserve(text.size());

        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (ch != MathDsl::DivOp || i + 1 >= text.size() || text.at(i + 1) != MathDsl::GroupStart) {
                out += ch;
                continue;
            }

            int depth = 0;
            int closePos = -1;
            for (int j = i + 1; j < text.size(); ++j) {
                const QChar inner = text.at(j);
                if (inner == MathDsl::GroupStart) {
                    ++depth;
                } else if (inner == MathDsl::GroupEnd) {
                    --depth;
                    if (depth == 0) {
                        closePos = j;
                        break;
                    }
                }
            }

            if (closePos < 0) {
                out += ch;
                continue;
            }

            const QString inside = text.mid(i + 2, closePos - (i + 2));
            const Tokens insideTokens = Evaluator::instance()->scan(inside);
            QString simplifiedInside = inside;
            if (!insideTokens.isEmpty())
                simplifiedInside = simplifyRepeatedMultiplicativeBasesForDisplay(inside, insideTokens);
            else
                simplifiedInside = simplifyMultiplicationOnlySegment(inside);

            QString base;
            int exponent = 0;
            if (parseSimplifiablePowerFactor(simplifiedInside, &base, &exponent)) {
                out += MathDsl::DivOp;
                out += simplifiedInside;
            } else {
                out += MathDsl::DivOp;
                out += MathDsl::GroupStart;
                out += simplifiedInside;
                out += MathDsl::GroupEnd;
            }

            i = closePos;
        }

        return out;
    };
    rebuilt = simplifyParenthesizedDenominators(rebuilt);

    auto simplifyAlgebraicMulDivChain = [&negativeUnitFactorCount](const QString& text) {
        if (RegExpPatterns::simpleParenthesizedDenominatorQuotient().match(text).hasMatch())
            return text;

        auto isFullyWrappedByOuterParens = [](const QString& expr) {
            if (expr.size() < 2 || expr.at(0) != MathDsl::GroupStart || expr.at(expr.size() - 1) != MathDsl::GroupEnd)
                return false;
            int depth = 0;
            for (int i = 0; i < expr.size(); ++i) {
                const QChar ch = expr.at(i);
                if (ch == MathDsl::GroupStart)
                    ++depth;
                else if (ch == MathDsl::GroupEnd)
                    --depth;
                if (depth == 0 && i < expr.size() - 1)
                    return false;
            }
            return depth == 0;
        };

        auto normalizeSimplifiableSignedFactor = [&isFullyWrappedByOuterParens](const QString& factor,
                                                                                 QString* unsignedFactor,
                                                                                 int* signOut) {
            QString normalized = factor.trimmed();
            while (isFullyWrappedByOuterParens(normalized))
                normalized = normalized.mid(1, normalized.size() - 2).trimmed();

            int sign = 1;
            while (!normalized.isEmpty()) {
                const QChar ch = normalized.at(0);
                if (ch == MathDsl::AddOp) {
                    normalized = normalized.mid(1).trimmed();
                    continue;
                }
                if (isMinus(ch)) {
                    sign = -sign;
                    normalized = normalized.mid(1).trimmed();
                    continue;
                }
                break;
            }

            while (isFullyWrappedByOuterParens(normalized))
                normalized = normalized.mid(1, normalized.size() - 2).trimmed();
            if (normalized.isEmpty())
                return false;

            *unsignedFactor = normalized;
            *signOut = sign;
            return true;
        };
        auto tryFoldParenthesizedNumericAddSubFactor = [](const QString& factor,
                                                          QString* foldedUnsignedFactor) {
            QString normalized = factor.trimmed();
            QString inner = normalized;
            if (normalized.startsWith(MathDsl::GroupStart) && normalized.endsWith(MathDsl::GroupEnd))
                inner = normalized.mid(1, normalized.size() - 2).trimmed();
            if (inner.isEmpty())
                return false;

            const Tokens innerTokens = Evaluator::instance()->scan(inner);
            if (innerTokens.isEmpty())
                return false;

            double total = 0.0;
            QString pendingOp = QStringLiteral("+");
            bool expectValue = true;
            for (const Token& token : innerTokens) {
                if (expectValue) {
                    if (!token.isNumber())
                        return false;
                    const QString numberText = token.text().trimmed();
                    if (!isSignedDecimalNumberText(numberText)
                        || !isSafeForDoubleArithmetic(numberText))
                    {
                        return false;
                    }
                    bool ok = false;
                    const double value = numberText.toDouble(&ok);
                    if (!ok)
                        return false;
                    total += (pendingOp == QLatin1String("-")) ? -value : value;
                    expectValue = false;
                    continue;
                }

                const Token::Operator op = token.asOperator();
                if (op != Token::Addition && op != Token::Subtraction)
                    return false;
                pendingOp = token.text();
                expectValue = true;
            }

            if (expectValue)
                return false;

            *foldedUnsignedFactor = formatSimplifiedDecimal(total);
            return true;
        };
        auto splitTopLevelMulDivChain = [](const QString& expr,
                                           QVector<QString>* outFactors,
                                           QVector<QString>* outOperators) {
            outFactors->clear();
            outOperators->clear();

            int depth = 0;
            int partStart = 0;
            for (int i = 0; i < expr.size(); ++i) {
                const QChar ch = expr.at(i);
                if (ch == MathDsl::GroupStart) {
                    ++depth;
                    continue;
                }
                if (ch == MathDsl::GroupEnd) {
                    if (depth > 0)
                        --depth;
                    continue;
                }
                if (depth == 0
                    && (ch == MathDsl::MulOpAl1
                        || ch == MathDsl::MulDotOp
                        || ch == MathDsl::MulCrossOp
                        || ch == MathDsl::DivOp
                        || ch == UnicodeChars::DivisionSign // ÷
                        || ch == UnicodeChars::BigSolidus // ⧸
                        || ch == UnicodeChars::DivisionSlash // ∕
                        || ch == MathDsl::IntDivOp))
                {
                    outFactors->append(expr.mid(partStart, i - partStart));
                    outOperators->append(QString(ch));
                    partStart = i + 1;
                } else if (depth == 0
                           && (ch == MathDsl::AddOp
                               || ch == MathDsl::SubOpAl1
                               || ch == MathDsl::SubOp))
                {
                    return false;
                }
            }
            outFactors->append(expr.mid(partStart));
            return outFactors->size() > 1;
        };

        QVector<QString> factors;
        QVector<QString> operators;
        int depth = 0;
        int partStart = 0;
        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (ch == MathDsl::GroupStart) {
                ++depth;
                continue;
            }
            if (ch == MathDsl::GroupEnd) {
                if (depth > 0)
                    --depth;
                continue;
            }
            if (depth == 0
                && (ch == MathDsl::MulOpAl1
                    || ch == MathDsl::MulDotOp
                    || ch == MathDsl::MulCrossOp
                    || ch == MathDsl::DivOp))
            {
                factors.append(text.mid(partStart, i - partStart));
                operators.append(QString(ch));
                partStart = i + 1;
            } else if (depth == 0 && ch == MathDsl::IntDivOp) {
                return text;
            }
        }
        factors.append(text.mid(partStart));
        if (factors.size() < 2)
            return text;

        QHash<QString, int> exponentByBase;
        QVector<QString> baseOrder;
        double numericCoefficient = 1.0;
        bool hasNonNumericBase = false;
        bool hasOnlyUnitBases = true;
        auto combineMulDivOps = [](const QString& outerOp, const QString& innerOp) {
            auto isDivisionOp = [](const QString& opText) {
                return opText == QLatin1String("/")
                    || opText == QLatin1String("\\")
                    || opText == QString(UnicodeChars::DivisionSign) // ÷
                    || opText == QString(UnicodeChars::BigSolidus) // ⧸
                    || opText == QString(UnicodeChars::DivisionSlash); // ∕
            };
            const bool outerIsDiv =
                isDivisionOp(outerOp);
            const bool innerIsDiv =
                isDivisionOp(innerOp);
            if (outerIsDiv)
                return innerIsDiv ? QStringLiteral("*") : QStringLiteral("/");
            return innerOp;
        };
        auto isUnitOnlyBase = [](const QString& baseText) {
            const Tokens baseTokens = Evaluator::instance()->scan(baseText);
            if (!baseTokens.valid() || baseTokens.isEmpty())
                return false;
            bool hasUnitIdentifier = false;
            for (const Token& token : baseTokens) {
                if (token.isUnitIdentifier()) {
                    hasUnitIdentifier = true;
                    continue;
                }
                if (token.isNumber())
                    continue;
                const Token::Operator op = token.asOperator();
                if (op == Token::AssociationStart
                    || op == Token::AssociationEnd
                    || op == Token::Exponentiation
                    || op == Token::Multiplication
                    || op == Token::Division
                    || op == Token::Addition
                    || op == Token::Subtraction)
                {
                    continue;
                }
                return false;
            }
            return hasUnitIdentifier;
        };

        for (int i = 0; i < factors.size(); ++i) {
            QString unsignedFactor;
            int factorSign = 1;
            if (!normalizeSimplifiableSignedFactor(factors.at(i), &unsignedFactor, &factorSign))
                return text;

            QString foldedNumericFactor;
            if (tryFoldParenthesizedNumericAddSubFactor(unsignedFactor, &foldedNumericFactor))
                unsignedFactor = foldedNumericFactor;

            if (factorSign < 0)
                ++negativeUnitFactorCount;

            const QString op = (i == 0) ? QStringLiteral("*") : operators.at(i - 1);
            QVector<QString> chainFactors;
            QVector<QString> chainOperators;
            if (!splitTopLevelMulDivChain(unsignedFactor, &chainFactors, &chainOperators)) {
                chainFactors = { unsignedFactor };
            }

            for (int j = 0; j < chainFactors.size(); ++j) {
                QString chainFactor = chainFactors.at(j).trimmed();
                QString foldedChainFactor;
                if (tryFoldParenthesizedNumericAddSubFactor(chainFactor, &foldedChainFactor))
                    chainFactor = foldedChainFactor;

                QString base;
                int exponent = 0;
                if (!parseSimplifiablePowerFactor(chainFactor, &base, &exponent))
                    return text;

                const QString innerOp = (j == 0) ? QStringLiteral("*") : chainOperators.at(j - 1);
                const QString effectiveOp = combineMulDivOps(op, innerOp);
                const int sign = (effectiveOp == QLatin1String("/")) ? -1 : 1;
                if (isUnsignedDecimalNumberText(base) && isSafeForDoubleArithmetic(base)) {
                    bool ok = false;
                    const double value = base.toDouble(&ok);
                    if (!ok)
                        return text;
                    numericCoefficient *= std::pow(value, sign * exponent);
                } else if (!isUnsignedDecimalNumberText(base)) {
                    if (!exponentByBase.contains(base))
                        baseOrder.append(base);
                    exponentByBase[base] += sign * exponent;
                    hasNonNumericBase = true;
                    if (hasOnlyUnitBases && !isUnitOnlyBase(base))
                        hasOnlyUnitBases = false;
                } else {
                    return text;
                }
            }
        }

        if (!hasNonNumericBase)
            return formatSimplifiedDecimal(numericCoefficient);

        QStringList positiveFactors;
        QStringList negativeFactors;
        for (const QString& base : baseOrder) {
            const int exponent = exponentByBase.value(base, 0);
            if (exponent > 0) {
                positiveFactors.append(exponent == 1
                    ? base
                    : QStringLiteral("%1^%2").arg(base).arg(exponent));
            } else if (exponent < 0) {
                const int denExp = -exponent;
                negativeFactors.append(denExp == 1
                    ? base
                    : QStringLiteral("%1^%2").arg(base).arg(denExp));
            }
        }

        QString result;
        const bool coefficientIsOne = std::abs(numericCoefficient - 1.0) < 1e-12;
        const bool shouldShowUnitCoefficientOne =
            coefficientIsOne
            && hasOnlyUnitBases
            && (!positiveFactors.isEmpty() || !negativeFactors.isEmpty());
        if (!coefficientIsOne
            || shouldShowUnitCoefficientOne
            || (positiveFactors.isEmpty() && !negativeFactors.isEmpty())) {
            QString coefficientText = formatSimplifiedDecimal(numericCoefficient);
            qint64 num = 0;
            qint64 den = 0;
            if (approximateRationalFromDouble(numericCoefficient, 1000000, &num, &den)
                && den != 1)
            {
                coefficientText = QStringLiteral("%1/%2").arg(num).arg(den);
            }
            result += coefficientText;
        }
        if (!positiveFactors.isEmpty()) {
            if (!result.isEmpty())
                result += MathDsl::MulOpAl1;
            result += positiveFactors.join(QString(MathDsl::MulOpAl1));
        }
        for (const QString& factor : negativeFactors) {
            if (result.isEmpty())
                result = QStringLiteral("1");
            result += MathDsl::DivOp;
            result += factor;
        }

        return result.isEmpty() ? text : result;
    };

    QString simplified = simplifyAlgebraicMulDivChain(rebuilt);
    if ((negativeUnitFactorCount % 2) != 0 && !simplified.isEmpty()) {
        if (simplified == QLatin1String("1"))
            return QStringLiteral("-1");
        return QStringLiteral("-") + simplified;
    }
    return simplified;

}

static QString simplifyRepeatedMultiplicativeBasesForDisplay(const QString& expression,
                                                             const Tokens& tokens)
{
    if (tokens.isEmpty())
        return expression;

    QVector<int> topLevelBinaryAddSubTokenIndices;
    int depth = 0;
    for (int i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens.at(i);
        const Token::Operator op = token.asOperator();
        if (op == Token::AssociationStart) {
            ++depth;
            continue;
        }
        if (op == Token::AssociationEnd) {
            if (depth > 0)
                --depth;
            continue;
        }
        if (depth != 0 || (op != Token::Addition && op != Token::Subtraction))
            continue;

        const bool hasLeftOperand = i > 0
            && tokenCanEndOperandForDisplaySpacing(tokens.at(i - 1));
        const bool hasRightOperand = i + 1 < tokens.size()
            && tokenCanStartOperandForDisplaySpacing(tokens.at(i + 1));
        if (hasLeftOperand && hasRightOperand)
            topLevelBinaryAddSubTokenIndices.append(i);
    }

    auto tokenRangeText = [&expression, &tokens](int startIndex, int endIndex) {
        const int startPos = tokens.at(startIndex).pos();
        const int endPos = tokens.at(endIndex).pos() + tokens.at(endIndex).size();
        return expression.mid(startPos, endPos - startPos);
    };

    if (topLevelBinaryAddSubTokenIndices.isEmpty())
        return simplifyRepeatedBasesInMultiplicativeTermForDisplay(expression);

    QVector<QString> simplifiedTerms;
    QVector<QString> additiveOperators;
    int termStartIndex = 0;
    for (int splitPos = 0; splitPos < topLevelBinaryAddSubTokenIndices.size(); ++splitPos) {
        const int operatorIndex = topLevelBinaryAddSubTokenIndices.at(splitPos);
        const int termEndIndex = operatorIndex - 1;
        if (termEndIndex >= termStartIndex) {
            const QString termText = tokenRangeText(termStartIndex, termEndIndex);
            simplifiedTerms.append(simplifyRepeatedBasesInMultiplicativeTermForDisplay(termText));
        }
        additiveOperators.append(tokenRangeText(operatorIndex, operatorIndex));
        termStartIndex = operatorIndex + 1;
    }

    if (termStartIndex < tokens.size()) {
        const QString lastTermText = tokenRangeText(termStartIndex, tokens.size() - 1);
        simplifiedTerms.append(simplifyRepeatedBasesInMultiplicativeTermForDisplay(lastTermText));
    }

    const auto isMinusOperator = [](const QString& op) {
        return op.size() == 1 && isMinus(op.at(0));
    };
    const auto normalizeFoldableNumericTerm = [&isMinusOperator](const QString& termText, double* out) {
        QString term = termText.trimmed();
        if (isSignedDecimalNumberText(term) && isSafeForDoubleArithmetic(term)) {
            bool ok = false;
            const double value = term.toDouble(&ok);
            if (ok) {
                *out = value;
                return true;
            }
        }
        if (!(term.startsWith(MathDsl::GroupStart) && term.endsWith(MathDsl::GroupEnd)))
            return false;

        const QString inner = term.mid(1, term.size() - 2).trimmed();
        const Tokens innerTokens = Evaluator::instance()->scan(inner);
        if (innerTokens.isEmpty())
            return false;

        double total = 0.0;
        QString pendingOp = QStringLiteral("+");
        bool expectValue = true;
        for (const Token& token : innerTokens) {
            if (expectValue) {
                if (!token.isNumber())
                    return false;
                const QString numberText = token.text().trimmed();
                if (!isSignedDecimalNumberText(numberText)
                    || !isSafeForDoubleArithmetic(numberText))
                {
                    return false;
                }
                bool ok = false;
                const double value = numberText.toDouble(&ok);
                if (!ok)
                    return false;
                total += isMinusOperator(pendingOp) ? -value : value;
                expectValue = false;
                continue;
            }

            const Token::Operator op = token.asOperator();
            if (op != Token::Addition && op != Token::Subtraction)
                return false;
            pendingOp = token.text();
            expectValue = true;
        }

        if (expectValue)
            return false;

        *out = total;
        return true;
    };
    const bool hasPercentTerm = std::any_of(
        simplifiedTerms.constBegin(),
        simplifiedTerms.constEnd(),
        [](const QString& term) { return term.contains(MathDsl::PercentOp); });
    if (hasPercentTerm) {
        if (simplifiedTerms.isEmpty())
            return expression;

        QVector<QString> normalizedTerms = simplifiedTerms;
        for (int i = 0; i < normalizedTerms.size(); ++i) {
            double value = 0.0;
            QString term = normalizedTerms.at(i).trimmed();
            if (term.contains(MathDsl::PercentOp))
                continue;
            if (isWrappedInOuterParentheses(term)) {
                const QString inner = term.mid(1, term.size() - 2);
                const Tokens innerTokens = Evaluator::instance()->scan(inner);
                if (!innerTokens.isEmpty()) {
                    const QString simplifiedInner =
                        simplifyRepeatedMultiplicativeBasesForDisplay(inner, innerTokens);
                    term = QStringLiteral("(%1)").arg(simplifiedInner);
                }
            }
            if (normalizeFoldableNumericTerm(term, &value))
                term = formatSimplifiedDecimal(value);
            normalizedTerms[i] = term;
        }

        QVector<QString> foldedTerms;
        QVector<QString> foldedOperators;
        QString currentTerm = normalizedTerms.at(0).trimmed();

        for (int i = 1; i < normalizedTerms.size(); ++i) {
            const QString op = additiveOperators.at(i - 1);
            const QString nextTerm = normalizedTerms.at(i).trimmed();

            double currentValue = 0.0;
            double nextValue = 0.0;
            const bool canFoldCurrent =
                !currentTerm.contains(MathDsl::PercentOp)
                && normalizeFoldableNumericTerm(currentTerm, &currentValue);
            const bool canFoldNext =
                !nextTerm.contains(MathDsl::PercentOp)
                && normalizeFoldableNumericTerm(nextTerm, &nextValue);

            if ((op == QLatin1String("+") || isMinusOperator(op))
                && canFoldCurrent
                && canFoldNext)
            {
                const double result = currentValue + (isMinusOperator(op) ? -nextValue : nextValue);
                currentTerm = formatSimplifiedDecimal(result);
                continue;
            }

            foldedTerms.append(currentTerm);
            foldedOperators.append(op);
            currentTerm = nextTerm;
        }
        foldedTerms.append(currentTerm);

        QString simplifiedPercentAware;
        simplifiedPercentAware += foldedTerms.at(0);
        for (int i = 1; i < foldedTerms.size(); ++i) {
            simplifiedPercentAware += foldedOperators.at(i - 1);
            simplifiedPercentAware += foldedTerms.at(i);
        }
        return simplifiedPercentAware;
    }

    QVector<double> constantValues;
    QVector<QString> constantSigns;
    QVector<QString> keptTerms;
    QVector<QString> keptOperators;
    for (int i = 0; i < simplifiedTerms.size(); ++i) {
        QString term = simplifiedTerms.at(i).trimmed();
        const QString op = (i == 0) ? QStringLiteral("+") : additiveOperators.at(i - 1);
        if (isSignedDecimalNumberText(term) && isSafeForDoubleArithmetic(term)) {
            bool ok = false;
            const double value = term.toDouble(&ok);
            if (ok) {
                constantValues.append(value);
                constantSigns.append(op);
                continue;
            }
        }

        if (keptTerms.isEmpty()) {
            if (isMinusOperator(op))
                keptTerms.append(QStringLiteral("-") + term);
            else
                keptTerms.append(term);
        } else {
            keptOperators.append(op);
            keptTerms.append(term);
        }
    }

    double foldedConstant = 0.0;
    for (int i = 0; i < constantValues.size(); ++i) {
        const QString op = constantSigns.at(i);
        foldedConstant += isMinusOperator(op) ? -constantValues.at(i) : constantValues.at(i);
    }

    if (!constantValues.isEmpty()) {
        if (keptTerms.isEmpty()) {
            keptTerms.append(formatSimplifiedDecimal(foldedConstant));
        } else if (foldedConstant > 0.0) {
            keptOperators.append(QStringLiteral("+"));
            keptTerms.append(formatSimplifiedDecimal(foldedConstant));
        } else if (foldedConstant < 0.0) {
            keptOperators.append(QStringLiteral("-"));
            keptTerms.append(formatSimplifiedDecimal(-foldedConstant));
        }
    }

    if (!keptTerms.isEmpty()) {
        auto canonicalizeBuiltInUnitAliases = [](const QString& termText) {
            const Tokens termTokens = Evaluator::instance()->scan(termText);
            if (!termTokens.valid() || termTokens.isEmpty())
                return termText;

            QString canonical = termText;
            int offset = 0;
            for (const Token& token : termTokens) {
                const bool isBuiltInUnitToken =
                    token.isUnitIdentifier()
                    || (token.isIdentifier() && s_isBuiltInUnitIdentifier(token.text()));
                if (!isBuiltInUnitToken)
                    continue;

                const QString normalizedUnit = normalizeUnitName(token.text());
                if (normalizedUnit.isEmpty() || normalizedUnit == token.text())
                    continue;

                const int start = token.pos() + offset;
                if (start < 0 || start + token.size() > canonical.size())
                    continue;
                canonical.replace(start, token.size(), normalizedUnit);
                offset += normalizedUnit.size() - token.size();
            }

            // Normalize bracketed aliases too ("[rev]" vs "[revolution]")
            // so additive term merging can treat them as the same base.
            int searchPos = 0;
            while (true) {
                const QRegularExpressionMatch m =
                    RegExpPatterns::bracketedSimpleUnitIdentifier().match(canonical, searchPos);
                if (!m.hasMatch())
                    break;
                const QString rawIdentifier = m.captured(1);
                if (!s_isBuiltInUnitIdentifier(rawIdentifier)) {
                    searchPos = m.capturedEnd();
                    continue;
                }
                const QString normalizedUnit = normalizeUnitName(rawIdentifier);
                if (!normalizedUnit.isEmpty()) {
                    const QString replacement = QStringLiteral("[%1]").arg(normalizedUnit);
                    canonical.replace(m.capturedStart(), m.capturedLength(), replacement);
                    searchPos = m.capturedStart() + replacement.size();
                } else {
                    searchPos = m.capturedEnd();
                }
            }
            canonical.remove(QRegularExpression(QStringLiteral("\\s+")));
            return canonical;
        };

        QHash<QString, double> termCoefficients;
        QVector<QString> termOrder;
        QHash<QString, QString> termDisplayByKey;
        auto splitLeadingNumericCoefficient = [](const QString& termText,
                                                 QString* baseOut,
                                                 double* coefficientOut) {
            const Tokens t = Evaluator::instance()->scan(termText);
            if (t.valid() && t.size() >= 3
                && t.at(0).isNumber()
                && t.at(1).asOperator() == Token::Multiplication) {
                const QString coeffText = t.at(0).text().trimmed();
                if (isSignedDecimalNumberText(coeffText) && isSafeForDoubleArithmetic(coeffText)) {
                    bool ok = false;
                    const double coefficient = coeffText.toDouble(&ok);
                    if (ok) {
                        const int baseStart = t.at(2).pos();
                        const int baseEnd = t.at(t.size() - 1).pos() + t.at(t.size() - 1).size();
                        if (baseStart >= 0 && baseEnd > baseStart && baseEnd <= termText.size()) {
                            const QString base = termText.mid(baseStart, baseEnd - baseStart).trimmed();
                            if (!base.isEmpty()) {
                                *baseOut = base;
                                *coefficientOut = coefficient;
                                return true;
                            }
                        }
                    }
                }
            }

            const QString trimmed = termText.trimmed();
            int i = 0;
            bool hasNegativePrefix = false;
            if (i < trimmed.size() && (trimmed.at(i) == MathDsl::AddOp || isMinus(trimmed.at(i)))) {
                hasNegativePrefix = isMinus(trimmed.at(i));
                ++i;
            }
            int numberStart = i;
            bool seenDigit = false;
            bool seenDot = false;
            while (i < trimmed.size()) {
                const QChar ch = trimmed.at(i);
                if (ch.isDigit()) {
                    seenDigit = true;
                    ++i;
                    continue;
                }
                if (!seenDot && ch == QLatin1Char('.')) {
                    seenDot = true;
                    ++i;
                    continue;
                }
                break;
            }
            if (!seenDigit)
                return false;
            QString coeffText = trimmed.mid(numberStart, i - numberStart);
            if (hasNegativePrefix)
                coeffText.prepend(MathDsl::SubOpAl1);
            while (i < trimmed.size() && trimmed.at(i).isSpace())
                ++i;
            if (i >= trimmed.size())
                return false;
            const QChar op = trimmed.at(i);
            // Fallback path for formatted text where scanner token kinds can
            // vary; keep operator matching on MathDsl multiplication symbols.
            const bool isMulOp =
                op == MathDsl::MulOpAl1
                || op == MathDsl::MulDotOp
                || op == MathDsl::MulCrossOp;
            if (!isMulOp)
                return false;
            ++i;
            while (i < trimmed.size() && trimmed.at(i).isSpace())
                ++i;
            if (i >= trimmed.size())
                return false;
            const QString base = trimmed.mid(i).trimmed();
            if (base.isEmpty())
                return false;

            if (!isSignedDecimalNumberText(coeffText) || !isSafeForDoubleArithmetic(coeffText))
                return false;
            bool ok = false;
            const double coefficient = coeffText.toDouble(&ok);
            if (!ok)
                return false;
            *baseOut = base;
            *coefficientOut = coefficient;
            return true;
        };
        for (int i = 0; i < keptTerms.size(); ++i) {
            QString term = keptTerms.at(i).trimmed();
            QString op = (i == 0) ? QString(MathDsl::AddOp) : keptOperators.at(i - 1);

            if (!term.isEmpty()) {
                bool termNegative = false;
                int signPos = 0;
                while (signPos < term.size()) {
                    const QChar signCh = term.at(signPos);
                    if (signCh == MathDsl::AddOp) {
                        ++signPos;
                        continue;
                    }
                    if (signCh == MathDsl::SubOpAl1 || signCh == MathDsl::SubOp) {
                        termNegative = !termNegative;
                        ++signPos;
                        continue;
                    }
                    break;
                }
                if (signPos > 0) {
                    term = term.mid(signPos).trimmed();
                    if (termNegative)
                        op = isMinusOperator(op) ? QString(MathDsl::AddOp) : QString(MathDsl::SubOpAl1);
                }
            }

            if (term.isEmpty())
                continue;

            double numericFactor = 1.0;
            QString baseTerm = term;
            if (splitLeadingNumericCoefficient(term, &baseTerm, &numericFactor))
                term = baseTerm;

            const double signedCoefficient =
                (isMinusOperator(op) ? -1.0 : 1.0) * numericFactor;
            const QString termKey = canonicalizeBuiltInUnitAliases(term);
            if (!termCoefficients.contains(termKey)) {
                termOrder.append(termKey);
                termDisplayByKey.insert(termKey, term);
            }
            termCoefficients[termKey] += signedCoefficient;
        }

        QVector<QString> mergedTerms;
        QVector<QString> mergedOperators;
        for (const QString& termKey : termOrder) {
            const double coefficient = termCoefficients.value(termKey, 0.0);
            if (std::abs(coefficient) < 1e-12)
                continue;
            const QString term = termDisplayByKey.value(termKey, termKey);

            const bool isNegative = coefficient < 0.0;
            const double magnitude = std::abs(coefficient);

            QString mergedTerm;
            if (std::abs(magnitude - 1.0) < 1e-12) {
                mergedTerm = term;
            } else {
                mergedTerm = formatSimplifiedDecimal(magnitude)
                    + QString(MathDsl::MulDotOp)
                    + term;
            }

            if (mergedTerms.isEmpty()) {
                mergedTerms.append(isNegative ? QString(MathDsl::SubOpAl1) + mergedTerm : mergedTerm);
            } else {
                mergedOperators.append(isNegative ? QString(MathDsl::SubOpAl1) : QString(MathDsl::AddOp));
                mergedTerms.append(mergedTerm);
            }
        }

        if (!mergedTerms.isEmpty()) {
            keptTerms = mergedTerms;
            keptOperators = mergedOperators;
        }
    }

    QString simplified;
    if (!keptTerms.isEmpty()) {
        simplified += keptTerms.at(0);
        for (int i = 1; i < keptTerms.size(); ++i) {
            simplified += keptOperators.at(i - 1);
            simplified += keptTerms.at(i);
        }
    }

    return simplified.isEmpty() ? expression : simplified;
}

static QString formatInterpretedExpressionForDisplayImpl(const QString& expression,
                                                         bool simplifyRepeatedMultiplicativeBases)
{
    if (expression.isEmpty())
        return expression;

    const int commentPos = findTopLevelCommentDelimiterPos(expression);
    const QString expressionPrefix = (commentPos >= 0)
        ? expression.left(commentPos).trimmed()
        : expression;
    QString commentSuffix;
    if (commentPos >= 0) {
        const QString commentText = expression.mid(commentPos + 1).trimmed();
        commentSuffix = QLatin1String(" ?");
        if (!commentText.isEmpty())
            commentSuffix += QLatin1String(" ") + commentText;
    }
    if (expressionPrefix.contains(MathDsl::ListStart)) {
        QString out;
        out.reserve(expressionPrefix.size() * 2);
        bool pendingSpace = false;
        for (int i = 0; i < expressionPrefix.size(); ++i) {
            const QChar ch = expressionPrefix.at(i);
            if (ch.isSpace()) {
                pendingSpace = true;
                continue;
            }
            if (ch == MathDsl::MulOpAl1 || ch == MathDsl::MulDotOp || ch == MathDsl::MulCrossOp) {
                out = out.trimmed();
                out += MathDsl::buildWrappedToken(MathDsl::MulCrossOp);
                pendingSpace = false;
                continue;
            }
            if (ch == MathDsl::AddOp
                || ((ch == MathDsl::SubOpAl1 || ch == MathDsl::SubOp) && i > 0))
            {
                out = out.trimmed();
                out += MathDsl::buildWrappedToken(ch == MathDsl::SubOp ? MathDsl::SubOpAl1 : ch);
                pendingSpace = false;
                continue;
            }
            if (ch == MathDsl::FunArgSep) {
                out = out.trimmed();
                out += QStringLiteral("; ");
                pendingSpace = false;
                continue;
            }
            if (ch == MathDsl::UnitStart
                && !out.isEmpty()
                && !out.back().isSpace()
                && out.back() != MathDsl::ListStart
                && out.back() != MathDsl::GroupStart) {
                out += MathDsl::QuantSp;
            }
            if (pendingSpace && !out.isEmpty()
                && !out.back().isSpace()
                && out.back() != MathDsl::ListStart
                && ch != MathDsl::ListEnd) {
                out += QLatin1Char(' ');
            }
            out += ch;
            pendingSpace = false;
        }
        return out.trimmed() + commentSuffix;
    }
    const bool hasTrigFunction =
        RegExpPatterns::trigFunctionCall().match(expressionPrefix).hasMatch();
    const bool allowAggressiveSimplification =
        simplifyRepeatedMultiplicativeBases
        // Keep unit-aware simplification conservative in general, but allow
        // trig-only expressions with explicit angle units (e.g. cos(pi[rad]))
        // to participate in symbolic folding.
        && ((!expressionPrefix.contains(MathDsl::UnitStart)
             && !expressionPrefix.contains(MathDsl::UnitEnd))
            || hasTrigFunction);

    auto scanForDisplay = [](const QString& text) {
        // Interpreted expressions are internally dot-based. For comma-based
        // display styles, scanning with a strict single-radix setting can
        // misclassify '.' as grouping separator and corrupt token structure.
        // For display-only tokenization we temporarily enable "both radix
        // chars accepted" mode, scan, then restore the previous setting.
        // This keeps display formatting stable without changing evaluation
        // behavior or persisted style.
        Settings* settings = Settings::instance();
        char previousRadix = 0;
        if (settings->isRadixCharacterBoth()) {
            previousRadix = MathDsl::MulOpAl1.toLatin1();
        } else if (!settings->isRadixCharacterAuto()) {
            previousRadix = settings->radixCharacter();
        }

        settings->setRadixCharacter(MathDsl::MulOpAl1.toLatin1());
        const Tokens tokens = Evaluator::instance()->scan(text);
        settings->setRadixCharacter(previousRadix);
        return tokens;
    };

    const Tokens scannedTokens = scanForDisplay(expressionPrefix);
    if (!scannedTokens.valid() || scannedTokens.isEmpty())
        return expressionPrefix + commentSuffix;
    auto isKnownUnitIdentifier = [](const QString& tokenText) {
        Evaluator* evaluator = Evaluator::instance();
        return s_isBuiltInUnitIdentifier(tokenText)
            || (evaluator && evaluator->hasUserUnit(tokenText));
    };
    bool hasUnitIdentifierToken = false;
    for (const Token& token : scannedTokens) {
        if (token.isUnitIdentifier()
            || (token.isIdentifier() && isKnownUnitIdentifier(token.text()))) {
            hasUnitIdentifierToken = true;
            break;
        }
    }

    const QString groupedExpression =
        groupHighPrecedenceAdditiveTermsForDisplay(expressionPrefix, scannedTokens);
    const Tokens groupedTokens = scanForDisplay(groupedExpression);
    if (!groupedTokens.valid() || groupedTokens.isEmpty())
        return groupedExpression + commentSuffix;

    auto normalizeNeutralMultiplicativeOnes = [](const QString& text) {
        static const QRegularExpression s_trailingTimesOne(
            QStringLiteral(R"(\s*(?:\*|·|×)\s*1(?:[.,]0+)?\s*$)"));
        static const QRegularExpression s_leadingOneTimes(
            QStringLiteral(R"(^\s*1(?:[.,]0+)?\s*(?:\*|·|×)\s*)"));
        static const QRegularExpression s_trailingDivOne(
            QStringLiteral(R"(\s*(?:/|÷|⧸)\s*1(?:[.,]0+)?\s*$)"));

        QString normalized = text;
        bool changed = false;
        do {
            changed = false;
            QString updated = normalized;
            updated.replace(s_trailingTimesOne, QString());
            if (updated != normalized) {
                normalized = updated;
                changed = true;
            }

            updated = normalized;
            updated.replace(s_leadingOneTimes, QString());
            if (updated != normalized) {
                normalized = updated;
                changed = true;
            }

            updated = normalized;
            updated.replace(s_trailingDivOne, QString());
            if (updated != normalized) {
                normalized = updated;
                changed = true;
            }
        } while (changed);

        auto isFullyWrappedByOuterParens = [](const QString& expr) {
            if (expr.size() < 2 || expr.at(0) != MathDsl::GroupStart || expr.at(expr.size() - 1) != MathDsl::GroupEnd)
                return false;
            int depth = 0;
            for (int i = 0; i < expr.size(); ++i) {
                const QChar ch = expr.at(i);
                if (ch == MathDsl::GroupStart)
                    ++depth;
                else if (ch == MathDsl::GroupEnd)
                    --depth;
                if (depth == 0 && i < expr.size() - 1)
                    return false;
            }
            return depth == 0;
        };

        while (isFullyWrappedByOuterParens(normalized))
            normalized = normalized.mid(1, normalized.size() - 2).trimmed();

        auto hasTopLevelLowPrecedenceOperator = [](const QString& expr) {
            int depth = 0;
            for (int i = 0; i < expr.size(); ++i) {
                const QChar ch = expr.at(i);
                if (ch == MathDsl::GroupStart) {
                    ++depth;
                    continue;
                }
                if (ch == MathDsl::GroupEnd) {
                    if (depth > 0)
                        --depth;
                    continue;
                }
                if (depth != 0)
                    continue;

                if (ch == MathDsl::AddOp
                        || ch == MathDsl::BitAndOp
                        || ch == MathDsl::BitOrOp
                        || ch == MathDsl::FunArgSep
                        || ch == MathDsl::Equals
                        || ch == MathDsl::CommentSep
                        || ch == MathDsl::LessThanOp
                        || ch == MathDsl::GreaterThanOp) {
                    return true;
                }
                if ((ch == MathDsl::SubOpAl1 || ch == MathDsl::SubOp) && i > 0)
                    return true;
            }
            return false;
        };

        auto isMulOp = [](QChar ch) {
            return ch == MathDsl::MulOpAl1
                || ch == MathDsl::MulDotOp
                || ch == MathDsl::MulCrossOp;
        };

        auto unwrapEdgeParenthesizedMulDivFactor = [&](QString* expr) {
            QString t = expr->trimmed();
            if (t.size() < 3)
                return false;

            // Unwrap "(factor) * rest" when factor has no top-level low-precedence ops.
            if (t.at(0) == MathDsl::GroupStart) {
                int depth = 0;
                int close = -1;
                for (int i = 0; i < t.size(); ++i) {
                    const QChar ch = t.at(i);
                    if (ch == MathDsl::GroupStart)
                        ++depth;
                    else if (ch == MathDsl::GroupEnd) {
                        --depth;
                        if (depth == 0) {
                            close = i;
                            break;
                        }
                    }
                }
                if (close > 0 && close < t.size() - 1) {
                    int next = close + 1;
                    while (next < t.size() && t.at(next).isSpace())
                        ++next;
                    if (next < t.size() && isMulOp(t.at(next))) {
                        const QString inner = t.mid(1, close - 1).trimmed();
                        const bool startsWithUnarySign =
                            !inner.isEmpty()
                            && (inner.at(0) == MathDsl::SubOpAl1
                                || inner.at(0) == MathDsl::SubOp
                                || inner.at(0) == MathDsl::AddOp);
                        if (!inner.isEmpty()
                                && !startsWithUnarySign
                                && !hasTopLevelLowPrecedenceOperator(inner)) {
                            t = inner + t.mid(close + 1);
                            *expr = t.trimmed();
                            return true;
                        }
                    }
                }
            }

            // Unwrap "left * (factor)" when factor has no top-level low-precedence ops.
            if (t.at(t.size() - 1) == MathDsl::GroupEnd) {
                int depth = 0;
                int open = -1;
                for (int i = t.size() - 1; i >= 0; --i) {
                    const QChar ch = t.at(i);
                    if (ch == MathDsl::GroupEnd)
                        ++depth;
                    else if (ch == MathDsl::GroupStart) {
                        --depth;
                        if (depth == 0) {
                            open = i;
                            break;
                        }
                    }
                }
                if (open > 0) {
                    int prev = open - 1;
                    while (prev >= 0 && t.at(prev).isSpace())
                        --prev;
                    if (prev >= 0 && isMulOp(t.at(prev))) {
                        const QString inner = t.mid(open + 1, t.size() - open - 2).trimmed();
                        const bool startsWithUnarySign =
                            !inner.isEmpty()
                            && (inner.at(0) == MathDsl::SubOpAl1
                                || inner.at(0) == MathDsl::SubOp
                                || inner.at(0) == MathDsl::AddOp);
                        if (!inner.isEmpty()
                                && !startsWithUnarySign
                                && !hasTopLevelLowPrecedenceOperator(inner)) {
                            t = t.left(open) + inner;
                            *expr = t.trimmed();
                            return true;
                        }
                    }
                }
            }
            return false;
        };

        while (unwrapEdgeParenthesizedMulDivFactor(&normalized)) {
            // Keep unwrapping edge multiplicative wrappers until stable.
        }

        return normalized;
    };

    QString displayExpression;
    if (allowAggressiveSimplification && (!hasUnitIdentifierToken || hasTrigFunction)) {
        displayExpression =
            simplifyRepeatedMultiplicativeBasesForDisplay(groupedExpression, groupedTokens);
        const QString normalized = normalizeNeutralMultiplicativeOnes(displayExpression);
        if (normalized != displayExpression) {
            const Tokens normalizedTokens = Evaluator::instance()->scan(normalized);
            displayExpression = normalizedTokens.isEmpty()
                ? normalized
                : simplifyRepeatedMultiplicativeBasesForDisplay(normalized, normalizedTokens);
        }
    } else {
        displayExpression = groupedExpression;
    }
    const Tokens tokens = scanForDisplay(displayExpression);
    if (!tokens.valid() || tokens.isEmpty())
        return displayExpression + commentSuffix;
    bool hasUnitIdentifierTokenInDisplay = false;
    bool unitOnlyAddSubExpression = true;
    for (const Token& token : tokens) {
        if (token.isUnitIdentifier()) {
            hasUnitIdentifierTokenInDisplay = true;
            continue;
        }
        if (token.isIdentifier() && isKnownUnitIdentifier(token.text())) {
            hasUnitIdentifierTokenInDisplay = true;
            continue;
        }
        const Token::Operator op = token.asOperator();
        if (op == Token::Addition || op == Token::Subtraction)
            continue;
        unitOnlyAddSubExpression = false;
        break;
    }
    unitOnlyAddSubExpression = unitOnlyAddSubExpression && hasUnitIdentifierTokenInDisplay;

    QString formatted;
    formatted.reserve(displayExpression.size() + tokens.size() * 2);
    auto parenthesisDepthBefore = [&tokens](int tokenIndex) {
        int depth = 0;
        for (int k = 0; k < tokenIndex; ++k) {
            const QString tokenText = tokens.at(k).text();
            if (tokenText.size() != 1)
                continue;
            const QChar ch = tokenText.at(0);
            if (ch == MathDsl::GroupStart) {
                ++depth;
            } else if (ch == MathDsl::GroupEnd && depth > 0) {
                --depth;
            }
        }
        return depth;
    };
    struct UnitBracketDisplayContext {
        bool compactDegreeSuffix = false;
        bool forceDegreeAlias = false;
    };
    QVector<UnitBracketDisplayContext> unitBracketContextStack;
    auto currentUnitBracketContext = [&unitBracketContextStack]() -> UnitBracketDisplayContext* {
        return unitBracketContextStack.isEmpty() ? nullptr : &unitBracketContextStack.last();
    };
    auto isFunctionCallGroupEnd = [&tokens](int closeIndex) {
        if (closeIndex < 0 || closeIndex >= tokens.size())
            return false;
        if (tokens.at(closeIndex).asOperator() != Token::AssociationEnd
            || tokens.at(closeIndex).text() != QLatin1String(")"))
            return false;

        int depth = 0;
        for (int i = closeIndex; i >= 0; --i) {
            const Token::Operator op = tokens.at(i).asOperator();
            if (op == Token::AssociationEnd && tokens.at(i).text() == QLatin1String(")")) {
                ++depth;
                continue;
            }
            if (op == Token::AssociationStart && tokens.at(i).text() == QLatin1String("(")) {
                --depth;
                if (depth == 0) {
                    return i > 0 && tokens.at(i - 1).isIdentifier();
                }
            }
        }
        return false;
    };
    auto normalizeCompoundDegreeUnitsInBracket = [](const QString& content) {
        if (content.isEmpty())
            return content;

        QStringList factors;
        QStringList operators;
        QString current;
        int parenDepth = 0;
        auto flushCurrent = [&]() {
            factors.append(current.trimmed());
            current.clear();
        };
        const auto isMultiplicationOp = [](const QChar& ch) {
            return ch == MathDsl::MulOpAl1
                || ch == MathDsl::MulDotOp
                || ch == MathDsl::MulCrossOp;
        };
        for (int i = 0; i < content.size(); ++i) {
            const QChar ch = content.at(i);
            if (ch == MathDsl::GroupStart) {
                ++parenDepth;
                current += ch;
                continue;
            }
            if (ch == MathDsl::GroupEnd) {
                if (parenDepth > 0)
                    --parenDepth;
                current += ch;
                continue;
            }
            if (parenDepth == 0
                && (isMultiplicationOp(ch) || ch == MathDsl::DivOp))
            {
                flushCurrent();
                operators.append(QString(ch));
                continue;
            }
            current += ch;
        }
        flushCurrent();
        if (factors.size() <= 1)
            return content;

        const QString degreeAlias = Units::degreeAliasSymbol();
        const QString degreeName = unitName(UnitId::Degree);
        const auto isDegreeFactor = [&](const QString& factor) {
            const QString t = factor.trimmed();
            if (t.isEmpty())
                return false;
            const QChar first = t.at(0);
            if (first == UnicodeChars::DegreeSign)
                return true;
            if (t.startsWith(degreeAlias)) {
                if (t.size() == degreeAlias.size())
                    return true;
                const QChar next = t.at(degreeAlias.size());
                return !next.isLetter();
            }
            if (t.startsWith(degreeName)) {
                if (t.size() == degreeName.size())
                    return true;
                const QChar next = t.at(degreeName.size());
                return !next.isLetter();
            }
            return false;
        };
        const auto normalizeDegreeFactor = [&](const QString& factor) -> QString {
            const QString t = factor.trimmed();
            if (t.isEmpty())
                return t;
            if (t.at(0) == UnicodeChars::DegreeSign) {
                QString normalized = degreeAlias;
                normalized += t.mid(1);
                return normalized;
            }
            if (t.startsWith(degreeName)) {
                const int n = degreeName.size();
                if (t.size() == n || !t.at(n).isLetter()) {
                    QString normalized = degreeAlias;
                    normalized += t.mid(n);
                    return normalized;
                }
            }
            if (t.startsWith(degreeAlias))
                return t;
            return t;
        };

        bool hasDivision = false;
        for (const QString& opText : operators) {
            if (!opText.isEmpty() && opText.at(0) == MathDsl::DivOp) {
                hasDivision = true;
                break;
            }
        }

        bool changed = false;
        for (int i = 0; i < factors.size(); ++i) {
            if (isDegreeFactor(factors.at(i))) {
                const QString normalized = normalizeDegreeFactor(factors.at(i));
                if (normalized != factors.at(i))
                    changed = true;
                factors[i] = normalized;
            }
        }

        if (!hasDivision) {
            QString degreeFactor;
            QStringList others;
            for (const QString& factor : factors) {
                if (degreeFactor.isEmpty() && isDegreeFactor(factor))
                    degreeFactor = normalizeDegreeFactor(factor);
                else
                    others.append(factor.trimmed());
            }
            if (!degreeFactor.isEmpty() && !others.isEmpty()) {
                changed = true;
                QString result = degreeFactor;
                const QString mulDotWrapped = QString(MathDsl::MulDotWrapSp)
                    + QString(MathDsl::MulDotOp)
                    + QString(MathDsl::MulDotWrapSp);
                for (const QString& factor : others)
                    result += mulDotWrapped + factor;
                return result;
            }
        }

        if (!changed)
            return content;

        QString rebuilt;
        for (int i = 0; i < factors.size(); ++i) {
            if (i > 0 && i - 1 < operators.size())
                rebuilt += operators.at(i - 1);
            rebuilt += factors.at(i).trimmed();
        }
        return rebuilt;
    };
    auto normalizeCompoundDegreeUnitsInDisplay = [&](QString text) {
        int i = 0;
        while (i < text.size()) {
            if (text.at(i) != MathDsl::UnitStart) {
                ++i;
                continue;
            }
            int depth = 1;
            int j = i + 1;
            for (; j < text.size(); ++j) {
                const QChar ch = text.at(j);
                if (ch == MathDsl::UnitStart)
                    ++depth;
                else if (ch == MathDsl::UnitEnd) {
                    --depth;
                    if (depth == 0)
                        break;
                }
            }
            if (j >= text.size())
                break;
            const QString content = text.mid(i + 1, j - i - 1);
            QString normalized = normalizeCompoundDegreeUnitsInBracket(content);
            normalized = UnitDisplayFormat::normalizeUnitTextForDisplay(normalized);
            if (normalized != content) {
                text.replace(i + 1, j - i - 1, normalized);
                j = i + 1 + normalized.size();
            }
            i = j + 1;
        }
        return text;
    };

    for (int i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens.at(i);
        const int parenthesisDepthBeforeToken = parenthesisDepthBefore(i);
        const Token::Operator op = token.asOperator();
        const bool isSpacingOperator =
            op == Token::Addition
            || op == Token::Subtraction
            || op == Token::Multiplication
            || op == Token::Division
            || op == Token::IntegerDivision
            || op == Token::UnitConversion
            || op == Token::Assignment
            || op == Token::ArithmeticLeftShift
            || op == Token::ArithmeticRightShift;
        const QString operatorText =
            op == Token::Subtraction ? QString(MathDsl::SubOp) : token.text();
        const bool shouldTreatIdentifierAsUnit =
            token.isIdentifier()
            && unitOnlyAddSubExpression
            && isKnownUnitIdentifier(operatorText)
            && !Evaluator::instance()->hasVariable(operatorText);
        const bool isUnitConversionTargetIdentifier =
            token.isIdentifier()
            && isKnownUnitIdentifier(operatorText)
            && i > 0
            && tokens.at(i - 1).asOperator() == Token::UnitConversion;
        const bool isTemperatureConversionTargetIdentifier =
            isUnitConversionTargetIdentifier
            && Units::isAffineUnitName(operatorText);
        const bool isDisplayUnitToken =
            token.isUnitIdentifier() || shouldTreatIdentifierAsUnit;
        QString displayTokenText = isDisplayUnitToken
            ? UnitDisplayFormat::shortDisplayName(operatorText)
            : operatorText;
        if (isDisplayUnitToken) {
            const UnitId id = unitId(normalizeUnitName(operatorText));
            if (id != UnitId::Unknown) {
                const QString symbol = unitSymbol(id);
                if (!symbol.isEmpty())
                    displayTokenText = symbol;
            }
            UnitBracketDisplayContext* unitContext = currentUnitBracketContext();
            if (id == UnitId::Degree
                && unitContext
                && unitContext->forceDegreeAlias)
            {
                displayTokenText = Units::degreeAliasSymbol();
            }
        }
        if (isTemperatureConversionTargetIdentifier) {
            const UnitId id = unitId(normalizeUnitName(operatorText));
            QString symbol = UnitDisplayFormat::shortDisplayName(operatorText);
            if (id != UnitId::Unknown) {
                const QString candidate = unitSymbol(id);
                if (!candidate.isEmpty())
                    symbol = candidate;
            }
            displayTokenText = QStringLiteral("[%1]").arg(symbol);
        }
        const bool isInsideExplicitUnitBlock =
            i > 0
            && tokens.at(i - 1).text().size() == 1
            && tokens.at(i - 1).text().at(0) == MathDsl::UnitStart;
        if (isDisplayUnitToken && unitOnlyAddSubExpression && !isInsideExplicitUnitBlock) {
            displayTokenText = QStringLiteral("1")
                + QString(MathDsl::QuantSp)
                + QStringLiteral("[%1]").arg(displayTokenText);
        }

        if (token.text().size() == 1 && token.text().at(0) == MathDsl::UnitStart) {
            const bool hasPreviousToken = i > 0;
            const Token previousToken = hasPreviousToken ? tokens.at(i - 1) : Token::null;
            const bool followsConversionArrow =
                hasPreviousToken && previousToken.asOperator() == Token::UnitConversion;
            const bool shouldAddValueUnitSpace =
                hasPreviousToken
                && (previousToken.isNumber()
                    || previousToken.isIdentifier()
                    || previousToken.isUnitIdentifier()
                    || isFunctionCallGroupEnd(i - 1));

            UnitBracketDisplayContext unitContext;
            int depth = 1;
            int closeIndex = -1;
            bool hasDivisionInside = false;
            for (int j = i + 1; j < tokens.size(); ++j) {
                const QString tokenText = tokens.at(j).text();
                if (tokenText.size() == 1 && tokenText.at(0) == MathDsl::UnitStart) {
                    ++depth;
                    continue;
                }
                if (tokenText.size() == 1 && tokenText.at(0) == MathDsl::UnitEnd) {
                    --depth;
                    if (depth == 0) {
                        closeIndex = j;
                        break;
                    }
                    continue;
                }
                if (depth == 1 && tokens.at(j).asOperator() == Token::Division)
                    hasDivisionInside = true;
            }
            const bool isAssignmentLeftSideUnitBlock =
                !hasPreviousToken
                && closeIndex >= 0
                && closeIndex + 1 < tokens.size()
                && tokens.at(closeIndex + 1).asOperator() == Token::Assignment;
            const bool shouldInsertImplicitUnitOne =
                (!hasPreviousToken
                    || previousToken.asOperator() == Token::Addition
                    || previousToken.asOperator() == Token::Subtraction
                    || previousToken.asOperator() == Token::Assignment
                    || previousToken.asOperator() == Token::ListSeparator
                    || previousToken.asOperator() == Token::AssociationStart)
                &&
                !followsConversionArrow
                && !shouldAddValueUnitSpace
                && !isAssignmentLeftSideUnitBlock
                && i + 1 < tokens.size()
                && tokens.at(i + 1).isUnitIdentifier();
            const bool isSimpleDegreeUnit =
                closeIndex == i + 2
                && (tokens.at(i + 1).isUnitIdentifier()
                    || (tokens.at(i + 1).isIdentifier()
                        && isKnownUnitIdentifier(tokens.at(i + 1).text())))
                && unitId(normalizeUnitName(tokens.at(i + 1).text())) == UnitId::Degree;
            unitContext.compactDegreeSuffix =
                isSimpleDegreeUnit
                && !followsConversionArrow
                && hasPreviousToken
                && previousToken.isNumber()
                && parenthesisDepthBeforeToken == 0;
            unitContext.forceDegreeAlias = hasDivisionInside;
            unitBracketContextStack.append(unitContext);

            if (shouldInsertImplicitUnitOne) {
                formatted += QStringLiteral("1");
                formatted += MathDsl::QuantSp;
            } else if (shouldAddValueUnitSpace && !unitContext.compactDegreeSuffix) {
                formatted += MathDsl::QuantSp;
            }
            if (unitContext.compactDegreeSuffix)
                continue;
        } else if (token.text().size() == 1 && token.text().at(0) == MathDsl::UnitEnd) {
            UnitBracketDisplayContext unitContext;
            if (!unitBracketContextStack.isEmpty()) {
                unitContext = unitBracketContextStack.last();
                unitBracketContextStack.removeLast();
            }
            if (unitContext.compactDegreeSuffix)
                continue;
        }

        if (!isSpacingOperator) {
            formatted += displayTokenText;
            continue;
        }

        const bool hasLeftOperand = i > 0
            && tokenCanEndOperandForDisplaySpacing(tokens.at(i - 1));
        const bool hasRightOperand = i + 1 < tokens.size()
            && tokenCanStartOperandForDisplaySpacing(tokens.at(i + 1));
        const bool isBinaryOperator = hasLeftOperand && hasRightOperand;
        if (!isBinaryOperator) {
            // Keep unary operators untouched (for example "−pi").
            formatted += displayTokenText;
            continue;
        }

        if ((op == Token::Addition || op == Token::Subtraction)
            && startsWithImaginaryLiteralForDisplay(tokens, i + 1)) {
            // Keep "a+bj"/"a-bj" compact in complex-mode interpreted output.
            formatted += operatorText;
            continue;
        }

        if (op == Token::Multiplication
            && i > 0
            && i + 1 < tokens.size()
            && tokens.at(i - 1).isNumber()
            && isImaginaryUnitTokenForDisplay(tokens.at(i + 1))) {
            // Render numeric imaginary literals as compact suffixes: "3j".
            continue;
        }

        if (op == Token::Addition) {
            formatted += MathDsl::AddWrap;
            formatted += MathDsl::AddOp;
            formatted += MathDsl::AddWrap;
            continue;
        }

        if (op == Token::Subtraction) {
            formatted += MathDsl::SubWrapSp;
            formatted += MathDsl::SubOp;
            formatted += MathDsl::SubWrapSp;
            continue;
        }

        if (op == Token::Division) {
            if (currentUnitBracketContext()) {
                formatted += MathDsl::DivOp;
            } else {
                formatted += MathDsl::DivWrap;
                formatted += MathDsl::DivOp;
                formatted += MathDsl::DivWrap;
            }
            continue;
        }

        if (op == Token::Multiplication) {
            const bool hasNumericLiteralLhs =
                isHardcodedNumericOperandOnLeftOfMultiplicationForDisplay(
                    tokens, i, true);
            const bool hasNumericLiteralRhs =
                isHardcodedNumericOperandOnRightOfMultiplicationForDisplay(
                    tokens, i, true);
            const QChar mulSign = (hasNumericLiteralLhs && hasNumericLiteralRhs)
                ? MathDsl::MulCrossOp
                : MathDsl::MulDotOp;
            const QChar mulSpace = mulSign == MathDsl::MulCrossOp
                ? MathDsl::MulCrossWrapSp
                : MathDsl::MulDotWrapSp;
            formatted += mulSpace;
            formatted += mulSign;
            formatted += mulSpace;
            continue;
        }

        formatted += MathDsl::AddWrap;
        formatted += displayTokenText;
        formatted += MathDsl::AddWrap;

    }

    formatted = normalizeCompoundDegreeUnitsInDisplay(formatted);
    formatted = renderIntegerPowersAsSuperscriptsForDisplay(
        formatted,
        allowAggressiveSimplification);
    formatted = normalizeCompoundDegreeUnitsInDisplay(formatted);
    return formatted + commentSuffix;
}

QString Evaluator::simplifyInterpretedExpression(const QString& expression)
{
    if (expression.isEmpty())
        return expression;

    const int commentPos = findTopLevelCommentDelimiterPos(expression);
    const QString expressionPrefix = (commentPos >= 0)
        ? expression.left(commentPos).trimmed()
        : expression;
    QString commentSuffix;
    if (commentPos >= 0) {
        const QString commentText = expression.mid(commentPos + 1).trimmed();
        commentSuffix = QLatin1String(" ?");
        if (!commentText.isEmpty())
            commentSuffix += QLatin1String(" ") + commentText;
    }

    if (expressionPrefix.contains(MathDsl::UnitStart)
        || expressionPrefix.contains(MathDsl::UnitEnd))
    {
        // Most unit-bearing interpreted expressions skip symbolic simplification
        // to avoid invalid rewrites across dimensions. Trig expressions are a
        // safe exception because explicit angle units are consumed by the trig
        // call and do not dimension the scalar result.
        const bool hasTrigFunction =
            RegExpPatterns::trigFunctionCall().match(expressionPrefix).hasMatch();
        if (!hasTrigFunction)
            return expressionPrefix + commentSuffix;
    }

    auto scanForDisplay = [](const QString& text) {
        // Same display-tokenization strategy as above:
        // force temporary dual-radix scan mode so internal dot-decimal
        // interpreted text is tokenized correctly under comma-style displays.
        Settings* settings = Settings::instance();
        char previousRadix = 0;
        if (settings->isRadixCharacterBoth()) {
            previousRadix = MathDsl::MulOpAl1.toLatin1();
        } else if (!settings->isRadixCharacterAuto()) {
            previousRadix = settings->radixCharacter();
        }

        settings->setRadixCharacter(MathDsl::MulOpAl1.toLatin1());
        const Tokens tokens = Evaluator::instance()->scan(text);
        settings->setRadixCharacter(previousRadix);
        return tokens;
    };

    const Tokens scannedTokens = scanForDisplay(expressionPrefix);
    if (!scannedTokens.valid() || scannedTokens.isEmpty())
        return expressionPrefix + commentSuffix;

    const QString groupedExpression =
        groupHighPrecedenceAdditiveTermsForDisplay(expressionPrefix, scannedTokens);
    const Tokens groupedTokens = scanForDisplay(groupedExpression);
    if (!groupedTokens.valid() || groupedTokens.isEmpty())
        return groupedExpression + commentSuffix;

    QString simplifiedExpression =
        simplifyRepeatedMultiplicativeBasesForDisplay(groupedExpression, groupedTokens);

    // Normalize neutral multiplicative factors that can block symbolic
    // cancellations in the display simplifier (for example "(a/b)*1" -> "a/b").
    static const QRegularExpression s_trailingTimesOne(
        QStringLiteral(R"(\s*(?:\*|·|×)\s*1(?:[.,]0+)?\s*$)"));
    static const QRegularExpression s_leadingOneTimes(
        QStringLiteral(R"(^\s*1(?:[.,]0+)?\s*(?:\*|·|×)\s*)"));
    static const QRegularExpression s_trailingDivOne(
        QStringLiteral(R"(\s*(?:/|÷|⧸)\s*1(?:[.,]0+)?\s*$)"));

    QString normalized = simplifiedExpression;
    bool changed = false;
    do {
        changed = false;
        QString updated = normalized;
        updated.replace(s_trailingTimesOne, QString());
        if (updated != normalized) {
            normalized = updated;
            changed = true;
        }

        updated = normalized;
        updated.replace(s_leadingOneTimes, QString());
        if (updated != normalized) {
            normalized = updated;
            changed = true;
        }

        updated = normalized;
        updated.replace(s_trailingDivOne, QString());
        if (updated != normalized) {
            normalized = updated;
            changed = true;
        }
    } while (changed);

    if (normalized != simplifiedExpression) {
        const Tokens normalizedTokens = scanForDisplay(normalized);
        if (normalizedTokens.valid() && !normalizedTokens.isEmpty())
            simplifiedExpression = simplifyRepeatedMultiplicativeBasesForDisplay(normalized, normalizedTokens);
        else
            simplifiedExpression = normalized;
    }

    return simplifiedExpression + commentSuffix;
}

QString Evaluator::formatInterpretedExpressionForDisplay(const QString& expression)
{
    return formatInterpretedExpressionForDisplayImpl(expression, false);
}

QString Evaluator::formatInterpretedExpressionSimplifiedForDisplay(const QString& expression)
{
    return formatInterpretedExpressionForDisplayImpl(expression, true);
}

static QStringList splitTopLevelFunctionArguments(const QString& text)
{
    QStringList arguments;
    int parenDepth = 0;
    int argumentStart = 0;

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == '(') {
            ++parenDepth;
        } else if (ch == ')') {
            if (parenDepth > 0)
                --parenDepth;
        } else if (ch == ';' && parenDepth == 0) {
            arguments.append(text.mid(argumentStart, i - argumentStart).trimmed());
            argumentStart = i + 1;
        }
    }

    arguments.append(text.mid(argumentStart).trimmed());
    return arguments;
}

TokenStack::TokenStack() : QVector<Token>()
{
    topIndex = 0;
    m_error = "";
    ensureSpace();
}

bool TokenStack::isEmpty() const
{
    return topIndex == 0;
}

unsigned TokenStack::itemCount() const
{
    return topIndex;
}

void TokenStack::push(const Token& token)
{
    ensureSpace();
    (*this)[topIndex++] = token;
}

Token TokenStack::pop()
{
    if (topIndex > 0)
        return Token(at(--topIndex));

    m_error = "token stack is empty (BUG)";
    return Token();
}

const Token& TokenStack::top()
{
    return top(0);
}

const Token& TokenStack::top(unsigned index)
{
    return (topIndex > (int)index) ? at(topIndex - index - 1) : Token::null;
}

void TokenStack::ensureSpace()
{
    int length = size();
    while (topIndex >= length) {
        length += 10;
        resize(length);
    }
}

/** Remove \a count tokens from the top of the stack, add a stxAbstract token
 * to the top and adjust its text position and minimum precedence.
 *
 * \param minPrecedence minimum precedence to set the top token, or
 * \c INVALID_PRECEDENCE if this method should use the minimum value from
 * the removed tokens.
 */
void TokenStack::reduce(int count, int minPrecedence)
{
    // assert(itemCount() > count);

    QList<Token> tokens;
    for (int i = 0 ; i < count ; ++i)
        tokens.append(pop());

    reduce(tokens, Token(Token::stxAbstract), minPrecedence);
}

/** Remove \a count tokens from the top of the stack, push \a top to the top
 * and adjust its text position and minimum precedence.
 *
 * \param minPrecedence minimum precedence to set the top token, or
 * \c INVALID_PRECEDENCE if this method should use the minimum value
 * from the removed tokens.
 */
void TokenStack::reduce(int count, Token&& top, int minPrecedence)
{
    // assert(itemCount() >= count);

    QList<Token> tokens;
    for (int i = 0 ; i < count ; ++i)
        tokens.append(pop());

    reduce(tokens, std::forward<Token>(top), minPrecedence);
}

/** Push \a top to the top and adjust its text position and minimum precedence
 * using \a tokens.
 *
 * \param minPrecedence minimum precedence to set the top token, or
 * \c INVALID_PRECEDENCE if this method should use the minimum value from the
 * removed tokens.
 */
void TokenStack::reduce(QList<Token> tokens, Token&& top, int minPrecedence)
{
#ifdef EVALUATOR_DEBUG
    {
        const auto& _tokens = tokens;
        qDebug() << "reduce(" << _tokens.size() << ", " << top.description()
                 << ", " << minPrecedence << ")";
        for (const auto& t : _tokens)
            qDebug() << t.description();
    }
#endif // EVALUATOR_DEBUG

    std::sort(tokens.begin(), tokens.end(), tokenPositionCompare);

    bool computeMinPrec = (minPrecedence == INVALID_PRECEDENCE);
    int min_prec = computeMinPrec ? MAX_PRECEDENCE : minPrecedence;
    int start = -1, end = -1;
    const auto& _tokens = tokens;
    for (auto& token : _tokens) {
        if (computeMinPrec) {
            Token::Operator op = token.asOperator();
            if (op != Token::Invalid) {
                int prec = opPrecedence(op);
                if (prec < min_prec)
                    min_prec = prec;
            }
        }

        if (token.pos() == -1 && token.size() == -1)
            continue;

        if (token.pos() == -1 || token.size() == -1) {

#ifdef EVALUATOR_DEBUG
            qDebug() << "BUG: found token with either pos or size not set, "
                        "but not both.";
#endif  // EVALUATOR_DEBUG
            continue;
        }

        if (start == -1) {
            start = token.pos();
        } else {

#ifdef EVALUATOR_DEBUG
            if (token.pos() != end)
                qDebug() << "BUG: tokens expressions are not successive.";
#endif  // EVALUATOR_DEBUG

        }

        end = token.pos() + token.size();
    }

    if (start != -1) {
        top.setPos(start);
        top.setSize(end - start);
    }

    top.setMinPrecedence(min_prec);

#ifdef EVALUATOR_DEBUG
    qDebug() << "=> " << top.description();
#endif  // EVALUATOR_DEBUG

    push(top);
}

#ifdef EVALUATOR_DEBUG
void Tokens::append(const Token& token)
{
    qDebug() << QString("tokens.append: type=%1 text=%2")
                    .arg(token.type())
                    .arg(token.text());
    QVector<Token>::append(token);
}
#endif // EVALUATOR_DEBUG

static bool isSubscriptDigit(QChar ch)
{
    return ch.unicode() >= UnicodeChars::SubscriptZero.unicode()
        && ch.unicode() <= UnicodeChars::SubscriptNine.unicode();
}

static QString superscriptDigitsToAscii(const QString& text)
{
    QString converted;
    converted.reserve(text.size());
    for (const QChar& ch : text) {
        if (ch == MathDsl::PowNeg) {
            converted.append(MathDsl::SubOp);
            continue;
        }
        if (ch == MathDsl::PowPos) {
            converted.append(MathDsl::AddOp);
            continue;
        }
        const QChar asciiDigit = MathDsl::superscriptDigitToAscii(ch);
        converted.append(asciiDigit.isNull() ? ch : asciiDigit);
    }
    return converted;
}

static bool isSubscriptLetter(QChar ch)
{
    switch (ch.unicode()) {
    case UnicodeChars::LatinSubscriptSmallLetterA.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterE.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterO.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterX.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterSchwa.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterH.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterK.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterL.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterM.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterN.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterP.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterS.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterT.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterI.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterR.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterU.unicode():
    case UnicodeChars::LatinSubscriptSmallLetterV.unicode():
    case UnicodeChars::GreekSubscriptSmallLetterBeta.unicode():
    case UnicodeChars::GreekSubscriptSmallLetterGamma.unicode():
    case UnicodeChars::GreekSubscriptSmallLetterRho.unicode():
    case UnicodeChars::GreekSubscriptSmallLetterPhi.unicode():
    case UnicodeChars::GreekSubscriptSmallLetterChi.unicode():
        return true;
    default:
        return false;
    }
}

// Helper function: return true for valid identifier start character.
static bool isIdentifierStart(QChar ch)
{
    return ch == UnicodeChars::LowLine
           || ch.unicode() == MathDsl::DollarSign.unicode()
           || ch.isLetter()
           || isSubscriptLetter(ch)
           || ch == UnicodeChars::Summation
           || ch == UnicodeChars::SquareRoot
           || ch == UnicodeChars::CubeRoot;
}

// Helper function: return true for valid identifier continuation character.
static bool isIdentifierContinue(QChar ch)
{
    return isIdentifierStart(ch) || ch.isDigit() || isSubscriptDigit(ch);
}

// Helper function: return true for valid radix characters.
bool Evaluator::isRadixChar(const QChar& ch)
{
    // Accept both '.' and ',' as potential radix markers at tokenization time.
    // Final decimal/grouping disambiguation is done later in fixNumberRadix().
    if (ch.unicode() == MathDsl::DotSep.unicode() || ch.unicode() == MathDsl::CommaSep.unicode())
        return true;

    if (Settings::instance()->isRadixCharacterBoth())
        return ch.unicode() == MathDsl::DotSep.unicode() || ch.unicode() == MathDsl::CommaSep.unicode();

    return ch.unicode() == Settings::instance()->radixCharacter();
}

// Helper function: return true for valid thousand separator characters.
bool Evaluator::isSeparatorChar(const QChar& ch)
{
    // Never treat letters or digits from any script as grouping separators.
    // This avoids silently collapsing expressions like "12天2" into "122".
    if (ch.isLetterOrNumber())
        return false;

    if (ch == UnicodeChars::Summation
        || ch == UnicodeChars::SquareRoot
        || ch == UnicodeChars::CubeRoot
        || isDegreeSign(ch))
        return false;

    if (isRadixChar(ch))
        return false;

    return RegExpPatterns::separatorToken().match(ch).hasMatch();
}

bool Evaluator::isCommentOnlyExpression(const QString& expr)
{
    for (QChar ch : expr) {
        if (!ch.isSpace())
            return ch == MathDsl::CommentSep;
    }

    return false;
}


QString Evaluator::fixNumberRadix(const QString& number)
{
    int dotCount = 0;
    int commaCount = 0;
    QChar lastRadixChar;

    // First pass: count dot/comma occurrences independently of the selected
    // radix setting. This keeps number parsing tolerant to copied values that
    // use a different but obvious decimal/grouping convention than the active
    // UI format (for example "1,56" while dot style is selected).
    for (int i = 0 ; i < number.size() ; ++i) {
        QChar c = number[i];
        if (c == MathDsl::DotSep || c == MathDsl::CommaSep) {
            lastRadixChar = c;
            if (c == MathDsl::DotSep)
                ++dotCount;
            else
                ++commaCount;
        }
    }

    // Decide which radix characters to ignore based on their occurence count.
    bool ignoreDot = dotCount != 1;
    bool ignoreComma = commaCount != 1;
    if (!ignoreDot && !ignoreComma) {
        // If both radix characters are present once,
        // consider the last one as the radix point.
        ignoreDot = lastRadixChar != MathDsl::DotSep;
        ignoreComma = lastRadixChar != ',';
    }

    QChar radixChar; // Null character by default.
    if (!ignoreDot)
        radixChar = MathDsl::DotSep;
    else if (!ignoreComma)
        radixChar = ',';

    // Second pass: write the result.
    QString result = "";
    for (int i = 0 ; i < number.size() ; ++i) {
        QChar c = number[i];
        if (c == MathDsl::DotSep || c == MathDsl::CommaSep) {
            if (c == radixChar)
                result.append(MathDsl::DotSep);
        } else
          result.append(c);
    }

    return result;
}

HNumber getNumber(const QString& number) {
    if (number.isEmpty())
        return HNumber(0);
    const QString sexagesimalStopChars =
        QStringLiteral("['\":") + MathDsl::ArcminOp + MathDsl::ArcsecOp + MathDsl::UnitEnd;
    int endPos = number.indexOf(QRegularExpression(sexagesimalStopChars)); // Stop at sexagesimal separators/markers.
    if (endPos == 0)
        return HNumber(0);
    if (endPos > 0)
        return HNumber(Evaluator::fixNumberRadix(number.left(endPos)).toStdString().c_str());
    return HNumber(Evaluator::fixNumberRadix(number).toStdString().c_str());
}

QString Evaluator::fixSexagesimal(const QString& number, QString& unit)
{
    unit.clear();
    QString bad, result = number;
    const QString minuteMarkClass = QStringLiteral("['") + MathDsl::ArcminOp + MathDsl::UnitEnd;
    const QString secondMarkClass = QStringLiteral("[\"") + MathDsl::ArcsecOp + MathDsl::UnitEnd;
    const auto isMinuteMark = [](QChar c) {
        return c == MathDsl::ArcminOpAl1 || c == MathDsl::ArcminOp;
    };
    const auto isSecondMark = [](QChar c) {
        return c == MathDsl::ArcsecOpAl1 || c == MathDsl::ArcsecOp;
    };

    bool arc = false;
    int colonCount = 0, degreeCount = 0, minuteCount = 0, secondCount = 0, digitCount = 0;
    for (int i = 0 ; i < number.size() ; ++i) { // check order and amount
        QChar c = number[i];
        if (isDegreeSign(c)) {
            if (colonCount || minuteCount || secondCount)
                return bad;
            if (++degreeCount > 1)
                return bad;
            arc = true;
        }
        else if (c == ':') {
            if (minuteCount || degreeCount || secondCount)
                return bad;
            if (++colonCount > 2)
                return bad;
        }
        else if (isMinuteMark(c)) {
            if (secondCount)
                return bad;
            if (++minuteCount > 1)
                return bad;
            arc = true;
        }
        else if (isSecondMark(c)) {
            if (degreeCount && !minuteCount)
                return bad;
            if (++secondCount > 1)
                return bad;
            arc = true;
        }
        else if (c.isDigit())
            ++digitCount;
    }

    if (colonCount || degreeCount || minuteCount || secondCount) {  // sexagesimal value
        if (digitCount == 0)
            return bad;
        HNumber::Format fixed = HNumber::Format::Fixed();
        HNumber mains(0), minutes(0), seconds(0), sign(1);
        int minPos = number.indexOf(arc ? QRegularExpression("[\\x{00B0}\\x{00BA}\\x{02DA}\\x{2218}]")
                                        : QRegularExpression(":"));
        if (minPos >= 0) {  // degree sign or first colon -> minutes
            mains = getNumber(number.left(minPos));    // hours or degrees
            if (mains.isNegative())
                sign = HNumber(-1);
            int secPos = number.indexOf(arc ? QRegularExpression(minuteMarkClass)
                                            : QRegularExpression(":"),
                                        minPos + 1);
            if (secPos >= 0) {  // single quote or second colon -> seconds
                minutes = getNumber(number.mid(minPos + 1));
                seconds = getNumber(number.mid(secPos + 1));
                if (seconds.isZero()) {  // postfix minutes
                    result = HMath::format(mains * HNumber(60) + minutes * sign, fixed);
                    unit = arc ? unitName(UnitId::Arcminute)
                               : unitName(UnitId::Minute);
                }
                else {  // minutes and seconds
                    result = HMath::format(mains * HNumber(3600) + minutes * HNumber(60) * sign + seconds * sign, fixed);
                    unit = arc ? unitName(UnitId::Arcsecond)
                               : unitName(UnitId::Second);
                }
            }
            else {  // just minutes
                minutes = getNumber(number.mid(minPos + 1));
                result = HMath::format(mains * HNumber(60) + minutes * sign, fixed);
                unit = arc ? unitName(UnitId::Arcminute)
                           : unitName(UnitId::Minute);
            }
            if (seconds.isZero() && minutes.isZero()) {  // postfix mains
                result = HMath::format(mains, fixed);
                unit = arc ? unitName(UnitId::Degree)
                           : unitName(UnitId::Hour);
            }
        }
        else if ( arc ) {
            int secPos = number.indexOf(QRegularExpression(minuteMarkClass));
            if (secPos >= 0) {  // single quote -> seconds
                minutes = getNumber(number.left(secPos));
                if (minutes.isNegative())
                    sign = HNumber(-1);
                seconds = getNumber(number.mid(secPos + 1));
                if (seconds.isZero()) {  // postfix minutes
                    result = HMath::format(minutes, fixed);
                    unit = unitName(UnitId::Arcminute);
                }
                else {
                    result = HMath::format(minutes * HNumber(60) + seconds * sign, fixed);
                    unit = unitName(UnitId::Arcsecond);
                }
            }
            else {
                int unitPos = number.indexOf(QRegularExpression(secondMarkClass));
                if (unitPos >= 0) {  // postfix seconds
                    seconds = getNumber(number.left(unitPos));
                    result = HMath::format(seconds, fixed);
                    unit = unitName(UnitId::Arcsecond);
                }
            }
        }
        int unitPos = number.indexOf(QRegularExpression(secondMarkClass));  // check digits after seconds unit
        if (unitPos >= 0 && !getNumber(number.mid(unitPos + 1)).isZero())
            return bad;
        if (!seconds.isZero() && (!minutes.isInteger() || !mains.isInteger()))
            return bad;
        if (!minutes.isZero() && !mains.isInteger())
            return bad;
        const bool hasScientificExponent =
            number.contains(QLatin1Char('e'), Qt::CaseInsensitive);
        int dotNumber = number.lastIndexOf(QRegularExpression("[.,]"));
        if (dotNumber >= 0 && !hasScientificExponent) {  // append decimals, remove possible postfix units
            int minPos = number.indexOf(QRegularExpression(minuteMarkClass));
            int secPos = number.indexOf(QRegularExpression(secondMarkClass));
            int unitPos = (secPos >= 0 && secPos < minPos) ? secPos : minPos;
            int dotResult = result.lastIndexOf(MathDsl::DotSep);
            if (dotResult >= 0)  // replace decimals with original ones for accuracy
                result.resize(dotResult);
            result += number.mid(dotNumber, unitPos < 0 ? -1 : unitPos - dotNumber);
        }
    }
    else
        result = fixNumberRadix(result);

    return result;
}

Evaluator* Evaluator::instance()
{
    if (!s_evaluatorInstance) {
        s_evaluatorInstance = new Evaluator;
        qAddPostRoutine(s_deleteEvaluator);
    }
    return s_evaluatorInstance;
}

QStringList Evaluator::builtInUnitIdentifiers()
{
    QStringList identifiers = s_builtInUnitLookup().keys();
    identifiers.sort();
    return identifiers;
}

Evaluator::Evaluator()
{
    reset();
}

void Evaluator::initializeBuiltInVariables()
{
    setVariable(QLatin1String("e"), DMath::e(), Variable::BuiltIn);
    setVariable(QString::fromUtf8("ℯ"), DMath::e(), Variable::BuiltIn);

    setVariable(QLatin1String("pi"), DMath::pi(), Variable::BuiltIn);
    setVariable(QString::fromUtf8("π"), DMath::pi(), Variable::BuiltIn);

    if (Settings::instance()->complexNumbers) {
        setVariable(QLatin1String("i"), DMath::i(), Variable::BuiltIn);
        setVariable(QLatin1String("j"), DMath::i(), Variable::BuiltIn);
    } else {
        if (hasVariable("i"))
            unsetVariable("i", ForceBuiltinVariableErasure(true));
        if (hasVariable("j"))
            unsetVariable("j", ForceBuiltinVariableErasure(true));
    }
    initializeAngleUnits();
}

void Evaluator::initializeAngleUnits()
{
    // Angle-dependent built-in unit values are resolved from the dedicated
    // unit lookup table (not from reserved variable names).
    s_builtInUnitLookup();
}

void Evaluator::setExpression(const QString& expr)
{
    m_expression = rewriteListBracesToInternalCalls(expr);
    m_dirty = true;
    m_valid = false;
    m_error = QString();
    m_interpretedExpression = QString();
    m_hasImplicitMultiplication = false;
}

QString Evaluator::expression() const
{
    return m_expression;
}

// Returns the validity. Note: empty expression is always invalid.
bool Evaluator::isValid()
{
    if (m_dirty) {
        Tokens tokens = scan(m_expression);
        if (!tokens.valid())
            compile(tokens);
        else
            m_valid = false;
    }
    return m_valid;
}

void Evaluator::reset()
{
    m_expression = QString();
    m_dirty = true;
    m_valid = false;
    m_error = QString();
    m_interpretedExpression = QString();
    m_constants.clear();
    m_constantTexts.clear();
    m_codes.clear();
    m_implicitMultiplicationOpcodeIndices.clear();
    m_assignId = QString();
    m_assignFunc = false;
    m_assignArg.clear();
    m_assignVarDescription = QString();
    m_assignUnitDescription = QString();
    m_assignFuncExpr = QString();
    m_assignUnitExpr = QString();
    m_assignFuncDescription = QString();
    m_assignUnit = false;
    m_session = nullptr;
    m_functionsInUse.clear();
    m_hasImplicitMultiplication = false;

    initializeBuiltInVariables();
}

void Evaluator::setSession(Session* s)
{
    m_session = s;
}

const Session* Evaluator::session()
{
    return m_session;
}

QString Evaluator::error() const
{
    return m_error;
}

QString Evaluator::interpretedExpression() const
{
    return m_interpretedExpression;
}

bool Evaluator::hasImplicitMultiplication() const
{
    return m_hasImplicitMultiplication;
}

// Returns list of token for the expression.
// This triggers again the lexical analysis step. It is however preferable
// (even when there's small performance penalty) because otherwise we need to
// store parsed tokens all the time which serves no good purpose.
Tokens Evaluator::tokens() const
{
    return scan(m_expression);
}

bool isArcTime(QChar ch) {
    return (isDegreeSign(ch)
            || ch == '\''
            || ch == '"'
            || ch == MathDsl::ArcminOp
            || ch == MathDsl::ArcsecOp
            || ch == ':');
}

Tokens Evaluator::scan(const QString& expr) const
{
    // Associate character codes with the highest number base
    // they might belong to.
    constexpr unsigned DIGIT_MAP_COUNT = 128;
    static unsigned char s_digitMap[DIGIT_MAP_COUNT] = { 0 };

    if (s_digitMap[0] == 0) {
        // Initialize the digits map.
        std::fill_n(s_digitMap, DIGIT_MAP_COUNT, 255);
        for (int i = '0' ; i <= '9' ; ++i)
            s_digitMap[i] = i - '0' + 1;
        for (int i = 'a' ; i <= 'z' ; ++i)
            s_digitMap[i] = i - 'a' + 11;
        for (int i = 'A' ; i <= 'Z' ; ++i)
            s_digitMap[i] = i - 'A' + 11;
    }

    // Result.
    Tokens tokens;

    // Parsing state.
    enum {
        Init, Start, Finish, Bad, InNumberPrefix, InNumber, InExpIndicator,
        InExponentBase, InExponent, InIdentifier, InNumberEnd
    } state;

    // Initialize variables.
    state = Init;
    int i = 0;
    QString ex = UnicodeChars::normalizeUnitSymbolAliases(expr);
    // Accept editor-produced spacing in unit conversion aliases:
    // "− >" or "- >" should tokenize as "->".
    ex.replace(
        QRegularExpression(QStringLiteral("([\\-−])\\s+>")),
        QStringLiteral("->"));
    // Accept superscript shorthand aliases for unit symbols (m², mm³, etc.)
    // only inside explicit unit brackets.
    int superscriptUnitBracketDepth = 0;
    for (int p = 0; p < ex.size();) {
        const QChar current = ex.at(p);
        if (current == MathDsl::UnitStart) {
            ++superscriptUnitBracketDepth;
            ++p;
            continue;
        }
        if (current == MathDsl::UnitEnd) {
            if (superscriptUnitBracketDepth > 0)
                --superscriptUnitBracketDepth;
            ++p;
            continue;
        }
        if (superscriptUnitBracketDepth == 0 || !isIdentifierStart(current)) {
            ++p;
            continue;
        }

        int identEnd = p + 1;
        while (identEnd < ex.size() && isIdentifierContinue(ex.at(identEnd)))
            ++identEnd;

        int superscriptStart = identEnd;
        if (superscriptStart < ex.size() && MathDsl::isSuperscriptSign(ex.at(superscriptStart)))
            ++superscriptStart;

        int superscriptEnd = superscriptStart;
        while (superscriptEnd < ex.size() && MathDsl::isSuperscriptDigit(ex.at(superscriptEnd)))
            ++superscriptEnd;

        if (superscriptEnd == superscriptStart) {
            p = identEnd;
            continue;
        }

        const QString identifier = ex.mid(p, identEnd - p);
        const QString superscript = ex.mid(identEnd, superscriptEnd - identEnd);
        const QString replacement =
            identifier + MathDsl::PowOp + superscriptDigitsToAscii(superscript);
        ex.replace(p, superscriptEnd - p, replacement);
        p += replacement.size();
    }
    QString tokenText, tokenUnit;
    int tokenStart = 0; // Includes leading spaces.
    Token::Type type;
    int numberBase = 10;
    int expBase = 0;
    int expStart = -1;  // Index of the exponent part in the expression.
    QString expText;    // Start of the exponent text matching /E[\+\-]*/
    int unitBracketDepth = 0;

    // Force a terminator.
    ex.append(QChar());

#ifdef EVALUATOR_DEBUG
    qDebug() << "Scanning" << ex;
#endif // EVALUATOR_DEBUG

    const auto isUnitArcSymbol = [](QChar c) {
        return c == MathDsl::ArcminOp
            || c == MathDsl::ArcsecOp
            || c == MathDsl::ArcminOpAl1
            || c == MathDsl::ArcsecOpAl1;
    };

    // Main loop.
    while (state != Bad && state != Finish && i < ex.length()) {
        QChar ch = ex.at(i);

#ifdef EVALUATOR_DEBUG
        qDebug() << QString("state=%1 ch=%2 i=%3 tokenText=%4")
                            .arg(state).arg(ch).arg(i).arg(tokenText);
#endif // EVALUATOR_DEBUG

        switch (state) {
        case Init:
            tokenStart = i;
            tokenText = "";
            tokenUnit = "";
            state = Start;

            // State variables reset
            expStart = -1;
            expText = "";
            numberBase = 10;
            expBase = 0;

            // C++17: [[fallthrough]];
            // fall through

        case Start:
            // Skip any whitespaces.
            if (ch.isSpace())
                ++i;
            else if (ch == MathDsl::CommentSep) // Comment.
                state = Finish;
            else if (ch.isDigit()) {
                // Check for number
                state = InNumberPrefix;
            } else if (ch == MathDsl::HexPrefixAl1) {
                // Simple hexadecimal notation
                tokenText.append(MathDsl::HexPrefix);
                numberBase = 16;
                state = InNumber;
                ++i;
            } else if (isRadixChar(ch)) {
                // Radix character?
                tokenText.append(ch);
                numberBase = 10;
                state = InNumber;
                ++i;
            } else if (unitBracketDepth == 0 && isDegreeSign(ch)) {
                // Outside explicit unit brackets, distinguish sexagesimal
                // degree starts from temperature/unit identifiers:
                // - "°34", "°'56" -> number
                // - "°C", "°F"    -> identifier
                const QChar next = ex.at(i + 1);
                if (next.isLetter()
                    || next == UnicodeChars::MicroSign
                    || next == UnicodeChars::GreekCapitalOmega) {
                    // Seed identifier token with the degree sign to avoid
                    // stalling on the same character in InIdentifier state.
                    tokenText.append(ex.at(i++));
                    state = InIdentifier;
                } else {
                    state = InNumberPrefix;
                }
            } else if (unitBracketDepth == 0 && isArcTime(ch)) {
                // Sexagesimal/time notation can start with degree markers,
                // e.g. "°34" or "°'56".
                state = InNumberPrefix;
            } else if (ch == '~') {
                int tokenSize = ++i - tokenStart;
                tokens.append(Token(Token::stxOperator, "~", tokenStart, tokenSize));
                state = Init;
            } else if (unitBracketDepth > 0 && isUnitArcSymbol(ch)) {
                // Inside explicit unit brackets, arcminute/arcsecond symbols
                // are unit identifiers, not sexagesimal separators.
                state = InIdentifier;
            } else if (isSeparatorChar(ch)) {
                // Leading separator, probably a number
                state = InNumberPrefix;
            } else if (ch.isNull()) // Terminator character.
                state = Finish;
            else if (isIdentifierStart(ch)
                     || (unitBracketDepth > 0
                         && (isDegreeSign(ch) || isUnitArcSymbol(ch)))) // Identifier or alphanumeric operator
                state = InIdentifier;
            else { // Look for operator match.
                int op;
                QString s;
                s = QString(ch).append(ex.at(i+1));
                op = matchOperator(s);
                // Check for one-char operator.
                if (op == Token::Invalid) {
                    s = QString(ch);
                    op = matchOperator(s);
                }
                // Any matched operator?
                if (op != Token::Invalid) {
                    switch(op) {
                        case Token::AssociationStart:
                            type = Token::stxOpenPar;
                            break;
                        case Token::AssociationEnd:
                            type = Token::stxClosePar;
                            break;
                        case Token::ListSeparator:
                            type = Token::stxSep;
                            break;
                        default: type = Token::stxOperator;
                    }
                    int len = s.length();
                    i += len;
                    int tokenSize = i - tokenStart;
                    const QString opText = s.left(len);
                    tokens.append(Token(type, opText, tokenStart, tokenSize));
                    if (opText == QString(MathDsl::UnitStart)) {
                        ++unitBracketDepth;
                    } else if (opText == QString(MathDsl::UnitEnd)) {
                        if (unitBracketDepth == 0) {
                            state = Bad;
                            break;
                        }
                        --unitBracketDepth;
                    }
                    state = Init;
                }
                else
                    state = Bad;
            }
            break;

        // Manage both identifier and alphanumeric operators.
        case InIdentifier:
            // Consume as long as alpha, dollar sign, underscore, or digit.
            if (isIdentifierContinue(ch)
                || (unitBracketDepth > 0
                    && (isDegreeSign(ch) || isUnitArcSymbol(ch))))
                tokenText.append(ex.at(i++));
            else { // We're done with identifier.
                QString identifier = tokenText;
                const bool isUnitContextIdentifier = unitBracketDepth > 0;
                if (isUnitContextIdentifier
                    && identifier.startsWith(QLatin1Char('u'))
                    && identifier.size() > 1)
                {
                    QString microIdentifier(identifier);
                    microIdentifier[0] = UnicodeChars::MicroSign;
                    if (s_isBuiltInUnitIdentifier(microIdentifier))
                        identifier = microIdentifier;
                }
                int tokenSize = i - tokenStart;
                if (!isUnitContextIdentifier && matchOperator(identifier)) {
                    tokens.append(Token(Token::stxOperator, identifier,
                                        tokenStart, tokenSize));
                } else {
                    const Token::Type identifierType = isUnitContextIdentifier
                        ? Token::stxUnitIdentifier
                        : Token::stxIdentifier;
                    tokens.append(Token(identifierType, identifier, tokenStart, tokenSize));
                }
                state = Init;
            }
            break;

        // Find out the number base.
        case InNumberPrefix:
            if (ch.isDigit()) {
                // Only consume the first digit and the second digit
                // if the first was 0.
                tokenText.append(ex.at(i++));
                if (tokenText != "0") {
                    numberBase = 10;
                    state = InNumber;
                }
            } else if (isArcTime(ch)) {
                // Time/Degree notation
                numberBase = 10;
                tokenText.append(ex.at(i++));
                state = InNumber;
            } else if (isExponent(ch, numberBase)) {
                if (tokenText.endsWith("0")) {
                    // Maybe exponent (tokenText is "0" or "-0").
                    numberBase = 10;
                    expText = ch.toUpper();
                    expStart = i;
                    ++i;
                    state = InExpIndicator;
                } else {
                    // Only leading separators.
                    state = Bad;
                }
            } else if (isRadixChar(ch)) {
                // Might be a radix point or a separator.
                // Collect it and decide later.
                tokenText.append(ch);
                numberBase = 10;
                state = InNumber;
                ++i;
            } else if (ch.toUpper() == 'X' && tokenText == "0") {
                // Hexadecimal number.
                numberBase = 16;
                tokenText.append('x');
                ++i;
                state = InNumber;
            } else if (ch.toUpper() == 'B' && tokenText == "0") {
                // Binary number.
                numberBase = 2;
                tokenText.append('b');
                ++i;
                state = InNumber;
            } else if (ch.toUpper() == 'O' && tokenText == "0") {
                // Octal number.
                numberBase = 8;
                tokenText.append('o');
                ++i;
                state = InNumber;
            } else if (ch.toUpper() == 'D' && tokenText == "0") {
                // Decimal number (with prefix).
                numberBase = 10;
                tokenText.append('d');
                ++i;
                state = InNumber;
            } else if (isSeparatorChar(ch)) {
                // Ignore thousand separators.
                ++i;
            } else if (tokenText.isEmpty() && (ch == '+' || isMinus(ch))) {
                // Allow expressions like "$-10" or "$+10".
                if (isMinus(ch))
                    tokenText.append(MathDsl::SubOpAl1);
                ++i;
            } else {
                if (tokenText.endsWith("0")) {
                    // Done with integer number (tokenText is "0" or "-0").
                    numberBase = 10;
                    state = InNumberEnd;
                } else {
                    // Only leading separators.
                    state = Bad;
                }
            }
            break;

        // Parse the number digits.
        case InNumber: {
            ushort c = ch.unicode();
            bool isDigit = c < DIGIT_MAP_COUNT && (s_digitMap[c] <= numberBase);

            if (isDigit) {
                // Consume as long as it's a digit
                tokenText.append(ex.at(i++).toUpper());
            } else if (isArcTime(ch)) {
                // Time/Degree notation
                tokenText.append(ex.at(i++));
            } else if (isExponent(ch, numberBase)) {
                // Maybe exponent
                expText = ch.toUpper();
                expStart = i;
                ++i;
                tokenText = fixNumberRadix(tokenText);
                if (!tokenText.isNull())
                    state = InExpIndicator;
                else
                    state = Bad;
            } else if (isRadixChar(ch)) {
                // Might be a radix point or a separator.
                // Collect it and decide later.
                tokenText.append(ch);
                ++i;
            } else if (isSeparatorChar(ch)) {
                // Ignore thousand separators.
                ++i;
            } else {
                // We're done with number.
                if (numberBase == 10)
                    tokenText = fixSexagesimal(tokenText, tokenUnit);
                else
                    tokenText = fixNumberRadix(tokenText);
                if (!tokenText.isNull())
                    state = InNumberEnd;
                else
                    state = Bad;
            }

            break;
        }

        // Validate exponent start.
        case InExpIndicator: {
            ushort c = ch.unicode();
            bool isDigit = c < DIGIT_MAP_COUNT && (s_digitMap[c] <= numberBase);

            if (expBase == 0) {
                // Set the default exponent base (same as number)
                expBase = numberBase;
            }

            if (expText.length() == 1 && (ch == '+' || isMinus(ch))) {
                // Possible + or - right after E.
                expText.append(ch == MathDsl::SubOp ? MathDsl::SubOpAl1 : ch);
                ++i;
            } else if (isDigit) {
                if (ch == '0') {
                    // Might be a base prefix
                    expText.append(ch);
                    ++i;
                    state = InExponentBase;
                } else {
                    // Parse the exponent absolute value.
                    tokenText.append(expText);
                    state = InExponent;
                }
            } else if (isSeparatorChar(ch)) {
                // Ignore thousand separators.
                ++i;
            } else {
                // Invalid thing here. Rollback: might be an identifier
                // used in implicit multiplication.
                i = expStart;
                state = InNumberEnd;
            }

            break;
        }

        // Detect exponent base.
        case InExponentBase: {
            int base = -1;
            switch (ch.toUpper().unicode()) {
                case 'B':
                    base = 2;
                    break;
                case 'O':
                    base = 8;
                    break;
                case 'D':
                    base = 10;
                    break;
                case 'X':
                    base = 16;
                    break;
            }

            if (base != -1) {
                // Specific exponent base found
                expBase = base;
                tokenText.append(expText);
                tokenText.append(ch.toLower());
                ++i;
            } else {
                // No exponent base specified, use the default one
                tokenText.append(expText);
            }

            state = InExponent;

            break;
        }

        // Parse exponent.
        case InExponent: {
            ushort c = ch.unicode();
            bool isDigit = c < DIGIT_MAP_COUNT && (s_digitMap[c] <= expBase);

            if (isDigit) {
                // Consume as long as it's a digit.
                tokenText.append(ex.at(i++));
            } else if (isArcTime(ch)) {
                // Allow sexagesimal markers right after scientific notation,
                // e.g. "8.4381406e4\"" or "8.4381406e4″".
                tokenText.append(ex.at(i++));
                state = InNumber;
            } else if (isSeparatorChar(ch)) {
                // Ignore thousand separators.
                ++i;
            } else {
                // We're done with floating-point number.
                state = InNumberEnd;
            };

            break;
        }

        case InNumberEnd: {
            int tokenSize = i - tokenStart;
            if (!tokenUnit.isEmpty()) {
                // Keep the sexagesimal number and its generated unit together:
                // e.g. "1:30 / 4:00" should parse as
                // "90[minute] / 4[hour]", not "(90 minute / 4) hour".
                tokens.append(Token(Token::stxNumber, tokenText,
                                    tokenStart, tokenSize));
                tokens.append(Token(Token::stxOpenPar, "[", tokenStart + tokenSize, 0));
                tokens.append(Token(Token::stxUnitIdentifier, tokenUnit,
                    tokenStart + tokenSize, 0));
                tokens.append(Token(Token::stxClosePar, "]", tokenStart + tokenSize, 0));
            } else {
                tokens.append(Token(Token::stxNumber, tokenText,
                                    tokenStart, tokenSize));
            }

            // Make sure a number cannot be followed by another number.
            if (ch.isDigit() || isRadixChar(ch) || ch == MathDsl::HexPrefixAl1)
                state = Bad;
            else
                state = Init;
            break;
        }

        case Bad:
            tokens.setValid(false);
            break;

        default:
            break;
        };
    }

    if (unitBracketDepth != 0)
        state = Bad;

    if (state == Bad)
        // Invalidating here too, because usually when we set state to Bad,
        // the case Bad won't be run.
        tokens.setValid(false);

    return tokens;
}

void Evaluator::compile(const Tokens& tokens)
{
#ifdef EVALUATOR_DEBUG
    QFile debugFile("eval.log");
    if (!debugFile.open(QIODevice::WriteOnly))
        qWarning() << "Could not open eval.log for writing:" << debugFile.errorString();
    QTextStream dbg(&debugFile);
#endif

    // Initialize variables.
    m_dirty = false;
    auto isGeneratedSexagesimalUnitToken = [](const Token& token) {
        if (token.type() != Token::stxUnitIdentifier || token.size() != 0)
            return false;
        const QString text = token.text();
        return text == unitName(UnitId::Hour)
            || text == unitName(UnitId::Minute)
            || text == unitName(UnitId::Second)
            || text == unitName(UnitId::Degree)
            || text == unitName(UnitId::Arcminute)
            || text == unitName(UnitId::Arcsecond);
    };

    bool hasGeneratedSexagesimalUnit = false;
    bool hasUnitConversionToken = false;
    for (int idx = 0; idx < tokens.size(); ++idx) {
        if (isGeneratedSexagesimalUnitToken(tokens.at(idx))) {
            hasGeneratedSexagesimalUnit = true;
        } else if (tokens.at(idx).isOperator()
                   && tokens.at(idx).asOperator() == Token::UnitConversion) {
            hasUnitConversionToken = true;
        }
    }

    m_valid = false;
    m_codes.clear();
    m_implicitMultiplicationOpcodeIndices.clear();
    m_constants.clear();
    m_constantTexts.clear();
    m_identifiers.clear();
    m_error = QString();
    m_interpretedExpression = QString();
    m_hasImplicitMultiplication = false;

    // Sanity check.
    if (tokens.count() == 0)
        return;

    TokenStack syntaxStack;
    QStack<int> argStack;
    unsigned argCount = 1;

    for (int i = 0; i <= tokens.count() && !syntaxStack.hasError(); ++i) {
        // Helper token: Invalid is end-of-expression.
        auto token = (i < tokens.count()) ? tokens.at(i)
                                          : Token(Token::stxOperator);
        auto tokenType = token.type();
        if (tokenType >= Token::stxOperator)
            tokenType = Token::stxOperator;

#ifdef EVALUATOR_DEBUG
        dbg << "\nToken: " << token.description() << "\n";
#endif

        // Unknown token is invalid.
        if (tokenType == Token::stxUnknown)
            break;

        // Try to apply all parsing rules.
#ifdef EVALUATOR_DEBUG
        dbg << "\tChecking rules..." << "\n";
#endif
        // Repeat until no more rule applies.
        bool argHandled = false;
        while (!syntaxStack.hasError()) {
            bool ruleFound = false;

            // Rule for function last argument: id (arg) -> arg.
            if (!ruleFound && syntaxStack.itemCount() >= 4) {
                Token par2 = syntaxStack.top();
                Token arg = syntaxStack.top(1);
                Token par1 = syntaxStack.top(2);
                Token id = syntaxStack.top(3);
                if (par2.asOperator() == Token::AssociationEnd
                    && arg.isOperand()
                    && par1.asOperator() == Token::AssociationStart
                    && par1.text() == QLatin1String("(")
                    && par2.text() == QLatin1String(")")
                    && id.isIdentifier())
                {
                    ruleFound = true;
                    syntaxStack.reduce(4, MAX_PRECEDENCE);
                    Opcode functionCall(Opcode::Function, argCount);
                    if (s_isSummationFunctionIdentifier(id.text()) && argCount == 3)
                    {
                        const QString argText = m_expression.mid(arg.pos(), arg.size());
                        const QStringList argList = splitTopLevelFunctionArguments(argText);
                        if (argList.count() == 3)
                            functionCall.text = argList.at(2);
                    } else if (s_isMolmassFunctionIdentifier(id.text()) && argCount == 1) {
                        functionCall.text = m_expression.mid(arg.pos(), arg.size());
                    } else if (s_isMassFunctionIdentifier(id.text()) && argCount == 2) {
                        const QString argText = m_expression.mid(arg.pos(), arg.size());
                        const QStringList argList = splitTopLevelFunctionArguments(argText);
                        if (argList.count() == 2)
                            functionCall.text = argList.at(1);
                    }
                    m_codes.append(functionCall);
#ifdef EVALUATOR_DEBUG
                        dbg << "\tRule for function last argument "
                            << argCount << " \n";
#endif
                    argCount = argStack.empty() ? 0 : argStack.pop();
                }
            }

            // Are we entering a function? If token is operator,
            // and stack already has: id (arg.
            if (!ruleFound && !argHandled && tokenType == Token::stxOperator
                 && syntaxStack.itemCount() >= 3)
            {
                Token arg = syntaxStack.top();
                Token par = syntaxStack.top(1);
                Token id = syntaxStack.top(2);
                if (arg.isOperand()
                    && par.asOperator() == Token::AssociationStart
                    && par.text() == QLatin1String("(")
                    && id.isIdentifier())
                {
                    ruleFound = true;
                    argStack.push(argCount);
#ifdef EVALUATOR_DEBUG
                    dbg << "\tEntering new function, pushing argcount="
                        << argCount << " of parent function\n";
#endif
                    argCount = 1;
                    break;
                }
           }

           // Rule for list or matrix literal last element: {arg} -> arg.
           if (!ruleFound && syntaxStack.itemCount() >= 3) {
               Token right = syntaxStack.top();
               Token arg = syntaxStack.top(1);
               Token left = syntaxStack.top(2);
               if (s_isClosingListBraceToken(right)
                   && arg.isOperand()
                   && s_isOpeningListBraceToken(left))
               {
                   ruleFound = true;
                   syntaxStack.reduce(3, MAX_PRECEDENCE);
                   m_codes.append(Opcode(Opcode::List, argCount));
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for list literal with "
                       << argCount << " elements\n";
#endif
                   argCount = argStack.empty() ? 0 : argStack.pop();
               }
           }

           // Are we entering a list literal?
           if (!ruleFound && !argHandled && tokenType == Token::stxOperator
                && syntaxStack.itemCount() >= 2)
           {
               Token arg = syntaxStack.top();
               Token left = syntaxStack.top(1);
               if (arg.isOperand()
                   && s_isOpeningListBraceToken(left))
               {
                   ruleFound = true;
                   argStack.push(argCount);
                   argCount = 1;
                   break;
               }
           }

           // Rule for postfix operators: Y POSTFIX -> Y.
           // Condition: Y is not an operator, POSTFIX is a postfix op.
           // Since we evaluate from left to right,
           // we need not check precedence at this point.
           if (!ruleFound && syntaxStack.itemCount() >= 2) {
               Token postfix = syntaxStack.top();
               Token y = syntaxStack.top(1);
               if (postfix.isOperator() && y.isOperand()) {
                   switch (postfix.asOperator()) {
                   case Token::Factorial:
                       ruleFound = true;
                       syntaxStack.reduce(2);
                       m_codes.append(Opcode(Opcode::Fact));
                       break;
                   case Token::Percent:
                       ruleFound = true;
                       syntaxStack.reduce(2);
                       m_codes.append(Opcode(Opcode::Percent));
                       break;
                   default:;
                   }
               }
#ifdef EVALUATOR_DEBUG
               if (ruleFound) {
                   dbg << "\tRule for postfix operator "
                       << postfix.text() << "\n";
               }
#endif
           }

           // Rule for explicit unit attachment: X [U] -> X.
           if (!ruleFound && syntaxStack.itemCount() >= 3) {
               Token right = syntaxStack.top();
               Token left = syntaxStack.top(1);
               Token lhs = syntaxStack.top(2);
               if (lhs.isOperand()
                   && s_isClosingUnitBracketToken(right)
                   && s_isOpeningUnitBracketToken(left))
               {
                   ruleFound = true;
                   syntaxStack.reduce(3, std::move(lhs), MAX_PRECEDENCE);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for empty unit attachment X[] -> X\n";
#endif
               }
           }

           // Rule for explicit unit attachment: X [U] -> X.
           if (!ruleFound && syntaxStack.itemCount() >= 4) {
               Token right = syntaxStack.top();
               Token unitExpr = syntaxStack.top(1);
               Token left = syntaxStack.top(2);
               Token lhs = syntaxStack.top(3);
               if (lhs.isOperand()
                   && unitExpr.isOperand()
                   && s_isClosingUnitBracketToken(right)
                   && s_isOpeningUnitBracketToken(left))
               {
                   ruleFound = true;
                   syntaxStack.reduce(4, std::move(lhs), MAX_PRECEDENCE);
                   m_codes.append(Opcode(Opcode::Unit));
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for unit attachment X[U] -> X\n";
#endif
               }
           }

           // Rule for parenthesized explicit unit attachment:
           // X * ([U]) -> X.
           if (!ruleFound && syntaxStack.itemCount() >= 5) {
               Token rightPar = syntaxStack.top();
               Token innerExpr = syntaxStack.top(1);
               Token leftPar = syntaxStack.top(2);
               Token op = syntaxStack.top(3);
               Token lhs = syntaxStack.top(4);
               if (lhs.isOperand()
                   && innerExpr.isOperand()
                   && op.asOperator() == Token::Multiplication
                   && rightPar.asOperator() == Token::AssociationEnd
                   && rightPar.text() == QLatin1String(")")
                   && leftPar.asOperator() == Token::AssociationStart
                   && leftPar.text() == QLatin1String("("))
               {
                   const QString innerSource = m_expression.mid(innerExpr.pos(), innerExpr.size()).trimmed();
                   if (innerSource.startsWith(MathDsl::UnitStart)
                       && innerSource.endsWith(MathDsl::UnitEnd))
                   {
                       ruleFound = true;
                       syntaxStack.reduce(5, std::move(lhs), MAX_PRECEDENCE);
                       m_codes.append(Opcode(Opcode::Unit));
#ifdef EVALUATOR_DEBUG
                       dbg << "\tRule for parenthesized unit attachment X*([U]) -> X\n";
#endif
                   }
               }
           }

           // Rule for parenthesis: (Y) -> Y.
           if (!ruleFound && syntaxStack.itemCount() >= 3) {
               Token right = syntaxStack.top();
               Token y = syntaxStack.top(1);
               Token left = syntaxStack.top(2);
               if (y.isOperand()
                   && right.asOperator() == Token::AssociationEnd
                   && left.asOperator() == Token::AssociationStart
                   && !s_isOpeningListBraceToken(left)
                   && !s_isClosingListBraceToken(right))
               {
                   ruleFound = true;
                   syntaxStack.reduce(3, MAX_PRECEDENCE);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for (Y) -> Y" << "\n";
#endif
               }
           }

           // Rule for simplified syntax for function,
           // e.g. "sin pi" or "cos 1.2". Conditions:
           // *precedence of function reduction >= precedence of next token.
           // *or next token is not an operator.
           if (!ruleFound && syntaxStack.itemCount() >= 2) {
               Token arg = syntaxStack.top();
               Token id = syntaxStack.top(1);
               if (arg.isOperand() && isFunction(id)
                   && (!token.isOperator()
                       || opPrecedence(Token::Function) >=
                              opPrecedence(token.asOperator())))
               {
                   ruleFound = true;
                   m_codes.append(Opcode(Opcode::Function, 1));
                   syntaxStack.reduce(2);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for simplified function syntax; function "
                       << id.text() << "\n";
#endif
               }
           }

           // Rule for unary operator in simplified function syntax.
           // This handles cases like "sin -90".
           if (!ruleFound && syntaxStack.itemCount() >= 3) {
               Token x = syntaxStack.top();
               Token op = syntaxStack.top(1);
               Token id = syntaxStack.top(2);
               if (x.isOperand() && isFunction(id)
                   && (op.asOperator() == Token::Addition
                   || op.asOperator() == Token::Subtraction
                   || op.asOperator() == Token::BitwiseLogicalNOT))
               {
                   ruleFound = true;
                   syntaxStack.reduce(2);
                   if (op.asOperator() == Token::Subtraction)
                        m_codes.append(Opcode(Opcode::Neg));
                    else if (op.asOperator() == Token::BitwiseLogicalNOT)
                        m_codes.append(Opcode(Opcode::BNot));
#ifdef EVALUATOR_DEBUG
                     dbg << "\tRule for unary operator in simplified "
                            "function syntax; function " << id.text() << "\n";
#endif
               }
           }

           // Rule for function arguments. If token is ; or ):
           // id (arg1 ; arg2 -> id (arg.
           // Must come before binary op rule, special case of the latter.
           if (!ruleFound && syntaxStack.itemCount() >= 5
               && token.isOperator()
               && (token.asOperator() == Token::AssociationEnd
                   || token.asOperator() == Token::ListSeparator))
           {
               Token arg2 = syntaxStack.top();
               Token sep = syntaxStack.top(1);
               Token arg1 = syntaxStack.top(2);
               Token par = syntaxStack.top(3);
               Token id = syntaxStack.top(4);
               if (arg2.isOperand()
                   && sep.asOperator() == Token::ListSeparator
                   && arg1.isOperand()
                   && par.asOperator() == Token::AssociationStart
                   && par.text() == QLatin1String("(")
                   && id.isIdentifier())
               {
                   ruleFound = true;
                   argHandled = true;
                   syntaxStack.reduce(3, MAX_PRECEDENCE);
                   ++argCount;
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for function argument "
                       << argCount << " \n";
#endif
               }
           }

           // Rule for list literal elements. If token is ; or }:
           // {arg1 ; arg2 -> {arg.
           if (!ruleFound && syntaxStack.itemCount() >= 4
               && token.isOperator()
               && (token.asOperator() == Token::AssociationEnd
                   || token.asOperator() == Token::ListSeparator))
           {
               Token arg2 = syntaxStack.top();
               Token sep = syntaxStack.top(1);
               Token arg1 = syntaxStack.top(2);
               Token left = syntaxStack.top(3);
               if (arg2.isOperand()
                   && sep.asOperator() == Token::ListSeparator
                   && arg1.isOperand()
                   && s_isOpeningListBraceToken(left))
               {
                   ruleFound = true;
                   argHandled = true;
                   syntaxStack.reduce(3, MAX_PRECEDENCE);
                   ++argCount;
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for list literal element "
                       << argCount << "\n";
#endif
               }
           }

           // Rule for trailing function argument separator:
           // id (arg ; ) -> id (arg )
           // id (arg ;   -> id (arg   (end-of-expression)
           // This keeps the already parsed argument list and discards
           // an empty trailing argument.
           if (!ruleFound && syntaxStack.itemCount() >= 4
               && token.isOperator()
               && (token.asOperator() == Token::AssociationEnd
                   || token.asOperator() == Token::Invalid))
           {
               Token sep = syntaxStack.top();
               Token arg = syntaxStack.top(1);
               Token par = syntaxStack.top(2);
               Token id = syntaxStack.top(3);
               if (sep.asOperator() == Token::ListSeparator
                   && arg.isOperand()
                   && par.asOperator() == Token::AssociationStart
                   && par.text() == QLatin1String("(")
                   && id.isIdentifier())
               {
                   ruleFound = true;
                   argHandled = true;
                   syntaxStack.reduce(2, std::move(arg), MAX_PRECEDENCE);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for trailing function argument separator\n";
#endif
               }
           }

           // Rule for function call with parentheses,
           // but without argument, e.g. "2*PI()".
           if (!ruleFound && syntaxStack.itemCount() >= 3) {
               Token par2 = syntaxStack.top();
               Token par1 = syntaxStack.top(1);
               Token id = syntaxStack.top(2);
               if (par2.asOperator() == Token::AssociationEnd
                   && par1.asOperator() == Token::AssociationStart
                   && par1.text() == QLatin1String("(")
                   && par2.text() == QLatin1String(")")
                   && id.isIdentifier())
               {
                   ruleFound = true;
                   syntaxStack.reduce(3, MAX_PRECEDENCE);
                   m_codes.append(Opcode(Opcode::Function, 0));
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for function call with parentheses, "
                          "but without argument\n";
#endif
               }
           }

           // Rule for binary operator:  A (op) B -> A.
           // Conditions: precedence of op >= precedence of token.
           // Action: push (op) to result e.g.
           // "A * B" becomes "A" if token is operator "+".
           // Exception: for caret (power operator), if op is another caret
           // then it doesn't apply, e.g. "2^3^2" is evaluated as "2^(3^2)".
           // Exception: doesn't apply if B is a function name (to manage
           // shift/reduce conflict with simplified function syntax
           // (issue #600).
           if (!ruleFound && syntaxStack.itemCount() >= 3) {
               Token b = syntaxStack.top();
               Token op = syntaxStack.top(1);
               Token a = syntaxStack.top(2);
               const bool disallowPowerWithExplicitUnitAttachment =
                   op.asOperator() == Token::Exponentiation
                   && s_tokenSourceContainsExplicitUnitAttachment(a, m_expression);
               if (disallowPowerWithExplicitUnitAttachment) {
                   m_error = tr("syntax error");
                   m_valid = false;
                   m_constants.clear();
                   m_constantTexts.clear();
                   m_codes.clear();
                   m_identifiers.clear();
                   m_implicitMultiplicationOpcodeIndices.clear();
                   m_interpretedExpression = QString();
                   m_hasImplicitMultiplication = false;
                   return;
               }
               const bool openingUnitBracketToken =
                   token.isOperator() && s_isOpeningUnitBracketToken(token);
               const bool forceReduceCompletedConversionBeforeAddSub =
                   op.asOperator() == Token::UnitConversion
                   && token.isOperator()
                   && (token.asOperator() == Token::Addition
                       || token.asOperator() == Token::Subtraction);
               if (a.isOperand() && b.isOperand() && op.isOperator()
                   && ( // Normal operator.
                       (token.isOperator()
                           && !openingUnitBracketToken
                           && (forceReduceCompletedConversionBeforeAddSub
                               || opPrecedence(op.asOperator()) >=
                                   opPrecedence(token.asOperator()))
#ifdef ALLOW_IMPLICIT_MULT
                           && (token.asOperator() != Token::AssociationStart
                               || opPrecedence(op.asOperator()) >= opPrecedence(Token::Multiplication))
#endif
                           && !(b.isIdentifier() && token.asOperator() == Token::AssociationStart)
                           && token.asOperator() != Token::Exponentiation)

                       || ( // May represent implicit multiplication.
                           token.isOperand()
                           && opPrecedence(op.asOperator()) >=
                               opPrecedence(Token::Multiplication)))
                   && !(isFunction(b)))
               {
                   ruleFound = true;
                   switch (op.asOperator()) {
                   // Simple binary operations.
                   case Token::Addition:
                       m_codes.append(Opcode::Add);
                       break;
                   case Token::Subtraction:
                       m_codes.append(Opcode::Sub);
                       break;
                   case Token::Multiplication:
                       m_codes.append(Opcode::Mul);
                       break;
                   case Token::Division:
                       m_codes.append(Opcode::Div);
                       break;
                   case Token::Exponentiation:
                       m_codes.append(Opcode::Pow);
                       break;
                   case Token::Modulo:
                       m_codes.append(Opcode::Modulo);
                       break;
                   case Token::IntegerDivision:
                       m_codes.append(Opcode::IntDiv);
                       break;
                   case Token::ArithmeticLeftShift:
                       m_codes.append(Opcode::LSh);
                       break;
                   case Token::ArithmeticRightShift:
                       m_codes.append(Opcode::RSh);
                       break;
                   case Token::BitwiseLogicalAND:
                       m_codes.append(Opcode::BAnd);
                       break;
                   case Token::BitwiseLogicalOR:
                       m_codes.append(Opcode::BOr);
                       break;
                   case Token::UnitConversion: {
                       static const QRegularExpression unitNameNumberRE(
                           "(^[0-9e\\+\\-\\.,]|[0-9e\\.,]$)",
                           QRegularExpression::CaseInsensitiveOption
                       );
                       QString unitName =
                           m_expression.mid(b.pos(), b.size()).simplified();
                       // Make sure the whole unit name can be used
                       // as a single operand in multiplications.
                       if (b.minPrecedence() <
                               opPrecedence(Token::Multiplication))
                       {
                           unitName = "(" + unitName + ")";
                       }
                       // Protect the unit name
                       // if it starts or ends with a number.
                       else {
                           QRegularExpressionMatch match = unitNameNumberRE.match(unitName);
                           if (match.hasMatch())
                               unitName = "(" + unitName + ")";
                       }
                       m_codes.append(Opcode(Opcode::Conv, unitName));
                       break;
                   }
                   default: break;
                   };
                   syntaxStack.reduce(3);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for binary operator" << "\n";
#endif
               }
           }

#ifdef ALLOW_IMPLICIT_MULT
           // Rule for implicit multiplication.
           // Action: Treat as A * B.
           // Exception: doesn't apply if B is a function name
           // (to manage shift/reduce conflict with simplified
           // function syntax (fixes issue #600).
           if (!ruleFound && syntaxStack.itemCount() >= 2) {
               Token b = syntaxStack.top();
               Token a = syntaxStack.top(1);

               if (a.isOperand() && b.isOperand()
                   && token.asOperator() != Token::AssociationStart
                   && ( // Token is normal operator.
                        (token.isOperator()
                            && !s_isOpeningUnitBracketToken(token)
                            && opPrecedence(Token::Multiplication) >=
                                   opPrecedence(token.asOperator()))
                        || token.isOperand()) // Implicit multiplication.
                   && !isFunction(b))
               {
                   ruleFound = true;
                   syntaxStack.reduce(2, opPrecedence(Token::Multiplication));
                   m_implicitMultiplicationOpcodeIndices.insert(m_codes.count());
                   m_codes.append(Opcode::Mul);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for implicit multiplication" << "\n";
#endif
               }

           }
#endif

           // Rule for unary operator:  (op1) (op2) X -> (op1) X.
           // Conditions: op2 is unary.
           // Current token has lower precedence than multiplication.
           if (!ruleFound
               && token.asOperator() != Token::AssociationStart
               && syntaxStack.itemCount() >= 3)
           {
               Token x = syntaxStack.top();
               Token op2 = syntaxStack.top(1);
               Token op1 = syntaxStack.top(2);
               if (x.isOperand() && op1.isOperator()
                   && (op2.asOperator() == Token::Addition
                       || op2.asOperator() == Token::Subtraction
                       || op2.asOperator() == Token::BitwiseLogicalNOT)
                   && !(token.isOperand() && isFunction(x))
                   && (token.isOperand()
                       || opPrecedence(token.asOperator()) <=
                              opPrecedence(Token::Multiplication)))
               {
                   ruleFound = true;
                   if (op2.asOperator() == Token::Subtraction)
                       m_codes.append(Opcode(Opcode::Neg));
                   else if (op2.asOperator() == Token::BitwiseLogicalNOT)
                       m_codes.append(Opcode(Opcode::BNot));

                   syntaxStack.reduce(2);
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for unary operator" << op2.text() << "\n";
#endif
               }
           }

           // Auxiliary rule for unary prefix operator:  (op) X -> X.
           // Conditions: op is unary, op is first in syntax stack.
           // Action: create code for (op). Unary MINUS or PLUS are
           // treated with the precedence of multiplication.
           if (!ruleFound
               && token.asOperator() != Token::AssociationStart
               && syntaxStack.itemCount() == 2)
           {
               Token x = syntaxStack.top();
               Token op = syntaxStack.top(1);
               if (x.isOperand()
                   && (op.asOperator() == Token::Addition
                       || op.asOperator() == Token::Subtraction
                       || op.asOperator() == Token::BitwiseLogicalNOT)
                   && !(token.isOperand() && isFunction(x))
                   && ((token.isOperator()
                           && opPrecedence(token.asOperator()) <=
                                  opPrecedence(Token::Multiplication))
                       || token.isOperand()))
               {
                   ruleFound = true;
                   if (op.asOperator() == Token::Subtraction)
                       m_codes.append(Opcode(Opcode::Neg));
                   else if (op.asOperator() == Token::BitwiseLogicalNOT)
                       m_codes.append(Opcode(Opcode::BNot));
#ifdef EVALUATOR_DEBUG
                   dbg << "\tRule for unary operator (auxiliary)" << "\n";
#endif
                   syntaxStack.reduce(2);
               }
           }

           if (!ruleFound)
               break;
        }

        // Can't apply rules anymore, push the token.
        syntaxStack.push(token);

        // For identifiers, generate code to load from references.
        if (tokenType == Token::stxIdentifier
            || tokenType == Token::stxUnitIdentifier)
        {
            m_identifiers.append(token.text());
            const Opcode::Type refOpcodeType =
                tokenType == Token::stxUnitIdentifier
                ? Opcode::UnitRef
                : Opcode::Ref;
            m_codes.append(Opcode(refOpcodeType, m_identifiers.count() - 1));
#ifdef EVALUATOR_DEBUG
            dbg << "\tPush " << token.text() << " to identifier pools" << "\n";
#endif
        }

        // For constants, generate code to load from a constant.
        if (tokenType == Token::stxNumber) {
            m_constants.append(token.asNumber());
            m_constantTexts.append(token.text());
            m_codes.append(Opcode(Opcode::Load, m_constants.count() - 1));
#ifdef EVALUATOR_DEBUG
            dbg << "\tPush " << token.asNumber()
                << " to constant pools" << "\n";
#endif
        }
    }

    m_valid = false;
    if (syntaxStack.hasError())
        m_error = syntaxStack.error();
    // syntaxStack must left only one operand
    // and end-of-expression (i.e. Invalid).
    else if (syntaxStack.itemCount() == 2
             && syntaxStack.top().isOperator()
             && syntaxStack.top().asOperator() == Token::Invalid
             && !syntaxStack.top(1).isOperator())
    {
        m_valid = true;

        m_hasImplicitMultiplication = !m_implicitMultiplicationOpcodeIndices.isEmpty();

        m_interpretedExpression = hasGeneratedSexagesimalUnit && !hasUnitConversionToken
            ? QString()
            : buildInterpretedExpressionFromOpcodes();
    }

#ifdef EVALUATOR_DEBUG
    dbg << "Dump: " << dump() << "\n";
    debugFile.close();
#endif

    // Bad parsing? Clean-up everything.
    if (!m_valid) {
        m_constants.clear();
        m_constantTexts.clear();
        m_codes.clear();
        m_identifiers.clear();
        m_implicitMultiplicationOpcodeIndices.clear();
        m_interpretedExpression = QString();
        m_hasImplicitMultiplication = false;
    }
}

QString Evaluator::buildInterpretedExpressionFromOpcodes() const
{
    struct RenderNode {
        QString text;
        int precedence;
        Opcode::Type rootOpcode;
        bool isLiteralSymbol;
        bool isNumericOnly;
        bool isUnaryNegation;
    };

    QVector<RenderNode> stack;
    QHash<int, QString> refs;

    auto wrapInParentheses = [](const QString& text) {
        return QStringLiteral("(%1)").arg(text);
    };
    auto makeUnaryNode = [&wrapInParentheses](const QString& symbol,
                                              const RenderNode& operand,
                                              int precedence) {
        QString operandText = operand.text;
        if (operand.precedence <= precedence)
            operandText = wrapInParentheses(operandText);
        return RenderNode{
            symbol + operandText,
            precedence,
            Opcode::Nop,
            operand.isLiteralSymbol,
            operand.isNumericOnly,
            symbol == QLatin1String("-")
        };
    };
    auto makeBinaryNode = [&wrapInParentheses, this](const RenderNode& left,
                                                     const RenderNode& right,
                                                     Opcode::Type opcodeType,
                                                     int opcodeIndex) {
        if (opcodeType == Opcode::Unit) {
            const int precedence = opcodePrecedence(opcodeType);
            QString leftText = left.text;
            if (left.precedence < precedence)
                leftText = wrapInParentheses(leftText);
            return RenderNode{
                leftText + MathDsl::UnitStart + right.text + MathDsl::UnitEnd,
                precedence,
                opcodeType,
                left.isLiteralSymbol || right.isLiteralSymbol,
                left.isNumericOnly && right.isNumericOnly,
                false
            };
        }

        Opcode::Type renderedOpcodeType = opcodeType;
        QString rightText = right.text;
        if (opcodeType == Opcode::Sub && right.isUnaryNegation) {
            // Prefer "a+b" over "a-(-b)" / "a--b" in interpreted output.
            renderedOpcodeType = Opcode::Add;
            if (rightText.startsWith(MathDsl::SubOpAl1))
                rightText.remove(0, 1);
        }

        const int precedence = opcodePrecedence(renderedOpcodeType);
        const bool isRightAssociative = isRightAssociativeOpcode(opcodeType);
        const bool isImplicitMultiplication =
            opcodeType == Opcode::Mul
            && m_implicitMultiplicationOpcodeIndices.contains(opcodeIndex);
        QString leftText = left.text;
        if (left.precedence < precedence
            || (isRightAssociative && left.precedence == precedence))
        {
            leftText = wrapInParentheses(leftText);
        }
        if (right.precedence < precedence
            || (!isRightAssociative && right.precedence == precedence))
        {
            rightText = wrapInParentheses(rightText);
        }
        // In contextual percent expressions, make the additive base explicit:
        // render "1+99+10%" as "(1+99)+10%".
        if ((opcodeType == Opcode::Add || opcodeType == Opcode::Sub)
            && right.rootOpcode == Opcode::Percent
            && (left.rootOpcode == Opcode::Add || left.rootOpcode == Opcode::Sub)
            && !isWrappedInOuterParentheses(leftText))
        {
            leftText = wrapInParentheses(leftText);
        }

        const bool leftIsAtomicNumericForGrouping =
            left.isNumericOnly
            && left.rootOpcode == Opcode::Nop
            && !left.isLiteralSymbol;
        const bool rightIsAtomicNumericForGrouping =
            right.isNumericOnly
            && right.rootOpcode == Opcode::Nop
            && !right.isLiteralSymbol;

        // Make "a / b c" style parses explicit as "(a / b) * c", except
        // for simple numeric factors where a flat mul/div chain is clearer.
        if (opcodeType == Opcode::Mul
            && isImplicitMultiplication
            && (left.rootOpcode == Opcode::Div
                || left.rootOpcode == Opcode::IntDiv
                || left.rootOpcode == Opcode::Modulo))
        {
            const bool allowFlatChainForSimpleDivisorFactor =
                left.rootOpcode == Opcode::Div && rightIsAtomicNumericForGrouping;
            if (!allowFlatChainForSimpleDivisorFactor)
                leftText = wrapInParentheses(leftText);
        }
        if (opcodeType == Opcode::Mul
            && isImplicitMultiplication
            && (right.rootOpcode == Opcode::Div
                || right.rootOpcode == Opcode::IntDiv
                || right.rootOpcode == Opcode::Modulo))
        {
            const bool allowFlatChainForSimpleDividendFactor =
                right.rootOpcode == Opcode::Div && leftIsAtomicNumericForGrouping;
            if (!allowFlatChainForSimpleDividendFactor)
                rightText = wrapInParentheses(rightText);
        }

        // For explicit multiplications, isolate most power terms to improve
        // readability in compound products (for example "1·(2^3)·3"), but
        // keep scientific-notation powers of ten flat ("x·10^n").
        if (opcodeType == Opcode::Mul && !isImplicitMultiplication) {
            static const QRegularExpression s_scientificPowerOfTenRE(
                QStringLiteral(R"(^10\^(?:[−-]?\d+|\([−-]?\d+\))$)"));
            const bool keepLeftNumericPowerUnwrappedBeforeAtomicNumber =
                left.rootOpcode == Opcode::Pow
                && left.isNumericOnly
                && rightIsAtomicNumericForGrouping;
            if (left.rootOpcode == Opcode::Pow
                && !left.isLiteralSymbol
                && !keepLeftNumericPowerUnwrappedBeforeAtomicNumber
                && !s_scientificPowerOfTenRE.match(leftText).hasMatch()
                && !isWrappedInOuterParentheses(leftText))
            {
                leftText = wrapInParentheses(leftText);
            }
            if (right.rootOpcode == Opcode::Pow
                && !right.isLiteralSymbol
                && !s_scientificPowerOfTenRE.match(rightText).hasMatch()
                && !isWrappedInOuterParentheses(rightText))
            {
                rightText = wrapInParentheses(rightText);
            }
        }

        // Keep explicit grouping for factorial denominators
        // (for example "1/(2!)").
        if (opcodeType == Opcode::Div
            && right.rootOpcode == Opcode::Fact
            && !isWrappedInOuterParentheses(rightText))
        {
            rightText = wrapInParentheses(rightText);
        }

        // Keep non-integer numeric exponents explicit (for example
        // "2^(12.5)" instead of "2^12.5").
        if (opcodeType == Opcode::Pow
            && !isWrappedInOuterParentheses(rightText))
        {
            normalizeUnsignedIntegerEquivalentDecimalText(rightText);
            const bool isNonIntegerNumericExponent =
                right.isNumericOnly
                && right.rootOpcode == Opcode::Nop
                && !isUnsignedDecimalIntegerText(rightText);
            const bool isFactorialExponent =
                right.rootOpcode == Opcode::Fact;
            const bool isChainedExponent =
                right.rootOpcode == Opcode::Pow;
            if (isNonIntegerNumericExponent || isFactorialExponent || isChainedExponent)
                rightText = wrapInParentheses(rightText);
        }

        const bool leftIsAtomicNumeric =
            left.isNumericOnly
            && left.rootOpcode == Opcode::Nop
            && !left.isLiteralSymbol;
        const bool rightIsAtomicNumeric =
            right.isNumericOnly
            && right.rootOpcode == Opcode::Nop
            && !right.isLiteralSymbol;
        const bool isNumberTimesNumber =
            opcodeType == Opcode::Mul
            && leftIsAtomicNumeric
            && rightIsAtomicNumeric;
        static const QRegularExpression s_endsWithDigitRE(QStringLiteral(R"([0-9]$)"));
        static const QRegularExpression s_endsWithDigitOrSuperscriptRE(
            QStringLiteral(R"([0-9⁰¹²³⁴⁵⁶⁷⁸⁹]$)"));
        const bool isNumericBoundaryTimesNumber =
            opcodeType == Opcode::Mul
            && !isImplicitMultiplication
            && rightIsAtomicNumeric
            && (s_endsWithDigitRE.match(leftText).hasMatch()
                || s_endsWithDigitOrSuperscriptRE.match(leftText).hasMatch());
        const bool isLiteralSymbolMultiplication =
            opcodeType == Opcode::Mul
            && !isNumberTimesNumber
            && !isNumericBoundaryTimesNumber;

        const QString infixSymbol =
            opcodeToInfixSymbol(
                renderedOpcodeType,
                isImplicitMultiplication || isLiteralSymbolMultiplication
            );
        return RenderNode{
            leftText + infixSymbol + rightText,
            precedence,
            renderedOpcodeType,
            left.isLiteralSymbol || right.isLiteralSymbol,
            left.isNumericOnly && right.isNumericOnly,
            false
        };
    };

    for (int opcodeIndex = 0; opcodeIndex < m_codes.count(); ++opcodeIndex) {
        const Opcode& opcode = m_codes.at(opcodeIndex);
        switch (opcode.type) {
        case Opcode::Nop:
            break;
        case Opcode::Load: {
            QString constantText = m_constantTexts.value(opcode.index);
            if (constantText.isEmpty()) {
                constantText = DMath::format(m_constants.value(opcode.index),
                                             Quantity::Format::Fixed());
            }
            stack.append({
                constantText,
                MAX_PRECEDENCE,
                Opcode::Nop,
                false,
                true,
                false
            });
            break;
        }
        case Opcode::Ref:
        case Opcode::UnitRef: {
            const QString identifier = m_identifiers.value(opcode.index);
            const bool isUnitReference = opcode.type == Opcode::UnitRef;
            stack.append({
                identifier,
                MAX_PRECEDENCE,
                Opcode::Nop,
                true,
                false,
                false
            });

            if (isUnitReference)
                break;

            if (m_assignArg.contains(identifier) || hasVariable(identifier))
                break;

            if (FunctionRepo::instance()->find(identifier)
                || hasUserFunction(identifier)
                || m_assignFunc)
            {
                refs.insert(stack.count(), identifier);
            }
            break;
        }
        case Opcode::Neg:
        case Opcode::BNot: {
            if (stack.isEmpty())
                return QString();
            const RenderNode operand = stack.takeLast();
            if (opcode.type == Opcode::Neg && operand.isUnaryNegation) {
                QString text = operand.text;
                if (text.startsWith(MathDsl::SubOpAl1))
                    text.remove(0, 1);
                stack.append({
                    text,
                    MAX_PRECEDENCE,
                    Opcode::Nop,
                    operand.isLiteralSymbol,
                    operand.isNumericOnly,
                    false
                });
                break;
            }
            const QString symbol = opcode.type == Opcode::Neg ? "-" : "~";
            stack.append(makeUnaryNode(symbol, operand,
                                       opcodePrecedence(opcode.type)));
            break;
        }
        case Opcode::Fact:
        case Opcode::Percent: {
            if (stack.isEmpty())
                return QString();
            const int precedence = opcodePrecedence(opcode.type);
            RenderNode operand = stack.takeLast();
            if (operand.precedence < precedence)
                operand.text = wrapInParentheses(operand.text);
            const QString symbol = opcode.type == Opcode::Fact
                ? QStringLiteral("!")
                : QStringLiteral("%");
            stack.append({
                operand.text + symbol,
                precedence,
                opcode.type,
                operand.isLiteralSymbol,
                operand.isNumericOnly,
                false
            });
            break;
        }
        case Opcode::Add:
        case Opcode::Sub:
        case Opcode::Mul:
        case Opcode::Div:
        case Opcode::Pow:
        case Opcode::Modulo:
        case Opcode::IntDiv:
        case Opcode::LSh:
        case Opcode::RSh:
        case Opcode::BAnd:
        case Opcode::BOr:
        case Opcode::Conv:
        case Opcode::Unit: {
            if (stack.size() < 2)
                return QString();
            const RenderNode right = stack.takeLast();
            const RenderNode left = stack.takeLast();
            stack.append(makeBinaryNode(left, right, opcode.type, opcodeIndex));
            break;
        }
        case Opcode::Function: {
            const int argumentCount = static_cast<int>(opcode.index);
            const int functionRefKey = stack.count() - argumentCount;
            QString functionName = refs.take(functionRefKey);
            if (functionName.isEmpty()
                && functionRefKey > 0
                && functionRefKey <= stack.count())
            {
                functionName = stack.at(functionRefKey - 1).text;
            }

            if (stack.count() < argumentCount + 1)
                return QString();

            QStringList arguments;
            for (int i = 0; i < argumentCount; ++i)
                arguments.prepend(stack.takeLast().text);

            // Discard function reference placeholder.
            stack.takeLast();

            if (functionName == QLatin1String("list")) {
                stack.append({
                    QString(MathDsl::ListStart) + arguments.join("; ") + QString(MathDsl::ListEnd),
                    MAX_PRECEDENCE,
                    Opcode::Nop,
                    false,
                    false,
                    false
                });
                break;
            }

            stack.append({
                QStringLiteral("%1(%2)").arg(
                    UnicodeChars::normalizeRootFunctionAliasesForDisplay(functionName),
                    arguments.join("; ")),
                MAX_PRECEDENCE,
                Opcode::Nop,
                true,
                false,
                false
            });
            break;
        }
        case Opcode::List: {
            const int elementCount = static_cast<int>(opcode.index);
            if (stack.count() < elementCount)
                return QString();
            QStringList elements;
            for (int i = 0; i < elementCount; ++i)
                elements.prepend(stack.takeLast().text);
            stack.append({
                QStringLiteral("{%1}").arg(elements.join("; ")),
                MAX_PRECEDENCE,
                Opcode::Nop,
                false,
                false,
                false
            });
            break;
        }
        }
    }

    if (stack.count() != 1)
        return QString();

    return stack.constLast().text;
}

bool hasMixedStandaloneExplicitAngleTermAddSub(const Tokens& tokens)
{
    if (tokens.isEmpty())
        return false;

    auto isTopLevelAddSub = [](const Token& token) {
        const Token::Operator op = token.asOperator();
        return op == Token::Addition || op == Token::Subtraction;
    };

    int parenDepth = 0;
    int unitDepth = 0;
    QVector<int> splitOps;
    for (int i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens.at(i);
        const QString text = token.text();
        if (text == QString(MathDsl::GroupStart)) {
            ++parenDepth;
            continue;
        }
        if (text == QString(MathDsl::GroupEnd)) {
            if (parenDepth > 0)
                --parenDepth;
            continue;
        }
        if (text == QString(MathDsl::UnitStart)) {
            ++unitDepth;
            continue;
        }
        if (text == QString(MathDsl::UnitEnd)) {
            if (unitDepth > 0)
                --unitDepth;
            continue;
        }
        if (parenDepth == 0 && unitDepth == 0 && isTopLevelAddSub(token))
            splitOps.append(i);
    }
    if (splitOps.isEmpty())
        return false;

    auto termHasFunctionCall = [&tokens](int start, int end) {
        for (int i = start; i < end; ++i) {
            if (!tokens.at(i).isIdentifier())
                continue;
            if (i + 1 < end && tokens.at(i + 1).text() == QString(MathDsl::GroupStart))
                return true;
        }
        return false;
    };

    auto termHasExplicitAngleUnit = [&tokens](int start, int end) {
        for (int i = start; i < end; ++i) {
            if (!tokens.at(i).isUnitIdentifier())
                continue;
            if (Units::isExplicitAngleUnitName(tokens.at(i).text()))
                return true;
        }
        return false;
    };

    bool hasStandaloneExplicitAngleTerm = false;
    bool hasNonStandaloneExplicitAngleTerm = false;
    int termStart = 0;
    for (int part = 0; part <= splitOps.size(); ++part) {
        const int termEnd = (part < splitOps.size()) ? splitOps.at(part) : tokens.size();
        if (termStart >= termEnd) {
            termStart = termEnd + 1;
            continue;
        }

        const bool hasAngleUnit = termHasExplicitAngleUnit(termStart, termEnd);
        const bool hasFunction = termHasFunctionCall(termStart, termEnd);
        if (hasAngleUnit && !hasFunction)
            hasStandaloneExplicitAngleTerm = true;
        else
            hasNonStandaloneExplicitAngleTerm = true;

        termStart = termEnd + 1;
    }

    return hasStandaloneExplicitAngleTerm && hasNonStandaloneExplicitAngleTerm;
}

Quantity Evaluator::evalNoAssign()
{
    Quantity result;

    if (m_dirty) {
        // Reset.
        m_assignId = QString();
        m_assignFunc = false;
        m_assignArg.clear();
        m_assignVarDescription = QString();
        m_assignUnitDescription = QString();
        m_assignFuncExpr = QString();
        m_assignUnitExpr = QString();
        m_assignFuncDescription = QString();
        m_assignUnit = false;
        m_interpretedExpression = QString();
        m_hasImplicitMultiplication = false;
        QString expressionToParse = m_expression;
        QString trailingComment;
        const int commentPos = m_expression.indexOf(MathDsl::CommentSep);
        if (commentPos >= 0)
            trailingComment = m_expression.mid(commentPos + 1).trimmed();
        if (m_expression.contains(MathDsl::CommentSep)) {
            QString expressionWithoutDescription;
            QString description;
            if (splitUserFunctionDescription(m_expression,
                                             &expressionWithoutDescription,
                                             &description)) {
                expressionToParse = expressionWithoutDescription;
                m_assignFuncDescription = description;
            } else if (splitVariableDescription(m_expression,
                                                &expressionWithoutDescription,
                                                &description))
            {
                expressionToParse = expressionWithoutDescription;
                m_assignVarDescription = description;
            }
        }
        Tokens tokens = scan(expressionToParse);

        // Invalid expression?
        if (!tokens.valid()) {
            m_error = tr("invalid expression");
            m_interpretedExpression = QString();
            m_hasImplicitMultiplication = false;
            return Quantity(0);
        }

        if (tokens.count() == 0 && isCommentOnlyExpression(m_expression)) {
            m_valid = true;
            m_constants.clear();
            m_constantTexts.clear();
            m_codes.clear();
            m_identifiers.clear();
            m_interpretedExpression = QString();
            m_hasImplicitMultiplication = false;
            return CMath::nan();
        }

        // User unit assignment?
        if (s_isUserUnitLhsTokens(tokens)) {
            m_assignId = tokens.at(1).text();
            m_assignUnit = true;
            m_assignUnitExpr = expressionToParse.section("=", 1, 1).trimmed();
            m_assignUnitDescription = m_assignVarDescription;
            for (int i = 0; i < 4; ++i)
                tokens.erase(tokens.begin());
        }
        // Variable assignment?
        else if (tokens.count() > 2
            && tokens.at(0).isIdentifier()
            && tokens.at(1).asOperator() == Token::Assignment)
        {
            m_assignId = tokens.at(0).text();
            tokens.erase(tokens.begin());
            tokens.erase(tokens.begin());
        } else if (tokens.count() > 2
                   && tokens.at(0).isIdentifier()
                   && tokens.at(1).asOperator() == Token::AssociationStart
                   && tokens.at(1).text() == QLatin1String("("))
        {
            // Check for function assignment.
            // Syntax:
            // ident opLeftPar (ident (opSemiColon ident)*)? opRightPar opEqual
            int t;
            if (tokens.count() > 4
                && tokens.at(2).asOperator() == Token::AssociationEnd)
            {
                // Functions with no argument.
                t = 3;
                if (tokens.at(3).asOperator() == Token::Assignment)
                    m_assignFunc = true;
            } else {
                for (t = 2; t + 1 < tokens.count(); t += 2)  {
                    if (!tokens.at(t).isIdentifier())
                        break;

                    m_assignArg.append(tokens.at(t).text());

                    if (tokens.at(t+1).asOperator() == Token::AssociationEnd) {
                        t += 2;
                        if (t < tokens.count()
                            && tokens.at(t).asOperator() == Token::Assignment)
                        {
                            m_assignFunc = true;
                        }

                        break;
                    } else if (tokens.at(t + 1)
                               .asOperator() != Token::ListSeparator)
                        break;
                }
            }

            if (m_assignFunc) {
                m_assignId = tokens.at(0).text();
                m_assignFuncExpr = expressionToParse.section("=", 1, 1).trimmed();
                for (; t >= 0; --t)
                    tokens.erase(tokens.begin());
            } else
                m_assignArg.clear();
        }

        if (hasMixedStandaloneExplicitAngleTermAddSub(tokens)) {
            m_error = tr("dimension mismatch - quantities with "
                         "different dimensions cannot be compared, "
                         "added, etc.");
            m_interpretedExpression = QString();
            m_hasImplicitMultiplication = false;
            return CNumber(0);
        }

        compile(tokens);
        if (!m_valid) {
            if (m_error.isEmpty())
                m_error = tr("syntax error");
            m_interpretedExpression = QString();
            m_hasImplicitMultiplication = false;
            return CNumber(0);
        }

        if (m_interpretedExpression.isEmpty() && !tokens.isEmpty())
            m_interpretedExpression = expressionToParse.trimmed();

        if (!m_assignId.isEmpty() && !m_interpretedExpression.isEmpty()) {
            if (m_assignFunc) {
                const QString leftSide = QStringLiteral("%1(%2)").arg(
                    m_assignId,
                    m_assignArg.join(";")
                );
                m_interpretedExpression =
                    leftSide + QStringLiteral("=") + m_interpretedExpression;
            } else if (m_assignUnit) {
                QString interpretedRightSide = m_interpretedExpression;
                if (s_isBracketedExpression(m_assignUnitExpr)
                    && !s_isBracketedExpression(interpretedRightSide))
                {
                    interpretedRightSide =
                        QStringLiteral("[%1]").arg(interpretedRightSide.trimmed());
                }
                m_interpretedExpression =
                    QStringLiteral("[%1]=%2").arg(m_assignId, interpretedRightSide);
            } else {
                m_interpretedExpression =
                    m_assignId + QStringLiteral("=") + m_interpretedExpression;
            }
        }

        if (!m_interpretedExpression.isEmpty() && !trailingComment.isEmpty())
            m_interpretedExpression += QStringLiteral(" ? ") + trailingComment;
    }

    result = exec(m_codes, m_constants, m_identifiers);
    return result;
}

Quantity Evaluator::exec(const QVector<Opcode>& opcodes,
                         const QVector<Quantity>& constants,
                         const QStringList& identifiers)
{
    struct StackEntry {
        Quantity value;
        bool isPercentValue;
    };
    QStack<StackEntry> stack;
    QHash<int, QString> refs;
    int index;
    Quantity val1, val2;
    bool val1IsPercent = false;
    bool rhsIsPercent = false;
    QVector<Quantity> args;
    QString fname;
    Function* function;
    const UserFunction* userFunction = nullptr;
    auto pushStackValue = [&stack](const Quantity& value, bool isPercentValue = false) {
        stack.push(StackEntry{value, isPercentValue});
    };
    auto popStackValue = [&stack](Quantity& value, bool* isPercentValue = nullptr) {
        const StackEntry entry = stack.pop();
        value = entry.value;
        if (isPercentValue)
            *isPercentValue = entry.isPercentValue;
    };
    auto hasPendingDeferredSummationContext = [&refs](int stackSize) {
        QHash<int, QString>::const_iterator it = refs.constBegin();
        for (; it != refs.constEnd(); ++it) {
            if (it.key() <= stackSize
                && s_isSummationFunctionIdentifier(it.value()))
            {
                return true;
            }
        }
        return false;
    };
    auto hasPendingDeferredMolmassContext = [&refs](int stackSize) {
        QHash<int, QString>::const_iterator it = refs.constBegin();
        for (; it != refs.constEnd(); ++it) {
            if (it.key() <= stackSize
                && s_isMolmassFunctionIdentifier(it.value()))
            {
                return true;
            }
        }
        return false;
    };
    auto hasPendingDeferredMassContext = [&refs](int stackSize) {
        QHash<int, QString>::const_iterator it = refs.constBegin();
        for (; it != refs.constEnd(); ++it) {
            if (it.key() <= stackSize
                && s_isMassFunctionIdentifier(it.value()))
            {
                return true;
            }
        }
        return false;
    };
    auto hasPendingDeferredOperandContext =
        [&hasPendingDeferredSummationContext, &hasPendingDeferredMolmassContext, &hasPendingDeferredMassContext](int stackSize) {
            return hasPendingDeferredSummationContext(stackSize)
                   || hasPendingDeferredMolmassContext(stackSize)
                   || hasPendingDeferredMassContext(stackSize);
        };
    auto checkOperatorResultWithDeferredNoOperand = [this, &hasPendingDeferredOperandContext, &stack](const Quantity& result) {
        if (result.error() == NoOperand
            && hasPendingDeferredOperandContext(stack.count()))
        {
            return result;
        }
        return checkOperatorResult(result);
    };

    for (int pc = 0; pc < opcodes.count(); ++pc) {
        const Opcode& opcode = opcodes.at(pc);
        index = opcode.index;
        switch (opcode.type) {
            // No operation.
            case Opcode::Nop:
                break;

            // Load a constant, push to stack.
            case Opcode::Load:
                val1 = constants.at(index);
                pushStackValue(val1);
                break;

            case Opcode::List: {
                if (stack.count() < index) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                QVector<Quantity> elements;
                for (; index; --index)
                    elements.prepend(stack.pop().value);
                val1 = Quantity::list(elements);
                val1 = checkOperatorResult(val1);
                pushStackValue(val1);
                break;
            }

            // Unary operation.
            case Opcode::Neg:
                if (stack.count() < 1) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                val1IsPercent = false;
                popStackValue(val1, &val1IsPercent);
                val1 = checkOperatorResultWithDeferredNoOperand(-val1);
                pushStackValue(val1, val1IsPercent);
                break;

            case Opcode::BNot:
                if (stack.count() < 1) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                val1 = checkOperatorResultWithDeferredNoOperand(~val1);
                pushStackValue(val1);
                break;

            // Binary operation: take two values from stack,
            // do the operation, push the result to stack.
            case Opcode::Add:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                rhsIsPercent = false;
                popStackValue(val1, &rhsIsPercent);
                popStackValue(val2);
                if (rhsIsPercent)
                    val1 = checkOperatorResultWithDeferredNoOperand(val2 * val1);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 + val1);
                pushStackValue(val2);
                break;

            case Opcode::Sub:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                rhsIsPercent = false;
                popStackValue(val1, &rhsIsPercent);
                popStackValue(val2);
                if (rhsIsPercent)
                    val1 = checkOperatorResultWithDeferredNoOperand(val2 * val1);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 - val1);
                pushStackValue(val2);
                break;

            case Opcode::Mul:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 * val1);
                pushStackValue(val2);
                break;

            case Opcode::Unit:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                {
                    HNumber affineBaseValue;
                    if (val2.isDimensionless()
                        && val2.numericValue().isNearReal()
                        && Units::tryConvertAffineToBase(
                            val1.unitName(), val2.numericValue().real, &affineBaseValue))
                    {
                        Quantity absolute = Quantity(affineBaseValue) * Units::kelvin();
                        absolute.setDisplayUnit(CNumber(1), val1.unitName());
                        val2 = checkOperatorResultWithDeferredNoOperand(absolute);
                    } else {
                        val2 = checkOperatorResultWithDeferredNoOperand(val2 * val1);
                    }
                }
                pushStackValue(val2);
                break;

            case Opcode::Div:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 / val1);
                pushStackValue(val2);
                break;

            case Opcode::Pow:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = checkOperatorResultWithDeferredNoOperand(DMath::raise(val2, val1));
                pushStackValue(val2);
                break;

            case Opcode::Fact:
                if (stack.count() < 1) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                val1 = checkOperatorResultWithDeferredNoOperand(DMath::factorial(val1));
                pushStackValue(val1);
                break;

            case Opcode::Percent:
                if (stack.count() < 1) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                val1 = checkOperatorResultWithDeferredNoOperand(val1 / Quantity(100));
                pushStackValue(val1, true);
                break;

            case Opcode::Modulo:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 % val1);
                pushStackValue(val2);
                break;

            case Opcode::IntDiv:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = checkOperatorResultWithDeferredNoOperand(val2 / val1);
                pushStackValue(DMath::integer(val2));
                break;

            case Opcode::LSh:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return DMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = val2 << val1;
                pushStackValue(val2);
                break;

            case Opcode::RSh:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return DMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 = val2 >> val1;
                pushStackValue(val2);
                break;

            case Opcode::BAnd:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return DMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 &= val1;
                pushStackValue(val2);
                break;

            case Opcode::BOr:
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return DMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                val2 |= val1;
                pushStackValue(val2);
                break;

            case Opcode::Conv: {
                auto isFullyWrappedByOuterParens = [](const QString& expr) {
                    if (expr.size() < 2 || expr.at(0) != MathDsl::GroupStart || expr.at(expr.size() - 1) != MathDsl::GroupEnd)
                        return false;
                    int depth = 0;
                    for (int i = 0; i < expr.size(); ++i) {
                        const QChar ch = expr.at(i);
                        if (ch == MathDsl::GroupStart)
                            ++depth;
                        else if (ch == MathDsl::GroupEnd)
                            --depth;
                        if (depth == 0 && i < expr.size() - 1)
                            return false;
                    }
                    return depth == 0;
                };
                auto isFullyWrappedByOuterBrackets = [](const QString& expr) {
                    if (expr.size() < 2 || expr.at(0) != MathDsl::UnitStart || expr.at(expr.size() - 1) != MathDsl::UnitEnd)
                        return false;
                    int depth = 0;
                    for (int i = 0; i < expr.size(); ++i) {
                        const QChar ch = expr.at(i);
                        if (ch == MathDsl::UnitStart)
                            ++depth;
                        else if (ch == MathDsl::UnitEnd)
                            --depth;
                        if (depth == 0 && i < expr.size() - 1)
                            return false;
                    }
                    return depth == 0;
                };
                if (stack.count() < 2) {
                    m_error = tr("invalid expression");
                    return HMath::nan();
                }
                popStackValue(val1);
                popStackValue(val2);
                auto targetContainsUnitIdentifier = [this](const QString& targetText) {
                    const Tokens targetTokens = scan(targetText);
                    if (!targetTokens.valid())
                        return false;
                    for (const Token& token : targetTokens) {
                        if (token.isUnitIdentifier()
                            || (token.isIdentifier() && tryGetAnyUnitQuantity(token.text(), nullptr))) {
                            return true;
                        }
                    }
                    return false;
                };
                if (!targetContainsUnitIdentifier(opcode.text)) {
                    m_error = tr("conversion target must be a unit");
                    return HMath::nan();
                }
                if (val1.isZero()) {
                    m_error = tr("unit must not be zero");
                    return HMath::nan();
                }
                if (!val1.sameDimension(val2)) {
                    m_error = tr("Conversion failed - dimension mismatch");
                    return HMath::nan();
                }
                const bool targetIsAffineUnit = Units::isAffineUnitName(val1.unitName());
                CNumber displayUnitValue =
                    targetIsAffineUnit
                    ? CNumber(1)
                    : val1.numericValue();
                QString displayUnitName = opcode.text.trimmed();
                if (isFullyWrappedByOuterBrackets(displayUnitName))
                    displayUnitName = displayUnitName.mid(1, displayUnitName.size() - 2).trimmed();
                if (displayUnitName.startsWith(QStringLiteral("(("))
                    && isFullyWrappedByOuterParens(displayUnitName))
                {
                    const QString onceUnwrapped = displayUnitName.mid(1, displayUnitName.size() - 2).trimmed();
                    displayUnitName = onceUnwrapped;
                }
                if (isFullyWrappedByOuterParens(displayUnitName)) {
                    const QString inner = displayUnitName.mid(1, displayUnitName.size() - 2).trimmed();
                    if (RegExpPatterns::simpleUnitIdentifier().match(inner).hasMatch())
                        displayUnitName = inner;
                }
                displayUnitName = UnicodeChars::normalizeUnitSymbolAliases(displayUnitName);
                const bool nestedConversionTarget =
                    opcode.text.contains(QLatin1String("->"))
                    || opcode.text.contains(MathDsl::TransOp);
                if (nestedConversionTarget) {
                    Quantity canonicalTarget(val1);
                    canonicalTarget.stripUnits();
                    if (!canonicalTarget.isDimensionless())
                        Units::findUnit(canonicalTarget);
                    if (canonicalTarget.hasUnit()) {
                        displayUnitValue = canonicalTarget.unit();
                        displayUnitName = canonicalTarget.unitName();
                    }
                }
                if (targetIsAffineUnit) {
                    // Affine temperatures keep canonical symbol output (°C/°F)
                    // even when the authored target uses an alternate alias.
                    displayUnitName = val1.unitName();
                }
                if (runtimeUnitNegativeExponentStyle() == Settings::UnitNegativeExponentSuperscript)
                    displayUnitName = s_applySuperscriptStyleToExplicitUnitText(displayUnitName);
                else
                    displayUnitName = s_renderUnitExponentsAsSuperscripts(displayUnitName);
                val2.setDisplayUnit(displayUnitValue, displayUnitName);
                pushStackValue(val2);
                break;
            }

            // Reference.
            case Opcode::Ref:
                fname = identifiers.at(index);
                if (m_assignArg.contains(fname)) {
                    // Argument.
                    pushStackValue(CMath::nan());
                } else if (hasVariable(fname)) {
                    // Variable.
                    pushStackValue(getVariable(fname).value());
                } else {
                    // Function.
                    function = FunctionRepo::instance()->find(fname);
                    if (function) {
                        pushStackValue(CMath::nan());
                        refs.insert(stack.count(), fname);
                    } else if (m_assignFunc) {
                        // Allow arbitrary identifiers
                        // when declaring user functions.
                        pushStackValue(CMath::nan());
                        refs.insert(stack.count(), fname);
                    } else if (hasUserFunction(fname)) {
                        pushStackValue(CMath::nan());
                        refs.insert(stack.count(), fname);
                    } else if (fname.compare("n", Qt::CaseInsensitive) == 0
                               && hasPendingDeferredSummationContext(stack.count()))
                    {
                        pushStackValue(CMath::nan());
                    } else if (hasPendingDeferredMolmassContext(stack.count())
                               || hasPendingDeferredMassContext(stack.count())) {
                        pushStackValue(CMath::nan());
                    } else {
                        m_error = "<b>" + fname + "</b>: "
                                  + tr("unknown function or variable");
                        return CMath::nan();
                    }
                }
                break;

            case Opcode::UnitRef:
                fname = identifiers.at(index);
                if (m_assignArg.contains(fname)) {
                    pushStackValue(CMath::nan());
                    break;
                }
                if (tryGetAnyUnitQuantity(fname, &val1)) {
                    pushStackValue(val1);
                    break;
                }
                m_error = "<b>" + fname + "</b>: " + tr("unknown unit");
                return CMath::nan();

            // Calling function.
            case Opcode::Function:
                fname = refs.take(stack.count() - index);
                // Identifiers followed by () must resolve to a callable.
                // Do not silently accept constants/variables as nullary calls.
                if (fname.isEmpty()) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }
                function = FunctionRepo::instance()->find(fname);

                userFunction = nullptr;
                if (!function) {
                    // Check if this is a valid user function call.
                    userFunction = getUserFunction(fname);
                }

                if (!function && !userFunction && !m_assignFunc) {
                    m_error = "<b>" + fname + "</b>: "
                              + tr("unknown function or variable");
                    return CMath::nan();
                }

                if (stack.count() < index) {
                    m_error = tr("invalid expression");
                    return CMath::nan();
                }

                args.clear();
                for(; index; --index)
                    args.insert(args.begin(), stack.pop().value);

                // Remove the NaN we put on the stack (needed to make the user
                // functions declaration work with arbitrary identifiers).
                stack.pop();

                // Show function signature if user has given no argument (yet).
                if (userFunction) {
                    if (!args.count()
                        && userFunction->arguments().count() != 0)
                    {
                        m_error = QString::fromLatin1("<b>%1</b>(%2)").arg(
                            userFunction->name(),
                            userFunction->arguments().join(";")
                        );
                        return CMath::nan();
                    }
                }

                if (m_assignFunc) {
                    // Allow arbitrary identifiers for declaring user functions.
                    pushStackValue(CMath::nan());
                } else if (userFunction) {
                    pushStackValue(execUserFunction(userFunction, args));
                    if (!m_error.isEmpty())
                        return CMath::nan();
                } else if (s_isMolmassFunctionIdentifier(fname)) {
                    if (args.count() != 1) {
                        m_error = QString::fromLatin1("<b>%1</b>: ").arg(fname)
                                + tr("wrong number of arguments");
                        return CMath::nan();
                    }
                    Quantity molarMass;
                    if (opcode.text.isEmpty() || !s_tryParseMolarMassFormula(opcode.text, &molarMass)) {
                        m_error = QString::fromLatin1("<b>%1</b>: ").arg(fname)
                                + tr("invalid expression");
                        return CMath::nan();
                    }
                    pushStackValue(molarMass);
                } else if (s_isMassFunctionIdentifier(fname)) {
                    if (args.count() != 2) {
                        m_error = QString::fromLatin1("<b>%1</b>: ").arg(fname)
                                + tr("wrong number of arguments");
                        return CMath::nan();
                    }
                    Quantity molarMass;
                    if (opcode.text.isEmpty() || !s_tryParseMolarMassFormula(opcode.text, &molarMass)) {
                        m_error = QString::fromLatin1("<b>%1</b>: ").arg(fname)
                                + tr("invalid expression");
                        return CMath::nan();
                    }
                    Quantity amount = args.at(0);
                    const Quantity oneMole = Units::mole();
                    if (amount.isDimensionless()) {
                        amount *= oneMole;
                    } else if (!amount.sameDimension(oneMole)) {
                        m_error = QString::fromLatin1("<b>%1</b>: ").arg(fname)
                                + tr("undefined for argument domain");
                        return CMath::nan();
                    }
                    Quantity massResult = amount * molarMass;
                    massResult.setDisplayUnit(
                        Units::gram().numericValue(),
                        QString(::unitSymbol(UnitId::Gram)));
                    pushStackValue(massResult);
                } else if (s_isSummationFunctionIdentifier(fname)) {
                    if (args.count() != 3) {
                        m_error = QString::fromLatin1("<b>%1</b>: ").arg(fname)
                                + tr("wrong number of arguments");
                        return CMath::nan();
                    }
                    if (!args.at(0).isInteger() || !args.at(1).isInteger()) {
                        m_error = QString::fromLatin1("<b>%1</b>: ").arg(fname)
                                + tr("undefined for argument domain");
                        return CMath::nan();
                    }

                    const bool hadN = hasVariable("n");
                    const Variable oldN = hadN ? getVariable("n")
                                               : Variable(QString(), Quantity(0));
                    auto restoreN = [this, hadN, oldN]() {
                        if (hadN) {
                            setVariable("n", oldN.value(), oldN.type(),
                                        oldN.description());
                        } else {
                            unsetVariable("n");
                        }
                    };

                    const Quantity start = args.at(0);
                    const Quantity end = args.at(1);
                    const Quantity step = (start <= end) ? Quantity(1) : Quantity(-1);
                    const bool hasExpression = !opcode.text.isEmpty();
                    Quantity sigmaResult(0);

                    for (Quantity n = start; step > 0 ? (n <= end) : (n >= end); n += step) {
                        setVariable("n", n);

                        Quantity term = args.at(2);
                        if (hasExpression) {
                            const bool savedDirty = m_dirty;
                            const QString savedExpression = m_expression;
                            const bool savedValid = m_valid;
                            const QString savedInterpretedExpression = m_interpretedExpression;
                            const QString savedAssignId = m_assignId;
                            const bool savedAssignFunc = m_assignFunc;
                            const QStringList savedAssignArg = m_assignArg;
                            const QVector<Opcode> savedCodes = m_codes;
                            const QVector<Quantity> savedConstants = m_constants;
                            const QStringList savedIdentifiers = m_identifiers;
                            const QString savedError = m_error;

                            setExpression(opcode.text);
                            term = evalNoAssign();
                            const QString termError = m_error;

                            m_dirty = savedDirty;
                            m_expression = savedExpression;
                            m_valid = savedValid;
                            m_interpretedExpression = savedInterpretedExpression;
                            m_assignId = savedAssignId;
                            m_assignFunc = savedAssignFunc;
                            m_assignArg = savedAssignArg;
                            m_codes = savedCodes;
                            m_constants = savedConstants;
                            m_identifiers = savedIdentifiers;
                            m_error = savedError;

                            if (!termError.isEmpty()) {
                                restoreN();
                                m_error = QString::fromLatin1("<b>%1</b>: ").arg(fname)
                                        + termError;
                                return CMath::nan();
                            }
                        }

                        sigmaResult += term;
                    }

                    restoreN();
                    pushStackValue(sigmaResult);
                } else {
                    pushStackValue(function->exec(args));
                    if (function->error()) {
                        if (!args.count() && function->error() == InvalidParamCount) {
                            m_error = QString::fromLatin1("<b>%1</b>(%2)").arg(
                                fname,
                                function->usage()
                            );
                        } else {
                            m_error = stringFromFunctionError(function);
                        }
                        return CMath::nan();
                    }
                }
                break;

            default:
                break;
        }
    }

    // More than one value in stack? Unsuccessful execution.
    if (stack.count() != 1) {
        m_error = tr("invalid expression");
        return CMath::nan();
    }
    return stack.pop().value;
}

Quantity Evaluator::execUserFunction(const UserFunction* function,
                                     QVector<Quantity>& arguments)
{
    // TODO: Replace user variables by user functions (with no argument)?
    if (arguments.count() != function->arguments().count()) {
        m_error = "<b>" + function->name() + "</b>: "
                  + tr("wrong number of arguments");
        return CMath::nan();
    }

    if (m_functionsInUse.contains(function->name())) {
           m_error = "<b>" + function->name() + "</b>: "
                     + tr("recursion not supported");
           return CMath::nan();
    }
    m_functionsInUse.insert(function->name());

    QVector<Opcode> newOpcodes;
    auto newConstants = function->constants; // Copy.

    // Replace references to function arguments by constants.
    for (int i = 0; i < function->opcodes.count(); ++i) {
        Opcode opcode = function->opcodes.at(i);

        if (opcode.type == Opcode::Ref) {
            // Check if the identifier is an argument name.
            QString name = function->identifiers.at(opcode.index);
            int argIdx = function->arguments().indexOf(name);
            if (argIdx >= 0) {
                // Replace the reference by a constant value.
                opcode = Opcode(Opcode::Load, newConstants.count());
                newConstants.append(arguments.at(argIdx));
            }
        }

        newOpcodes.append(opcode);
    }

    auto result = exec(newOpcodes, newConstants, function->identifiers);
    if (!m_error.isEmpty()) {
        // Tell the user where the error happened.
        m_error = "<b>" + function->name() + "</b>: " + m_error;
    }

    m_functionsInUse.remove(function->name());
    return result;
}

bool Evaluator::isUserFunctionAssign() const
{
    return m_assignFunc;
}

bool Evaluator::isUserUnitAssign() const
{
    return m_assignUnit;
}

bool Evaluator::isUserVariableAssign() const
{
    return !m_assignId.isEmpty() && !m_assignFunc && !m_assignUnit;
}

bool Evaluator::isBuiltInVariable(const QString& id) const
{
    // Defining variables with the same name as existing functions
    // is not supported for now.
    if (FunctionRepo::instance()->find(id))
        return true;

    if (!m_session || !m_session->hasVariable(id))
        return false;

    return m_session->getVariable(id).type() == Variable::BuiltIn;
}

Quantity Evaluator::eval()
{
    Quantity result = evalNoAssign(); // This sets m_assignId.

    if (!m_error.isEmpty())
        return result;

    if (!m_assignUnit && isBuiltInVariable(m_assignId)) {
        m_error = tr("%1 is a reserved name, "
                     "please choose another").arg(m_assignId);
        return CMath::nan();
    }
    // Handle user variable or function assignment.
    if (!m_assignId.isEmpty()) {
        if (m_assignFunc) {
            if (hasVariable(m_assignId)) {
                m_error = tr("%1 is a variable name, please choose another "
                             "or delete the variable").arg(m_assignId);
                return CMath::nan();
            }

            // Check that each argument is unique and not a reserved identifier.
            for (int i = 0; i < m_assignArg.count() - 1; ++i) {
                const QString& argName = m_assignArg.at(i);

                if (m_assignArg.indexOf(argName, i + 1) != -1) {
                    m_error = tr("argument %1 is used "
                                 "more than once").arg(argName);
                    return CMath::nan();
                }

                if (isBuiltInVariable(argName)) {
                    m_error = tr("%1 is a reserved name, "
                                 "please choose another").arg(argName);
                    return CMath::nan();
                }
            }

            if (m_codes.isEmpty())
                return CMath::nan();
            UserFunction userFunction(m_assignId, m_assignArg,
                                      m_assignFuncExpr, m_assignFuncDescription);
            if (!m_interpretedExpression.isEmpty()) {
                const QString leftSide = QStringLiteral("%1(%2)=").arg(
                    m_assignId,
                    m_assignArg.join(";")
                );
                QString interpretedBody = m_interpretedExpression;
                if (interpretedBody.startsWith(leftSide))
                    interpretedBody = interpretedBody.mid(leftSide.size());
                else {
                    const int assignmentPos = interpretedBody.indexOf(MathDsl::Equals);
                    if (assignmentPos >= 0)
                        interpretedBody = interpretedBody.mid(assignmentPos + 1);
                }
                const int commentPos = findTopLevelCommentDelimiterPos(interpretedBody);
                if (commentPos >= 0)
                    interpretedBody = interpretedBody.left(commentPos).trimmed();
                userFunction.setInterpretedExpression(interpretedBody);
            }
            userFunction.constants = m_constants;
            userFunction.identifiers = m_identifiers;
            userFunction.opcodes = m_codes;

            setUserFunction(userFunction);

        } else if (m_assignUnit) {
            if (s_isBuiltInUnitIdentifier(m_assignId)) {
                m_error = tr("%1 is a built-in unit name, please choose another")
                    .arg(m_assignId);
                return CMath::nan();
            }
            if (result.isZero()) {
                m_error = tr("unit must not be zero");
                return CMath::nan();
            }
            UserUnit userUnit(m_assignId, result, m_assignUnitExpr,
                              m_interpretedExpression, m_assignUnitDescription);
            setUserUnit(userUnit);
        } else {
            if (hasUserFunction(m_assignId)) {
                m_error = tr("%1 is a user function name, please choose "
                             "another or delete the function").arg(m_assignId);
                return CMath::nan();
            }

            setVariable(m_assignId, result, Variable::UserDefined,
                        m_assignVarDescription);
        }
    }

    return result;
}

Quantity Evaluator::evalUpdateAns()
{
    auto result = eval();
    if (m_error.isEmpty() && !m_assignFunc && !m_assignUnit
        && !isCommentOnlyExpression(m_expression))
        setVariable(QLatin1String("ans"), result, Variable::BuiltIn);
    return result;
}

void Evaluator::setVariable(const QString& id, Quantity value,
                            Variable::Type type,
                            const QString& description,
                            const QString& formattedValue)
{
    if (!m_session)
        m_session = new Session;
    Variable variable(id, value, type, description);
    if (type == Variable::UserDefined) {
        if (!formattedValue.isEmpty())
            variable.setFormattedValue(formattedValue);
        else
            variable.setFormattedValue(NumberFormatter::format(value));
    }
    m_session->addVariable(variable);
}

Variable Evaluator::getVariable(const QString& id) const
{
    if (id.isEmpty() || !m_session)
        return Variable(QLatin1String(""), Quantity(0));

    return m_session->getVariable(id);
}

bool Evaluator::hasVariable(const QString& id) const
{
    if (id.isEmpty() || !m_session)
        return false;
    else
        return m_session->hasVariable(id);
}

void Evaluator::unsetVariable(const QString& id,
                              ForceBuiltinVariableErasure force)
{
    if (!m_session || (m_session->isBuiltInVariable(id) && !force))
        return;
    m_session->removeVariable(id);
}

QList<Variable> Evaluator::getVariables() const
{
    return m_session ? m_session->variablesToList() : QList<Variable>();
}

QList<Variable> Evaluator::getUserDefinedVariables() const
{
    auto result = getVariables();
    auto iter = result.begin();
    while (iter != result.end()) {
        if ((*iter).type() == Variable::BuiltIn)
            iter = result.erase(iter);
        else
            ++iter;
    }
    return result;
}

QList<Variable> Evaluator::getUserDefinedVariablesPlusAns() const
{
    auto result = getUserDefinedVariables();
    auto ans = getVariable(QLatin1String("ans"));
    if (!ans.identifier().isEmpty())
        result.append(ans);
    return result;
}

void Evaluator::unsetAllUserDefinedVariables()
{
    if (!m_session)
        return;
    auto ansBackup = getVariable(QLatin1String("ans")).value();
    m_session->clearVariables();
    setVariable(QLatin1String("ans"), ansBackup, Variable::BuiltIn);
    initializeBuiltInVariables();
}

static void replaceSuperscriptPowersWithCaretEquivalent(QString& expr)
{
    auto isIdentifierChar = [](const QChar& ch) {
        return isIdentifierContinue(ch);
    };
    auto isIdentifierStartChar = [](const QChar& ch) {
        return isIdentifierStart(ch);
    };
    auto isSuperscriptPowerChar = [](const QChar& ch) {
        return MathDsl::isSuperscriptPowerChar(ch);
    };

    // Convert function-call powers from "f²(x)" to "f(x)²" so later conversion
    // can produce the parser-friendly "f(x)^2" form.
    int i = 0;
    while (i < expr.size()) {
        if (!isSuperscriptPowerChar(expr.at(i))) {
            ++i;
            continue;
        }

        int superscriptEnd = i + 1;
        while (superscriptEnd < expr.size() && isSuperscriptPowerChar(expr.at(superscriptEnd)))
            ++superscriptEnd;

        int nameStart = i - 1;
        while (nameStart >= 0 && isIdentifierChar(expr.at(nameStart)))
            --nameStart;
        ++nameStart;
        while (nameStart < i && !isIdentifierStartChar(expr.at(nameStart)))
            ++nameStart;
        if (nameStart >= i || !isIdentifierStartChar(expr.at(nameStart))) {
            i = superscriptEnd;
            continue;
        }

        int argsStart = superscriptEnd;
        while (argsStart < expr.size() && expr.at(argsStart).isSpace())
            ++argsStart;
        if (argsStart >= expr.size() || expr.at(argsStart) != MathDsl::GroupStart) {
            i = superscriptEnd;
            continue;
        }

        int depth = 0;
        int argsEnd = -1;
        for (int j = argsStart; j < expr.size(); ++j) {
            const QChar ch = expr.at(j);
            if (ch == MathDsl::GroupStart) {
                ++depth;
            } else if (ch == MathDsl::GroupEnd) {
                --depth;
                if (depth == 0) {
                    argsEnd = j;
                    break;
                }
            }
        }
        if (argsEnd < 0) {
            i = superscriptEnd;
            continue;
        }

        const QString superscript = expr.mid(i, superscriptEnd - i);
        const QString callArgs = expr.mid(argsStart, argsEnd - argsStart + 1);
        int trailingPos = argsEnd + 1;
        while (trailingPos < expr.size() && expr.at(trailingPos).isSpace())
            ++trailingPos;
        const bool followedByAnotherSuperscript =
            trailingPos < expr.size() && isSuperscriptPowerChar(expr.at(trailingPos));

        if (followedByAnotherSuperscript) {
            const QString functionName = expr.mid(nameStart, i - nameStart);
            const QString replacement = QStringLiteral("(%1%2%3)")
                .arg(functionName, callArgs, superscript);
            expr.replace(nameStart, argsEnd - nameStart + 1, replacement);
            i = nameStart + replacement.size();
        } else {
            const QString replacement = callArgs + superscript;
            expr.replace(i, argsEnd - i + 1, replacement);
            i += replacement.size();
        }
    }

    static const QRegularExpression s_superscriptPowersRE(
        QStringLiteral("(")
        + QString(MathDsl::PowNeg)
        + QStringLiteral(")?[")
        + QString(MathDsl::Pow0)
        + QString(MathDsl::Pow1)
        + QString(MathDsl::Pow2)
        + QString(MathDsl::Pow3)
        + QString(MathDsl::Pow4)
        + QString(MathDsl::Pow5)
        + QString(MathDsl::Pow6)
        + QString(MathDsl::Pow7)
        + QString(MathDsl::Pow8)
        + QString(MathDsl::Pow9)
        + QStringLiteral("]+")
    );

    int offset = 0;
    while (true) {
      QRegularExpressionMatch match = s_superscriptPowersRE.match(expr, offset);
      if (!match.hasMatch())
          break;

      QString power = match.captured();
      for (int pos = power.size() - 1; pos >= 0; --pos) {
          const QChar c = power.at(pos);
          if (c == MathDsl::PowNeg) {
              power.replace(pos, 1, MathDsl::SubOpAl1);
              continue;
          }

          const QChar asciiDigit = MathDsl::superscriptDigitToAscii(c);
          if (!asciiDigit.isNull())
              power.replace(pos, 1, asciiDigit);
      }

      bool isNegative = match.capturedStart(1) != -1;
      if (isNegative)
          power = "^(" + power + ")";
      else
          power = "^" + power;

      expr.replace(match.capturedStart(), match.capturedLength(), power);
      offset = match.capturedStart() + power.size();
    }
}

static void normalizeFunctionCaretPowers(QString& expr)
{
    auto isIdentifierChar = [](const QChar& ch) {
        return isIdentifierContinue(ch);
    };
    auto isIdentifierStartChar = [](const QChar& ch) {
        return isIdentifierStart(ch);
    };
    auto parseIntegerExponentToken = [](const QString& input, int start, int* endOut) {
        int pos = start;
        bool parenthesized = false;
        if (pos < input.size() && input.at(pos) == MathDsl::GroupStart) {
            parenthesized = true;
            ++pos;
        }
        if (pos < input.size() && (input.at(pos) == MathDsl::AddOp || input.at(pos) == MathDsl::SubOpAl1))
            ++pos;
        const int digitsStart = pos;
        while (pos < input.size() && input.at(pos).isDigit())
            ++pos;
        if (pos == digitsStart)
            return false;
        if (parenthesized) {
            if (pos >= input.size() || input.at(pos) != MathDsl::GroupEnd)
                return false;
            ++pos;
        }
        *endOut = pos;
        return true;
    };

    int i = 0;
    while (i < expr.size()) {
        if (!isIdentifierStartChar(expr.at(i))) {
            ++i;
            continue;
        }

        int nameEnd = i + 1;
        while (nameEnd < expr.size() && isIdentifierChar(expr.at(nameEnd)))
            ++nameEnd;

        int afterName = nameEnd;
        while (afterName < expr.size() && expr.at(afterName).isSpace())
            ++afterName;
        if (afterName >= expr.size() || expr.at(afterName) != MathDsl::PowOp) {
            i = nameEnd;
            continue;
        }

        int expStart = afterName + 1;
        while (expStart < expr.size() && expr.at(expStart).isSpace())
            ++expStart;

        int expEnd = expStart;
        if (!parseIntegerExponentToken(expr, expStart, &expEnd)) {
            i = nameEnd;
            continue;
        }

        int argsStart = expEnd;
        while (argsStart < expr.size() && expr.at(argsStart).isSpace())
            ++argsStart;
        if (argsStart >= expr.size() || expr.at(argsStart) != MathDsl::GroupStart) {
            i = nameEnd;
            continue;
        }

        int depth = 0;
        int argsEnd = -1;
        for (int j = argsStart; j < expr.size(); ++j) {
            const QChar ch = expr.at(j);
            if (ch == MathDsl::GroupStart)
                ++depth;
            else if (ch == MathDsl::GroupEnd) {
                --depth;
                if (depth == 0) {
                    argsEnd = j;
                    break;
                }
            }
        }
        if (argsEnd < 0) {
            i = nameEnd;
            continue;
        }

        const QString callArgs = expr.mid(argsStart, argsEnd - argsStart + 1);
        const QString exponentToken = expr.mid(expStart, expEnd - expStart);
        const QString replacement = callArgs + MathDsl::PowOp + exponentToken;
        expr.replace(afterName, argsEnd - afterName + 1, replacement);
        i = i + replacement.size();
    }
}

QList<UserFunction> Evaluator::getUserFunctions() const
{
        return m_session ? m_session->UserFunctionsToList()
                         : QList<UserFunction>();
}

QList<UserUnit> Evaluator::getUserUnits() const
{
    return m_session ? m_session->userUnitsToList() : QList<UserUnit>();
}

void Evaluator::setUserFunction(const UserFunction& f)
{
    if (!m_session)
        m_session = new Session;
    m_session->addUserFunction(f);
}

void Evaluator::unsetUserFunction(const QString& fname)
{
    m_session->removeUserFunction(fname);
}

void Evaluator::unsetAllUserFunctions()
{
    m_session->clearUserFunctions();
}

bool Evaluator::hasUserFunction(const QString& fname) const
{
    bool invalid = fname.isEmpty() || !m_session;
    return (invalid) ? false : m_session->hasUserFunction(fname);
}

const UserFunction* Evaluator::getUserFunction(const QString& fname) const
{
    if (hasUserFunction(fname))
        return m_session->getUserFunction(fname);
    else
        return nullptr;
}

QStringList Evaluator::userUnitIdentifiers() const
{
    QStringList identifiers;
    if (!m_session)
        return identifiers;
    const auto units = m_session->userUnitsToList();
    for (const UserUnit& unit : units)
        identifiers.append(unit.name());
    identifiers.sort();
    return identifiers;
}

QStringList Evaluator::allUnitIdentifiers() const
{
    QStringList identifiers = builtInUnitIdentifiers();
    const QStringList userUnits = userUnitIdentifiers();
    for (const QString& unit : userUnits) {
        if (!identifiers.contains(unit))
            identifiers.append(unit);
    }
    identifiers.sort();
    return identifiers;
}

void Evaluator::setUserUnit(const UserUnit& unit)
{
    if (!m_session)
        m_session = new Session;
    m_session->addUserUnit(unit);
}

void Evaluator::unsetUserUnit(const QString& name)
{
    if (!m_session)
        return;
    m_session->removeUserUnit(name);
}

void Evaluator::unsetAllUserUnits()
{
    if (!m_session)
        return;
    m_session->clearUserUnits();
}

bool Evaluator::hasUserUnit(const QString& name) const
{
    bool invalid = name.isEmpty() || !m_session;
    return invalid ? false : m_session->hasUserUnit(name);
}

const UserUnit* Evaluator::getUserUnit(const QString& name) const
{
    if (hasUserUnit(name))
        return m_session->getUserUnit(name);
    return nullptr;
}

bool Evaluator::tryGetAnyUnitQuantity(const QString& identifier, Quantity* valueOut) const
{
    if (s_tryGetBuiltInUnitQuantity(identifier, valueOut))
        return true;
    const UserUnit* unit = getUserUnit(identifier);
    if (!unit)
        return false;
    if (valueOut)
        *valueOut = unit->value();
    return true;
}

QString Evaluator::autoFix(const QString& expr)
{
    QString result;

    // Strip off all funny characters.
    for (int c = 0; c < expr.length(); ++c) {
        const QChar ch = expr.at(c);
        if (ch.isSpace()) {
            result.append(QLatin1Char(' '));
            continue;
        }
        if (ch >= QChar(32))
            result.append(ch);
    }

    // No extra whitespaces at the beginning and at the end.
    result = result.trimmed();

    // Keep comments untouched by expression normalization.
    QString commentText;
    const int commentPos = result.indexOf(MathDsl::CommentSep);
    if (commentPos >= 0) {
        commentText = result.mid(commentPos + 1).trimmed();
        result = result.left(commentPos).trimmed();
    }

    // Strip trailing equal sign (=).
    while (result.endsWith("="))
        result = result.left(result.length() - 1);

    auto closeUnmatchedListBraces = [&]() {
        QString closingStack;
        for (int i = 0; i < result.size(); ++i) {
            const QChar ch = result.at(i);
            if (ch == MathDsl::ListStart) {
                closingStack.append(MathDsl::ListEnd);
                continue;
            }
            if (ch != MathDsl::ListEnd || closingStack.isEmpty())
                continue;
            closingStack.chop(1);
        }
        while (!closingStack.isEmpty()) {
            result.append(closingStack.at(closingStack.size() - 1));
            closingStack.chop(1);
        }
    };
    closeUnmatchedListBraces();

    // Make committed evaluation consistent with live/selection previews by
    // swallowing safe trailing incomplete tokens (e.g. "1+2+").
    QString withoutTrailingIncompleteToken;
    if (s_expressionWithoutIgnorableTrailingToken(result, &withoutTrailingIncompleteToken))
        result = withoutTrailingIncompleteToken;

    normalizeFunctionCaretPowers(result);
    replaceSuperscriptPowersWithCaretEquivalent(result);
    // Treat an empty parenthesized exponent as a no-op, so expressions like
    // "3^()" keep evaluating as "3" instead of raising a syntax error.
    result.replace(QRegularExpression(QStringLiteral(R"(\^\s*\(\s*\))")),
                   QString());
    result = UnicodeChars::normalizeUnitSymbolAliases(result);
    result = UnicodeChars::normalizeRootFunctionAliasesForDisplay(result);

    // Normalize parenthesized unit attachment shorthand:
    // X([U]) -> X[U], including extra outer parentheses around [U].
    auto canEndOperandBeforeParen = [](QChar ch) {
        return ch.isLetterOrNumber()
               || ch == QLatin1Char('_')
               || ch == MathDsl::GroupEnd
               || ch == MathDsl::UnitEnd
               || ch == MathDsl::UnitStart
               || ch == MathDsl::FactorOp
               || ch == MathDsl::PercentOp;
    };
    auto isIdentifierChar = [](QChar ch) {
        return ch.isLetterOrNumber() || ch == QLatin1Char('_');
    };
    auto isBalancedBracketExpression = [](const QString& text) {
        if (text.isEmpty() || text.at(0) != MathDsl::UnitStart || text.at(text.size() - 1) != MathDsl::UnitEnd)
            return false;
        int depth = 0;
        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (ch == MathDsl::UnitStart) {
                ++depth;
            } else if (ch == MathDsl::UnitEnd) {
                --depth;
                if (depth < 0)
                    return false;
            }
        }
        return depth == 0;
    };
    auto isWrappedByOuterParens = [](const QString& text) {
        if (text.size() < 2 || text.at(0) != MathDsl::GroupStart || text.at(text.size() - 1) != MathDsl::GroupEnd)
            return false;
        int depth = 0;
        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            if (ch == MathDsl::GroupStart) {
                ++depth;
            } else if (ch == MathDsl::GroupEnd) {
                --depth;
                if (depth == 0 && i != text.size() - 1)
                    return false;
                if (depth < 0)
                    return false;
            }
        }
        return depth == 0;
    };
    auto normalizeParenthesizedUnitAttachment = [&]() {
        for (int i = 0; i < result.size(); ++i) {
            if (result.at(i) != MathDsl::GroupStart)
                continue;

            int prev = i - 1;
            while (prev >= 0 && result.at(prev).isSpace())
                --prev;
            if (prev < 0 || !canEndOperandBeforeParen(result.at(prev)))
                continue;

            // Do not rewrite actual function-call parentheses, e.g. "sin([m])".
            if (isIdentifierChar(result.at(prev))) {
                int nameStart = prev;
                while (nameStart > 0 && isIdentifierChar(result.at(nameStart - 1)))
                    --nameStart;
                const QString name = result.mid(nameStart, prev - nameStart + 1);
                if (FunctionRepo::instance()->find(name) || hasUserFunction(name))
                    continue;
            }

            int depth = 0;
            int closeParenPos = -1;
            for (int p = i; p < result.size(); ++p) {
                const QChar ch = result.at(p);
                if (ch == MathDsl::GroupStart) {
                    ++depth;
                } else if (ch == MathDsl::GroupEnd) {
                    --depth;
                    if (depth == 0) {
                        closeParenPos = p;
                        break;
                    }
                }
            }
            if (closeParenPos < 0)
                continue;

            QString inner = result.mid(i + 1, closeParenPos - i - 1).trimmed();
            while (isWrappedByOuterParens(inner))
                inner = inner.mid(1, inner.size() - 2).trimmed();
            if (!isBalancedBracketExpression(inner))
                continue;

            result.replace(i, closeParenPos - i + 1, inner);
            i += inner.size() - 1;
        }
    };
    normalizeParenthesizedUnitAttachment();

    // Normalize "X / N[U]" into "X / N·[U]" when X is not already a
    // unit-attached term in source text. This aligns editor-entered
    // "4/2[s]" with implicit-multiplication grouping while preserving
    // "6[W]/2[s]" as unit-division.
    auto normalizeDivisionByValueUnitAttachment = [&]() {
        QString rewritten;
        rewritten.reserve(result.size() + 8);
        int cursor = 0;
        while (cursor < result.size()) {
            if (result.at(cursor) != MathDsl::DivOp) {
                rewritten += result.at(cursor);
                ++cursor;
                continue;
            }

            int left = cursor - 1;
            while (left >= 0 && result.at(left).isSpace())
                --left;
            if (left >= 0 && result.at(left) == MathDsl::UnitEnd) {
                rewritten += result.at(cursor);
                ++cursor;
                continue;
            }

            int rhs = cursor + 1;
            while (rhs < result.size() && result.at(rhs).isSpace())
                ++rhs;
            if (rhs < result.size()
                && (MathDsl::isAdditionOperator(result.at(rhs))
                    || MathDsl::isSubtractionOperator(result.at(rhs)))) {
                ++rhs;
            }

            bool hasDigits = false;
            while (rhs < result.size() && result.at(rhs).isDigit()) {
                ++rhs;
                hasDigits = true;
            }
            if (rhs < result.size()
                && (result.at(rhs) == MathDsl::DotSep
                    || result.at(rhs) == MathDsl::CommaSep)) {
                ++rhs;
                while (rhs < result.size() && result.at(rhs).isDigit()) {
                    ++rhs;
                    hasDigits = true;
                }
            }
            if (!hasDigits) {
                rewritten += result.at(cursor);
                ++cursor;
                continue;
            }

            int unitStart = rhs;
            while (unitStart < result.size() && result.at(unitStart).isSpace())
                ++unitStart;
            if (unitStart >= result.size() || result.at(unitStart) != MathDsl::UnitStart) {
                rewritten += result.at(cursor);
                ++cursor;
                continue;
            }

            int depth = 0;
            int unitEnd = -1;
            for (int i = unitStart; i < result.size(); ++i) {
                const QChar ch = result.at(i);
                if (ch == MathDsl::UnitStart) {
                    ++depth;
                } else if (ch == MathDsl::UnitEnd) {
                    --depth;
                    if (depth == 0) {
                        unitEnd = i;
                        break;
                    }
                    if (depth < 0)
                        break;
                }
            }
            if (unitEnd < 0) {
                rewritten += result.at(cursor);
                ++cursor;
                continue;
            }

            rewritten += result.mid(cursor, unitStart - cursor);
            rewritten += MathDsl::MulDotOp;
            rewritten += result.mid(unitStart, unitEnd - unitStart + 1);
            cursor = unitEnd + 1;
        }
        result = rewritten;
    };
    normalizeDivisionByValueUnitAttachment();

    // Normalize symbolic multiplication to a compact "·" form.
    // Examples:
    //   "2×pi" -> "2·pi"
    //   "2   ×    pi   pi" -> "2·pi·pi"
    {
        const QString dotOperator(MathDsl::MulDotOp);
        const Tokens tokens = Evaluator::scan(result);
        for (int i = tokens.count() - 2; i >= 1; --i) {
            const Token& op = tokens.at(i);
            if (op.asOperator() != Token::Multiplication)
                continue;

            const Token& left = tokens.at(i - 1);
            const Token& right = tokens.at(i + 1);
            const bool leftLooksMultiplicativeOperand =
                left.isIdentifier()
                || left.isUnitIdentifier()
                || left.isNumber()
                || left.asOperator() == Token::AssociationEnd
                || left.asOperator() == Token::Factorial
                || left.asOperator() == Token::Percent;
            const bool rightLooksMultiplicativeOperand =
                right.isIdentifier()
                || right.isUnitIdentifier()
                || right.isNumber()
                || right.asOperator() == Token::AssociationStart;
            if (!leftLooksMultiplicativeOperand || !rightLooksMultiplicativeOperand)
                continue;

            // Keep explicit multiplication sign for pure number×number.
            if (left.isNumber() && right.isNumber())
                continue;

            result.replace(op.pos(), op.size(), dotOperator);
        }

        // Accept ASCII 'u' as a typing-friendly micro prefix for units:
        // normalize identifiers to 'µ*' only inside explicit unit brackets.
        for (int i = tokens.count() - 1; i >= 0; --i) {
            const Token& token = tokens.at(i);
            if (!token.isUnitIdentifier())
                continue;

            const QString sourceText = result.mid(token.pos(), token.size()).trimmed();
            if (!sourceText.startsWith(QLatin1Char('u')) || sourceText.size() <= 1)
                continue;

            const QString normalizedText = token.text();
            if (!normalizedText.startsWith(UnicodeChars::MicroSign))
                continue;

            if (!s_isBuiltInUnitIdentifier(normalizedText))
                continue;

            result.replace(token.pos(), token.size(), normalizedText);
        }

        // Remove extra spaces around explicit dot multiplications.
        result.replace(QRegularExpression(QStringLiteral("\\s*%1\\s*").arg(dotOperator)),
                       dotOperator);

        // Rewrite identifier adjacency to dot multiplication, except when
        // the left identifier is a known function (e.g. keep "sin pi").
        static const QRegularExpression s_adjacentIdentifiersRE(
            QStringLiteral(R"(([A-Za-z_][A-Za-z_0-9]*)\s+([A-Za-z_][A-Za-z_0-9]*))")
        );
        int offset = 0;
        while (true) {
            const QRegularExpressionMatch match =
                s_adjacentIdentifiersRE.match(result, offset);
            if (!match.hasMatch())
                break;

            const QString left = match.captured(1);
            const QString right = match.captured(2);
            const bool keepAsUnitConversionAlias =
                left.compare(MathDsl::TransOpAl1, Qt::CaseInsensitive) == 0
                || right.compare(MathDsl::TransOpAl1, Qt::CaseInsensitive) == 0;
            const bool keepAsFunctionCall =
                FunctionRepo::instance()->find(left) || hasUserFunction(left);
            if (keepAsUnitConversionAlias || keepAsFunctionCall) {
                offset = match.capturedStart(2);
                continue;
            }

            const int start = match.capturedStart();
            result.replace(start, match.capturedLength(),
                           left + dotOperator + right);
            offset = start + left.length() + 1 + right.length();
        }
    }

    // Rewrite common shift-vs-add/sub ambiguity:
    // "a<<b+c" -> "(a << b) + c" and "a>>b-c" -> "(a >> b) - c".
    // This targets simple three-operand forms only.
    while (true) {
        const Tokens tokens = Evaluator::scan(result);
        bool updated = false;

        for (int i = 1; i + 3 < tokens.count(); ++i) {
            const Token& left = tokens.at(i - 1);
            const Token& shift = tokens.at(i);
            const Token& right = tokens.at(i + 1);
            const Token& addSub = tokens.at(i + 2);
            const Token& tail = tokens.at(i + 3);

            const Token::Operator shiftOp = shift.asOperator();
            if (shiftOp != Token::ArithmeticLeftShift
                && shiftOp != Token::ArithmeticRightShift)
            {
                continue;
            }
            if (!left.isOperand() || !right.isOperand() || !tail.isOperand())
                continue;
            if (addSub.asOperator() != Token::Addition
                && addSub.asOperator() != Token::Subtraction)
            {
                continue;
            }

            const int startPos = left.pos();
            const int endPos = tail.pos() + tail.size();
            const QString leftText = result.mid(left.pos(), left.size()).trimmed();
            const QString shiftText =
                shiftOp == Token::ArithmeticLeftShift ? QStringLiteral("<<")
                                                      : QStringLiteral(">>");
            const QString rightText = result.mid(right.pos(), right.size()).trimmed();
            const QString addSubText = result.mid(addSub.pos(), addSub.size()).trimmed();
            const QString tailText = result.mid(tail.pos(), tail.size()).trimmed();
            const QString replacement = QStringLiteral("(%1 %2 %3) %4 %5")
                .arg(leftText, shiftText, rightText, addSubText, tailText);
            result.replace(startPos, endPos - startPos, replacement);
            updated = true;
            break;
        }

        if (!updated)
            break;
    }

    // Automagically close unmatched grouping delimiters.
    //
    // We do this for both parentheses and unit brackets to mirror the tolerant
    // typing behavior used elsewhere in the editor (e.g. "sin(1" -> "sin(1)",
    // "2[m3" -> "2[m3]"). The stack keeps expected closing delimiters in
    // nesting order, so mixed groups close correctly ("([..." -> "...])").
    // The fix is only applied when scanning reaches the end of the expression,
    // avoiding synthetic closers when parsing already stopped earlier.
    Tokens tokens = Evaluator::scan(result);
    if (tokens.count()) {
        QString closingStack;
        for (int i = 0; i < tokens.count(); ++i) {
            if (tokens.at(i).asOperator() == Token::AssociationStart) {
                if (tokens.at(i).text() == QLatin1String("("))
                    closingStack.append(MathDsl::GroupEnd);
                else if (tokens.at(i).text() == QString(MathDsl::UnitStart))
                    closingStack.append(MathDsl::UnitEnd);
                else if (tokens.at(i).text() == QString(MathDsl::ListStart))
                    closingStack.append(MathDsl::ListEnd);
                continue;
            }
            if (tokens.at(i).asOperator() != Token::AssociationEnd)
                continue;
            if (closingStack.isEmpty())
                continue;
            const QChar closing = tokens.at(i).text().isEmpty()
                ? QChar()
                : tokens.at(i).text().at(0);
            if (closingStack.at(closingStack.size() - 1) == closing)
                closingStack.chop(1);
        }

        // If the scanner stops in the middle, do not bother to apply fix.
        const Token& lastToken = tokens.at(tokens.count() - 1);
        if (lastToken.pos() + lastToken.size() >= result.length())
            while (!closingStack.isEmpty()) {
                result.append(closingStack.at(closingStack.size() - 1));
                closingStack.chop(1);
            }
    }
    normalizeParenthesizedUnitAttachment();

    // Special treatment for simple function
    // e.g. "cos" is regarded as "cos(ans)".
    if (!result.isEmpty()) {
        Tokens tokens = Evaluator::scan(result);

        if (tokens.count() == 1
            && tokens.at(0).isIdentifier()
            && FunctionRepo::instance()->find(tokens.at(0).text()))
        {
            result.append("(ans)");
        }
    }

    if (commentPos >= 0) {
        result += QLatin1String(" ?");
        if (!commentText.isEmpty())
            result += QLatin1String(" ") + commentText;
    }

    return result;
}

QString Evaluator::dump()
{
    QString result;
    int c;

    if (m_dirty) {
        Tokens tokens = scan(m_expression);
        compile(tokens);
    }

    result = QString("Expression: [%1]\n").arg(m_expression);

    result.append("  Constants:\n");
    for (c = 0; c < m_constants.count(); ++c) {
        auto val = m_constants.at(c);
        result += QString("    #%1 = %2\n").arg(c).arg(
            DMath::format(val, Quantity::Format::Fixed())
        );
    }

    result.append("\n");
    result.append("  Identifiers:\n");
    for (c = 0; c < m_identifiers.count(); ++c) {
        result += QString("    #%1 = %2\n").arg(c).arg(m_identifiers.at(c));
    }

    result.append("\n");
    result.append("  Code:\n");
    for (int i = 0; i < m_codes.count(); ++i) {
        QString code;
        switch (m_codes.at(i).type) {
            case Opcode::Load:
                code = QString("Load #%1").arg(m_codes.at(i).index);
                break;
            case Opcode::Ref:
                code = QString("Ref #%1").arg(m_codes.at(i).index);
                break;
            case Opcode::UnitRef:
                code = QString("UnitRef #%1").arg(m_codes.at(i).index);
                break;
            case Opcode::Function:
                code = QString("Function (%1)").arg(m_codes.at(i).index);
                break;
            case Opcode::List:
                code = QString("List (%1)").arg(m_codes.at(i).index);
                break;
            case Opcode::Add:
                code = "Add";
                break;
            case Opcode::Sub:
                code = "Sub";
                break;
            case Opcode::Mul:
                code = "Mul";
                break;
            case Opcode::Unit:
                code = "Unit";
                break;
            case Opcode::Div:
                code = "Div";
                break;
            case Opcode::Neg:
                code = "Neg";
                break;
            case Opcode::BNot:
                code = "BNot";
                break;
            case Opcode::Pow:
                code = "Pow";
                break;
            case Opcode::Fact:
                code = "Fact";
                break;
            case Opcode::Percent:
                code = "Percent";
                break;
            case Opcode::Modulo:
                code = "Modulo";
                break;
            case Opcode::IntDiv:
                code = "IntDiv";
                break;
            case Opcode::LSh:
                code = "LSh";
                break;
            case Opcode::RSh:
                code = "RSh";
                break;
            case Opcode::BAnd:
                code = "BAnd";
                break;
            case Opcode::BOr:
                code = "BOr";
                break;
            case Opcode::Conv:
                code = "Conv";
                break;
            default:
                code = "Unknown";
                break;
        }
        result.append("   ").append(code).append("\n");
    }

    return result;
}
