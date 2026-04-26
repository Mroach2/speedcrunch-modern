// This file is part of the SpeedCrunch project
// Copyright (C) 2015 Pol Welter <polwelter@gmail.com>
// Copyright (C) 2016-2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; see the file COPYING.  If not, write to
// the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301, USA.

#ifndef UNITS_H
#define UNITS_H

#include <QHash>
#include <QList>
#include <QMap>
#include "math/quantity.h"
#include "math/rational.h"

class QString;
class QStringView;
class Rational;

enum class UnitQuantity {
    Unspecified,
    Length,
    Time,
    Mass,
    ElectricCurrent,
    ThermodynamicTemperature,
    AmountOfSubstance,
    LuminousIntensity,
    Information,
    PlaneAngle,
    SolidAngle,
    Frequency,
    Force,
    Pressure,
    Stress,
    Energy,
    Work,
    AmountOfHeat,
    Power,
    RadiantFlux,
    ElectricCharge,
    ElectricPotentialDifference,
    Capacitance,
    ElectricResistance,
    ElectricConductance,
    MagneticFlux,
    MagneticFluxDensity,
    Inductance,
    CelsiusTemperature,
    LuminousFlux,
    Illuminance,
    ActivityReferredToARadionuclide,
    AbsorbedDose,
    Kerma,
    DoseEquivalent,
    CatalyticActivity,
    Area,
    Volume,
    Speed,
    Velocity,
    DimensionlessRatio
};

enum class UnitId {
    Unknown,
    Second,
    Metre,
    Kilogram,
    Ampere,
    Kelvin,
    Mole,
    Candela,
    Radian,
    Steradian,
    Hertz,
    Newton,
    Pascal,
    Joule,
    Watt,
    Coulomb,
    Volt,
    Farad,
    Ohm,
    Siemens,
    Weber,
    Tesla,
    Henry,
    DegreeCelsius,
    Lumen,
    Lux,
    Becquerel,
    Gray,
    Sievert,
    Katal,
    CubicMetre,
    CubicMillimetre,
    CubicCentimetre,
    CubicDecimetre,
    CubicKilometre,
    SquareMillimetre,
    SquareKilometre,
    SquareMetre,
    Minute,
    Hour,
    Day,
    AstronomicalUnit,
    Degree,
    Arcminute,
    Arcsecond,
    Milliarcsecond,
    Microarcsecond,
    Hectare,
    Litre,
    Tonne,
    Dalton,
    Electronvolt,
    Acre,
    Angstrom,
    Atmosphere,
    AtomicMassUnit,
    Bar,
    Bit,
    BritishThermalUnit,
    Byte,
    Calorie,
    Carat,
    Cup,
    CupImp,
    CupJp,
    CupUs,
    DegreeFahrenheit,
    ElementaryCharge,
    Fathom,
    FluidOunceImp,
    FluidOunceUs,
    FluidDramImp,
    FluidDramUs,
    Foot,
    SquareFoot,
    CubicFoot,
    Furlong,
    GallonImp,
    GallonUs,
    GillImp,
    GillUs,
    Gradian,
    Grain,
    Gram,
    Hartley,
    HartreeEnergyUnit,
    Horsepower,
    Inch,
    SquareInch,
    CubicInch,
    Karat,
    Knot,
    Lightminute,
    Lightsecond,
    Lightyear,
    LongTon,
    Mile,
    SquareMile,
    CubicMile,
    MilePerHour,
    KilometrePerHour,
    Nat,
    NauticalMile,
    Ounce,
    Parsec,
    PartsPerBillion,
    PartsPerMillion,
    PintImp,
    PintUs,
    Pound,
    PoundsPerSqinch,
    QuartImp,
    QuartUs,
    BarrelOil,
    BarrelBeerUs,
    Rod,
    SquareYard,
    CubicYard,
    ShortTon,
    Stone,
    Tablespoon,
    TablespoonAu,
    TablespoonImp,
    TablespoonUs,
    DessertSpoon,
    Teaspoon,
    TeaspoonImp,
    TeaspoonUs,
    Torr,
    Turn,
    Revolution,
    RevolutionPerMinute,
    Week,
    KilowattHour,
    MillimetreOfMercury,
    Quad,
    Yard,
    YearJulian,
    YearSidereal,
    YearTropical
};

enum class UnitPhraseId {
    NewtonMetre,
    WattSecond,
    VoltSquaredAmpere,
    JoulePerSquareMetre,
    JoulePerCubicMetre
};

enum class PrefixId {
    Quecto,
    Ronto,
    Yocto,
    Zepto,
    Atto,
    Femto,
    Pico,
    Nano,
    Micro,
    Milli,
    Centi,
    Deci,
    Deca,
    Hecto,
    Kilo,
    Mega,
    Giga,
    Tera,
    Peta,
    Exa,
    Zetta,
    Yotta,
    Ronna,
    Quetta,
    Kibi,
    Mebi,
    Gibi,
    Tebi,
    Pebi,
    Exbi,
    Zebi,
    Yobi,
    Robi,
    Quebi,
    Unknown
};

using UnitDimension = QMap<UnitQuantity, Rational>;

QString unitQuantityDimensionKey(UnitQuantity quantity);
bool unitQuantityFromDimensionKey(const QString& key, UnitQuantity* out);
QString normalizeUnitName(const QString& name);
UnitId unitId(const QString& normalizedName);
QString unitName(UnitId id);
QString unitSymbol(UnitId id);
bool isPreferredCanonicalDisplayUnit(UnitId id);
QStringView unitPhrase(UnitPhraseId key);
PrefixId prefixId(const QString& normalizedName);
QString prefixName(PrefixId id);
QString prefixSymbol(PrefixId id);
const QList<PrefixId>& siPrefixIds();
const QList<PrefixId>& binaryPrefixIds();

struct Unit {
    QString name;
    Quantity value;
    Unit(QString name, Quantity value)
        : name(name)
        , value(value)
    { }
    Unit()
        : name("")
        , value(1)
    { }
};

class Units {
public:
    static QString angleModeUnitSymbol(char angleMode);
    static QString degreeAliasSymbol();
    static void findUnit(Quantity& q);
    static const QHash<QString, Quantity>& builtInUnitValues();
    static QHash<QString, Quantity> builtInUnitLookup(char angleMode);
    static bool isAffineUnitName(const QString& unitName);
    static bool tryConvertAffineToBase(const QString& unitName,
                                       const HNumber& value,
                                       HNumber* baseValueOut);
    static bool tryConvertAffineFromBase(const QString& unitName,
                                         const HNumber& baseValue,
                                         HNumber* valueOut);
    static bool isExplicitAngleUnitName(const QString& unitName);
    static bool tryConvertExplicitAngleToRadians(Quantity* angle);

    // Base SI units.
    static const Quantity metre();
    static const Quantity second();
    static const Quantity kilogram();
    static const Quantity ampere();
    static const Quantity mole();
    static const Quantity kelvin();
    static const Quantity candela();

    // Base non-SI units.
    static const Quantity bit();

    // SI prefixes.
    static const Quantity quecto();
    static const Quantity ronto();
    static const Quantity yocto();
    static const Quantity zepto();
    static const Quantity atto();
    static const Quantity femto();
    static const Quantity pico();
    static const Quantity nano();
    static const Quantity micro();
    static const Quantity milli();
    static const Quantity centi();
    static const Quantity deci();

    static const Quantity deca();
    static const Quantity hecto();
    static const Quantity kilo();
    static const Quantity mega();
    static const Quantity giga();
    static const Quantity tera();
    static const Quantity peta();
    static const Quantity exa();
    static const Quantity zetta();
    static const Quantity yotta();
    static const Quantity ronna();
    static const Quantity quetta();

    // Binary prefixes.
    static const Quantity kibi();
    static const Quantity mebi();
    static const Quantity gibi();
    static const Quantity tebi();
    static const Quantity pebi();
    static const Quantity exbi();
    static const Quantity zebi();
    static const Quantity yobi();
    static const Quantity robi();
    static const Quantity quebi();

    // Derived SI units.
    static const Quantity square_metre();
    static const Quantity cubic_metre();
    static const Quantity cubic_millimetre();
    static const Quantity cubic_centimetre();
    static const Quantity cubic_decimetre();
    static const Quantity cubic_kilometre();
    static const Quantity square_millimetre();
    static const Quantity square_kilometre();
    static const Quantity newton();
    static const Quantity hertz();
    static const Quantity radian();
    static const Quantity degree();
    static const Quantity gradian();
    static const Quantity turn();
    static const Quantity revolution();
    static const Quantity arcminute();
    static const Quantity arcsecond();
    static const Quantity milliarcsecond();
    static const Quantity microarcsecond();
    static const Quantity steradian();
    static const Quantity pascal();
    static const Quantity joule();
    static const Quantity watt();
    static const Quantity coulomb();
    static const Quantity volt();
    static const Quantity farad();
    static const Quantity ohm();
    static const Quantity siemens();
    static const Quantity weber();
    static const Quantity tesla();
    static const Quantity henry();
    static const Quantity lumen();
    static const Quantity lux();
    static const Quantity becquerel();
    static const Quantity gray();
    static const Quantity sievert();
    static const Quantity katal();

    // Derived from SI units.
    // Mass.
    static const Quantity tonne();
    static const Quantity short_ton();
    static const Quantity long_ton();
    static const Quantity pound();
    static const Quantity ounce();
    static const Quantity grain();
    static const Quantity gram();
    static const Quantity atomic_mass_unit();
    static const Quantity carat();
    // Distance/length.
    static const Quantity micron();
    static const Quantity angstrom();
    static const Quantity astronomical_unit();
    static const Quantity lightyear();
    static const Quantity lightsecond();
    static const Quantity lightminute();
    static const Quantity parsec();
    // US measures.
    static const Quantity inch();
    static const Quantity square_inch();
    static const Quantity foot();
    static const Quantity square_foot();
    static const Quantity yard();
    static const Quantity square_yard();
    static const Quantity cubic_yard();
    static const Quantity mile();
    static const Quantity square_mile();
    static const Quantity cubic_mile();
    static const Quantity rod();
    static const Quantity furlong();
    // Nautical (US).
    static const Quantity fathom();
    static const Quantity nautical_mile();
    static const Quantity cable();
    // Area.
    static const Quantity are();
    static const Quantity hectare();
    static const Quantity decare();
    static const Quantity acre();
    // Volume.
    static const Quantity gallon_us();
    static const Quantity gallon_imp();
    static const Quantity quart_us();
    static const Quantity quart_imp();
    static const Quantity pint_us();
    static const Quantity pint_imp();
    static const Quantity fluid_ounce_us();
    static const Quantity fluid_ounce_imp();
    static const Quantity fluid_dram_us();
    static const Quantity fluid_dram_imp();
    static const Quantity gill_us();
    static const Quantity gill_imp();
    static const Quantity barrel_oil();
    static const Quantity barrel_beer_us();
    static const Quantity cubic_inch();
    static const Quantity cubic_foot();
    static const Quantity litre();
    // Time.
    static const Quantity minute();
    static const Quantity hour();
    static const Quantity day();
    static const Quantity week();
    static const Quantity century();
    static const Quantity julian_year();
    static const Quantity tropical_year();
    static const Quantity sidereal_year();
    // Concentration.
    static const Quantity percent();
    static const Quantity ppm();
    static const Quantity ppb();
    static const Quantity karat();
    // Pressure.
    static const Quantity bar();
    static const Quantity atmosphere();
    static const Quantity torr();
    static const Quantity pounds_per_sqinch();
    // Energy.
    static const Quantity electronvolt();
    static const Quantity calorie();
    static const Quantity british_thermal_unit();
    // Information.
    static const Quantity nat();
    static const Quantity hartley();
    static const Quantity byte();
    // Cooking.
    // Note: these again differ from US to UK, Australia, Japan, ...
    // Since for cooking generally not that high a precision is
    // required, let's just stick with the so called 'legal' variant.
    static const Quantity tablespoon();
    static const Quantity tablespoon_au();
    static const Quantity tablespoon_imp();
    static const Quantity tablespoon_us();
    static const Quantity teaspoon();
    static const Quantity teaspoon_imp();
    static const Quantity teaspoon_us();
    static const Quantity cup();
    static const Quantity cup_imp();
    static const Quantity cup_jp();
    static const Quantity cup_us();
    // Various others.
    static const Quantity gravity();
    static const Quantity speed_of_light();
    static const Quantity elementary_charge();
    static const Quantity speed_of_sound_STP();
    static const Quantity knot();
    static const Quantity mile_per_hour();
    static const Quantity kilometre_per_hour();
    static const Quantity revolution_per_minute();
    static const Quantity horsepower();
    static const Quantity kilowatt_hour();
    static const Quantity millimetre_of_mercury();
    static const Quantity stone();
    static const Quantity dessert_spoon();
    static const Quantity quad();

private:
    static void pushUnit(Quantity q, QString name);
    static void clearCache() { m_cache.clear(); }
    static QHash<QMap<UnitQuantity, Rational>, Unit> m_matchLookup;
    static QMap<QString, Quantity> m_cache;
    static void initTable();
};

#endif // UNITS_H
