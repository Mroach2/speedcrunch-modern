// SPDX-FileCopyrightText: 2007-2010, 2013-2016, 2018, 2020, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_EVALUATOR_H
#define CORE_EVALUATOR_H

#include "core/functions.h"
#include "core/opcode.h"
#include "core/variable.h"
#include "core/userfunction.h"
#include "core/userunit.h"

#include "math/hmath.h"
#include "math/cmath.h"
#include "math/quantity.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class Session;

class Token {
public:
    enum Operator {
        Invalid = 0,
        Addition, Subtraction, Multiplication,
        Division, IntegerDivision, Exponentiation,
        Super0, Super1, Super2, Super3, Super4,
        Super5, Super6, Super7, Super8, Super9,
        AssociationStart, AssociationEnd,
        ListSeparator, Factorial, Assignment, Modulo, Percent,
        ArithmeticLeftShift, ArithmeticRightShift,
        BitwiseLogicalNOT,
        BitwiseLogicalAND, BitwiseLogicalOR,
        UnitConversion,
        Function // For managing shift/reduce conflicts.
    };
    enum Type {
        stxUnknown, stxNumber, stxIdentifier, stxUnitIdentifier, stxAbstract, // isOperand
        stxOperator, stxOpenPar, stxClosePar, stxSep // isOperator
    };

    static const Token null;

    Token(Type = stxUnknown, const QString& = QString(), int pos = -1,
          int size = -1);
    Token(const Token&);

    Quantity asNumber() const;
    Operator asOperator() const;
    QString description() const;
    bool isNumber() const { return m_type == stxNumber; }
    bool isOperator() const { return m_type >= stxOperator; }
    bool isIdentifier() const { return m_type == stxIdentifier; }
    bool isUnitIdentifier() const { return m_type == stxUnitIdentifier; }
    bool isAbstract() const { return m_type == stxAbstract; }
    bool isOperand() const { return isNumber() || isIdentifier()
                                    || isUnitIdentifier() || isAbstract(); }
    int pos() const { return m_pos; }
    void setPos(int pos) { m_pos = pos; }
    int size() const { return m_size; }
    void setSize(int size) { m_size = size; }
    QString text() const { return m_text; }
    Type type() const { return m_type; }
    int minPrecedence() const { return m_minPrecedence; }
    void setMinPrecedence(int value) { m_minPrecedence = value; }

    Token& operator=(const Token&);

protected:
    // Start position of the text that token represents in the expression
    // (might include extra space characters).
    int m_pos;
    // Size of text that token represents in the expression
    // (might include extra space characters).
    int m_size;
    // Precedence of the operator with the lowest precedence contained
    // in this token.
    int m_minPrecedence;
    // Normalized version of that token text (only valid when the token
    // represents a single token).
    QString m_text;
    Type m_type;
};

class Tokens : public QVector<Token> {
public:
    Tokens()
        : QVector<Token>()
        , m_valid(true)
    { }

    bool valid() const { return m_valid; }
    void setValid(bool v) { m_valid = v; }

#ifdef EVALUATOR_DEBUG
    void append(const Token&);
#endif  /* EVALUATOR_DEBUG */

protected:
    bool m_valid;
};

class Evaluator : public QObject {
    Q_OBJECT
    using ForceBuiltinVariableErasure = bool;

public:
    static Evaluator* instance();
    void reset();

    void setSession(Session*);
    const Session* session();

    static bool isSeparatorChar(const QChar&);
    static bool isRadixChar(const QChar&);
    static bool isCommentOnlyExpression(const QString&);
    static QString simplifyInterpretedExpression(const QString&);
    static QString formatInterpretedExpressionForDisplay(const QString&);
    static QString formatInterpretedExpressionSimplifiedForDisplay(const QString&);
    static QString fixNumberRadix(const QString&);
    static QString fixSexagesimal(const QString&, QString& unit);
    static QStringList builtInUnitIdentifiers();

    QString autoFix(const QString&);
    QString dump();
    QString error() const;
    QString interpretedExpression() const;
    Quantity eval();
    Quantity evalNoAssign();
    Quantity evalUpdateAns();
    QString expression() const;
    bool hasImplicitMultiplication() const;
    bool isValid();
    Tokens scan(const QString&) const;
    void setExpression(const QString&);
    Tokens tokens() const;
    bool isUserFunctionAssign() const;
    bool isUserUnitAssign() const;
    bool isUserVariableAssign() const;

    Variable getVariable(const QString&) const;
    QList<Variable> getVariables() const;
    QList<Variable> getUserDefinedVariables() const;
    QList<Variable> getUserDefinedVariablesPlusAns() const;
    void setVariable(const QString&, Quantity,
                     Variable::Type = Variable::UserDefined,
                     const QString& = QString(),
                     const QString& = QString());
    void unsetVariable(const QString&, ForceBuiltinVariableErasure = false);
    void unsetAllUserDefinedVariables();
    bool isBuiltInVariable(const QString&) const;
    bool hasVariable(const QString&) const;
    void initializeBuiltInVariables();
    void initializeAngleUnits();

    QList<UserFunction> getUserFunctions() const;
    void setUserFunction(const UserFunction&);
    void unsetUserFunction(const QString&);
    void unsetAllUserFunctions();
    bool hasUserFunction(const QString&) const;

    QList<UserUnit> getUserUnits() const;
    QStringList userUnitIdentifiers() const;
    QStringList allUnitIdentifiers() const;
    void setUserUnit(const UserUnit&);
    void unsetUserUnit(const QString&);
    void unsetAllUserUnits();
    bool hasUserUnit(const QString&) const;
    const UserUnit* getUserUnit(const QString&) const;

protected:
    void compile(const Tokens&);

private:
    Evaluator();
    Q_DISABLE_COPY(Evaluator)

    bool m_dirty;
    QString m_error;
    QString m_expression;
    QString m_interpretedExpression;
    bool m_valid;
    QString m_assignId;
    bool m_assignFunc;
    QStringList m_assignArg;
    QString m_assignVarDescription;
    QString m_assignUnitDescription;
    QString m_assignFuncExpr;
    QString m_assignUnitExpr;
    QString m_assignFuncDescription;
    bool m_assignUnit;
    QVector<Opcode> m_codes;
    QVector<Quantity> m_constants;
    QStringList m_constantTexts;
    QStringList m_identifiers;
    QSet<int> m_implicitMultiplicationOpcodeIndices;
    Session* m_session;
    QSet<QString> m_functionsInUse;
    bool m_hasImplicitMultiplication;

    const Quantity& checkOperatorResult(const Quantity&);
    static QString stringFromFunctionError(Function*);
    Quantity exec(const QVector<Opcode>& opcodes,
                  const QVector<Quantity>& constants,
                  const QStringList& identifiers);
    QString buildInterpretedExpressionFromOpcodes() const;
    Quantity execUserFunction(const UserFunction* function,
                              QVector<Quantity>& arguments);
    const UserFunction* getUserFunction(const QString&) const;
    bool tryGetAnyUnitQuantity(const QString& identifier, Quantity* valueOut) const;

    bool isFunction(Token token) {
        return token.isIdentifier()
                && (FunctionRepo::instance()->find(token.text())
                    || hasUserFunction(token.text()));
    }
};

#endif
