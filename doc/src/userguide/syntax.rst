Syntax
======

This part of the documentation explains the syntax of valid SpeedCrunch input. As you will see, SpeedCrunch honors most conventions for mathematical expressions. You will find using SpeedCrunch to be very natural and intuitive, especially so if you are already familiar with a programming language.


Number Notation
---------------

Decimal Form
++++++++++++

When you would like to specify a non-integer value, simply enter the number as you would write it on paper,
with either a period (``.``) or a comma (``,``) as the decimal separator.
Accepted decimal separator and grouping conventions depend on the selected
:ref:`Number Format <radix_character>`.

Trailing zeros after the decimal point (like in ``12.300``) or leading zeros before it (``0012.3``) are redundant and can be included or omitted to the user's preference. Expressions like ``.5`` as a shorthand notation for ``0.5`` are also permitted.

.. _digit_grouping_separators:

Digit Grouping Separators
+++++++++++++++++++++++++

To improve readability, SpeedCrunch accepts grouping separators inside number literals. Grouping separators are optional and ignored during evaluation.

Allowed grouping separators are characters that are **not** letters or digits and are **not** number operators. In practice, this includes many punctuation and symbol characters, for example:

* ``_`` (underscore): ``12_345_678``
* Space (U+0020): ``12 345 678``
* Currency symbols: ``$12,345``, ``€12 345``, ``12¥345``

Not allowed as grouping separators:

* Letters from any language (for example ``天``, ``é``, ``Ж``)
* Digits from any language
* Radix characters (``.`` or ``,`` depending on settings) when they are used as decimal separators
* Operator characters and other reserved syntax tokens

Some characters cannot be used for digit grouping because they already have an assigned meaning in expressions:

* ``#`` starts hexadecimal number notation (for example ``#FF``).
* ``!`` is the factorial operator (for example ``5!``).
* ``:`` is used in sexagesimal time/angle notation (for example ``12:34:56``).
* ``°`` is used in sexagesimal angle notation (for example ``12°34'56``).
* ``′`` is used in sexagesimal angle notation (for example ``12°34′56″``).
* ``&`` is the bitwise AND operator (for example ``6 & 3``).
* ``?`` starts a comment (for example ``1+2 ? note``).

Examples:

* ``12$345.678$9`` evaluates as ``12345.6789``
* ``12天2`` is rejected (it is **not** treated as ``122``)


.. _scientific_notation:

Scientific Notation
+++++++++++++++++++

When dealing with very small or very large numbers (think the size of an atom or of a galaxy) the notation above is inconvenient. These are more commonly expressed in scientific notation; for instance, 1.234*10\ :sup:`-9` is preferable to 0.000000001234.

Naturally, in SpeedCrunch this could be written as ``1.234*10^-9``, but there's also a shorthand notation: ``1.234e-9``. Here, the ``e`` represents ``*10^``, but it is considered a part of the number literal and treated with higher precedence. For example, ``1e2^3`` is equivalent to ``(1e2)^3 = 100^3``. The scale of a number (sometimes called its exponent) always begins with the scale character ``E`` or ``e`` followed by a signed integer. So ``e+10``, ``e-4``, ``E-0`` are all valid scale expressions. If the sign is '+', you may simply omit it: ``e0``, ``E10``. The significand (i.e. the part preceding the exponent) is required; exactly one exponent must be specified.

Equivalent examples::

    98e3
    98*10^3
    98x10³

Compared to most calculators, SpeedCrunch can accept very large numbers without overflowing (e.g. both ``1e+536870911`` and ``1e-536870911`` are still valid). However, only about 78 significant digits are stored at any point. Any digits beyond that are lost.

Non-Decimal Bases
+++++++++++++++++

In addition to decimal (base-10) numbers, SpeedCrunch provides support for binary (base-2), octal (base-8) and hexadecimal (base-16) numbers.
You can enter a number in any of these bases by marking it with the corresponding prefix:

* ``0b`` or ``0B`` for **binary**, e.g. ``0b10010``.
* ``0o`` or ``0O`` for **octal**, e.g. ``0o1412``.
* ``0d`` or ``0D`` for **decimal**. These can be omitted since decimal is the default base.
* ``0x``, ``0X``, or ``#`` for **hexadecimal**. The additional six digits are represented by the upper or lower case letters ``a`` to ``f``, e.g. ``0xdeadbeef`` or ``0xDEADBEEF``.

You may even enter fractional values in any of these bases. Note that scientific notation is not
supported for non-decimal bases, however. Examples::

    0b1.01
    = 1.25

    0xf.a
    = 15.625

To have SpeedCrunch output its results in a base other than decimal, you may use one of the functions :func:`bin`, :func:`oct`, :func:`dec`, or :func:`hex`::

    hex(12341)
    = 0x3035

The effect of these functions only applies to the immediate result and doesn't carry to future
operations::

    0x2 * hex(12341)
    = 24682

The same applies to :func:`sci` and :func:`eng`: they only affect the formatted display of the immediate value, not the numeric value used in later arithmetic. In particular, operand order does not matter::

    1 + sci(123456.789)
    = 123457.789

    sci(123456.789) + 1
    = 123457.789

For explicit decimal exponent notations, use :func:`sci` (scientific notation) or :func:`eng` (engineering notation)::

    sci(12341)
    = 1.2341e4

    eng(12341)
    = 12.341e3

To force the exponent used by :func:`eng`, pass an optional second argument. It accepts either a direct exponent that is a multiple of 3, or a matching power of ten (including SI prefixes)::

    eng(0.000123456; [milli])
    = 0.123456e-3

    eng(0.000123456; -4)
    = 1.23456e-4

For assembly-style fixed-width formatting, use :func:`binpad`, :func:`octpad`, or :func:`hexpad`.
These functions only accept real, dimensionless integer arguments and also only affect the immediate result::

    hexpad(15)
    = 0x0F

    binpad(1536; 32)
    = 0b00000000000000000000011000000000

To change the base that is used for displaying results, select one of the corresponding settings in :menuselection:`Settings --> Results --> Notation`.

SpeedCrunch stores integers with a precision of up to 256 bits. Since this would be unwieldy,
the binary representation of a negative number in SpeedCrunch is *not* its two's complement.
Instead, like with other bases, the value and the sign are represented separately::

    bin(-1)
    = -0b1

See :func:`mask` and :func:`unmask` to convert a negative number into the two's complement form.

Any integer larger than the 256-bit limit will be silently converted into a floating point number, making it susceptible to rounding errors.
To specify large integers, using the shift operators (``1 << n``) is preferable to exponentiation (``2 ^ n``) as the latter are floating point
calculations and thus susceptible to rounding errors.


.. _sexagesimal_values:

Sexagesimal Values
++++++++++++++++++

    .. versionadded:: 1.0

Sexagesimal values in SpeedCrunch are angle degrees or time values represented with minutes and seconds.

When sexagesimal mode is selected in :menuselection:`Settings --> Results --> Notation`, dimensionless and time results are displayed as sexagesimal values. All other results are displayed as fixed-point decimal values. Actual sexagesimal math depends on the result. Dimensionless results are handled as degrees with minutes and seconds generated from the decimal part. With time dimension results, base unit is second and the integer part is divided to minutes and hours.

In input, characters ``°`` (degree), ``:`` (colon), ``′`` (prime) and ``″`` (double prime) can be used for entering sexagesimal values. Degree sign ``°`` separates degrees and minutes. First colon character ``:`` separates hours and minutes. Prime ``′`` or second colon character ``:`` separates minutes and seconds. Additionally, postfix double prime ``″`` can be used as an arc second unit. ASCII single quote ``'`` and double quote ``"`` are accepted as aliases and are normalized to ``′`` and ``″`` on editor insertion/paste. Because the degree sign is difficult to produce from keyboard, at sign ``@`` is automatically converted to it.

Amount of minutes or seconds is not limited to values below 60. It is possible to input time 90 minutes after noon::

    12:90
    = 13:30:00
    
Dimensionless input values are automatically considered to be in current angle mode. For example, in radian mode::

    pi
    = 180°00'00

For trigonometric input, explicit angle units override angle mode. For example,
``cos(pi*[rad])``, ``cos(180*[deg])``, ``cos(200*[gon])`` and
``cos(0.5*[turn])`` all evaluate to ``-1`` regardless of the global angle mode.
The aliases ``deg`` for ``degree``, ``gon`` for ``gradian``, and ``rev`` for
``revolution`` are supported. ``revolution`` is equivalent to ``turn`` (that is,
``2*pi`` radians).
``deg`` is normalized to ``°`` in autocomplete insertion and displayed result
units.
    
Only last part of sexagesimal input value can contain decimals.

Following tables show some possible input notations and their results in both fixed-point decimal and sexagesimal modes.
Sexagesimal round-trips (for example arcsecond → DMS → radians) are designed to keep precision drift negligible.
In the first table, fixed-point decimal values assume angle mode is set to degrees:

.. list-table::
   :header-rows: 1

   * - Input
     - Fixed-Point Decimal
     - Sexagesimal
   * - ``0``
     - ``0``
     - ``0°00'00``
   * - ``°'56``
     - ``0.01555556``
     - ``0°00'56.00``
   * - ``56"``
     - ``0.01555556``
     - ``0°00'56.00``
   * - ``56[arcsec]``
     - ``0.01555556``
     - ``0°00'56.00``
   * - ``56.78"``
     - ``0.01577222``
     - ``0°00'56.78``
   * - ``°34``
     - ``0.56666667``
     - ``0°34'00.00``
   * - ``34'``
     - ``0.56666667``
     - ``0°34'00.00``
   * - ``34[arcmin]``
     - ``0.56666667``
     - ``0°34'00.00``
   * - ``34'56``
     - ``0.58222222``
     - ``0°34'56.00``
   * - ``12°``
     - ``12.00000000``
     - ``12°00'00.00``
   * - ``12°34``
     - ``12.56666667``
     - ``12°34'00.00``
   * - ``12°34.5``
     - ``12.57500000``
     - ``12°34'30.00``
   * - ``12°34'56``
     - ``12.58222222``
     - ``12°34'56.00``
   * - ``12°34'56.78``
     - ``12.58243889``
     - ``12°34'56.78``

Use canonical symbols ``′`` and ``″`` (insertable with ``'`` and ``"``),
or the aliases ``arcmin`` and ``arcsec``. Full names ``arcminute`` and
``arcsecond`` are also accepted.

.. list-table::
   :header-rows: 1

   * - Input
     - Fixed-Point Decimal
     - Sexagesimal
   * - ``0[s]``
     - ``0 [s]``
     - ``0:00:00``
   * - ``::56``
     - ``56.00 [s]``
     - ``0:00:56.00``
   * - ``56[s]``
     - ``56.00 [s]``
     - ``0:00:56.00``
   * - ``:34``
     - ``2040.00 [s]``
     - ``0:34:00.00``
   * - ``34[min]``
     - ``2040.00 [s]``
     - ``0:34:00.00``
   * - ``12:``
     - ``43200.00 [s]``
     - ``12:00:00.00``
   * - ``12[h]``
     - ``43200.00 [s]``
     - ``12:00:00.00``
   * - ``12:34``
     - ``45240.00 [s]``
     - ``12:34:00.00``
   * - ``12:34.5``
     - ``45270.00 [s]``
     - ``12:34:30.00``
   * - ``12:34:56``
     - ``45296.00 [s]``
     - ``12:34:56.00``
   * - ``12:34:56.78``
     - ``45296.78 [s]``
     - ``12:34:56.78``

Note that when entering time values with colons, no additional dimension units are needed. Formatting itself works as an unit.

Comments
--------

The question mark character ``?`` starts a comment. Everything from ``?`` to the end of the line is ignored by the evaluator::

    1 + 2 ? simple sum
    = 3

A line can also be comment-only. If the first non-space character is ``?``, the whole line is treated as a comment::

    ? start algorithm

      ? this is also a comment-only line

Operators and Precedence
------------------------

When writing an expression like ``10+5*4``, which operation will be executed first? The common rules of operator precedence tell us that in this case multipication shall be computed first, hence the result is ``30``. We also distinguish **unary** operators (which act on a single number/operand) and **binary** operators (which link two operands).

SpeedCrunch supports the following operators, listed in order of decreasing precedence:

.. Note: When making changes to these tables, also check that they look ok with LaTeX; these big
.. tables can be problematic.

.. tabularcolumns:: |p{0.2\linewidth}|p{0.5\linewidth}|p{0.25\linewidth}|

+-------------------------------+---------------------------------------------------------------+-------------------------+
| Operator                      | Description                                                   | Examples                |
+===============================+===============================================================+=========================+
| ``(...)``                     | **Parentheses**                                               | ``(2+3)*4 = 5*4 = 20``  |
|                               |   Parentheses mark precedence                                 |                         |
|                               |   explicitly.                                                 |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+
| ``x!``                        | **Factorial**                                                 | ``5! = 120``            |
|                               |   Computes the factorial of its                               |                         |
|                               |   argument. See also :func:`gamma()`.                         |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+
| ``x%``                        | **Percent**                                                   | ``50% = 0.5``           |
|                               |   Postfix operator equivalent to                              | ``1000+12% = 1120``     |
|                               |   division by 100.                                            | ``1000-12% = 880``      |
|                               |   In ``a+b%`` and ``a-b%``, the percentage                    | ``1000*12% = 120``      |
|                               |   is applied relative to the                                  | ``1000/25% = 4000``     |
|                               |   entire left operand ``a`` (after normal precedence).        | ``1+2+3+10% = 6.6``     |
|                               |   So ``1+(2+3)+10%`` is interpreted as                        | ``1+(2+3)+10% = 6.6``   |
|                               |   ``(1+(2+3))+10%``.                                          |                         |
|                               |                                                               |                         |
|                               | .. versionadded:: 1.0                                         |                         |
|                               |    Contextual percent semantics.                              |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+
| ``a ^ b``, ``a ** b``, ``a²`` | **Exponentiation**                                            |                         |
|                               |   ``a²`` is shorthand for ``a^2``;                            | ``3²⁰ = 3^20``          |
|                               |   ``**`` is equivalent to ``^`` and can be                    | ``2**10 = 1024``        |
|                               |   used as an easy way to insert powers.                       |                         |
|                               |   contiguous superscript digits are                           | ``2¹⁰ = 1024``          |
|                               |   parsed as one integer exponent. Both                        |                         |
|                               |   function notations ``f^n(x)`` and ``fⁿ(x)``                 | ``cos^2(pi)=cos²(pi)``  |
|                               |   are interpreted identically.                                |                         |
|                               |   text-operator variants are equivalent. Note                 |                         |
|                               |   that the power operation is                                 |                         |
|                               |   *right-associative*, i.e. it is                             | ``2^2^3 = 2^8 = 256``   |
|                               |   evaluated from right to left.                               |                         |
|                               |   In *real mode*, fractional powers of                        |                         |
|                               |   negative numbers are only defined for                       | ``(-1)^(2/3) = 1``      |
|                               |   rational exponents with odd denominators.                   |                         |
|                               |   The result is negative only when the reduced                | ``(-8)^(2/3) = 4``      |
|                               |   rational exponent has an odd numerator.                     |                         |
|                               |   Positive and negative powers can be written                 | ``10^3``, ``10^-3``,    |
|                               |   with either ``^`` or ``**``.                                | ``10**3``, ``10**-3``   |
|                               |                                                               |                         |
|                               | .. versionadded:: 1.0                                         |                         |
|                               |    Integer superscript powers and ``f^n(x)`` function-power   |                         |
|                               |    notation (equivalent to ``fⁿ(x)``).                        |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+
| ``+x``, ``-x``, ``~x``        | **Unary plus, minus, and bitwise NOT**                        | ``~5 = -6``             |
|                               |   ``~x`` is equivalent to :func:`not(x)`.                     | ``-~(-1) = -not(-1)``   |
|                               |                                                               |                         |
|                               | .. versionadded:: 1.0                                         |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+
| ``a \ b``                     | **Integer division**                                          | ``5\4 = 1``             |
|                               |   Divides the operands and truncates                          |                         |
|                               |   the result to an integer.                                   |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+
| ``a * b``, ``a × b``, ``a b``,| **Multiplication and division**                               | ``3 sqrt(2)``           |
| ``a / b``, ``a ⧸ b``          |                                                               |                         |
|                               |   In many situations, *implicit                               |                         |
|                               |   multiplication* allows writing                              |                         |
|                               |   multiplications without the ``*``                           |                         |
|                               |   operator.                                                   |                         |
|                               |   In the expression editor and Session Import,                |                         |
|                               |   these symbols are normalized to ``×``:                      |                         |
|                               |   ``∗``, ``·``, ``⋅``, ``∙``, ``*``,                          |                         |
|                               |   ``⨉``, ``⨯``, ``✕``, ``✖``.                                 |                         |
|                               |   In the expression editor and Session Import,                |                         |
|                               |   these symbols are normalized to ``⧸``:                      |                         |
|                               |   ``/``, ``÷``.                                               |                         |
|                               |   Implicit multiplication has                                 | ``6/2(2+1)=9``          |
|                               |   the same precedence as ``*`` and ``/``,                     |                         |
|                               |   evaluated from left to right.                               |                         |
|                               |   The result display makes this association explicit by       |                         |
|                               |   showing the interpreted expression before the numeric       |                         |
|                               |   result.                                                     |                         |
|                               |                                                               |                         |
|                               | .. versionadded:: 0.12                                        |                         |
|                               |    Implicit multiplication was added                          |                         |
|                               |    SpeedCrunch 0.12.                                          |                         |
|                               |                                                               |                         |
|                               | .. versionadded:: 1.0                                         |                         |
|                               |    Multiplication and division symbol aliases.                |                         |
|                               |    Improved explicit interpretation/association in results.   |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+
| ``a + b``, ``a - b``          | **Addition and subtraction**                                  |                         |
|                               |   In the expression editor and Session Import,                |                         |
|                               |   ``＋`` is normalized to ``+``.                              |                         |
|                               |   In the expression editor and Session Import,                |                         |
|                               |   these symbols are normalized to ``−``:                      |                         |
|                               |   ``-``, ``－``, ``﹣``, ``‐``, ``‑``,                        |                         |
|                               |   ``–``, ``—``, ``―``, ``⁃``.                                 |                         |
|                               |                                                               |                         |
|                               | .. versionadded:: 1.0                                         |                         |
|                               |    Addition and subtraction symbol aliases.                   |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+
| ``a << n``, ``a >> n``        | **Left/right arithmetic shifts**                              | ``0b11<<1 = 0b110``     |
|                               |   Shifts the first operand left/right                         |                         |
|                               |   by ``n`` bits. See also :func:`shl`                         | ``0b100>>2 = 0b1``      |
|                               |   and :func:`shr`.                                            |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+
| ``a & b``                     | **Bitwise AND**                                               | ``0b11 & 0b10 = 0b10``  |
|                               |   See also :func:`and`.                                       |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+
| ``a | b``                     | **Bitwise OR**                                                | ``0b10 | 0b01 = 0b11``  |
|                               |   See also :func:`or`.                                        |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+
| ``->``, ``in``, ``--``        | **Unit conversion**                                           | ``10[m] in [mi]``       |
|                               |   Convert the operand into the given                          |                         |
|                               |   unit. Both forms are equivalent. See                        | ``10[m] -> [mi]``       |
|                               |   ``--`` is a shortcut equivalent to ``->`` and ``in``.       | ``10[m] -- [mi]``       |
|                               |   :ref:`units` for more information.                          |                         |
+-------------------------------+---------------------------------------------------------------+-------------------------+

For negative bases in *real mode*, the exponentiation rule can be read as:

1. Express the exponent as a reduced rational ``p/q``.
2. If ``q`` is even, the result is ``NaN``.
3. If ``q`` is odd, the result is real, with sign set by ``p``:
   odd ``p`` gives a negative result, even ``p`` gives a positive result.

Examples::

    (-13)^(1/3) = -2.3513346877207574895
    (-13)^(2/3) = 5.5287748136788717828
    (-13)^(1/9.123) = 1.32465488702830785593
    (-13)^(1/2) = NaN
    (-13)^pi = NaN
