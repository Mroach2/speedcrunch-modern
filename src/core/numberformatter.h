// SPDX-FileCopyrightText: 2013, 2015-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_NUMBERFORMATTER_H
#define CORE_NUMBERFORMATTER_H

#include "quantity.h"

#include <QtCore/QString>

struct NumberFormatter {
    static QString format(HNumber &num) { return format(Quantity(num)); }
    static QString format(CNumber &num) { return format(Quantity(num)); }
    static QString format(Quantity);
    static QString format(Quantity, char resultFormatOverride);
    static QString format(Quantity, char resultFormatOverride, int precisionOverride,
                          bool useComplexNotation, char complexNotationOverride);
    static QString formatTrigSymbolic(Quantity);
    static QString formatNumericLiteralForDisplay(const QString& input);
    static QString rewriteScientificNotationForDisplay(const QString& input);
    static bool tryFormatStandaloneNumericLiteralForDisplay(const QString& input, QString* output);
};

#endif
