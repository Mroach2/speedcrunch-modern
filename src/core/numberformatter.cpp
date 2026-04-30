// SPDX-FileCopyrightText: 2013, 2015-2018, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/numberformatter.h"

#include "core/regexpatterns.h"
#include "core/unitdisplayformat.h"
#include "core/settings.h"
#include "core/unicodechars.h"
#include "core/mathdsl.h"
#include "math/quantity.h"
#include "math/rational.h"
#include "core/units.h"

#include <algorithm>


static const QChar g_dotChar = MathDsl::MulDotOp;
static const QChar g_minusChar = MathDsl::SubOp;

namespace {

constexpr int RationalFormatMaxDenominator = 1000000;
constexpr int TrigPiFormatMaxDenominator = 12;
// Internal sentinel set by rat()/ratio()/rational() via Quantity::Format::precision.
// It forces rational display for a single result, with automatic decimal fallback.
constexpr int ForcedRationalPrecision = -999;
HNumber rationalFormatRelativeTolerance()
{
    return HNumber("1e-60");
}

HNumber trigSymbolicRelativeTolerance()
{
    return HNumber("1e-30");
}

bool isCloseTo(const HNumber& value, const HNumber& reference)
{
    const HNumber diff = HMath::abs(value - reference);
    const HNumber scale = HMath::max(HMath::abs(value), HMath::abs(reference));
    const HNumber one(1);
    return diff <= trigSymbolicRelativeTolerance() * (scale < one ? one : scale);
}

QString formatFraction(const QString& numerator, int denominator)
{
    const QString operatorSpace(MathDsl::DivWrap);
    return numerator + operatorSpace + MathDsl::DivOp
           + operatorSpace + QString::number(denominator);
}

QString formatPiMultiple(const HNumber& value)
{
    const HNumber pi = HMath::pi();
    if (HMath::abs(value) > HNumber(2) * pi)
        return QString();

    Rational ratio;
    if (!Rational::approximate(value / pi, TrigPiFormatMaxDenominator,
            trigSymbolicRelativeTolerance(), &ratio)) {
        return QString();
    }

    const int numerator = ratio.numerator();
    const int denominator = ratio.denominator();
    if (numerator == 0)
        return QString();
    if (qAbs(numerator) > TrigPiFormatMaxDenominator)
        return QString();

    if (denominator != 1
            && denominator != 2
            && denominator != 3
            && denominator != 4
            && denominator != 6
            && denominator != 8
            && denominator != 12) {
        return QString();
    }

    const HNumber expected = pi * HNumber(numerator) / HNumber(denominator);
    if (!isCloseTo(value, expected))
        return QString();

    const QString mulSpace(MathDsl::MulDotWrapSp);
    if (denominator == 1) {
        if (numerator == 1)
            return QStringLiteral("pi");
        if (numerator == -1)
            return QStringLiteral("-pi");
        return QString::number(numerator) + mulSpace + MathDsl::MulDotOp
               + mulSpace + QStringLiteral("pi");
    }

    QString numeratorText;
    if (numerator == 1)
        numeratorText = QStringLiteral("pi");
    else if (numerator == -1)
        numeratorText = QStringLiteral("-pi");
    else
        numeratorText = QString::number(numerator) + mulSpace + MathDsl::MulDotOp
                        + mulSpace + QStringLiteral("pi");

    return formatFraction(numeratorText, denominator);
}

QString formatCommonTrigRadical(const HNumber& value)
{
    const bool negative = value.isNegative();
    const HNumber absValue = negative ? -value : value;

    const HNumber sqrt2 = HMath::sqrt(HNumber(2));
    const HNumber sqrt3 = HMath::sqrt(HNumber(3));

    QString symbol;
    if (isCloseTo(absValue, sqrt2 / HNumber(2)))
        symbol = formatFraction(QStringLiteral("sqrt(2)"), 2);
    else if (isCloseTo(absValue, sqrt3 / HNumber(2)))
        symbol = formatFraction(QStringLiteral("sqrt(3)"), 2);
    else if (isCloseTo(absValue, sqrt3 / HNumber(3)))
        symbol = formatFraction(QStringLiteral("sqrt(3)"), 3);
    else if (isCloseTo(absValue, HNumber(2) * sqrt3 / HNumber(3)))
        symbol = formatFraction(QStringLiteral("2 sqrt(3)"), 3);
    else if (isCloseTo(absValue, sqrt2))
        symbol = QStringLiteral("sqrt(2)");
    else if (isCloseTo(absValue, sqrt3))
        symbol = QStringLiteral("sqrt(3)");

    if (symbol.isEmpty())
        return QString();
    return negative ? QStringLiteral("-") + symbol : symbol;
}

QString formatCommonTrigFraction(const HNumber& value)
{
    const bool negative = value.isNegative();
    const HNumber absValue = negative ? -value : value;

    QString symbol;
    if (isCloseTo(absValue, HNumber(1) / HNumber(2)))
        symbol = formatFraction(QStringLiteral("1"), 2);

    if (symbol.isEmpty())
        return QString();
    return negative ? QStringLiteral("-") + symbol : symbol;
}

QString formatTrigSymbolicDisplay(const Quantity& q, const HNumber& value)
{
    if (!q.isDimensionless() || !q.unitName().isEmpty())
        return QString();

    QString symbolic = formatPiMultiple(value);
    if (!symbolic.isEmpty())
        return symbolic;

    symbolic = formatCommonTrigFraction(value);
    if (!symbolic.isEmpty())
        return symbolic;

    return formatCommonTrigRadical(value);
}

QString formatRationalDisplay(Quantity q)
{
    if (q.isNan())
        return QString();

    if (!q.hasUnit() && !q.isDimensionless()) {
        q.cleanDimension();
        Units::findUnit(q);
    }

    CNumber number = q.numericValue();
    number /= q.unit();
    if (!number.isNearReal())
        return QString();

    QString symbolicTrig = formatTrigSymbolicDisplay(q, number.real);
    if (!symbolicTrig.isEmpty())
        return symbolicTrig;

    Rational rational;
    if (!Rational::approximate(number.real, RationalFormatMaxDenominator,
            rationalFormatRelativeTolerance(), &rational)) {
        return QString();
    }

    QString result;
    if (rational.denominator() == 1) {
        result = QString::number(rational.numerator());
    } else {
        result = formatFraction(QString::number(rational.numerator()), rational.denominator());
    }

    const QString unitName = q.unitName();
    if (!unitName.isEmpty())
        result += QStringLiteral("[%1]").arg(unitName);
    return result;
}

bool isValidDigitForBase(const QChar ch, int base)
{
    if (base <= 10)
        return ch >= QLatin1Char('0') && ch < QLatin1Char('0' + base);

    if (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
        return true;

    const QChar lower = ch.toLower();
    return lower >= QLatin1Char('a') && lower < QLatin1Char('a' + (base - 10));
}

bool isValidGroupedIntegerPart(const QString& part, QChar separator, int base, int groupSize)
{
    const QStringList groups = part.split(separator, Qt::KeepEmptyParts);
    if (groups.isEmpty())
        return false;

    for (int i = 0; i < groups.size(); ++i) {
        const QString& group = groups.at(i);
        if (group.isEmpty())
            return false;
        const int expectedSize = (i == 0) ? -1 : groupSize;
        if (expectedSize > 0 && group.size() != expectedSize)
            return false;
        if (i == 0 && group.size() > groupSize)
            return false;
        for (const QChar ch : group) {
            if (!isValidDigitForBase(ch, base))
                return false;
        }
    }

    return true;
}

bool canonicalizeLiteralMantissa(const QString& input, int base, int groupingSize,
                                 bool allowGroupingHeuristic, QString* output)
{
    const int dotCount = input.count(MathDsl::DotSep);
    const int commaCount = input.count(MathDsl::CommaSep);

    if (dotCount == 0 && commaCount == 0) {
        if (input.isEmpty())
            return false;
        for (const QChar ch : input) {
            if (!isValidDigitForBase(ch, base))
                return false;
        }
        *output = input;
        return true;
    }

    QChar radixSep;
    QChar groupingSep;
    bool hasRadix = false;
    if (dotCount > 0 && commaCount > 0) {
        const int lastDot = input.lastIndexOf(MathDsl::DotSep);
        const int lastComma = input.lastIndexOf(MathDsl::CommaSep);
        hasRadix = true;
        radixSep = (lastDot > lastComma) ? MathDsl::DotSep : MathDsl::CommaSep;
        groupingSep = (radixSep == MathDsl::DotSep) ? MathDsl::CommaSep : MathDsl::DotSep;
        if (input.count(radixSep) > 1)
            return false;
    } else {
        const QChar sep = (dotCount > 0) ? MathDsl::DotSep : MathDsl::CommaSep;
        const int sepCount = input.count(sep);
        if (sepCount == 1) {
            const int sepPos = input.indexOf(sep);
            const QString left = input.left(sepPos);
            const QString right = input.mid(sepPos + 1);
            const Settings* settings = Settings::instance();
            const QChar configuredDecimalSep(settings->decimalSeparator());
            const bool separatorIsConfiguredDecimal = sep == configuredDecimalSep;
            const bool looksGroupedInteger = allowGroupingHeuristic
                && !separatorIsConfiguredDecimal
                && !left.isEmpty()
                && right.size() == groupingSize
                && std::all_of(left.cbegin(), left.cend(),
                               [base](QChar ch) { return isValidDigitForBase(ch, base); })
                && std::all_of(right.cbegin(), right.cend(),
                               [base](QChar ch) { return isValidDigitForBase(ch, base); });
            if (looksGroupedInteger) {
                hasRadix = false;
                groupingSep = sep;
            } else {
                hasRadix = true;
                radixSep = sep;
            }
        } else {
            hasRadix = false;
            groupingSep = sep;
            if (!isValidGroupedIntegerPart(input, groupingSep, base, groupingSize))
                return false;
        }
    }

    if (!hasRadix) {
        QString integral = input;
        if (!groupingSep.isNull())
            integral.remove(groupingSep);
        if (integral.isEmpty())
            return false;
        for (const QChar ch : integral) {
            if (!isValidDigitForBase(ch, base))
                return false;
        }
        *output = integral;
        return true;
    }

    const int radixPos = input.lastIndexOf(radixSep);
    if (radixPos <= 0 || radixPos >= input.size() - 1)
        return false;

    QString integral = input.left(radixPos);
    QString fractional = input.mid(radixPos + 1);
    if (integral.isEmpty() || fractional.isEmpty())
        return false;

    if (!groupingSep.isNull()) {
        if (!isValidGroupedIntegerPart(integral, groupingSep, base, groupingSize))
            return false;
        integral.remove(groupingSep);
        fractional.remove(groupingSep);
    }

    for (const QChar ch : integral) {
        if (!isValidDigitForBase(ch, base))
            return false;
    }
    for (const QChar ch : fractional) {
        if (!isValidDigitForBase(ch, base))
            return false;
    }

    *output = integral + MathDsl::DotSep + fractional;
    return true;
}

bool canonicalizeStandaloneNumericLiteral(const QString& input, QString* output)
{
    QString trimmed = input.trimmed();
    if (trimmed.isEmpty())
        return false;

    QString sign;
    if (trimmed.startsWith(MathDsl::AddOp)) {
        trimmed.remove(0, 1);
    } else if (trimmed.startsWith(MathDsl::SubOpAl1)
               || trimmed.startsWith(g_minusChar)) {
        sign = QStringLiteral("-");
        trimmed.remove(0, 1);
    }
    if (trimmed.isEmpty())
        return false;

    QString mantissa = trimmed;
    QString exponent;
    int base = 10;
    int groupingSize = 3;
    bool allowGroupingHeuristic = true;

    QString prefix;
    if (trimmed.startsWith(MathDsl::HexPrefix, Qt::CaseInsensitive)) {
        base = 16;
        groupingSize = 4;
        allowGroupingHeuristic = false;
        prefix = trimmed.left(2);
        mantissa = trimmed.mid(2);
    } else if (trimmed.startsWith(MathDsl::OctPrefix, Qt::CaseInsensitive)) {
        base = 8;
        groupingSize = 3;
        allowGroupingHeuristic = false;
        prefix = trimmed.left(2);
        mantissa = trimmed.mid(2);
    } else if (trimmed.startsWith(MathDsl::BinPrefix, Qt::CaseInsensitive)) {
        base = 2;
        groupingSize = 4;
        allowGroupingHeuristic = false;
        prefix = trimmed.left(2);
        mantissa = trimmed.mid(2);
    } else {
        const int exponentPos = mantissa.indexOf(RegExpPatterns::decimalExponentSuffix());
        if (exponentPos >= 0) {
            exponent = mantissa.mid(exponentPos);
            mantissa = mantissa.left(exponentPos);
            allowGroupingHeuristic = false;
        }
    }

    if (mantissa.isEmpty())
        return false;

    QString canonicalMantissa;
    if (!canonicalizeLiteralMantissa(mantissa, base, groupingSize,
                                     allowGroupingHeuristic, &canonicalMantissa)) {
        return false;
    }

    QString canonical = sign + prefix + canonicalMantissa + exponent;
    if (base == 10) {
        const QByteArray bytes = canonical.toLatin1();
        const char* rest = nullptr;
        const HNumber parsed = HMath::parse_str(bytes.constData(), &rest);
        if (parsed.isNan() || !rest || *rest != '\0')
            return false;
    }

    *output = canonical;
    return true;
}

} // namespace

QString NumberFormatter::format(Quantity q)
{
    return format(q, '\0');
}

QString NumberFormatter::formatTrigSymbolic(Quantity q)
{
    if (q.isNan())
        return QString();

    CNumber number = q.numericValue();
    number /= q.unit();
    if (!number.isNearReal())
        return QString();

    return formatTrigSymbolicDisplay(q, number.real);
}

QString NumberFormatter::format(Quantity q, char resultFormatOverride)
{
    Settings* settings = Settings::instance();
    return format(q,
                  resultFormatOverride,
                  settings->resultPrecision,
                  settings->complexNumbers,
                  settings->resultFormatComplex);
}

QString NumberFormatter::format(Quantity q, char resultFormatOverride,
                                int precisionOverride, bool useComplexNotation,
                                char complexNotationOverride)
{
    Settings* settings = Settings::instance();
    CMath::setImaginaryUnitSymbol(settings->imaginaryUnit);
    CMath::setPolarAngleUnit(settings->angleUnit);
    const char activeResultFormat = resultFormatOverride == '\0' ? settings->resultFormat : resultFormatOverride;
    QString result;

    Quantity::Format format = q.format();
    // Check per-expression rational-display override encoded in format precision.
    const bool forceRationalDisplay = format.precision == ForcedRationalPrecision;
    const bool hasExplicitFormatOverride =
        format.base != Quantity::Format::Base::Null
        || format.mode != Quantity::Format::Mode::Null;
    if (((!hasExplicitFormatOverride && activeResultFormat == 'r') || forceRationalDisplay)
            && format.base != Quantity::Format::Base::Binary
            && format.base != Quantity::Format::Base::Octal
            && format.base != Quantity::Format::Base::Hexadecimal
            && format.mode != Quantity::Format::Mode::Sexagesimal) {
        result = formatRationalDisplay(q);
    }

    if (format.base == Quantity::Format::Base::Null) {
        switch (activeResultFormat) {
        case 'b':
            format.base = Quantity::Format::Base::Binary;
            break;
        case 'o':
            format.base = Quantity::Format::Base::Octal;
            break;
        case 'h':
            format.base = Quantity::Format::Base::Hexadecimal;
            break;
        case 'n':
        case 'f':
        case 'e':
        case 'r':
        case 's':
        case 'g':
        default:
            format.base = Quantity::Format::Base::Decimal;
            break;
        }
    }

    if (format.mode == Quantity::Format::Mode::Null) {
        if (format.base == Quantity::Format::Base::Decimal) {
          switch (activeResultFormat) {
          case 'n':
              format.mode = Quantity::Format::Mode::Engineering;
              break;
          case 'f':
              format.mode = Quantity::Format::Mode::Fixed;
              break;
          case 'e':
              format.mode = Quantity::Format::Mode::Scientific;
              break;
          case 's':
              format.mode = Quantity::Format::Mode::Sexagesimal;
              break;
          case 'r':
          case 'g':
          default:
              format.mode = Quantity::Format::Mode::General;
              break;
          }
        } else {
            format.mode = Quantity::Format::Mode::Fixed;
        }
    }

    if (format.precision == Quantity::Format::PrecisionNull)
        format.precision = precisionOverride;
    if (format.notation == Quantity::Format::Notation::Null) {
        if (useComplexNotation) {
            if (complexNotationOverride == 'c')
                format.notation = Quantity::Format::Notation::Cartesian;
            else if (complexNotationOverride == 'p')
                format.notation = Quantity::Format::Notation::Polar;
            else if (complexNotationOverride == 'a')
                format.notation = Quantity::Format::Notation::PolarAngle;
        }
    }

    const bool useSexagesimalOutput =
        format.base == Quantity::Format::Base::Decimal
        && format.mode == Quantity::Format::Mode::Sexagesimal;

    bool time = false, arc = q.isDimensionless();
    if (useSexagesimalOutput && q.hasDimension()) {
        auto dimension = q.getDimensionByQuantity();
        if (dimension.count() == 1 && dimension.firstKey() == UnitQuantity::Time) {
            auto iterator = dimension.begin();
            if ( iterator->numerator() == 1 && iterator->denominator() == 1) {
                q.clearDimension(); // remove unit, formatting itself is unit
                time = true;
            }
        }
    }

    if (arc && useSexagesimalOutput) {     // convert to arcseconds
        // Sexagesimal angle formatting is unitless "D°M′S″" output. If the
        // value carries an explicit angle display unit (e.g. [rad]), drop it
        // before numeric formatting/parsing to avoid contaminating the
        // intermediate scalar with unit text.
        q.stripUnits();
        if (settings->angleUnit == 'r')
            q /= Quantity(HMath::pi() / HNumber(180));
        else if (settings->angleUnit == 'g')
            q /= Quantity(HNumber(200) / HNumber(180));
        else if (settings->angleUnit == 't' || settings->angleUnit == 'v')
            q *= Quantity(360);
        q *= Quantity(3600);
    }

    bool negative = false;
    if (useSexagesimalOutput && q.isNegative()) {
        q *= Quantity(HNumber(-1));
        negative = true;
    }

    if (result.isEmpty())
        result = DMath::format(q, format);

    if (useSexagesimalOutput && (arc || time)) {   // sexagesimal
        int dotPos = result.indexOf(MathDsl::DotSep);
        HNumber seconds(dotPos > 0 ? result.left(dotPos).toStdString().c_str() : result.toStdString().c_str());
        HNumber mains = HMath::floor(seconds / HNumber(3600));
        seconds -= (mains * HNumber(3600));
        HNumber minutes = HMath::floor(seconds / HNumber(60));
        seconds -= (minutes * HNumber(60));
        HNumber::Format fixed = HNumber::Format::Fixed();
        QString sexa = HMath::format(mains, fixed);
        sexa.append(time ? MathDsl::TimeSep : MathDsl::Deg).append(minutes < 10 ? "0" : "").append(HMath::format(minutes, fixed));
        sexa.append(time ? MathDsl::TimeSep : MathDsl::ArcminOp).append(seconds < 10 ? "0" : "").append(HMath::format(seconds, fixed));
        if (dotPos > 0)     // append decimals
            sexa.append(result.mid(dotPos));
        if (!time)
            sexa.append(MathDsl::ArcsecOp);
        result = sexa;
    }

    if (negative)
        result.insert(0, MathDsl::SubOpAl1);

    if (settings->decimalSeparator() == MathDsl::CommaSep.toLatin1())
        result.replace(MathDsl::DotSep, MathDsl::CommaSep);

    if (q.hasUnit() || !q.isDimensionless()) {
        bool unitNormalized = false;
        const int openBracket = result.lastIndexOf(MathDsl::UnitStart);
        const int closeBracket = result.lastIndexOf(MathDsl::UnitEnd);
        if (openBracket >= 0
            && closeBracket > openBracket
            && closeBracket == result.size() - 1)
        {
            const QString normalizedUnit =
                UnitDisplayFormat::normalizeUnitTextForDisplay(
                    result.mid(openBracket + 1, closeBracket - openBracket - 1));
            result = result.left(openBracket + 1)
                + normalizedUnit
                + MathDsl::UnitEnd;
            unitNormalized = true;
        }

        if (!unitNormalized) {
            const int firstSpace = result.indexOf(QLatin1Char(' '));
            const int lastSpace = result.lastIndexOf(QLatin1Char(' '));
            int splitSpace = firstSpace;
            if (firstSpace >= 0 && lastSpace > firstSpace) {
                const QString between = result.mid(firstSpace + 1, lastSpace - firstSpace - 1).trimmed();
                const bool isScientificMantissaTail =
                    (between.startsWith(MathDsl::MulCrossOp)
                     || between.startsWith(MathDsl::MulOpAl1))
                    && between.contains(QStringLiteral("10"));
                if (isScientificMantissaTail)
                    splitSpace = lastSpace;
            }
            if (splitSpace >= 0 && splitSpace + 1 < result.size()) {
                const QString normalizedUnit =
                    UnitDisplayFormat::normalizeUnitTextForDisplay(result.mid(splitSpace + 1));
                result = result.left(splitSpace)
                    + MathDsl::UnitStart
                    + normalizedUnit
                    + MathDsl::UnitEnd;
            }
        }
    }

    result.replace(MathDsl::SubOpAl1, g_minusChar);

    // Replace all spaces between units with dot operator.
    int emptySpaces = 0;
    for (auto& ch : result) {
        if (ch.isSpace()) {
            ++emptySpaces;
            if (emptySpaces > 1)
                ch = g_dotChar;
        } else {
            emptySpaces = 0;
        }
    }

    return result;
}

QString NumberFormatter::formatNumericLiteralForDisplay(const QString& input)
{
    const Settings* settings = Settings::instance();
    const QString separator = settings->digitGroupingSeparator();
    const bool groupingEnabled = settings->isDigitGroupingEnabled() && !separator.isEmpty();
    const QChar decimalSep = QChar(settings->decimalSeparator());
    const QChar altDecimalSep = (decimalSep == MathDsl::DotSep) ? MathDsl::CommaSep : MathDsl::DotSep;

    const bool indianGrouping = settings->isIndianDigitGrouping();
    auto groupPart = [&separator](const QString& digits, int groupSize, bool fromRight) {
        if (digits.size() <= groupSize)
            return digits;

        QString result;
        if (fromRight) {
            int first = digits.size() % groupSize;
            if (first == 0)
                first = groupSize;
            result = digits.left(first);
            for (int i = first; i < digits.size(); i += groupSize) {
                result += separator;
                result += digits.mid(i, groupSize);
            }
            return result;
        }

        for (int i = 0; i < digits.size(); i += groupSize) {
            if (i > 0)
                result += separator;
            result += digits.mid(i, groupSize);
        }
        return result;
    };
    auto groupIntegral = [&](const QString& integral, int standardSize) -> QString {
        if (!indianGrouping || standardSize != 3 || integral.size() <= 3)
            return groupPart(integral, standardSize, true);

        const int leadingLength = integral.size() - 3;
        const QString leading = integral.left(leadingLength);
        const QString tail = integral.right(3);
        return groupPart(leading, 2, true) + separator + tail;
    };

    QString output;
    int lastPos = 0;
    auto it = RegExpPatterns::numericToken().globalMatch(input);
    while (it.hasNext()) {
        const auto match = it.next();
        output += input.mid(lastPos, match.capturedStart() - lastPos);

        QString token = match.captured(0);
        int prefixLength = 0;
        int groupSize = 3;
        if (token.startsWith(MathDsl::HexPrefix, Qt::CaseInsensitive)) {
            prefixLength = 2;
            groupSize = 4;
        } else if (token.startsWith(MathDsl::OctPrefix, Qt::CaseInsensitive)) {
            prefixLength = 2;
            groupSize = 3;
        } else if (token.startsWith(MathDsl::BinPrefix, Qt::CaseInsensitive)) {
            prefixLength = 2;
            groupSize = 4;
        }

        const QString prefix = token.left(prefixLength);
        QString body = token.mid(prefixLength);
        QString exponent;
        if (prefixLength == 0) {
            const int exponentPos = body.indexOf(RegExpPatterns::decimalExponentSuffix());
            if (exponentPos >= 0) {
                exponent = body.mid(exponentPos);
                exponent[0] = QLatin1Char('e');
                body = body.left(exponentPos);
            }
        }

        int radixPos = body.lastIndexOf(decimalSep);
        if (radixPos < 0)
            radixPos = body.lastIndexOf(altDecimalSep);

        auto stripGroupingChars = [&](const QString& part) {
            QString stripped;
            stripped.reserve(part.size());
            for (const QChar c : part) {
                if (c.isDigit() || c.isLetter())
                    stripped.append(c);
            }
            return stripped;
        };

        if (radixPos >= 0) {
            const QString integral = stripGroupingChars(body.left(radixPos));
            const QString fractional = stripGroupingChars(body.mid(radixPos + 1));
            QString groupedFractional = fractional;
            if (groupingEnabled && !settings->digitGroupingIntegerPartOnly)
                groupedFractional = groupPart(fractional, groupSize, false);
            token = prefix
                + (groupingEnabled ? groupIntegral(integral, groupSize) : integral)
                + decimalSep
                + groupedFractional
                + exponent;
        } else {
            const QString integral = stripGroupingChars(body);
            token = prefix + (groupingEnabled ? groupIntegral(integral, groupSize) : integral) + exponent;
        }

        output += token;
        lastPos = match.capturedEnd();
    }
    output += input.mid(lastPos);
    return output;
}

QString NumberFormatter::rewriteScientificNotationForDisplay(const QString& input)
{
    QString output;
    int lastPos = 0;
    auto it = RegExpPatterns::numericToken().globalMatch(input);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        output += input.mid(lastPos, match.capturedStart() - lastPos);

        const QString token = match.captured(0);
        QString rewritten = token;
        if (!token.startsWith(MathDsl::HexPrefix, Qt::CaseInsensitive)
            && !token.startsWith(MathDsl::OctPrefix, Qt::CaseInsensitive)
            && !token.startsWith(MathDsl::BinPrefix, Qt::CaseInsensitive)) {
            const int exponentPos = token.indexOf(RegExpPatterns::decimalExponentSuffix());
            if (exponentPos > 0) {
                const QString mantissa = token.left(exponentPos);
                QString exponent = token.mid(exponentPos + 1);

                bool negativeExponent = false;
                if (!exponent.isEmpty()) {
                    if (exponent.at(0) == MathDsl::AddOp) {
                        exponent.remove(0, 1);
                    } else if (exponent.at(0) == MathDsl::SubOpAl1
                               || exponent.at(0) == UnicodeChars::MinusSign) {
                        negativeExponent = true;
                        exponent.remove(0, 1);
                    }
                }

                QString superscript;
                if (negativeExponent)
                    superscript += MathDsl::PowNeg;
                for (const QChar ch : exponent) {
                    const QChar superscriptDigit = MathDsl::asciiDigitToSuperscript(ch);
                    if (superscriptDigit.isNull()) {
                        superscript.clear();
                        break;
                    }
                    superscript += superscriptDigit;
                }

                if (!superscript.isEmpty()) {
                    rewritten = mantissa
                        + QString(MathDsl::MulCrossWrapSp)
                        + MathDsl::MulCrossOp
                        + QString(MathDsl::MulCrossWrapSp)
                        + QStringLiteral("10")
                        + superscript;
                }
            }
        }

        output += rewritten;
        lastPos = match.capturedEnd();
    }
    output += input.mid(lastPos);
    return output;
}

bool NumberFormatter::tryFormatStandaloneNumericLiteralForDisplay(const QString& input, QString* output)
{
    if (!output)
        return false;

    QString canonical;
    if (!canonicalizeStandaloneNumericLiteral(input, &canonical))
        return false;

    *output = formatNumericLiteralForDisplay(canonical);
    return true;
}
