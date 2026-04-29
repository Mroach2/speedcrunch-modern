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


#ifndef CORE_SESSIONHISTORY_H
#define CORE_SESSIONHISTORY_H

#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QList>
#include <QVector>

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
public:
    HistoryEntry() : m_expr(""), m_result(0) {}
    HistoryEntry(const QJsonObject & json);
    HistoryEntry(const QString & expr, const Quantity & num);
    HistoryEntry(const QString & expr, const Quantity & num, const QString& interpretedExpr);
    HistoryEntry(const QString & expr, const EvaluationContext& ctx);
    HistoryEntry(const QString & expr, const Quantity & num, const QString& interpretedExpr, const EvaluationContext& ctx);
    HistoryEntry(const HistoryEntry & other)
        : m_expr(other.m_expr), m_interpretedExpr(other.m_interpretedExpr), m_result(other.m_result), m_ctx(other.m_ctx), m_hasCtx(other.m_hasCtx), m_renderedLines(other.m_renderedLines) {}
    HistoryEntry& operator=(const HistoryEntry& other) = default;    

    void setExpr(const QString & e);
    void setInterpretedExpr(const QString& e);
    void setResult(const Quantity & n);
    void setContext(const EvaluationContext& ctx);
    void setRenderedLines(const QStringList& lines);

    QString expr() const;
    QString interpretedExpr() const;
    Quantity result() const;
    EvaluationContext context() const;
    const EvaluationContext& contextRef() const;
    bool hasContext() const;
    QStringList renderedLines() const;
    bool hasRenderedLines() const;

    void serialize(QJsonObject & json) const;
    void deSerialize(const QJsonObject & json);
};

#endif // CORE_SESSIONHISTORY_H
