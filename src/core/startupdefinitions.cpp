// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/startupdefinitions.h"

#include "core/settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace {
QString startupDefinitionsPath()
{
    return Settings::getDataPath() + QStringLiteral("/definitions.json");
}
}

void StartupDefinitions::loadInto(Settings* settings)
{
    if (!settings)
        return;

    settings->startupUserDefinitions.clear();
    settings->startupUserDefinitionsOverwrite = false;
    settings->startupUserDefinitionsApplyBeforeRestore = false;

    QFile file(startupDefinitionsPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
        return;

    const QJsonObject json = doc.object();
    QStringList definitionLines;
    const QJsonValue startupDefinitionsValue = json.value(
        QLatin1String(StartupDefinitions::JsonKeys::StartupDefinitions));
    if (startupDefinitionsValue.isArray()) {
        const QJsonArray definitionsArray = startupDefinitionsValue.toArray();
        for (const QJsonValue& value : definitionsArray) {
            if (value.isString())
                definitionLines.append(value.toString());
        }
    } else if (startupDefinitionsValue.isString()) {
        // Accept legacy string format if present.
        definitionLines = startupDefinitionsValue.toString().split(QLatin1Char('\n'));
    }
    settings->startupUserDefinitions = definitionLines.join(QLatin1Char('\n'));
    settings->startupUserDefinitionsOverwrite = json.value(
        QLatin1String(StartupDefinitions::JsonKeys::Overwrite)).toBool(false);
    settings->startupUserDefinitionsApplyBeforeRestore = json.value(
        QLatin1String(StartupDefinitions::JsonKeys::ApplyBeforeRestore)).toBool(false);
}

void StartupDefinitions::saveFrom(const Settings* settings)
{
    if (!settings)
        return;

    const QString path = startupDefinitionsPath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    QJsonObject json;
    QJsonArray definitionsArray;
    const QStringList definitionLines = settings->startupUserDefinitions.split(QLatin1Char('\n'));
    for (const QString& line : definitionLines) {
        if (!line.trimmed().isEmpty())
            definitionsArray.append(line);
    }
    json[QLatin1String(StartupDefinitions::JsonKeys::StartupDefinitions)] = definitionsArray;
    json[QLatin1String(StartupDefinitions::JsonKeys::Overwrite)] = settings->startupUserDefinitionsOverwrite;
    json[QLatin1String(StartupDefinitions::JsonKeys::ApplyBeforeRestore)] = settings->startupUserDefinitionsApplyBeforeRestore;
    file.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
}
