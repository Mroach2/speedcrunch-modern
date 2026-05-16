Units and Canonicalization
==========================

SpeedCrunch supports unit-aware arithmetic and conversions. Results are
dimensionally consistent and then formatted with readability-oriented
canonicalization rules.


Display Policy
--------------

The formatter prefers meaningful derived units over full SI base expansion when
that improves readability. For example:

* ``[kg*m*s^(-2)]`` displays as ``[N]``
* ``[kg*m^2*s^(-2)]`` displays as ``[J]``
* ``[kg*m^(-1)*s^(-2)]`` displays as ``[Pa]``

When a result is already expressed in a useful derived form, it is preserved:

* ``12[J]`` stays ``12[J]`` (not ``12[N⋅m]``)
* ``2[J] + 3[J]`` displays as ``5[J]``
* ``2[W*h]`` stays in ``W⋅h`` context instead of being expanded unnecessarily
* ``1[Btu]`` stays ``1[Btu]`` instead of being converted to joules
* ``1[in]`` stays ``1[in]`` instead of being converted to metres

SpeedCrunch also avoids forcing a semantic choice when dimensions are
equivalent but context differs. A common example is ``N⋅m``: it can represent
torque, while ``J`` typically represents energy. Without an explicit conversion
request, SpeedCrunch preserves authored intent rather than silently replacing one
with the other.

When negative exponents appear together with positive ones, SpeedCrunch may
render denominator form for readability:

* ``C⁴⋅m⁴⋅J⁻³`` may display as ``C⁴⋅m⁴ / J³``

Unit Notation
-------------

In :menuselection:`Settings --> Results`, SpeedCrunch provides a
``Unit Notation`` submenu with two display styles:

* ``Exponential (m·s⁻¹)`` (default): keeps products with signed exponents,
  such as ``kg⋅m²⋅s⁻³``.
* ``Fractional (m/s)``: moves negative exponents to the denominator when there
  is at least one positive exponent, such as ``kg⋅m² / s³``.

This is a display preference only; numeric values and dimensions are unchanged.


Authored Units
--------------

SpeedCrunch preserves authored units that carry practical or domain meaning.
This includes accepted metric units and many non-SI units, such as ``tonne``,
``hectare``, ``inch``, ``foot``, ``Btu``, ``eV``, ``Eh``, ``bar``, ``atm``,
``psi``, ``Torr``, ``mmHg``, ``hp``, ``kWh``, ``bit``, and ``byte``.

For example:

* ``[tonne]`` displays as ``1 tonne``
* ``[ha]`` displays as ``1 ha``
* ``[Eh]`` displays as ``1 Eh``
* ``[Btu]`` displays as ``1 Btu``

This preservation affects display only. Explicit conversion still converts to
the requested target:

* ``[tonne] -> [kg]``
* ``[Btu] -> [J]``
* ``[in] -> [cm]``

Time units are handled specially because they also participate in
sexagesimal/time display. Use an explicit conversion target when you want a
particular time unit display.

When adding or subtracting compatible units, SpeedCrunch may choose a more
readable display unit for the result instead of blindly keeping the first
operand's unit. For example, a tiny tonne value added to kilogram and gram
values can display in kilograms:

* ``0.00000001[t] + 3.2[kg] + 2342.7[g]`` displays as
  ``5.54271 kilogram``

Larger mass results can still remain in tonnes when that is the more readable
display. Explicit conversion remains the way to force a target unit.

Time sums use the same readability principle for explicit time units. Results
of at least one hour display in hours; results below one hour but at least one
minute display in minutes; smaller results display in seconds. Sexagesimal
expressions keep their dedicated sexagesimal display behavior.

For units that support SI prefixes, SpeedCrunch also uses a best-fit
engineering prefix when the unprefixed unit would make the displayed number
unnecessarily large or small. The chosen prefix aims to keep the numeric part
in a readable range, typically from ``1`` up to but not including ``1000``:

* ``1[ps]`` displays as ``1 ps``
* ``0.0001[s] + 0.89999[ms] + 1[ns] + 1[ps]`` displays as
  ``999.991001 µs``
* ``0.0001[s] + 0.899999[ms] + 1[ns] + 1[ps]`` displays as
  ``1.000000001 ms``
* ``0.0001[m] + 0.899999[mm] + 1[nm] + 1[pm]`` displays as
  ``1.000000001 mm``
* ``0.1[m]`` displays as ``100 mm``, not ``1 dm``
* ``0.01[m]`` displays as ``10 mm``, not ``1 cm``
* ``0.1[g]`` displays as ``100 mg``
* ``0.1[L]`` displays as ``100 mL``
* ``0.1[s]`` displays as ``100 ms``
* ``0.1[Pa]`` displays as ``100 mPa``

This prefix selection is not limited to time units. It applies to supported
SI-prefixable units across dimensions, while still respecting special display
rules such as hours/minutes/seconds for ordinary time sums and explicit
conversion targets requested with ``->`` or ``in``. The small SI prefixes
``d``, ``c``, ``da`` and ``h`` are accepted for input where supported, but
SpeedCrunch does not choose them automatically for result display.

In scientific notation mode, SpeedCrunch keeps the power of ten in the numeric
part and displays a single SI-prefixable unit with its unprefixed base symbol:

* ``0.0000000001234234235234[m]`` displays as
  ``1.234234235234 × 10⁻¹⁰ m``, not ``1.234234235234 × 10² pm``

Scientific notation displayed with a multiplication sign and superscript
exponent can be reused in unit expressions. For example,
``1 × 10⁻¹²[s]`` is interpreted like ``1e-12[s]`` and displays as ``1 ps``.


Composed Units
--------------

For products and quotients, SpeedCrunch keeps authored composite structure where
possible, unless a clear canonical derived target is recognized.

Examples that canonicalize:

* ``V*A -> W``
* ``F*V -> C``
* ``V*s -> Wb``
* ``T*m^2 -> Wb``
* ``Pa*m^2 -> N``
* ``J/s -> W``

Examples that preserve composite intent:

* ``J*Pa`` remains a composed derived expression, because there is no single
  broadly expected named SI derived unit for this product and preserving the
  authored structure is more readable than base expansion.
* ``N*W`` remains a composed derived expression for the same reason: no common
  canonical named unit is generally expected by users for this product.
* ``W*h`` remains in ``W⋅h`` form because it is a common domain unit for energy
  usage; preserving ``h`` keeps the practical meaning users typically intend.
* ``km/h`` displays as ``kph`` and ``mi/h`` displays as
  ``mph`` instead of being expanded to ``m⋅s⁻¹``. Speed units are
  usually chosen for domain readability, so SpeedCrunch preserves common
  authored speed forms when they match a supported named unit.
* ``nmi/h`` displays as ``knot`` for the same reason.

Preserved display forms are still normal units. Use explicit conversion when
you want another expression:

* ``[km/h] -> [m/s]``
* ``[mph] -> [km/h]``


Conversions
-----------

Use explicit conversion to request a specific target unit expression:

* ``[N*m] -> [J]``
* ``[J/s] -> [W]``
* ``[V] -> [J/C]``
* ``10[m] in [cm]``
* ``10[m] -- [cm]``

SpeedCrunch supports three equivalent conversion operators:
``->``, ``in`` (keyword alias), and ``--`` (shortcut alias).

If no explicit conversion target is requested, SpeedCrunch applies the
canonicalization/display policy above.

Built-in Units Table
--------------------

.. include:: units_table.rst
