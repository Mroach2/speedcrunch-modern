User-Defined Variables, Functions and Units
===========================================


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



.. _units:

Units
-----
.. versionadded:: 0.12

SpeedCrunch includes a powerful system for units and unit conversions. It provides an extensive list of built-in units and easily allows you to define your own.

Units are attached to the term on their left using square brackets::

    5 [ft]
    = 1.524 [m]

If the left-hand side is parenthesized, the bracket applies to the whole parenthesized expression::

    (5+6)[ly]
    = 11 [ly]

By default SpeedCrunch converts the quantity into SI units::

    60[mi/h]
    = 26.8224 [m⋅s⁻¹]

This alone would not be terribly useful. However, it is possible to convert the value to a different unit using the conversion operator ``->``
(``in`` can be used as an alias)::

    50[yd] + 2[ft] in [cm]
    = 4632.96 [cm]

    10[kn] -> [km/h]
    = 18.52 [km/h]

Displayed value-unit formatting uses a narrow no-break space (U+202F) between
the numeric value and the unit block, for example ``1.23 [m]``. Input
accepts unit attachment with or without that separator (for example both
``1[m]`` and ``1 [m]``).

In the result display, final value-with-unit result lines are shown without
unit brackets (for example ``1.23 m``) to improve readability. Interpreted
and simplified expression lines keep bracketed units (for example ``→ [m]``)
so conversion targets remain explicit. When a result is inserted back into the
editor (for example by double-clicking), SpeedCrunch uses canonical bracketed
unit syntax again.

Note that all built-in unit names are singular and use American English spelling. This is independent of the language selected for SpeedCrunch's interface.

As seen in the example above, you can use any SI prefixes such as ``k`` or ``c``.
They are treated like any other unit, so separate them with a space from the base unit they refer to inside brackets.
For astronomical distances, ``pc`` supports positive SI-prefixed forms such as ``kpc`` and ``Mpc``.

Since units are now explicit in brackets, short identifiers such as ``a``, ``mg`` and ``l`` are free to use as variable names without conflicting with units.

Information units (bit/byte)
----------------------------

For the information dimension, SpeedCrunch supports both ``bit`` (short form ``b``)
and ``byte`` (short form ``B``).

Prefixes
^^^^^^^^

Positive SI prefixes are accepted for both families (for example ``kB``, ``MB``,
``kb``, ``Mb``), while negative SI prefixes are rejected.

Family mixing rules
^^^^^^^^^^^^^^^^^^^

When adding/subtracting information quantities, SpeedCrunch does not implicitly mix
bit-family and byte-family values. Use an explicit conversion if you want to switch
family::

    1[B] + 8[b]
    = error

    1[B] + (8[b] -> [B])
    = 2 [B]

Result unit selection
^^^^^^^^^^^^^^^^^^^^^

For sums inside the same family, the displayed result keeps the coarsest unit used
in the expression::

    2[MB] + 3[PB] + 4[TB]
    = 3.004000002 [PB]

.. warning::

   Prefixes cannot be used on their own. Always attach them to a unit symbol.
   For instance, if you intend to express the unit 'newtons per centimeter', do not type ``[N / c m]``. Make the order explicit with ``[N / (cm)]``.

An important feature of SpeedCrunch's unit system is *dimensional checking*. Simply put, it prevents comparing apples and pears: if you try to convert ``[s]`` to ``[m]``, SpeedCrunch will complain, stating that the dimensions do not match. Indeed, the dimension of ``s`` is *time*, while ``m`` denotes a *length*, thus they cannot be compared, added, etc. When adding, multiplying, or otherwise manipulating units, SpeedCrunch will track the dimension and raise an error if it detects an invalid operation. For instance, if you type ``[m^2]``, the result will be a quantity with the dimension *length*\ :sup:`2` which can only be compared to other quantities with the same dimension. Currently, the available dimensions and their associated primitive units are:

* *Length*: ``m``
* *Mass*: ``kg``
* *Time*: ``s``
* *Electric current*: ``A``
* *Amount*: ``mol``
* *Luminous intensity*: ``cd``
* *Temperature*: ``K``
* *Information*: ``b``

Temperature conversions also support the affine scales ``degree_celsius`` (short form
``°C``) and ``degree_fahrenheit`` (short form ``°F``). The input aliases ``ºC``,
``˚C``, ``ºF`` and ``˚F`` are accepted and normalized to ``°C`` and ``°F``::

    77 [°F] -> [°C]
    = 25 [°C]

    25 [°C] -> [K]
    = 298.15 [K]

    298.15 [K] -> [°C]
    = 25 [°C]

    77 [°F] -> [°C]
    = 25 [°C]

Defining a custom unit uses bracketed unit assignment::

    [earth_radius] = 6730[km]

    3.5[au] in [earth_radius]
    = 77799.78416790490341753343 [earth_radius]

Any variable or expression can be used as the right-hand side of a conversion expression::

    10[m] in (1[yd] + 2[ft])
    = 6.56167979002624671916 (1[yd] + 2[ft])

Use unit symbols by default (for example ``m``, ``s``, ``B``, ``b``). Full
built-in names are accepted as an alternative input form.

Built-in short forms include:

* Length/astronomy: ``au`` (``astronomical_unit``), ``ly`` (``lightyear``),
  ``ls`` (``lightsecond``), ``lmin`` (``lightminute``), ``pc`` (``parsec``),
  ``in`` (``inch``), ``ft`` (``foot``).
* Time: ``min`` (``minute``), ``h`` (``hour``), ``cy`` (``century``).
* Angle: ``deg`` (``degree``), ``gon`` (``gradian``).
* Information: ``b`` (``bit``), ``B`` (``byte``).
* Other common aliases: ``u``/``Da`` (``atomic_mass_unit``), ``nmi`` (``nautical_mile``).

For units that allow prefixes, the same short forms can be used in prefixed
form as well (for example ``kpc``, ``Mpc``, ``MB``, ``kb``).
Common examples include ``mm``, ``cm``, ``km``, ``mg``, ``kg``, ``mV``,
``kW``, ``MeV``, ``dL`` and ``dl``.

SI-accepted mass unit ``t`` (full name ``tonne``) is available and equals
``1000[kg]``::

    1[t] -> [kg]
    = 1000 [kg]

    1000[kg] -> [t]
    = 1 [t]

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


.. _complex_numbers:

Complex numbers
---------------
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



Some of the built-in functions are able to handle arguments with a dimension. Refer to the documentation of a particular function for more information.
