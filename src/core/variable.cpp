// SPDX-FileCopyrightText: 2015-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "variable.h"
#include "sessionjsonkeys.h"


Variable::Variable(const QJsonObject &json)
{
    deSerialize(json);
}

void Variable::serialize(QJsonObject &json) const
{
    json[QLatin1String(SessionJsonKeys::Variable::Id)] = m_identifier;
    QJsonObject value;
    m_value.serialize(value);
    json[QLatin1String(SessionJsonKeys::Variable::Quantity)] = value;
    if (!m_description.isEmpty())
        json[QLatin1String(SessionJsonKeys::Variable::Description)] = m_description;
    if (!m_formattedValue.isEmpty())
        json[QLatin1String(SessionJsonKeys::Variable::FormattedValue)] = m_formattedValue;
}

void Variable::deSerialize(const QJsonObject &json)
{
    if (json.contains(QLatin1String(SessionJsonKeys::Variable::Id)))
        m_identifier = json[QLatin1String(SessionJsonKeys::Variable::Id)].toString();

    m_type = (m_identifier == QStringLiteral("ans")) ? BuiltIn : UserDefined;

    if (json.contains(QLatin1String(SessionJsonKeys::Variable::Quantity)))
        m_value = Quantity(json[QLatin1String(SessionJsonKeys::Variable::Quantity)].toObject());

    m_description = json[QLatin1String(SessionJsonKeys::Variable::Description)].toString();
    m_formattedValue = json[QLatin1String(SessionJsonKeys::Variable::FormattedValue)].toString();
}
