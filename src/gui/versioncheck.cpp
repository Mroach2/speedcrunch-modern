// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/versioncheck.h"

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>
#include <QVector>

namespace {

static const char WEBSITE_URL[] = "https://speedcrunch.org";
static const char VERSION_URL[] = "https://speedcrunch.org/version";
static const char LAST_CHECK_KEY[] = "updates/lastVersionCheckUtcMsecs";
static const char LAST_FETCHED_VERSION_KEY[] = "updates/lastFetchedVersion";
static const char LAST_NOTIFIED_VERSION_KEY[] = "updates/lastNotifiedVersion";
const qint64 CHECK_INTERVAL_MSECS = 24LL * 60LL * 60LL * 1000LL;
Q_LOGGING_CATEGORY(lcVersionCheck, "speedcrunch.updatecheck")

static QDebug versionCheckDebug()
{
    return QMessageLogger(QT_MESSAGELOG_FILE,
                          QT_MESSAGELOG_LINE,
                          QT_MESSAGELOG_FUNC,
                          lcVersionCheck().categoryName()).debug();
}

}

VersionCheck::VersionCheck(QWidget* parentWindow, QObject* parent)
    : QObject(parent),
      m_parentWindow(parentWindow)
{
}

void VersionCheck::checkForUpdateIfDue()
{
    if (m_pendingReply) {
        versionCheckDebug() << "Update check skipped: request already in progress.";
        return;
    }
    if (!isCheckDue()) {
        versionCheckDebug() << "Update check skipped: next check not due yet.";
        return;
    }

    startRequest(CheckTriggerAutomatic);
}

void VersionCheck::checkForUpdateNow()
{
    if (m_pendingReply) {
        versionCheckDebug() << "Manual update check requested while another request is running.";
        showVerificationFailedDialog(m_parentWindow);
        return;
    }

    startRequest(CheckTriggerManual);
}

void VersionCheck::handleReplyFinished()
{
    QNetworkReply* reply = m_pendingReply;
    m_pendingReply = 0;

    if (!reply)
        return;

    versionCheckDebug() << "Update check response received."
                            << "HTTP status:"
                            << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    const QNetworkReply::NetworkError error = reply->error();
    const bool ok = (error == QNetworkReply::NoError);
    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    if (!ok) {
        versionCheckDebug() << "Update check failed:" << reply->errorString();
        if (m_trigger == CheckTriggerManual) {
            if (isConnectivityError(error))
                showNoConnectivityDialog(m_parentWindow);
            else
                showVerificationFailedDialog(m_parentWindow);
        }
        return;
    }

    const QString latestVersion = QString::fromUtf8(payload).trimmed().section('\n', 0, 0).trimmed();
    const QString currentVersion = QString::fromLatin1(SPEEDCRUNCH_VERSION);
    int comparison = 0;
    const bool comparable = compareVersions(latestVersion, currentVersion, &comparison);
    storeFetchedVersion(latestVersion);

    versionCheckDebug() << "Parsed versions:"
                            << "remote =" << latestVersion
                            << ", local =" << currentVersion
                            << ", comparable =" << comparable
                            << ", compare result =" << comparison;

    if (!comparable) {
        versionCheckDebug() << "Could not compare versions.";
        if (m_trigger == CheckTriggerManual)
            showVerificationFailedDialog(m_parentWindow);
    } else if (comparison > 0) {
        const bool shouldNotify = (m_trigger == CheckTriggerManual) || shouldNotifyForVersion(latestVersion);
        if (shouldNotify) {
            versionCheckDebug() << "Newer version found; showing update dialog.";
            showUpdateDialog(m_parentWindow, latestVersion);
            if (m_trigger == CheckTriggerAutomatic)
                markVersionNotified(latestVersion);
        } else {
            versionCheckDebug() << "Newer version found, but user was already notified for this version.";
        }
    } else {
        versionCheckDebug() << "No newer version available.";
        if (m_trigger == CheckTriggerManual)
            showUpToDateDialog(m_parentWindow, currentVersion);
    }
}

bool VersionCheck::compareVersions(const QString& left, const QString& right, int* result)
{
    if (!result)
        return false;
    *result = 0;

    QVector<qulonglong> leftComponents;
    QVector<qulonglong> rightComponents;
    if (!tryParseVersion(left, &leftComponents) || !tryParseVersion(right, &rightComponents))
        return false;

    const int count = qMax(leftComponents.size(), rightComponents.size());
    for (int i = 0; i < count; ++i) {
        const qulonglong leftPart = (i < leftComponents.size()) ? leftComponents.at(i) : 0;
        const qulonglong rightPart = (i < rightComponents.size()) ? rightComponents.at(i) : 0;
        if (leftPart < rightPart)
            *result = -1;
        if (leftPart > rightPart)
            *result = 1;
        if (*result != 0)
            return true;
    }

    return true;
}

bool VersionCheck::tryParseVersion(const QString& version, QVector<qulonglong>* components)
{
    if (!components)
        return false;
    components->clear();

    const QString trimmed = version.trimmed();
    if (trimmed.isEmpty())
        return false;

    const QStringList parts = trimmed.split('.', Qt::KeepEmptyParts);
    for (const QString& part : parts) {
        if (part.isEmpty())
            return false;

        for (const QChar ch : part) {
            if (!ch.isDigit())
                return false;
        }

        bool ok = false;
        const qulonglong value = part.toULongLong(&ok);
        if (!ok)
            return false;
        components->append(value);
    }

    return !components->isEmpty();
}

void VersionCheck::showUpdateDialog(QWidget* parent, const QString& latestVersion)
{
    versionCheckDebug() << "Displaying update dialog for version" << latestVersion;

    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Update Available"));

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QLabel* text = new QLabel(
        QObject::tr("A newer version (%1) is available.<br/><a href=\"%2\">Visit website to download.</a>")
            .arg(latestVersion, QString::fromLatin1(WEBSITE_URL)),
        &dialog);
    text->setTextFormat(Qt::RichText);
    text->setTextInteractionFlags(Qt::TextBrowserInteraction);
    text->setOpenExternalLinks(false);
    QObject::connect(text, &QLabel::linkActivated, &dialog, [](const QString& url) {
        QDesktopServices::openUrl(QUrl(url));
    });
    layout->addWidget(text);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

void VersionCheck::showUpToDateDialog(QWidget* parent, const QString& currentVersion)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Check for Updates"));

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QLabel* text = new QLabel(
        QObject::tr("SpeedCrunch %1 is up to date.").arg(currentVersion),
        &dialog);
    text->setWordWrap(true);
    layout->addWidget(text);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

void VersionCheck::showNoConnectivityDialog(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Check for Updates"));

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QLabel* text = new QLabel(
        QObject::tr("No connectivity. Please check your internet connection and try again."),
        &dialog);
    text->setWordWrap(true);
    layout->addWidget(text);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

void VersionCheck::showVerificationFailedDialog(QWidget* parent)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(QObject::tr("Check for Updates"));

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QLabel* text = new QLabel(
        QObject::tr("Could not verify the latest available version."),
        &dialog);
    text->setWordWrap(true);
    layout->addWidget(text);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    layout->addWidget(buttons);

    dialog.exec();
}

bool VersionCheck::isConnectivityError(QNetworkReply::NetworkError error)
{
    return error == QNetworkReply::ConnectionRefusedError
        || error == QNetworkReply::RemoteHostClosedError
        || error == QNetworkReply::HostNotFoundError
        || error == QNetworkReply::TimeoutError
        || error == QNetworkReply::OperationCanceledError
        || error == QNetworkReply::SslHandshakeFailedError
        || error == QNetworkReply::TemporaryNetworkFailureError
        || error == QNetworkReply::NetworkSessionFailedError
        || error == QNetworkReply::BackgroundRequestNotAllowedError
        || error == QNetworkReply::TooManyRedirectsError
        || error == QNetworkReply::InsecureRedirectError
        || error == QNetworkReply::ProxyConnectionRefusedError
        || error == QNetworkReply::ProxyConnectionClosedError
        || error == QNetworkReply::ProxyNotFoundError
        || error == QNetworkReply::ProxyTimeoutError
        || error == QNetworkReply::ProxyAuthenticationRequiredError
        || error == QNetworkReply::UnknownNetworkError
        || error == QNetworkReply::UnknownProxyError;
}

void VersionCheck::markCheckAttempted()
{
    QSettings settings;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    settings.setValue(QString::fromLatin1(LAST_CHECK_KEY), now);
    versionCheckDebug() << "Recorded update check timestamp (UTC msecs):" << now;
}

bool VersionCheck::isCheckDue() const
{
    QSettings settings;
    const qint64 lastCheck = settings.value(QString::fromLatin1(LAST_CHECK_KEY), 0).toLongLong();
    if (lastCheck <= 0) {
        versionCheckDebug() << "No previous update check timestamp found; check is due.";
        return true;
    }

    const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - lastCheck;
    const bool due = elapsed >= CHECK_INTERVAL_MSECS;
    versionCheckDebug() << "Last update check elapsed msecs:" << elapsed
                            << "; interval msecs:" << CHECK_INTERVAL_MSECS
                            << "; due =" << due;
    return due;
}

void VersionCheck::startRequest(CheckTrigger trigger)
{
    m_trigger = trigger;

    QNetworkRequest request(QUrl(QString::fromLatin1(VERSION_URL)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(5000);

    versionCheckDebug() << "Starting update check request to" << QString::fromLatin1(VERSION_URL)
                           << "; trigger =" << (trigger == CheckTriggerManual ? "manual" : "automatic");
    markCheckAttempted();
    m_pendingReply = m_networkAccessManager.get(request);
    connect(m_pendingReply, SIGNAL(finished()), this, SLOT(handleReplyFinished()));
}

void VersionCheck::storeFetchedVersion(const QString& version)
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(LAST_FETCHED_VERSION_KEY), version);
    versionCheckDebug() << "Recorded latest fetched version:" << version;
}

bool VersionCheck::shouldNotifyForVersion(const QString& version) const
{
    QSettings settings;
    const QString lastNotified = settings.value(QString::fromLatin1(LAST_NOTIFIED_VERSION_KEY)).toString().trimmed();
    if (lastNotified.isEmpty())
        return true;

    int comparison = 0;
    const bool comparable = compareVersions(version, lastNotified, &comparison);
    if (!comparable) {
        versionCheckDebug() << "Could not compare against last notified version; notifying by default."
                               << "remote =" << version << ", lastNotified =" << lastNotified;
        return true;
    }

    return comparison > 0;
}

void VersionCheck::markVersionNotified(const QString& version)
{
    QSettings settings;
    settings.setValue(QString::fromLatin1(LAST_NOTIFIED_VERSION_KEY), version);
    versionCheckDebug() << "Recorded notified version:" << version;
}
