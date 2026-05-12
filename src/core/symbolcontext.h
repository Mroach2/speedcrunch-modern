// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef CORE_SYMBOLCONTEXT_H
#define CORE_SYMBOLCONTEXT_H

#include "core/userfunction.h"
#include "core/userunit.h"
#include "core/variable.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include <algorithm>

class GlobalSymbols {
public:
    static GlobalSymbols& instance()
    {
        static GlobalSymbols symbols;
        return symbols;
    }

    void clearUserDefinitionRegistry()
    {
        m_userVariables.clear();
        m_userFunctions.clear();
        m_userUnits.clear();
    }

    void registerUserVariable(const QString& id) { m_userVariables.insert(id); }
    void registerUserFunction(const QString& name) { m_userFunctions.insert(name); }
    void registerUserUnit(const QString& name) { m_userUnits.insert(name); }

    bool isUserVariable(const QString& id) const { return m_userVariables.contains(id); }
    bool isUserFunction(const QString& name) const { return m_userFunctions.contains(name); }
    bool isUserUnit(const QString& name) const { return m_userUnits.contains(name); }

private:
    GlobalSymbols() = default;
    Q_DISABLE_COPY(GlobalSymbols)

    QSet<QString> m_userVariables;
    QSet<QString> m_userFunctions;
    QSet<QString> m_userUnits;
};

class SessionVariableRepository {
public:
    void add(const Variable& variable) { m_variables[variable.identifier()] = variable; }
    bool contains(const QString& id) const { return m_variables.contains(id); }
    void remove(const QString& id) { m_variables.remove(id); }
    void clear() { m_variables.clear(); }
    Variable get(const QString& id) const { return m_variables.value(id); }
    QList<Variable> toList() const
    {
        QList<Variable> variables = m_variables.values();
        std::sort(variables.begin(), variables.end(), [](const Variable& lhs, const Variable& rhs) {
            return lhs.identifier().compare(rhs.identifier(), Qt::CaseInsensitive) < 0;
        });
        return variables;
    }
    bool isBuiltIn(const QString& id) const
    {
        return m_variables.contains(id) && m_variables.value(id).type() == Variable::BuiltIn;
    }

private:
    QHash<QString, Variable> m_variables;
};

class SessionFunctionRepository {
public:
    void add(const UserFunction& function) { m_functions[function.name()] = function; }
    bool contains(const QString& name) const { return m_functions.contains(name); }
    void remove(const QString& name) { m_functions.remove(name); }
    void clear() { m_functions.clear(); }
    QList<UserFunction> toList() const
    {
        QList<UserFunction> functions = m_functions.values();
        std::sort(functions.begin(), functions.end(), [](const UserFunction& lhs, const UserFunction& rhs) {
            return lhs.name().compare(rhs.name(), Qt::CaseInsensitive) < 0;
        });
        return functions;
    }
    const UserFunction* get(const QString& name) const
    {
        auto it = m_functions.constFind(name);
        return it == m_functions.constEnd() ? nullptr : &it.value();
    }

private:
    QHash<QString, UserFunction> m_functions;
};

class SessionUnitRepository {
public:
    void add(const UserUnit& unit) { m_units[unit.name()] = unit; }
    bool contains(const QString& name) const { return m_units.contains(name); }
    void remove(const QString& name) { m_units.remove(name); }
    void clear() { m_units.clear(); }
    QList<UserUnit> toList() const
    {
        QList<UserUnit> units = m_units.values();
        std::sort(units.begin(), units.end(), [](const UserUnit& lhs, const UserUnit& rhs) {
            return lhs.name().compare(rhs.name(), Qt::CaseInsensitive) < 0;
        });
        return units;
    }
    const UserUnit* get(const QString& name) const
    {
        auto it = m_units.constFind(name);
        return it == m_units.constEnd() ? nullptr : &it.value();
    }

private:
    QHash<QString, UserUnit> m_units;
};

class SymbolContext {
public:
    SessionVariableRepository& variables() { return m_variables; }
    const SessionVariableRepository& variables() const { return m_variables; }

    SessionFunctionRepository& functions() { return m_functions; }
    const SessionFunctionRepository& functions() const { return m_functions; }

    SessionUnitRepository& units() { return m_units; }
    const SessionUnitRepository& units() const { return m_units; }

    GlobalSymbols& globals() { return GlobalSymbols::instance(); }
    const GlobalSymbols& globals() const { return GlobalSymbols::instance(); }

private:
    SessionVariableRepository m_variables;
    SessionFunctionRepository m_functions;
    SessionUnitRepository m_units;
};

#endif
