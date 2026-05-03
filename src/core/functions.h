// SPDX-FileCopyrightText: 2007-2009, 2013-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_FUNCTION_H
#define CORE_FUNCTION_H

#include "core/errors.h"
#include "math/quantity.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVector>

class Function;
enum class FunctionDomain {
    Arithmetic,
    Chemistry,
    Complex,
    Combinatorics,
    Probability,
    Statistics,
    Aggregation,
    Random,
    BaseConversion,
    NumberFormatting,
    IntegerArithmetic,
    SpecialFunctions,
    ExponentialLogarithmic,
    AngleConversion,
    Trigonometry,
    Bitwise,
    FloatingPoint,
    DateTime,
    LinearAlgebra
};

class Function : public QObject {
    Q_OBJECT
public:
    typedef QVector<Quantity> ArgumentList;
    typedef Quantity (*FunctionImpl)(Function*, const ArgumentList&);

    Function(const QString& identifier, FunctionImpl ptr, FunctionDomain domainType, QObject* parent = 0)
        : QObject(parent)
        , m_identifier(identifier)
        , m_ptr(ptr)
        , m_domainType(domainType)
    { }

    const QString& identifier() const { return m_identifier; }
    const QString& name() const { return m_name; }
    const QString& usage() const { return m_usage; }
    FunctionDomain domainType() const { return m_domainType; }
    const QString& domain() const { return m_domain; }
    Error error() const { return m_error; }
    Quantity exec(const ArgumentList&);

    void setName(const QString& name) { m_name = name; }
    void setUsage(const QString& usage) { m_usage = usage; }
    void setDomain(const QString& domain) { m_domain = domain; }
    void setError(Error error) { m_error = error; }

private:
    Q_DISABLE_COPY(Function)
    Function();

    QString m_identifier;
    QString m_name;
    QString m_usage;
    Error m_error;
    FunctionImpl m_ptr;
    FunctionDomain m_domainType;
    QString m_domain;
};

class FunctionRepo : public QObject {
    Q_OBJECT
public:
    static FunctionRepo* instance();

    void insert(Function*);
    QStringList getIdentifiers() const;
    Function* find(const QString& identifier) const;
    bool isIdentifierAliasOf(const QString& identifier, const QString& canonicalIdentifier) const;
    QString displayIdentifier(const QString& identifier) const;
    const QStringList& domains() const;

public slots:
    void retranslateText();

private:
    Q_DISABLE_COPY(FunctionRepo)
    FunctionRepo();

    void createFunctions();
    void setFunctionNames();
    void setNonTranslatableFunctionUsages();
    void setTranslatableFunctionUsages();

    QHash<QString, Function*> m_functions;
};

#endif // CORE_FUNCTION_H
