// SPDX-FileCopyrightText: 2015-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "session.h"
#include "sessionhistory.h"
#include "variable.h"
#include "evaluator.h"
#include "settings.h"
#include "sessionjsonkeys.h"

#include <QFile>
#include <QJsonDocument>
#include <functions.h>
#include <algorithm>

namespace {
EvaluationContext currentEvaluationContextFromSettings()
{
    Settings* settings = Settings::instance();
    EvaluationContext ctx;
    ctx.main.fmt = settings->resultFormat;
    ctx.main.prec = settings->resultPrecision;
    ctx.main.cplx = settings->resultFormatComplex;

    if (settings->multipleResultLinesEnabled) {
        if (settings->secondaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->alternativeResultFormat, settings->secondaryResultPrecision, settings->secondaryResultFormatComplex});
        if (settings->tertiaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->tertiaryResultFormat, settings->tertiaryResultPrecision, settings->tertiaryResultFormatComplex});
        if (settings->quaternaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->quaternaryResultFormat, settings->quaternaryResultPrecision, settings->quaternaryResultFormatComplex});
        if (settings->quinaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->quinaryResultFormat, settings->quinaryResultPrecision, settings->quinaryResultFormatComplex});
    }

    ctx.complexOn = settings->complexNumbers;
    ctx.unit = settings->imaginaryUnit;
    ctx.angle = settings->angleUnit;
    ctx.unitExp = settings->unitNegativeExponentStyle;
    ctx.round = settings->resultRoundingMode;
    return ctx;
}

void applyEvaluationContextToSettings(const EvaluationContext& ctx)
{
    Settings* settings = Settings::instance();
    settings->resultFormat = ctx.main.fmt;
    settings->resultPrecision = ctx.main.prec;
    settings->resultFormatComplex = ctx.main.cplx;

    settings->multipleResultLinesEnabled = !ctx.extras.isEmpty();
    settings->secondaryResultEnabled = false;
    settings->tertiaryResultEnabled = false;
    settings->quaternaryResultEnabled = false;
    settings->quinaryResultEnabled = false;

    if (ctx.extras.size() > 0) {
        settings->secondaryResultEnabled = true;
        settings->alternativeResultFormat = ctx.extras.at(0).fmt;
        settings->secondaryResultPrecision = ctx.extras.at(0).prec;
        settings->secondaryResultFormatComplex = ctx.extras.at(0).cplx;
    }
    if (ctx.extras.size() > 1) {
        settings->tertiaryResultEnabled = true;
        settings->tertiaryResultFormat = ctx.extras.at(1).fmt;
        settings->tertiaryResultPrecision = ctx.extras.at(1).prec;
        settings->tertiaryResultFormatComplex = ctx.extras.at(1).cplx;
    }
    if (ctx.extras.size() > 2) {
        settings->quaternaryResultEnabled = true;
        settings->quaternaryResultFormat = ctx.extras.at(2).fmt;
        settings->quaternaryResultPrecision = ctx.extras.at(2).prec;
        settings->quaternaryResultFormatComplex = ctx.extras.at(2).cplx;
    }
    if (ctx.extras.size() > 3) {
        settings->quinaryResultEnabled = true;
        settings->quinaryResultFormat = ctx.extras.at(3).fmt;
        settings->quinaryResultPrecision = ctx.extras.at(3).prec;
        settings->quinaryResultFormatComplex = ctx.extras.at(3).cplx;
    }

    settings->complexNumbers = ctx.complexOn;
    settings->imaginaryUnit = (ctx.unit == 'j') ? 'j' : 'i';
    settings->angleUnit = ctx.angle;
    settings->unitNegativeExponentStyle = isValidUnitNegativeExponentStyle(ctx.unitExp)
        ? ctx.unitExp
        : Settings::UnitNegativeExponentSuperscript;
    settings->resultRoundingMode = isValidResultRoundingMode(ctx.round)
        ? ctx.round
        : Settings::ResultRoundingHalfAwayFromZero;
    DMath::complexMode = settings->complexNumbers;
    CMath::setImaginaryUnitSymbol(settings->imaginaryUnit);
    setRuntimeUnitNegativeExponentStyle(settings->unitNegativeExponentStyle);
    setRuntimeResultRoundingMode(settings->resultRoundingMode);
}
}

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
    json[QLatin1String(SessionJsonKeys::SchemaVersion)] = SessionJsonKeys::SchemaVersionValue;
    json[QLatin1String(SessionJsonKeys::SpeedCrunch)] = QString(SPEEDCRUNCH_VERSION);
    json[QLatin1String(SessionJsonKeys::Session)] = QLatin1String(SessionJsonKeys::SessionValueMain);

    // history
    QJsonArray hist_entries;
    for (int i = 0; i < m_history.size(); ++i) {
        QJsonObject curr_entry_obj;
        historyEntryAtRef(i).serialize(curr_entry_obj);
        hist_entries.append(curr_entry_obj);
    }
    json[QLatin1String(SessionJsonKeys::History)] = hist_entries;

    //variables
    QJsonArray var_entries;
    QHashIterator<QString, Variable> i(m_variables);
    while(i.hasNext()) {
        i.next();
        QJsonObject curr_entry_obj;
        //ignore builtin variables
        if(i.value().type()==Variable::BuiltIn && i.value().identifier()!=QLatin1String("ans"))
            continue;
        i.value().serialize(curr_entry_obj);
        var_entries.append(curr_entry_obj);
    }
    json[QLatin1String(SessionJsonKeys::Variables)] = var_entries;


    // functions
    QJsonArray func_entries;
    QHashIterator<QString, UserFunction> j(m_userFunctions);
    while(j.hasNext()) {
        j.next();
        QJsonObject curr_entry_obj;
        j.value().serialize(curr_entry_obj);
        func_entries.append(curr_entry_obj);
    }
    json[QLatin1String(SessionJsonKeys::Functions)] = func_entries;

    QJsonArray unit_entries;
    QHashIterator<QString, UserUnit> k(m_userUnits);
    while (k.hasNext()) {
        k.next();
        QJsonObject curr_entry_obj;
        k.value().serialize(curr_entry_obj);
        unit_entries.append(curr_entry_obj);
    }
    json[QLatin1String(SessionJsonKeys::Units)] = unit_entries;
}

int Session::deSerialize(const QJsonObject &json, bool merge=false)
{
    const int schemaVersion = json[QLatin1String(SessionJsonKeys::SchemaVersion)].toInt();
    (void)schemaVersion;
    QString version = json[QLatin1String(SessionJsonKeys::SpeedCrunch)].toString();
    if(!merge) {
        m_history.clear();
        m_historyHead = 0;
        m_variables.clear();
        m_userFunctions.clear();
        m_userUnits.clear();
    }

    Evaluator::instance()->initializeBuiltInVariables();

    QJsonArray hist_obj = json[QLatin1String(SessionJsonKeys::History)].toArray();
    int n = hist_obj.size();
    for (int i = 0; i < n; ++i)
        addHistoryEntry(HistoryEntry(hist_obj[i].toObject()));

    QJsonArray var_obj = json[QLatin1String(SessionJsonKeys::Variables)].toArray();
    n = var_obj.size();
    for(int i=0; i<n; ++i) {
        QJsonObject var = var_obj[i].toObject();
        m_variables[var[QLatin1String(SessionJsonKeys::Variable::Id)].toString()].deSerialize(var);
    }

    QJsonArray func_obj = json[QLatin1String(SessionJsonKeys::Functions)].toArray();
    n = func_obj.size();
    for(int i=0; i<n; ++i) {
        UserFunction func(func_obj[i].toObject());
        addUserFunction(func);
    }

    if (json.contains(QLatin1String(SessionJsonKeys::Units))) {
        QJsonArray unit_obj = json[QLatin1String(SessionJsonKeys::Units)].toArray();
        const int n = unit_obj.size();
        for (int i = 0; i < n; ++i) {
            UserUnit unit(unit_obj[i].toObject());
            addUserUnit(unit);
        }
    }

    // Recover ans from history when missing or NaN, e.g. older sessions where
    // comment-only lines could overwrite ans with NaN.
    const EvaluationContext originalContext = currentEvaluationContextFromSettings();
    const bool hasAns = hasVariable("ans");
    const bool hasContextHistory = !m_history.isEmpty() && m_history.first().hasContext();
    const bool needsAnsRecovery = hasContextHistory || !hasAns || getVariable("ans").value().isNan();
    if (needsAnsRecovery) {
        Quantity recoveredValue = CMath::nan();
        for (int i = m_history.size() - 1; i >= 0; --i) {
            const Quantity value = historyEntryAtRef(i).result();
            if (!value.isNan()) {
                recoveredValue = value;
                break;
            }
        }

        if (recoveredValue.isNan() && hasContextHistory) {
            Evaluator* evaluator = Evaluator::instance();
            for (int i = 0; i < m_history.size(); ++i) {
                const HistoryEntry entry = historyEntryAtRef(i);
                applyEvaluationContextToSettings(entry.contextRef());
                evaluator->setExpression(evaluator->autoFix(entry.expr()));
                const Quantity value = evaluator->evalUpdateAns();
                if (evaluator->error().isEmpty() && !value.isNan())
                    recoveredValue = value;
            }
            applyEvaluationContextToSettings(originalContext);
        }

        if (!recoveredValue.isNan()) {
            addVariable(Variable("ans", recoveredValue, Variable::BuiltIn));
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
