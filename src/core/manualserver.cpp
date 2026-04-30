// SPDX-FileCopyrightText: 2016, 2021-2022, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later



#include "core/manualserver.h"
#include "core/settings.h"

#include <QtCore/QDir>
#include <QtCore/QLocale>
#include <QFile>
#include <QtHelp/QHelpEngineCore>
#include <QtHelp/QHelpLink>
#include <QString>
#include <QMap>
#include <QEvent>

#define FALLBACK_LANG "en_US"

#define COPY_OVERWRITE(source, dest) \
    if (QFile::exists(dest)) \
        QFile::remove(dest); \
    QFile::copy(source, dest);

#define QHC_NAME(lang) ("manual-" + QString(lang) + ".qhc")
#define QCH_NAME(lang) ("manual-" + QString(lang) + ".qch")
#define QHC_RES_PATH(lang) (":/manual/" + QHC_NAME(lang))
#define QCH_RES_PATH(lang) (":/manual/" + QCH_NAME(lang))

ManualServer* ManualServer::s_instance = nullptr;

QString ManualServer::deployDocs()
{
    QString dest = Settings::getCachePath() + "/manual/";
    QDir qdir;
    qdir.mkpath(dest);
    QString lang = Settings::instance()->language;
    m_deployedLanguage = lang;
    if (lang == "C")
        lang = QLocale().name();

    if (!isSupportedLanguage(lang)) {
        // Extract the language from the locale and try to find a generic translation for it
        QStringList languages = QLocale((QLocale(lang).language())).uiLanguages();
        lang = FALLBACK_LANG;
        for (int i = 0; i < languages.size(); ++i) {
            QString extra_lang = languages[i].replace('-', '_');
            if (isSupportedLanguage(extra_lang)) {
                lang = extra_lang;
                break;
            }
        }
    }

    QFile::remove(dest + QHC_NAME(lang));
    QFile::remove(dest + QCH_NAME(lang));

    QFile::copy(QHC_RES_PATH(lang), dest + QHC_NAME(lang));
    QFile::copy(QCH_RES_PATH(lang), dest + QCH_NAME(lang));

    QFile qhc(dest + QHC_NAME(lang));
    qhc.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    QFile qch(dest + QCH_NAME(lang));
    qch.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);

    return dest + "manual-" + lang + ".qhc";
}

bool ManualServer::isSupportedLanguage(const QString& lang) {
  return (QFile(QHC_RES_PATH(lang)).exists() && QFile(QCH_RES_PATH(lang)).exists());
}

void ManualServer::setupHelpEngine()
{
    QString collectionFile = deployDocs() ;

    m_helpEngine = new QHelpEngineCore(collectionFile, this);

    QStringList filters = m_helpEngine->customFilters();
    if (!filters.isEmpty())
        m_helpEngine->setCurrentFilter(filters.first());
}

ManualServer *ManualServer::instance()
{
    if (!s_instance)
        s_instance = new ManualServer();
    return s_instance;
}

QUrl ManualServer::homePage()
{
    ensureCorrectLanguage();
    if (!m_helpEngine)
        return QUrl();
    auto docs = m_helpEngine->documentsForKeyword("SpeedCrunch");
    if (!docs.isEmpty()) {
        return docs[0].url;
    }
    return QUrl();
}

QUrl ManualServer::urlForKeyword(const QString& keyword)
{
    ensureCorrectLanguage();
    if (!m_helpEngine)
        return QUrl();
    auto constants = m_helpEngine->documentsForIdentifier(QString("constant.%1").arg(keyword));
    if (!constants.isEmpty()) {
        return constants[0].url;
    }
    auto functions = m_helpEngine->documentsForIdentifier(QString("function.%1").arg(keyword));
    if (!functions.isEmpty()) {
        return functions[0].url;
    }
    return QUrl();
}

QByteArray ManualServer::fileData(const QUrl &url)
{
    ensureCorrectLanguage();
    return m_helpEngine->fileData(url);
}

void ManualServer::languageChanged()
{
    setupHelpEngine();
}

void ManualServer::ensureCorrectLanguage()
{
    if (Settings::instance()->language != m_deployedLanguage)
        languageChanged();
}

ManualServer::ManualServer()
{
    m_helpEngine = nullptr;
}
