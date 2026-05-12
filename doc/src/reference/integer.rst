Integer & Bitwise Arithmetic Functions
======================================

Bitwise Operations
------------------

.. function:: and(x1; x2; ...)

    Performs a bitwise logical AND on the submitted parameters (at least two). All parameters have to be real integers from the range -2\ :sup:`255` to +2\ :sup:`255`-1 (signed or unsigned 256 bit integers), non integer arguments are rounded toward zero. The result ranges from -2\ :sup:`255` to +2\ :sup:`255`-1 (signed integer).

.. function:: or(x1; x2; ...)

    Performs a bitwise logical OR on the submitted parameters (at least two). All parameters have to be integers from the range -2\ :sup:`255` to +2\ :sup:`255`-1 (signed integer), non integer arguments are rounded toward zero.

.. function:: xor(x1; x2; ...)

    Performs a bitwise logical XOR on the submitted parameters (at least two). All parameters have to be integers from the range -2\ :sup:`255` to +2\ :sup:`255`-1  (signed integer), non integer arguments are rounded toward zero.

.. function:: popcount(n)

    .. versionadded:: 1.0

    Returns the number of set bits (``1`` bits) in the 256-bit two's complement
    representation of ``n``.

    The argument must be real and dimensionless, in the logic range used by
    the bitwise functions. Non-integer arguments are rounded toward zero.

.. function:: not(n)

    The :func:`not` function is defined by ``not(x) = -x-1``, giving the same result as the one's complement operator ``~`` in C/C++.

    .. warning:: This function does *not* simply flip the bits!

    The unary prefix operator ``~`` is equivalent to :func:`not`::

        ~5
        = -6
        not(5)
        = -6

        -~(-1)
        = 0
        -not(-1)
        = 0

    .. versionadded:: 1.0

.. function:: shl(x; n)

    Performs an arithmetic left shift.

    :param x: The number (bit pattern) to shift, -2\ :sup:`255` <= ``x`` <= +2\ :sup:`256`-1.
    :param n: Number of bits to shift, -255 <= ``n`` <= 255. Must be integer.

    Note that ``n`` < 0 results in a right shift. The result ranges from -2\ :sup:`255` to +2\ :sup:`255`-1 (signed integer). ``x`` is rounded toward zero before shifting.  If ``n`` = 0, ``x`` is returned without rounding.

    Shifted out bits are always dropped. During a right shift, the most significant bit (bit 255) is copied. During a left shift, zero bits are shifted in.


.. function:: shr(x; n)

    Performs an arithmetic right shift.

    :param x: The number (bit pattern) to shift, -2\ :sup:`255` <= ``x`` <= +2\ :sup:`256`-1.
    :param n: Number of bits to shift, -255 <= ``n`` <= 255. Must be integer.

    Note that ``n`` < 0 results in a left shift. The result ranges from -2\ :sup:`255` to +2\ :sup:`255`-1 (signed integer). ``x`` is rounded toward zero before shifting.  If ``n`` = 0, ``x`` is returned without rounding.

    Shifted out bits are always dropped. During a right shift, the most significant bit (bit 255) is copied. During a left shift, zero bits are shifted in.

.. function:: mask(x; n)

    Returns the lowest ``n`` bits from ``x``. For this, ``x`` must be in the range -2\ :sup:`255` <= ``x`` <= +2\ :sup:`256`-1, and ``n`` must be an integer, 1 <= ``n`` <= 255. ``x`` is rounded toward zero.

    The result is always unsigned.

    Example: Getting the two's complement of -1 in a 16-bit system::

        hex(mask(-1; 16))
        = 0xFFFF

.. function:: unmask(x; n)

    Takes the lower ``n`` bits from ``x`` and sign-extends them to full 256 bits. This means that bit at position *n-1* is copied to all upper bits.

    The value of ``x`` must be in the range *-2* :sup:`255` *<= x <= +2* :sup:`256` *-1*, and ``n`` must be an integer, *1 <= n <= 255*. ``x`` is rounded toward zero.

    Example: Converting a number in two's complement representation to a signed number::

        unmask(0xFFFF; 16)
        = -1
        unmask(0x1FFF; 16)
        = 0x1FFF



Numeral Bases
-------------

The following functions only change the format for the current result. To change the base
that is used for displaying results, select one of the corresponding settings in :menuselection:`Settings --> Results --> Format`.

.. function:: bin(n)

    Format ``n`` as binary (base-2).

.. function:: oct(n)

    Format ``n`` as octal (base-8).

.. function:: dec(n)

    Format ``n`` as decimal (base-10).

.. function:: sci(n)

    Format ``n`` in scientific notation.

.. function:: eng(n [; exponent])

    Format ``n`` in engineering notation.

    If ``exponent`` is provided, force the ``e`` exponent suffix to that value. ``exponent`` can be:

    * an integer exponent that is a multiple of 3 (for example, ``-3`` or ``6``), or
    * a power of ten with exponent a multiple of 3 (for example, ``milli``, ``micro``, ``kilo``, ``mega``, ``pico``).

    ``eng(0.000123456; milli)`` is equivalent to ``eng(0.000123456; -3)``.

.. function:: rat(n)
              ratio(n)
              rational(n)

    Format ``n`` in rational form when possible. If a suitable rational form is not found,
    the result falls back to decimal automatic formatting.

.. function:: hex(n)

    Format ``n`` as hexadecimal (base-16).

.. function:: binpad(n [; bits])

    .. versionadded:: 1.0

    Format integer ``n`` as binary (base-2) and left-pad the integer part with zeros.

    If ``bits`` is omitted, the width is padded to the next multiple of 8 bits (byte boundary).
    If ``bits`` is provided, the width is padded to at least ``bits`` (it never truncates).

    Only real, dimensionless integer arguments are allowed.

.. function:: octpad(n [; bits])

    .. versionadded:: 1.0

    Format integer ``n`` as octal (base-8) and left-pad the integer part with zeros.

    If ``bits`` is omitted, the width is padded to the next multiple of 8 bits (byte boundary).
    If ``bits`` is provided, the width is padded to at least ``bits`` (it never truncates).

    Only real, dimensionless integer arguments are allowed.

.. function:: hexpad(n [; bits])

    .. versionadded:: 1.0

    Format integer ``n`` as hexadecimal (base-16) and left-pad the integer part with zeros.

    If ``bits`` is omitted, the width is padded to the next multiple of 8 bits (byte boundary).
    If ``bits`` is provided, the width is padded to at least ``bits`` (it never truncates).

    Only real, dimensionless integer arguments are allowed.


Rounding
--------

.. function:: ceil(x)

    Round ``x`` to the next largest integer. This is the expression-level
    counterpart to :menuselection:`Settings --> Results --> Rounding Mode -->
    Toward +Infinity`. Only real, dimensionless arguments are allowed.

.. function:: floor(x)

    Round ``x`` to the next smallest integer. This is the expression-level
    counterpart to :menuselection:`Settings --> Results --> Rounding Mode -->
    Toward -Infinity`. Only real, dimensionless arguments are allowed.


.. function:: round(x [; n])

    Round ``x`` to the nearest number with ``n`` fractional digits using
    half-away-from-zero rounding; ``n`` may be omitted, in which case ``x`` is
    rounded to the closest integer.

    This is the expression-level counterpart to
    :menuselection:`Settings --> Results --> Rounding Mode --> Half Away from
    Zero (Arithmetic)`. It does not depend on the global rounding mode.

    Example::

        round(0.5)
        = 1

        round(-0.5)
        = -1

        round(1.5)
        = 2

        round(12.345; 2)
        = 12.34

        round(12345; -2)
        = 12300

    ``x`` must be real. ``n`` must be a real, dimensionless integer.

.. function:: roundeven(x [; n])

    Round ``x`` to the nearest number with ``n`` fractional digits using
    half-even, also known as banker's rounding; ``n`` may be omitted, in which
    case ``x`` is rounded to the closest integer.

    Ties are rounded to the value whose last kept digit is even. This is the
    expression-level counterpart to
    :menuselection:`Settings --> Results --> Rounding Mode --> Half Even
    (Banker's)`. It does not depend on the global rounding mode.

    Example::

        roundeven(0.5)
        = 0

        roundeven(1.5)
        = 2

        roundeven(2.5)
        = 2

        roundeven(12.345; 2)
        = 12.34

    ``x`` must be real. ``n`` must be a real, dimensionless integer.

.. function:: trunc(x [; n])

    Truncate (round toward zero) ``x`` to the next number with ``n``
    fractional digits; ``n`` may be omitted, in which case ``x`` is rounded to
    integer. This is the expression-level counterpart to
    :menuselection:`Settings --> Results --> Rounding Mode --> Toward Zero
    (Truncation)`. Only real, dimensionless arguments are allowed.

    .. seealso::
       | :func:`int`
       | :func:`frac`
       | :func:`ceil`
       | :func:`floor`
       | :func:`round`
       | :func:`roundeven`


Integer Division
----------------

.. function:: idiv(a; b)

    Compute the integer part of the division ``a/b``. The result is guaranteed to be exact. While ``int(a/b)`` covers a larger range of arguments, the result is computed via floating point arithmetics and may be subject to rounding errors. This function will instead yield an error if the parameters exceed the safe bounds.

    It is possible to apply :func:`idiv` to non-integers as well, but be aware that rounding errors might be lead to off-by-one errors. If the result depends on the validity of the guard digits, *NaN* is returned.

    Only real, dimensionless arguments are allowed.

.. function:: mod(a; b)

    Compute the remainder of the integer division ``a/b``. The divisor ``b`` must be non-zero. The result takes the sign of ``a``.

    This function is paired with :func:`idiv` (truncating integer quotient), so
    ``a = idiv(a; b) * b + mod(a; b)``.

    Example: ``mod(-1; 360) = -1``.
    If you want wrap-around behavior in ``[0, 360)``, use :func:`emod`:
    ``emod(-1; 360) = 359``.

    For large powers, avoid computing the full power first (for example,
    ``mod(3^233; 353)``), since the intermediate value may lose precision.
    Use :func:`powmod` instead.

    This function always returns an exact result, provided that the parameters are exact.

    You can use this function with non-integers as well, but rounding errors might lead to off-by-one errors. Evaluating :func:`mod` can be computationally expensive, so the function is internally restricted to 250 division loops.

    Only real, dimensionless arguments are allowed.

.. function:: emod(a; b)

    .. versionadded:: 1.0

    Compute the Euclidean remainder of ``a/b``. The divisor ``b`` must be non-zero.
    The result has the sign of ``b`` (or is zero), and its absolute value is
    smaller than the absolute value of ``b``.

    For positive ``b``, this maps values to ``[0, b)``; for example,
    ``emod(-1; 360) = 359``.

    This function always returns an exact result, provided that the parameters are exact.

    You can use this function with non-integers as well, but rounding errors might lead to off-by-one errors.

    Only real, dimensionless arguments are allowed.

.. function:: powmod(base; exponent; modulo)

    .. versionadded:: 1.0

    Compute ``base^exponent`` reduced by ``modulo`` using modular exponentiation.
    This avoids building huge intermediate powers and is therefore the recommended
    approach for large exponents (for example in cryptography).

    Arguments must be real, integer, and dimensionless.
    ``modulo`` must be non-zero, and ``exponent`` must be non-negative.

    The result follows :func:`emod` semantics (same sign as ``modulo`` or zero).
    Example: ``powmod(3; 233; 353) = 248``.

.. function:: gcd(n1; n2; ...)

    Returns the greatest common divisor of the arguments (at least two must be given). You can use this function to reduce a rational number.
    If a rational number is given as ``p/q``, its reduced form is ``(p / gcd(p; q)) / (q / gcd(p; q))``.

    Only real, integer arguments are allowed.

.. function:: lcm(n1; n2; ...)

    .. versionadded:: 1.0

    Returns the least common multiple of the arguments (at least two must be given).
    The result is always non-negative.

    ``gcd()`` and ``lcm()`` are related by:

    .. math::

        lcm(n1; n2) = n1 * n2 / gcd(n1; n2)

    Only real, integer arguments are allowed.
