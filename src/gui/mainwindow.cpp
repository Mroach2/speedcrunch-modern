// SPDX-FileCopyrightText: 2007-2020, 2022, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/mainwindow.h"

#include "core/constants.h"
#include "core/evaluator.h"
#include "core/functions.h"
#include "core/numberformatter.h"
#include "core/regexpatterns.h"
#include "core/settings.h"
#include "core/session.h"
#include "core/startupdefinitions.h"
#include "core/unicodechars.h"
#include "core/variable.h"
#include "core/sessionhistory.h"
#include "core/userfunction.h"
#include "gui/aboutbox.h"
#include "gui/bitfieldwidget.h"
#include "gui/bookdock.h"
#include "gui/genericdock.h"
#include "gui/constantswidget.h"
#include "gui/customkeypaddialog.h"
#include "gui/editorutils.h"
#include "gui/functionswidget.h"
#include "gui/historywidget.h"
#include "gui/userfunctionlistwidget.h"
#include "gui/userunitlistwidget.h"
#include "gui/variablelistwidget.h"
#include "gui/versioncheck.h"
#include "gui/editor.h"
#include "gui/historywidget.h"
#include "gui/manualwindow.h"
#include "gui/numberformatdialog.h"
#include "gui/notationandprecisiondialog.h"
#include "core/manualserver.h"
#include "gui/resultdisplay.h"
#include "gui/resultlineformatutils.h"
#include "gui/syntaxhighlighter.h"
#include "math/cmath.h"
#include "math/floatnum/floatconfig.h"
#include "core/mathdsl.h"
#include "core/units.h"

#include <QLatin1String>
#include <QLocale>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QColorDialog>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFont>
#include <QFontDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>
#include <QSpinBox>
#include <QStandardPaths>
#include <QScrollBar>
#include <QStatusBar>
#include <QStyle>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QJsonDocument>

#include <algorithm>

#ifdef Q_OS_WIN32
#include "windows.h"
#include <shlobj.h>
#endif // Q_OS_WIN32

namespace {
EvaluationContext currentEvaluationContext(const Settings* settings)
{
    EvaluationContext ctx;
    ctx.main.fmt = settings->resultFormat;
    ctx.main.prec = settings->resultPrecision;
    ctx.main.cplx = settings->resultFormatComplex;
    if (settings->multipleResultLinesEnabled) {
        if (settings->secondaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->alternativeResultFormat, settings->secondaryResultPrecision, settings->secondaryResultFormatComplex});
        if (settings->tertiaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->tertiaryResultFormat, settings->tertiaryResultPrecision, settings->tertiaryResultFormatComplex});
        if (settings->quaternaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->quaternaryResultFormat, settings->quaternaryResultPrecision, settings->quaternaryResultFormatComplex});
        if (settings->quinaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->quinaryResultFormat, settings->quinaryResultPrecision, settings->quinaryResultFormatComplex});
    }
    ctx.complexOn = settings->complexNumbers;
    ctx.unit = settings->imaginaryUnit;
    ctx.angle = settings->angleUnit;
    ctx.unitExp = settings->unitNegativeExponentStyle;
    ctx.round = settings->resultRoundingMode;
    return ctx;
}

void applyEvaluationContext(Settings* settings, const EvaluationContext& ctx)
{
    settings->resultFormat = ctx.main.fmt;
    settings->resultPrecision = ctx.main.prec;
    settings->resultFormatComplex = ctx.main.cplx;

    settings->multipleResultLinesEnabled = !ctx.extras.isEmpty();
    settings->secondaryResultEnabled = false;
    settings->tertiaryResultEnabled = false;
    settings->quaternaryResultEnabled = false;
    settings->quinaryResultEnabled = false;

    auto applyExtra = [settings](int index, const ResultLineContext& line) {
        if (index == 0) {
            settings->secondaryResultEnabled = true;
            settings->alternativeResultFormat = line.fmt;
            settings->secondaryResultPrecision = line.prec;
            settings->secondaryResultFormatComplex = line.cplx;
        } else if (index == 1) {
            settings->tertiaryResultEnabled = true;
            settings->tertiaryResultFormat = line.fmt;
            settings->tertiaryResultPrecision = line.prec;
            settings->tertiaryResultFormatComplex = line.cplx;
        } else if (index == 2) {
            settings->quaternaryResultEnabled = true;
            settings->quaternaryResultFormat = line.fmt;
            settings->quaternaryResultPrecision = line.prec;
            settings->quaternaryResultFormatComplex = line.cplx;
        } else if (index == 3) {
            settings->quinaryResultEnabled = true;
            settings->quinaryResultFormat = line.fmt;
            settings->quinaryResultPrecision = line.prec;
            settings->quinaryResultFormatComplex = line.cplx;
        }
    };
    for (int i = 0; i < ctx.extras.size() && i < 4; ++i)
        applyExtra(i, ctx.extras.at(i));

    settings->complexNumbers = ctx.complexOn;
    settings->imaginaryUnit = (ctx.unit == 'j') ? 'j' : 'i';
    settings->angleUnit = ctx.angle;
    settings->unitNegativeExponentStyle = isValidUnitNegativeExponentStyle(ctx.unitExp)
        ? ctx.unitExp
        : Settings::UnitNegativeExponentSuperscript;
    settings->resultRoundingMode = isValidResultRoundingMode(ctx.round)
        ? ctx.round
        : Settings::ResultRoundingHalfAwayFromZero;

    DMath::complexMode = settings->complexNumbers;
    CMath::setImaginaryUnitSymbol(settings->imaginaryUnit);
    setRuntimeUnitNegativeExponentStyle(settings->unitNegativeExponentStyle);
    setRuntimeResultRoundingMode(settings->resultRoundingMode);
}

QStringList renderedLinesForHistoryEntry(const HistoryEntry& entry, Settings* settings)
{
    const EvaluationContext previousContext = currentEvaluationContext(settings);
    applyEvaluationContext(settings, entry.contextRef());

    QStringList lines;
    lines.append(ResultLineFormatUtils::formattedExpressionLineForDisplay(
        entry.expr(),
        entry.interpretedExpr()));
    if (!entry.result().isNan()) {
        lines.append(ResultLineFormatUtils::formatResultLinesForDisplay(
            entry.expr(),
            entry.interpretedExpr(),
            entry.result(),
            false,
            true));
    }

    applyEvaluationContext(settings, previousContext);
    return lines;
}
}

QTranslator* MainWindow::createTranslator(const QString& langCode)
{
    QTranslator* translator = new QTranslator;
    QLocale locale(langCode == "C" ? QLocale().name() : langCode);

    if(!translator->load(locale, QString(":/locale/"))) {
        // There are regional Portuguese translations only for Brazil and Portugal.
        // Unsupported Portuguese variants (e.g. pt_AO) should fall back to pt_PT.
        if (locale.language() == QLocale::Portuguese) {
            const QLocale::Territory territory = locale.territory();
            if (territory != QLocale::Brazil && territory != QLocale::Portugal) {
                if (translator->load(QLocale(QLocale::Portuguese, QLocale::Portugal), QString(":/locale/")))
                    return translator;
            }
        }

        // Strip the country and try to find a generic translation for this language
        locale = QLocale(locale.language());
        if (!translator->load(locale, QString(":/locale/"))) {
            // Handle the case where the translation file cannot be loaded
            // For example, log an error, use a default language, etc.
        }
    }

    return translator;
}

static bool isVisibleKeypadMode(Settings::KeypadMode mode)
{
    return mode == Settings::KeypadModeBasicWide
        || mode == Settings::KeypadModeScientificWide
        || mode == Settings::KeypadModeScientificNarrow
        || mode == Settings::KeypadModeCustom;
}

static bool isWaylandPlatform()
{
    const auto platform = QGuiApplication::platformName();
    const bool isWayland = (platform == "wayland");
    return isWayland;
}

QString colorSchemeRoleLabel(ColorScheme::Role role)
{
    switch (role) {
    case ColorScheme::Cursor: return QStringLiteral("cursor");
    case ColorScheme::Number: return QStringLiteral("number");
    case ColorScheme::Parens: return QStringLiteral("parens");
    case ColorScheme::Result: return QStringLiteral("result");
    case ColorScheme::Comment: return QStringLiteral("comment");
    case ColorScheme::Matched: return QStringLiteral("matched");
    case ColorScheme::Function: return QStringLiteral("function");
    case ColorScheme::Operator: return QStringLiteral("operator");
    case ColorScheme::Variable: return QStringLiteral("variable");
    case ColorScheme::ScrollBar: return QStringLiteral("scrollbar");
    case ColorScheme::Separator: return QStringLiteral("separator");
    case ColorScheme::Background: return QStringLiteral("background");
    case ColorScheme::EditorBackground: return QStringLiteral("editorbackground");
    }
    return QString();
}

void updateColorButtonStyle(QPushButton* button, const QColor& color)
{
    if (!button || !color.isValid())
        return;

    const int brightness = qRound(0.299 * color.red() + 0.587 * color.green() + 0.114 * color.blue());
    const QString textColor = brightness >= 160 ? QStringLiteral("#111111") : QStringLiteral("#f5f5f5");

    button->setText(color.name());
    button->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: %2; }")
                              .arg(color.name(), textColor));
}

static void typeTextThroughEditorInputRules(Editor* editor, const QString& text)
{
    if (!editor || text.isEmpty())
        return;

    const auto keyForChar = [](QChar ch) -> int {
        switch (ch.unicode()) {
        case MathDsl::DotSep.unicode(): return Qt::Key_Period;
        case MathDsl::CommaSep.unicode(): return Qt::Key_Comma;
        case MathDsl::AddOp.unicode(): return Qt::Key_Plus;
        case MathDsl::SubOpAl1.unicode(): return Qt::Key_Minus;
        case MathDsl::DivOp.unicode(): return Qt::Key_Slash;
        case MathDsl::MulOpAl1.unicode(): return Qt::Key_Asterisk;
        case MathDsl::PowOp.unicode(): return Qt::Key_AsciiCircum;
        case MathDsl::GroupStart.unicode(): return Qt::Key_ParenLeft;
        case MathDsl::GroupEnd.unicode(): return Qt::Key_ParenRight;
        case MathDsl::PercentOp.unicode(): return Qt::Key_Percent;
        case MathDsl::FactorOp.unicode(): return Qt::Key_Exclam;
        default: break;
        }
        if (ch.isDigit())
            return Qt::Key_0 + (ch.unicode() - MathDsl::Dig0.unicode());
        return Qt::Key_unknown;
    };

    for (const QChar ch : text) {
        const int key = keyForChar(ch);
        QKeyEvent keyEvent(QEvent::KeyPress, key, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(editor, &keyEvent);
    }
}

namespace {

struct AssignmentTarget {
    QString identifier;
    bool isFunction = false;
    bool isUnit = false;
    bool valid = false;
};

bool splitUserFunctionDescriptionForImport(const QString& expression,
                                           QString* expressionWithoutDescription)
{
    const int equalsPos = expression.indexOf(MathDsl::Equals);
    if (equalsPos < 0)
        return false;

    const QString leftSide = expression.left(equalsPos);
    if (!leftSide.contains(MathDsl::GroupStart) || !leftSide.contains(MathDsl::GroupEnd))
        return false;

    int depth = 0;
    const int n = expression.size();
    for (int i = equalsPos + 1; i < n; ++i) {
        const QChar ch = expression.at(i);
        if (ch == MathDsl::GroupStart) {
            ++depth;
        } else if (ch == MathDsl::GroupEnd && depth > 0) {
            --depth;
        } else if (ch == UnicodeChars::QuestionMark && depth == 0) {
            *expressionWithoutDescription = expression.left(i).trimmed();
            return true;
        }
    }

    return false;
}

AssignmentTarget assignmentTargetFromExpression(Evaluator* evaluator, const QString& expression)
{
    AssignmentTarget target;
    if (!evaluator)
        return target;

    QString expressionToParse = expression;
    splitUserFunctionDescriptionForImport(expression, &expressionToParse);
    Tokens tokens = evaluator->scan(expressionToParse);

    if (!tokens.valid())
        return target;

    if (tokens.count() > 2
        && tokens.at(0).isIdentifier()
        && tokens.at(1).asOperator() == Token::Assignment)
    {
        target.identifier = tokens.at(0).text();
        target.valid = true;
        return target;
    }

    if (tokens.count() > 4
        && tokens.at(0).asOperator() == Token::AssociationStart
        && tokens.at(0).text() == QString(MathDsl::UnitStart)
        && tokens.at(1).isUnitIdentifier()
        && tokens.at(2).asOperator() == Token::AssociationEnd
        && tokens.at(2).text() == QString(MathDsl::UnitEnd)
        && tokens.at(3).asOperator() == Token::Assignment)
    {
        target.identifier = tokens.at(1).text();
        target.isUnit = true;
        target.valid = true;
        return target;
    }

    if (tokens.count() > 2
        && tokens.at(0).isIdentifier()
        && tokens.at(1).asOperator() == Token::AssociationStart
        && tokens.at(1).text() == QLatin1String("("))
    {
        bool assignFunc = false;
        int t = 0;

        if (tokens.count() > 4
            && tokens.at(2).asOperator() == Token::AssociationEnd
            && tokens.at(2).text() == QLatin1String(")"))
        {
            t = 3;
            if (tokens.at(3).asOperator() == Token::Assignment)
                assignFunc = true;
        } else {
            for (t = 2; t + 1 < tokens.count(); t += 2)  {
                if (!tokens.at(t).isIdentifier())
                    break;

                if (tokens.at(t + 1).asOperator() == Token::AssociationEnd
                    && tokens.at(t + 1).text() == QLatin1String(")")) {
                    t += 2;
                    if (t < tokens.count()
                        && tokens.at(t).asOperator() == Token::Assignment)
                    {
                        assignFunc = true;
                    }
                    break;
                } else if (tokens.at(t + 1).asOperator() != Token::ListSeparator) {
                    break;
                }
            }
        }

        if (assignFunc) {
            target.identifier = tokens.at(0).text();
            target.isFunction = true;
            target.valid = true;
        }
    }

    return target;
}

bool findUserFunctionByName(const QList<UserFunction>& functions, const QString& name, UserFunction* function)
{
    for (const UserFunction& candidate : functions) {
        if (candidate.name() == name) {
            if (function)
                *function = candidate;
            return true;
        }
    }

    return false;
}

QString normalizedDisplaySelectionForEvaluation(QString selected)
{
    selected.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));

    const QStringList rawLines = selected.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QStringList candidateLines;
    candidateLines.reserve(rawLines.size());
    for (const QString& rawLine : rawLines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith(QLatin1String("=")))
            continue;
        candidateLines.append(line);
    }

    if (!candidateLines.isEmpty())
        return candidateLines.join(QLatin1Char(' '));

    for (const QString& rawLine : rawLines) {
        QString line = rawLine.trimmed();
        if (line.startsWith(QLatin1String("=")))
            line = line.mid(1).trimmed();
        if (!line.isEmpty())
            return line;
    }

    return selected.trimmed();
}

} // namespace

void MainWindow::createUi()
{
    createActions();
    createActionGroups();
    createActionShortcuts();
    createMenus();
    createFixedWidgets();
    createFixedConnections();

    setWindowTitle("SpeedCrunch");
    setWindowIcon(QPixmap(":/speedcrunch.png"));

    m_copyWidget = m_widgets.editor;
}

void MainWindow::createActions()
{
    m_actions.sessionExportHtml = new QAction(this);
    m_actions.sessionExportPlainText = new QAction(this);
    m_actions.sessionImport = new QAction(this);
    m_actions.sessionImportUserDefinitions = new QAction(this);
    m_actions.sessionLoad = new QAction(this);
    m_actions.sessionQuit = new QAction(this);
    m_actions.sessionSave = new QAction(this);
    m_actions.editClearExpression = new QAction(this);
    m_actions.editClearHistory = new QAction(this);
    m_actions.editCopyLastResult = new QAction(this);
    m_actions.editCopy = new QAction(this);
    m_actions.editPaste = new QAction(this);
    m_actions.editSelectExpression = new QAction(this);
    m_actions.editWrapSelection = new QAction(this);
    m_actions.viewConstants = new QAction(this);
    m_actions.viewFullScreenMode = new QAction(this);
    m_actions.viewFunctions = new QAction(this);
    m_actions.viewHistory = new QAction(this);
    m_actions.viewKeypadDisabled = new QAction(this);
    m_actions.viewKeypadBasicWide = new QAction(this);
    m_actions.viewKeypadScientificWide = new QAction(this);
    m_actions.viewKeypadScientificNarrow = new QAction(this);
    m_actions.viewKeypadCustom = new QAction(this);
    m_actions.viewKeypadZoom100 = new QAction(this);
    m_actions.viewKeypadZoom150 = new QAction(this);
    m_actions.viewKeypadZoom200 = new QAction(this);
    m_actions.viewFormulaBook = new QAction(this);
    m_actions.viewStatusBar = new QAction(this);
    m_actions.viewMenuBar = new QAction(this);
    m_actions.viewVariables = new QAction(this);
    m_actions.viewBitfield = new QAction(this);
    m_actions.viewUserFunctions = new QAction(this);
    m_actions.viewUserUnits = new QAction(this);
    m_actions.settingsAngleUnitDegree = new QAction(this);
    m_actions.settingsAngleUnitRadian = new QAction(this);
    m_actions.settingsAngleUnitGradian = new QAction(this);
    m_actions.settingsAngleUnitTurn = new QAction(this);
    m_actions.settingsAngleUnitRevolution = new QAction(this);
    m_actions.settingsBehaviorAlwaysOnTop = new QAction(this);
    m_actions.settingsBehaviorAutoAns = new QAction(this);
    m_actions.settingsBehaviorAutoCompletion = new QAction(this);
    m_actions.settingsBehaviorAutoCompletionBuiltInFunctions = new QAction(this);
    m_actions.settingsBehaviorAutoCompletionBuiltInVariables = new QAction(this);
    m_actions.settingsBehaviorAutoCompletionLongFormUnits = new QAction(this);
    m_actions.settingsBehaviorAutoCompletionUserFunctions = new QAction(this);
    m_actions.settingsBehaviorAutoCompletionUserVariables = new QAction(this);
    m_actions.settingsBehaviorEmptyHistoryHint = new QAction(this);
    m_actions.settingsBehaviorLeaveLastExpression = new QAction(this);
    m_actions.settingsBehaviorNumberFormat = new QAction(this);
    m_actions.settingsBehaviorResultSlots = new QAction(this);
    m_actions.settingsBehaviorUpDownArrowNever = new QAction(this);
    m_actions.settingsBehaviorUpDownArrowAlways = new QAction(this);
    m_actions.settingsBehaviorUpDownArrowSingleLineOnly = new QAction(this);
    m_actions.settingsBehaviorPartialResults = new QAction(this);
    m_actions.settingsBehaviorHistorySavingNever = new QAction(this);
    m_actions.settingsBehaviorHistorySavingOnExit = new QAction(this);
    m_actions.settingsBehaviorHistorySavingContinuously = new QAction(this);
    m_actions.settingsBehaviorSaveWindowPositionOnExit = new QAction(this);
    m_actions.settingsBehaviorSingleInstance = new QAction(this);
    m_actions.settingsBehaviorSyntaxHighlighting = new QAction(this);
    m_actions.settingsBehaviorHoverHighlightResults = new QAction(this);
    m_actions.settingsBehaviorDigitGroupingNone = new QAction(this);
    m_actions.settingsBehaviorDigitGroupingOneSpace = new QAction(this);
    m_actions.settingsBehaviorDigitGroupingTwoSpaces = new QAction(this);
    m_actions.settingsBehaviorDigitGroupingThreeSpaces = new QAction(this);
    m_actions.settingsBehaviorDigitGroupingIntegerPartOnly = new QAction(this);
    m_actions.settingsBehaviorAutoResultToClipboard = new QAction(this);
    m_actions.settingsBehaviorSimplifyResultExpressions = new QAction(this);
    m_actions.settingsBehaviorHistorySizeLimit = new QAction(this);
    m_actions.settingsResultFormatComplexDisabled = new QAction(this);
    m_actions.settingsDisplayFont = new QAction(this);
    m_actions.settingsDisplayColorSchemeCustom = new QAction(this);
    m_actions.settingsLanguage = new QAction(this);
    m_actions.settingsRadixCharComma = new QAction(this);
    m_actions.settingsRadixCharDefault = new QAction(this);
    m_actions.settingsRadixCharDot = new QAction(this);
    m_actions.settingsRadixCharBoth = new QAction(this);
    m_actions.settingsResultFormat0Digits = new QAction(this);
    m_actions.settingsResultFormat15Digits = new QAction(this);
    m_actions.settingsResultFormat2Digits = new QAction(this);
    m_actions.settingsResultFormat3Digits = new QAction(this);
    m_actions.settingsResultFormat50Digits = new QAction(this);
    m_actions.settingsResultFormat8Digits = new QAction(this);
    m_actions.settingsResultFormatCustomDigits = new QAction(this);
    m_actions.settingsResultRoundingHalfAwayFromZero = new QAction(this);
    m_actions.settingsResultRoundingHalfEven = new QAction(this);
    m_actions.settingsResultRoundingTowardZero = new QAction(this);
    m_actions.settingsResultRoundingTowardPositiveInfinity = new QAction(this);
    m_actions.settingsResultRoundingTowardNegativeInfinity = new QAction(this);
    m_actions.settingsResultFormatAutoPrecision = new QAction(this);
    m_actions.settingsResultFormatBinary = new QAction(this);
    m_actions.settingsResultFormatEngineering = new QAction(this);
    m_actions.settingsResultFormatFixed = new QAction(this);
    m_actions.settingsResultFormatGeneral = new QAction(this);
    m_actions.settingsResultFormatHexadecimal = new QAction(this);
    m_actions.settingsResultFormatOctal = new QAction(this);
    m_actions.settingsResultFormatRational = new QAction(this);
    m_actions.settingsResultFormatScientific = new QAction(this);
    m_actions.settingsResultFormatCartesian= new QAction(this);
    m_actions.settingsResultFormatPolar = new QAction(this);
    m_actions.settingsResultFormatPolarAngle = new QAction(this);
    m_actions.settingsImaginaryUnitI = new QAction(this);
    m_actions.settingsImaginaryUnitJ = new QAction(this);
    m_actions.settingsResultFormatSexagesimal = new QAction(this);
    m_actions.settingsUnitNegativeExponentSuperscript = new QAction(this);
    m_actions.settingsUnitNegativeExponentFraction = new QAction(this);
    m_actions.helpManual = new QAction(this);
    m_actions.helpUpdates = new QAction(this);
    m_actions.helpFeedback = new QAction(this);
    m_actions.helpCommunity = new QAction(this);
    m_actions.helpNews = new QAction(this);
    m_actions.helpDonate = new QAction(this);
    m_actions.helpAbout = new QAction(this);
    m_actions.contextHelp = new QAction(this);

    m_actions.settingsAngleUnitDegree->setCheckable(true);
    m_actions.settingsAngleUnitRadian->setCheckable(true);
    m_actions.settingsAngleUnitGradian->setCheckable(true);
    m_actions.settingsAngleUnitTurn->setCheckable(true);
    m_actions.settingsAngleUnitRevolution->setCheckable(true);
    m_actions.settingsBehaviorAlwaysOnTop->setCheckable(true);
    m_actions.settingsBehaviorAutoAns->setCheckable(true);
    m_actions.settingsBehaviorAutoCompletion->setCheckable(true);
    m_actions.settingsBehaviorAutoCompletionBuiltInFunctions->setCheckable(true);
    m_actions.settingsBehaviorAutoCompletionBuiltInVariables->setCheckable(true);
    m_actions.settingsBehaviorAutoCompletionLongFormUnits->setCheckable(true);
    m_actions.settingsBehaviorAutoCompletionUserFunctions->setCheckable(true);
    m_actions.settingsBehaviorAutoCompletionUserVariables->setCheckable(true);
    m_actions.settingsBehaviorEmptyHistoryHint->setCheckable(true);
    m_actions.settingsBehaviorLeaveLastExpression->setCheckable(true);
    m_actions.settingsBehaviorUpDownArrowNever->setCheckable(true);
    m_actions.settingsBehaviorUpDownArrowNever->setData(Settings::UpDownArrowBehaviorNever);
    m_actions.settingsBehaviorUpDownArrowAlways->setCheckable(true);
    m_actions.settingsBehaviorUpDownArrowAlways->setData(Settings::UpDownArrowBehaviorAlways);
    m_actions.settingsBehaviorUpDownArrowSingleLineOnly->setCheckable(true);
    m_actions.settingsBehaviorUpDownArrowSingleLineOnly->setData(Settings::UpDownArrowBehaviorSingleLineOnly);
    m_actions.settingsBehaviorPartialResults->setCheckable(true);
    m_actions.settingsBehaviorHistorySavingNever->setCheckable(true);
    m_actions.settingsBehaviorHistorySavingNever->setData(Settings::HistorySavingNever);
    m_actions.settingsBehaviorHistorySavingOnExit->setCheckable(true);
    m_actions.settingsBehaviorHistorySavingOnExit->setData(Settings::HistorySavingOnExit);
    m_actions.settingsBehaviorHistorySavingContinuously->setCheckable(true);
    m_actions.settingsBehaviorHistorySavingContinuously->setData(Settings::HistorySavingContinuously);
    m_actions.settingsBehaviorSaveWindowPositionOnExit->setCheckable(true);
    m_actions.settingsBehaviorSingleInstance->setCheckable(true);
    m_actions.settingsBehaviorSyntaxHighlighting->setCheckable(true);
    m_actions.settingsBehaviorHoverHighlightResults->setCheckable(true);
    m_actions.settingsBehaviorDigitGroupingNone->setCheckable(true);
    m_actions.settingsBehaviorDigitGroupingNone->setData(0);
    m_actions.settingsBehaviorDigitGroupingOneSpace->setCheckable(true);
    m_actions.settingsBehaviorDigitGroupingOneSpace->setData(1);
    m_actions.settingsBehaviorDigitGroupingTwoSpaces->setCheckable(true);
    m_actions.settingsBehaviorDigitGroupingTwoSpaces->setData(2);
    m_actions.settingsBehaviorDigitGroupingThreeSpaces->setCheckable(true);
    m_actions.settingsBehaviorDigitGroupingThreeSpaces->setData(3);
    m_actions.settingsBehaviorDigitGroupingIntegerPartOnly->setCheckable(true);
    m_actions.settingsBehaviorAutoResultToClipboard->setCheckable(true);
    m_actions.settingsBehaviorSimplifyResultExpressions->setCheckable(true);
    m_actions.settingsResultFormatComplexDisabled->setCheckable(true);
    m_actions.settingsDisplayColorSchemeCustom->setCheckable(true);
    m_actions.settingsRadixCharComma->setCheckable(true);
    m_actions.settingsRadixCharDefault->setCheckable(true);
    m_actions.settingsRadixCharDot->setCheckable(true);
    m_actions.settingsRadixCharBoth->setCheckable(true);
    m_actions.settingsResultFormat0Digits->setCheckable(true);
    m_actions.settingsResultFormat15Digits->setCheckable(true);
    m_actions.settingsResultFormat2Digits->setCheckable(true);
    m_actions.settingsResultFormat3Digits->setCheckable(true);
    m_actions.settingsResultFormat50Digits->setCheckable(true);
    m_actions.settingsResultFormat8Digits->setCheckable(true);
    m_actions.settingsResultFormatCustomDigits->setCheckable(true);
    m_actions.settingsResultRoundingHalfAwayFromZero->setCheckable(true);
    m_actions.settingsResultRoundingHalfAwayFromZero->setData(Settings::ResultRoundingHalfAwayFromZero);
    m_actions.settingsResultRoundingHalfEven->setCheckable(true);
    m_actions.settingsResultRoundingHalfEven->setData(Settings::ResultRoundingHalfEven);
    m_actions.settingsResultRoundingTowardZero->setCheckable(true);
    m_actions.settingsResultRoundingTowardZero->setData(Settings::ResultRoundingTowardZero);
    m_actions.settingsResultRoundingTowardPositiveInfinity->setCheckable(true);
    m_actions.settingsResultRoundingTowardPositiveInfinity->setData(Settings::ResultRoundingTowardPositiveInfinity);
    m_actions.settingsResultRoundingTowardNegativeInfinity->setCheckable(true);
    m_actions.settingsResultRoundingTowardNegativeInfinity->setData(Settings::ResultRoundingTowardNegativeInfinity);
    m_actions.settingsResultFormatAutoPrecision->setCheckable(true);
    m_actions.settingsResultFormatBinary->setCheckable(true);
    m_actions.settingsResultFormatCartesian->setCheckable(true);
    m_actions.settingsResultFormatEngineering->setCheckable(true);
    m_actions.settingsResultFormatFixed->setCheckable(true);
    m_actions.settingsResultFormatGeneral->setCheckable(true);
    m_actions.settingsResultFormatHexadecimal->setCheckable(true);
    m_actions.settingsResultFormatOctal->setCheckable(true);
    m_actions.settingsResultFormatPolar->setCheckable(true);
    m_actions.settingsResultFormatPolarAngle->setCheckable(true);
    m_actions.settingsImaginaryUnitI->setCheckable(true);
    m_actions.settingsImaginaryUnitJ->setCheckable(true);
    m_actions.settingsResultFormatRational->setCheckable(true);
    m_actions.settingsResultFormatScientific->setCheckable(true);
    m_actions.settingsResultFormatSexagesimal->setCheckable(true);
    m_actions.settingsUnitNegativeExponentSuperscript->setCheckable(true);
    m_actions.settingsUnitNegativeExponentSuperscript->setData(
        static_cast<int>(Settings::UnitNegativeExponentSuperscript));
    m_actions.settingsUnitNegativeExponentFraction->setCheckable(true);
    m_actions.settingsUnitNegativeExponentFraction->setData(
        static_cast<int>(Settings::UnitNegativeExponentFraction));
    m_actions.viewConstants->setCheckable(true);
    m_actions.viewFullScreenMode->setCheckable(true);
    m_actions.viewFunctions->setCheckable(true);
    m_actions.viewHistory->setCheckable(true);
    m_actions.viewKeypadDisabled->setCheckable(true);
    m_actions.viewKeypadDisabled->setData(Settings::KeypadModeDisabled);
    m_actions.viewKeypadBasicWide->setCheckable(true);
    m_actions.viewKeypadBasicWide->setData(Settings::KeypadModeBasicWide);
    m_actions.viewKeypadScientificWide->setCheckable(true);
    m_actions.viewKeypadScientificWide->setData(Settings::KeypadModeScientificWide);
    m_actions.viewKeypadScientificNarrow->setCheckable(true);
    m_actions.viewKeypadScientificNarrow->setData(Settings::KeypadModeScientificNarrow);
    m_actions.viewKeypadCustom->setCheckable(true);
    m_actions.viewKeypadCustom->setData(Settings::KeypadModeCustom);
    m_actions.viewKeypadZoom100->setCheckable(true);
    m_actions.viewKeypadZoom100->setData(100);
    m_actions.viewKeypadZoom150->setCheckable(true);
    m_actions.viewKeypadZoom150->setData(150);
    m_actions.viewKeypadZoom200->setCheckable(true);
    m_actions.viewKeypadZoom200->setData(200);
    m_actions.viewFormulaBook->setCheckable(true);
    m_actions.viewStatusBar->setCheckable(true);
    m_actions.viewMenuBar->setCheckable(true);
    m_actions.viewVariables->setCheckable(true);
    m_actions.viewBitfield->setCheckable(true);
    m_actions.viewUserFunctions->setCheckable(true);
    m_actions.viewUserUnits->setCheckable(true);

    const auto schemes = ColorScheme::enumerate(); // TODO: use qAsConst().
    for (auto& colorScheme : schemes) {
        auto action = new QAction(this);
        action->setCheckable(true);
        action->setText(colorScheme);
        action->setData(colorScheme);
        m_actions.settingsDisplayColorSchemes.append(action);
    }
}

void MainWindow::retranslateText()
{
    QTranslator* tr = 0;
    tr = createTranslator(m_settings->language);
    if (tr) {
        if (m_translator) {
            qApp->removeTranslator(m_translator);
            m_translator->deleteLater();
        }

        qApp->installTranslator(tr);
        m_translator = tr;
    } else {
        qApp->removeTranslator(m_translator);
        m_translator = 0;
    }

    setMenusText();
    setActionsText();
    setStatusBarText();
    setWidgetsDirection();
}

void MainWindow::setStatusBarText()
{
    if (m_status.angleUnit) {
        m_status.angleUnitLabel->setText(MainWindow::tr("Angle Mode:"));
        m_status.resultFormatLabel->setText(MainWindow::tr("Notation:"));
        m_status.resultPrecisionLabel->setText(MainWindow::tr("Precision:"));
        m_status.complexFormLabel->setText(MainWindow::tr("Complex Form:"));
        m_status.angleUnit->setText(statusBarAngleUnitValue());
        m_status.resultFormat->setText(statusBarResultFormatValue());
        m_status.resultPrecision->setText(statusBarResultPrecisionValue());
        m_status.complexForm->setText(statusBarComplexFormValue());

        m_status.angleUnit->setToolTip(MainWindow::tr("Angle unit"));
        m_status.resultFormat->setToolTip(MainWindow::tr("Result notation"));
        m_status.resultPrecision->setToolTip(MainWindow::tr("Result precision"));
        m_status.complexForm->setToolTip(MainWindow::tr("Complex form"));
        updateStatusBarSectionVisibility();
    }
}

void MainWindow::updateStatusBarSectionVisibility()
{
    if (!m_status.angleUnit)
        return;

    QStatusBar* bar = statusBar();
    const int availableWidth = bar->contentsRect().width();
    const int spacing = qMax(0, bar->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, bar));

    struct Section {
        QWidget* widget;
        bool enabled;
    };

    const Section sections[] = {
        { m_status.resultFormatSection, true },
        { m_status.resultPrecisionSection, true },
        { m_status.angleUnitSection, true },
        { m_status.complexFormSection, m_settings->complexNumbers }
    };

    int usedWidth = 0;
    bool hasVisibleSection = false;
    for (const Section& section : sections) {
        if (!section.enabled) {
            section.widget->setVisible(false);
            continue;
        }

        const int sectionWidth = section.widget->sizeHint().width();
        const int gap = hasVisibleSection ? spacing : 0;
        if (usedWidth + gap + sectionWidth <= availableWidth) {
            section.widget->setVisible(true);
            usedWidth += gap + sectionWidth;
            hasVisibleSection = true;
        } else {
            section.widget->setVisible(false);
        }
    }
}

QString MainWindow::statusBarAngleUnitValue() const
{
    return (m_settings->angleUnit == 'r' ? MainWindow::tr("Radian")
        : (m_settings->angleUnit == 'g') ? MainWindow::tr("Gradian")
        : (m_settings->angleUnit == 't') ? MainWindow::tr("Turn")
        : (m_settings->angleUnit == 'v') ? MainWindow::tr("Revolution")
        : MainWindow::tr("Degree"));
}

QString MainWindow::statusBarResultFormatValue() const
{
    switch (m_settings->resultFormat) {
        case 'b': return MainWindow::tr("Binary");
        case 'o': return MainWindow::tr("Octal");
        case 'h': return MainWindow::tr("Hexadecimal");
        case 's': return MainWindow::tr("Sexagesimal");
        case 'f': return MainWindow::tr("Fixed-point decimal");
        case 'n': return MainWindow::tr("Engineering decimal");
        case 'e': return MainWindow::tr("Scientific decimal");
        case 'r': return MainWindow::tr("Rational");
        case 'g': return MainWindow::tr("Automatic decimal");
        default : return QString();
    }
}

QString MainWindow::statusBarResultPrecisionValue() const
{
    if (m_settings->resultPrecision < 0)
        return MainWindow::tr("Automatic");
    return QString::number(m_settings->resultPrecision);
}

QString MainWindow::statusBarComplexFormValue() const
{
    if (m_settings->resultFormatComplex == 'p')
        return MainWindow::tr("Polar (Exponential)");
    if (m_settings->resultFormatComplex == 'a')
        return MainWindow::tr("Polar (Angle)");
    return MainWindow::tr("Rectangular (Cartesian)");
}

void MainWindow::setActionsText()
{
    m_actions.sessionExportHtml->setText(MainWindow::tr("&HTML"));
    m_actions.sessionExportPlainText->setText(MainWindow::tr("Plain &text"));
    m_actions.sessionImport->setText(MainWindow::tr("&Import..."));
    m_actions.sessionImportUserDefinitions->setText(MainWindow::tr("Startup &Definitions..."));
    m_actions.sessionLoad->setText(MainWindow::tr("&Load..."));
    m_actions.sessionQuit->setText(MainWindow::tr("&Quit"));
    m_actions.sessionSave->setText(MainWindow::tr("&Save..."));

    m_actions.editClearExpression->setText(MainWindow::tr("Clear E&xpression"));
    m_actions.editClearHistory->setText(MainWindow::tr("Clear &History"));
    m_actions.editCopyLastResult->setText(MainWindow::tr("Copy Last &Result"));
    m_actions.editCopy->setText(MainWindow::tr("&Copy"));
    m_actions.editPaste->setText(MainWindow::tr("&Paste"));
    m_actions.editSelectExpression->setText(MainWindow::tr("&Select Expression"));
    m_actions.editWrapSelection->setText(MainWindow::tr("&Wrap Selection in Parentheses"));

    m_actions.viewConstants->setText(MainWindow::tr("&Constants"));
    m_actions.viewFullScreenMode->setText(MainWindow::tr("F&ull Screen Mode"));
    m_actions.viewFunctions->setText(MainWindow::tr("&Functions"));
    m_actions.viewHistory->setText(MainWindow::tr("&History"));
    updateKeypadDisabledActionText();
    m_actions.viewKeypadBasicWide->setText(MainWindow::tr("&Basic"));
    m_actions.viewKeypadScientificWide->setText(MainWindow::tr("&Scientific (wide)"));
    m_actions.viewKeypadScientificNarrow->setText(MainWindow::tr("Scientific (narrow)"));
    m_actions.viewKeypadCustom->setText(MainWindow::tr("&Custom..."));
    m_actions.viewKeypadZoom100->setText(MainWindow::tr("100%"));
    m_actions.viewKeypadZoom150->setText(MainWindow::tr("150%"));
    m_actions.viewKeypadZoom200->setText(MainWindow::tr("200%"));
    m_actions.viewFormulaBook->setText(MainWindow::tr("Formula &Book"));
    m_actions.viewStatusBar->setText(MainWindow::tr("&Status Bar"));
    m_actions.viewMenuBar->setText(MainWindow::tr("Main &Menu"));
    m_actions.viewVariables->setText(MainWindow::tr("User &Variables"));
    m_actions.viewBitfield->setText(MainWindow::tr("Bitfield"));
    m_actions.viewUserFunctions->setText(MainWindow::tr("Use&r Functions"));
    m_actions.viewUserUnits->setText(MainWindow::tr("User &Units"));

    m_actions.settingsAngleUnitDegree->setText(MainWindow::tr("&Degree"));
    m_actions.settingsAngleUnitRadian->setText(MainWindow::tr("&Radian"));
    m_actions.settingsAngleUnitGradian->setText(MainWindow::tr("&Gradian"));
    m_actions.settingsAngleUnitTurn->setText(MainWindow::tr("&Turn"));
    m_actions.settingsAngleUnitRevolution->setText(MainWindow::tr("&Revolution"));
    m_actions.settingsBehaviorAlwaysOnTop->setText(MainWindow::tr("Always on &Top"));
    m_actions.settingsBehaviorAutoAns->setText(MainWindow::tr("Auto-Insert \"ans\" When Starting with an Operator"));
    m_actions.settingsBehaviorAutoAns->setToolTip(MainWindow::tr("If a new expression starts with +, -, *, or /, SpeedCrunch inserts \"ans\" first."));
    m_actions.settingsBehaviorAutoAns->setStatusTip(MainWindow::tr("If a new expression starts with +, -, *, or /, SpeedCrunch inserts \"ans\" first."));
    m_actions.settingsBehaviorAutoCompletion->setText(MainWindow::tr("Automatic &Completion"));
    m_actions.settingsBehaviorAutoCompletionBuiltInFunctions->setText(MainWindow::tr("Built-in &functions"));
    m_actions.settingsBehaviorAutoCompletionBuiltInVariables->setText(MainWindow::tr("Built-in &variables"));
    m_actions.settingsBehaviorAutoCompletionLongFormUnits->setText(MainWindow::tr("&Units"));
    m_actions.settingsBehaviorAutoCompletionUserFunctions->setText(MainWindow::tr("User &functions"));
    m_actions.settingsBehaviorAutoCompletionUserVariables->setText(MainWindow::tr("User &variables"));
    m_actions.settingsBehaviorEmptyHistoryHint->setText(MainWindow::tr("Show Empty History &Hint"));
    m_actions.settingsBehaviorEmptyHistoryHint->setToolTip(MainWindow::tr("When history is empty, show a hint in the status area."));
    m_actions.settingsBehaviorEmptyHistoryHint->setStatusTip(MainWindow::tr("When history is empty, show a hint in the status area."));
    m_actions.settingsBehaviorPartialResults->setText(MainWindow::tr("Show Live Result &Preview"));
    m_actions.settingsBehaviorHistorySavingNever->setText(MainWindow::tr("&Never"));
    m_actions.settingsBehaviorHistorySavingOnExit->setText(MainWindow::tr("On &Exit"));
    m_actions.settingsBehaviorHistorySavingContinuously->setText(MainWindow::tr("&Continuously"));
    m_actions.settingsBehaviorSaveWindowPositionOnExit->setText(MainWindow::tr("Save &Window Position on Exit"));
    m_actions.settingsBehaviorSingleInstance->setText(MainWindow::tr("Single &Instance"));
    m_actions.settingsBehaviorSingleInstance->setToolTip(MainWindow::tr("When enabled, launching SpeedCrunch again focuses the existing window instead of opening another instance."));
    m_actions.settingsBehaviorSingleInstance->setStatusTip(MainWindow::tr("When enabled, launching SpeedCrunch again focuses the existing window instead of opening another instance."));
    m_actions.settingsBehaviorSyntaxHighlighting->setText(MainWindow::tr("Syntax &Highlighting"));
    m_actions.settingsBehaviorHoverHighlightResults->setText(MainWindow::tr("Hover Highlighting"));
    m_actions.settingsBehaviorDigitGroupingNone->setText(MainWindow::tr("Disabled"));
    m_actions.settingsBehaviorDigitGroupingOneSpace->setText(MainWindow::tr("Small Space"));
    m_actions.settingsBehaviorDigitGroupingTwoSpaces->setText(MainWindow::tr("Medium Space"));
    m_actions.settingsBehaviorDigitGroupingThreeSpaces->setText(MainWindow::tr("Large Space"));
    m_actions.settingsBehaviorDigitGroupingIntegerPartOnly->setText(MainWindow::tr("Group Integer Part Only"));
    m_actions.settingsBehaviorLeaveLastExpression->setText(MainWindow::tr("Keep Entered Expression After Evaluate"));
    m_actions.settingsBehaviorNumberFormat->setText(MainWindow::tr("Number Format..."));
    m_actions.settingsBehaviorResultSlots->setText(MainWindow::tr("Notation && Precision..."));
    m_actions.settingsBehaviorLeaveLastExpression->setToolTip(MainWindow::tr("After pressing Enter, keep the entered expression selected in the editor."));
    m_actions.settingsBehaviorLeaveLastExpression->setStatusTip(MainWindow::tr("After pressing Enter, keep the entered expression selected in the editor."));
    m_actions.settingsBehaviorUpDownArrowNever->setText(MainWindow::tr("Never"));
    m_actions.settingsBehaviorUpDownArrowAlways->setText(MainWindow::tr("Always"));
    m_actions.settingsBehaviorUpDownArrowSingleLineOnly->setText(MainWindow::tr("Only for Single-Line Expressions"));
    m_actions.settingsBehaviorAutoResultToClipboard->setText(MainWindow::tr("Automatically Copy New Results to Clipboard"));
    m_actions.settingsBehaviorSimplifyResultExpressions->setText(MainWindow::tr("Simplify Displayed Expressions"));
    m_actions.settingsBehaviorHistorySizeLimit->setText(MainWindow::tr("History Size &Limit..."));
    updateComplexDisabledActionText();
    m_actions.settingsRadixCharComma->setText(MainWindow::tr("&Comma"));
    m_actions.settingsRadixCharDefault->setText(MainWindow::tr("&System Default"));
    m_actions.settingsRadixCharDot->setText(MainWindow::tr("&Dot"));
    m_actions.settingsRadixCharBoth->setText(MainWindow::tr("Dot &And Comma"));
    m_actions.settingsResultFormat0Digits->setText(MainWindow::tr("&0 Digits"));
    m_actions.settingsResultFormat15Digits->setText(MainWindow::tr("&15 Digits"));
    m_actions.settingsResultFormat2Digits->setText(MainWindow::tr("&2 Digits"));
    m_actions.settingsResultFormat3Digits->setText(MainWindow::tr("&3 Digits"));
    m_actions.settingsResultFormat50Digits->setText(MainWindow::tr("&50 Digits"));
    m_actions.settingsResultFormat8Digits->setText(MainWindow::tr("&8 Digits"));
    m_actions.settingsResultFormatCustomDigits->setText(MainWindow::tr("&Custom..."));
    m_actions.settingsResultRoundingHalfAwayFromZero->setText(
        MainWindow::tr("Half Away from Zero (&Arithmetic)"));
    m_actions.settingsResultRoundingHalfEven->setText(
        MainWindow::tr("Half &Even (Banker's)"));
    m_actions.settingsResultRoundingTowardZero->setText(MainWindow::tr("Toward &Zero"));
    m_actions.settingsResultRoundingTowardPositiveInfinity->setText(MainWindow::tr("Toward +&Infinity"));
    m_actions.settingsResultRoundingTowardNegativeInfinity->setText(MainWindow::tr("Toward -I&nfinity"));
    m_actions.settingsResultFormatAutoPrecision->setText(MainWindow::tr("&Automatic"));
    m_actions.settingsResultFormatGeneral->setText(MainWindow::tr("&Automatic"));
    m_actions.settingsResultFormatFixed->setText(MainWindow::tr("&Fixed-Point"));
    m_actions.settingsResultFormatEngineering->setText(MainWindow::tr("&Engineering"));
    m_actions.settingsResultFormatScientific->setText(MainWindow::tr("&Scientific"));
    m_actions.settingsResultFormatRational->setText(MainWindow::tr("&Rational"));
    m_actions.settingsResultFormatBinary->setText(MainWindow::tr("&Binary"));
    m_actions.settingsResultFormatOctal->setText(MainWindow::tr("&Octal"));
    m_actions.settingsResultFormatHexadecimal->setText(MainWindow::tr("&Hexadecimal"));
    m_actions.settingsResultFormatSexagesimal->setText(MainWindow::tr("&Sexagesimal"));
    m_actions.settingsUnitNegativeExponentSuperscript->setText(
        MainWindow::tr("Superscript &Exponents"));
    m_actions.settingsUnitNegativeExponentFraction->setText(
        MainWindow::tr("&Fraction Form"));
    m_actions.settingsResultFormatCartesian->setText(MainWindow::tr("&Rectangular (Cartesian)"));
    m_actions.settingsResultFormatPolar->setText(MainWindow::tr("Polar (&Exponential)"));
    m_actions.settingsResultFormatPolarAngle->setText(MainWindow::tr("Polar (&Angle)"));
    m_actions.settingsImaginaryUnitI->setText(MainWindow::tr("Imaginary Unit &i"));
    m_actions.settingsImaginaryUnitJ->setText(MainWindow::tr("Imaginary Unit &j"));
    m_actions.settingsDisplayFont->setText(MainWindow::tr("&Font..."));
    m_actions.settingsDisplayColorSchemeCustom->setText(MainWindow::tr("&Custom..."));
    m_actions.settingsLanguage->setText(MainWindow::tr("&Language..."));

    m_actions.helpManual->setText(MainWindow::tr("User &Manual"));
    m_actions.contextHelp->setText(MainWindow::tr("Context Help"));
    m_actions.helpUpdates->setText(MainWindow::tr("Check for &Updates"));
    m_actions.helpFeedback->setText(MainWindow::tr("Send &Feedback"));
    m_actions.helpCommunity->setText(MainWindow::tr("Join &Community"));
    m_actions.helpNews->setText(MainWindow::tr("&News Feed"));
    m_actions.helpDonate->setText(MainWindow::tr("&Donate"));
    m_actions.helpAbout->setText(MainWindow::tr("About &SpeedCrunch"));
}

void MainWindow::createActionGroups()
{
    m_actionGroups.resultFormat = new QActionGroup(this);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatBinary);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatGeneral);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatFixed);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatEngineering);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatScientific);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatRational);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatOctal);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatHexadecimal);
    m_actionGroups.resultFormat->addAction(m_actions.settingsResultFormatSexagesimal);


    m_actionGroups.complexFormat = new QActionGroup(this);
    m_actionGroups.complexFormat->addAction(m_actions.settingsResultFormatCartesian);
    m_actionGroups.complexFormat->addAction(m_actions.settingsResultFormatPolar);
    m_actionGroups.complexFormat->addAction(m_actions.settingsResultFormatPolarAngle);

    m_actionGroups.imaginaryUnit = new QActionGroup(this);
    m_actionGroups.imaginaryUnit->addAction(m_actions.settingsResultFormatComplexDisabled);
    m_actionGroups.imaginaryUnit->addAction(m_actions.settingsImaginaryUnitI);
    m_actionGroups.imaginaryUnit->addAction(m_actions.settingsImaginaryUnitJ);

    m_actionGroups.radixChar = new QActionGroup(this);
    m_actionGroups.radixChar->addAction(m_actions.settingsRadixCharDefault);
    m_actionGroups.radixChar->addAction(m_actions.settingsRadixCharDot);
    m_actionGroups.radixChar->addAction(m_actions.settingsRadixCharComma);
    m_actionGroups.radixChar->addAction(m_actions.settingsRadixCharBoth);

    m_actionGroups.digits = new QActionGroup(this);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormatAutoPrecision);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat0Digits);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat2Digits);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat3Digits);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat8Digits);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat15Digits);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormat50Digits);
    m_actionGroups.digits->addAction(m_actions.settingsResultFormatCustomDigits);

    m_actionGroups.resultRoundingMode = new QActionGroup(this);
    m_actionGroups.resultRoundingMode->addAction(m_actions.settingsResultRoundingHalfAwayFromZero);
    m_actionGroups.resultRoundingMode->addAction(m_actions.settingsResultRoundingHalfEven);
    m_actionGroups.resultRoundingMode->addAction(m_actions.settingsResultRoundingTowardZero);
    m_actionGroups.resultRoundingMode->addAction(m_actions.settingsResultRoundingTowardPositiveInfinity);
    m_actionGroups.resultRoundingMode->addAction(m_actions.settingsResultRoundingTowardNegativeInfinity);

    m_actionGroups.angle = new QActionGroup(this);
    m_actionGroups.angle->addAction(m_actions.settingsAngleUnitDegree);
    m_actionGroups.angle->addAction(m_actions.settingsAngleUnitRadian);
    m_actionGroups.angle->addAction(m_actions.settingsAngleUnitGradian);
    m_actionGroups.angle->addAction(m_actions.settingsAngleUnitTurn);
    m_actionGroups.angle->addAction(m_actions.settingsAngleUnitRevolution);

    m_actionGroups.colorScheme = new QActionGroup(this);
    const auto schemes = m_actions.settingsDisplayColorSchemes;
    for (auto& action : schemes)
        m_actionGroups.colorScheme->addAction(action);

    m_actionGroups.digitGrouping = new QActionGroup(this);
    m_actionGroups.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingNone);
    m_actionGroups.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingOneSpace);
    m_actionGroups.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingTwoSpaces);
    m_actionGroups.digitGrouping->addAction(m_actions.settingsBehaviorDigitGroupingThreeSpaces);

    m_actionGroups.historySaving = new QActionGroup(this);
    m_actionGroups.historySaving->addAction(m_actions.settingsBehaviorHistorySavingNever);
    m_actionGroups.historySaving->addAction(m_actions.settingsBehaviorHistorySavingOnExit);
    m_actionGroups.historySaving->addAction(m_actions.settingsBehaviorHistorySavingContinuously);

    m_actionGroups.upDownArrowBehavior = new QActionGroup(this);
    m_actionGroups.upDownArrowBehavior->addAction(m_actions.settingsBehaviorUpDownArrowNever);
    m_actionGroups.upDownArrowBehavior->addAction(m_actions.settingsBehaviorUpDownArrowAlways);
    m_actionGroups.upDownArrowBehavior->addAction(m_actions.settingsBehaviorUpDownArrowSingleLineOnly);

    m_actionGroups.keypad = new QActionGroup(this);
    m_actionGroups.keypad->addAction(m_actions.viewKeypadDisabled);
    m_actionGroups.keypad->addAction(m_actions.viewKeypadBasicWide);
    m_actionGroups.keypad->addAction(m_actions.viewKeypadScientificWide);
    m_actionGroups.keypad->addAction(m_actions.viewKeypadScientificNarrow);
    m_actionGroups.keypad->addAction(m_actions.viewKeypadCustom);

    m_actionGroups.keypadZoom = new QActionGroup(this);
    m_actionGroups.keypadZoom->addAction(m_actions.viewKeypadZoom100);
    m_actionGroups.keypadZoom->addAction(m_actions.viewKeypadZoom150);
    m_actionGroups.keypadZoom->addAction(m_actions.viewKeypadZoom200);

    m_actionGroups.unitNegativeExponentStyle = new QActionGroup(this);
    m_actionGroups.unitNegativeExponentStyle->addAction(
        m_actions.settingsUnitNegativeExponentSuperscript);
    m_actionGroups.unitNegativeExponentStyle->addAction(
        m_actions.settingsUnitNegativeExponentFraction);
}

void MainWindow::createActionShortcuts()
{
    m_actions.sessionLoad->setShortcut(Qt::CTRL | Qt::Key_L);
    m_actions.sessionQuit->setShortcut(Qt::CTRL | Qt::Key_Q);
    m_actions.sessionSave->setShortcut(Qt::CTRL | Qt::Key_S);
    m_actions.editClearHistory->setShortcut(Qt::CTRL | Qt::Key_N);
    m_actions.editCopyLastResult->setShortcut(Qt::CTRL | Qt::Key_R);
    m_actions.editCopy->setShortcut(Qt::CTRL | Qt::Key_C);
    m_actions.editPaste->setShortcut(Qt::CTRL | Qt::Key_V);
    m_actions.editSelectExpression->setShortcut(Qt::CTRL | Qt::Key_A);
    m_actions.editWrapSelection->setShortcuts({
        QKeySequence(Qt::CTRL | Qt::Key_ParenLeft),
        QKeySequence(Qt::CTRL | Qt::Key_ParenRight)
    });
    m_actions.viewBitfield->setShortcut(Qt::CTRL | Qt::Key_6);
    m_actions.viewConstants->setShortcut(Qt::CTRL | Qt::Key_2);
    m_actions.viewFullScreenMode->setShortcut(Qt::Key_F11);
    m_actions.viewFunctions->setShortcut(Qt::CTRL | Qt::Key_3);
    m_actions.viewHistory->setShortcut(Qt::CTRL | Qt::Key_7);
    m_actions.viewFormulaBook->setShortcut(Qt::CTRL | Qt::Key_1);
    m_actions.viewStatusBar->setShortcut(Qt::CTRL | Qt::Key_B);
    m_actions.viewVariables->setShortcut(Qt::CTRL | Qt::Key_4);
    m_actions.viewUserFunctions->setShortcut(Qt::CTRL | Qt::Key_5);
    m_actions.viewUserUnits->setShortcut(Qt::CTRL | Qt::Key_8);
    m_actions.settingsResultFormatGeneral->setShortcut(Qt::Key_F2);
    m_actions.settingsResultFormatFixed->setShortcut(Qt::Key_F3);
    m_actions.settingsResultFormatEngineering->setShortcut(Qt::Key_F4);
    m_actions.settingsResultFormatScientific->setShortcut(Qt::Key_F5);
    m_actions.settingsResultFormatBinary->setShortcut(Qt::Key_F6);
    m_actions.settingsResultFormatOctal->setShortcut(Qt::Key_F7);
    m_actions.settingsResultFormatHexadecimal->setShortcut(Qt::Key_F8);
    m_actions.settingsResultFormatSexagesimal->setShortcut(Qt::Key_F9);
    m_actions.contextHelp->setShortcut(Qt::Key_F1);
}

void MainWindow::createMenus()
{
    m_menus.session = new QMenu("", this);
    menuBar()->addMenu(m_menus.session);
    m_menus.session->addAction(m_actions.sessionLoad);
    m_menus.session->addAction(m_actions.sessionSave);
    m_menus.session->addSeparator();
    m_menus.session->addAction(m_actions.sessionImport);
    m_menus.session->addAction(m_actions.sessionImportUserDefinitions);
    m_menus.sessionExport = m_menus.session->addMenu("");
    m_menus.sessionExport->addAction(m_actions.sessionExportPlainText);
    m_menus.sessionExport->addAction(m_actions.sessionExportHtml);
    m_menus.session->addSeparator();
    m_menus.session->addAction(m_actions.sessionQuit);

    m_menus.edit = new QMenu("", this);
    menuBar()->addMenu(m_menus.edit);
    m_menus.edit->addAction(m_actions.editCopy);
    m_menus.edit->addAction(m_actions.editCopyLastResult);
    m_menus.edit->addAction(m_actions.editPaste);
    m_menus.edit->addAction(m_actions.editSelectExpression);
    m_menus.edit->addAction(m_actions.editClearExpression);
    m_menus.edit->addAction(m_actions.editClearHistory);
    m_menus.edit->addAction(m_actions.editWrapSelection);

    m_menus.view = new QMenu("", this);
    menuBar()->addMenu(m_menus.view);
    m_menus.keypad = m_menus.view->addMenu("");
    m_menus.keypad->addAction(m_actions.viewKeypadDisabled);
    m_menus.keypad->addAction(m_actions.viewKeypadBasicWide);
    m_menus.keypad->addAction(m_actions.viewKeypadScientificWide);
    m_menus.keypad->addAction(m_actions.viewKeypadScientificNarrow);
    m_menus.keypad->addAction(m_actions.viewKeypadCustom);
    m_menus.keypad->addSeparator();
    m_menus.keypadZoom = m_menus.keypad->addMenu("");
    m_menus.keypadZoom->addAction(m_actions.viewKeypadZoom100);
    m_menus.keypadZoom->addAction(m_actions.viewKeypadZoom150);
    m_menus.keypadZoom->addAction(m_actions.viewKeypadZoom200);
    m_menus.view->addSeparator();
    m_menus.view->addAction(m_actions.viewFormulaBook);
    m_menus.view->addAction(m_actions.viewConstants);
    m_menus.view->addAction(m_actions.viewFunctions);
    m_menus.view->addAction(m_actions.viewVariables);
    m_menus.view->addAction(m_actions.viewUserFunctions);
    m_menus.view->addAction(m_actions.viewUserUnits);
    m_menus.view->addAction(m_actions.viewBitfield);
    m_menus.view->addAction(m_actions.viewHistory);
    m_menus.view->addSeparator();
    m_menus.view->addAction(m_actions.viewStatusBar);
#if !defined(Q_OS_MACOS)
    m_menus.view->addAction(m_actions.viewMenuBar);
#endif
    m_menus.view->addSeparator();
    m_menus.view->addAction(m_actions.viewFullScreenMode);

    m_menus.settings = new QMenu("", this);
    menuBar()->addMenu(m_menus.settings);

    m_menus.display = m_menus.settings->addMenu("");
    m_menus.colorScheme = m_menus.display->addMenu("");
    const auto schemes = m_actions.settingsDisplayColorSchemes;
    for (auto& action : schemes)
        m_menus.colorScheme->addAction(action);
    m_menus.colorScheme->addSeparator();
    m_menus.colorScheme->addAction(m_actions.settingsDisplayColorSchemeCustom);
    m_menus.display->addAction(m_actions.settingsDisplayFont);
    m_menus.display->addSeparator();
    m_menus.display->addAction(m_actions.settingsBehaviorSyntaxHighlighting);
    m_menus.display->addAction(m_actions.settingsBehaviorHoverHighlightResults);
    m_menus.editing = m_menus.settings->addMenu("");
    m_menus.autoCompletion = m_menus.editing->addMenu("");
    m_menus.autoCompletion->addAction(m_actions.settingsBehaviorAutoCompletionBuiltInFunctions);
    m_menus.autoCompletion->addAction(m_actions.settingsBehaviorAutoCompletionBuiltInVariables);
    m_menus.autoCompletion->addAction(m_actions.settingsBehaviorAutoCompletionLongFormUnits);
    m_menus.autoCompletion->addAction(m_actions.settingsBehaviorAutoCompletionUserFunctions);
    m_menus.autoCompletion->addAction(m_actions.settingsBehaviorAutoCompletionUserVariables);
    m_menus.editing->addAction(m_actions.settingsBehaviorAutoAns);
    m_menus.editing->addAction(m_actions.settingsBehaviorEmptyHistoryHint);
    m_menus.editing->addAction(m_actions.settingsBehaviorLeaveLastExpression);
    m_menus.upDownArrowBehavior = m_menus.editing->addMenu("");
    m_menus.upDownArrowBehavior->addAction(m_actions.settingsBehaviorUpDownArrowNever);
    m_menus.upDownArrowBehavior->addAction(m_actions.settingsBehaviorUpDownArrowAlways);
    m_menus.upDownArrowBehavior->addAction(m_actions.settingsBehaviorUpDownArrowSingleLineOnly);

    m_menus.results = m_menus.settings->addMenu("");
    m_menus.results->addAction(m_actions.settingsBehaviorNumberFormat);
    m_menus.results->addAction(m_actions.settingsBehaviorResultSlots);
    m_menus.results->addSeparator();
    m_menus.unitNegativeExponentStyle = m_menus.results->addMenu("");
    m_menus.unitNegativeExponentStyle->addAction(
        m_actions.settingsUnitNegativeExponentSuperscript);
    m_menus.unitNegativeExponentStyle->addAction(
        m_actions.settingsUnitNegativeExponentFraction);
    m_menus.resultRoundingMode = m_menus.results->addMenu("");
    m_menus.resultRoundingMode->addAction(m_actions.settingsResultRoundingHalfAwayFromZero);
    m_menus.resultRoundingMode->addAction(m_actions.settingsResultRoundingHalfEven);
    m_menus.resultRoundingMode->addAction(m_actions.settingsResultRoundingTowardZero);
    m_menus.resultRoundingMode->addAction(m_actions.settingsResultRoundingTowardPositiveInfinity);
    m_menus.resultRoundingMode->addAction(m_actions.settingsResultRoundingTowardNegativeInfinity);
    m_menus.results->addSeparator();

    // Deprecated direct menus kept as internal context menus only; users should
    // configure these via "Notation & Precision...".
    m_menus.resultFormat = new QMenu("", this);
    m_menus.decimal = m_menus.resultFormat->addMenu("");
    m_menus.decimal->addAction(m_actions.settingsResultFormatGeneral);
    m_menus.decimal->addAction(m_actions.settingsResultFormatFixed);
    m_menus.decimal->addAction(m_actions.settingsResultFormatEngineering);
    m_menus.decimal->addAction(m_actions.settingsResultFormatScientific);
    m_menus.resultFormat->addAction(m_actions.settingsResultFormatRational);
    m_menus.resultFormat->addAction(m_actions.settingsResultFormatBinary);
    m_menus.resultFormat->addAction(m_actions.settingsResultFormatOctal);
    m_menus.resultFormat->addAction(m_actions.settingsResultFormatHexadecimal);
    m_menus.resultFormat->addAction(m_actions.settingsResultFormatSexagesimal);

    m_menus.precision = new QMenu("", this);
    m_menus.precision->addAction(m_actions.settingsResultFormatAutoPrecision);
    m_menus.precision->addAction(m_actions.settingsResultFormat0Digits);
    m_menus.precision->addAction(m_actions.settingsResultFormat2Digits);
    m_menus.precision->addAction(m_actions.settingsResultFormat3Digits);
    m_menus.precision->addAction(m_actions.settingsResultFormat8Digits);
    m_menus.precision->addAction(m_actions.settingsResultFormat15Digits);
    m_menus.precision->addAction(m_actions.settingsResultFormat50Digits);
    m_menus.precision->addSeparator();
    m_menus.precision->addAction(m_actions.settingsResultFormatCustomDigits);

    m_menus.complexForm = new QMenu("", this);
    m_menus.complexForm->addAction(m_actions.settingsResultFormatCartesian);
    m_menus.complexForm->addAction(m_actions.settingsResultFormatPolar);
    m_menus.complexForm->addAction(m_actions.settingsResultFormatPolarAngle);

    m_menus.complexNumbers = new QMenu("", this);
    m_menus.complexNumbers->addAction(m_actions.settingsResultFormatComplexDisabled);
    m_menus.complexNumbers->addAction(m_actions.settingsImaginaryUnitI);
    m_menus.complexNumbers->addAction(m_actions.settingsImaginaryUnitJ);

    m_menus.results->addAction(m_actions.settingsBehaviorPartialResults);
    m_menus.results->addAction(m_actions.settingsBehaviorSimplifyResultExpressions);
    m_menus.results->addAction(m_actions.settingsBehaviorAutoResultToClipboard);

    m_menus.angleUnit = m_menus.settings->addMenu("");
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitDegree);
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitRadian);
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitGradian);
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitTurn);
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitRevolution);

    m_menus.settings->addMenu(m_menus.complexNumbers);

    m_menus.history = m_menus.settings->addMenu("");
    m_menus.historySaving = m_menus.history->addMenu("");
    m_menus.historySaving->addAction(m_actions.settingsBehaviorHistorySavingNever);
    m_menus.historySaving->addAction(m_actions.settingsBehaviorHistorySavingOnExit);
    m_menus.historySaving->addAction(m_actions.settingsBehaviorHistorySavingContinuously);
    m_menus.history->addAction(m_actions.settingsBehaviorHistorySizeLimit);

    m_menus.window = m_menus.settings->addMenu("");
    m_menus.window->addAction(m_actions.settingsBehaviorSaveWindowPositionOnExit);
    m_menus.window->addAction(m_actions.settingsBehaviorSingleInstance);
    if (!isWaylandPlatform())
        m_menus.window->addAction(m_actions.settingsBehaviorAlwaysOnTop);

    m_menus.settings->addAction(m_actions.settingsLanguage);

    m_menus.help = new QMenu("", this);
    menuBar()->addMenu(m_menus.help);
    m_menus.help->addAction(m_actions.helpManual);
    m_menus.help->addAction(m_actions.contextHelp);
    m_menus.help->addSeparator();
    m_menus.help->addAction(m_actions.helpUpdates);
    m_menus.help->addAction(m_actions.helpFeedback);
    m_menus.help->addAction(m_actions.helpCommunity);
    m_menus.help->addAction(m_actions.helpNews);
    m_menus.help->addAction(m_actions.helpDonate);
    m_menus.help->addSeparator();
    m_menus.help->addAction(m_actions.helpAbout);

    addActions(menuBar()->actions());
}

void MainWindow::setMenusText()
{
    m_menus.session->setTitle(MainWindow::tr("&Session"));
    m_menus.sessionExport->setTitle(MainWindow::tr("&Export"));
    m_menus.edit->setTitle(MainWindow::tr("&Edit"));
    m_menus.view->setTitle(MainWindow::tr("&View"));
    m_menus.keypad->setTitle(MainWindow::tr("&Keypad"));
    m_menus.keypadZoom->setTitle(MainWindow::tr("&Zoom"));
    m_menus.settings->setTitle(MainWindow::tr("Se&ttings"));
    m_menus.results->setTitle(MainWindow::tr("&Results"));
    m_menus.unitNegativeExponentStyle->setTitle(MainWindow::tr("Unit Exponent Style"));
    m_menus.resultRoundingMode->setTitle(MainWindow::tr("Rounding Mode"));
    m_menus.resultFormat->setTitle(MainWindow::tr("&Notation"));
    m_menus.decimal->setTitle(MainWindow::tr("&Decimal"));
    m_menus.precision->setTitle(MainWindow::tr("&Precision"));
    m_menus.angleUnit->setTitle(MainWindow::tr("&Angle Mode"));
    m_menus.complexNumbers->setTitle(MainWindow::tr("Complex &Numbers"));
    m_menus.window->setTitle(MainWindow::tr("&Window"));
    m_menus.editing->setTitle(MainWindow::tr("&Editing"));
    m_menus.autoCompletion->setTitle(MainWindow::tr("A&utocomplete"));
    m_menus.upDownArrowBehavior->setTitle(MainWindow::tr("Up/Down Arrow History"));
    m_menus.history->setTitle(MainWindow::tr("&History"));
    m_menus.historySaving->setTitle(MainWindow::tr("History &Saving"));
    m_menus.display->setTitle(MainWindow::tr("&Appearance"));
    m_menus.colorScheme->setTitle(MainWindow::tr("Theme"));
    m_menus.help->setTitle(MainWindow::tr("&Help"));
}

void MainWindow::updateComplexDisabledActionText()
{
    if (m_actions.settingsResultFormatComplexDisabled->isChecked())
        m_actions.settingsResultFormatComplexDisabled->setText(MainWindow::tr("&Disabled"));
    else
        m_actions.settingsResultFormatComplexDisabled->setText(MainWindow::tr("&Disable"));
}

void MainWindow::updateKeypadDisabledActionText()
{
    if (m_actions.viewKeypadDisabled->isChecked())
        m_actions.viewKeypadDisabled->setText(MainWindow::tr("&Disabled"));
    else
        m_actions.viewKeypadDisabled->setText(MainWindow::tr("&Disable"));
}

void MainWindow::createStatusBar()
{
    QStatusBar* bar = statusBar();

    m_status.angleUnitSection = new QWidget(bar);
    m_status.resultFormatSection = new QWidget(bar);
    m_status.resultPrecisionSection = new QWidget(bar);
    m_status.complexFormSection = new QWidget(bar);

    m_status.angleUnitLabel = new QLabel(m_status.angleUnitSection);
    m_status.resultFormatLabel = new QLabel(m_status.resultFormatSection);
    m_status.resultPrecisionLabel = new QLabel(m_status.resultPrecisionSection);
    m_status.complexFormLabel = new QLabel(m_status.complexFormSection);

    m_status.angleUnit = new QPushButton(bar);
    m_status.resultFormat = new QPushButton(bar);
    m_status.resultPrecision = new QPushButton(bar);
    m_status.complexForm = new QPushButton(bar);

    m_status.angleUnit->setParent(m_status.angleUnitSection);
    m_status.resultFormat->setParent(m_status.resultFormatSection);
    m_status.resultPrecision->setParent(m_status.resultPrecisionSection);
    m_status.complexForm->setParent(m_status.complexFormSection);

    QHBoxLayout* angleLayout = new QHBoxLayout(m_status.angleUnitSection);
    QHBoxLayout* formatLayout = new QHBoxLayout(m_status.resultFormatSection);
    QHBoxLayout* precisionLayout = new QHBoxLayout(m_status.resultPrecisionSection);
    QHBoxLayout* complexFormLayout = new QHBoxLayout(m_status.complexFormSection);
    angleLayout->setContentsMargins(0, 0, 0, 0);
    formatLayout->setContentsMargins(0, 0, 0, 0);
    precisionLayout->setContentsMargins(0, 0, 0, 0);
    complexFormLayout->setContentsMargins(0, 0, 0, 0);
    angleLayout->setSpacing(2);
    formatLayout->setSpacing(2);
    precisionLayout->setSpacing(2);
    complexFormLayout->setSpacing(2);
    angleLayout->addWidget(m_status.angleUnitLabel);
    angleLayout->addWidget(m_status.angleUnit);
    formatLayout->addWidget(m_status.resultFormatLabel);
    formatLayout->addWidget(m_status.resultFormat);
    precisionLayout->addWidget(m_status.resultPrecisionLabel);
    precisionLayout->addWidget(m_status.resultPrecision);
    complexFormLayout->addWidget(m_status.complexFormLabel);
    complexFormLayout->addWidget(m_status.complexForm);

    QFont boldFont = m_status.angleUnitLabel->font();
    boldFont.setBold(true);
    m_status.angleUnitLabel->setFont(boldFont);
    m_status.resultFormatLabel->setFont(boldFont);
    m_status.resultPrecisionLabel->setFont(boldFont);
    m_status.complexFormLabel->setFont(boldFont);
    m_status.angleUnitLabel->setCursor(Qt::PointingHandCursor);
    m_status.resultFormatLabel->setCursor(Qt::PointingHandCursor);
    m_status.resultPrecisionLabel->setCursor(Qt::PointingHandCursor);
    m_status.complexFormLabel->setCursor(Qt::PointingHandCursor);
    m_status.angleUnitLabel->installEventFilter(this);
    m_status.resultFormatLabel->installEventFilter(this);
    m_status.resultPrecisionLabel->installEventFilter(this);
    m_status.complexFormLabel->installEventFilter(this);

    m_status.angleUnit->setFocusPolicy(Qt::NoFocus);
    m_status.resultFormat->setFocusPolicy(Qt::NoFocus);
    m_status.resultPrecision->setFocusPolicy(Qt::NoFocus);
    m_status.complexForm->setFocusPolicy(Qt::NoFocus);

    m_status.angleUnit->setFlat(true);
    m_status.resultFormat->setFlat(true);
    m_status.resultPrecision->setFlat(true);
    m_status.complexForm->setFlat(true);

    m_status.angleUnit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_status.angleUnit, SIGNAL(customContextMenuRequested(const QPoint&)),
        SLOT(showAngleModeContextMenu(const QPoint&)));

    m_status.resultFormat->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_status.resultFormat, SIGNAL(customContextMenuRequested(const QPoint&)),
        SLOT(showResultFormatContextMenu(const QPoint&)));
    m_status.resultPrecision->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_status.resultPrecision, SIGNAL(customContextMenuRequested(const QPoint&)),
        SLOT(showPrecisionContextMenu(const QPoint&)));
    m_status.complexForm->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_status.complexForm, SIGNAL(customContextMenuRequested(const QPoint&)),
        SLOT(showComplexFormContextMenu(const QPoint&)));

    connect(m_status.angleUnit, &QPushButton::clicked, this, [this]() {
        showAngleModeContextMenu(QPoint(0, m_status.angleUnit->height()));
    });
    connect(m_status.resultFormat, &QPushButton::clicked, this, [this]() {
        showResultFormatContextMenu(QPoint(0, m_status.resultFormat->height()));
    });
    connect(m_status.resultPrecision, &QPushButton::clicked, this, [this]() {
        showPrecisionContextMenu(QPoint(0, m_status.resultPrecision->height()));
    });
    connect(m_status.complexForm, &QPushButton::clicked, this, [this]() {
        showComplexFormContextMenu(QPoint(0, m_status.complexForm->height()));
    });

    bar->addWidget(m_status.resultFormatSection);
    bar->addWidget(m_status.resultPrecisionSection);
    bar->addWidget(m_status.angleUnitSection);
    bar->addWidget(m_status.complexFormSection);

    setStatusBarText();
    // When the status bar is recreated via View > Status Bar, geometry might not
    // be updated yet and width can be 0, which hides all sections. Recompute
    // once the event loop lays out the status bar.
    QTimer::singleShot(0, this, [this]() {
        updateStatusBarSectionVisibility();
    });
}

void MainWindow::createFixedWidgets()
{
    m_widgets.root = new QWidget(this);
    setCentralWidget(m_widgets.root);

    m_layouts.root = new QVBoxLayout(m_widgets.root);
    m_layouts.root->setSpacing(0);
    m_layouts.root->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout* displayLayout = new QHBoxLayout();
    displayLayout->setSpacing(0);
    m_widgets.display = new ResultDisplay(m_widgets.root);
    m_widgets.display->setFrameStyle(QFrame::NoFrame);
    displayLayout->addWidget(m_widgets.display);
    m_layouts.root->addLayout(displayLayout);

    QHBoxLayout* editorLayout = new QHBoxLayout();
    editorLayout->setSpacing(0);
    m_widgets.editor = new Editor(m_widgets.root);
    m_widgets.editor->setFrameStyle(QFrame::NoFrame);
    m_widgets.editor->setFocus();
    editorLayout->addWidget(m_widgets.editor);
    m_layouts.root->addLayout(editorLayout);

    m_widgets.state = new QLabel(this);
    m_widgets.state->setPalette(QToolTip::palette());
    m_widgets.state->setAutoFillBackground(true);
    m_widgets.state->setFrameShape(QFrame::NoFrame);
    m_widgets.state->installEventFilter(this);
    m_widgets.stateCloseButton = new QPushButton(QStringLiteral("×"), m_widgets.state);
    m_widgets.stateCloseButton->setFocusPolicy(Qt::NoFocus);
    m_widgets.stateCloseButton->setFlat(true);
    m_widgets.stateCloseButton->setToolTip(tr("Close preview"));
    m_widgets.stateCloseButton->setStyleSheet(QStringLiteral(
        "QPushButton{border:none;background:transparent;padding:0;margin:0;outline:none;}"
        "QPushButton:hover{background:transparent;}"
        "QPushButton:pressed{background:transparent;}"));
    connect(m_widgets.stateCloseButton, &QPushButton::clicked, this, &MainWindow::hideStateLabel);
    m_widgets.state->hide();
}

void MainWindow::createBitField() {
    m_widgets.bitField = new BitFieldWidget(m_widgets.root);
    m_layouts.root->addWidget(m_widgets.bitField);
    m_widgets.bitField->show();
    m_widgets.display->verticalScrollBar()->setValue(m_widgets.display->verticalScrollBar()->maximum());
    connect(m_widgets.bitField, SIGNAL(bitsChanged(const QString&)), SLOT(handleBitsChanged(const QString&)));
    m_settings->bitfieldVisible = true;
}

void MainWindow::createKeypad()
{
    if (m_widgets.keypad)
        return;

    if (m_settings->keypadMode == Settings::KeypadModeCustom) {
        QList<Keypad::CustomButtonDescription> customButtons;
        for (const auto& button : m_settings->customKeypad.buttons) {
            if (button.row < 0 || button.row >= m_settings->customKeypad.rows
                    || button.column < 0 || button.column >= m_settings->customKeypad.columns) {
                continue;
            }
            Keypad::CustomButtonDescription description;
            description.label = button.label;
            description.text = button.text;
            description.action = static_cast<int>(button.action);
            description.row = button.row;
            description.column = button.column;
            customButtons.append(description);
        }
        m_widgets.keypad = new Keypad(customButtons, m_widgets.root, m_settings->keypadZoomPercent);
        connect(m_widgets.keypad, &Keypad::customButtonPressed,
                this, &MainWindow::handleCustomKeypadButtonPress);
    } else {
        Keypad::LayoutMode layoutMode = Keypad::LayoutModeScientificWide;
        if (m_settings->keypadMode == Settings::KeypadModeBasicWide)
            layoutMode = Keypad::LayoutModeBasicWide;
        else if (m_settings->keypadMode == Settings::KeypadModeScientificNarrow)
            layoutMode = Keypad::LayoutModeScientificNarrow;
        m_widgets.keypad = new Keypad(layoutMode, m_widgets.root, m_settings->keypadZoomPercent);
        connect(m_widgets.keypad, SIGNAL(buttonPressed(Keypad::Button)), SLOT(handleKeypadButtonPress(Keypad::Button)));
        connect(this, SIGNAL(radixCharacterChanged()), m_widgets.keypad, SLOT(handleRadixCharacterChange()));
    }
    m_widgets.keypad->setFocusPolicy(Qt::NoFocus);
    m_widgets.keypad->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_widgets.keypad, SIGNAL(customContextMenuRequested(const QPoint&)),
            SLOT(showKeypadContextMenu(const QPoint&)));

    m_layouts.keypad = new QHBoxLayout();
    m_layouts.keypad->addStretch();
    m_layouts.keypad->addWidget(m_widgets.keypad);
    m_layouts.keypad->addStretch();
    m_layouts.root->addLayout(m_layouts.keypad);

    m_widgets.keypad->show();
    m_settings->keypadVisible = true;
}

void MainWindow::createBookDock(bool)
{
    m_docks.book = new BookDock(this);
    m_docks.book->setObjectName("BookDock");
    m_docks.book->installEventFilter(this);
    m_docks.book->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.book,
            SIGNAL(expressionSelected(const QString&)),
            SLOT(insertTextIntoEditor(const QString&)));

    // No focus for this dock.
    addTabifiedDock(m_docks.book, false);
    if (!m_settings->formulaBookActivePage.isEmpty())
        m_docks.book->openPage(QUrl(m_settings->formulaBookActivePage));
    m_settings->formulaBookDockVisible = true;
}

void MainWindow::createConstantsDock(bool takeFocus)
{
    m_docks.constants = new GenericDock<ConstantsWidget>("MainWindow", QT_TR_NOOP("Constants"), this);
    m_docks.constants->setObjectName("ConstantsDock");
    m_docks.constants->installEventFilter(this);
    m_docks.constants->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.constants->widget(), &ConstantsWidget::constantSelected,
            this, &MainWindow::insertConstantIntoEditor);
    connect(this, &MainWindow::radixCharacterChanged,
            m_docks.constants->widget(), &ConstantsWidget::handleRadixCharacterChange);

    addTabifiedDock(m_docks.constants, takeFocus);
    m_docks.constants->widget()->restoreState(
        m_settings->constantsDockDomain,
        m_settings->constantsDockSubdomain,
        m_settings->constantsDockSearchText);
    m_settings->constantsDockVisible = true;
}

void MainWindow::createFunctionsDock(bool takeFocus)
{
    m_docks.functions = new GenericDock<FunctionsWidget>("MainWindow", QT_TR_NOOP("Functions"), this);
    m_docks.functions->setObjectName("FunctionsDock");
    m_docks.functions->installEventFilter(this);
    m_docks.functions->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.functions->widget(), &FunctionsWidget::functionSelected,
            this, &MainWindow::insertFunctionIntoEditor);

    addTabifiedDock(m_docks.functions, takeFocus);
    m_docks.functions->widget()->setSelectedDomain(m_settings->functionsDockDomain);
    m_docks.functions->widget()->setSearchText(m_settings->functionsDockSearchText);
    m_settings->functionsDockVisible = true;
}

void MainWindow::createHistoryDock(bool)
{
    m_docks.history = new GenericDock<HistoryWidget>("MainWindow", QT_TR_NOOP("History"), this);
    m_docks.history->setObjectName("HistoryDock");
    m_docks.history->installEventFilter(this);
    m_docks.history->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.history->widget(), &HistoryWidget::expressionSelected,
            this, &MainWindow::insertTextIntoEditor);
    connect(m_docks.history->widget(), &HistoryWidget::removeHistoryEntryRequested,
            this, &MainWindow::removeHistoryEntryAt);
    connect(m_docks.history->widget(), &HistoryWidget::removeHistoryEntriesAboveRequested,
            this, &MainWindow::removeHistoryEntriesAbove);
    connect(m_docks.history->widget(), &HistoryWidget::removeHistoryEntriesBelowRequested,
            this, &MainWindow::removeHistoryEntriesBelow);
    connect(this, &MainWindow::historyChanged,
            m_docks.history->widget(), &HistoryWidget::updateHistory);

    // No focus for this dock.
    addTabifiedDock(m_docks.history, false);
    m_settings->historyDockVisible = true;
}

void MainWindow::createVariablesDock(bool takeFocus)
{
    m_docks.variables = new GenericDock<VariableListWidget>("MainWindow", QT_TR_NOOP("User Variables"), this);
    m_docks.variables->setObjectName("VariablesDock");
    m_docks.variables->installEventFilter(this);
    m_docks.variables->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.variables->widget(), &VariableListWidget::variableSelected,
            this, &MainWindow::insertVariableIntoEditor);
    connect(m_docks.variables->widget(), &VariableListWidget::variableEdited,
            this, &MainWindow::insertTextIntoEditor);
    connect(this, &MainWindow::radixCharacterChanged,
            m_docks.variables->widget(), &VariableListWidget::updateList);
    connect(this, &MainWindow::variablesChanged,
            m_docks.variables->widget(), &VariableListWidget::updateList);

    addTabifiedDock(m_docks.variables, takeFocus);
    m_docks.variables->widget()->setSearchText(m_settings->variablesDockSearchText);
    m_settings->variablesDockVisible = true;
}

void MainWindow::createUserFunctionsDock(bool takeFocus)
{
    m_docks.userFunctions = new GenericDock<UserFunctionListWidget>("MainWindow", QT_TR_NOOP("User Functions"), this);
    m_docks.userFunctions->setObjectName("UserFunctionsDock");
    m_docks.userFunctions->installEventFilter(this);
    m_docks.userFunctions->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.userFunctions->widget(), &UserFunctionListWidget::userFunctionSelected,
            this, &MainWindow::insertUserFunctionIntoEditor);
    connect(m_docks.userFunctions->widget(), &UserFunctionListWidget::userFunctionEdited,
            this, &MainWindow::insertUserFunctionIntoEditor);
    connect(this, &MainWindow::radixCharacterChanged,
            m_docks.userFunctions->widget(), &UserFunctionListWidget::updateList);
    connect(this, &MainWindow::functionsChanged,
            m_docks.userFunctions->widget(), &UserFunctionListWidget::updateList);

    addTabifiedDock(m_docks.userFunctions, takeFocus);
    m_docks.userFunctions->widget()->setSearchText(m_settings->userFunctionsDockSearchText);
    m_settings->userFunctionsDockVisible = true;
}

void MainWindow::createUserUnitsDock(bool takeFocus)
{
    m_docks.userUnits = new GenericDock<UserUnitListWidget>("MainWindow", QT_TR_NOOP("User Units"), this);
    m_docks.userUnits->setObjectName("UserUnitsDock");
    m_docks.userUnits->installEventFilter(this);
    m_docks.userUnits->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.userUnits->widget(), &UserUnitListWidget::userUnitSelected,
            this, &MainWindow::insertUserUnitIntoEditor);
    connect(m_docks.userUnits->widget(), &UserUnitListWidget::userUnitEdited,
            this, &MainWindow::insertTextIntoEditor);
    connect(this, &MainWindow::radixCharacterChanged,
            m_docks.userUnits->widget(), &UserUnitListWidget::updateList);
    connect(this, &MainWindow::unitsChanged,
            m_docks.userUnits->widget(), &UserUnitListWidget::updateList);

    addTabifiedDock(m_docks.userUnits, takeFocus);
    m_docks.userUnits->widget()->setSearchText(m_settings->userUnitsDockSearchText);
    m_settings->userUnitsDockVisible = true;
}

void MainWindow::addTabifiedDock(QDockWidget* newDock, bool takeFocus, Qt::DockWidgetArea area)
{
    connect(newDock, &QDockWidget::visibilityChanged, this, &MainWindow::handleDockWidgetVisibilityChanged);
    addDockWidget(area, newDock);
    // Try to find an existing dock we can tabify with.
    const auto allDocks = m_allDocks; // TODO: Use Qt 5.7's qAsConst().
    for (auto& d : allDocks) {
        if (dockWidgetArea(d) == area)
            tabifyDockWidget(d, newDock);
    }
    m_allDocks.append(newDock);
    newDock->show();
    newDock->raise();
    if (takeFocus)
        newDock->setFocus();
}

void MainWindow::deleteDock(QDockWidget* dock)
{
    removeDockWidget(dock);
    m_allDocks.removeAll(dock);
    disconnect(dock);
    dock->deleteLater();
}

void MainWindow::createFixedConnections()
{
    connect(m_actions.sessionExportHtml, SIGNAL(triggered()), SLOT(exportHtml()));
    connect(m_actions.sessionExportPlainText, SIGNAL(triggered()), SLOT(exportPlainText()));
    connect(m_actions.sessionImport, SIGNAL(triggered()), SLOT(showSessionImportDialog()));
    connect(m_actions.sessionImportUserDefinitions, SIGNAL(triggered()), SLOT(showUserDefinitionsImportDialog()));
    connect(m_actions.sessionLoad, SIGNAL(triggered()), SLOT(showSessionLoadDialog()));
    connect(m_actions.sessionQuit, SIGNAL(triggered()), SLOT(close()));
    connect(m_actions.sessionSave, SIGNAL(triggered()), SLOT(saveSessionDialog()));

    connect(m_actions.editClearExpression, SIGNAL(triggered()), SLOT(clearEditorAndBitfield()));
    connect(m_actions.editClearHistory, SIGNAL(triggered()), SLOT(clearHistory()));
    connect(m_actions.editCopyLastResult, SIGNAL(triggered()), SLOT(copyResultToClipboard()));
    connect(m_actions.editCopy, SIGNAL(triggered()), SLOT(copy()));
    connect(m_actions.editPaste, SIGNAL(triggered()), m_widgets.editor, SLOT(paste()));
    connect(m_actions.editSelectExpression, SIGNAL(triggered()), SLOT(selectEditorExpression()));
    connect(m_actions.editWrapSelection, SIGNAL(triggered()), SLOT(wrapSelection()));

    connect(m_actions.viewFullScreenMode, SIGNAL(toggled(bool)), SLOT(setFullScreenEnabled(bool)));
    connect(m_actionGroups.keypad, SIGNAL(triggered(QAction*)), SLOT(setKeypadMode(QAction*)));
    connect(m_actions.viewKeypadDisabled, &QAction::toggled,
            this, [this](bool) {
                updateKeypadDisabledActionText();
            });
    connect(m_actionGroups.keypadZoom, SIGNAL(triggered(QAction*)), SLOT(setKeypadZoom(QAction*)));
    connect(m_actions.viewStatusBar, SIGNAL(toggled(bool)), SLOT(setStatusBarVisible(bool)));
#if !defined(Q_OS_MACOS)
    connect(m_actions.viewMenuBar, SIGNAL(toggled(bool)), SLOT(setMenuBarVisible(bool)));
#endif
    connect(m_actions.viewBitfield, SIGNAL(toggled(bool)), SLOT(setBitfieldVisible(bool)));

    connect(m_actions.viewConstants, SIGNAL(triggered(bool)), SLOT(setConstantsDockVisible(bool)));
    connect(m_actions.viewFunctions, SIGNAL(triggered(bool)), SLOT(setFunctionsDockVisible(bool)));
    connect(m_actions.viewHistory, SIGNAL(triggered(bool)), SLOT(setHistoryDockVisible(bool)));
    connect(m_actions.viewFormulaBook, SIGNAL(triggered(bool)), SLOT(setFormulaBookDockVisible(bool)));
    connect(m_actions.viewVariables, SIGNAL(triggered(bool)), SLOT(setVariablesDockVisible(bool)));
    connect(m_actions.viewUserFunctions, SIGNAL(triggered(bool)), SLOT(setUserFunctionsDockVisible(bool)));
    connect(m_actions.viewUserUnits, SIGNAL(triggered(bool)), SLOT(setUserUnitsDockVisible(bool)));

    connect(m_actions.settingsAngleUnitDegree, SIGNAL(triggered()), SLOT(setAngleModeDegree()));
    connect(m_actions.settingsAngleUnitRadian, SIGNAL(triggered()), SLOT(setAngleModeRadian()));
    connect(m_actions.settingsAngleUnitGradian, SIGNAL(triggered()), SLOT(setAngleModeGradian()));
    connect(m_actions.settingsAngleUnitTurn, SIGNAL(triggered()), SLOT(setAngleModeTurn()));
    connect(m_actions.settingsAngleUnitRevolution, SIGNAL(triggered()), SLOT(setAngleModeRevolution()));

    if (!isWaylandPlatform())
        connect(m_actions.settingsBehaviorAlwaysOnTop, SIGNAL(toggled(bool)), SLOT(setAlwaysOnTopEnabled(bool)));
    connect(m_actions.settingsBehaviorAutoCompletion, SIGNAL(toggled(bool)), SLOT(setAutoCompletionEnabled(bool)));
    connect(m_actions.settingsBehaviorAutoCompletionBuiltInFunctions, SIGNAL(toggled(bool)), SLOT(setAutoCompletionBuiltInFunctionsEnabled(bool)));
    connect(m_actions.settingsBehaviorAutoCompletionBuiltInVariables, SIGNAL(toggled(bool)), SLOT(setAutoCompletionBuiltInVariablesEnabled(bool)));
    connect(m_actions.settingsBehaviorAutoCompletionLongFormUnits, SIGNAL(toggled(bool)), SLOT(setAutoCompletionLongFormUnitsEnabled(bool)));
    connect(m_actions.settingsBehaviorAutoCompletionUserFunctions, SIGNAL(toggled(bool)), SLOT(setAutoCompletionUserFunctionsEnabled(bool)));
    connect(m_actions.settingsBehaviorAutoCompletionUserVariables, SIGNAL(toggled(bool)), SLOT(setAutoCompletionUserVariablesEnabled(bool)));
    connect(m_actions.settingsBehaviorAutoAns, SIGNAL(toggled(bool)), SLOT(setAutoAnsEnabled(bool)));
    connect(m_actions.settingsBehaviorEmptyHistoryHint, SIGNAL(toggled(bool)), SLOT(setEmptyHistoryHintEnabled(bool)));
    connect(m_actions.settingsBehaviorPartialResults, SIGNAL(toggled(bool)), SLOT(setAutoCalcEnabled(bool)));
    connect(m_actionGroups.historySaving, SIGNAL(triggered(QAction*)), SLOT(setHistorySaving(QAction*)));
    connect(m_actions.settingsBehaviorHistorySizeLimit, SIGNAL(triggered()), SLOT(setHistorySizeLimit()));
    connect(m_actions.settingsBehaviorSaveWindowPositionOnExit, SIGNAL(toggled(bool)), SLOT(setWindowPositionSaveEnabled(bool)));
    connect(m_actions.settingsBehaviorSingleInstance, SIGNAL(toggled(bool)), SLOT(setSingleInstanceEnabled(bool)));
    connect(m_actions.settingsBehaviorSyntaxHighlighting, SIGNAL(toggled(bool)), SLOT(setSyntaxHighlightingEnabled(bool)));
    connect(m_actions.settingsBehaviorHoverHighlightResults, SIGNAL(toggled(bool)), SLOT(setHoverHighlightResultsEnabled(bool)));
    connect(m_actionGroups.digitGrouping, SIGNAL(triggered(QAction*)), SLOT(setDigitGrouping(QAction*)));
    connect(m_actions.settingsBehaviorDigitGroupingIntegerPartOnly, SIGNAL(toggled(bool)), SLOT(setDigitGroupingIntegerPartOnlyEnabled(bool)));
    connect(m_actions.settingsBehaviorLeaveLastExpression, SIGNAL(toggled(bool)), SLOT(setLeaveLastExpressionEnabled(bool)));
    connect(m_actions.settingsBehaviorNumberFormat, SIGNAL(triggered()), SLOT(showNumberFormatDialog()));
    connect(m_actions.settingsBehaviorResultSlots, SIGNAL(triggered()), SLOT(showResultSlotsDialog()));
    connect(m_actionGroups.upDownArrowBehavior, SIGNAL(triggered(QAction*)), SLOT(setUpDownArrowBehavior(QAction*)));
    connect(m_actions.settingsBehaviorAutoResultToClipboard, SIGNAL(toggled(bool)), SLOT(setAutoResultToClipboardEnabled(bool)));
    connect(m_actions.settingsBehaviorSimplifyResultExpressions, SIGNAL(toggled(bool)), SLOT(setSimplifyResultExpressionsEnabled(bool)));
    connect(m_actions.settingsRadixCharComma, SIGNAL(triggered()), SLOT(setRadixCharacterComma()));
    connect(m_actions.settingsRadixCharDefault, SIGNAL(triggered()), SLOT(setRadixCharacterAutomatic()));
    connect(m_actions.settingsRadixCharDot, SIGNAL(triggered()), SLOT(setRadixCharacterDot()));
    connect(m_actions.settingsRadixCharBoth, SIGNAL(triggered()), SLOT(setRadixCharacterBoth()));

    connect(m_actions.settingsResultFormat0Digits, &QAction::triggered, [this]() { this->setResultPrecision(0); });
    connect(m_actions.settingsResultFormat15Digits, SIGNAL(triggered()), SLOT(setResultPrecision15Digits()));
    connect(m_actions.settingsResultFormat2Digits, SIGNAL(triggered()), SLOT(setResultPrecision2Digits()));
    connect(m_actions.settingsResultFormat3Digits, SIGNAL(triggered()), SLOT(setResultPrecision3Digits()));
    connect(m_actions.settingsResultFormat50Digits, SIGNAL(triggered()), SLOT(setResultPrecision50Digits()));
    connect(m_actions.settingsResultFormat8Digits, SIGNAL(triggered()), SLOT(setResultPrecision8Digits()));
    connect(m_actions.settingsResultFormatCustomDigits, SIGNAL(triggered()), SLOT(setResultPrecisionCustom()));
    connect(m_actions.settingsResultFormatAutoPrecision, SIGNAL(triggered()), SLOT(setResultPrecisionAutomatic()));
    connect(m_actions.settingsResultFormatBinary, SIGNAL(triggered()), SLOT(setResultFormatBinary()));
    connect(m_actions.settingsResultFormatComplexDisabled, &QAction::toggled,
            this, [this](bool) {
                updateComplexDisabledActionText();
                setResultFormatComplexDisabled();
            });
    connect(m_actions.settingsResultFormatCartesian, SIGNAL(triggered()), SLOT(setResultFormatCartesian()));
    connect(m_actions.settingsResultFormatEngineering, SIGNAL(triggered()), SLOT(setResultFormatEngineering()));
    connect(m_actions.settingsResultFormatFixed, SIGNAL(triggered()), SLOT(setResultFormatFixed()));
    connect(m_actions.settingsResultFormatGeneral, SIGNAL(triggered()), SLOT(setResultFormatGeneral()));
    connect(m_actions.settingsResultFormatHexadecimal, SIGNAL(triggered()), SLOT(setResultFormatHexadecimal()));
    connect(m_actions.settingsImaginaryUnitI, SIGNAL(triggered()), SLOT(setImaginaryUnitI()));
    connect(m_actions.settingsImaginaryUnitJ, SIGNAL(triggered()), SLOT(setImaginaryUnitJ()));
    connect(m_actions.settingsResultFormatOctal, SIGNAL(triggered()), SLOT(setResultFormatOctal()));
    connect(m_actions.settingsResultFormatPolar, SIGNAL(triggered()), SLOT(setResultFormatPolar()));
    connect(m_actions.settingsResultFormatPolarAngle, SIGNAL(triggered()), SLOT(setResultFormatPolarAngle()));
    connect(m_actions.settingsResultFormatRational, SIGNAL(triggered()), SLOT(setResultFormatRational()));
    connect(m_actions.settingsResultFormatSexagesimal, SIGNAL(triggered()), SLOT(setResultFormatSexagesimal()));
    connect(m_actions.settingsResultFormatScientific, SIGNAL(triggered()), SLOT(setResultFormatScientific()));
    connect(m_actionGroups.unitNegativeExponentStyle, SIGNAL(triggered(QAction*)),
            SLOT(setUnitNegativeExponentStyle(QAction*)));
    connect(m_actionGroups.resultRoundingMode, SIGNAL(triggered(QAction*)),
            SLOT(setResultRoundingMode(QAction*)));

    connect(m_actions.settingsLanguage, SIGNAL(triggered()), SLOT(showLanguageChooserDialog()));

    connect(m_actions.helpManual, SIGNAL(triggered()), SLOT(showManualWindow()));
    connect(m_actions.contextHelp, SIGNAL(triggered()), SLOT(showContextHelp()));
    connect(m_actions.helpUpdates, SIGNAL(triggered()), SLOT(checkForUpdates()));
    connect(m_actions.helpFeedback, SIGNAL(triggered()), SLOT(openFeedbackURL()));
    connect(m_actions.helpCommunity, SIGNAL(triggered()), SLOT(openCommunityURL()));
    connect(m_actions.helpNews, SIGNAL(triggered()), SLOT(openNewsURL()));
    connect(m_actions.helpDonate, SIGNAL(triggered()), SLOT(openDonateURL()));
    connect(m_actions.helpAbout, SIGNAL(triggered()), SLOT(showAboutDialog()));

    connect(m_widgets.editor, SIGNAL(autoCalcDisabled()), SLOT(hideStateLabel()));
    connect(m_widgets.editor, SIGNAL(autoCalcMessageAvailable(const QString&)), SLOT(handleAutoCalcMessageAvailable(const QString&)));
    connect(m_widgets.editor, SIGNAL(autoCalcQuantityAvailable(const Quantity&)), SLOT(handleAutoCalcQuantityAvailable(const Quantity&)));
    connect(m_widgets.editor, SIGNAL(returnPressed()), SLOT(evaluateEditorExpression()));
    connect(m_widgets.editor, SIGNAL(escapePressed()), SLOT(handleEditorEscapePressed()));
    connect(m_widgets.editor, SIGNAL(shiftDownPressed()), SLOT(decreaseDisplayFontPointSize()));
    connect(m_widgets.editor, SIGNAL(shiftUpPressed()), SLOT(increaseDisplayFontPointSize()));
    connect(m_widgets.editor, SIGNAL(controlPageUpPressed()), m_widgets.display, SLOT(scrollToTop()));
    connect(m_widgets.editor, SIGNAL(controlPageDownPressed()), m_widgets.display, SLOT(scrollToBottom()));
    connect(m_widgets.editor, SIGNAL(shiftPageUpPressed()), m_widgets.display, SLOT(scrollLineUp()));
    connect(m_widgets.editor, SIGNAL(shiftPageDownPressed()), m_widgets.display, SLOT(scrollLineDown()));
    connect(m_widgets.editor, SIGNAL(pageUpPressed()), m_widgets.display, SLOT(scrollPageUp()));
    connect(m_widgets.editor, SIGNAL(pageDownPressed()), m_widgets.display, SLOT(scrollPageDown()));
    connect(m_widgets.editor, SIGNAL(textChanged()), SLOT(handleEditorTextChange()));
    connect(m_widgets.editor, SIGNAL(copyAvailable(bool)), SLOT(handleCopyAvailable(bool)));
    connect(m_widgets.editor, SIGNAL(copySequencePressed()), SLOT(copy()));
    connect(m_widgets.editor, SIGNAL(selectionChanged()), SLOT(handleEditorSelectionChange()));
    connect(this, SIGNAL(historyChanged()), m_widgets.editor, SLOT(updateHistory()));

    connect(m_widgets.display, SIGNAL(copyAvailable(bool)), SLOT(handleCopyAvailable(bool)));
    connect(m_widgets.display, SIGNAL(clicked()), SLOT(hideStateLabel()));
    connect(m_widgets.display, SIGNAL(expressionSelected(const QString&)), SLOT(insertTextIntoEditor(const QString&)));
    connect(m_widgets.display, SIGNAL(editHistoryEntryRequested(int)), SLOT(startHistoryEntryEdit(int)));
    connect(m_widgets.display, SIGNAL(editHistoryEntryContextRequested(int)), SLOT(editHistoryEntryContext(int)));
    connect(m_widgets.display, SIGNAL(cancelHistoryEditRequested()), SLOT(cancelHistoryEntryEdit()));
    connect(m_widgets.display, SIGNAL(removeHistoryEntryRequested(int)), SLOT(removeHistoryEntryAt(int)));
    connect(m_widgets.display, SIGNAL(removeHistoryEntriesAboveRequested(int)), SLOT(removeHistoryEntriesAbove(int)));
    connect(m_widgets.display, SIGNAL(removeHistoryEntriesBelowRequested(int)), SLOT(removeHistoryEntriesBelow(int)));
    connect(m_widgets.display, SIGNAL(selectionChanged()), SLOT(handleDisplaySelectionChange()));
    connect(m_widgets.display, SIGNAL(shiftWheelUp()), SLOT(increaseDisplayFontPointSize()));
    connect(m_widgets.display, SIGNAL(shiftWheelDown()), SLOT(decreaseDisplayFontPointSize()));
    connect(m_widgets.display, SIGNAL(controlWheelUp()), SLOT(increaseDisplayFontPointSize()));
    connect(m_widgets.display, SIGNAL(controlWheelDown()), SLOT(decreaseDisplayFontPointSize()));
    connect(m_widgets.display, SIGNAL(shiftControlWheelDown()), SLOT(decreaseOpacity()));
    connect(m_widgets.display, SIGNAL(shiftControlWheelUp()), SLOT(increaseOpacity()));
    connect(this, SIGNAL(historyChanged()), m_widgets.display, SLOT(refresh()));

    connect(this, SIGNAL(radixCharacterChanged()), m_widgets.display, SLOT(refresh()));
    connect(this, SIGNAL(radixCharacterChanged()), m_widgets.editor, SLOT(refreshAutoCalc()));
    connect(this, SIGNAL(angleUnitChanged()), m_widgets.editor, SLOT(refreshAutoCalc()));
    connect(this, SIGNAL(complexNumbersChanged()), m_widgets.display, SLOT(refreshLastHistoryEntry()));
    connect(this, SIGNAL(complexNumbersChanged()), m_widgets.editor, SLOT(refreshAutoCalc()));
    connect(this, SIGNAL(resultFormatChanged()), m_widgets.display, SLOT(refreshLastHistoryEntry()));
    connect(this, SIGNAL(resultFormatChanged()), m_widgets.editor, SLOT(refreshAutoCalc()));
    connect(this, SIGNAL(resultPrecisionChanged()), m_widgets.display, SLOT(refreshLastHistoryEntry()));
    connect(this, SIGNAL(resultPrecisionChanged()), m_widgets.editor, SLOT(refreshAutoCalc()));
    connect(this, SIGNAL(resultRoundingModeChanged()), m_widgets.display, SLOT(refreshLastHistoryEntry()));
    connect(this, SIGNAL(resultRoundingModeChanged()), m_widgets.editor, SLOT(refreshAutoCalc()));
    connect(this, SIGNAL(colorSchemeChanged()), m_widgets.display, SLOT(rehighlight()));
    connect(this, SIGNAL(colorSchemeChanged()), m_widgets.editor, SLOT(rehighlight()));
    connect(m_actionGroups.colorScheme, &QActionGroup::hovered, this, &MainWindow::applyColorSchemeFromAction);
    connect(m_menus.colorScheme, &QMenu::aboutToHide, this, &MainWindow::revertColorScheme);
    connect(m_menus.colorScheme, &QMenu::aboutToShow, this, &MainWindow::saveColorSchemeToRevert);
    connect(this, SIGNAL(syntaxHighlightingChanged()), m_widgets.display, SLOT(rehighlight()));
    connect(this, SIGNAL(syntaxHighlightingChanged()), m_widgets.editor, SLOT(rehighlight()));

    connect(m_actions.settingsDisplayFont, SIGNAL(triggered()), SLOT(showFontDialog()));
    connect(m_actions.settingsDisplayColorSchemeCustom, SIGNAL(triggered()), SLOT(showCustomThemeDialog()));

    const auto schemes = m_actions.settingsDisplayColorSchemes;
    for (auto& action : schemes) // TODO: Use Qt 5.7's qAsConst();
        connect(action, SIGNAL(triggered()), SLOT(applySelectedColorScheme()));

    connect(this, SIGNAL(languageChanged()), SLOT(retranslateText()));
}

void MainWindow::applySettings()
{
    emit languageChanged();

    setFormulaBookDockVisible(m_settings->formulaBookDockVisible, false);
    m_actions.viewFormulaBook->setChecked(m_settings->formulaBookDockVisible);

    setConstantsDockVisible(m_settings->constantsDockVisible, false);
    m_actions.viewConstants->setChecked(m_settings->constantsDockVisible);

    setFunctionsDockVisible(m_settings->functionsDockVisible, false);
    m_actions.viewFunctions->setChecked(m_settings->functionsDockVisible);

    setHistoryDockVisible(m_settings->historyDockVisible, false);
    m_actions.viewHistory->setChecked(m_settings->historyDockVisible);

    setVariablesDockVisible(m_settings->variablesDockVisible, false);
    m_actions.viewVariables->setChecked(m_settings->variablesDockVisible);

    setUserFunctionsDockVisible(m_settings->userFunctionsDockVisible, false);
    m_actions.viewUserFunctions->setChecked(m_settings->userFunctionsDockVisible);

    setUserUnitsDockVisible(m_settings->userUnitsDockVisible, false);
    m_actions.viewUserUnits->setChecked(m_settings->userUnitsDockVisible);

    m_actions.viewBitfield->setChecked(m_settings->bitfieldVisible);
    switch (m_settings->keypadMode) {
    case Settings::KeypadModeBasicWide:
        m_actions.viewKeypadBasicWide->setChecked(true);
        break;
    case Settings::KeypadModeScientificWide:
        m_actions.viewKeypadScientificWide->setChecked(true);
        break;
    case Settings::KeypadModeScientificNarrow:
        m_actions.viewKeypadScientificNarrow->setChecked(true);
        break;
    case Settings::KeypadModeCustom:
        m_actions.viewKeypadCustom->setChecked(true);
        break;
    case Settings::KeypadModeDisabled:
    default:
        m_actions.viewKeypadDisabled->setChecked(true);
        break;
    }
    switch (m_settings->keypadZoomPercent) {
    case 150:
        m_actions.viewKeypadZoom150->setChecked(true);
        break;
    case 200:
        m_actions.viewKeypadZoom200->setChecked(true);
        break;
    case 100:
    default:
        m_actions.viewKeypadZoom100->setChecked(true);
        break;
    }
    setKeypadVisible(isVisibleKeypadMode(m_settings->keypadMode));
    m_actions.viewStatusBar->setChecked(m_settings->statusBarVisible);
#if !defined(Q_OS_MACOS)
    setMenuBarVisible(m_settings->menuBarVisible);
    m_actions.viewMenuBar->setChecked(m_settings->menuBarVisible);
#endif

    if (!restoreGeometry(m_settings->windowGeometry)) {
        // We couldn't restore the saved geometry; that means it was either empty
        // or just isn't valid anymore so we use default size and position.
        int defaultWidth = 640;
        if (m_settings->windowGeometry.isEmpty() && m_widgets.keypad)
            defaultWidth = m_widgets.keypad->sizeHint().width();
        resize(defaultWidth, 480);
        QRect screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();
        move(screenGeometry.center() - rect().center());
    }
    restoreState(m_settings->windowState);

    m_actions.viewFullScreenMode->setChecked(m_settings->windowOnfullScreen);
    if (!isWaylandPlatform())
        m_actions.settingsBehaviorAlwaysOnTop->setChecked(m_settings->windowAlwaysOnTop);

    if (m_settings->angleUnit == 'r')
        m_actions.settingsAngleUnitRadian->setChecked(true);
    else if (m_settings->angleUnit == 'd')
        m_actions.settingsAngleUnitDegree->setChecked(true);
    else if (m_settings->angleUnit == 'g')
        m_actions.settingsAngleUnitGradian->setChecked(true);
    else if (m_settings->angleUnit == 't')
        m_actions.settingsAngleUnitTurn->setChecked(true);
    else if (m_settings->angleUnit == 'v')
        m_actions.settingsAngleUnitRevolution->setChecked(true);

    if (m_settings->historySaving == Settings::HistorySavingNever) {
        m_actions.settingsBehaviorHistorySavingNever->setChecked(true);
    } else if (m_settings->historySaving == Settings::HistorySavingContinuously) {
        m_actions.settingsBehaviorHistorySavingContinuously->setChecked(true);
    } else {
        m_actions.settingsBehaviorHistorySavingOnExit->setChecked(true);
    }

    StartupDefinitions::loadInto(m_settings);

    if (m_settings->startupUserDefinitionsApplyBeforeRestore)
        applyStartupUserDefinitions();
    restoreSession(m_settings->historySaving != Settings::HistorySavingNever);
    if (!m_settings->startupUserDefinitionsApplyBeforeRestore)
        applyStartupUserDefinitions();

    m_actions.settingsBehaviorLeaveLastExpression->setChecked(m_settings->leaveLastExpression);
    switch (m_settings->upDownArrowBehavior) {
    case Settings::UpDownArrowBehaviorNever:
        m_actions.settingsBehaviorUpDownArrowNever->setChecked(true);
        break;
    case Settings::UpDownArrowBehaviorSingleLineOnly:
        m_actions.settingsBehaviorUpDownArrowSingleLineOnly->setChecked(true);
        break;
    case Settings::UpDownArrowBehaviorAlways:
    default:
        m_actions.settingsBehaviorUpDownArrowAlways->setChecked(true);
        break;
    }
    m_actions.settingsBehaviorEmptyHistoryHint->setChecked(m_settings->showEmptyHistoryHint);
    m_actions.settingsBehaviorSaveWindowPositionOnExit->setChecked(m_settings->windowPositionSave);
    m_actions.settingsBehaviorSingleInstance->setChecked(m_settings->singleInstance);


    checkInitialResultFormat();
    checkInitialResultPrecision();
    checkInitialComplexFormat();
    checkInitialImaginaryUnit();
    switch (m_settings->resultRoundingMode) {
    case Settings::ResultRoundingHalfEven:
        m_actions.settingsResultRoundingHalfEven->setChecked(true);
        break;
    case Settings::ResultRoundingTowardZero:
        m_actions.settingsResultRoundingTowardZero->setChecked(true);
        break;
    case Settings::ResultRoundingTowardPositiveInfinity:
        m_actions.settingsResultRoundingTowardPositiveInfinity->setChecked(true);
        break;
    case Settings::ResultRoundingTowardNegativeInfinity:
        m_actions.settingsResultRoundingTowardNegativeInfinity->setChecked(true);
        break;
    case Settings::ResultRoundingHalfAwayFromZero:
    default:
        m_actions.settingsResultRoundingHalfAwayFromZero->setChecked(true);
        break;
    }
    setRuntimeResultRoundingMode(m_settings->resultRoundingMode);
    if (m_settings->unitNegativeExponentStyle == Settings::UnitNegativeExponentFraction)
        m_actions.settingsUnitNegativeExponentFraction->setChecked(true);
    else
        m_actions.settingsUnitNegativeExponentSuperscript->setChecked(true);
    setRuntimeUnitNegativeExponentStyle(m_settings->unitNegativeExponentStyle);

    if (m_settings->autoAns)
        m_actions.settingsBehaviorAutoAns->setChecked(true);
    else
        setAutoAnsEnabled(false);

    if (m_settings->autoCalc)
        m_actions.settingsBehaviorPartialResults->setChecked(true);
    else
        setAutoCalcEnabled(false);

    if (m_settings->autoCompletion)
        m_actions.settingsBehaviorAutoCompletion->setChecked(true);
    else
        setAutoCompletionEnabled(false);
    m_actions.settingsBehaviorAutoCompletionBuiltInFunctions->setChecked(
        m_settings->autoCompletionBuiltInFunctions);
    m_actions.settingsBehaviorAutoCompletionBuiltInVariables->setChecked(
        m_settings->autoCompletionBuiltInVariables);
    m_actions.settingsBehaviorAutoCompletionLongFormUnits->setChecked(
        m_settings->autoCompletionLongFormUnits);
    m_actions.settingsBehaviorAutoCompletionUserFunctions->setChecked(
        m_settings->autoCompletionUserFunctions);
    m_actions.settingsBehaviorAutoCompletionUserVariables->setChecked(
        m_settings->autoCompletionUserVariables);

    if (m_settings->syntaxHighlighting)
        m_actions.settingsBehaviorSyntaxHighlighting->setChecked(true);
    else
        setSyntaxHighlightingEnabled(false);

    if (m_settings->hoverHighlightResults)
        m_actions.settingsBehaviorHoverHighlightResults->setChecked(true);
    else
        setHoverHighlightResultsEnabled(false);

    if (m_settings->autoResultToClipboard)
        m_actions.settingsBehaviorAutoResultToClipboard->setChecked(true);
    else
        setAutoResultToClipboardEnabled(false);

    if (m_settings->simplifyResultExpressions)
        m_actions.settingsBehaviorSimplifyResultExpressions->setChecked(true);
    else
        setSimplifyResultExpressionsEnabled(false);

    QFont font;
    font.fromString(m_settings->displayFont);
    m_widgets.display->setFont(font);
    m_widgets.editor->setFont(font);

    m_widgets.display->verticalScrollBar()->setValue(m_widgets.display->verticalScrollBar()->maximum());

    const auto schemes = m_actions.settingsDisplayColorSchemes;
    bool colorSchemeMatched = false;
    for (auto& action : schemes) {
        if (m_settings->colorScheme == action->data().toString()) {
            action->setChecked(true);
            colorSchemeMatched = true;
        }
    }
    m_actions.settingsDisplayColorSchemeCustom->setChecked(!colorSchemeMatched
                                                           && m_settings->colorScheme == QLatin1String("Custom"));

    if (m_widgets.display->isEmpty())
        QTimer::singleShot(0, this, SLOT(showReadyMessage()));
}

void MainWindow::showManualWindow()
{
    if (m_widgets.manual) {
        m_widgets.manual->raise();
        m_widgets.manual->activateWindow();
        return;
    }

    m_widgets.manual = new ManualWindow();
    if (!m_widgets.manual->restoreGeometry(m_settings->manualWindowGeometry))
        m_widgets.manual->resize(640, 480);
    m_widgets.manual->show();
    connect(m_widgets.manual, SIGNAL(windowClosed()), SLOT(handleManualClosed()));
}

void MainWindow::showContextHelp()
{
    QString kw = "";
    if(m_widgets.editor->hasFocus()) {
        kw = m_widgets.editor->getKeyword();
        if (kw != "") {
            auto url = m_manualServer->urlForKeyword(kw);
            if (url.isValid()) {
                showManualWindow();
                m_widgets.manual->openPage(url);
            }
        }
    }
}

void MainWindow::showReadyMessage()
{
    if (!m_settings->showEmptyHistoryHint)
        return;
    showStateLabel(tr("Type an expression here"));
}

void MainWindow::checkInitialResultFormat()
{
    switch (m_settings->resultFormat) {
        case 'g': m_actions.settingsResultFormatGeneral->setChecked(true); break;
        case 'n': m_actions.settingsResultFormatEngineering->setChecked(true); break;
        case 'e': m_actions.settingsResultFormatScientific->setChecked(true); break;
        case 'r': m_actions.settingsResultFormatRational->setChecked(true); break;
        case 'h': m_actions.settingsResultFormatHexadecimal->setChecked(true); break;
        case 'o': m_actions.settingsResultFormatOctal->setChecked(true); break;
        case 'b': m_actions.settingsResultFormatBinary->setChecked(true); break;
        case 's': m_actions.settingsResultFormatSexagesimal->setChecked(true); break;
        default : m_actions.settingsResultFormatFixed->setChecked(true);
    }
}

void MainWindow::checkInitialComplexFormat()
{
    m_actions.settingsResultFormatComplexDisabled->setChecked(!m_settings->complexNumbers);
    updateComplexDisabledActionText();
}

void MainWindow::checkInitialImaginaryUnit()
{
    if (!m_settings->complexNumbers)
        return;

    if (m_settings->imaginaryUnit == 'j')
        m_actions.settingsImaginaryUnitJ->setChecked(true);
    else
        m_actions.settingsImaginaryUnitI->setChecked(true);
}

void MainWindow::checkInitialResultPrecision()
{
    switch (m_settings->resultPrecision) {
        case 0: m_actions.settingsResultFormat0Digits->setChecked(true); break;
        case 2: m_actions.settingsResultFormat2Digits->setChecked(true); break;
        case 3: m_actions.settingsResultFormat3Digits->setChecked(true); break;
        case 8: m_actions.settingsResultFormat8Digits->setChecked(true); break;
        case 15: m_actions.settingsResultFormat15Digits->setChecked(true); break;
        case 50: m_actions.settingsResultFormat50Digits->setChecked(true); break;
        case -1: m_actions.settingsResultFormatAutoPrecision->setChecked(true); break;
        default: m_actions.settingsResultFormatCustomDigits->setChecked(true); break;
    }
}

void MainWindow::checkInitialDigitGrouping()
{
    switch (m_settings->digitGrouping) {
        case 1: m_actions.settingsBehaviorDigitGroupingOneSpace->setChecked(true); break;
        case 2: m_actions.settingsBehaviorDigitGroupingTwoSpaces->setChecked(true); break;
        case 3: m_actions.settingsBehaviorDigitGroupingThreeSpaces->setChecked(true); break;
        default:
        case 0: m_actions.settingsBehaviorDigitGroupingNone->setChecked(true); break;
    }
}



void MainWindow::saveSettings()
{
    if (m_docks.constants) {
        m_settings->constantsDockDomain = m_docks.constants->widget()->selectedDomain();
        m_settings->constantsDockSubdomain = m_docks.constants->widget()->selectedSubdomain();
        m_settings->constantsDockSearchText = m_docks.constants->widget()->searchText();
    }
    if (m_docks.functions) {
        m_settings->functionsDockDomain = m_docks.functions->widget()->selectedDomain();
        m_settings->functionsDockSearchText = m_docks.functions->widget()->searchText();
    }
    if (m_docks.userFunctions)
        m_settings->userFunctionsDockSearchText = m_docks.userFunctions->widget()->searchText();
    if (m_docks.userUnits)
        m_settings->userUnitsDockSearchText = m_docks.userUnits->widget()->searchText();
    if (m_docks.variables)
        m_settings->variablesDockSearchText = m_docks.variables->widget()->searchText();
    if (m_docks.book)
        m_settings->formulaBookActivePage = m_docks.book->currentPage();

    m_settings->windowGeometry = m_settings->windowPositionSave ? saveGeometry() : QByteArray();
    if (m_widgets.manual)
        m_settings->manualWindowGeometry = m_settings->windowPositionSave ? m_widgets.manual->saveGeometry() : QByteArray();
    m_settings->windowState = saveState();
    m_settings->displayFont = m_widgets.display->font().toString();

    m_settings->save();
}

void MainWindow::saveSession(QString & fname, bool saveHistory)
{
    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }


    QJsonObject json;
    m_session->serialize(json);
    if (!saveHistory)
        json.remove(QLatin1String("history"));
    QJsonDocument doc(json);
    file.write(doc.toJson(QJsonDocument::Compact));

    file.close();
}

MainWindow::MainWindow()
    : QMainWindow()
{

    m_session = new Session();
    m_constants = Constants::instance();
    m_evaluator = Evaluator::instance();
    m_functions = FunctionRepo::instance();
    m_evaluator->setSession(m_session);
    m_evaluator->initializeBuiltInVariables();

    m_translator = 0;
    m_settings = Settings::instance();
    DMath::complexMode = m_settings->complexNumbers;
    CMath::setImaginaryUnitSymbol(m_settings->imaginaryUnit);

    m_widgets.manual = 0;
    m_widgets.keypad  = 0;

    m_conditions.autoAns = false;

    m_docks.book = 0;
    m_docks.history = 0;
    m_docks.constants = 0;
    m_docks.functions = 0;
    m_docks.variables = 0;
    m_docks.userFunctions = 0;
    m_docks.userUnits = 0;

    m_status.angleUnit = 0;
    m_status.angleUnitSection = 0;
    m_status.angleUnitLabel = 0;
    m_status.resultFormat = 0;
    m_status.resultFormatSection = 0;
    m_status.resultFormatLabel = 0;
    m_status.resultPrecision = 0;
    m_status.resultPrecisionSection = 0;
    m_status.resultPrecisionLabel = 0;
    m_status.complexForm = 0;
    m_status.complexFormSection = 0;
    m_status.complexFormLabel = 0;

    m_copyWidget = 0;
    m_pendingHistoryEditIndex = -1;
    m_shutdownStateSaved = false;
    m_versionCheck = 0;

    createUi();
    applySettings();

    if (!m_settings->hasNumberFormatStyleSetting)
        QTimer::singleShot(0, this, SLOT(showNumberFormatDialog()));

    m_versionCheck = new VersionCheck(this, this);
    QTimer::singleShot(0, this, [this]() {
        if (m_versionCheck)
            m_versionCheck->checkForUpdateIfDue();
    });

    m_manualServer = ManualServer::instance();
    connect(this, SIGNAL(languageChanged()), m_manualServer, SLOT(ensureCorrectLanguage()));
}

MainWindow::~MainWindow()
{
    if (m_docks.book)
        deleteBookDock();
    if (m_docks.constants)
        deleteConstantsDock();
    if (m_docks.variables)
        deleteVariablesDock();
    if (m_docks.userFunctions)
        deleteUserFunctionsDock();
    if (m_docks.userUnits)
        deleteUserUnitsDock();
    if (m_docks.functions)
        deleteFunctionsDock();
    if (m_docks.history)
        deleteHistoryDock();
    delete m_session;
}

void MainWindow::showAboutDialog()
{
    AboutBox dialog(this);
    dialog.resize(480, 640);
    dialog.exec();
}

void MainWindow::clearHistory()
{
    if (m_session->historyIsEmpty())
        return;

    const QMessageBox::StandardButton confirmation = QMessageBox::question(
        this,
        tr("Clear History"),
        tr("Are you sure you want to clear the calculation history?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (confirmation != QMessageBox::Yes)
        return;

    m_session->clearHistory();
    m_pendingHistoryEditIndex = -1;
    m_widgets.display->setEditingHistoryIndex(-1);
    m_widgets.editor->setHistoryArrowNavigationEnabled(true);
    clearEditorAndBitfield();
    emit historyChanged();

    m_conditions.autoAns = false;
}

void MainWindow::clearEditor()
{
    m_widgets.editor->clear();
    m_widgets.editor->setFocus();
}

void MainWindow::clearEditorAndBitfield()
{
    clearEditor();
    if (m_widgets.bitField)
        m_widgets.bitField->clear();
}

void MainWindow::copyResultToClipboard()
{
    QClipboard* cb = QApplication::clipboard();
    Quantity q = m_evaluator->getVariable(QLatin1String("ans")).value();
    QString strToCopy(NumberFormatter::format(q));
    strToCopy.replace(UnicodeChars::MinusSign, MathDsl::SubOpAl1);
    cb->setText(strToCopy, QClipboard::Clipboard);
}

void MainWindow::decreaseOpacity()
{
    if (windowOpacity() > 0.4)
        setWindowOpacity(windowOpacity() - 0.1);
}

void MainWindow::increaseOpacity()
{
    if (windowOpacity() < 1.0)
        setWindowOpacity(windowOpacity() + 0.1);
}

void MainWindow::deleteVariables()
{
    m_session->clearVariables();

    if (m_settings->variablesDockVisible)
        m_docks.variables->widget()->updateList();
}

void MainWindow::deleteUserFunctions()
{
    m_session->clearUserFunctions();

    if (m_settings->userFunctionsDockVisible)
        m_docks.userFunctions->widget()->updateList();
}

void MainWindow::setResultPrecision2Digits()
{
    setResultPrecision(2);
}

void MainWindow::setResultPrecision3Digits()
{
    setResultPrecision(3);
}

void MainWindow::setResultPrecision8Digits()
{
    setResultPrecision(8);
}

void MainWindow::setResultPrecision15Digits()
{
    setResultPrecision(15);
}

void MainWindow::setResultPrecision50Digits()
{
    setResultPrecision(50);
}

void MainWindow::setResultPrecisionAutomatic()
{
    setResultPrecision(-1);
}

void MainWindow::setResultPrecisionCustom()
{
    bool ok = false;
    const int current = (m_settings->resultPrecision >= 0) ? m_settings->resultPrecision : 8;
    const int precision = QInputDialog::getInt(this,
        tr("Custom Precision"),
        tr("Fractional digits:"),
        current,
        0,
        50,
        1,
        &ok);

    if (ok)
        setResultPrecision(precision);

    checkInitialResultPrecision();
}

void MainWindow::applySelectedColorScheme()
{
    m_settings->colorScheme = m_actionGroups.colorScheme->checkedAction()->data().toString();
    m_actions.settingsDisplayColorSchemeCustom->setChecked(false);
    emit colorSchemeChanged();
}

void MainWindow::applyColorSchemeFromAction(QAction* action)
{
    m_settings->colorScheme = action->data().toString();
    m_actions.settingsDisplayColorSchemeCustom->setChecked(false);
    emit colorSchemeChanged();
}

void MainWindow::saveColorSchemeToRevert()
{
    m_colorSchemeToRevert = m_settings->colorScheme;
    m_customColorSchemeJsonToRevert = m_settings->customColorSchemeJson;
}

void MainWindow::revertColorScheme()
{
    m_settings->colorScheme = m_colorSchemeToRevert;
    m_settings->customColorSchemeJson = m_customColorSchemeJsonToRevert;
    m_actions.settingsDisplayColorSchemeCustom->setChecked(m_settings->colorScheme == QLatin1String("Custom"));
    emit colorSchemeChanged();
}

void MainWindow::showCustomThemeDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Custom Theme"));
    dialog.setMinimumSize(720, 560);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(
        tr("Customize every theme color role. Import or export theme files in JSON format."),
        &dialog));

    QPlainTextEdit* preview = new QPlainTextEdit(&dialog);
    preview->setReadOnly(true);
    preview->setPlainText(
        QStringLiteral("2+2*5\n"
                       "3^4 + 2^10\n"
                       "sin(pi/6) + cos(pi/3)\n"
                       "distance=42[km]\n"
                       "speed=distance/1.5[h]\n"
                       "f(x)=x^2+2*x+1\n"
                       "f(ans) ? quadratic sample\n"
                       "= 256"));
    auto previewHighlighter = new SyntaxHighlighter(preview);
    layout->addWidget(preview);

    QWidget* rolesWidget = new QWidget(&dialog);
    QGridLayout* roleLayout = new QGridLayout(rolesWidget);
    roleLayout->setContentsMargins(0, 0, 0, 0);
    roleLayout->setHorizontalSpacing(10);
    roleLayout->setVerticalSpacing(6);
    layout->addWidget(rolesWidget);

    auto currentScheme = ColorScheme::loadByName(m_settings->colorScheme);
    if (!currentScheme.isValid()) {
        const QJsonDocument customDoc = QJsonDocument::fromJson(m_settings->customColorSchemeJson.toUtf8());
        currentScheme = ColorScheme(customDoc);
    }
    if (!currentScheme.isValid())
        currentScheme = ColorScheme::loadByName(QStringLiteral("Terminal"));
    if (!currentScheme.isValid())
        currentScheme = ColorScheme(QJsonDocument(QJsonObject()));

    QMap<ColorScheme::Role, QColor> colorsByRole;
    const auto roleEntries = ColorScheme::roleNames();
    for (const auto& roleEntry : roleEntries)
        colorsByRole.insert(roleEntry.second, currentScheme.colorForRole(roleEntry.second));

    QMap<ColorScheme::Role, QPushButton*> roleButtons;
    const auto applyPreview = [&colorsByRole, preview, previewHighlighter]() {
        QJsonObject object;
        const auto roles = ColorScheme::roleNames();
        for (const auto& roleEntry : roles)
            object.insert(roleEntry.first, colorsByRole.value(roleEntry.second).name());
        const ColorScheme scheme = ColorScheme::fromJsonObject(object);
        previewHighlighter->setColorScheme(ColorScheme::fromJsonObject(scheme.toJsonObject()));
        QPalette palette = preview->palette();
        palette.setColor(QPalette::Base, scheme.colorForRole(ColorScheme::Background));
        preview->setPalette(palette);
        previewHighlighter->rehighlight();
    };

    int row = 0;
    for (const auto& roleEntry : roleEntries) {
        const ColorScheme::Role role = roleEntry.second;
        QLabel* roleLabel = new QLabel(colorSchemeRoleLabel(role), rolesWidget);
        QPushButton* colorButton = new QPushButton(rolesWidget);
        updateColorButtonStyle(colorButton, colorsByRole.value(role));
        roleButtons.insert(role, colorButton);
        roleLayout->addWidget(roleLabel, row, 0);
        roleLayout->addWidget(colorButton, row, 1);
        connect(colorButton, &QPushButton::clicked, &dialog, [&, role]() {
            const QColor initial = colorsByRole.value(role);
            const QColor chosen = QColorDialog::getColor(initial, &dialog, tr("Select color for %1").arg(colorSchemeRoleLabel(role)));
            if (!chosen.isValid())
                return;
            colorsByRole[role] = chosen;
            updateColorButtonStyle(roleButtons.value(role), chosen);
            applyPreview();
        });
        ++row;
    }

    applyPreview();

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QPushButton* importButton = buttons->addButton(tr("Import..."), QDialogButtonBox::ActionRole);
    QPushButton* exportButton = buttons->addButton(tr("Export..."), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    connect(importButton, &QPushButton::clicked, this, [&, roleEntries]() {
        const QString filePath = QFileDialog::getOpenFileName(
            this, tr("Import Theme"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            tr("Theme file (*.json);;All files (*)"));
        if (filePath.isEmpty())
            return;
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, tr("Error"), tr("Can't read from file %1").arg(filePath));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        const ColorScheme importedScheme(doc);
        if (!importedScheme.isValid()) {
            QMessageBox::critical(this, tr("Error"), tr("Invalid theme file."));
            return;
        }
        for (const auto& roleEntry : roleEntries) {
            const QColor color = importedScheme.colorForRole(roleEntry.second);
            colorsByRole[roleEntry.second] = color;
            updateColorButtonStyle(roleButtons.value(roleEntry.second), color);
        }
        applyPreview();
    });
    connect(exportButton, &QPushButton::clicked, this, [&, roleEntries]() {
        QString filePath = QFileDialog::getSaveFileName(
            this, tr("Export Theme"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            tr("Theme file (*.json);;All files (*)"));
        if (filePath.isEmpty())
            return;
        if (!filePath.endsWith(QLatin1String(".json"), Qt::CaseInsensitive))
            filePath += QLatin1String(".json");
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::critical(this, tr("Error"), tr("Can't write to file %1").arg(filePath));
            return;
        }
        QJsonObject object;
        for (const auto& roleEntry : roleEntries)
            object.insert(roleEntry.first, colorsByRole.value(roleEntry.second).name());
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    });

    if (dialog.exec() != QDialog::Accepted) {
        m_actions.settingsDisplayColorSchemeCustom->setChecked(m_settings->colorScheme == QLatin1String("Custom"));
        return;
    }

    QJsonObject object;
    for (const auto& roleEntry : roleEntries)
        object.insert(roleEntry.first, colorsByRole.value(roleEntry.second).name());
    m_settings->customColorSchemeJson = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    m_settings->colorScheme = QStringLiteral("Custom");
    m_actions.settingsDisplayColorSchemeCustom->setChecked(true);
    emit colorSchemeChanged();
}

void MainWindow::selectEditorExpression()
{
    activateWindow();
    m_widgets.editor->selectAll();
    m_widgets.editor->setFocus();
}

void MainWindow::hideStateLabel()
{
    m_widgets.state->hide();
}

void MainWindow::handleEditorEscapePressed()
{
    if (m_widgets.state->isVisible()) {
        hideStateLabel();
        return;
    }

    cancelHistoryEntryEdit();
}

void MainWindow::showSessionLoadDialog()
{
    QString filters = tr("SpeedCrunch Sessions (*.json);;All Files (*)");
    QString fname = QFileDialog::getOpenFileName(this, tr("Load Session"), QString(), filters);
    if (fname.isEmpty())
        return;

    QFile file(fname);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't read from file %1").arg(fname));
        return;
    }

    // Ask for merge with current session.
    bool merge;
    QString mergeMsg = tr(
        "Merge session being loaded with current session?\n"
        "If no, current variables and display will be cleared."
    );
    QMessageBox::StandardButton button =
        QMessageBox::question(this, tr("Merge?"), mergeMsg,
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);

    if (button == QMessageBox::Yes)
        merge = true;
    else if (button == QMessageBox::No)
        merge = false;
    else return;

    QByteArray data = file.readAll();
    QJsonDocument doc(QJsonDocument::fromJson(data));
    m_session->deSerialize(doc.object(), merge);

    file.close();
    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();
    emit unitsChanged();

}

void MainWindow::wrapSelection()
{
    m_widgets.editor->wrapSelection();
}

void MainWindow::saveSessionDialog()
{
    QString filters = tr("SpeedCrunch Sessions (*.json);;All Files (*)");
    const QString sessionBaseName =
        QString(QLatin1String("session-%1"))
        .arg(QDateTime::currentDateTime().toString(QLatin1String("yyyy_MM_dd-HH_mm_ss")));
    const QString defaultFileName = sessionBaseName + QLatin1String(".json");

    QFileDialog dialog(this, tr("Save Session"), QString(), filters);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.selectFile(defaultFileName);
    QTimer::singleShot(0, &dialog, [sessionBaseName, &dialog]() {
        if (QLineEdit* fileNameEdit = dialog.findChild<QLineEdit*>())
            fileNameEdit->setSelection(0, sessionBaseName.size());
    });

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QStringList selectedFiles = dialog.selectedFiles();
    if (selectedFiles.isEmpty())
        return;

    QString fname = selectedFiles.constFirst();
    if (fname.isEmpty())
        return;

    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't write to file %1").arg(fname));
        return;
    }

    QJsonObject json;
    m_session->serialize(json);
    QJsonDocument doc(json);
    file.write(doc.toJson());

    file.close();
}

void MainWindow::showSessionImportDialog()
{
    QString filters = tr("All Files (*)");
    QString fname = QFileDialog::getOpenFileName(this, tr("Import Session"), QString(), filters);
    if (fname.isEmpty())
        return;

    QFile file(fname);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't read from file %1").arg(fname));
        return;
    }

    // Ask for merge with current session.
    QString mergeMsg = tr(
        "Merge session being imported with current session?\n"
        "If no, current variables and display will be cleared."
    );

    QMessageBox::StandardButton button =
        QMessageBox::question(this, tr("Merge?"), mergeMsg,
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);

    if (button == QMessageBox::Cancel)
        return;
    if (button == QMessageBox::No) {
        m_session->clearHistory();
        m_session->clearVariables();
        m_session->clearUserFunctions();
        m_session->clearUserUnits();
        m_evaluator->initializeBuiltInVariables();
    }

    QTextStream stream(&file);
    QString exp = stream.readLine();
    bool ignoreAll = false;
    while (!exp.isNull()) {
        const QString normalizedExp =
            EditorUtils::normalizeExpressionOperators(exp);
        m_widgets.editor->setText(normalizedExp);

        QString str = m_evaluator->autoFix(normalizedExp);

        m_evaluator->setExpression(str);

        Quantity result = m_evaluator->evalUpdateAns();
        if (!m_evaluator->error().isEmpty()) {
            if (!ignoreAll) {
                QMessageBox::StandardButton button =
                    QMessageBox::warning(this, tr("Error"), tr("Ignore error?") + "\n" + m_evaluator->error(),
                        QMessageBox::Yes | QMessageBox::YesToAll
                        | QMessageBox::Cancel, QMessageBox::Yes);

                if (button == QMessageBox::Cancel)
                    return;
                if (button == QMessageBox::YesToAll)
                    ignoreAll = true;
            }
        } else {
            const QString interpretedExpr = m_evaluator->interpretedExpression();
            HistoryEntry historyEntry(normalizedExp, result, interpretedExpr);
            historyEntry.setRenderedLines(renderedLinesForHistoryEntry(historyEntry, m_settings));
            m_session->addHistoryEntry(historyEntry);
            m_widgets.editor->setText(str);
            m_widgets.editor->selectAll();
            m_widgets.editor->stopAutoCalc();
            m_widgets.editor->stopAutoComplete();
            if(!result.isNan())
                m_conditions.autoAns = true;
        }

        exp = stream.readLine();
    }

    file.close();
    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();
    emit unitsChanged();

    if (!isActiveWindow())
        activateWindow();
}

void MainWindow::importUserDefinitionsFromText(const QString& text, bool overwriteExisting,
                                               int* importedVariables, int* importedFunctions,
                                               int* ignoredLines, QList<int>* ignoredLineNumbers,
                                               bool dryRun)
{
    int localImportedVariables = 0;
    int localImportedFunctions = 0;
    int localIgnoredLines = 0;
    QList<int> localIgnoredLineNumbers;
    Session sessionBackup;
    bool autoAnsBackup = false;

    if (dryRun && m_session) {
        sessionBackup = *m_session;
        autoAnsBackup = m_conditions.autoAns;
    }

    QString inputText = text;
    QTextStream stream(&inputText, QIODevice::ReadOnly);
    int lineNumber = 0;
    while (!stream.atEnd()) {
        const QString rawLine = stream.readLine();
        ++lineNumber;
        const QString normalizedExpression =
            EditorUtils::normalizeExpressionOperators(rawLine).trimmed();
        if (normalizedExpression.isEmpty())
            continue;

        const QString expression = m_evaluator->autoFix(normalizedExpression);
        if (expression.isEmpty() || Evaluator::isCommentOnlyExpression(expression))
            continue;

        const AssignmentTarget target =
            assignmentTargetFromExpression(m_evaluator, expression);
        if (!target.valid) {
            ++localIgnoredLines;
            localIgnoredLineNumbers.append(lineNumber);
            continue;
        }

        const bool hasExistingVariable = m_evaluator->hasVariable(target.identifier);
        const bool hasExistingUserVariable =
            hasExistingVariable && !m_evaluator->isBuiltInVariable(target.identifier);
        const bool hasExistingUserFunction =
            m_evaluator->hasUserFunction(target.identifier);
        bool hasExistingUserUnit =
            m_evaluator->hasUserUnit(target.identifier);
        if (hasExistingUserUnit) {
            const UserUnit* existingUnit = m_evaluator->getUserUnit(target.identifier);
            if (!existingUnit || existingUnit->value().isZero()) {
                m_evaluator->unsetUserUnit(target.identifier);
                hasExistingUserUnit = false;
            }
        }

        if (!overwriteExisting && (hasExistingVariable || hasExistingUserFunction || hasExistingUserUnit)) {
            ++localIgnoredLines;
            localIgnoredLineNumbers.append(lineNumber);
            continue;
        }

        Variable previousVariable;
        UserFunction previousFunction;
        UserUnit previousUnit;
        bool hasPreviousUserFunction = false;
        if (hasExistingUserVariable)
            previousVariable = m_evaluator->getVariable(target.identifier);
        if (hasExistingUserFunction)
            hasPreviousUserFunction = findUserFunctionByName(
                m_evaluator->getUserFunctions(),
                target.identifier,
                &previousFunction);
        if (hasExistingUserUnit && m_evaluator->getUserUnit(target.identifier))
            previousUnit = *m_evaluator->getUserUnit(target.identifier);

        if (overwriteExisting) {
            if (target.isFunction && hasExistingUserVariable) {
                m_evaluator->unsetVariable(target.identifier);
            } else if (!target.isFunction && !target.isUnit && hasExistingUserFunction) {
                m_evaluator->unsetUserFunction(target.identifier);
            } else if (target.isUnit) {
                if (hasExistingUserVariable)
                    m_evaluator->unsetVariable(target.identifier);
                if (hasExistingUserFunction)
                    m_evaluator->unsetUserFunction(target.identifier);
                if (hasExistingUserUnit)
                    m_evaluator->unsetUserUnit(target.identifier);
            }
        }

        m_evaluator->setExpression(expression);
        m_evaluator->eval();

        bool importSucceeded = false;
        if (m_evaluator->error().isEmpty()) {
            if (target.isFunction) {
                importSucceeded = m_evaluator->hasUserFunction(target.identifier);
            } else if (target.isUnit) {
                importSucceeded = m_evaluator->hasUserUnit(target.identifier);
            } else if (m_evaluator->hasVariable(target.identifier)
                       && !m_evaluator->isBuiltInVariable(target.identifier))
            {
                const Variable importedVariable = m_evaluator->getVariable(target.identifier);
                importSucceeded = !importedVariable.value().isNan();
            }
        }

        if (!importSucceeded) {
            if (target.isFunction) {
                if (m_evaluator->hasUserFunction(target.identifier))
                    m_evaluator->unsetUserFunction(target.identifier);
            } else if (target.isUnit) {
                if (m_evaluator->hasUserUnit(target.identifier))
                    m_evaluator->unsetUserUnit(target.identifier);
            } else if (m_evaluator->hasVariable(target.identifier)
                       && !m_evaluator->isBuiltInVariable(target.identifier))
            {
                m_evaluator->unsetVariable(target.identifier);
            }

            if (hasExistingUserVariable) {
                m_evaluator->setVariable(
                    previousVariable.identifier(),
                    previousVariable.value(),
                    previousVariable.type(),
                    previousVariable.description());
            }
            if (hasPreviousUserFunction)
                m_evaluator->setUserFunction(previousFunction);
            if (hasExistingUserUnit)
                m_evaluator->setUserUnit(previousUnit);

            ++localIgnoredLines;
            localIgnoredLineNumbers.append(lineNumber);
            continue;
        }

        if (target.isFunction)
            ++localImportedFunctions;
        else
            ++localImportedVariables;
    }

    if (!dryRun && (localImportedVariables > 0 || localImportedFunctions > 0)) {
        emit variablesChanged();
        emit functionsChanged();
        emit unitsChanged();
    }

    if (dryRun && m_session) {
        *m_session = sessionBackup;
        m_conditions.autoAns = autoAnsBackup;
    }

    if (importedVariables)
        *importedVariables = localImportedVariables;
    if (importedFunctions)
        *importedFunctions = localImportedFunctions;
    if (ignoredLines)
        *ignoredLines = localIgnoredLines;
    if (ignoredLineNumbers)
        *ignoredLineNumbers = localIgnoredLineNumbers;
}

void MainWindow::applyStartupUserDefinitions()
{
    const QString definitionsText = m_settings->startupUserDefinitions.trimmed();
    if (definitionsText.isEmpty())
        return;

    importUserDefinitionsFromText(
        definitionsText,
        m_settings->startupUserDefinitionsOverwrite);
}

void MainWindow::showUserDefinitionsImportDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Startup Definitions"));
    dialog.setMinimumSize(640, 420);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QLabel* info = new QLabel(
        tr("Define user variables, functions and units to load automatically on startup.\n"
           "Enter one definition per line."),
        &dialog);
    info->setWordWrap(true);
    layout->addWidget(info);

    QComboBox* strategy = new QComboBox(&dialog);
    strategy->addItem(tr("Merge with existing user functions, units and variables"), false);
    strategy->addItem(tr("Overwrite existing user functions, units and variables if applicable"), true);
    strategy->setCurrentIndex(m_settings->startupUserDefinitionsOverwrite ? 1 : 0);
    layout->addWidget(strategy);

    QLabel* overwriteWarning = new QLabel(
        tr("Warning: Overwrite mode replaces existing user variables/functions/units "
           "when imported names collide."),
        &dialog);
    overwriteWarning->setWordWrap(true);
    overwriteWarning->setStyleSheet(QStringLiteral(
        "QLabel { background-color: #fff3cd; color: #664d03; border: 1px solid #ffecb5; padding: 6px; }"));
    overwriteWarning->setVisible(strategy->currentData().toBool());
    layout->addWidget(overwriteWarning);

    QPlainTextEdit* textEdit = new QPlainTextEdit(&dialog);
    textEdit->setPlainText(m_settings->startupUserDefinitions);
    textEdit->setPlaceholderText(tr("Examples:\n"
                                    "my_rate=1.25\n"
                                    "f(x)=x^2+1\n"
                                    "[cm_s]=[centimetre/second]"));
    SyntaxHighlighter* startupDefinitionsHighlighter = new SyntaxHighlighter(textEdit);
    connect(this, &MainWindow::colorSchemeChanged, &dialog, [startupDefinitionsHighlighter]() {
        startupDefinitionsHighlighter->update();
    });
    connect(this, &MainWindow::syntaxHighlightingChanged, &dialog, [startupDefinitionsHighlighter]() {
        startupDefinitionsHighlighter->rehighlight();
    });
    QPlainTextEdit* lineNumbers = new QPlainTextEdit(&dialog);
    lineNumbers->setReadOnly(true);
    lineNumbers->setFrameStyle(QFrame::NoFrame);
    lineNumbers->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lineNumbers->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lineNumbers->setWordWrapMode(QTextOption::NoWrap);
    lineNumbers->setTextInteractionFlags(Qt::NoTextInteraction);
    lineNumbers->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: palette(alternate-base); color: palette(mid); }"));
    lineNumbers->setFixedWidth(56);
    lineNumbers->setFont(textEdit->font());

    QWidget* editorRow = new QWidget(&dialog);
    QHBoxLayout* editorRowLayout = new QHBoxLayout(editorRow);
    editorRowLayout->setContentsMargins(0, 0, 0, 0);
    editorRowLayout->setSpacing(0);
    editorRowLayout->addWidget(lineNumbers);
    editorRowLayout->addWidget(textEdit);
    layout->addWidget(editorRow);

    const auto updateLineNumbers = [textEdit, lineNumbers]() {
        QStringList lines;
        lines.reserve(textEdit->blockCount());
        for (int i = 1; i <= textEdit->blockCount(); ++i)
            lines.append(QString::number(i));
        lineNumbers->setPlainText(lines.join(QLatin1Char('\n')));
        lineNumbers->verticalScrollBar()->setValue(textEdit->verticalScrollBar()->value());
    };
    connect(textEdit, &QPlainTextEdit::textChanged, &dialog, updateLineNumbers);
    connect(textEdit->verticalScrollBar(), &QScrollBar::valueChanged, &dialog,
            [textEdit, lineNumbers](int) {
                lineNumbers->verticalScrollBar()->setValue(textEdit->verticalScrollBar()->value());
            });
    updateLineNumbers();

    QCheckBox* applyBeforeRestore = new QCheckBox(
        tr("Apply before session restore (advanced)"),
        &dialog);
    applyBeforeRestore->setChecked(m_settings->startupUserDefinitionsApplyBeforeRestore);
    layout->addWidget(applyBeforeRestore);

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal,
        &dialog);
    QPushButton* applyNowButton = buttons->addButton(tr("Apply Now"), QDialogButtonBox::ActionRole);
    QPushButton* testNowButton = buttons->addButton(tr("Test Now"), QDialogButtonBox::ActionRole);
    QPushButton* importButton = buttons->addButton(tr("Import..."), QDialogButtonBox::ActionRole);
    QPushButton* exportButton = buttons->addButton(tr("Export..."), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(strategy, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog,
            [strategy, overwriteWarning](int) {
                overwriteWarning->setVisible(strategy->currentData().toBool());
            });
    connect(importButton, &QPushButton::clicked, this, [this, textEdit]() {
        const QString fname = QFileDialog::getOpenFileName(
            this,
            tr("Import Startup Definitions"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            tr("Text file (*.txt);;All files (*)"));
        if (fname.isEmpty())
            return;

        QFile file(fname);
        if (!file.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, tr("Error"), tr("Can't read from file %1").arg(fname));
            return;
        }

        QTextStream stream(&file);
        textEdit->setPlainText(stream.readAll());
    });
    connect(exportButton, &QPushButton::clicked, this, [this, textEdit]() {
        QString fname = QFileDialog::getSaveFileName(
            this,
            tr("Export Startup Definitions"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
            tr("Text file (*.txt);;All files (*)"));
        if (fname.isEmpty())
            return;

        if (!fname.endsWith(QLatin1String(".txt"), Qt::CaseInsensitive))
            fname += QLatin1String(".txt");

        QFile file(fname);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::critical(this, tr("Error"), tr("Can't write to file %1").arg(fname));
            return;
        }

        QTextStream stream(&file);
        stream << textEdit->toPlainText();
    });
    connect(testNowButton, &QPushButton::clicked, this, [this, textEdit, strategy]() {
        int importedVariables = 0;
        int importedFunctions = 0;
        int ignoredLines = 0;
        QList<int> ignoredLineNumbers;
        importUserDefinitionsFromText(
            textEdit->toPlainText(),
            strategy->currentData().toBool(),
            &importedVariables,
            &importedFunctions,
            &ignoredLines,
            &ignoredLineNumbers,
            true);

        QString ignoredLineNumbersText;
        if (!ignoredLineNumbers.isEmpty()) {
            QStringList values;
            values.reserve(ignoredLineNumbers.size());
            for (int line : ignoredLineNumbers)
                values.append(QString::number(line));
            ignoredLineNumbersText = values.join(QStringLiteral(", "));
        }

        QMessageBox::information(
            this,
            tr("Test Results"),
            tr("Imported variables: %1\nImported functions: %2\nLine numbers with errors: %3")
                .arg(importedVariables)
                .arg(importedFunctions)
                .arg(ignoredLineNumbersText.isEmpty() ? tr("none") : ignoredLineNumbersText));
    });
    connect(applyNowButton, &QPushButton::clicked, this, [this, textEdit, strategy]() {
        int importedVariables = 0;
        int importedFunctions = 0;
        int ignoredLines = 0;
        QList<int> ignoredLineNumbers;
        importUserDefinitionsFromText(
            textEdit->toPlainText(),
            strategy->currentData().toBool(),
            &importedVariables,
            &importedFunctions,
            &ignoredLines,
            &ignoredLineNumbers,
            false);

        QString ignoredLineNumbersText;
        if (!ignoredLineNumbers.isEmpty()) {
            QStringList values;
            values.reserve(ignoredLineNumbers.size());
            for (int line : ignoredLineNumbers)
                values.append(QString::number(line));
            ignoredLineNumbersText = values.join(QStringLiteral(", "));
        }

        QMessageBox::information(
            this,
            tr("Apply Results"),
            tr("Imported variables: %1\nImported functions: %2\nLine numbers with errors: %3")
                .arg(importedVariables)
                .arg(importedFunctions)
                .arg(ignoredLineNumbersText.isEmpty() ? tr("none") : ignoredLineNumbersText));
    });

    if (dialog.exec() != QDialog::Accepted)
        return;

    m_settings->startupUserDefinitions = textEdit->toPlainText();
    m_settings->startupUserDefinitionsOverwrite = strategy->currentData().toBool();
    m_settings->startupUserDefinitionsApplyBeforeRestore = applyBeforeRestore->isChecked();
    StartupDefinitions::saveFrom(m_settings);

    showStateLabel(tr("Startup user definitions saved."));
}

void MainWindow::setAlwaysOnTopEnabled(bool b)
{
    m_settings->windowAlwaysOnTop = b;

    QPoint cur = mapToGlobal(QPoint(0, 0));
    if (b)
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    else
        setWindowFlags(windowFlags() & (~ Qt::WindowStaysOnTopHint));
    move(cur);
    show();
}

void MainWindow::setAutoAnsEnabled(bool b)
{
    m_settings->autoAns = b;
}

void MainWindow::setAutoCalcEnabled(bool b)
{
    m_settings->autoCalc = b;
    m_widgets.editor->setAutoCalcEnabled(b);
}

void MainWindow::setHistorySaving(QAction* action)
{
    if (!action)
        return;
    m_settings->historySaving = static_cast<Settings::HistorySaving>(action->data().toInt());
}

void MainWindow::setHistorySizeLimit()
{
    bool ok = false;
    const int current = m_settings->maxHistoryEntries;
    const int value = QInputDialog::getInt(
        this,
        tr("History Size Limit"),
        tr("Maximum number of history entries (0 = unlimited):"),
        current,
        0,
        1000000,
        100,
        &ok);

    if (!ok || value == current)
        return;

    m_settings->maxHistoryEntries = value;
    m_session->applyHistoryLimit();
    m_conditions.autoAns = !m_session->historyIsEmpty();
    emit historyChanged();
}

void MainWindow::setLeaveLastExpressionEnabled(bool b)
{
    m_settings->leaveLastExpression = b;
}

void MainWindow::setUpDownArrowBehavior(QAction* action)
{
    if (!action)
        return;
    m_settings->upDownArrowBehavior =
        static_cast<Settings::UpDownArrowBehavior>(action->data().toInt());
}

void MainWindow::setEmptyHistoryHintEnabled(bool b)
{
    m_settings->showEmptyHistoryHint = b;
    if (b && m_widgets.display->isEmpty())
        showReadyMessage();
    else if (!b)
        hideStateLabel();
}

void MainWindow::setWindowPositionSaveEnabled(bool b)
{
    m_settings->windowPositionSave = b;
}

void MainWindow::setSingleInstanceEnabled(bool b)
{
    m_settings->singleInstance = b;
}

void MainWindow::setAutoCompletionEnabled(bool b)
{
    m_settings->autoCompletion = b;
    m_widgets.editor->setAutoCompletionEnabled(b);
}

void MainWindow::setAutoCompletionBuiltInFunctionsEnabled(bool b)
{
    m_settings->autoCompletionBuiltInFunctions = b;
}

void MainWindow::setAutoCompletionBuiltInVariablesEnabled(bool b)
{
    m_settings->autoCompletionBuiltInVariables = b;
}

void MainWindow::setAutoCompletionLongFormUnitsEnabled(bool b)
{
    m_settings->autoCompletionLongFormUnits = b;
}

void MainWindow::setAutoCompletionUserFunctionsEnabled(bool b)
{
    m_settings->autoCompletionUserFunctions = b;
}

void MainWindow::setAutoCompletionUserVariablesEnabled(bool b)
{
    m_settings->autoCompletionUserVariables = b;
}

void MainWindow::setBitfieldVisible(bool b)
{
    if (b)
        createBitField();
    else
        deleteBitField();
}

void MainWindow::setSyntaxHighlightingEnabled(bool b)
{
    m_settings->syntaxHighlighting = b;
    emit syntaxHighlightingChanged();
}

void MainWindow::setDigitGrouping(QAction *action)
{
    m_settings->digitGrouping = action->data().toInt();
    emit historyChanged();
    m_widgets.editor->refreshAutoCalc();
    emit syntaxHighlightingChanged();
}

void MainWindow::setDigitGroupingIntegerPartOnlyEnabled(bool b)
{
    m_settings->digitGroupingIntegerPartOnly = b;
    emit historyChanged();
    m_widgets.editor->refreshAutoCalc();
    emit syntaxHighlightingChanged();
}

void MainWindow::setAutoResultToClipboardEnabled(bool b)
{
    m_settings->autoResultToClipboard = b;
}

void MainWindow::setSimplifyResultExpressionsEnabled(bool b)
{
    m_settings->simplifyResultExpressions = b;
    emit historyChanged();
    m_widgets.editor->refreshAutoCalc();
}

void MainWindow::setHoverHighlightResultsEnabled(bool b)
{
    m_settings->hoverHighlightResults = b;
    m_widgets.display->setHoverHighlightEnabled(b);
}

void MainWindow::setAngleModeDegree()
{
    if (m_settings->angleUnit == 'd')
        return;

    m_settings->angleUnit = 'd';

    setStatusBarText();

    m_evaluator->initializeAngleUnits();
    emit angleUnitChanged();
}

void MainWindow::setAngleModeRadian()
{
    if (m_settings->angleUnit == 'r')
        return;

    m_settings->angleUnit = 'r';

    setStatusBarText();

    m_evaluator->initializeAngleUnits();
    emit angleUnitChanged();
}

void MainWindow::setAngleModeGradian()
{
    if (m_settings->angleUnit == 'g')
        return;

    m_settings->angleUnit = 'g';

    setStatusBarText();

    m_evaluator->initializeAngleUnits();
    emit angleUnitChanged();
}

void MainWindow::setAngleModeTurn()
{
    if (m_settings->angleUnit == 't')
        return;

    m_settings->angleUnit = 't';

    setStatusBarText();

    m_evaluator->initializeAngleUnits();
    emit angleUnitChanged();
}

void MainWindow::setAngleModeRevolution()
{
    if (m_settings->angleUnit == 'v')
        return;

    m_settings->angleUnit = 'v';

    setStatusBarText();

    m_evaluator->initializeAngleUnits();
    emit angleUnitChanged();
}

inline static QString documentsLocation()
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

void MainWindow::exportHtml()
{
    QString fname = QFileDialog::getSaveFileName(this, tr("Export session as HTML"),
        documentsLocation(), tr("HTML file (*.html)"));

    if (fname.isEmpty())
        return;

    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't write to file %1").arg(fname));
        return;
    }

    QTextStream stream(& file);
    stream << m_widgets.display->exportHtml();

    file.close();
}

void MainWindow::exportPlainText()
{
    QString fname = QFileDialog::getSaveFileName(this, tr("Export session as plain text"),                                                 
                            documentsLocation(), tr("Text file (*.txt);;Any file (*.*)"));

    if (fname.isEmpty())
        return;

    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Can't write to file %1").arg(fname));
        return;
    }

    QByteArray text;
    QTextStream stream(&text, QIODevice::WriteOnly | QIODevice::Text);
    stream << m_widgets.display->document()->toPlainText();
    stream.flush();

    file.write(text);
    file.close();
}

void MainWindow::setWidgetsDirection()
{
    QLocale::Language lang = QLocale().language();
    bool rtlSystem = (lang == QLocale::Hebrew || lang == QLocale::Arabic || lang == QLocale::Persian);

    QString code = m_settings->language;
    bool rtlCustom = (code.contains("he") || code.contains("ar") || code.contains("fa"));

    if ((m_settings->language == "C" && rtlSystem) || rtlCustom)
        qApp->setLayoutDirection(Qt::RightToLeft);
    else
        qApp->setLayoutDirection(Qt::LeftToRight);
}

void MainWindow::showFontDialog()
{
    bool ok;
    QFont f = QFontDialog::getFont(&ok, m_widgets.display->font(), this, tr("Display font"));
    if (!ok)
        return;
    m_widgets.display->setFont(f);
    m_widgets.editor->setFont(f);
    if (m_widgets.state->isVisible())
        showStateLabel(m_widgets.state->text());
}

void MainWindow::setStatusBarVisible(bool b)
{
    b ? createStatusBar() : deleteStatusBar();
    m_settings->statusBarVisible = b;
}

void MainWindow::setMenuBarVisible(bool b)
{
    menuBar()->setVisible(b);
    m_settings->menuBarVisible = b;
}

void MainWindow::showStateLabel(const QString& msg)
{
    const int closeButtonSize = qMax(14, m_widgets.editor->fontMetrics().height() - 2);
    const int closeButtonRightPadding = 2;
    const int closeButtonTopPadding = 1;
    const int closeButtonReservedWidth = closeButtonSize + closeButtonRightPadding + 2;
    m_widgets.state->setContentsMargins(6, 3, closeButtonReservedWidth, 3);
    m_widgets.state->setFont(m_widgets.editor->font());
    m_widgets.state->setText(msg);
    m_widgets.stateCloseButton->setFixedSize(closeButtonSize, closeButtonSize);
    m_widgets.state->adjustSize();
    m_widgets.stateCloseButton->move(
        m_widgets.state->width() - closeButtonSize - closeButtonRightPadding,
        closeButtonTopPadding);
    m_widgets.stateCloseButton->show();
    m_widgets.stateCloseButton->raise();
    m_widgets.state->show();
    m_widgets.state->raise();
    const int height = m_widgets.state->height();
    QPoint pos = mapFromGlobal(m_widgets.editor->mapToGlobal(QPoint(0, -height)));
    m_widgets.state->move(pos);
}

void MainWindow::handleAutoCalcMessageAvailable(const QString& message)
{
    showStateLabel(message);
}

void MainWindow::handleAutoCalcQuantityAvailable(const Quantity& quantity)
{
    if (m_settings->bitfieldVisible)
        m_widgets.bitField->updateBits(quantity);
}

void MainWindow::setFullScreenEnabled(bool b)
{
    m_settings->windowOnfullScreen = b;
    b ? showFullScreen() : showNormal();
}

bool MainWindow::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_widgets.state && e->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(e);
        if (mouseEvent->button() == Qt::LeftButton) {
            hideStateLabel();
            return true;
        }
    }

    if (o == m_status.angleUnitLabel || o == m_status.resultFormatLabel
            || o == m_status.resultPrecisionLabel || o == m_status.complexFormLabel) {
        if (e->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(e);
            if (mouseEvent->button() == Qt::LeftButton) {
                const QPoint popupPoint(0, static_cast<QWidget*>(o)->height());
                if (o == m_status.angleUnitLabel)
                    showAngleModeContextMenu(m_status.angleUnit->mapFromGlobal(static_cast<QWidget*>(o)->mapToGlobal(popupPoint)));
                else if (o == m_status.complexFormLabel)
                    showComplexFormContextMenu(m_status.complexForm->mapFromGlobal(static_cast<QWidget*>(o)->mapToGlobal(popupPoint)));
                else if (o == m_status.resultPrecisionLabel)
                    showPrecisionContextMenu(m_status.resultPrecision->mapFromGlobal(static_cast<QWidget*>(o)->mapToGlobal(popupPoint)));
                else
                    showResultFormatContextMenu(m_status.resultFormat->mapFromGlobal(static_cast<QWidget*>(o)->mapToGlobal(popupPoint)));
                return true;
            }
            if (mouseEvent->button() == Qt::RightButton) {
                const QPoint globalPoint = static_cast<QWidget*>(o)->mapToGlobal(mouseEvent->pos());
                if (o == m_status.angleUnitLabel)
                    showAngleModeContextMenu(m_status.angleUnit->mapFromGlobal(globalPoint));
                else if (o == m_status.complexFormLabel)
                    showComplexFormContextMenu(m_status.complexForm->mapFromGlobal(globalPoint));
                else if (o == m_status.resultPrecisionLabel)
                    showPrecisionContextMenu(m_status.resultPrecision->mapFromGlobal(globalPoint));
                else
                    showResultFormatContextMenu(m_status.resultFormat->mapFromGlobal(globalPoint));
                return true;
            }
        }
        if (e->type() == QEvent::ContextMenu) {
            QContextMenuEvent* contextMenuEvent = static_cast<QContextMenuEvent*>(e);
            const QPoint globalPoint = contextMenuEvent->globalPos();
            if (o == m_status.angleUnitLabel)
                showAngleModeContextMenu(m_status.angleUnit->mapFromGlobal(globalPoint));
            else if (o == m_status.complexFormLabel)
                showComplexFormContextMenu(m_status.complexForm->mapFromGlobal(globalPoint));
            else if (o == m_status.resultPrecisionLabel)
                showPrecisionContextMenu(m_status.resultPrecision->mapFromGlobal(globalPoint));
            else
                showResultFormatContextMenu(m_status.resultFormat->mapFromGlobal(globalPoint));
            return true;
        }
    }

    if (o == m_docks.book) {
        if (e->type() == QEvent::Close) {
            deleteBookDock();
            return true;
        }
        return false;
    }

    if (o == m_docks.constants) {
        if (e->type() == QEvent::Close) {
            deleteConstantsDock();
            return true;
        }
        return false;
    }

    if (o == m_docks.functions) {
        if (e->type() == QEvent::Close) {
            deleteFunctionsDock();
            return true;
        }
        return false;
    }

    if (o == m_docks.history) {
        if (e->type() == QEvent::Close) {
            deleteHistoryDock();
            return true;
        }
        return false;
    }

    if (o == m_docks.variables) {
        if (e->type() == QEvent::Close) {
            deleteVariablesDock();
            return true;
        }
        return false;
    }

    if (o == m_docks.userFunctions) {
        if (e->type() == QEvent::Close) {
            deleteUserFunctionsDock();
            return true;
        }
        return false;
    }

    if (o == m_docks.userUnits) {
        if (e->type() == QEvent::Close) {
            deleteUserUnitsDock();
            return true;
        }
        return false;
    }

    return QMainWindow::eventFilter(o, e);
}

void MainWindow::deleteKeypad()
{
    if (!m_widgets.keypad)
        return;

    disconnect(m_widgets.keypad);
    m_widgets.keypad->deleteLater();
    m_widgets.keypad = 0;

    m_layouts.root->removeItem(m_layouts.keypad);
    m_layouts.keypad->deleteLater();
    m_layouts.keypad = 0;

    m_settings->keypadVisible = false;
}

void MainWindow::deleteStatusBar()
{
    statusBar()->hide();
    m_status.angleUnitSection->deleteLater();
    m_status.angleUnit = 0;
    m_status.angleUnitSection = 0;
    m_status.angleUnitLabel = 0;

    m_status.resultFormatSection->deleteLater();
    m_status.resultFormat = 0;
    m_status.resultFormatSection = 0;
    m_status.resultFormatLabel = 0;
    m_status.resultPrecisionSection->deleteLater();
    m_status.resultPrecision = 0;
    m_status.resultPrecisionSection = 0;
    m_status.resultPrecisionLabel = 0;
    m_status.complexFormSection->deleteLater();
    m_status.complexForm = 0;
    m_status.complexFormSection = 0;
    m_status.complexFormLabel = 0;

    setStatusBar(0);
}

void MainWindow::deleteBitField()
{
    m_widgets.bitField->hide();
    m_layouts.root->removeWidget(m_widgets.bitField);
    disconnect(m_widgets.bitField);
    m_widgets.bitField->deleteLater();
    m_widgets.bitField = 0;
    m_settings->bitfieldVisible = false;
}

void MainWindow::deleteBookDock()
{
    if (!m_docks.book)
        return;

    deleteDock(m_docks.book);
    m_docks.book = nullptr;
    m_actions.viewFormulaBook->setChecked(false);
    m_settings->formulaBookDockVisible = false;
}

void MainWindow::deleteConstantsDock()
{
    if (!m_docks.constants)
        return;

    deleteDock(m_docks.constants);
    m_docks.constants = nullptr;
    m_actions.viewConstants->setChecked(false);
    m_settings->constantsDockVisible = false;
}

void MainWindow::deleteFunctionsDock()
{
    if (!m_docks.functions)
        return;

    deleteDock(m_docks.functions);
    m_docks.functions = nullptr;
    m_actions.viewFunctions->setChecked(false);
    m_settings->functionsDockVisible = false;
}

void MainWindow::deleteHistoryDock()
{
    if (!m_docks.history)
        return;

    deleteDock(m_docks.history);
    m_docks.history = nullptr;
    m_actions.viewHistory->setChecked(false);
    m_settings->historyDockVisible = false;
}

void MainWindow::deleteVariablesDock()
{
    if (!m_docks.variables)
        return;

    deleteDock(m_docks.variables);
    m_docks.variables = nullptr;
    m_actions.viewVariables->setChecked(false);
    m_settings->variablesDockVisible = false;
}

void MainWindow::deleteUserFunctionsDock()
{
    if (!m_docks.userFunctions)
        return;

    deleteDock(m_docks.userFunctions);
    m_docks.userFunctions = nullptr;
    m_actions.viewUserFunctions->setChecked(false);
    m_settings->userFunctionsDockVisible = false;
}

void MainWindow::deleteUserUnitsDock()
{
    if (!m_docks.userUnits)
        return;

    deleteDock(m_docks.userUnits);
    m_docks.userUnits = nullptr;
    m_actions.viewUserUnits->setChecked(false);
    m_settings->userUnitsDockVisible = false;
}

void MainWindow::setFunctionsDockVisible(bool b, bool takeFocus)
{
    if (b)
        createFunctionsDock(takeFocus);
    else
        deleteFunctionsDock();
}

void MainWindow::setFormulaBookDockVisible(bool b, bool takeFocus)
{
    if (b)
        createBookDock(takeFocus);
    else
        deleteBookDock();
}

void MainWindow::setConstantsDockVisible(bool b, bool takeFocus)
{
    if (b)
        createConstantsDock(takeFocus);
    else
        deleteConstantsDock();
}

void MainWindow::setHistoryDockVisible(bool b, bool takeFocus)
{
    if (b)
        createHistoryDock(takeFocus);
    else
        deleteHistoryDock();
}

void MainWindow::setVariablesDockVisible(bool b, bool takeFocus)
{
    if (b)
        createVariablesDock(takeFocus);
    else
        deleteVariablesDock();
}

void MainWindow::setUserFunctionsDockVisible(bool b, bool takeFocus)
{
    if (b)
        createUserFunctionsDock(takeFocus);
    else
        deleteUserFunctionsDock();
}

void MainWindow::setUserUnitsDockVisible(bool b, bool takeFocus)
{
    if (b)
        createUserUnitsDock(takeFocus);
    else
        deleteUserUnitsDock();
}

void MainWindow::setKeypadVisible(bool b)
{
    if (b && !m_widgets.keypad)
        createKeypad();
    else if (!b && m_widgets.keypad)
        deleteKeypad();
}

void MainWindow::setKeypadMode(QAction* action)
{
    if (!action)
        return;

    const Settings::KeypadMode mode = static_cast<Settings::KeypadMode>(action->data().toInt());
    const bool isCustomMode = (mode == Settings::KeypadModeCustom);
    if (isCustomMode && !configureCustomKeypad()) {
        switch (m_settings->keypadMode) {
        case Settings::KeypadModeBasicWide:
            m_actions.viewKeypadBasicWide->setChecked(true);
            break;
        case Settings::KeypadModeScientificWide:
            m_actions.viewKeypadScientificWide->setChecked(true);
            break;
        case Settings::KeypadModeScientificNarrow:
            m_actions.viewKeypadScientificNarrow->setChecked(true);
            break;
        case Settings::KeypadModeCustom:
            m_actions.viewKeypadCustom->setChecked(true);
            break;
        case Settings::KeypadModeDisabled:
        default:
            m_actions.viewKeypadDisabled->setChecked(true);
            break;
        }
        return;
    }

    if (m_settings->keypadMode == mode && !isCustomMode)
        return;

    const bool wasVisible = isVisibleKeypadMode(m_settings->keypadMode);
    const bool nowVisible = isVisibleKeypadMode(mode);
    m_settings->keypadMode = mode;

    if (wasVisible && nowVisible) {
        deleteKeypad();
        createKeypad();
        return;
    }

    setKeypadVisible(nowVisible);
}

void MainWindow::setKeypadZoom(QAction* action)
{
    if (!action)
        return;

    const int zoomPercent = action->data().toInt();
    if (zoomPercent != 100 && zoomPercent != 150 && zoomPercent != 200)
        return;
    if (m_settings->keypadZoomPercent == zoomPercent)
        return;

    m_settings->keypadZoomPercent = zoomPercent;
    if (m_widgets.keypad) {
        deleteKeypad();
        createKeypad();
    }
}

bool MainWindow::configureCustomKeypad()
{
    CustomKeypadDialog dialog(m_settings->customKeypad, this);
    if (dialog.exec() != QDialog::Accepted)
        return false;

    m_settings->customKeypad = dialog.customKeypad();
    return true;
}

void MainWindow::setResultFormatBinary()
{
    setResultFormat('b');
    setStatusBarText();
}

void MainWindow::setResultFormatComplexDisabled()
{
    const bool shouldDisable = m_actions.settingsResultFormatComplexDisabled->isChecked();
    const bool isDisabled = !m_settings->complexNumbers;
    if (shouldDisable == isDisabled)
        return;

    m_settings->complexNumbers = !shouldDisable;
    DMath::complexMode = !shouldDisable;
    m_evaluator->initializeBuiltInVariables();
    setStatusBarText();
    emit complexNumbersChanged();
    emit resultFormatChanged();
}

void MainWindow::setResultFormatCartesian()
{
    if (m_settings->complexNumbers && m_settings->resultFormatComplex == 'c')
        return;

    const bool complexWasDisabled = !m_settings->complexNumbers;
    m_settings->complexNumbers = true;
    m_settings->resultFormatComplex = 'c';
    if (complexWasDisabled) {
        DMath::complexMode = true;
        m_evaluator->initializeBuiltInVariables();
    }
    setStatusBarText();
    if (complexWasDisabled)
        emit complexNumbersChanged();
    emit resultFormatChanged();
}

void MainWindow::setResultFormatEngineering()
{
    setResultFormat('n');
    setStatusBarText();
}

void MainWindow::setResultFormatFixed()
{
    setResultFormat('f');
    setStatusBarText();
}
void MainWindow::setResultFormatGeneral()
{
    setResultFormat('g');
    setStatusBarText();
}

void MainWindow::setResultFormatHexadecimal()
{
    setResultFormat('h');
    setStatusBarText();
}

void MainWindow::setImaginaryUnitI()
{
    if (m_settings->complexNumbers && m_settings->imaginaryUnit == 'i')
        return;

    m_settings->complexNumbers = true;
    DMath::complexMode = true;
    m_settings->imaginaryUnit = 'i';
    CMath::setImaginaryUnitSymbol(QLatin1Char('i'));
    m_evaluator->initializeBuiltInVariables();
    setStatusBarText();
    emit complexNumbersChanged();
    emit resultFormatChanged();
}

void MainWindow::setImaginaryUnitJ()
{
    if (m_settings->complexNumbers && m_settings->imaginaryUnit == 'j')
        return;

    m_settings->complexNumbers = true;
    DMath::complexMode = true;
    m_settings->imaginaryUnit = 'j';
    CMath::setImaginaryUnitSymbol(QLatin1Char('j'));
    m_evaluator->initializeBuiltInVariables();
    setStatusBarText();
    emit complexNumbersChanged();
    emit resultFormatChanged();
}

void MainWindow::setResultFormatOctal()
{
    setResultFormat('o');
    setStatusBarText();
}

void MainWindow::setResultFormatPolar()
{
    if (m_settings->complexNumbers && m_settings->resultFormatComplex == 'p')
        return;

    const bool complexWasDisabled = !m_settings->complexNumbers;
    m_settings->complexNumbers = true;
    m_settings->resultFormatComplex = 'p';
    if (complexWasDisabled) {
        DMath::complexMode = true;
        m_evaluator->initializeBuiltInVariables();
    }
    setStatusBarText();
    if (complexWasDisabled)
        emit complexNumbersChanged();
    emit resultFormatChanged();
}

void MainWindow::setResultFormatPolarAngle()
{
    if (m_settings->complexNumbers && m_settings->resultFormatComplex == 'a')
        return;

    const bool complexWasDisabled = !m_settings->complexNumbers;
    m_settings->complexNumbers = true;
    m_settings->resultFormatComplex = 'a';
    if (complexWasDisabled) {
        DMath::complexMode = true;
        m_evaluator->initializeBuiltInVariables();
    }
    setStatusBarText();
    if (complexWasDisabled)
        emit complexNumbersChanged();
    emit resultFormatChanged();
}

void MainWindow::showAngleModeContextMenu(const QPoint& point)
{
    m_menus.angleUnit->popup(m_status.angleUnit->mapToGlobal(point));
}

void MainWindow::setResultFormatScientific()
{
    setResultFormat('e');
    setStatusBarText();
}

void MainWindow::setResultFormatRational()
{
    setResultFormat('r');
    setStatusBarText();
}

void MainWindow::setResultFormatSexagesimal()
{
    setResultFormat('s');
    setStatusBarText();
}

void MainWindow::insertConstantIntoEditor(const QString& c)
{
    if (c.isEmpty())
        return;

    QString s = c;
    s.replace(MathDsl::DotSep, m_settings->radixCharacter());
    insertTextIntoEditor(s);
}

void MainWindow::insertTextIntoEditor(const QString& s)
{
    if (s.isEmpty())
        return;

    const QString normalized = EditorUtils::normalizeExpressionOperatorsForEditorInput(s);
    const bool atExpressionStart = [&]() {
        const QString text = m_widgets.editor->text();
        int i = m_widgets.editor->textCursor().position() - 1;
        while (i >= 0 && text.at(i).isSpace())
            --i;
        return i < 0;
    }();
    if (atExpressionStart && !normalized.isEmpty()) {
        int firstNonSpace = 0;
        while (firstNonSpace < normalized.size() && normalized.at(firstNonSpace).isSpace())
            ++firstNonSpace;

        if (firstNonSpace < normalized.size()) {
            const QChar leadingChar = normalized.at(firstNonSpace);
            if (!EditorUtils::isAllowedLeadingCharAtExpressionStart(leadingChar, m_settings->autoAns))
                return;
        }
    }

    bool shouldAutoComplete = m_widgets.editor->isAutoCompletionEnabled();
    m_widgets.editor->setAutoCompletionEnabled(false);
    m_widgets.editor->insert(normalized);
    m_widgets.editor->setAutoCompletionEnabled(shouldAutoComplete);

    if (!isActiveWindow())
        activateWindow();
    m_widgets.editor->setFocus();
}

void MainWindow::insertFunctionIntoEditor(const QString& f)
{
    if (f.isEmpty())
        return;

    const QString functionCall = f + QStringLiteral("()");
    const bool keepAsciiFunctionName =
        (f.compare(QStringLiteral("sqrt"), Qt::CaseInsensitive) == 0
         || f.compare(QStringLiteral("cbrt"), Qt::CaseInsensitive) == 0
         || f.compare(QStringLiteral("summation"), Qt::CaseInsensitive) == 0);
    if (keepAsciiFunctionName) {
        bool shouldAutoComplete = m_widgets.editor->isAutoCompletionEnabled();
        m_widgets.editor->setAutoCompletionEnabled(false);
        m_widgets.editor->insertPlainText(functionCall);
        m_widgets.editor->setAutoCompletionEnabled(shouldAutoComplete);

        if (!isActiveWindow())
            activateWindow();
        m_widgets.editor->setFocus();
    } else {
        insertTextIntoEditor(functionCall);
    }

    QTextCursor cursor = m_widgets.editor->textCursor();
    cursor.movePosition(QTextCursor::PreviousCharacter);
    m_widgets.editor->setTextCursor(cursor);
}

void MainWindow::handleKeypadButtonPress(Keypad::Button b)
{
    const auto typeWithRules = [this](const QString& s) {
        typeTextThroughEditorInputRules(m_widgets.editor, s);
        if (!isActiveWindow())
            activateWindow();
        m_widgets.editor->setFocus();
    };

    switch (b) {
    case Keypad::Key0: typeWithRules("0"); break;
    case Keypad::Key1: typeWithRules("1"); break;
    case Keypad::Key2: typeWithRules("2"); break;
    case Keypad::Key3: typeWithRules("3"); break;
    case Keypad::Key4: typeWithRules("4"); break;
    case Keypad::Key5: typeWithRules("5"); break;
    case Keypad::Key6: typeWithRules("6"); break;
    case Keypad::Key7: typeWithRules("7"); break;
    case Keypad::Key8: typeWithRules("8"); break;
    case Keypad::Key9: typeWithRules("9"); break;

    case Keypad::KeyPlus: typeWithRules("+"); break;
    case Keypad::KeyMinus: typeWithRules("−"); break;
    case Keypad::KeyTimes: typeWithRules(QString(MathDsl::MulCrossOp)); break;
    case Keypad::KeyDivide: typeWithRules("÷"); break;

    case Keypad::KeyEE: insertTextIntoEditor("e"); break;
    case Keypad::KeyLeftPar: typeWithRules("("); break;
    case Keypad::KeyRightPar: typeWithRules(")"); break;
    case Keypad::KeyRaise: typeWithRules("^"); break;
    case Keypad::KeyBackspace: {
        m_widgets.editor->doBackspace();
        if (!isActiveWindow())
            activateWindow();
        m_widgets.editor->setFocus();
        break;
    }
    case Keypad::KeyPercent: typeWithRules("%"); break;
    case Keypad::KeyFactorial: typeWithRules("!"); break;

    case Keypad::KeyX: insertTextIntoEditor("x"); break;
    case Keypad::KeyXEquals: insertTextIntoEditor("x="); break;
    case Keypad::KeyPi: insertTextIntoEditor("pi"); break;
    case Keypad::KeyAns: insertTextIntoEditor("ans"); break;

    case Keypad::KeySqrt: insertTextIntoEditor("sqrt("); break;
    case Keypad::KeyCbrt: insertTextIntoEditor("cbrt("); break;
    case Keypad::KeyLg: insertTextIntoEditor("lg("); break;
    case Keypad::KeyMod: insertTextIntoEditor("mod("); break;
    case Keypad::KeyLn: insertTextIntoEditor("ln("); break;
    case Keypad::KeyExp:insertTextIntoEditor("exp("); break;
    case Keypad::KeySin: insertTextIntoEditor("sin("); break;
    case Keypad::KeyCos: insertTextIntoEditor("cos("); break;
    case Keypad::KeyTan: insertTextIntoEditor("tan("); break;
    case Keypad::KeyAcos: insertTextIntoEditor("arccos("); break;
    case Keypad::KeyAtan: insertTextIntoEditor("arctan("); break;
    case Keypad::KeyAsin: insertTextIntoEditor("arcsin("); break;

    case Keypad::KeyRadixChar: typeWithRules(QString(m_settings->radixCharacter())); break;

    case Keypad::KeyClear: clearEditor(); break;
    case Keypad::KeyEquals: evaluateEditorExpression(); break;

    default: break;
    }
}

void MainWindow::handleCustomKeypadButtonPress(int action, const QString& text)
{
    const auto typeWithRules = [this](const QString& s) {
        typeTextThroughEditorInputRules(m_widgets.editor, s);
        if (!isActiveWindow())
            activateWindow();
        m_widgets.editor->setFocus();
    };

    switch (static_cast<Settings::CustomKeypadButtonAction>(action)) {
    case Settings::CustomKeypadActionInsertText:
        typeWithRules(text);
        break;
    case Settings::CustomKeypadActionBackspace: {
        m_widgets.editor->doBackspace();
        if (!isActiveWindow())
            activateWindow();
        m_widgets.editor->setFocus();
        break;
    }
    case Settings::CustomKeypadActionClearExpression:
        clearEditor();
        break;
    case Settings::CustomKeypadActionEvaluateExpression:
        evaluateEditorExpression();
        break;
    default:
        break;
    }
}

void MainWindow::checkForUpdates()
{
    if (!m_versionCheck)
        return;
    m_versionCheck->checkForUpdateNow();
}

void MainWindow::openFeedbackURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1("https://bitbucket.org/heldercorreia/speedcrunch/issues?status=new&status=open")));
}

void MainWindow::openCommunityURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1("https://groups.google.com/group/speedcrunch/")));
}

void MainWindow::openNewsURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1("http://speedcrunch.blogspot.com/")));
}

void MainWindow::openDonateURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1("https://www.speedcrunch.org/donate.html")));
}

void MainWindow::copy()
{
    m_copyWidget->copy();
}

void MainWindow::restoreSession(bool restoreHistory) {
    QString data_path = Settings::getDataPath();
    QDir qdir;
    qdir.mkpath(data_path);
    data_path.append("/history.json");

    QFile file(data_path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    QJsonDocument doc(QJsonDocument::fromJson(data));
    QJsonObject json = doc.object();
    if (!restoreHistory)
        json.remove(QLatin1String("history"));
    m_session->deSerialize(json, true);

    file.close();
    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();
    emit unitsChanged();

    m_conditions.autoAns = restoreHistory && !m_session->historyIsEmpty();
}

void MainWindow::evaluateEditorExpression()
{
    const bool startedFromHistoryEdit = (m_pendingHistoryEditIndex >= 0);
    const QString enteredExpr = m_widgets.editor->text();
    QString expr = m_evaluator->autoFix(enteredExpr);
    const bool isCommentOnly = Evaluator::isCommentOnlyExpression(expr);

    if (expr.isEmpty())
        return;

    if (m_pendingHistoryEditIndex >= 0) {
        const int previousDisplayScrollValue = m_widgets.display->verticalScrollBar()->value();
        const auto restoreDisplayScroll = [this, previousDisplayScrollValue]() {
            QScrollBar* bar = m_widgets.display->verticalScrollBar();
            const int clamped = qBound(bar->minimum(), previousDisplayScrollValue, bar->maximum());
            bar->setValue(clamped);
            QTimer::singleShot(0, this, [this, previousDisplayScrollValue]() {
                QScrollBar* deferredBar = m_widgets.display->verticalScrollBar();
                const int deferredClamped = qBound(deferredBar->minimum(),
                                                   previousDisplayScrollValue,
                                                   deferredBar->maximum());
                deferredBar->setValue(deferredClamped);
            });
        };
        const int historySize = m_session->historySize();
        if (m_pendingHistoryEditIndex >= historySize) {
            m_pendingHistoryEditIndex = -1;
            m_widgets.display->setEditingHistoryIndex(-1);
            m_widgets.editor->setHistoryArrowNavigationEnabled(true);
            m_widgets.editor->clear();
            restoreDisplayScroll();
        } else {
            const QList<HistoryEntry> previousEntries = historyEntries();
            QList<HistoryEntry> updatedEntries = previousEntries;
            HistoryEntry updatedEntry = updatedEntries.at(m_pendingHistoryEditIndex);
            updatedEntry.setExpr(enteredExpr);
            updatedEntries[m_pendingHistoryEditIndex] = updatedEntry;

            int errorIndex = -1;
            QString errorText;
            if (!rebuildSessionFromEntries(updatedEntries, m_pendingHistoryEditIndex, &errorIndex, &errorText)) {
                m_widgets.display->setEditingHistoryIndex(m_pendingHistoryEditIndex);
                restoreDisplayScroll();
                showStateLabel(tr("Could not recalculate from calculation %1: %2").arg(errorIndex + 1).arg(errorText));
                return;
            }

            m_pendingHistoryEditIndex = -1;
            m_widgets.display->setEditingHistoryIndex(-1);
            m_widgets.editor->setHistoryArrowNavigationEnabled(true);
            emit historyChanged();
            emit variablesChanged();
            emit functionsChanged();
            emit unitsChanged();
            restoreDisplayScroll();
            m_widgets.editor->clear();

            m_widgets.editor->stopAutoCalc();
            m_widgets.editor->stopAutoComplete();
            return;
        }
    }

    const EvaluationContext evalContext = currentEvaluationContext(m_settings);
    m_evaluator->setExpression(expr);
    Quantity result = m_evaluator->evalUpdateAns();

    if (!m_evaluator->error().isEmpty()) {
        showStateLabel(m_evaluator->error());
        return;
    }

    if (m_evaluator->isUserFunctionAssign()) {
        result = CMath::nan();
        emit functionsChanged();
    } else if (m_evaluator->isUserUnitAssign()) {
        result = CMath::nan();
        emit unitsChanged();
    } else if (result.isNan() && !isCommentOnly)
        return;

    const QString interpretedExpr = m_evaluator->interpretedExpression();
    HistoryEntry historyEntry(enteredExpr, result, interpretedExpr, evalContext);
    historyEntry.setRenderedLines(renderedLinesForHistoryEntry(historyEntry, m_settings));
    m_session->addHistoryEntry(historyEntry);
    if (m_settings->historySaving == Settings::HistorySavingContinuously)
        saveSessionToDefaultPath();
    const bool userVariableAssign = m_evaluator->isUserVariableAssign();
    emit historyChanged();
    if (!startedFromHistoryEdit)
        m_widgets.display->verticalScrollBar()->setValue(m_widgets.display->verticalScrollBar()->maximum());
    if (userVariableAssign)
        emit variablesChanged();
    if (m_evaluator->isUserUnitAssign())
        emit unitsChanged();

    if (m_settings->bitfieldVisible)
        m_widgets.bitField->updateBits(result);

    if (m_settings->autoResultToClipboard)
        copyResultToClipboard();

    if (m_settings->leaveLastExpression)
        m_widgets.editor->selectAll();
    else
        m_widgets.editor->clear();

    m_widgets.editor->stopAutoCalc();
    m_widgets.editor->stopAutoComplete();
    if (!result.isNan())
        m_conditions.autoAns = true;
}

void MainWindow::startHistoryEntryEdit(int index)
{
    const int historySize = m_session->historySize();
    if (index < 0 || index >= historySize)
        return;

    m_pendingHistoryEditIndex = index;
    m_widgets.display->setEditingHistoryIndex(index);
    m_widgets.editor->setHistoryArrowNavigationEnabled(false);
    m_widgets.editor->setText(m_session->historyEntryAt(index).expr());
    m_widgets.editor->setFocus();
    m_widgets.editor->setCursorPosition(m_widgets.editor->text().size());
    showStateLabel(tr("Editing calculation. Press Esc twice to cancel."));
}

void MainWindow::editHistoryEntryContext(int index)
{
    const int historySize = m_session->historySize();
    if (index < 0 || index >= historySize)
        return;

    HistoryEntry entry = m_session->historyEntryAt(index);
    EvaluationContext ctx = entry.context();

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Calculation Settings"));
    QVBoxLayout* root = new QVBoxLayout(&dialog);
    QGroupBox* commonGroup = new QGroupBox(tr("Common Settings"), &dialog);
    QFormLayout* globalForm = new QFormLayout(commonGroup);
    root->addWidget(commonGroup);

    auto addNotationItems = [](QComboBox* combo) {
        combo->addItem(QObject::tr("Automatic decimal"), QStringLiteral("g"));
        combo->addItem(QObject::tr("Fixed-point decimal"), QStringLiteral("f"));
        combo->addItem(QObject::tr("Engineering decimal"), QStringLiteral("n"));
        combo->addItem(QObject::tr("Scientific decimal"), QStringLiteral("e"));
        combo->addItem(QObject::tr("Rational"), QStringLiteral("r"));
        combo->addItem(QObject::tr("Binary"), QStringLiteral("b"));
        combo->addItem(QObject::tr("Octal"), QStringLiteral("o"));
        combo->addItem(QObject::tr("Hexadecimal"), QStringLiteral("h"));
        combo->addItem(QObject::tr("Sexagesimal"), QStringLiteral("s"));
    };

    auto addComplexFormItems = [](QComboBox* combo) {
        combo->addItem(QObject::tr("Rectangular (Cartesian)"), QStringLiteral("c"));
        combo->addItem(QObject::tr("Polar (Exponential)"), QStringLiteral("p"));
        combo->addItem(QObject::tr("Polar (Angle)"), QStringLiteral("a"));
    };

    QComboBox* angle = new QComboBox(&dialog);
    angle->addItem(tr("Radian"), QStringLiteral("r"));
    angle->addItem(tr("Degree"), QStringLiteral("d"));
    angle->addItem(tr("Gradian"), QStringLiteral("g"));
    angle->addItem(tr("Turn"), QStringLiteral("t"));
    angle->addItem(tr("Revolution"), QStringLiteral("v"));
    angle->setCurrentIndex(qMax(0, angle->findData(QString(QChar(ctx.angle)))));
    globalForm->addRow(tr("Angle mode:"), angle);

    QComboBox* unitExp = new QComboBox(&dialog);
    unitExp->addItem(tr("Superscript"), QStringLiteral("s"));
    unitExp->addItem(tr("Fraction"), QStringLiteral("f"));
    unitExp->setCurrentIndex((ctx.unitExp == 'f') ? 1 : 0);
    globalForm->addRow(tr("Unit exponent style:"), unitExp);

    QComboBox* round = new QComboBox(&dialog);
    round->addItem(tr("Half away from zero"), QStringLiteral("a"));
    round->addItem(tr("Half to even"), QStringLiteral("e"));
    round->addItem(tr("Toward zero"), QStringLiteral("z"));
    round->addItem(tr("Toward +infinity"), QStringLiteral("p"));
    round->addItem(tr("Toward -infinity"), QStringLiteral("m"));
    round->setCurrentIndex(qMax(0, round->findData(QString(QChar(ctx.round)))));
    globalForm->addRow(tr("Rounding mode:"), round);

    QComboBox* imagUnit = new QComboBox(&dialog);
    imagUnit->addItem(QStringLiteral("i"), QStringLiteral("i"));
    imagUnit->addItem(QStringLiteral("j"), QStringLiteral("j"));
    imagUnit->setCurrentIndex(ctx.unit == 'j' ? 1 : 0);
    globalForm->addRow(tr("Complex unit:"), imagUnit);

    QCheckBox* complexOn = new QCheckBox(tr("Enable complex numbers"), &dialog);
    complexOn->setChecked(ctx.complexOn);
    globalForm->addRow(QString(), complexOn);

    struct LineUiState {
        bool enabled = true;
        char fmt = 'g';
        int prec = -1;
        char cplx = 'c';
    };
    std::array<LineUiState, 5> lines;
    lines[0].enabled = true;
    lines[0].fmt = ctx.main.fmt;
    lines[0].prec = ctx.main.prec;
    lines[0].cplx = ctx.main.cplx;
    for (int i = 1; i < 5; ++i) {
        lines[i].enabled = (i - 1) < ctx.extras.size();
        if (lines[i].enabled) {
            lines[i].fmt = ctx.extras.at(i - 1).fmt;
            lines[i].prec = ctx.extras.at(i - 1).prec;
            lines[i].cplx = ctx.extras.at(i - 1).cplx;
        }
    }

    QComboBox* lineSelector = new QComboBox(&dialog);
    lineSelector->addItem(tr("Main Line"));
    lineSelector->addItem(tr("Extra Line #1"));
    lineSelector->addItem(tr("Extra Line #2"));
    lineSelector->addItem(tr("Extra Line #3"));
    lineSelector->addItem(tr("Extra Line #4"));

    QGroupBox* selectorGroup = new QGroupBox(tr("Result Line"), &dialog);
    QFormLayout* selectorForm = new QFormLayout(selectorGroup);
    selectorForm->addRow(tr("Configure:"), lineSelector);
    root->addWidget(selectorGroup);

    QGroupBox* lineGroup = new QGroupBox(tr("Result Configuration"), &dialog);
    QFormLayout* lineForm = new QFormLayout(lineGroup);
    QCheckBox* lineEnabled = new QCheckBox(tr("Enable this result line"), lineGroup);
    QComboBox* lineFmt = new QComboBox(lineGroup);
    addNotationItems(lineFmt);
    QCheckBox* lineAutoPrecision = new QCheckBox(tr("Automatic"), lineGroup);
    QSpinBox* linePrecision = new QSpinBox(lineGroup);
    linePrecision->setRange(0, 50);
    QComboBox* lineCplx = new QComboBox(lineGroup);
    addComplexFormItems(lineCplx);

    QWidget* precisionRow = new QWidget(lineGroup);
    QHBoxLayout* precisionLayout = new QHBoxLayout(precisionRow);
    precisionLayout->setContentsMargins(0, 0, 0, 0);
    precisionLayout->addWidget(lineAutoPrecision);
    precisionLayout->addWidget(linePrecision);
    lineForm->addRow(QString(), lineEnabled);
    lineForm->addRow(tr("Notation:"), lineFmt);
    lineForm->addRow(tr("Decimal places:"), precisionRow);
    lineForm->addRow(tr("Complex form:"), lineCplx);
    root->addWidget(lineGroup);

    int activeLineIndex = 0;
    auto loadLineUi = [&](int i) {
        const LineUiState& st = lines.at(i);
        const bool isMain = (i == 0);
        const bool allow = isMain || st.enabled;

        lineEnabled->blockSignals(true);
        lineFmt->blockSignals(true);
        lineAutoPrecision->blockSignals(true);
        linePrecision->blockSignals(true);
        lineCplx->blockSignals(true);

        lineEnabled->setVisible(!isMain);
        lineEnabled->setChecked(isMain ? true : st.enabled);
        lineFmt->setCurrentIndex(qMax(0, lineFmt->findData(QString(QChar(st.fmt)))));
        lineAutoPrecision->setChecked(st.prec < 0);
        linePrecision->setValue(st.prec < 0 ? 8 : st.prec);
        lineFmt->setEnabled(allow);
        lineAutoPrecision->setEnabled(allow);
        linePrecision->setEnabled(allow && st.prec >= 0);
        lineCplx->setCurrentIndex(qMax(0, lineCplx->findData(QString(QChar(st.cplx)))));
        lineCplx->setEnabled(allow && complexOn->isChecked());

        lineEnabled->blockSignals(false);
        lineFmt->blockSignals(false);
        lineAutoPrecision->blockSignals(false);
        linePrecision->blockSignals(false);
        lineCplx->blockSignals(false);
    };

    auto saveLineUi = [&](int i) {
        LineUiState& st = lines[i];
        st.enabled = (i == 0) ? true : lineEnabled->isChecked();
        st.fmt = lineFmt->currentData().toString().at(0).toLatin1();
        st.prec = lineAutoPrecision->isChecked() ? -1 : linePrecision->value();
        st.cplx = lineCplx->currentData().toString().at(0).toLatin1();
    };

    connect(lineAutoPrecision, &QCheckBox::toggled, linePrecision, [linePrecision](bool checked) {
        linePrecision->setEnabled(!checked);
    });
    connect(lineSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, [&](int newIndex) {
        saveLineUi(activeLineIndex);
        activeLineIndex = qBound(0, newIndex, 4);
        loadLineUi(activeLineIndex);
    });
    connect(lineEnabled, &QCheckBox::toggled, &dialog, [&](bool enabled) {
        const bool allow = (activeLineIndex == 0) || enabled;
        lineFmt->setEnabled(allow);
        lineAutoPrecision->setEnabled(allow);
        linePrecision->setEnabled(allow && !lineAutoPrecision->isChecked());
        lineCplx->setEnabled(allow && complexOn->isChecked());
    });
    connect(complexOn, &QCheckBox::toggled, &dialog, [&](bool enabled) {
        imagUnit->setEnabled(enabled);
        const bool allow = (activeLineIndex == 0) || lineEnabled->isChecked();
        lineCplx->setEnabled(allow && enabled);
    });
    lineSelector->setCurrentIndex(0);
    activeLineIndex = 0;
    loadLineUi(0);
    imagUnit->setEnabled(complexOn->isChecked());

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    saveLineUi(activeLineIndex);
    ctx.main.fmt = lines[0].fmt;
    ctx.main.prec = lines[0].prec;
    ctx.main.cplx = lines[0].cplx;
    ctx.complexOn = complexOn->isChecked();
    ctx.unit = imagUnit->currentData().toString().at(0).toLatin1();
    ctx.angle = angle->currentData().toString().at(0).toLatin1();
    ctx.unitExp = unitExp->currentData().toString().at(0).toLatin1();
    ctx.round = round->currentData().toString().at(0).toLatin1();
    ctx.extras.clear();
    for (int i = 1; i < 5; ++i) {
        if (!lines[i].enabled)
            continue;
        ResultLineContext line;
        line.fmt = lines[i].fmt;
        line.prec = lines[i].prec;
        line.cplx = lines[i].cplx;
        ctx.extras.append(line);
    }

    const int previousDisplayScrollValue = m_widgets.display->verticalScrollBar()->value();
    const QList<HistoryEntry> previousEntries = historyEntries();
    QList<HistoryEntry> updatedEntries = previousEntries;
    HistoryEntry updatedEntry = updatedEntries.at(index);
    updatedEntry.setContext(ctx);
    updatedEntries[index] = updatedEntry;

    int errorIndex = -1;
    QString errorText;
    if (!rebuildSessionFromEntries(updatedEntries, index, &errorIndex, &errorText)) {
        QScrollBar* bar = m_widgets.display->verticalScrollBar();
        bar->setValue(qBound(bar->minimum(), previousDisplayScrollValue, bar->maximum()));
        showStateLabel(tr("Could not recalculate from calculation %1: %2").arg(errorIndex + 1).arg(errorText));
        return;
    }

    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();
    emit unitsChanged();
    QScrollBar* bar = m_widgets.display->verticalScrollBar();
    const int clamped = qBound(bar->minimum(), previousDisplayScrollValue, bar->maximum());
    bar->setValue(clamped);
    QTimer::singleShot(0, this, [this, previousDisplayScrollValue]() {
        QScrollBar* deferredBar = m_widgets.display->verticalScrollBar();
        const int deferredClamped = qBound(deferredBar->minimum(),
                                           previousDisplayScrollValue,
                                           deferredBar->maximum());
        deferredBar->setValue(deferredClamped);
    });
    if (m_settings->historySaving == Settings::HistorySavingContinuously)
        saveSessionToDefaultPath();
}

void MainWindow::cancelHistoryEntryEdit()
{
    if (m_pendingHistoryEditIndex < 0)
        return;

    const int previousDisplayScrollValue = m_widgets.display->verticalScrollBar()->value();
    m_pendingHistoryEditIndex = -1;
    m_widgets.display->setEditingHistoryIndex(-1);
    m_widgets.editor->setHistoryArrowNavigationEnabled(true);
    m_widgets.display->verticalScrollBar()->setValue(previousDisplayScrollValue);
    m_widgets.editor->clear();
    showReadyMessage();
}

QList<HistoryEntry> MainWindow::historyEntries() const
{
    QList<HistoryEntry> entries;
    const int historySize = m_session->historySize();
    entries.reserve(historySize);
    for (int i = 0; i < historySize; ++i)
        entries.append(m_session->historyEntryAtRef(i));
    return entries;
}

bool MainWindow::rebuildSessionFromEntries(const QList<HistoryEntry>& entries,
                                           int startIndex,
                                           int* errorIndex,
                                           QString* errorText)
{
    if (errorIndex)
        *errorIndex = -1;
    if (errorText)
        *errorText = QString();

    if (startIndex < 0 || startIndex > entries.size()) {
        if (errorText)
            *errorText = tr("Invalid recalculation start index");
        return false;
    }

    const Session previousSessionState = *m_session;
    const bool previousAutoAns = m_conditions.autoAns;

    if (startIndex == 0) {
        bool hasBaselineAns = false;
        Quantity baselineAnsValue = CMath::nan();
        const bool hadPreviousAns = m_evaluator->hasVariable(QStringLiteral("ans"));
        if (hadPreviousAns) {
            const Variable previousAnsVariable = m_evaluator->getVariable(QStringLiteral("ans"));
            baselineAnsValue = previousAnsVariable.value();
            hasBaselineAns = !baselineAnsValue.isNan();
        }
        if (!hasBaselineAns) {
            for (int i = previousSessionState.historySize() - 1; i >= 0; --i) {
                const Quantity candidate = previousSessionState.historyEntryAtRef(i).result();
                if (!candidate.isNan()) {
                    baselineAnsValue = candidate;
                    hasBaselineAns = true;
                    break;
                }
            }
        }
        m_session->clearHistory();
        m_session->clearVariables();
        m_session->clearUserFunctions();
        m_evaluator->initializeBuiltInVariables();
        if (hasBaselineAns) {
            m_evaluator->setVariable(
                QStringLiteral("ans"),
                baselineAnsValue,
                Variable::BuiltIn);
        }
        m_conditions.autoAns = false;
    } else {
        while (m_session->historySize() > startIndex)
            m_session->removeHistoryEntryAt(m_session->historySize() - 1);
    }

    const EvaluationContext originalContext = currentEvaluationContext(m_settings);

    for (int i = startIndex; i < entries.size(); ++i) {
        const HistoryEntry entry = entries.at(i);
        const QString currentExpr = entry.expr();
        applyEvaluationContext(m_settings, entry.contextRef());
        const QString evalExpr = m_evaluator->autoFix(currentExpr);
        const bool isCommentOnly = Evaluator::isCommentOnlyExpression(evalExpr);

        m_evaluator->setExpression(evalExpr);
        Quantity result = m_evaluator->evalUpdateAns();
        if (!m_evaluator->error().isEmpty()) {
            if (errorIndex)
                *errorIndex = i;
            if (errorText)
                *errorText = m_evaluator->error();
            applyEvaluationContext(m_settings, originalContext);
            *m_session = previousSessionState;
            m_conditions.autoAns = previousAutoAns;
            return false;
        }

        if (m_evaluator->isUserFunctionAssign())
            result = CMath::nan();
        else if (result.isNan() && !isCommentOnly)
            continue;

        const QString interpretedExpr = m_evaluator->interpretedExpression();
        HistoryEntry rebuiltEntry(currentExpr, result, interpretedExpr, entry.contextRef());
        rebuiltEntry.setRenderedLines(renderedLinesForHistoryEntry(rebuiltEntry, m_settings));
        m_session->addHistoryEntry(rebuiltEntry);
    }

    applyEvaluationContext(m_settings, originalContext);
    const bool hasAns = m_evaluator->hasVariable(QStringLiteral("ans"));
    m_conditions.autoAns = hasAns && !m_evaluator->getVariable(QStringLiteral("ans")).value().isNan();
    return true;
}

void MainWindow::removeHistoryEntryAt(int index)
{
    const int historySize = m_session->historySize();
    if (index < 0 || index >= historySize)
        return;

    m_session->removeHistoryEntryAt(index);
    if (m_pendingHistoryEditIndex == index)
        m_pendingHistoryEditIndex = -1;
    else if (m_pendingHistoryEditIndex > index)
        --m_pendingHistoryEditIndex;
    m_widgets.display->setEditingHistoryIndex(m_pendingHistoryEditIndex);
    m_widgets.editor->setHistoryArrowNavigationEnabled(m_pendingHistoryEditIndex < 0);
    m_conditions.autoAns = !m_session->historyIsEmpty();
    emit historyChanged();
}

void MainWindow::removeHistoryEntriesAbove(int index)
{
    const int historySize = m_session->historySize();
    if (historySize == 0 || index <= 0 || index >= historySize)
        return;

    for (int i = 0; i < index; ++i)
        m_session->removeHistoryEntryAt(0);

    if (m_pendingHistoryEditIndex >= 0) {
        if (m_pendingHistoryEditIndex < index)
            m_pendingHistoryEditIndex = -1;
        else
            m_pendingHistoryEditIndex -= index;
    }
    m_widgets.display->setEditingHistoryIndex(m_pendingHistoryEditIndex);
    m_widgets.editor->setHistoryArrowNavigationEnabled(m_pendingHistoryEditIndex < 0);
    m_conditions.autoAns = !m_session->historyIsEmpty();
    emit historyChanged();
}

void MainWindow::removeHistoryEntriesBelow(int index)
{
    const int historySize = m_session->historySize();
    if (historySize == 0 || index < 0 || index >= historySize - 1)
        return;

    for (int i = historySize - 1; i > index; --i)
        m_session->removeHistoryEntryAt(i);

    if (m_pendingHistoryEditIndex > index)
        m_pendingHistoryEditIndex = -1;
    m_widgets.display->setEditingHistoryIndex(m_pendingHistoryEditIndex);
    m_widgets.editor->setHistoryArrowNavigationEnabled(m_pendingHistoryEditIndex < 0);
    m_conditions.autoAns = !m_session->historyIsEmpty();
    emit historyChanged();
}

void MainWindow::clearTextEditSelection(QPlainTextEdit* edit)
{
    QTextCursor cursor = edit->textCursor();
    if (cursor.hasSelection()) {
        cursor.clearSelection();
        edit->setTextCursor(cursor);
    }
}

void MainWindow::handleManualClosed()
{
    disconnect(m_widgets.manual);
    m_settings->manualWindowGeometry = m_settings->windowPositionSave ? m_widgets.manual->saveGeometry() : QByteArray();
    m_widgets.manual->deleteLater();
    m_widgets.manual = 0;
}

void MainWindow::handleDisplaySelectionChange()
{
    clearTextEditSelection(m_widgets.editor);
    const QTextCursor displayCursor = m_widgets.display->textCursor();
    if (displayCursor.hasSelection()) {
        const QString rawSelected = displayCursor.selectedText();
        if (rawSelected.contains(RegExpPatterns::lineBreak())) {
            m_widgets.editor->autoCalcSelection(rawSelected);
            return;
        }

        const QString selected = normalizedDisplaySelectionForEvaluation(rawSelected);
        m_widgets.editor->autoCalcSelection(selected);
        return;
    }

    hideStateLabel();
}

void MainWindow::handleEditorSelectionChange()
{
    clearTextEditSelection(m_widgets.display);
    if (m_widgets.editor->textCursor().hasSelection())
        return;

    if (m_widgets.editor->text().trimmed().isEmpty()) {
        hideStateLabel();
        return;
    }

    m_widgets.editor->refreshAutoCalc();
}

void MainWindow::handleCopyAvailable(bool copyAvailable)
{
    if (!copyAvailable)
        return;
    QPlainTextEdit* const textEdit = static_cast<QPlainTextEdit*>(sender());
    if (textEdit)
        m_copyWidget = textEdit;
}

void MainWindow::handleBitsChanged(const QString& str)
{
    Quantity num(CNumber(str.toLatin1().data()));
    auto result = DMath::format(num, Quantity::Format::Fixed() + Quantity::Format::Hexadecimal());
    insertTextIntoEditor(result);
    showStateLabel(QString("Current value: %1").arg(NumberFormatter::format(num)));

    auto cursor = m_widgets.editor->textCursor();
    if (cursor.hasSelection())
        cursor.removeSelectedText();
    cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, result.length());
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, result.length());
    m_widgets.editor->setTextCursor(cursor);
}

void MainWindow::handleEditorTextChange()
{
    m_widgets.display->clearHoverFeedback();
    clearTextEditSelection(m_widgets.display);
    if (m_widgets.editor->text().trimmed().isEmpty()) {
        hideStateLabel();
        return;
    }

    if (m_conditions.autoAns && m_settings->autoAns) {
        QString expr = m_evaluator->autoFix(m_widgets.editor->text());
        if (expr.isEmpty())
            return;

        Tokens tokens = m_evaluator->scan(expr);
        if (tokens.count() == 1) {
            const auto mode = EditorUtils::autoAnsRewriteModeForLeadingOperator(tokens.at(0).text());
            if (mode != EditorUtils::AutoAnsNoRewrite) {
                m_conditions.autoAns = false;
                expr = EditorUtils::applyAutoAnsRewrite(expr, mode);
                m_widgets.editor->setText(expr);
                m_widgets.editor->setCursorPosition(expr.length());
            }
        }
    }
}

void MainWindow::handleDockWidgetVisibilityChanged(bool visible)
{
    QDockWidget* dock = qobject_cast<QDockWidget*>(sender());
    if (!dock)
        return;

    // Pass the focus back to the editor if the dock that is being hidden has the focus.
    QWidget* focusWidget = dock->focusWidget();
    if (focusWidget && !visible && focusWidget->hasFocus())
        m_widgets.editor->setFocus();
}

void MainWindow::insertVariableIntoEditor(const QString& v)
{
    insertTextIntoEditor(v);
}

void MainWindow::insertUserFunctionIntoEditor(const QString& v)
{
    insertTextIntoEditor(v);
}

void MainWindow::insertUserUnitIntoEditor(const QString& v)
{
    insertTextIntoEditor(v);
}

void MainWindow::setRadixCharacterAutomatic()
{
    setRadixCharacter(0);
}

void MainWindow::setRadixCharacterDot()
{
    setRadixCharacter(MathDsl::DotSep.toLatin1());
}

void MainWindow::setRadixCharacterComma()
{
    setRadixCharacter(MathDsl::CommaSep.toLatin1());
}

void MainWindow::setRadixCharacterBoth()
{
    setRadixCharacter(MathDsl::MulOpAl1.toLatin1());
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    persistSessionAndSettingsForShutdown();
    e->accept();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    updateStatusBarSectionVisibility();

    if (m_widgets.state->isVisible())
        showStateLabel(m_widgets.state->text());
}

void MainWindow::persistSessionAndSettingsForShutdown()
{
    if (m_shutdownStateSaved)
        return;

    m_shutdownStateSaved = true;
    if (m_widgets.manual) {
        m_widgets.manual->close();
    }
    saveSettings();
    saveSessionToDefaultPath(m_settings->historySaving != Settings::HistorySavingNever);
}

void MainWindow::saveSessionToDefaultPath(bool saveHistory)
{
    QString data_path = Settings::getDataPath();
    QDir qdir;
    qdir.mkpath(data_path);
    data_path.append("/history.json");
    saveSession(data_path, saveHistory);
}

void MainWindow::setResultPrecision(int p)
{
    if (m_settings->resultPrecision == p)
        return;

    m_settings->resultPrecision = p;
    setStatusBarText();
    emit resultPrecisionChanged();
}

void MainWindow::setResultFormat(char c)
{
    if (m_settings->resultFormat == c)
        return;

    m_settings->resultFormat = c;
    emit resultFormatChanged();
}

void MainWindow::setUnitNegativeExponentStyle(QAction* action)
{
    const Settings::UnitNegativeExponentStyle style =
        static_cast<Settings::UnitNegativeExponentStyle>(action->data().toInt());
    if (style != Settings::UnitNegativeExponentSuperscript
            && style != Settings::UnitNegativeExponentFraction) {
        return;
    }
    if (m_settings->unitNegativeExponentStyle == style)
        return;

    m_settings->unitNegativeExponentStyle = style;
    setRuntimeUnitNegativeExponentStyle(style);
    // Unit exponent style changes should affect only future evaluations and
    // live editor previews, not previously displayed history entries.
    m_widgets.editor->refreshAutoCalc();
}

void MainWindow::setResultRoundingMode(QAction* action)
{
    const Settings::ResultRoundingMode mode =
        static_cast<Settings::ResultRoundingMode>(action->data().toInt());
    if (mode != Settings::ResultRoundingHalfAwayFromZero
            && mode != Settings::ResultRoundingHalfEven
            && mode != Settings::ResultRoundingTowardZero
            && mode != Settings::ResultRoundingTowardPositiveInfinity
            && mode != Settings::ResultRoundingTowardNegativeInfinity) {
        return;
    }
    if (m_settings->resultRoundingMode == mode)
        return;

    m_settings->resultRoundingMode = mode;
    setRuntimeResultRoundingMode(mode);
    emit resultRoundingModeChanged();
}

void MainWindow::setRadixCharacter(char c)
{
    m_settings->setRadixCharacter(c);
    emit radixCharacterChanged();
}

void MainWindow::showNumberFormatDialog()
{
    NumberFormatDialog dialog(this);
    dialog.setSelection(m_settings->numberFormatStyle);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const Settings::NumberFormatStyle selectedStyle = dialog.selectedStyle();
    if (m_settings->numberFormatStyle == selectedStyle)
        return;

    m_settings->numberFormatStyle = selectedStyle;
    m_settings->applyNumberFormatStyle();
    emit syntaxHighlightingChanged();
    // Number format changes from this dialog should not rewrite previous
    // history entries in Result Display. Only refresh live editor previews.
    m_widgets.editor->refreshAutoCalc();
}

void MainWindow::showResultSlotsDialog()
{
    ResultSlotsDialog dialog(this);
    connect(&dialog, &ResultSlotsDialog::settingsApplied, this, [this]() {
        DMath::complexMode = m_settings->complexNumbers;
        if (m_settings->complexNumbers)
            m_evaluator->initializeBuiltInVariables();
        setStatusBarText();
        // Notation/precision changes from this dialog must not rewrite
        // previously displayed history lines. Only refresh the active editor
        // previews (live result and selection tooltip).
        m_widgets.editor->refreshAutoCalc();
    });
    dialog.exec();
}

void MainWindow::increaseDisplayFontPointSize()
{
    m_widgets.display->increaseFontPointSize();
    m_widgets.editor->increaseFontPointSize();
    if (m_widgets.state->isVisible())
        showStateLabel(m_widgets.state->text());
}

void MainWindow::decreaseDisplayFontPointSize()
{
    m_widgets.display->decreaseFontPointSize();
    m_widgets.editor->decreaseFontPointSize();
    if (m_widgets.state->isVisible())
        showStateLabel(m_widgets.state->text());
}

void MainWindow::showLanguageChooserDialog()
{
    QMap<QString, QString> map;

    // List all available translations from the resource files
    QDir localeDir(":/locale/", "*.qm");
    QFileInfoList localeList = localeDir.entryInfoList();
    for (int i = 0; i < localeList.size(); ++i) {
        QFileInfo fileInfo = localeList.at(i);
        QString localeName = fileInfo.baseName();
        QString langName = QLocale(localeName).nativeLanguageName();

        // Kludges for region-specific translations
        if(localeName == "es") langName = QString::fromUtf8("Español (Latinoamérica)");
        if(localeName == "es_ES") langName = QString::fromUtf8("Español (España)");
        if(localeName == "pt_BR") langName = QString::fromUtf8("Português (Brasil)");
        if(localeName == "pt_PT") langName = QString::fromUtf8("Português (Portugal)");

        // The first letter is not always capitalized so force it
        langName[0] = langName[0].toUpper();
        map.insert(langName, localeName);
    }

    const auto values = map.values();
    int current = values.indexOf(m_settings->language) + 1;

    QString defaultKey = tr("System Default");
    QStringList keys(QStringList() << defaultKey << map.keys());

    bool ok;
    QString langName = QInputDialog::getItem(this, tr("Language"), tr("Select the language:"),
        keys, current, false, &ok);
    if (ok && !langName.isEmpty()) {
        QString value = (langName == defaultKey) ? QLatin1String("C") : map.value(langName);
        if (m_settings->language != value) {
            m_settings->language = value;
            emit languageChanged();
        }
    }
}

void MainWindow::showResultFormatContextMenu(const QPoint& point)
{
    m_menus.resultFormat->popup(m_status.resultFormat->mapToGlobal(point));
}

void MainWindow::showPrecisionContextMenu(const QPoint& point)
{
    QMenu menu(this);
    QActionGroup modeGroup(&menu);
    modeGroup.setExclusive(true);

    QAction* automaticAction = menu.addAction(MainWindow::tr("Automatic"));
    automaticAction->setCheckable(true);
    automaticAction->setChecked(m_settings->resultPrecision < 0);
    modeGroup.addAction(automaticAction);
    connect(automaticAction, &QAction::triggered, this, [this]() {
        setResultPrecision(-1);
    });

    QAction* customAction = menu.addAction(MainWindow::tr("Custom"));
    customAction->setCheckable(true);
    customAction->setChecked(m_settings->resultPrecision >= 0);
    modeGroup.addAction(customAction);

    menu.addSeparator();

    QWidget* precisionEditor = new QWidget(&menu);
    QHBoxLayout* precisionLayout = new QHBoxLayout(precisionEditor);
    precisionLayout->setContentsMargins(8, 4, 8, 4);
    precisionLayout->setSpacing(6);

    QLabel* precisionLabel = new QLabel(MainWindow::tr("Decimal places:"), precisionEditor);
    QSpinBox* precisionSpin = new QSpinBox(precisionEditor);
    precisionSpin->setRange(0, 50);
    precisionSpin->setValue(m_settings->resultPrecision < 0 ? 8 : m_settings->resultPrecision);
    precisionSpin->setEnabled(m_settings->resultPrecision >= 0);

    connect(precisionSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        setResultPrecision(value);
    });
    connect(precisionSpin, QOverload<int>::of(&QSpinBox::valueChanged), customAction, [customAction](int) {
        customAction->setChecked(true);
    });
    connect(customAction, &QAction::triggered, this, [this, precisionSpin]() {
        setResultPrecision(precisionSpin->value());
    });
    connect(automaticAction, &QAction::toggled, precisionSpin, [precisionSpin](bool automatic) {
        precisionSpin->setEnabled(!automatic);
    });

    precisionLayout->addWidget(precisionLabel);
    precisionLayout->addWidget(precisionSpin);

    QWidgetAction* editorAction = new QWidgetAction(&menu);
    editorAction->setDefaultWidget(precisionEditor);
    menu.addAction(editorAction);

    menu.exec(m_status.resultPrecision->mapToGlobal(point));
}

void MainWindow::showComplexFormContextMenu(const QPoint& point)
{
    m_menus.complexForm->popup(m_status.complexForm->mapToGlobal(point));
}

void MainWindow::showKeypadContextMenu(const QPoint& point)
{
    if (!m_widgets.keypad)
        return;
    m_menus.keypad->popup(m_widgets.keypad->mapToGlobal(point));
}
