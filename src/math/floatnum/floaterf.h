// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FLOATERF_H
#define FLOATERF_H

#include "math/floatnum/floatseries.h"

#ifdef __cplusplus
extern "C" {
#endif

#define erfnear0 erfseries
#define erfcbigx erfcasymptotic

char _erf(floatnum x, int digits);
char _erfc(floatnum x, int digits);

#ifdef __cplusplus
}
#endif

#endif /* FLOATERF_H */
