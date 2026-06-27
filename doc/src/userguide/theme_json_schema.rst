Theme JSON Schema
=================

SpeedCrunch theme files are JSON objects with supported color-role keys.
Each color-role value is a color string in ``#RRGGBB`` format.
Theme files also include required schema metadata and may include optional
descriptive and format metadata.

Schema
------

.. code-block:: json

   {
     "$schema": "https://json-schema.org/draft/2020-12/schema",
     "$id": "https://speedcrunch.org/schemas/theme-v1.schema.json",
     "title": "SpeedCrunch Theme",
     "description": "Defines the colors used by a SpeedCrunch theme.",
     "type": "object",
     "additionalProperties": false,
     "required": [
       "$schema",
       "$id",
       "number",
       "parens",
       "list",
       "unit",
       "result",
       "comment",
       "function",
       "operator",
       "variable",
       "separator",
       "background"
     ],
     "properties": {
       "$schema": {
         "const": "https://json-schema.org/draft/2020-12/schema",
         "description": "JSON Schema draft identifier"
       },
       "$id": {
         "const": "https://speedcrunch.org/schemas/theme-v1.schema.json",
         "description": "Theme schema identifier"
       },
       "name": {
         "type": "string",
         "description": "Optional theme display name"
       },
       "author": {
         "type": "string",
         "description": "Optional theme author"
       },
       "homepage": {
         "type": "string",
         "description": "Optional theme homepage"
       },
       "version": {
         "type": "string",
         "description": "Optional theme version chosen by the theme author"
       },
       "number": { "$ref": "#/$defs/color" },
       "parens": { "$ref": "#/$defs/color" },
       "list": { "$ref": "#/$defs/color" },
       "unit": { "$ref": "#/$defs/color" },
       "result": { "$ref": "#/$defs/color" },
       "comment": { "$ref": "#/$defs/color" },
       "function": { "$ref": "#/$defs/color" },
       "operator": { "$ref": "#/$defs/color" },
       "variable": { "$ref": "#/$defs/color" },
       "separator": { "$ref": "#/$defs/color" },
       "background": { "$ref": "#/$defs/color" },
       "primary": {
         "$ref": "#/$defs/color",
         "description": "Optional primary/accent color override"
       }
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
* ``background`` also supplies the base color for the generated application
  chrome, expression-editor surface colors, and successive generated surfaces
  used by dock titles, controls and headers, and content areas. Scrollbar
  colors are generated from the background of the surface that owns each
  scrollbar, so theme files do not include a separate key for them.
* ``primary`` is optional. When present, it overrides the generated
  primary/accent color used for active selections, focused editor outlines,
  cursor accents, pane splitters, and primary-hue keypad fills. When omitted,
  SpeedCrunch generates the primary/accent color from ``background``.
* ``name``, ``author``, ``homepage``, and ``version`` are optional metadata
  fields. ``version`` is a string that belongs to the theme author and is not
  used as the schema or format version. A non-empty ``name`` is used as the
  theme display name.

Fictitious Example
------------------

The following example is fictitious and provided only as a usage example:

.. code-block:: json

   {
     "$schema": "https://json-schema.org/draft/2020-12/schema",
     "$id": "https://speedcrunch.org/schemas/theme-v1.schema.json",
     "name": "Example Theme",
     "author": "Example Author",
     "homepage": "https://example.com",
     "version": "1",

     "number": "#6ED3FF",
     "parens": "#C8A2C8",
     "list": "#B39DDB",
     "unit": "#9CDCFE",
     "result": "#E8F1FF",
     "comment": "#7F8C8D",
     "function": "#FFB347",
     "operator": "#F5F5F5",
     "variable": "#FF8DA1",
     "separator": "#2F3640",
     "background": "#111827",
     "primary": "#6EE7B7"
   }
