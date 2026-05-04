Theme JSON Schema
=================

SpeedCrunch theme files are JSON objects with a fixed set of color-role keys.
Each value is a color string in ``#RRGGBB`` format.

Schema
------

.. code-block:: json

   {
     "$schema": "https://json-schema.org/draft/2020-12/schema",
     "$id": "https://speedcrunch.org/schemas/theme.schema.json",
     "title": "SpeedCrunch Theme",
     "type": "object",
     "additionalProperties": false,
     "required": [
       "cursor",
       "number",
       "parens",
       "list",
       "unit",
       "result",
       "comment",
       "matched",
       "function",
       "operator",
       "variable",
       "scrollbar",
       "separator",
       "background",
       "editorbackground"
     ],
     "properties": {
       "cursor": { "$ref": "#/$defs/color" },
     "number": { "$ref": "#/$defs/color" },
     "parens": { "$ref": "#/$defs/color" },
     "list": { "$ref": "#/$defs/color" },
     "unit": { "$ref": "#/$defs/color" },
     "result": { "$ref": "#/$defs/color" },
       "comment": { "$ref": "#/$defs/color" },
       "matched": { "$ref": "#/$defs/color" },
       "function": { "$ref": "#/$defs/color" },
       "operator": { "$ref": "#/$defs/color" },
       "variable": { "$ref": "#/$defs/color" },
       "scrollbar": { "$ref": "#/$defs/color" },
       "separator": { "$ref": "#/$defs/color" },
       "background": { "$ref": "#/$defs/color" },
       "editorbackground": { "$ref": "#/$defs/color" }
     },
     "$defs": {
       "color": {
         "type": "string",
         "pattern": "^#[0-9a-fA-F]{6}$",
         "description": "Hex color in the form #RRGGBB"
       }
     }
   }

Notes
-----

* ``parens`` controls highlighting for ``()``.
* ``list`` controls highlighting for list/matrix curly braces ``{}`` only.
* ``unit`` controls highlighting for square-bracketed unit blocks, including
  both ``[]`` and everything inside them.

Fictitious Example
------------------

The following example is fictitious and provided only as a usage example:

.. code-block:: json

   {
     "cursor": "#F4C430",
     "number": "#6ED3FF",
     "parens": "#C8A2C8",
     "list": "#B39DDB",
     "unit": "#9CDCFE",
     "result": "#E8F1FF",
     "comment": "#7F8C8D",
     "matched": "#3CB371",
     "function": "#FFB347",
     "operator": "#F5F5F5",
     "variable": "#FF8DA1",
     "scrollbar": "#4B5563",
     "separator": "#2F3640",
     "background": "#111827",
     "editorbackground": "#0B1220"
   }
