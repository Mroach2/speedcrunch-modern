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

#include "sessionhistory.h"
#include "core/settings.h"
#include "math/cmath.h"

namespace {
EvaluationContext contextFromCurrentSettings()
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

QJsonObject serializeResultLineContext(const ResultLineContext& line)
{
    QJsonObject json;
    json["fmt"] = QString(QChar(line.fmt));
    json["prec"] = line.prec;
    json["cplx"] = QString(QChar(line.cplx));
    return json;
}

ResultLineContext deserializeResultLineContext(const QJsonObject& json)
{
    ResultLineContext line;
    const QString fmt = json["fmt"].toString();
    if (fmt.size() == 1)
        line.fmt = fmt.at(0).toLatin1();
    line.prec = json["prec"].toInt(-1);
    const QString cplx = json["cplx"].toString();
    if (cplx.size() == 1)
        line.cplx = cplx.at(0).toLatin1();
    return line;
}
}

void EvaluationContext::serialize(QJsonObject& json) const
{
    QJsonObject lines;
    lines["main"] = serializeResultLineContext(main);
    QJsonArray extrasArray;
    for (const ResultLineContext& line : extras)
        extrasArray.append(serializeResultLineContext(line));
    lines["extras"] = extrasArray;
    json["lines"] = lines;

    QJsonObject complex;
    complex["on"] = complexOn;
    complex["unit"] = QString(QChar(unit));
    json["complex"] = complex;
    json["angle"] = QString(QChar(angle));
    json["unitExp"] = QString(QChar(unitExp));
    json["round"] = QString(QChar(round));
}

void EvaluationContext::deSerialize(const QJsonObject& json)
{
    *this = EvaluationContext();

    const QJsonObject lines = json["lines"].toObject();
    if (lines.contains("main"))
        main = deserializeResultLineContext(lines["main"].toObject());
    const QJsonArray extrasArray = lines["extras"].toArray();
    for (const QJsonValue& value : extrasArray) {
        if (value.isObject())
            extras.append(deserializeResultLineContext(value.toObject()));
    }
    while (extras.size() > 4)
        extras.removeLast();

    const QJsonObject complex = json["complex"].toObject();
    complexOn = complex["on"].toBool(false);
    const QString unitText = complex["unit"].toString();
    if (unitText.size() == 1)
        unit = unitText.at(0).toLatin1();

    const QString angleText = json["angle"].toString();
    if (angleText.size() == 1)
        angle = angleText.at(0).toLatin1();

    const QString unitExpText = json["unitExp"].toString();
    if (unitExpText.size() == 1)
        unitExp = unitExpText.at(0).toLatin1();
    if (!isValidUnitNegativeExponentStyle(unitExp))
        unitExp = Settings::UnitNegativeExponentSuperscript;

    const QString roundText = json["round"].toString();
    if (roundText.size() == 1)
        round = roundText.at(0).toLatin1();
    if (!isValidResultRoundingMode(round))
        round = Settings::ResultRoundingHalfAwayFromZero;
}


HistoryEntry::HistoryEntry(const QJsonObject & json)
{
    deSerialize(json);
}

HistoryEntry::HistoryEntry(const QString & expr, const Quantity & num)
    : m_expr(expr), m_result(num), m_ctx(contextFromCurrentSettings()), m_hasCtx(true)
{
}

HistoryEntry::HistoryEntry(const QString & expr, const Quantity & num, const QString& interpretedExpr)
    : m_expr(expr), m_interpretedExpr(interpretedExpr), m_result(num),
      m_ctx(contextFromCurrentSettings()), m_hasCtx(true)
{
}

HistoryEntry::HistoryEntry(const QString & expr, const EvaluationContext& ctx)
    : m_expr(expr), m_result(0), m_ctx(ctx), m_hasCtx(true)
{
}

HistoryEntry::HistoryEntry(const QString & expr, const Quantity & num, const QString& interpretedExpr,
                           const EvaluationContext& ctx)
    : m_expr(expr), m_interpretedExpr(interpretedExpr), m_result(num), m_ctx(ctx), m_hasCtx(true)
{
}

QString HistoryEntry::expr() const
{
    return m_expr;
}

Quantity HistoryEntry::result() const
{
    return m_result;
}

QString HistoryEntry::interpretedExpr() const
{
    return m_interpretedExpr;
}

void HistoryEntry::setExpr(const QString & e)
{
    m_expr = e;
}

void HistoryEntry::setInterpretedExpr(const QString& e)
{
    m_interpretedExpr = e;
}

void HistoryEntry::setResult(const Quantity & n)
{
    m_result = n;
}

void HistoryEntry::setContext(const EvaluationContext& ctx)
{
    m_ctx = ctx;
}

void HistoryEntry::serialize(QJsonObject & json) const
{
    json["expr"] = m_expr;
    QJsonObject ctx;
    m_ctx.serialize(ctx);
    json["ctx"] = ctx;
}

void HistoryEntry::deSerialize(const QJsonObject & json)
{
    *this = HistoryEntry();
    m_result = CMath::nan();

    if (json.contains("expr"))
        m_expr = json["expr"].toString();

    if (json.contains("ctx")) {
        m_ctx.deSerialize(json["ctx"].toObject());
        m_hasCtx = true;
    }
}

EvaluationContext HistoryEntry::context() const
{
    return m_ctx;
}

const EvaluationContext& HistoryEntry::contextRef() const
{
    return m_ctx;
}

bool HistoryEntry::hasContext() const
{
    return m_hasCtx;
}
