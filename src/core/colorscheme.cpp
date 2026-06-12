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

ColorScheme::ColorScheme(const QJsonDocument& doc)
    : m_valid(false)
{
    if (!doc.isObject())
        return;

    const auto obj = doc.object();
    const QJsonValue schemeValue = obj.value(QStringLiteral("scheme"));
    if (!schemeValue.isUndefined()
            && (!schemeValue.isDouble() || schemeValue.toInt() != SchemeVersion)) {
        return;
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
    QColor color = m_colors[role];
    if (!color.isValid())
        return getFallbackColor(role);
    else
        return color;
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
    object.insert(QStringLiteral("scheme"), SchemeVersion);
    const auto roleEntries = roleNames();
    for (const auto& roleEntry : roleEntries)
        object.insert(roleEntry.first, colorForRole(roleEntry.second).name());
    return object;
}

ColorScheme ColorScheme::fromJsonObject(const QJsonObject& object)
{
    return ColorScheme(QJsonDocument(object));
}

QVector<QPair<QString, ColorScheme::Role>> ColorScheme::roleNames()
{
    return {
        { QStringLiteral("cursor"), ColorScheme::Cursor },
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
    };
}
