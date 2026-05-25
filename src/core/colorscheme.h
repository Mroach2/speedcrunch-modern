// SPDX-FileCopyrightText: 2009-2010, 2013-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_COLORSCHEME_H
#define CORE_COLORSCHEME_H

#include <QtCore/QHash>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QPair>
#include <QtCore/QVector>
#include <QColor>

class ColorScheme {
public:
    // Theme JSON schema documentation: doc/src/userguide/theme_json_schema.rst
    enum Role {
        Cursor,
        Number,
        Parens,
        List,
        Unit,
        Result,
        Comment,
        Matched,
        Function,
        Operator,
        Variable,
        Separator,
        Background
    };

    static const int SchemeVersion = 1;

    ColorScheme() : m_valid(false) { }
    ColorScheme(const QJsonDocument& doc);
    bool isValid() const { return m_valid; }
    QColor colorForRole(Role role) const;
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
    QHash<Role, QColor> m_colors;
};

#endif
