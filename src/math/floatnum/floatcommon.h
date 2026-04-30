// SPDX-FileCopyrightText: 2007-2009, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef FLOATCOMMON_H
# define FLOATCOMMON_H
#include "floatnum.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846  // Pi
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880  // Square root of 2
#endif

#ifndef M_LN2
#define M_LN2 0.69314718055994530942    // Natural log of 2
#endif

#ifndef M_LN10
#define M_LN10 2.30258509299404568402  // Natural logarithm of 10
#endif

#ifndef M_LOG2E
#define M_LOG2E 1.44269504088896340736  // Log base 2 of e
#endif

#ifndef M_LOG10E
#define M_LOG10E 0.43429448190325182765 // Log base 10 of e
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* helper, checks parameters. Sets float_error to NaNOperand
   or InvalidPrecision and sets <x> to NaN, if the parameters do
   not meet the requirements of routines for higher mathematical
   functions, and returns 0 in this case */
char chckmathparam(floatnum x, int digits);

/* helper, determines, how many decimal digits the exponent of <x> has.
   If the exponent is 0, -1 is returned */
int logexp(cfloatnum x);

/* helper, returns the <digits> first decimal digits and the sign of a
   significand, encoded in an integer. */
int leadingdigits(cfloatnum x, int digits);

/* convenience wrapper for float_setscientific, setting the last
   parameter to NULLTERMINATED */
void float_setasciiz(floatnum x, const char* asciiz);

/* convenience wrapper for float_add, adds a signed integer to <summand1>
   and places the result in <sum> */
char float_addi(floatnum sum, cfloatnum summand1,
               int summand2, int digits);

/* convenience wrapper for float_mul, multiplies a signed integer with
   <factor1> and places the result in <product> */
char float_muli(floatnum product, cfloatnum factor1,
               int factor2, int digits);

/* convenience wrapper for float_div, divides <dividend> by a signed integer
   and places the result in <quotient> */
char float_divi(floatnum quotient, cfloatnum dividend,
               int divisor, int digits);

/* convenience wrapper for float_cmp: compares the absolute value of
   both operands */
int float_abscmp(floatnum x, floatnum y);

/* convenience wrapper for float_div, returns 1/<x> */
char float_reciprocal(floatnum x, int digits);

/* compares two numbers in a normal fashion, but returns equal, if their
   relative difference is less than 1e-<digits>, i.e.
   |(x-y)/max(x,y)| < 1e-<digits> */
int float_relcmp(floatnum x, floatnum y, int digits);

/* returns whether x is an integer */
char float_isinteger(cfloatnum x);

/* returns the integer part of x as integer. If x exceeds the
   integer range, 0 is returned */
int float_asinteger(cfloatnum x);

/* rounds x in TONEAREST mode. If x overflows, the rounding is reverted.
   Does not report errors */
void float_checkedround(floatnum x, int digits);
void float_checkedroundmode(floatnum x, int digits, roundmode mode);

/* a fast way to multiply with a power of ten, does not set float_error
   on overflow or NaN, returns silently NaN instead*/
void float_addexp(floatnum x, int smd);

/* returns 0, if the integer part of x is even */
char float_isodd(floatnum x);

/* an extension of float_int: you can choose the round mode
   errors: FLOAT_NANOPERAND
           FLOAT_OVERFLOW (if EXP_MAX is really small) */
char float_roundtoint(floatnum x, roundmode mode);

float float_asfloat(cfloatnum x);

void float_setfloat(floatnum dest, float x);

float aprxsqrt(float x);
float aprxln(float x);
float aprxlog10(float x);
float aprxlog2(float x);
float aprxlngamma(float x);
float aprxlog10fn(cfloatnum x);

#ifdef __cplusplus
}
#endif

#endif /* FLOATCOMMON_H */
