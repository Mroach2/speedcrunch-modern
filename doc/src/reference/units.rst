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

SpeedCrunch also avoids forcing a semantic choice when dimensions are
equivalent but context differs. A common example is ``N⋅m``: it can represent
torque, while ``J`` typically represents energy. Without an explicit conversion
request, SpeedCrunch preserves authored intent rather than silently replacing one
with the other.

When negative exponents appear together with positive ones, SpeedCrunch may
render denominator form for readability:

* ``C⁴⋅m⁴⋅J⁻³`` may display as ``C⁴⋅m⁴ / J³``

Unit Exponent Style
-------------------

In :menuselection:`Settings --> Results`, SpeedCrunch provides a
``Unit Exponent Style`` submenu with two display styles:

* ``Superscript Exponents`` (default): keeps products with signed exponents,
  such as ``kg⋅m²⋅s⁻³``.
* ``Fraction Form``: moves negative exponents to the denominator when there is
  at least one positive exponent, such as ``kg⋅m² / s³``.

This is a display preference only; numeric values and dimensions are unchanged.


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
