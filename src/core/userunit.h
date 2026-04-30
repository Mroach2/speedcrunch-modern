// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_USERUNIT_H
#define CORE_USERUNIT_H

#include "math/quantity.h"

#include <QJsonObject>
#include <QString>

class UserUnit
{
public:
    UserUnit() {}
    UserUnit(const QString& name,
             const Quantity& value,
             const QString& expression = QString(),
             const QString& interpretedExpression = QString(),
             const QString& description = QString())
        : m_name(name)
        , m_value(value)
        , m_expression(expression)
        , m_interpretedExpression(interpretedExpression)
        , m_description(description)
    {
    }
    UserUnit(const QJsonObject& json);

    QString name() const { return m_name; }
    Quantity value() const { return m_value; }
    QString expression() const { return m_expression; }
    QString interpretedExpression() const { return m_interpretedExpression; }
    QString description() const { return m_description; }

    void setName(const QString& value) { m_name = value; }
    void setValue(const Quantity& value) { m_value = value; }
    void setExpression(const QString& value) { m_expression = value; }
    void setInterpretedExpression(const QString& value) { m_interpretedExpression = value; }
    void setDescription(const QString& value) { m_description = value; }

    void serialize(QJsonObject& json) const;
    void deSerialize(const QJsonObject& json);

private:
    QString m_name;
    Quantity m_value;
    QString m_expression;
    QString m_interpretedExpression;
    QString m_description;
};

#endif // CORE_USERUNIT_H
