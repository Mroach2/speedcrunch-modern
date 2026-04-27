// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "core/userunit.h"

UserUnit::UserUnit(const QJsonObject& json)
{
    deSerialize(json);
}

void UserUnit::serialize(QJsonObject& json) const
{
    json[QStringLiteral("name")] = m_name;

    QJsonObject valueJson;
    m_value.serialize(valueJson);
    json[QStringLiteral("value")] = valueJson;

    if (!m_expression.isEmpty())
        json[QStringLiteral("expression")] = m_expression;
    if (!m_interpretedExpression.isEmpty())
        json[QStringLiteral("interpretedExpression")] = m_interpretedExpression;
    if (!m_description.isEmpty())
        json[QStringLiteral("description")] = m_description;
}

void UserUnit::deSerialize(const QJsonObject& json)
{
    m_name = json[QStringLiteral("name")].toString();
    if (json.contains(QStringLiteral("value")) && json[QStringLiteral("value")].isObject())
        m_value.deSerialize(json[QStringLiteral("value")].toObject());
    else
        m_value = Quantity(1);

    m_expression = json[QStringLiteral("expression")].toString();
    m_interpretedExpression = json[QStringLiteral("interpretedExpression")].toString();
    m_description = json[QStringLiteral("description")].toString();
}
