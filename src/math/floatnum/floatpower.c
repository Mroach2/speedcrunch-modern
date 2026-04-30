// SPDX-FileCopyrightText: 2007-2008 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "floatpower.h"
#include "floatipower.h"
#include "floatcommon.h"
#include "floatlog.h"
#include "floatexp.h"

char
_raise(
  floatnum x,
  cfloatnum exponent,
  int digits)
{
  int iexp;
  int extra;

  extra = float_getexponent(exponent);
  if (digits + extra > maxdigits)
    extra = maxdigits - digits;
  if (float_isinteger(exponent))
  {
    iexp = leadingdigits(exponent, float_getexponent(exponent) + 1);
    if (float_iszero(exponent) || iexp !=  0)
      return _raisei(x, iexp, digits+extra);
  }
  if (digits + extra > MATHPRECISION)
    extra = MATHPRECISION - digits;
  _ln(x, digits+extra);
  if (!float_mul(x, x, exponent, digits+extra))
    return 0;
  return _exp(x, digits);
}
