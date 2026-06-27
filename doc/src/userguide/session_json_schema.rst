Session JSON Schema
===================

SpeedCrunch session files are JSON objects with stored calculation history,
session-local variables, user functions, user units, and global user-definition
startup lines. Session files include a schema identifier so JSON editors and
SpeedCrunch can associate them with the current SpeedCrunch session format.

Schema
------

.. code-block:: json

   {
     "$schema": "https://json-schema.org/draft/2020-12/schema",
     "$id": "https://speedcrunch.org/schemas/session-v1.schema.json",
     "title": "SpeedCrunch Session",
     "type": "object",
     "required": [
       "$schema",
       "$id",
       "session",
       "limit",
       "history",
       "variables",
       "functions",
       "units",
       "globals"
     ],
     "additionalProperties": false,
     "properties": {
       "$schema": {
         "const": "https://json-schema.org/draft/2020-12/schema",
         "description": "JSON Schema dialect identifier"
       },
       "$id": {
         "const": "https://speedcrunch.org/schemas/session-v1.schema.json",
         "description": "Session schema identifier"
       },
       "session": {
         "type": "string",
         "minLength": 1
       },
       "limit": {
         "type": "integer",
         "minimum": 0,
         "description": "Maximum stored history entries for this session; 0 means unlimited."
       },
       "history": {
         "type": "array",
         "items": {
           "$ref": "#/$defs/historyEntry"
         }
       },
       "variables": {
         "type": "array",
         "items": {
           "$ref": "#/$defs/variable"
         }
       },
       "functions": {
         "type": "array",
         "items": {
           "$ref": "#/$defs/userFunction"
         }
       },
       "units": {
         "type": "array",
         "items": {
           "$ref": "#/$defs/userUnit"
         }
       },
       "globals": {
         "type": "array",
         "items": {
           "type": "string"
         }
       }
     },
     "$defs": {
       "historyEntry": {
         "type": "object",
         "required": [
           "xpr",
           "itp",
           "rst",
           "ctx",
           "edt"
         ],
         "additionalProperties": false,
         "properties": {
           "xpr": {
             "type": "string"
           },
           "itp": {
             "type": "string"
           },
           "rst": {
             "$ref": "#/$defs/quantity"
           },
           "ctx": {
             "$ref": "#/$defs/evaluationContext"
           },
           "edt": {
             "type": "integer"
           },
           "prt": {
             "type": "array",
             "items": {
               "type": "string"
             }
           }
         }
       },
       "evaluationContext": {
         "type": "object",
         "required": [
           "lns",
           "cpx",
           "ang",
           "uxp",
           "rnd"
         ],
         "additionalProperties": false,
         "properties": {
           "lns": {
             "type": "object",
             "required": [
               "main",
               "ext"
             ],
             "additionalProperties": false,
             "properties": {
               "main": {
                 "$ref": "#/$defs/resultLineContext"
               },
               "ext": {
                 "type": "array",
                 "maxItems": 4,
                 "items": {
                   "$ref": "#/$defs/resultLineContext"
                 }
               }
             }
           },
           "cpx": {
             "type": "string",
             "enum": [
               "off",
               "i",
               "j"
             ]
           },
           "ang": {
             "type": "string",
             "minLength": 1,
             "maxLength": 1
           },
           "uxp": {
             "type": "string",
             "minLength": 1,
             "maxLength": 1
           },
           "rnd": {
             "type": "string",
             "minLength": 1,
             "maxLength": 1
           }
         }
       },
       "resultLineContext": {
         "type": "object",
         "required": [
           "ntn",
           "prc",
           "cpx"
         ],
         "additionalProperties": false,
         "properties": {
           "ntn": {
             "type": "string",
             "minLength": 1,
             "maxLength": 1
           },
           "prc": {
             "type": "integer"
           },
           "cpx": {
             "type": "string",
             "minLength": 1,
             "maxLength": 1
           }
         }
       },
       "variable": {
         "type": "object",
         "required": [
           "id",
           "qty"
         ],
         "additionalProperties": false,
         "properties": {
           "id": {
             "type": "string"
           },
           "qty": {
             "$ref": "#/$defs/quantity"
           },
           "dsc": {
             "type": "string"
           },
           "fmt": {
             "type": "string"
           }
         }
       },
       "userFunction": {
         "type": "object",
         "required": [
           "id",
           "args",
           "xpr"
         ],
         "additionalProperties": false,
         "properties": {
           "id": {
             "type": "string"
           },
           "args": {
             "type": "array",
             "items": {
               "type": "string"
             }
           },
           "xpr": {
             "type": "string"
           },
           "itp": {
             "type": "string"
           },
           "dsc": {
             "type": "string"
           },
           "opcodes": {
             "type": "array",
             "items": {
               "$ref": "#/$defs/opcode"
             }
           },
           "constants": {
             "type": "array",
             "items": {
               "$ref": "#/$defs/cNumber"
             }
           },
           "identifiers": {
             "type": "array",
             "items": {
               "type": "string"
             }
           }
         }
       },
       "opcode": {
         "type": "object",
         "required": [
           "t",
           "i"
         ],
         "additionalProperties": false,
         "properties": {
           "t": {
             "type": "integer"
           },
           "i": {
             "type": "integer"
           },
           "text": {
             "type": "string"
           }
         }
       },
       "userUnit": {
         "type": "object",
         "required": [
           "id",
           "qty"
         ],
         "additionalProperties": false,
         "properties": {
           "id": {
             "type": "string"
           },
           "qty": {
             "$ref": "#/$defs/quantity"
           },
           "xpr": {
             "type": "string"
           },
           "itp": {
             "type": "string"
           },
           "dsc": {
             "type": "string"
           }
         }
       },
       "quantity": {
         "type": "object",
         "required": [
           "val"
         ],
         "additionalProperties": true,
         "properties": {
           "val": {
             "type": "string"
           },
           "dim": {
             "type": "object",
             "additionalProperties": {
               "type": "string"
             }
           },
           "unit": {
             "type": "object",
             "additionalProperties": true
           },
           "unit_name": {
             "type": "string"
           },
           "format": {
             "type": "object",
             "additionalProperties": true
           }
         }
       },
       "cNumber": {
         "type": "object",
         "required": [
           "value"
         ],
         "additionalProperties": true,
         "properties": {
           "value": {
             "type": "string"
           }
         }
       }
     }
   }

Notes
-----

* ``$schema`` identifies the JSON Schema draft used by the session metadata.
  SpeedCrunch session files use
  ``https://json-schema.org/draft/2020-12/schema``.
* ``$id`` identifies the SpeedCrunch JSON Schema for the session format:
  ``https://speedcrunch.org/schemas/session-v1.schema.json``.
* ``session`` stores the session name. Empty names are normalized to ``main``
  by SpeedCrunch.
* ``limit`` stores the calculation history limit for the session. A value of
  ``0`` means unlimited history.
* ``history`` stores saved calculations. The compact field names preserve the
  existing session file format: for example ``xpr`` is the expression, ``rst``
  is the result quantity, and ``ctx`` is the evaluation context.
* ``variables``, ``functions``, and ``units`` store session-local user
  definitions. Definitions from global startup user definitions are stored in
  ``globals`` instead.
