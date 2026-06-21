// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/colorscheme.h"
#include "core/settings.h"
#include "gui/bitfieldwidget.h"
#include "gui/constantswidget.h"
#include "gui/dockliststyle.h"
#include "gui/editor.h"
#include "gui/functionswidget.h"
#include "gui/keypad.h"
#include "gui/mainwindow.h"
#include "gui/notationandprecisiondialog.h"
#include "gui/oklchutils.h"
#include "gui/resultdisplay.h"
#include "gui/themedlineedit.h"
#include "gui/uiconfig.h"
#include "math/quantity.h"

#include <QCoreApplication>
#include <QAbstractItemView>
#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDockWidget>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QFocusEvent>
#include <QHeaderView>
#include <QHelpEvent>
#include <QImage>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QMainWindow>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMouseEvent>
#include <QMargins>
#include <QLineEdit>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QScopeGuard>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QTest>
#include <QTextBrowser>
#include <QToolButton>
#include <QTranslator>
#include <QTimer>
#include <QTreeWidget>

namespace {
QWidget* paneWidgetForDisplay(ResultDisplay* display)
{
    QWidget* widget = display;
    while (widget != nullptr && !qobject_cast<QSplitter*>(widget->parentWidget()))
        widget = widget->parentWidget();
    return widget;
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

bool editorHasPrimaryOutline(const Editor* editor, const QColor& primary)
{
    return editor != nullptr
        && editor->styleSheet().contains(
            QStringLiteral("border: %1px solid %2")
                .arg(UiConfig::OutlineStrokeWidth)
                .arg(primary.name()));
}

bool anyEditorHasPrimaryOutline(const QList<Editor*>& editors, const QColor& primary)
{
    for (const Editor* editor : editors) {
        if (editorHasPrimaryOutline(editor, primary))
            return true;
    }
    return false;
}

bool colorsAreClose(const QColor& actual, const QColor& expected, int tolerance = 2)
{
    return qAbs(actual.red() - expected.red()) <= tolerance
        && qAbs(actual.green() - expected.green()) <= tolerance
        && qAbs(actual.blue() - expected.blue()) <= tolerance;
}

class FunctionsTestTranslator : public QTranslator {
public:
    QString translate(const char* context,
                      const char* sourceText,
                      const char* disambiguation = nullptr,
                      int n = -1) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);

        if (qstrcmp(context, "FunctionsWidget") == 0
            && qstrcmp(sourceText, "Domain") == 0) {
            return QStringLiteral("Translated Domain");
        }

        return QString();
    }
};

QPushButton* keypadButtonWithText(Keypad* keypad, const QString& text)
{
    if (keypad == nullptr)
        return nullptr;

    for (QPushButton* button : keypad->findChildren<QPushButton*>()) {
        if (button->text() == text)
            return button;
    }
    return nullptr;
}

QAction* keypadModeAction(MainWindow* window, Settings::KeypadMode mode)
{
    if (window == nullptr)
        return nullptr;

    for (QAction* action : window->findChildren<QAction*>()) {
        if (action->isCheckable()
                && action->data().isValid()
                && action->data().toInt() == static_cast<int>(mode)) {
            return action;
        }
    }
    return nullptr;
}

QColor keypadPrimaryStateBackgroundForTest(const QColor& primary,
                                           const QColor& stateBackground,
                                           const QColor& normalBackground)
{
    Oklch primaryOklch = qColorToOklch(primary);
    const Oklch stateOklch = qColorToOklch(stateBackground);
    const Oklch normalOklch = qColorToOklch(normalBackground);
    const double offset = stateOklch.l - normalOklch.l;
    if (qAbs(offset) < 1e-9)
        return primary;

    primaryOklch.l = qBound(0.0, primaryOklch.l + offset, 1.0);
    return oklchToValidSrgbQColor(primaryOklch);
}

QColor keypadPrimaryHueFillForTest(const QColor& primary,
                                   const QColor& stateBackground,
                                   const QColor& normalBackground,
                                   int primaryPercent)
{
    const double primaryRatio =
        double(qBound(0, primaryPercent, 100)) / 100.0;
    if (primaryRatio <= 0.0)
        return stateBackground;

    const QColor primaryStateBackground =
        keypadPrimaryStateBackgroundForTest(primary, stateBackground, normalBackground);
    if (primaryRatio >= 1.0)
        return primaryStateBackground;

    const Oklch stateOklch = qColorToOklch(stateBackground);
    const Oklch primaryStateOklch = qColorToOklch(primaryStateBackground);
    return oklchToValidSrgbQColor(Oklch {
        stateOklch.l + (primaryStateOklch.l - stateOklch.l) * primaryRatio,
        stateOklch.c + (primaryStateOklch.c - stateOklch.c) * primaryRatio,
        primaryStateOklch.h,
        stateOklch.alpha
    });
}

QImage dockSeparatorPrimitiveImage(QWidget* widget,
                                   QStyle::State state,
                                   const QRect& separatorRect,
                                   const QSize& imageSize)
{
    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QStyleOption option;
    option.rect = separatorRect;
    option.state = state;
    option.palette = widget->palette();

    QPainter painter(&image);
    QApplication::style()->drawPrimitive(QStyle::PE_IndicatorDockWidgetResizeHandle,
                                         &option,
                                         &painter,
                                         widget);
    return image;
}

QImage dockSeparatorPrimitiveImage(QWidget* widget, QStyle::State state, const QSize& size = QSize(32, 8))
{
    return dockSeparatorPrimitiveImage(widget, state, QRect(QPoint(0, 0), size), size);
}

QColor dockSeparatorPrimitiveColor(QWidget* widget, QStyle::State state)
{
    const QImage image = dockSeparatorPrimitiveImage(widget, state);
    return image.pixelColor(image.rect().center());
}

QRect sessionTabPillRect(const QTabBar* tabBar)
{
    return tabBar->tabRect(tabBar->currentIndex()).adjusted(2, 3, -2, 0);
}

QColor selectedSessionTabFillColor(QTabBar* tabBar)
{
    const QRect pill = sessionTabPillRect(tabBar);
    const QImage image = tabBar->grab().toImage();
    return image.pixelColor(pill.left() + 6, pill.center().y());
}

bool selectedSessionTabHasBottomIndicator(QTabBar* tabBar, const QColor& color)
{
    const QRect pill = sessionTabPillRect(tabBar);
    const QImage image = tabBar->grab().toImage();
    const int firstY = qMax(pill.top(), pill.bottom() - UiConfig::ActiveSessionTabIndicatorStrokeWidth);
    for (int y = firstY; y <= pill.bottom(); ++y) {
        for (int x = pill.left() + 4; x <= pill.right() - 4; ++x) {
            if (image.rect().contains(x, y) && colorsAreClose(image.pixelColor(x, y), color))
                return true;
        }
    }
    return false;
}

Editor* editorForDisplay(ResultDisplay* display)
{
    QWidget* page = display != nullptr ? display->parentWidget() : nullptr;
    return page ? page->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
}

QTabBar* tabBarForDisplay(ResultDisplay* display)
{
    QWidget* pane = paneWidgetForDisplay(display);
    return pane ? pane->findChild<QTabBar*>() : nullptr;
}

QString visibleResultPreviewText(const MainWindow& window)
{
    for (QLabel* label : window.findChildren<QLabel*>()) {
        if (!label->isVisible())
            continue;
        const QString text = label->text();
        if (text.contains(QStringLiteral("Current result:"))
            || text.contains(QStringLiteral("Selection result:"))) {
            return text;
        }
    }
    return QString();
}

struct MainWindowStateGuard {
    Settings* settings = Settings::instance();
    QString oldColorScheme = settings->colorScheme;
    QString oldCustomColorSchemeJson = settings->customColorSchemeJson;
    QString oldSessionLayoutJson = settings->sessionLayoutJson;
    QString oldConstantsDockDomain = settings->constantsDockDomain;
    QString oldConstantsDockSubdomain = settings->constantsDockSubdomain;
    QString oldConstantsDockSearchText = settings->constantsDockSearchText;
    QByteArray oldWindowState = settings->windowState;
    QByteArray oldWindowGeometry = settings->windowGeometry;
    bool oldConstantsDockVisible = settings->constantsDockVisible;
    bool oldFunctionsDockVisible = settings->functionsDockVisible;
    bool oldHistoryDockVisible = settings->historyDockVisible;
    bool oldKeypadVisible = settings->keypadVisible;
    bool oldFormulaBookDockVisible = settings->formulaBookDockVisible;
    bool oldVariablesDockVisible = settings->variablesDockVisible;
    bool oldUserFunctionsDockVisible = settings->userFunctionsDockVisible;
    bool oldUserUnitsDockVisible = settings->userUnitsDockVisible;
    bool oldBitfieldVisible = settings->bitfieldVisible;
    Settings::KeypadMode oldKeypadMode = settings->keypadMode;
    bool oldWindowPositionSave = settings->windowPositionSave;
    bool oldStatusBarVisible = settings->statusBarVisible;
    bool oldHasNumberFormatStyleSetting = settings->hasNumberFormatStyleSetting;
    QByteArray oldSkipUpdateCheck = qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
    bool hadSkipUpdateCheck = qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");

    MainWindowStateGuard()
    {
        settings->windowPositionSave = false;
        qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    }

    ~MainWindowStateGuard()
    {
        settings->colorScheme = oldColorScheme;
        settings->customColorSchemeJson = oldCustomColorSchemeJson;
        settings->sessionLayoutJson = oldSessionLayoutJson;
        settings->constantsDockDomain = oldConstantsDockDomain;
        settings->constantsDockSubdomain = oldConstantsDockSubdomain;
        settings->constantsDockSearchText = oldConstantsDockSearchText;
        settings->windowState = oldWindowState;
        settings->windowGeometry = oldWindowGeometry;
        settings->constantsDockVisible = oldConstantsDockVisible;
        settings->functionsDockVisible = oldFunctionsDockVisible;
        settings->historyDockVisible = oldHistoryDockVisible;
        settings->keypadVisible = oldKeypadVisible;
        settings->formulaBookDockVisible = oldFormulaBookDockVisible;
        settings->variablesDockVisible = oldVariablesDockVisible;
        settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
        settings->userUnitsDockVisible = oldUserUnitsDockVisible;
        settings->bitfieldVisible = oldBitfieldVisible;
        settings->keypadMode = oldKeypadMode;
        settings->windowPositionSave = oldWindowPositionSave;
        settings->statusBarVisible = oldStatusBarVisible;
        settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
        if (hadSkipUpdateCheck)
            qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
        else
            qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
    }
};

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
    void color_scheme_roles_exclude_obsolete_scrollbar();
    void theme_dialog_preserves_list_scroll_and_fills_role_color_buttons();
    void result_display_insets_viewport_horizontally();
    void result_display_scrollbar_hover_keeps_viewport_width_stable();
    void result_display_context_menu_hides_main_menu_when_menu_bar_visible();
    void bitfield_selected_bit_keeps_primary_fill_while_hovered();
    void bitfield_buttons_use_configured_generated_shades();
    void dock_list_selected_row_keeps_primary_fill_while_hovered();
    void custom_keypad_action_stays_checked_after_dialog_accepts();
    void keypad_power_button_uses_exponent_label_but_inserts_caret();
    void functions_dock_retranslates_domain_label_after_language_change();
    void main_window_applies_primary_role_to_active_editor_and_dock_selection();
    void current_result_tooltip_stays_hidden_after_escape_and_arrow_caret_move();
    void current_result_tooltip_stays_hidden_after_escape_and_mouse_caret_move();
    void current_result_tooltip_hides_when_dragging_splitters();
    void calculation_settings_dialog_matches_notation_precision_layout();
    void main_window_uses_generated_theme_surface_for_chrome_and_editor();
    void restored_session_layout_reapplies_generated_theme_surfaces();
    void dock_surfaces_use_successive_generated_shades();
    void restored_constants_dock_empty_filter_fills_header();
    void dock_scroll_corner_uses_scrollbar_track_fill();
    void dock_separator_style_uses_primary_while_hovered_or_dragged();
    void constants_dock_uses_configured_narrow_minimum_width();
    void dock_search_focus_suppresses_editor_primary_outline_across_panes();
    void dock_selection_inserts_into_active_session_pane_after_focus_transfer();
    void clicking_tab_activates_own_pane_in_nested_split_layout();
    void active_pane_survives_window_reactivation_focus_replay();
    void focused_dock_search_survives_window_reactivation_focus_replay();
    void focusing_loaded_pane_preserves_its_current_scroll_position();
    void persisting_layout_captures_visible_scroll_positions_for_all_panes();
    void switching_session_tabs_preserves_each_editor_text();
    void session_tabs_reorder_with_horizontal_drag();
    void closing_and_reopening_docks_keeps_attached_widgets();
};

void TestDisplayUi::color_scheme_roles_exclude_obsolete_scrollbar()
{
    const auto roles = ColorScheme::roleNames();
    for (const auto& roleEntry : roles) {
        QVERIFY(roleEntry.first != QStringLiteral("scrollbar"));
        QVERIFY(roleEntry.first != QStringLiteral("cursor"));
        QVERIFY(roleEntry.first != QStringLiteral("matched"));
    }

    const ColorScheme scheme = ColorScheme::fromJsonObject(QJsonObject{
        {QStringLiteral("background"), QStringLiteral("#1f3229")},
        {QStringLiteral("cursor"), QStringLiteral("#ffff00")},
        {QStringLiteral("scrollbar"), QStringLiteral("#ff00ff")},
        {QStringLiteral("matched"), QStringLiteral("#00ffff")}
    });
    QVERIFY(scheme.isValid());
    QVERIFY(!scheme.toJsonObject().contains(QStringLiteral("scrollbar")));
    QVERIFY(!scheme.toJsonObject().contains(QStringLiteral("cursor")));
    QVERIFY(!scheme.toJsonObject().contains(QStringLiteral("matched")));
}

void TestDisplayUi::theme_dialog_preserves_list_scroll_and_fills_role_color_buttons()
{
    MainWindowStateGuard guard;
    Settings* settings = Settings::instance();

    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("background"), QStringLiteral("#123456")}
    }).toJson(QJsonDocument::Compact));
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QString failure;
    int lightScrollBefore = -1;
    int lightScrollAfter = -1;
    int darkScrollBefore = -1;
    int darkScrollAfter = -1;
    QColor backgroundButtonColor;
    QColor backgroundButtonPixel;

    const auto recordFailure = [&failure](const QString& message) {
        if (failure.isEmpty())
            failure = message;
    };

    QTimer::singleShot(0, &window, [&]() {
        QDialog* dialog = window.findChild<QDialog*>(QStringLiteral("ThemeDialog"));
        if (dialog == nullptr) {
            recordFailure(QStringLiteral("Theme dialog was not found."));
            return;
        }

        const auto finishDialog = qScopeGuard([dialog]() {
            dialog->reject();
        });
        Q_UNUSED(finishDialog);

        QListWidget* lightList = dialog->findChild<QListWidget*>(QStringLiteral("LightThemeList"));
        QListWidget* darkList = dialog->findChild<QListWidget*>(QStringLiteral("DarkThemeList"));
        QPushButton* backgroundButton = dialog->findChild<QPushButton*>(
            QStringLiteral("ThemeColorButton_background"));

        if (lightList == nullptr || darkList == nullptr || backgroundButton == nullptr) {
            recordFailure(QStringLiteral("Theme dialog controls were not found."));
            return;
        }
        const auto constrainListHeight = [](QListWidget* list) {
            const int rowHeight = list->sizeHintForRow(0) > 0
                ? list->sizeHintForRow(0)
                : list->fontMetrics().height() + 6;
            list->setFixedHeight(rowHeight * 3 + list->frameWidth() * 2);
        };
        constrainListHeight(lightList);
        constrainListHeight(darkList);
        if (dialog->layout() != nullptr)
            dialog->layout()->activate();
        QCoreApplication::processEvents();

        if (lightList->verticalScrollBar()->maximum() <= 0
                || darkList->verticalScrollBar()->maximum() <= 0) {
            recordFailure(QStringLiteral("Theme lists are not scrollable: light count %1 max %2, dark count %3 max %4.")
                              .arg(lightList->count())
                              .arg(lightList->verticalScrollBar()->maximum())
                              .arg(darkList->count())
                              .arg(darkList->verticalScrollBar()->maximum()));
            return;
        }

        const auto clickVisibleTheme = [&recordFailure](QListWidget* list) {
            const QModelIndex index = list->indexAt(QPoint(list->viewport()->width() / 2,
                                                           list->viewport()->height() / 2));
            if (!index.isValid()) {
                recordFailure(QStringLiteral("No visible theme item was found for clicking."));
                return false;
            }

            QTest::mouseClick(list->viewport(),
                              Qt::LeftButton,
                              Qt::NoModifier,
                              list->visualRect(index).center());
            QCoreApplication::processEvents();
            QCoreApplication::processEvents();
            return true;
        };

        lightList->verticalScrollBar()->setValue(qMin(2, lightList->verticalScrollBar()->maximum()));
        QCoreApplication::processEvents();
        lightScrollBefore = lightList->verticalScrollBar()->value();
        if (!clickVisibleTheme(lightList))
            return;
        lightScrollAfter = lightList->verticalScrollBar()->value();

        darkList->verticalScrollBar()->setValue(qMin(2, darkList->verticalScrollBar()->maximum()));
        QCoreApplication::processEvents();
        darkScrollBefore = darkList->verticalScrollBar()->value();
        if (!clickVisibleTheme(darkList))
            return;
        darkScrollAfter = darkList->verticalScrollBar()->value();

        backgroundButtonColor = QColor(backgroundButton->text());
        const QImage buttonImage = backgroundButton->grab().toImage();
        backgroundButtonPixel = buttonImage.pixelColor(buttonImage.width() - 6,
                                                       buttonImage.height() / 2);
    });

    QVERIFY(QMetaObject::invokeMethod(&window, "showCustomThemeDialog", Qt::DirectConnection));
    QVERIFY2(failure.isEmpty(), qPrintable(failure));
    QCOMPARE(lightScrollAfter, lightScrollBefore);
    QCOMPARE(darkScrollAfter, darkScrollBefore);
    QVERIFY(colorsAreClose(backgroundButtonPixel, backgroundButtonColor, 3));
}

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
    display->setThemeInteractionColors(QColor(QStringLiteral("#333333")),
                                       QColor(QStringLiteral("#111111")),
                                       QColor(QStringLiteral("#eeeeee")),
                                       QColor(QStringLiteral("#222222")),
                                       QColor(QStringLiteral("#ffffff")));
    window.setCentralWidget(display);
    window.resize(360, 180);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(window.menuBar()->isVisible());
    QVERIFY(!contextMenuContainsMainMenu(display));
    QMenu* menu = display->createContextMenu(display->rect().center());
    QVERIFY(menu->styleSheet().contains(QStringLiteral("#111111")));
    QVERIFY(menu->styleSheet().contains(QStringLiteral("#eeeeee")));
    QVERIFY(menu->styleSheet().contains(QStringLiteral("#222222")));
    QVERIFY(menu->styleSheet().contains(QStringLiteral("#ffffff")));
    QVERIFY(menu->styleSheet().contains(QStringLiteral("border-radius: 8px")));
    delete menu;

    window.menuBar()->hide();
    QVERIFY(!window.menuBar()->isVisible());
    QVERIFY(contextMenuContainsMainMenu(display));
}

void TestDisplayUi::bitfield_selected_bit_keeps_primary_fill_while_hovered()
{
    const QColor background(QStringLiteral("#202124"));
    const QColor foreground(QStringLiteral("#d6d8dc"));
    const QColor hoverBackground(QStringLiteral("#4a5568"));
    const QColor hoverForeground(QStringLiteral("#f8fafc"));
    const QColor primaryBackground(QStringLiteral("#2f80ed"));
    const QColor primaryForeground(QStringLiteral("#ffffff"));

    BitWidget bit(3);
    bit.setThemeColors(background,
                       foreground,
                       hoverBackground,
                       hoverForeground,
                       primaryBackground,
                       primaryForeground);
    bit.resize(48, 48);
    bit.show();
    QVERIFY(QTest::qWaitForWindowExposed(&bit));

    const QPoint sampledFill(4, 4);
    QTest::mouseMove(&bit, bit.rect().center());
    QTRY_VERIFY(bit.underMouse());
    QTRY_COMPARE(bit.grab().toImage().pixelColor(sampledFill).name(),
                 hoverBackground.name());

    bit.setState(true);
    QTRY_COMPARE(bit.grab().toImage().pixelColor(sampledFill).name(),
                 primaryBackground.name());
}

void TestDisplayUi::bitfield_buttons_use_configured_generated_shades()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = QStringLiteral("{\"background\":\"#e5eee8\"}");
    settings->bitfieldVisible = true;
    settings->keypadVisible = false;

    const QVector<QColor> shades =
        generateOklchShades(QColor(QStringLiteral("#e5eee8")), 6, ThemePolarity::Light);
    const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(shades);
    const QColor buttonFill = shades.at(UiConfig::BitfieldButtonFillShade);
    const QColor buttonForeground = foregrounds.at(UiConfig::BitfieldButtonFillShade);
    const QColor buttonHoverFill = shades.at(UiConfig::BitfieldButtonHoverFillShade);
    const QColor buttonHoverForeground =
        foregrounds.at(UiConfig::BitfieldButtonHoverFillShade);
    const QColor buttonPressedFill = shades.at(UiConfig::BitfieldButtonPressedFillShade);
    const QColor buttonPressedForeground =
        foregrounds.at(UiConfig::BitfieldButtonPressedFillShade);

    MainWindow window;
    window.show();
    QCoreApplication::processEvents();

    BitFieldWidget* bitfield = window.findChild<BitFieldWidget*>();
    QVERIFY(bitfield != nullptr);
    const QList<QPushButton*> buttons = bitfield->findChildren<QPushButton*>();
    QCOMPARE(buttons.size(), 4);

    for (QPushButton* button : buttons) {
        const QString style = button->styleSheet();
        QCOMPARE(button->palette().color(QPalette::Button).name(), buttonFill.name());
        QCOMPARE(button->palette().color(QPalette::ButtonText).name(),
                 buttonForeground.name());
        QVERIFY(style.contains(QStringLiteral("background-color: %1")
                                   .arg(buttonFill.name())));
        QVERIFY(style.contains(QStringLiteral("color: %1").arg(buttonForeground.name())));
        QVERIFY(style.contains(QStringLiteral("background-color: %1")
                                   .arg(buttonHoverFill.name())));
        QVERIFY(style.contains(QStringLiteral("color: %1").arg(buttonHoverForeground.name())));
        QVERIFY(style.contains(QStringLiteral("background-color: %1")
                                   .arg(buttonPressedFill.name())));
        QVERIFY(style.contains(QStringLiteral("color: %1")
                                   .arg(buttonPressedForeground.name())));
    }
}

void TestDisplayUi::dock_list_selected_row_keeps_primary_fill_while_hovered()
{
    const QColor background(QStringLiteral("#202124"));
    const QColor foreground(QStringLiteral("#d6d8dc"));
    const QColor hoverBackground(QStringLiteral("#4a5568"));
    const QColor hoverForeground(QStringLiteral("#f8fafc"));
    const QColor primaryBackground(QStringLiteral("#2f80ed"));
    const QColor primaryForeground(QStringLiteral("#ffffff"));
    const QColor inactiveBackground(QStringLiteral("#3b4252"));
    const QColor inactiveForeground(QStringLiteral("#eceff4"));

    QTreeWidget table;
    table.setColumnCount(3);
    table.setRootIsDecorated(false);
    table.setSelectionBehavior(QAbstractItemView::SelectRows);
    table.header()->hide();
    DockListStyle::apply(&table);
    table.setProperty("dockListHoverBackground", hoverBackground);
    table.setProperty("dockListHoverForeground", hoverForeground);
    table.setProperty("dockListActiveSelectionBackground", primaryBackground);
    table.setProperty("dockListActiveSelectionForeground", primaryForeground);
    table.setProperty("dockListInactiveSelectionBackground", inactiveBackground);
    table.setProperty("dockListInactiveSelectionForeground", inactiveForeground);
    table.setStyleSheet(QStringLiteral(
        "QAbstractItemView { background-color: %1; color: %2; border: 0; }")
                            .arg(background.name(),
                                 foreground.name()));

    auto* item = new QTreeWidgetItem(&table, QStringList{
        QStringLiteral("x"),
        QStringLiteral("42"),
        QStringLiteral("m")
    });
    table.setColumnWidth(0, 70);
    table.setColumnWidth(1, 70);
    table.setColumnWidth(2, 70);
    table.resize(260, 80);
    table.show();
    QVERIFY(QTest::qWaitForWindowExposed(&table));

    const QModelIndex index = table.indexFromItem(item, 0);
    const QModelIndex secondColumnIndex = table.indexFromItem(item, 1);
    const QRect itemRect = table.visualRect(index);
    const QRect secondColumnRect = table.visualRect(secondColumnIndex);
    QVERIFY(itemRect.isValid());
    QVERIFY(secondColumnRect.isValid());
    const QPoint sampledFill(itemRect.right() - 4, itemRect.center().y());
    const QPoint sampledColumnBoundary(secondColumnRect.left() + 1, itemRect.top() + 2);
    const QPoint sampledRoundedCorner(itemRect.left() + 1, itemRect.top() + 1);

    QTest::mouseMove(table.viewport(), itemRect.center());
    QTRY_COMPARE(table.property("dockListHoveredRow").toInt(), 0);
    QImage hoveredImage = table.viewport()->grab().toImage();
    QVERIFY2(colorsAreClose(hoveredImage.pixelColor(sampledFill), hoverBackground, 24),
             qPrintable(QStringLiteral("hover sample is %1, expected %2")
                            .arg(hoveredImage.pixelColor(sampledFill).name(),
                                 hoverBackground.name())));
    QVERIFY2(colorsAreClose(hoveredImage.pixelColor(sampledColumnBoundary), hoverBackground, 32),
             qPrintable(QStringLiteral("column-boundary hover sample is %1, expected %2")
                            .arg(hoveredImage.pixelColor(sampledColumnBoundary).name(),
                                 hoverBackground.name())));
    QVERIFY(!colorsAreClose(hoveredImage.pixelColor(sampledRoundedCorner), hoverBackground, 8));
    QVERIFY(hoveredImage.pixelColor(sampledFill).name() != primaryBackground.name());

    table.setCurrentItem(item);
    item->setSelected(true);
    table.setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(table.hasFocus());
    QTRY_COMPARE(table.viewport()->grab().toImage().pixelColor(sampledFill).name(),
                 primaryBackground.name());
}

void TestDisplayUi::custom_keypad_action_stays_checked_after_dialog_accepts()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        Settings::KeypadMode oldKeypadMode;
        bool oldKeypadVisible;
        Settings::CustomKeypad oldCustomKeypad;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->keypadMode = oldKeypadMode;
            settings->keypadVisible = oldKeypadVisible;
            settings->customKeypad = oldCustomKeypad;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->keypadMode,
        settings->keypadVisible,
        settings->customKeypad,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->keypadVisible = true;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QAction* basicAction = keypadModeAction(&window, Settings::KeypadModeBasicWide);
    QAction* customAction = keypadModeAction(&window, Settings::KeypadModeCustom);
    QVERIFY(basicAction != nullptr);
    QVERIFY(customAction != nullptr);
    QVERIFY(basicAction->isChecked());

    QTimer::singleShot(0, &window, []() {
        QDialog* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog != nullptr)
            dialog->accept();
    });
    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "setKeypadMode",
                                      Qt::DirectConnection,
                                      Q_ARG(QAction*, customAction)));
    QCoreApplication::processEvents();

    QCOMPARE(settings->keypadMode, Settings::KeypadModeCustom);
    QVERIFY(customAction->isChecked());
    QVERIFY(!basicAction->isChecked());
}

void TestDisplayUi::keypad_power_button_uses_exponent_label_but_inserts_caret()
{
    const QString powerLabel = QString::fromUtf8("xʸ");

    for (const Keypad::LayoutMode layoutMode : {
             Keypad::LayoutModeScientificWide,
             Keypad::LayoutModeScientificNarrow
         }) {
        const QList<Keypad::CustomButtonDescription> presetButtons =
            Keypad::presetCustomButtons(layoutMode, QLatin1Char('.'));
        bool foundPowerButton = false;
        for (const auto& button : presetButtons) {
            QVERIFY(button.label != QStringLiteral("^"));
            if (button.label != powerLabel)
                continue;

            foundPowerButton = true;
            QCOMPARE(button.action, int(Settings::CustomKeypadActionInsertText));
            QCOMPARE(button.text, QStringLiteral("^"));
        }
        QVERIFY(foundPowerButton);
    }

    MainWindowStateGuard guard;
    Settings* settings = Settings::instance();
    settings->keypadMode = Settings::KeypadModeScientificWide;
    settings->keypadVisible = true;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    Keypad* keypad = window.findChild<Keypad*>();
    QVERIFY(keypad != nullptr);
    QPushButton* powerButton = keypadButtonWithText(keypad, powerLabel);
    QVERIFY(powerButton != nullptr);
    QCOMPARE(keypadButtonWithText(keypad, QStringLiteral("^")), nullptr);

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setText(QStringLiteral("2"));
    editor->setCursorPosition(editor->text().size());

    QTest::mouseClick(powerButton, Qt::LeftButton);
    QTRY_COMPARE(editor->text(), QStringLiteral("2^"));
}

void TestDisplayUi::functions_dock_retranslates_domain_label_after_language_change()
{
    FunctionsWidget widget;
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    QLabel* domainLabel = nullptr;
    for (QLabel* label : widget.findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("Domain")) {
            domainLabel = label;
            break;
        }
    }
    QVERIFY(domainLabel != nullptr);

    FunctionsTestTranslator translator;
    QCoreApplication::installTranslator(&translator);
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&widget, &languageChange);

    QCOMPARE(domainLabel->text(), QStringLiteral("Translated Domain"));

    QCoreApplication::removeTranslator(&translator);
}

void TestDisplayUi::main_window_applies_primary_role_to_active_editor_and_dock_selection()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
        bool oldConstantsDockVisible;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->constantsDockVisible,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    const QColor base(QStringLiteral("#1f3229"));
    const QColor generatedPrimary = generatePrimaryFromBackground(base);
    const QVector<QColor> shades = generateOklchShades(base, 6, ThemePolarity::Dark);
    const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(shades);
    QVERIFY(generatedPrimary.isValid());

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("background"), base.name()}
    }).toJson(QJsonDocument::Compact));
    settings->constantsDockVisible = true;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    QCOMPARE(editor->palette().color(QPalette::Text).name(), generatedPrimary.name());
    QVERIFY(editor->palette().color(QPalette::Base).name() != generatedPrimary.name());
    QTRY_VERIFY(editor->styleSheet().contains(QStringLiteral("color: %1;").arg(generatedPrimary.name())));
    QTRY_VERIFY(editorHasPrimaryOutline(editor, generatedPrimary));
    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_VERIFY(editorHasPrimaryOutline(editor, generatedPrimary));

    QDockWidget* constantsDock = window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QTreeWidget* table = constantsDock->findChild<QTreeWidget*>();
    QVERIFY(table != nullptr);
    QCOMPARE(table->property("dockListActiveSelectionBackground").value<QColor>().name(),
             generatedPrimary.name());
    QCOMPARE(table->property("dockListActiveSelectionForeground").value<QColor>().name(),
             aaForegroundForBackground(generatedPrimary).name());
    QCOMPARE(table->property("dockListInactiveSelectionBackground").value<QColor>().name(),
             shades.at(4).name());
    QCOMPARE(table->property("dockListInactiveSelectionForeground").value<QColor>().name(),
             foregrounds.at(4).name());

    settings->customColorSchemeJson = QString::fromUtf8(QJsonDocument(QJsonObject{
        {QStringLiteral("background"), base.name()}
    }).toJson(QJsonDocument::Compact));
    window.colorSchemeChanged();
    QCoreApplication::processEvents();

    QCOMPARE(editor->palette().color(QPalette::Text).name(), generatedPrimary.name());
    QVERIFY(editor->styleSheet().contains(QStringLiteral("color: %1;").arg(generatedPrimary.name())));
    QCOMPARE(table->property("dockListActiveSelectionBackground").value<QColor>().name(),
             generatedPrimary.name());
    QCOMPARE(table->property("dockListInactiveSelectionBackground").value<QColor>().name(),
             shades.at(4).name());
}

void TestDisplayUi::current_result_tooltip_stays_hidden_after_escape_and_arrow_caret_move()
{
    MainWindowStateGuard guard;
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setFocus();
    editor->setText(QStringLiteral("1+24"));
    editor->setCursorPosition(editor->text().size());
    editor->refreshAutoCalc();
    QTRY_VERIFY(visibleResultPreviewText(window).contains(QStringLiteral("Current result:")));

    QTest::keyClick(editor, Qt::Key_Escape);
    QTRY_VERIFY(visibleResultPreviewText(window).isEmpty());

    QTest::keyClick(editor, Qt::Key_Left);

    QTRY_VERIFY2(visibleResultPreviewText(window).isEmpty(),
                 qPrintable(QStringLiteral("Caret movement should not reopen the result tooltip, got: %1")
                                .arg(visibleResultPreviewText(window))));
}

void TestDisplayUi::current_result_tooltip_stays_hidden_after_escape_and_mouse_caret_move()
{
    MainWindowStateGuard guard;
    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setFocus();
    editor->setText(QStringLiteral("1+24"));
    editor->setCursorPosition(editor->text().size());
    editor->refreshAutoCalc();
    QTRY_VERIFY(visibleResultPreviewText(window).contains(QStringLiteral("Current result:")));

    QTest::keyClick(editor, Qt::Key_Escape);
    QTRY_VERIFY(visibleResultPreviewText(window).isEmpty());

    QTextCursor cursor = editor->textCursor();
    cursor.setPosition(1);
    const QPoint clickPosition = editor->cursorRect(cursor).center();
    QTest::mouseClick(editor->viewport(), Qt::LeftButton, Qt::NoModifier, clickPosition);

    QTRY_VERIFY2(visibleResultPreviewText(window).isEmpty(),
                 qPrintable(QStringLiteral("Mouse caret movement should not reopen the result tooltip, got: %1")
                                .arg(visibleResultPreviewText(window))));
}

void TestDisplayUi::current_result_tooltip_hides_when_dragging_splitters()
{
    MainWindowStateGuard guard;
    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setFocus();
    editor->setText(QStringLiteral("1+24"));
    editor->setCursorPosition(editor->text().size());
    editor->refreshAutoCalc();
    QTRY_VERIFY(visibleResultPreviewText(window).contains(QStringLiteral("Current result:")));

    QSplitter* splitContainer =
        window.findChild<QSplitter*>(QStringLiteral("MainSplitContainer"));
    QVERIFY(splitContainer != nullptr);
    QVERIFY(splitContainer->count() > 1);
    QSplitterHandle* handle = splitContainer->handle(1);
    QVERIFY(handle != nullptr);
    const QPoint handleCenter = handle->rect().center();
    QTest::mousePress(handle, Qt::LeftButton, Qt::NoModifier, handleCenter);
    QTest::mouseMove(handle, handleCenter + QPoint(8, 0));
    QTest::mouseRelease(handle, Qt::LeftButton, Qt::NoModifier, handleCenter + QPoint(8, 0));
    QTRY_VERIFY(visibleResultPreviewText(window).isEmpty());

    editor->setText(QStringLiteral("1+25"));
    editor->setCursorPosition(editor->text().size());
    editor->refreshAutoCalc();
    QTRY_VERIFY(visibleResultPreviewText(window).contains(QStringLiteral("Current result:")));

    window.setCursor(Qt::SplitHCursor);
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, window.rect().center());
    QTest::mouseMove(&window, window.rect().center() + QPoint(8, 0));
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier,
                        window.rect().center() + QPoint(8, 0));
    window.unsetCursor();
    QTRY_VERIFY(visibleResultPreviewText(window).isEmpty());
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

void TestDisplayUi::main_window_uses_generated_theme_surface_for_chrome_and_editor()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
        Settings::KeypadMode oldKeypadMode;
        bool oldKeypadVisible;
        bool oldStatusBarVisible;
        bool oldBitfieldVisible;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
            settings->keypadMode = oldKeypadMode;
            settings->keypadVisible = oldKeypadVisible;
            settings->statusBarVisible = oldStatusBarVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->keypadMode,
        settings->keypadVisible,
        settings->statusBarVisible,
        settings->bitfieldVisible,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->statusBarVisible = true;
    settings->bitfieldVisible = true;
    settings->hasNumberFormatStyleSetting = true;

    const auto verifyTheme = [settings](const QString& baseName, ThemePolarity polarity) {
        QJsonObject colors;
        colors.insert(QStringLiteral("background"), baseName);
        settings->customColorSchemeJson =
            QString::fromUtf8(QJsonDocument(colors).toJson(QJsonDocument::Compact));
        settings->keypadVisible = true;
        settings->bitfieldVisible = true;

        const QColor base(baseName);
        const QVector<QColor> shades = generateOklchShades(base, 6, polarity);
        const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(shades);
        const QColor expectedResultSurface = shades.at(UiConfig::ResultDisplayShade);
        const QColor expectedWindowSurface = shades.at(UiConfig::WindowBackgroundShade);
        const QColor expectedKeypadSurface = shades.at(UiConfig::KeypadBackgroundShade);
        const QColor expectedEditorSurface = shades.at(UiConfig::DockBackgroundShade);
        const QColor expectedHeaderSurface = shades.at(UiConfig::DockHeaderShade);
        const QColor expectedInputSurface = shades.at(UiConfig::DockUnfocusedSelectedItemShade);
        const QColor expectedKeypadButtonSurface = shades.at(UiConfig::KeypadButtonShade);
        const QColor expectedBitfieldButtonSurface =
            shades.at(UiConfig::BitfieldButtonFillShade);
        const QColor expectedBitfieldButtonHoverSurface =
            shades.at(UiConfig::BitfieldButtonHoverFillShade);
        const QColor expectedBitfieldButtonPressedSurface =
            shades.at(UiConfig::BitfieldButtonPressedFillShade);
        const QColor expectedStatusBarSurface = shades.at(UiConfig::StatusBarBackgroundShade);
        const QColor expectedPrimary = generatePrimaryFromBackground(base);
        const QColor expectedWindowForeground = foregrounds.at(UiConfig::WindowBackgroundShade);
        const QColor expectedKeypadForeground = foregrounds.at(UiConfig::KeypadBackgroundShade);
        const QColor expectedEditorForeground = foregrounds.at(UiConfig::DockBackgroundShade);
        const QColor expectedHeaderForeground = foregrounds.at(UiConfig::DockHeaderShade);
        const QColor expectedInputForeground =
            foregrounds.at(UiConfig::DockUnfocusedSelectedItemShade);
        const QColor expectedKeypadButtonForeground = foregrounds.at(UiConfig::KeypadButtonShade);
        const QColor expectedBitfieldButtonForeground =
            foregrounds.at(UiConfig::BitfieldButtonFillShade);
        const QColor expectedBitfieldButtonHoverForeground =
            foregrounds.at(UiConfig::BitfieldButtonHoverFillShade);
        const QColor expectedBitfieldButtonPressedForeground =
            foregrounds.at(UiConfig::BitfieldButtonPressedFillShade);

        MainWindow window;
        window.show();
        QCoreApplication::processEvents();

        QCOMPARE(window.palette().color(QPalette::Window).name(), expectedWindowSurface.name());
        QCOMPARE(window.palette().color(QPalette::WindowText).name(),
                 expectedWindowForeground.name());
        QCOMPARE(window.palette().color(QPalette::ButtonText).name(),
                 expectedWindowForeground.name());

        Editor* editor = window.findChild<Editor*>();
        ResultDisplay* display = window.findChild<ResultDisplay*>();
        QSplitter* splitContainer =
            window.findChild<QSplitter*>(QStringLiteral("MainSplitContainer"));
        Keypad* keypad = window.findChild<Keypad*>();
        BitFieldWidget* bitfield = window.findChild<BitFieldWidget*>();
        BitWidget* bit = bitfield ? bitfield->findChild<BitWidget*>() : nullptr;
        QStatusBar* statusBar =
            window.findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(editor != nullptr);
        QVERIFY(display != nullptr);
        QVERIFY(splitContainer != nullptr);
        QVERIFY(keypad != nullptr);
        QVERIFY(bitfield != nullptr);
        QVERIFY(bit != nullptr);
        QVERIFY(statusBar != nullptr);
        QCOMPARE(statusBar->findChildren<QPushButton*>().size(), 3);
        QVERIFY(QMetaObject::invokeMethod(&window,
                                          "setStatusBarVisible",
                                          Qt::DirectConnection,
                                          Q_ARG(bool, true)));
        QCOMPARE(statusBar->findChildren<QPushButton*>().size(), 3);
        QWidget* pane = paneWidgetForDisplay(display);
        QWidget* page = display->parentWidget();
        QTabBar* sessionTabBar = pane ? pane->findChild<QTabBar*>() : nullptr;
        QVERIFY(pane != nullptr);
        QVERIFY(page != nullptr);
        QVERIFY(sessionTabBar != nullptr);
        QCOMPARE(splitContainer->palette().color(QPalette::Window).name(),
                 expectedWindowSurface.name());
        QCOMPARE(pane->palette().color(QPalette::Window).name(),
                 expectedResultSurface.name());
        QCOMPARE(page->palette().color(QPalette::Window).name(),
                 expectedResultSurface.name());
        QCOMPARE(page->parentWidget()->palette().color(QPalette::Window).name(),
                 expectedResultSurface.name());
        QCOMPARE(display->palette().color(QPalette::Base).name(), expectedResultSurface.name());
        QCOMPARE(display->viewport()->palette().color(QPalette::Base).name(),
                 expectedResultSurface.name());
        QVERIFY(display->styleSheet().contains(expectedResultSurface.name()));
        QVERIFY(display->viewport()->styleSheet().contains(expectedResultSurface.name()));
        const QImage displayImage = display->viewport()->grab().toImage();
        QVERIFY(!displayImage.isNull());
        QCOMPARE(displayImage.pixelColor(displayImage.width() / 2,
                                         displayImage.height() / 2).name(),
                 expectedResultSurface.name());
        sessionTabBar->show();
        QCoreApplication::processEvents();
        QVERIFY(sessionTabBar->isVisible());
        const QImage tabBarImage = sessionTabBar->grab().toImage();
        QVERIFY(!tabBarImage.isNull());
        QCOMPARE(tabBarImage.pixelColor(tabBarImage.width() - 1,
                                        tabBarImage.height() / 2).name(),
                 expectedWindowSurface.name());
        QWidget* tabBarRow = sessionTabBar->parentWidget();
        QVERIFY(tabBarRow != nullptr);
        QCOMPARE(tabBarRow->palette().color(QPalette::Window).name(),
                 expectedWindowSurface.name());
        QVERIFY(tabBarRow->styleSheet().contains(expectedWindowSurface.name()));
        QCOMPARE(editor->palette().color(QPalette::Base).name(),
                 expectedEditorSurface.name());
        QVERIFY(editor->styleSheet().contains(expectedEditorSurface.name()));
        QVERIFY(editor->styleSheet().contains(QStringLiteral("border-radius: 13px")));
        QVERIFY(editor->styleSheet().contains(QStringLiteral("padding: 10px 18px")));
        QVERIFY(editor->viewport()->styleSheet().contains(QStringLiteral("background: transparent")));
        QCOMPARE(editor->cursorWidth(), 2);
        QCOMPARE(editor->graphicsEffect(), nullptr);
        QVERIFY(editor->mask().isEmpty());
        QCOMPARE(keypad->palette().color(QPalette::Window).name(), expectedKeypadSurface.name());
        QCOMPARE(keypad->palette().color(QPalette::WindowText).name(),
                 expectedKeypadForeground.name());
        QPushButton* keypadButton = keypadButtonWithText(keypad, QStringLiteral("%"));
        QPushButton* keypadDigitButton = keypadButtonWithText(keypad, QStringLiteral("7"));
        QPushButton* keypadDecimalButton =
            keypadButtonWithText(keypad, QString(QChar(settings->radixCharacter())));
        QPushButton* keypadEvaluateButton = keypadButtonWithText(keypad, QStringLiteral("="));
        QVERIFY(keypadButton != nullptr);
        QVERIFY(keypadDigitButton != nullptr);
        QVERIFY(keypadDecimalButton != nullptr);
        QVERIFY(keypadEvaluateButton != nullptr);
        QWidget* keypadContainer = keypad->parentWidget();
        QVERIFY(keypadContainer != nullptr);
        QCOMPARE(keypadContainer->palette().color(QPalette::Window).name(),
                 expectedKeypadSurface.name());
        QVERIFY(keypadContainer->styleSheet().contains(expectedKeypadSurface.name()));
        const QImage keypadContainerImage = keypadContainer->grab().toImage();
        QVERIFY(!keypadContainerImage.isNull());
        QCOMPARE(keypadContainerImage.pixelColor(0, keypadContainerImage.height() / 2).name(),
                 expectedKeypadSurface.name());
        QCOMPARE(keypadContainerImage.pixelColor(keypadContainerImage.width() - 1,
                                                 keypadContainerImage.height() / 2).name(),
                 expectedKeypadSurface.name());
        const QString keypadButtonStyle = keypadButton->styleSheet();
        QCOMPARE(keypadButton->palette().color(QPalette::Button).name(),
                 expectedKeypadButtonSurface.name());
        QCOMPARE(keypadButton->palette().color(QPalette::ButtonText).name(),
                 expectedKeypadButtonForeground.name());
        QVERIFY(keypadButtonStyle.contains(expectedKeypadButtonSurface.name()));
        QVERIFY(keypadButtonStyle.contains(expectedKeypadButtonForeground.name()));
        QVERIFY(keypadButtonStyle.contains(expectedHeaderSurface.name()));
        QVERIFY(keypadButtonStyle.contains(expectedHeaderForeground.name()));
        QVERIFY(keypadButtonStyle.contains(expectedInputSurface.name()));
        QVERIFY(keypadButtonStyle.contains(expectedInputForeground.name()));
        QVERIFY(keypadButtonStyle.contains(QStringLiteral("border: none")));
        QVERIFY(keypadButtonStyle.contains(QStringLiteral("qlineargradient")));
        QVERIFY(keypadButtonStyle.contains(QStringLiteral("border-radius: %1px")
                                               .arg(UiConfig::KeypadButtonCornerRadius)));
        QVERIFY(keypadButtonStyle.contains(QStringLiteral("margin: %1px")
                                               .arg(UiConfig::KeypadButtonMargin)));
        QVERIFY2(keypadButtonStyle.contains(QStringLiteral("padding: %1px")
                                                .arg(UiConfig::KeypadButtonPadding)),
                 qPrintable(keypadButtonStyle));
        QVERIFY(keypad->layout() != nullptr);
        QCOMPARE(keypad->layout()->contentsMargins(),
                 QMargins(UiConfig::KeypadButtonMargin,
                          UiConfig::KeypadButtonMargin,
                          UiConfig::KeypadButtonMargin,
                          UiConfig::KeypadButtonMargin));
        const QColor expectedDigitSurface =
            keypadPrimaryHueFillForTest(
                expectedPrimary,
                expectedKeypadButtonSurface,
                expectedKeypadButtonSurface,
                UiConfig::KeypadDigitPrimaryHueChromaPercent);
        const QColor expectedDigitForeground = aaForegroundForBackground(expectedDigitSurface);
        for (QPushButton* digitButton : {keypadDigitButton, keypadDecimalButton}) {
            const QString digitStyle = digitButton->styleSheet();
            QCOMPARE(digitButton->palette().color(QPalette::Button).name(),
                     expectedDigitSurface.name());
            QCOMPARE(digitButton->palette().color(QPalette::ButtonText).name(),
                     expectedDigitForeground.name());
            QVERIFY(digitStyle.contains(expectedDigitSurface.name()));
            QVERIFY(digitStyle.contains(expectedDigitForeground.name()));
            QVERIFY(digitStyle.contains(QStringLiteral("qlineargradient")));
        }
        const QColor expectedOperatorSurface =
            keypadPrimaryHueFillForTest(
                expectedPrimary,
                expectedKeypadButtonSurface,
                expectedKeypadButtonSurface,
                UiConfig::KeypadOperatorPrimaryHueChromaPercent);
        const QColor expectedOperatorHoverSurface =
            keypadPrimaryHueFillForTest(
                expectedPrimary,
                expectedHeaderSurface,
                expectedKeypadButtonSurface,
                UiConfig::KeypadOperatorPrimaryHueChromaPercent);
        const QColor expectedOperatorPressedSurface =
            keypadPrimaryHueFillForTest(
                expectedPrimary,
                expectedInputSurface,
                expectedKeypadButtonSurface,
                UiConfig::KeypadOperatorPrimaryHueChromaPercent);
        const QColor expectedOperatorForeground = aaForegroundForBackground(expectedOperatorSurface);
        const QColor expectedOperatorHoverForeground =
            aaForegroundForBackground(expectedOperatorHoverSurface);
        const QColor expectedOperatorPressedForeground =
            aaForegroundForBackground(expectedOperatorPressedSurface);
        const QStringList keypadOperatorLabels = {
            QStringLiteral("+"),
            QString::fromUtf8("−"),
            QString::fromUtf8("×"),
            QString::fromUtf8("÷")
        };
        for (const QString& label : keypadOperatorLabels) {
            QPushButton* keypadOperatorButton = keypadButtonWithText(keypad, label);
            QVERIFY2(keypadOperatorButton != nullptr, qPrintable(label));
            const QString keypadOperatorStyle = keypadOperatorButton->styleSheet();
            QCOMPARE(keypadOperatorButton->palette().color(QPalette::Button).name(),
                     expectedOperatorSurface.name());
            QCOMPARE(keypadOperatorButton->palette().color(QPalette::ButtonText).name(),
                     expectedOperatorForeground.name());
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorSurface.name()));
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorForeground.name()));
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorHoverSurface.name()));
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorHoverForeground.name()));
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorPressedSurface.name()));
            QVERIFY(keypadOperatorStyle.contains(expectedOperatorPressedForeground.name()));
            QVERIFY(keypadOperatorStyle.contains(QStringLiteral("qlineargradient")));
        }
        const QColor expectedEvaluateSurface =
            keypadPrimaryHueFillForTest(
                expectedPrimary,
                expectedKeypadButtonSurface,
                expectedKeypadButtonSurface,
                UiConfig::KeypadEvaluatePrimaryHueChromaPercent);
        const QColor expectedEvaluateForeground =
            aaForegroundForBackground(expectedEvaluateSurface);
        QCOMPARE(keypadEvaluateButton->palette().color(QPalette::Button).name(),
                 expectedEvaluateSurface.name());
        QCOMPARE(keypadEvaluateButton->palette().color(QPalette::ButtonText).name(),
                 expectedEvaluateForeground.name());
        QVERIFY(keypadEvaluateButton->styleSheet().contains(expectedEvaluateSurface.name()));
        QVERIFY(keypadEvaluateButton->styleSheet().contains(expectedEvaluateForeground.name()));
        QVERIFY(keypadEvaluateButton->styleSheet().contains(QStringLiteral("qlineargradient")));
        QCOMPARE(bitfield->palette().color(QPalette::Window).name(), expectedEditorSurface.name());
        QCOMPARE(bitfield->palette().color(QPalette::Button).name(), expectedEditorSurface.name());
        QVERIFY(bit->styleSheet().contains(expectedEditorForeground.name()));
        QVERIFY(bit->styleSheet().contains(expectedEditorSurface.name()));
        QVERIFY(bit->styleSheet().contains(expectedHeaderSurface.name()));
        QVERIFY(bit->styleSheet().contains(expectedHeaderForeground.name()));
        QVERIFY(bit->styleSheet().contains(expectedPrimary.name()));
        QVERIFY(bit->styleSheet().contains(aaForegroundForBackground(expectedPrimary).name()));
        QVERIFY(bitfield->styleSheet().contains(expectedEditorForeground.name()));
        QVERIFY(bitfield->styleSheet().contains(expectedEditorSurface.name()));
        QPushButton* bitfieldButton = bitfield->findChild<QPushButton*>();
        QVERIFY(bitfieldButton != nullptr);
        QCOMPARE(bitfieldButton->palette().color(QPalette::Button).name(),
                 expectedBitfieldButtonSurface.name());
        QCOMPARE(bitfieldButton->palette().color(QPalette::ButtonText).name(),
                 expectedBitfieldButtonForeground.name());
        QVERIFY(bitfieldButton->styleSheet().contains(expectedBitfieldButtonSurface.name()));
        QVERIFY(bitfieldButton->styleSheet().contains(expectedBitfieldButtonForeground.name()));
        QVERIFY(bitfieldButton->styleSheet().contains(expectedBitfieldButtonHoverSurface.name()));
        QVERIFY(bitfieldButton->styleSheet().contains(
            expectedBitfieldButtonHoverForeground.name()));
        QVERIFY(bitfieldButton->styleSheet().contains(expectedBitfieldButtonPressedSurface.name()));
        QVERIFY(bitfieldButton->styleSheet().contains(
            expectedBitfieldButtonPressedForeground.name()));
        QCOMPARE(statusBar->palette().color(QPalette::Window).name(),
                 expectedStatusBarSurface.name());
    };

    verifyTheme(QStringLiteral("#300a24"), ThemePolarity::Dark);
    verifyTheme(QStringLiteral("#1f3229"), ThemePolarity::Dark);
    verifyTheme(QStringLiteral("#e5eee8"), ThemePolarity::Light);

    settings->customColorSchemeJson = QStringLiteral("{\"background\":\"#e5eee8\"}");
    MainWindow changedWindow;
    changedWindow.show();
    QCoreApplication::processEvents();

    settings->customColorSchemeJson = QStringLiteral("{\"background\":\"#300a24\"}");
    changedWindow.colorSchemeChanged();
    QCoreApplication::processEvents();

    const QVector<QColor> changedShades =
        generateOklchShades(QColor(QStringLiteral("#300a24")), 6, ThemePolarity::Dark);
    const QColor changedPrimary = generatePrimaryFromBackground(QColor(QStringLiteral("#300a24")));
    ResultDisplay* changedDisplay = changedWindow.findChild<ResultDisplay*>();
    Editor* changedEditor = changedWindow.findChild<Editor*>();
    BitFieldWidget* changedBitfield = changedWindow.findChild<BitFieldWidget*>();
    Keypad* changedKeypad = changedWindow.findChild<Keypad*>();
    QPushButton* changedKeypadButton =
        changedKeypad ? keypadButtonWithText(changedKeypad, QStringLiteral("%")) : nullptr;
    QPushButton* changedDigitButton =
        changedKeypad ? keypadButtonWithText(changedKeypad, QStringLiteral("7")) : nullptr;
    QPushButton* changedOperatorButton =
        changedKeypad ? keypadButtonWithText(changedKeypad, QStringLiteral("+")) : nullptr;
    QPushButton* changedEvaluateButton =
        changedKeypad ? keypadButtonWithText(changedKeypad, QStringLiteral("=")) : nullptr;
    QVERIFY(changedDisplay != nullptr);
    QVERIFY(changedEditor != nullptr);
    QVERIFY(changedBitfield != nullptr);
    QVERIFY(changedKeypad != nullptr);
    QVERIFY(changedKeypadButton != nullptr);
    QVERIFY(changedDigitButton != nullptr);
    QVERIFY(changedOperatorButton != nullptr);
    QVERIFY(changedEvaluateButton != nullptr);
    QCOMPARE(changedWindow.palette().color(QPalette::Window).name(),
             changedShades.at(UiConfig::WindowBackgroundShade).name());
    QCOMPARE(changedDisplay->palette().color(QPalette::Base).name(),
             changedShades.at(UiConfig::ResultDisplayShade).name());
    QCOMPARE(changedEditor->viewport()->palette().color(QPalette::Base).name(),
             changedShades.at(UiConfig::DockBackgroundShade).name());
    QCOMPARE(changedEditor->parentWidget()->palette().color(QPalette::Window).name(),
             changedShades.at(UiConfig::ResultDisplayShade).name());
    QCOMPARE(changedBitfield->palette().color(QPalette::Window).name(),
             changedShades.at(UiConfig::DockBackgroundShade).name());
    QCOMPARE(changedKeypadButton->palette().color(QPalette::Button).name(),
             changedShades.at(UiConfig::KeypadButtonShade).name());
    QVERIFY(changedKeypadButton->styleSheet().contains(
        changedShades.at(UiConfig::KeypadButtonShade).name()));
    const QColor changedDigitSurface = keypadPrimaryHueFillForTest(
        changedPrimary,
        changedShades.at(UiConfig::KeypadButtonShade),
        changedShades.at(UiConfig::KeypadButtonShade),
        UiConfig::KeypadDigitPrimaryHueChromaPercent);
    QCOMPARE(changedDigitButton->palette().color(QPalette::Button).name(),
             changedDigitSurface.name());
    QCOMPARE(changedDigitButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedDigitSurface).name());
    QVERIFY(changedDigitButton->styleSheet().contains(changedDigitSurface.name()));
    const QColor changedOperatorSurface = keypadPrimaryHueFillForTest(
        changedPrimary,
        changedShades.at(UiConfig::KeypadButtonShade),
        changedShades.at(UiConfig::KeypadButtonShade),
        UiConfig::KeypadOperatorPrimaryHueChromaPercent);
    QCOMPARE(changedOperatorButton->palette().color(QPalette::Button).name(),
             changedOperatorSurface.name());
    QCOMPARE(changedOperatorButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedOperatorSurface).name());
    QVERIFY(changedOperatorButton->styleSheet().contains(changedOperatorSurface.name()));
    const QColor changedEvaluateSurface = keypadPrimaryHueFillForTest(
        changedPrimary,
        changedShades.at(UiConfig::KeypadButtonShade),
        changedShades.at(UiConfig::KeypadButtonShade),
        UiConfig::KeypadEvaluatePrimaryHueChromaPercent);
    QCOMPARE(changedEvaluateButton->palette().color(QPalette::Button).name(),
             changedEvaluateSurface.name());
    QCOMPARE(changedEvaluateButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedEvaluateSurface).name());
    QVERIFY(changedEvaluateButton->styleSheet().contains(changedEvaluateSurface.name()));

    QAction* scientificNarrowAction =
        keypadModeAction(&changedWindow, Settings::KeypadModeScientificNarrow);
    QVERIFY(scientificNarrowAction != nullptr);
    QVERIFY(QMetaObject::invokeMethod(&changedWindow,
                                      "setKeypadMode",
                                      Qt::DirectConnection,
                                      Q_ARG(QAction*, scientificNarrowAction)));
    QCoreApplication::processEvents();

    Keypad* switchedKeypad = changedWindow.findChild<Keypad*>();
    QPushButton* switchedDigitButton =
        switchedKeypad ? keypadButtonWithText(switchedKeypad, QStringLiteral("7")) : nullptr;
    QPushButton* switchedOperatorButton =
        switchedKeypad ? keypadButtonWithText(switchedKeypad, QStringLiteral("+")) : nullptr;
    QPushButton* switchedEvaluateButton =
        switchedKeypad ? keypadButtonWithText(switchedKeypad, QStringLiteral("=")) : nullptr;
    QVERIFY(switchedKeypad != nullptr);
    QVERIFY(switchedDigitButton != nullptr);
    QVERIFY(switchedOperatorButton != nullptr);
    QVERIFY(switchedEvaluateButton != nullptr);
    QCOMPARE(switchedDigitButton->palette().color(QPalette::Button).name(),
             changedDigitSurface.name());
    QCOMPARE(switchedDigitButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedDigitSurface).name());
    QVERIFY(switchedDigitButton->styleSheet().contains(changedDigitSurface.name()));
    QCOMPARE(switchedOperatorButton->palette().color(QPalette::Button).name(),
             changedOperatorSurface.name());
    QCOMPARE(switchedOperatorButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedOperatorSurface).name());
    QVERIFY(switchedOperatorButton->styleSheet().contains(changedOperatorSurface.name()));
    QCOMPARE(switchedEvaluateButton->palette().color(QPalette::Button).name(),
             changedEvaluateSurface.name());
    QCOMPARE(switchedEvaluateButton->palette().color(QPalette::ButtonText).name(),
             aaForegroundForBackground(changedEvaluateSurface).name());
    QVERIFY(switchedEvaluateButton->styleSheet().contains(changedEvaluateSurface.name()));

    QVERIFY(!changedKeypadButton->styleSheet().contains(
        generateOklchShades(QColor(QStringLiteral("#e5eee8")), 6, ThemePolarity::Light)
            .at(UiConfig::KeypadButtonShade)
            .name()));
    const QImage changedDisplayImage = changedDisplay->viewport()->grab().toImage();
    QVERIFY(!changedDisplayImage.isNull());
    QCOMPARE(changedDisplayImage.pixelColor(changedDisplayImage.width() / 2,
                                            changedDisplayImage.height() / 2).name(),
             QStringLiteral("#300a24"));

    if (!UiConfig::OklchThemeDebugReportEnabled)
        return;

    QFile report(QDir(QDir::tempPath()).absoluteFilePath(
        QStringLiteral("speedcrunch-oklch-theme-report.html")));
    QVERIFY(report.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString reportHtml = QString::fromUtf8(report.readAll());
    QVERIFY(reportHtml.contains(QStringLiteral("Runtime widget samples")));
    QVERIFY(reportHtml.contains(QStringLiteral("<code>1=#300A24</code>")));
    QVERIFY(reportHtml.contains(QStringLiteral("ResultDisplay 1")));
    QVERIFY(reportHtml.contains(QStringLiteral("<td><code>#300A24</code></td>")));
}

void TestDisplayUi::restored_session_layout_reapplies_generated_theme_surfaces()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
        QString oldSessionLayoutJson;
        Settings::KeypadMode oldKeypadMode;
        bool oldStatusBarVisible;
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
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
            settings->sessionLayoutJson = oldSessionLayoutJson;
            settings->keypadMode = oldKeypadMode;
            settings->statusBarVisible = oldStatusBarVisible;
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
        settings,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->sessionLayoutJson,
        settings->keypadMode,
        settings->statusBarVisible,
        settings->constantsDockVisible,
        settings->functionsDockVisible,
        settings->historyDockVisible,
        settings->keypadVisible,
        settings->formulaBookDockVisible,
        settings->variablesDockVisible,
        settings->userFunctionsDockVisible,
        settings->userUnitsDockVisible,
        settings->bitfieldVisible,
        settings->windowPositionSave,
        settings->hasNumberFormatStyleSetting
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = QStringLiteral("{\"background\":\"#300a24\"}");
    settings->sessionLayoutJson.clear();
    settings->keypadMode = Settings::KeypadModeBasicWide;
    settings->statusBarVisible = true;
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = true;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    {
        MainWindow sourceWindow;
        sourceWindow.show();
        QCoreApplication::processEvents();
        QVERIFY(QMetaObject::invokeMethod(&sourceWindow, "splitActivePaneRight",
                                          Qt::DirectConnection));
        QCoreApplication::processEvents();
        QTRY_COMPARE(sourceWindow.findChildren<ResultDisplay*>().size(), 2);
        sourceWindow.persistSessionAndSettingsForShutdown();
    }

    const QString restoredLayout = settings->sessionLayoutJson;
    QVERIFY(!restoredLayout.isEmpty());

    MainWindow restoredWindow;
    restoredWindow.show();
    QCoreApplication::processEvents();
    QTRY_COMPARE(restoredWindow.findChildren<ResultDisplay*>().size(), 2);

    const QVector<QColor> shades =
        generateOklchShades(QColor(QStringLiteral("#300a24")), 6, ThemePolarity::Dark);
    const QColor paneFill = shades.at(UiConfig::ResultDisplayShade);
    const QColor chromeFill = shades.at(UiConfig::WindowBackgroundShade);
    const QColor keypadFill = shades.at(UiConfig::KeypadBackgroundShade);
    const QColor editorFill = shades.at(UiConfig::DockBackgroundShade);

    QSplitter* splitContainer =
        restoredWindow.findChild<QSplitter*>(QStringLiteral("MainSplitContainer"));
    QVERIFY(splitContainer != nullptr);
    QCOMPARE(splitContainer->palette().color(QPalette::Window).name(), chromeFill.name());

    for (ResultDisplay* display : restoredWindow.findChildren<ResultDisplay*>()) {
        QCOMPARE(display->palette().color(QPalette::Base).name(), paneFill.name());
        QCOMPARE(display->viewport()->palette().color(QPalette::Base).name(),
                 paneFill.name());
        const QImage displayImage = display->viewport()->grab().toImage();
        QVERIFY(!displayImage.isNull());
        QCOMPARE(displayImage.pixelColor(displayImage.width() / 2,
                                         displayImage.height() / 2).name(),
                 paneFill.name());
        QWidget* pane = paneWidgetForDisplay(display);
        QVERIFY(pane != nullptr);
        QCOMPARE(pane->palette().color(QPalette::Window).name(), paneFill.name());
    }

    for (Editor* editor : restoredWindow.findChildren<Editor*>())
        QCOMPARE(editor->palette().color(QPalette::Base).name(), editorFill.name());

    Keypad* keypad = restoredWindow.findChild<Keypad*>();
    QVERIFY(keypad != nullptr);
    QCOMPARE(keypad->palette().color(QPalette::Window).name(), keypadFill.name());
    QWidget* keypadContainer = keypad->parentWidget();
    QVERIFY(keypadContainer != nullptr);
    QCOMPARE(keypadContainer->palette().color(QPalette::Window).name(), keypadFill.name());
}

void TestDisplayUi::dock_surfaces_use_successive_generated_shades()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
        QByteArray oldWindowState;
        bool oldConstantsDockVisible;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
            settings->windowState = oldWindowState;
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->windowState,
        settings->constantsDockVisible,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = QStringLiteral("{\"background\":\"#1f3229\"}");
    settings->windowState.clear();
    settings->constantsDockVisible = true;
    settings->hasNumberFormatStyleSetting = true;

    const QVector<QColor> shades =
        generateOklchShades(QColor(QStringLiteral("#1f3229")), 6, ThemePolarity::Dark);
    const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(shades);
    const QColor primary = generatePrimaryFromBackground(QColor(QStringLiteral("#1f3229")));

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    const QColor titleFill = shades.at(UiConfig::DockHeaderShade);
    const QColor titleText = foregrounds.at(UiConfig::DockHeaderShade);
    const QColor headerButtonFill = shades.at(UiConfig::DockHeaderButtonFillShade);
    const QColor headerButtonText = foregrounds.at(UiConfig::DockHeaderButtonFillShade);
    const QColor headerButtonHoverFill =
        shades.at(UiConfig::DockHeaderButtonHoverFillShade);
    const QColor headerButtonHoverText =
        foregrounds.at(UiConfig::DockHeaderButtonHoverFillShade);
    const QColor controlFill = shades.at(4);
    const QColor controlText = foregrounds.at(4);
    const QColor contentFill = shades.at(2);
    const QColor contentText = foregrounds.at(2);
    const QColor hoveredItemFill = shades.at(UiConfig::DockHoveredItemShade);
    const QColor hoveredItemText = foregrounds.at(UiConfig::DockHoveredItemShade);
    const int comboPopupShade = qMin(UiConfig::DockBackgroundShade + 1, UiConfig::Shade600);
    const QColor comboPopupFill = shades.at(comboPopupShade);
    const QColor comboPopupText = foregrounds.at(comboPopupShade);
    const QColor completionPopupFill = shades.at(UiConfig::CompletionPopupBackgroundShade);
    const QColor completionPopupText = foregrounds.at(UiConfig::CompletionPopupBackgroundShade);
    const QColor completionPopupOutlineFill = shades.at(UiConfig::CompletionPopupOutlineShade);
    const QColor chromeFill = shades.at(UiConfig::WindowBackgroundShade);
    const QColor chromeText = foregrounds.at(UiConfig::WindowBackgroundShade);
    const QColor resultFill = shades.at(UiConfig::ResultDisplayShade);
    const QColor resultText = foregrounds.at(UiConfig::ResultDisplayShade);
    const QColor hoverFill = shades.at(5);
    const QColor textInputOutlineFill = shades.at(UiConfig::DockTextInputOutlineShade);
    const QColor scrollToBottomOutlineFill =
        shades.at(UiConfig::ScrollToBottomButtonOutlineShade);
    const QColor splitterFill = shades.at(UiConfig::SplitterShade);

    const QList<QDockWidget*> docks = window.findChildren<QDockWidget*>();
    QVERIFY(!docks.isEmpty());
    QSplitter* splitContainer =
        window.findChild<QSplitter*>(QStringLiteral("MainSplitContainer"));
    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(splitContainer != nullptr);
    QVERIFY(display != nullptr);
    QWidget* pane = display->parentWidget();
    Editor* editor = pane ? pane->findChild<Editor*>() : nullptr;
    QVERIFY(editor != nullptr);
    QVERIFY(splitContainer->styleSheet().contains(splitterFill.name()));
    QVERIFY(splitContainer->styleSheet().contains(primary.name()));
    QVERIFY(splitContainer->styleSheet().contains(QStringLiteral("QSplitter::handle:hover")));
    QVERIFY(splitContainer->styleSheet().contains(QStringLiteral("QSplitter::handle:pressed")));
    const QList<QSplitterHandle*> splitterHandles = window.findChildren<QSplitterHandle*>();
    QVERIFY(!splitterHandles.isEmpty());
    for (QSplitterHandle* splitterHandle : splitterHandles) {
        QVERIFY(splitterHandle->styleSheet().contains(splitterFill.name()));
        QVERIFY(splitterHandle->styleSheet().contains(primary.name()));
        QVERIFY(splitterHandle->styleSheet().contains(QStringLiteral("QSplitterHandle:hover")));
        QVERIFY(splitterHandle->styleSheet().contains(QStringLiteral("QSplitterHandle:pressed")));
    }
    QCOMPARE(window.property("speedcrunchDockSeparatorNormalColor").value<QColor>(), splitterFill);
    QCOMPARE(window.property("speedcrunchDockSeparatorActiveColor").value<QColor>(), primary);
    const QString resultScrollBarStyle = display->verticalScrollBar()->styleSheet();
    QVERIFY(resultScrollBarStyle.contains(shades.at(1).name()));
    QVERIFY(resultScrollBarStyle.contains(contentFill.name()));
    QVERIFY(resultScrollBarStyle.contains(titleFill.name()));
    QVERIFY(resultScrollBarStyle.contains(controlFill.name()));
    QToolButton* scrollToBottomButton =
        display->findChild<QToolButton*>(QStringLiteral("ScrollToBottomButton"));
    QVERIFY(scrollToBottomButton != nullptr);
    QVERIFY(scrollToBottomButton->styleSheet().contains(contentFill.name()));
    QVERIFY(scrollToBottomButton->styleSheet().contains(titleFill.name()));
    QVERIFY(scrollToBottomButton->styleSheet().contains(QStringLiteral("border: %1px solid %2")
                                                            .arg(UiConfig::OutlineStrokeWidth)
                                                            .arg(scrollToBottomOutlineFill.name())));
    QVERIFY(!scrollToBottomButton->icon().isNull());
    for (QDockWidget* dock : docks) {
        QCOMPARE(dock->palette().color(QPalette::Window).name(), titleFill.name());
        QCOMPARE(dock->palette().color(QPalette::WindowText).name(), titleText.name());
        QVERIFY(dock->styleSheet().contains(titleFill.name()));
        QVERIFY(dock->styleSheet().contains(titleText.name()));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("padding: 5px 4px")));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("QDockWidget::close-button")));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("QDockWidget::float-button")));
        QVERIFY(dock->styleSheet().contains(headerButtonFill.name()));
        QVERIFY(dock->styleSheet().contains(headerButtonText.name()));
        QVERIFY(dock->styleSheet().contains(headerButtonHoverFill.name()));
        QVERIFY(dock->styleSheet().contains(headerButtonHoverText.name()));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("border: none")));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("border-radius: 9px")));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("width: 18px")));
        QVERIFY(dock->styleSheet().contains(QStringLiteral("height: 18px")));
    }

    for (QDockWidget* dock : docks) {
        QWidget* dockContent = dock->widget();
        QVERIFY(dockContent != nullptr);
        const QList<QComboBox*> comboBoxes = dockContent->findChildren<QComboBox*>();
        for (QAbstractItemView* view : dockContent->findChildren<QAbstractItemView*>()) {
            if (qobject_cast<QHeaderView*>(view))
                continue;

            bool comboPopup = false;
            for (const QComboBox* comboBox : comboBoxes)
                comboPopup = comboPopup || comboBox->view() == view || comboBox->isAncestorOf(view);
            if (comboPopup)
                continue;

            QVERIFY(view->parentWidget() != nullptr);
            QVERIFY(view->parentWidget()->layout() != nullptr);
            const QMargins viewMargins = view->parentWidget()->layout()->contentsMargins();
            if (viewMargins != QMargins(0, 0, 0, 0)) {
                const QString message = QStringLiteral("%1 in %2 has list/table margins %3,%4,%5,%6")
                    .arg(QString::fromLatin1(view->metaObject()->className()),
                         dock->objectName())
                    .arg(viewMargins.left())
                    .arg(viewMargins.top())
                    .arg(viewMargins.right())
                    .arg(viewMargins.bottom());
                QFAIL(qPrintable(message));
            }
        }
    }

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QDockWidget* functionsDock =
        window.findChild<QDockWidget*>(QStringLiteral("FunctionsDock"));
    QVERIFY(constantsDock != nullptr);
    QVERIFY(functionsDock != nullptr);
    ConstantsWidget* constantsWidget = qobject_cast<ConstantsWidget*>(constantsDock->widget());
    QVERIFY(constantsWidget != nullptr);
    bool foundCloseButton = false;
    bool foundFloatButton = false;
    for (QAbstractButton* button : constantsDock->findChildren<QAbstractButton*>()) {
        const QString name = button->objectName();
        if (name != QStringLiteral("qt_dockwidget_closebutton")
            && name != QStringLiteral("qt_dockwidget_floatbutton")) {
            continue;
        }
        foundCloseButton |= name == QStringLiteral("qt_dockwidget_closebutton");
        foundFloatButton |= name == QStringLiteral("qt_dockwidget_floatbutton");
        QCOMPARE(button->palette().color(QPalette::Button).name(), headerButtonFill.name());
        QCOMPARE(button->palette().color(QPalette::ButtonText).name(), headerButtonText.name());
        QCOMPARE(button->cursor().shape(), Qt::PointingHandCursor);
        QVERIFY(button->hasMouseTracking());
        QCOMPARE(button->minimumSize(), QSize(18, 18));
        QCOMPARE(button->maximumSize(), QSize(18, 18));
        QCOMPARE(button->iconSize(), QSize(18, 18));
        QVERIFY(button->styleSheet().contains(headerButtonFill.name()));
        QVERIFY(button->styleSheet().contains(headerButtonText.name()));
        QVERIFY(button->styleSheet().contains(headerButtonHoverFill.name()));
        QVERIFY(button->styleSheet().contains(headerButtonHoverText.name()));
        QVERIFY(button->styleSheet().contains(QStringLiteral("border-radius: 9px")));
        QVERIFY(!button->icon().isNull());
        const QImage iconImage = button->icon().pixmap(QSize(18, 18)).toImage();
        QVERIFY(!iconImage.isNull());
        QVERIFY(iconImage.pixelColor(0, 0).alpha() < 32);
        QVERIFY2(colorsAreClose(iconImage.pixelColor(2, 9), headerButtonFill, 3),
                 qPrintable(QStringLiteral("icon fill %1 expected %2")
                                .arg(iconImage.pixelColor(2, 9).name(),
                                     headerButtonFill.name())));
        const QPoint buttonCenter = button->rect().center();
        QMouseEvent buttonMoveEvent(QEvent::MouseMove,
                                    QPointF(buttonCenter),
                                    QPointF(button->mapToGlobal(buttonCenter)),
                                    Qt::NoButton,
                                    Qt::NoButton,
                                    Qt::NoModifier);
        QCoreApplication::sendEvent(button, &buttonMoveEvent);
        const QImage hoverIconImage = button->icon().pixmap(QSize(18, 18)).toImage();
        QVERIFY(!hoverIconImage.isNull());
        QVERIFY2(colorsAreClose(hoverIconImage.pixelColor(2, 9), headerButtonHoverFill, 3),
                 qPrintable(QStringLiteral("hover icon fill %1 expected %2")
                                .arg(hoverIconImage.pixelColor(2, 9).name(),
                                     headerButtonHoverFill.name())));
        QEvent leaveEvent(QEvent::Leave);
        QCoreApplication::sendEvent(button, &leaveEvent);
    }
    QVERIFY(foundCloseButton);
    QVERIFY(foundFloatButton);
    QComboBox* comboBox = constantsDock->findChild<QComboBox*>();
    QLineEdit* searchBox = constantsDock->findChild<QLineEdit*>();
    QTreeWidget* table = constantsDock->findChild<QTreeWidget*>();
    QLabel* searchLabel = nullptr;
    for (QLabel* label : constantsDock->findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("Search")) {
            searchLabel = label;
            break;
        }
    }
    QVERIFY(comboBox != nullptr);
    QVERIFY(searchBox != nullptr);
    QVERIFY(searchLabel != nullptr);
    QVERIFY(table != nullptr);
    QVERIFY(table->header() != nullptr);

    QCOMPARE(constantsDock->widget()->palette().color(QPalette::Window).name(), contentFill.name());
    QVERIFY(constantsDock->widget()->styleSheet().contains(contentFill.name()));
    QCOMPARE(comboBox->palette().color(QPalette::Button).name(), contentFill.name());
    QCOMPARE(comboBox->palette().color(QPalette::ButtonText).name(), contentText.name());
    QVERIFY(comboBox->styleSheet().contains(contentFill.name()));
    QVERIFY(comboBox->styleSheet().contains(contentText.name()));
    QVERIFY(comboBox->styleSheet().contains(QStringLiteral("padding: 4px 32px 4px 8px")));
    QVERIFY(comboBox->styleSheet().contains(QStringLiteral("width: 28px")));
    QVERIFY(comboBox->styleSheet().contains(comboPopupFill.name()));
    QVERIFY(comboBox->styleSheet().contains(comboPopupText.name()));
    QCOMPARE(comboBox->view()->palette().color(QPalette::Base).name(), comboPopupFill.name());
    QCOMPARE(comboBox->view()->palette().color(QPalette::Text).name(), comboPopupText.name());
    QVERIFY(comboBox->view()->styleSheet().contains(comboPopupFill.name()));
    QVERIFY(comboBox->view()->styleSheet().contains(comboPopupText.name()));
    QVERIFY(comboBox->styleSheet().contains(QStringLiteral("QComboBox QAbstractItemView")));
    QVERIFY(comboBox->styleSheet().contains(QStringLiteral("border: 0; outline: 0")));
    QVERIFY(comboBox->view()->styleSheet().contains(QStringLiteral("border: 0; border-radius: 8px; outline: 0")));
    QVERIFY(comboBox->view()->styleSheet().contains(QStringLiteral("QAbstractItemView::item")));
    QVERIFY(comboBox->view()->styleSheet().contains(QStringLiteral("border: 0; border-radius: 6px;")));
    QVERIFY(comboBox->view()->styleSheet().contains(hoveredItemFill.name()));
    QVERIFY(comboBox->view()->styleSheet().contains(hoveredItemText.name()));
    QVERIFY(comboBox->view()->verticalScrollBar()->styleSheet().contains(comboPopupFill.name()));
    QVERIFY(comboBox->view()->verticalScrollBar()->styleSheet().contains(controlFill.name()));
    QVERIFY(comboBox->view()->verticalScrollBar()->styleSheet().contains(hoverFill.name()));
    QVERIFY(table->header()->styleSheet().contains(contentFill.name()));
    QVERIFY(table->header()->styleSheet().contains(contentText.name()));
    QVERIFY(table->header()->styleSheet().contains(titleFill.name()));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("border: 0")));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("border-top: 1px solid")));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("border-right: 1px solid")));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("border-bottom: 1px solid")));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("QHeaderView::section:last")));
    QVERIFY(table->header()->styleSheet().contains(QStringLiteral("border-right: 0")));
    QCOMPARE(searchBox->palette().color(QPalette::Base).name(), contentFill.name());
    QCOMPARE(searchBox->palette().color(QPalette::Text).name(), contentText.name());
    ThemedLineEdit* themedSearchBox = dynamic_cast<ThemedLineEdit*>(searchBox);
    QVERIFY(themedSearchBox != nullptr);
    QCOMPARE(themedSearchBox->cursorColor().name(), primary.name());
    const QString focusRingBorderTemplate = QStringLiteral("border: %1px solid %2");
    QVERIFY(searchBox->styleSheet().contains(focusRingBorderTemplate
                                                 .arg(UiConfig::DockTextInputUnfocusedOutlineStrokeWidth)
                                                 .arg(textInputOutlineFill.name())));
    QVERIFY(searchBox->styleSheet().contains(QStringLiteral("QLineEdit:focus")));
    QVERIFY(searchBox->styleSheet().contains(focusRingBorderTemplate
                                                 .arg(UiConfig::OutlineStrokeWidth)
                                                 .arg(primary.name())));
    QVERIFY(searchBox->styleSheet().contains(primary.name()));
    QVERIFY(searchBox->property("speedcrunchDockTextInput").toBool());
    QTRY_VERIFY(editorHasPrimaryOutline(editor, primary));
    Editor inactiveEditor;
    inactiveEditor.setThemePrimaryColor(primary, true);
    QVERIFY(inactiveEditor.styleSheet().contains(focusRingBorderTemplate
                                                     .arg(UiConfig::OutlineStrokeWidth)
                                                     .arg(primary.name())));
    inactiveEditor.setThemePrimaryColor(primary, false);
    QVERIFY(inactiveEditor.styleSheet().contains(QStringLiteral("color: %1;").arg(primary.name())));
    QVERIFY2(!inactiveEditor.styleSheet().contains(focusRingBorderTemplate
                                                       .arg(UiConfig::OutlineStrokeWidth)
                                                       .arg(primary.name())),
             qPrintable(inactiveEditor.styleSheet()));
    QCOMPARE(editor->cursorColor().name(), primary.name());
    editor->setText(QStringLiteral("123"));
    editor->setCursorPosition(editor->text().size());
    QCoreApplication::processEvents();
    const QRect editorNativeCursorRect = editor->cursorRect();
    QVERIFY(editorNativeCursorRect.isValid());
    QVERIFY(editorNativeCursorRect.height() > 0);
    const QRect editorCursorRect(
        editorNativeCursorRect.x() + (editorNativeCursorRect.width() - 2) / 2,
        editorNativeCursorRect.y(),
        2,
        editorNativeCursorRect.height());
    const QImage focusedEditorImage = editor->viewport()->grab().toImage();
    QVERIFY(focusedEditorImage.rect().contains(editorCursorRect.center()));
    for (int x = editorCursorRect.left(); x <= editorCursorRect.right(); ++x)
        QCOMPARE(focusedEditorImage.pixelColor(x, editorCursorRect.center().y()).name(), primary.name());
    const int editorBlinkTimeout = qMax(1000, QApplication::cursorFlashTime() + 250);
    const auto editorCursorPixelName = [&]() {
        return editor->viewport()->grab().toImage().pixelColor(editorCursorRect.center()).name();
    };
    QTRY_VERIFY_WITH_TIMEOUT(editorCursorPixelName() != primary.name(), editorBlinkTimeout);
    QTRY_COMPARE_WITH_TIMEOUT(editorCursorPixelName(), primary.name(), editorBlinkTimeout);
    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "handleApplicationFocusChanged",
                                      Qt::DirectConnection,
                                      Q_ARG(QWidget*, editor),
                                      Q_ARG(QWidget*, searchBox)));
    constantsDock->show();
    constantsDock->raise();
    constantsDock->setMinimumWidth(UiConfig::ConstantsDockDefaultWidth);
    window.resizeDocks(QList<QDockWidget*> { constantsDock },
                       QList<int> { UiConfig::ConstantsDockDefaultWidth },
                       Qt::Horizontal);
    QCoreApplication::processEvents();
    const auto visibleComboTextPixelCount = [&]() {
        const QImage image = comboBox->grab().toImage();
        int textPixels = 0;
        for (int y = 4; y < image.height() - 4; ++y) {
            for (int x = 8; x < image.width() - 40; ++x) {
                if (colorsAreClose(image.pixelColor(x, y), contentText, 42))
                    ++textPixels;
            }
        }
        return textPixels;
    };
    QTRY_VERIFY2(visibleComboTextPixelCount() > 8,
                 qPrintable(QStringLiteral("visible text pixels=%1 width=%2")
                                .arg(visibleComboTextPixelCount())
                                .arg(comboBox->width())));
    searchBox->clear();
    QVERIFY2(searchBox->focusPolicy() != Qt::NoFocus,
             qPrintable(QStringLiteral("focusPolicy=%1").arg(int(searchBox->focusPolicy()))));
    editor->setFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(editor->hasFocus());
    QTest::mouseClick(searchBox, Qt::LeftButton);
    QTRY_VERIFY(searchBox->hasFocus());
    QCOMPARE(themedSearchBox->cursorColor().name(), primary.name());
    QTest::keyClicks(searchBox, "2323123das23");
    QCOMPARE(searchBox->text(), QStringLiteral("2323123das23"));
    const QRect nativeCursorRect =
        themedSearchBox->inputMethodQuery(Qt::ImCursorRectangle).toRect();
    QVERIFY(nativeCursorRect.isValid());
    QVERIFY(nativeCursorRect.height() > 0);
    const QRect cursorRect(nativeCursorRect.x() + (nativeCursorRect.width() - 2) / 2 + 1,
                           nativeCursorRect.y(),
                           2,
                           nativeCursorRect.height());
    const QImage focusedSearchImage = themedSearchBox->grab().toImage();
    QVERIFY(focusedSearchImage.rect().contains(cursorRect.center()));
    for (int x = cursorRect.left(); x <= cursorRect.right(); ++x)
        QCOMPARE(focusedSearchImage.pixelColor(x, cursorRect.center().y()).name(), primary.name());
    const QPoint leftOfCursor(cursorRect.left() - 1, cursorRect.center().y());
    const QPoint rightOfCursor(cursorRect.right() + 1, cursorRect.center().y());
    if (focusedSearchImage.rect().contains(leftOfCursor))
        QVERIFY(focusedSearchImage.pixelColor(leftOfCursor).name() != primary.name());
    if (focusedSearchImage.rect().contains(rightOfCursor))
        QVERIFY(focusedSearchImage.pixelColor(rightOfCursor).name() != primary.name());
    const int blinkTimeout = qMax(1000, QApplication::cursorFlashTime() + 250);
    const auto cursorPixelName = [&]() {
        return themedSearchBox->grab().toImage().pixelColor(cursorRect.center()).name();
    };
    QTRY_VERIFY_WITH_TIMEOUT(cursorPixelName() != primary.name(), blinkTimeout);
    QTRY_COMPARE_WITH_TIMEOUT(cursorPixelName(), primary.name(), blinkTimeout);
    const QImage visibleAgainSearchImage = themedSearchBox->grab().toImage();
    if (visibleAgainSearchImage.rect().contains(leftOfCursor))
        QVERIFY(visibleAgainSearchImage.pixelColor(leftOfCursor).name() != primary.name());
    if (visibleAgainSearchImage.rect().contains(rightOfCursor))
        QVERIFY(visibleAgainSearchImage.pixelColor(rightOfCursor).name() != primary.name());
    searchBox->clear();
    QTest::keyClicks(searchBox, "mol");
    QCOMPARE(searchBox->text(), QStringLiteral("mol"));
    searchBox->clear();
    comboBox->showPopup();
    QTRY_VERIFY(comboBox->view()->isVisible());
    QCOMPARE(comboBox->view()->frameShape(), QFrame::NoFrame);
    QWidget* comboPopupChrome = comboBox->view()->window();
    if (comboPopupChrome == comboBox->window())
        comboPopupChrome = comboBox->view();
    QVERIFY(comboPopupChrome != nullptr);
    QTRY_VERIFY(!comboPopupChrome->mask().isEmpty());
    comboBox->hidePopup();
    QTRY_VERIFY(!comboBox->view()->isVisible());
    QCOMPARE(searchLabel->palette().color(QPalette::Window).name(), contentFill.name());
    QCOMPARE(searchLabel->palette().color(QPalette::WindowText).name(), contentText.name());
    QVERIFY(searchLabel->styleSheet().contains(contentFill.name()));
    QVERIFY(searchLabel->styleSheet().contains(contentText.name()));
    QCOMPARE(table->palette().color(QPalette::Base).name(), contentFill.name());
    QCOMPARE(table->palette().color(QPalette::Text).name(), contentText.name());
    QCOMPARE(table->property("dockListHoverBackground").value<QColor>().name(),
             hoveredItemFill.name());
    QCOMPARE(table->property("dockListHoverForeground").value<QColor>().name(),
             hoveredItemText.name());
    QCOMPARE(table->viewport()->palette().color(QPalette::Base).name(), contentFill.name());
    QVERIFY(table->styleSheet().contains(QStringLiteral("padding: 6px 8px")));
    QVERIFY(table->styleSheet().contains(QStringLiteral("border: 0")));
    QCOMPARE(table->frameShape(), QFrame::NoFrame);
    QCOMPARE(table->property("dockListInactiveSelectionBackground").value<QColor>().name(),
             controlFill.name());
    QCOMPARE(table->property("dockListInactiveSelectionForeground").value<QColor>().name(),
             controlText.name());
    const QString tableScrollBarStyle = table->verticalScrollBar()->styleSheet();
    QVERIFY(tableScrollBarStyle.contains(contentFill.name()));
    QVERIFY(tableScrollBarStyle.contains(titleFill.name()));
    QVERIFY(tableScrollBarStyle.contains(controlFill.name()));
    QVERIFY(tableScrollBarStyle.contains(hoverFill.name()));
    QTRY_VERIFY(table->topLevelItemCount() > 0);
    QTreeWidgetItem* tooltipItem = table->topLevelItem(0);
    QVERIFY(tooltipItem != nullptr);
    table->scrollToItem(tooltipItem);
    QCoreApplication::processEvents();
    const QRect tooltipRect = table->visualItemRect(tooltipItem);
    QVERIFY(tooltipRect.isValid());
    const QPoint tooltipPos = tooltipRect.center();
    QHelpEvent tooltipEvent(QEvent::ToolTip,
                            tooltipPos,
                            table->viewport()->mapToGlobal(tooltipPos));
    QCoreApplication::sendEvent(table->viewport(), &tooltipEvent);
    QFrame* summaryPopup =
        constantsWidget->findChild<QFrame*>(QStringLiteral("constantsSummaryPopup"));
    QTRY_VERIFY(summaryPopup != nullptr && summaryPopup->isVisible());
    QLabel* summaryPopupLabel =
        summaryPopup->findChild<QLabel*>(QStringLiteral("constantsSummaryPopupLabel"));
    QVERIFY(summaryPopupLabel != nullptr);
    QCOMPARE(summaryPopup->palette().color(QPalette::Window).name(),
             completionPopupFill.name());
    QCOMPARE(summaryPopup->palette().color(QPalette::WindowText).name(),
             completionPopupText.name());
    QCOMPARE(summaryPopupLabel->palette().color(QPalette::WindowText).name(),
             completionPopupText.name());
    QVERIFY(summaryPopup->styleSheet().contains(completionPopupFill.name()));
    QVERIFY(summaryPopup->styleSheet().contains(completionPopupText.name()));
    QVERIFY(summaryPopup->styleSheet().contains(
        QStringLiteral("border: %1px solid %2")
            .arg(UiConfig::OutlineStrokeWidth)
            .arg(completionPopupOutlineFill.name())));
    QVERIFY(summaryPopup->styleSheet().contains(
        QStringLiteral("border-radius: %1px")
            .arg(UiConfig::CompletionPopupCornerRadius)));
    QVERIFY(!summaryPopup->mask().isEmpty());
    summaryPopup->hide();
    const QMargins dockRootMargins = constantsDock->widget()->layout()->contentsMargins();
    QCOMPARE(dockRootMargins, QMargins(0, 0, 0, 0));
    const QMargins searchRowMargins =
        searchLabel->parentWidget()->layout()->contentsMargins();
    QCOMPARE(searchRowMargins, QMargins(8, 6, 8, 6));
    QVERIFY(searchLabel->parentWidget()->styleSheet().contains(contentFill.name()));
    bool foundStyledDockMenu = false;
    for (QMenu* menu : window.findChildren<QMenu*>()) {
        if (!menu->styleSheet().contains(titleFill.name()))
            continue;
        foundStyledDockMenu = true;
        QVERIFY(menu->styleSheet().contains(titleText.name()));
        QVERIFY(menu->styleSheet().contains(controlFill.name()));
        QVERIFY(menu->styleSheet().contains(controlText.name()));
        QVERIFY(menu->styleSheet().contains(QStringLiteral("border-radius: 8px")));
    }
    QVERIFY(foundStyledDockMenu);

    searchBox->setText(QStringLiteral("no-such-constant-filter-value"));
    QTest::qWait(650);
    QCoreApplication::processEvents();
    QLabel* noMatchLabel = nullptr;
    for (QLabel* label : constantsDock->findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("No match found")) {
            noMatchLabel = label;
            break;
        }
    }
    QVERIFY(noMatchLabel != nullptr);
    QVERIFY(!noMatchLabel->isHidden());
    QCOMPARE(noMatchLabel->palette().color(QPalette::WindowText).name(), contentText.name());
    QVERIFY(noMatchLabel->styleSheet().contains(contentText.name()));
    QCOMPARE(table->topLevelItemCount(), 0);
    QVERIFY2(table->header()->length() >= table->header()->width() - 1,
             qPrintable(QStringLiteral("length=%1 header=%2 sections=%3,%4,%5")
                            .arg(table->header()->length())
                            .arg(table->header()->width())
                            .arg(table->header()->sectionSize(0))
                            .arg(table->header()->sectionSize(1))
                            .arg(table->header()->sectionSize(2))));
    searchBox->clear();
    QTest::qWait(650);
    QCoreApplication::processEvents();
    QVERIFY(table->topLevelItemCount() > 0);
    QVERIFY(!table->header()->stretchLastSection());

    QLabel* domainLabel = nullptr;
    for (QLabel* label : functionsDock->findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("Domain")) {
            domainLabel = label;
            break;
        }
    }
    QVERIFY(domainLabel != nullptr);
    QCOMPARE(domainLabel->palette().color(QPalette::Window).name(), contentFill.name());
    QCOMPARE(domainLabel->palette().color(QPalette::WindowText).name(), contentText.name());
    QVERIFY(domainLabel->styleSheet().contains(contentFill.name()));
    QVERIFY(domainLabel->styleSheet().contains(contentText.name()));
    const QMargins domainRowMargins =
        domainLabel->parentWidget()->layout()->contentsMargins();
    QCOMPARE(domainRowMargins, QMargins(8, 6, 8, 6));
    QVERIFY(domainLabel->parentWidget()->styleSheet().contains(contentFill.name()));

    constantsDock->show();
    constantsDock->raise();
    QCoreApplication::processEvents();
    const QImage searchImage = searchBox->grab().toImage();
    QVERIFY(!searchImage.isNull());
    QCOMPARE(searchImage.pixelColor(searchImage.width() / 2, searchImage.height() / 2).name(),
             contentFill.name());

    QDockWidget* bookDock = window.findChild<QDockWidget*>(QStringLiteral("BookDock"));
    QVERIFY(bookDock != nullptr);
    QVERIFY(bookDock->widget() != nullptr);
    QVERIFY(bookDock->widget()->layout() != nullptr);
    QCOMPARE(bookDock->widget()->layout()->contentsMargins(), QMargins(0, 0, 0, 0));
    QTextBrowser* bookBrowser = bookDock->findChild<QTextBrowser*>();
    QVERIFY(bookBrowser != nullptr);
    QCOMPARE(bookBrowser->palette().color(QPalette::Base).name(), contentFill.name());
    QCOMPARE(bookBrowser->palette().color(QPalette::Text).name(), contentText.name());
    QCOMPARE(bookBrowser->viewport()->palette().color(QPalette::Base).name(), contentFill.name());
    QVERIFY(bookBrowser->toHtml().contains(contentFill.name()));
    const QString bookScrollBarStyle = bookBrowser->verticalScrollBar()->styleSheet();
    QVERIFY(bookScrollBarStyle.contains(contentFill.name()));
    QVERIFY(bookScrollBarStyle.contains(titleFill.name()));
    QVERIFY(bookScrollBarStyle.contains(controlFill.name()));
    QVERIFY(bookScrollBarStyle.contains(hoverFill.name()));

    bool foundDockTabs = false;
    for (QTabBar* tabBar : window.findChildren<QTabBar*>()) {
        bool dockNavigationTabBar = false;
        for (int i = 0; i < tabBar->count(); ++i) {
            const QString text = tabBar->tabText(i);
            if (text == QStringLiteral("Constants") || text == QStringLiteral("Functions")) {
                dockNavigationTabBar = true;
                break;
            }
        }
        if (dockNavigationTabBar
            && tabBar->isVisible()
            && tabBar->styleSheet().contains(titleFill.name())) {
            foundDockTabs = true;
            QCOMPARE(tabBar->palette().color(QPalette::WindowText).name(), titleText.name());
            QVERIFY(tabBar->styleSheet().contains(QStringLiteral("background-color: transparent")));
            QVERIFY(tabBar->styleSheet().contains(chromeFill.name()));
            QVERIFY(tabBar->styleSheet().contains(chromeText.name()));
            QVERIFY(tabBar->styleSheet().contains(resultFill.name()));
            QVERIFY(tabBar->styleSheet().contains(resultText.name()));
            QVERIFY(tabBar->styleSheet().contains(titleFill.name()));
            QVERIFY(tabBar->styleSheet().contains(titleText.name()));
            QVERIFY(tabBar->styleSheet().contains(QStringLiteral("padding: 5px 14px")));
            QVERIFY(tabBar->styleSheet().contains(QStringLiteral("margin: 2px 1px")));
            QVERIFY(tabBar->property("speedcrunchDockSystemTabBar").toBool());
            QVERIFY(tabBar->hasMouseTracking());
            QVERIFY(!tabBar->drawBase());
            int hoveredTab = -1;
            for (int i = 0; i < tabBar->count(); ++i) {
                if (tabBar->tabText(i) == QStringLiteral("Constants")
                    || tabBar->tabText(i) == QStringLiteral("Functions")) {
                    hoveredTab = i;
                    break;
                }
            }
            QVERIFY(hoveredTab >= 0);
            const QPoint hoverPos = tabBar->tabRect(hoveredTab).center();
            QCOMPARE(tabBar->tabAt(hoverPos), hoveredTab);
            QMouseEvent moveEvent(QEvent::MouseMove,
                                  QPointF(hoverPos),
                                  QPointF(tabBar->mapToGlobal(hoverPos)),
                                  Qt::NoButton,
                                  Qt::NoButton,
                                  Qt::NoModifier);
            QCoreApplication::sendEvent(tabBar, &moveEvent);
            QTRY_COMPARE(tabBar->cursor().shape(), Qt::PointingHandCursor);
            QWidget* tabBarParent = tabBar->parentWidget();
            QVERIFY(tabBarParent != nullptr);
            QCOMPARE(tabBarParent->palette().color(QPalette::Window).name(), chromeFill.name());
            QVERIFY(tabBarParent->styleSheet().contains(chromeFill.name()));
            const QImage tabBarImage = tabBar->grab().toImage();
            QVERIFY(!tabBarImage.isNull());
            QCOMPARE(tabBarImage.pixelColor(tabBarImage.width() - 1,
                                            tabBarImage.height() / 2).name(),
                     chromeFill.name());
            QCOMPARE(tabBarImage.pixelColor(tabBarImage.width() - 1, 0).name(),
                     chromeFill.name());
        }
    }
    QVERIFY(foundDockTabs);

    settings->customColorSchemeJson = QStringLiteral("{\"background\":\"#300a24\"}");
    window.colorSchemeChanged();
    QCoreApplication::processEvents();

    const QVector<QColor> changedShades =
        generateOklchShades(QColor(QStringLiteral("#300a24")), 6, ThemePolarity::Dark);
    const QVector<QColor> changedForegrounds = aaForegroundsForBackgrounds(changedShades);
    const QColor changedContentFill = changedShades.at(2);
    const QColor changedContentText = changedForegrounds.at(2);
    const QColor changedTitleFill = changedShades.at(3);
    const QColor changedTitleText = changedForegrounds.at(3);
    const QColor changedPrimary = generatePrimaryFromBackground(QColor(QStringLiteral("#300a24")));

    QTRY_VERIFY(constantsDock->styleSheet().contains(changedTitleFill.name()));
    QVERIFY(constantsDock->styleSheet().contains(changedTitleText.name()));
    QVERIFY(!constantsDock->styleSheet().contains(titleFill.name()));
    bool foundUpdatedDockTab = false;
    for (QTabBar* tabBar : window.findChildren<QTabBar*>()) {
        if (!tabBar->styleSheet().contains(changedTitleFill.name()))
            continue;
        foundUpdatedDockTab = true;
        QVERIFY(tabBar->styleSheet().contains(changedTitleText.name()));
    }
    QVERIFY(foundUpdatedDockTab);
    QCOMPARE(constantsDock->widget()->palette().color(QPalette::Window).name(),
             changedContentFill.name());
    QVERIFY(constantsDock->widget()->styleSheet().contains(changedContentFill.name()));
    QVERIFY(!constantsDock->widget()->styleSheet().contains(contentFill.name()));
    QCOMPARE(searchLabel->palette().color(QPalette::Window).name(),
             changedContentFill.name());
    QCOMPARE(searchLabel->palette().color(QPalette::WindowText).name(),
             changedContentText.name());
    QVERIFY(searchLabel->parentWidget()->styleSheet().contains(changedContentFill.name()));
    QCOMPARE(domainLabel->palette().color(QPalette::Window).name(),
             changedContentFill.name());
    QCOMPARE(domainLabel->palette().color(QPalette::WindowText).name(),
             changedContentText.name());
    QVERIFY(domainLabel->parentWidget()->styleSheet().contains(changedContentFill.name()));
    QCOMPARE(searchBox->palette().color(QPalette::Base).name(), changedContentFill.name());
    QCOMPARE(searchBox->palette().color(QPalette::Text).name(), changedContentText.name());
    QCOMPARE(themedSearchBox->cursorColor().name(), changedPrimary.name());
    QVERIFY(searchBox->styleSheet().contains(QStringLiteral("border: %1px solid %2")
                                                 .arg(UiConfig::OutlineStrokeWidth)
                                                 .arg(changedPrimary.name())));
    QCOMPARE(comboBox->palette().color(QPalette::Button).name(), changedContentFill.name());
    QCOMPARE(comboBox->palette().color(QPalette::ButtonText).name(), changedContentText.name());
    QCOMPARE(table->palette().color(QPalette::Base).name(), changedContentFill.name());
    QCOMPARE(table->viewport()->palette().color(QPalette::Base).name(),
             changedContentFill.name());
    QVERIFY(table->styleSheet().contains(changedContentFill.name()));
}

void TestDisplayUi::restored_constants_dock_empty_filter_fills_header()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->windowGeometry.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->statusBarVisible = false;
    settings->hasNumberFormatStyleSetting = true;
    settings->constantsDockDomain.clear();
    settings->constantsDockSubdomain.clear();
    settings->constantsDockSearchText = QStringLiteral("no-such-constant-filter-value");

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QLineEdit* searchBox = constantsDock->findChild<QLineEdit*>();
    QTreeWidget* table = constantsDock->findChild<QTreeWidget*>();
    QVERIFY(searchBox != nullptr);
    QVERIFY(table != nullptr);
    QVERIFY(table->header() != nullptr);
    QCOMPARE(searchBox->text(), QStringLiteral("no-such-constant-filter-value"));
    QCOMPARE(table->topLevelItemCount(), 0);
    QTRY_VERIFY2(table->header()->length() >= table->header()->width() - 1,
                 qPrintable(QStringLiteral("length=%1 header=%2 sections=%3,%4,%5")
                                .arg(table->header()->length())
                                .arg(table->header()->width())
                                .arg(table->header()->sectionSize(0))
                                .arg(table->header()->sectionSize(1))
                                .arg(table->header()->sectionSize(2))));
}

void TestDisplayUi::dock_scroll_corner_uses_scrollbar_track_fill()
{
    MainWindowStateGuard guard;
    Settings* settings = guard.settings;

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = QStringLiteral("{\"background\":\"#1f3229\"}");
    settings->sessionLayoutJson.clear();
    settings->windowState.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    const QColor base(QStringLiteral("#1f3229"));
    const QVector<QColor> shades = generateOklchShades(base, 6, ThemePolarity::Dark);
    const QColor expectedTrackFill = shades.at(UiConfig::DockBackgroundShade);

    MainWindow window;
    window.resize(700, 420);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QTreeWidget* table = constantsDock->findChild<QTreeWidget*>();
    QVERIFY(table != nullptr);

    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    for (int column = 0; column < table->columnCount(); ++column)
        table->setColumnWidth(column, 240);
    constantsDock->show();
    constantsDock->raise();
    window.resizeDocks(QList<QDockWidget*> { constantsDock },
                       QList<int> { UiConfig::ConstantsDockMinimumWidth },
                       Qt::Horizontal);
    QCoreApplication::processEvents();

    QWidget* corner = table->cornerWidget();
    QVERIFY(corner != nullptr);
    QCOMPARE(corner->palette().color(QPalette::Window).name(), expectedTrackFill.name());
    QVERIFY(corner->styleSheet().contains(expectedTrackFill.name()));
    QVERIFY(corner->styleSheet().contains(QStringLiteral("border: 0")));
    QTRY_VERIFY(table->horizontalScrollBar()->isVisible());
    QTRY_VERIFY(table->verticalScrollBar()->isVisible());
    QTRY_VERIFY(corner->isVisible());

    const QImage image = corner->grab().toImage();
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 1);
    QVERIFY(image.height() > 1);
    const QList<QPoint> samplePoints {
        QPoint(0, 0),
        QPoint(image.width() - 1, 0),
        QPoint(0, image.height() - 1),
        QPoint(image.width() - 1, image.height() - 1),
        image.rect().center()
    };
    for (const QPoint& point : samplePoints) {
        const QColor sampled = image.pixelColor(point);
        QVERIFY2(colorsAreClose(sampled, expectedTrackFill, 3),
                 qPrintable(QStringLiteral("corner sample %1,%2 is %3, expected %4")
                                .arg(point.x())
                                .arg(point.y())
                                .arg(sampled.name(), expectedTrackFill.name())));
    }
}

void TestDisplayUi::dock_separator_style_uses_primary_while_hovered_or_dragged()
{
    MainWindowStateGuard guard;
    guard.settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    const QColor normal(QStringLiteral("#123456"));
    const QColor primary(QStringLiteral("#abcdef"));

    QWidget propertyOwner;
    propertyOwner.setProperty("speedcrunchDockSeparatorNormalColor", normal);
    propertyOwner.setProperty("speedcrunchDockSeparatorActiveColor", primary);
    propertyOwner.resize(96, 64);
    QWidget styleHost(&propertyOwner);
    styleHost.resize(64, 32);
    styleHost.move(0, 0);
    propertyOwner.show();
    QVERIFY(QTest::qWaitForWindowExposed(&propertyOwner));

    QCursor::setPos(styleHost.mapToGlobal(QPoint(50, 24)));
    QCoreApplication::processEvents();
    QCOMPARE(dockSeparatorPrimitiveColor(&styleHost, QStyle::State_None).name(),
             normal.name());

    QCursor::setPos(styleHost.mapToGlobal(QPoint(4, 4)));
    QCoreApplication::processEvents();
    QCOMPARE(dockSeparatorPrimitiveColor(&styleHost, QStyle::State_None).name(),
             primary.name());
    QImage horizontalSeparator =
        dockSeparatorPrimitiveImage(&styleHost, QStyle::State_None, QSize(32, 8));
    const int horizontalStrokeTop =
        (horizontalSeparator.height() - UiConfig::DockSplitterStrokeWidth) / 2;
    const int horizontalStrokeBottom =
        horizontalStrokeTop + UiConfig::DockSplitterStrokeWidth - 1;
    for (int y = horizontalStrokeTop; y <= horizontalStrokeBottom; ++y)
        QCOMPARE(horizontalSeparator.pixelColor(horizontalSeparator.width() / 2, y).name(),
                 primary.name());
    if (horizontalStrokeTop > 0)
        QCOMPARE(horizontalSeparator.pixelColor(horizontalSeparator.width() / 2,
                                                horizontalStrokeTop - 1).alpha(), 0);
    if (horizontalStrokeBottom + 1 < horizontalSeparator.height())
        QCOMPARE(horizontalSeparator.pixelColor(horizontalSeparator.width() / 2,
                                                horizontalStrokeBottom + 1).alpha(), 0);

    QImage verticalSeparator =
        dockSeparatorPrimitiveImage(&styleHost, QStyle::State_None, QSize(8, 32));
    const int verticalStrokeLeft =
        (verticalSeparator.width() - UiConfig::DockSplitterStrokeWidth) / 2;
    const int verticalStrokeRight =
        verticalStrokeLeft + UiConfig::DockSplitterStrokeWidth - 1;
    for (int x = verticalStrokeLeft; x <= verticalStrokeRight; ++x)
        QCOMPARE(verticalSeparator.pixelColor(x, verticalSeparator.height() / 2).name(),
                 primary.name());
    if (verticalStrokeLeft > 0)
        QCOMPARE(verticalSeparator.pixelColor(verticalStrokeLeft - 1,
                                              verticalSeparator.height() / 2).alpha(), 0);
    if (verticalStrokeRight + 1 < verticalSeparator.width())
        QCOMPARE(verticalSeparator.pixelColor(verticalStrokeRight + 1,
                                              verticalSeparator.height() / 2).alpha(), 0);

    QCursor::setPos(styleHost.mapToGlobal(QPoint(4, 1)));
    QCoreApplication::processEvents();
    QImage thinHorizontalSeparator = dockSeparatorPrimitiveImage(&styleHost,
                                                                QStyle::State_None,
                                                                QRect(0, 0, 32, 1),
                                                                QSize(32, 3));
    QCOMPARE(thinHorizontalSeparator.pixelColor(thinHorizontalSeparator.width() / 2, 1).name(),
             primary.name());
    QCOMPARE(thinHorizontalSeparator.pixelColor(thinHorizontalSeparator.width() / 2, 2).alpha(),
             0);

    QCursor::setPos(styleHost.mapToGlobal(QPoint(1, 4)));
    QCoreApplication::processEvents();
    QImage thinVerticalSeparator = dockSeparatorPrimitiveImage(&styleHost,
                                                              QStyle::State_None,
                                                              QRect(0, 0, 1, 32),
                                                              QSize(3, 32));
    QCOMPARE(thinVerticalSeparator.pixelColor(1, thinVerticalSeparator.height() / 2).name(),
             primary.name());
    QCOMPARE(thinVerticalSeparator.pixelColor(2, thinVerticalSeparator.height() / 2).alpha(),
             0);

    QCursor::setPos(styleHost.mapToGlobal(QPoint(50, 24)));
    QCoreApplication::processEvents();
    QCOMPARE(dockSeparatorPrimitiveColor(&styleHost, QStyle::State_Sunken).name(),
             primary.name());
}

void TestDisplayUi::constants_dock_uses_configured_narrow_minimum_width()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        bool oldConstantsDockVisible;
        bool oldFunctionsDockVisible;
        bool oldHistoryDockVisible;
        bool oldFormulaBookDockVisible;
        bool oldVariablesDockVisible;
        bool oldUserFunctionsDockVisible;
        bool oldUserUnitsDockVisible;
        bool oldBitfieldVisible;
        Settings::KeypadMode oldKeypadMode;
        bool oldKeypadVisible;
        bool oldHasNumberFormatStyleSetting;
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->constantsDockVisible = oldConstantsDockVisible;
            settings->functionsDockVisible = oldFunctionsDockVisible;
            settings->historyDockVisible = oldHistoryDockVisible;
            settings->formulaBookDockVisible = oldFormulaBookDockVisible;
            settings->variablesDockVisible = oldVariablesDockVisible;
            settings->userFunctionsDockVisible = oldUserFunctionsDockVisible;
            settings->userUnitsDockVisible = oldUserUnitsDockVisible;
            settings->bitfieldVisible = oldBitfieldVisible;
            settings->keypadMode = oldKeypadMode;
            settings->keypadVisible = oldKeypadVisible;
            settings->hasNumberFormatStyleSetting = oldHasNumberFormatStyleSetting;
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
        }
    } guard {
        settings,
        settings->constantsDockVisible,
        settings->functionsDockVisible,
        settings->historyDockVisible,
        settings->formulaBookDockVisible,
        settings->variablesDockVisible,
        settings->userFunctionsDockVisible,
        settings->userUnitsDockVisible,
        settings->bitfieldVisible,
        settings->keypadMode,
        settings->keypadVisible,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->keypadMode = Settings::KeypadModeDisabled;
    settings->keypadVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QCOMPARE(constantsDock->minimumWidth(), UiConfig::ConstantsDockMinimumWidth);

    QVERIFY(constantsDock->widget()->minimumSizeHint().width()
            <= UiConfig::ConstantsDockMinimumWidth);
}

void TestDisplayUi::dock_search_focus_suppresses_editor_primary_outline_across_panes()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
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
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
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
        settings,
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->sessionLayoutJson,
        settings->constantsDockVisible,
        settings->functionsDockVisible,
        settings->historyDockVisible,
        settings->keypadVisible,
        settings->formulaBookDockVisible,
        settings->variablesDockVisible,
        settings->userFunctionsDockVisible,
        settings->userUnitsDockVisible,
        settings->bitfieldVisible,
        settings->windowPositionSave,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = QStringLiteral("{\"background\":\"#1f3229\"}");
    settings->sessionLayoutJson.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    const QColor primary = generatePrimaryFromBackground(QColor(QStringLiteral("#1f3229")));

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(window.findChildren<Editor*>().size(), 2);

    const QVector<QColor> shades =
        generateOklchShades(QColor(QStringLiteral("#1f3229")), 6, ThemePolarity::Dark);
    const QColor selectedTabFill = shades.at(UiConfig::SelectedSessionTabFillShade);
    QList<QTabBar*> tabBars = window.findChildren<QTabBar*>();
    tabBars.erase(std::remove_if(tabBars.begin(), tabBars.end(), [](QTabBar* tabBar) {
        return tabBar->count() == 0 || !tabBar->isVisible();
    }), tabBars.end());
    QCOMPARE(tabBars.size(), 2);
    for (QTabBar* tabBar : tabBars) {
        QVERIFY2(colorsAreClose(selectedSessionTabFillColor(tabBar), selectedTabFill),
                 qPrintable(QStringLiteral("fill=%1 expected=%2")
                                .arg(selectedSessionTabFillColor(tabBar).name(),
                                     selectedTabFill.name())));
    }
    QCOMPARE(std::count_if(tabBars.cbegin(), tabBars.cend(), [&primary](QTabBar* tabBar) {
        return selectedSessionTabHasBottomIndicator(tabBar, primary);
    }), 1);

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QLineEdit* searchBox = constantsDock->findChild<QLineEdit*>();
    QVERIFY(searchBox != nullptr);

    QList<Editor*> editors = window.findChildren<Editor*>();
    QTRY_VERIFY(anyEditorHasPrimaryOutline(editors, primary));

    constantsDock->show();
    constantsDock->raise();
    QCoreApplication::processEvents();
    QTest::mouseClick(searchBox, Qt::LeftButton);
    QTRY_VERIFY(searchBox->hasFocus());
    QTRY_VERIFY(!anyEditorHasPrimaryOutline(window.findChildren<Editor*>(), primary));

    for (int i = 0; i < editors.size(); ++i) {
        editors.at(i)->setText(QStringLiteral("pane %1").arg(i + 1));
        QCoreApplication::processEvents();
        QVERIFY(searchBox->hasFocus());
        QVERIFY2(!anyEditorHasPrimaryOutline(window.findChildren<Editor*>(), primary),
                 qPrintable(editors.at(i)->styleSheet()));
    }
}

void TestDisplayUi::dock_selection_inserts_into_active_session_pane_after_focus_transfer()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
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
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

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
        settings,
        settings->sessionLayoutJson,
        settings->constantsDockVisible,
        settings->functionsDockVisible,
        settings->historyDockVisible,
        settings->keypadVisible,
        settings->formulaBookDockVisible,
        settings->variablesDockVisible,
        settings->userFunctionsDockVisible,
        settings->userUnitsDockVisible,
        settings->bitfieldVisible,
        settings->windowPositionSave,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->sessionLayoutJson.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    ResultDisplay* firstDisplay = window.findChild<ResultDisplay*>();
    QVERIFY(firstDisplay != nullptr);
    QWidget* firstPage = firstDisplay->parentWidget();
    Editor* firstEditor = firstPage ? firstPage->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    QVERIFY(firstEditor != nullptr);

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(window.findChildren<ResultDisplay*>().size(), 2);

    const QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    ResultDisplay* secondDisplay = displays.first() == firstDisplay ? displays.last() : displays.first();
    QWidget* secondPage = secondDisplay->parentWidget();
    Editor* secondEditor = secondPage ? secondPage->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    QVERIFY(secondEditor != nullptr);

    secondEditor->setText(QString());
    firstEditor->setText(QStringLiteral("x"));
    firstEditor->setCursorPosition(firstEditor->text().size());
    QWidget* firstPane = paneWidgetForDisplay(firstDisplay);
    QWidget* secondPane = paneWidgetForDisplay(secondDisplay);
    QVERIFY(firstPane != nullptr);
    QVERIFY(secondPane != nullptr);
    QCOMPARE(firstPane->objectName(), QStringLiteral("SessionPane"));
    QCOMPARE(secondPane->objectName(), QStringLiteral("SessionPane"));
    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QTreeWidget* constantsList = constantsDock->findChild<QTreeWidget*>();
    QVERIFY(constantsList != nullptr);

    constantsDock->show();
    constantsDock->raise();
    QCoreApplication::processEvents();
    QTest::mouseClick(constantsList->viewport(), Qt::LeftButton, Qt::NoModifier,
                      constantsList->viewport()->rect().center());
    QTRY_VERIFY(constantsList->hasFocus());

    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "insertConstantIntoEditor",
                                      Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("pi"))));
    QVERIFY2(firstEditor->text() == QStringLiteral("x") && secondEditor->text() == QStringLiteral("pi"),
             qPrintable(QStringLiteral("first='%1' second='%2'")
                            .arg(firstEditor->text(), secondEditor->text())));
}

void TestDisplayUi::clicking_tab_activates_own_pane_in_nested_split_layout()
{
    Settings* settings = Settings::instance();
    struct SettingsGuard {
        Settings* settings;
        QString oldColorScheme;
        QString oldCustomColorSchemeJson;
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
        QByteArray oldSkipUpdateCheck;
        bool hadSkipUpdateCheck;

        ~SettingsGuard()
        {
            settings->colorScheme = oldColorScheme;
            settings->customColorSchemeJson = oldCustomColorSchemeJson;
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
        settings,
        settings->colorScheme,
        settings->customColorSchemeJson,
        settings->sessionLayoutJson,
        settings->constantsDockVisible,
        settings->functionsDockVisible,
        settings->historyDockVisible,
        settings->keypadVisible,
        settings->formulaBookDockVisible,
        settings->variablesDockVisible,
        settings->userFunctionsDockVisible,
        settings->userUnitsDockVisible,
        settings->bitfieldVisible,
        settings->windowPositionSave,
        settings->hasNumberFormatStyleSetting,
        qgetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK"),
        qEnvironmentVariableIsSet("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK")
    };

    qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", "1");
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = QStringLiteral("{\"background\":\"#1f3229\"}");
    settings->sessionLayoutJson.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->windowPositionSave = false;
    settings->hasNumberFormatStyleSetting = true;

    const QColor primary = generatePrimaryFromBackground(QColor(QStringLiteral("#1f3229")));

    MainWindow window;
    window.resize(900, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneDown", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(window.findChildren<ResultDisplay*>().size(), 3);

    QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    std::sort(displays.begin(), displays.end(), [](ResultDisplay* lhs, ResultDisplay* rhs) {
        const QPoint lhsPos = lhs->mapToGlobal(QPoint(0, 0));
        const QPoint rhsPos = rhs->mapToGlobal(QPoint(0, 0));
        if (lhsPos.y() != rhsPos.y())
            return lhsPos.y() < rhsPos.y();
        return lhsPos.x() < rhsPos.x();
    });
    ResultDisplay* paneA = displays.at(0);
    ResultDisplay* paneB = displays.at(1);
    ResultDisplay* paneC = displays.at(2);
    QVERIFY(paneA->mapToGlobal(QPoint(0, 0)).x() < paneB->mapToGlobal(QPoint(0, 0)).x());
    QVERIFY(paneC->mapToGlobal(QPoint(0, 0)).y() > paneB->mapToGlobal(QPoint(0, 0)).y());

    Editor* editorA = editorForDisplay(paneA);
    Editor* editorB = editorForDisplay(paneB);
    Editor* editorC = editorForDisplay(paneC);
    QTabBar* tabA = tabBarForDisplay(paneA);
    QTabBar* tabB = tabBarForDisplay(paneB);
    QTabBar* tabC = tabBarForDisplay(paneC);
    QVERIFY(editorA != nullptr);
    QVERIFY(editorB != nullptr);
    QVERIFY(editorC != nullptr);
    QVERIFY(tabA != nullptr);
    QVERIFY(tabB != nullptr);
    QVERIFY(tabC != nullptr);
    QVERIFY(qAbs(tabA->mapToGlobal(QPoint(0, 0)).x() - paneA->mapToGlobal(QPoint(0, 0)).x()) < 8);
    QVERIFY(qAbs(tabB->mapToGlobal(QPoint(0, 0)).x() - paneB->mapToGlobal(QPoint(0, 0)).x()) < 8);
    QVERIFY(qAbs(tabC->mapToGlobal(QPoint(0, 0)).x() - paneC->mapToGlobal(QPoint(0, 0)).x()) < 8);

    QTest::mouseClick(paneB->viewport(), Qt::LeftButton, Qt::NoModifier,
                      paneB->viewport()->rect().center());
    QTRY_VERIFY(editorHasPrimaryOutline(editorB, primary));
    QVERIFY(selectedSessionTabHasBottomIndicator(tabB, primary));

    QVERIFY(tabA->count() > 0);
    QTest::mouseClick(tabA, Qt::LeftButton, Qt::NoModifier,
                      tabA->tabRect(0).center());
    QTRY_VERIFY2(editorHasPrimaryOutline(editorA, primary),
                 qPrintable(QStringLiteral("A=%1 B=%2 C=%3 tabA=%4 tabB=%5 tabC=%6 focus=%7")
                                .arg(editorHasPrimaryOutline(editorA, primary))
                                .arg(editorHasPrimaryOutline(editorB, primary))
                                .arg(editorHasPrimaryOutline(editorC, primary))
                                .arg(selectedSessionTabHasBottomIndicator(tabA, primary))
                                .arg(selectedSessionTabHasBottomIndicator(tabB, primary))
                                .arg(selectedSessionTabHasBottomIndicator(tabC, primary))
                                .arg(QApplication::focusWidget()
                                         ? QString::fromLatin1(QApplication::focusWidget()->metaObject()->className())
                                         : QStringLiteral("<none>"))));
    QVERIFY(selectedSessionTabHasBottomIndicator(tabA, primary));
    QVERIFY(!editorHasPrimaryOutline(editorC, primary));
    QVERIFY(!selectedSessionTabHasBottomIndicator(tabC, primary));
}

void TestDisplayUi::active_pane_survives_window_reactivation_focus_replay()
{
    MainWindowStateGuard guard;
    Settings* settings = Settings::instance();
    settings->colorScheme = QStringLiteral("Custom");
    settings->customColorSchemeJson = QStringLiteral("{\"background\":\"#1f3229\"}");
    settings->sessionLayoutJson.clear();
    settings->constantsDockVisible = false;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    const QColor primary = generatePrimaryFromBackground(QColor(QStringLiteral("#1f3229")));

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(window.findChildren<ResultDisplay*>().size(), 3);

    QList<ResultDisplay*> displays = window.findChildren<ResultDisplay*>();
    std::sort(displays.begin(), displays.end(), [](ResultDisplay* lhs, ResultDisplay* rhs) {
        return lhs->mapToGlobal(QPoint(0, 0)).x() < rhs->mapToGlobal(QPoint(0, 0)).x();
    });

    ResultDisplay* firstDisplay = displays.at(0);
    Editor* firstEditor = editorForDisplay(firstDisplay);
    QVERIFY(firstEditor != nullptr);

    QTest::mouseClick(firstDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      firstDisplay->viewport()->rect().center());
    QTRY_VERIFY(editorHasPrimaryOutline(firstEditor, primary));

    for (ResultDisplay* display : displays) {
        Editor* editor = editorForDisplay(display);
        QVERIFY(editor != nullptr);
        QFocusEvent focusIn(QEvent::FocusIn, Qt::OtherFocusReason);
        QCoreApplication::sendEvent(editor->viewport(), &focusIn);
    }
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "insertTextIntoEditor",
                                      Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("p"))));
    QVERIFY2(firstEditor->text() == QStringLiteral("p"),
             qPrintable(QStringLiteral("passive replay pane0='%1' pane1='%2' pane2='%3'")
                            .arg(editorForDisplay(displays.at(0))->text(),
                                 editorForDisplay(displays.at(1))->text(),
                                 editorForDisplay(displays.at(2))->text())));
    firstEditor->clear();

    // Let pane-creation focus timers settle, then reselect the first pane so
    // WindowDeactivate captures the same stable state as a real app switch.
    QCoreApplication::processEvents();
    QTest::mouseClick(firstDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      firstDisplay->viewport()->rect().center());
    QTRY_VERIFY(editorHasPrimaryOutline(firstEditor, primary));

    QEvent windowDeactivate(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(&window, &windowDeactivate);
    QEvent windowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&window, &windowActivate);
    for (ResultDisplay* display : displays) {
        Editor* editor = editorForDisplay(display);
        QVERIFY(editor != nullptr);
        QFocusEvent focusIn(QEvent::FocusIn, Qt::OtherFocusReason);
        QCoreApplication::sendEvent(editor->viewport(), &focusIn);
    }
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window,
                                      "insertTextIntoEditor",
                                      Qt::DirectConnection,
                                      Q_ARG(QString, QStringLiteral("z"))));
    QVERIFY2(firstEditor->text() == QStringLiteral("z"),
             qPrintable(QStringLiteral("after activation pane0='%1' pane1='%2' pane2='%3'")
                            .arg(editorForDisplay(displays.at(0))->text(),
                                 editorForDisplay(displays.at(1))->text(),
                                 editorForDisplay(displays.at(2))->text())));

    QTRY_VERIFY2(editorHasPrimaryOutline(firstEditor, primary),
                 qPrintable(QStringLiteral("pane0=%1 pane1=%2 pane2=%3 focus=%4")
                                .arg(editorHasPrimaryOutline(editorForDisplay(displays.at(0)), primary))
                                .arg(editorHasPrimaryOutline(editorForDisplay(displays.at(1)), primary))
                                .arg(editorHasPrimaryOutline(editorForDisplay(displays.at(2)), primary))
                                .arg(QApplication::focusWidget()
                                         ? QString::fromLatin1(QApplication::focusWidget()->metaObject()->className())
                                         : QStringLiteral("<none>"))));
    for (int i = 1; i < displays.size(); ++i)
        QVERIFY(!editorHasPrimaryOutline(editorForDisplay(displays.at(i)), primary));
}

void TestDisplayUi::focused_dock_search_survives_window_reactivation_focus_replay()
{
    MainWindowStateGuard guard;
    Settings* settings = Settings::instance();
    settings->sessionLayoutJson.clear();
    settings->constantsDockVisible = true;
    settings->functionsDockVisible = false;
    settings->historyDockVisible = false;
    settings->keypadVisible = false;
    settings->formulaBookDockVisible = false;
    settings->variablesDockVisible = false;
    settings->userFunctionsDockVisible = false;
    settings->userUnitsDockVisible = false;
    settings->bitfieldVisible = false;
    settings->hasNumberFormatStyleSetting = true;

    MainWindow window;
    window.resize(900, 500);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "splitActivePaneRight", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(window.findChildren<Editor*>().size(), 2);

    QDockWidget* constantsDock =
        window.findChild<QDockWidget*>(QStringLiteral("ConstantsDock"));
    QVERIFY(constantsDock != nullptr);
    QLineEdit* searchBox = constantsDock->findChild<QLineEdit*>();
    QVERIFY(searchBox != nullptr);

    constantsDock->show();
    constantsDock->raise();
    QCoreApplication::processEvents();
    QTest::mouseClick(searchBox, Qt::LeftButton);
    QTRY_VERIFY(searchBox->hasFocus());

    QEvent windowDeactivate(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(&window, &windowDeactivate);
    QEvent windowActivate(QEvent::WindowActivate);
    QCoreApplication::sendEvent(&window, &windowActivate);
    for (Editor* editor : window.findChildren<Editor*>()) {
        QFocusEvent focusIn(QEvent::FocusIn, Qt::OtherFocusReason);
        QCoreApplication::sendEvent(editor->viewport(), &focusIn);
    }
    QCoreApplication::processEvents();

    QTRY_VERIFY(searchBox->hasFocus());
    QTest::keyClicks(searchBox, "mol");
    QCOMPARE(searchBox->text(), QStringLiteral("mol"));
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
    QTRY_VERIFY(secondEditor->hasFocus());

    // Make the already-loaded first pane diverge from the scroll snapshot that
    // was captured when focus moved to the second pane.
    firstScrollBar->setValue(20);
    QCOMPARE(firstScrollBar->value(), 20);

    QTest::mouseClick(firstDisplay->viewport(), Qt::LeftButton, Qt::NoModifier,
                      firstDisplay->viewport()->rect().center());
    QCoreApplication::processEvents();
    QTRY_VERIFY(firstEditor->hasFocus());

    QTabBar* secondTabBar = paneWidgetForDisplay(secondDisplay)->findChild<QTabBar*>();
    QVERIFY(secondTabBar != nullptr);
    QVERIFY(secondTabBar->isVisible());
    QTest::mouseClick(secondTabBar, Qt::LeftButton, Qt::NoModifier,
                      secondTabBar->tabRect(secondTabBar->currentIndex()).center());
    QCoreApplication::processEvents();
    QTRY_VERIFY(secondEditor->hasFocus());

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

void TestDisplayUi::switching_session_tabs_preserves_each_editor_text()
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
            if (hadSkipUpdateCheck)
                qputenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK", oldSkipUpdateCheck);
            else
                qunsetenv("SPEEDCRUNCH_TEST_SKIP_UPDATE_CHECK");
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
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    ResultDisplay* display = window.findChild<ResultDisplay*>();
    QVERIFY(display != nullptr);
    QWidget* pane = paneWidgetForDisplay(display);
    QTabBar* tabBar = pane ? pane->findChild<QTabBar*>() : nullptr;
    QVERIFY(tabBar != nullptr);

    Editor* editor = window.findChild<Editor*>();
    QVERIFY(editor != nullptr);
    editor->setText(QStringLiteral("first tab draft"));
    editor->setCursorPosition(5);
    QCoreApplication::processEvents();

    QVERIFY(QMetaObject::invokeMethod(&window, "showNewSessionDialog", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QTRY_COMPARE(tabBar->count(), 2);
    QCOMPARE(tabBar->currentIndex(), 1);

    editor->setText(QStringLiteral("second tab draft"));
    editor->setCursorPosition(6);
    QCoreApplication::processEvents();

    QTest::mouseClick(tabBar, Qt::LeftButton, Qt::NoModifier, tabBar->tabRect(0).center());
    QCoreApplication::processEvents();
    QCOMPARE(editor->text(), QStringLiteral("first tab draft"));

    QTest::mouseClick(tabBar, Qt::LeftButton, Qt::NoModifier, tabBar->tabRect(1).center());
    QCoreApplication::processEvents();
    QCOMPARE(editor->text(), QStringLiteral("second tab draft"));
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
