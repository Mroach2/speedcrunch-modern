// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_COMPLEXFORM_H
#define CORE_COMPLEXFORM_H

#include <QString>

namespace ComplexForm {

inline constexpr char Rectangular = 'r';
inline constexpr char Exponential = 'e';
inline constexpr char Trigonometric = 't';
inline constexpr char Cis = 'c';
inline constexpr char Phasor = 'p';
inline constexpr char Default = Rectangular;

inline bool isValid(char form)
{
    return form == Rectangular
        || form == Exponential
        || form == Trigonometric
        || form == Cis
        || form == Phasor;
}

inline char fromString(const QString& form, char fallback = Default)
{
    if (form.size() != 1)
        return fallback;

    const char value = form.at(0).toLatin1();
    return isValid(value) ? value : fallback;
}

inline QString toString(char form)
{
    return QString(QChar(isValid(form) ? form : Default));
}

}

#endif // CORE_COMPLEXFORM_H
