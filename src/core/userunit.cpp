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
