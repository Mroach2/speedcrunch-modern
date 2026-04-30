// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_FUNCTIONTOOLTIPUTILS_H
#define GUI_FUNCTIONTOOLTIPUTILS_H

#include <QString>

class Evaluator;

namespace FunctionTooltipUtils {

QString activeFunctionUsageTooltip(const Evaluator* evaluator,
                                   const QString& expression,
                                   int cursorPosition);

}

#endif
