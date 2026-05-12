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
#include <QString>
#include <QJsonArray>
#include <memory>

class Evaluator;
class Session {
private:
    typedef QList<HistoryEntry> History ;
    History m_history;
    int m_historyHead = 0; // Logical index 0 maps to physical m_historyHead.
    std::unique_ptr<Evaluator> m_evaluator;
    QString m_name;
    QString m_editorText;
    int physicalHistoryIndex(int logicalIndex) const;
    void normalizeHistoryOrder();
    void bindEvaluator();

public:
    Session();
    Session(QJsonObject & json);
    Session(const Session& other);
    Session& operator=(const Session& other);
    ~Session();

    Evaluator* evaluator() { return m_evaluator.get(); }
    const Evaluator* evaluator() const { return m_evaluator.get(); }

    void load();
    void save();

    void serialize(QJsonObject &json) const;
    int deSerialize(const QJsonObject & json, bool merge);
    QString name() const { return m_name; }
    void setName(const QString& name);
    QString editorText() const { return m_editorText; }
    void setEditorText(const QString& text) { m_editorText = text; }


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
