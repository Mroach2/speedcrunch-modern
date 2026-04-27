// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "gui/functiontooltiputils.h"
#include "core/evaluator.h"
#include "core/functions.h"
#include "core/mathdsl.h"
#include "core/unicodechars.h"
#include "core/userfunction.h"

#include <QVector>

namespace {

struct ActiveFunctionCallContext {
    QString functionName;
    int argumentIndex;
};

static bool trailingFunctionIdentifierAtCursor(const Evaluator* evaluator,
                                               const QString& expression,
                                               int cursorPosition,
                                               QString* functionName)
{
    if (!functionName)
        return false;

    const int safeCursorPosition = qBound(0, cursorPosition, expression.length());
    const QString prefix = expression.left(safeCursorPosition);
    const Tokens tokens = evaluator->scan(prefix);
    if (!tokens.valid() || tokens.isEmpty())
        return false;

    const Token last = tokens.last();
    if (!last.isIdentifier())
        return false;

    const int tokenEnd = last.pos() + last.size();
    if (tokenEnd != prefix.length())
        return false;

    *functionName = last.text();
    return true;
}

static bool openFunctionCallPrefixAtCursor(const QString& expression,
                                           int cursorPosition,
                                           QString* functionName)
{
    if (!functionName)
        return false;

    const int safeCursorPosition = qBound(0, cursorPosition, expression.length());
    const QString prefix = expression.left(safeCursorPosition);
    static const QRegularExpression s_openCallAtCursor(
        QStringLiteral(R"(([A-Za-z_][A-Za-z_0-9]*)\s*\(\s*$)"));
    const QRegularExpressionMatch match = s_openCallAtCursor.match(prefix);
    if (!match.hasMatch())
        return false;

    *functionName = match.captured(1);
    return true;
}

static bool emptyFunctionCallAroundCursor(const QString& expression,
                                          int cursorPosition,
                                          QString* functionName)
{
    if (!functionName)
        return false;

    const int safeCursorPosition = qBound(0, cursorPosition, expression.length());

    int openPos = -1;
    int closePos = -1;
    for (int i = 0; i < expression.size(); ++i) {
        if (expression.at(i) != MathDsl::GroupStart)
            continue;
        int depth = 1;
        for (int j = i + 1; j < expression.size(); ++j) {
            if (expression.at(j) == MathDsl::GroupStart)
                ++depth;
            else if (expression.at(j) == MathDsl::GroupEnd)
                --depth;
            if (depth == 0) {
                const bool cursorInside =
                    safeCursorPosition >= i + 1 && safeCursorPosition <= j + 1;
                if (cursorInside) {
                    openPos = i;
                    closePos = j;
                }
                i = j;
                break;
            }
        }
    }

    if (openPos < 0 || closePos < 0)
        return false;

    const QString inside = expression.mid(openPos + 1, closePos - openPos - 1).trimmed();
    if (!inside.isEmpty())
        return false;

    int nameEnd = openPos - 1;
    while (nameEnd >= 0 && expression.at(nameEnd).isSpace())
        --nameEnd;
    if (nameEnd < 0)
        return false;

    int nameStart = nameEnd;
    while (nameStart >= 0
           && (expression.at(nameStart).isLetterOrNumber()
               || expression.at(nameStart) == UnicodeChars::LowLine)) {
        --nameStart;
    }
    ++nameStart;
    if (nameStart > nameEnd)
        return false;

    *functionName = expression.mid(nameStart, nameEnd - nameStart + 1);
    return true;
}

bool activeFunctionCallContext(const Evaluator* evaluator,
                               const QString& expression,
                               int cursorPosition,
                               ActiveFunctionCallContext* context)
{
    if (!context)
        return false;

    const int safeCursorPosition = qBound(0, cursorPosition, expression.length());
    const Tokens tokens = evaluator->scan(expression.left(safeCursorPosition));
    if (!tokens.valid() || tokens.isEmpty())
        return false;

    struct Scope {
        QString functionName;
        int argumentIndex;
    };
    QVector<Scope> scopes;

    Token previous = Token::null;
    bool hasPrevious = false;

    for (int i = 0; i < tokens.count(); ++i) {
        const Token token = tokens.at(i);
        const Token::Operator op = token.asOperator();

        if (op == Token::AssociationStart && token.text() == QLatin1String("(")) {
            Scope scope;
            scope.argumentIndex = 0;
            if (hasPrevious && previous.isIdentifier())
                scope.functionName = previous.text();
            scopes.append(scope);
        } else if (op == Token::AssociationEnd && token.text() == QLatin1String(")")) {
            if (!scopes.isEmpty())
                scopes.removeLast();
        } else if (op == Token::ListSeparator) {
            if (!scopes.isEmpty() && !scopes.last().functionName.isEmpty())
                ++scopes.last().argumentIndex;
        }

        previous = token;
        hasPrevious = true;
    }

    for (int i = scopes.count() - 1; i >= 0; --i) {
        if (!scopes.at(i).functionName.isEmpty()) {
            context->functionName = scopes.at(i).functionName;
            context->argumentIndex = scopes.at(i).argumentIndex;
            return true;
        }
    }

    return false;
}

QString formatFunctionUsageTooltip(const QString& functionName,
                                   const QStringList& parameters,
                                   int argumentIndex,
                                   bool escapeParameters)
{
    QStringList displayParameters;
    displayParameters.reserve(parameters.count());

    for (int i = 0; i < parameters.count(); ++i) {
        QString parameter = escapeParameters
            ? parameters.at(i).toHtmlEscaped()
            : parameters.at(i);
        if (i == argumentIndex) {
            int leading = 0;
            while (leading < parameter.size() && parameter.at(leading).isSpace())
                ++leading;
            int trailing = 0;
            while (trailing < parameter.size() - leading
                   && parameter.at(parameter.size() - 1 - trailing).isSpace()) {
                ++trailing;
            }
            const QString prefix = parameter.left(leading);
            const QString core = parameter.mid(
                leading, parameter.size() - leading - trailing);
            const QString suffix = parameter.right(trailing);
            parameter = prefix + QStringLiteral("<b>%1</b>").arg(core) + suffix;
        }
        displayParameters.append(parameter);
    }

    const QString escapedFunctionName = functionName.toHtmlEscaped();
    return QStringLiteral("<b>%1</b>(%2)")
        .arg(escapedFunctionName, displayParameters.join(";"));
}

} // namespace

namespace FunctionTooltipUtils {

QString activeFunctionUsageTooltip(const Evaluator* evaluator,
                                   const QString& expression,
                                   int cursorPosition)
{
    ActiveFunctionCallContext context;
    if (!activeFunctionCallContext(evaluator, expression, cursorPosition, &context)) {
        // Fallback for manual typing: show usage when cursor is just after a
        // recognized function identifier, even before typing '('.
        QString trailingFunctionName;
        if (!emptyFunctionCallAroundCursor(expression, cursorPosition, &trailingFunctionName)
            && !openFunctionCallPrefixAtCursor(expression, cursorPosition, &trailingFunctionName)
            && !trailingFunctionIdentifierAtCursor(
                evaluator, expression, cursorPosition, &trailingFunctionName)) {
            return QString();
        }
        context.functionName = trailingFunctionName;
        context.argumentIndex = 0;
    }

    if (Function* function = FunctionRepo::instance()->find(context.functionName)) {
        const QString usage = function->usage();
        if (usage.isEmpty())
            return QStringLiteral("<b>%1</b>()").arg(context.functionName.toHtmlEscaped());

        return formatFunctionUsageTooltip(
            context.functionName,
            usage.split(';'),
            context.argumentIndex,
            false
        );
    }

    const auto userFunctions = evaluator->getUserFunctions();
    for (const UserFunction& userFunction : userFunctions) {
        if (userFunction.name().compare(context.functionName, Qt::CaseInsensitive) != 0)
            continue;

        return formatFunctionUsageTooltip(
            userFunction.name(),
            userFunction.arguments(),
            context.argumentIndex,
            true
        );
    }

    return QString();
}

} // namespace FunctionTooltipUtils
