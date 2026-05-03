// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


/*
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://speedcrunch.org/schemas/session.json",
  "title": "SpeedCrunch Session",
  "type": "object",
  "required": ["scheme", "speedcrunch", "session", "history", "variables", "functions", "units"],
  "additionalProperties": false,
  "properties": {
    "scheme": { "type": "integer", "const": 1 },
    "speedcrunch": { "type": "string" },
    "session": { "type": "string", "const": "main" },
    "history": {
      "type": "array",
      "items": { "$ref": "#/$defs/historyEntry" }
    },
    "variables": {
      "type": "array",
      "items": { "$ref": "#/$defs/variable" }
    },
    "functions": {
      "type": "array",
      "items": { "$ref": "#/$defs/userFunction" }
    },
    "units": {
      "type": "array",
      "items": { "$ref": "#/$defs/userUnit" }
    }
  },
  "$defs": {
    "historyEntry": {
      "type": "object",
      "required": ["xpr", "itp", "rst", "ctx", "edt"],
      "additionalProperties": false,
      "properties": {
        "xpr": { "type": "string" },
        "itp": { "type": "string" },
        "rst": { "$ref": "#/$defs/quantity" },
        "ctx": { "$ref": "#/$defs/evaluationContext" },
        "edt": { "type": "integer" },
        "prt": {
          "type": "array",
          "items": { "type": "string" }
        }
      }
    },
    "evaluationContext": {
      "type": "object",
      "required": ["lns", "cpx", "ang", "uxp", "rnd"],
      "additionalProperties": false,
      "properties": {
        "lns": {
          "type": "object",
          "required": ["main", "ext"],
          "additionalProperties": false,
          "properties": {
            "main": { "$ref": "#/$defs/resultLineContext" },
            "ext": {
              "type": "array",
              "maxItems": 4,
              "items": { "$ref": "#/$defs/resultLineContext" }
            }
          }
        },
        "cpx": {
          "type": "string",
          "enum": ["off", "i", "j"]
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
      "required": ["ntn", "prc", "cpx"],
      "additionalProperties": false,
      "properties": {
        "ntn": {
          "type": "string",
          "minLength": 1,
          "maxLength": 1
        },
        "prc": { "type": "integer" },
        "cpx": {
          "type": "string",
          "minLength": 1,
          "maxLength": 1
        }
      }
    },
    "variable": {
      "type": "object",
      "required": ["id", "qty"],
      "additionalProperties": false,
      "properties": {
        "id": { "type": "string" },
        "qty": { "$ref": "#/$defs/quantity" },
        "dsc": { "type": "string" }
      }
    },
    "userFunction": {
      "type": "object",
      "required": ["id", "args", "xpr"],
      "additionalProperties": false,
      "properties": {
        "id": { "type": "string" },
        "args": {
          "type": "array",
          "items": { "type": "string" }
        },
        "xpr": { "type": "string" },
        "itp": { "type": "string" },
        "dsc": { "type": "string" },
        "opcodes": {
          "type": "array",
          "items": { "$ref": "#/$defs/opcode" }
        },
        "constants": {
          "type": "array",
          "items": { "$ref": "#/$defs/cNumber" }
        },
        "identifiers": {
          "type": "array",
          "items": { "type": "string" }
        }
      }
    },
    "opcode": {
      "type": "object",
      "required": ["t", "i"],
      "additionalProperties": false,
      "properties": {
        "t": { "type": "integer" },
        "i": { "type": "integer" },
        "text": { "type": "string" }
      }
    },
    "userUnit": {
      "type": "object",
      "required": ["id", "qty"],
      "additionalProperties": false,
      "properties": {
        "id": { "type": "string" },
        "qty": { "$ref": "#/$defs/quantity" },
        "xpr": { "type": "string" },
        "itp": { "type": "string" },
        "dsc": { "type": "string" }
      }
    },
    "quantity": {
      "type": "object",
      "required": ["val"],
      "additionalProperties": true,
      "properties": {
        "val": { "type": "string" },
        "dim": {
          "type": "object",
          "additionalProperties": { "type": "string" }
        },
        "unit": {
          "type": "object",
          "additionalProperties": true
        },
        "unit_name": { "type": "string" },
        "format": {
          "type": "object",
          "additionalProperties": true
        }
      }
    },
    "cNumber": {
      "type": "object",
      "required": ["value"],
      "additionalProperties": true,
      "properties": {
        "value": { "type": "string" }
      }
    }
  }
}
*/

#ifndef CORE_SESSIONJSONKEYS_H
#define CORE_SESSIONJSONKEYS_H

namespace SessionJsonKeys {
inline constexpr const char* SchemaVersion = "scheme";
inline constexpr int SchemaVersionValue = 1;
inline constexpr const char* SpeedCrunch = "speedcrunch";
inline constexpr const char* Session = "session";
inline constexpr const char* SessionValueMain = "main";
inline constexpr const char* History = "history";
inline constexpr const char* Variables = "variables";
inline constexpr const char* Functions = "functions";
inline constexpr const char* Units = "units";

namespace Common {
inline constexpr const char* Id = "id";
inline constexpr const char* Description = "dsc";
inline constexpr const char* Expression = "xpr";
inline constexpr const char* InterpretedExpression = "itp";
}

namespace HistoryEntry {
inline constexpr const char* Expression = "xpr";
inline constexpr const char* InterpretedExpression = "itp";
inline constexpr const char* Result = "rst";
inline constexpr const char* Context = "ctx";
inline constexpr const char* EditTimestamp = "edt";
inline constexpr const char* PrintedLines = "prt";
}

namespace EvaluationContext {
inline constexpr const char* Lines = "lns";
inline constexpr const char* MainLine = "main";
inline constexpr const char* ExtraLines = "ext";
inline constexpr const char* Complex = "cpx";
inline constexpr const char* Angle = "ang";
inline constexpr const char* UnitExponentStyle = "uxp";
inline constexpr const char* Rounding = "rnd";

namespace Line {
inline constexpr const char* Notation = "ntn";
inline constexpr const char* Precision = "prc";
inline constexpr const char* ComplexFormat = "cpx";
}

namespace ComplexValues {
inline constexpr const char* Off = "off";
inline constexpr const char* I = "i";
inline constexpr const char* J = "j";
}
}

namespace Variable {
inline constexpr const char* Id = "id";
inline constexpr const char* Quantity = "qty";
inline constexpr const char* Description = "dsc";
inline constexpr const char* FormattedValue = "fmt";
}

namespace Function {
inline constexpr const char* Id = "id";
inline constexpr const char* Arguments = "args";
inline constexpr const char* Expression = "xpr";
inline constexpr const char* InterpretedExpression = "itp";
inline constexpr const char* Description = "dsc";
inline constexpr const char* Opcodes = "opcodes";
inline constexpr const char* Constants = "constants";
inline constexpr const char* Identifiers = "identifiers";

namespace Opcode {
inline constexpr const char* Type = "t";
inline constexpr const char* Index = "i";
inline constexpr const char* Text = "text";
}
}
}

#endif // CORE_SESSIONJSONKEYS_H
