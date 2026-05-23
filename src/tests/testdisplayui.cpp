// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/settings.h"
#include "gui/editor.h"
#include "gui/mainwindow.h"
#include "gui/notationandprecisiondialog.h"
#include "gui/resultdisplay.h"
#include "math/quantity.h"

#include <QCoreApplication>
#include <QDockWidget>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QPointer>
#include <QScrollBar>
#include <QSplitter>
#include <QTabBar>
#include <QTest>

namespace {
QWidget* paneWidgetForDisplay(ResultDisplay* display)
{
    QWidget* widget = display;
    while (widget != nullptr && !qobject_cast<QSplitter*>(widget->parentWidget()))
        widget = widget->parentWidget();
    return widget;
}

QWidget* inactiveOverlayForDisplay(ResultDisplay* display)
{
    QWidget* pane = paneWidgetForDisplay(display);
    return pane ? pane->findChild<QWidget*>(QStringLiteral("InactivePaneOverlay"), Qt::FindDirectChildrenOnly) : nullptr;
}

QRect tabBarRectInPane(ResultDisplay* display)
{
    QWidget* pane = paneWidgetForDisplay(display);
    QTabBar* tabBar = pane ? pane->findChild<QTabBar*>() : nullptr;
    if (pane == nullptr || tabBar == nullptr)
        return QRect();

    return QRect(tabBar->mapTo(pane, QPoint(0, 0)), tabBar->size());
}

void appendPaneScrollValues(const QJsonObject& node, QList<int>* values)
{
    const QString type = node.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("pane")) {
        const QJsonArray tabs = node.value(QStringLiteral("tabs")).toArray();
        for (const QJsonValue& tabValue : tabs) {
            const QJsonObject scroll = tabValue.toObject().value(QStringLiteral("scroll")).toObject();
            if (scroll.contains(QStringLiteral("value")))
                values->append(scroll.value(QStringLiteral("value")).toInt(-1));
        }
        return;
    }

    if (type != QLatin1String("split"))
        return;

    const QJsonArray children = node.value(QStringLiteral("children")).toArray();
    for (const QJsonValue& childValue : children) {
        if (childValue.isObject())
            appendPaneScrollValues(childValue.toObject(), values);
    }
}

void appendPaneEditorTexts(const QJsonObject& node, QStringList* texts)
{
    const QString type = node.value(QStringLiteral("type")).toString();
    if (type == QLatin1String("pane")) {
        const QString active = node.value(QStringLiteral("active")).toString();
        const QJsonArray tabs = node.value(QStringLiteral("tabs")).toArray();
        for (const QJsonValue& tabValue : tabs) {
            const QJsonObject tab = tabValue.toObject();
            if (tab.value(QStringLiteral("name")).toString() != active)
                continue;
            const QJsonObject editor = tab.value(QStringLiteral("editor")).toObject();
            if (editor.contains(QStringLiteral("text")))
                texts->append(editor.value(QStringLiteral("text")).toString());
        }
        return;
    }

    if (type != QLatin1String("split"))
        return;

    const QJsonArray children = node.value(QStringLiteral("children")).toArray();
    for (const QJsonValue& childValue : children) {
        if (childValue.isObject())
            appendPaneEditorTexts(childValue.toObject(), texts);
    }
}

bool menuContainsActionText(const QMenu* menu, const QString& text)
{
    for (QAction* action : menu->actions()) {
        if (action->text() == text)
            return true;
        if (action->menu() != nullptr && menuContainsActionText(action->menu(), text))
            return true;
    }
    return false;
}

class MenuTestResultDisplay : public ResultDisplay {
public:
    explicit MenuTestResultDisplay(QWidget* parent = nullptr)
        : ResultDisplay(parent)
    {
    }

    using ResultDisplay::createContextMenu;
};

bool contextMenuContainsMainMenu(MenuTestResultDisplay* display)
{
    QMenu* menu = display->createContextMenu(display->rect().center());
    const bool mainMenuSeen = menuContainsActionText(menu, QStringLiteral("Main Menu"));
    delete menu;
    return mainMenuSeen;
}

void sendTabDragMouseEvent(QTabBar* tabBar, QEvent::Type type, const QPoint& pos,
                           Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type,
                      QPointF(pos),
                      QPointF(tabBar->mapToGlobal(pos)),
                      button,
                      buttons,
                      Qt::NoModifier);
    QCoreApplication::sendEvent(tabBar, &event);
}
}

class TestDisplayUi : public QObject {
    Q_OBJECT

private slots:
    void result_display_insets_viewport_horizontally();
    void result_display_scrollbar_hover_keeps_viewport_width_stable();
    void result_display_context_menu_hides_main_menu_when_menu_bar_visible();
    void calculation_settings_dialog_matches_notation_precision_layout();
    void focusing_loaded_pane_preserves_its_current_scroll_position();
    void persisting_layout_captures_visible_scroll_positions_for_all_panes();
    void session_tabs_reorder_with_horizontal_drag();
    void closing_and_reopening_docks_keeps_attached_widgets();
};

void TestDisplayUi::result_display_insets_viewport_horizontally()
{
    ResultDisplay display;
    display.resize(320, 200);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));

    const QRect viewportGeometry = display.viewport()->geometry();
    QVERIFY(viewportGeometry.left() > 0);
    QVERIFY(display.width() - viewportGeometry.right() - 1 > 0);
}

void TestDisplayUi::result_display_scrollbar_hover_keeps_viewport_width_stable()
{
    ResultDisplay display;
    display.resize(360, 160);
    display.show();
    QVERIFY(QTest::qWaitForWindowExposed(&display));

    Quantity value;
    for (int i = 0; i < 40; ++i)
        display.append(QStringLiteral("123456789012345678901234567890"), value);

    QScrollBar* scrollBar = display.verticalScrollBar();
    QVERIFY(scrollBar->maximum() > scrollBar->minimum());

    const QRect initialViewportGeometry = display.viewport()->geometry();
    QEvent enterEvent(QEvent::Enter);
    QCoreApplication::sendEvent(scrollBar, &enterEvent);
    QCOMPARE(display.viewport()->geometry(), initialViewportGeometry);

    QEvent leaveEvent(QEvent::Leave);
    QCoreApplication::sendEvent(scrollBar, &leaveEvent);
    QCOMPARE(display.viewport()->geometry(), initialViewportGeometry);
}

void TestDisplayUi::result_display_context_menu_hides_main_menu_when_menu_bar_visible()
{
    QMainWindow window;
    window.menuBar()->addMenu(QStringLiteral("File"))->addAction(QStringLiteral("Dummy"));
    MenuTestResultDisplay* display = new MenuTestResultDisplay(&window);
    window.setCentralWidget(display);
    window.resize(360, 180);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.menuBar()->isVisible());
    QVERIFY(!contextMenuContainsMainMenu(display));

    window.menuBar()->hide();
    QVERIFY(!window.menuBar()->isVisible());
    QVERIFY(contextMenuContainsMainMenu(display));
}

void TestDisplayUi::calculation_settings_dialog_matches_notation_precision_layout()
{
    EvaluationContext context;
    context.main.fmt = 'f';
    context.main.prec = 3;
    context.angle = 'd';
    context.extras.append(ResultLineContext{'e', 5, ComplexForm::Default});

    ResultSlotsDialog dialog(QStringLiteral("Calculation Settings"), context);
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Calculation Settings"));

    const QList<QLabel*> labels = dialog.findChildren<QLabel*>();
    for (QLabel* label : labels)
        QVERIFY(label->text() != QStringLiteral("Angle Mode"));

    const EvaluationContext updated = dialog.evaluationContext(context);
    QCOMPARE(updated.angle, 'd');
    QCOMPARE(updated.main.fmt, 'f');
    QCOMPARE(updated.main.prec, 3);
    QCOMPARE(updated.extras.size(), 1);
    QCOMPARE(updated.extras.at(0).fmt, 'e');
    QCOMPARE(updated.extras.at(0).prec, 5);
}

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
    ResultDisplay* secondDisplay = displays.first() == firstDisplay ? displays.last() : displays.first();
    QWidget* secondPage = secondDisplay->parentWidget();
    Editor* secondEditor = secondPage ? secondPage->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    QVERIFY(secondEditor != nullptr);
    QWidget* firstOverlay = inactiveOverlayForDisplay(firstDisplay);
    QWidget* secondOverlay = inactiveOverlayForDisplay(secondDisplay);
    QVERIFY(firstOverlay != nullptr);
    QVERIFY(secondOverlay != nullptr);
    QVERIFY(firstOverlay->isVisible());
    QVERIFY(!secondOverlay->isVisible());
    QVERIFY(!firstOverlay->geometry().intersects(tabBarRectInPane(firstDisplay)));
    QTRY_VERIFY(secondEditor->hasFocus());

    // Make the already-loaded first pane diverge from the scroll snapshot that
    // was captured when focus moved to the second pane.
    firstScrollBar->setValue(20);
    QCOMPARE(firstScrollBar->value(), 20);

    QTest::mouseClick(firstDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      firstDisplay->viewport()->rect().center());
    QCoreApplication::processEvents();
    QTRY_VERIFY(firstEditor->hasFocus());
    QVERIFY(!firstOverlay->isVisible());
    QVERIFY(secondOverlay->isVisible());
    QVERIFY(!secondOverlay->geometry().intersects(tabBarRectInPane(secondDisplay)));

    QTabBar* secondTabBar = paneWidgetForDisplay(secondDisplay)->findChild<QTabBar*>();
    QVERIFY(secondTabBar != nullptr);
    QVERIFY(secondTabBar->isVisible());
    QTest::mouseClick(secondTabBar, Qt::LeftButton, Qt::NoModifier,
                      secondTabBar->tabRect(secondTabBar->currentIndex()).center());
    QCoreApplication::processEvents();
    QTRY_VERIFY(secondEditor->hasFocus());
    QVERIFY(firstOverlay->isVisible());
    QVERIFY(!secondOverlay->isVisible());
    QVERIFY(!firstOverlay->geometry().intersects(tabBarRectInPane(firstDisplay)));

    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents();

    QCOMPARE(firstScrollBar->value(), 20);
}

void TestDisplayUi::persisting_layout_captures_visible_scroll_positions_for_all_panes()
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
    QVERIFY(firstScrollBar->maximum() > 30);
    firstScrollBar->setValue(10);
    QCOMPARE(firstScrollBar->value(), 10);

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();

    const QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    QCOMPARE(displays.size(), 2);
    ResultDisplay* secondDisplay = displays.first() == firstDisplay ? displays.last() : displays.first();
    QWidget* secondPage = secondDisplay->parentWidget();
    Editor* secondEditor = secondPage ? secondPage->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    QVERIFY(secondEditor != nullptr);

    for (int i = 0; i < 80; ++i) {
        secondEditor->setText(QString::number(i + 100));
        QVERIFY(QMetaObject::invokeMethod(&window, "evaluateEditorExpression", Qt::DirectConnection));
    }
    QCoreApplication::processEvents();

    QScrollBar* secondScrollBar = secondDisplay->verticalScrollBar();
    QVERIFY(secondScrollBar->maximum() > 30);
    secondScrollBar->setValue(15);
    QCOMPARE(secondScrollBar->value(), 15);

    firstEditor->setText(QStringLiteral("first pane draft"));
    firstEditor->setCursorPosition(5);
    secondEditor->setText(QStringLiteral("second pane draft"));
    secondEditor->setCursorPosition(6);

    firstScrollBar->setValue(22);
    QCOMPARE(firstScrollBar->value(), 22);

    window.persistSessionAndSettingsForShutdown();

    const QJsonDocument layoutDoc = QJsonDocument::fromJson(appSettings->sessionLayoutJson.toUtf8());
    QVERIFY(layoutDoc.isObject());
    const QJsonArray windows = layoutDoc.object().value(QStringLiteral("windows")).toArray();
    QCOMPARE(windows.size(), 1);
    const QJsonObject root = windows.first().toObject().value(QStringLiteral("root")).toObject();
    QList<int> scrollValues;
    appendPaneScrollValues(root, &scrollValues);

    QVERIFY(scrollValues.contains(22));
    QVERIFY(scrollValues.contains(15));

    QStringList editorTexts;
    appendPaneEditorTexts(root, &editorTexts);
    QVERIFY(editorTexts.contains(QStringLiteral("first pane draft")));
    QVERIFY(editorTexts.contains(QStringLiteral("second pane draft")));
}

void TestDisplayUi::session_tabs_reorder_with_horizontal_drag()
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

    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QCoreApplication::processEvents();

    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(display != nullptr);
    QWidget* pane = paneWidgetForDisplay(display);
    QTabBar* tabBar = pane ? pane->findChild<QTabBar*>() : nullptr;
    QVERIFY(tabBar != nullptr);
    QVERIFY(tabBar->isVisible());
    QCOMPARE(tabBar->count(), 3);

    const auto dragTab = [tabBar](int from, int to) {
        const QPoint start = tabBar->tabRect(from).center();
        const QPoint end = to == 0
            ? QPoint(tabBar->tabRect(0).left() + 1, tabBar->tabRect(0).center().y())
            : QPoint(tabBar->tabRect(to).right() - 2, tabBar->tabRect(to).center().y());

        sendTabDragMouseEvent(tabBar, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        const int step = qMax(1, qAbs(end.x() - start.x()) / 6);
        if (end.x() >= start.x()) {
            for (int x = start.x(); x <= end.x(); x += step)
                sendTabDragMouseEvent(tabBar, QEvent::MouseMove, QPoint(x, start.y()), Qt::NoButton, Qt::LeftButton);
        } else {
            for (int x = start.x(); x >= end.x(); x -= step)
                sendTabDragMouseEvent(tabBar, QEvent::MouseMove, QPoint(x, start.y()), Qt::NoButton, Qt::LeftButton);
        }
        sendTabDragMouseEvent(tabBar, QEvent::MouseMove, end, Qt::NoButton, Qt::LeftButton);
        sendTabDragMouseEvent(tabBar, QEvent::MouseButtonRelease, end, Qt::LeftButton, Qt::NoButton);
    };

    const QString firstTab = tabBar->tabText(0);
    dragTab(0, 2);
    QCoreApplication::processEvents();
    QCOMPARE(tabBar->tabText(tabBar->count() - 1), firstTab);

    dragTab(tabBar->count() - 1, 0);
    QCoreApplication::processEvents();
    QCOMPARE(tabBar->tabText(0), firstTab);
}

void TestDisplayUi::closing_and_reopening_docks_keeps_attached_widgets()
{
    constexpr int dockLayoutStateVersion = 1;
    Settings* appSettings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldSessionLayoutJson;
        QByteArray oldWindowState;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldKeypadVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        bool oldHasNumberFormatStyleSetting;

        ~SettingsGuard()
        {
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->windowState = oldWindowState;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->keypadVisible = oldKeypadVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
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
        appSettings->windowState,
        appSettings->constantsDockVisible,
        appSettings->functionsDockVisible,
        appSettings->historyDockVisible,
        appSettings->keypadVisible,
        appSettings->formulaBookDockVisible,
        appSettings->variablesDockVisible,
        appSettings->userFunctionsDockVisible,
        appSettings->userUnitsDockVisible,
        appSettings->bitfieldVisible,
        appSettings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    appSettings->sessionLayoutJson.clear();
    appSettings->windowState.clear();
    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->keypadVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;
    appSettings->hasNumberFormatStyleSetting = true;

    QByteArray legacyDockState;
    QByteArray populatedDockState;
    {
        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        struct DockSpec {
            const char* setter;
            const char* objectName;
            bool hasFocusArgument;
        };
        const DockSpec dockSpecs[] = {
            { "setBitfieldVisible", "BitfieldDock", false },
            { "setFormulaBookDockVisible", "BookDock", true },
            { "setConstantsDockVisible", "ConstantsDock", true },
            { "setFunctionsDockVisible", "FunctionsDock", true },
            { "setHistoryDockVisible", "HistoryDock", true },
            { "setVariablesDockVisible", "VariablesDock", true },
            { "setUserFunctionsDockVisible", "UserFunctionsDock", true },
            { "setUserUnitsDockVisible", "UserUnitsDock", true }
        };

        const auto invokeVisible = [&window](const DockSpec& spec, bool visible) {
            if (!spec.hasFocusArgument) {
                return QMetaObject::invokeMethod(&window, spec.setter, Qt::DirectConnection,
                                                 Q_ARG(bool, visible));
            }
            return QMetaObject::invokeMethod(&window, spec.setter, Qt::DirectConnection,
                                             Q_ARG(bool, visible), Q_ARG(bool, false));
        };

        const auto dockCount = [&window](const QString& objectName) {
            int count = 0;
            for (QDockWidget* dock : window.findChildren<QDockWidget*>()) {
                if (dock->objectName() == objectName)
                    ++count;
            }
            return count;
        };

        for (const DockSpec& spec : dockSpecs) {
            const QString objectName = QString::fromLatin1(spec.objectName);
            QPointer<QDockWidget> originalDock = window.findChild<QDockWidget*>(objectName);
            QVERIFY(originalDock != nullptr);
            QVERIFY(!originalDock->isVisible());
            QCOMPARE(dockCount(objectName), 1);

            QVERIFY(invokeVisible(spec, true));
            QCoreApplication::processEvents();

            originalDock->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QCoreApplication::processEvents();

            QVERIFY(originalDock != nullptr);
            QVERIFY(!originalDock->isVisible());
            QVERIFY(window.dockWidgetArea(originalDock) != Qt::NoDockWidgetArea);
            QCOMPARE(dockCount(objectName), 1);

            const QByteArray hiddenState = window.saveState(dockLayoutStateVersion);
            QVERIFY(window.restoreState(hiddenState, dockLayoutStateVersion));
            QVERIFY(invokeVisible(spec, true));
            QCoreApplication::processEvents();
            QCOMPARE(window.findChild<QDockWidget*>(objectName), originalDock.data());
            QCOMPARE(dockCount(objectName), 1);
        }

        legacyDockState = window.saveState();
        populatedDockState = window.saveState(dockLayoutStateVersion);
    }

    appSettings->constantsDockVisible = false;
    appSettings->functionsDockVisible = false;
    appSettings->historyDockVisible = false;
    appSettings->formulaBookDockVisible = false;
    appSettings->variablesDockVisible = false;
    appSettings->userFunctionsDockVisible = false;
    appSettings->userUnitsDockVisible = false;
    appSettings->bitfieldVisible = false;

    {
        appSettings->windowState = legacyDockState;
        MainWindow legacyStateWindow;
        legacyStateWindow.show();
        QCoreApplication::processEvents();

        QDockWidget* bitfield = legacyStateWindow.findChild<QDockWidget*>(QStringLiteral("BitfieldDock"));
        QVERIFY(bitfield != nullptr);
        QVERIFY(!bitfield->isVisible());
    }

    {
        appSettings->windowState = populatedDockState;
        MainWindow restoredWindow;
        restoredWindow.show();
        QCoreApplication::processEvents();

        const char* dockNames[] = {
            "BitfieldDock", "BookDock", "ConstantsDock", "FunctionsDock",
            "HistoryDock", "VariablesDock", "UserFunctionsDock", "UserUnitsDock"
        };
        for (const char* dockName : dockNames)
            QCOMPARE(restoredWindow.findChildren<QDockWidget*>(QString::fromLatin1(dockName)).size(), 1);
        QDockWidget* bitfield = restoredWindow.findChild<QDockWidget*>(QStringLiteral("BitfieldDock"));
        QVERIFY(bitfield != nullptr);
        QVERIFY(bitfield->isVisible());
    }
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
