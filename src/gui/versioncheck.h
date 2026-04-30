// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_VERSIONCHECK_H
#define GUI_VERSIONCHECK_H

#include <QObject>
#include <QPointer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QVector>

class QWidget;

class VersionCheck : public QObject {
    Q_OBJECT

public:
    enum CheckTrigger {
        CheckTriggerAutomatic,
        CheckTriggerManual
    };

    explicit VersionCheck(QWidget* parentWindow, QObject* parent = 0);

    void checkForUpdateIfDue();
    void checkForUpdateNow();

private slots:
    void handleReplyFinished();

private:
    static bool compareVersions(const QString& left, const QString& right, int* result);
    static bool tryParseVersion(const QString& version, QVector<qulonglong>* components);
    static void showUpdateDialog(QWidget* parent, const QString& latestVersion);
    static void showUpToDateDialog(QWidget* parent, const QString& currentVersion);
    static void showNoConnectivityDialog(QWidget* parent);
    static void showVerificationFailedDialog(QWidget* parent);
    static bool isConnectivityError(QNetworkReply::NetworkError error);

    void markCheckAttempted();
    bool isCheckDue() const;
    void startRequest(CheckTrigger trigger);
    void storeFetchedVersion(const QString& version);
    bool shouldNotifyForVersion(const QString& version) const;
    void markVersionNotified(const QString& version);

    QPointer<QWidget> m_parentWindow;
    QNetworkAccessManager m_networkAccessManager;
    QPointer<QNetworkReply> m_pendingReply;
    CheckTrigger m_trigger = CheckTriggerAutomatic;
};

#endif
