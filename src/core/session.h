// SPDX-FileCopyrightText: 2015-2016, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_SESSION_H
#define CORE_SESSION_H

#include "hmath.h"
#include "sessionhistory.h"
#include "variable.h"
#include "userfunction.h"
#include "userunit.h"
#include <QList>
#include <QHash>
#include <QString>
#include <QJsonArray>

class Session {
private:
    typedef QList<HistoryEntry> History ;
    typedef QHash<QString, Variable> VariableContainer;
    typedef QHash<QString, UserFunction> FunctionContainer;
    typedef QHash<QString, UserUnit> UnitContainer;
    History m_history;
    int m_historyHead = 0; // Logical index 0 maps to physical m_historyHead.
    VariableContainer m_variables;
    FunctionContainer m_userFunctions;
    UnitContainer m_userUnits;
    int physicalHistoryIndex(int logicalIndex) const;
    void normalizeHistoryOrder();

public:
    Session() {}
    Session(QJsonObject & json);
    Session& operator=(const Session&) = default;

    void load();
    void save();

    void serialize(QJsonObject &json) const;
    int deSerialize(const QJsonObject & json, bool merge);


    void addVariable(const Variable & var);
    bool hasVariable(const QString & id) const;
    void removeVariable(const QString & id);
    void clearVariables();
    Variable getVariable(const QString & id) const;
    QList<Variable> variablesToList() const;
    bool isBuiltInVariable(const QString &id) const;

    void addHistoryEntry(const HistoryEntry & entry);
    void insertHistoryEntry(const int index, const HistoryEntry & entry);
    void removeHistoryEntryAt(const int index);
    int historySize() const { return m_history.size(); }
    bool historyIsEmpty() const { return m_history.isEmpty(); }
    const HistoryEntry& historyEntryAtRef(const int index) const;
    HistoryEntry historyEntryAt(const int index) const;
    QList<HistoryEntry> historyToList() const;
    void applyHistoryLimit();
    void clearHistory();

    void addUserFunction(const UserFunction & func);
    void removeUserFunction(const QString & str);
    void clearUserFunctions();
    bool hasUserFunction(const QString & str) const;
    QList<UserFunction> UserFunctionsToList() const;
    const UserFunction * getUserFunction(const QString & fname) const;

    void addUserUnit(const UserUnit& unit);
    void removeUserUnit(const QString& name);
    void clearUserUnits();
    bool hasUserUnit(const QString& name) const;
    QList<UserUnit> userUnitsToList() const;
    const UserUnit* getUserUnit(const QString& name) const;
};

#endif // CORE_SESSION_H
