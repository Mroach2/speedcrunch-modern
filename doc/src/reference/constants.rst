.. _constants:

Constants
=========

SpeedCrunch provides a large collection of built-in scientific constants.
In the Constants widget, each entry has:

* a translated display ``Name``
* an optional ``Symbol``
* a numeric ``Value``
* an optional ``Unit``

Double-clicking a constant entry inserts its value into the editor
(followed by unit, when present).

Constants in the widget can be filtered by category and subcategory.


.. constant:: pi
.. constant:: π

   .. versionadded:: 1.0

   The mathematical constant π. Since π is an irrational number, the value is an
   approximation with as much precision as SpeedCrunch allows. This constant may be
   referred to either as ``pi`` or as ``π`` (Unicode U+03C0 GREEK SMALL LETTER PI).
   The following Unicode variants are also accepted and normalized to ``π``:
   ``𝜋`` (U+1D70B), ``𝝅`` (U+1D745), ``𝞹`` (U+1D7B9), ``𝛑`` (U+1D6D1).
   In displayed expressions/results, SpeedCrunch renders these forms as ``π`` and
   also displays standalone ``pi`` as ``π``.


.. constant:: e
.. constant:: ℯ

   The mathematical constant *e*. The value is an approximation with as much precision
   as SpeedCrunch supports. This constant may be referred to either as ``e`` or as
   ``ℯ`` (Unicode U+212F SCRIPT SMALL E).


.. constant:: i
.. constant:: j

   These are aliases for the imaginary unit such that ``i ^ 2 = -1`` and
   ``j ^ 2 = -1``.

   The displayed symbol used in formatted results can be selected in
   :menuselection:`Settings --> Results --> Complex Numbers` as either ``i`` (default) or ``j``.

   Using this constant, complex numbers are expressed as the sum of a real and an
   imaginary part::

       c = 4 + 2i

.. TODO: link to docs on complex number functionality.
