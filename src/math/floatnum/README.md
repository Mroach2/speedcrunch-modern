# Floatnum Module

This entire module was originally written by Wolf Lammen (`ookami1 <at> gmx <dot> de`) from 2007-2009.
Only occasional maintenance and extension changes have been performed by other authors since then.

## Purpose

`floatnum` is SpeedCrunch's decimal floating-point backend for high-precision and numerically stable
evaluation. The code in this directory provides:

- core number representation and primitive operations
- conversion and I/O helpers for decimal formats
- series/infrastructure helpers reused by multiple functions
- elementary and higher math functions implemented on top of the same engine

## File Roles

### Core and shared infrastructure

- `floatconfig.h`: compile-time configuration limits, constants, and feature toggles.
- `floatlong.h`, `floatlong.c`: fixed-width helper utilities used by low-level routines.
- `floatio.h`, `floatio.c`: digit-sequence and textual I/O support.
- `floatnum.h`, `floatnum.c`: central decimal floating-point type and primitive arithmetic/state handling.
- `floatconst.h`, `floatconst.c`: reusable mathematical constants and coefficient tables.
- `floatcommon.h`, `floatcommon.c`: common utility routines shared by most math operations.
- `floatconvert.h`, `floatconvert.c`: radix/format conversion routines between numeric forms.

### Series and building-block algorithms

- `floatseries.h`, `floatseries.c`: reusable series expansions used by transcendental functions.
- `floatipower.h`, `floatipower.c`: integer-power helpers.

### Function families

- `floatexp.h`, `floatexp.c`: exponential and related functions.
- `floatlog.h`, `floatlog.c`: logarithmic functions.
- `floattrig.h`, `floattrig.c`: trigonometric functions.
- `floatpower.h`, `floatpower.c`: generic power functions built from lower-level components.
- `floatgamma.h`, `floatgamma.c`: Gamma-related computations.
- `floaterf.h`, `floaterf.c`: error function and normal-distribution related integrals.
- `floatlogic.h`, `floatlogic.c`: logical/integer-like operations for `floatnum` values.
- `floathmath.h`, `floathmath.c`: high-level facade that combines multiple families.
- `floatincgamma.h`, `floatincgamma.c`: incomplete-gamma-related operations built on higher-level math.

## Practical Structure

- Base layer: `floatconfig`, `floatio`, `floatlong`, `floatnum`
- Shared math substrate: `floatconst`, `floatcommon`, `floatconvert`, `floatseries`, `floatipower`
- Specialized families: `floatexp`, `floatlog`, `floattrig`, `floatpower`, `floatgamma`, `floaterf`, `floatlogic`
- Orchestration/top-level composition: `floathmath`, then `floatincgamma`

## Tests

This module has a dedicated test target, `testfloatnum`, focused on validating the `floatnum`
engine and related math routines.

- Test source: [`src/tests/testfloatnum.c`](../tests/testfloatnum.c)
