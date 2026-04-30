// SPDX-FileCopyrightText: 2015-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "variable.h"


Variable::Variable(const QJsonObject &json)
{
    deSerialize(json);
}

void Variable::serialize(QJsonObject &json) const
{
    json["id"] = m_identifier;
    QJsonObject value;
    m_value.serialize(value);
    json["qty"] = value;
    if (!m_description.isEmpty())
        json["dsc"] = m_description;
}

void Variable::deSerialize(const QJsonObject &json)
{
    if (json.contains("id"))
        m_identifier = json["id"].toString();

    m_type = (m_identifier == QStringLiteral("ans")) ? BuiltIn : UserDefined;

    if (json.contains("qty"))
        m_value = Quantity(json["qty"].toObject());

    m_description = json["dsc"].toString();
}
