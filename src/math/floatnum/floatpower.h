// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FLOATPOWER_H
# define FLOATPOWER_H

#include "floatnum.h"

#ifdef __cplusplus
extern "C" {
#endif

char _raise(floatnum x, cfloatnum exponent, int digits);

#ifdef __cplusplus
}
#endif

#endif /* FLOATPOWER_H */
