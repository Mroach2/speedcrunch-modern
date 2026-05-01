// SPDX-FileCopyrightText: 2007-2010, 2012-2018, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_MAINWINDOW_H
#define GUI_MAINWINDOW_H

#include "gui/keypad.h"
#include "math/quantity.h"

#include <QMainWindow>
#include <QStringList>

class AutoHideLabel;
class BitFieldWidget;
template <class Widget> class GenericDock;
class BookDock;
class Constants;
class ConstantsWidget;
class Editor;
class Evaluator;
class HistoryEntry;
class UserFunction;
class FunctionRepo;
class FunctionsWidget;
class HistoryWidget;
class ManualWindow;
class ManualServer;
class ResultDisplay;
class Session;
class Settings;
class UserFunctionListWidget;
class UserUnitListWidget;
class Variable;
class VariableListWidget;
class VersionCheck;

class QActionGroup;
class QHBoxLayout;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QResizeEvent;
class QTranslator;
class QVBoxLayout;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();
    void persistSessionAndSettingsForShutdown();

signals:
    void angleUnitChanged();
    void complexNumbersChanged();
    void colorSchemeChanged();
    void languageChanged();
    void radixCharacterChanged();
    void resultFormatChanged();
    void resultPrecisionChanged();
    void resultRoundingModeChanged();
    void syntaxHighlightingChanged();
    void historyChanged();
    void variablesChanged();
    void functionsChanged();
    void unitsChanged();

public slots:
    void copy();

private slots:
    void applyColorSchemeFromAction(QAction*);
    void applySelectedColorScheme();
    void clearEditor();
    void clearEditorAndBitfield();
    void clearHistory();
    void copyResultToClipboard();
    void decreaseDisplayFontPointSize();
    void decreaseOpacity();
    void deleteVariables();
    void deleteUserFunctions();
    void evaluateEditorExpression();
    void cancelHistoryEntryEdit();
    void exportHtml();
    void exportPlainText();
    void handleAutoCalcMessageAvailable(const QString&);
    void handleAutoCalcQuantityAvailable(const Quantity&);
    void handleCopyAvailable(bool copyAvailable);
    void handleBitsChanged(const QString&);
    void handleKeypadButtonPress(Keypad::Button);
    void handleEditorTextChange();
    void handleEditorEscapePressed();
    void handleCustomKeypadButtonPress(int action, const QString& text);
    void handleDisplaySelectionChange();
    void handleEditorSelectionChange();
    void handleManualClosed();
    void handleDockWidgetVisibilityChanged(bool visible);
    void hideStateLabel();
    void increaseDisplayFontPointSize();
    void increaseOpacity();
    void insertConstantIntoEditor(const QString&);
    void insertFunctionIntoEditor(const QString&);
    void insertTextIntoEditor(const QString&);
    void insertVariableIntoEditor(const QString&);
    void insertUserFunctionIntoEditor(const QString&);
    void insertUserUnitIntoEditor(const QString&);
    void checkForUpdates();
    void openFeedbackURL();
    void openCommunityURL();
    void openFacebookGroupURL();
    void openNewsURL();
    void openDonateURL();
    void retranslateText();
    void revertColorScheme();
    void saveColorSchemeToRevert();
    void saveSessionDialog();
    void selectEditorExpression();
    void setAlwaysOnTopEnabled(bool);
    void setAngleModeDegree();
    void setAngleModeRadian();
    void setAngleModeGradian();
    void setAngleModeTurn();
    void setAngleModeRevolution();
    void setAutoAnsEnabled(bool);
    void setAutoCalcEnabled(bool);
    void setAutoCompletionEnabled(bool);
    void setAutoCompletionBuiltInFunctionsEnabled(bool);
    void setAutoCompletionBuiltInVariablesEnabled(bool);
    void setAutoCompletionLongFormUnitsEnabled(bool);
    void setAutoCompletionUserFunctionsEnabled(bool);
    void setAutoCompletionUserVariablesEnabled(bool);
    void setBitfieldVisible(bool);
    void setHistorySizeLimit();
    void setConstantsDockVisible(bool, bool takeFocus = true);
    void setFormulaBookDockVisible(bool, bool takeFocus = true);
    void setFullScreenEnabled(bool);
    void setFunctionsDockVisible(bool, bool takeFocus = true);
    void setHistoryDockVisible(bool, bool takeFocus = true);
    void setHistorySaving(QAction*);
    void setKeypadMode(QAction*);
    void setKeypadZoom(QAction*);
    void setKeypadVisible(bool);
    void setLeaveLastExpressionEnabled(bool);
    void setUpDownArrowBehavior(QAction*);
    void setEmptyHistoryHintEnabled(bool);
    void setRadixCharacterAutomatic();
    void setRadixCharacter(char);
    void setRadixCharacterComma();
    void setRadixCharacterDot();
    void setRadixCharacterBoth();
    void setResultFormatBinary();
    void setResultFormatComplexDisabled();
    void setResultFormatCartesian();
    void setResultFormat(char);
    void setResultFormatEngineering();
    void setResultFormatFixed();
    void setResultFormatGeneral();
    void setResultFormatHexadecimal();
    void setImaginaryUnitI();
    void setImaginaryUnitJ();
    void setResultFormatOctal();
    void setResultFormatPolar();
    void setResultFormatPolarAngle();
    void setResultFormatRational();
    void setResultFormatScientific();
    void setResultFormatSexagesimal();
    void setUnitNegativeExponentStyle(QAction*);
    void setResultPrecision15Digits();
    void setResultPrecision2Digits();
    void setResultPrecision3Digits();
    void setResultPrecision50Digits();
    void setResultPrecision8Digits();
    void setResultPrecisionAutomatic();
    void setResultPrecisionCustom();
    void setResultPrecision(int);
    void setResultRoundingMode(QAction*);
    void setMenuBarVisible(bool);
    void setStatusBarVisible(bool);
    void setSyntaxHighlightingEnabled(bool);
    void setDigitGrouping(QAction*);
    void setDigitGroupingIntegerPartOnlyEnabled(bool);
    void setAutoResultToClipboardEnabled(bool);
    void setSimplifyResultExpressionsEnabled(bool);
    void setHoverHighlightResultsEnabled(bool);
    void setVariablesDockVisible(bool, bool takeFocus = true);
    void setUserFunctionsDockVisible(bool, bool takeFocus = true);
    void setUserUnitsDockVisible(bool, bool takeFocus = true);
    void setWindowPositionSaveEnabled(bool);
    void setSingleInstanceEnabled(bool);
    void setWidgetsDirection();
    void showAboutDialog();
    void showStateLabel(const QString&);
    void showFontDialog();
    void showLanguageChooserDialog();
    void showManualWindow();
    void showNumberFormatDialog();
    void showResultSlotsDialog();
    void showCustomThemeDialog();
    void showContextHelp();
    void showReadyMessage();
    void showAngleModeContextMenu(const QPoint&);
    void showPrecisionContextMenu(const QPoint&);
    void showComplexFormContextMenu(const QPoint&);
    void showKeypadContextMenu(const QPoint&);
    void showResultFormatContextMenu(const QPoint&);
    void showSessionImportDialog();
    void showUserDefinitionsImportDialog();
    void showSessionLoadDialog();
    void wrapSelection();
    void startHistoryEntryEdit(int index);
    void editHistoryEntryContext(int index);
    void removeHistoryEntryAt(int index);
    void removeHistoryEntriesAbove(int index);
    void removeHistoryEntriesBelow(int index);

protected:
    void closeEvent(QCloseEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;

private:
    Q_DISABLE_COPY(MainWindow)

    void clearTextEditSelection(QPlainTextEdit*);
    void addTabifiedDock(QDockWidget*, bool takeFocus, Qt::DockWidgetArea = Qt::RightDockWidgetArea);
    void deleteDock(QDockWidget*);
    void createUi();
    void createActions();
    void createActionGroups();
    void createActionShortcuts();
    void createMenus();
    void createStatusBar();
    void createFixedWidgets();
    void createKeypad();
    void createBitField();
    void createBookDock(bool takeFocus = true);
    void createConstantsDock(bool takeFocus = true);
    void createFunctionsDock(bool takeFocus = true);
    void createHistoryDock(bool takeFocus = true);
    void createVariablesDock(bool takeFocus = true);
    void createUserFunctionsDock(bool takeFocus = true);
    void createUserUnitsDock(bool takeFocus = true);
    void createFixedConnections();
    void applySettings();
    void checkInitialResultFormat();
    void checkInitialComplexFormat();
    void checkInitialImaginaryUnit();
    void checkInitialResultPrecision();
    void checkInitialLanguage();
    void checkInitialDigitGrouping();
    void restoreSession(bool restoreHistory = true);
    void deleteKeypad();
    void deleteStatusBar();
    void deleteBitField();
    void deleteBookDock();
    void deleteConstantsDock();
    void deleteFunctionsDock();
    void deleteHistoryDock();
    void deleteVariablesDock();
    void deleteUserFunctionsDock();
    void deleteUserUnitsDock();
    void saveSettings();
    void saveSessionToDefaultPath(bool saveHistory = true);
    void saveSession(QString &fname, bool saveHistory = true);
    bool configureCustomKeypad();
    void setActionsText();
    void updateComplexDisabledActionText();
    void updateKeypadDisabledActionText();
    void setMenusText();
    void setStatusBarText();
    void updateStatusBarSectionVisibility();
    QString statusBarAngleUnitValue() const;
    QString statusBarComplexFormValue() const;
    QString statusBarResultPrecisionValue() const;
    QString statusBarResultFormatValue() const;
    void applyStartupUserDefinitions();
    void importUserDefinitionsFromText(const QString& text, bool overwriteExisting,
                                       int* importedVariables = nullptr,
                                       int* importedFunctions = nullptr,
                                       int* ignoredLines = nullptr,
                                       QList<int>* ignoredLineNumbers = nullptr,
                                       bool dryRun = false);
    bool rebuildSessionFromEntries(const QList<HistoryEntry>& entries,
                                   int startIndex = 0,
                                   int* errorIndex = nullptr,
                                   QString* errorText = nullptr);
    QList<HistoryEntry> historyEntries() const;

    static QTranslator* createTranslator(const QString& langCode);

    struct {
        QAction* sessionLoad;
        QAction* sessionSave;
        QAction* sessionImport;
        QAction* sessionImportUserDefinitions;
        QAction* sessionExportHtml;
        QAction* sessionExportPlainText;
        QAction* sessionQuit;
        QAction* editCopy;
        QAction* editCopyLastResult;
        QAction* editPaste;
        QAction* editSelectExpression;
        QAction* editInsertFunction;
        QAction* editInsertVariable;
        QAction* editDeleteVariable;
        QAction* editInsertUserFunction;
        QAction* editDeleteUserFunction;
        QAction* editClearExpression;
        QAction* editClearHistory;
        QAction* editWrapSelection;
        QAction* viewKeypadDisabled;
        QAction* viewKeypadBasicWide;
        QAction* viewKeypadScientificWide;
        QAction* viewKeypadScientificNarrow;
        QAction* viewKeypadCustom;
        QAction* viewKeypadZoom100;
        QAction* viewKeypadZoom150;
        QAction* viewKeypadZoom200;
        QAction* viewFormulaBook;
        QAction* viewConstants;
        QAction* viewFunctions;
        QAction* viewVariables;
        QAction* viewUserFunctions;
        QAction* viewUserUnits;
        QAction* viewHistory;
        QAction* viewStatusBar;
        QAction* viewMenuBar;
        QAction* viewFullScreenMode;
        QAction* viewBitfield;
        QAction* settingsResultFormatGeneral;
        QAction* settingsResultFormatFixed;
        QAction* settingsResultFormatEngineering;
        QAction* settingsResultFormatScientific;
        QAction* settingsResultFormatRational;
        QAction* settingsResultFormatAutoPrecision;
        QAction* settingsResultFormat0Digits;
        QAction* settingsResultFormat2Digits;
        QAction* settingsResultFormat3Digits;
        QAction* settingsResultFormat8Digits;
        QAction* settingsResultFormat15Digits;
        QAction* settingsResultFormat50Digits;
        QAction* settingsResultFormatCustomDigits;
        QAction* settingsResultRoundingHalfAwayFromZero;
        QAction* settingsResultRoundingHalfEven;
        QAction* settingsResultRoundingTowardZero;
        QAction* settingsResultRoundingTowardPositiveInfinity;
        QAction* settingsResultRoundingTowardNegativeInfinity;
        QAction* settingsResultFormatBinary;
        QAction* settingsResultFormatOctal;
        QAction* settingsResultFormatCartesian;
        QAction* settingsResultFormatPolar;
        QAction* settingsResultFormatPolarAngle;
        QAction* settingsImaginaryUnitI;
        QAction* settingsImaginaryUnitJ;
        QAction* settingsResultFormatHexadecimal;
        QAction* settingsResultFormatSexagesimal;
        QAction* settingsUnitNegativeExponentSuperscript;
        QAction* settingsUnitNegativeExponentFraction;
        QAction* settingsAngleUnitRadian;
        QAction* settingsAngleUnitDegree;
        QAction* settingsAngleUnitGradian;
        QAction* settingsAngleUnitTurn;
        QAction* settingsAngleUnitRevolution;
        QAction* settingsBehaviorSaveWindowPositionOnExit;
        QAction* settingsBehaviorPartialResults;
        QAction* settingsBehaviorAutoCompletion;
        QAction* settingsBehaviorAutoCompletionBuiltInFunctions;
        QAction* settingsBehaviorAutoCompletionBuiltInVariables;
        QAction* settingsBehaviorAutoCompletionLongFormUnits;
        QAction* settingsBehaviorAutoCompletionUserFunctions;
        QAction* settingsBehaviorAutoCompletionUserVariables;
        QAction* settingsBehaviorSyntaxHighlighting;
        QAction* settingsBehaviorHoverHighlightResults;
        QAction* settingsBehaviorEmptyHistoryHint;
        QAction* settingsBehaviorDigitGroupingNone;
        QAction* settingsBehaviorDigitGroupingOneSpace;
        QAction* settingsBehaviorDigitGroupingTwoSpaces;
        QAction* settingsBehaviorDigitGroupingThreeSpaces;
        QAction* settingsBehaviorDigitGroupingIntegerPartOnly;
        QAction* settingsBehaviorAutoAns;
        QAction* settingsBehaviorLeaveLastExpression;
        QAction* settingsBehaviorNumberFormat;
        QAction* settingsBehaviorResultSlots;
        QAction* settingsBehaviorUpDownArrowNever;
        QAction* settingsBehaviorUpDownArrowAlways;
        QAction* settingsBehaviorUpDownArrowSingleLineOnly;
        QAction* settingsBehaviorAlwaysOnTop;
        QAction* settingsBehaviorAutoResultToClipboard;
        QAction* settingsBehaviorSimplifyResultExpressions;
        QAction* settingsBehaviorHistorySavingNever;
        QAction* settingsBehaviorHistorySavingOnExit;
        QAction* settingsBehaviorHistorySavingContinuously;
        QAction* settingsBehaviorHistorySizeLimit;
        QAction* settingsBehaviorSingleInstance;
        QAction* settingsRadixCharBoth;
        QAction* settingsResultFormatComplexDisabled;
        QAction* settingsDisplayZoomIn;
        QAction* settingsDisplayZoomOut;
        QAction* settingsDisplayFont;
        QAction* settingsDisplayColorSchemeCustom;
        QVector<QAction*> settingsDisplayColorSchemes;
        QAction* settingsRadixCharDefault;
        QAction* settingsRadixCharDot;
        QAction* settingsRadixCharComma;
        QAction* settingsLanguage;
        QAction* helpManual;
        QAction* helpUpdates;
        QAction* helpFeedback;
        QAction* helpCommunity;
        QAction* helpFacebookGroup;
        QAction* helpNews;
        QAction* helpDonate;
        QAction* helpAbout;
        QAction* contextHelp;
    } m_actions;

    struct {
        QActionGroup* angle;
        QActionGroup* colorScheme;
        QActionGroup* digits;
        QActionGroup* resultRoundingMode;
        QActionGroup* resultFormat;
        QActionGroup* complexFormat;
        QActionGroup* imaginaryUnit;
        QActionGroup* radixChar;
        QActionGroup* digitGrouping;
        QActionGroup* historySaving;
        QActionGroup* upDownArrowBehavior;
        QActionGroup* keypad;
        QActionGroup* keypadZoom;
        QActionGroup* unitNegativeExponentStyle;
    } m_actionGroups;

    struct {
        QMenu* angleUnit;
        QMenu* complexForm;
        QMenu* complexNumbers;
        QMenu* editing;
        QMenu* autoCompletion;
        QMenu* history;
        QMenu* historySaving;
        QMenu* colorScheme;
        QMenu* decimal;
        QMenu* digitGrouping;
        QMenu* display;
        QMenu* edit;
        QMenu* results;
        QMenu* unitNegativeExponentStyle;
        QMenu* resultRoundingMode;
        QMenu* resultFormat;
        QMenu* inputFormat;
        QMenu* help;
        QMenu* precision;
        QMenu* radixChar;
        QMenu* upDownArrowBehavior;
        QMenu* session;
        QMenu* sessionExport;
        QMenu* settings;
        QMenu* view;
        QMenu* keypad;
        QMenu* keypadZoom;
        QMenu* window;
    } m_menus;

    struct {
        QVBoxLayout* root;
        QHBoxLayout* keypad;
    } m_layouts;

    struct {
        QLabel* state;
        QPushButton* stateCloseButton;
        ResultDisplay* display;
        Editor* editor;
        Keypad* keypad = nullptr;
        QWidget* root;
        ManualWindow* manual = nullptr;
        BitFieldWidget* bitField = nullptr;
    } m_widgets;

    struct {
        BookDock* book;
        GenericDock<ConstantsWidget>* constants;
        GenericDock<FunctionsWidget>* functions;
        GenericDock<HistoryWidget>* history;
        GenericDock<VariableListWidget>* variables;
        GenericDock<UserFunctionListWidget>* userFunctions;
        GenericDock<UserUnitListWidget>* userUnits;
    } m_docks;
    QList<QDockWidget*> m_allDocks;

    struct {
        bool autoAns;
    } m_conditions;

    struct {
        QWidget* angleUnitSection;
        QLabel* angleUnitLabel;
        QPushButton* angleUnit;
        QWidget* resultFormatSection;
        QLabel* resultFormatLabel;
        QPushButton* resultFormat;
        QWidget* resultPrecisionSection;
        QLabel* resultPrecisionLabel;
        QPushButton* resultPrecision;
        QWidget* complexFormSection;
        QLabel* complexFormLabel;
        QPushButton* complexForm;
    } m_status;

    Constants* m_constants;
    Evaluator* m_evaluator;
    FunctionRepo* m_functions;
    Settings* m_settings;
    Session* m_session;
    QTranslator* m_translator;
    QPlainTextEdit* m_copyWidget;
    ManualServer* m_manualServer;
    VersionCheck* m_versionCheck;
    QString m_colorSchemeToRevert;
    QString m_customColorSchemeJsonToRevert;
    int m_pendingHistoryEditIndex;
    bool m_shutdownStateSaved;
};

#endif // GUI_MAINWINDOW_H
