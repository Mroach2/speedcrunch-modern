// SPDX-FileCopyrightText: 2015, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_OPCODE_H
#define CORE_OPCODE_H

#include<QString>


class Opcode
{
public:
    enum  Type { Nop, Load, Ref, Function, List, Add, Sub, Neg, BNot, Mul, Div, Pow,
           Fact, Modulo, IntDiv, LSh, RSh, BAnd, BOr, Conv, Percent,
           UnitRef, Unit };

    Type type;
    unsigned index;

    // TODO: this is only needed for Conv Op. Maybe refactor this to a smarter place?
    // TODO: only keep a pointer to the string
    QString text;

    Opcode() : type(Nop), index(0) {}
    Opcode(Type t) : type(t), index(0) {}
    Opcode(Type t, QString txt) : type(t), index(0), text(txt) {}
    Opcode(Type t, unsigned i): type(t), index(i) {}
};

#endif // CORE_OPCODE_H
