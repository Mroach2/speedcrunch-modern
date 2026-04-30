// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_STARTUPDEFINITIONS_H
#define CORE_STARTUPDEFINITIONS_H

/*
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "https://speedcrunch.org/schemas/startup-definitions.json",
  "title": "SpeedCrunch Startup Definitions",
  "type": "object",
  "required": ["startupDefinitions", "overwrite", "applyBeforeRestore"],
  "additionalProperties": false,
  "properties": {
    "startupDefinitions": {
      "type": "array",
      "items": { "type": "string" }
    },
    "overwrite": { "type": "boolean" },
    "applyBeforeRestore": { "type": "boolean" }
  }
}
*/

class Settings;

namespace StartupDefinitions {
namespace JsonKeys {
inline constexpr const char* StartupDefinitions = "startupDefinitions";
inline constexpr const char* Overwrite = "overwrite";
inline constexpr const char* ApplyBeforeRestore = "applyBeforeRestore";
}

void loadInto(Settings* settings);
void saveFrom(const Settings* settings);
}

#endif // CORE_STARTUPDEFINITIONS_H
