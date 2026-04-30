// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef DISPLAYFORMATUTILS_H
#define DISPLAYFORMATUTILS_H

#include <QString>

namespace DisplayFormatUtils {

/*
   Display spacing rules (single source of truth)
   ----------------------------------------------
   Operator sign constants come from MathDsl.

   1) Arithmetic expression operators
      - use MathDsl::AddWrap around AdditionSign
      - use MathDsl::SubtractionSpace around SubtractionSign
      - use MathDsl::DivWrap around DivisionSign
      - use MathDsl::MulDotWrapSp around MulDotSign
      - use MathDsl::MulCrossWrapSp around MulCrossSign

      Examples:
      - 2 + 3 + 4
      - 2 − 3 − 4
      - 2 / 3 / 4
      - 2 · cos(45) · sin(90)
      - 2 × 3 × 4

   2) Number + unit pair
      - Between numeric value and unit bracket use MathDsl::QuantSp.
      - Example: 2 [kg]

   3) Composite units
      - Inside unit expressions there are no surrounding spaces around unit
        multiplication/division operators, regardless of operator sign.
      - Example: [kg·m/s²]

   4) Conversion target display
      - Preserve explicit source RHS unit brackets for display, e.g.
        `3 [m] -> [km]` displays as `3 [m] → [km]`.
 */
QString applyDigitGroupingForDisplay(const QString& input);
QString applyOperatorSpacingForDisplay(const QString& input);
QString applyValueUnitSpacingForDisplay(const QString& input);
QString preserveConversionTargetBracketsForDisplay(const QString& displayedInterpreted,
                                                   const QString& sourceExpression);

}

#endif // DISPLAYFORMATUTILS_H
