// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/settings.h"
#include "gui/editor.h"
#include "gui/mainwindow.h"
#include "gui/resultdisplay.h"

#include <QCoreApplication>
#include <QScrollBar>
#include <QTest>

class TestDisplayUi : public QObject {
    Q_OBJECT

private slots:
    void focusing_loaded_pane_preserves_its_current_scroll_position();
};

void TestDisplayUi::focusing_loaded_pane_preserves_its_current_scroll_position()
{
    Settings* appSettings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldSessionLayoutJson;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldWindowPositionSave;
        bool oldHasNumberFormatStyleSetting;

        ~SettingsGuard()
        {
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->windowPositionSave = oldWindowPositionSave;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        appSettings,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        appSettings->sessionLayoutJson,
        appSettings->constantsDockVisible,
        appSettings->functionsDockVisible,
        appSettings->historyDockVisible,
        appSettings->keypadVisible,
        appSettings->formulaBookDockVisible,
        appSettings->variablesDockVisible,
        appSettings->userFunctionsDockVisible,
        appSettings->userUnitsDockVisible,
        appSettings->bitfieldVisible,
        appSettings->windowPositionSave,
        appSettings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");

    appSettings->sessionLayoutJson.clear();
    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->keypadVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;
    appSettings->windowPositionSave = false;
    appSettings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QCoreApplication::processEvents();

    ResultDisplay* firstDisplay = window.findChild<ResultDisplay*>();
    Editor* firstEditor = window.findChild<Editor*>();
    QVERIFY(firstDisplay != nullptr);
    QVERIFY(firstEditor != nullptr);

    for (int i = 0; i < 80; ++i) {
        firstEditor->setText(QString::number(i));
        QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression", Qt::DirectConnection));
    }
    QCoreApplication::processEvents();

    QScrollBar* firstScrollBar = firstDisplay->verticalScrollBar();
    QVERIFY(firstScrollBar->maximum() > 20);
    firstScrollBar->setValue(10);
    QCOMPARE(firstScrollBar->value(), 10);

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();

    const QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    QCOMPARE(displays.size(), 2);

    // Make the already-loaded first pane diverge from the scroll snapshot that
    // was captured when focus moved to the second pane.
    firstScrollBar->setValue(20);
    QCOMPARE(firstScrollBar->value(), 20);

    QTest::mouseClick(firstDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      firstDisplay->viewport()->rect().center());
    QCoreApplication::processEvents();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    QCOMPARE(firstScrollBar->value(), 20);
}

int main(int argc, char** argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestDisplayUi test;
    return QTest::qExec(&test, argc, argv);
}

#include "testdisplayui.moc"
