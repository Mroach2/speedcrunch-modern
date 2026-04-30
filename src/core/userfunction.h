// SPDX-FileCopyrightText: 2015-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later




#ifndef CORE_USERFUNCTION_H
#define CORE_USERFUNCTION_H

#include<QString>
#include<QStringList>
#include<QVector>
#include "core/opcode.h"
#include "math/quantity.h"

class UserFunction
{
private:
    QString m_name;
    QStringList m_arguments;
    QString m_expression;
    QString m_interpretedExpression;
    QString m_description;

public:
    QVector<Quantity> constants;
    QStringList identifiers;
    QVector<Opcode> opcodes;

    UserFunction(QString name, QStringList arguments, QString expression,
                 QString description = QString(),
                 QString interpretedExpression = QString())
        : m_name(name)
        , m_arguments(arguments)
        , m_expression(expression)
        , m_interpretedExpression(interpretedExpression)
        , m_description(description)
    {}
    UserFunction() {}
    UserFunction(const QJsonObject & json);

    QString name() const;
    QStringList arguments() const;
    QString expression() const;
    QString interpretedExpression() const;
    QString description() const;

    void setName(const QString & str);
    void setArguments(const QStringList & args);
    void setExpression(const QString & expr);
    void setInterpretedExpression(const QString& expr);
    void setDescription(const QString & expr);

    void serialize(QJsonObject & json) const;
    void deSerialize(const QJsonObject & json);
};

#endif // CORE_USERFUNCTION_H
