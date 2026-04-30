// SPDX-FileCopyrightText: 2016, 2022, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_MANUALSERVER_H
#define CORE_MANUALSERVER_H

#include <QObject>
#include <QMap>

class QHelpEngineCore;

class QCloseEvent;
class QUrl;
class QString;
class QByteArray;

class ManualServer : public QObject {
    Q_OBJECT

private:
    QString deployDocs();
    void setupHelpEngine();

public:
    static ManualServer* instance();
    QUrl homePage();
    QUrl urlForKeyword(const QString& keyword);
    QByteArray fileData(const QUrl &url);
    bool isSupportedLanguage(const QString&);

public slots:
    void ensureCorrectLanguage();

private:
    ManualServer();
    Q_DISABLE_COPY(ManualServer)

    void languageChanged();

    QHelpEngineCore *m_helpEngine;
    static ManualServer* s_instance;
    QString m_deployedLanguage;
};

#endif // CORE_MANUALSERVER_H
