// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_ANGLEMODE_H
#define CORE_ANGLEMODE_H

namespace AngleMode {

inline constexpr char Radian = 'r';
inline constexpr char Degree = 'd';
inline constexpr char Gradian = 'g';
inline constexpr char Turn = 't';
inline constexpr char Revolution = 'v';
inline constexpr char Default = Radian;

inline bool isValid(char mode)
{
    return mode == Radian
        || mode == Degree
        || mode == Gradian
        || mode == Turn
        || mode == Revolution;
}

}

#endif // CORE_ANGLEMODE_H
