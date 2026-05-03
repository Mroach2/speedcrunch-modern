// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/units.h"

#include "core/unicodechars.h"
#include "core/settings.h"
#include "math/hmath.h"
#include "core/mathdsl.h"
#include "math/quantity.h"
#include "math/rational.h"

#include <QString>
#include <QStringList>
#include <QStringView>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <array>

/*
    - Source #1:
        The International System of Units - 9th edition (2019)
        Bureau International des Poids et Mesures
        Published in May 2019 - Revised in August 2025
        https://doi.org/10.59161/AUEZ1291

    - Source #2:
        SI Reference Point
        Bureau International des Poids et Mesures
        https://si-digital-framework.org/SI/units

    - Source #3:
        The Unified Code for Units of Measure
        Regenstrief Institute, Inc. and the UCUM Organization
        Version: 2.2 | Revision: N/A | Date: 2024-06-17
        https://ucum.org/ucum

    - Source #4:
        https://www.nist.gov/system/files/documents/2025/12/30/appc-26-HB44-20251222.pdf

    - Source #5:
        https://qudt.org/vocab/unit/
*/

namespace UnitText {
    inline constexpr QStringView NewtonMetre{u"newton metre", 12};
    inline constexpr QStringView WattSecond{u"watt second", 11};
    inline constexpr QStringView VoltSquaredAmpere{u"volt² ampere", 12};
    inline constexpr QStringView JoulePerSquareMetre{u"joule / metre²", 14};
    inline constexpr QStringView JoulePerCubicMetre{u"joule / metre³", 14};
} // namespace UnitText

namespace UnitName {
    inline const QString Acre = QStringLiteral("acre");
    inline const QString Ampere = QStringLiteral("ampere");
    inline const QString Angstrom = QStringLiteral("angstrom");
    inline const QString Arcminute = QStringLiteral("arcminute");
    inline const QString Arcsecond = QStringLiteral("arcsecond");
    inline const QString Milliarcsecond = QStringLiteral("milliarcsecond");
    inline const QString Microarcsecond = QStringLiteral("microarcsecond");
    inline const QString AstronomicalUnit = QStringLiteral("astronomical_unit");
    inline const QString Atmosphere = QStringLiteral("atmosphere");
    inline const QString AtomicMassUnit = QStringLiteral("atomic_mass_unit");
    inline const QString Bar = QStringLiteral("bar");
    inline const QString Becquerel = QStringLiteral("becquerel");
    inline const QString Bit = QStringLiteral("bit");
    inline const QString BritishThermalUnit = QStringLiteral("british_thermal_unit");
    inline const QString Byte = QStringLiteral("byte");
    inline const QString Calorie = QStringLiteral("calorie");
    inline const QString Candela = QStringLiteral("candela");
    inline const QString Carat = QStringLiteral("carat");
    inline const QString Coulomb = QStringLiteral("coulomb");
    inline const QString CubicMetre = QStringLiteral("cubic_metre");
    inline const QString CubicMillimetre = QStringLiteral("cubic_millimetre");
    inline const QString CubicCentimetre = QStringLiteral("cubic_centimetre");
    inline const QString CubicDecimetre = QStringLiteral("cubic_decimetre");
    inline const QString CubicKilometre = QStringLiteral("cubic_kilometre");
    inline const QString SquareMillimetre = QStringLiteral("square_millimetre");
    inline const QString SquareKilometre = QStringLiteral("square_kilometre");
    inline const QString Cup = QStringLiteral("cup");
    inline const QString CupImp = QStringLiteral("cup_imp");
    inline const QString CupJp = QStringLiteral("cup_jp");
    inline const QString CupUs = QStringLiteral("cup_us");
    inline const QString Dalton = QStringLiteral("dalton");
    inline const QString Day = QStringLiteral("day");
    inline const QString Degree = QStringLiteral("degree");
    inline const QString DegreeCelsius = QStringLiteral("degree_celsius");
    inline const QString DegreeFahrenheit = QStringLiteral("degree_fahrenheit");
    inline const QString Electronvolt = QStringLiteral("electronvolt");
    inline const QString Farad = QStringLiteral("farad");
    inline const QString Fathom = QStringLiteral("fathom");
    inline const QString FluidOunceImp = QStringLiteral("fluid_ounce_imp");
    inline const QString FluidOunceUs = QStringLiteral("fluid_ounce_us");
    inline const QString FluidDramImp = QStringLiteral("fluid_dram_imp");
    inline const QString FluidDramUs = QStringLiteral("fluid_dram_us");
    inline const QString Foot = QStringLiteral("foot");
    inline const QString SquareFoot = QStringLiteral("square_foot");
    inline const QString CubicFoot = QStringLiteral("cubic_foot");
    inline const QString Furlong = QStringLiteral("furlong");
    inline const QString GallonImp = QStringLiteral("gallon_imp");
    inline const QString GallonUs = QStringLiteral("gallon_us");
    inline const QString GillImp = QStringLiteral("gill_imp");
    inline const QString GillUs = QStringLiteral("gill_us");
    inline const QString Gradian = QStringLiteral("gradian");
    inline const QString Grain = QStringLiteral("grain");
    inline const QString Gram = QStringLiteral("gram");
    inline const QString Gray = QStringLiteral("gray");
    inline const QString Hartley = QStringLiteral("hartley");
    inline const QString HartreeEnergyUnit = QStringLiteral("hartree_energy_unit");
    inline const QString Hectare = QStringLiteral("hectare");
    inline const QString Henry = QStringLiteral("henry");
    inline const QString Hertz = QStringLiteral("hertz");
    inline const QString Horsepower = QStringLiteral("horsepower");
    inline const QString Hour = QStringLiteral("hour");
    inline const QString Inch = QStringLiteral("inch");
    inline const QString SquareInch = QStringLiteral("square_inch");
    inline const QString CubicInch = QStringLiteral("cubic_inch");
    inline const QString Joule = QStringLiteral("joule");
    inline const QString Karat = QStringLiteral("karat");
    inline const QString Katal = QStringLiteral("katal");
    inline const QString Kelvin = QStringLiteral("kelvin");
    inline const QString Kilogram = QStringLiteral("kilogram");
    inline const QString Knot = QStringLiteral("knot");
    inline const QString Lightminute = QStringLiteral("lightminute");
    inline const QString Lightsecond = QStringLiteral("lightsecond");
    inline const QString Lightyear = QStringLiteral("lightyear");
    inline const QString Litre = QStringLiteral("litre");
    inline const QString LongTon = QStringLiteral("long_ton");
    inline const QString Lumen = QStringLiteral("lumen");
    inline const QString Lux = QStringLiteral("lux");
    inline const QString Metre = QStringLiteral("metre");
    inline const QString Mile = QStringLiteral("mile");
    inline const QString SquareMile = QStringLiteral("square_mile");
    inline const QString CubicMile = QStringLiteral("cubic_mile");
    inline const QString MilePerHour = QStringLiteral("mile_per_hour");
    inline const QString KilometrePerHour = QStringLiteral("kilometre_per_hour");
    inline const QString Minute = QStringLiteral("minute");
    inline const QString Mole = QStringLiteral("mole");
    inline const QString Nat = QStringLiteral("nat");
    inline const QString NauticalMile = QStringLiteral("nautical_mile");
    inline const QString Newton = QStringLiteral("newton");
    inline const QString Ohm = QStringLiteral("ohm");
    inline const QString Ounce = QStringLiteral("ounce");
    inline const QString Parsec = QStringLiteral("parsec");
    inline const QString Pascal = QStringLiteral("pascal");
    inline const QString PintImp = QStringLiteral("pint_imp");
    inline const QString PintUs = QStringLiteral("pint_us");
    inline const QString Pound = QStringLiteral("pound");
    inline const QString PoundsPerSqinch = QStringLiteral("pounds_per_sqinch");
    inline const QString QuartImp = QStringLiteral("quart_imp");
    inline const QString QuartUs = QStringLiteral("quart_us");
    inline const QString BarrelOil = QStringLiteral("barrel_oil");
    inline const QString BarrelBeerUs = QStringLiteral("barrel_beer_us");
    inline const QString Radian = QStringLiteral("radian");
    inline const QString Rod = QStringLiteral("rod");
    inline const QString SquareYard = QStringLiteral("square_yard");
    inline const QString CubicYard = QStringLiteral("cubic_yard");
    inline const QString Second = QStringLiteral("second");
    inline const QString ShortTon = QStringLiteral("short_ton");
    inline const QString Stone = QStringLiteral("stone");
    inline const QString Siemens = QStringLiteral("siemens");
    inline const QString Sievert = QStringLiteral("sievert");
    inline const QString SquareMetre = QStringLiteral("square_metre");
    inline const QString Steradian = QStringLiteral("steradian");
    inline const QString Tablespoon = QStringLiteral("tablespoon");
    inline const QString TablespoonAu = QStringLiteral("tablespoon_au");
    inline const QString TablespoonImp = QStringLiteral("tablespoon_imp");
    inline const QString TablespoonUs = QStringLiteral("tablespoon_us");
    inline const QString DessertSpoon = QStringLiteral("dessert_spoon");
    inline const QString Teaspoon = QStringLiteral("teaspoon");
    inline const QString TeaspoonImp = QStringLiteral("teaspoon_imp");
    inline const QString TeaspoonUs = QStringLiteral("teaspoon_us");
    inline const QString Tesla = QStringLiteral("tesla");
    inline const QString Tonne = QStringLiteral("tonne");
    inline const QString Torr = QStringLiteral("torr");
    inline const QString Turn = QStringLiteral("turn");
    inline const QString Revolution = QStringLiteral("revolution");
    inline const QString RevolutionPerMinute = QStringLiteral("revolution_per_minute");
    inline const QString Volt = QStringLiteral("volt");
    inline const QString Watt = QStringLiteral("watt");
    inline const QString KilowattHour = QStringLiteral("kilowatt_hour");
    inline const QString MillimetreOfMercury = QStringLiteral("millimetre_of_mercury");
    inline const QString Quad = QStringLiteral("quad");
    inline const QString Weber = QStringLiteral("weber");
    inline const QString Week = QStringLiteral("week");
    inline const QString Yard = QStringLiteral("yard");
    inline const QString CenturyJulian = QStringLiteral("century_julian");
    inline const QString YearJulian = QStringLiteral("year_julian");
    inline const QString YearSidereal = QStringLiteral("year_sidereal");
    inline const QString YearTropical = QStringLiteral("year_tropical");
} // namespace UnitName

namespace UnitSymbol {
    inline const QString Acre = QStringLiteral("ac");
    inline const QString Ampere = QStringLiteral("A");
    inline const QString Angstrom = QStringLiteral("Å");
    inline const QString Arcminute = QStringLiteral("′");
    inline const QString Arcsecond = QStringLiteral("″");
    inline const QString Milliarcsecond = QStringLiteral("mas");
    inline const QString Microarcsecond = QStringLiteral("µas");
    inline const QString AstronomicalUnit = QStringLiteral("au");
    inline const QString Atmosphere = QStringLiteral("atm");
    inline const QString AtomicMassUnit = QStringLiteral("u");
    inline const QString Bar = QStringLiteral("bar");
    inline const QString Becquerel = QStringLiteral("Bq");
    inline const QString Bit = QStringLiteral("b");
    inline const QString BritishThermalUnit = QStringLiteral("Btu");
    inline const QString Byte = QStringLiteral("B");
    inline const QString Calorie = QStringLiteral("cal");
    inline const QString Candela = QStringLiteral("cd");
    inline const QString Carat = QStringLiteral("ct");
    inline const QString Coulomb = QStringLiteral("C");
    inline const QString CubicMetre = QStringLiteral("m³");
    inline const QString CubicMillimetre = QStringLiteral("mm³");
    inline const QString CubicCentimetre = QStringLiteral("cm³");
    inline const QString CubicDecimetre = QStringLiteral("dm³");
    inline const QString CubicKilometre = QStringLiteral("km³");
    inline const QString SquareMillimetre = QStringLiteral("mm²");
    inline const QString SquareKilometre = QStringLiteral("km²");
    inline const QString Cup = QStringLiteral("cup");
    inline const QString CupImp = QStringLiteral("cup_imp");
    inline const QString CupJp = QStringLiteral("cup_jp");
    inline const QString CupUs = QStringLiteral("cup_us");
    inline const QString Dalton = QStringLiteral("Da");
    inline const QString Day = QStringLiteral("d");
    inline const QString Degree = QStringLiteral("°");
    inline const QString DegreeCelsius = QStringLiteral("°C");
    inline const QString DegreeFahrenheit = QStringLiteral("°F");
    inline const QString Electronvolt = QStringLiteral("eV");
    inline const QString Farad = QStringLiteral("F");
    inline const QString Fathom = QStringLiteral("fathom"); // Source #5
    inline const QString FluidOunceImp = QStringLiteral("floz_imp");
    inline const QString FluidOunceUs = QStringLiteral("floz_us");
    inline const QString FluidDramImp = QStringLiteral("fldr_imp");
    inline const QString FluidDramUs = QStringLiteral("fldr_us");
    inline const QString Foot = QStringLiteral("ft");
    inline const QString SquareFoot = QStringLiteral("ft²");
    inline const QString CubicFoot = QStringLiteral("ft³");
    inline const QString Furlong = QStringLiteral("fur"); // Source #4
    inline const QString GallonImp = QStringLiteral("gal_imp");
    inline const QString GallonUs = QStringLiteral("gal_us");
    inline const QString GillImp = QStringLiteral("gill_imp");
    inline const QString GillUs = QStringLiteral("gill_us");
    inline const QString Gradian = QStringLiteral("gon");
    inline const QString Grain = QStringLiteral("gr");
    inline const QString Gram = QStringLiteral("g");
    inline const QString Gray = QStringLiteral("Gy");
    inline const QString Hartley = QStringLiteral("Hart");
    inline const QString HartreeEnergyUnit = QStringLiteral("Eh");
    inline const QString Hectare = QStringLiteral("ha");
    inline const QString Henry = QStringLiteral("H");
    inline const QString Hertz = QStringLiteral("Hz");
    inline const QString Horsepower = QStringLiteral("hp");
    inline const QString Hour = QStringLiteral("h");
    inline const QString Inch = MathDsl::TransOpAl1;
    inline const QString SquareInch = QStringLiteral("in²");
    inline const QString CubicInch = QStringLiteral("in³");
    inline const QString Joule = QStringLiteral("J");
    inline const QString Karat = QStringLiteral("Kt");
    inline const QString Katal = QStringLiteral("kat");
    inline const QString Kelvin = QStringLiteral("K");
    inline const QString Kilogram = QStringLiteral("kg");
    inline const QString Knot = QStringLiteral("kn");
    inline const QString Lightminute = QStringLiteral("lmin");
    inline const QString Lightsecond = QStringLiteral("ls");
    inline const QString Lightyear = QStringLiteral("ly");
    inline const QString Litre = QStringLiteral("L");
    inline const QString LongTon = QStringLiteral("long_ton");
    inline const QString Lumen = QStringLiteral("lm");
    inline const QString Lux = QStringLiteral("lx");
    inline const QString Metre = QStringLiteral("m");
    inline const QString Mile = QStringLiteral("mi");
    inline const QString SquareMile = QStringLiteral("mi²");
    inline const QString CubicMile = QStringLiteral("mi³");
    inline const QString MilePerHour = QStringLiteral("mph");
    inline const QString KilometrePerHour = QStringLiteral("kph");
    inline const QString Minute = QStringLiteral("min");
    inline const QString Mole = QStringLiteral("mol");
    inline const QString Nat = QStringLiteral("nat");
    inline const QString NauticalMile = QStringLiteral("nmi");
    inline const QString Newton = QStringLiteral("N");
    inline const QString Ohm = QStringLiteral("Ω");
    inline const QString Ounce = QStringLiteral("oz");
    inline const QString Parsec = QStringLiteral("pc");
    inline const QString Pascal = QStringLiteral("Pa");
    inline const QString PintImp = QStringLiteral("pt_imp");
    inline const QString PintUs = QStringLiteral("pt_us");
    inline const QString Pound = QStringLiteral("lb");
    inline const QString PoundsPerSqinch = QStringLiteral("psi");
    inline const QString QuartImp = QStringLiteral("qt_imp");
    inline const QString QuartUs = QStringLiteral("qt_us");
    inline const QString BarrelOil = QStringLiteral("bbl_oil");
    inline const QString BarrelBeerUs = QStringLiteral("bbl_beer");
    inline const QString Radian = QStringLiteral("rad");
    inline const QString Rod = QStringLiteral("rd");  // Source #4
    inline const QString SquareYard = QStringLiteral("yd²");
    inline const QString CubicYard = QStringLiteral("yd³");
    inline const QString Second = QStringLiteral("s");
    inline const QString ShortTon = QStringLiteral("tn"); // Source #4
    inline const QString Stone = QStringLiteral("st");
    inline const QString Siemens = QStringLiteral("S");
    inline const QString Sievert = QStringLiteral("Sv");
    inline const QString SquareMetre = QStringLiteral("m²");
    inline const QString Steradian = QStringLiteral("sr");
    inline const QString Tablespoon = QStringLiteral("tbsp");
    inline const QString TablespoonAu = QStringLiteral("tbsp_au");
    inline const QString TablespoonImp = QStringLiteral("tbsp_imp");
    inline const QString TablespoonUs = QStringLiteral("tbsp_us");
    inline const QString DessertSpoon = QStringLiteral("dsp");
    inline const QString Teaspoon = QStringLiteral("tsp");
    inline const QString TeaspoonImp = QStringLiteral("tsp_imp");
    inline const QString TeaspoonUs = QStringLiteral("tsp_us");
    inline const QString Tesla = QStringLiteral("T");
    inline const QString Tonne = QStringLiteral("t");
    inline const QString Torr = QStringLiteral("Torr");
    inline const QString Turn = QStringLiteral("tr");
    inline const QString Revolution = QStringLiteral("rev");
    inline const QString RevolutionPerMinute = QStringLiteral("rpm");
    inline const QString Volt = QStringLiteral("V");
    inline const QString Watt = QStringLiteral("W");
    inline const QString KilowattHour = QStringLiteral("kWh");
    inline const QString MillimetreOfMercury = QStringLiteral("mmHg");
    inline const QString Quad = QStringLiteral("quad");
    inline const QString Weber = QStringLiteral("Wb");
    inline const QString Week = QStringLiteral("wk");
    inline const QString Yard = QStringLiteral("yd");
    inline const QString CenturyJulian = QStringLiteral("cy_jul");
    inline const QString YearJulian = QStringLiteral("a_jul");
    inline const QString YearSidereal = QStringLiteral("a_sid");
    inline const QString YearTropical = QStringLiteral("a_trop");
} // namespace UnitSymbol

namespace UnitAltSymbol {
    inline const QString Arcminute = QStringLiteral("arcmin");
    inline const QString Arcsecond = QStringLiteral("arcsec");
    inline const QString Degree1 = QStringLiteral("deg");
    inline const QString Degree2 = QStringLiteral("˚");
    inline const QString DegreeCelsius1 = QStringLiteral("ºC");
    inline const QString DegreeCelsius2 = QStringLiteral("oC");
    inline const QString DegreeCelsius3 = QStringLiteral("degC");
    inline const QString DegreeCelsius4 = QStringLiteral("Cel");
    inline const QString DegreeCelsius5 = QStringLiteral("˚C");
    inline const QString DegreeFahrenheit1 = QStringLiteral("ºF");
    inline const QString DegreeFahrenheit2 = QStringLiteral("oF");
    inline const QString DegreeFahrenheit3 = QStringLiteral("degF");
    inline const QString DegreeFahrenheit4 = QStringLiteral("Fah");
    inline const QString DegreeFahrenheit5 = QStringLiteral("˚F");
    inline const QString MicroarcsecondAscii = QStringLiteral("uas");
    inline const QString CubicCentimetre = QStringLiteral("cc");
    inline const QString Litre = QStringLiteral("l");
    inline const QString Turn = QStringLiteral("pla");
} // namespace UnitAltSymbol

struct UnitRegistry;

namespace {
char s_runtimeAngleModeForUnits = 'r';

QString composeAnglePerSecondUnitName(const QString& angleUnitName)
{
    const QString secondName = unitName(UnitId::Second);
    if (runtimeUnitNegativeExponentStyle() == Settings::UnitNegativeExponentSuperscript) {
        return angleUnitName + QLatin1Char(' ')
               + secondName
               + QString(MathDsl::PowNeg)
               + QString(MathDsl::Pow1);
    }
    return angleUnitName + QStringLiteral(" / ") + secondName;
}

QString toSuperscriptExponent(int exponent)
{
    QString result;
    if (exponent < 0)
        result += MathDsl::PowNeg;
    for (const QChar& digit : QString::number(qAbs(exponent)))
        result += MathDsl::asciiDigitToSuperscript(digit);
    return result;
}

int parseTrailingExponent(const QString& factor, int* exponentStart, bool* ok)
{
    *ok = false;
    *exponentStart = factor.size();
    if (factor.isEmpty())
        return 0;

    int suffixStart = factor.size();
    while (suffixStart > 0) {
        const QChar ch = factor.at(suffixStart - 1);
        if (!MathDsl::isSuperscriptDigit(ch) && !MathDsl::isSuperscriptSign(ch))
            break;
        --suffixStart;
    }
    if (suffixStart < factor.size()) {
        bool negative = false;
        int pos = suffixStart;
        if (factor.at(pos) == MathDsl::PowNeg || factor.at(pos) == MathDsl::PowPos) {
            negative = factor.at(pos) == MathDsl::PowNeg;
            ++pos;
        }
        if (pos >= factor.size())
            return 0;

        QString asciiDigits;
        for (; pos < factor.size(); ++pos) {
            const QChar ascii = MathDsl::superscriptDigitToAscii(factor.at(pos));
            if (ascii.isNull())
                return 0;
            asciiDigits += ascii;
        }
        bool intOk = false;
        const int value = asciiDigits.toInt(&intOk);
        if (!intOk)
            return 0;
        *ok = true;
        *exponentStart = suffixStart;
        return negative ? -value : value;
    }

    const int caret = factor.lastIndexOf(MathDsl::PowOp);
    if (caret >= 0 && caret + 1 < factor.size()) {
        const QString expText = factor.mid(caret + 1).trimmed();
        bool intOk = false;
        const int value = expText.toInt(&intOk);
        if (intOk) {
            *ok = true;
            *exponentStart = caret;
            return value;
        }
    }

    return 0;
}

QString negateUnitFactorExponent(const QString& factor)
{
    const QString trimmed = factor.trimmed();
    if (trimmed.isEmpty())
        return trimmed;

    int exponentStart = trimmed.size();
    bool hasExponent = false;
    const int exponent = parseTrailingExponent(trimmed, &exponentStart, &hasExponent);
    if (hasExponent)
        return trimmed.left(exponentStart) + toSuperscriptExponent(-exponent);
    return trimmed + toSuperscriptExponent(-1);
}

QString applySuperscriptStyleToUnitName(QString unitName)
{
    if (!unitName.contains(MathDsl::DivOp))
        return unitName;

    QStringList parts = unitName.split(MathDsl::DivOp);
    if (parts.isEmpty())
        return unitName;

    QString normalized = parts.takeFirst().trimmed();
    for (QString denominator : parts) {
        denominator = denominator.trimmed();
        if (denominator.startsWith(MathDsl::GroupStart)
            && denominator.endsWith(MathDsl::GroupEnd)
            && denominator.size() > 2)
        {
            denominator = denominator.mid(1, denominator.size() - 2).trimmed();
        }

        const QStringList factors = denominator.split(
            QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        for (const QString& rawFactor : factors) {
            const QString negated = negateUnitFactorExponent(rawFactor);
            if (negated.isEmpty())
                continue;
            if (!normalized.isEmpty())
                normalized += QLatin1Char(' ');
            normalized += negated;
        }
    }

    return normalized;
}
    const UnitRegistry& s_unitRegistry();

    enum SiPrefixPolicy {
        NoSiPrefixes = 0,
        PositiveSiPrefixes = 1 << 0,
        NegativeSiPrefixes = 1 << 1,
        BinaryPrefixes = 1 << 2
    };
    static constexpr SiPrefixPolicy AllSiPrefixes =
        static_cast<SiPrefixPolicy>(PositiveSiPrefixes | NegativeSiPrefixes);

    enum class UnitFamily {
        SiBase,
        SiDerived,
        SiAccepted,
        Other
    };

    struct UnitAliasSpec {
        QString longName;
        QString localized;
        Quantity value;
        QString shortName;
        QString alternateShortName;
        SiPrefixPolicy siPrefixPolicy;
    };

    struct AffineDefinition {
        // Affine conversion handlers only transform numeric offsets/scales.
        // Unit dimensions for identifiers still come from UnitSpec::linearValue
        // (e.g. degree_celsius/degree_fahrenheit use Units::kelvin).
        HNumber (*toBase)(const HNumber&) = nullptr;
        HNumber (*fromBase)(const HNumber&) = nullptr;
    };

    struct UnitSpec {
        QString name;
        const char* localized = "";
        QString symbol;
        QList<QString> aliases;
        UnitFamily family;
        QList<UnitQuantity> quantities;
        SiPrefixPolicy siPrefixPolicy;
        const Quantity (*linearValue)();
        AffineDefinition affine = {};
    };

    struct PrefixSpec {
        PrefixId id;
        QString longName;
        QString symbol;
        int power;
        Quantity value;
    };
} // namespace

HNumber s_celsiusToKelvin(const HNumber& c);
HNumber s_kelvinToCelsius(const HNumber& k);
HNumber s_fahrenheitToKelvin(const HNumber& f);
HNumber s_kelvinToFahrenheit(const HNumber& k);
const Quantity s_hartreeEnergyUnit();

const QHash<UnitId, UnitSpec>& s_unitSpecs()
{
    // Single source of truth for built-in units.
    // See Sources at top of this file.
    static const QHash<UnitId, UnitSpec> specs = {
        // SI base units
        {UnitId::Second, UnitSpec{UnitName::Second, QT_TR_NOOP("second"), UnitSymbol::Second, {}, UnitFamily::SiBase, {UnitQuantity::Time}, AllSiPrefixes, &Units::second}},
        {UnitId::Metre, UnitSpec{UnitName::Metre, QT_TR_NOOP("metre"), UnitSymbol::Metre, {}, UnitFamily::SiBase, {UnitQuantity::Length}, AllSiPrefixes, &Units::metre}},
        {UnitId::Kilogram, UnitSpec{UnitName::Kilogram, QT_TR_NOOP("kilogram"), UnitSymbol::Kilogram, {}, UnitFamily::SiBase, {UnitQuantity::Mass}, AllSiPrefixes, &Units::kilogram}},
        {UnitId::Ampere, UnitSpec{UnitName::Ampere, QT_TR_NOOP("Ampere"), UnitSymbol::Ampere, {}, UnitFamily::SiBase, {UnitQuantity::ElectricCurrent}, AllSiPrefixes, &Units::ampere}},
        {UnitId::Kelvin, UnitSpec{UnitName::Kelvin, QT_TR_NOOP("Kelvin"), UnitSymbol::Kelvin, {}, UnitFamily::SiBase, {UnitQuantity::ThermodynamicTemperature}, AllSiPrefixes, &Units::kelvin}},
        {UnitId::Mole, UnitSpec{UnitName::Mole, QT_TR_NOOP("mole"), UnitSymbol::Mole, {}, UnitFamily::SiBase, {UnitQuantity::AmountOfSubstance}, AllSiPrefixes, &Units::mole}},
        {UnitId::Candela, UnitSpec{UnitName::Candela, QT_TR_NOOP("candela"), UnitSymbol::Candela, {}, UnitFamily::SiBase, {UnitQuantity::LuminousIntensity}, AllSiPrefixes, &Units::candela}},
        // SI derived units
        {UnitId::Radian, UnitSpec{UnitName::Radian, QT_TR_NOOP("radian"), UnitSymbol::Radian, {}, UnitFamily::SiDerived, {UnitQuantity::PlaneAngle}, AllSiPrefixes, &Units::radian}},
        {UnitId::Steradian, UnitSpec{UnitName::Steradian, QT_TR_NOOP("steradian"), UnitSymbol::Steradian, {}, UnitFamily::SiDerived, {UnitQuantity::SolidAngle}, AllSiPrefixes, &Units::steradian}},
        {UnitId::Hertz, UnitSpec{UnitName::Hertz, QT_TR_NOOP("Hertz"), UnitSymbol::Hertz, {}, UnitFamily::SiDerived, {UnitQuantity::Frequency}, AllSiPrefixes, &Units::hertz}},
        {UnitId::Newton, UnitSpec{UnitName::Newton, QT_TR_NOOP("Newton"), UnitSymbol::Newton, {}, UnitFamily::SiDerived, {UnitQuantity::Force}, AllSiPrefixes, &Units::newton}},
        {UnitId::Pascal, UnitSpec{UnitName::Pascal, QT_TR_NOOP("Pascal"), UnitSymbol::Pascal, {}, UnitFamily::SiDerived, {UnitQuantity::Pressure, UnitQuantity::Stress}, AllSiPrefixes, &Units::pascal}},
        {UnitId::Joule, UnitSpec{UnitName::Joule, QT_TR_NOOP("Joule"), UnitSymbol::Joule, {}, UnitFamily::SiDerived, {UnitQuantity::Energy, UnitQuantity::Work, UnitQuantity::AmountOfHeat}, AllSiPrefixes, &Units::joule}},
        {UnitId::Watt, UnitSpec{UnitName::Watt, QT_TR_NOOP("Watt"), UnitSymbol::Watt, {}, UnitFamily::SiDerived, {UnitQuantity::Power, UnitQuantity::RadiantFlux}, AllSiPrefixes, &Units::watt}},
        {UnitId::Coulomb, UnitSpec{UnitName::Coulomb, QT_TR_NOOP("Coulomb"), UnitSymbol::Coulomb, {}, UnitFamily::SiDerived, {UnitQuantity::ElectricCharge}, AllSiPrefixes, &Units::coulomb}},
        {UnitId::Volt, UnitSpec{UnitName::Volt, QT_TR_NOOP("Volt"), UnitSymbol::Volt, {}, UnitFamily::SiDerived, {UnitQuantity::ElectricPotentialDifference}, AllSiPrefixes, &Units::volt}},
        {UnitId::Farad, UnitSpec{UnitName::Farad, QT_TR_NOOP("Farad"), UnitSymbol::Farad, {}, UnitFamily::SiDerived, {UnitQuantity::Capacitance}, AllSiPrefixes, &Units::farad}},
        {UnitId::Ohm, UnitSpec{UnitName::Ohm, QT_TR_NOOP("Ohm"), UnitSymbol::Ohm, {}, UnitFamily::SiDerived, {UnitQuantity::ElectricResistance}, AllSiPrefixes, &Units::ohm}},
        {UnitId::Siemens, UnitSpec{UnitName::Siemens, QT_TR_NOOP("Siemens"), UnitSymbol::Siemens, {}, UnitFamily::SiDerived, {UnitQuantity::ElectricConductance}, AllSiPrefixes, &Units::siemens}},
        {UnitId::Weber, UnitSpec{UnitName::Weber, QT_TR_NOOP("Weber"), UnitSymbol::Weber, {}, UnitFamily::SiDerived, {UnitQuantity::MagneticFlux}, AllSiPrefixes, &Units::weber}},
        {UnitId::Tesla, UnitSpec{UnitName::Tesla, QT_TR_NOOP("Tesla"), UnitSymbol::Tesla, {}, UnitFamily::SiDerived, {UnitQuantity::MagneticFluxDensity}, AllSiPrefixes, &Units::tesla}},
        {UnitId::Henry, UnitSpec{UnitName::Henry, QT_TR_NOOP("Henry"), UnitSymbol::Henry, {}, UnitFamily::SiDerived, {UnitQuantity::Inductance}, AllSiPrefixes, &Units::henry}},
        {UnitId::DegreeCelsius, UnitSpec{UnitName::DegreeCelsius, QT_TR_NOOP("degree Celsius"), UnitSymbol::DegreeCelsius, {UnitAltSymbol::DegreeCelsius1, UnitAltSymbol::DegreeCelsius2, UnitAltSymbol::DegreeCelsius3, UnitAltSymbol::DegreeCelsius4, UnitAltSymbol::DegreeCelsius5}, UnitFamily::SiAccepted, {UnitQuantity::CelsiusTemperature}, NoSiPrefixes, &Units::kelvin, {&s_celsiusToKelvin, &s_kelvinToCelsius}}},
        {UnitId::Lumen, UnitSpec{UnitName::Lumen, QT_TR_NOOP("lumen"), UnitSymbol::Lumen, {}, UnitFamily::SiDerived, {UnitQuantity::LuminousFlux}, AllSiPrefixes, &Units::lumen}},
        {UnitId::Lux, UnitSpec{UnitName::Lux, QT_TR_NOOP("lux"), UnitSymbol::Lux, {}, UnitFamily::SiDerived, {UnitQuantity::Illuminance}, AllSiPrefixes, &Units::lux}},
        {UnitId::Becquerel, UnitSpec{UnitName::Becquerel, QT_TR_NOOP("Becquerel"), UnitSymbol::Becquerel, {}, UnitFamily::SiDerived, {UnitQuantity::ActivityReferredToARadionuclide}, AllSiPrefixes, &Units::becquerel}},
        {UnitId::Gray, UnitSpec{UnitName::Gray, QT_TR_NOOP("Gray"), UnitSymbol::Gray, {}, UnitFamily::SiDerived, {UnitQuantity::AbsorbedDose, UnitQuantity::Kerma}, AllSiPrefixes, &Units::gray}},
        {UnitId::Sievert, UnitSpec{UnitName::Sievert, QT_TR_NOOP("sievert"), UnitSymbol::Sievert, {}, UnitFamily::SiDerived, {UnitQuantity::DoseEquivalent}, AllSiPrefixes, &Units::sievert}},
        {UnitId::Katal, UnitSpec{UnitName::Katal, QT_TR_NOOP("katal"), UnitSymbol::Katal, {}, UnitFamily::SiDerived, {UnitQuantity::CatalyticActivity}, AllSiPrefixes, &Units::katal}},
        {UnitId::CubicMetre, UnitSpec{UnitName::CubicMetre, QT_TR_NOOP("cubic metre"), UnitSymbol::CubicMetre, {}, UnitFamily::SiDerived, {UnitQuantity::Volume}, AllSiPrefixes, &Units::cubic_metre}},
        {UnitId::CubicMillimetre, UnitSpec{UnitName::CubicMillimetre, QT_TR_NOOP("cubic millimetre"), UnitSymbol::CubicMillimetre, {}, UnitFamily::SiDerived, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cubic_millimetre}},
        {UnitId::CubicCentimetre, UnitSpec{UnitName::CubicCentimetre, QT_TR_NOOP("cubic centimetre"), UnitSymbol::CubicCentimetre, {UnitAltSymbol::CubicCentimetre}, UnitFamily::SiDerived, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cubic_centimetre}},
        {UnitId::CubicDecimetre, UnitSpec{UnitName::CubicDecimetre, QT_TR_NOOP("cubic decimetre"), UnitSymbol::CubicDecimetre, {}, UnitFamily::SiDerived, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cubic_decimetre}},
        {UnitId::CubicKilometre, UnitSpec{UnitName::CubicKilometre, QT_TR_NOOP("cubic kilometre"), UnitSymbol::CubicKilometre, {}, UnitFamily::SiDerived, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cubic_kilometre}},
        {UnitId::SquareMillimetre, UnitSpec{UnitName::SquareMillimetre, QT_TR_NOOP("square millimetre"), UnitSymbol::SquareMillimetre, {}, UnitFamily::SiDerived, {UnitQuantity::Area}, NoSiPrefixes, &Units::square_millimetre}},
        {UnitId::SquareKilometre, UnitSpec{UnitName::SquareKilometre, QT_TR_NOOP("square kilometre"), UnitSymbol::SquareKilometre, {}, UnitFamily::SiDerived, {UnitQuantity::Area}, NoSiPrefixes, &Units::square_kilometre}},
        {UnitId::SquareMetre, UnitSpec{UnitName::SquareMetre, QT_TR_NOOP("square metre"), UnitSymbol::SquareMetre, {}, UnitFamily::SiDerived, {UnitQuantity::Area}, AllSiPrefixes, &Units::square_metre}},
        // SI-accepted units
        {UnitId::Minute, UnitSpec{UnitName::Minute, QT_TR_NOOP("minute"), UnitSymbol::Minute, {}, UnitFamily::SiAccepted, {UnitQuantity::Time}, NoSiPrefixes, &Units::minute}},
        {UnitId::Hour, UnitSpec{UnitName::Hour, QT_TR_NOOP("hour"), UnitSymbol::Hour, {}, UnitFamily::SiAccepted, {UnitQuantity::Time}, NoSiPrefixes, &Units::hour}},
        {UnitId::Day, UnitSpec{UnitName::Day, QT_TR_NOOP("day"), UnitSymbol::Day, {}, UnitFamily::SiAccepted, {UnitQuantity::Time}, NoSiPrefixes, &Units::day}},
        {UnitId::AstronomicalUnit, UnitSpec{UnitName::AstronomicalUnit, QT_TR_NOOP("astronomical unit"), UnitSymbol::AstronomicalUnit, {}, UnitFamily::SiAccepted, {UnitQuantity::Length}, NoSiPrefixes, &Units::astronomical_unit}},
        {UnitId::Degree, UnitSpec{UnitName::Degree, QT_TR_NOOP("degree"), UnitSymbol::Degree, {UnitAltSymbol::Degree1, UnitAltSymbol::Degree2}, UnitFamily::SiAccepted, {UnitQuantity::PlaneAngle}, NoSiPrefixes, &Units::degree}},
        {UnitId::Arcminute, UnitSpec{UnitName::Arcminute, QT_TR_NOOP("arcminute"), UnitSymbol::Arcminute, {UnitAltSymbol::Arcminute}, UnitFamily::SiAccepted, {UnitQuantity::PlaneAngle}, NoSiPrefixes, &Units::arcminute}},
        {UnitId::Arcsecond, UnitSpec{UnitName::Arcsecond, QT_TR_NOOP("arcsecond"), UnitSymbol::Arcsecond, {UnitAltSymbol::Arcsecond}, UnitFamily::SiAccepted, {UnitQuantity::PlaneAngle}, NoSiPrefixes, &Units::arcsecond}},
        {UnitId::Milliarcsecond, UnitSpec{UnitName::Milliarcsecond, QT_TR_NOOP("milliarcsecond"), UnitSymbol::Milliarcsecond, {}, UnitFamily::SiAccepted, {UnitQuantity::PlaneAngle}, NoSiPrefixes, &Units::milliarcsecond}},
        {UnitId::Microarcsecond, UnitSpec{UnitName::Microarcsecond, QT_TR_NOOP("microarcsecond"), UnitSymbol::Microarcsecond, {UnitAltSymbol::MicroarcsecondAscii}, UnitFamily::SiAccepted, {UnitQuantity::PlaneAngle}, NoSiPrefixes, &Units::microarcsecond}},
        {UnitId::Hectare, UnitSpec{UnitName::Hectare, QT_TR_NOOP("hectare"), UnitSymbol::Hectare, {}, UnitFamily::SiAccepted, {UnitQuantity::Area}, NoSiPrefixes, &Units::hectare}},
        {UnitId::Litre, UnitSpec{UnitName::Litre, QT_TR_NOOP("litre"), UnitSymbol::Litre, {UnitAltSymbol::Litre}, UnitFamily::SiAccepted, {UnitQuantity::Volume}, AllSiPrefixes, &Units::litre}},
        {UnitId::Tonne, UnitSpec{UnitName::Tonne, QT_TR_NOOP("tonne"), UnitSymbol::Tonne, {}, UnitFamily::SiAccepted, {UnitQuantity::Mass}, AllSiPrefixes, &Units::tonne}},
        {UnitId::Dalton, UnitSpec{UnitName::Dalton, QT_TR_NOOP("Dalton"), UnitSymbol::Dalton, {}, UnitFamily::SiAccepted, {UnitQuantity::Mass}, NoSiPrefixes, &Units::atomic_mass_unit}},
        {UnitId::Electronvolt, UnitSpec{UnitName::Electronvolt, QT_TR_NOOP("electronvolt"), UnitSymbol::Electronvolt, {}, UnitFamily::SiAccepted, {UnitQuantity::Energy}, AllSiPrefixes, &Units::electronvolt}},

        {UnitId::Acre, UnitSpec{UnitName::Acre, QT_TR_NOOP("acre"), UnitSymbol::Acre, {}, UnitFamily::Other, {UnitQuantity::Area}, NoSiPrefixes, &Units::acre}},
        {UnitId::Angstrom, UnitSpec{UnitName::Angstrom, QT_TR_NOOP("Angstrom"), UnitSymbol::Angstrom, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::angstrom}},
        {UnitId::Atmosphere, UnitSpec{UnitName::Atmosphere, QT_TR_NOOP("atmosphere"), UnitSymbol::Atmosphere, {}, UnitFamily::Other, {UnitQuantity::Pressure}, NoSiPrefixes, &Units::atmosphere}},
        {UnitId::AtomicMassUnit, UnitSpec{UnitName::AtomicMassUnit, QT_TR_NOOP("atomic mass unit"), UnitSymbol::AtomicMassUnit, {}, UnitFamily::Other, {UnitQuantity::Mass}, NoSiPrefixes, &Units::atomic_mass_unit}},
        {UnitId::Bar, UnitSpec{UnitName::Bar, QT_TR_NOOP("bar"), UnitSymbol::Bar, {}, UnitFamily::Other, {UnitQuantity::Pressure}, AllSiPrefixes, &Units::bar}},
        {UnitId::Bit, UnitSpec{UnitName::Bit, QT_TR_NOOP("bit"), UnitSymbol::Bit, {}, UnitFamily::Other, {UnitQuantity::Information}, static_cast<SiPrefixPolicy>(PositiveSiPrefixes | BinaryPrefixes), &Units::bit}},
        {UnitId::BritishThermalUnit, UnitSpec{UnitName::BritishThermalUnit, QT_TR_NOOP("British thermal unit"), UnitSymbol::BritishThermalUnit, {}, UnitFamily::Other, {UnitQuantity::AmountOfHeat}, NoSiPrefixes, &Units::british_thermal_unit}},
        {UnitId::Byte, UnitSpec{UnitName::Byte, QT_TR_NOOP("byte"), UnitSymbol::Byte, {}, UnitFamily::Other, {UnitQuantity::Information}, static_cast<SiPrefixPolicy>(PositiveSiPrefixes | BinaryPrefixes), &Units::byte}},
        {UnitId::Calorie, UnitSpec{UnitName::Calorie, QT_TR_NOOP("calorie"), UnitSymbol::Calorie, {}, UnitFamily::Other, {UnitQuantity::AmountOfHeat}, NoSiPrefixes, &Units::calorie}},
        {UnitId::Carat, UnitSpec{UnitName::Carat, QT_TR_NOOP("carat"), UnitSymbol::Carat, {}, UnitFamily::Other, {UnitQuantity::Mass}, NoSiPrefixes, &Units::carat}},
        {UnitId::Cup, UnitSpec{UnitName::Cup, QT_TR_NOOP("cup"), UnitSymbol::Cup, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cup}},
        {UnitId::CupImp, UnitSpec{UnitName::CupImp, QT_TR_NOOP("cup (Imperial)"), UnitSymbol::CupImp, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cup_imp}},
        {UnitId::CupJp, UnitSpec{UnitName::CupJp, QT_TR_NOOP("cup (Japanese)"), UnitSymbol::CupJp, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cup_jp}},
        {UnitId::CupUs, UnitSpec{UnitName::CupUs, QT_TR_NOOP("cup (US)"), UnitSymbol::CupUs, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cup_us}},
        {UnitId::DegreeFahrenheit, UnitSpec{UnitName::DegreeFahrenheit, QT_TR_NOOP("degree Fahrenheit"), UnitSymbol::DegreeFahrenheit, {UnitAltSymbol::DegreeFahrenheit1, UnitAltSymbol::DegreeFahrenheit2, UnitAltSymbol::DegreeFahrenheit3, UnitAltSymbol::DegreeFahrenheit4, UnitAltSymbol::DegreeFahrenheit5}, UnitFamily::Other, {UnitQuantity::CelsiusTemperature}, NoSiPrefixes, &Units::kelvin, {&s_fahrenheitToKelvin, &s_kelvinToFahrenheit}}},
        {UnitId::Fathom, UnitSpec{UnitName::Fathom, QT_TR_NOOP("fathom"), UnitSymbol::Fathom, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::fathom}},
        {UnitId::FluidOunceImp, UnitSpec{UnitName::FluidOunceImp, QT_TR_NOOP("fluid ounce (Imperial)"), UnitSymbol::FluidOunceImp, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::fluid_ounce_imp}},
        {UnitId::FluidOunceUs, UnitSpec{UnitName::FluidOunceUs, QT_TR_NOOP("fluid ounce (US)"), UnitSymbol::FluidOunceUs, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::fluid_ounce_us}},
        {UnitId::FluidDramImp, UnitSpec{UnitName::FluidDramImp, QT_TR_NOOP("fluid dram (Imperial)"), UnitSymbol::FluidDramImp, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::fluid_dram_imp}},
        {UnitId::FluidDramUs, UnitSpec{UnitName::FluidDramUs, QT_TR_NOOP("fluid dram (US)"), UnitSymbol::FluidDramUs, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::fluid_dram_us}},
        {UnitId::Foot, UnitSpec{UnitName::Foot, QT_TR_NOOP("foot"), UnitSymbol::Foot, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::foot}},
        {UnitId::SquareFoot, UnitSpec{UnitName::SquareFoot, QT_TR_NOOP("square foot"), UnitSymbol::SquareFoot, {}, UnitFamily::Other, {UnitQuantity::Area}, NoSiPrefixes, &Units::square_foot}},
        {UnitId::CubicFoot, UnitSpec{UnitName::CubicFoot, QT_TR_NOOP("cubic foot"), UnitSymbol::CubicFoot, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cubic_foot}},
        {UnitId::Furlong, UnitSpec{UnitName::Furlong, QT_TR_NOOP("furlong"), UnitSymbol::Furlong, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::furlong}},
        {UnitId::GallonImp, UnitSpec{UnitName::GallonImp, QT_TR_NOOP("gallon (Imperial)"), UnitSymbol::GallonImp, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::gallon_imp}},
        {UnitId::GallonUs, UnitSpec{UnitName::GallonUs, QT_TR_NOOP("gallon (US)"), UnitSymbol::GallonUs, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::gallon_us}},
        {UnitId::GillImp, UnitSpec{UnitName::GillImp, QT_TR_NOOP("gill (Imperial)"), UnitSymbol::GillImp, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::gill_imp}},
        {UnitId::GillUs, UnitSpec{UnitName::GillUs, QT_TR_NOOP("gill (US)"), UnitSymbol::GillUs, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::gill_us}},
        {UnitId::Gradian, UnitSpec{UnitName::Gradian, QT_TR_NOOP("gradian"), UnitSymbol::Gradian, {}, UnitFamily::Other, {UnitQuantity::PlaneAngle}, NoSiPrefixes, &Units::gradian}},
        {UnitId::Grain, UnitSpec{UnitName::Grain, QT_TR_NOOP("grain"), UnitSymbol::Grain, {}, UnitFamily::Other, {UnitQuantity::Mass}, NoSiPrefixes, &Units::grain}},
        {UnitId::Gram, UnitSpec{UnitName::Gram, QT_TR_NOOP("gram"), UnitSymbol::Gram, {}, UnitFamily::Other, {UnitQuantity::Mass}, AllSiPrefixes, &Units::gram}},
        {UnitId::Hartley, UnitSpec{UnitName::Hartley, QT_TR_NOOP("Hartley"), UnitSymbol::Hartley, {}, UnitFamily::Other, {UnitQuantity::Information}, NoSiPrefixes, &Units::hartley}},
        {UnitId::HartreeEnergyUnit, UnitSpec{UnitName::HartreeEnergyUnit, QT_TR_NOOP("Hartree energy unit"), UnitSymbol::HartreeEnergyUnit, {}, UnitFamily::Other, {UnitQuantity::Energy}, NoSiPrefixes, &s_hartreeEnergyUnit}},
        {UnitId::Horsepower, UnitSpec{UnitName::Horsepower, QT_TR_NOOP("horsepower"), UnitSymbol::Horsepower, {}, UnitFamily::Other, {UnitQuantity::Power}, NoSiPrefixes, &Units::horsepower}},
        {UnitId::Inch, UnitSpec{UnitName::Inch, QT_TR_NOOP("inch"), UnitSymbol::Inch, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::inch}},
        {UnitId::SquareInch, UnitSpec{UnitName::SquareInch, QT_TR_NOOP("square inch"), UnitSymbol::SquareInch, {}, UnitFamily::Other, {UnitQuantity::Area}, NoSiPrefixes, &Units::square_inch}},
        {UnitId::CubicInch, UnitSpec{UnitName::CubicInch, QT_TR_NOOP("cubic inch"), UnitSymbol::CubicInch, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cubic_inch}},
        {UnitId::Karat, UnitSpec{UnitName::Karat, QT_TR_NOOP("karat"), UnitSymbol::Karat, {}, UnitFamily::Other, {UnitQuantity::Mass}, NoSiPrefixes, &Units::karat}},
        {UnitId::Knot, UnitSpec{UnitName::Knot, QT_TR_NOOP("knot"), UnitSymbol::Knot, {}, UnitFamily::Other, {UnitQuantity::Speed, UnitQuantity::Velocity}, NoSiPrefixes, &Units::knot}},
        {UnitId::Lightminute, UnitSpec{UnitName::Lightminute, QT_TR_NOOP("lightminute"), UnitSymbol::Lightminute, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::lightminute}},
        {UnitId::Lightsecond, UnitSpec{UnitName::Lightsecond, QT_TR_NOOP("lightsecond"), UnitSymbol::Lightsecond, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::lightsecond}},
        {UnitId::Lightyear, UnitSpec{UnitName::Lightyear, QT_TR_NOOP("lightyear"), UnitSymbol::Lightyear, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::lightyear}},
        {UnitId::LongTon, UnitSpec{UnitName::LongTon, QT_TR_NOOP("long ton"), UnitSymbol::LongTon, {}, UnitFamily::Other, {UnitQuantity::Mass}, NoSiPrefixes, &Units::long_ton}},
        {UnitId::Mile, UnitSpec{UnitName::Mile, QT_TR_NOOP("mile"), UnitSymbol::Mile, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::mile}},
        {UnitId::SquareMile, UnitSpec{UnitName::SquareMile, QT_TR_NOOP("square mile"), UnitSymbol::SquareMile, {}, UnitFamily::Other, {UnitQuantity::Area}, NoSiPrefixes, &Units::square_mile}},
        {UnitId::CubicMile, UnitSpec{UnitName::CubicMile, QT_TR_NOOP("cubic mile"), UnitSymbol::CubicMile, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cubic_mile}},
        {UnitId::MilePerHour, UnitSpec{UnitName::MilePerHour, QT_TR_NOOP("mile per hour"), UnitSymbol::MilePerHour, {}, UnitFamily::Other, {UnitQuantity::Speed, UnitQuantity::Velocity}, NoSiPrefixes, &Units::mile_per_hour}},
        {UnitId::KilometrePerHour, UnitSpec{UnitName::KilometrePerHour, QT_TR_NOOP("kilometre per hour"), UnitSymbol::KilometrePerHour, {}, UnitFamily::Other, {UnitQuantity::Speed, UnitQuantity::Velocity}, NoSiPrefixes, &Units::kilometre_per_hour}},
        {UnitId::Nat, UnitSpec{UnitName::Nat, QT_TR_NOOP("nat"), UnitSymbol::Nat, {}, UnitFamily::Other, {UnitQuantity::Information}, NoSiPrefixes, &Units::nat}},
        {UnitId::NauticalMile, UnitSpec{UnitName::NauticalMile, QT_TR_NOOP("nautical mile"), UnitSymbol::NauticalMile, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::nautical_mile}},
        {UnitId::Ounce, UnitSpec{UnitName::Ounce, QT_TR_NOOP("ounce"), UnitSymbol::Ounce, {}, UnitFamily::Other, {UnitQuantity::Mass}, NoSiPrefixes, &Units::ounce}},
        {UnitId::Parsec, UnitSpec{UnitName::Parsec, QT_TR_NOOP("parsec"), UnitSymbol::Parsec, {}, UnitFamily::Other, {UnitQuantity::Length}, PositiveSiPrefixes, &Units::parsec}},
        {UnitId::PintImp, UnitSpec{UnitName::PintImp, QT_TR_NOOP("pint (Imperial)"), UnitSymbol::PintImp, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::pint_imp}},
        {UnitId::PintUs, UnitSpec{UnitName::PintUs, QT_TR_NOOP("pint (US)"), UnitSymbol::PintUs, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::pint_us}},
        {UnitId::Pound, UnitSpec{UnitName::Pound, QT_TR_NOOP("pound"), UnitSymbol::Pound, {}, UnitFamily::Other, {UnitQuantity::Mass}, NoSiPrefixes, &Units::pound}},
        {UnitId::PoundsPerSqinch, UnitSpec{UnitName::PoundsPerSqinch, QT_TR_NOOP("pounds per square inch"), UnitSymbol::PoundsPerSqinch, {}, UnitFamily::Other, {UnitQuantity::Pressure}, NoSiPrefixes, &Units::pounds_per_sqinch}},
        {UnitId::QuartImp, UnitSpec{UnitName::QuartImp, QT_TR_NOOP("quart (Imperial)"), UnitSymbol::QuartImp, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::quart_imp}},
        {UnitId::QuartUs, UnitSpec{UnitName::QuartUs, QT_TR_NOOP("quart (US)"), UnitSymbol::QuartUs, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::quart_us}},
        {UnitId::BarrelOil, UnitSpec{UnitName::BarrelOil, QT_TR_NOOP("oil barrel"), UnitSymbol::BarrelOil, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::barrel_oil}},
        {UnitId::BarrelBeerUs, UnitSpec{UnitName::BarrelBeerUs, QT_TR_NOOP("beer barrel (US)"), UnitSymbol::BarrelBeerUs, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::barrel_beer_us}},
        {UnitId::Rod, UnitSpec{UnitName::Rod, QT_TR_NOOP("rod"), UnitSymbol::Rod, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::rod}},
        {UnitId::SquareYard, UnitSpec{UnitName::SquareYard, QT_TR_NOOP("square yard"), UnitSymbol::SquareYard, {}, UnitFamily::Other, {UnitQuantity::Area}, NoSiPrefixes, &Units::square_yard}},
        {UnitId::CubicYard, UnitSpec{UnitName::CubicYard, QT_TR_NOOP("cubic yard"), UnitSymbol::CubicYard, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::cubic_yard}},
        {UnitId::ShortTon, UnitSpec{UnitName::ShortTon, QT_TR_NOOP("short ton"), UnitSymbol::ShortTon, {}, UnitFamily::Other, {UnitQuantity::Mass}, NoSiPrefixes, &Units::short_ton}},
        {UnitId::Stone, UnitSpec{UnitName::Stone, QT_TR_NOOP("stone"), UnitSymbol::Stone, {}, UnitFamily::Other, {UnitQuantity::Mass}, NoSiPrefixes, &Units::stone}},
        {UnitId::Tablespoon, UnitSpec{UnitName::Tablespoon, QT_TR_NOOP("tablespoon"), UnitSymbol::Tablespoon, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::tablespoon}},
        {UnitId::TablespoonAu, UnitSpec{UnitName::TablespoonAu, QT_TR_NOOP("tablespoon (Australian)"), UnitSymbol::TablespoonAu, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::tablespoon_au}},
        {UnitId::TablespoonImp, UnitSpec{UnitName::TablespoonImp, QT_TR_NOOP("tablespoon (Imperial)"), UnitSymbol::TablespoonImp, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::tablespoon_imp}},
        {UnitId::TablespoonUs, UnitSpec{UnitName::TablespoonUs, QT_TR_NOOP("tablespoon (US)"), UnitSymbol::TablespoonUs, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::tablespoon_us}},
        {UnitId::DessertSpoon, UnitSpec{UnitName::DessertSpoon, QT_TR_NOOP("dessert spoon"), UnitSymbol::DessertSpoon, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::dessert_spoon}},
        {UnitId::Teaspoon, UnitSpec{UnitName::Teaspoon, QT_TR_NOOP("teaspoon"), UnitSymbol::Teaspoon, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::teaspoon}},
        {UnitId::TeaspoonImp, UnitSpec{UnitName::TeaspoonImp, QT_TR_NOOP("teaspoon (Imperial)"), UnitSymbol::TeaspoonImp, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::teaspoon_imp}},
        {UnitId::TeaspoonUs, UnitSpec{UnitName::TeaspoonUs, QT_TR_NOOP("teaspoon (US)"), UnitSymbol::TeaspoonUs, {}, UnitFamily::Other, {UnitQuantity::Volume}, NoSiPrefixes, &Units::teaspoon_us}},
        {UnitId::Torr, UnitSpec{UnitName::Torr, QT_TR_NOOP("Torr"), UnitSymbol::Torr, {}, UnitFamily::Other, {UnitQuantity::Pressure}, NoSiPrefixes, &Units::torr}},
        {UnitId::Turn, UnitSpec{UnitName::Turn, QT_TR_NOOP("turn"), UnitSymbol::Turn, {UnitAltSymbol::Turn}, UnitFamily::Other, {UnitQuantity::PlaneAngle}, NoSiPrefixes, &Units::turn}},
        {UnitId::Revolution, UnitSpec{UnitName::Revolution, QT_TR_NOOP("revolution"), UnitSymbol::Revolution, {}, UnitFamily::Other, {UnitQuantity::PlaneAngle}, NoSiPrefixes, &Units::revolution}},
        {UnitId::RevolutionPerMinute, UnitSpec{UnitName::RevolutionPerMinute, QT_TR_NOOP("revolution per minute"), UnitSymbol::RevolutionPerMinute, {}, UnitFamily::Other, {UnitQuantity::Frequency}, NoSiPrefixes, &Units::revolution_per_minute}},
        {UnitId::Week, UnitSpec{UnitName::Week, QT_TR_NOOP("week"), UnitSymbol::Week, {}, UnitFamily::Other, {UnitQuantity::Time}, NoSiPrefixes, &Units::week}},
        {UnitId::KilowattHour, UnitSpec{UnitName::KilowattHour, QT_TR_NOOP("kilowatt hour"), UnitSymbol::KilowattHour, {}, UnitFamily::Other, {UnitQuantity::Energy, UnitQuantity::Work, UnitQuantity::AmountOfHeat}, NoSiPrefixes, &Units::kilowatt_hour}},
        {UnitId::MillimetreOfMercury, UnitSpec{UnitName::MillimetreOfMercury, QT_TR_NOOP("millimetre of mercury"), UnitSymbol::MillimetreOfMercury, {}, UnitFamily::Other, {UnitQuantity::Pressure}, NoSiPrefixes, &Units::millimetre_of_mercury}},
        {UnitId::Quad, UnitSpec{UnitName::Quad, QT_TR_NOOP("quad"), UnitSymbol::Quad, {}, UnitFamily::Other, {UnitQuantity::AmountOfHeat}, NoSiPrefixes, &Units::quad}},
        {UnitId::Yard, UnitSpec{UnitName::Yard, QT_TR_NOOP("yard"), UnitSymbol::Yard, {}, UnitFamily::Other, {UnitQuantity::Length}, NoSiPrefixes, &Units::yard}},
        {UnitId::CenturyJulian, UnitSpec{UnitName::CenturyJulian, QT_TR_NOOP("Julian century"), UnitSymbol::CenturyJulian, {}, UnitFamily::Other, {UnitQuantity::Time}, NoSiPrefixes, &Units::julian_century}},
        {UnitId::YearJulian, UnitSpec{UnitName::YearJulian, QT_TR_NOOP("Julian year"), UnitSymbol::YearJulian, {}, UnitFamily::Other, {UnitQuantity::Time}, NoSiPrefixes, &Units::julian_year}},
        {UnitId::YearSidereal, UnitSpec{UnitName::YearSidereal, QT_TR_NOOP("sidereal year"), UnitSymbol::YearSidereal, {}, UnitFamily::Other, {UnitQuantity::Time}, NoSiPrefixes, &Units::sidereal_year}},
        {UnitId::YearTropical, UnitSpec{UnitName::YearTropical, QT_TR_NOOP("tropical year"), UnitSymbol::YearTropical, {}, UnitFamily::Other, {UnitQuantity::Time}, NoSiPrefixes, &Units::tropical_year}},
    };
    return specs;
}

// Stable textual keys for base dimensions used in expression rendering/parsing.
// Canonical base-unit names are derived from unit specs (see helpers below).
struct DimensionKeyEntry {
    UnitQuantity quantity;
    QString dimensionKey;
};

const std::array<DimensionKeyEntry, 8>& s_dimensionKeyEntries()
{
    // Keep these keys stable: they are consumed by
    // unitQuantityDimensionKey()/unitQuantityFromDimensionKey().
    static const std::array<DimensionKeyEntry, 8> entries = {{
        {UnitQuantity::Mass, QStringLiteral("mass")},
        {UnitQuantity::Length, QStringLiteral("length")},
        {UnitQuantity::Time, QStringLiteral("time")},
        {UnitQuantity::ElectricCurrent, QStringLiteral("electric current")},
        {UnitQuantity::ThermodynamicTemperature, QStringLiteral("temperature")},
        {UnitQuantity::AmountOfSubstance, QStringLiteral("amount")},
        {UnitQuantity::LuminousIntensity, QStringLiteral("luminous intensity")},
        {UnitQuantity::Information, QStringLiteral("information")}
    }};
    return entries;
}

#define SI_PREFIX_TABLE(X) \
    X(Quecto, quecto, "quecto", "q", -30) \
    X(Ronto, ronto, "ronto", "r", -27) \
    X(Yocto, yocto, "yocto", "y", -24) \
    X(Zepto, zepto, "zepto", "z", -21) \
    X(Atto, atto, "atto", "a", -18) \
    X(Femto, femto, "femto", "f", -15) \
    X(Pico, pico, "pico", "p", -12) \
    X(Nano, nano, "nano", "n", -9) \
    X(Micro, micro, "micro", QString(UnicodeChars::MicroSign), -6) \
    X(Milli, milli, "milli", "m", -3) \
    X(Centi, centi, "centi", "c", -2) \
    X(Deci, deci, "deci", "d", -1) \
    X(Deca, deca, "deca", "da", 1) \
    X(Hecto, hecto, "hecto", "h", 2) \
    X(Kilo, kilo, "kilo", "k", 3) \
    X(Mega, mega, "mega", "M", 6) \
    X(Giga, giga, "giga", "G", 9) \
    X(Tera, tera, "tera", "T", 12) \
    X(Peta, peta, "peta", "P", 15) \
    X(Exa, exa, "exa", "E", 18) \
    X(Zetta, zetta, "zetta", "Z", 21) \
    X(Yotta, yotta, "yotta", "Y", 24) \
    X(Ronna, ronna, "ronna", "R", 27) \
    X(Quetta, quetta, "quetta", "Q", 30)

#define BINARY_PREFIX_TABLE(X) \
    X(Kibi, kibi, "kibi", "Ki", 10) \
    X(Mebi, mebi, "mebi", "Mi", 20) \
    X(Gibi, gibi, "gibi", "Gi", 30) \
    X(Tebi, tebi, "tebi", "Ti", 40) \
    X(Pebi, pebi, "pebi", "Pi", 50) \
    X(Exbi, exbi, "exbi", "Ei", 60) \
    X(Zebi, zebi, "zebi", "Zi", 70) \
    X(Yobi, yobi, "yobi", "Yi", 80) \
    X(Robi, robi, "robi", "Ri", 90) \
    X(Quebi, quebi, "quebi", "Qi", 100)

struct UnitRegistry {
    /*
        - valuesByIdentifier:
            Flat identifier->value map consumed by evaluator/unit lookup clients
            via Units::builtInUnitValues() and Units::builtInUnitLookup().

        - displaySymbolsByIdentifier:
            Maps canonical/alias identifiers to preferred short display symbols
            (e.g. "ohm" -> "Ω"), consumed by unit display-format helpers.

        - affineByIdentifier:
            Affine conversion metadata for identifiers like degree_celsius and
            degree_fahrenheit, used by affine conversion helpers.

        - canonicalMatchLookup:
            Dimension-to-canonical-unit map used by Units::findUnit() to select
            preferred display units for computed dimensions.
     */
    QHash<QString, Quantity> valuesByIdentifier;
    QHash<QString, QString> localizedByIdentifier;
    QHash<QString, QString> displaySymbolsByIdentifier;
    QHash<QString, AffineDefinition> affineByIdentifier;
    QHash<QMap<UnitQuantity, Rational>, Unit> canonicalMatchLookup;
};

// Returns the stable textual key for a base dimension quantity.
// Used by formatting/parsing fallback paths for unnamed dimensions.
QString unitQuantityDimensionKey(UnitQuantity quantity)
{
    for (const DimensionKeyEntry& entry : s_dimensionKeyEntries()) {
        if (entry.quantity == quantity)
            return entry.dimensionKey;
    }
    return QString();
}

// Parses a textual base-dimension key back into UnitQuantity.
// Returns false for unknown keys or null output pointers.
bool unitQuantityFromDimensionKey(const QString& key, UnitQuantity* out)
{
    if (!out)
        return false;

    for (const DimensionKeyEntry& entry : s_dimensionKeyEntries()) {
        if (key == entry.dimensionKey) {
            *out = entry.quantity;
            return true;
        }
    }
    return false;
}

// Normalizes user-provided unit text for identifier lookups:
// trims, removes outer parentheses, and lowercases.
QString normalizeUnitName(const QString& name)
{
    QString normalized = name.trimmed();
    if (normalized.startsWith(MathDsl::GroupStart)
        && normalized.endsWith(MathDsl::GroupEnd)
        && normalized.size() > 2)
        normalized = normalized.mid(1, normalized.size() - 2).trimmed();
    return normalized.toLower();
}

// Marks units that should be preferred as canonical display names
// when rendering matched dimensions.
bool isPreferredCanonicalDisplayUnit(UnitId id)
{
    switch (id) {
    case UnitId::Newton: case UnitId::Joule: case UnitId::Watt: case UnitId::Pascal:
    case UnitId::Coulomb: case UnitId::Volt: case UnitId::Ohm: case UnitId::Siemens:
    case UnitId::Farad: case UnitId::Weber: case UnitId::Tesla: case UnitId::Henry:
    case UnitId::Hertz: case UnitId::Becquerel: case UnitId::Gray: case UnitId::Sievert:
    case UnitId::Katal: case UnitId::Lumen: case UnitId::Lux:
        return true;
    default:
        return false;
    }
}

// Returns predefined phrase fragments used when composing display text.
QStringView unitPhrase(UnitPhraseId key)
{
    switch (key) {
    case UnitPhraseId::NewtonMetre: return UnitText::NewtonMetre;
    case UnitPhraseId::WattSecond: return UnitText::WattSecond;
    case UnitPhraseId::VoltSquaredAmpere: return UnitText::VoltSquaredAmpere;
    case UnitPhraseId::JoulePerSquareMetre: return UnitText::JoulePerSquareMetre;
    case UnitPhraseId::JoulePerCubicMetre: return UnitText::JoulePerCubicMetre;
    default: return QStringView();
    }
}

HNumber s_celsiusToKelvin(const HNumber& c)
{
    return c + HNumber("273.15");
}

HNumber s_kelvinToCelsius(const HNumber& k)
{
    return k - HNumber("273.15");
}

HNumber s_fahrenheitToKelvin(const HNumber& f)
{
    return (f + HNumber("459.67")) * HNumber("5") / HNumber("9");
}

HNumber s_kelvinToFahrenheit(const HNumber& k)
{
    return k * HNumber("9") / HNumber("5") - HNumber("459.67");
}

Quantity s_powerOfTen(int exponent)
{
    HNumber result(1);
    const HNumber ten(10);
    if (exponent >= 0) {
        for (int i = 0; i < exponent; ++i)
            result *= ten;
    } else {
        for (int i = 0; i < -exponent; ++i)
            result /= ten;
    }
    return Quantity(result);
}

Quantity s_powerOfTwo(int exponent)
{
    HNumber result(1);
    const HNumber two(2);
    for (int i = 0; i < exponent; ++i)
        result *= two;
    return Quantity(result);
}

const Quantity s_hartreeEnergyUnit()
{
    Quantity hartreeEnergy = HNumber("4.3597447222060e-18") * Units::joule();
    hartreeEnergy.setDisplayUnit(Units::joule().numericValue(), unitName(UnitId::Joule));
    return hartreeEnergy;
}

const QList<UnitId>& s_unitSpecOrder()
{
    static const QList<UnitId> order = [] {
        QList<UnitId> ids = s_unitSpecs().keys();
        std::sort(ids.begin(), ids.end(), [](UnitId a, UnitId b) {
            return static_cast<int>(a) < static_cast<int>(b);
        });
        return ids;
    }();
    return order;
}

QString s_baseUnitCanonicalNameForQuantity(UnitQuantity quantity)
{
    // Single source of truth:
    // infer canonical base-unit names from SI base rows in unit specs.
    const QHash<UnitId, UnitSpec>& specs = s_unitSpecs();
    for (const UnitId id : s_unitSpecOrder()) {
        const UnitSpec& spec = specs.value(id);
        if (spec.family != UnitFamily::SiBase || spec.quantities.isEmpty())
            continue;
        if (spec.quantities.first() == quantity)
            return spec.name;
    }
    return QString();
}

const QList<UnitQuantity>& s_baseDimensionOutputOrder()
{
    // Preferred base-dimension print order for fallback decomposition.
    static const QList<UnitQuantity> order = {
        UnitQuantity::Mass,
        UnitQuantity::Length,
        UnitQuantity::Time,
        UnitQuantity::ElectricCurrent,
        UnitQuantity::ThermodynamicTemperature,
        UnitQuantity::AmountOfSubstance,
        UnitQuantity::LuminousIntensity,
        UnitQuantity::Information
    };
    return order;
}

int s_baseDimensionOutputOrderIndex(UnitQuantity quantity)
{
    // Order index space starts at 2 because compact sorting reserves:
    // 0 => coulomb, 1 => joule.
    const int index = s_baseDimensionOutputOrder().indexOf(quantity);
    return index < 0 ? 100 : (2 + index);
}

const QList<PrefixSpec>& s_siPrefixes()
{
    static const QList<PrefixSpec> prefixes = {
#define SI_PREFIX_SPEC(id, func, name, symbol, exponent) \
        {PrefixId::id, name, symbol, exponent, s_powerOfTen(exponent)},
        SI_PREFIX_TABLE(SI_PREFIX_SPEC)
#undef SI_PREFIX_SPEC
    };
    return prefixes;
}

const QList<PrefixSpec>& s_binaryPrefixes()
{
    static const QList<PrefixSpec> prefixes = {
#define BINARY_PREFIX_SPEC(id, func, name, symbol, exponent) \
        {PrefixId::id, name, symbol, exponent, s_powerOfTwo(exponent)},
        BINARY_PREFIX_TABLE(BINARY_PREFIX_SPEC)
#undef BINARY_PREFIX_SPEC
    };
    return prefixes;
}

// Filters SI short-prefix aliases to keep only readable/established forms
// for deca/hecto combinations.
bool shouldAddSiPrefixedShortAlias(const QString& prefixSymbol, const QString& unitShortName)
{
    if (prefixSymbol != QLatin1String("da") && prefixSymbol != QLatin1String("h"))
        return true;

    const QString alias = prefixSymbol + unitShortName;
    return alias == QLatin1String("hPa")
           || alias == QLatin1String("hm")
           || alias == QLatin1String("hL")
           || alias == QLatin1String("hl")
           || alias == QLatin1String("dag");
}

enum class AngleUnitKind {
    None,
    Radian,
    Degree,
    Gradian,
    Turn,
    Arcminute,
    Arcsecond,
    Milliarcsecond,
    Microarcsecond
};

// Normalizes explicit angle unit tokens, including masculine ordinal aliasing.
QString normalizeAngleUnitName(const QString& unitName)
{
    QString normalized = unitName.trimmed().toLower();
    normalized.replace(UnicodeChars::MasculineOrdinalIndicator, UnicodeChars::DegreeSign); // º -> °
    return normalized;
}

AngleUnitKind angleUnitKindFromName(const QString& name)
{
    const QString normalized = normalizeAngleUnitName(name);
    if (normalized == unitName(UnitId::Radian)
        || normalized == UnitSymbol::Radian)
        return AngleUnitKind::Radian;
    if (normalized == unitName(UnitId::Degree)
        || normalized == UnitAltSymbol::Degree1
        || normalized == UnitSymbol::Degree)
        return AngleUnitKind::Degree;
    if (normalized == UnitName::Gradian
        || normalized == UnitSymbol::Gradian)
        return AngleUnitKind::Gradian;
    if (normalized == UnitName::Turn
        || normalized == UnitSymbol::Turn
        || normalized == UnitName::Revolution
        || normalized == UnitSymbol::Revolution)
        return AngleUnitKind::Turn;
    if (normalized == unitName(UnitId::Arcminute)
        || normalized == UnitAltSymbol::Arcminute
        || normalized == UnitSymbol::Arcminute)
        return AngleUnitKind::Arcminute;
    if (normalized == unitName(UnitId::Arcsecond)
        || normalized == UnitAltSymbol::Arcsecond
        || normalized == UnitSymbol::Arcsecond)
        return AngleUnitKind::Arcsecond;
    if (normalized == unitName(UnitId::Milliarcsecond)
        || normalized == UnitSymbol::Milliarcsecond)
        return AngleUnitKind::Milliarcsecond;
    if (normalized == unitName(UnitId::Microarcsecond)
        || normalized == UnitSymbol::Microarcsecond
        || normalized == UnitAltSymbol::MicroarcsecondAscii)
        return AngleUnitKind::Microarcsecond;
    return AngleUnitKind::None;
}

Quantity angleUnitValueForMode(AngleUnitKind angleUnit, char angleMode)
{
    Quantity modeReference = Units::radian();
    if (angleMode == 'd')
        modeReference = Units::degree();
    else if (angleMode == 'g')
        modeReference = Units::gradian();
    else if (angleMode == 't' || angleMode == 'v')
        modeReference = Units::turn();

    if (angleUnit == AngleUnitKind::Radian) {
        Quantity rad = Units::radian() / modeReference;
        rad.setDisplayUnit(Quantity(1).numericValue(), Units::angleModeUnitSymbol(angleMode));
        return rad;
    }

    if (angleUnit == AngleUnitKind::Degree) {
        Quantity degree = Units::degree() / modeReference;
        degree.setDisplayUnit(Quantity(1).numericValue(), Units::angleModeUnitSymbol(angleMode));
        return degree;
    }
    if (angleUnit == AngleUnitKind::Gradian) {
        Quantity gradian = Units::gradian() / modeReference;
        gradian.setDisplayUnit(Quantity(1).numericValue(), Units::angleModeUnitSymbol(angleMode));
        return gradian;
    }
    if (angleUnit == AngleUnitKind::Turn) {
        Quantity turn = Units::turn() / modeReference;
        turn.setDisplayUnit(Quantity(1).numericValue(), Units::angleModeUnitSymbol(angleMode));
        return turn;
    }
    if (angleUnit == AngleUnitKind::Arcminute) {
        Quantity arcminute = Units::arcminute() / modeReference;
        arcminute.setDisplayUnit(Quantity(1).numericValue(), Units::angleModeUnitSymbol(angleMode));
        return arcminute;
    }
    if (angleUnit == AngleUnitKind::Arcsecond) {
        Quantity arcsecond = Units::arcsecond() / modeReference;
        arcsecond.setDisplayUnit(Quantity(1).numericValue(), Units::angleModeUnitSymbol(angleMode));
        return arcsecond;
    }
    if (angleUnit == AngleUnitKind::Milliarcsecond) {
        Quantity milliarcsecond = Units::milliarcsecond() / modeReference;
        milliarcsecond.setDisplayUnit(Quantity(1).numericValue(), Units::angleModeUnitSymbol(angleMode));
        return milliarcsecond;
    }
    if (angleUnit == AngleUnitKind::Microarcsecond) {
        Quantity microarcsecond = Units::microarcsecond() / modeReference;
        microarcsecond.setDisplayUnit(Quantity(1).numericValue(), Units::angleModeUnitSymbol(angleMode));
        return microarcsecond;
    }
    return Quantity(0);
}

bool tryGetPrefixedExplicitAngleUnitValueForMode(const QString& identifier,
                                                 char angleMode,
                                                 Quantity* out)
{
    if (!out)
        return false;

    const QString normalized = UnicodeChars::normalizeUnitSymbolAliases(identifier.trimmed());
    if (normalized.isEmpty())
        return false;

    const auto tryMatchPrefix = [&](const QString& prefixText,
                                    const Quantity& prefixValue,
                                    Qt::CaseSensitivity caseSensitivity) {
        if (!normalized.startsWith(prefixText, caseSensitivity))
            return false;
        const QString base = normalized.mid(prefixText.size());
        const AngleUnitKind baseUnitKind = angleUnitKindFromName(base);
        if (baseUnitKind == AngleUnitKind::None)
            return false;
        *out = prefixValue * angleUnitValueForMode(baseUnitKind, angleMode);
        return true;
    };

    for (const PrefixSpec& prefix : s_siPrefixes()) {
        if (tryMatchPrefix(prefix.longName, prefix.value, Qt::CaseInsensitive))
            return true;
        if (tryMatchPrefix(prefix.symbol, prefix.value, Qt::CaseSensitive))
            return true;
        if (prefix.id == PrefixId::Micro
            && tryMatchPrefix(QStringLiteral("u"), prefix.value, Qt::CaseSensitive))
        {
            return true;
        }
    }

    return false;
}

QString unitName(UnitId id)
{
    return s_unitSpecs().value(id).name;
}

const char* unitLocalizedName(UnitId id)
{
    const auto it = s_unitSpecs().constFind(id);
    if (it == s_unitSpecs().constEnd())
        return "";
    return it.value().localized ? it.value().localized : "";
}

QString unitLocalizedIdentifierName(const QString& identifier)
{
    const auto it = s_unitRegistry().localizedByIdentifier.constFind(identifier);
    if (it != s_unitRegistry().localizedByIdentifier.constEnd())
        return it.value();

    const UnitId id = unitId(normalizeUnitName(identifier));
    return id == UnitId::Unknown ? QString() : QString::fromUtf8(unitLocalizedName(id));
}

QString unitSymbol(UnitId id)
{
    return s_unitSpecs().value(id).symbol;
}

QString Units::angleModeUnitSymbol(char angleMode)
{
    if (angleMode == 'd')
        return UnitSymbol::Degree;
    if (angleMode == 'g')
        return UnitSymbol::Gradian;
    if (angleMode == 't')
        return UnitSymbol::Turn;
    if (angleMode == 'v')
        return UnitSymbol::Revolution;
    return UnitSymbol::Radian;
}

QString Units::degreeAliasSymbol()
{
    return UnitAltSymbol::Degree1;
}

QString Units::arcminuteAliasSymbol()
{
    return UnitAltSymbol::Arcminute;
}

QString Units::arcsecondAliasSymbol()
{
    return UnitAltSymbol::Arcsecond;
}

QStringList Units::affineUnitAliases(UnitId id)
{
    switch (id) {
    case UnitId::DegreeCelsius:
        return {
            UnitAltSymbol::DegreeCelsius1,
            UnitAltSymbol::DegreeCelsius2,
            UnitAltSymbol::DegreeCelsius3,
            UnitAltSymbol::DegreeCelsius4,
            UnitAltSymbol::DegreeCelsius5
        };
    case UnitId::DegreeFahrenheit:
        return {
            UnitAltSymbol::DegreeFahrenheit1,
            UnitAltSymbol::DegreeFahrenheit2,
            UnitAltSymbol::DegreeFahrenheit3,
            UnitAltSymbol::DegreeFahrenheit4,
            UnitAltSymbol::DegreeFahrenheit5
        };
    default:
        return {};
    }
}

QString prefixName(PrefixId id)
{
    for (const PrefixSpec& prefix : s_siPrefixes()) {
        if (prefix.id == id)
            return prefix.longName;
    }
    for (const PrefixSpec& prefix : s_binaryPrefixes()) {
        if (prefix.id == id)
            return prefix.longName;
    }
    return QString();
}

QString prefixSymbol(PrefixId id)
{
    for (const PrefixSpec& prefix : s_siPrefixes()) {
        if (prefix.id == id)
            return prefix.symbol;
    }
    for (const PrefixSpec& prefix : s_binaryPrefixes()) {
        if (prefix.id == id)
            return prefix.symbol;
    }
    return QString();
}

PrefixId prefixId(const QString& normalizedName)
{
    const QString key = normalizedName.trimmed();
    for (const PrefixSpec& prefix : s_siPrefixes()) {
        if (key == prefix.longName || key == prefix.symbol)
            return prefix.id;
    }
    for (const PrefixSpec& prefix : s_binaryPrefixes()) {
        if (key == prefix.longName || key == prefix.symbol)
            return prefix.id;
    }
    return PrefixId::Unknown;
}

const QList<PrefixId>& siPrefixIds()
{
    static const QList<PrefixId> ids = [] {
        QList<PrefixId> list;
        for (const PrefixSpec& prefix : s_siPrefixes())
            list.append(prefix.id);
        return list;
    }();
    return ids;
}

const QList<PrefixId>& binaryPrefixIds()
{
    static const QList<PrefixId> ids = [] {
        QList<PrefixId> list;
        for (const PrefixSpec& prefix : s_binaryPrefixes())
            list.append(prefix.id);
        return list;
    }();
    return ids;
}

Quantity unitValue(UnitId id)
{
    const auto it = s_unitSpecs().constFind(id);
    if (it == s_unitSpecs().constEnd())
        return Quantity(1);
    const UnitSpec& spec = it.value();
    return spec.linearValue ? spec.linearValue() : Quantity(1);
}

/*
   Resolve a normalized unit identifier (canonical name or supported alias)
   to its UnitId.

   Expected input:
   - already normalized text (for example via normalizeUnitName()).
   - case-insensitive handling is achieved by normalization before calling.

   Optimization:
   - Uses a function-local static QHash<QString, UnitId> built once on first
     call, then reused for all subsequent lookups.
   - Canonical names/symbols/aliases are inferred from unit specs
     (single source of truth).
 */
UnitId unitId(const QString& name)
{
    static const QHash<QString, UnitId> byName = [] {
        QHash<QString, UnitId> map;
        QHash<QString, int> normalizedSymbolUseCount;
        const QHash<UnitId, UnitSpec>& specs = s_unitSpecs();
        for (const UnitId id : s_unitSpecOrder()) {
            const UnitSpec& spec = specs.value(id);
            if (!spec.symbol.isEmpty()) {
                const QString normalizedSymbol = normalizeUnitName(spec.symbol);
                normalizedSymbolUseCount.insert(
                    normalizedSymbol,
                    normalizedSymbolUseCount.value(normalizedSymbol, 0) + 1);
            }
        }

        const auto insertIfMissing = [&](const QString& key, UnitId id) {
            if (key.isEmpty() || map.contains(key))
                return;
            map.insert(key, id);
        };
        for (const UnitId id : s_unitSpecOrder()) {
            const UnitSpec& spec = specs.value(id);
            insertIfMissing(normalizeUnitName(spec.name), id);
            if (!spec.symbol.isEmpty()) {
                const QString normalizedSymbol = normalizeUnitName(spec.symbol);
                if (normalizedSymbolUseCount.value(normalizedSymbol) == 1)
                    insertIfMissing(normalizedSymbol, id);
            }
            for (const QString& alias : spec.aliases) {
                if (alias.isEmpty())
                    continue;
                insertIfMissing(normalizeUnitName(alias), id);
            }
        }
        return map;
    }();
    return byName.value(name, UnitId::Unknown);
}

template <typename Builder>
const Quantity s_cachedUnit(QMap<QString, Quantity>& cache, const QString& name, Builder&& builder)
{
    auto it = cache.constFind(name);
    if (it == cache.cend())
        it = cache.insert(name, builder());
    return it.value();
}

// Builds a base-unit quantity from the spec dimension list of a canonical name.
Quantity s_baseUnitFromDefinitions(const QString& canonicalName)
{
    const QHash<UnitId, UnitSpec>& specs = s_unitSpecs();
    for (const UnitId id : s_unitSpecOrder()) {
        const UnitSpec& spec = specs.value(id);
        if (spec.name != canonicalName)
            continue;
        if (spec.quantities.isEmpty())
            break;
        Quantity value(1);
        value.modifyDimension(spec.quantities.first(), 1);
        return value;
    }
    return Quantity(1);
}

template <typename FallbackBuilder>
Quantity s_unitValueFromDefinitions(const QString& canonicalName,
                                    const Quantity (*selfGetter)(),
                                    FallbackBuilder&& fallbackBuilder)
{
    // Uses declarative spec linear value when available; otherwise keeps
    // the fallback builder expression for compatibility.
    const QHash<UnitId, UnitSpec>& specs = s_unitSpecs();
    for (const UnitId id : s_unitSpecOrder()) {
        const UnitSpec& spec = specs.value(id);
        if (spec.name != canonicalName)
            continue;
        if (spec.linearValue && spec.linearValue != selfGetter)
            return spec.linearValue();
        break;
    }
    Quantity fallbackValue = fallbackBuilder();
    // Joule is defined from N*m. Without this explicit override, the fallback
    // display token propagates as "newton metre", which then leaks into heat
    // units derived from joule (for example Btu). Keep joule's canonical name
    // so conversions like Btu -> J and Btu -> joule render as energy units.
    if (canonicalName == UnitName::Joule)
        fallbackValue.setDisplayUnit(fallbackValue.numericValue(), canonicalName);
    return fallbackValue;
}

#define DEFINE_DERIVED_UNIT(name, value) \
    const Quantity Units::name() \
    { \
        const QString unitKey = QStringLiteral(#name); \
        return s_cachedUnit(m_cache, unitKey, [unitKey]() { \
            return s_unitValueFromDefinitions(unitKey, &Units::name, []() { \
                return Quantity(value); \
            }); \
        }); \
    }

#define DEFINE_BASE_UNIT(name) \
    const Quantity Units::name() \
    { \
        const QString unitKey = QStringLiteral(#name); \
        return s_cachedUnit(m_cache, unitKey, [unitKey]() { return s_baseUnitFromDefinitions(unitKey); }); \
    }

QHash<QMap<UnitQuantity, Rational>, Unit> Units::m_matchLookup;
QMap<QString, Quantity> Units::m_cache;


// Adds a normalized quantity->name mapping into the canonical match table.
void Units::pushUnit(Quantity q, QString name)
{
    q.cleanDimension();
    Unit u(name, q);
    m_matchLookup.insert(q.getDimensionByQuantity(), u);
}


// Initialize the lookup table for automatic matching.
void Units::initTable()
{
    // Base canonicalization map derived from unit specs.
    // Entries in this map are preferred renderings for matching dimensions.
    m_matchLookup = s_unitRegistry().canonicalMatchLookup;

    // Additional preferred canonical renderings for composite dimensions that
    // are not represented by a single unit spec row.
    //
    // Important: these entries DO NOT mark dimensions as "never simplify".
    // They do the opposite: when a dimension matches, formatting prefers the
    // named/compound form below. Ambiguity exceptions are handled separately by
    // denyAutoCanonicalization() in findUnit().
    const auto addCanonical = [&](Quantity q, const QString& name) {
        q.cleanDimension();
        m_matchLookup.insert(q.getDimensionByQuantity(), Unit(name, q));
    };
    addCanonical(Units::volt()*Units::volt()*Units::ampere(), "volt² ampere");
    addCanonical(Units::volt()*Units::volt() / Units::ampere(), "volt² ampere⁻¹");
    addCanonical(Units::ohm()*Units::metre(), "ohm metre");
    addCanonical(Units::siemens() / Units::metre(), "siemens metre⁻¹");
    addCanonical(Units::siemens() / Units::metre() / Units::mole(), "siemens metre⁻¹ mole⁻¹");
    addCanonical(Units::coulomb() / Units::cubic_metre(), "coulomb metre⁻³");
    addCanonical(Units::coulomb() / Units::square_metre(), "coulomb metre⁻²");
    addCanonical(Units::coulomb() / Units::kilogram(), "coulomb kilogram⁻¹");
    addCanonical(Units::farad() / Units::metre(), "farad metre⁻¹");
    addCanonical(Units::henry() / Units::metre(), "henry metre⁻¹");
    addCanonical(Quantity(1) / Units::metre() / Units::tesla(), "metre⁻¹ tesla⁻¹");
    addCanonical(Units::joule() / Units::kilogram() / Units::kelvin(), "joule kilogram⁻¹ kelvin⁻¹");
    addCanonical(Units::joule() / Units::mole() / Units::kelvin(), "joule mole⁻¹ kelvin⁻¹");
    addCanonical(Units::mole() / Units::second() / Units::cubic_metre(), "mole second⁻¹ metre⁻³");
    addCanonical(Units::newton() / Units::metre(), "newton metre⁻¹");
    addCanonical(Units::pascal() * Units::second(), "pascal second");
    addCanonical(Units::volt() / Units::metre(), "volt metre⁻¹");
    addCanonical(Units::watt() / Units::metre() / Units::kelvin(), "watt metre⁻¹ kelvin⁻¹");
    addCanonical(Units::watt() / Units::square_metre(), "watt/metre²");
    addCanonical(Units::joule() / Units::kelvin(), "joule/kelvin");
    addCanonical(Units::joule() / Units::kilogram(), "joule/kilogram");
    addCanonical(Units::kilogram()*Units::square_metre()*Units::volt() / (Units::second()*Units::second()*Units::second()*Units::second()), "kilogram metre² volt second⁻⁴");
}

// Resolves and formats the best unit display for a quantity dimension:
// prefers canonical named units, otherwise renders composed factors.
void Units::findUnit(Quantity& q)
{
    struct ExplicitAngleFactorInfo {
        bool hasFactor = false;
        Rational exponent = Rational(0);
        AngleUnitKind preferredKind = AngleUnitKind::None;
    };
    const auto explicitAngleFactorInfo = [](const QString& text) {
        ExplicitAngleFactorInfo info;
        if (text.isEmpty())
            return info;
        int sign = 1;
        QString token;
        auto flushToken = [&]() {
            if (token.isEmpty())
                return;
            QString factor = token.trimmed();
            token.clear();
            if (factor.isEmpty())
                return;
            int exponentStart = factor.size();
            bool hasExponent = false;
            int exponent = parseTrailingExponent(factor, &exponentStart, &hasExponent);
            QString base = factor.left(exponentStart).trimmed();
            while (!base.isEmpty()
                   && (UnicodeChars::isSuperscriptDigit(base.at(base.size() - 1))
                       || UnicodeChars::isSuperscriptSign(base.at(base.size() - 1))))
            {
                base.chop(1);
            }
            if (base.isEmpty())
                return;
            const AngleUnitKind kind = angleUnitKindFromName(base);
            if (kind == AngleUnitKind::None)
                return;
            info.hasFactor = true;
            if (info.preferredKind == AngleUnitKind::None)
                info.preferredKind = kind;
            if (!hasExponent)
                exponent = 1;
            info.exponent += Rational(sign * exponent);
        };
        for (int i = 0; i < text.size(); ++i) {
            const QChar ch = text.at(i);
            const bool isUnitChar =
                UnicodeChars::isUnitIdentifierChar(ch)
                || UnicodeChars::isSuperscriptDigit(ch)
                || UnicodeChars::isSuperscriptSign(ch);
            if (isUnitChar) {
                token += ch;
                continue;
            }
            flushToken();
            if (ch == MathDsl::DivOp)
                sign = -1;
        }
        flushToken();
        return info;
    };

    const QString originalUnitName = q.unitName();
    QString unit_name = "";
    CNumber unit(1);
    q.cleanDimension();

    if (m_matchLookup.isEmpty())
        initTable();

    const QMap<UnitQuantity, Rational> dim = q.getDimensionByQuantity();
    const ExplicitAngleFactorInfo angleInfo = explicitAngleFactorInfo(originalUnitName);
    const bool preserveExplicitAngleFactor =
        !originalUnitName.isEmpty()
        && !q.isDimensionless()
        && angleInfo.hasFactor
        && !angleInfo.exponent.isZero();
    // Policy: only auto-canonicalize dimensions that map to a single,
    // overwhelmingly standard named unit without major semantic collision.
    // Example: N·m is intentionally kept explicit in multiplication code
    // (see Quantity::operator*) and only becomes J on explicit conversion.
    // These denylist entries intentionally keep expressions expanded to
    // avoid silently choosing one meaning among multiple valid ones.
    const auto denyAutoCanonicalization = [&](const QString& candidateName,
                                              const QMap<UnitQuantity, Rational>& candidateDim) {
        auto makeDim = [](std::initializer_list<std::pair<UnitQuantity, int>> items) {
            QMap<UnitQuantity, Rational> d;
            for (const auto& item : items)
                d.insert(item.first, Rational(item.second));
            return d;
        };

        static const QMap<UnitQuantity, Rational> perSecondDim =
            makeDim({{UnitQuantity::Time, -1}});
        static const QMap<UnitQuantity, Rational> specificEnergyDim =
            makeDim({{UnitQuantity::Length, 2}, {UnitQuantity::Time, -2}});

        const QString name = candidateName.toLower();
        if ((name == unitName(UnitId::Hertz) || name == unitName(UnitId::Becquerel))
            && candidateDim == perSecondDim)
        {
            return true;
        }
        if ((name == unitName(UnitId::Gray) || name == unitName(UnitId::Sievert))
            && candidateDim == specificEnergyDim)
        {
            return true;
        }
        return false;
    };
    if (m_matchLookup.contains(dim) && !preserveExplicitAngleFactor) {
        Unit temp(m_matchLookup[dim]);
        if (runtimeUnitNegativeExponentStyle() == Settings::UnitNegativeExponentSuperscript)
            temp.name = applySuperscriptStyleToUnitName(temp.name);
        // For denylisted dimensions, keep base/compound form instead of forcing
        // a potentially ambiguous derived-unit name.
        if (!denyAutoCanonicalization(temp.name, dim))
            q.setDisplayUnit(temp.value.numericValue(), temp.name);
    } else {
        const auto superscriptFromExponent = [](const Rational& value) {
            QString exponent = value.toString();
            if (exponent.contains('/'))
                exponent = QString(MathDsl::PowOp) + QString(MathDsl::GroupStart) + exponent + MathDsl::GroupEnd;
            else if (exponent == "1")
                exponent = "";
            else
                exponent = QString(MathDsl::PowOp) + exponent;

            if (exponent.startsWith(MathDsl::PowOp)
                && !exponent.contains(MathDsl::GroupStart))
            {
                QString superscript;
                superscript.reserve(exponent.size());
                for (int i = 1; i < exponent.size(); ++i) {
                    const QChar ch = exponent.at(i);
                    if (ch == MathDsl::SubOpAl1) {
                        superscript += MathDsl::PowNeg;
                    } else if (ch == QLatin1Char('+')) {
                        superscript += MathDsl::PowPos;
                    } else if (ch >= MathDsl::Dig0 && ch <= MathDsl::Dig9) {
                        superscript += MathDsl::asciiDigitToSuperscript(ch);
                    } else {
                        return exponent;
                    }
                }
                if (!superscript.isEmpty())
                    return superscript;
            }
            return exponent;
        };

        struct UnitFactor {
            QString name;
            Rational exponent;
        };

        QList<UnitFactor> factors;
        const auto appendFactor = [&](const QString& name, const Rational& exponent) {
            factors.append(UnitFactor{name, exponent});
        };
        if (preserveExplicitAngleFactor) {
            const auto compositeAngleDisplayName = [](AngleUnitKind kind) {
                if (kind == AngleUnitKind::Radian)
                    return unitName(UnitId::Radian);
                if (kind == AngleUnitKind::Gradian)
                    return UnitSymbol::Gradian;
                if (kind == AngleUnitKind::Turn)
                    return UnitSymbol::Turn;
                return UnitAltSymbol::Degree1;
            };
            appendFactor(compositeAngleDisplayName(angleInfo.preferredKind), angleInfo.exponent);
            const Quantity explicitAngleScale = angleUnitValueForMode(
                angleInfo.preferredKind == AngleUnitKind::None
                    ? AngleUnitKind::Degree
                    : angleInfo.preferredKind,
                s_runtimeAngleModeForUnits);
            const QByteArray exponentText = angleInfo.exponent.toString().toLatin1();
            unit *= HMath::raise(explicitAngleScale.numericValue().real,
                                 HNumber(exponentText.constData()));
        }

        // Prefer recognizable derived SI units before base-unit fallback.
        QMap<UnitQuantity, Rational> remaining = q.getDimensionByQuantity();
        static const QList<Unit> derivedCandidates = {
            Unit(unitName(UnitId::Tesla), unitValue(UnitId::Tesla)),
            Unit(unitName(UnitId::Weber), unitValue(UnitId::Weber)),
            Unit(unitName(UnitId::Henry), unitValue(UnitId::Henry)),
            Unit(unitName(UnitId::Farad), unitValue(UnitId::Farad)),
            Unit(unitName(UnitId::Ohm), unitValue(UnitId::Ohm)),
            Unit(unitName(UnitId::Siemens), unitValue(UnitId::Siemens)),
            Unit(unitName(UnitId::Volt), unitValue(UnitId::Volt)),
            Unit(unitName(UnitId::Coulomb), unitValue(UnitId::Coulomb)),
            Unit(unitName(UnitId::Joule), unitValue(UnitId::Joule)),
            Unit(unitName(UnitId::Newton), unitValue(UnitId::Newton)),
            Unit(unitName(UnitId::Pascal), unitValue(UnitId::Pascal)),
            Unit(unitName(UnitId::Watt), unitValue(UnitId::Watt)),
            Unit(unitName(UnitId::Hertz), unitValue(UnitId::Hertz)),
            Unit(unitName(UnitId::Becquerel), unitValue(UnitId::Becquerel)),
            Unit(unitName(UnitId::Gray), unitValue(UnitId::Gray)),
            Unit(unitName(UnitId::Sievert), unitValue(UnitId::Sievert)),
            Unit(unitName(UnitId::Katal), unitValue(UnitId::Katal)),
            Unit(unitName(UnitId::Lumen), unitValue(UnitId::Lumen)),
            Unit(unitName(UnitId::Lux), unitValue(UnitId::Lux)),
            Unit(unitName(UnitId::Steradian), unitValue(UnitId::Steradian))
        };
        const QString lumenName = unitName(UnitId::Lumen);

        bool progress = true;
        while (progress) {
            progress = false;
            for (const Unit& candidate : derivedCandidates) {
                Quantity qc(candidate.value);
                qc.cleanDimension();
                const auto cdim = qc.getDimensionByQuantity();
                if (cdim.isEmpty()
                    || (cdim.size() < 2 && candidate.name != lumenName))
                    continue;

                bool firstRatio = true;
                Rational ratio;
                bool fits = true;
                for (auto it = cdim.constBegin(); it != cdim.constEnd(); ++it) {
                    if (!remaining.contains(it.key())) {
                        fits = false;
                        break;
                    }
                    const Rational rv = remaining.value(it.key());
                    const Rational cv = it.value();
                    Rational r(rv.numerator() * cv.denominator(),
                               rv.denominator() * cv.numerator());
                    if (firstRatio) {
                        ratio = r;
                        firstRatio = false;
                    } else if (r != ratio) {
                        fits = false;
                        break;
                    }
                }

                if (!fits || firstRatio || ratio.isZero() || ratio.denominator() != 1)
                    continue;

                const Rational ratioInt(ratio.numerator(), 1);
                for (auto it = cdim.constBegin(); it != cdim.constEnd(); ++it) {
                    const UnitQuantity key = it.key();
                    remaining[key] -= ratioInt * it.value();
                    if (remaining[key].isZero())
                        remaining.remove(key);
                }
                if (!denyAutoCanonicalization(candidate.name, qc.getDimensionByQuantity()))
                    appendFactor(candidate.name, ratioInt);
                else {
                    // Keep base-unit expansion for denied automatic canonicalizations.
                    // Revert the tentative reduction and continue searching.
                    for (auto it = cdim.constBegin(); it != cdim.constEnd(); ++it) {
                        const UnitQuantity key = it.key();
                        remaining[key] += ratioInt * it.value();
                        if (remaining[key].isZero())
                            remaining.remove(key);
                    }
                    continue;
                }
                progress = true;
                break;
            }
        }

        auto appendDimension = [&](UnitQuantity key, const Rational& value) {
            const QString baseUnitName = s_baseUnitCanonicalNameForQuantity(key);
            if (!baseUnitName.isEmpty())
                appendFactor(baseUnitName, value);
            else {
                const QString dimensionKey = unitQuantityDimensionKey(key);
                appendFactor(!dimensionKey.isEmpty() ? dimensionKey : QStringLiteral("dimension"), value);
            }
        };

        // Keep auto-generated base-unit products in canonical output order:
        // kilogram, metre, second, ampere, kelvin, mole, candela, information.
        // (Angle units such as radian/steradian are represented explicitly when used.)
        QSet<UnitQuantity> handledDimensions;
        for (const UnitQuantity key : s_baseDimensionOutputOrder()) {
            if (!remaining.contains(key))
                continue;
            appendDimension(key, remaining.value(key));
            handledDimensions.insert(key);
        }
        for (auto i = remaining.constBegin(); i != remaining.constEnd(); ++i) {
            if (!handledDimensions.contains(i.key()))
                appendDimension(i.key(), i.value());
        }

        // Readability optimization for mixed electro-mechanical dimensions:
        // when both electrical current and mass are present, prefer a compact
        // representation in terms of C and J (plus minimal base leftovers)
        // when that is clearly shorter than base-heavy decomposition.
        const auto exponentComplexity = [](const Rational& exponent) {
            const int n = qAbs(exponent.numerator());
            const int d = exponent.denominator();
            return d == 1 ? n : (n + d);
        };
        const auto factorPenalty = [exponentComplexity](const UnitFactor& factor) {
            const UnitId factorId = unitId(normalizeUnitName(factor.name));
            const auto specIt = s_unitSpecs().constFind(factorId);
            const bool isBase =
                specIt != s_unitSpecs().constEnd()
                && specIt.value().family == UnitFamily::SiBase;
            const int unitWeight = isBase ? 6 : 2;
            return unitWeight * exponentComplexity(factor.exponent);
        };
        const auto scoreFactors = [&](const QList<UnitFactor>& candidateFactors) {
            int score = 0;
            for (const UnitFactor& factor : candidateFactors)
                score += factorPenalty(factor);
            score += candidateFactors.size() * 2;
            return score;
        };

        const Rational zero(0);
        const Rational two(2);
        const Rational currentExp = dim.value(UnitQuantity::ElectricCurrent, zero);
        const Rational massExp = dim.value(UnitQuantity::Mass, zero);
        if (!currentExp.isZero()
            && !massExp.isZero()
            && currentExp.denominator() == 1
            && massExp.denominator() == 1)
        {
            const Rational lengthExp = dim.value(UnitQuantity::Length, zero);
            const Rational timeExp = dim.value(UnitQuantity::Time, zero);
            if (lengthExp.denominator() == 1 && timeExp.denominator() == 1) {
                QList<UnitFactor> compactFactors;
                compactFactors.reserve(factors.size());

                compactFactors.append(UnitFactor{unitName(UnitId::Coulomb), currentExp});
                compactFactors.append(UnitFactor{unitName(UnitId::Joule), massExp});

                const Rational residualLength = lengthExp - (two * massExp);
                const Rational residualTime = timeExp - currentExp + (two * massExp);
                if (!residualLength.isZero())
                    compactFactors.append(UnitFactor{unitName(UnitId::Metre), residualLength});
                if (!residualTime.isZero())
                    compactFactors.append(UnitFactor{unitName(UnitId::Second), residualTime});

                for (auto it = dim.constBegin(); it != dim.constEnd(); ++it) {
                    const UnitQuantity key = it.key();
                    const bool isCoreCompactDimension =
                        key == UnitQuantity::ElectricCurrent
                        || key == UnitQuantity::Mass
                        || key == UnitQuantity::Length
                        || key == UnitQuantity::Time;
                    if (isCoreCompactDimension)
                        continue;
                    if (!it.value().isZero()) {
                        const QString baseUnitName = s_baseUnitCanonicalNameForQuantity(key);
                        const QString dimensionKey = unitQuantityDimensionKey(key);
                        const QString factorName = !baseUnitName.isEmpty()
                            ? baseUnitName
                            : (!dimensionKey.isEmpty()
                               ? dimensionKey
                               : QStringLiteral("dimension"));
                        compactFactors.append(UnitFactor{factorName, it.value()});
                    }
                }

                // Keep deterministic order: C, J, then leftovers in canonical
                // base-dimension order and finally unknown dimensions.
                const auto orderIndex = [](const QString& name) {
                    if (name == unitName(UnitId::Coulomb)) return 0;
                    if (name == unitName(UnitId::Joule)) return 1;
                    const auto& order = s_baseDimensionOutputOrder();
                    for (const UnitQuantity quantity : order) {
                        if (name == s_baseUnitCanonicalNameForQuantity(quantity))
                            return s_baseDimensionOutputOrderIndex(quantity);
                    }
                    return 100;
                };
                std::sort(compactFactors.begin(), compactFactors.end(),
                          [&](const UnitFactor& a, const UnitFactor& b) {
                              const int ao = orderIndex(a.name);
                              const int bo = orderIndex(b.name);
                              if (ao != bo)
                                  return ao < bo;
                              return a.name < b.name;
                          });

                if (scoreFactors(compactFactors) < scoreFactors(factors))
                    factors = compactFactors;
            }
        }

        if (runtimeUnitNegativeExponentStyle() == Settings::UnitNegativeExponentSuperscript) {
            QStringList terms;
            for (const UnitFactor& factor : factors)
                terms << (factor.name + superscriptFromExponent(factor.exponent));
            unit_name = MathDsl::QuantSp + terms.join(QString(MathDsl::MulDotOp));
        } else {
            QStringList numeratorTerms;
            QStringList denominatorTerms;
            for (const UnitFactor& factor : factors) {
                if (factor.exponent > zero) {
                    numeratorTerms << (factor.name + superscriptFromExponent(factor.exponent));
                } else if (factor.exponent < zero) {
                    const Rational absExponent(-factor.exponent.numerator(),
                                               factor.exponent.denominator());
                    denominatorTerms << (factor.name + superscriptFromExponent(absExponent));
                }
            }

            const QString numeratorText = numeratorTerms.join(QString(MathDsl::MulDotOp));
            const QString denominatorText = denominatorTerms.join(QString(MathDsl::MulDotOp));
            if (!numeratorTerms.isEmpty() && !denominatorTerms.isEmpty()) {
                unit_name = MathDsl::QuantSp + numeratorText + MathDsl::DivOp;
                if (denominatorTerms.size() > 1)
                    unit_name += MathDsl::GroupStart + denominatorText + MathDsl::GroupEnd;
                else
                    unit_name += denominatorText;
            } else if (!numeratorTerms.isEmpty()) {
                unit_name = MathDsl::QuantSp + numeratorText;
            } else if (!denominatorTerms.isEmpty()) {
                // Keep inverse-only units in exponent form (s⁻¹, m⁻¹⋅s⁻², ...).
                QStringList inverseTerms;
                for (const UnitFactor& factor : factors) {
                    if (factor.exponent < zero)
                        inverseTerms << (factor.name + superscriptFromExponent(factor.exponent));
                }
                unit_name = MathDsl::QuantSp + inverseTerms.join(QString(MathDsl::MulDotOp));
            }
        }

        q.setDisplayUnit(unit, unit_name.trimmed());
    }
}

namespace {

// Builds runtime lookup registries from declarative unit specs:
// identifiers, display symbols, affine converters and canonical matches.
static UnitRegistry s_buildUnitRegistry()
{
    UnitRegistry registry;
    QList<UnitAliasSpec> unitAliasSpecs;

    const auto addUnique = [&](const QString& name,
                               const Quantity& value,
                               const QString& localizedName = QString()) {
        if (name.isEmpty() || registry.valuesByIdentifier.contains(name))
            return;
        registry.valuesByIdentifier.insert(name, value);
        if (!localizedName.isEmpty())
            registry.localizedByIdentifier.insert(name, localizedName);
    };

    const auto localizedFromSpec = [](const UnitSpec& spec) {
        return spec.localized ? QString::fromUtf8(spec.localized) : QString();
    };

    const auto prefixedLocalizedName = [](const QString& prefix, const QString& base) -> QString {
        if (prefix.isEmpty() || base.isEmpty())
            return QString();
        QString loweredBase = base;
        loweredBase[0] = loweredBase.at(0).toLower();
        return prefix + loweredBase;
    };

    const QHash<UnitId, UnitSpec>& specs = s_unitSpecs();
    for (const UnitId id : s_unitSpecOrder()) {
        const UnitSpec& spec = specs.value(id);
        // Spec -> runtime registry:
        // canonical name/symbol/aliases are all registered from this single source.
        const Quantity value = spec.linearValue ? spec.linearValue() : Quantity(1);
        const QString localized = localizedFromSpec(spec);
        addUnique(spec.name, value, localized);
        const QString symbol = spec.symbol;

        if (!symbol.isEmpty()) {
            addUnique(symbol, value, localized);
            registry.displaySymbolsByIdentifier.insert(spec.name, symbol);
            registry.displaySymbolsByIdentifier.insert(symbol, symbol);
        }
        for (const QString& alias : spec.aliases) {
            if (alias.isEmpty())
                continue;
            addUnique(alias, value, localized);
            if (!symbol.isEmpty())
                registry.displaySymbolsByIdentifier.insert(alias, symbol);
        }

        // Prefix generation currently consumes one alternate short alias
        // (the first alias entry), in addition to the canonical symbol.
        const QString primaryAlias = spec.aliases.isEmpty()
            ? QString()
            : spec.aliases.first();
        unitAliasSpecs.append({
            spec.name,
            localized,
            value,
            symbol,
            primaryAlias,
            spec.siPrefixPolicy
        });

        if (spec.affine.toBase && spec.affine.fromBase) {
            registry.affineByIdentifier.insert(spec.name, spec.affine);
            if (!symbol.isEmpty())
                registry.affineByIdentifier.insert(symbol, spec.affine);
            for (const QString& alias : spec.aliases) {
                if (alias.isEmpty())
                    continue;
                registry.affineByIdentifier.insert(alias, spec.affine);
            }
            continue;
        }

        // Build canonical dimension->unit lookup from declarative spec.
        // Keep only unambiguous SI base/derived rows and preserve first-in wins.
        if ((spec.family == UnitFamily::SiBase
             || spec.family == UnitFamily::SiDerived)
            && !spec.quantities.contains(UnitQuantity::Unspecified)
            && spec.quantities.size() == 1)
        {
            const UnitQuantity quantity = spec.quantities.first();
            // Keep ambiguous or purely notational units out of auto-canonicalization.
            if (quantity == UnitQuantity::PlaneAngle
                || quantity == UnitQuantity::SolidAngle
                || quantity == UnitQuantity::Frequency
                || quantity == UnitQuantity::Area
                || quantity == UnitQuantity::ActivityReferredToARadionuclide
                || quantity == UnitQuantity::AbsorbedDose
                || quantity == UnitQuantity::DoseEquivalent
                || quantity == UnitQuantity::Information)
            {
                continue;
            }
            Quantity canonicalValue = value;
            canonicalValue.cleanDimension();
            const QMap<UnitQuantity, Rational> dim = canonicalValue.getDimensionByQuantity();
            if (!dim.isEmpty() && !registry.canonicalMatchLookup.contains(dim)) {
                registry.canonicalMatchLookup.insert(
                    dim,
                    Unit(spec.name, canonicalValue));
            }
        }
    }

    // Automatic SI-prefixed aliases for every prefixable unit.
    // Example: kilo + m² => km², kilo + m³ => km³.
    // This is spec-driven via UnitAliasSpec built above from unit specs.
    for (const PrefixSpec& prefix : s_siPrefixes()) {
        for (const UnitAliasSpec& unit : unitAliasSpecs) {
            if (prefix.power > 0 && !(unit.siPrefixPolicy & PositiveSiPrefixes))
                continue;
            if (prefix.power < 0 && !(unit.siPrefixPolicy & NegativeSiPrefixes))
                continue;
            const Quantity prefixedValue = prefix.value * unit.value;
            const QString localized = prefixedLocalizedName(prefix.longName, unit.localized);
            // Long form: kilo + metre => kilometre
            addUnique(prefix.longName + unit.longName, prefixedValue, localized);
            if (!unit.shortName.isEmpty()
                && shouldAddSiPrefixedShortAlias(prefix.symbol, unit.shortName))
                // Symbol form: k + m => km; k + m² => km²; k + m³ => km³
                addUnique(prefix.symbol + unit.shortName, prefixedValue, localized);
            if (!unit.alternateShortName.isEmpty()
                && shouldAddSiPrefixedShortAlias(prefix.symbol, unit.alternateShortName))
                addUnique(prefix.symbol + unit.alternateShortName, prefixedValue, localized);
        }
    }

    // Automatic binary-prefixed aliases are generated from unit specs.
    // Standalone binary prefixes (kibi/mebi/...) are not registered as units.
    for (const PrefixSpec& prefix : s_binaryPrefixes()) {
        for (const UnitAliasSpec& unit : unitAliasSpecs) {
            if (!(unit.siPrefixPolicy & BinaryPrefixes))
                continue;
            const Quantity prefixedValue = prefix.value * unit.value;
            const QString localized = prefixedLocalizedName(prefix.longName, unit.localized);
            addUnique(prefix.longName + unit.longName, prefixedValue, localized);
            if (!unit.shortName.isEmpty())
                addUnique(prefix.symbol + unit.shortName, prefixedValue, localized);
        }
    }

    return registry;
}

const UnitRegistry& s_unitRegistry()
{
    static const UnitRegistry registry = s_buildUnitRegistry();
    return registry;
}

} // namespace

// Returns built-in unit identifiers mapped to their base quantity values.
const QHash<QString, Quantity>& Units::builtInUnitValues()
{
    return s_unitRegistry().valuesByIdentifier;
}

QHash<QString, Quantity> Units::builtInUnitLookup(char angleMode)
{
    s_runtimeAngleModeForUnits = angleMode;
    QHash<QString, Quantity> lookup;

    const auto& unitValues = Units::builtInUnitValues();
    for (auto it = unitValues.constBegin(); it != unitValues.constEnd(); ++it) {
        const QString& identifier = it.key();
        const Quantity& value = it.value();
        Quantity unitValue = value;
        const bool isInformationUnit =
            unitValue.getDimensionByQuantity().contains(UnitQuantity::Information);
        const UnitId id = unitId(normalizeUnitName(identifier));
        const bool isCanonicalIdentifier =
            id != UnitId::Unknown && identifier == unitName(id);
        const bool keepDisplayUnit =
            (id == UnitId::Steradian)
            || ((id == UnitId::Hertz
              || id == UnitId::Becquerel
              || id == UnitId::Gray
              || id == UnitId::Sievert
              || id == UnitId::Pascal
              || id == UnitId::Newton
              || id == UnitId::Metre)
             && isCanonicalIdentifier)
            || isInformationUnit;
        if (keepDisplayUnit)
            unitValue.setDisplayUnit(value.numericValue(), identifier);
        lookup.insert(identifier, unitValue);
    }

    const auto insertDisplayed = [&](const QString& key,
                                     const Quantity& value,
                                     const QString& displayName) {
        Quantity q(value);
        q.setDisplayUnit(value.numericValue(), displayName);
        lookup.insert(key, q);
    };

    // Force display-name preservation for short symbols/aliases that are
    // intentionally user-facing spellings. Without this, some lookups keep
    // value equivalence but lose the authored display token.
    insertDisplayed(UnitSymbol::Joule, unitValue(UnitId::Joule), unitName(UnitId::Joule));
    insertDisplayed(UnitSymbol::Watt, unitValue(UnitId::Watt), unitName(UnitId::Watt));
    insertDisplayed(UnitSymbol::Volt, unitValue(UnitId::Volt), unitName(UnitId::Volt));
    insertDisplayed(UnitSymbol::Farad, unitValue(UnitId::Farad), unitName(UnitId::Farad));
    insertDisplayed(UnitSymbol::Henry, unitValue(UnitId::Henry), unitName(UnitId::Henry));
    insertDisplayed(UnitSymbol::Tesla, unitValue(UnitId::Tesla), unitName(UnitId::Tesla));
    insertDisplayed(UnitSymbol::Newton, unitValue(UnitId::Newton), unitName(UnitId::Newton));
    insertDisplayed(UnitSymbol::Pascal, unitValue(UnitId::Pascal), unitName(UnitId::Pascal));
    insertDisplayed(UnitName::Ohm, unitValue(UnitId::Ohm), unitName(UnitId::Ohm));
    insertDisplayed(UnitSymbol::Ohm, unitValue(UnitId::Ohm), unitName(UnitId::Ohm));
    insertDisplayed(UnitSymbol::Second, unitValue(UnitId::Second), unitName(UnitId::Second));
    insertDisplayed(UnitSymbol::Hour, unitValue(UnitId::Hour), UnitSymbol::Hour);
    const auto insertAffineDisplayAliases = [&](UnitId id) {
        const auto specIt = s_unitSpecs().constFind(id);
        if (specIt == s_unitSpecs().constEnd())
            return;
        const UnitSpec& spec = specIt.value();
        if (!(spec.affine.toBase && spec.affine.fromBase))
            return;

        insertDisplayed(spec.name, unitValue(UnitId::Kelvin), spec.name);
        if (!spec.symbol.isEmpty())
            insertDisplayed(spec.symbol, Units::kelvin(), spec.symbol);
        for (const QString& alias : spec.aliases) {
            if (alias.isEmpty())
                continue;
            insertDisplayed(alias, Units::kelvin(),
                            spec.symbol.isEmpty() ? spec.name : spec.symbol);
        }
    };
    insertAffineDisplayAliases(UnitId::DegreeCelsius);
    insertAffineDisplayAliases(UnitId::DegreeFahrenheit);

    {
        Quantity hz = unitValue(UnitId::Hertz);
        hz.setDisplayUnit(unitValue(UnitId::Hertz).numericValue(), unitName(UnitId::Hertz));
        lookup.insert(UnitSymbol::Hertz, hz);
    }
    {
        Quantity bq = unitValue(UnitId::Becquerel);
        bq.setDisplayUnit(unitValue(UnitId::Becquerel).numericValue(), unitName(UnitId::Becquerel));
        lookup.insert(UnitSymbol::Becquerel, bq);
    }

    lookup.insert(unitName(UnitId::Radian), angleUnitValueForMode(AngleUnitKind::Radian, angleMode));
    lookup.insert(UnitSymbol::Radian, angleUnitValueForMode(AngleUnitKind::Radian, angleMode));
    lookup.insert(unitName(UnitId::Degree), angleUnitValueForMode(AngleUnitKind::Degree, angleMode));
    lookup.insert(UnitAltSymbol::Degree1, angleUnitValueForMode(AngleUnitKind::Degree, angleMode));
    lookup.insert(UnitSymbol::Degree, angleUnitValueForMode(AngleUnitKind::Degree, angleMode));
    lookup.insert(UnitName::Gradian, angleUnitValueForMode(AngleUnitKind::Gradian, angleMode));
    lookup.insert(UnitSymbol::Gradian, angleUnitValueForMode(AngleUnitKind::Gradian, angleMode));
    lookup.insert(UnitName::Turn, angleUnitValueForMode(AngleUnitKind::Turn, angleMode));
    lookup.insert(UnitSymbol::Turn, angleUnitValueForMode(AngleUnitKind::Turn, angleMode));
    lookup.insert(UnitName::Revolution, angleUnitValueForMode(AngleUnitKind::Turn, angleMode));
    lookup.insert(UnitSymbol::Revolution, angleUnitValueForMode(AngleUnitKind::Turn, angleMode));
    {
        Quantity rpm = angleUnitValueForMode(AngleUnitKind::Turn, angleMode) / Units::minute();
        const QString rpmDisplayName =
            composeAnglePerSecondUnitName(Units::angleModeUnitSymbol(angleMode));
        rpm.setDisplayUnit(rpm.unit(), rpmDisplayName);
        lookup.insert(unitName(UnitId::RevolutionPerMinute), rpm);
        lookup.insert(UnitSymbol::RevolutionPerMinute, rpm);
    }
    lookup.insert(unitName(UnitId::Arcminute), angleUnitValueForMode(AngleUnitKind::Arcminute, angleMode));
    lookup.insert(unitName(UnitId::Arcsecond), angleUnitValueForMode(AngleUnitKind::Arcsecond, angleMode));
    lookup.insert(unitName(UnitId::Milliarcsecond), angleUnitValueForMode(AngleUnitKind::Milliarcsecond, angleMode));
    lookup.insert(unitName(UnitId::Microarcsecond), angleUnitValueForMode(AngleUnitKind::Microarcsecond, angleMode));
    lookup.insert(UnitAltSymbol::Arcminute, angleUnitValueForMode(AngleUnitKind::Arcminute, angleMode));
    lookup.insert(UnitAltSymbol::Arcsecond, angleUnitValueForMode(AngleUnitKind::Arcsecond, angleMode));
    lookup.insert(UnitSymbol::Arcminute, angleUnitValueForMode(AngleUnitKind::Arcminute, angleMode));
    lookup.insert(UnitSymbol::Arcsecond, angleUnitValueForMode(AngleUnitKind::Arcsecond, angleMode));
    lookup.insert(UnitSymbol::Milliarcsecond, angleUnitValueForMode(AngleUnitKind::Milliarcsecond, angleMode));
    lookup.insert(UnitSymbol::Microarcsecond, angleUnitValueForMode(AngleUnitKind::Microarcsecond, angleMode));
    lookup.insert(UnitAltSymbol::MicroarcsecondAscii, angleUnitValueForMode(AngleUnitKind::Microarcsecond, angleMode));

    for (auto it = lookup.begin(); it != lookup.end(); ++it) {
        Quantity prefixedAngleValue;
        if (tryGetPrefixedExplicitAngleUnitValueForMode(it.key(), angleMode, &prefixedAngleValue))
            it.value() = prefixedAngleValue;
    }

    return lookup;
}

bool Units::isExplicitAngleUnitName(const QString& unitName)
{
    return angleUnitKindFromName(unitName) != AngleUnitKind::None;
}

bool Units::isAffineUnitName(const QString& unitName)
{
    return s_unitRegistry().affineByIdentifier.contains(
        UnicodeChars::normalizeUnitSymbolAliases(unitName.trimmed()));
}

bool Units::tryConvertAffineToBase(const QString& unitName,
                                   const HNumber& value,
                                   HNumber* baseValueOut)
{
    if (!baseValueOut)
        return false;

    const QString normalized = UnicodeChars::normalizeUnitSymbolAliases(unitName.trimmed());
    const auto it = s_unitRegistry().affineByIdentifier.constFind(normalized);
    if (it == s_unitRegistry().affineByIdentifier.constEnd() || !it.value().toBase)
        return false;
    *baseValueOut = it.value().toBase(value);
    return true;
}

bool Units::tryConvertAffineFromBase(const QString& unitName,
                                     const HNumber& baseValue,
                                     HNumber* valueOut)
{
    if (!valueOut)
        return false;

    const QString normalized = UnicodeChars::normalizeUnitSymbolAliases(unitName.trimmed());
    const auto it = s_unitRegistry().affineByIdentifier.constFind(normalized);
    if (it == s_unitRegistry().affineByIdentifier.constEnd() || !it.value().fromBase)
        return false;
    *valueOut = it.value().fromBase(baseValue);
    return true;
}

// Converts explicit-angle quantities to radians in-place and strips units.
bool Units::tryConvertExplicitAngleToRadians(Quantity* angle)
{
    if (!angle || !angle->hasUnit())
        return false;

    const AngleUnitKind unitKind = angleUnitKindFromName(angle->unitName());
    if (unitKind == AngleUnitKind::None)
        return false;

    if (unitKind == AngleUnitKind::Degree)
        *angle = DMath::deg2rad(*angle);
    else if (unitKind == AngleUnitKind::Gradian)
        *angle = DMath::gon2rad(*angle);
    else if (unitKind == AngleUnitKind::Turn)
        *angle *= Quantity(2) * DMath::pi();
    else if (unitKind == AngleUnitKind::Arcminute)
        *angle = DMath::deg2rad(*angle / Quantity(60));
    else if (unitKind == AngleUnitKind::Arcsecond)
        *angle = DMath::deg2rad(*angle / Quantity(3600));
    else if (unitKind == AngleUnitKind::Milliarcsecond)
        *angle = DMath::deg2rad(*angle / Quantity(3600000));
    else if (unitKind == AngleUnitKind::Microarcsecond)
        *angle = DMath::deg2rad(*angle / HNumber("3600000000"));

    angle->stripUnits();
    return true;
}

DEFINE_BASE_UNIT(metre)
DEFINE_BASE_UNIT(second)
DEFINE_BASE_UNIT(kilogram)
DEFINE_BASE_UNIT(ampere)
DEFINE_BASE_UNIT(mole)
DEFINE_BASE_UNIT(kelvin)
DEFINE_BASE_UNIT(candela)
DEFINE_BASE_UNIT(byte) // Internal primitive for the information dimension (not an SI base unit).

DEFINE_DERIVED_UNIT(newton, Units::metre() * Units::kilogram() / (Units::second()*Units::second()))
DEFINE_DERIVED_UNIT(hertz, Quantity(1) / Units::second())
DEFINE_DERIVED_UNIT(radian, Quantity(1))
DEFINE_DERIVED_UNIT(degree, HMath::pi() * Units::radian() / HNumber(180))
DEFINE_DERIVED_UNIT(gradian, HMath::pi() / HNumber(200))
DEFINE_DERIVED_UNIT(turn, HNumber(2) * HMath::pi() * Units::radian())
DEFINE_DERIVED_UNIT(revolution, HNumber(2) * HMath::pi() * Units::radian())
DEFINE_DERIVED_UNIT(arcminute, Units::degree() / HNumber(60))
DEFINE_DERIVED_UNIT(arcsecond, Units::arcminute() / HNumber(60))
DEFINE_DERIVED_UNIT(milliarcsecond, Units::milli() * Units::arcsecond())
DEFINE_DERIVED_UNIT(microarcsecond, Units::micro() * Units::arcsecond())
DEFINE_DERIVED_UNIT(pascal, Units::newton() / Units::square_metre())
DEFINE_DERIVED_UNIT(joule, Units::newton() * Units::metre())
DEFINE_DERIVED_UNIT(watt, Units::joule() / Units::second())
DEFINE_DERIVED_UNIT(coulomb, Units::ampere() * Units::second())
DEFINE_DERIVED_UNIT(volt, Units::watt() / Units::ampere())
DEFINE_DERIVED_UNIT(farad, Units::coulomb() / Units::volt())
DEFINE_DERIVED_UNIT(ohm, Units::volt() / Units::ampere())
DEFINE_DERIVED_UNIT(siemens, Units::ampere() / Units::volt())
DEFINE_DERIVED_UNIT(weber, Units::volt() * Units::second())
DEFINE_DERIVED_UNIT(tesla, Units::weber() / Units::square_metre())
DEFINE_DERIVED_UNIT(henry, Units::weber() / Units::ampere())
DEFINE_DERIVED_UNIT(becquerel, Quantity(1) / Units::second())
DEFINE_DERIVED_UNIT(gray, Units::joule() / Units::kilogram())
DEFINE_DERIVED_UNIT(sievert, Units::joule() / Units::kilogram())
DEFINE_DERIVED_UNIT(katal, Units::mole() / Units::second())
DEFINE_DERIVED_UNIT(steradian, Quantity(1))
DEFINE_DERIVED_UNIT(lumen, Units::candela()*Units::steradian())
DEFINE_DERIVED_UNIT(lux, Units::lumen()/Units::square_metre())
DEFINE_DERIVED_UNIT(electronvolt, HNumber("1.602176634e-19") * Units::joule())

DEFINE_DERIVED_UNIT(tonne, Units::mega() * Units::gram())
DEFINE_DERIVED_UNIT(gram, Units::milli() * Units::kilogram())
DEFINE_DERIVED_UNIT(pound, HNumber("0.45359237") * Units::kilogram()) // International avoirdupois.
DEFINE_DERIVED_UNIT(stone, HNumber(14) * Units::pound())
DEFINE_DERIVED_UNIT(ounce, Units::pound() / HNumber(16))
DEFINE_DERIVED_UNIT(grain, Units::pound() / HNumber(7000))
DEFINE_DERIVED_UNIT(short_ton, HNumber(2000) * Units::pound())
DEFINE_DERIVED_UNIT(long_ton, HNumber(2240) * Units::pound())

DEFINE_DERIVED_UNIT(atomic_mass_unit, HNumber("1.66053906892e-27") * Units::kilogram()) // CODATA 2022: https://physics.nist.gov/cgi-bin/cuu/Value?ukg
DEFINE_DERIVED_UNIT(carat, HNumber(200) * Units::milli()*Units::gram()) // Do not confuse with karat below.
DEFINE_DERIVED_UNIT(angstrom, HNumber("1e-10") * Units::metre() )

DEFINE_DERIVED_UNIT(astronomical_unit, HNumber("149597870700") * Units::metre()) // IAU 2012 Resolution B2.
DEFINE_DERIVED_UNIT(lightyear, Units::speed_of_light() * Units::julian_year())
DEFINE_DERIVED_UNIT(lightminute, Units::speed_of_light() * Units::minute())
DEFINE_DERIVED_UNIT(lightsecond, Units::speed_of_light() * Units::second())
DEFINE_DERIVED_UNIT(parsec, HNumber(648000)/HMath::pi() * Units::astronomical_unit()) // IAU 2015 Resolution B2.

DEFINE_DERIVED_UNIT(inch, HNumber("0.0254") * Units::metre()) // International inch.
DEFINE_DERIVED_UNIT(square_inch, Units::inch() * Units::inch())
DEFINE_DERIVED_UNIT(cubic_inch, Units::inch() * Units::inch() * Units::inch())
DEFINE_DERIVED_UNIT(foot, HNumber(12) * Units::inch())
DEFINE_DERIVED_UNIT(square_foot, Units::foot() * Units::foot())
DEFINE_DERIVED_UNIT(cubic_foot, Units::foot() * Units::foot() * Units::foot())
DEFINE_DERIVED_UNIT(yard, HNumber(36) * Units::inch())
DEFINE_DERIVED_UNIT(square_yard, Units::yard() * Units::yard())
DEFINE_DERIVED_UNIT(cubic_yard, Units::yard() * Units::yard() * Units::yard())
DEFINE_DERIVED_UNIT(mile, HNumber(1760) * Units::yard())
DEFINE_DERIVED_UNIT(square_mile, Units::mile() * Units::mile())
DEFINE_DERIVED_UNIT(cubic_mile, Units::mile() * Units::mile() * Units::mile())
DEFINE_DERIVED_UNIT(rod, HNumber("5.5") * Units::yard())
DEFINE_DERIVED_UNIT(furlong, HNumber(40) * Units::rod())
DEFINE_DERIVED_UNIT(fathom, HNumber(6) * Units::foot())
DEFINE_DERIVED_UNIT(nautical_mile, HNumber("1852") * Units::metre()) // UCUM: [nmi_i]

DEFINE_DERIVED_UNIT(square_metre, Units::metre() * Units::metre())
DEFINE_DERIVED_UNIT(hectare, HNumber(10000) * Units::square_metre())
DEFINE_DERIVED_UNIT(acre, Units::mile() * Units::mile() / HNumber(640))

DEFINE_DERIVED_UNIT(cubic_metre, Units::square_metre() * Units::metre())
DEFINE_DERIVED_UNIT(cubic_millimetre, Units::milli() * Units::milli() * Units::milli() * Units::cubic_metre())
DEFINE_DERIVED_UNIT(cubic_centimetre, Units::centi() * Units::centi() * Units::centi() * Units::cubic_metre())
DEFINE_DERIVED_UNIT(cubic_decimetre, Units::deci() * Units::deci() * Units::deci() * Units::cubic_metre())
DEFINE_DERIVED_UNIT(cubic_kilometre, Units::kilo() * Units::kilo() * Units::kilo() * Units::cubic_metre())
DEFINE_DERIVED_UNIT(square_millimetre, Units::milli() * Units::milli() * Units::square_metre())
DEFINE_DERIVED_UNIT(square_kilometre, Units::kilo() * Units::kilo() * Units::square_metre())
DEFINE_DERIVED_UNIT(litre, Units::milli() * Units::cubic_metre())
DEFINE_DERIVED_UNIT(gallon_us, HNumber("3.785411784") * Units::litre())
DEFINE_DERIVED_UNIT(gallon_imp, HNumber("4.54609") * Units::litre())
DEFINE_DERIVED_UNIT(quart_us, Units::gallon_us() / HNumber(4))
DEFINE_DERIVED_UNIT(quart_imp, Units::gallon_imp() / HNumber(4))
DEFINE_DERIVED_UNIT(pint_us, Units::gallon_us() / HNumber(8))
DEFINE_DERIVED_UNIT(pint_imp, Units::gallon_imp() / HNumber(8))
DEFINE_DERIVED_UNIT(gill_us, Units::pint_us() / HNumber(4))
DEFINE_DERIVED_UNIT(gill_imp, Units::pint_imp() / HNumber(4))
DEFINE_DERIVED_UNIT(fluid_ounce_us, Units::gallon_us() / HNumber(128))
DEFINE_DERIVED_UNIT(fluid_ounce_imp, Units::gallon_imp() / HNumber(160))
DEFINE_DERIVED_UNIT(fluid_dram_us, Units::fluid_ounce_us() / HNumber(8))
DEFINE_DERIVED_UNIT(fluid_dram_imp, Units::fluid_ounce_imp() / HNumber(8))
DEFINE_DERIVED_UNIT(barrel_oil, HNumber(42) * Units::gallon_us())
DEFINE_DERIVED_UNIT(barrel_beer_us, HNumber("31.5") * Units::gallon_us())
DEFINE_DERIVED_UNIT(teaspoon, HNumber(5) * Units::milli()*Units::litre())
DEFINE_DERIVED_UNIT(teaspoon_imp, Units::gallon_imp() / HNumber(768))
DEFINE_DERIVED_UNIT(teaspoon_us, Units::gallon_us() / HNumber(768))
DEFINE_DERIVED_UNIT(dessert_spoon, HNumber(10) * Units::milli()*Units::litre())
DEFINE_DERIVED_UNIT(tablespoon, HNumber(15) * Units::milli()*Units::litre())
DEFINE_DERIVED_UNIT(tablespoon_au, HNumber("0.02") * Units::litre())
DEFINE_DERIVED_UNIT(tablespoon_imp, Units::gallon_imp() / HNumber(256))
DEFINE_DERIVED_UNIT(tablespoon_us, Units::gallon_us() / HNumber(256))
DEFINE_DERIVED_UNIT(cup, HNumber(240) * Units::milli()*Units::litre())
DEFINE_DERIVED_UNIT(cup_imp, Units::gallon_imp() / HNumber(16))
DEFINE_DERIVED_UNIT(cup_jp, HNumber("0.2") * Units::litre())
DEFINE_DERIVED_UNIT(cup_us, Units::gallon_us() / HNumber(16))

DEFINE_DERIVED_UNIT(minute, HNumber(60) * Units::second())
DEFINE_DERIVED_UNIT(hour, HNumber(60) * Units::minute())
DEFINE_DERIVED_UNIT(day, HNumber(24) * Units::hour())
DEFINE_DERIVED_UNIT(week, HNumber(7) * Units::day())
DEFINE_DERIVED_UNIT(julian_year, HNumber("365.25") * Units::day())
DEFINE_DERIVED_UNIT(julian_century, HNumber(100) * Units::julian_year())
DEFINE_DERIVED_UNIT(tropical_year, HNumber("365.242190402") * Units::day()) // Approx.: changes over time due to Earth's precession.
DEFINE_DERIVED_UNIT(sidereal_year, HNumber("365.256363004") * Units::day()) // http://hpiers.obspm.fr/eop-pc/models/constants.html

DEFINE_DERIVED_UNIT(karat, Rational(1,24).toHNumber()) // Do not confuse with carat above.

DEFINE_DERIVED_UNIT(bar, HNumber("1e5") * Units::pascal())
DEFINE_DERIVED_UNIT(atmosphere, HNumber("1.01325") * Units::bar())
DEFINE_DERIVED_UNIT(torr, Units::atmosphere() / HNumber(760))
DEFINE_DERIVED_UNIT(pounds_per_sqinch, Units::pound() * Units::gravity() / (Units::inch()*Units::inch()))

DEFINE_DERIVED_UNIT(calorie, HNumber("4.184") * Units::joule()) // International Table calorie.
DEFINE_DERIVED_UNIT(british_thermal_unit, HNumber("1055.05585262") * Units::joule()) // International standard ISO 31-4
DEFINE_DERIVED_UNIT(quad, Units::peta() * Units::british_thermal_unit())

DEFINE_DERIVED_UNIT(nat, Units::bit() / HMath::ln(2))
DEFINE_DERIVED_UNIT(hartley, HMath::ln(10) * Units::nat())
DEFINE_DERIVED_UNIT(bit, Units::byte() / HNumber(8))

DEFINE_DERIVED_UNIT(gravity, HNumber("9.80665") * Units::newton() / Units::kilogram()) // 3rd CGPM (1901, CR 70).
DEFINE_DERIVED_UNIT(speed_of_light, HNumber(299792458) * Units::metre() / Units::second())
DEFINE_DERIVED_UNIT(speed_of_sound_STP, HNumber(331) * Units::metre()/Units::second())
DEFINE_DERIVED_UNIT(knot, Units::nautical_mile()/Units::hour())
DEFINE_DERIVED_UNIT(mile_per_hour, HNumber("1609.344") * Units::metre() / Units::hour())
DEFINE_DERIVED_UNIT(kilometre_per_hour, Units::kilo() * Units::metre() / Units::hour())
DEFINE_DERIVED_UNIT(revolution_per_minute, Units::revolution() / Units::minute())

DEFINE_DERIVED_UNIT(horsepower, HNumber(550) * Units::foot() * Units::pound() * Units::gravity() / Units::second()) // Imperial horsepower.
DEFINE_DERIVED_UNIT(kilowatt_hour, Units::kilo() * Units::watt() * Units::hour())
DEFINE_DERIVED_UNIT(millimetre_of_mercury, HNumber("133.322387415") * Units::pascal())

#define SI_PREFIX_CACHE(id, func, name, symbol, exponent) \
    DEFINE_DERIVED_UNIT(func, s_powerOfTen(exponent))
SI_PREFIX_TABLE(SI_PREFIX_CACHE)
#undef SI_PREFIX_CACHE

#define BINARY_PREFIX_CACHE(id, func, name, symbol, exponent) \
    DEFINE_DERIVED_UNIT(func, s_powerOfTwo(exponent))
BINARY_PREFIX_TABLE(BINARY_PREFIX_CACHE)
#undef BINARY_PREFIX_CACHE

#undef SI_PREFIX_TABLE
#undef BINARY_PREFIX_TABLE
