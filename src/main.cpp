// SPDX-FileCopyrightText: 2007-2010, 2013-2014, 2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/mainwindow.h"
#include "core/settings.h"

#include <QCoreApplication>
#include <QApplication>
#include <QGuiApplication>
#include <QCryptographicHash>
#include <QAbstractSocket>
#include <QDebug>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMetaObject>
#include <QEvent>
#include <QThread>
#include <QVariant>

#include <atomic>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_UNIX
#include <QSocketNotifier>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

// Dynamic property name mirrored by MainWindow. It lets the GUI layer observe
// shutdown state even when quit starts from the application/singleton layer.
constexpr const char* kShutdownInProgressProperty = "speedcrunchShutdownInProgress";

QString singletonServerName()
{
    const QString scope = QCoreApplication::applicationFilePath()
        + QLatin1Char('|')
        + Settings::getConfigPath();
    const QByteArray hash = QCryptographicHash::hash(scope.toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("speedcrunch-single-instance-%1").arg(QString::fromLatin1(hash.left(24)));
}

void activateMainWindow(MainWindow* window)
{
    if (!window)
        return;

    if (window->windowState() & Qt::WindowMinimized)
        window->setWindowState(window->windowState() & ~Qt::WindowMinimized);

    window->show();
    window->raise();
    window->activateWindow();
}

bool notifyRunningInstance(const QString& serverName)
{
    QLocalSocket socket;
    socket.connectToServer(serverName, QIODevice::ReadWrite);
    if (!socket.waitForConnected(250))
        return false;

    socket.write("activate\n");
    if (!socket.waitForBytesWritten(250))
        return false;
    if (!socket.waitForReadyRead(750))
        return false;
    // Treat the instance as running only after it explicitly accepts the
    // activation. If it is already shutting down, the new process should take
    // over instead of exiting and leaving the user with no restored windows.
    const QByteArray response = socket.readAll().trimmed();
    socket.disconnectFromServer();
    return response == QByteArrayLiteral("accepted");
}

bool startSingletonServer(QLocalServer* server, const QString& serverName, bool* alreadyRunning)
{
    if (alreadyRunning)
        *alreadyRunning = false;

    if (server->listen(serverName))
        return true;

    if (server->serverError() == QAbstractSocket::AddressInUseError) {
        if (notifyRunningInstance(serverName)) {
            if (alreadyRunning)
                *alreadyRunning = true;
            return false;
        }
        QLocalServer::removeServer(serverName);
        if (server->listen(serverName))
            return true;
    }

    qWarning() << "Could not initialize singleton server:" << server->errorString();
    return false;
}

MainWindow* g_mainWindow = 0;
std::atomic_bool g_eventLoopRunning(false);
std::atomic_bool g_shutdownInProgress(false);

bool shutdownInProgress()
{
    const QCoreApplication* application = QCoreApplication::instance();
    // Keep both an atomic flag and the QApplication property: signal handlers
    // and singleton callbacks use the atomic, while MainWindow close handling
    // reads the property.
    return g_shutdownInProgress.load()
        || (application && application->property(kShutdownInProgressProperty).toBool());
}

class ShutdownEventFilter : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        Q_UNUSED(watched);
        if (event != nullptr && event->type() == QEvent::Quit) {
            // Mark shutdown as early as possible. aboutToQuit can be too late
            // for nested close events that decide whether to save child-window
            // layout changes.
            g_shutdownInProgress.store(true);
            if (QCoreApplication::instance())
                QCoreApplication::instance()->setProperty(kShutdownInProgressProperty, true);
        }
        return false;
    }
};

void persistAndQuit()
{
    // Console/signal shutdown paths bypass the normal window-close entry point,
    // so they must publish shutdown state before asking MainWindow to persist.
    g_shutdownInProgress.store(true);
    if (QCoreApplication::instance())
        QCoreApplication::instance()->setProperty(kShutdownInProgressProperty, true);
    if (g_mainWindow)
        g_mainWindow->persistSessionAndSettingsForShutdown();
    if (QCoreApplication::instance())
        QCoreApplication::quit();
}

void requestGracefulShutdown(Qt::ConnectionType connectionType = Qt::QueuedConnection)
{
    QCoreApplication* application = QCoreApplication::instance();
    if (!application)
        return;

    if (QThread::currentThread() == application->thread()) {
        persistAndQuit();
        return;
    }

    QMetaObject::invokeMethod(application, []() {
        persistAndQuit();
    }, connectionType);
}

#ifdef Q_OS_UNIX
int g_unixSignalFds[2] = { -1, -1 };

void handleUnixSignal(int signalNumber)
{
    const char signalCode = static_cast<char>(signalNumber);
    const ssize_t written = ::write(g_unixSignalFds[0], &signalCode, sizeof(signalCode));
    Q_UNUSED(written);
}

bool installUnixSignalHandler(int signalNumber)
{
    struct sigaction action = {};
    action.sa_handler = handleUnixSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    return ::sigaction(signalNumber, &action, 0) == 0;
}

bool setupUnixTerminationSignalHandlers()
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_unixSignalFds) != 0)
        return false;

    const bool ok = installUnixSignalHandler(SIGTERM)
        && installUnixSignalHandler(SIGINT)
        && installUnixSignalHandler(SIGHUP);
    if (!ok) {
        ::close(g_unixSignalFds[0]);
        ::close(g_unixSignalFds[1]);
        g_unixSignalFds[0] = -1;
        g_unixSignalFds[1] = -1;
    }

    return ok;
}
#endif

#ifdef Q_OS_WIN
BOOL WINAPI handleWindowsConsoleControl(DWORD controlType)
{
    switch (controlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        requestGracefulShutdown(g_eventLoopRunning.load()
            ? Qt::BlockingQueuedConnection
            : Qt::QueuedConnection);
        return TRUE;
    default:
        return FALSE;
    }
}
#endif

}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    ShutdownEventFilter shutdownEventFilter(&application);
    application.installEventFilter(&shutdownEventFilter);

    QCoreApplication::setApplicationName("SpeedCrunch");
    QCoreApplication::setOrganizationDomain("speedcrunch.org");
    QGuiApplication::setDesktopFileName("org.speedcrunch.SpeedCrunch");

    Settings::instance();
    QLocalServer singletonServer;
    bool pendingActivation = false;

    QObject::connect(&application, &QCoreApplication::aboutToQuit, &application, [&]() {
        // Stop accepting single-instance activations during teardown. A second
        // launch that races with quit should create the replacement instance,
        // not send activation to a process that is already saving and exiting.
        g_shutdownInProgress.store(true);
        application.setProperty(kShutdownInProgressProperty, true);
        singletonServer.close();
        if (g_mainWindow)
            g_mainWindow->persistSessionAndSettingsForShutdown();
#ifdef Q_OS_UNIX
        if (g_unixSignalFds[0] != -1) {
            ::close(g_unixSignalFds[0]);
            g_unixSignalFds[0] = -1;
        }
        if (g_unixSignalFds[1] != -1) {
            ::close(g_unixSignalFds[1]);
            g_unixSignalFds[1] = -1;
        }
#endif
    });

#ifdef Q_OS_UNIX
    QSocketNotifier* unixSignalNotifier = 0;
    if (setupUnixTerminationSignalHandlers()) {
        unixSignalNotifier = new QSocketNotifier(g_unixSignalFds[1], QSocketNotifier::Read, &application);
        QObject::connect(unixSignalNotifier, &QSocketNotifier::activated, &application, [&]() {
            char signalCode = 0;
            const ssize_t bytesRead = ::read(g_unixSignalFds[1], &signalCode, sizeof(signalCode));
            if (bytesRead > 0)
                requestGracefulShutdown(Qt::QueuedConnection);
        });
    } else {
        qWarning() << "Could not install Unix termination signal handlers.";
    }
#endif

#ifdef Q_OS_WIN
    if (!SetConsoleCtrlHandler(handleWindowsConsoleControl, TRUE))
        qWarning() << "Could not install Windows console termination handler.";
#endif

    const QString serverName = singletonServerName();
    if (notifyRunningInstance(serverName))
        return 0;

    bool alreadyRunning = false;
    if (startSingletonServer(&singletonServer, serverName, &alreadyRunning)) {
        QObject::connect(&singletonServer, &QLocalServer::newConnection, &application, [&]() {
            bool acceptedActivation = false;
            while (singletonServer.hasPendingConnections()) {
                QLocalSocket* socket = singletonServer.nextPendingConnection();
                if (!socket)
                    continue;
                if (shutdownInProgress()) {
                    // Refuse activation during shutdown so the launching
                    // process removes the stale server and performs a full
                    // restore of all saved windows.
                    socket->write("shutting-down\n");
                    socket->waitForBytesWritten(250);
                    socket->close();
                    socket->deleteLater();
                    continue;
                }

                socket->write("accepted\n");
                socket->waitForBytesWritten(250);
                socket->close();
                socket->deleteLater();
                acceptedActivation = true;
            }

            // If all pending sockets were refused because shutdown is in
            // progress, do not raise a window that is about to disappear.
            if (!acceptedActivation)
                return;
            if (g_mainWindow)
                activateMainWindow(g_mainWindow);
            else
                pendingActivation = true;
        });
    } else if (alreadyRunning) {
        return 0;
    }

    MainWindow window;
    g_mainWindow = &window;
    window.show();

    if (pendingActivation)
        activateMainWindow(g_mainWindow);

    QObject::connect(&application, &QGuiApplication::lastWindowClosed, &application, [&]() {
        if (!shutdownInProgress())
            application.quit();
    });

    g_eventLoopRunning.store(true);
    const int result = application.exec();
    g_eventLoopRunning.store(false);
    g_mainWindow = 0;
    return result;
}
