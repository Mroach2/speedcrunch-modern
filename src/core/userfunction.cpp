// SPDX-FileCopyrightText: 2015-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later



#include "core/userfunction.h"
#include "core/evaluator.h"
#include "core/opcode.h"
#include "core/sessionjsonkeys.h"
#include <QJsonArray>


//#define SAVE_COMPILED_FORM



UserFunction::UserFunction(const QJsonObject &json) : UserFunction()
{
    if (json.contains(QLatin1String(SessionJsonKeys::Function::Id)))
        m_name = json[QLatin1String(SessionJsonKeys::Function::Id)].toString();

    if (json.contains(QLatin1String(SessionJsonKeys::Function::Arguments))) {
        const QJsonArray args_json = json[QLatin1String(SessionJsonKeys::Function::Arguments)].toArray();
        int n = json[QLatin1String(SessionJsonKeys::Function::Arguments)].toArray().size();
        for(int i=0; i<n; ++i)
            m_arguments.append(args_json.at(i).toString());
    }

    if (json.contains(QLatin1String(SessionJsonKeys::Function::Expression)))
        m_expression = json[QLatin1String(SessionJsonKeys::Function::Expression)].toString();
    if (json.contains(QLatin1String(SessionJsonKeys::Function::InterpretedExpression)))
        m_interpretedExpression = json[QLatin1String(SessionJsonKeys::Function::InterpretedExpression)].toString();
    if(json.contains(QLatin1String(SessionJsonKeys::Function::Description)))
        m_description = json[QLatin1String(SessionJsonKeys::Function::Description)].toString();

    if(json.contains(QLatin1String(SessionJsonKeys::Function::Opcodes))) {
        const QJsonArray  & codes_json = json[QLatin1String(SessionJsonKeys::Function::Opcodes)].toArray();
        for(int i=0; i<codes_json.size(); ++i) {
            Opcode opcode(static_cast<Opcode::Type>(
                              codes_json[i].toObject()[QLatin1String(SessionJsonKeys::Function::Opcode::Type)].toInt()),
                          codes_json[i].toObject()[QLatin1String(SessionJsonKeys::Function::Opcode::Index)].toInt());
            if(codes_json[i].toObject().contains(QLatin1String(SessionJsonKeys::Function::Opcode::Text)))
                opcode.text = codes_json[i].toObject()[QLatin1String(SessionJsonKeys::Function::Opcode::Text)].toString();
            opcodes.append(opcode);
        }

        const QJsonArray & const_json = json[QLatin1String(SessionJsonKeys::Function::Constants)].toArray();
        for(int i=0; i<const_json.size(); ++i) {
            CNumber hn(const_json[i].toObject());
            constants.append(hn);
        }

        const QJsonArray & id_json = json[QLatin1String(SessionJsonKeys::Function::Identifiers)].toArray();
        for(int i=0; i<id_json.size(); ++i) {
            identifiers.append(id_json[i].toString());
        }

    }
}

QString UserFunction::name() const
{
    return m_name;
}

QStringList UserFunction::arguments() const
{
    return m_arguments;
}

QString UserFunction::expression() const
{
    return m_expression;
}

QString UserFunction::interpretedExpression() const
{
    return m_interpretedExpression;
}

QString UserFunction::description() const
{
    return m_description;
}

void UserFunction::setName(const QString &str)
{
    m_name = str;
}

void UserFunction::setArguments(const QStringList &args)
{
    m_arguments = args;
}

void UserFunction::setExpression(const QString &expr)
{
    m_expression = expr;
}

void UserFunction::setInterpretedExpression(const QString& expr)
{
    m_interpretedExpression = expr;
}

void UserFunction::setDescription(const QString &expr)
{
    m_description = expr;
}

void UserFunction::serialize(QJsonObject &json) const
{
    json[QLatin1String(SessionJsonKeys::Function::Id)] = m_name;
    QJsonArray args;
    for(int i=0; i<m_arguments.count(); ++i)
        args.append(m_arguments[i]);
    json[QLatin1String(SessionJsonKeys::Function::Arguments)] = args;
    json[QLatin1String(SessionJsonKeys::Function::Expression)] = m_expression;
    if (m_interpretedExpression != "")
        json[QLatin1String(SessionJsonKeys::Function::InterpretedExpression)] = m_interpretedExpression;
    if(m_description!="")
        json[QLatin1String(SessionJsonKeys::Function::Description)] = m_description;

#ifdef SAVE_COMPILED_FORM
    // if compiled form is available, save it as well
    if(!opcodes.isEmpty()) {
        QJsonArray opcodes_json;
        for(int i=0;i<opcodes.size(); ++i) {
            QJsonObject curr_code_json;
            const Opcode & curr_code = opcodes.at(i);
            curr_code_json[QLatin1String(SessionJsonKeys::Function::Opcode::Type)] = curr_code.type;
            curr_code_json[QLatin1String(SessionJsonKeys::Function::Opcode::Index)] = int(curr_code.index);
            if(curr_code.text != "")
                curr_code_json[QLatin1String(SessionJsonKeys::Function::Opcode::Text)] = curr_code.text;
            opcodes_json.append(curr_code_json);
        }
        json[QLatin1String(SessionJsonKeys::Function::Opcodes)] = opcodes_json;

        QJsonArray constants_json;
        for(int i=0; i<constants.size(); ++i) {
            QJsonObject curr_const_json;
            constants.at(i).serialize(curr_const_json);
            constants_json.append(curr_const_json);
        }
        json[QLatin1String(SessionJsonKeys::Function::Constants)] = constants_json;

        QJsonArray identifiers_json;
        for(int i=0; i<identifiers.size(); ++i) {
            identifiers_json.append(identifiers.at(i));
        }
        json[QLatin1String(SessionJsonKeys::Function::Identifiers)] = identifiers_json;
    }
#endif
}

void UserFunction::deSerialize(const QJsonObject &json)
{
    *this = UserFunction(json);

}
