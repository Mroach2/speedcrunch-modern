User-Defined Variables and Functions
====================================


.. _variables:

Variables
---------

When working on more sophisticated problems, you will likely find that you frequently need to access
results from previous computations. As we have already seen, you can simply recall results from the
result window. However, SpeedCrunch also offers another more powerful way: Variables. Variables allow
you to store and recall any value, by assigning it a name. Variables are defined using the :samp:`{variable}={value}` syntax::

    a = 5.123

You can attach a comment to the definition by appending ``?`` and text::

    a = 5.123 ? calibration value

This comment is displayed in the User Variables dock widget and in autocomplete suggestions.

Now you can access this value via the name ``a`` much like you would use a built-in constant like :const:`pi`.

Naturally, when assigning a value, the right-hand-side can be an arbitrarily complex expression::

    mass = 100+20
    = 120

    g = 9.81
    = 9.81

    weight = mass*g
    = 1177.2

    somethingelse = ln(sqrt(123) + ans)
    = 7.08027102937165690787

As you see, using descriptive variable names can make the calculation history much more readable.


.. _user_functions:

User Functions
--------------
.. versionadded:: 0.12

Just as you can define your own variables, it is also possible to define your own functions. While SpeedCrunch comes with an extensive collection of built-in functions (:ref:`sc:functionindex`), defining
your own functions can be very useful when you find yourself repeating a similar computation over and over again.

Defining a custom function is similar to defining a variable::

    f(x) = 5*x+8

Function definitions can also include a trailing comment::

    f(x) = 5*x+8 ? affine transform

Like variable comments, this comment is shown in the Functions dock widget and in autocomplete suggestions.

You can now use the new function ``f`` just like any of the built-in ones::

    f(5)
    = 33

Functions with more arguments are possible as well; simply separate the parameters with a semicolon::

    f(x; y) = x*y + x

    f(2; 3)
    = 8


.. _complex_numbers:

Complex numbers
===============
.. versionadded:: 0.12

SpeedCrunch supports calculations involving complex numbers. To use them, select :menuselection:`Settings --> Results --> Complex Numbers --> Cartesian` or :menuselection:`Polar`. The imaginary unit can be entered as either ``i`` or ``j``::

    j^2
    = -1

    (5+3j)/(8-2j)
    = 0.5+0.5j

A note on the syntax of complex numbers: ``5j`` denotes the number ``5*j`` while ``j5`` is a variable named 'j5'. If necessary, consider writing the multiplication explicitly, i.e. ``j*5``.
The symbol used in displayed results can be selected via :menuselection:`Settings --> Results --> Complex Numbers --> Imaginary Unit 'i'` or :menuselection:`Imaginary Unit 'j'`.

Not every function in SpeedCrunch supports complex arguments. Refer to a function's documentation for more information.

Caution is advised when using functions like :func:`cbrt` or any fractional power operation with complex numbers.
With complex number support enabled, the power operation ``x^(1/3)`` will return the first complex cubic root of ``x`` which is usually non-real.
However, when given a real argument, :func:`cbrt` will *always* return the real cubic root, regardless of whether or not complex numbers are enabled.
In *real mode*, ``x^y`` with negative real ``x`` is only defined when ``y`` can be reduced to a rational with an odd denominator;
otherwise the result is ``NaN``.

When complex numbers are disabled, the imaginary-unit constants :const:`i` and :const:`j` are not available. However, previously stored variables may still contain complex values.
In that case, the imaginary part of these numbers is discarded when passing them as an argument to a built-in function.


.. _user_units:

User Units
----------

You can define your own unit identifiers using square brackets on the left-hand side::

    [two_cubic_metres] = 2[m^3]
    [ten_metres] = 10[m]
    [cm_s] = [cm/s]

User unit definitions can also include a trailing comment::

    [cm_s] = [cm/s] ? centimeters per second

Like user variables and functions, comments are shown in the corresponding dock widget and in autocomplete suggestions.


.. _units:

Units
=====
.. versionadded:: 0.12

SpeedCrunch includes a powerful system for units and unit conversions. It provides an extensive list of built-in units and easily allows you to define your own.

Units are attached to the term on their left using square brackets::

    5[foot]
    = 1.524 [meter]

If the left-hand side is parenthesized, the bracket applies to the whole parenthesized expression::

    (5+6)[lightyear]
    = 11 [lightyear]

By default SpeedCrunch converts the quantity into SI units::

    60[mile/hour]
    = 26.8224 [meter⋅second⁻¹]

This alone would not be terribly useful. However, it is possible to convert the value to a different unit using the conversion operator ``->``
(``in`` can be used as an alias)::

    50[yard] + 2[foot] in [centi meter]
    = 4632.96 [centi⋅meter]

    10[knot] -> [kilo meter / hour]
    = 18.52 [kilo⋅meter / hour]

Displayed value-unit formatting uses a narrow no-break space (U+202F) between
the numeric value and the unit block, for example ``1.23 [meter]``. Input
accepts unit attachment with or without that separator (for example both
``1[meter]`` and ``1 [meter]``).

In the result display, final value-with-unit result lines are shown without
unit brackets (for example ``1.23 m``) to improve readability. Interpreted
and simplified expression lines keep bracketed units (for example ``→ [meter]``)
so conversion targets remain explicit. When a result is inserted back into the
editor (for example by double-clicking), SpeedCrunch uses canonical bracketed
unit syntax again.

Note that all built-in unit names are singular and use American English spelling. This is independent of the language selected for SpeedCrunch's interface.

As seen in the example above, you can use any SI prefix like ``kilo`` or ``centi``.
They are treated like any other unit, so separate them with a space from the base unit they refer to inside brackets.
For astronomical distances, ``parsec`` also supports positive SI-prefixed short forms such as ``kpc`` and ``Mpc``.

Since units are now explicit in brackets, short identifiers such as ``a``, ``mg`` and ``l`` are free to use as variable names without conflicting with units.

Information units (bit/byte)
----------------------------

For the information dimension, SpeedCrunch supports both ``bit`` (short form ``b``)
and ``byte`` (short form ``B``).

Positive SI prefixes are accepted for both families (for example ``kB``, ``MB``,
``kb``, ``Mb``), while negative SI prefixes are rejected.

When adding/subtracting information quantities, SpeedCrunch does not implicitly mix
bit-family and byte-family values. Use an explicit conversion if you want to switch
family::

    1[B] + 8[b]
    = error

    1[B] + (8[b] -> [B])
    = 2 [B]

For sums inside the same family, the displayed result keeps the coarsest unit used
in the expression::

    2[MB] + 3[PB] + 4[TB]
    = 3.004000002 [PB]

.. warning::

   In SpeedCrunch (unlike in textbook notation), prefixes can be used on their own (for example ``[kilo]``). Their use follows the same rules of precedence as any other mathematical operation.
   For instance, if you intend to express the unit 'newtons per centimeter', do not type ``[newton / centi meter]``. Make the order explicit with ``[newton / (centi meter)]``.

An important feature of SpeedCrunch's unit system is *dimensional checking*. Simply put, it prevents comparing apples and pears: if you try to convert ``[second]`` to ``[meter]``, SpeedCrunch will complain, stating that the dimensions do not match. Indeed, the dimension of ``second`` is *time*, while ``meter`` denotes a *length*, thus they cannot be compared, added, etc. When adding, multiplying, or otherwise manipulating units, SpeedCrunch will track the dimension and raise an error if it detects an invalid operation. For instance, if you type ``[meter^2]``, the result will be a quantity with the dimension *length*\ :sup:`2` which can only be compared to other quantities with the same dimension. Currently, the available dimensions and their associated primitive units are:

* *Length*: ``meter``
* *Mass*: ``kilogram``
* *Time*: ``second``
* *Electric current*: ``ampere``
* *Amount*: ``mole``
* *Luminous intensity*: ``candela``
* *Temperature*: ``kelvin``
* *Information*: ``bit``

Temperature conversions also support the affine scales ``degree_celsius`` (short form
``°C``) and ``degree_fahrenheit`` (short form ``°F``). The input aliases ``ºC``,
``˚C``, ``ºF`` and ``˚F`` are accepted and normalized to ``°C`` and ``°F``::

    77 [°F] -> [°C]
    = 25 [°C]

    25 [degree_celsius] -> [K]
    = 298.15 [K]

    298.15 [K] -> [degree_celsius]
    = 25 [degree_celsius]

    77 [degree_fahrenheit] -> [degree_celsius]
    = 25 [degree_celsius]

Defining a custom unit uses bracketed unit assignment::

    [earth_radius] = 6730[kilo meter]

    3.5[astronomical_unit] in [earth_radius]
    = 77799.78416790490341753343 [earth_radius]

Any variable or expression can be used as the right-hand side of a conversion expression::

    10[meter] in (1[yard] + 2[foot])
    = 6.56167979002624671916 (1[yard] + 2[foot])

Although full built-in unit names are always accepted, many units also support short
forms (for example ``m``, ``s``, ``B``, ``b``). If you frequently use a particular
set of units, consider defining aliases with non-reserved names::

    [metre_alias] = [metre]
    [centimetre_alias] = [centimetre]
    [foot_alias] = [foot]

Built-in short forms include:

* Length/astronomy: ``au`` (``astronomical_unit``), ``ly`` (``lightyear``),
  ``ls`` (``lightsecond``), ``lmin`` (``lightminute``), ``pc`` (``parsec``),
  ``in`` (``inch``), ``ft`` (``foot``).
* Time: ``min`` (``minute``), ``h`` (``hour``), ``cy`` (``century``).
* Angle: ``deg`` (``degree``), ``grad``/``gon`` (``gradian``).
* Information: ``b`` (``bit``), ``B`` (``byte``).
* Other common aliases: ``u``/``Da`` (``atomic_mass_unit``), ``nmi`` (``nautical_mile``).

For units that allow prefixes, the same short forms can be used in prefixed
form as well (for example ``kpc``, ``Mpc``, ``MB``, ``kb``).
Common examples include ``mm``, ``cm``, ``km``, ``mg``, ``kg``, ``mV``,
``kW``, ``MeV``, ``dL`` and ``dl``.

SI-accepted mass unit ``tonne`` (short form ``t``) is available and equals
``1000[kilogram]``::

    1[tonne] -> [kilogram]
    = 1000 [kilogram]

    1000[kilogram] -> [t]
    = 1 [t]

Some of the built-in functions are able to handle arguments with a dimension. Refer to the documentation of a particular function for more information.
