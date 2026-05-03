// SPDX-FileCopyrightText: 2015-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "sessionhistory.h"
#include "core/settings.h"
#include "core/sessionjsonkeys.h"
#include "math/cmath.h"
#include <QDateTime>

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
    json[QLatin1String(SessionJsonKeys::EvaluationContext::Line::Notation)] = QString(QChar(line.fmt));
    json[QLatin1String(SessionJsonKeys::EvaluationContext::Line::Precision)] = line.prec;
    json[QLatin1String(SessionJsonKeys::EvaluationContext::Line::ComplexFormat)] = QString(QChar(line.cplx));
    return json;
}

ResultLineContext deserializeResultLineContext(const QJsonObject& json)
{
    ResultLineContext line;
    const QString fmt = json[QLatin1String(SessionJsonKeys::EvaluationContext::Line::Notation)].toString();
    if (fmt.size() == 1)
        line.fmt = fmt.at(0).toLatin1();
    line.prec = json[QLatin1String(SessionJsonKeys::EvaluationContext::Line::Precision)].toInt(-1);
    const QString cplx = json[QLatin1String(SessionJsonKeys::EvaluationContext::Line::ComplexFormat)].toString();
    if (cplx.size() == 1)
        line.cplx = cplx.at(0).toLatin1();
    return line;
}

qint64 currentEditTimestampMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}
}

void EvaluationContext::serialize(QJsonObject& json) const
{
    QJsonObject lines;
    lines[QLatin1String(SessionJsonKeys::EvaluationContext::MainLine)] = serializeResultLineContext(main);
    QJsonArray extrasArray;
    for (const ResultLineContext& line : extras)
        extrasArray.append(serializeResultLineContext(line));
    lines[QLatin1String(SessionJsonKeys::EvaluationContext::ExtraLines)] = extrasArray;
    json[QLatin1String(SessionJsonKeys::EvaluationContext::Lines)] = lines;

    if (!complexOn)
        json[QLatin1String(SessionJsonKeys::EvaluationContext::Complex)] = QLatin1String(SessionJsonKeys::EvaluationContext::ComplexValues::Off);
    else
        json[QLatin1String(SessionJsonKeys::EvaluationContext::Complex)] =
            (unit == 'j')
            ? QLatin1String(SessionJsonKeys::EvaluationContext::ComplexValues::J)
            : QLatin1String(SessionJsonKeys::EvaluationContext::ComplexValues::I);
    json[QLatin1String(SessionJsonKeys::EvaluationContext::Angle)] = QString(QChar(angle));
    json[QLatin1String(SessionJsonKeys::EvaluationContext::UnitExponentStyle)] = QString(QChar(unitExp));
    json[QLatin1String(SessionJsonKeys::EvaluationContext::Rounding)] = QString(QChar(round));
}

void EvaluationContext::deSerialize(const QJsonObject& json)
{
    *this = EvaluationContext();

    const QJsonObject lines = json[QLatin1String(SessionJsonKeys::EvaluationContext::Lines)].toObject();
    if (lines.contains(QLatin1String(SessionJsonKeys::EvaluationContext::MainLine)))
        main = deserializeResultLineContext(lines[QLatin1String(SessionJsonKeys::EvaluationContext::MainLine)].toObject());
    const QJsonArray extrasArray = lines[QLatin1String(SessionJsonKeys::EvaluationContext::ExtraLines)].toArray();
    for (const QJsonValue& value : extrasArray) {
        if (value.isObject())
            extras.append(deserializeResultLineContext(value.toObject()));
    }
    while (extras.size() > 4)
        extras.removeLast();

    const QString complexText = json[QLatin1String(SessionJsonKeys::EvaluationContext::Complex)].toString();
    if (complexText == QLatin1String(SessionJsonKeys::EvaluationContext::ComplexValues::J)) {
        complexOn = true;
        unit = 'j';
    } else if (complexText == QLatin1String(SessionJsonKeys::EvaluationContext::ComplexValues::I)) {
        complexOn = true;
        unit = 'i';
    } else {
        complexOn = false;
        unit = 'i';
    }

    const QString angleText = json[QLatin1String(SessionJsonKeys::EvaluationContext::Angle)].toString();
    if (angleText.size() == 1)
        angle = angleText.at(0).toLatin1();

    const QString unitExpText = json[QLatin1String(SessionJsonKeys::EvaluationContext::UnitExponentStyle)].toString();
    if (unitExpText.size() == 1)
        unitExp = unitExpText.at(0).toLatin1();
    if (!isValidUnitNegativeExponentStyle(unitExp))
        unitExp = Settings::UnitNegativeExponentSuperscript;

    const QString roundText = json[QLatin1String(SessionJsonKeys::EvaluationContext::Rounding)].toString();
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
    : m_expr(expr), m_result(num), m_ctx(contextFromCurrentSettings()),
      m_hasCtx(true), m_editTimestamp(currentEditTimestampMs())
{
}

HistoryEntry::HistoryEntry(const QString & expr, const Quantity & num, const QString& interpretedExpr)
    : m_expr(expr), m_interpretedExpr(interpretedExpr), m_result(num),
      m_ctx(contextFromCurrentSettings()), m_hasCtx(true), m_editTimestamp(currentEditTimestampMs())
{
}

HistoryEntry::HistoryEntry(const QString & expr, const EvaluationContext& ctx)
    : m_expr(expr), m_result(0), m_ctx(ctx), m_hasCtx(true), m_editTimestamp(currentEditTimestampMs())
{
}

HistoryEntry::HistoryEntry(const QString & expr, const Quantity & num, const QString& interpretedExpr,
                           const EvaluationContext& ctx)
    : m_expr(expr), m_interpretedExpr(interpretedExpr), m_result(num),
      m_ctx(ctx), m_hasCtx(true), m_editTimestamp(currentEditTimestampMs())
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
    m_editTimestamp = currentEditTimestampMs();
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
    m_editTimestamp = currentEditTimestampMs();
}

void HistoryEntry::setRenderedLines(const QStringList& lines)
{
    m_renderedLines = lines;
}

void HistoryEntry::setEditTimestamp(qint64 timestamp)
{
    m_editTimestamp = timestamp;
}

void HistoryEntry::serialize(QJsonObject & json) const
{
    json[QLatin1String(SessionJsonKeys::HistoryEntry::Expression)] = m_expr;
    json[QLatin1String(SessionJsonKeys::HistoryEntry::InterpretedExpression)] = m_interpretedExpr;
    QJsonObject result;
    m_result.serialize(result);
    json[QLatin1String(SessionJsonKeys::HistoryEntry::Result)] = result;
    QJsonObject ctx;
    m_ctx.serialize(ctx);
    json[QLatin1String(SessionJsonKeys::HistoryEntry::Context)] = ctx;
    json[QLatin1String(SessionJsonKeys::HistoryEntry::EditTimestamp)] = m_editTimestamp;
    if (!m_renderedLines.isEmpty()) {
        QJsonArray renderedLines;
        for (const QString& line : m_renderedLines)
            renderedLines.append(line);
        json[QLatin1String(SessionJsonKeys::HistoryEntry::PrintedLines)] = renderedLines;
    }
}

void HistoryEntry::deSerialize(const QJsonObject & json)
{
    *this = HistoryEntry();
    m_result = CMath::nan();

    if (json.contains(QLatin1String(SessionJsonKeys::HistoryEntry::Expression)))
        m_expr = json[QLatin1String(SessionJsonKeys::HistoryEntry::Expression)].toString();
    if (json.contains(QLatin1String(SessionJsonKeys::HistoryEntry::InterpretedExpression)))
        m_interpretedExpr = json[QLatin1String(SessionJsonKeys::HistoryEntry::InterpretedExpression)].toString();
    if (json.contains(QLatin1String(SessionJsonKeys::HistoryEntry::Result))
            && json[QLatin1String(SessionJsonKeys::HistoryEntry::Result)].isObject())
        m_result = Quantity::deSerialize(json[QLatin1String(SessionJsonKeys::HistoryEntry::Result)].toObject());

    if (json.contains(QLatin1String(SessionJsonKeys::HistoryEntry::Context))) {
        m_ctx.deSerialize(json[QLatin1String(SessionJsonKeys::HistoryEntry::Context)].toObject());
        m_hasCtx = true;
    }
    if (json.contains(QLatin1String(SessionJsonKeys::HistoryEntry::EditTimestamp)))
        m_editTimestamp = json[QLatin1String(SessionJsonKeys::HistoryEntry::EditTimestamp)].toVariant().toLongLong();
    else
        m_editTimestamp = currentEditTimestampMs();

    if (json.contains(QLatin1String(SessionJsonKeys::HistoryEntry::PrintedLines))) {
        const QJsonArray renderedLines = json[QLatin1String(SessionJsonKeys::HistoryEntry::PrintedLines)].toArray();
        for (const QJsonValue& value : renderedLines) {
            if (value.isString())
                m_renderedLines.append(value.toString());
        }
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

QStringList HistoryEntry::renderedLines() const
{
    return m_renderedLines;
}

bool HistoryEntry::hasRenderedLines() const
{
    return !m_renderedLines.isEmpty();
}

qint64 HistoryEntry::editTimestamp() const
{
    return m_editTimestamp;
}
