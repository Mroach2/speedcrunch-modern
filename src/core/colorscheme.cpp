// SPDX-FileCopyrightText: 2009-2010, 2013-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/colorscheme.h"

#include "core/settings.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonValue>
#include <QLatin1String>
#include <QApplication>
#include <QPalette>

static const constexpr auto COLOR_SCHEME_EXTENSION = "json";

static const QVector<QString> colorSchemeSearchPaths()
{
    static QVector<QString> searchPaths;
    if (searchPaths.isEmpty()) {
        // By only populating the paths in a function when they're used, we ensure all the QApplication
        // fields that are used by QStandardPaths are set.
        searchPaths.append(QString("%1/color-schemes").arg(Settings::getDataPath()));
        searchPaths.append(QStringLiteral(":/color-schemes"));
    }
    return searchPaths;
}

QVector<QString> ColorScheme::fileSystemSearchPaths()
{
    QVector<QString> paths;
    const auto searchPaths = colorSchemeSearchPaths();
    for (const auto& path : searchPaths) {
        if (!path.startsWith(QLatin1Char(':')))
            paths.append(path);
    }
    return paths;
}

bool ColorScheme::isBuiltInName(const QString& name)
{
    return loadFromFile(QStringLiteral(":/color-schemes/%1.%2").arg(name, COLOR_SCHEME_EXTENSION)).isValid();
}

QString ColorScheme::filePathForName(const QString& name)
{
    for (const auto& path : colorSchemeSearchPaths()) {
        const QString fileName = QString("%1/%2.%3").arg(path, name, COLOR_SCHEME_EXTENSION);
        if (loadFromFile(fileName).isValid())
            return fileName;
    }
    return QString();
}

static QColor getFallbackColor(ColorScheme::Role role)
{
    switch (role) {
    case ColorScheme::Background:
        return QApplication::palette().color(QPalette::Base);
    default:
        return QApplication::palette().color(QPalette::Text);
    }
}

static bool hasSupportedThemeSchema(const QJsonObject& obj)
{
    const QJsonValue schemaValue = obj.value(QStringLiteral("$schema"));
    const QJsonValue idValue = obj.value(QStringLiteral("$id"));
    if (!schemaValue.isString() || schemaValue.toString() != QLatin1String(ColorScheme::SchemaDraft))
        return false;
    if (!idValue.isString() || idValue.toString() != QLatin1String(ColorScheme::SchemaId))
        return false;

    const QJsonValue versionValue = obj.value(QStringLiteral("version"));
    return versionValue.isUndefined() || versionValue.isString();
}

ColorScheme::ColorScheme(const QJsonDocument& doc)
    : m_valid(false)
{
    if (!doc.isObject())
        return;

    const auto obj = doc.object();
    if (!hasSupportedThemeSchema(obj))
        return;

    const QJsonValue nameValue = obj.value(QStringLiteral("name"));
    if (nameValue.isString()) {
        const QString name = nameValue.toString().trimmed();
        if (!name.isEmpty())
            m_displayName = name;
    }

    const auto roleEntries = roleNames();
    for (const auto& role : roleEntries) {
        auto v = obj.value(role.first);
        if (v.isUndefined())
            // Having a key missing is fine...
            continue;
        auto color = QColor(v.toString());
        if (!color.isValid())
            // ...having one that's not a color is not.
            return;
        m_colors.insert(role.second, color);
    }
    m_valid = true;
}

QColor ColorScheme::colorForRole(Role role) const
{
    QColor color = m_colors.value(role);
    if (!color.isValid())
        return getFallbackColor(role);
    else
        return color;
}

bool ColorScheme::hasColorForRole(Role role) const
{
    return m_colors.contains(role) && m_colors.value(role).isValid();
}

QStringList ColorScheme::enumerate()
{
    QMap<QString, void*> colorSchemes;
    for (auto& searchPath : colorSchemeSearchPaths()) {
        QDir dir(searchPath);
        dir.setFilter(QDir::Files | QDir::Readable);
        dir.setNameFilters({ QString("*.%1").arg(COLOR_SCHEME_EXTENSION) });
        const auto infoList = dir.entryInfoList();
        for (auto& info : infoList) // TODO: Use Qt 5.7's qAsConst().
            colorSchemes.insert(info.completeBaseName(), nullptr);
    }
    // Since this is a QMap, the keys are already sorted in ascending order.
    return colorSchemes.keys();
}

ColorScheme ColorScheme::loadFromFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return ColorScheme();
    // TODO: Better error handling.
    return ColorScheme(QJsonDocument::fromJson(file.readAll()));
}

ColorScheme ColorScheme::loadByName(const QString& name)
{
    const QString fileName = filePathForName(name);
    if (!fileName.isEmpty())
        return loadFromFile(fileName);
    return ColorScheme();
}

QJsonObject ColorScheme::toJsonObject() const
{
    QJsonObject object;
    object.insert(QStringLiteral("$schema"), QString::fromLatin1(SchemaDraft));
    object.insert(QStringLiteral("$id"), QString::fromLatin1(SchemaId));
    if (!m_displayName.isEmpty())
        object.insert(QStringLiteral("name"), m_displayName);
    const auto roleEntries = roleNames();
    for (const auto& roleEntry : roleEntries) {
        if (roleEntry.second == Primary && !hasColorForRole(Primary))
            continue;
        object.insert(roleEntry.first, colorForRole(roleEntry.second).name());
    }
    return object;
}

ColorScheme ColorScheme::fromJsonObject(const QJsonObject& object)
{
    return ColorScheme(QJsonDocument(object));
}

QVector<QPair<QString, ColorScheme::Role>> ColorScheme::roleNames()
{
    return {
        { QStringLiteral("number"), ColorScheme::Number },
        { QStringLiteral("parens"), ColorScheme::Parens },
        { QStringLiteral("list"), ColorScheme::List },
        { QStringLiteral("unit"), ColorScheme::Unit },
        { QStringLiteral("result"), ColorScheme::Result },
        { QStringLiteral("comment"), ColorScheme::Comment },
        { QStringLiteral("function"), ColorScheme::Function },
        { QStringLiteral("operator"), ColorScheme::Operator },
        { QStringLiteral("variable"), ColorScheme::Variable },
        { QStringLiteral("separator"), ColorScheme::Separator },
        { QStringLiteral("background"), ColorScheme::Background },
        { QStringLiteral("primary"), ColorScheme::Primary },
    };
}
