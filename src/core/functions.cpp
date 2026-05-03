// SPDX-FileCopyrightText: 2006-2011, 2013-2019, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/functions.h"

#include "core/mathdsl.h"
#include "core/settings.h"
#include "core/units.h"
#include "core/unicodechars.h"
#include "math/hmath.h"
#include "math/cmath.h"
#include "math/floatnum/floatconfig.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHash>
#include <QTimeZone>

#include <algorithm>
#include <functional>
#include <cfloat>
#include <cmath>
#include <ctime>
#include <limits>
#include <numeric>
#include <random>
#include <string>

static QString domainToDisplay(FunctionDomain domain)
{
    switch (domain) {
    case FunctionDomain::Arithmetic: return FunctionRepo::tr("Arithmetic");
    case FunctionDomain::Chemistry: return FunctionRepo::tr("Chemistry");
    case FunctionDomain::Complex: return FunctionRepo::tr("Complex");
    case FunctionDomain::Combinatorics: return FunctionRepo::tr("Combinatorics");
    case FunctionDomain::Probability: return FunctionRepo::tr("Probability");
    case FunctionDomain::Statistics: return FunctionRepo::tr("Statistics");
    case FunctionDomain::Aggregation: return FunctionRepo::tr("Aggregation");
    case FunctionDomain::Random: return FunctionRepo::tr("Random");
    case FunctionDomain::BaseConversion: return FunctionRepo::tr("Base conversion");
    case FunctionDomain::NumberFormatting: return FunctionRepo::tr("Number formatting");
    case FunctionDomain::IntegerArithmetic: return FunctionRepo::tr("Integer arithmetic");
    case FunctionDomain::SpecialFunctions: return FunctionRepo::tr("Special functions");
    case FunctionDomain::ExponentialLogarithmic: return FunctionRepo::tr("Exponential & Logarithmic");
    case FunctionDomain::AngleConversion: return FunctionRepo::tr("Angle conversion");
    case FunctionDomain::Trigonometry: return FunctionRepo::tr("Trigonometry");
    case FunctionDomain::Bitwise: return FunctionRepo::tr("Bitwise");
    case FunctionDomain::FloatingPoint: return FunctionRepo::tr("Floating point");
    case FunctionDomain::DateTime: return FunctionRepo::tr("Date & Time");
    case FunctionDomain::LinearAlgebra: return FunctionRepo::tr("Linear algebra");
    }
    return QString();
}

#define FUNCTION_INSERT(DOMAIN, ID) insert(new Function(#ID, function_ ## ID, DOMAIN, this))
#define FUNCTION_USAGE(ID, USAGE) find(#ID)->setUsage(QString::fromLatin1(USAGE));
#define FUNCTION_USAGE_TR(ID, USAGE) find(#ID)->setUsage(USAGE);
#define FUNCTION_NAME(ID, NAME) find(#ID)->setName(NAME)

enum class StatisticalNormalization {
    Population,
    Sample
};

#define ENSURE_MINIMUM_ARGUMENT_COUNT(i) \
    if (args.count() < i) { \
        f->setError(InvalidParamCount); \
        return CMath::nan(InvalidParamCount); \
    }

#define ENSURE_ARGUMENT_COUNT(i) \
    if (args.count() != (i)) { \
        f->setError(InvalidParamCount); \
        return CMath::nan(InvalidParamCount); \
    }

#define ENSURE_EITHER_ARGUMENT_COUNT(i, j) \
    if (args.count() != (i) && args.count() != (j)) { \
        f->setError(InvalidParamCount); \
        return CMath::nan(InvalidParamCount); \
    }

#define ENSURE_SAME_DIMENSION() \
    for(int i=0; i<args.count()-1; ++i) { \
        if(!args.at(i).sameDimension(args.at((i)+1))) \
            return DMath::nan(InvalidDimension);\
    }

#define ENSURE_REAL_ARGUMENT(i) \
    if (!args[i].isReal()) { \
        f->setError(OutOfDomain); \
        return CMath::nan(); \
    }

#define ENSURE_REAL_ARGUMENTS() \
    for (int i = 0; i < args.count(); i++) { \
        ENSURE_REAL_ARGUMENT(i); \
    }

#define CONVERT_ARGUMENT_ANGLE(angle) \
    { \
    const bool convertedExplicitAngleUnit = Units::tryConvertExplicitAngleToRadians(&(angle)); \
    if (!convertedExplicitAngleUnit && Settings::instance()->angleUnit == 'd') { \
        if (angle.isReal()) \
            angle = DMath::deg2rad(angle); \
        else { \
            f->setError(OutOfDomain); \
            return DMath::nan(); \
        } \
    } \
    else if (!convertedExplicitAngleUnit && Settings::instance()->angleUnit == 'g') { \
        if (angle.isReal()) \
            angle = DMath::gon2rad(angle); \
        else { \
            f->setError(OutOfDomain); \
            return DMath::nan(); \
        } \
    } \
    else if (!convertedExplicitAngleUnit && (Settings::instance()->angleUnit == 't' \
            || Settings::instance()->angleUnit == 'v')) { \
        if (angle.isReal()) \
            angle *= Quantity(2) * DMath::pi(); \
        else { \
            f->setError(OutOfDomain); \
            return DMath::nan(); \
        } \
    } \
    }

#define CONVERT_RESULT_ANGLE(result) \
    if (Settings::instance()->angleUnit == 'd') \
        result = DMath::rad2deg(result); \
    else if (Settings::instance()->angleUnit == 'g') \
        result = DMath::rad2gon(result); \
    else if (Settings::instance()->angleUnit == 't' \
            || Settings::instance()->angleUnit == 'v') \
        result /= Quantity(2) * DMath::pi();

static FunctionRepo* s_FunctionRepoInstance = 0;
static const int s_defaultRandomDigits = 16;
// Internal sentinel used to request per-expression rational display in NumberFormatter.
// This piggybacks on precision because Quantity::Format has no dedicated rational-override flag.
static const int s_forcedRationalPrecision = -999;

// FIXME: destructor seems not to be called
static void s_deleteFunctions()
{
    delete s_FunctionRepoInstance;
}

// Shared pseudo-random engine for random built-ins.
static std::mt19937_64& s_randomEngine()
{
    static std::mt19937_64 engine([] {
        const auto now = static_cast<unsigned long long>(QDateTime::currentMSecsSinceEpoch());
        std::seed_seq seed{
            static_cast<unsigned int>(std::random_device{}()),
            static_cast<unsigned int>(std::random_device{}()),
            static_cast<unsigned int>(now & 0xFFFFFFFFULL),
            static_cast<unsigned int>((now >> 32) & 0xFFFFFFFFULL)
        };
        return std::mt19937_64(seed);
    }());
    return engine;
}

static Quantity s_randomFraction(int digits)
{
    std::uniform_int_distribution<int> digitDistribution(0, 9);
    std::string literal = "0.";
    literal.reserve(2 + std::max(0, digits));
    for (int i = 0; i < digits; ++i) {
        literal.push_back(static_cast<char>('0' + digitDistribution(s_randomEngine())));
    }
    return Quantity(HNumber(literal.c_str()));
}

static Quantity s_nonDecimalPad(Function* f, const Function::ArgumentList& args, Quantity::Format format)
{
    ENSURE_EITHER_ARGUMENT_COUNT(1, 2);
    const Quantity value = args.at(0);
    if (!value.isInteger()) {
        f->setError(OutOfDomain);
        return DMath::nan();
    }

    Quantity::Format padding = Quantity::Format::PadToByteBoundary();
    if (args.count() == 2) {
        const Quantity bitsArg = args.at(1);
        if (!bitsArg.isInteger()) {
            f->setError(OutOfDomain);
            return DMath::nan();
        }

        const int bits = bitsArg.numericValue().toInt();
        if (bits <= 0) {
            f->setError(OutOfDomain);
            return DMath::nan();
        }
        padding = Quantity::Format::PadBits(bits);
    }

    return Quantity(value).setFormat(format + padding + value.format());
}

static bool s_tryExtractForcedExponent(const Quantity& value, int* out)
{
    if (!value.isReal() || !value.isDimensionless())
        return false;

    const HNumber numeric = value.numericValue().real;
    if (numeric.isZero()) {
        *out = 0;
        return true;
    }

    if (numeric.isPositive()) {
        const QString scientific = HMath::format(
            numeric,
            HNumber::Format::Scientific()
            + HNumber::Format::Decimal()
            + HNumber::Format::Precision(-1));
        const int exponentPos = scientific.indexOf(QLatin1Char('e'));
        if (exponentPos > 0 && scientific.left(exponentPos) == QStringLiteral("1")) {
            bool ok = false;
            const int prefixExponent = scientific.mid(exponentPos + 1).toInt(&ok);
            if (ok && prefixExponent % 3 == 0) {
                *out = prefixExponent;
                return true;
            }
        }
    }

    if (!value.isInteger())
        return false;

    const HNumber intMax(std::numeric_limits<int>::max());
    const HNumber intMin(std::numeric_limits<int>::min());
    if (numeric > intMax || numeric < intMin)
        return false;

    const int exponent = numeric.toInt();
    if (exponent % 3 != 0)
        return false;

    *out = exponent;
    return true;
}

static Function::ArgumentList s_collectionElementsOrArgs(const Function::ArgumentList& args)
{
    if (args.count() == 1 && args.at(0).isCollection())
        return args.at(0).elements();
    return args;
}

static bool s_ensureOneCollectionArg(Function* f, const Function::ArgumentList& args, Quantity* collection)
{
    if (args.count() != 1 || !args.at(0).isCollection()) {
        f->setError(OutOfDomain);
        return false;
    }
    *collection = args.at(0);
    return true;
}

static bool s_ensureMatrixArg(Function* f, const Function::ArgumentList& args, Quantity* matrix)
{
    if (!s_ensureOneCollectionArg(f, args, matrix) || !matrix->isMatrix()) {
        f->setError(OutOfDomain);
        return false;
    }
    return true;
}

static Quantity s_makeScalarList(const QVector<Quantity>& elements)
{
    return Quantity::list(elements);
}

static Quantity s_makeMatrix(const QVector<QVector<Quantity>>& rows)
{
    return Quantity::matrix(rows);
}

Quantity Function::exec(const Function::ArgumentList& args)
{
    if (!m_ptr)
        return CMath::nan();
    setError(Success);
    Quantity result = (*m_ptr)(this, args);
    if(result.error())
        setError(result.error());
    return result;
}

Quantity function_abs(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::abs(args.at(0));
}

Quantity function_average(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    const Function::ArgumentList values = s_collectionElementsOrArgs(args);
    if (values.count() < 1 || (!args.at(0).isCollection() && values.count() < 2)) {
        f->setError(InvalidParamCount);
        return CMath::nan(InvalidParamCount);
    }
    return std::accumulate(values.begin()+1, values.end(), *values.begin()) / Quantity(values.count());
}

Quantity function_mean(Function* f, const Function::ArgumentList& args)
{
    return function_average(f, args);
}

Quantity function_median(Function* f, const Function::ArgumentList& args);

Quantity function_list(Function* f, const Function::ArgumentList& args)
{
    ENSURE_MINIMUM_ARGUMENT_COUNT(1);
    Quantity result = Quantity::list(args);
    if (result.isNan()) {
        f->setError(result.error() == DimensionMismatch ? DimensionMismatch : OutOfDomain);
        return result;
    }
    return result;
}

Quantity function_absdev(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    const Function::ArgumentList values = s_collectionElementsOrArgs(args);
    if (values.count() < 1 || (!args.at(0).isCollection() && values.count() < 2)) {
        f->setError(InvalidParamCount);
        return CMath::nan(InvalidParamCount);
    }
    Quantity mean = function_average(f, values);
    if (mean.isNan())
        return mean;   // pass the error along
    Quantity acc = 0;
    for (int i = 0; i < values.count(); ++i)
        acc += DMath::abs(values.at(i) - mean);
    return acc / Quantity(values.count());
}

Quantity function_mad(Function* f, const Function::ArgumentList& args)
{
    const Function::ArgumentList values = s_collectionElementsOrArgs(args);
    if (values.count() < 1 || (!args.at(0).isCollection() && values.count() < 2)) {
        f->setError(InvalidParamCount);
        return CMath::nan(InvalidParamCount);
    }
    const Quantity median = function_median(f, values);
    if (median.isNan())
        return median;
    Function::ArgumentList deviations;
    deviations.reserve(values.count());
    for (const Quantity& value : values)
        deviations.append(DMath::abs(value - median));
    return function_median(f, deviations);
}

Quantity function_int(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::integer(args[0]);
}

Quantity function_trunc(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_EITHER_ARGUMENT_COUNT(1, 2);
    Quantity num = args.at(0);
    if (args.count() == 2) {
        Quantity argprec = args.at(1);
        if (argprec != 0) {
            if (!argprec.isInteger()) {
                f->setError(OutOfDomain);
                return DMath::nan();
            }
            int prec = argprec.numericValue().toInt();
            if (prec)
                return DMath::trunc(num, prec);
            // The second parameter exceeds the integer limits.
            if (argprec < 0)
                return Quantity(0);
            return num;
        }
    }
    return DMath::trunc(num);
}

Quantity function_frac(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::frac(args[0]);
}

Quantity function_floor(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::floor(args[0]);
}

Quantity function_ceil(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::ceil(args[0]);
}

Quantity function_gcd(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_MINIMUM_ARGUMENT_COUNT(2);
    for (int i = 0; i < args.count(); ++i)
        if (!args[i].isInteger()) {
            f->setError(OutOfDomain);
            return DMath::nan();
        }
    return std::accumulate(args.begin() + 1, args.end(), args.at(0), DMath::gcd);
}

Quantity function_lcm(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_MINIMUM_ARGUMENT_COUNT(2);
    for (int i = 0; i < args.count(); ++i)
        if (!args[i].isInteger()) {
            f->setError(OutOfDomain);
            return DMath::nan();
        }
    return std::accumulate(args.begin() + 1, args.end(), args.at(0), DMath::lcm);
}

Quantity function_round(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_EITHER_ARGUMENT_COUNT(1, 2);
    Quantity num = args.at(0);
    int prec = 0;
    if (args.count() == 2) {
        Quantity argprec = args.at(1);
        if (argprec != 0) {
            if (!argprec.isInteger()) {
                f->setError(OutOfDomain);
                return DMath::nan();
            }
            prec = argprec.numericValue().toInt();
            if (prec == 0)
                return num;
            // The second parameter exceeds the integer limits.
            if (argprec < 0)
                return Quantity(0);
        }
    }

    if (!num.isReal()) {
        f->setError(OutOfDomain);
        return DMath::nan();
    }

    const HNumber scale = HMath::raise(HNumber(10), prec);
    const HNumber scaled = num.numericValue().real * scale;
    HNumber rounded;
    switch (runtimeResultRoundingMode()) {
    case Settings::ResultRoundingHalfAwayFromZero: {
        const HNumber absRounded = HMath::floor(HMath::abs(scaled) + HNumber("0.5"));
        rounded = scaled.isNegative() ? -absRounded : absRounded;
        break;
    }
    case Settings::ResultRoundingTowardZero:
        rounded = HMath::trunc(scaled);
        break;
    case Settings::ResultRoundingTowardPositiveInfinity:
        rounded = HMath::ceil(scaled);
        break;
    case Settings::ResultRoundingTowardNegativeInfinity:
        rounded = HMath::floor(scaled);
        break;
    case Settings::ResultRoundingHalfEven:
    default:
        rounded = HMath::round(scaled);
        break;
    }

    Quantity result(num);
    result = Quantity(CNumber(rounded / scale));
    result.copyDimension(num);
    if (num.hasUnit())
        result.setDisplayUnit(num.unit(), num.unitName());
    return result;
}

Quantity function_sqrt(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::sqrt(args[0]);
}

static Quantity s_variance(Function* f,
                           const Function::ArgumentList& args,
                           StatisticalNormalization normalization)
{
    const Function::ArgumentList values = s_collectionElementsOrArgs(args);
    const int divisor = normalization == StatisticalNormalization::Sample
        ? values.count() - 1
        : values.count();
    if (divisor <= 0) {
        f->setError(InvalidParamCount);
        return CMath::nan(InvalidParamCount);
    }

    Quantity mean = function_average(f, values);
    if (mean.isNan())
        return mean;

    Quantity acc(DMath::real(values[0] - mean)*DMath::real(values[0] - mean)
            + DMath::imag(values[0] - mean)*DMath::imag(values[0] - mean));
    for (int i = 1; i < values.count(); ++i) {
        Quantity q(values[i] - mean);
        acc += DMath::real(q)*DMath::real(q) + DMath::imag(q)*DMath::imag(q);
    }

    return acc / Quantity(divisor);
}

Quantity function_varp(Function* f, const Function::ArgumentList& args)
{
    return s_variance(f, args, StatisticalNormalization::Population);
}

Quantity function_vars(Function* f, const Function::ArgumentList& args)
{
    return s_variance(f, args, StatisticalNormalization::Sample);
}

static Quantity s_stdev(Function* f,
                        const Function::ArgumentList& args,
                        StatisticalNormalization normalization)
{
    /* TODO : complex mode switch for this function */
    const Quantity variance = s_variance(f, args, normalization);
    return variance.isNan() ? variance : DMath::sqrt(variance);
}

Quantity function_stdevp(Function* f, const Function::ArgumentList& args)
{
    return s_stdev(f, args, StatisticalNormalization::Population);
}

Quantity function_stdevs(Function* f, const Function::ArgumentList& args)
{
    return s_stdev(f, args, StatisticalNormalization::Sample);
}

Quantity function_cbrt(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::cbrt(args[0]);
}

Quantity function_exp(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::exp(args[0]);
}

Quantity function_ln(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::ln(args[0]);
}

Quantity function_lg(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::lg(args[0]);
}

Quantity function_lb(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::lb(args[0]);
}

Quantity function_log10(Function* f, const Function::ArgumentList& args)
{
    return function_lg(f, args);
}

Quantity function_log2(Function* f, const Function::ArgumentList& args)
{
    return function_lb(f, args);
}

Quantity function_log(Function* f, const Function::ArgumentList& args)
{
     /* TODO : complex mode switch for this function */
     ENSURE_ARGUMENT_COUNT(2);
     return DMath::log(args.at(0), args.at(1));
}

Quantity function_real(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::real(args.at(0));
}

Quantity function_imag(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::imag(args.at(0));
}

Quantity function_conj(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::conj(args.at(0));
}

Quantity function_phase(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    Quantity angle = DMath::phase(args.at(0));
    CONVERT_RESULT_ANGLE(angle);
    return angle;
}


Quantity function_sin(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    Quantity angle = args.at(0);
    CONVERT_ARGUMENT_ANGLE(angle);
    Quantity result = DMath::sin(angle);
    // Trig outputs are always pure scalars; explicit input-angle display units
    // (e.g. °/rad) must not leak into the function result.
    result.stripUnits();
    return result;
}

Quantity function_cos(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    Quantity angle = args.at(0);
    CONVERT_ARGUMENT_ANGLE(angle);
    Quantity result = DMath::cos(angle);
    // Trig outputs are always pure scalars; explicit input-angle display units
    // (e.g. °/rad) must not leak into the function result.
    result.stripUnits();
    return result;
}

Quantity function_tan(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    Quantity angle = args.at(0);
    CONVERT_ARGUMENT_ANGLE(angle);
    Quantity result = DMath::tan(angle);
    // Trig outputs are always pure scalars; explicit input-angle display units
    // (e.g. °/rad) must not leak into the function result.
    result.stripUnits();
    return result;
}

Quantity function_cot(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    Quantity angle = args.at(0);
    CONVERT_ARGUMENT_ANGLE(angle);
    Quantity result = DMath::cot(angle);
    // Trig outputs are always pure scalars; explicit input-angle display units
    // (e.g. °/rad) must not leak into the function result.
    result.stripUnits();
    return result;
}

Quantity function_sec(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    Quantity angle = args.at(0);
    CONVERT_ARGUMENT_ANGLE(angle);
    Quantity result = DMath::sec(angle);
    // Trig outputs are always pure scalars; explicit input-angle display units
    // (e.g. °/rad) must not leak into the function result.
    result.stripUnits();
    return result;
}

Quantity function_csc(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    Quantity angle = args.at(0);
    CONVERT_ARGUMENT_ANGLE(angle);
    Quantity result = DMath::csc(angle);
    // Trig outputs are always pure scalars; explicit input-angle display units
    // (e.g. °/rad) must not leak into the function result.
    result.stripUnits();
    return result;
}

Quantity function_arcsin(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    Quantity result;
    result = DMath::arcsin(args.at(0));
    CONVERT_RESULT_ANGLE(result);
    return result;
}

Quantity function_arccos(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    Quantity result;
    result = DMath::arccos(args.at(0));
    CONVERT_RESULT_ANGLE(result);
    return result;
}

Quantity function_arctan(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    Quantity result;
    result = DMath::arctan(args.at(0));
    CONVERT_RESULT_ANGLE(result);
    return result;
}

Quantity function_arctan2(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(2);
    Quantity result;
    result = DMath::arctan2(args.at(0), args.at(1));
    CONVERT_RESULT_ANGLE(result);
    return result;
}

Quantity function_sinh(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::sinh(args[0]);
}

Quantity function_cosh(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::cosh(args[0]);
}

Quantity function_tanh(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::tanh(args[0]);
}

Quantity function_arsinh(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::arsinh(args[0]);
}

Quantity function_arcosh(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::arcosh(args[0]);
}

Quantity function_artanh(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::artanh(args[0]);
}

Quantity function_erf(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::erf(args[0]);
}

Quantity function_erfc(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::erfc(args[0]);
}

Quantity function_gamma(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::gamma(args[0]);
}

Quantity function_lngamma(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    ENSURE_REAL_ARGUMENT(0);
    return DMath::lnGamma(args[0]);
}

Quantity function_rand(Function* f, const Function::ArgumentList& args)
{
    ENSURE_EITHER_ARGUMENT_COUNT(0, 1);

    int digits = s_defaultRandomDigits;
    if (args.count() == 1) {
        if (args.at(0).isNan() && args.at(0).error() == NoOperand) {
            return s_randomFraction(digits);
        }
        if (!args.at(0).isInteger() || args.at(0) < Quantity(0)) {
            f->setError(OutOfDomain);
            return DMath::nan(OutOfDomain);
        }
        if (args.at(0) > Quantity(DECPRECISION)) {
            f->setError(InvalidPrecision);
            return DMath::nan(InvalidPrecision);
        }
        digits = args.at(0).numericValue().toInt();
    }

    return s_randomFraction(digits);
}

Quantity function_randint(Function* f, const Function::ArgumentList& args)
{
    ENSURE_EITHER_ARGUMENT_COUNT(1, 2);

    for (int i = 0; i < args.count(); ++i) {
        if (!args.at(i).isInteger()) {
            f->setError(OutOfDomain);
            return DMath::nan(OutOfDomain);
        }
    }

    Quantity lower = Quantity(0);
    Quantity upper = args.at(0);
    if (args.count() == 2) {
        lower = std::min(args.at(0), args.at(1));
        upper = std::max(args.at(0), args.at(1));
    } else if (upper < Quantity(0)) {
        lower = upper;
        upper = Quantity(0);
    }

    const Quantity span = (upper - lower) + Quantity(1);
    if (span.isNan()) {
        return span;
    }

    const Quantity offset = DMath::floor(s_randomFraction(DECPRECISION) * span);
    return lower + offset;
}

Quantity function_sgn(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::sgn(args[0]);
}

Quantity function_ncr(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return DMath::nCr(args.at(0), args.at(1));
}

Quantity function_npr(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return DMath::nPr(args.at(0), args.at(1));
}

Quantity function_degrees(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::rad2deg(args[0]);
}

Quantity function_radians(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::deg2rad(args[0]);
}

Quantity function_gradians(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::rad2gon(args[0]);
}

Quantity function_turns(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return args[0] / (Quantity(2) * DMath::pi());
}

Quantity function_max(Function* f, const Function::ArgumentList& args)
{
    const Function::ArgumentList values = s_collectionElementsOrArgs(args);
    if (values.count() < 1 || (!args.at(0).isCollection() && values.count() < 2)) {
        f->setError(InvalidParamCount);
        return CMath::nan(InvalidParamCount);
    }
    for (const Quantity& value : values) {
        if (!value.isReal()) {
            f->setError(OutOfDomain);
            return CMath::nan();
        }
        if (!value.sameDimension(values.at(0)))
            return DMath::nan(InvalidDimension);
    }
    return *std::max_element(values.begin(), values.end());
}

Quantity function_median(Function* f, const Function::ArgumentList& args)
{
    const Function::ArgumentList values = s_collectionElementsOrArgs(args);
    if (values.count() < 1 || (!args.at(0).isCollection() && values.count() < 2)) {
        f->setError(InvalidParamCount);
        return CMath::nan(InvalidParamCount);
    }
    for (const Quantity& value : values) {
        if (!value.isReal()) {
            f->setError(OutOfDomain);
            return CMath::nan();
        }
        if (!value.sameDimension(values.at(0)))
            return DMath::nan(InvalidDimension);
    }

    Function::ArgumentList sortedArgs = values;
    std::sort(sortedArgs.begin(), sortedArgs.end());

    if ((values.count() & 1) == 1)
        return sortedArgs.at((values.count() - 1) / 2);

    const int centerLeft = values.count() / 2 - 1;
    return (sortedArgs.at(centerLeft) + sortedArgs.at(centerLeft + 1)) / Quantity(2);
}

Quantity function_min(Function* f, const Function::ArgumentList& args)
{
    const Function::ArgumentList values = s_collectionElementsOrArgs(args);
    if (values.count() < 1 || (!args.at(0).isCollection() && values.count() < 2)) {
        f->setError(InvalidParamCount);
        return CMath::nan(InvalidParamCount);
    }
    for (const Quantity& value : values) {
        if (!value.isReal()) {
            f->setError(OutOfDomain);
            return CMath::nan();
        }
        if (!value.sameDimension(values.at(0)))
            return DMath::nan(InvalidDimension);
    }
    return *std::min_element(values.begin(), values.end());
}

Quantity function_sum(Function* f, const Function::ArgumentList& args)
{
    const Function::ArgumentList values = s_collectionElementsOrArgs(args);
    if (values.count() < 1 || (!args.at(0).isCollection() && values.count() < 2)) {
        f->setError(InvalidParamCount);
        return CMath::nan(InvalidParamCount);
    }
    return std::accumulate(values.begin(), values.end(), Quantity(0));
}

Quantity function_count(Function* f, const Function::ArgumentList& args)
{
    if (args.count() == 1 && args.at(0).isCollection())
        return Quantity(args.at(0).elementCount());
    ENSURE_MINIMUM_ARGUMENT_COUNT(1);
    return Quantity(args.count());
}

Quantity function_flatten(Function* f, const Function::ArgumentList& args)
{
    Quantity matrix;
    if (!s_ensureMatrixArg(f, args, &matrix))
        return CMath::nan();
    return s_makeScalarList(matrix.elements());
}

Quantity function_rows(Function* f, const Function::ArgumentList& args)
{
    Quantity matrix;
    if (!s_ensureMatrixArg(f, args, &matrix))
        return CMath::nan();
    return Quantity(matrix.rows());
}

Quantity function_cols(Function* f, const Function::ArgumentList& args)
{
    Quantity matrix;
    if (!s_ensureMatrixArg(f, args, &matrix))
        return CMath::nan();
    return Quantity(matrix.columns());
}

Quantity function_shape(Function* f, const Function::ArgumentList& args)
{
    Quantity value;
    if (!s_ensureOneCollectionArg(f, args, &value))
        return CMath::nan();
    QVector<Quantity> dims;
    dims.append(Quantity(value.isList() ? value.columns() : value.rows()));
    if (value.isMatrix())
        dims.append(Quantity(value.columns()));
    return s_makeScalarList(dims);
}

Quantity function_transpose(Function* f, const Function::ArgumentList& args)
{
    Quantity matrix;
    if (!s_ensureMatrixArg(f, args, &matrix))
        return CMath::nan();
    QVector<QVector<Quantity>> rows;
    for (int c = 0; c < matrix.columns(); ++c) {
        QVector<Quantity> row;
        for (int r = 0; r < matrix.rows(); ++r)
            row.append(matrix.matrix().at(r).at(c));
        rows.append(row);
    }
    return s_makeMatrix(rows);
}

Quantity function_dot(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(2);
    if (!args.at(0).isList() || !args.at(1).isList()
        || args.at(0).columns() != args.at(1).columns()) {
        f->setError(OutOfDomain);
        return CMath::nan();
    }
    Quantity result(0);
    const QVector<Quantity> left = args.at(0).elements();
    const QVector<Quantity> right = args.at(1).elements();
    for (int i = 0; i < left.size(); ++i)
        result += left.at(i) * right.at(i);
    return result;
}

Quantity function_cross(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(2);
    if (!args.at(0).isList() || !args.at(1).isList()
        || args.at(0).columns() != 3 || args.at(1).columns() != 3) {
        f->setError(OutOfDomain);
        return CMath::nan();
    }
    const QVector<Quantity> a = args.at(0).elements();
    const QVector<Quantity> b = args.at(1).elements();
    QVector<Quantity> result;
    result << (a.at(1) * b.at(2) - a.at(2) * b.at(1));
    result << (a.at(2) * b.at(0) - a.at(0) * b.at(2));
    result << (a.at(0) * b.at(1) - a.at(1) * b.at(0));
    return s_makeScalarList(result);
}

Quantity function_norm(Function* f, const Function::ArgumentList& args)
{
    Quantity value;
    if (!s_ensureOneCollectionArg(f, args, &value))
        return CMath::nan();
    Quantity sumSquares(0);
    for (const Quantity& element : value.elements())
        sumSquares += element * element;
    return DMath::sqrt(sumSquares);
}

Quantity function_trace(Function* f, const Function::ArgumentList& args)
{
    Quantity matrix;
    if (!s_ensureMatrixArg(f, args, &matrix) || matrix.rows() != matrix.columns()) {
        f->setError(OutOfDomain);
        return CMath::nan();
    }
    Quantity result(0);
    const auto rows = matrix.matrix();
    for (int i = 0; i < matrix.rows(); ++i)
        result += rows.at(i).at(i);
    return result;
}

Quantity function_det(Function* f, const Function::ArgumentList& args)
{
    Quantity matrix;
    if (!s_ensureMatrixArg(f, args, &matrix) || matrix.rows() != matrix.columns()) {
        f->setError(OutOfDomain);
        return CMath::nan();
    }
    QVector<QVector<Quantity>> a = matrix.matrix();
    const int n = matrix.rows();
    Quantity det(1);
    int sign = 1;
    for (int i = 0; i < n; ++i) {
        int pivot = i;
        while (pivot < n && a.at(pivot).at(i).isZero())
            ++pivot;
        if (pivot == n)
            return Quantity(0);
        if (pivot != i) {
            std::swap(a[pivot], a[i]);
            sign = -sign;
        }
        const Quantity pivotValue = a.at(i).at(i);
        det *= pivotValue;
        for (int r = i + 1; r < n; ++r) {
            const Quantity factor = a.at(r).at(i) / pivotValue;
            for (int c = i; c < n; ++c)
                a[r][c] -= factor * a.at(i).at(c);
        }
    }
    return sign < 0 ? -det : det;
}

Quantity function_inv(Function* f, const Function::ArgumentList& args)
{
    Quantity matrix;
    if (!s_ensureMatrixArg(f, args, &matrix) || matrix.rows() != matrix.columns()) {
        f->setError(OutOfDomain);
        return CMath::nan();
    }
    QVector<QVector<Quantity>> a = matrix.matrix();
    const int n = matrix.rows();
    QVector<QVector<Quantity>> inv;
    for (int r = 0; r < n; ++r) {
        QVector<Quantity> row;
        for (int c = 0; c < n; ++c)
            row.append(Quantity(r == c ? 1 : 0));
        inv.append(row);
    }

    for (int i = 0; i < n; ++i) {
        int pivot = i;
        while (pivot < n && a.at(pivot).at(i).isZero())
            ++pivot;
        if (pivot == n) {
            f->setError(OutOfDomain);
            return CMath::nan();
        }
        if (pivot != i) {
            std::swap(a[pivot], a[i]);
            std::swap(inv[pivot], inv[i]);
        }
        const Quantity pivotValue = a.at(i).at(i);
        for (int c = 0; c < n; ++c) {
            a[i][c] /= pivotValue;
            inv[i][c] /= pivotValue;
        }
        for (int r = 0; r < n; ++r) {
            if (r == i)
                continue;
            const Quantity factor = a.at(r).at(i);
            for (int c = 0; c < n; ++c) {
                a[r][c] -= factor * a.at(i).at(c);
                inv[r][c] -= factor * inv.at(i).at(c);
            }
        }
    }
    return s_makeMatrix(inv);
}

Quantity function_rank(Function* f, const Function::ArgumentList& args)
{
    Quantity matrix;
    if (!s_ensureMatrixArg(f, args, &matrix))
        return CMath::nan();
    QVector<QVector<Quantity>> a = matrix.matrix();
    int rank = 0;
    int row = 0;
    for (int col = 0; col < matrix.columns() && row < matrix.rows(); ++col) {
        int pivot = row;
        while (pivot < matrix.rows() && a.at(pivot).at(col).isZero())
            ++pivot;
        if (pivot == matrix.rows())
            continue;
        if (pivot != row)
            std::swap(a[pivot], a[row]);
        const Quantity pivotValue = a.at(row).at(col);
        for (int c = col; c < matrix.columns(); ++c)
            a[row][c] /= pivotValue;
        for (int r = 0; r < matrix.rows(); ++r) {
            if (r == row)
                continue;
            const Quantity factor = a.at(r).at(col);
            for (int c = col; c < matrix.columns(); ++c)
                a[r][c] -= factor * a.at(row).at(c);
        }
        ++rank;
        ++row;
    }
    return Quantity(rank);
}

static Quantity s_covarianceMatrix(Function* f,
                                   const Function::ArgumentList& args,
                                   bool normalize,
                                   StatisticalNormalization normalization)
{
    Quantity matrix;
    if (!s_ensureMatrixArg(f, args, &matrix) || matrix.rows() < 2) {
        f->setError(OutOfDomain);
        return CMath::nan();
    }
    const int divisor = normalization == StatisticalNormalization::Sample
        ? matrix.rows() - 1
        : matrix.rows();
    if (divisor <= 0) {
        f->setError(InvalidParamCount);
        return CMath::nan(InvalidParamCount);
    }
    const auto data = matrix.matrix();
    QVector<Quantity> means;
    for (int c = 0; c < matrix.columns(); ++c) {
        Quantity sum(0);
        for (int r = 0; r < matrix.rows(); ++r)
            sum += data.at(r).at(c);
        means.append(sum / Quantity(matrix.rows()));
    }
    QVector<QVector<Quantity>> covRows;
    for (int i = 0; i < matrix.columns(); ++i) {
        QVector<Quantity> row;
        for (int j = 0; j < matrix.columns(); ++j) {
            Quantity acc(0);
            for (int r = 0; r < matrix.rows(); ++r)
                acc += (data.at(r).at(i) - means.at(i)) * (data.at(r).at(j) - means.at(j));
            row.append(acc / Quantity(divisor));
        }
        covRows.append(row);
    }
    if (!normalize)
        return s_makeMatrix(covRows);
    QVector<QVector<Quantity>> corrRows;
    for (int i = 0; i < matrix.columns(); ++i) {
        QVector<Quantity> row;
        for (int j = 0; j < matrix.columns(); ++j) {
            const Quantity denom = DMath::sqrt(covRows.at(i).at(i) * covRows.at(j).at(j));
            row.append(covRows.at(i).at(j) / denom);
        }
        corrRows.append(row);
    }
    return s_makeMatrix(corrRows);
}

Quantity function_covp(Function* f, const Function::ArgumentList& args)
{
    return s_covarianceMatrix(f, args, false, StatisticalNormalization::Population);
}

Quantity function_covs(Function* f, const Function::ArgumentList& args)
{
    return s_covarianceMatrix(f, args, false, StatisticalNormalization::Sample);
}

Quantity function_corrp(Function* f, const Function::ArgumentList& args)
{
    return s_covarianceMatrix(f, args, true, StatisticalNormalization::Population);
}

Quantity function_corrs(Function* f, const Function::ArgumentList& args)
{
    return s_covarianceMatrix(f, args, true, StatisticalNormalization::Sample);
}

Quantity function_summation(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(3);
    if (!args.at(0).isInteger() || !args.at(1).isInteger()) {
        f->setError(OutOfDomain);
        return DMath::nan(OutOfDomain);
    }

    const Quantity start = args.at(0);
    const Quantity end = args.at(1);
    const Quantity step = (start <= end) ? Quantity(1) : Quantity(-1);
    Quantity result(0);

    for (Quantity n = start; step > 0 ? (n <= end) : (n >= end); n += step) {
        result += args.at(2);
    }

    return result;
}

Quantity function_product(Function* f, const Function::ArgumentList& args)
{
    ENSURE_MINIMUM_ARGUMENT_COUNT(2);
    return std::accumulate(args.begin(), args.end(), Quantity(1), std::multiplies<Quantity>());
}

Quantity function_geomean(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    const Function::ArgumentList values = s_collectionElementsOrArgs(args);
    if (values.count() < 1 || (!args.at(0).isCollection() && values.count() < 2)) {
        f->setError(InvalidParamCount);
        return CMath::nan(InvalidParamCount);
    }

    Quantity result = std::accumulate(values.begin(), values.end(), Quantity(1),
        std::multiplies<Quantity>());

    if (result <= Quantity(0))
        return DMath::nan(OutOfDomain);

    if (values.count() == 1)
        return result;

    if (values.count() == 2)
        return DMath::sqrt(result);

    return  DMath::raise(result, Quantity(1)/Quantity(values.count()));
}

Quantity function_dec(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return Quantity(args.at(0)).setFormat(Quantity::Format::Decimal() + Quantity(args.at(0)).format());
}

Quantity function_rat(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return Quantity(args.at(0)).setFormat(
        Quantity::Format::Decimal()
        + Quantity::Format::General()
        + Quantity::Format::Precision(s_forcedRationalPrecision)
        + Quantity(args.at(0)).format());
}

Quantity function_hex(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return Quantity(args.at(0)).setFormat(Quantity::Format::Fixed() + Quantity::Format::Hexadecimal() + Quantity(args.at(0)).format());
}

Quantity function_oct(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return Quantity(args.at(0)).setFormat(Quantity::Format::Fixed() + Quantity::Format::Octal() + Quantity(args.at(0)).format());
}

Quantity function_bin(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return Quantity(args.at(0)).setFormat(Quantity::Format::Fixed() + Quantity::Format::Binary() + Quantity(args.at(0)).format());
}

Quantity function_sci(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return Quantity(args.at(0)).setFormat(Quantity::Format::Scientific() + Quantity::Format::Decimal() + Quantity(args.at(0)).format());
}

Quantity function_eng(Function* f, const Function::ArgumentList& args)
{
    ENSURE_EITHER_ARGUMENT_COUNT(1, 2);
    Quantity::Format format = Quantity::Format::Engineering() + Quantity::Format::Decimal() + Quantity(args.at(0)).format();
    if (args.count() == 2) {
        int forcedExponent = 0;
        if (!s_tryExtractForcedExponent(args.at(1), &forcedExponent)) {
            f->setError(OutOfDomain);
            return DMath::nan();
        }
        format = format + Quantity::Format::ForcedExponent(forcedExponent);
    }
    return Quantity(args.at(0)).setFormat(format);
}

Quantity function_binpad(Function* f, const Function::ArgumentList& args)
{
    return s_nonDecimalPad(f, args, Quantity::Format::Fixed() + Quantity::Format::Binary());
}

Quantity function_hexpad(Function* f, const Function::ArgumentList& args)
{
    return s_nonDecimalPad(f, args, Quantity::Format::Fixed() + Quantity::Format::Hexadecimal());
}

Quantity function_octpad(Function* f, const Function::ArgumentList& args)
{
    return s_nonDecimalPad(f, args, Quantity::Format::Fixed() + Quantity::Format::Octal());
}

Quantity function_cart(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return Quantity(args.at(0)).setFormat(Quantity::Format::Cartesian() + Quantity(args.at(0)).format());
}

Quantity function_polar(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return Quantity(args.at(0)).setFormat(Quantity::Format::Polar() + Quantity(args.at(0)).format());
}

Quantity function_binompmf(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(3);
    return DMath::binomialPmf(args.at(0), args.at(1), args.at(2));
}

Quantity function_binomcdf(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(3);
    return DMath::binomialCdf(args.at(0), args.at(1), args.at(2));
}

Quantity function_binommean(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return DMath::binomialMean(args.at(0), args.at(1));
}

Quantity function_binomvar(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return DMath::binomialVariance(args.at(0), args.at(1));
}

Quantity function_hyperpmf(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(4);
    return DMath::hypergeometricPmf(args.at(0), args.at(1), args.at(2), args.at(3));
}

Quantity function_hypercdf(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(4);
    return DMath::hypergeometricCdf(args.at(0), args.at(1), args.at(2), args.at(3));
}

Quantity function_hypermean(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(3);
    return DMath::hypergeometricMean(args.at(0), args.at(1), args.at(2));
}

Quantity function_hypervar(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(3);
    return DMath::hypergeometricVariance(args.at(0), args.at(1), args.at(2));
}

Quantity function_poipmf(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return DMath::poissonPmf(args.at(0), args.at(1));
}

Quantity function_poicdf(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return DMath::poissonCdf(args.at(0), args.at(1));
}

Quantity function_poimean(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::poissonMean(args.at(0));
}

Quantity function_poivar(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::poissonVariance(args.at(0));
}

Quantity function_mask(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return DMath::mask(args.at(0), args.at(1));
}

Quantity function_unmask(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return DMath::sgnext(args.at(0), args.at(1));
}

Quantity function_not(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return ~args.at(0);
}

Quantity function_and(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_MINIMUM_ARGUMENT_COUNT(2);
    return std::accumulate(args.begin(), args.end(), Quantity(-1),
        std::mem_fn(&Quantity::operator&));
}

Quantity function_or(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_MINIMUM_ARGUMENT_COUNT(2);
    return std::accumulate(args.begin(), args.end(), Quantity(0),
        std::mem_fn(&Quantity::operator|));
}

Quantity function_xor(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_MINIMUM_ARGUMENT_COUNT(2);
    return std::accumulate(args.begin(), args.end(), Quantity(0),
        std::mem_fn(&Quantity::operator^));
}

Quantity function_popcount(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);

    const Quantity value = args.at(0);
    Quantity result(0);
    for (int i = 0; i < LOGICRANGE; ++i) {
        const Quantity bit = value & (Quantity(1) << Quantity(i));
        if (bit.isNan()) {
            return bit;
        }
        if (!bit.isZero()) {
            result += Quantity(1);
        }
    }
    return result;
}

Quantity function_shl(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return DMath::ashr(args.at(0), -args.at(1));
}

Quantity function_shr(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return DMath::ashr(args.at(0), args.at(1));
}

Quantity function_idiv(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return DMath::idiv(args.at(0), args.at(1));
}

Quantity function_mod(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    return args.at(0) % args.at(1);
}

Quantity function_emod(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(2);
    Quantity result = args.at(0) % args.at(1);
    if (result.isNan() || result.isZero()) {
        return result;
    }
    if (result.isNegative() != args.at(1).isNegative()) {
        result += args.at(1);
    }
    return result;
}

Quantity function_powmod(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(3);

    const Quantity base = args.at(0);
    const Quantity exponent = args.at(1);
    const Quantity modulo = args.at(2);

    if (!base.isInteger() || !exponent.isInteger() || !modulo.isInteger()) {
        f->setError(TypeMismatch);
        return DMath::nan(TypeMismatch);
    }
    if (modulo.isZero()) {
        f->setError(ZeroDivide);
        return DMath::nan(ZeroDivide);
    }
    if (exponent.isNegative()) {
        f->setError(OutOfDomain);
        return DMath::nan(OutOfDomain);
    }

    // Euclidean reduction helper: result has divisor sign (or is zero).
    auto emod = [&modulo](const Quantity& value) {
        Quantity result = value % modulo;
        if (result.isNan() || result.isZero()) {
            return result;
        }
        if (result.isNegative() != modulo.isNegative()) {
            result += modulo;
        }
        return result;
    };

    Quantity e = exponent;
    Quantity factor = emod(base);
    Quantity result = emod(Quantity(1));
    const Quantity zero(0);
    const Quantity two(2);

    while (e > zero) {
        if (!(e % two).isZero()) {
            result = emod(result * factor);
        }
        e = DMath::idiv(e, two);
        if (e > zero) {
            factor = emod(factor * factor);
        }
    }

    return result;
}

Quantity function_ieee754_decode(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_EITHER_ARGUMENT_COUNT(3, 4);
    if (args.count() == 3) {
        return DMath::decodeIeee754(args.at(0), args.at(1), args.at(2));
    } else {
        return DMath::decodeIeee754(args.at(0), args.at(1), args.at(2), args.at(3));
    }
}

Quantity function_ieee754_encode(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_EITHER_ARGUMENT_COUNT(3, 4);
    if (args.count() == 3) {
        return DMath::encodeIeee754(args.at(0), args.at(1), args.at(2));
    } else {
        return DMath::encodeIeee754(args.at(0), args.at(1), args.at(2), args.at(3));
    }
}

Quantity function_ieee754_half_decode(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::decodeIeee754(args.at(0), 5, 10);
}

Quantity function_ieee754_half_encode(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::encodeIeee754(args.at(0), 5, 10);
}

Quantity function_ieee754_single_decode(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::decodeIeee754(args.at(0), 8, 23);
}

Quantity function_ieee754_single_encode(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::encodeIeee754(args.at(0), 8, 23);
}

Quantity function_ieee754_double_decode(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::decodeIeee754(args.at(0), 11, 52);
}

Quantity function_ieee754_double_encode(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::encodeIeee754(args.at(0), 11, 52);
}

Quantity function_ieee754_quad_decode(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::decodeIeee754(args.at(0), 15, 112);
}

Quantity function_ieee754_quad_encode(Function* f, const Function::ArgumentList& args)
{
    /* TODO : complex mode switch for this function */
    ENSURE_ARGUMENT_COUNT(1);
    return DMath::encodeIeee754(args.at(0), 15, 112);
}

static Quantity s_ieee754_round(const Function::ArgumentList& args, int exponentBits, int significandBits)
{
    const Quantity encoded = DMath::encodeIeee754(args.at(0), exponentBits, significandBits);
    return DMath::decodeIeee754(encoded, exponentBits, significandBits);
}

static Quantity s_ieee754_residual(const Function::ArgumentList& args, int exponentBits, int significandBits)
{
    return args.at(0) - s_ieee754_round(args, exponentBits, significandBits);
}

Quantity function_ieee754_round_half(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return s_ieee754_round(args, 5, 10);
}

Quantity function_ieee754_round_float(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return s_ieee754_round(args, 8, 23);
}

Quantity function_ieee754_round_double(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return s_ieee754_round(args, 11, 52);
}

Quantity function_ieee754_round_quad(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return s_ieee754_round(args, 15, 112);
}

Quantity function_ieee754_half_residual(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return s_ieee754_residual(args, 5, 10);
}

Quantity function_ieee754_float_residual(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return s_ieee754_residual(args, 8, 23);
}

Quantity function_ieee754_double_residual(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return s_ieee754_residual(args, 11, 52);
}

Quantity function_ieee754_quad_residual(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    return s_ieee754_residual(args, 15, 112);
}

Quantity function_datetime(Function* f, const Function::ArgumentList& args)
{
    ENSURE_EITHER_ARGUMENT_COUNT(1, 2);
    time_t timestamp = args.at(0).numericValue().toInt();
    struct tm *time;

    char format[] = "%Y%m%d.%H%M%S";
    char res[32];

    if (args.count() == 2) {
        timestamp += (args.at(1).numericValue() * 3600).toInt();
        time = std::gmtime(&timestamp);
    } else {
        time = localtime(&timestamp);
    }

    if (time == nullptr) {
        // Happen when specifying a negative timestamp on Windows
        f->setError(OutOfIntegerRange);
        return CMath::nan(OutOfIntegerRange);
    }

    strftime(res, sizeof(res), format, time);
    HNumber Temp(res);
    return Quantity(Temp).setFormat(Quantity::Format::Fixed() + Quantity::Format::Decimal() + Quantity::Format::Precision(6));

}

Quantity function_epoch(Function* f, const Function::ArgumentList& args)
{
    ENSURE_EITHER_ARGUMENT_COUNT(1, 2);
    ENSURE_REAL_ARGUMENT(0);
    if (args.count() == 2)
        ENSURE_REAL_ARGUMENT(1);

    const QString rawDateTime = HMath::format(
        args.at(0).numericValue().real,
        HNumber::Format::Fixed() + HNumber::Format::Point() + HNumber::Format::Precision(6));
    const QStringList parts = rawDateTime.split(MathDsl::DotSep);
    if (parts.count() != 2 || parts.at(0).size() != 8 || parts.at(1).size() != 6) {
        f->setError(OutOfDomain);
        return DMath::nan();
    }

    bool okYear = false, okMonth = false, okDay = false;
    bool okHour = false, okMinute = false, okSecond = false;
    const int year = parts.at(0).mid(0, 4).toInt(&okYear);
    const int month = parts.at(0).mid(4, 2).toInt(&okMonth);
    const int day = parts.at(0).mid(6, 2).toInt(&okDay);
    const int hour = parts.at(1).mid(0, 2).toInt(&okHour);
    const int minute = parts.at(1).mid(2, 2).toInt(&okMinute);
    const int second = parts.at(1).mid(4, 2).toInt(&okSecond);
    if (!okYear || !okMonth || !okDay || !okHour || !okMinute || !okSecond) {
        f->setError(OutOfDomain);
        return DMath::nan();
    }

    const QDate date(year, month, day);
    const QTime time(hour, minute, second);
    if (!date.isValid() || !time.isValid()) {
        f->setError(OutOfDomain);
        return DMath::nan();
    }

    QDateTime dateTime(date, time);
    if (args.count() == 2) {
        const int offsetSeconds = (args.at(1).numericValue() * 3600).toInt();
        const QTimeZone zone(offsetSeconds);
        if (!zone.isValid()) {
            f->setError(OutOfDomain);
            return DMath::nan();
        }
        dateTime = QDateTime(date, time, zone);
    }

    const qint64 epochSeconds = dateTime.toSecsSinceEpoch();
    const QByteArray epochText = QByteArray::number(epochSeconds);
    return Quantity(HNumber(epochText.constData()));
}

Quantity function_molmass(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(1);
    f->setError(OutOfDomain);
    return DMath::nan(OutOfDomain);
}

Quantity function_mass(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(2);
    f->setError(OutOfDomain);
    return DMath::nan(OutOfDomain);
}

Quantity function_molarity(Function* f, const Function::ArgumentList& args)
{
    ENSURE_ARGUMENT_COUNT(2);

    Quantity amount = args.at(0);
    Quantity volume = args.at(1);
    const Quantity oneMole = Units::mole();
    const Quantity oneLitre = Units::litre();

    if (amount.isDimensionless()) {
        amount *= oneMole;
    } else if (!amount.sameDimension(oneMole)) {
        f->setError(OutOfDomain);
        return DMath::nan(OutOfDomain);
    }

    if (volume.isDimensionless()) {
        volume *= oneLitre;
    } else if (!volume.sameDimension(oneLitre)) {
        f->setError(OutOfDomain);
        return DMath::nan(OutOfDomain);
    }

    Quantity concentration = amount / volume;
    const Quantity molPerLitre = oneMole / oneLitre;
    const QString molPerLitreSymbol =
        QString(::unitSymbol(UnitId::Mole))
        + MathDsl::DivOp
        + QString(::unitSymbol(UnitId::Litre));
    concentration.setDisplayUnit(molPerLitre.numericValue(), molPerLitreSymbol);
    return concentration;
}

void FunctionRepo::createFunctions()
{
    // Aggregation.
    FUNCTION_INSERT(FunctionDomain::Aggregation, max);
    FUNCTION_INSERT(FunctionDomain::Aggregation, min);
    FUNCTION_INSERT(FunctionDomain::Aggregation, product);
    FUNCTION_INSERT(FunctionDomain::Aggregation, sum);
    FUNCTION_INSERT(FunctionDomain::Aggregation, summation);
    FUNCTION_INSERT(FunctionDomain::Aggregation, count);
    FUNCTION_INSERT(FunctionDomain::Aggregation, list);

    // Angle conversion.
    FUNCTION_INSERT(FunctionDomain::AngleConversion, degrees);
    FUNCTION_INSERT(FunctionDomain::AngleConversion, gradians);
    FUNCTION_INSERT(FunctionDomain::AngleConversion, radians);
    FUNCTION_INSERT(FunctionDomain::AngleConversion, turns);

    // Arithmetic.
    FUNCTION_INSERT(FunctionDomain::Arithmetic, abs);
    FUNCTION_INSERT(FunctionDomain::Arithmetic, cbrt);
    FUNCTION_INSERT(FunctionDomain::Arithmetic, ceil);
    FUNCTION_INSERT(FunctionDomain::Arithmetic, floor);
    FUNCTION_INSERT(FunctionDomain::Arithmetic, frac);
    FUNCTION_INSERT(FunctionDomain::Arithmetic, int);
    FUNCTION_INSERT(FunctionDomain::Arithmetic, round);
    FUNCTION_INSERT(FunctionDomain::Arithmetic, sgn);
    FUNCTION_INSERT(FunctionDomain::Arithmetic, sqrt);
    FUNCTION_INSERT(FunctionDomain::Arithmetic, trunc);

    // Base conversion.
    FUNCTION_INSERT(FunctionDomain::BaseConversion, bin);
    FUNCTION_INSERT(FunctionDomain::BaseConversion, binpad);
    FUNCTION_INSERT(FunctionDomain::BaseConversion, dec);
    FUNCTION_INSERT(FunctionDomain::BaseConversion, hex);
    FUNCTION_INSERT(FunctionDomain::BaseConversion, hexpad);
    FUNCTION_INSERT(FunctionDomain::BaseConversion, oct);
    FUNCTION_INSERT(FunctionDomain::BaseConversion, octpad);

    // Bitwise.
    FUNCTION_INSERT(FunctionDomain::Bitwise, and);
    FUNCTION_INSERT(FunctionDomain::Bitwise, mask);
    FUNCTION_INSERT(FunctionDomain::Bitwise, not);
    FUNCTION_INSERT(FunctionDomain::Bitwise, or);
    FUNCTION_INSERT(FunctionDomain::Bitwise, popcount);
    FUNCTION_INSERT(FunctionDomain::Bitwise, shl);
    FUNCTION_INSERT(FunctionDomain::Bitwise, shr);
    FUNCTION_INSERT(FunctionDomain::Bitwise, unmask);
    FUNCTION_INSERT(FunctionDomain::Bitwise, xor);

    // Chemistry.
    FUNCTION_INSERT(FunctionDomain::Chemistry, mass);
    FUNCTION_INSERT(FunctionDomain::Chemistry, molarity);
    FUNCTION_INSERT(FunctionDomain::Chemistry, molmass);

    // Combinatorics.
    FUNCTION_INSERT(FunctionDomain::Combinatorics, ncr);
    FUNCTION_INSERT(FunctionDomain::Combinatorics, npr);

    // Complex.
    FUNCTION_INSERT(FunctionDomain::Complex, cart);
    FUNCTION_INSERT(FunctionDomain::Complex, conj);
    FUNCTION_INSERT(FunctionDomain::Complex, imag);
    FUNCTION_INSERT(FunctionDomain::Complex, phase);
    FUNCTION_INSERT(FunctionDomain::Complex, polar);
    FUNCTION_INSERT(FunctionDomain::Complex, real);

    // Date & Time.
    FUNCTION_INSERT(FunctionDomain::DateTime, datetime);
    FUNCTION_INSERT(FunctionDomain::DateTime, epoch);

    // Exponential & Logarithmic.
    FUNCTION_INSERT(FunctionDomain::ExponentialLogarithmic, exp);
    FUNCTION_INSERT(FunctionDomain::ExponentialLogarithmic, ln);
    FUNCTION_INSERT(FunctionDomain::ExponentialLogarithmic, log);
    FUNCTION_INSERT(FunctionDomain::ExponentialLogarithmic, log10);
    FUNCTION_INSERT(FunctionDomain::ExponentialLogarithmic, log2);

    // Floating point.
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_decode);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_double_decode);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_double_encode);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_double_residual);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_encode);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_float_residual);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_half_decode);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_half_encode);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_half_residual);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_quad_decode);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_quad_encode);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_quad_residual);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_round_double);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_round_float);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_round_half);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_round_quad);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_single_decode);
    FUNCTION_INSERT(FunctionDomain::FloatingPoint, ieee754_single_encode);

    // Integer arithmetic.
    FUNCTION_INSERT(FunctionDomain::IntegerArithmetic, emod);
    FUNCTION_INSERT(FunctionDomain::IntegerArithmetic, gcd);
    FUNCTION_INSERT(FunctionDomain::IntegerArithmetic, idiv);
    FUNCTION_INSERT(FunctionDomain::IntegerArithmetic, lcm);
    FUNCTION_INSERT(FunctionDomain::IntegerArithmetic, mod);
    FUNCTION_INSERT(FunctionDomain::IntegerArithmetic, powmod);

    // Number formatting.
    FUNCTION_INSERT(FunctionDomain::NumberFormatting, eng);
    FUNCTION_INSERT(FunctionDomain::NumberFormatting, rat);
    FUNCTION_INSERT(FunctionDomain::NumberFormatting, sci);

    // Probability.
    FUNCTION_INSERT(FunctionDomain::Probability, binomcdf);
    FUNCTION_INSERT(FunctionDomain::Probability, binommean);
    FUNCTION_INSERT(FunctionDomain::Probability, binompmf);
    FUNCTION_INSERT(FunctionDomain::Probability, binomvar);
    FUNCTION_INSERT(FunctionDomain::Probability, hypercdf);
    FUNCTION_INSERT(FunctionDomain::Probability, hypermean);
    FUNCTION_INSERT(FunctionDomain::Probability, hyperpmf);
    FUNCTION_INSERT(FunctionDomain::Probability, hypervar);
    FUNCTION_INSERT(FunctionDomain::Probability, poicdf);
    FUNCTION_INSERT(FunctionDomain::Probability, poimean);
    FUNCTION_INSERT(FunctionDomain::Probability, poipmf);
    FUNCTION_INSERT(FunctionDomain::Probability, poivar);

    // Random.
    FUNCTION_INSERT(FunctionDomain::Random, rand);
    FUNCTION_INSERT(FunctionDomain::Random, randint);

    // Special functions.
    FUNCTION_INSERT(FunctionDomain::SpecialFunctions, erf);
    FUNCTION_INSERT(FunctionDomain::SpecialFunctions, erfc);
    FUNCTION_INSERT(FunctionDomain::SpecialFunctions, gamma);
    FUNCTION_INSERT(FunctionDomain::SpecialFunctions, lngamma);

    // Statistics.
    FUNCTION_INSERT(FunctionDomain::Statistics, absdev);
    FUNCTION_INSERT(FunctionDomain::Statistics, average);
    FUNCTION_INSERT(FunctionDomain::Statistics, mean);
    FUNCTION_INSERT(FunctionDomain::Statistics, geomean);
    FUNCTION_INSERT(FunctionDomain::Statistics, mad);
    FUNCTION_INSERT(FunctionDomain::Statistics, median);
    FUNCTION_INSERT(FunctionDomain::Statistics, stdevp);
    FUNCTION_INSERT(FunctionDomain::Statistics, stdevs);
    FUNCTION_INSERT(FunctionDomain::Statistics, varp);
    FUNCTION_INSERT(FunctionDomain::Statistics, vars);

    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, cols);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, corrp);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, corrs);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, covp);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, covs);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, cross);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, det);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, dot);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, flatten);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, inv);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, norm);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, rank);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, rows);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, shape);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, trace);
    FUNCTION_INSERT(FunctionDomain::LinearAlgebra, transpose);

    // Trigonometry.
    FUNCTION_INSERT(FunctionDomain::Trigonometry, arccos);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, arcosh);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, arcsin);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, arctan);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, arctan2);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, arsinh);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, artanh);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, cos);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, cosh);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, cot);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, csc);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, sec);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, sin);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, sinh);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, tan);
    FUNCTION_INSERT(FunctionDomain::Trigonometry, tanh);

// Symbol aliases.
    m_functions.insert(QString(UnicodeChars::Summation).toUpper(), find(QStringLiteral("summation")));
    m_functions.insert(QString(UnicodeChars::SquareRoot).toUpper(), find(QStringLiteral("sqrt")));
    m_functions.insert(QString(UnicodeChars::CubeRoot).toUpper(), find(QStringLiteral("cbrt")));
    m_functions.insert(QStringLiteral("RATIO"), find(QStringLiteral("rat")));
    m_functions.insert(QStringLiteral("RATIONAL"), find(QStringLiteral("rat")));
}

FunctionRepo* FunctionRepo::instance()
{
    if (!s_FunctionRepoInstance) {
        s_FunctionRepoInstance = new FunctionRepo;
        qAddPostRoutine(s_deleteFunctions);
    }
    return s_FunctionRepoInstance;
}

FunctionRepo::FunctionRepo()
{
    createFunctions();
    setNonTranslatableFunctionUsages();
    retranslateText();
}

void FunctionRepo::insert(Function* function)
{
    if (!function)
        return;
    function->setDomain(domainToDisplay(function->domainType()));
    m_functions.insert(function->identifier().toUpper(), function);
}

Function* FunctionRepo::find(const QString& identifier) const
{
    if (identifier.isNull())
        return 0;
    return m_functions.value(identifier.toUpper(), 0);
}

bool FunctionRepo::isIdentifierAliasOf(const QString& identifier, const QString& canonicalIdentifier) const
{
    Function* canonical = find(canonicalIdentifier);
    if (!canonical)
        return false;
    return find(identifier) == canonical;
}

QString FunctionRepo::displayIdentifier(const QString& identifier) const
{
    Function* function = find(identifier);
    if (!function)
        return identifier;

    const bool isAsciiWord = std::all_of(identifier.begin(), identifier.end(), [](QChar ch) {
        return (ch >= QLatin1Char('A') && ch <= QLatin1Char('Z'))
               || (ch >= QLatin1Char('a') && ch <= QLatin1Char('z'))
               || (ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
               || ch == QLatin1Char('_');
    });
    if (isAsciiWord)
        return function->identifier();

    return identifier;
}

QStringList FunctionRepo::getIdentifiers() const
{
    QStringList result = m_functions.keys();
    std::transform(result.begin(), result.end(), result.begin(), [this](const QString& s) {
        return displayIdentifier(s);
    });
    result.removeDuplicates();
    return result;
}

const QStringList& FunctionRepo::domains() const
{
    static QStringList result;
    result = QStringList()
        << domainToDisplay(FunctionDomain::Arithmetic)
        << domainToDisplay(FunctionDomain::Trigonometry)
        << domainToDisplay(FunctionDomain::ExponentialLogarithmic)
        << domainToDisplay(FunctionDomain::Aggregation)
        << domainToDisplay(FunctionDomain::Statistics)
        << domainToDisplay(FunctionDomain::LinearAlgebra)
        << domainToDisplay(FunctionDomain::Complex)
        << domainToDisplay(FunctionDomain::AngleConversion)
        << domainToDisplay(FunctionDomain::BaseConversion)
        << domainToDisplay(FunctionDomain::IntegerArithmetic)
        << domainToDisplay(FunctionDomain::NumberFormatting)
        << domainToDisplay(FunctionDomain::Random)
        << domainToDisplay(FunctionDomain::Probability)
        << domainToDisplay(FunctionDomain::Combinatorics)
        << domainToDisplay(FunctionDomain::DateTime)
        << domainToDisplay(FunctionDomain::Bitwise)
        << domainToDisplay(FunctionDomain::FloatingPoint)
        << domainToDisplay(FunctionDomain::SpecialFunctions)
        << domainToDisplay(FunctionDomain::Chemistry);
    return result;
}

void FunctionRepo::setNonTranslatableFunctionUsages()
{
    FUNCTION_USAGE(abs, "x");
    FUNCTION_USAGE(absdev, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(list, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(arccos, "x");
    FUNCTION_USAGE(and, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(arcosh, "x");
    FUNCTION_USAGE(arsinh, "x");
    FUNCTION_USAGE(artanh, "x");
    FUNCTION_USAGE(arcsin, "x");
    FUNCTION_USAGE(arctan, "x");
    FUNCTION_USAGE(arctan2, "x; y");
    FUNCTION_USAGE(average, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(bin, "n");
    FUNCTION_USAGE(binpad, "n [; bits]");
    FUNCTION_USAGE(cart, "x");
    FUNCTION_USAGE(cbrt, "x");
    FUNCTION_USAGE(ceil, "x");
    FUNCTION_USAGE(conj, "x");
    FUNCTION_USAGE(cos, "x");
    FUNCTION_USAGE(cosh, "x");
    FUNCTION_USAGE(cols, "matrix");
    FUNCTION_USAGE(corrp, "matrix");
    FUNCTION_USAGE(corrs, "matrix");
    FUNCTION_USAGE(count, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(covp, "matrix");
    FUNCTION_USAGE(covs, "matrix");
    FUNCTION_USAGE(cross, "list; list");
    FUNCTION_USAGE(cot, "x");
    FUNCTION_USAGE(csc, "x");
    FUNCTION_USAGE(dec, "x");
    FUNCTION_USAGE(degrees, "x");
    FUNCTION_USAGE(det, "matrix");
    FUNCTION_USAGE(dot, "list; list");
    FUNCTION_USAGE(eng, "x [; exponent]");
    FUNCTION_USAGE(erf, "x");
    FUNCTION_USAGE(erfc, "x");
    FUNCTION_USAGE(exp, "x");
    FUNCTION_USAGE(floor, "x");
    FUNCTION_USAGE(flatten, "matrix");
    FUNCTION_USAGE(frac, "x");
    FUNCTION_USAGE(gamma, "x");
    FUNCTION_USAGE(gcd, "n<sub>1</sub>; n<sub>2</sub>; ...");
    FUNCTION_USAGE(lcm, "n<sub>1</sub>; n<sub>2</sub>; ...");
    FUNCTION_USAGE(geomean, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(gradians, "x");
    FUNCTION_USAGE(hex, "n");
    FUNCTION_USAGE(hexpad, "n [; bits]");
    FUNCTION_USAGE(ieee754_half_decode, "x");
    FUNCTION_USAGE(ieee754_half_encode, "x");
    FUNCTION_USAGE(ieee754_single_decode, "x");
    FUNCTION_USAGE(ieee754_single_encode, "x");
    FUNCTION_USAGE(ieee754_double_decode, "x");
    FUNCTION_USAGE(ieee754_double_encode, "x");
    FUNCTION_USAGE(ieee754_quad_decode, "x");
    FUNCTION_USAGE(ieee754_quad_encode, "x");
    FUNCTION_USAGE(ieee754_round_half, "x");
    FUNCTION_USAGE(ieee754_round_float, "x");
    FUNCTION_USAGE(ieee754_round_double, "x");
    FUNCTION_USAGE(ieee754_round_quad, "x");
    FUNCTION_USAGE(ieee754_half_residual, "x");
    FUNCTION_USAGE(ieee754_float_residual, "x");
    FUNCTION_USAGE(ieee754_double_residual, "x");
    FUNCTION_USAGE(ieee754_quad_residual, "x");
    FUNCTION_USAGE(int, "x");
    FUNCTION_USAGE(imag, "x");
    FUNCTION_USAGE(inv, "matrix");
    FUNCTION_USAGE(log2, "x");
    FUNCTION_USAGE(log10, "x");
    FUNCTION_USAGE(ln, "x");
    FUNCTION_USAGE(lngamma, "x");
    FUNCTION_USAGE(mad, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(mass, "mol; formula");
    FUNCTION_USAGE(molarity, "n; V");
    FUNCTION_USAGE(molmass, "formula");
    FUNCTION_USAGE(max, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(median, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(mean, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(min, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(ncr, "x<sub>1</sub>; x<sub>2</sub>");
    FUNCTION_USAGE(not, "n");
    FUNCTION_USAGE(npr, "x<sub>1</sub>; x<sub>2</sub>");
    FUNCTION_USAGE(oct, "n");
    FUNCTION_USAGE(octpad, "n [; bits]");
    FUNCTION_USAGE(norm, "list-or-matrix");
    FUNCTION_USAGE(or, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(popcount, "n");
    FUNCTION_USAGE(polar, "x");
    FUNCTION_USAGE(product, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(phase, "x");
    FUNCTION_USAGE(radians, "x");
    FUNCTION_USAGE(rank, "matrix");
    FUNCTION_USAGE(real, "x");
    FUNCTION_USAGE(rat, "x");
    FUNCTION_USAGE(sci, "x");
    FUNCTION_USAGE(sec, "x)");
    FUNCTION_USAGE(sgn, "x");
    FUNCTION_USAGE(rows, "matrix");
    FUNCTION_USAGE(shape, "list-or-matrix");
    FUNCTION_USAGE(summation, "start; end; expression");
    FUNCTION_USAGE(sin, "x");
    FUNCTION_USAGE(sinh, "x");
    FUNCTION_USAGE(sqrt, "x");
    FUNCTION_USAGE(stdevp, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(stdevs, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(sum, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(tan, "x");
    FUNCTION_USAGE(turns, "x");
    FUNCTION_USAGE(tanh, "x");
    FUNCTION_USAGE(trunc, "x");
    FUNCTION_USAGE(trace, "matrix");
    FUNCTION_USAGE(transpose, "matrix");
    FUNCTION_USAGE(varp, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(vars, "x<sub>1</sub>; x<sub>2</sub>; ...");
    FUNCTION_USAGE(xor, "x<sub>1</sub>; x<sub>2</sub>; ...");
}

void FunctionRepo::setTranslatableFunctionUsages()
{
    FUNCTION_USAGE_TR(binomcdf, tr("max; trials; probability"));
    FUNCTION_USAGE_TR(binommean, tr("trials; probability"));
    FUNCTION_USAGE_TR(binompmf, tr("hits; trials; probability"));
    FUNCTION_USAGE_TR(binomvar, tr("trials; probability"));
    FUNCTION_USAGE_TR(datetime, tr("unix_timestamp; x hours offset to GMT"));
    FUNCTION_USAGE_TR(epoch, tr("yyyymmdd.hhmmss; x hours offset to GMT"));
    FUNCTION_USAGE_TR(hypercdf, tr("max; total; hits; trials"));
    FUNCTION_USAGE_TR(hypermean, tr("total; hits; trials"));
    FUNCTION_USAGE_TR(hyperpmf, tr("count; total; hits; trials"));
    FUNCTION_USAGE_TR(hypervar, tr("total; hits; trials"));
    FUNCTION_USAGE_TR(idiv, tr("dividend; divisor"));
    FUNCTION_USAGE_TR(ieee754_decode, tr("x; exponent_bits; significand_bits [; exponent_bias]"));
    FUNCTION_USAGE_TR(ieee754_encode, tr("x; exponent_bits; significand_bits [; exponent_bias]"));
    FUNCTION_USAGE_TR(log, tr("base; x"));
    FUNCTION_USAGE_TR(rand, tr("[digits]"));
    FUNCTION_USAGE_TR(randint, tr("max [; min]"));
    FUNCTION_USAGE_TR(mask, "x; bits");
    FUNCTION_USAGE_TR(mod, tr("value; modulo"));
    FUNCTION_USAGE_TR(emod, tr("value; modulo"));
    FUNCTION_USAGE_TR(powmod, tr("base; exponent; modulo"));
    FUNCTION_USAGE_TR(poicdf, tr("events; average_events"));
    FUNCTION_USAGE_TR(poimean, tr("average_events"));
    FUNCTION_USAGE_TR(poipmf, tr("events; average_events"));
    FUNCTION_USAGE_TR(poivar, tr("average_events"));
    FUNCTION_USAGE_TR(round, tr("x [; precision]"));
    FUNCTION_USAGE_TR(shl, "x; bits");
    FUNCTION_USAGE_TR(shr, "x; bits");
    FUNCTION_USAGE_TR(unmask, "x; bits");
}

void FunctionRepo::setFunctionNames()
{
    FUNCTION_NAME(abs, tr("Absolute Value"));
    FUNCTION_NAME(absdev, tr("Absolute Deviation"));
    FUNCTION_NAME(list, tr("List Literal"));
    FUNCTION_NAME(arccos, tr("Arc Cosine"));
    FUNCTION_NAME(and, tr("Logical AND"));
    FUNCTION_NAME(arcosh, tr("Area Hyperbolic Cosine"));
    FUNCTION_NAME(arsinh, tr("Area Hyperbolic Sine"));
    FUNCTION_NAME(artanh, tr("Area Hyperbolic Tangent"));
    FUNCTION_NAME(arcsin, tr("Arc Sine"));
    FUNCTION_NAME(arctan, tr("Arc Tangent"));
    FUNCTION_NAME(arctan2, tr("Arc Tangent with two Arguments"));
    FUNCTION_NAME(average, tr("Average (Arithmetic Mean)"));
    FUNCTION_NAME(bin, tr("Convert to Binary Representation"));
    FUNCTION_NAME(binpad, tr("Convert to Padded Binary Representation"));
    FUNCTION_NAME(binomcdf, tr("Binomial Cumulative Distribution Function"));
    FUNCTION_NAME(binommean, tr("Binomial Distribution Mean"));
    FUNCTION_NAME(binompmf, tr("Binomial Probability Mass Function"));
    FUNCTION_NAME(binomvar, tr("Binomial Distribution Variance"));
    FUNCTION_NAME(cart, tr("Convert to Cartesian Notation"));
    FUNCTION_NAME(cbrt, tr("Cube Root"));
    FUNCTION_NAME(ceil, tr("Ceiling"));
    FUNCTION_NAME(conj, tr("Complex Conjugate"));
    FUNCTION_NAME(cos, tr("Cosine"));
    FUNCTION_NAME(cosh, tr("Hyperbolic Cosine"));
    FUNCTION_NAME(cols, tr("Matrix Column Count"));
    FUNCTION_NAME(corrp, tr("Population Correlation Matrix (n)"));
    FUNCTION_NAME(corrs, tr("Sample Correlation Matrix (n-1)"));
    FUNCTION_NAME(count, tr("Count"));
    FUNCTION_NAME(covp, tr("Population Covariance Matrix (n)"));
    FUNCTION_NAME(covs, tr("Sample Covariance Matrix (n-1)"));
    FUNCTION_NAME(cross, tr("Cross Product"));
    FUNCTION_NAME(cot, tr("Cotangent"));
    FUNCTION_NAME(csc, tr("Cosecant"));
    FUNCTION_NAME(datetime, tr("Convert Unix timestamp to Date"));
    FUNCTION_NAME(epoch, tr("Convert Date to Unix timestamp"));
    FUNCTION_NAME(dec, tr("Convert to Decimal Representation"));
    FUNCTION_NAME(degrees, tr("Degrees of Arc"));
    FUNCTION_NAME(det, tr("Determinant"));
    FUNCTION_NAME(dot, tr("Dot Product"));
    FUNCTION_NAME(eng, tr("Convert to Engineering Notation"));
    FUNCTION_NAME(erf, tr("Error Function"));
    FUNCTION_NAME(erfc, tr("Complementary Error Function"));
    FUNCTION_NAME(exp, tr("Exponential"));
    FUNCTION_NAME(floor, tr("Floor"));
    FUNCTION_NAME(flatten, tr("Flatten Matrix"));
    FUNCTION_NAME(frac, tr("Fractional Part"));
    FUNCTION_NAME(gamma, tr("Extension of Factorials [= (x-1)!]"));
    FUNCTION_NAME(gcd, tr("Greatest Common Divisor"));
    FUNCTION_NAME(lcm, tr("Least Common Multiple"));
    FUNCTION_NAME(geomean, tr("Geometric Mean"));
    FUNCTION_NAME(gradians, tr("Gradians of arc"));
    FUNCTION_NAME(hex, tr("Convert to Hexadecimal Representation"));
    FUNCTION_NAME(hexpad, tr("Convert to Padded Hexadecimal Representation"));
    FUNCTION_NAME(hypercdf, tr("Hypergeometric Cumulative Distribution Function"));
    FUNCTION_NAME(hypermean, tr("Hypergeometric Distribution Mean"));
    FUNCTION_NAME(hyperpmf, tr("Hypergeometric Probability Mass Function"));
    FUNCTION_NAME(hypervar, tr("Hypergeometric Distribution Variance"));
    FUNCTION_NAME(idiv, tr("Integer Quotient"));
    FUNCTION_NAME(int, tr("Integer Part"));
    FUNCTION_NAME(imag, tr("Imaginary Part"));
    FUNCTION_NAME(inv, tr("Inverse Matrix"));
    FUNCTION_NAME(ieee754_decode, tr("Decode IEEE-754 Binary Value"));
    FUNCTION_NAME(ieee754_encode, tr("Encode IEEE-754 Binary Value"));
    FUNCTION_NAME(ieee754_half_decode, tr("Decode 16-bit Half-Precision Value"));
    FUNCTION_NAME(ieee754_half_encode, tr("Encode 16-bit Half-Precision Value"));
    FUNCTION_NAME(ieee754_single_decode, tr("Decode 32-bit Single-Precision Value"));
    FUNCTION_NAME(ieee754_single_encode, tr("Encode 32-bit Single-Precision Value"));
    FUNCTION_NAME(ieee754_double_decode, tr("Decode 64-bit Double-Precision Value"));
    FUNCTION_NAME(ieee754_double_encode, tr("Encode 64-bit Double-Precision Value"));
    FUNCTION_NAME(ieee754_quad_decode, tr("Decode 128-bit Quad-Precision Value"));
    FUNCTION_NAME(ieee754_quad_encode, tr("Encode 128-bit Quad-Precision Value"));
    FUNCTION_NAME(ieee754_round_half, tr("Round to 16-bit Half-Precision Value"));
    FUNCTION_NAME(ieee754_round_float, tr("Round to 32-bit Single-Precision Value"));
    FUNCTION_NAME(ieee754_round_double, tr("Round to 64-bit Double-Precision Value"));
    FUNCTION_NAME(ieee754_round_quad, tr("Round to 128-bit Quad-Precision Value"));
    FUNCTION_NAME(ieee754_half_residual, tr("Residual from 16-bit Half-Precision Rounding"));
    FUNCTION_NAME(ieee754_float_residual, tr("Residual from 32-bit Single-Precision Rounding"));
    FUNCTION_NAME(ieee754_double_residual, tr("Residual from 64-bit Double-Precision Rounding"));
    FUNCTION_NAME(ieee754_quad_residual, tr("Residual from 128-bit Quad-Precision Rounding"));
    FUNCTION_NAME(log2, tr("Binary Logarithm"));
    FUNCTION_NAME(log10, tr("Common Logarithm"));
    FUNCTION_NAME(ln, tr("Natural Logarithm"));
    FUNCTION_NAME(lngamma, "ln(abs(Gamma))");
    FUNCTION_NAME(mass, tr("Substance Mass"));
    FUNCTION_NAME(molarity, tr("Molarity"));
    FUNCTION_NAME(molmass, tr("Molar Mass"));
    FUNCTION_NAME(log, tr("Logarithm to Arbitrary Base"));
    FUNCTION_NAME(mad, tr("Median Absolute Deviation"));
    FUNCTION_NAME(mask, tr("Mask to a bit size"));
    FUNCTION_NAME(max, tr("Maximum"));
    FUNCTION_NAME(median, tr("Median Value (50th Percentile)"));
    FUNCTION_NAME(mean, tr("Mean"));
    FUNCTION_NAME(min, tr("Minimum"));
    FUNCTION_NAME(rand, tr("Random Decimal Number"));
    FUNCTION_NAME(randint, tr("Random Integer Number"));
    FUNCTION_NAME(rat, tr("Convert to Rational Representation"));
    FUNCTION_NAME(mod, tr("Modulo"));
    FUNCTION_NAME(emod, tr("Euclidean Modulo"));
    FUNCTION_NAME(powmod, tr("Modular Exponentiation"));
    FUNCTION_NAME(ncr, tr("Combination (Binomial Coefficient)"));
    FUNCTION_NAME(not, tr("Logical NOT"));
    FUNCTION_NAME(npr, tr("Permutation (Arrangement)"));
    FUNCTION_NAME(oct, tr("Convert to Octal Representation"));
    FUNCTION_NAME(octpad, tr("Convert to Padded Octal Representation"));
    FUNCTION_NAME(norm, tr("Vector or Matrix Norm"));
    FUNCTION_NAME(or, tr("Logical OR"));
    FUNCTION_NAME(popcount, tr("Population Count (Hamming Weight)"));
    FUNCTION_NAME(phase, tr("Phase of Complex Number"));
    FUNCTION_NAME(poicdf, tr("Poissonian Cumulative Distribution Function"));
    FUNCTION_NAME(poimean, tr("Poissonian Distribution Mean"));
    FUNCTION_NAME(poipmf, tr("Poissonian Probability Mass Function"));
    FUNCTION_NAME(poivar, tr("Poissonian Distribution Variance"));
    FUNCTION_NAME(polar, tr("Convert to Polar Notation"));
    FUNCTION_NAME(product, tr("Product"));
    FUNCTION_NAME(radians, tr("Radians"));
    FUNCTION_NAME(rank, tr("Matrix Rank"));
    FUNCTION_NAME(real, tr("Real Part"));
    FUNCTION_NAME(round, tr("Rounding"));
    FUNCTION_NAME(sci, tr("Convert to Scientific Notation"));
    FUNCTION_NAME(sec, tr("Secant"));
    FUNCTION_NAME(shl, tr("Arithmetic Shift Left"));
    FUNCTION_NAME(shr, tr("Arithmetic Shift Right"));
    FUNCTION_NAME(sgn, tr("Signum"));
    FUNCTION_NAME(rows, tr("Matrix Row Count"));
    FUNCTION_NAME(shape, tr("List or Matrix Shape"));
    FUNCTION_NAME(summation, tr("Summation"));
    FUNCTION_NAME(sin, tr("Sine"));
    FUNCTION_NAME(sinh, tr("Hyperbolic Sine"));
    FUNCTION_NAME(sqrt, tr("Square Root"));
    FUNCTION_NAME(stdevp, tr("Population Standard Deviation (n)"));
    FUNCTION_NAME(stdevs, tr("Sample Standard Deviation (n-1)"));
    FUNCTION_NAME(sum, tr("Sum"));
    FUNCTION_NAME(tan, tr("Tangent"));
    FUNCTION_NAME(turns, tr("Turns"));
    FUNCTION_NAME(tanh, tr("Hyperbolic Tangent"));
    FUNCTION_NAME(trunc, tr("Truncation"));
    FUNCTION_NAME(trace, tr("Matrix Trace"));
    FUNCTION_NAME(transpose, tr("Transpose Matrix"));
    FUNCTION_NAME(unmask, tr("Sign-extend a value"));
    FUNCTION_NAME(varp, tr("Population Variance (n)"));
    FUNCTION_NAME(vars, tr("Sample Variance (n-1)"));
    FUNCTION_NAME(xor, tr("Logical XOR"));
}

void FunctionRepo::retranslateText()
{
    setFunctionNames();
    setTranslatableFunctionUsages();
}
