// SPDX-FileCopyrightText: 2015-2016, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later



#ifndef CORE_SESSIONHISTORY_H
#define CORE_SESSIONHISTORY_H

#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>
#include <QtGlobal>

#include "math/quantity.h"

struct ResultLineContext
{
    char fmt = 'g';
    int prec = -1;
    char cplx = 'c';
};

struct EvaluationContext
{
    ResultLineContext main;
    QVector<ResultLineContext> extras;
    bool complexOn = false;
    char unit = 'i';
    char angle = 'r';
    char unitExp = 's';
    char round = 'a';

    void serialize(QJsonObject& json) const;
    void deSerialize(const QJsonObject& json);
};

class HistoryEntry
{
private:
    QString m_expr;
    QString m_interpretedExpr;
    Quantity m_result;
    EvaluationContext m_ctx;
    bool m_hasCtx = false;
    QStringList m_renderedLines;
    qint64 m_editTimestamp = 0;
public:
    HistoryEntry() : m_expr(""), m_result(0) {}
    HistoryEntry(const QJsonObject & json);
    HistoryEntry(const QString & expr, const Quantity & num);
    HistoryEntry(const QString & expr, const Quantity & num, const QString& interpretedExpr);
    HistoryEntry(const QString & expr, const EvaluationContext& ctx);
    HistoryEntry(const QString & expr, const Quantity & num, const QString& interpretedExpr, const EvaluationContext& ctx);
    HistoryEntry(const HistoryEntry & other)
        : m_expr(other.m_expr), m_interpretedExpr(other.m_interpretedExpr),
          m_result(other.m_result), m_ctx(other.m_ctx), m_hasCtx(other.m_hasCtx),
          m_renderedLines(other.m_renderedLines), m_editTimestamp(other.m_editTimestamp) {}
    HistoryEntry& operator=(const HistoryEntry& other) = default;    

    void setExpr(const QString & e);
    void setInterpretedExpr(const QString& e);
    void setResult(const Quantity & n);
    void setContext(const EvaluationContext& ctx);
    void setRenderedLines(const QStringList& lines);
    void setEditTimestamp(qint64 timestamp);

    QString expr() const;
    QString interpretedExpr() const;
    Quantity result() const;
    EvaluationContext context() const;
    const EvaluationContext& contextRef() const;
    bool hasContext() const;
    QStringList renderedLines() const;
    bool hasRenderedLines() const;
    qint64 editTimestamp() const;

    void serialize(QJsonObject & json) const;
    void deSerialize(const QJsonObject & json);
};

#endif // CORE_SESSIONHISTORY_H
