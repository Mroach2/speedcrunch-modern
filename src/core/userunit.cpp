// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/userunit.h"

UserUnit::UserUnit(const QJsonObject& json)
{
    deSerialize(json);
}

void UserUnit::serialize(QJsonObject& json) const
{
    json[QStringLiteral("id")] = m_name;

    QJsonObject valueJson;
    m_value.serialize(valueJson);
    json[QStringLiteral("qty")] = valueJson;

    if (!m_expression.isEmpty())
        json[QStringLiteral("xpr")] = m_expression;
    if (!m_interpretedExpression.isEmpty())
        json[QStringLiteral("itp")] = m_interpretedExpression;
    if (!m_description.isEmpty())
        json[QStringLiteral("dsc")] = m_description;
}

void UserUnit::deSerialize(const QJsonObject& json)
{
    m_name = json[QStringLiteral("id")].toString();
    if (json.contains(QStringLiteral("qty")) && json[QStringLiteral("qty")].isObject())
        m_value = Quantity::deSerialize(json[QStringLiteral("qty")].toObject());
    else
        m_value = Quantity(1);

    m_expression = json[QStringLiteral("xpr")].toString();
    m_interpretedExpression = json[QStringLiteral("itp")].toString();
    m_description = json[QStringLiteral("dsc")].toString();
}
