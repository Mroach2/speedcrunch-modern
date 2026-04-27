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

#include "session.h"
#include "sessionhistory.h"
#include "variable.h"
#include "evaluator.h"
#include "settings.h"

#include <QFile>
#include <QJsonDocument>
#include <functions.h>
#include <algorithm>

static int historyLimit()
{
    return std::max(0, Settings::instance()->maxHistoryEntries);
}

static void trimHistory(QList<HistoryEntry>& history)
{
    const int limit = historyLimit();
    if (limit == 0 || history.size() <= limit)
        return;
    history.remove(0, history.size() - limit);
}

int Session::physicalHistoryIndex(int logicalIndex) const
{
    const int size = m_history.size();
    if (size == 0)
        return -1;
    return (m_historyHead + logicalIndex) % size;
}

void Session::normalizeHistoryOrder()
{
    if (m_historyHead == 0 || m_history.isEmpty())
        return;

    History normalized;
    normalized.reserve(m_history.size());
    for (int i = 0; i < m_history.size(); ++i)
        normalized.append(m_history.at(physicalHistoryIndex(i)));

    m_history.swap(normalized);
    m_historyHead = 0;
}

void Session::serialize(QJsonObject &json) const
{
    json["version"] = QString(SPEEDCRUNCH_VERSION);

    // history
    QJsonArray hist_entries;
    for (int i = 0; i < m_history.size(); ++i) {
        QJsonObject curr_entry_obj;
        historyEntryAtRef(i).serialize(curr_entry_obj);
        hist_entries.append(curr_entry_obj);
    }
    json["history"] = hist_entries;

    //variables
    QJsonArray var_entries;
    QHashIterator<QString, Variable> i(m_variables);
    while(i.hasNext()) {
        i.next();
        QJsonObject curr_entry_obj;
        //ignore builtin variables
        if(i.value().type()==Variable::BuiltIn && i.value().identifier()!="ans")
            continue;
        i.value().serialize(curr_entry_obj);
        var_entries.append(curr_entry_obj);
    }
    json["variables"] = var_entries;


    // functions
    QJsonArray func_entries;
    QHashIterator<QString, UserFunction> j(m_userFunctions);
    while(j.hasNext()) {
        j.next();
        QJsonObject curr_entry_obj;
        j.value().serialize(curr_entry_obj);
        func_entries.append(curr_entry_obj);
    }
    json["functions"] = func_entries;

    QJsonArray unit_entries;
    QHashIterator<QString, UserUnit> k(m_userUnits);
    while (k.hasNext()) {
        k.next();
        QJsonObject curr_entry_obj;
        k.value().serialize(curr_entry_obj);
        unit_entries.append(curr_entry_obj);
    }
    json["units"] = unit_entries;
}

int Session::deSerialize(const QJsonObject &json, bool merge=false)
{
    QString version = json["version"].toString();
    if(!merge) {
        m_history.clear();
        m_historyHead = 0;
        m_variables.clear();
        m_userFunctions.clear();
        m_userUnits.clear();
    }

    Evaluator::instance()->initializeBuiltInVariables();

    if (json.contains("history")) {
        QJsonArray hist_obj = json["history"].toArray();
        int n = hist_obj.size();
        for (int i = 0; i < n; ++i)
            addHistoryEntry(HistoryEntry(hist_obj[i].toObject()));
    }

    if (json.contains("variables")) {
        QJsonArray var_obj = json["variables"].toArray();
        int n = var_obj.size();
        for(int i=0; i<n; ++i) {
            QJsonObject var = var_obj[i].toObject();
            m_variables[var["identifier"].toString()].deSerialize(var);
        }
    }

    if (json.contains("functions")) {
        QJsonArray func_obj = json["functions"].toArray();
        int n = func_obj.size();
        for(int i=0; i<n; ++i) {
            UserFunction func(func_obj[i].toObject());
            addUserFunction(func);
        }
    }

    if (json.contains("units")) {
        QJsonArray unit_obj = json["units"].toArray();
        const int n = unit_obj.size();
        for (int i = 0; i < n; ++i) {
            UserUnit unit(unit_obj[i].toObject());
            addUserUnit(unit);
        }
    }

    // Recover ans from history when missing or NaN, e.g. older sessions where
    // comment-only lines could overwrite ans with NaN.
    const bool hasAns = hasVariable("ans");
    const bool needsAnsRecovery = !hasAns || getVariable("ans").value().isNan();
    if (needsAnsRecovery) {
        for (int i = m_history.size() - 1; i >= 0; --i) {
            const Quantity value = historyEntryAtRef(i).result();
            if (!value.isNan()) {
                addVariable(Variable("ans", value, Variable::BuiltIn));
                break;
            }
        }
    }

    return version==SPEEDCRUNCH_VERSION;
}

void Session::addVariable(const Variable &var)
{
    QString id = var.identifier();
    m_variables[id] = var;
}

bool Session::hasVariable(const QString &id) const
{
    return m_variables.contains(id);
}

void Session::removeVariable(const QString &id)
{
    m_variables.remove(id);
}

void Session::clearVariables()
{
    m_variables.clear();
}

Variable Session::getVariable(const QString &id) const
{
    return m_variables.value(id);
}

QList<Variable> Session::variablesToList() const
{
    return m_variables.values();
}

bool Session::isBuiltInVariable(const QString & id) const
{
    // Defining variables with the same name as existing functions is not supported for now.
    if(FunctionRepo::instance()->find(id))
        return true;
    if(!m_variables.contains(id))
        return false;

    return m_variables.value(id).type() == Variable::BuiltIn;
}

void Session::addHistoryEntry(const HistoryEntry &entry)
{
    const int limit = historyLimit();
    if (limit > 0) {
        if (m_history.size() < limit) {
            m_history.append(entry);
        } else if (limit > 0) {
            m_history[m_historyHead] = entry;
            m_historyHead = (m_historyHead + 1) % limit;
        }
        return;
    }

    m_history.append(entry);
}

void Session::insertHistoryEntry(const int index, const HistoryEntry &entry)
{
    normalizeHistoryOrder();
    m_history.insert(index, entry);
    trimHistory(m_history);
}

void Session::removeHistoryEntryAt(const int index)
{
    normalizeHistoryOrder();
    m_history.removeAt(index);
}

const HistoryEntry& Session::historyEntryAtRef(const int index) const
{
    return m_history.at(physicalHistoryIndex(index));
}

HistoryEntry Session::historyEntryAt(const int index) const
{
    return historyEntryAtRef(index);
}

QList<HistoryEntry> Session::historyToList() const
{
    if (m_historyHead == 0)
        return m_history;

    QList<HistoryEntry> ordered;
    ordered.reserve(m_history.size());
    for (int i = 0; i < m_history.size(); ++i)
        ordered.append(historyEntryAtRef(i));
    return ordered;
}

void Session::applyHistoryLimit()
{
    normalizeHistoryOrder();
    trimHistory(m_history);
}

void Session::clearHistory()
{
    m_history.clear();
    m_historyHead = 0;
}

void Session::addUserFunction(const UserFunction &func)
{
    if(func.opcodes.isEmpty()) {
        // We need to compile the function, so pretend the user typed it.
        QString expression = func.name() + "(" + func.arguments().join(";") + ")=" + func.expression();
        if (!func.description().isEmpty())
            expression += " ? " + func.description();
        Evaluator::instance()->setExpression(expression);
        Evaluator::instance()->eval();
    } else {
        QString name = func.name();
        m_userFunctions[name] = func;
    }
}

void Session::removeUserFunction(const QString &str)
{
    m_userFunctions.remove(str);
}

void Session::clearUserFunctions()
{
    m_userFunctions.clear();
}

bool Session::hasUserFunction(const QString &str) const
{
    return m_userFunctions.contains(str);
}

QList<UserFunction> Session::UserFunctionsToList() const
{
    return m_userFunctions.values();
}

const UserFunction * Session::getUserFunction(const QString &fname) const
{
    return &*m_userFunctions.find(fname);
}

void Session::addUserUnit(const UserUnit& unit)
{
    const QString name = unit.name();
    if (name.isEmpty())
        return;
    m_userUnits[name] = unit;
}

void Session::removeUserUnit(const QString& name)
{
    m_userUnits.remove(name);
}

void Session::clearUserUnits()
{
    m_userUnits.clear();
}

bool Session::hasUserUnit(const QString& name) const
{
    return m_userUnits.contains(name);
}

QList<UserUnit> Session::userUnitsToList() const
{
    return m_userUnits.values();
}

const UserUnit* Session::getUserUnit(const QString& name) const
{
    return &*m_userUnits.find(name);
}
