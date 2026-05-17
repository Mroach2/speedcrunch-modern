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
#include <QObject>
#include <functions.h>
#include <algorithm>

namespace {
EvaluationContext currentEvaluationContextFromSettings()
{
    Settings* settings = Settings::instance();
    EvaluationContext ctx;
    ctx.main.fmt = settings->resultFormat;
    ctx.main.prec = settings->resultPrecision;
    ctx.main.cplx = settings->resultComplexForm;

    if (settings->multipleResultLinesEnabled) {
        if (settings->secondaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->alternativeResultFormat, settings->secondaryResultPrecision, settings->secondaryResultComplexForm});
        if (settings->tertiaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->tertiaryResultFormat, settings->tertiaryResultPrecision, settings->tertiaryResultComplexForm});
        if (settings->quaternaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->quaternaryResultFormat, settings->quaternaryResultPrecision, settings->quaternaryResultComplexForm});
        if (settings->quinaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->quinaryResultFormat, settings->quinaryResultPrecision, settings->quinaryResultComplexForm});
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
    settings->resultComplexForm = ctx.main.cplx;

    settings->multipleResultLinesEnabled = !ctx.extras.isEmpty();
    settings->secondaryResultEnabled = false;
    settings->tertiaryResultEnabled = false;
    settings->quaternaryResultEnabled = false;
    settings->quinaryResultEnabled = false;

    if (ctx.extras.size() > 0) {
        settings->secondaryResultEnabled = true;
        settings->alternativeResultFormat = ctx.extras.at(0).fmt;
        settings->secondaryResultPrecision = ctx.extras.at(0).prec;
        settings->secondaryResultComplexForm = ctx.extras.at(0).cplx;
    }
    if (ctx.extras.size() > 1) {
        settings->tertiaryResultEnabled = true;
        settings->tertiaryResultFormat = ctx.extras.at(1).fmt;
        settings->tertiaryResultPrecision = ctx.extras.at(1).prec;
        settings->tertiaryResultComplexForm = ctx.extras.at(1).cplx;
    }
    if (ctx.extras.size() > 2) {
        settings->quaternaryResultEnabled = true;
        settings->quaternaryResultFormat = ctx.extras.at(2).fmt;
        settings->quaternaryResultPrecision = ctx.extras.at(2).prec;
        settings->quaternaryResultComplexForm = ctx.extras.at(2).cplx;
    }
    if (ctx.extras.size() > 3) {
        settings->quinaryResultEnabled = true;
        settings->quinaryResultFormat = ctx.extras.at(3).fmt;
        settings->quinaryResultPrecision = ctx.extras.at(3).prec;
        settings->quinaryResultComplexForm = ctx.extras.at(3).cplx;
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

static void trimHistory(QList<HistoryEntry>& history, int limit)
{
    if (limit == 0 || history.size() <= limit)
        return;
    history.remove(0, history.size() - limit);
}

class SessionSerialization {
public:
    static void serialize(const Session& session, QJsonObject& json);
    static int deserialize(Session& session, const QJsonObject& json, bool merge);

private:
    static void recoverAns(Session& session);
};

void SessionSerialization::serialize(const Session& session, QJsonObject& json)
{
    const QString globalVariableTag = QObject::tr("Global User Variable");
    const QString globalFunctionTag = QObject::tr("Global User Function");
    const QString globalUnitTag = QObject::tr("Global User Unit");

    json[QLatin1String(SessionJsonKeys::SchemaVersion)] = SessionJsonKeys::SchemaVersionValue;
    json[QLatin1String(SessionJsonKeys::Session)] = session.name().isEmpty()
        ? QLatin1String(SessionJsonKeys::SessionValueMain)
        : session.name();
    json[QLatin1String(SessionJsonKeys::Limit)] = session.historyLimit();

    QJsonArray hist_entries;
    for (int i = 0; i < session.historySize(); ++i) {
        QJsonObject curr_entry_obj;
        session.historyEntryAtRef(i).serialize(curr_entry_obj);
        hist_entries.append(curr_entry_obj);
    }
    json[QLatin1String(SessionJsonKeys::History)] = hist_entries;

    QJsonArray var_entries;
    const QList<Variable> variables = session.variablesToList();
    for (const Variable& variable : variables) {
        QJsonObject curr_entry_obj;
        if (variable.type() == Variable::BuiltIn && variable.identifier() != QLatin1String("ans"))
            continue;
        if (variable.description().contains(globalVariableTag))
            continue;
        variable.serialize(curr_entry_obj);
        var_entries.append(curr_entry_obj);
    }
    json[QLatin1String(SessionJsonKeys::Variables)] = var_entries;

    QJsonArray func_entries;
    const QList<UserFunction> functions = session.UserFunctionsToList();
    for (const UserFunction& function : functions) {
        QJsonObject curr_entry_obj;
        if (function.description().contains(globalFunctionTag))
            continue;
        function.serialize(curr_entry_obj);
        func_entries.append(curr_entry_obj);
    }
    json[QLatin1String(SessionJsonKeys::Functions)] = func_entries;

    QJsonArray unit_entries;
    const QList<UserUnit> units = session.userUnitsToList();
    for (const UserUnit& unit : units) {
        QJsonObject curr_entry_obj;
        if (unit.description().contains(globalUnitTag))
            continue;
        unit.serialize(curr_entry_obj);
        unit_entries.append(curr_entry_obj);
    }
    json[QLatin1String(SessionJsonKeys::Units)] = unit_entries;

    QJsonArray globals;
    const QStringList definitions = Settings::instance()->startupUserDefinitions.split(QLatin1Char('\n'));
    for (const QString& line : definitions) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            globals.append(trimmed);
    }
    json[QLatin1String(SessionJsonKeys::Globals)] = globals;
}

int SessionSerialization::deserialize(Session& session, const QJsonObject& json, bool merge)
{
    auto hasType = [&json](const char* key, bool (QJsonValue::*predicate)() const) {
        const QJsonValue value = json.value(QLatin1String(key));
        return value.isUndefined() || (value.*predicate)();
    };
    auto arrayContainsOnlyObjects = [&json](const char* key) {
        const QJsonValue value = json.value(QLatin1String(key));
        if (value.isUndefined())
            return true;
        if (!value.isArray())
            return false;
        const QJsonArray array = value.toArray();
        for (const QJsonValue& entry : array) {
            if (!entry.isObject())
                return false;
        }
        return true;
    };

    if (json.isEmpty())
        return false;

    const QJsonValue schemaVersion = json.value(QLatin1String(SessionJsonKeys::SchemaVersion));
    if (!schemaVersion.isUndefined()
            && (!schemaVersion.isDouble() || schemaVersion.toInt() != SessionJsonKeys::SchemaVersionValue)) {
        return false;
    }
    if (!hasType(SessionJsonKeys::Session, &QJsonValue::isString)
            || !hasType(SessionJsonKeys::Limit, &QJsonValue::isDouble)
            || !hasType(SessionJsonKeys::History, &QJsonValue::isArray)
            || !hasType(SessionJsonKeys::Variables, &QJsonValue::isArray)
            || !hasType(SessionJsonKeys::Functions, &QJsonValue::isArray)
            || !hasType(SessionJsonKeys::Units, &QJsonValue::isArray)
            || !hasType(SessionJsonKeys::Globals, &QJsonValue::isArray)
            || !arrayContainsOnlyObjects(SessionJsonKeys::History)
            || !arrayContainsOnlyObjects(SessionJsonKeys::Variables)
            || !arrayContainsOnlyObjects(SessionJsonKeys::Functions)
            || !arrayContainsOnlyObjects(SessionJsonKeys::Units)) {
        return false;
    }

    if (!merge) {
        session.clearHistory();
        session.clearVariables();
        session.clearUserFunctions();
        session.clearUserUnits();
        session.setEditorText(QString());
    }
    if (!merge)
        session.setName(json[QLatin1String(SessionJsonKeys::Session)].toString());

    if (json.contains(QLatin1String(SessionJsonKeys::Limit)))
        session.setHistoryLimit(json[QLatin1String(SessionJsonKeys::Limit)].toInt(1000));

    session.evaluator()->initializeBuiltInVariables();

    QJsonArray hist_obj = json[QLatin1String(SessionJsonKeys::History)].toArray();
    int n = hist_obj.size();
    const int limit = session.historyLimit();
    const int firstHistoryIndex = limit > 0 ? std::max(0, n - limit) : 0;
    for (int i = firstHistoryIndex; i < n; ++i)
        session.addHistoryEntry(HistoryEntry(hist_obj[i].toObject()));

    QJsonArray var_obj = json[QLatin1String(SessionJsonKeys::Variables)].toArray();
    n = var_obj.size();
    for (int i = 0; i < n; ++i) {
        QJsonObject var = var_obj[i].toObject();
        Variable variable;
        variable.deSerialize(var);
        session.addVariable(variable);
    }

    QJsonArray func_obj = json[QLatin1String(SessionJsonKeys::Functions)].toArray();
    n = func_obj.size();
    for (int i = 0; i < n; ++i) {
        UserFunction func(func_obj[i].toObject());
        session.addUserFunction(func);
    }

    if (json.contains(QLatin1String(SessionJsonKeys::Units))) {
        QJsonArray unit_obj = json[QLatin1String(SessionJsonKeys::Units)].toArray();
        const int unitCount = unit_obj.size();
        for (int i = 0; i < unitCount; ++i) {
            UserUnit unit(unit_obj[i].toObject());
            session.addUserUnit(unit);
        }
    }
    if (!merge && json.contains(QLatin1String(SessionJsonKeys::Globals))) {
        const QJsonArray globalsObj = json[QLatin1String(SessionJsonKeys::Globals)].toArray();
        QStringList lines;
        lines.reserve(globalsObj.size());
        for (const QJsonValue& value : globalsObj) {
            if (value.isString() && !value.toString().trimmed().isEmpty())
                lines.append(value.toString().trimmed());
        }
        if (!lines.isEmpty() && Settings::instance()->startupUserDefinitions.trimmed().isEmpty())
            Settings::instance()->startupUserDefinitions = lines.join(QLatin1Char('\n'));
    }

    recoverAns(session);
    return true;
}

void SessionSerialization::recoverAns(Session& session)
{
    const EvaluationContext originalContext = currentEvaluationContextFromSettings();
    const bool hasAns = session.hasVariable("ans");
    const bool hasContextHistory = session.historySize() > 0 && session.historyEntryAtRef(0).hasContext();
    const bool needsAnsRecovery = hasContextHistory || !hasAns || session.getVariable("ans").value().isNan();
    if (!needsAnsRecovery)
        return;

    Quantity recoveredValue = CMath::nan();
    for (int i = session.historySize() - 1; i >= 0; --i) {
        const Quantity value = session.historyEntryAtRef(i).result();
        if (!value.isNan()) {
            recoveredValue = value;
            break;
        }
    }

    if (recoveredValue.isNan() && hasContextHistory) {
        Evaluator* evaluator = session.evaluator();
        for (int i = 0; i < session.historySize(); ++i) {
            const HistoryEntry entry = session.historyEntryAtRef(i);
            applyEvaluationContextToSettings(entry.contextRef());
            evaluator->setExpression(evaluator->autoFix(entry.expr()));
            const Quantity value = evaluator->evalUpdateAns();
            if (evaluator->error().isEmpty() && !value.isNan())
                recoveredValue = value;
        }
        applyEvaluationContextToSettings(originalContext);
    }

    if (!recoveredValue.isNan())
        session.addVariable(Variable("ans", recoveredValue, Variable::BuiltIn));
}

Session::Session()
    : m_evaluator(new Evaluator)
    , m_name(QLatin1String(SessionJsonKeys::SessionValueMain))
{
    bindEvaluator();
}

Session::Session(QJsonObject& json)
    : Session()
{
    deSerialize(json, false);
}

Session::Session(const Session& other)
    : m_history(other.m_history)
    , m_historyHead(other.m_historyHead)
    , m_evaluator(new Evaluator)
    , m_name(other.m_name)
    , m_editorText(other.m_editorText)
    , m_historyLimit(other.m_historyLimit)
{
    bindEvaluator();
    m_evaluator->copySymbolContextFrom(*other.m_evaluator);
}

Session& Session::operator=(const Session& other)
{
    if (this == &other)
        return *this;

    m_history = other.m_history;
    m_historyHead = other.m_historyHead;
    m_name = other.m_name;
    m_editorText = other.m_editorText;
    m_historyLimit = other.m_historyLimit;
    bindEvaluator();
    m_evaluator->copySymbolContextFrom(*other.m_evaluator);
    return *this;
}

Session::~Session() = default;

void Session::bindEvaluator()
{
    if (!m_evaluator)
        m_evaluator.reset(new Evaluator);
    m_evaluator->initializeBuiltInVariables();
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
    SessionSerialization::serialize(*this, json);
}

int Session::deSerialize(const QJsonObject &json, bool merge=false)
{
    return SessionSerialization::deserialize(*this, json, merge);
}

void Session::setName(const QString& name)
{
    const QString trimmed = name.trimmed();
    m_name = trimmed.isEmpty()
        ? QLatin1String(SessionJsonKeys::SessionValueMain)
        : trimmed;
}

void Session::setHistoryLimit(int limit)
{
    m_historyLimit = std::max(0, limit);
    applyHistoryLimit();
}

void Session::addVariable(const Variable &var)
{
    evaluator()->symbolContext().variables().add(var);
}

bool Session::hasVariable(const QString &id) const
{
    return evaluator()->symbolContext().variables().contains(id);
}

void Session::removeVariable(const QString &id)
{
    evaluator()->symbolContext().variables().remove(id);
}

void Session::clearVariables()
{
    evaluator()->symbolContext().variables().clear();
}

Variable Session::getVariable(const QString &id) const
{
    return evaluator()->symbolContext().variables().get(id);
}

QList<Variable> Session::variablesToList() const
{
    return evaluator()->symbolContext().variables().toList();
}

bool Session::isBuiltInVariable(const QString & id) const
{
    // Defining variables with the same name as existing functions is not supported for now.
    if(FunctionRepo::instance()->find(id))
        return true;
    if(!hasVariable(id))
        return false;

    return getVariable(id).type() == Variable::BuiltIn;
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

bool Session::nextHistoryEntryReachesLimit() const
{
    const int limit = historyLimit();
    return limit > 0 && m_history.size() == limit - 1;
}

void Session::insertHistoryEntry(const int index, const HistoryEntry &entry)
{
    normalizeHistoryOrder();
    m_history.insert(index, entry);
    trimHistory(m_history, historyLimit());
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
    trimHistory(m_history, historyLimit());
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
        evaluator()->setExpression(expression);
        evaluator()->eval();
    } else {
        evaluator()->symbolContext().functions().add(func);
    }
}

void Session::removeUserFunction(const QString &str)
{
    evaluator()->symbolContext().functions().remove(str);
}

void Session::clearUserFunctions()
{
    evaluator()->symbolContext().functions().clear();
}

bool Session::hasUserFunction(const QString &str) const
{
    return evaluator()->symbolContext().functions().contains(str);
}

QList<UserFunction> Session::UserFunctionsToList() const
{
    return evaluator()->symbolContext().functions().toList();
}

const UserFunction * Session::getUserFunction(const QString &fname) const
{
    return evaluator()->symbolContext().functions().get(fname);
}

void Session::addUserUnit(const UserUnit& unit)
{
    const QString name = unit.name();
    if (name.isEmpty())
        return;
    evaluator()->symbolContext().units().add(unit);
}

void Session::removeUserUnit(const QString& name)
{
    evaluator()->symbolContext().units().remove(name);
}

void Session::clearUserUnits()
{
    evaluator()->symbolContext().units().clear();
}

bool Session::hasUserUnit(const QString& name) const
{
    return evaluator()->symbolContext().units().contains(name);
}

QList<UserUnit> Session::userUnitsToList() const
{
    return evaluator()->symbolContext().units().toList();
}

const UserUnit* Session::getUserUnit(const QString& name) const
{
    return evaluator()->symbolContext().units().get(name);
}
