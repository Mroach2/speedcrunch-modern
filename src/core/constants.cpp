// SPDX-FileCopyrightText: 2007-2011, 2013-2014, 2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/constants.h"

#include "core/numberformatter.h"
#include "math/hmath.h"

#include <QCoreApplication>

#include <algorithm>

static Constants* s_constantsInstance = nullptr;

static void s_deleteConstants()
{
    delete s_constantsInstance;
}

struct Constants::Private
{
    QStringList domains;
    QList<Constant> list;

    void populate();
    void retranslateText();
};

// Internal domain/subdomain identifiers used in constant declarations.

static QString domainToDisplay(ConstantDomain domain)
{
    switch (domain) {
    case ConstantDomain::Mathematics:
        return Constants::tr("Mathematics");
    case ConstantDomain::PhysicsCodata2022:
        return Constants::tr("Physics (CODATA 2022)");
    case ConstantDomain::ChemistryIupacCiaaw2021:
        return Constants::tr("Chemistry (IUPAC - CIAAW 2021)");
    case ConstantDomain::Astronomy:
        return Constants::tr("Astronomy");
    case ConstantDomain::Uncategorized:
    default:
        return Constants::tr("Uncategorized");
    }
}

static QString subdomainToDisplay(ConstantSubdomain subdomain)
{
    switch (subdomain) {
    case ConstantSubdomain::Universal:
        return Constants::tr("Universal");
    case ConstantSubdomain::Electromagnetic:
        return Constants::tr("Electromagnetic");
    case ConstantSubdomain::Physicochemical:
        return Constants::tr("Physicochemical");
    case ConstantSubdomain::AtomicNuclearGeneral:
        return Constants::tr("Atomic & Nuclear  — General");
    case ConstantSubdomain::AtomicNuclearElectroweak:
        return Constants::tr("Atomic & Nuclear  — Electroweak");
    case ConstantSubdomain::AtomicNuclearElectron:
        return Constants::tr("Atomic & Nuclear  — Electron");
    case ConstantSubdomain::AtomicNuclearMuon:
        return Constants::tr("Atomic & Nuclear  — Muon");
    case ConstantSubdomain::AtomicNuclearTau:
        return Constants::tr("Atomic & Nuclear  — Tau");
    case ConstantSubdomain::AtomicNuclearProton:
        return Constants::tr("Atomic & Nuclear  — Proton");
    case ConstantSubdomain::AtomicNuclearNeutron:
        return Constants::tr("Atomic & Nuclear  — Neutron");
    case ConstantSubdomain::AtomicNuclearDeuteron:
        return Constants::tr("Atomic & Nuclear  — Deuteron");
    case ConstantSubdomain::AtomicNuclearTriton:
        return Constants::tr("Atomic & Nuclear  — Triton");
    case ConstantSubdomain::AtomicNuclearHelion:
        return Constants::tr("Atomic & Nuclear  — Helion");
    case ConstantSubdomain::AtomicNuclearAlphaParticle:
        return Constants::tr("Atomic & Nuclear  — Alpha particle");
    case ConstantSubdomain::AtomicNuclearAtomicUnits:
        return Constants::tr("Atomic & Nuclear — Atomic units");
    case ConstantSubdomain::AtomicNuclearEnergyConversionRelationships:
        return Constants::tr("Atomic & Nuclear — Energy conversion relationships");
    case ConstantSubdomain::AtomicNuclearXrayUnits:
        return Constants::tr("Atomic & Nuclear — X-ray units");
    case ConstantSubdomain::AtomicNuclearMagneticShieldingCorrections:
        return Constants::tr("Atomic & Nuclear — Magnetic shielding corrections");
    case ConstantSubdomain::Masses:
        return Constants::tr("Masses");
    case ConstantSubdomain::Lifetimes:
        return Constants::tr("Lifetimes");
    case ConstantSubdomain::MolarMasses:
        return Constants::tr("Molar Masses");
    case ConstantSubdomain::Electronegativity:
        return Constants::tr("Electronegativity");
    case ConstantSubdomain::IonizationEnergy:
        return Constants::tr("Ionization Energy");
    case ConstantSubdomain::AstronomyNominalIau2015:
        return Constants::tr("Nominal Constants (IAU 2015)");
    case ConstantSubdomain::AstronomyCurrentBestEstimatesIauNsfa202604:
        return Constants::tr("Current Best Estimates (IAU NSFA 2026-04)");
    case ConstantSubdomain::None:
    default:
        return QString();
    }
}

#define SET_CONSTANT_CONTEXT(DOMAIN,SUBDOMAIN) \
    domain = DOMAIN; \
    subdomain = SUBDOMAIN;

#define PUSH_CONSTANT(NAME,VALUE,UNIT) \
    constant.domainType = domain; \
    constant.subdomainType = subdomain; \
    constant.name = Constants::tr(NAME); \
    constant.value = QLatin1String(VALUE); \
    constant.unit  = QString::fromUtf8(UNIT); \
    constant.domain = domainToDisplay(constant.domainType); \
    constant.subdomain = subdomainToDisplay(constant.subdomainType); \
    list << constant;

void Constants::Private::populate()
{
    domains.clear();
    list.clear();

    /*
        PHYSICS: CODATA 2022
        https://physics.nist.gov/cuu/Constants/

        PARTICLE PHYSICS: PDG 2024
        https://pdg.lbl.gov/2024/download/db2024.pdf (relevant info starts at PDF page 8 / document page 6 )
        https://journals.aps.org/prd/pdf/10.1103/PhysRevD.110.030001 (relevant info starts at page 137)

        CHEMISTRY: IUPAC - CIAAW 2021
        https://www.ciaaw.org/atomic-weights.htm
        https://doi.org/10.1515/pac-2019-0603
        https://pubchem.ncbi.nlm.nih.gov/periodic-table/

        Astronomy — Nominal Constants (IAU 2015)
        https://arxiv.org/pdf/1510.07674

        Astronomy — Current Best Estimates (IAU NSFA 2026-04)
        https://iau-a3.gitlab.io/NSFA/NSFA_cbe
    */

    Constant constant;
    ConstantDomain domain;
    ConstantSubdomain subdomain;

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::Universal);
    PUSH_CONSTANT(QT_TR_NOOP("characteristic impedance of vacuum (Z₀)"), "376.730313412", "Ω");
    PUSH_CONSTANT(QT_TR_NOOP("reduced Planck constant (ℏ)"), "1.054571817e-34", "J·s");
    PUSH_CONSTANT(QT_TR_NOOP("vacuum electric permittivity (ϵ₀)"), "8.8541878188e-12", "F·m⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Newtonian constant of gravitation (G)"), "6.67430e-11", "m³·kg⁻¹·s⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("vacuum magnetic permeability (μ₀)"), "1.25663706127e-6", "N·A⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("Planck constant (h)"), "6.62607015e-34", "J·s");
    PUSH_CONSTANT(QT_TR_NOOP("speed of light in vacuum (c)"), "299792458", "m·s⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("standard acceleration of gravity (gₙ)"), "9.80665", "m·s⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("Newtonian constant of gravitation over h-bar c"), "6.70883e-39", "(GeV/c²)⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("Planck length"), "1.616255e-35", "m");
    PUSH_CONSTANT(QT_TR_NOOP("Planck constant in eV/Hz (h)"), "4.135667696e-15", "eV·Hz⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Planck mass"), "2.176434e-8", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("Planck mass energy equivalent in GeV"), "1.220890e19", "GeV");
    PUSH_CONSTANT(QT_TR_NOOP("Planck temperature"), "1.416784e32", "K");
    PUSH_CONSTANT(QT_TR_NOOP("Planck time"), "5.391247e-44", "s");
    PUSH_CONSTANT(QT_TR_NOOP("reduced Planck constant in eV·s (ℏ)"), "6.582119569e-16", "eV·s");
    PUSH_CONSTANT(QT_TR_NOOP("reduced Planck constant times c in MeV·fm"), "197.3269804", "MeV·fm");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::Electromagnetic);
    PUSH_CONSTANT(QT_TR_NOOP("Bohr magneton"), "9.2740100657e-24", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("conductance quantum (G₀)"), "7.748091729e-5", "S");
    PUSH_CONSTANT(QT_TR_NOOP("Coulomb constant (kₑ)"), "8.98755178617079867048e9", "N·m²·C⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("elementary charge (e)"), "1.602176634e-19", "C");
    PUSH_CONSTANT(QT_TR_NOOP("elementary charge over h-bar"), "1.519267447e15", "A·J⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("conventional value of Josephson constant"), "483597.9e9", "Hz·V⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Josephson constant"), "483597.8484e9", "Hz·V⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("conventional value of ampere-90"), "1.00000008887", "A");
    PUSH_CONSTANT(QT_TR_NOOP("conventional value of coulomb-90"), "1.00000008887", "C");
    PUSH_CONSTANT(QT_TR_NOOP("conventional value of farad-90"), "0.99999998220", "F");
    PUSH_CONSTANT(QT_TR_NOOP("conventional value of henry-90"), "1.00000001779", "H");
    PUSH_CONSTANT(QT_TR_NOOP("conventional value of ohm-90"), "1.00000001779", "Ω");
    PUSH_CONSTANT(QT_TR_NOOP("conventional value of volt-90"), "1.00000010666", "V");
    PUSH_CONSTANT(QT_TR_NOOP("conventional value of watt-90"), "1.00000019553", "W");
    PUSH_CONSTANT(QT_TR_NOOP("magnetic flux quantum (Φ₀)"), "2.067833848e-15", "Wb");
    PUSH_CONSTANT(QT_TR_NOOP("nuclear magneton"), "5.0507837393e-27", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("inverse of conductance quantum"), "12906.40372", "Ω");
    PUSH_CONSTANT(QT_TR_NOOP("conventional value of von Klitzing constant"), "25812.807", "Ω");
    PUSH_CONSTANT(QT_TR_NOOP("von Klitzing constant"), "25812.80745", "Ω");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of magnetic dipole moment"), "1.85480201315e-23", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of magnetic flux density"), "2.35051757077e5", "T");
    PUSH_CONSTANT(QT_TR_NOOP("Bohr magneton in eV/T"), "5.7883817982e-5", "eV·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Bohr magneton in Hz/T"), "1.39962449171e10", "Hz·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Bohr magneton in inverse meter per tesla"), "46.686447719", "m⁻¹·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Bohr magneton in K/T"), "0.67171381472", "K·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("nuclear magneton in eV/T"), "3.15245125417e-8", "eV·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("nuclear magneton in inverse meter per tesla"), "2.54262341009e-2", "m⁻¹·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("nuclear magneton in K/T"), "3.6582677706e-4", "K·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("nuclear magneton in MHz/T"), "7.6225932188", "MHz·T⁻¹");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearGeneral);
    PUSH_CONSTANT(QT_TR_NOOP("Bohr radius (a₀)"), "5.29177210544e-11", "m");
    PUSH_CONSTANT(QT_TR_NOOP("fine-structure constant (α)"), "7.2973525643e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("Hartree energy (Eₕ)"), "4.3597447222060e-18", "J");
    PUSH_CONSTANT(QT_TR_NOOP("Hartree energy in eV"), "27.211386245981", "eV");
    PUSH_CONSTANT(QT_TR_NOOP("quantum of circulation"), "3.6369475467e-4", "m²·s⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("quantum of circulation times 2"), "7.2738950934e-4", "m²·s⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Rydberg constant (R∞)"), "10973731.568157", "m⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("atomic mass unit-hartree relationship"), "3.4231776922e7", "E_h");
    PUSH_CONSTANT(QT_TR_NOOP("atomic mass unit-hertz relationship"), "2.25234272185e23", "Hz");
    PUSH_CONSTANT(QT_TR_NOOP("atomic mass unit-inverse meter relationship"), "7.5130066209e14", "m⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("atomic mass unit-joule relationship"), "1.49241808768e-10", "J");
    PUSH_CONSTANT(QT_TR_NOOP("atomic mass unit-kelvin relationship"), "1.08095402067e13", "K");
    PUSH_CONSTANT(QT_TR_NOOP("atomic mass unit-kilogram relationship"), "1.66053906892e-27", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of 1st hyperpolarizability"), "3.2063612996e-53", "C³·m³·J⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of 2nd hyperpolarizability"), "6.2353799735e-65", "C⁴·m⁴·J⁻³");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of charge (e)"), "1.602176634e-19", "C");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of charge density"), "1.08120238677e12", "C·m⁻³");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of current"), "6.6236182375082e-3", "A");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of electric dipole moment"), "8.4783536198e-30", "C·m");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of electric field"), "5.14220675112e11", "V·m⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of electric field gradient"), "9.7173624424e21", "V·m⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of electric polarizability"), "1.64877727212e-41", "C²·m²·J⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of electric potential"), "27.211386245981", "V");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of electric quadrupole moment"), "4.4865515185e-40", "C·m²");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of action"), "1.054571817e-34", "J·s");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of energy"), "4.3597447222060e-18", "J");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of force"), "8.2387235038e-8", "N");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of length"), "5.29177210544e-11", "m");
    PUSH_CONSTANT(QT_TR_NOOP("hertz-atomic mass unit relationship"), "4.4398216590e-24", "u");
    PUSH_CONSTANT(QT_TR_NOOP("hertz-electron volt relationship"), "4.135667696e-15", "eV");
    PUSH_CONSTANT(QT_TR_NOOP("hertz-hartree relationship"), "1.5198298460574e-16", "E_h");
    PUSH_CONSTANT(QT_TR_NOOP("hertz-inverse meter relationship"), "3.335640951e-9", "m⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("hertz-joule relationship"), "6.62607015e-34", "J");
    PUSH_CONSTANT(QT_TR_NOOP("hertz-kelvin relationship"), "4.799243073e-11", "K");
    PUSH_CONSTANT(QT_TR_NOOP("hertz-kilogram relationship"), "7.372497323e-51", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("hyperfine transition frequency of Cs-133"), "9192631770", "Hz");
    PUSH_CONSTANT(QT_TR_NOOP("inverse fine-structure constant (α⁻¹)"), "137.035999177", "");
    PUSH_CONSTANT(QT_TR_NOOP("inverse meter-atomic mass unit relationship"), "1.33102504824e-15", "u");
    PUSH_CONSTANT(QT_TR_NOOP("inverse meter-electron volt relationship"), "1.239841984e-6", "eV");
    PUSH_CONSTANT(QT_TR_NOOP("inverse meter-hartree relationship"), "4.5563352529132e-8", "E_h");
    PUSH_CONSTANT(QT_TR_NOOP("inverse meter-hertz relationship"), "299792458", "Hz");
    PUSH_CONSTANT(QT_TR_NOOP("inverse meter-joule relationship"), "1.986445857e-25", "J");
    PUSH_CONSTANT(QT_TR_NOOP("inverse meter-kelvin relationship"), "1.438776877e-2", "K");
    PUSH_CONSTANT(QT_TR_NOOP("inverse meter-kilogram relationship"), "2.210219094e-42", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("joule-atomic mass unit relationship"), "6.7005352471e9", "u");
    PUSH_CONSTANT(QT_TR_NOOP("joule-electron volt relationship"), "6.241509074e18", "eV");
    PUSH_CONSTANT(QT_TR_NOOP("joule-hertz relationship"), "1.509190179e33", "Hz");
    PUSH_CONSTANT(QT_TR_NOOP("joule-hartree relationship"), "2.2937122783969e17", "E_h");
    PUSH_CONSTANT(QT_TR_NOOP("joule-inverse meter relationship"), "5.034116567e24", "m⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("joule-kelvin relationship"), "7.242970516e22", "K");
    PUSH_CONSTANT(QT_TR_NOOP("joule-kilogram relationship"), "1.112650056e-17", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("kelvin-atomic mass unit relationship"), "9.2510872884e-14", "u");
    PUSH_CONSTANT(QT_TR_NOOP("kelvin-electron volt relationship"), "8.617333262e-5", "eV");
    PUSH_CONSTANT(QT_TR_NOOP("kelvin-hertz relationship"), "2.083661912e10", "Hz");
    PUSH_CONSTANT(QT_TR_NOOP("kelvin-hartree relationship"), "3.1668115634564e-6", "E_h");
    PUSH_CONSTANT(QT_TR_NOOP("kelvin-inverse meter relationship"), "69.50348004", "m⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("kelvin-joule relationship"), "1.380649e-23", "J");
    PUSH_CONSTANT(QT_TR_NOOP("kelvin-kilogram relationship"), "1.536179187e-40", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("kilogram-atomic mass unit relationship"), "6.0221407537e26", "u");
    PUSH_CONSTANT(QT_TR_NOOP("kilogram-electron volt relationship"), "5.609588603e35", "eV");
    PUSH_CONSTANT(QT_TR_NOOP("kilogram-hertz relationship"), "1.356392489e50", "Hz");
    PUSH_CONSTANT(QT_TR_NOOP("kilogram-hartree relationship"), "2.0614857887415e34", "E_h");
    PUSH_CONSTANT(QT_TR_NOOP("kilogram-inverse meter relationship"), "4.524438335e41", "m⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("kilogram-joule relationship"), "8.987551787e16", "J");
    PUSH_CONSTANT(QT_TR_NOOP("kilogram-kelvin relationship"), "6.509657260e39", "K");
    PUSH_CONSTANT(QT_TR_NOOP("lattice parameter of silicon (a)"), "5.431020511e-10", "m");
    PUSH_CONSTANT(QT_TR_NOOP("luminous efficacy"), "683", "lm·W⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("natural unit of energy"), "8.1871057880e-14", "J");
    PUSH_CONSTANT(QT_TR_NOOP("natural unit of energy in MeV"), "0.51099895069", "MeV");
    PUSH_CONSTANT(QT_TR_NOOP("natural unit of action (ℏ)"), "1.054571817e-34", "J·s");
    PUSH_CONSTANT(QT_TR_NOOP("natural unit of action in eV·s"), "6.582119569e-16", "eV·s");
    PUSH_CONSTANT(QT_TR_NOOP("natural unit of length"), "3.8615926744e-13", "m");
    PUSH_CONSTANT(QT_TR_NOOP("natural unit of mass"), "9.1093837139e-31", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("natural unit of momentum"), "2.73092453446e-22", "kg·m·s⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("natural unit of momentum in MeV/c"), "0.51099895069", "MeV/c");
    PUSH_CONSTANT(QT_TR_NOOP("natural unit of time"), "1.28808866644e-21", "s");
    PUSH_CONSTANT(QT_TR_NOOP("natural unit of velocity"), "299792458", "m·s⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Rydberg constant times c in Hz"), "3.2898419602500e15", "Hz");
    PUSH_CONSTANT(QT_TR_NOOP("Rydberg constant times hc in eV"), "13.605693122990", "eV");
    PUSH_CONSTANT(QT_TR_NOOP("Rydberg constant times hc in J"), "2.1798723611030e-18", "J");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearElectroweak);
    PUSH_CONSTANT(QT_TR_NOOP("Fermi coupling constant"), "1.1663787e-5", "GeV⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("weak mixing angle"), "0.22305", "");
    PUSH_CONSTANT(QT_TR_NOOP("W to Z mass ratio"), "0.88145", "");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearElectron);
    PUSH_CONSTANT(QT_TR_NOOP("Thomson cross section (σₑ)"), "6.6524587051e-29", "m²");
    PUSH_CONSTANT(QT_TR_NOOP("electron mass (mₑ)"), "9.1093837139e-31", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("electron mass in u"), "5.485799090441e-4", "u");
    PUSH_CONSTANT(QT_TR_NOOP("electron mass energy equivalent (mₑ·c²)"), "8.1871057880e-14", "J");
    PUSH_CONSTANT(QT_TR_NOOP("electron mass energy equivalent in MeV"), "0.51099895069", "MeV");
    PUSH_CONSTANT(QT_TR_NOOP("Compton wavelength"), "2.42631023538e-12", "m");
    PUSH_CONSTANT(QT_TR_NOOP("reduced Compton wavelength"), "3.8615926744e-13", "m");
    PUSH_CONSTANT(QT_TR_NOOP("classical electron radius (rₑ)"), "2.8179403205e-15", "m");
    PUSH_CONSTANT(QT_TR_NOOP("electron charge to mass quotient"), "-1.75882000838e11", "C·kg⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("electron-deuteron magnetic moment ratio"), "-2143.9234921", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron-deuteron mass ratio"), "2.724437107629e-4", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron g factor (gₑ)"), "-2.00231930436092", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron gyromagnetic ratio (γₑ)"), "1.76085962784e11", "s⁻¹·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("electron gyromagnetic ratio in MHz/T"), "28024.9513861", "MHz·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("electron-helion mass ratio"), "1.819543074649e-4", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron magnetic moment (μₑ)"), "-9.2847646917e-24", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("electron magnetic moment anomaly (aₑ)"), "1.15965218046e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron magnetic moment to Bohr magneton ratio"), "-1.00115965218046", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron magnetic moment to nuclear magneton ratio"), "-1838.281971877", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron molar mass (Mₑ)"), "5.4857990962e-7", "kg·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("electron-muon magnetic moment ratio"), "206.7669881", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron-muon mass ratio"), "4.83633170e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron-neutron magnetic moment ratio"), "960.92048", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron-neutron mass ratio"), "5.4386734416e-4", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron-proton magnetic moment ratio"), "-658.21068789", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron-proton mass ratio"), "5.446170214889e-4", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron relative atomic mass (Ar(e))"), "5.485799090441e-4", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron-tau mass ratio"), "2.87585e-4", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron to alpha particle mass ratio"), "1.370933554733e-4", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron to shielded helion magnetic moment ratio"), "864.05823986", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron to shielded proton magnetic moment ratio"), "-658.2275856", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron-triton mass ratio"), "1.819200062327e-4", "");
    PUSH_CONSTANT(QT_TR_NOOP("electron volt-atomic mass unit relationship"), "1.07354410083e-9", "u");
    PUSH_CONSTANT(QT_TR_NOOP("electron volt-hartree relationship"), "3.6749322175665e-2", "E_h");
    PUSH_CONSTANT(QT_TR_NOOP("electron volt-hertz relationship"), "2.417989242e14", "Hz");
    PUSH_CONSTANT(QT_TR_NOOP("electron volt-inverse meter relationship"), "8.065543937e5", "m⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("electron volt-joule relationship"), "1.602176634e-19", "J");
    PUSH_CONSTANT(QT_TR_NOOP("electron volt-kelvin relationship"), "1.160451812e4", "K");
    PUSH_CONSTANT(QT_TR_NOOP("electron volt-kilogram relationship"), "1.782661921e-36", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("hartree-atomic mass unit relationship"), "2.92126231797e-8", "u");
    PUSH_CONSTANT(QT_TR_NOOP("hartree-electron volt relationship"), "27.211386245981", "eV");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearMuon);
    PUSH_CONSTANT(QT_TR_NOOP("muon Compton wavelength"), "1.173444110e-14", "m");
    PUSH_CONSTANT(QT_TR_NOOP("muon-electron mass ratio"), "206.7682827", "");
    PUSH_CONSTANT(QT_TR_NOOP("muon g factor"), "-2.00233184123", "");
    PUSH_CONSTANT(QT_TR_NOOP("muon magnetic moment"), "-4.49044830e-26", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("muon magnetic moment anomaly"), "1.16592062e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("muon magnetic moment to Bohr magneton ratio"), "-4.84197048e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("muon magnetic moment to nuclear magneton ratio"), "-8.89059704", "");
    PUSH_CONSTANT(QT_TR_NOOP("muon mass"), "1.883531627e-28", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("muon mass energy equivalent"), "1.692833804e-11", "J");
    PUSH_CONSTANT(QT_TR_NOOP("muon mass energy equivalent in MeV"), "105.6583755", "MeV·c⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("muon mass in u"), "0.1134289257", "u");
    PUSH_CONSTANT(QT_TR_NOOP("muon molar mass"), "1.134289258e-4", "kg·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("muon-neutron mass ratio"), "0.1124545168", "");
    PUSH_CONSTANT(QT_TR_NOOP("muon-proton magnetic moment ratio"), "-3.183345146", "");
    PUSH_CONSTANT(QT_TR_NOOP("muon-proton mass ratio"), "0.1126095262", "");
    PUSH_CONSTANT(QT_TR_NOOP("muon-tau mass ratio"), "5.94635e-2", "");
    PUSH_CONSTANT(QT_TR_NOOP("reduced muon Compton wavelength"), "1.867594306e-15", "m");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearTau);
    PUSH_CONSTANT(QT_TR_NOOP("reduced tau Compton wavelength"), "1.110538e-16", "m");
    PUSH_CONSTANT(QT_TR_NOOP("tau Compton wavelength"), "6.97771e-16", "m");
    PUSH_CONSTANT(QT_TR_NOOP("tau-electron mass ratio"), "3477.23", "");
    PUSH_CONSTANT(QT_TR_NOOP("tau mass"), "3.16754e-27", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("tau mass energy equivalent"), "2.84684e-10", "J");
    PUSH_CONSTANT(QT_TR_NOOP("tau mass in u"), "1.90754", "u");
    PUSH_CONSTANT(QT_TR_NOOP("tau molar mass"), "1.90754e-3", "kg·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("tau-muon mass ratio"), "16.8170", "");
    PUSH_CONSTANT(QT_TR_NOOP("tau-neutron mass ratio"), "1.89115", "");
    PUSH_CONSTANT(QT_TR_NOOP("tau-proton mass ratio"), "1.89376", "");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearProton);
    PUSH_CONSTANT(QT_TR_NOOP("proton charge to mass quotient"), "9.5788331430e7", "C·kg⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("proton Compton wavelength"), "1.32140985360e-15", "m");
    PUSH_CONSTANT(QT_TR_NOOP("proton-electron mass ratio"), "1836.152673426", "");
    PUSH_CONSTANT(QT_TR_NOOP("proton g factor (gₚ)"), "5.5856946893", "");
    PUSH_CONSTANT(QT_TR_NOOP("proton gyromagnetic ratio (γₚ)"), "2.6752218708e8", "s⁻¹·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("proton gyromagnetic ratio in MHz/T"), "42.577478461", "MHz·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("proton magnetic moment (μₚ)"), "1.41060679545e-26", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("proton magnetic moment to Bohr magneton ratio"), "1.52103220230e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("proton magnetic moment to nuclear magneton ratio"), "2.79284734463", "");
    PUSH_CONSTANT(QT_TR_NOOP("proton magnetic shielding correction"), "2.56715e-5", "");
    PUSH_CONSTANT(QT_TR_NOOP("proton mass (mₚ)"), "1.67262192595e-27", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("proton mass energy equivalent"), "1.50327761802e-10", "J");
    PUSH_CONSTANT(QT_TR_NOOP("proton mass energy equivalent in MeV"), "938.27208943", "MeV·c⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("proton mass in u"), "1.0072764665789", "u");
    PUSH_CONSTANT(QT_TR_NOOP("proton molar mass (Mₚ)"), "1.00727646764e-3", "kg·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("proton-muon mass ratio"), "8.88024338", "");
    PUSH_CONSTANT(QT_TR_NOOP("proton-neutron magnetic moment ratio"), "-1.45989802", "");
    PUSH_CONSTANT(QT_TR_NOOP("proton-neutron mass ratio"), "0.99862347797", "");
    PUSH_CONSTANT(QT_TR_NOOP("proton relative atomic mass (Ar(p))"), "1.0072764665789", "");
    PUSH_CONSTANT(QT_TR_NOOP("proton rms charge radius"), "8.4075e-16", "m");
    PUSH_CONSTANT(QT_TR_NOOP("proton-tau mass ratio"), "0.528051", "");
    PUSH_CONSTANT(QT_TR_NOOP("reduced proton Compton wavelength"), "2.10308910051e-16", "m");
    PUSH_CONSTANT(QT_TR_NOOP("shielded proton gyromagnetic ratio"), "2.675153194e8", "s⁻¹·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("shielded proton gyromagnetic ratio in MHz/T"), "42.57638543", "MHz·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("shielded proton magnetic moment"), "1.4105705830e-26", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("shielded proton magnetic moment to Bohr magneton ratio"), "1.5209931551e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("shielded proton magnetic moment to nuclear magneton ratio"), "2.792775648", "");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearNeutron);
    PUSH_CONSTANT(QT_TR_NOOP("neutron Compton wavelength"), "1.31959090382e-15", "m");
    PUSH_CONSTANT(QT_TR_NOOP("neutron-electron magnetic moment ratio"), "1.04066884e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("neutron-electron mass ratio"), "1838.68366200", "");
    PUSH_CONSTANT(QT_TR_NOOP("neutron g factor (gₙ)"), "-3.82608552", "");
    PUSH_CONSTANT(QT_TR_NOOP("neutron gyromagnetic ratio (γₙ)"), "1.83247174e8", "s⁻¹·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("neutron gyromagnetic ratio in MHz/T"), "29.1646935", "MHz·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("neutron magnetic moment (μₙ)"), "-9.6623653e-27", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("neutron magnetic moment to Bohr magneton ratio"), "-1.04187565e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("neutron magnetic moment to nuclear magneton ratio"), "-1.91304276", "");
    PUSH_CONSTANT(QT_TR_NOOP("neutron mass (mₙ)"), "1.67492750056e-27", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("neutron mass energy equivalent"), "1.50534976514e-10", "J");
    PUSH_CONSTANT(QT_TR_NOOP("neutron mass energy equivalent in MeV"), "939.56542194", "MeV·c⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("neutron mass in u"), "1.00866491606", "u");
    PUSH_CONSTANT(QT_TR_NOOP("neutron molar mass (Mₙ)"), "1.00866491712e-3", "kg·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("neutron-muon mass ratio"), "8.89248408", "");
    PUSH_CONSTANT(QT_TR_NOOP("neutron-proton magnetic moment ratio"), "-0.68497935", "");
    PUSH_CONSTANT(QT_TR_NOOP("neutron-proton mass difference"), "2.30557461e-30", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("neutron-proton mass difference energy equivalent"), "2.07214712e-13", "J");
    PUSH_CONSTANT(QT_TR_NOOP("neutron-proton mass difference energy equivalent in MeV"), "1.29333251", "MeV");
    PUSH_CONSTANT(QT_TR_NOOP("neutron-proton mass difference in u"), "1.38844948e-3", "u");
    PUSH_CONSTANT(QT_TR_NOOP("neutron-proton mass ratio"), "1.00137841946", "");
    PUSH_CONSTANT(QT_TR_NOOP("neutron relative atomic mass (Ar(n))"), "1.00866491606", "");
    PUSH_CONSTANT(QT_TR_NOOP("neutron-tau mass ratio"), "0.528779", "");
    PUSH_CONSTANT(QT_TR_NOOP("neutron to shielded proton magnetic moment ratio"), "-0.68499694", "");
    PUSH_CONSTANT(QT_TR_NOOP("reduced neutron Compton wavelength"), "2.1001941520e-16", "m");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearDeuteron);
    PUSH_CONSTANT(QT_TR_NOOP("deuteron-electron magnetic moment ratio"), "-4.664345550e-4", "");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron-electron mass ratio"), "3670.482967655", "");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron g factor"), "0.8574382335", "");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron magnetic moment"), "4.330735087e-27", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron magnetic moment to Bohr magneton ratio"), "4.669754568e-4", "");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron magnetic moment to nuclear magneton ratio"), "0.8574382335", "");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron mass"), "3.3435837768e-27", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron mass energy equivalent"), "3.00506323491e-10", "J");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron mass energy equivalent in MeV"), "1875.61294500", "MeV");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron mass in u"), "2.013553212544", "u");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron molar mass"), "2.01355321466e-3", "kg·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron-neutron magnetic moment ratio"), "-0.44820652", "");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron-proton magnetic moment ratio"), "0.30701220930", "");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron-proton mass ratio"), "1.9990075012699", "");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron relative atomic mass (Ar(d))"), "2.013553212544", "");
    PUSH_CONSTANT(QT_TR_NOOP("deuteron rms charge radius"), "2.12778e-15", "m");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearTriton);
    PUSH_CONSTANT(QT_TR_NOOP("triton-electron mass ratio"), "5496.92153551", "");
    PUSH_CONSTANT(QT_TR_NOOP("triton g factor (gₜ)"), "5.957924930", "");
    PUSH_CONSTANT(QT_TR_NOOP("triton magnetic moment (μₜ)"), "1.5046095178e-26", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("triton magnetic moment to Bohr magneton ratio"), "1.6223936648e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("triton magnetic moment to nuclear magneton ratio"), "2.9789624650", "");
    PUSH_CONSTANT(QT_TR_NOOP("triton mass (mₜ)"), "5.0073567512e-27", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("triton mass energy equivalent"), "4.5003878119e-10", "J");
    PUSH_CONSTANT(QT_TR_NOOP("triton mass energy equivalent in MeV"), "2808.92113668", "MeV");
    PUSH_CONSTANT(QT_TR_NOOP("triton mass in u"), "3.01550071597", "u");
    PUSH_CONSTANT(QT_TR_NOOP("triton molar mass (Mₜ)"), "3.01550071913e-3", "kg·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("triton-proton mass ratio"), "2.99371703403", "");
    PUSH_CONSTANT(QT_TR_NOOP("triton relative atomic mass (Ar(t))"), "3.01550071597", "");
    PUSH_CONSTANT(QT_TR_NOOP("triton to proton magnetic moment ratio"), "1.0666399189", "");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearHelion);
    PUSH_CONSTANT(QT_TR_NOOP("helion-electron mass ratio"), "5495.88527984", "");
    PUSH_CONSTANT(QT_TR_NOOP("helion g factor (gₕ)"), "-4.2552506995", "");
    PUSH_CONSTANT(QT_TR_NOOP("helion magnetic moment (μₕ)"), "-1.07461755198e-26", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("helion magnetic moment to Bohr magneton ratio"), "-1.15874098083e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("helion magnetic moment to nuclear magneton ratio"), "-2.1276253498", "");
    PUSH_CONSTANT(QT_TR_NOOP("helion mass (mₕ)"), "5.0064127862e-27", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("helion mass energy equivalent"), "4.4995394185e-10", "J");
    PUSH_CONSTANT(QT_TR_NOOP("helion mass energy equivalent in MeV"), "2808.39161112", "MeV");
    PUSH_CONSTANT(QT_TR_NOOP("helion mass in u"), "3.014932246932", "u");
    PUSH_CONSTANT(QT_TR_NOOP("helion molar mass (Mₕ)"), "3.01493225010e-3", "kg·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("helion-proton mass ratio"), "2.993152671552", "");
    PUSH_CONSTANT(QT_TR_NOOP("helion relative atomic mass (Ar(h))"), "3.014932246932", "");
    PUSH_CONSTANT(QT_TR_NOOP("helion shielding shift"), "5.9967029e-5", "");
    PUSH_CONSTANT(QT_TR_NOOP("shielded helion gyromagnetic ratio"), "2.0378946078e8", "s⁻¹·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("shielded helion gyromagnetic ratio in MHz/T"), "32.434100033", "MHz·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("shielded helion magnetic moment"), "-1.07455311035e-26", "J·T⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("shielded helion magnetic moment to Bohr magneton ratio"), "-1.15867149457e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("shielded helion magnetic moment to nuclear magneton ratio"), "-2.1274977624", "");
    PUSH_CONSTANT(QT_TR_NOOP("shielded helion to proton magnetic moment ratio"), "-0.76176657721", "");
    PUSH_CONSTANT(QT_TR_NOOP("shielded helion to shielded proton magnetic moment ratio"), "-0.7617861334", "");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearAlphaParticle);
    PUSH_CONSTANT(QT_TR_NOOP("alpha particle mass"), "6.6446573450e-27", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("alpha particle mass in u"), "4.001506179129", "u");
    PUSH_CONSTANT(QT_TR_NOOP("alpha particle mass energy equivalent"), "5.9719201997e-10", "J");
    PUSH_CONSTANT(QT_TR_NOOP("alpha particle mass energy equivalent in MeV"), "3727.3794118", "MeV");
    PUSH_CONSTANT(QT_TR_NOOP("alpha particle-electron mass ratio"), "7294.29954171", "");
    PUSH_CONSTANT(QT_TR_NOOP("alpha particle-proton mass ratio"), "3.972599690252", "");
    PUSH_CONSTANT(QT_TR_NOOP("alpha particle rms charge radius"), "1.6785e-15", "m");
    PUSH_CONSTANT(QT_TR_NOOP("alpha particle molar mass"), "4.0015061833e-3", "kg·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("alpha particle relative atomic mass (Ar(α))"), "4.001506179129", "");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::Physicochemical);
    PUSH_CONSTANT(QT_TR_NOOP("atomic mass constant (mᵤ)"), "1.66053906892e-27", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("Avogadro constant"), "6.02214076e23", "mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Boltzmann constant (k)"), "1.380649e-23", "J·K⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Boltzmann constant in eV/K"), "8.617333262e-5", "eV·K⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Boltzmann constant in Hz/K"), "2.083661912e10", "Hz·K⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Boltzmann constant in inverse meter per kelvin"), "69.50348004", "m⁻¹·K⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("electron volt (eV)"), "1.602176634e-19", "J");
    PUSH_CONSTANT(QT_TR_NOOP("Faraday constant (F)"), "96485.33212", "C·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("first radiation constant (c₁)"), "3.741771852e-16", "W·m²");
    PUSH_CONSTANT(QT_TR_NOOP("first radiation constant for spectral radiance"), "1.191042972e-16", "W·m²·sr⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("molar gas constant (R)"), "8.314462618", "J·K⁻¹·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Loschmidt constant (n₀, 273.15 K, 100 kPa)"), "2.6516467e25", "m⁻³");
    PUSH_CONSTANT(QT_TR_NOOP("Loschmidt constant (n₀, 273.15 K, 101.325 kPa)"), "2.6867811e25", "m⁻³");
    PUSH_CONSTANT(QT_TR_NOOP("molar Planck constant"), "3.990312712e-10", "J·s·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("second radiation constant (c₂)"), "1.438776877e-2", "m·K");
    PUSH_CONSTANT(QT_TR_NOOP("Stefan-Boltzmann constant (σ)"), "5.670374419e-8", "W·m⁻²·K⁻⁴");
    PUSH_CONSTANT(QT_TR_NOOP("lattice spacing of ideal Si (d₂₂₀)"), "1.920155716e-10", "m");
    PUSH_CONSTANT(QT_TR_NOOP("atomic mass constant energy equivalent"), "1.49241808768e-10", "J");
    PUSH_CONSTANT(QT_TR_NOOP("atomic mass constant energy equivalent in MeV"), "931.49410372", "MeV");
    PUSH_CONSTANT(QT_TR_NOOP("atomic mass unit-electron volt relationship"), "9.3149410372e8", "eV");
    PUSH_CONSTANT(QT_TR_NOOP("molar mass constant (Mᵤ)"), "1.00000000105e-3", "kg·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("molar mass of carbon-12"), "12.0000000126e-3", "kg·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("molar volume of ideal gas (273.15 K, 100 kPa)"), "22.71095464e-3", "m³·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("molar volume of ideal gas (273.15 K, 101.325 kPa)"), "22.41396954e-3", "m³·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("molar volume of silicon"), "1.205883199e-5", "m³·mol⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Sackur-Tetrode constant (1 K, 100 kPa)"), "-1.15170753496", "");
    PUSH_CONSTANT(QT_TR_NOOP("Sackur-Tetrode constant (1 K, 101.325 kPa)"), "-1.16487052149", "");
    PUSH_CONSTANT(QT_TR_NOOP("Wien frequency displacement law constant"), "5.878925757e10", "Hz·K⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("Wien wavelength displacement law constant"), "2.897771955e-3", "m·K");
    PUSH_CONSTANT(QT_TR_NOOP("standard atmosphere (atm)"), "101325", "Pa");
    PUSH_CONSTANT(QT_TR_NOOP("standard-state pressure (p°)"), "100000", "Pa");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearXrayUnits);
    PUSH_CONSTANT(QT_TR_NOOP("Angstrom star (Å*)"), "1.00001495e-10", "m");
    PUSH_CONSTANT(QT_TR_NOOP("Copper x unit"), "1.00207697e-13", "m");
    PUSH_CONSTANT(QT_TR_NOOP("Molybdenum x unit"), "1.00209952e-13", "m");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearAtomicUnits);
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of magnetizability"), "7.8910365794e-29", "J·T⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of mass (mₑ)"), "9.1093837139e-31", "kg");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of momentum"), "1.99285191545e-24", "kg·m·s⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of permittivity"), "1.11265005620e-10", "F·m⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of time"), "2.4188843265864e-17", "s");
    PUSH_CONSTANT(QT_TR_NOOP("atomic unit of velocity"), "2.18769126216e6", "m·s⁻¹");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearEnergyConversionRelationships);
    PUSH_CONSTANT(QT_TR_NOOP("hartree-hertz relationship"), "6.5796839204999e15", "Hz");
    PUSH_CONSTANT(QT_TR_NOOP("hartree-inverse meter relationship"), "2.1947463136314e7", "m⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("hartree-joule relationship"), "4.3597447222060e-18", "J");
    PUSH_CONSTANT(QT_TR_NOOP("hartree-kelvin relationship"), "3.1577502480398e5", "K");
    PUSH_CONSTANT(QT_TR_NOOP("hartree-kilogram relationship"), "4.8508702095419e-35", "kg");

    SET_CONSTANT_CONTEXT(ConstantDomain::PhysicsCodata2022, ConstantSubdomain::AtomicNuclearMagneticShieldingCorrections);
    PUSH_CONSTANT(QT_TR_NOOP("shielding difference of d and p in HD"), "1.98770e-8", "");
    PUSH_CONSTANT(QT_TR_NOOP("shielding difference of t and p in HT"), "2.39450e-8", "");

    // Mathematics.
    SET_CONSTANT_CONTEXT(ConstantDomain::Mathematics, ConstantSubdomain::None);
    PUSH_CONSTANT(QT_TR_NOOP("pi (π)"), "3.14159265358979323846264338327950288419716939937511", "");
    PUSH_CONSTANT(QT_TR_NOOP("Euler's number (e)"), "2.71828182845904523536028747135266249775724709369996", "");
    PUSH_CONSTANT(QT_TR_NOOP("golden ratio (φ)"), "1.61803398874989484820458683436563811772030917980576", "");
    PUSH_CONSTANT(QT_TR_NOOP("Euler-Mascheroni constant (γ)"), "0.57721566490153286060651209008240243104215933593992", "");

    // Astronomy.
    SET_CONSTANT_CONTEXT(ConstantDomain::Astronomy, ConstantSubdomain::AstronomyNominalIau2015);
    PUSH_CONSTANT(QT_TR_NOOP("nominal solar radius"), "6.957e8", "m");
    PUSH_CONSTANT(QT_TR_NOOP("nominal solar total irradiance"), "1361", "W·m⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("nominal solar luminosity"), "3.828e26", "W");
    PUSH_CONSTANT(QT_TR_NOOP("nominal solar effective temperature"), "5772", "K");
    PUSH_CONSTANT(QT_TR_NOOP("nominal solar mass parameter"), "1.3271244e20", "m³·s⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("nominal terrestrial equatorial radius"), "6.3781e6", "m");
    PUSH_CONSTANT(QT_TR_NOOP("nominal terrestrial polar radius"), "6.3568e6", "m");
    PUSH_CONSTANT(QT_TR_NOOP("nominal terrestrial mass parameter"), "3.986004e14", "m³·s⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("nominal jovian equatorial radius"), "7.1492e7", "m");
    PUSH_CONSTANT(QT_TR_NOOP("nominal jovian polar radius"), "6.6854e7", "m");
    PUSH_CONSTANT(QT_TR_NOOP("nominal jovian mass parameter"), "1.2668653e17", "m³·s⁻²");
    SET_CONSTANT_CONTEXT(ConstantDomain::Astronomy, ConstantSubdomain::AstronomyCurrentBestEstimatesIauNsfa202604);
    PUSH_CONSTANT(QT_TR_NOOP("average value of one minus the rate of TCG relative to TCB"), "1.48082686741e-8", "");
    PUSH_CONSTANT(QT_TR_NOOP("solar mass parameter (TCB-compatible)"), "1.32712442099e20", "m³·s⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("solar mass parameter (TDB-compatible)"), "1.32712440041e20", "m³·s⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("equatorial radius of the Earth (TT-compatible)"), "6.3781366e6", "m");
    PUSH_CONSTANT(QT_TR_NOOP("dynamical form factor of the Earth"), "1.0826359e-3", "");
    PUSH_CONSTANT(QT_TR_NOOP("time rate of change in the dynamical form factor of the Earth"), "-3.0e-9", "cy_jul⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("geocentric gravitational constant (TCB-compatible)"), "3.986004418e14", "m³·s⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("geocentric gravitational constant (TT-compatible)"), "3.986004415e14", "m³·s⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("geocentric gravitational constant (TDB-compatible)"), "3.986004356e14", "m³·s⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("potential of the geoid"), "62636853.4", "m²·s⁻²");
    PUSH_CONSTANT(QT_TR_NOOP("nominal mean angular velocity of the Earth (TT-compatible)"), "7.292115e-5", "rad·s⁻¹");
    PUSH_CONSTANT(QT_TR_NOOP("ratio mass of the Moon to the Earth"), "1.23000371e-2", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of the Sun to Mercury"), "6.023657330e6", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of the Sun to Venus"), "4.08523719e5", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of the Sun to Mars"), "3.09870359e6", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of the Sun to Jupiter"), "1.047348644e3", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of the Sun to Saturn"), "3.4979018e3", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of the Sun to Uranus"), "2.2902951e4", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of the Sun to Neptune"), "1.941226e4", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of the Sun to (134340) Pluto"), "1.3605e8", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of the Sun to (136199) Eris"), "1.191e8", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of (1) Ceres to the Sun"), "4.72e-10", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of (2) Pallas to the Sun"), "1.03e-10", "");
    PUSH_CONSTANT(QT_TR_NOOP("ratio of the mass of (4) Vesta to the Sun"), "1.3026846e-10", "");
    PUSH_CONSTANT(QT_TR_NOOP("mean obliquity of the ecliptic at J2000.0"), "84381.406", "arcsec");

    domains << domainToDisplay(ConstantDomain::Mathematics)
            << domainToDisplay(ConstantDomain::PhysicsCodata2022)
            // << domainToDisplay(ConstantDomain::ChemistryIupacCiaaw2021)
            << domainToDisplay(ConstantDomain::Astronomy);
}

void Constants::Private::retranslateText()
{
    populate();
}

Constants* Constants::instance()
{
    if (!s_constantsInstance) {
        s_constantsInstance = new Constants;
        qAddPostRoutine(s_deleteConstants);
    }
    return s_constantsInstance;
}

Constants::Constants()
    : d(new Constants::Private)
{
    setObjectName("Constants");
    d->retranslateText();
}

const QList<Constant>& Constants::list() const
{
    return d->list;
}

const QStringList& Constants::domains() const
{
    return d->domains;
}

QStringList Constants::subdomains(const QString& domain) const
{
    if (domain == domainToDisplay(ConstantDomain::PhysicsCodata2022)) {
        return QStringList{
            subdomainToDisplay(ConstantSubdomain::Universal),
            subdomainToDisplay(ConstantSubdomain::Electromagnetic),
            subdomainToDisplay(ConstantSubdomain::Physicochemical),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearGeneral),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearElectroweak),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearElectron),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearMuon),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearTau),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearProton),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearNeutron),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearDeuteron),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearTriton),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearHelion),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearAlphaParticle),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearAtomicUnits),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearEnergyConversionRelationships),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearXrayUnits),
            subdomainToDisplay(ConstantSubdomain::AtomicNuclearMagneticShieldingCorrections),
        };
    }
    if (domain == domainToDisplay(ConstantDomain::ChemistryIupacCiaaw2021)) {
        return QStringList{
            subdomainToDisplay(ConstantSubdomain::MolarMasses),
            subdomainToDisplay(ConstantSubdomain::Electronegativity),
            subdomainToDisplay(ConstantSubdomain::IonizationEnergy)
        };
    }
    if (domain == domainToDisplay(ConstantDomain::Astronomy)) {
        return QStringList{
            subdomainToDisplay(ConstantSubdomain::AstronomyNominalIau2015),
            subdomainToDisplay(ConstantSubdomain::AstronomyCurrentBestEstimatesIauNsfa202604),
        };
    }
    return QStringList();
}

void Constants::retranslateText()
{
    d->retranslateText();
}

Constants::~Constants()
{
}
