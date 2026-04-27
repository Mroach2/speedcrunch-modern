// This file is part of the SpeedCrunch project
// Copyright (C) 2015 Pol Welter <polwelter@gmail.com>
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
