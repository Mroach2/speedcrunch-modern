// SPDX-FileCopyrightText: 2007-2010, 2013-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_CONSTANTS_H
#define CORE_CONSTANTS_H

#include <QObject>
#include <QStringList>

#include <memory>

enum class ConstantDomain {
    Uncategorized,
    Mathematics,
    PhysicsCodata2022,
    ChemistryIupacCiaaw2021,
    Astronomy
};

enum class ConstantSubdomain {
    None,
    Universal,
    Electromagnetic,
    Physicochemical,
    AtomicNuclearGeneral,
    AtomicNuclearElectroweak,
    AtomicNuclearElectron,
    AtomicNuclearMuon,
    AtomicNuclearTau,
    AtomicNuclearProton,
    AtomicNuclearNeutron,
    AtomicNuclearDeuteron,
    AtomicNuclearTriton,
    AtomicNuclearHelion,
    AtomicNuclearAlphaParticle,
    AtomicNuclearAtomicUnits,
    AtomicNuclearEnergyConversionRelationships,
    AtomicNuclearXrayUnits,
    AtomicNuclearMagneticShieldingCorrections,
    Masses,
    Lifetimes,
    MolarMasses,
    Electronegativity,
    IonizationEnergy,
    AstronomyNominalIau2015,
    AstronomyCurrentBestEstimatesIauNsfa202604
};

struct Constant {
    ConstantDomain domainType = ConstantDomain::Uncategorized;
    ConstantSubdomain subdomainType = ConstantSubdomain::None;
    QString domain;
    QString subdomain;
    QString name;
    QString unit;
    QString value;
};

class Constants : public QObject {
    Q_OBJECT

public:
    ~Constants(); //  For unique_ptr, define after Private is complete.
    static Constants* instance();
    const QStringList& domains() const;
    QStringList subdomains(const QString& domain) const;
    const QList<Constant>& list() const;

public slots:
    void retranslateText();

private:
    struct Private;
    std::unique_ptr<Private> d;

    Constants();
    Q_DISABLE_COPY(Constants)
};

#endif
