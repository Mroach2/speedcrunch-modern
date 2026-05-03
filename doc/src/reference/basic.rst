Basic Math Functions
====================

General
-------

.. function:: abs(x)

    Return the absolute value of ``x``, commonly written as \|x\|. In real mode, it flips the sign of a negative number, thus forcing it to be positive. When given a complex number as argument, it returns the modulus of the number.

    The argument can have a dimension.

    Example::

        abs(-3 meter)
        = 3 meter

        abs(4 + 3j)
        = 5

.. function:: sqrt(x)

    Return the square root of ``x``. If complex numbers are disabled, this function is only defined for *x > 0*. In *complex mode*, any complex number may be specified, yielding the complex root in the upper half plane.

    Alias: ``√(x)``.

    .. versionadded:: 1.0

    The argument may have a dimension.

.. function:: cbrt(x)

    Compute the third (cubic) root of ``x``. In *real mode*, it accepts any real number. Negative numbers will yield a negative cubic root::

        cbrt(-27)
        = -3

    In *complex mode*, this function accepts any complex input. The result will generally be the first complex root, i.e. the one with a phase between 0 and π/3. Real negative arguments however will still yield a real (negative) result, matching the function's behavior in *real mode*. Use ``x^(1/3)`` to get the first complex root.

    Alias: ``∛(x)``.

    .. versionadded:: 1.0

.. function:: exp(x)

    Compute the natural exponential function.

    The argument must be dimensionless.

    .. seealso::
       | :func:`ln` (natural logarithm)

.. function:: ln(x)

    Compute the natural logarithm. In *real mode*, the argument must be real, with *x > 0*.

    In *complex mode*, any non-zero number may be given. The result will be the principal value. The branch cut runs across the negative real axis. Nevertheless, in SpeedCrunch :func:`ln` is defined for negative real numbers as *ln(-x) = ln(\|x\|)) + πj*, extending the branch from the *upper* half-plane.

.. function:: lb(x)

    Compute the binary logarithm. In *complex mode* the same rules apply as for :func:`ln`.

.. function:: lg(x)

    Compute the decimal logarithm. In *complex mode* the same rules apply as for :func:`ln`.

.. function:: log(n; x)

    Compute the logarithm of base ``n`` . In *complex mode* the same rules apply as for :func:`ln`.

.. function:: datetime(unix_timestamp [; offset])

    .. versionadded:: 1.0

    Convert a Unix timestamp (seconds since 1970-01-01 00:00:00 UTC) into a
    numeric date/time value formatted as ``YYYYMMDD.HHMMSS``.

    If ``offset`` is omitted, the timestamp is converted using the local system
    timezone. If ``offset`` is provided, it is interpreted as hours offset to
    GMT/UTC (fractional offsets are supported), and conversion is performed in
    UTC after applying that offset.

    This function is the inverse of :func:`epoch` when using the same ``offset``.

    Example::

        datetime(1514761200; 1)
        = 20180101.000000

        datetime(1551464695; -3.5)
        = 20190301.145455

.. function:: epoch(yyyymmdd.hhmmss [; offset])

    .. versionadded:: 1.0

    Convert a numeric date/time value in ``YYYYMMDD.HHMMSS`` format to a Unix
    timestamp (seconds since 1970-01-01 00:00:00 UTC).

    If ``offset`` is omitted, the value is interpreted in the local system
    timezone. If ``offset`` is provided, it is interpreted as hours offset to
    GMT/UTC (fractional offsets are supported).

    This function is the inverse of :func:`datetime` when using the same ``offset``.

    Example::

        epoch(20180101.000000; 1)
        = 1514761200

        epoch(20190301.145455; -3.5)
        = 1551464695

.. function:: molmass(formula)

    Compute the molar mass of a chemical formula and return the result with
    dimension ``g/mol``.

    The ``formula`` argument is case-sensitive and supports either plain digits
    (for example ``C6H12O6``) or subscript digits (for example ``C₆H₁₂O₆``).
    Parsing fails for invalid formulas.

    Advanced chemical notation is not supported yet. For example, grouped or
    hydrated formulas such as ``K4[Fe(CN)6]·3H2O`` currently fail to parse.

    The element table follows the official IUPAC CIAAW 2021 abridged standard
    atomic weights.

    Example::

        molmass(C6H12O6)
        = 180.156 g/mol

.. function:: mass(mol; formula)

    Compute the mass of a substance amount and return the result with dimension
    ``g``.

    This function reuses :func:`molmass` internally and is equivalent to:
    ``mol * molmass(formula)``.

    The ``formula`` argument follows the same parsing rules as :func:`molmass`.

    Example::

        mass(1; C6H12O6)
        = 180.156 g

.. function:: molarity(n; V)

    Compute molarity as amount of substance divided by solution volume and
    return the result with dimension ``mol/L``.

    This function is equivalent to: ``n / V``.

    If ``n`` or ``V`` is dimensionless, SpeedCrunch interprets them as ``mol``
    and ``L``, respectively.

    Example::

        molarity(0.50; 1.00)
        = 0.5 mol/L

Lists and Matrices
------------------

.. versionadded:: 1.0

Lists are written with braces and semicolon-separated elements. Matrices are
written as a list of row lists. Only one-dimensional lists and two-dimensional
matrices are supported::

    mylist = {1; 2; 3; 4; 5}
    mat = {{1; 2; 3}; {4; 5; 6}}

The function-style notation ``list(...)`` is also accepted on input, for
example ``list(1; 2; 3)`` is equivalent to ``{1; 2; 3}``.

List and matrix results use the same spacing, for example ``{1; 2; 3}`` and
``{{1; 2}; {3; 4}}``.

List and matrix elements must be dimensionless. Units cannot be attached to
individual elements or to a list or matrix as a whole.

Supported arithmetic operations are elementwise addition and subtraction for
same-shaped lists or matrices, scalar multiplication and division, and matrix
multiplication for compatible matrices::

    {1; 2; 3} + {4; 5; 6}
    = {5; 7; 9}

    2 * {{1; 2}; {3; 4}}
    = {{2; 4}; {6; 8}}

    {{1; 2}; {3; 4}} * {{5; 6}; {7; 8}}
    = {{19; 22}; {43; 50}}

The aggregation and statistics functions :func:`count`, :func:`sum`,
:func:`min`, :func:`max`, :func:`average`, :func:`mean`, :func:`median`,
:func:`varp`, :func:`vars`, :func:`stdevp`, and :func:`stdevs` accept a list or
matrix as a single argument. Matrix inputs are flattened for these scalar
statistics. The ``p`` variants use population normalization (``n``); the ``s``
variants use sample normalization (``n-1``).

:func:`covp`, :func:`covs`, :func:`corrp`, and :func:`corrs` treat matrices
column-wise: rows are observations and columns are variables.

.. function:: dot(list1; list2)

    .. versionadded:: 1.0

    Return the dot product of two equal-length lists.

.. function:: cross(list1; list2)

    .. versionadded:: 1.0

    Return the cross product of two three-element lists.

.. function:: norm(list-or-matrix)

    .. versionadded:: 1.0

    Return the Euclidean norm of a list, or the Frobenius norm of a matrix.

.. function:: transpose(matrix)

    .. versionadded:: 1.0

    Return the transposed matrix.

.. function:: det(matrix)

    .. versionadded:: 1.0

    Return the determinant of a square matrix.

.. function:: inv(matrix)

    .. versionadded:: 1.0

    Return the inverse of a square matrix.

.. function:: trace(matrix)

    .. versionadded:: 1.0

    Return the sum of the diagonal of a square matrix.

.. function:: rank(matrix)

    .. versionadded:: 1.0

    Return the matrix rank.

.. function:: covp(matrix)

    .. versionadded:: 1.0

    Return the population covariance matrix. Rows are observations and columns
    are variables. This uses population normalization (``n``).

.. function:: covs(matrix)

    .. versionadded:: 1.0

    Return the sample covariance matrix. Rows are observations and columns are
    variables. This uses sample normalization (``n-1``).

.. function:: corrp(matrix)

    .. versionadded:: 1.0

    Return the population Pearson correlation matrix. Rows are observations and
    columns are variables. This is derived from :func:`covp`.

.. function:: corrs(matrix)

    .. versionadded:: 1.0

    Return the sample Pearson correlation matrix. Rows are observations and
    columns are variables. This is derived from :func:`covs`.

.. function:: flatten(matrix)

    .. versionadded:: 1.0

    Return all matrix elements as a single list in row-major order.

.. function:: rows(matrix)

    .. versionadded:: 1.0

    Return the row count.

.. function:: cols(matrix)

    .. versionadded:: 1.0

    Return the column count.

.. function:: shape(list-or-matrix)

    .. versionadded:: 1.0

    Return ``{n}`` for a list and ``{rows; cols}`` for a matrix.


.. _trigonometric:

Trigonometric & Inverse Trigonometric
-------------------------------------

For direct trigonometric input (:func:`sin`, :func:`cos`, :func:`tan`,
:func:`cot`, :func:`sec`, :func:`csc`), explicit angle units
(``rad``, ``degree``, ``gradian``/``grad``/``gon``, ``turn``, ``arcminute``,
``arcsecond``) override the global angle mode. Unitless values follow the
current angle mode.

.. function:: sin(x)

    Returns the sine of ``x``. The behavior depends on both the angle mode setting and on whether complex numbers are enabled.

    In *degrees*, *gradians*, or *turns* modes, the argument is assumed to be expressed in such that :func:`sin` is periodic with a period of 360 degrees, 400 gradians, or 1 turn, respectively: *sin(x) = sin(x+360)*, *sin(x) = sin(x+400)*, or *sin(x) = sin(x+1)*. Complex arguments are allowed only in *radians* mode, regardless of the corresponding setting.

    When *radians* are set as the angle mode, :func:`sin` will be 2π-periodic. The argument may be complex.

    For real arguments beyond approx. \|x\|>10\ :sup:`77`, SpeedCrunch no longer recognizes the periodicity of the function and issues an error.

    The argument of :func:`sin` must be dimensionless.

    The inverse function is :func:`arcsin`.

    .. seealso::
        | :func:`cos`
        | :func:`tan`
        | :func:`cot`
        | :func:`arcsin`

.. function:: cos(x)

    Returns the cosine of ``x``. The behavior depends on both the angle mode setting and on whether complex numbers are enabled.

    In *degrees*, *gradians*, or *turns* modes, the argument is assumed to be expressed in such that :func:`cos` is periodic with a period of 360 degrees, 400 gradians, or 1 turn, respectively: *cos(x) = cos(x+360)*, *cos(x) = cos(x+400)*, or *cos(x) = cos(x+1)*. Complex arguments are allowed only in *radians* mode, regardless of the corresponding setting.

    When *radians* are set as the angle mode, :func:`cos` will be 2π-periodic. The argument may be complex.

    For real arguments beyond approx. \|x\|>10\ :sup:`77`, SpeedCrunch no longer recognizes the periodicity of the function and issues an error.

    The argument of :func:`cos` must be dimensionless.

    The inverse function is :func:`arccos`.

    .. seealso::
        | :func:`sin`
        | :func:`tan`
        | :func:`cot`
        | :func:`sec`
        | :func:`arccos`

.. function:: tan(x)

    Returns the tangent of ``x``. The behavior depends on both the angle mode setting and on whether complex numbers are enabled.

    In *degrees*, *gradians*, or *turns* modes, the argument is assumed to be expressed in such that :func:`tan` is periodic with a period of 360 degrees, 400 gradians, or 1 turn, respectively: *tan(x) = tan(x+360)*, *tan(x) = tan(x+400)*, or *tan(x) = tan(x+1)*. Complex arguments are allowed only in *radians* mode, regardless of the corresponding setting.

    When *radians* are set as the angle mode, :func:`tan` will be π-periodic. The argument may be complex.

    The argument of :func:`tan` must be dimensionless.

    The inverse function is :func:`arctan`.

    .. seealso::
        | :func:`cos`
        | :func:`sin`
        | :func:`cot`

.. function:: cot(x)

    Returns the cotangent of ``x``. The behavior depends on both the angle mode setting and on whether complex numbers are enabled.

    In *degrees*, *gradians*, or *turns* modes, the argument is assumed to be expressed in such that :func:`cot` is periodic with a period of 360 degrees, 400 gradians, or 1 turn, respectively: *cot(x) = cot(x+360)*, *cot(x) = cot(x+400)*, or *cot(x) = cot(x+1)*. Complex arguments are allowed only in *radians* mode, regardless of the corresponding setting.

    When *radians* are set as the angle mode, :func:`cot` will be π-periodic. The argument may be complex.

    The argument of :func:`cot` must be dimensionless.

    .. seealso::
        | :func:`cos`
        | :func:`sin`
        | :func:`tan`

.. function:: sec(x)

    Returns the secant of ``x``, defined as the reciprocal cosine of ``x``: *sec(x) = 1/cos(x)*. The behavior depends on both the angle mode setting and on whether complex numbers are enabled.

    In *degrees*, *gradians*, or *turns* modes, the argument is assumed to be expressed in such that :func:`sec` is periodic with a period of 360 degrees, 400 gradians, or 1 turn, respectively: *sec(x) = sec(x+360)*, *sec(x) = sec(x+400)*, or *sec(x) = sec(x+1)*. Complex arguments are allowed only in *radians* mode, regardless of the corresponding setting.

    When *radians* are set as the angle mode, :func:`sec` will be 2π-periodic. The argument may be complex.

    For real arguments beyond approx. \|x\|>10\ :sup:`77`, SpeedCrunch no longer recognizes the periodicity of the function and issues an error.

    The argument of :func:`sec` must be dimensionless.

.. function:: csc(x)

    Returns the cosecant of ``x``, defined as the reciprocal sine of ``x``: *csc(x) = 1/sin(x)*. The behavior depends on both the angle mode setting and on whether complex numbers are enabled.

    In *degrees*, *gradians*, or *turns* modes, the argument is assumed to be expressed in such that :func:`csc` is periodic with a period of 360 degrees, 400 gradians, or 1 turn, respectively: *csc(x) = csc(x+360)*, *csc(x) = csc(x+400)*, or *csc(x) = csc(x+1)*. Complex arguments are allowed only in *radians* mode, regardless of the corresponding setting.

    When *radians* are set as the angle mode, :func:`csc` will be 2π-periodic. The argument may be complex.

    For real arguments beyond approx. \|x\|>10\ :sup:`77`, SpeedCrunch no longer recognizes the periodicity of the function and issues an error.

    The argument of :func:`csc` must be dimensionless.


.. function:: arccos(x)

    Returns the inverse cosine of ``x``, such that *cos(arccos(x)) = x*. The behavior of the function depends on both the angle mode setting and on whether complex numbers are enabled.

    In *degrees*, *gradians*, or *turns* modes, :func:`arccos` takes a real argument from *[-1, 1]*, and the return value is in the range *[0, 180]*, *[0, 200]*, or *[0, 0.5]*, respectively. Real arguments outside *[-1, 1]* and complex numbers are allowed only in *radians* mode.

    When *radians* are set as the angle mode, :func:`arccos` maps an element from *[-1, 1]* to a value in *[0, π]*. When complex numbers are enabled in addition, :func:`arccos` may take any argument from the complex plane. In complex mode, *arccos(-1) = π* and *arccos(1) = 0* will yield the same result as in real mode.

    The argument of :func:`arccos` must be dimensionless.

    The inverse function is :func:`cos`.

.. function:: arcsin(x)

    Returns the inverse sine of ``x``, such that *sin(arcsin(x)) = x*. The behavior of the function depends on both the angle mode setting and on whether complex numbers are enabled.

    In *degrees*, *gradians*, or *turns* modes, :func:`arcsin` takes a real argument from *[-1, 1]*, and the return value is in the range *[-90, 90]*, *[-100, 100]*, or *[-0.25, 0.25]*, respectively. Real arguments outside *[-1, 1]* and complex numbers are allowed only in *radians* mode.

    When *radians* are set as the angle mode, :func:`arcsin` maps an element from *[-1, 1]* to a value in *[-π/2, π/2]*. When complex numbers are enabled in addition, :func:`arcsin` may take any argument from the complex plane. In complex mode, *arcsin(-1) = π/2* and *arcsin(1) = π/2* will yield the same result as in real mode.

    The argument of :func:`arccos` must be dimensionless.

    The inverse function is :func:`sin`.


.. function:: arctan(x)

    Returns the inverse tangent of ``x``, such that *tan(arctan(x)) = x*. The behavior of the function depends on both the angle mode setting and on whether complex numbers are enabled.

    In *degrees*, *gradians*, or *turns* modes, :func:`arctan` takes a real argument from *[-1, 1]*, and the return value is in the range *[-90, 90]*, *[-100, 100]*, or *[-0.25, 0.25]*, respectively. Real arguments outside *[-1, 1]* and complex numbers are not allowed only in *radians* mode.

    When *radians* are set as the angle mode, :func:`arctan` maps a real number to a value in *[-π/2, π/2]*. When complex numbers are enabled in addition, :func:`arctan` may take any argument from the complex plane, except for *+j* and *-j*.

    The argument of :func:`arctan` must be dimensionless.

    The inverse function is :func:`tan`.

.. function:: arctan2(x, y)

    Returns the angle formed by the vector *(x, y)* and the X axis. If the point *(x, y)* lies in the first quadrant (i.e. both *x > 0* and *y > 0* are true), it is given by *arctan(y/x)*. However, the function handles vectors in other quadrants as well.

    The behavior of the function depends on the angle mode setting. In *degrees*, *gradians*, or *turns* modes, this function returns a value in the range *]-180, 180]*, *]-200, 200]*, or *]-0.5, 0.5]*, respectively. When *radians* are set as the angle mode, the return value lies in the range *]-π, π]*.

    Unlike :func:`arctan` this function only accepts real arguments.

    The argument values must be dimensionless.


Hyperbolic & Inverse Hyperbolic
-------------------------------

.. function:: sinh(x)

    Return the hyperbolic sine of ``x``. In *complex mode*, any complex number may be used as the argument.

    The argument must be dimensionless.

    The inverse function is :func:`arsinh`.


.. function:: cosh(x)

    Return the hyperbolic cosine of ``x``. In *complex mode*, any complex number may be used as the argument.

    The argument must be dimensionless.

    The inverse function is :func:`arcosh`.


.. function:: tanh(x)

    Return the hyperbolic tangent of ``x``. In *complex mode*, any complex number may be used as the argument.

    The argument must be dimensionless.

    The inverse function is :func:`artanh`.


.. function:: arsinh(x)

    Compute the area hyperbolic sine of ``x``, the inverse function to :func:`sinh`. *arsinh(x)* is the only solution to *cosh(y) = x*.

    In complex mode, the function is defined for any complex ``z`` as *arsinh(z) = ln[z + (z* :sup:`2` *+1)* :sup:`1/2` *]*.

    The function only accepts dimensionless arguments.


.. function:: arcosh(x)

    Compute the area hyperbolic cosine of ``x``, the inverse function to :func:`cosh`. *arcosh(x)* is the positive solution to *cosh(y) = x*. Except for *x=1*, the second solution to this equation will be given by *-arcosh(x)*.

    In real mode, the parameter ``x`` must be *> 1*. In complex mode, the function is defined for any complex ``z`` as *arcosh(z) = ln[z + (z* :sup:`2` *-1)* :sup:`2` *]*.

    The function only accepts dimensionless arguments.


.. function:: artanh(x)

    Compute the area hyperbolic tangent of ``x``, the inverse function to :func:`tanh`. *artanh(x)* is the only solution to *tanh(y) = x*. In real mode, the parameter ``x`` has to fulfill *-1 < x < 1*.

    In complex mode, this function accepts any argument except for -1 and +1. In the complex plane, it is defined as *artanh(z) = 1/2 \* ln[(z+1)/(z-1)]*.

    The function only accepts dimensionless arguments.


Special
-------

.. function:: erf(x)

    Compute the error function, evaluated in ``x``. The error function is closely related to the Gaussian cumulative density function.

    Note that currently only real arguments are allowed. Furthermore, the function only accepts dimensionless arguments.

.. function:: erfc(x)

    Compute the complementary error function, evaluated in ``x``. The complementary error function is defined by ``erfc(x) = 1 - erf(x)``.

    Note that currently only real arguments are allowed. Furthermore, the function only accepts dimensionless arguments.

.. function:: gamma(x)

    Evaluates the gamma function (frequently denoted by the Greek letter Γ). The gamma function is an analytic extension to the factorial operation which is defined on real numbers as well. The relation between factorial and the gamma function is given by *Γ(n) = (n - 1)!*.

    Note that currently only real arguments are allowed. Furthermore, the function only accepts dimensionless arguments.

    The computation of the factorial operation is in fact implemented via :func:`gamma`. This means that in SpeedCrunch, factorials of non-integer numbers are allowed.

.. function:: lngamma(x)

    Computes ``ln(abs(gamma(x)))``. As the gamma function grows extremely quickly, it is sometimes easier to work with its logarithm instead. :func:`lngamma` allows much larger arguments that would otherwise overflow :func:`gamma`.

    Note that currently only real arguments are allowed. Furthermore, the function only accepts dimensionless arguments.


Complex Numbers
---------------

.. function:: real(x)

    Return the real part of a complex number ``x``.

    The argument may have a dimension.

.. function:: imag(x)

    Return the imaginary part of a complex number ``x``.

    The argument may have a dimension.

.. function:: conj(x)

    .. versionadded:: 1.0

    Return the complex conjugate of a complex number ``x``.

    In *complex mode*, this function accepts any real or complex input. In *real mode*, the imaginary portion of ``x`` is stripped to ensure that the value returned is purely real.

.. function:: phase(x)

    Returns the phase (angle) of a complex number ``x``. The unit of the angle corresponds to the current angle mode.

    The argument may have a dimension.

    .. seealso::
       | :func:`abs` (absolute value)

.. function:: polar(x)

    Converts the complex number ``x`` to polar form.

    In the ``Polar (Exponential)`` complex-number display mode, values are
    shown as *r e* :sup:`uɸ`, where ``u`` is the selected imaginary-unit
    symbol (``i`` or ``j``), and ɸ is in radians.

    In the ``Polar (Angle)`` complex-number display mode, values are shown as
    *r ∠ ɸ*, and ɸ follows the global angle-unit setting.

.. function:: cart(x)

    Converts the complex number ``x`` to cartesian form, i.e. the form
    *a + b u*, where ``u`` is the currently selected imaginary-unit
    symbol (``i`` or ``j``).


Various
-------

.. function:: sgn(x)

    For *x >= 0*, return +1. For *x < 0*, return -1.

.. function:: radians(x)

    Convert the angle ``x`` into radians. Independently of the angle mode setting, this function will assume that ``x`` is given in degrees and return ``pi*x/180``.

    The function accepts real arguments that are either dimensionless (interpreted
    as degrees) or explicitly tagged with an angle unit in ``[]`` (for example
    ``[rad]``, ``[degree]``, ``[gradian]``, ``[turn]``, ``[arcminute]``,
    ``[arcsecond]``).

    With explicit unit blocks, the equivalent conversion is ``x[deg] -> [rad]``.

.. function:: degrees(x)

    Convert the angle ``x`` into degrees. Independently of the angle mode setting, this function will assume that ``x`` is given in radians and return ``180*x/pi``.

    The function accepts real arguments that are either dimensionless (interpreted
    as radians) or explicitly tagged with an angle unit in ``[]``.

    With explicit unit blocks, the equivalent conversion is ``x[rad] -> [deg]``.

.. function:: gradians(x)

    .. versionadded:: 1.0

    Convert the angle ``x`` into gradians. Independently of the angle mode setting, this function will assume that ``x`` is given in radians and return ``200*x/pi``.

    The function accepts real arguments that are either dimensionless (interpreted
    as radians) or explicitly tagged with an angle unit in ``[]``.

    With explicit unit blocks, the equivalent conversion is ``x[rad] -> [grad]``.

.. function:: turns(x)

    .. versionadded:: 1.0

    Convert the angle ``x`` into turns. Independently of the angle mode setting, this function will assume that ``x`` is given in radians and return ``x/(2*pi)``.

    The function accepts real arguments that are either dimensionless (interpreted
    as radians) or explicitly tagged with an angle unit in ``[]``.

    With explicit unit blocks, the equivalent conversion is ``x[rad] -> [turn]``.

.. function:: int(x)

    Returns the integer part of ``x``, effectively rounding it towards zero.

    The function only accepts real, dimensionless arguments.

.. function:: frac(x)

    Returns the fractional (non-integer) part of ``x``, given by ``frac(x) = x - int(x)``.

    The function only accepts real, dimensionless arguments.
