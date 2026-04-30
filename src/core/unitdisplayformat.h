// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef UNITDISPLAYFORMAT_H
#define UNITDISPLAYFORMAT_H

#include <QString>

namespace UnitDisplayFormat {

QString shortDisplayName(const QString& name);
QString normalizeUnitTextForDisplay(const QString& text);

}

#endif // UNITDISPLAYFORMAT_H
