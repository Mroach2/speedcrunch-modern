// SPDX-FileCopyrightText: 2015-2016, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_VARIABLE_H
#define CORE_VARIABLE_H

#include <QString>
#include <QSet>
#include <QList>

#include "math/quantity.h"

class Variable
{
public:
    enum Type { BuiltIn, UserDefined };
private:
    QString m_identifier;
    Quantity m_value;
    Type m_type;
    QString m_description;
    QString m_formattedValue;
public:
    Variable() : m_identifier(""), m_value(0), m_type(UserDefined), m_description("") {}
    Variable(const QJsonObject & json);
    Variable(const QString & id, const Quantity & val, Type t = UserDefined,
             const QString& description = QString())
        : m_identifier(id), m_value(val), m_type(t), m_description(description) {}
    Variable(const Variable & other)
        :  m_identifier(other.m_identifier), m_value(other.m_value),
           m_type(other.m_type), m_description(other.m_description),
           m_formattedValue(other.m_formattedValue) {}
    Variable& operator=(const Variable&) = default;

    Quantity value() const {return m_value;}
    QString identifier() const {return m_identifier;}
    Type type() const {return m_type;}
    QString description() const {return m_description;}
    QString formattedValue() const { return m_formattedValue; }

    void setValue(const Quantity & val) {m_value = val;}
    void set_identifier(const QString & str) {m_identifier = str;}
    void set_type(const Type t) {m_type = t;}
    void setDescription(const QString& description) {m_description = description;}
    void setFormattedValue(const QString& formattedValue) { m_formattedValue = formattedValue; }

    void serialize(QJsonObject & json) const;
    void deSerialize(const QJsonObject & json);
    bool operator==(const Variable& other) const { return m_identifier == other.m_identifier; }
};

#endif // CORE_VARIABLE_H
