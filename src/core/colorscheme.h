// SPDX-FileCopyrightText: 2009-2010, 2013-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_COLORSCHEME_H
#define CORE_COLORSCHEME_H

#include <QtCore/QHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QVector>
#include <QColor>

class ColorScheme {
public:
    // Theme JSON schema documentation: doc/src/userguide/theme_json_schema.rst
    enum Role {
        Number,
        Parens,
        List,
        Unit,
        Result,
        Comment,
        Function,
        Operator,
        Variable,
        Separator,
        Background,
        Primary
    };

    static constexpr const char* SchemaDraft = "https://json-schema.org/draft/2020-12/schema";
    static constexpr const char* SchemaId = "https://speedcrunch.org/schemas/theme-v1.schema.json";

    ColorScheme() : m_valid(false) { }
    ColorScheme(const QJsonDocument& doc);
    bool isValid() const { return m_valid; }
    QString displayName() const { return m_displayName; }
    QColor colorForRole(Role role) const;
    bool hasColorForRole(Role role) const;
    QJsonObject toJsonObject() const;

    static QStringList enumerate();
    static bool isBuiltInName(const QString& name);
    static QVector<QString> fileSystemSearchPaths();
    static QString filePathForName(const QString& name);
    static ColorScheme loadFromFile(const QString& path);
    static ColorScheme loadByName(const QString& name);
    static ColorScheme fromJsonObject(const QJsonObject& object);
    static QVector<QPair<QString, Role>> roleNames();

private:
    bool m_valid;
    QString m_displayName;
    QHash<Role, QColor> m_colors;
};

#endif
