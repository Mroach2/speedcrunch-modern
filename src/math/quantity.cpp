// SPDX-FileCopyrightText: 2016-2018, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later



#include "quantity.h"

#include "core/unicodechars.h"
#include "core/mathdsl.h"
#include "core/settings.h"
#include "rational.h"
#include "core/units.h"

#include <QRegularExpression>
#include <QStringList>

#define RATIONAL_TOL HNumber("1e-20")

#define ENSURE_DIMENSIONLESS(x) \
    if (!(x).isDimensionless()) \
        return DMath::nan(InvalidDimension);

#define ENSURE_SAME_DIMENSION(x, y) \
    if ((!(x).sameDimension(y))) \
        return DMath::nan(DimensionMismatch);

namespace {

enum class SemanticUnitKind {
    None,
    Frequency,
    Activity,
    AbsorbedDose,
    EquivalentDose
};

enum class InformationUnitFamily {
    None,
    Bit,
    Byte
};

SemanticUnitKind semanticUnitKind(const QString& unitName)
{
    switch (unitId(normalizeUnitName(unitName))) {
    case UnitId::Hertz:
        return SemanticUnitKind::Frequency;
    case UnitId::Becquerel:
        return SemanticUnitKind::Activity;
    case UnitId::Gray:
        return SemanticUnitKind::AbsorbedDose;
    case UnitId::Sievert:
        return SemanticUnitKind::EquivalentDose;
    default:
        return SemanticUnitKind::None;
    }
}

InformationUnitFamily informationUnitFamily(const QString& unitName)
{
    QString n = unitName.trimmed();
    if (n.startsWith(MathDsl::GroupStart) && n.endsWith(MathDsl::GroupEnd) && n.size() > 2)
        n = n.mid(1, n.size() - 2).trimmed();
    if (n.isEmpty())
        return InformationUnitFamily::None;

    if (n.endsWith(QLatin1String("byte"), Qt::CaseInsensitive)
        || n.endsWith(QLatin1Char('B')))
        return InformationUnitFamily::Byte;
    if (n.endsWith(QLatin1String("bit"), Qt::CaseInsensitive)
        || n.endsWith(QLatin1Char('b')))
        return InformationUnitFamily::Bit;
    return InformationUnitFamily::None;
}

bool isSteradianUnitName(const QString& unitName)
{
    return unitId(normalizeUnitName(unitName)) == UnitId::Steradian;
}

enum class AffineTemperatureScale {
    None,
    Celsius,
    Fahrenheit
};

AffineTemperatureScale affineTemperatureScaleFromDisplayUnitName(const QString& unitName)
{
    QString normalized = UnicodeChars::normalizeUnitSymbolAliases(unitName).trimmed();
    if (normalized.isEmpty())
        return AffineTemperatureScale::None;

    auto matchesUnitAndAliases = [&](UnitId id) {
        if (normalized == ::unitSymbol(id) || normalized == ::unitName(id))
            return true;
        const QStringList aliases = Units::affineUnitAliases(id);
        for (const QString& alias : aliases) {
            if (normalized == UnicodeChars::normalizeUnitSymbolAliases(alias).trimmed())
                return true;
        }
        return false;
    };

    if (matchesUnitAndAliases(UnitId::DegreeCelsius)) {
        return AffineTemperatureScale::Celsius;
    }
    if (matchesUnitAndAliases(UnitId::DegreeFahrenheit)) {
        return AffineTemperatureScale::Fahrenheit;
    }

    return AffineTemperatureScale::None;
}

bool tryAffineScaleByRealScalar(const Quantity& affineQuantity,
                                const HNumber& scalar,
                                CNumber* scaledBaseValueOut)
{
    if (!scaledBaseValueOut
        || !affineQuantity.hasUnit()
        || !affineQuantity.numericValue().isNearReal())
    {
        return false;
    }

    const AffineTemperatureScale scale =
        affineTemperatureScaleFromDisplayUnitName(affineQuantity.unitName());
    if (scale == AffineTemperatureScale::None)
        return false;

    static const HNumber kelvinOffset("273.15");
    static const HNumber five(5);
    static const HNumber nine(9);
    static const HNumber thirtyTwo(32);

    HNumber affineValue;
    if (scale == AffineTemperatureScale::Celsius) {
        affineValue = affineQuantity.numericValue().real - kelvinOffset;
    } else {
        affineValue =
            (affineQuantity.numericValue().real - kelvinOffset) * (nine / five)
            + thirtyTwo;
    }

    const HNumber scaledAffineValue = affineValue * scalar;
    HNumber scaledBaseValue;
    if (scale == AffineTemperatureScale::Celsius) {
        scaledBaseValue = scaledAffineValue + kelvinOffset;
    } else {
        scaledBaseValue =
            ((scaledAffineValue - thirtyTwo) * (five / nine)) + kelvinOffset;
    }

    *scaledBaseValueOut = CNumber(scaledBaseValue);
    return true;
}

bool tryAffineValueFromBase(const Quantity& affineQuantity,
                            HNumber* affineValueOut,
                            AffineTemperatureScale* scaleOut = nullptr)
{
    if (!affineValueOut
        || !affineQuantity.hasUnit()
        || !affineQuantity.numericValue().isNearReal())
    {
        return false;
    }

    const AffineTemperatureScale scale =
        affineTemperatureScaleFromDisplayUnitName(affineQuantity.unitName());
    if (scale == AffineTemperatureScale::None)
        return false;

    static const HNumber kelvinOffset("273.15");
    static const HNumber five(5);
    static const HNumber nine(9);
    static const HNumber thirtyTwo(32);

    if (scale == AffineTemperatureScale::Celsius) {
        *affineValueOut = affineQuantity.numericValue().real - kelvinOffset;
    } else {
        *affineValueOut =
            (affineQuantity.numericValue().real - kelvinOffset) * (nine / five)
            + thirtyTwo;
    }

    if (scaleOut)
        *scaleOut = scale;
    return true;
}

bool tryAffineBaseFromValue(const HNumber& affineValue,
                            const AffineTemperatureScale scale,
                            HNumber* baseValueOut)
{
    if (!baseValueOut || scale == AffineTemperatureScale::None)
        return false;

    static const HNumber kelvinOffset("273.15");
    static const HNumber five(5);
    static const HNumber nine(9);
    static const HNumber thirtyTwo(32);

    if (scale == AffineTemperatureScale::Celsius) {
        *baseValueOut = affineValue + kelvinOffset;
    } else {
        *baseValueOut = ((affineValue - thirtyTwo) * (five / nine)) + kelvinOffset;
    }
    return true;
}

bool isExactDimension(const Quantity& q, const QMap<UnitQuantity, Rational>& expected)
{
    return q.getDimensionByQuantity() == expected;
}

bool parseUnitFactorAndExponent(const QString& token, QString* baseOut, int* exponentOut)
{
    if (!baseOut || !exponentOut)
        return false;
    QString t = token.trimmed();
    if (t.isEmpty())
        return false;

    int exponent = 1;
    int suffixStart = t.size();
    while (suffixStart > 0) {
        const QChar ch = t.at(suffixStart - 1);
        if (!UnicodeChars::isSuperscriptDigit(ch)
            && !UnicodeChars::isSuperscriptSign(ch))
        {
            break;
        }
        --suffixStart;
    }
    if (suffixStart < t.size()) {
        bool negative = false;
        int pos = suffixStart;
        if (t.at(pos) == MathDsl::PowNeg || t.at(pos) == MathDsl::PowPos) {
            negative = t.at(pos) == MathDsl::PowNeg;
            ++pos;
        }
        if (pos >= t.size())
            return false;
        QString digits;
        for (; pos < t.size(); ++pos) {
            const QChar ascii = MathDsl::superscriptDigitToAscii(t.at(pos));
            if (ascii.isNull())
                return false;
            digits += ascii;
        }
        bool ok = false;
        int value = digits.toInt(&ok);
        if (!ok)
            return false;
        exponent = negative ? -value : value;
        t = t.left(suffixStart).trimmed();
    } else {
        const int caret = t.lastIndexOf(MathDsl::PowOp);
        if (caret >= 0 && caret + 1 < t.size()) {
            bool ok = false;
            const int value = t.mid(caret + 1).trimmed().toInt(&ok);
            if (!ok)
                return false;
            exponent = value;
            t = t.left(caret).trimmed();
        }
    }

    if (t.isEmpty())
        return false;
    *baseOut = t;
    *exponentOut = exponent;
    return true;
}

QString formatUnitFactorWithExponent(const QString& base, int exponent)
{
    if (exponent == 1)
        return base;
    QString suffix;
    if (exponent < 0)
        suffix += MathDsl::PowNeg;
    const QString digits = QString::number(qAbs(exponent));
    for (const QChar ch : digits)
        suffix += MathDsl::asciiDigitToSuperscript(ch);
    return base + suffix;
}

bool unitTextContainsExplicitAngleFactor(const QString& unitText)
{
    if (unitText.isEmpty())
        return false;
    QString token;
    auto flushTokenIsAngle = [&]() {
        if (token.isEmpty())
            return false;
        QString base;
        int exponent = 0;
        const QString raw = token;
        token.clear();
        if (!parseUnitFactorAndExponent(raw, &base, &exponent))
            return false;
        return Units::isExplicitAngleUnitName(base);
    };
    for (int i = 0; i < unitText.size(); ++i) {
        const QChar ch = unitText.at(i);
        const bool isUnitChar =
            UnicodeChars::isUnitIdentifierChar(ch)
            || UnicodeChars::isSuperscriptDigit(ch)
            || UnicodeChars::isSuperscriptSign(ch)
            || ch == MathDsl::PowOp;
        if (isUnitChar) {
            token += ch;
            continue;
        }
        if (flushTokenIsAngle())
            return true;
    }
    return flushTokenIsAngle();
}

QString simplifyExplicitAngleCompositeDisplayUnits(const QString& unitName)
{
    QString normalized = unitName.trimmed();
    if (normalized.isEmpty())
        return normalized;

    if (!unitTextContainsExplicitAngleFactor(normalized))
        return normalized;
    if (normalized.contains(MathDsl::DivOp)
        || normalized.contains(MathDsl::AddOp)
        || normalized.contains(MathDsl::SubOpAl1)
        || normalized.contains(MathDsl::GroupStart)
        || normalized.contains(MathDsl::GroupEnd))
    {
        return normalized;
    }

    struct Factor {
        QString base;
        int exponent = 1;
    };
    QList<Factor> factors;
    QString token;
    auto flushToken = [&]() -> bool {
        if (token.isEmpty())
            return true;
        QString base;
        int exponent = 0;
        const QString raw = token;
        token.clear();
        if (!parseUnitFactorAndExponent(raw, &base, &exponent))
            return false;
        factors.append(Factor{base, exponent});
        return true;
    };
    const auto isMulSeparator = [](const QChar ch) {
        return ch.isSpace()
            || ch == MathDsl::MulOpAl1
            || ch == MathDsl::MulDotOp
            || ch == MathDsl::MulCrossOp;
    };
    for (int i = 0; i < normalized.size(); ++i) {
        const QChar ch = normalized.at(i);
        if (isMulSeparator(ch)) {
            if (!flushToken())
                return unitName;
            continue;
        }
        token += ch;
    }
    if (!flushToken())
        return unitName;
    if (factors.size() < 2)
        return normalized;

    const QString kgName = ::unitName(UnitId::Kilogram);
    const QString kgSymbol = ::unitSymbol(UnitId::Kilogram);
    const QString mName = ::unitName(UnitId::Metre);
    const QString mSymbol = ::unitSymbol(UnitId::Metre);
    const QString sName = ::unitName(UnitId::Second);
    const QString sSymbol = ::unitSymbol(UnitId::Second);
    const QString jSymbol = ::unitSymbol(UnitId::Joule);
    const QString metreName = ::unitName(UnitId::Metre);
    for (int i = 0; i < factors.size(); ++i) {
        if (factors[i].base == ::unitName(UnitId::SquareMetre)) {
            factors[i].base = metreName;
            factors[i].exponent *= 2;
        } else if (factors[i].base == ::unitName(UnitId::CubicMetre)) {
            factors[i].base = metreName;
            factors[i].exponent *= 3;
        }
    }

    int kgIndex = -1;
    int mIndex = -1;
    int sIndex = -1;
    for (int i = 0; i < factors.size(); ++i) {
        const QString b = factors.at(i).base;
        if (kgIndex < 0 && (b == kgName || b == kgSymbol))
            kgIndex = i;
        else if (mIndex < 0 && (b == mName || b == mSymbol))
            mIndex = i;
        else if (sIndex < 0 && (b == sName || b == sSymbol))
            sIndex = i;
    }

    if (kgIndex < 0 || mIndex < 0 || sIndex < 0) {
        return normalized;
    }
    if (factors[kgIndex].exponent <= 0 || factors[mIndex].exponent < 2) {
        return normalized;
    }

    const int jExp = qMin(factors[kgIndex].exponent, factors[mIndex].exponent / 2);
    if (jExp <= 0)
        return normalized;

    factors[kgIndex].exponent -= jExp;
    factors[mIndex].exponent -= 2 * jExp;
    factors[sIndex].exponent += 2 * jExp;

    int jIndex = -1;
    for (int i = 0; i < factors.size(); ++i) {
        if (factors.at(i).base == jSymbol) {
            jIndex = i;
            break;
        }
    }
    if (jIndex >= 0) {
        factors[jIndex].exponent += jExp;
    } else {
        factors.insert(kgIndex, Factor{jSymbol, jExp});
    }

    QStringList rendered;
    for (const Factor& factor : factors) {
        if (factor.exponent == 0)
            continue;
        rendered << formatUnitFactorWithExponent(factor.base, factor.exponent);
    }
    if (rendered.isEmpty())
        return normalized;
    return rendered.join(QString(MathDsl::MulDotOp));
}

QString simplifyRepeatedCompositeDisplayUnitFactors(const QString& unitName)
{
    QString normalized = unitName.trimmed();
    if (normalized.isEmpty())
        return normalized;

    if (normalized.contains(MathDsl::DivOp)
        || normalized.contains(MathDsl::AddOp)
        || normalized.contains(MathDsl::SubOpAl1)
        || normalized.contains(MathDsl::GroupStart)
        || normalized.contains(MathDsl::GroupEnd))
    {
        return normalized;
    }

    QMap<QString, int> exponentsByBase;
    QStringList order;
    QString token;
    const auto isMulSeparator = [](const QChar ch) {
        return ch.isSpace()
            || ch == MathDsl::MulOpAl1
            || ch == MathDsl::MulDotOp
            || ch == MathDsl::MulCrossOp;
    };
    auto flushToken = [&]() -> bool {
        const QString raw = token.trimmed();
        token.clear();
        if (raw.isEmpty())
            return true;
        QString base;
        int exponent = 0;
        if (!parseUnitFactorAndExponent(raw, &base, &exponent))
            return false;
        if (!exponentsByBase.contains(base))
            order.append(base);
        exponentsByBase[base] += exponent;
        return true;
    };

    for (int i = 0; i < normalized.size(); ++i) {
        const QChar ch = normalized.at(i);
        if (isMulSeparator(ch)) {
            if (!flushToken())
                return unitName;
            continue;
        }
        token += ch;
    }
    if (!flushToken())
        return unitName;

    QStringList rendered;
    for (const QString& base : order) {
        const int exponent = exponentsByBase.value(base);
        if (exponent == 0)
            continue;
        rendered << formatUnitFactorWithExponent(base, exponent);
    }
    if (rendered.isEmpty())
        return normalized;
    return rendered.join(QString(MathDsl::MulDotOp));
}

QString normalizeDisplayUnitNameForOutput(const QString& unitName)
{
    QString normalized = unitName.trimmed();
    if (normalized.isEmpty())
        return normalized;

    if (unitId(normalized) == UnitId::Degree) {
        return QString(UnicodeChars::DegreeSign);
    }

    const bool isCompositeUnit =
        normalized.contains(MathDsl::MulOpAl1)
        || normalized.contains(MathDsl::MulDotOp)
        || normalized.contains(MathDsl::MulCrossOp)
        || normalized.contains(MathDsl::DivOp);
    if (isCompositeUnit
        && (normalized.contains(UnicodeChars::DegreeSign)
            || normalized.contains(UnicodeChars::MasculineOrdinalIndicator)))
    {
        normalized.replace(UnicodeChars::MasculineOrdinalIndicator, UnicodeChars::DegreeSign);
        normalized.replace(QString(UnicodeChars::DegreeSign), Units::degreeAliasSymbol());
    }
    normalized = simplifyExplicitAngleCompositeDisplayUnits(normalized);

    // Keep explicit conversion-expression targets unchanged; they are meant
    // to mirror user-entered structure (operators and grouping).
    if (normalized.contains(MathDsl::MulOpAl1)
        || normalized.contains(MathDsl::MulDotOp)
        || normalized.contains(MathDsl::MulCrossOp)
        || normalized.contains(MathDsl::DivOp)
        || normalized.contains(MathDsl::AddOp)
        || normalized.contains(MathDsl::SubOpAl1)
        || normalized.contains(MathDsl::GroupStart)
        || normalized.contains(MathDsl::GroupEnd)
        || normalized.contains(QLatin1String("->"))
        || normalized.contains(MathDsl::TransOp))
    {
        return normalized;
    }

    normalized.replace(QRegularExpression(QStringLiteral("\\s+")),
                       QString(MathDsl::MulDotOp));
    return normalized;
}

QString composeProductUnitName(const QString& left, const QString& right)
{
    const QString l = left.trimmed();
    const QString r = right.trimmed();
    if (l.isEmpty())
        return r;
    if (r.isEmpty())
        return l;
    return l + QLatin1Char(' ') + r;
}

bool needsGroupingInQuotient(const QString& unitName)
{
    const QString n = unitName.trimmed();
    return n.contains(QLatin1Char(' '))
           || n.contains(MathDsl::MulOpAl1)
           || n.contains(MathDsl::MulDotOp)
           || n.contains(MathDsl::MulCrossOp)
           || n.contains(QLatin1Char('/'));
}

QString composeQuotientUnitName(const QString& numerator, const QString& denominator)
{
    const QString n = numerator.trimmed();
    QString d = denominator.trimmed();
    if (n.isEmpty())
        return d;
    if (d.isEmpty())
        return n;
    if (needsGroupingInQuotient(d))
        d = QString(MathDsl::GroupStart) + d + QString(MathDsl::GroupEnd);
    return n + QStringLiteral(" / ") + d;
}

QString composeAnglePerSecondUnitName(const QString& angleUnitName)
{
    const QString secondName = ::unitName(UnitId::Second);
    if (runtimeUnitNegativeExponentStyle() == Settings::UnitNegativeExponentSuperscript) {
        return composeProductUnitName(
            angleUnitName,
            secondName + QString(MathDsl::PowNeg) + QString(MathDsl::Pow1));
    }
    return composeQuotientUnitName(angleUnitName, secondName);
}

QMap<UnitQuantity, Rational> dimensionCandela()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::LuminousIntensity, Rational(1));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionMetre()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::Length, Rational(1));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionSecond()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::Time, Rational(1));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionAmpere()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::ElectricCurrent, Rational(1));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionCubicMetre()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::Length, Rational(3));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionSquareMetre()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::Length, Rational(2));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionPascal()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::Mass, Rational(1));
    dim.insert(UnitQuantity::Length, Rational(-1));
    dim.insert(UnitQuantity::Time, Rational(-2));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionNewton()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::Mass, Rational(1));
    dim.insert(UnitQuantity::Length, Rational(1));
    dim.insert(UnitQuantity::Time, Rational(-2));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionJoule()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::Mass, Rational(1));
    dim.insert(UnitQuantity::Length, Rational(2));
    dim.insert(UnitQuantity::Time, Rational(-2));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionWatt()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::Mass, Rational(1));
    dim.insert(UnitQuantity::Length, Rational(2));
    dim.insert(UnitQuantity::Time, Rational(-3));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionVolt()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::Mass, Rational(1));
    dim.insert(UnitQuantity::Length, Rational(2));
    dim.insert(UnitQuantity::Time, Rational(-3));
    dim.insert(UnitQuantity::ElectricCurrent, Rational(-1));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionFarad()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::Mass, Rational(-1));
    dim.insert(UnitQuantity::Length, Rational(-2));
    dim.insert(UnitQuantity::Time, Rational(4));
    dim.insert(UnitQuantity::ElectricCurrent, Rational(2));
    return dim;
}

QMap<UnitQuantity, Rational> dimensionTesla()
{
    QMap<UnitQuantity, Rational> dim;
    dim.insert(UnitQuantity::Mass, Rational(1));
    dim.insert(UnitQuantity::Time, Rational(-2));
    dim.insert(UnitQuantity::ElectricCurrent, Rational(-1));
    return dim;
}

}

Quantity operator-(const Quantity& q)
{
    Quantity res(q);
    res.m_numericValue = -res.m_numericValue;
    return res;
}

Quantity operator-(const Quantity& a, const Quantity& b)
{
    Quantity res(a);
    res.m_format = Quantity::Format();
    if (!a.sameDimension(b))
        return DMath::nan(DimensionMismatch);
    const SemanticUnitKind k1 = semanticUnitKind(a.unitName());
    const SemanticUnitKind k2 = semanticUnitKind(b.unitName());
    if (k1 != SemanticUnitKind::None
        && k2 != SemanticUnitKind::None
        && k1 != k2)
    {
        return DMath::nan(DimensionMismatch);
    }
    HNumber lhsAffine;
    HNumber rhsAffine;
    AffineTemperatureScale lhsScale = AffineTemperatureScale::None;
    AffineTemperatureScale rhsScale = AffineTemperatureScale::None;
    const bool lhsIsAffine = tryAffineValueFromBase(a, &lhsAffine, &lhsScale);
    const bool rhsIsAffine = tryAffineValueFromBase(b, &rhsAffine, &rhsScale);
    if (lhsIsAffine && rhsIsAffine && lhsScale == rhsScale) {
        HNumber baseDiff;
        if (tryAffineBaseFromValue(lhsAffine - rhsAffine, lhsScale, &baseDiff))
            res.m_numericValue = CNumber(baseDiff);
        else
            res.m_numericValue -= b.m_numericValue;
    } else {
        res.m_numericValue -= b.m_numericValue;
    }
    return res;
}

bool operator>(const Quantity& l, const Quantity& r)
{
    if (l.sameDimension(r))
        return l.m_numericValue > r.m_numericValue;
    return false;
}

bool operator<(const Quantity& l, const Quantity& r)
{
    if (l.sameDimension(r))
        return l.m_numericValue < r.m_numericValue;
    return false;
}

bool operator>=(const Quantity& l, const Quantity& r)
{
    if (l.sameDimension(r))
        return l.m_numericValue >= r.m_numericValue;
    return false;
}

bool operator<=(const Quantity& l, const Quantity& r)
{
    if (l.sameDimension(r))
        return l.m_numericValue <= r.m_numericValue;
    return false;
}

bool operator==(const Quantity& l, const Quantity& r)
{
    if (l.sameDimension(r))
        return l.m_numericValue == r.m_numericValue;
    return false;
}

// Returns TRUE upon dimension mismatch.
bool operator!=(const Quantity& l, const Quantity& r)
{
    if (l.sameDimension(r))
        return l.m_numericValue != r.m_numericValue;
    return true;
}

Quantity operator*(const HNumber& l, const Quantity& r)
{
    return r * l;
}

Quantity operator*(const CNumber& l, const Quantity& r)
{
    return r * l;
}

Quantity operator/(const HNumber& l, const Quantity& r)
{
    return Quantity(l) / r;
}

Quantity operator/(const CNumber& l, const Quantity& r)
{
    return Quantity(l) / r;
}

Quantity::Quantity()
    : m_numericValue(0)
    , m_unit(nullptr)
    , m_unitName("")
{
}

Quantity::Quantity(const Quantity& other)
    : m_numericValue(other.m_numericValue)
    , m_dimension(other.m_dimension)
    , m_unit(nullptr)
    , m_unitName(other.m_unitName)
    , m_format(other.m_format)
{
    if (other.hasUnit())
        this->m_unit = new CNumber(other.unit());
    cleanDimension();
}

Quantity::Quantity(int i)
    : Quantity(CNumber(i))
{
}

Quantity::Quantity(const QJsonObject& json)
    : Quantity()
{
    *this = deSerialize(json);
}

Quantity::Quantity(const HNumber& h)
    : Quantity(CNumber(h))
{
}

Quantity::Quantity(const CNumber& c)
    : Quantity()
{
    this->m_numericValue = c;
}

Quantity::~Quantity()
{
    delete m_unit;
}

bool Quantity::isNan() const
{
    return m_numericValue.isNan();
}

bool Quantity::isZero() const
{
    return m_numericValue.isZero();
}

bool Quantity::isReal() const
{
    return m_numericValue.isReal();
}

bool Quantity::isPositive() const
{
    return m_numericValue.isPositive();
}

bool Quantity::isNegative() const
{
    return m_numericValue.isNegative();
}

bool Quantity::isInteger() const
{
    return (!this->hasDimension() && !this->hasUnit()) && m_numericValue.isInteger();
}

bool Quantity::hasUnit() const
{
    return this->m_unit != NULL;
}

CNumber Quantity::unit() const
{
    if (this->hasUnit())
        return CNumber(*(this->m_unit));
    return CNumber(1);
}

QString Quantity::unitName() const
{
    if (this->hasUnit())
        return m_unitName;
    return "";
}

CNumber Quantity::numericValue() const
{
    return m_numericValue;
}

Quantity& Quantity::setDisplayUnit(const CNumber unit, const QString& name)
{
    if (unit.isNan())
        *this = DMath::nan(InvalidDimension);
    else {
        stripUnits();
        m_unit = new CNumber(unit);
        m_unitName = name;
    }
    return *this;
}

Quantity& Quantity::setFormat(Format c)
{
    m_format = c;
    return *this;
}

void Quantity::stripUnits()
{
    delete m_unit;
    m_unit = nullptr;
    m_unitName = "";
}

bool Quantity::hasDimension() const
{
    return !this->m_dimension.empty();
}

/*
 * Unlike hasDimension(), this does a clean up first, i.e. it
 * checks for redundant exponents.
 */
bool Quantity::isDimensionless() const
{
    Quantity temp(*this);
    temp.cleanDimension();
    return temp.m_dimension.empty();
}

QMap<QString, Rational> Quantity::getDimension() const
{
    Quantity temp(*this);
    temp.cleanDimension();
    QMap<QString, Rational> result;
    for (auto it = temp.m_dimension.constBegin(); it != temp.m_dimension.constEnd(); ++it) {
        const QString key = unitQuantityDimensionKey(it.key());
        if (key.isEmpty())
            continue;
        result.insert(key, it.value());
    }
    return result;
}

QMap<UnitQuantity, Rational> Quantity::getDimensionByQuantity() const
{
    Quantity temp(*this);
    temp.cleanDimension();
    return temp.m_dimension;
}

void Quantity::modifyDimension(const QString& key, const Rational& exponent)
{
    UnitQuantity quantity = UnitQuantity::Unspecified;
    if (!unitQuantityFromDimensionKey(key, &quantity))
        return;
    modifyDimension(quantity, exponent);
}

void Quantity::modifyDimension(UnitQuantity key, const Rational& exponent)
{
    if (exponent.isZero())
        m_dimension.remove(key);
    else
        m_dimension.insert(key, exponent);
}

void Quantity::copyDimension(const Quantity& other)
{
    clearDimension();
    this->m_dimension = other.m_dimension;
}

void Quantity::clearDimension()
{
    this->m_dimension.clear();
}

// Note: Does NOT clean the dimension vector first.
// The calling function must do so on its own.
bool Quantity::sameDimension(const Quantity& other) const
{
    return this->m_dimension == other.m_dimension;
}

void Quantity::cleanDimension()
{
    auto i = m_dimension.begin();
    while (i != m_dimension.end()) {
        if (i.value().isZero())
            i = m_dimension.erase(i);
        else
            ++i;
    }
}

void Quantity::serialize(QJsonObject& json) const
{
    json["val"] = CMath::format(
        m_numericValue,
        CNumber::Format::Fixed() + CNumber::Format::Precision(DECPRECISION));

    if (hasDimension()) {
        QJsonObject dim_json;
        auto i = m_dimension.constBegin();
        while (i != m_dimension.constEnd()) {
            const auto& exp = i.value();
            const QString key = unitQuantityDimensionKey(i.key());
            if (!key.isEmpty())
                dim_json[key] = exp.toString();
            ++i;
        }
        json["dim"] = dim_json;
    }

    if (hasUnit()) {
        QJsonObject unit_json;
        m_unit->serialize(unit_json);
        json["unit"] = unit_json;
        json["unit_name"] = m_unitName;
    }

    if (!m_format.isNull()) {
        QJsonObject format_json;
        m_format.serialize(format_json);
        json["format"] = format_json;
    }
}

Quantity Quantity::deSerialize(const QJsonObject& json)
{
    Quantity result;
    if (json.contains("val")) {
        QString str = json["val"].toString();
        str.replace(",", ".");
        result.m_numericValue = CNumber(str.toLatin1().constData());
    }
    result.stripUnits();
    if (json.contains("unit")) {
        QJsonObject unit_json = json["unit"].toObject();
        result.m_unit = new CNumber(unit_json);
    }
    if (json.contains("unit_name"))
        result.m_unitName = json["unit_name"].toString();

    if (json.contains("dim")) {
        QJsonObject dim_json = json["dim"].toObject();
        for (int i = 0; i < dim_json.count(); ++i) {
            auto key = dim_json.keys().at(i);
            Rational val(dim_json[key].toString());
            UnitQuantity quantity = UnitQuantity::Unspecified;
            if (unitQuantityFromDimensionKey(key, &quantity))
                result.modifyDimension(quantity, val);
        }
    }
    if (json.contains("format")) {
        QJsonObject format_json = json["format"].toObject();
        result.m_format = Quantity::Format::deSerialize(format_json);
    }
    return result;
}

Error Quantity::error() const
{
    return m_numericValue.error();
}

Quantity& Quantity::operator=(const Quantity& other)
{
    m_numericValue = other.m_numericValue;
    m_dimension = other.m_dimension;
    m_format = other.m_format;
    stripUnits();
    if(other.hasUnit()) {
        m_unit = new CNumber(*other.m_unit);
        m_unitName = other.m_unitName;
    }
    cleanDimension();
    return *this;
}

Quantity Quantity::operator+(const Quantity& other) const
{
    if (!this->sameDimension(other))
        return DMath::nan(DimensionMismatch);
    const InformationUnitFamily info1 = informationUnitFamily(this->unitName());
    const InformationUnitFamily info2 = informationUnitFamily(other.unitName());
    if (info1 != InformationUnitFamily::None
        && info2 != InformationUnitFamily::None
        && info1 != info2)
    {
        // Do not mix byte-family and bit-family implicitly.
        return DMath::nan(DimensionMismatch);
    }
    const SemanticUnitKind k1 = semanticUnitKind(this->unitName());
    const SemanticUnitKind k2 = semanticUnitKind(other.unitName());
    if (k1 != SemanticUnitKind::None
        && k2 != SemanticUnitKind::None
        && k1 != k2)
    {
        return DMath::nan(DimensionMismatch);
    }
    Quantity result(*this);
    result.m_format = Quantity::Format();
    HNumber lhsAffine;
    HNumber rhsAffine;
    AffineTemperatureScale lhsScale = AffineTemperatureScale::None;
    AffineTemperatureScale rhsScale = AffineTemperatureScale::None;
    const bool lhsIsAffine = tryAffineValueFromBase(*this, &lhsAffine, &lhsScale);
    const bool rhsIsAffine = tryAffineValueFromBase(other, &rhsAffine, &rhsScale);
    if (lhsIsAffine && rhsIsAffine && lhsScale == rhsScale) {
        HNumber baseSum;
        if (tryAffineBaseFromValue(lhsAffine + rhsAffine, lhsScale, &baseSum))
            result.m_numericValue = CNumber(baseSum);
        else
            result.m_numericValue += other.m_numericValue;
    } else {
        result.m_numericValue += other.m_numericValue;
    }
    if (info1 != InformationUnitFamily::None && info1 == info2
        && this->hasUnit() && other.hasUnit())
    {
        // For storage-style math keep the coarsest explicit unit from operands.
        if (this->unit().real >= other.unit().real)
            result.setDisplayUnit(this->unit(), this->unitName());
        else
            result.setDisplayUnit(other.unit(), other.unitName());
    }
    return result;
}

Quantity& Quantity::operator+=(const Quantity& other)
{
    if (!this->sameDimension(other))
        *this = DMath::nan(DimensionMismatch);
    else {
        const InformationUnitFamily info1 = informationUnitFamily(this->unitName());
        const InformationUnitFamily info2 = informationUnitFamily(other.unitName());
        if (info1 != InformationUnitFamily::None
            && info2 != InformationUnitFamily::None
            && info1 != info2)
        {
            *this = DMath::nan(DimensionMismatch);
            return *this;
        }
        const SemanticUnitKind k1 = semanticUnitKind(this->unitName());
        const SemanticUnitKind k2 = semanticUnitKind(other.unitName());
        if (k1 != SemanticUnitKind::None
            && k2 != SemanticUnitKind::None
            && k1 != k2)
        {
            *this = DMath::nan(DimensionMismatch);
            return *this;
        }
        HNumber lhsAffine;
        HNumber rhsAffine;
        AffineTemperatureScale lhsScale = AffineTemperatureScale::None;
        AffineTemperatureScale rhsScale = AffineTemperatureScale::None;
        const bool lhsIsAffine = tryAffineValueFromBase(*this, &lhsAffine, &lhsScale);
        const bool rhsIsAffine = tryAffineValueFromBase(other, &rhsAffine, &rhsScale);
        if (lhsIsAffine && rhsIsAffine && lhsScale == rhsScale) {
            HNumber baseSum;
            if (tryAffineBaseFromValue(lhsAffine + rhsAffine, lhsScale, &baseSum))
                this->m_numericValue = CNumber(baseSum);
            else
                this->m_numericValue += other.m_numericValue;
        } else {
            this->m_numericValue += other.m_numericValue;
        }
        if (info1 != InformationUnitFamily::None && info1 == info2
            && this->hasUnit() && other.hasUnit())
        {
            if (this->unit().real < other.unit().real)
                this->setDisplayUnit(other.unit(), other.unitName());
        }
    }
    return *this;
}

Quantity& Quantity::operator-=(const Quantity& other)
{
    return operator=(*this - other);
}

Quantity Quantity::operator*(const Quantity& other) const
{
    Quantity result(*this);

    if (this->isDimensionless()
        && this->numericValue().isNearReal()
        && other.hasUnit()
        && affineTemperatureScaleFromDisplayUnitName(other.unitName()) != AffineTemperatureScale::None)
    {
        CNumber scaledBaseValue;
        if (tryAffineScaleByRealScalar(other, this->numericValue().real, &scaledBaseValue)) {
            result = other;
            result.m_numericValue = scaledBaseValue;
            return result;
        }
    } else if (other.isDimensionless()
               && other.numericValue().isNearReal()
               && this->hasUnit()
               && affineTemperatureScaleFromDisplayUnitName(this->unitName()) != AffineTemperatureScale::None)
    {
        CNumber scaledBaseValue;
        if (tryAffineScaleByRealScalar(*this, other.numericValue().real, &scaledBaseValue)) {
            result = *this;
            result.m_numericValue = scaledBaseValue;
            return result;
        }
    }

    result.m_numericValue *= other.m_numericValue;
    const bool lhsExplicitAngleUnit =
        this->hasUnit() && Units::isExplicitAngleUnitName(this->unitName());
    const bool rhsExplicitAngleUnit =
        other.hasUnit() && Units::isExplicitAngleUnitName(other.unitName());
    const bool lhsExplicitAngleDimensionless =
        lhsExplicitAngleUnit && this->isDimensionless();
    const bool rhsExplicitAngleDimensionless =
        rhsExplicitAngleUnit && other.isDimensionless();
    const auto isPureInverseTimeQuantity = [](const Quantity& quantity) {
        QMap<UnitQuantity, Rational> dimension = quantity.getDimensionByQuantity();
        return dimension.size() == 1
               && dimension.contains(UnitQuantity::Time)
               && dimension.value(UnitQuantity::Time) == Rational(-1);
    };
    const auto copyDisplayUnit = [&](const Quantity& source) {
        result.stripUnits();
        result.m_unit = new CNumber(source.unit());
        result.m_unitName = source.unitName();
    };
    const auto unitTextContainsExplicitAngleFactor = [](const QString& unitText) {
        if (unitText.isEmpty())
            return false;
        QString token;
        auto flushTokenIsAngle = [&]() {
            if (token.isEmpty())
                return false;
            QString base = token.trimmed();
            token.clear();
            while (!base.isEmpty()
                   && (UnicodeChars::isSuperscriptDigit(base.at(base.size() - 1))
                       || UnicodeChars::isSuperscriptSign(base.at(base.size() - 1))))
            {
                base.chop(1);
            }
            return !base.isEmpty() && Units::isExplicitAngleUnitName(base);
        };
        for (int i = 0; i < unitText.size(); ++i) {
            const QChar ch = unitText.at(i);
            const bool isUnitChar =
                UnicodeChars::isUnitIdentifierChar(ch)
                || UnicodeChars::isSuperscriptDigit(ch)
                || UnicodeChars::isSuperscriptSign(ch);
            if (isUnitChar) {
                token += ch;
                continue;
            }
            if (flushTokenIsAngle())
                return true;
        }
        return flushTokenIsAngle();
    };
    if (!other.isDimensionless()) {
        result.stripUnits();
        auto i = other.m_dimension.constBegin();
        while (i != other.m_dimension.constEnd()) {
            const auto& exp = i.value();
            const auto& name = i.key();
            if (!result.m_dimension.contains(name))
                result.m_dimension[name] = Rational(0);
            result.m_dimension[name] += exp;
            ++i;
        }
        result.cleanDimension();

        // Preserve preferred display unit when multiplying a dimensionless
        // value by a quantity with an explicit display unit.
        if (this->isDimensionless()
            && other.hasUnit()
            && !lhsExplicitAngleUnit)
        {
            result.m_unit = new CNumber(*other.m_unit);
            result.m_unitName = other.m_unitName;
        }
    }

    if (!result.hasUnit()) {
        const QString n1 = normalizeUnitName(this->unitName());
        const QString n2 = normalizeUnitName(other.unitName());
        const UnitId u1 = unitId(n1);
        const UnitId u2 = unitId(n2);
        const bool hasExplicitAngleFactorOperand =
            unitTextContainsExplicitAngleFactor(this->unitName())
            || unitTextContainsExplicitAngleFactor(other.unitName());
        const bool isNewtonMetrePair =
            (u1 == UnitId::Newton && u2 == UnitId::Metre)
            || (u1 == UnitId::Metre && u2 == UnitId::Newton)
            || (isExactDimension(*this, dimensionNewton()) && isExactDimension(other, dimensionMetre()))
            || (isExactDimension(*this, dimensionMetre()) && isExactDimension(other, dimensionNewton()));
        if (!hasExplicitAngleFactorOperand && isNewtonMetrePair) {
            // Keep N·m explicit by default because it is semantically ambiguous:
            // energy is expressed in J, while torque is commonly expressed as N·m.
            // Users can still request an explicit conversion target ("-> joule").
            result.setDisplayUnit(this->unit() * other.unit(), QString(unitPhrase(UnitPhraseId::NewtonMetre)));
        } else if (!hasExplicitAngleFactorOperand
                   && ((u1 == UnitId::Pascal
                    && isExactDimension(other, dimensionCubicMetre()))
                   || (u2 == UnitId::Pascal
                       && isExactDimension(*this, dimensionCubicMetre()))))
        {
            result.setDisplayUnit(this->unit() * other.unit(), QString(::unitName(UnitId::Joule)));
        } else if (!hasExplicitAngleFactorOperand
                   && ((u1 == UnitId::Watt && u2 == UnitId::Second)
                   || (u2 == UnitId::Watt && u1 == UnitId::Second)
                   || (isExactDimension(*this, dimensionWatt())
                       && isExactDimension(other, dimensionSecond())
                       && !other.hasUnit())
                   || (isExactDimension(*this, dimensionSecond())
                       && isExactDimension(other, dimensionWatt())
                       && !this->hasUnit())))
        {
            // Keep W·s explicit by default: although dimensionally equal to J,
            // it often conveys time-integration context (power over time).
            // Auto-collapsing to J can hide that intent unless user requests
            // an explicit conversion ("-> joule").
            result.setDisplayUnit(this->unit() * other.unit(), QString(unitPhrase(UnitPhraseId::WattSecond)));
        } else if (!hasExplicitAngleFactorOperand
                   && ((u1 == UnitId::Coulomb && u2 == UnitId::Volt)
                       || (u2 == UnitId::Coulomb && u1 == UnitId::Volt)))
        {
            result.setDisplayUnit(this->unit() * other.unit(), QString(::unitName(UnitId::Joule)));
        } else if (!hasExplicitAngleFactorOperand
                   && ((isExactDimension(*this, dimensionVolt())
                    && isExactDimension(other, dimensionSecond()))
                   || (isExactDimension(other, dimensionVolt())
                       && isExactDimension(*this, dimensionSecond()))))
        {
            result.setDisplayUnit(this->unit() * other.unit(), QString(::unitName(UnitId::Weber)));
        } else if (!hasExplicitAngleFactorOperand
                   && ((isExactDimension(*this, dimensionVolt())
                    && isExactDimension(other, dimensionAmpere()))
                   || (isExactDimension(other, dimensionVolt())
                       && isExactDimension(*this, dimensionAmpere()))))
        {
            result.setDisplayUnit(this->unit() * other.unit(), QString(::unitName(UnitId::Watt)));
        } else if (!hasExplicitAngleFactorOperand
                   && ((isExactDimension(*this, dimensionWatt())
                    && isExactDimension(other, dimensionVolt()))
                   || (isExactDimension(other, dimensionWatt())
                       && isExactDimension(*this, dimensionVolt()))))
        {
            result.setDisplayUnit(this->unit() * other.unit(),
                                  QString(unitPhrase(UnitPhraseId::VoltSquaredAmpere)));
        } else if (!hasExplicitAngleFactorOperand
                   && ((isExactDimension(*this, dimensionFarad())
                    && isExactDimension(other, dimensionVolt()))
                   || (isExactDimension(other, dimensionFarad())
                       && isExactDimension(*this, dimensionVolt()))))
        {
            result.setDisplayUnit(this->unit() * other.unit(), QString(::unitName(UnitId::Coulomb)));
        } else if (!hasExplicitAngleFactorOperand
                   && ((isExactDimension(*this, dimensionTesla())
                    && isExactDimension(other, dimensionSquareMetre()))
                   || (isExactDimension(other, dimensionTesla())
                       && isExactDimension(*this, dimensionSquareMetre()))))
        {
            result.setDisplayUnit(this->unit() * other.unit(), QString(::unitName(UnitId::Weber)));
        } else if (!hasExplicitAngleFactorOperand
                   && ((isExactDimension(*this, dimensionPascal())
                    && isExactDimension(other, dimensionSquareMetre()))
                   || (isExactDimension(other, dimensionPascal())
                       && isExactDimension(*this, dimensionSquareMetre()))))
        {
            result.setDisplayUnit(this->unit() * other.unit(), QString(::unitName(UnitId::Newton)));
        } else if (((this->hasUnit() && Units::isExplicitAngleUnitName(this->unitName())
                     && isPureInverseTimeQuantity(other))
                    || (other.hasUnit() && Units::isExplicitAngleUnitName(other.unitName())
                        && isPureInverseTimeQuantity(*this))))
        {
            const QString angleUnitName =
                this->hasUnit() && Units::isExplicitAngleUnitName(this->unitName())
                ? this->unitName()
                : other.unitName();
            result.setDisplayUnit(this->unit() * other.unit(),
                                  composeAnglePerSecondUnitName(angleUnitName));
        } else if (this->hasUnit()
                   && other.hasUnit()
                   && !result.isDimensionless()
                   && !(isExactDimension(*this, dimensionMetre())
                        && isExactDimension(other, dimensionMetre()))) {
            result.setDisplayUnit(this->unit() * other.unit(),
                                  composeProductUnitName(this->unitName(), other.unitName()));
        } else if (!hasExplicitAngleFactorOperand
                   && ((isSteradianUnitName(n1)
                    && isExactDimension(other, dimensionCandela()))
                   || (isSteradianUnitName(n2)
                       && isExactDimension(*this, dimensionCandela()))))
        {
            // This mapping is unambiguous in this context: cd·sr is luminous flux.
            result.setDisplayUnit(this->unit() * other.unit(), QString(::unitName(UnitId::Lumen)));
        }
    }

    if (!result.hasUnit()) {
        if (this->isDimensionless() && other.hasUnit()) {
            copyDisplayUnit(other);
        } else if (other.isDimensionless() && this->hasUnit()) {
            copyDisplayUnit(*this);
        }
    }

    if (lhsExplicitAngleDimensionless && !other.isDimensionless()) {
        QString otherUnitName = other.unitName();
        if (otherUnitName.isEmpty()) {
            Quantity canonical(other);
            canonical.cleanDimension();
            Units::findUnit(canonical);
            otherUnitName = canonical.unitName();
        }
        const QString displayName =
            isPureInverseTimeQuantity(other)
            ? composeAnglePerSecondUnitName(this->unitName())
            : composeProductUnitName(this->unitName(), otherUnitName);
        result.setDisplayUnit(this->unit() * other.unit(), displayName);
    } else if (rhsExplicitAngleDimensionless && !this->isDimensionless()) {
        QString thisUnitName = this->unitName();
        if (thisUnitName.isEmpty()) {
            Quantity canonical(*this);
            canonical.cleanDimension();
            Units::findUnit(canonical);
            thisUnitName = canonical.unitName();
        }
        const QString displayName =
            isPureInverseTimeQuantity(*this)
            ? composeAnglePerSecondUnitName(other.unitName())
            : composeProductUnitName(other.unitName(), thisUnitName);
        result.setDisplayUnit(this->unit() * other.unit(), displayName);
    }

    if (!result.hasUnit() && !result.isDimensionless()) {
        if (this->hasUnit()
            && !other.hasUnit()
            && !other.isDimensionless()
            && unitTextContainsExplicitAngleFactor(this->unitName()))
        {
            Quantity canonical(other);
            canonical.cleanDimension();
            Units::findUnit(canonical);
            if (canonical.hasUnit()) {
                result.setDisplayUnit(this->unit() * other.unit(),
                                      composeProductUnitName(this->unitName(),
                                                             canonical.unitName()));
            }
        } else if (other.hasUnit()
                   && !this->hasUnit()
                   && !this->isDimensionless()
                   && unitTextContainsExplicitAngleFactor(other.unitName()))
        {
            Quantity canonical(*this);
            canonical.cleanDimension();
            Units::findUnit(canonical);
            if (canonical.hasUnit()) {
                result.setDisplayUnit(this->unit() * other.unit(),
                                      composeProductUnitName(other.unitName(),
                                                             canonical.unitName()));
            }
        }
    }

    if (result.hasUnit()) {
        const QString factorSimplified = simplifyRepeatedCompositeDisplayUnitFactors(result.unitName());
        if (factorSimplified != result.unitName())
            result.setDisplayUnit(result.unit(), factorSimplified);
        const QString simplified = simplifyExplicitAngleCompositeDisplayUnits(result.unitName());
        if (simplified != result.unitName())
            result.setDisplayUnit(result.unit(), simplified);
    }

    return result;
}

Quantity Quantity::operator*(const CNumber& other) const
{
    Quantity result(*this);
    result.m_numericValue *= other;
    return result;
}

Quantity Quantity::operator*(const HNumber& other) const
{
    return operator*(CNumber(other));
}

Quantity &Quantity::operator*=(const Quantity& other)
{
    return operator=(*this * other);
}

Quantity Quantity::operator/(const Quantity& other) const
{
    Quantity result(*this);

    if (other.isDimensionless()
        && other.numericValue().isNearReal()
        && this->hasUnit()
        && affineTemperatureScaleFromDisplayUnitName(this->unitName()) != AffineTemperatureScale::None)
    {
        CNumber scaledBaseValue;
        if (tryAffineScaleByRealScalar(*this,
                                       HNumber(1) / other.numericValue().real,
                                       &scaledBaseValue))
        {
            result = *this;
            result.m_numericValue = scaledBaseValue;
            return result;
        }
    }

    result.m_numericValue /= other.m_numericValue;
    const auto isPureTimeQuantity = [](const Quantity& quantity) {
        QMap<UnitQuantity, Rational> dimension = quantity.getDimensionByQuantity();
        return dimension.size() == 1
               && dimension.contains(UnitQuantity::Time)
               && dimension.value(UnitQuantity::Time) == Rational(1);
    };
    const auto copyDisplayUnit = [&](const Quantity& source) {
        result.stripUnits();
        result.m_unit = new CNumber(source.unit());
        result.m_unitName = source.unitName();
    };
    const auto unitTextContainsExplicitAngleFactor = [](const QString& unitText) {
        if (unitText.isEmpty())
            return false;
        QString token;
        auto flushTokenIsAngle = [&]() {
            if (token.isEmpty())
                return false;
            QString base = token.trimmed();
            token.clear();
            while (!base.isEmpty()
                   && (UnicodeChars::isSuperscriptDigit(base.at(base.size() - 1))
                       || UnicodeChars::isSuperscriptSign(base.at(base.size() - 1))))
            {
                base.chop(1);
            }
            return !base.isEmpty() && Units::isExplicitAngleUnitName(base);
        };
        for (int i = 0; i < unitText.size(); ++i) {
            const QChar ch = unitText.at(i);
            const bool isUnitChar =
                UnicodeChars::isUnitIdentifierChar(ch)
                || UnicodeChars::isSuperscriptDigit(ch)
                || UnicodeChars::isSuperscriptSign(ch);
            if (isUnitChar) {
                token += ch;
                continue;
            }
            if (flushTokenIsAngle())
                return true;
        }
        return flushTokenIsAngle();
    };
    const auto firstExplicitAngleFactorToken = [](const QString& unitText) -> QString {
        if (unitText.isEmpty())
            return QString();
        QString token;
        const QString degreeName = ::unitName(UnitId::Degree);
        const QString degreeAlias = Units::degreeAliasSymbol();
        auto normalizeAngleToken = [&](const QString& rawToken) -> QString {
            QString normalized = rawToken.trimmed();
            if (normalized.isEmpty())
                return normalized;

            int suffixStart = normalized.size();
            while (suffixStart > 0) {
                const QChar ch = normalized.at(suffixStart - 1);
                if (!UnicodeChars::isSuperscriptDigit(ch)
                    && !UnicodeChars::isSuperscriptSign(ch))
                {
                    break;
                }
                --suffixStart;
            }
            QString base = normalized.left(suffixStart).trimmed();
            const QString exponentSuffix = normalized.mid(suffixStart);
            while (!base.isEmpty()
                   && (UnicodeChars::isSuperscriptDigit(base.at(base.size() - 1))
                       || UnicodeChars::isSuperscriptSign(base.at(base.size() - 1))))
            {
                base.chop(1);
            }
            if (!Units::isExplicitAngleUnitName(base))
                return QString();

            if (base.startsWith(UnicodeChars::DegreeSign))
                return degreeAlias + base.mid(1) + exponentSuffix;
            if (base.startsWith(degreeName)) {
                const int n = degreeName.size();
                if (base.size() == n || !base.at(n).isLetter())
                    return degreeAlias + base.mid(n) + exponentSuffix;
            }
            if (base.startsWith(degreeAlias))
                return base + exponentSuffix;
            return base + exponentSuffix;
        };
        auto flushToken = [&]() {
            if (token.isEmpty())
                return QString();
            QString raw = token;
            token.clear();
            return normalizeAngleToken(raw);
        };
        for (int i = 0; i < unitText.size(); ++i) {
            const QChar ch = unitText.at(i);
            const bool isUnitChar =
                UnicodeChars::isUnitIdentifierChar(ch)
                || UnicodeChars::isSuperscriptDigit(ch)
                || UnicodeChars::isSuperscriptSign(ch);
            if (isUnitChar) {
                token += ch;
                continue;
            }
            const QString factor = flushToken();
            if (!factor.isEmpty())
                return factor;
        }
        return flushToken();
    };
    if (!other.isDimensionless()) {
        result.stripUnits();
        auto i = other.m_dimension.constBegin();
        while (i != other.m_dimension.constEnd()) {
            const auto& exp = i.value();
            const auto& name = i.key();
            if (!result.m_dimension.contains(name))
                result.m_dimension[name] = Rational(0);
            result.m_dimension[name] -= exp;
            ++i;
        }
        result.cleanDimension();

        const QString n1 = normalizeUnitName(this->unitName());
        const UnitId u1 = unitId(n1);
        // Keep user-intended energy-per-geometry forms in their original domain
        // unless conversion is explicitly requested, to avoid context loss:
        // J/m² (energy/area) vs N/m (force/length), and
        // J/m³ (energy density) vs Pa (pressure).
        if (((this->hasUnit() && u1 == UnitId::Joule)
             || isExactDimension(*this, dimensionJoule()))
            && isExactDimension(other, dimensionSquareMetre()))
        {
            result.setDisplayUnit(this->unit() / other.unit(),
                                  QString(unitPhrase(UnitPhraseId::JoulePerSquareMetre)));
            return result;
        }
        if (((this->hasUnit() && u1 == UnitId::Joule)
             || isExactDimension(*this, dimensionJoule()))
            && isExactDimension(other, dimensionCubicMetre()))
        {
            result.setDisplayUnit(this->unit() / other.unit(),
                                  QString(unitPhrase(UnitPhraseId::JoulePerCubicMetre)));
            return result;
        }

        if (isExactDimension(*this, dimensionJoule())
            && isExactDimension(other, dimensionSecond()))
        {
            result.setDisplayUnit(this->unit() / other.unit(), QString(::unitName(UnitId::Watt)));
            return result;
        }

        // Preserve explicit angle-unit intent for angular-rate expressions,
        // even when the denominator unit has no own display token (e.g. minute).
        if (this->hasUnit()
            && Units::isExplicitAngleUnitName(this->unitName())
            && isPureTimeQuantity(other))
        {
            result.setDisplayUnit(this->unit() / other.unit(),
                                  composeAnglePerSecondUnitName(this->unitName()));
            return result;
        }

        if (this->hasUnit() && other.hasUnit() && !result.isDimensionless()) {
            Quantity canonical(result);
            canonical.cleanDimension();
            Units::findUnit(canonical);
            const bool hasExplicitAngleFactorOperand =
                unitTextContainsExplicitAngleFactor(this->unitName())
                || unitTextContainsExplicitAngleFactor(other.unitName());
            if (hasExplicitAngleFactorOperand) {
                QString angleFactorToken =
                    firstExplicitAngleFactorToken(this->unitName());
                if (angleFactorToken.isEmpty())
                    angleFactorToken = firstExplicitAngleFactorToken(other.unitName());

                QString canonicalName = canonical.unitName();
                QString displayName;
                if (canonicalName.isEmpty()) {
                    displayName = angleFactorToken;
                } else if (unitTextContainsExplicitAngleFactor(canonicalName)) {
                    displayName = canonicalName;
                } else if (!angleFactorToken.isEmpty()) {
                    displayName = composeProductUnitName(angleFactorToken, canonicalName);
                } else {
                    displayName = canonicalName;
                }

                if (!displayName.isEmpty()) {
                    const CNumber displayUnitValue =
                        canonical.hasUnit()
                        ? canonical.unit()
                        : (this->unit() / other.unit());
                    result.setDisplayUnit(displayUnitValue, displayName);
                    return result;
                }
            }
            const UnitId canonicalId =
                unitId(normalizeUnitName(canonical.unitName()));
            if (isPreferredCanonicalDisplayUnit(canonicalId))
            {
                result.setDisplayUnit(canonical.unit(), canonical.unitName());
            } else {
                result.setDisplayUnit(this->unit() / other.unit(),
                                      composeQuotientUnitName(this->unitName(), other.unitName()));
            }
            return result;
        }

        if (!result.hasUnit()
            && !result.isDimensionless()
            && this->hasUnit()
            && !other.hasUnit()
            && unitTextContainsExplicitAngleFactor(this->unitName()))
        {
            Quantity canonicalResult(result);
            canonicalResult.cleanDimension();
            Units::findUnit(canonicalResult);

            QString angleFactorToken = firstExplicitAngleFactorToken(this->unitName());
            QString canonicalName = canonicalResult.unitName();
            QString displayName;
            if (canonicalName.isEmpty()) {
                displayName = angleFactorToken;
            } else if (unitTextContainsExplicitAngleFactor(canonicalName)) {
                displayName = canonicalName;
            } else if (!angleFactorToken.isEmpty()) {
                displayName = composeProductUnitName(angleFactorToken, canonicalName);
            } else {
                displayName = canonicalName;
            }

            if (!displayName.isEmpty()) {
                const CNumber displayUnitValue =
                    canonicalResult.hasUnit()
                    ? canonicalResult.unit()
                    : (this->unit() / other.unit());
                result.setDisplayUnit(displayUnitValue, displayName);
                return result;
            }
        }
    } else if (this->hasUnit()) {
        const bool divisorHasExplicitAngleUnit =
            other.hasUnit() && Units::isExplicitAngleUnitName(other.unitName());
        if (divisorHasExplicitAngleUnit) {
            result.setDisplayUnit(this->unit() / other.unit(),
                                  composeQuotientUnitName(this->unitName(), other.unitName()));
        } else {
            copyDisplayUnit(*this);
        }
    } else if (other.hasUnit()
               && Units::isExplicitAngleUnitName(other.unitName())
               && !result.isDimensionless())
    {
        Quantity canonicalNumerator(*this);
        canonicalNumerator.cleanDimension();
        Units::findUnit(canonicalNumerator);
        if (canonicalNumerator.hasUnit()) {
            result.setDisplayUnit(canonicalNumerator.unit() / other.unit(),
                                  composeQuotientUnitName(canonicalNumerator.unitName(),
                                                          other.unitName()));
        }
    }

    if (result.hasUnit()) {
        const QString simplified = simplifyExplicitAngleCompositeDisplayUnits(result.unitName());
        if (simplified != result.unitName())
            result.setDisplayUnit(result.unit(), simplified);
    }

    return result;
}

Quantity Quantity::operator/(const HNumber& other) const
{
    return operator/(CNumber(other));
}

Quantity Quantity::operator/(const CNumber& other) const
{
    Quantity result(*this);
    result.m_numericValue /= other;
    result.cleanDimension();
    return result;
}

Quantity &Quantity::operator/=(const Quantity& other)
{
    return operator=(*this/other);
}

Quantity Quantity::operator%(const Quantity& other) const
{
    Quantity result(*this);
    result.m_numericValue = result.m_numericValue % other.m_numericValue;
    return result;
}

Quantity Quantity::operator&(const Quantity& other) const
{
    ENSURE_DIMENSIONLESS(*this);
    ENSURE_DIMENSIONLESS(other);
    Quantity result(*this);
    result.m_numericValue &= other.m_numericValue;
    return result;
}

Quantity &Quantity::operator&=(const Quantity& other)
{
    return operator=(*this & other);
}

Quantity Quantity::operator|(const Quantity& other) const
{
    ENSURE_DIMENSIONLESS(*this);
    ENSURE_DIMENSIONLESS(other);
    Quantity result(*this);
    result.m_numericValue |= other.m_numericValue;
    return result;
}

Quantity &Quantity::operator|=(const Quantity& other)
{
    return operator=(*this | other);
}

Quantity Quantity::operator^(const Quantity& other) const
{
    ENSURE_DIMENSIONLESS(*this);
    ENSURE_DIMENSIONLESS(other);
    Quantity result(*this);
    result.m_numericValue ^= other.m_numericValue;
    return result;
}

Quantity &Quantity::operator^=(const Quantity& other)
{
    return operator=(*this ^ other);
}

Quantity Quantity::operator~() const
{
    ENSURE_DIMENSIONLESS(*this);
    Quantity result(*this);
    result.m_numericValue= ~result.m_numericValue;
    return result;
}

Quantity Quantity::operator>>(const Quantity& other) const
{
    ENSURE_DIMENSIONLESS(*this);
    ENSURE_DIMENSIONLESS(other);
    Quantity result(*this);
    result.m_numericValue = result.m_numericValue >> other.m_numericValue;
    return result;
}

Quantity Quantity::operator<<(const Quantity& other) const
{
    ENSURE_DIMENSIONLESS(*this);
    ENSURE_DIMENSIONLESS(other);
    Quantity result(*this);
    result.m_numericValue = result.m_numericValue << other.m_numericValue;
    return result;
}

Quantity::Format::Format()
    : CNumber::Format()
{
}

Quantity::Format::Format(const CNumber::Format& other)
    : CNumber::Format(other)
{
}

Quantity::Format::Format(const HNumber::Format& other)
    : CNumber::Format(other)
{
}

Quantity::Format Quantity::Format::operator+(const Quantity::Format& other) const
{
    return Quantity::Format(CNumber::Format::operator+(static_cast<const CNumber::Format&>(other)));
}

void Quantity::Format::serialize(QJsonObject& json) const
{
    switch (mode) {
    case Mode::General:
        json["mode"] = QStringLiteral("General");
        break;
    case Mode::Fixed:
        json["mode"] = QStringLiteral("Fixed");
        break;
    case Mode::Scientific:
        json["mode"] = QStringLiteral("Scientific");
        break;
    case Mode::Engineering:
        json["mode"] = QStringLiteral("Engineering");
        break;
    case Mode::Sexagesimal:
        json["mode"] = QStringLiteral("Sexagesimal");
        break;
    case Mode::Null:
        break;
    }

    switch (base) {
    case Base::Binary:
        json["base"] = QStringLiteral("Binary");
        break;
    case Base::Octal:
        json["base"] = QStringLiteral("Octal");
        break;
    case Base::Hexadecimal:
        json["base"] = QStringLiteral("Hexadecimal");
        break;
    case Base::Decimal:
        json["base"] = QStringLiteral("Decimal");
        break;
    case Base::Null:
        break;
    }

    switch (notation) {
    case Notation::Cartesian:
        json["form"] = QStringLiteral("Cartesian");
        break;
    case Notation::Polar:
        json["form"] = QStringLiteral("Polar");
        break;
    case Notation::PolarAngle:
        json["form"] = QStringLiteral("PolarAngle");
        break;
    case Notation::Null:
    default:
        break;
    }

    if (precision != PrecisionNull)
        json["precision"] = precision;
    if (paddedBits != 0)
        json["padded_bits"] = paddedBits;
    if (forcedExponent != ForcedExponentNull)
        json["forced_exponent"] = forcedExponent;
}

Quantity::Format Quantity::Format::deSerialize(const QJsonObject& json)
{
    Format result;
    if (json.contains("mode")) {
        auto strMode = json["mode"].toString();
        if (strMode == "General")
            result.mode = Mode::General;
        else if (strMode == "Fixed")
            result.mode = Mode::Fixed;
        else if (strMode == "Scientific")
            result.mode = Mode::Scientific;
        else if (strMode == "Engineering")
            result.mode = Mode::Engineering;
        else if (strMode == "Sexagesimal")
            result.mode = Mode::Sexagesimal;
        else
            result.mode = Mode::Null;
    } else
        result.mode = Mode::Null;

    if (json.contains("base")) {
        auto strBase = json["base"].toString();
        if (strBase == "Binary")
            result.base = Base::Binary;
        else if (strBase == "Octal")
            result.base = Base::Octal;
        else if (strBase == "Decimal")
            result.base = Base::Decimal;
        else if (strBase == "Hexadecimal")
            result.base = Base::Hexadecimal;
        else
            result.base = Base::Null;
    } else
        result.base = Base::Null;

    if (json.contains("form")) {
        auto strNotation = json["form"].toString();
        if (strNotation == "Cartesian")
            result.notation = Notation::Cartesian;
        else if (strNotation == "Polar")
            result.notation = Notation::Polar;
        else if (strNotation == "PolarAngle")
            result.notation = Notation::PolarAngle;
        else
            result.notation = Notation::Null;
    } else
        result.notation = Notation::Null;


    result.precision = json.contains("precision") ? json["precision"].toInt() : PrecisionNull;
    result.paddedBits = json.contains("padded_bits") ? json["padded_bits"].toInt() : 0;
    result.forcedExponent = json.contains("forced_exponent")
        ? json["forced_exponent"].toInt()
        : ForcedExponentNull;
    return result;
}

bool Quantity::Format::isNull() const
{
    return (mode == Mode::Null
        && base == Base::Null
        && precision == PrecisionNull
        && notation == Notation::Null
        && paddedBits == 0
        && forcedExponent == ForcedExponentNull);
}

// DMath
// =====

bool DMath::complexMode = true;

#define COMPLEX_WRAP_1(fct, arg) \
    (DMath::complexMode ? CMath::fct(arg) : CNumber(HMath::fct(arg.real)))

#define COMPLEX_WRAP_2(fct, arg1, arg2) \
    (DMath::complexMode ? CMath::fct(arg1, arg2) : CNumber(HMath::fct(arg1.real, arg2.real)))

#define COMPLEX_WRAP_3(fct, arg1, arg2, arg3) \
    (DMath::complexMode ? CMath::fct(arg1, arg2, arg3) : CNumber(HMath::fct(arg1.real, arg2.real, arg3.real)))

#define COMPLEX_WRAP_4(fct, arg1, arg2, arg3, arg4) \
    (DMath::complexMode ? CMath::fct(arg1, arg2, arg3, arg4) \
    : CNumber(HMath::fct(arg1.real, arg2.real, arg3.real, arg4.real)))

//  Wrappers for functions that are only defined for dimensionless arguments

// Mo argument.
#define WRAPPER_DMATH_0(fct) \
    Quantity DMath::fct() \
    { \
        return Quantity(CMath::fct()); \
    } \

// One argument.
#define WRAPPER_DMATH_1(fct) \
    Quantity DMath::fct(const Quantity& arg1) \
    { \
        ENSURE_DIMENSIONLESS(arg1); \
        return Quantity(COMPLEX_WRAP_1(fct, arg1.m_numericValue)); \
    }

// Two arguments.
#define WRAPPER_DMATH_2(fct) \
    Quantity DMath::fct(const Quantity& arg1, const Quantity& arg2) \
    { \
        ENSURE_DIMENSIONLESS(arg1); \
        ENSURE_DIMENSIONLESS(arg2); \
        return Quantity(COMPLEX_WRAP_2(fct, arg1.m_numericValue, arg2.m_numericValue)); \
    }

// Three arguments.
#define WRAPPER_DMATH_3(fct) \
    Quantity DMath::fct(const Quantity& arg1, const Quantity& arg2, const Quantity& arg3) \
    { \
        ENSURE_DIMENSIONLESS(arg1); \
        ENSURE_DIMENSIONLESS(arg2); \
        ENSURE_DIMENSIONLESS(arg3); \
        return Quantity(COMPLEX_WRAP_3(fct, arg1.m_numericValue, arg2.m_numericValue, arg3.m_numericValue)); \
    }

// Four arguments.
#define WRAPPER_DMATH_4(fct) \
    Quantity DMath::fct(const Quantity& arg1, const Quantity& arg2, const Quantity& arg3, const Quantity& arg4) \
    { \
        ENSURE_DIMENSIONLESS(arg1); \
        ENSURE_DIMENSIONLESS(arg2); \
        ENSURE_DIMENSIONLESS(arg3); \
        ENSURE_DIMENSIONLESS(arg4); \
        return Quantity(COMPLEX_WRAP_4(fct, arg1.m_numericValue, arg2.m_numericValue, arg3.m_numericValue, \
                                       arg4.m_numericValue)); \
    }

WRAPPER_DMATH_0(e)
WRAPPER_DMATH_0(pi)
WRAPPER_DMATH_0(phi)
WRAPPER_DMATH_0(i)

Quantity DMath::nan(Error error)
{
    return Quantity(CMath::nan(error));
}

WRAPPER_DMATH_1(rad2deg)
WRAPPER_DMATH_1(deg2rad)
WRAPPER_DMATH_1(rad2gon)
WRAPPER_DMATH_1(gon2rad)
WRAPPER_DMATH_1(integer)
WRAPPER_DMATH_1(frac)
WRAPPER_DMATH_1(floor)
WRAPPER_DMATH_1(ceil)
WRAPPER_DMATH_1(exp)
WRAPPER_DMATH_1(ln)
WRAPPER_DMATH_1(lg)
WRAPPER_DMATH_1(lb)
WRAPPER_DMATH_2(log)
WRAPPER_DMATH_1(sinh)
WRAPPER_DMATH_1(cosh)
WRAPPER_DMATH_1(tanh)
WRAPPER_DMATH_1(arsinh)
WRAPPER_DMATH_1(arcosh)
WRAPPER_DMATH_1(artanh)
WRAPPER_DMATH_1(sin)
WRAPPER_DMATH_1(cos)
WRAPPER_DMATH_1(tan)
WRAPPER_DMATH_1(cot)
WRAPPER_DMATH_1(sec)
WRAPPER_DMATH_1(csc)
WRAPPER_DMATH_1(arcsin)
WRAPPER_DMATH_1(arccos)
WRAPPER_DMATH_1(arctan)
WRAPPER_DMATH_2(arctan2)

WRAPPER_DMATH_2(factorial)
WRAPPER_DMATH_1(gamma)
WRAPPER_DMATH_1(lnGamma)
WRAPPER_DMATH_1(erf)
WRAPPER_DMATH_1(erfc)

WRAPPER_DMATH_2(gcd)
WRAPPER_DMATH_2(lcm)
WRAPPER_DMATH_2(idiv)

Quantity DMath::round(const Quantity& n, int prec)
{
    ENSURE_DIMENSIONLESS(n);
    return DMath::complexMode ?
        CMath::round(n.numericValue(), prec) :
        CNumber(HMath::round(n.numericValue().real, prec));
}

Quantity DMath::trunc(const Quantity& n, int prec)
{
    ENSURE_DIMENSIONLESS(n);
    return DMath::complexMode ?
        CMath::trunc(n.numericValue(), prec)
        : CNumber(HMath::trunc(n.numericValue().real, prec));
}

WRAPPER_DMATH_2(nCr)
WRAPPER_DMATH_2(nPr)
WRAPPER_DMATH_3(binomialPmf)
WRAPPER_DMATH_3(binomialCdf)
WRAPPER_DMATH_2(binomialMean)
WRAPPER_DMATH_2(binomialVariance)
WRAPPER_DMATH_4(hypergeometricPmf)
WRAPPER_DMATH_4(hypergeometricCdf)
WRAPPER_DMATH_3(hypergeometricMean)
WRAPPER_DMATH_3(hypergeometricVariance)
WRAPPER_DMATH_2(poissonPmf)
WRAPPER_DMATH_2(poissonCdf)
WRAPPER_DMATH_1(poissonMean)
WRAPPER_DMATH_1(poissonVariance)

WRAPPER_DMATH_2(mask)
WRAPPER_DMATH_2(sgnext)
WRAPPER_DMATH_2(ashr)


WRAPPER_DMATH_3(decodeIeee754)
WRAPPER_DMATH_4(decodeIeee754)


QString DMath::format(Quantity q, Quantity::Format format)
{
    format = q.format() + format;  // Left hand side operator takes priority.

    // Handle units.
    if (!q.hasUnit() && !q.isDimensionless()) {
        q.cleanDimension();
        Units::findUnit(q);
    }
    const QString normalizedUnitName = normalizeDisplayUnitNameForOutput(q.unitName());
    const bool hasUnitSuffix = !normalizedUnitName.isEmpty();
    const bool compactDegreeSuffix = normalizedUnitName == QString(UnicodeChars::DegreeSign);
    const QString unit_name = hasUnitSuffix
        ? (compactDegreeSuffix ? normalizedUnitName : (QStringLiteral(" ") + normalizedUnitName))
        : QString();
    CNumber unit = q.unit();
    CNumber number = q.m_numericValue;

    number /= unit;
    if (number.isNearReal()) {
        HNumber affineValue;
        if (Units::tryConvertAffineFromBase(q.unitName(), number.real, &affineValue))
            number.real = affineValue;
    }

    QString result = CMath::format(number, format);

    if (!number.real.isZero() && !number.imag.isZero() && hasUnitSuffix)
        result = "(" + result + ")";

    if (hasUnitSuffix)
        result.append(unit_name);

    return result;
}

Quantity DMath::real(const Quantity& x)
{
    Quantity result(x);
    result.m_numericValue = result.m_numericValue.real;
    return result;
}

Quantity DMath::imag(const Quantity& x)
{
    Quantity result(x);
    result.m_numericValue = result.m_numericValue.imag;
    return result;
}

Quantity DMath::conj(const Quantity& n)
{
    Quantity result = Quantity(n);
    // If in Real mode, just strip the imaginary part.
    result.m_numericValue = complexMode ?
        CMath::conj(result.m_numericValue)
        : CMath::real(result.m_numericValue);

    return result;
}

Quantity DMath::abs(const Quantity& n)
{
    Quantity result(n);
    result.m_numericValue = COMPLEX_WRAP_1(abs, n.m_numericValue);
    return result;
}

Quantity DMath::phase(const Quantity& n)
{
    return CMath::phase(n.numericValue());
}

Quantity DMath::sqrt(const Quantity& n)
{
    Quantity result(COMPLEX_WRAP_1(sqrt, n.m_numericValue));
    auto i = n.m_dimension.constBegin();
    while (i != n.m_dimension.constEnd()) {
        auto& exp = i.value();
        auto& name = i.key();
        result.modifyDimension(name, exp * Rational(1,2));
        ++i;
    }
    return result;
}

Quantity DMath::cbrt(const Quantity& n)
{
    Quantity result(COMPLEX_WRAP_1(cbrt, n.m_numericValue));
    auto i = n.m_dimension.constBegin();
    while (i != n.m_dimension.constEnd()) {
        auto& exp = i.value();
        auto& name = i.key();
        result.modifyDimension(name, exp * Rational(1,3));
        ++i;
    }
    return result;
}

Quantity DMath::raise(const Quantity& n1, int n)
{
    Quantity result;
    result.m_numericValue = complexMode ?
        CMath::raise(n1.m_numericValue, n)
        : CNumber(HMath::raise(n1.m_numericValue.real, n));
    auto i = n1.m_dimension.constBegin();
    while (i != n1.m_dimension.constEnd()) {
        auto& exp = i.value();
        auto& name = i.key();
        result.modifyDimension(name, exp * n);
        ++i;
    }
    if (n1.hasUnit() && n != 0)
    {
        result.setDisplayUnit(CMath::raise(n1.unit(), n),
                              formatUnitFactorWithExponent(n1.unitName(), n));
    }
    return result;
}

Quantity DMath::raise(const Quantity& n1, const Quantity& n2)
{
    if (!n2.isDimensionless() || (!n1.isDimensionless() && !n2.isReal() && complexMode))
        return DMath::nan(InvalidDimension);

    // First get the new numeric value.
    Quantity result(COMPLEX_WRAP_2(raise, n1.m_numericValue, n2.m_numericValue));

    if (n1.isDimensionless()) {
        Rational exponent(n2.m_numericValue.real);
        if (n1.hasUnit()
            && abs(exponent.toHNumber() - n2.m_numericValue.real) < RATIONAL_TOL
            && exponent.denominator() == 1
            && exponent.numerator() != 0)
        {
            const int integerExponent = exponent.numerator();
            result.setDisplayUnit(CMath::raise(n1.unit(), integerExponent),
                                  formatUnitFactorWithExponent(n1.unitName(), integerExponent));
        }
        return result;
    }

    // We can now assume that n1 has a dimension, but n2 is real.
    // Compute the new dimension: try to convert n2 to a Rational. If n2 is not
    // rational, return NaN.

    // For negative bases only allow odd denominators.
    Rational exponent(n2.m_numericValue.real);
    if (abs(exponent.toHNumber() - n2.m_numericValue.real) >= RATIONAL_TOL
       || (n1.isNegative() && exponent.denominator() % 2 == 0))
        return DMath::nan(OutOfDomain);

    // Compute new dimension.
    auto i = n1.m_dimension.constBegin();
    while (i != n1.m_dimension.constEnd()) {
        result.modifyDimension(i.key(), i.value()*exponent);
        ++i;
    }
    return result;
}

Quantity DMath::sgn(const Quantity& x)
{
    return Quantity(CMath::sgn(x.m_numericValue));
}

Quantity DMath::encodeIeee754(const Quantity& val, const Quantity& exp_bits, const Quantity& significand_bits)
{
    ENSURE_DIMENSIONLESS(val);
    ENSURE_DIMENSIONLESS(exp_bits);
    ENSURE_DIMENSIONLESS(significand_bits);

    Quantity result(CMath::encodeIeee754(val.numericValue(), exp_bits.numericValue(), significand_bits.numericValue()));
    result.m_format = result.m_format + Quantity::Format::Fixed() + Quantity::Format::Hexadecimal();
    return result;
}

Quantity DMath::encodeIeee754(const Quantity& val, const Quantity& exp_bits, const Quantity& significand_bits,
                              const Quantity& exp_bias)
{
    ENSURE_DIMENSIONLESS(val);
    ENSURE_DIMENSIONLESS(exp_bits);
    ENSURE_DIMENSIONLESS(significand_bits);
    ENSURE_DIMENSIONLESS(exp_bias);

    Quantity result(CMath::encodeIeee754(val.numericValue(), exp_bits.numericValue(), significand_bits.numericValue(),
                                         exp_bias.numericValue()));
    result.m_format = result.m_format + Quantity::Format::Fixed() + Quantity::Format::Hexadecimal();
    return result;
}
