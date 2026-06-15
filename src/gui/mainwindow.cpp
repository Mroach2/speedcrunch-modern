// SPDX-FileCopyrightText: 2007-2020, 2022, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/mainwindow.h"

#include "core/complexform.h"
#include "core/constants.h"
#include "core/evaluator.h"
#include "core/functions.h"
#include "core/numberformatter.h"
#include "core/regexpatterns.h"
#include "core/settings.h"
#include "core/session.h"
#include "core/sessionjsonkeys.h"
#include "core/userdefinitions.h"
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
#include "gui/dockcomboboxchevron.h"
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
#include "gui/oklchutils.h"
#include "gui/splittertreeutils.h"
#include "core/manualserver.h"
#include "gui/resultdisplay.h"
#include "gui/resultlineformatutils.h"
#include "gui/syntaxhighlighter.h"
#include "gui/themedlineedit.h"
#include "gui/uiconfig.h"
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
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QComboBox>
#include <QColorDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFont>
#include <QFontDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMetaObject>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMimeData>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPointer>
#include <QProxyStyle>
#include <QDropEvent>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QSplitterHandle>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QScrollBar>
#include <QSet>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QToolButton>
#include <QToolTip>
#include <QTreeWidget>
#include <QThread>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#ifdef Q_OS_WIN32
#include "windows.h"
#include <shlobj.h>
#endif // Q_OS_WIN32

namespace {
constexpr const char* kFeedbackUrl = "https://www.speedcrunch.org/issues.html";
constexpr const char* kCommunityUrl = "https://groups.google.com/group/speedcrunch/";
constexpr const char* kFacebookGroupUrl = "https://www.facebook.com/groups/1783793218546797";
constexpr const char* kNewsUrl = "http://speedcrunch.blogspot.com/";
constexpr const char* kSourceUrl = "https://www.speedcrunch.org/source.html";
constexpr const char* kDonateUrl = "https://www.speedcrunch.org/donate.html";

QString sessionsPath()
{
    return QDir(Settings::getDataPath()).filePath(QStringLiteral("sessions"));
}

bool ensureSessionsPath()
{
    QDir dir;
    return dir.mkpath(sessionsPath());
}

bool directoryIsEmpty(const QString& path)
{
    const QDir dir(path);
    return !dir.exists()
        || dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
}

QString normalizedSessionName(QString name)
{
    name = name.trimmed();
    return name.isEmpty()
        ? QLatin1String(SessionJsonKeys::SessionValueMain)
        : name;
}

bool hasCurrentSessionSchema(const QByteArray& data)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return false;

    const QJsonValue schema = doc.object().value(QLatin1String(SessionJsonKeys::SchemaVersion));
    return schema.isDouble() && schema.toInt() == SessionJsonKeys::SchemaVersionValue;
}

bool readValidSessionJson(const QString& filePath, QJsonObject* json)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    const QJsonObject object = doc.object();
    const QJsonValue schema = object.value(QLatin1String(SessionJsonKeys::SchemaVersion));
    if (!schema.isDouble() || schema.toInt() != SessionJsonKeys::SchemaVersionValue)
        return false;

    const QString name = object.value(QLatin1String(SessionJsonKeys::Session)).toString().trimmed();
    if (name.isEmpty())
        return false;

    if (json != nullptr)
        *json = object;
    return true;
}

QString sessionFilePath(const QString& sessionName);
QString firstAvailableUntitledSessionName(const QHash<QString, Session*>& sessions);

void migrateLegacyHistoryIfNeeded()
{
    const QDir dataDir(Settings::getDataPath());
    const QString legacyPath = dataDir.filePath(QStringLiteral("history.json"));
    QFile legacyFile(legacyPath);
    if (!legacyFile.exists())
        return;

    const QString sessionDirPath = sessionsPath();
    const QFileInfo sessionDirInfo(sessionDirPath);
    const bool shouldMigrate = !sessionDirInfo.exists()
        || (sessionDirInfo.isDir() && directoryIsEmpty(sessionDirPath));
    if (!shouldMigrate)
        return;

    if (!legacyFile.open(QIODevice::ReadOnly))
        return;

    const QByteArray data = legacyFile.readAll();
    legacyFile.close();
    if (!hasCurrentSessionSchema(data))
        return;

    if (!ensureSessionsPath())
        return;

    QJsonObject sessionJson = QJsonDocument::fromJson(data).object();
    const QHash<QString, Session*> loadedSessions;
    const QString sessionName = firstAvailableUntitledSessionName(loadedSessions);
    sessionJson[QLatin1String(SessionJsonKeys::Session)] = sessionName;
    const QByteArray migratedData = QJsonDocument(sessionJson).toJson(QJsonDocument::Compact);

    QFile migratedSession(sessionFilePath(sessionName));
    if (!migratedSession.open(QIODevice::WriteOnly))
        return;

    if (migratedSession.write(migratedData) != migratedData.size())
        return;

    migratedSession.close();
    legacyFile.remove();
}

QString sessionFileBaseName(QString sessionName)
{
    sessionName = normalizedSessionName(sessionName);

    QString safeName;
    safeName.reserve(sessionName.size());
    for (const QChar ch : sessionName) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_') || ch == QLatin1Char(' '))
            safeName.append(ch);
        else
            safeName.append(QLatin1Char('_'));
    }

    const QString trimmedSafeName = safeName.trimmed();
    return trimmedSafeName.isEmpty()
        ? QLatin1String(SessionJsonKeys::SessionValueMain)
        : trimmedSafeName;
}

QString sessionFilePath(const QString& sessionName)
{
    ensureSessionsPath();
    return QDir(sessionsPath()).filePath(sessionFileBaseName(sessionName) + QLatin1String(".json"));
}

QMutex& asyncSessionIoMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, quint64>& asyncSessionSaveGenerations()
{
    static QHash<QString, quint64> generations;
    return generations;
}

QList<QThread*>& asyncSessionIoThreads()
{
    static QList<QThread*> threads;
    return threads;
}

struct SessionLoadSpec {
    QString name;
    QString filePath;
    QJsonObject tab;
};

bool g_restoringExtraWindows = false;
bool g_multiWindowSpawnDone = false;
constexpr int DockLayoutStateVersion = 1;

bool asyncSessionSaveIsCurrent(const QString& filePath, quint64 generation)
{
    QMutexLocker locker(&asyncSessionIoMutex());
    return asyncSessionSaveGenerations().value(filePath) == generation;
}

void unregisterAsyncSessionIoThread(QThread* thread)
{
    QMutexLocker locker(&asyncSessionIoMutex());
    asyncSessionIoThreads().removeAll(thread);
}

void writeSessionJsonToFile(const QJsonObject& json, const QString& filePath, quint64 generation = 0)
{
    const QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);

    if (generation != 0 && !asyncSessionSaveIsCurrent(filePath, generation))
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return;
    file.write(data);
    file.close();
}

void saveSessionAsync(const Session& session, const QString& filePath)
{
    ensureSessionsPath();

    QJsonObject json;
    session.serialize(json);

    quint64 generation = 0;
    {
        QMutexLocker locker(&asyncSessionIoMutex());
        generation = asyncSessionSaveGenerations().value(filePath) + 1;
        asyncSessionSaveGenerations().insert(filePath, generation);
    }

    QThread* thread = QThread::create([json, filePath, generation]() {
        if (!asyncSessionSaveIsCurrent(filePath, generation))
            return;
        writeSessionJsonToFile(json, filePath, generation);
    });

    {
        QMutexLocker locker(&asyncSessionIoMutex());
        asyncSessionIoThreads().append(thread);
    }
    QObject::connect(thread, &QThread::finished, thread, [thread]() {
        unregisterAsyncSessionIoThread(thread);
        thread->deleteLater();
    });
    thread->start();
}

void waitForAsyncSessionIo()
{
    while (true) {
        QList<QThread*> threads;
        {
            QMutexLocker locker(&asyncSessionIoMutex());
            threads = asyncSessionIoThreads();
        }
        if (threads.isEmpty())
            return;

        for (QThread* thread : threads) {
            if (thread != nullptr) {
                thread->wait();
                unregisterAsyncSessionIoThread(thread);
            }
        }
    }
}

bool loadedSessionNameExists(const QHash<QString, Session*>& sessions, const QString& name)
{
    for (const QString& existingName : sessions.keys()) {
        if (existingName.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

QString firstAvailableUntitledSessionName(const QHash<QString, Session*>& sessions)
{
    for (int number = 1; number < std::numeric_limits<int>::max(); ++number) {
        const QString name = QStringLiteral("Untitled-%1").arg(number);
        if (!loadedSessionNameExists(sessions, name) && !QFileInfo::exists(sessionFilePath(name)))
            return name;
    }

    return QStringLiteral("Untitled");
}

int untitledSessionNumber(const QString& name)
{
    static const QString prefix = QStringLiteral("Untitled-");
    if (!name.startsWith(prefix, Qt::CaseInsensitive))
        return -1;

    bool ok = false;
    const int number = name.mid(prefix.size()).toInt(&ok);
    return ok && number > 0 ? number : -1;
}

bool isReusableUntitledSession(const Session* session)
{
    if (session == nullptr || !session->historyIsEmpty())
        return false;

    const Evaluator* evaluator = session->evaluator();
    const QList<Variable> variables = session->variablesToList();
    for (const Variable& variable : variables) {
        if (variable.type() != Variable::BuiltIn
            && !(evaluator && evaluator->isGlobalUserVariable(variable.identifier())))
            return false;
    }

    const QList<UserFunction> functions = session->UserFunctionsToList();
    for (const UserFunction& function : functions) {
        if (!(evaluator && evaluator->isGlobalUserFunction(function.name())))
            return false;
    }

    const QList<UserUnit> units = session->userUnitsToList();
    for (const UserUnit& unit : units) {
        if (!(evaluator && evaluator->isGlobalUserUnit(unit.name())))
            return false;
    }

    return true;
}

bool sessionHasPersistableContent(const Session* session)
{
    if (session == nullptr)
        return false;
    if (!session->historyIsEmpty())
        return true;

    const Evaluator* evaluator = session->evaluator();
    const QList<Variable> variables = session->variablesToList();
    for (const Variable& variable : variables) {
        if (variable.type() != Variable::BuiltIn
            && !(evaluator && evaluator->isGlobalUserVariable(variable.identifier()))) {
            return true;
        }
    }

    const QList<UserFunction> functions = session->UserFunctionsToList();
    for (const UserFunction& function : functions) {
        if (!(evaluator && evaluator->isGlobalUserFunction(function.name())))
            return true;
    }

    const QList<UserUnit> units = session->userUnitsToList();
    for (const UserUnit& unit : units) {
        if (!(evaluator && evaluator->isGlobalUserUnit(unit.name())))
            return true;
    }

    return false;
}

bool shouldDeleteSessionFileOnClose(const QString& sessionName, const Session* session)
{
    return untitledSessionNumber(sessionName) > 0 && isReusableUntitledSession(session);
}

QJsonObject editorState(Editor* editor)
{
    QJsonObject state;
    if (editor == nullptr)
        return state;

    state.insert(QStringLiteral("text"), editor->text());
    state.insert(QStringLiteral("cursor"), editor->cursorPosition());
    return state;
}

void restoreEditorState(Editor* editor, const QJsonObject& state, const QString& fallbackText)
{
    if (editor == nullptr)
        return;

    const QString text = state.value(QStringLiteral("text")).toString(fallbackText);
    const int cursor = state.contains(QStringLiteral("cursor"))
        ? state.value(QStringLiteral("cursor")).toInt(text.size())
        : text.size();
    {
        const QSignalBlocker blocker(editor);
        editor->setText(text);
        editor->setCursorPosition(qBound(0, cursor, text.size()));
    }
    if (!text.trimmed().isEmpty())
        editor->refreshAutoCalc();
}

QJsonObject sessionLayoutEntry(const QString& name,
                               const QPair<int, int>& viewportAnchor,
                               int scrollValue,
                               const QJsonObject& editor,
                               bool includeEditor)
{
    QJsonObject entry;
    entry.insert(QStringLiteral("name"), name);
    entry.insert(QStringLiteral("file"), QString(sessionFileBaseName(name) + QLatin1String(".json")));
    if (viewportAnchor.first >= 0 || scrollValue >= 0) {
        QJsonObject scroll;
        if (viewportAnchor.first >= 0) {
            scroll.insert(QStringLiteral("block"), viewportAnchor.first);
            scroll.insert(QStringLiteral("offset"), viewportAnchor.second);
        }
        if (scrollValue >= 0)
            scroll.insert(QStringLiteral("value"), scrollValue);
        entry.insert(QStringLiteral("scroll"), scroll);
    }
    if (includeEditor)
        entry.insert(QStringLiteral("editor"), editor);
    return entry;
}

EvaluationContext currentEvaluationContext(const Settings* settings)
{
    EvaluationContext ctx;
    ctx.main.fmt = settings->resultFormat;
    ctx.main.prec = settings->resultPrecision;
    ctx.main.cplx = settings->resultComplexForm;
    if (settings->multipleResultLinesEnabled) {
        if (settings->secondaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->alternativeResultFormat, settings->secondaryResultPrecision, settings->secondaryResultComplexForm});
        if (settings->tertiaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->tertiaryResultFormat, settings->tertiaryResultPrecision, settings->tertiaryResultComplexForm});
        if (settings->quaternaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->quaternaryResultFormat, settings->quaternaryResultPrecision, settings->quaternaryResultComplexForm});
        if (settings->quinaryResultEnabled)
            ctx.extras.append(ResultLineContext{settings->quinaryResultFormat, settings->quinaryResultPrecision, settings->quinaryResultComplexForm});
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
    settings->resultComplexForm = ctx.main.cplx;

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
            settings->secondaryResultComplexForm = line.cplx;
        } else if (index == 1) {
            settings->tertiaryResultEnabled = true;
            settings->tertiaryResultFormat = line.fmt;
            settings->tertiaryResultPrecision = line.prec;
            settings->tertiaryResultComplexForm = line.cplx;
        } else if (index == 2) {
            settings->quaternaryResultEnabled = true;
            settings->quaternaryResultFormat = line.fmt;
            settings->quaternaryResultPrecision = line.prec;
            settings->quaternaryResultComplexForm = line.cplx;
        } else if (index == 3) {
            settings->quinaryResultEnabled = true;
            settings->quinaryResultFormat = line.fmt;
            settings->quinaryResultPrecision = line.prec;
            settings->quinaryResultComplexForm = line.cplx;
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

QStringList renderedLinesForHistoryEntry(const HistoryEntry& entry, Settings* settings, const Evaluator* evaluator)
{
    const EvaluationContext previousContext = currentEvaluationContext(settings);
    applyEvaluationContext(settings, entry.contextRef());

    QStringList lines;
    lines.append(ResultLineFormatUtils::formattedExpressionLineForDisplay(
        entry.expr(),
        entry.interpretedExpr(),
        evaluator));
    if (!entry.result().isNan()) {
        lines.append(ResultLineFormatUtils::formatResultLinesForDisplay(
            entry.expr(),
            entry.interpretedExpr(),
            entry.result(),
            false,
            true,
            evaluator));
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
    case ColorScheme::Number: return QStringLiteral("number");
    case ColorScheme::Parens: return QStringLiteral("parens");
    case ColorScheme::List: return QStringLiteral("list");
    case ColorScheme::Unit: return QStringLiteral("unit");
    case ColorScheme::Result: return QStringLiteral("result");
    case ColorScheme::Comment: return QStringLiteral("comment");
    case ColorScheme::Function: return QStringLiteral("function");
    case ColorScheme::Operator: return QStringLiteral("operator");
    case ColorScheme::Variable: return QStringLiteral("variable");
    case ColorScheme::Separator: return QStringLiteral("separator");
    case ColorScheme::Background: return QStringLiteral("background");
    }
    return QString();
}

void updateColorButtonStyle(QPushButton* button, const QColor& color)
{
    if (!button || !color.isValid())
        return;

    const QColor textColor = aaForegroundForBackground(color);

    button->setText(color.name());
    button->setMinimumWidth(button->fontMetrics().horizontalAdvance(QStringLiteral("#000000")) + 22);
    button->setStyleSheet(QStringLiteral(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: 1px solid %2;
            border-radius: 3px;
            padding: 2px 8px;
        }
    )").arg(color.name(), textColor.name()));
}

enum class ColorSchemeFilter {
    Dark,
    Light
};

bool colorSchemeMatchesFilter(const ColorScheme& scheme, ColorSchemeFilter filter)
{
    const QColor background = scheme.colorForRole(ColorScheme::Background);
    const ThemePolarity polarity = themePolarityForBackground(background);

    return filter == ColorSchemeFilter::Dark
        ? polarity == ThemePolarity::Dark
        : polarity == ThemePolarity::Light;
}

ColorScheme activeColorScheme(const Settings* settings)
{
    if (settings && settings->colorScheme == QLatin1String("Custom")) {
        const QJsonDocument doc = QJsonDocument::fromJson(settings->customColorSchemeJson.toUtf8());
        const ColorScheme custom(doc);
        if (custom.isValid())
            return custom;
    }

    const ColorScheme named = ColorScheme::loadByName(settings ? settings->colorScheme : QString());
    if (named.isValid())
        return named;
    return ColorScheme::loadByName(QStringLiteral("Terminal"));
}

struct ThemeSurfaceColors
{
    QColor background;
    QColor foreground;
};

struct ThemeScrollBarColors
{
    QColor track;
    QColor thumb;
    QColor hoverThumb;
    QColor pressedThumb;
};

constexpr int kThemeGeneratedShadeCount = UiConfig::Shade600 + 1;
constexpr int kDockListHorizontalPadding = 8;
constexpr int kDockListVerticalPadding = 6;

struct GeneratedThemeSurfaces
{
    QColor base;
    ThemeSurfaceColors primary;
    ThemePolarity polarity;
    QVector<QColor> backgrounds;
    QVector<QColor> foregrounds;
    // 100: outer window chrome, including tab-row empty space and dock padding.
    ThemeSurfaceColors window;
    // 200: result display and active session surface. This is the background
    // role from the selected SpeedCrunch theme.
    ThemeSurfaceColors result;
    // 300: expression editor, dock list/table content, bitfield, and normal
    // keypad buttons. The editor no longer has an independent theme role; it
    // is derived from the result-display background by moving one OKLCH shade
    // step away from the base surface.
    ThemeSurfaceColors editorAndLists;
    // 400: dock title bars, dock/list/table headers, combo popup fill,
    // list/table hovered items, active dock tabs, selected session tabs,
    // popup/input borders, and hovered keypad or bitfield buttons.
    ThemeSurfaceColors headersAndBorders;
    // 500: input controls, search boxes, combo boxes, hovered session tabs,
    // and pressed keypad or bitfield buttons.
    ThemeSurfaceColors inputs;
    // 600: inactive tabs and session tab close-button hover fills.
    ThemeSurfaceColors hoverAndInactiveTabs;
};

GeneratedThemeSurfaces generatedSurfaceColorsForScheme(const ColorScheme& scheme)
{
    const QColor base = scheme.isValid()
        ? scheme.colorForRole(ColorScheme::Background)
        : QApplication::palette().color(QPalette::Base);
    // The generated primary enters the app theme pipeline here, where
    // configured theme colors are resolved into concrete UI roles. It
    // intentionally has no color-scheme role: every theme gets a readable
    // accent that is perceptually derived from its background while unrelated
    // roles continue to come from the scheme or surface generator.
    const QColor generatedPrimary = generatePrimaryFromBackground(base);
    const QColor primary = generatedPrimary.isValid()
        ? generatedPrimary
        : QApplication::palette().color(QPalette::Text);
    const ThemePolarity polarity = themePolarityForBackground(base);
    const QVector<QColor> shades = generateOklchShades(base,
                                                       kThemeGeneratedShadeCount,
                                                       polarity);
    const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(shades);
    const QColor fallbackForeground = aaForegroundForBackground(base);
    const auto surface = [&](int index) {
        return ThemeSurfaceColors {
            shades.value(index, base),
            foregrounds.value(index, fallbackForeground)
        };
    };
    return {
        base,
        ThemeSurfaceColors { primary, aaForegroundForBackground(primary) },
        polarity,
        shades,
        foregrounds,
        surface(UiConfig::WindowBackgroundShade),
        surface(UiConfig::ResultDisplayShade),
        surface(UiConfig::DockBackgroundShade),
        surface(UiConfig::DockHeaderShade),
        surface(UiConfig::DockUnfocusedSelectedItemShade),
        surface(UiConfig::Shade600)
    };
}

GeneratedThemeSurfaces generatedSurfaceColors(const Settings* settings)
{
    return generatedSurfaceColorsForScheme(activeColorScheme(settings));
}

QPalette paletteForThemeSurface(const QPalette& inherited, const ThemeSurfaceColors& surface)
{
    QPalette palette = inherited;
    palette.setColor(QPalette::Window, surface.background);
    palette.setColor(QPalette::WindowText, surface.foreground);
    palette.setColor(QPalette::Base, surface.background);
    palette.setColor(QPalette::Text, surface.foreground);
    palette.setColor(QPalette::Button, surface.background);
    palette.setColor(QPalette::ButtonText, surface.foreground);
    return palette;
}

ThemeSurfaceColors themeSurfaceForShadeIndex(const GeneratedThemeSurfaces& surfaces, int shadeIndex)
{
    const QColor fallbackBackground = surfaces.base.isValid()
        ? surfaces.base
        : QApplication::palette().color(QPalette::Base);
    const QColor fallbackForeground = aaForegroundForBackground(fallbackBackground);
    return {
        surfaces.backgrounds.value(shadeIndex, fallbackBackground),
        surfaces.foregrounds.value(shadeIndex, fallbackForeground)
    };
}

ThemeScrollBarColors scrollBarColorsForSurfaceIndex(const GeneratedThemeSurfaces& surfaces, int surfaceIndex)
{
    const auto colorAt = [&surfaces](int index) {
        return surfaces.backgrounds.value(qBound(0, index, surfaces.backgrounds.size() - 1));
    };
    return {
        colorAt(surfaceIndex),
        colorAt(surfaceIndex + 1),
        colorAt(surfaceIndex + 2),
        colorAt(surfaceIndex + 3)
    };
}

constexpr auto DockSeparatorNormalColorProperty = "speedcrunchDockSeparatorNormalColor";
constexpr auto DockSeparatorActiveColorProperty = "speedcrunchDockSeparatorActiveColor";
constexpr auto DockSeparatorStyleInstalledProperty = "speedcrunchDockSeparatorStyleInstalled";

QColor dockSeparatorColorForWidget(const QWidget* widget, const char* propertyName)
{
    for (const QWidget* current = widget; current != nullptr; current = current->parentWidget()) {
        const QColor color = current->property(propertyName).value<QColor>();
        if (color.isValid())
            return color;
    }
    return QColor();
}

QRect dockSeparatorStrokeRect(const QRect& separatorRect)
{
    QRect strokeRect = separatorRect;
    if (strokeRect.width() > strokeRect.height()) {
        const int height = qMax(1, UiConfig::DockSplitterStrokeWidth);
        strokeRect.setTop(strokeRect.center().y() - (height - 1) / 2);
        strokeRect.setHeight(height);
    } else {
        const int width = qMax(1, UiConfig::DockSplitterStrokeWidth);
        strokeRect.setLeft(strokeRect.center().x() - (width - 1) / 2);
        strokeRect.setWidth(width);
    }
    return strokeRect;
}

class DockSeparatorStyle : public QProxyStyle {
public:
    using QProxyStyle::QProxyStyle;

    void drawPrimitive(PrimitiveElement element,
                       const QStyleOption* option,
                       QPainter* painter,
                       const QWidget* widget = nullptr) const override
    {
        if (element != PE_IndicatorDockWidgetResizeHandle || option == nullptr || widget == nullptr) {
            QProxyStyle::drawPrimitive(element, option, painter, widget);
            return;
        }

        const QColor normalColor = dockSeparatorColorForWidget(widget, DockSeparatorNormalColorProperty);
        const QColor activeColor = dockSeparatorColorForWidget(widget, DockSeparatorActiveColorProperty);
        if (!normalColor.isValid() || !activeColor.isValid()) {
            QProxyStyle::drawPrimitive(element, option, painter, widget);
            return;
        }

        const QRect strokeRect = dockSeparatorStrokeRect(option->rect);
        const QPoint cursorPosition = widget->mapFromGlobal(QCursor::pos());
        const bool cursorOverStroke = strokeRect.contains(cursorPosition);
        const bool active = option->state.testFlag(State_MouseOver)
            || option->state.testFlag(State_Sunken)
            || cursorOverStroke;
        painter->fillRect(strokeRect, active ? activeColor : normalColor);
    }
};

void ensureDockSeparatorStyleInstalled()
{
    if (QApplication::style()->property(DockSeparatorStyleInstalledProperty).toBool())
        return;

    QStyle* style = new DockSeparatorStyle(QApplication::style());
    style->setProperty(DockSeparatorStyleInstalledProperty, true);
    QApplication::setStyle(style);
}

QString splitterStyleSheet(const QColor& normal, const QColor& active)
{
    return QStringLiteral(
        "QSplitter::handle { background-color: %1; }"
        "QSplitter::handle:hover, QSplitter::handle:pressed { background-color: %2; }")
        .arg(normal.name(), active.name());
}

QString splitterHandleStyleSheet(const QColor& normal, const QColor& active)
{
    return QStringLiteral(
        "QSplitterHandle { background-color: %1; }"
        "QSplitterHandle:hover, QSplitterHandle:pressed { background-color: %2; }")
        .arg(normal.name(), active.name());
}

QString scrollBarStyleSheet(const ThemeScrollBarColors& colors)
{
    return QStringLiteral(
        "QScrollBar:vertical {"
        " background: %1; border: 0; margin: 0; width: 10px;"
        "}"
        "QScrollBar:horizontal {"
        " background: %1; border: 0; margin: 0; height: 10px;"
        "}"
        "QScrollBar::handle:vertical, QScrollBar::handle:horizontal {"
        " background: %2; border: 0; border-radius: 4px; min-height: 20px; min-width: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover {"
        " background: %3;"
        "}"
        "QScrollBar::handle:vertical:pressed, QScrollBar::handle:horizontal:pressed {"
        " background: %4;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        " background: %1; border: 0; width: 0; height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        " background: %1;"
        "}")
        .arg(colors.track.name(),
             colors.thumb.name(),
             colors.hoverThumb.name(),
             colors.pressedThumb.name());
}

void applyScrollCornerColorToScrollArea(QAbstractScrollArea* area, const QColor& color)
{
    if (area == nullptr || !color.isValid())
        return;

    QWidget* corner = area->cornerWidget();
    if (corner == nullptr) {
        corner = new QWidget(area);
        area->setCornerWidget(corner);
    }

    QPalette palette = corner->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        palette.setColor(group, QPalette::Window, color);
        palette.setColor(group, QPalette::Base, color);
        palette.setColor(group, QPalette::Button, color);
    }
    corner->setPalette(palette);
    corner->setAutoFillBackground(true);
    corner->setAttribute(Qt::WA_StyledBackground, true);
    corner->setStyleSheet(QStringLiteral("QWidget { background-color: %1; border: 0; }")
                              .arg(color.name()));
}

void applyMenuSurface(QMenu* menu,
                      const ThemeSurfaceColors& surface,
                      const ThemeSurfaceColors& selectedSurface,
                      QSet<QMenu*>* visitedMenus)
{
    if (menu == nullptr)
        return;
    if (visitedMenus != nullptr) {
        if (visitedMenus->contains(menu))
            return;
        visitedMenus->insert(menu);
    }

    QPalette palette = menu->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        palette.setColor(group, QPalette::Window, surface.background);
        palette.setColor(group, QPalette::Base, surface.background);
        palette.setColor(group, QPalette::Text, surface.foreground);
        palette.setColor(group, QPalette::WindowText, surface.foreground);
        palette.setColor(group, QPalette::ButtonText, surface.foreground);
        palette.setColor(group, QPalette::Highlight, selectedSurface.background);
        palette.setColor(group, QPalette::HighlightedText, selectedSurface.foreground);
    }
    menu->setPalette(palette);
    menu->setStyleSheet(QStringLiteral(
        "QMenu {"
        " background-color: %1; color: %2;"
        " border: 1px solid %3; border-radius: 8px;"
        "}"
        "QMenu::item:selected {"
        " background-color: %4; color: %5;"
        "}")
                            .arg(surface.background.name(),
                                 surface.foreground.name(),
                                 selectedSurface.background.name(),
                                 selectedSurface.background.name(),
                                 selectedSurface.foreground.name()));

    for (QAction* action : menu->actions()) {
        if (QMenu* submenu = action->menu())
            applyMenuSurface(submenu, surface, selectedSurface, visitedMenus);
    }
}

void applyMenuSurface(QMenu* menu,
                      const ThemeSurfaceColors& surface,
                      const ThemeSurfaceColors& selectedSurface)
{
    QSet<QMenu*> visitedMenus;
    applyMenuSurface(menu, surface, selectedSurface, &visitedMenus);
}

void applyScrollBarColorsToScrollArea(QAbstractScrollArea* area, const ThemeScrollBarColors& colors)
{
    if (area == nullptr)
        return;

    const QString styleSheet = scrollBarStyleSheet(colors);
    if (QScrollBar* bar = area->verticalScrollBar())
        bar->setStyleSheet(styleSheet);
    if (QScrollBar* bar = area->horizontalScrollBar())
        bar->setStyleSheet(styleSheet);
    applyScrollCornerColorToScrollArea(area, colors.track);
}

QIcon dockTitleButtonIcon(bool isCloseButton, const QColor& foreground)
{
    QPixmap pixmap(12, 12);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(foreground, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (isCloseButton) {
        painter.drawLine(QPointF(3, 3), QPointF(9, 9));
        painter.drawLine(QPointF(9, 3), QPointF(3, 9));
    } else {
        painter.drawRect(QRectF(3, 3, 6, 6));
    }
    return QIcon(pixmap);
}

QIcon dockSearchClearButtonIcon(const QColor& fill, const QColor& cross)
{
    const auto pixmapForScale = [&fill, &cross](qreal scale) {
        constexpr int logicalSize = 16;
        const int physicalSize = qRound(logicalSize * scale);
        QPixmap pixmap(physicalSize, physicalSize);
        pixmap.setDevicePixelRatio(scale);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawEllipse(QRectF(1.5, 1.5, 13.0, 13.0));

        QPen pen(cross, 1.45, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.drawLine(QPointF(5.25, 5.25), QPointF(10.75, 10.75));
        painter.drawLine(QPointF(10.75, 5.25), QPointF(5.25, 10.75));
        return pixmap;
    };

    QIcon icon;
    icon.addPixmap(pixmapForScale(1.0));
    icon.addPixmap(pixmapForScale(2.0));
    icon.addPixmap(pixmapForScale(3.0));
    return icon;
}

void applyDockSearchClearButtonIcon(QLineEdit* searchBox, const QIcon& icon)
{
    if (searchBox == nullptr)
        return;

    for (QAction* action : searchBox->actions())
        action->setIcon(icon);

    for (QToolButton* button : searchBox->findChildren<QToolButton*>()) {
        if (QAction* action = button->defaultAction())
            action->setIcon(icon);
        button->setIcon(icon);
        button->setIconSize(QSize(16, 16));
    }
}

void applyDockSearchClearButtonIcon(QLineEdit* searchBox,
                                    const GeneratedThemeSurfaces& surfaces,
                                    bool focused)
{
    if (searchBox == nullptr)
        return;

    const ThemeSurfaceColors clearButtonSurface = focused
        ? surfaces.primary
        : themeSurfaceForShadeIndex(surfaces, UiConfig::DockTextInputOutlineShade);
    applyDockSearchClearButtonIcon(searchBox,
                                   dockSearchClearButtonIcon(clearButtonSurface.background,
                                                             clearButtonSurface.foreground));
}

void applyDockSearchClearButtonIcon(QLineEdit* searchBox,
                                    const GeneratedThemeSurfaces& surfaces)
{
    applyDockSearchClearButtonIcon(searchBox, surfaces, searchBox != nullptr && searchBox->hasFocus());
}

void applySurfaceToLabel(QLabel* label, const ThemeSurfaceColors& surface)
{
    if (label == nullptr)
        return;

    label->setPalette(paletteForThemeSurface(label->palette(), surface));
    label->setStyleSheet(QStringLiteral(
        "QLabel { background-color: %1; color: %2; }")
                             .arg(surface.background.name(),
                                  surface.foreground.name()));
}

void applyNoMatchLabelSurface(QAbstractItemView* view, const ThemeSurfaceColors& surface)
{
    if (view == nullptr || view->viewport() == nullptr)
        return;

    for (QLabel* label : view->viewport()->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
        if (!label->property("dockListNoMatchLabel").toBool())
            continue;
        applySurfaceToLabel(label, surface);
        label->setStyleSheet(QStringLiteral("QLabel { background: transparent; color: %1; }")
                                 .arg(surface.foreground.name()));
    }
}

bool isStructuralDockBackgroundWidget(QWidget* widget)
{
    if (widget == nullptr)
        return false;
    if (qobject_cast<QAbstractButton*>(widget)
        || qobject_cast<QAbstractScrollArea*>(widget)
        || qobject_cast<QComboBox*>(widget)
        || qobject_cast<QHeaderView*>(widget)
        || qobject_cast<QLabel*>(widget)
        || qobject_cast<QLineEdit*>(widget)
        || qobject_cast<QMenu*>(widget)) {
        return false;
    }
    if (qobject_cast<QAbstractScrollArea*>(widget->parentWidget()))
        return false;
    return true;
}

void applySurfaceToStructuralDockWidget(QWidget* widget, const ThemeSurfaceColors& surface)
{
    if (!isStructuralDockBackgroundWidget(widget))
        return;

    widget->setPalette(paletteForThemeSurface(widget->palette(), surface));
    widget->setAutoFillBackground(true);
    widget->setAttribute(Qt::WA_StyledBackground, true);
    widget->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                              .arg(surface.background.name(),
                                   surface.foreground.name()));
}

void applySurfaceToContainingRow(QWidget* root, QWidget* item, const ThemeSurfaceColors& surface)
{
    if (root == nullptr || item == nullptr)
        return;

    QList<QWidget*> candidates = root->findChildren<QWidget*>();
    candidates.prepend(root);
    for (QWidget* candidate : candidates) {
        QLayout* layout = candidate->layout();
        if (layout == nullptr || layout->indexOf(item) < 0)
            continue;

        applySurfaceToStructuralDockWidget(candidate, surface);
        for (int index = 0; index < layout->count(); ++index) {
            applySurfaceToLabel(qobject_cast<QLabel*>(layout->itemAt(index)->widget()), surface);
        }
        return;
    }
}

bool isComboBoxPopupView(const QAbstractItemView* view, const QList<QComboBox*>& comboBoxes)
{
    for (const QComboBox* comboBox : comboBoxes) {
        if (comboBox->view() == view || comboBox->isAncestorOf(view))
            return true;
    }
    return false;
}

void applyGeneratedDockChromeSurfaces(QDockWidget* dock, const GeneratedThemeSurfaces& surfaces)
{
    if (dock == nullptr)
        return;

    dock->setPalette(paletteForThemeSurface(dock->palette(), surfaces.headersAndBorders));
    dock->setStyleSheet(QStringLiteral(
        "QDockWidget {"
        " color: %2;"
        "}"
        "QDockWidget::title {"
        " background-color: %1; color: %2; padding: 5px 4px;"
        "}"
        "QDockWidget::close-button, QDockWidget::float-button {"
        " background-color: %3; color: %4;"
        " border: 1px solid %1; border-radius: 8px; padding: 2px;"
        "}")
                            .arg(surfaces.headersAndBorders.background.name(),
                                 surfaces.headersAndBorders.foreground.name(),
                                 surfaces.inputs.background.name(),
                                 surfaces.inputs.foreground.name()));
    dock->style()->unpolish(dock);
    dock->style()->polish(dock);
    dock->setWindowTitle(dock->windowTitle());
    dock->update();
    for (QAbstractButton* button : dock->findChildren<QAbstractButton*>()) {
        const bool closeButton = button->objectName() == QLatin1String("qt_dockwidget_closebutton");
        const bool floatButton = button->objectName() == QLatin1String("qt_dockwidget_floatbutton");
        if (!closeButton && !floatButton)
            continue;
        button->setPalette(paletteForThemeSurface(button->palette(), surfaces.inputs));
        button->setIcon(dockTitleButtonIcon(closeButton, surfaces.inputs.foreground));
    }
}

void applyGeneratedDockContentSurfaces(MainWindow* owner, QDockWidget* dock, const GeneratedThemeSurfaces& surfaces)
{
    QWidget* dockContent = dock->widget();
    if (dockContent == nullptr)
        return;

    const ThemeSurfaceColors dockBackground =
        themeSurfaceForShadeIndex(surfaces, UiConfig::DockBackgroundShade);
    const ThemeSurfaceColors dockHeader =
        themeSurfaceForShadeIndex(surfaces, UiConfig::DockHeaderShade);
    const ThemeSurfaceColors dockHoveredItem =
        themeSurfaceForShadeIndex(surfaces, UiConfig::DockHoveredItemShade);
    const int dockComboPopupShade =
        qMin(UiConfig::DockBackgroundShade + 1, UiConfig::Shade600);
    const ThemeSurfaceColors dockComboPopup =
        themeSurfaceForShadeIndex(surfaces, dockComboPopupShade);
    const ThemeSurfaceColors dockTextInput =
        themeSurfaceForShadeIndex(surfaces, UiConfig::DockTextInputShade);
    const ThemeSurfaceColors dockTextInputOutline =
        themeSurfaceForShadeIndex(surfaces, UiConfig::DockTextInputOutlineShade);
    const ThemeSurfaceColors dockUnfocusedSelection =
        themeSurfaceForShadeIndex(surfaces, UiConfig::DockUnfocusedSelectedItemShade);
    BitFieldWidget* bitField = qobject_cast<BitFieldWidget*>(dockContent);
    const QList<QComboBox*> comboBoxes = dockContent->findChildren<QComboBox*>();
    bool hasListOrTable = false;
    for (QAbstractItemView* view : dockContent->findChildren<QAbstractItemView*>()) {
        if (!isComboBoxPopupView(view, comboBoxes)) {
            hasListOrTable = true;
            break;
        }
    }
    Q_UNUSED(hasListOrTable);
    const ThemeSurfaceColors& dockSurface = dockBackground;

    dockContent->setPalette(paletteForThemeSurface(dockContent->palette(), dockSurface));
    dockContent->setAutoFillBackground(true);
    applySurfaceToStructuralDockWidget(dockContent, dockSurface);
    for (QWidget* child : dockContent->findChildren<QWidget*>())
        applySurfaceToStructuralDockWidget(child, dockSurface);
    if (bitField != nullptr)
        bitField->setThemeColors(dockBackground.background,
                                 dockBackground.foreground,
                                 dockHeader.background,
                                 dockHeader.foreground,
                                 dockUnfocusedSelection.background,
                                 dockUnfocusedSelection.foreground,
                                 surfaces.primary.background,
                                 surfaces.primary.foreground);
    const ThemeScrollBarColors listScrollBars =
        scrollBarColorsForSurfaceIndex(surfaces, UiConfig::DockBackgroundShade);
    const ThemeScrollBarColors comboPopupScrollBars =
        scrollBarColorsForSurfaceIndex(surfaces, dockComboPopupShade);

    if (BookDock* bookDock = qobject_cast<BookDock*>(dock)) {
        bookDock->setContentSurfaceColors(dockBackground.background,
                                          dockBackground.foreground);
    }

    for (QAbstractScrollArea* scrollArea : dockContent->findChildren<QAbstractScrollArea*>()) {
        if (qobject_cast<QAbstractItemView*>(scrollArea))
            continue;
        applyScrollBarColorsToScrollArea(scrollArea, listScrollBars);
    }

    for (QComboBox* comboBox : comboBoxes) {
        applySurfaceToContainingRow(dockContent, comboBox, dockSurface);
        comboBox->setPalette(paletteForThemeSurface(comboBox->palette(), dockTextInput));
        comboBox->setStyleSheet(QStringLiteral(
            "QComboBox {"
            " background-color: %1; color: %2;"
            " border: 1px solid %3; border-radius: 8px; padding: 4px %6px 4px 8px;"
            "}"
            "QComboBox::drop-down {"
            " subcontrol-origin: border; subcontrol-position: top right;"
            " width: %7px; border: none; background: transparent;"
            "}"
            "QComboBox::down-arrow {"
            " image: none; width: 0px; height: 0px;"
            "}"
            "QComboBox QAbstractItemView {"
            " background-color: %4; color: %5;"
            " border: 0; outline: 0;"
            "}")
                                    .arg(dockTextInput.background.name(),
                                         dockTextInput.foreground.name(),
                                         dockTextInputOutline.background.name(),
                                         dockComboPopup.background.name(),
                                         dockComboPopup.foreground.name())
                                    .arg(DockComboBoxChevron::IndicatorWidth + 4)
                                    .arg(DockComboBoxChevron::IndicatorWidth));
        DockComboBoxChevron::apply(comboBox,
                                   dockTextInput.foreground,
                                   dockTextInputOutline.background);
        if (QAbstractItemView* popupView = comboBox->view()) {
            QPalette popupPalette = paletteForThemeSurface(popupView->palette(), dockComboPopup);
            popupPalette.setColor(QPalette::Highlight, dockHoveredItem.background);
            popupPalette.setColor(QPalette::HighlightedText, dockHoveredItem.foreground);
            popupView->setStyleSheet(QStringLiteral(
                "QAbstractItemView {"
                " background-color: %1; color: %2;"
                " border: 0; border-radius: %7px; outline: 0;"
                " padding: %3px %4px;"
                "}"
                "QAbstractItemView::item {"
                " border: 0; border-radius: %8px;"
                "}"
                "QAbstractItemView::item:hover {"
                " background-color: %5; color: %6;"
                " border-radius: %8px;"
                "}")
                                         .arg(dockComboPopup.background.name(),
                                              dockComboPopup.foreground.name())
                                         .arg(kDockListVerticalPadding)
                                         .arg(kDockListHorizontalPadding)
                                         .arg(dockHoveredItem.background.name(),
                                              dockHoveredItem.foreground.name())
                                         .arg(UiConfig::CompletionPopupCornerRadius)
                                         .arg(UiConfig::DockHoveredItemCornerRadius)
                                     + scrollBarStyleSheet(comboPopupScrollBars));
            popupView->setPalette(popupPalette);
            popupView->viewport()->setPalette(popupPalette);
            applyScrollBarColorsToScrollArea(popupView, comboPopupScrollBars);
        }
    }

    for (QLineEdit* searchBox : dockContent->findChildren<QLineEdit*>()) {
        applySurfaceToContainingRow(dockContent, searchBox, dockSurface);
        searchBox->setProperty("speedcrunchDockTextInput", true);
        if (owner != nullptr)
            searchBox->installEventFilter(owner);
        searchBox->setPalette(paletteForThemeSurface(searchBox->palette(), dockTextInput));
        if (ThemedLineEdit* themedSearchBox = dynamic_cast<ThemedLineEdit*>(searchBox))
            themedSearchBox->setCursorColor(surfaces.primary.background);
        searchBox->setStyleSheet(QStringLiteral(
            "QLineEdit {"
            " background-color: %1; color: %2;"
            " border: %5px solid %3; border-radius: 8px; padding: 4px 8px;"
            "}"
            "QLineEdit:focus {"
            " border: %5px solid %4;"
            "}")
                                     .arg(dockTextInput.background.name(),
                                          dockTextInput.foreground.name(),
                                          dockTextInputOutline.background.name(),
                                          surfaces.primary.background.name())
                                     .arg(UiConfig::OutlineStrokeWidth));
        applyDockSearchClearButtonIcon(searchBox, surfaces);
    }

    for (QAbstractItemView* view : dockContent->findChildren<QAbstractItemView*>()) {
        if (isComboBoxPopupView(view, comboBoxes))
            continue;
        QPalette palette = paletteForThemeSurface(view->palette(), dockBackground);
        palette.setColor(QPalette::Active, QPalette::Highlight, surfaces.primary.background);
        palette.setColor(QPalette::Active, QPalette::HighlightedText, surfaces.primary.foreground);
        palette.setColor(QPalette::Inactive, QPalette::Highlight, dockUnfocusedSelection.background);
        palette.setColor(QPalette::Inactive, QPalette::HighlightedText, dockUnfocusedSelection.foreground);
        palette.setColor(QPalette::Disabled, QPalette::Highlight, dockUnfocusedSelection.background);
        palette.setColor(QPalette::Disabled, QPalette::HighlightedText, dockUnfocusedSelection.foreground);
        view->setProperty("dockListHoverBackground", dockHoveredItem.background);
        view->setProperty("dockListHoverForeground", dockHoveredItem.foreground);
        view->setProperty("dockListActiveSelectionBackground", surfaces.primary.background);
        view->setProperty("dockListActiveSelectionForeground", surfaces.primary.foreground);
        view->setProperty("dockListInactiveSelectionBackground", dockUnfocusedSelection.background);
        view->setProperty("dockListInactiveSelectionForeground", dockUnfocusedSelection.foreground);
        view->setStyleSheet(QStringLiteral(
            "QAbstractItemView {"
            " background-color: %1; color: %2;"
            " border: 0;"
            " padding: %5px %6px;"
            "}"
            "QAbstractItemView::item:hover {"
            " background-color: %3; color: %4;"
            " border-radius: %7px;"
            "}")
                                .arg(dockBackground.background.name(),
                                     dockBackground.foreground.name(),
                                     dockHoveredItem.background.name(),
                                     dockHoveredItem.foreground.name())
                                .arg(kDockListVerticalPadding)
                                .arg(kDockListHorizontalPadding)
                                .arg(UiConfig::DockHoveredItemCornerRadius)
                            + scrollBarStyleSheet(listScrollBars));
        view->setPalette(palette);
        view->viewport()->setPalette(palette);
        applyScrollBarColorsToScrollArea(view, listScrollBars);
        applyNoMatchLabelSurface(view, dockBackground);
    }

    for (QHeaderView* header : dockContent->findChildren<QHeaderView*>()) {
        header->setPalette(paletteForThemeSurface(header->palette(), dockBackground));
        header->setStyleSheet(QStringLiteral(
            "QHeaderView { background-color: %1; color: %2; border: 0; }"
            "QHeaderView::section {"
            " background-color: %1; color: %2;"
            " border: 0;"
            " border-top: 1px solid %3;"
            " border-right: 1px solid %3;"
            " border-bottom: 1px solid %3;"
            " padding: 4px 8px;"
            "}"
            "QHeaderView::section:first {"
            " border-left: 0;"
            "}"
            "QHeaderView::section:last {"
            " border-right: 0;"
            "}")
                                  .arg(dockBackground.background.name(),
                                       dockBackground.foreground.name(),
                                       dockHeader.background.name()));
    }
}

void applyThemeBackgroundRoleToWidget(QWidget* widget, const QColor& background)
{
    if (widget == nullptr)
        return;

    QPalette pal = widget->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        pal.setColor(group, QPalette::Window, background);
        pal.setColor(group, QPalette::Base, background);
        pal.setColor(group, QPalette::Button, background);
    }
    widget->setPalette(pal);
    widget->setAutoFillBackground(true);
    widget->setAttribute(Qt::WA_StyledBackground, true);
    if (!qobject_cast<QSplitter*>(widget))
        widget->setStyleSheet(QStringLiteral("background-color: %1;").arg(background.name()));
}

void applyDockTabBarBackgroundToWidget(QWidget* widget, const ThemeSurfaceColors& surface)
{
    if (widget == nullptr)
        return;

    widget->setProperty("speedcrunchDockTabBackground", true);
    widget->setPalette(paletteForThemeSurface(widget->palette(), surface));
    widget->setAutoFillBackground(true);
    widget->setAttribute(Qt::WA_StyledBackground, true);
    widget->setStyleSheet(QStringLiteral(
        "QWidget[speedcrunchDockTabBackground=\"true\"] {"
        " background-color: %1; border: 0;"
        "}")
                              .arg(surface.background.name()));
}

QString oklchThemeReportPath()
{
    return QDir(QDir::tempPath()).absoluteFilePath(
        QStringLiteral("speedcrunch-oklch-theme-report.html"));
}

// Theme application can run multiple times while startup restores widgets, but
// later user theme changes still need a fresh diagnostics report. Keying by the
// generated colors suppresses duplicate writes without turning the report into a
// process-wide one-shot.
QString oklchThemeReportKey(const GeneratedThemeSurfaces& surfaces)
{
    QStringList colors;
    colors.reserve(surfaces.backgrounds.size() + surfaces.foregrounds.size() + 2);
    colors.append(surfaces.base.name(QColor::HexArgb));
    colors.append(QString::number(static_cast<int>(surfaces.polarity)));
    for (const QColor& color : surfaces.backgrounds)
        colors.append(color.name(QColor::HexArgb));
    for (const QColor& color : surfaces.foregrounds)
        colors.append(color.name(QColor::HexArgb));
    return colors.join(QLatin1Char('|'));
}

QString& lastOklchThemeReportKey()
{
    static QString key;
    return key;
}

QString debugColorName(const QColor& color)
{
    return color.isValid()
        ? color.name(QColor::HexRgb).toUpper()
        : QStringLiteral("(invalid)");
}

QString paletteColorName(const QWidget* widget, QPalette::ColorRole role)
{
    return widget ? debugColorName(widget->palette().color(role)) : QStringLiteral("(missing)");
}

QString grabbedCenterColorName(QWidget* widget)
{
    if (widget == nullptr || widget->size().isEmpty() || !widget->isVisible())
        return QStringLiteral("(unavailable)");

    const QPixmap pixmap = widget->grab();
    const QImage image = pixmap.toImage();
    if (image.isNull() || image.width() <= 0 || image.height() <= 0)
        return QStringLiteral("(unavailable)");

    return debugColorName(image.pixelColor(image.width() / 2, image.height() / 2));
}

QString htmlTableCell(const QString& value)
{
    return QStringLiteral("<td><code>%1</code></td>").arg(value.toHtmlEscaped());
}

void appendDiagnosticRow(QTextStream& out,
                         const QString& widget,
                         const QString& expected,
                         const QString& palette,
                         const QString& viewportPalette,
                         const QString& grab,
                         const QString& styleSheet)
{
    out << "<tr><th scope=\"row\">" << widget.toHtmlEscaped() << "</th>"
        << htmlTableCell(expected)
        << htmlTableCell(palette)
        << htmlTableCell(viewportPalette)
        << htmlTableCell(grab)
        << htmlTableCell(styleSheet)
        << "</tr>\n";
}

QString shortStyleSheet(const QWidget* widget)
{
    if (widget == nullptr)
        return QStringLiteral("(missing)");

    QString styleSheet = widget->styleSheet().simplified();
    constexpr int kMaximumStyleSheetLength = 120;
    if (styleSheet.size() > kMaximumStyleSheetLength)
        styleSheet = styleSheet.left(kMaximumStyleSheetLength) + QStringLiteral("...");
    return styleSheet.isEmpty() ? QStringLiteral("(empty)") : styleSheet;
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

int& activeSessionTabDragCount()
{
    static int count = 0;
    return count;
}

QPointer<MainWindow>& primaryMainWindow()
{
    static QPointer<MainWindow> window;
    return window;
}

QList<QPointer<MainWindow>>& allMainWindows()
{
    static QList<QPointer<MainWindow>> windows;
    return windows;
}

QHash<QString, QPointer<MainWindow>>& windowIds()
{
    static QHash<QString, QPointer<MainWindow>> ids;
    return ids;
}

QPointer<ResultDisplay>& globallyActiveDisplay()
{
    static QPointer<ResultDisplay> display;
    return display;
}

QPointer<Editor>& globallyActiveEditor()
{
    static QPointer<Editor> editor;
    return editor;
}

bool& activeEditorDisplayPaneActivationInProgress()
{
    static bool inProgress = false;
    return inProgress;
}

int& dockTextInputFocusTransferDepth()
{
    static int depth = 0;
    return depth;
}

bool dockTextInputFocusTransferInProgress()
{
    return dockTextInputFocusTransferDepth() > 0;
}

QPointer<QWidget>& pendingDockTextInputFocusTarget()
{
    static QPointer<QWidget> target;
    return target;
}

QPointer<QWidget>& pendingDockFocusTarget()
{
    static QPointer<QWidget> target;
    return target;
}

QPointer<ResultDisplay>& pendingSessionTabActivationDisplay()
{
    static QPointer<ResultDisplay> display;
    return display;
}

QPointer<Editor>& pendingSessionTabActivationEditor()
{
    static QPointer<Editor> editor;
    return editor;
}

QPointer<Editor>& pendingWindowActivationEditor()
{
    static QPointer<Editor> editor;
    return editor;
}

QPointer<Editor>& editorBeforeWindowDeactivate()
{
    static QPointer<Editor> editor;
    return editor;
}

QPointer<QWidget>& focusWidgetBeforeWindowDeactivate()
{
    static QPointer<QWidget> widget;
    return widget;
}

QPointer<QWidget>& lastFocusWidgetInActiveWindow()
{
    static QPointer<QWidget> widget;
    return widget;
}

QPointer<QWidget>& pendingWindowActivationFocusWidget()
{
    static QPointer<QWidget> widget;
    return widget;
}

int& windowActivationRestoreGeneration()
{
    static int generation = 0;
    return generation;
}

void cancelWindowActivationRestore()
{
    pendingWindowActivationEditor() = nullptr;
    pendingWindowActivationFocusWidget() = nullptr;
    ++windowActivationRestoreGeneration();
}

class DockTextInputFocusTransferGuard {
public:
    DockTextInputFocusTransferGuard()
    {
        ++dockTextInputFocusTransferDepth();
    }

    ~DockTextInputFocusTransferGuard()
    {
        --dockTextInputFocusTransferDepth();
    }
};

bool& appShutdownInProgress()
{
    static bool shuttingDown = false;
    return shuttingDown;
}

bool applicationShutdownInProgress()
{
    // MainWindow close events can be triggered by more than one shutdown path:
    // the primary window, QApplication::quit(), and the single-instance startup
    // handshake in main.cpp. Keep this helper in sync with main.cpp's dynamic
    // property so child windows can tell a real app shutdown from an ordinary
    // child-window close.
    return appShutdownInProgress()
        || (qApp && qApp->property("speedcrunchShutdownInProgress").toBool());
}


QList<QPointer<QWidget>>& panesPendingSessionTabDragDeletion()
{
    static QList<QPointer<QWidget>> panes;
    return panes;
}

void flushPanesPendingSessionTabDragDeletion()
{
    if (activeSessionTabDragCount() > 0)
        return;

    QList<QPointer<QWidget>> panes = panesPendingSessionTabDragDeletion();
    panesPendingSessionTabDragDeletion().clear();
    for (const QPointer<QWidget>& pane : panes) {
        if (pane != nullptr)
            pane->deleteLater();
    }
}

void deletePaneAfterSessionTabDrag(QWidget* pane)
{
    if (pane == nullptr)
        return;

    pane->hide();
    pane->setParent(nullptr);
    if (activeSessionTabDragCount() > 0) {
        panesPendingSessionTabDragDeletion().append(QPointer<QWidget>(pane));
        return;
    }

    pane->deleteLater();
}

class SessionTabDragGuard {
public:
    SessionTabDragGuard()
    {
        ++activeSessionTabDragCount();
    }

    ~SessionTabDragGuard()
    {
        --activeSessionTabDragCount();
        flushPanesPendingSessionTabDragDeletion();
    }
};

class SessionTabBar : public QTabBar {
public:
    explicit SessionTabBar(QWidget* parent = nullptr)
        : QTabBar(parent)
    {
        setAcceptDrops(true);
        setDrawBase(false);
        setElideMode(Qt::ElideRight);
        setExpanding(true);
        setMouseTracking(true);
        setMovable(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setUsesScrollButtons(false);
        setContextMenuPolicy(Qt::CustomContextMenu);
        applyStyle(QColor());
    }

    static QColor secondarySurfaceColor(const QColor& surface)
    {
        return surface.lightnessF() < 0.5 ? surface.lighter(118) : surface.darker(108);
    }

    void applyStyle(const QColor& selectedText,
                    const QColor& selectedSurface = QColor(),
                    const QColor& hoverText = QColor(),
                    const QColor& hoverSurface = QColor(),
                    const QColor& stripSurface = QColor(),
                    const QColor& inactiveText = QColor(),
                    const QColor& closeButtonHoverText = QColor(),
                    const QColor& closeButtonHoverSurface = QColor())
    {
        const QColor fg = selectedText.isValid()
            ? selectedText
            : palette().color(QPalette::WindowText);
        const QPalette pal = palette();
        const QColor selected = selectedSurface.isValid() ? selectedSurface : pal.color(QPalette::Window);
        const QColor hovered = hoverSurface.isValid()
            ? hoverSurface
            : secondarySurfaceColor(selected);
        const QColor strip = stripSurface.isValid() ? stripSurface : selected;
        const QColor text = inactiveText.isValid()
            ? inactiveText
            : pal.color(QPalette::WindowText);
        const QColor hoveredText = hoverText.isValid()
            ? hoverText
            : text;
        const QColor closeButtonHovered = closeButtonHoverSurface.isValid()
            ? closeButtonHoverSurface
            : hovered;
        const QColor closeButtonHoveredText = closeButtonHoverText.isValid()
            ? closeButtonHoverText
            : hoveredText;
        m_tabStripColor = strip;
        m_inactiveTabColor = strip;
        m_hoveredTabColor = hovered;
        m_selectedTabColor = selected;
        m_tabTextColor = text;
        m_hoveredTextColor = hoveredText;
        m_selectedTextColor = fg;
        m_closeButtonHoverColor = closeButtonHovered;
        m_closeButtonHoverTextColor = closeButtonHoveredText;

        setStyleSheet(QStringLiteral(R"(
                QToolButton {
                    background: transparent;
                    border: none;
                    padding: 0px;
                    margin: 0px;
                }

                QToolButton:hover,
                QToolButton:pressed {
                    background: transparent;
                    border: none;
                }
            )"));
        if (QWidget* row = parentWidget()) {
            QPalette rowPalette = row->palette();
            for (const QPalette::ColorGroup group : {QPalette::Active,
                                                     QPalette::Inactive,
                                                     QPalette::Disabled}) {
                rowPalette.setColor(group, QPalette::Window, m_tabStripColor);
                rowPalette.setColor(group, QPalette::Base, m_tabStripColor);
            }
            row->setPalette(rowPalette);
            row->setAutoFillBackground(true);
            row->setAttribute(Qt::WA_StyledBackground, true);
            row->setStyleSheet(QStringLiteral("background-color: %1;")
                                   .arg(m_tabStripColor.name()));
        }
        QPalette tabPalette = palette();
        for (const QPalette::ColorGroup group : {QPalette::Active,
                                                 QPalette::Inactive,
                                                 QPalette::Disabled}) {
            tabPalette.setColor(group, QPalette::Window, m_tabStripColor);
            tabPalette.setColor(group, QPalette::Base, m_tabStripColor);
        }
        setPalette(tabPalette);
        setAutoFillBackground(true);
        updateGeometry();
        update();
    }

    std::function<void(const QString&, const QPoint&)> tabContextMenuRequested;
    std::function<void(SessionTabBar*, const QString&, int)> sessionTabDropped;
    std::function<MainWindow*()> sourceMainWindow;
    std::function<void(const QString&, const QPoint&)> sessionTabDetached;
    std::function<void(const QString&)> tabCloseRequested;
    std::function<void(const QString&)> tabActivated;

    void setActivePaneSelectedTabIndicatorColor(const QColor& color)
    {
        const QColor activeColor = color.isValid() ? color : QColor();
        if (m_activePaneSelectedTabIndicatorColor == activeColor)
            return;
        m_activePaneSelectedTabIndicatorColor = activeColor;
        refreshCloseButtons();
        update();
    }

    void refreshCloseButtons()
    {
        bool visibilityChanged = false;
        for (int i = 0; i < count(); ++i) {
            const bool showButton = !m_visualDragActive
                && (i == currentIndex() || i == m_hoveredTabIndex);
            QWidget* existingButton = tabButton(i, QTabBar::RightSide);
            QToolButton* closeButton = qobject_cast<QToolButton*>(existingButton);
            if (closeButton == nullptr) {
                closeButton = new QToolButton(this);
                closeButton->setAutoRaise(false);
                closeButton->setCursor(Qt::PointingHandCursor);
                closeButton->setFocusPolicy(Qt::NoFocus);
                closeButton->setText(QStringLiteral("×"));
                closeButton->setStyleSheet(QStringLiteral(R"(
                    QToolButton {
                        background: transparent;
                        border: none;
                        margin: 3px 3px 3px 0px;
                        padding: 0px;
                        min-width: 18px;
                        max-width: 18px;
                        min-height: 18px;
                        max-height: 18px;
                        border-radius: 9px;
                        font-weight: 400;
                        text-align: center;
                    }

                    QToolButton:hover {
                        background: rgba(127, 127, 127, 80);
                    }

                    QToolButton:pressed {
                        background: rgba(127, 127, 127, 192);
                    }
                )"));
                closeButton->setToolTip(tr("Close Session"));
                QFont closeFont = closeButton->font();
                closeFont.setBold(false);
                closeFont.setPixelSize(qMax(11, fontMetrics().height() - 5));
                closeButton->setFont(closeFont);
                const int buttonExtent = qMax(16, fontMetrics().height() + 1);
                closeButton->setFixedSize(buttonExtent, buttonExtent);
                setTabButton(i, QTabBar::RightSide, closeButton);
                connect(closeButton, &QToolButton::clicked, this, [this, closeButton]() {
                    int tabIndex = -1;
                    for (int i = 0; i < count(); ++i) {
                        if (tabButton(i, QTabBar::RightSide) == closeButton) {
                            tabIndex = i;
                            break;
                        }
                    }
                    if (tabIndex >= 0 && tabCloseRequested)
                        tabCloseRequested(tabText(tabIndex));
                });
            }
            const bool selected = i == currentIndex();
            const bool hovered = i == m_hoveredTabIndex;
            const QColor normalText = selected ? m_selectedTextColor : (hovered ? m_hoveredTextColor : m_tabTextColor);
            const QColor hoveredText = m_closeButtonHoverTextColor.isValid()
                ? m_closeButtonHoverTextColor
                : normalText;
            closeButton->setStyleSheet(QStringLiteral(R"(
                    QToolButton {
                        background: transparent;
                        color: %1;
                        border: none;
                        margin: 3px 3px 3px 0px;
                        padding: 0px;
                        min-width: 18px;
                        max-width: 18px;
                        min-height: 18px;
                        max-height: 18px;
                        border-radius: 9px;
                        font-weight: 400;
                        text-align: center;
                    }

                    QToolButton:hover {
                        background: %2;
                        color: %3;
                    }

                    QToolButton:pressed {
                        background: %2;
                        color: %3;
                    }
                )")
                                           .arg(normalText.name(),
                                                m_closeButtonHoverColor.name(),
                                                hoveredText.name()));
            visibilityChanged = visibilityChanged || closeButton->isVisible() != showButton;
            closeButton->setVisible(showButton);
        }
        if (visibilityChanged)
            update();
    }

protected:
    void contextMenuEvent(QContextMenuEvent* event) override
    {
        const int index = tabAt(event->pos());
        if (index >= 0) {
            setCurrentIndex(index);
            if (tabContextMenuRequested)
                tabContextMenuRequested(tabText(index), event->globalPos());
            event->accept();
            return;
        }

        QTabBar::contextMenuEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragTabIndex = tabAt(event->pos());
            m_dragSessionName = m_dragTabIndex >= 0 ? tabText(m_dragTabIndex) : QString();
            m_dragTabPressOffsetX = m_dragTabIndex >= 0
                ? event->pos().x() - tabRect(m_dragTabIndex).left()
                : 0;
        }
        const QString pressedSession = m_dragSessionName;

        if (event->button() == Qt::LeftButton && !pressedSession.isEmpty()) {
            if (currentIndex() != m_dragTabIndex)
                setCurrentIndex(m_dragTabIndex);
            else if (tabActivated)
                tabActivated(pressedSession);
            refreshCloseButtons();
            event->accept();
            return;
        }
        QTabBar::mousePressEvent(event);
        refreshCloseButtons();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        updateHoveredTab(event->pos());

        const bool hasPressedTab = (event->buttons() & Qt::LeftButton)
            && m_dragTabIndex >= 0;
        if (!hasPressedTab) {
            QTabBar::mouseMoveEvent(event);
            refreshCloseButtons();
            return;
        }

        if (!shouldStartCrossBarDrag(event->pos(), event->globalPosition().toPoint())) {
            updateDraggedTabVisual(event->pos());
            event->accept();
            refreshCloseButtons();
            return;
        }

        m_visualDragActive = false;
        m_visualTargetIndex = -1;
        refreshCloseButtons();
        repaint();

        QMimeData* mime = new QMimeData();
        QJsonObject payload;
        payload.insert(QStringLiteral("session"), m_dragSessionName);
        if (sourceMainWindow) {
            if (MainWindow* window = sourceMainWindow())
                payload.insert(QStringLiteral("windowId"), window->objectName());
        }
        mime->setData(QStringLiteral("application/x-speedcrunch-session-tab"),
                      QJsonDocument(payload).toJson(QJsonDocument::Compact));

        QDrag* drag = new QDrag(this);
        drag->setMimeData(mime);
        const int currentDragIndex = qMax(0, indexOfDragSession());
        drag->setPixmap(grab(tabRect(currentDragIndex)));
        const QPoint clampedPos = clampTabDragPos(event->pos());
        drag->setHotSpot(clampedPos - tabRect(currentDragIndex).topLeft());
        QPointer<SessionTabBar> self(this);
        SessionTabDragGuard dragGuard;
        const Qt::DropAction dropAction = drag->exec(Qt::MoveAction);
        if (self == nullptr)
            return;
        if (dropAction != Qt::MoveAction && sessionTabDetached && !m_dragSessionName.isEmpty())
            sessionTabDetached(m_dragSessionName, QCursor::pos());
        m_dragTabIndex = -1;
        m_dragSessionName.clear();
        m_dropIndicatorIndex = -1;
        update();
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && m_visualDragActive) {
            commitVisualTabDrag();
            m_dragTabIndex = -1;
            m_dragSessionName.clear();
            m_visualDragActive = false;
            m_visualTargetIndex = -1;
            refreshCloseButtons();
            update();
            event->accept();
            return;
        }

        QTabBar::mouseReleaseEvent(event);
        if (event->button() == Qt::LeftButton) {
            m_dragTabIndex = -1;
            m_dragSessionName.clear();
            m_visualDragActive = false;
            m_visualTargetIndex = -1;
            refreshCloseButtons();
            update();
        }
    }

    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (event->mimeData()->hasFormat(QStringLiteral("application/x-speedcrunch-session-tab"))) {
            updateDropIndicator(event->position().toPoint());
            event->acceptProposedAction();
        }
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        if (event->mimeData()->hasFormat(QStringLiteral("application/x-speedcrunch-session-tab"))) {
            updateDropIndicator(event->position().toPoint());
            event->acceptProposedAction();
        }
    }

    void dragLeaveEvent(QDragLeaveEvent* event) override
    {
        m_dropIndicatorIndex = -1;
        update();
        QTabBar::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent* event) override
    {
        const QMimeData* mime = event->mimeData();
        if (!mime->hasFormat(QStringLiteral("application/x-speedcrunch-session-tab")))
            return;

        QString sessionName;
        const QByteArray payload = mime->data(QStringLiteral("application/x-speedcrunch-session-tab"));
        const QJsonDocument payloadDoc = QJsonDocument::fromJson(payload);
        if (payloadDoc.isObject())
            sessionName = payloadDoc.object().value(QStringLiteral("session")).toString();
        if (sessionName.isEmpty())
            sessionName = QString::fromUtf8(payload);
        SessionTabBar* sourceTabBar = dynamic_cast<SessionTabBar*>(event->source());
        int targetIndex = tabAt(event->position().toPoint());
        if (targetIndex < 0)
            targetIndex = count();

        if (sessionTabDropped)
            sessionTabDropped(sourceTabBar, sessionName, targetIndex);
        m_dropIndicatorIndex = -1;
        update();
        event->acceptProposedAction();
    }

    void leaveEvent(QEvent* event) override
    {
        m_hoveredTabIndex = -1;
        setCursor(Qt::ArrowCursor);
        refreshCloseButtons();
        update();
        QTabBar::leaveEvent(event);
    }

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::HoverMove) {
            QHoverEvent* hoverEvent = static_cast<QHoverEvent*>(event);
            updateHoveredTab(hoverEvent->position().toPoint());
        }
        return QTabBar::event(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QPainter tabPainter(this);
        tabPainter.setRenderHint(QPainter::Antialiasing, true);
        tabPainter.fillRect(event->rect(), m_tabStripColor);

        const int visualDragIndex = m_visualDragActive ? indexOfDragSession() : -1;
        const int draggedWidth = visualDragIndex >= 0 ? tabRect(visualDragIndex).width() : 0;
        QVector<QRect> paintedRects(count());
        for (int i = 0; i < count(); ++i) {
            if (i == visualDragIndex)
                continue;

            QRect rect = tabRect(i);
            if (m_visualDragActive && m_visualTargetIndex >= 0) {
                if (m_visualTargetIndex > visualDragIndex && i > visualDragIndex && i <= m_visualTargetIndex)
                    rect.translate(-draggedWidth, 0);
                else if (m_visualTargetIndex < visualDragIndex && i >= m_visualTargetIndex && i < visualDragIndex)
                    rect.translate(draggedWidth, 0);
            }
            paintedRects[i] = rect;
            paintTab(&tabPainter, i, rect, false);
        }

        for (int i = 0; i + 1 < count(); ++i) {
            if (i == visualDragIndex || i + 1 == visualDragIndex)
                continue;
            if (i == currentIndex() || i + 1 == currentIndex())
                continue;
            if (i == m_hoveredTabIndex || i + 1 == m_hoveredTabIndex)
                continue;
            if (paintedRects[i].isEmpty() || paintedRects[i + 1].isEmpty())
                continue;

            const int x = (paintedRects[i].right() + paintedRects[i + 1].left()) / 2;
            const int top = pillRect(paintedRects[i]).top() + 5;
            const int bottom = pillRect(paintedRects[i]).bottom() - 5;
            tabPainter.setPen(QPen(m_hoveredTabColor, 1));
            tabPainter.drawLine(QPoint(x, top), QPoint(x, bottom));
        }

        if (visualDragIndex >= 0) {
            QRect draggedRect = tabRect(visualDragIndex);
            draggedRect.moveLeft(qRound(m_visualDragLeft));
            paintTab(&tabPainter, visualDragIndex, draggedRect, true);
        }

        if (m_dropIndicatorIndex < 0)
            return;

        int x = width() - 1;
        if (m_dropIndicatorIndex < count())
            x = tabRect(m_dropIndicatorIndex).left();
        else if (count() > 0)
            x = tabRect(count() - 1).right() + 1;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(palette().highlight().color(), 2));
        painter.drawLine(QPoint(x, 4), QPoint(x, height() - 4));
    }

    QSize tabSizeHint(int index) const override
    {
        const int closeExtent = qMax(16, fontMetrics().height() + 1);
        const int closeWidth = closeExtent + 8;
        const int width = fontMetrics().horizontalAdvance(tabText(index)) + 30 + closeWidth;
        const int height = qMax(30, fontMetrics().height() + 12);
        return QSize(width, height);
    }

private:
    void updateHoveredTab(const QPoint& pos)
    {
        const int hoveredTabIndex = tabAt(pos);
        if (m_hoveredTabIndex != hoveredTabIndex) {
            m_hoveredTabIndex = hoveredTabIndex;
            refreshCloseButtons();
            update();
        }
        setCursor(hoveredTabIndex >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }

    QRect pillRect(const QRect& tabRect) const
    {
        return tabRect.adjusted(2, 3, -2, 0);
    }

    QPainterPath topRoundedTabPath(const QRect& rect) const
    {
        const QRectF tab(rect);
        const qreal radius = qMin(tab.width() / 2.0, tab.height() / 2.0);
        QPainterPath path;
        path.moveTo(tab.left(), tab.bottom());
        path.lineTo(tab.left(), tab.top() + radius);
        path.quadTo(tab.left(), tab.top(), tab.left() + radius, tab.top());
        path.lineTo(tab.right() - radius, tab.top());
        path.quadTo(tab.right(), tab.top(), tab.right(), tab.top() + radius);
        path.lineTo(tab.right(), tab.bottom());
        path.closeSubpath();
        return path;
    }

    void paintTab(QPainter* painter, int index, const QRect& rect, bool dragged) const
    {
        if (index < 0 || index >= count() || rect.isEmpty())
            return;

        const bool selected = index == currentIndex();
        const bool hovered = index == m_hoveredTabIndex && !m_visualDragActive;
        const QRect pill = pillRect(rect);
        if (selected || hovered || dragged) {
            const QColor fill = selected
                ? m_selectedTabColor
                : (hovered || dragged ? m_hoveredTabColor : m_inactiveTabColor);
            painter->setPen(Qt::NoPen);
            painter->setBrush(fill);
            painter->drawPath(topRoundedTabPath(pill));
        }
        if (selected && m_activePaneSelectedTabIndicatorColor.isValid()) {
            const int strokeWidth = UiConfig::ActiveSessionTabIndicatorStrokeWidth;
            const qreal y = pill.bottom() - (strokeWidth - 1) / 2.0;
            painter->setPen(QPen(m_activePaneSelectedTabIndicatorColor,
                                 strokeWidth,
                                 Qt::SolidLine,
                                 Qt::SquareCap));
            painter->setBrush(Qt::NoBrush);
            painter->drawLine(QPointF(pill.left(), y), QPointF(pill.right(), y));
        }

        const int rightButtonWidth = tabButton(index, QTabBar::RightSide) != nullptr
            && tabButton(index, QTabBar::RightSide)->isVisible()
            ? tabButton(index, QTabBar::RightSide)->width() + 6
            : 0;
        const QRect textRect = pill.adjusted(12, 0, -12 - rightButtonWidth, 0);
        painter->setPen(selected ? m_selectedTextColor : (hovered ? m_hoveredTextColor : m_tabTextColor));
        painter->setFont(font());
        painter->drawText(textRect,
                          Qt::AlignVCenter | Qt::AlignLeft,
                          fontMetrics().elidedText(tabText(index), elideMode(), textRect.width()));
    }

    void updateDropIndicator(const QPoint& pos)
    {
        int index = tabAt(clampDropPos(pos));
        if (index < 0) {
            if (count() > 0 && pos.x() < tabRect(0).left())
                index = 0;
            else
                index = count();
        }
        if (index < 0)
            index = count();
        else if (clampDropPos(pos).x() > tabRect(index).center().x())
            ++index;

        if (m_dropIndicatorIndex != index) {
            m_dropIndicatorIndex = index;
            update();
        }
    }

    bool shouldStartCrossBarDrag(const QPoint& pos, const QPoint& globalPos) const
    {
        if (count() == 0)
            return true;

        const int detachDistance = QApplication::startDragDistance();
        if (pos.y() < -detachDistance || pos.y() >= height() + detachDistance)
            return true;

        QWidget* topLevel = window();
        return topLevel != nullptr && !topLevel->frameGeometry().contains(globalPos);
    }

    QPoint clampTabDragPos(const QPoint& pos) const
    {
        if (count() == 0)
            return pos;

        return QPoint(qBound(tabRect(0).left(), pos.x(), tabRect(count() - 1).right()),
                      qBound(0, pos.y(), height() - 1));
    }

    QPoint clampDropPos(const QPoint& pos) const
    {
        if (count() == 0)
            return pos;

        return QPoint(qBound(tabRect(0).left(), pos.x(), tabRect(count() - 1).right()),
                      qBound(0, pos.y(), height() - 1));
    }

    int visualTargetIndexForDrag(qreal visualLeft, int draggedWidth) const
    {
        const int sourceIndex = indexOfDragSession();
        if (sourceIndex < 0 || sourceIndex >= count())
            return -1;

        int targetIndex = sourceIndex;
        if (visualLeft < tabRect(sourceIndex).left()) {
            while (targetIndex > 0 && visualLeft < tabRect(targetIndex - 1).center().x())
                --targetIndex;
        } else {
            const qreal visualRight = visualLeft + draggedWidth;
            while (targetIndex + 1 < count() && visualRight > tabRect(targetIndex + 1).center().x())
                ++targetIndex;
        }
        return qBound(0, targetIndex, count() - 1);
    }

    void updateDraggedTabVisual(const QPoint& pos)
    {
        const int index = indexOfDragSession();
        if (index < 0 || index >= count())
            return;

        const int leftLimit = count() > 0 ? tabRect(0).left() : 0;
        const int rightLimit = count() > 0 ? tabRect(count() - 1).right() - tabRect(index).width() + 1 : width();
        const int clampedRightLimit = qMax(leftLimit, rightLimit);
        m_visualDragActive = true;
        m_visualDragLeft = qBound(leftLimit, pos.x() - m_dragTabPressOffsetX, clampedRightLimit);
        m_visualTargetIndex = visualTargetIndexForDrag(m_visualDragLeft, tabRect(index).width());
        update();
    }

    void commitVisualTabDrag()
    {
        int index = indexOfDragSession();
        if (index < 0 || index >= count())
            return;
        if (m_visualTargetIndex < 0 || m_visualTargetIndex == index)
            return;

        moveTab(index, m_visualTargetIndex);
        m_dragTabIndex = m_visualTargetIndex;
        setCurrentIndex(m_visualTargetIndex);
    }

    int m_dragTabPressOffsetX = 0;
    int m_dragTabIndex = -1;
    int m_hoveredTabIndex = -1;
    int m_dropIndicatorIndex = -1;
    bool m_visualDragActive = false;
    qreal m_visualDragLeft = 0.0;
    int m_visualTargetIndex = -1;
    QColor m_tabStripColor;
    QColor m_inactiveTabColor;
    QColor m_hoveredTabColor;
    QColor m_selectedTabColor;
    QColor m_activePaneSelectedTabIndicatorColor;
    QColor m_tabTextColor;
    QColor m_hoveredTextColor;
    QColor m_selectedTextColor;
    QColor m_closeButtonHoverColor;
    QColor m_closeButtonHoverTextColor;
    QString m_dragSessionName;

    int indexOfDragSession() const
    {
        for (int i = 0; i < count(); ++i) {
            if (tabText(i) == m_dragSessionName)
                return i;
        }
        return m_dragTabIndex;
    }
};

enum class PaneDropZone {
    Center,
    Top,
    Bottom,
    Left,
    Right
};

class SessionPane : public QWidget {
public:
    explicit SessionPane(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("SessionPane"));
        setAcceptDrops(true);
        m_overlay = new QWidget(this);
        m_overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_overlay->setStyleSheet(QStringLiteral("background: rgba(0, 0, 0, 80);"));
        m_overlay->hide();
    }

    std::function<void(SessionTabBar*, const QString&, const QPoint&)> sessionTabDroppedOnPane;
    std::function<bool(SessionTabBar*)> shouldShowOverlayForDrag;
    std::function<void()> paneActivated;
    void setOverlayAreaWidget(QWidget* widget)
    {
        m_overlayAreaWidget = widget;
    }

    PaneDropZone dropZoneForPanePosition(const QPoint& panePos) const
    {
        return dropZoneForPosition(panePos);
    }

    void watchDropTarget(QWidget* widget)
    {
        if (widget == nullptr)
            return;
        widget->installEventFilter(this);
        widget->setAcceptDrops(true);
    }

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        if (paneActivated)
            paneActivated();
        QWidget::mousePressEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (acceptSessionTabDrag(event))
            return;
        QWidget::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        if (acceptSessionTabDrag(event, event->position().toPoint()))
            return;
        QWidget::dragMoveEvent(event);
    }

    void dropEvent(QDropEvent* event) override
    {
        if (handleSessionTabDrop(event, event->position().toPoint()))
            return;
        QWidget::dropEvent(event);
    }

    void dragLeaveEvent(QDragLeaveEvent* event) override
    {
        hideOverlay();
        QWidget::dragLeaveEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        if (m_overlay != nullptr)
            m_overlay->setGeometry(overlayRect(PaneDropZone::Center));
        QWidget::resizeEvent(event);
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        QWidget* watchedWidget = qobject_cast<QWidget*>(watched);
        if (watchedWidget == nullptr)
            return QWidget::eventFilter(watched, event);

        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent* dragEvent = static_cast<QDragEnterEvent*>(event);
            return acceptSessionTabDrag(dragEvent);
        }
        if (event->type() == QEvent::DragMove) {
            QDragMoveEvent* dragEvent = static_cast<QDragMoveEvent*>(event);
            return acceptSessionTabDrag(dragEvent, watchedWidget->mapTo(this, dragEvent->position().toPoint()));
        }
        if (event->type() == QEvent::Drop) {
            QDropEvent* dropEvent = static_cast<QDropEvent*>(event);
            return handleSessionTabDrop(dropEvent, watchedWidget->mapTo(this, dropEvent->position().toPoint()));
        }
        if (event->type() == QEvent::DragLeave) {
            hideOverlay();
            return false;
        }
        if (event->type() == QEvent::MouseButtonPress) {
            if (paneActivated)
                paneActivated();
            return false;
        }

        return QWidget::eventFilter(watched, event);
    }

private:
    bool acceptSessionTabDrag(QDragMoveEvent* event, const QPoint& panePos = QPoint())
    {
        if (!event->mimeData()->hasFormat(QStringLiteral("application/x-speedcrunch-session-tab")))
            return false;
        SessionTabBar* sourceTabBar = dynamic_cast<SessionTabBar*>(event->source());
        if (shouldShowOverlayForDrag && !shouldShowOverlayForDrag(sourceTabBar)) {
            hideOverlay();
            event->acceptProposedAction();
            return true;
        }
        updateOverlay(panePos.isNull() ? event->position().toPoint() : panePos);
        event->acceptProposedAction();
        return true;
    }

    bool handleSessionTabDrop(QDropEvent* event, const QPoint& panePos)
    {
        const QMimeData* mime = event->mimeData();
        if (!mime->hasFormat(QStringLiteral("application/x-speedcrunch-session-tab")))
            return false;

        QString sessionName;
        const QByteArray payload = mime->data(QStringLiteral("application/x-speedcrunch-session-tab"));
        const QJsonDocument payloadDoc = QJsonDocument::fromJson(payload);
        if (payloadDoc.isObject())
            sessionName = payloadDoc.object().value(QStringLiteral("session")).toString();
        if (sessionName.isEmpty())
            sessionName = QString::fromUtf8(payload);
        SessionTabBar* sourceTabBar = dynamic_cast<SessionTabBar*>(event->source());
        if (sourceTabBar == nullptr || sessionName.isEmpty())
            return false;

        hideOverlay();
        if (sessionTabDroppedOnPane)
            sessionTabDroppedOnPane(sourceTabBar, sessionName, panePos);
        event->acceptProposedAction();
        return true;
    }

    void updateOverlay(const QPoint& panePos)
    {
        if (m_overlay == nullptr)
            return;

        m_overlay->setGeometry(overlayRect(dropZoneForPosition(panePos)));
        m_overlay->raise();
        m_overlay->show();
    }

    void hideOverlay()
    {
        if (m_overlay != nullptr)
            m_overlay->hide();
    }

    PaneDropZone dropZoneForPosition(const QPoint& panePos) const
    {
        const QPointF pos(panePos);
        const QRect rect = overlayBounds();
        const QPointF center(rect.center());
        const QPointF top(rect.left() + rect.width() / 2.0, rect.top());
        const QPointF bottom(rect.left() + rect.width() / 2.0, rect.bottom());
        const QPointF left(rect.left(), rect.top() + rect.height() / 2.0);
        const QPointF right(rect.right(), rect.top() + rect.height() / 2.0);

        const auto distanceSquared = [&pos](const QPointF& point) {
            const QPointF delta = pos - point;
            return delta.x() * delta.x() + delta.y() * delta.y();
        };

        PaneDropZone zone = PaneDropZone::Center;
        qreal bestDistance = distanceSquared(center);
        const auto consider = [&bestDistance, &zone, &distanceSquared](PaneDropZone candidate, const QPointF& point) {
            const qreal distance = distanceSquared(point);
            if (distance < bestDistance) {
                bestDistance = distance;
                zone = candidate;
            }
        };
        consider(PaneDropZone::Top, top);
        consider(PaneDropZone::Bottom, bottom);
        consider(PaneDropZone::Left, left);
        consider(PaneDropZone::Right, right);
        return zone;
    }

    QRect overlayRect(PaneDropZone zone) const
    {
        const QRect rect = overlayBounds();
        switch (zone) {
        case PaneDropZone::Center:
            return rect;
        case PaneDropZone::Top:
            return QRect(rect.left(), rect.top(), rect.width(), rect.height() / 2);
        case PaneDropZone::Bottom:
            return QRect(rect.left(), rect.top() + rect.height() / 2, rect.width(), rect.height() - rect.height() / 2);
        case PaneDropZone::Left:
            return QRect(rect.left(), rect.top(), rect.width() / 2, rect.height());
        case PaneDropZone::Right:
            return QRect(rect.left() + rect.width() / 2, rect.top(), rect.width() - rect.width() / 2, rect.height());
        }
        return rect;
    }

    QRect overlayBounds() const
    {
        if (m_overlayAreaWidget == nullptr)
            return this->rect();

        const QPoint topLeft = m_overlayAreaWidget->mapTo(this, QPoint(0, 0));
        return QRect(topLeft, m_overlayAreaWidget->size());
    }

    QWidget* m_overlay = nullptr;
    QWidget* m_overlayAreaWidget = nullptr;
};

QWidget* paneWidgetForDisplay(ResultDisplay* display)
{
    QWidget* widget = display;
    while (widget != nullptr && !qobject_cast<QSplitter*>(widget->parentWidget()))
        widget = widget->parentWidget();
    return widget;
}

bool splitAssignmentDescriptionForImport(const QString& expression,
                                         QString* expressionWithoutDescription,
                                         QString* description = nullptr)
{
    const int equalsPos = expression.indexOf(MathDsl::Equals);
    if (equalsPos < 0)
        return false;

    int depth = 0;
    const int n = expression.size();
    for (int i = equalsPos + 1; i < n; ++i) {
        const QChar ch = expression.at(i);
        if (ch == MathDsl::GroupStart) {
            ++depth;
        } else if (ch == MathDsl::GroupEnd && depth > 0) {
            --depth;
        } else if (ch == MathDsl::CommentSep && depth == 0) {
            *expressionWithoutDescription = expression.left(i).trimmed();
            if (description)
                *description = expression.mid(i + 1).trimmed();
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
    splitAssignmentDescriptionForImport(expression, &expressionToParse);
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
    m_actions.settingsBehaviorSaveWindowPositionOnExit = new QAction(this);
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
    m_actions.settingsResultFormatTrigonometric = new QAction(this);
    m_actions.settingsResultFormatCis = new QAction(this);
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
    m_actions.helpFacebookGroup = new QAction(this);
    m_actions.helpNews = new QAction(this);
    m_actions.helpSource = new QAction(this);
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
    m_actions.settingsBehaviorSaveWindowPositionOnExit->setCheckable(true);
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
    m_actions.settingsResultFormatTrigonometric->setCheckable(true);
    m_actions.settingsResultFormatCis->setCheckable(true);
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
    for (const QPointer<MainWindow>& ptr : allMainWindows()) {
        if (MainWindow* window = ptr.data())
            window->setStatusBarText();
    }
    setWidgetsDirection();
}

void MainWindow::setStatusBarText()
{
    if (m_status.angleUnit) {
        m_status.angleUnitLabel->setText(MainWindow::tr("Angle Mode:"));
        m_status.resultFormatLabel->setText(MainWindow::tr("Notation:"));
        m_status.resultPrecisionLabel->setText(MainWindow::tr("Precision:"));
        m_status.angleUnit->setText(statusBarAngleUnitValue());
        m_status.resultFormat->setText(statusBarResultFormatValue());
        m_status.resultPrecision->setText(statusBarResultPrecisionValue());
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
        { m_status.angleUnitSection, true }
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

void MainWindow::updateColorSchemeActionState()
{
    const auto schemes = m_actions.settingsDisplayColorSchemes;
    bool colorSchemeMatched = false;
    for (auto& action : schemes) {
        const bool matches = m_settings->colorScheme == action->data().toString();
        action->setChecked(matches);
        colorSchemeMatched = colorSchemeMatched || matches;
    }
    m_actions.settingsDisplayColorSchemeCustom->setChecked(!colorSchemeMatched
                                                           && m_settings->colorScheme == QLatin1String("Custom"));
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

void MainWindow::setActionsText()
{
    m_actions.sessionExportHtml->setText(MainWindow::tr("&HTML"));
    m_actions.sessionExportPlainText->setText(MainWindow::tr("Plain &text"));
    m_actions.sessionImport->setText(MainWindow::tr("&Import..."));
    m_actions.sessionImportUserDefinitions->setText(MainWindow::tr("User &Definitions..."));
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
    m_actions.settingsBehaviorSaveWindowPositionOnExit->setText(MainWindow::tr("Save &Window Position on Exit"));
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
        MainWindow::tr("Nearest, Half &Away (round)"));
    m_actions.settingsResultRoundingHalfEven->setText(
        MainWindow::tr("Nearest, Half &Even (roundeven)"));
    m_actions.settingsResultRoundingTowardZero->setText(MainWindow::tr("Toward &Zero (trunc)"));
    m_actions.settingsResultRoundingTowardPositiveInfinity->setText(
        MainWindow::tr("Toward +&∞ (ceil)"));
    m_actions.settingsResultRoundingTowardNegativeInfinity->setText(
        MainWindow::tr("Toward −&∞ (floor)"));
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
        MainWindow::tr("&Exponential (m·s⁻¹)"));
    m_actions.settingsUnitNegativeExponentFraction->setText(
        MainWindow::tr("&Fractional (m/s)"));
    m_actions.settingsResultFormatCartesian->setText(MainWindow::tr("&Rectangular (a + bi)"));
    m_actions.settingsResultFormatPolar->setText(MainWindow::tr("Exponential (reⁱᶿ)"));
    m_actions.settingsResultFormatTrigonometric->setText(
        MainWindow::tr("Trigonometric (r(cos θ + i·sin θ))"));
    m_actions.settingsResultFormatCis->setText(MainWindow::tr("Cis (r·cis(θ))"));
    m_actions.settingsResultFormatPolarAngle->setText(MainWindow::tr("Phasor (r∠θ)"));
    m_actions.settingsImaginaryUnitI->setText(MainWindow::tr("&i"));
    m_actions.settingsImaginaryUnitJ->setText(MainWindow::tr("&j"));
    m_actions.settingsDisplayFont->setText(MainWindow::tr("&Font..."));
    m_actions.settingsDisplayColorSchemeCustom->setText(MainWindow::tr("&Theme..."));
    m_actions.settingsLanguage->setText(MainWindow::tr("&Language..."));

    m_actions.helpManual->setText(MainWindow::tr("User &Manual"));
    m_actions.contextHelp->setText(MainWindow::tr("Context Help"));
    m_actions.helpUpdates->setText(MainWindow::tr("Check for &Updates"));
    m_actions.helpFeedback->setText(MainWindow::tr("Issue Tracker"));
    m_actions.helpCommunity->setText(MainWindow::tr("Google Group"));
    m_actions.helpFacebookGroup->setText(MainWindow::tr("Facebook &Group"));
    m_actions.helpNews->setText(MainWindow::tr("&Blogspot"));
    m_actions.helpSource->setText(MainWindow::tr("Source Code"));
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
    m_actionGroups.complexFormat->addAction(m_actions.settingsResultFormatTrigonometric);
    m_actionGroups.complexFormat->addAction(m_actions.settingsResultFormatCis);
    m_actionGroups.complexFormat->addAction(m_actions.settingsResultFormatPolarAngle);

    m_actionGroups.imaginaryUnit = new QActionGroup(this);
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
    m_menus.session->addAction(m_actions.settingsBehaviorHistorySizeLimit);
    m_menus.session->addSeparator();
    m_menus.session->addAction(m_actions.sessionImport);
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
    m_menus.view->addAction(m_actions.viewStatusBar);
    m_menus.view->addSeparator();
    m_menus.view->addAction(m_actions.viewFormulaBook);
    m_menus.view->addAction(m_actions.viewConstants);
    m_menus.view->addAction(m_actions.viewFunctions);
    m_menus.view->addSeparator();
    m_menus.view->addAction(m_actions.viewVariables);
    m_menus.view->addAction(m_actions.viewUserFunctions);
    m_menus.view->addAction(m_actions.viewUserUnits);
    m_menus.view->addSeparator();
    m_menus.view->addAction(m_actions.viewHistory);
    m_menus.view->addAction(m_actions.viewBitfield);
    m_menus.view->addSeparator();
#if !defined(Q_OS_MACOS)
    m_menus.view->addAction(m_actions.viewMenuBar);
    m_menus.view->addAction(m_actions.viewFullScreenMode);
#endif
    m_menus.settings = new QMenu("", this);
    menuBar()->addMenu(m_menus.settings);

    m_menus.display = m_menus.settings->addMenu("");
    m_menus.display->addAction(m_actions.settingsDisplayColorSchemeCustom);
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
    m_menus.resultRoundingMode = m_menus.results->addMenu("");
    m_menus.resultRoundingMode->addAction(m_actions.settingsResultRoundingHalfAwayFromZero);
    m_menus.resultRoundingMode->addAction(m_actions.settingsResultRoundingHalfEven);
    m_menus.resultRoundingMode->addAction(m_actions.settingsResultRoundingTowardZero);
    m_menus.resultRoundingMode->addAction(m_actions.settingsResultRoundingTowardPositiveInfinity);
    m_menus.resultRoundingMode->addAction(m_actions.settingsResultRoundingTowardNegativeInfinity);
    m_menus.complexNumbers = m_menus.results->addMenu("");
    m_menus.complexForm = m_menus.complexNumbers->addMenu("");
    m_menus.complexForm->addAction(m_actions.settingsResultFormatCartesian);
    m_menus.complexForm->addAction(m_actions.settingsResultFormatPolar);
    m_menus.complexForm->addAction(m_actions.settingsResultFormatTrigonometric);
    m_menus.complexForm->addAction(m_actions.settingsResultFormatCis);
    m_menus.complexForm->addAction(m_actions.settingsResultFormatPolarAngle);
    m_menus.imaginaryUnit = m_menus.complexNumbers->addMenu("");
    m_menus.imaginaryUnit->addAction(m_actions.settingsImaginaryUnitI);
    m_menus.imaginaryUnit->addAction(m_actions.settingsImaginaryUnitJ);
    m_menus.unitNegativeExponentStyle = m_menus.results->addMenu("");
    m_menus.unitNegativeExponentStyle->addAction(
        m_actions.settingsUnitNegativeExponentSuperscript);
    m_menus.unitNegativeExponentStyle->addAction(
        m_actions.settingsUnitNegativeExponentFraction);
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

    m_menus.results->addAction(m_actions.settingsBehaviorPartialResults);
    m_menus.results->addAction(m_actions.settingsBehaviorSimplifyResultExpressions);
    m_menus.results->addAction(m_actions.settingsBehaviorAutoResultToClipboard);

    m_menus.symbols = m_menus.settings->addMenu("");
    m_menus.symbols->addAction(m_actions.sessionImportUserDefinitions);

    m_menus.angleUnit = m_menus.settings->addMenu("");
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitDegree);
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitRadian);
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitGradian);
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitTurn);
    m_menus.angleUnit->addAction(m_actions.settingsAngleUnitRevolution);

    m_menus.window = m_menus.settings->addMenu("");
    m_menus.window->addAction(m_actions.settingsBehaviorSaveWindowPositionOnExit);
    if (!isWaylandPlatform())
        m_menus.window->addAction(m_actions.settingsBehaviorAlwaysOnTop);

    m_menus.settings->addAction(m_actions.settingsLanguage);

    m_menus.help = new QMenu("", this);
    menuBar()->addMenu(m_menus.help);
    m_menus.help->addAction(m_actions.helpManual);
    m_menus.help->addAction(m_actions.contextHelp);
    m_menus.help->addSeparator();
    m_menus.help->addAction(m_actions.helpCommunity);
    m_menus.help->addAction(m_actions.helpFacebookGroup);
    m_menus.help->addAction(m_actions.helpNews);
    m_menus.help->addSeparator();
    m_menus.help->addAction(m_actions.helpFeedback);
    m_menus.help->addAction(m_actions.helpSource);
    m_menus.help->addAction(m_actions.helpDonate);
    m_menus.help->addSeparator();
    m_menus.help->addAction(m_actions.helpUpdates);
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
    m_menus.symbols->setTitle(MainWindow::tr("&Symbols"));
    m_menus.unitNegativeExponentStyle->setTitle(MainWindow::tr("Unit Notation"));
    m_menus.resultRoundingMode->setTitle(MainWindow::tr("Rounding Mode"));
    m_menus.resultFormat->setTitle(MainWindow::tr("&Notation"));
    m_menus.decimal->setTitle(MainWindow::tr("&Decimal"));
    m_menus.precision->setTitle(MainWindow::tr("&Precision"));
    m_menus.angleUnit->setTitle(MainWindow::tr("&Angle Mode"));
    m_menus.complexNumbers->setTitle(MainWindow::tr("Complex &Numbers"));
    m_menus.complexForm->setTitle(MainWindow::tr("&Form"));
    m_menus.imaginaryUnit->setTitle(MainWindow::tr("&Imaginary Unit"));
    m_menus.window->setTitle(MainWindow::tr("&Window"));
    m_menus.editing->setTitle(MainWindow::tr("&Editing"));
    m_menus.autoCompletion->setTitle(MainWindow::tr("A&utocomplete"));
    m_menus.upDownArrowBehavior->setTitle(MainWindow::tr("Up/Down Arrow History"));
    m_menus.display->setTitle(MainWindow::tr("&Appearance"));
    m_menus.help->setTitle(MainWindow::tr("&Help"));
}

void MainWindow::updateKeypadDisabledActionText()
{
    if (m_actions.viewKeypadDisabled->isChecked())
        m_actions.viewKeypadDisabled->setText(MainWindow::tr("&Disabled"));
    else
        m_actions.viewKeypadDisabled->setText(MainWindow::tr("&Disable"));
}

void MainWindow::updateKeypadModeActionState()
{
    const QSignalBlocker keypadBlocker(m_actionGroups.keypad);
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
}

void MainWindow::createStatusBar()
{
    QStatusBar* bar = statusBar();
    if (m_status.angleUnitSection != nullptr) {
        bar->show();
        setStatusBarText();
        updateStatusBarSectionVisibility();
        return;
    }

    m_status.angleUnitSection = new QWidget(bar);
    m_status.resultFormatSection = new QWidget(bar);
    m_status.resultPrecisionSection = new QWidget(bar);

    m_status.angleUnitLabel = new QLabel(m_status.angleUnitSection);
    m_status.resultFormatLabel = new QLabel(m_status.resultFormatSection);
    m_status.resultPrecisionLabel = new QLabel(m_status.resultPrecisionSection);

    m_status.angleUnit = new QPushButton(bar);
    m_status.resultFormat = new QPushButton(bar);
    m_status.resultPrecision = new QPushButton(bar);

    m_status.angleUnit->setParent(m_status.angleUnitSection);
    m_status.resultFormat->setParent(m_status.resultFormatSection);
    m_status.resultPrecision->setParent(m_status.resultPrecisionSection);

    QHBoxLayout* angleLayout = new QHBoxLayout(m_status.angleUnitSection);
    QHBoxLayout* formatLayout = new QHBoxLayout(m_status.resultFormatSection);
    QHBoxLayout* precisionLayout = new QHBoxLayout(m_status.resultPrecisionSection);
    angleLayout->setContentsMargins(0, 0, 0, 0);
    formatLayout->setContentsMargins(0, 0, 0, 0);
    precisionLayout->setContentsMargins(0, 0, 0, 0);
    angleLayout->setSpacing(2);
    formatLayout->setSpacing(2);
    precisionLayout->setSpacing(2);
    angleLayout->addWidget(m_status.angleUnitLabel);
    angleLayout->addWidget(m_status.angleUnit);
    formatLayout->addWidget(m_status.resultFormatLabel);
    formatLayout->addWidget(m_status.resultFormat);
    precisionLayout->addWidget(m_status.resultPrecisionLabel);
    precisionLayout->addWidget(m_status.resultPrecision);

    QFont boldFont = m_status.angleUnitLabel->font();
    boldFont.setBold(true);
    m_status.angleUnitLabel->setFont(boldFont);
    m_status.resultFormatLabel->setFont(boldFont);
    m_status.resultPrecisionLabel->setFont(boldFont);
    m_status.angleUnitLabel->setCursor(Qt::PointingHandCursor);
    m_status.resultFormatLabel->setCursor(Qt::PointingHandCursor);
    m_status.resultPrecisionLabel->setCursor(Qt::PointingHandCursor);
    m_status.angleUnitLabel->installEventFilter(this);
    m_status.resultFormatLabel->installEventFilter(this);
    m_status.resultPrecisionLabel->installEventFilter(this);

    m_status.angleUnit->setFocusPolicy(Qt::NoFocus);
    m_status.resultFormat->setFocusPolicy(Qt::NoFocus);
    m_status.resultPrecision->setFocusPolicy(Qt::NoFocus);

    m_status.angleUnit->setFlat(true);
    m_status.resultFormat->setFlat(true);
    m_status.resultPrecision->setFlat(true);

    m_status.angleUnit->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_status.angleUnit, SIGNAL(customContextMenuRequested(const QPoint&)),
        SLOT(showAngleModeContextMenu(const QPoint&)));

    m_status.resultFormat->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_status.resultFormat, SIGNAL(customContextMenuRequested(const QPoint&)),
        SLOT(showResultFormatContextMenu(const QPoint&)));
    m_status.resultPrecision->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_status.resultPrecision, SIGNAL(customContextMenuRequested(const QPoint&)),
        SLOT(showPrecisionContextMenu(const QPoint&)));

    connect(m_status.angleUnit, &QPushButton::clicked, this, [this]() {
        showAngleModeContextMenu(QPoint(0, m_status.angleUnit->height()));
    });
    connect(m_status.resultFormat, &QPushButton::clicked, this, [this]() {
        showResultFormatContextMenu(QPoint(0, m_status.resultFormat->height()));
    });
    connect(m_status.resultPrecision, &QPushButton::clicked, this, [this]() {
        showPrecisionContextMenu(QPoint(0, m_status.resultPrecision->height()));
    });

    bar->addWidget(m_status.resultFormatSection);
    bar->addWidget(m_status.resultPrecisionSection);
    bar->addWidget(m_status.angleUnitSection);

    for (const QPointer<MainWindow>& ptr : allMainWindows()) {
        if (MainWindow* window = ptr.data())
            window->setStatusBarText();
    }
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

    m_widgets.splitContainer = new QSplitter(Qt::Horizontal, m_widgets.root);
    m_widgets.splitContainer->setObjectName(QStringLiteral("MainSplitContainer"));
    m_widgets.splitContainer->setChildrenCollapsible(false);
    m_widgets.splitContainer->setHandleWidth(UiConfig::SessionPaneSplitterWidth);
    applyThemeBackgroundRoleToWidget(m_widgets.splitContainer,
                                     generatedSurfaceColors(m_settings).window.background);
    updateSplitterStyleSheet();
    m_layouts.root->addWidget(m_widgets.splitContainer, 1);

    m_widgets.display = new ResultDisplay();
    m_widgets.display->setFrameStyle(QFrame::NoFrame);
    m_widgets.editor = new Editor();
    m_widgets.editor->setFrameStyle(QFrame::NoFrame);
    m_widgets.editor->setFocus();
    m_widgets.editor->installEventFilter(this);
    m_widgets.editor->viewport()->installEventFilter(this);
    m_widgets.splitContainer->addWidget(createEditorDisplayPane(m_widgets.display, m_widgets.editor));
    m_paneSessionNames.insert(m_widgets.display, m_session ? m_session->name() : QString());
    m_paneSessionTabs.insert(m_widgets.display, QStringList(m_session ? m_session->name() : QString()));
    m_widgets.display->setSession(m_session);
    m_widgets.editor->setSession(m_session);

    m_widgets.state = new QLabel(this);
    m_widgets.state->setPalette(QToolTip::palette());
    m_widgets.state->setAutoFillBackground(true);
    m_widgets.state->setFrameShape(QFrame::NoFrame);
    m_widgets.state->installEventFilter(this);
    m_widgets.stateCloseButton = new QPushButton(QStringLiteral("×"), m_widgets.state);
    m_widgets.stateCloseButton->setFocusPolicy(Qt::NoFocus);
    m_widgets.stateCloseButton->setFlat(true);
    m_widgets.stateCloseButton->setToolTip(tr("Close preview"));
    m_widgets.stateCloseButton->setStyleSheet(QStringLiteral(R"(
        QPushButton {
            border: none;
            background: transparent;
            padding: 0;
            margin: 0;
            outline: none;
        }

        QPushButton:hover {
            background: transparent;
        }

        QPushButton:pressed {
            background: transparent;
        }
    )"));
    connect(m_widgets.stateCloseButton, &QPushButton::clicked, this, &MainWindow::hideStateLabel);
    m_widgets.state->hide();
}

QWidget* MainWindow::createEditorDisplayPane(ResultDisplay* display, Editor* editor)
{
    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    SessionPane* pane = new SessionPane(m_widgets.splitContainer);
    applyThemeBackgroundRoleToWidget(pane, surfaces.result.background);
    QVBoxLayout* layout = new QVBoxLayout(pane);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    SessionTabBar* tabBar = new SessionTabBar(pane);
    tabBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    QWidget* tabBarRow = new QWidget(pane);
    QHBoxLayout* tabBarRowLayout = new QHBoxLayout(tabBarRow);
    tabBarRowLayout->setSpacing(0);
    tabBarRowLayout->setContentsMargins(0, 0, 0, 0);
    tabBarRowLayout->addWidget(tabBar);
    tabBarRowLayout->addStretch(1);

    QStackedWidget* stack = new QStackedWidget(pane);
    applyThemeBackgroundRoleToWidget(stack, surfaces.result.background);
    QWidget* page = new QWidget(stack);
    applyThemeBackgroundRoleToWidget(page, surfaces.result.background);
    display->setThemeSurfaceColor(surfaces.result.background);
    display->setThemeInteractionColors(surfaces.editorAndLists.background,
                                       surfaces.headersAndBorders.background,
                                       surfaces.headersAndBorders.foreground,
                                       surfaces.inputs.background,
                                       surfaces.inputs.foreground);
    display->rehighlight();
    editor->setThemeSurfaceColor(surfaces.editorAndLists.background,
                                 surfaces.result.background);
    const ThemeSurfaceColors completionPopup =
        themeSurfaceForShadeIndex(surfaces, UiConfig::CompletionPopupBackgroundShade);
    const ThemeSurfaceColors completionScrollbarThumb =
        themeSurfaceForShadeIndex(surfaces, UiConfig::CompletionPopupScrollbarThumbShade);
    const ThemeSurfaceColors completionSelectedRow =
        themeSurfaceForShadeIndex(surfaces, UiConfig::CompletionPopupSelectedRowShade);
    const ThemeSurfaceColors completionOutline =
        themeSurfaceForShadeIndex(surfaces, UiConfig::CompletionPopupOutlineShade);
    editor->setThemeCompletionColors(completionPopup.background,
                                     completionPopup.foreground,
                                     completionScrollbarThumb.background,
                                     completionScrollbarThumb.foreground,
                                     completionSelectedRow.background,
                                     completionSelectedRow.foreground,
                                     completionOutline.background,
                                     UiConfig::CompletionPopupCornerRadius);
    editor->rehighlight();
    QVBoxLayout* pageLayout = new QVBoxLayout(page);
    pageLayout->setSpacing(0);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(display);
    pageLayout->addWidget(editor);
    stack->addWidget(page);

    layout->addWidget(tabBarRow);
    layout->addWidget(stack, 1);

    m_paneTabBars.insert(display, tabBar);
    m_tabBarDisplays.insert(tabBar, display);

    connect(tabBar, &QTabBar::currentChanged, this, [this, tabBar, display, editor](int index) {
        if (index < 0)
            return;
        pendingSessionTabActivationDisplay() = display;
        pendingSessionTabActivationEditor() = editor;
        setActiveEditorDisplayPane(display, editor, true);
        switchPaneToSession(display, tabBar->tabText(index));
        QPointer<ResultDisplay> pendingDisplay(display);
        QPointer<Editor> pendingEditor(editor);
        QTimer::singleShot(0, this, [pendingDisplay, pendingEditor]() {
            if (pendingSessionTabActivationDisplay() == pendingDisplay
                && pendingSessionTabActivationEditor() == pendingEditor) {
                pendingSessionTabActivationDisplay() = nullptr;
                pendingSessionTabActivationEditor() = nullptr;
            }
        });
        tabBar->refreshCloseButtons();
    });
    tabBar->tabActivated = [this, display, editor](const QString& sessionName) {
        pendingSessionTabActivationDisplay() = display;
        pendingSessionTabActivationEditor() = editor;
        setActiveEditorDisplayPane(display, editor, true);
        switchPaneToSession(display, sessionName);
        QPointer<ResultDisplay> pendingDisplay(display);
        QPointer<Editor> pendingEditor(editor);
        QTimer::singleShot(0, this, [pendingDisplay, pendingEditor]() {
            if (pendingSessionTabActivationDisplay() == pendingDisplay
                && pendingSessionTabActivationEditor() == pendingEditor) {
                pendingSessionTabActivationDisplay() = nullptr;
                pendingSessionTabActivationEditor() = nullptr;
            }
        });
    };
    connect(tabBar, &QTabBar::tabMoved, this, [this, tabBar, display](int, int) {
        QStringList names;
        for (int i = 0; i < tabBar->count(); ++i)
            names.append(tabBar->tabText(i));
        m_paneSessionTabs.insert(display, names);
        saveSessionLayout(false);
    });
    connect(tabBar, &QTabBar::tabBarDoubleClicked, this, [this, tabBar, display, editor](int index) {
        if (index < 0 || index >= tabBar->count())
            return;
        setActiveEditorDisplayPane(display, editor);
        switchPaneToSession(display, tabBar->tabText(index));
        showRenameSessionDialog();
    });
    tabBar->tabContextMenuRequested = [this, display, editor](const QString& sessionName, const QPoint& globalPos) {
        switchPaneToSession(display, sessionName);

        QMenu menu(this);
        QAction* newSessionAction = menu.addAction(tr("New Session"));
        QAction* openSessionAction = menu.addAction(tr("Open Session"));
        menu.addSeparator();
        QAction* splitLeftAction = menu.addAction(tr("Split Left"));
        QAction* splitRightAction = menu.addAction(tr("Split Right"));
        QAction* splitUpAction = menu.addAction(tr("Split Up"));
        QAction* splitDownAction = menu.addAction(tr("Split Down"));
        menu.addSeparator();
        menu.addAction(tr("Import Session"));
        menu.addAction(tr("Export Session"));
        menu.addSeparator();
        QAction* duplicateSessionAction = menu.addAction(tr("Duplicate Session"));
        QAction* renameSessionAction = menu.addAction(tr("Rename Session"));
        QAction* clearSessionAction = menu.addAction(tr("Clear Session"));
        QAction* deleteSessionAction = menu.addAction(tr("Delete Session"));
        QAction* closeSessionAction = menu.addAction(tr("Close Session"));
        QAction* closePaneAction = menu.addAction(tr("Close Pane"));

        const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
        applyMenuSurface(&menu, surfaces.headersAndBorders, surfaces.inputs);
        QAction* selectedAction = menu.exec(globalPos);
        if (selectedAction == nullptr)
            return;

        setActiveEditorDisplayPane(display, editor);
        if (selectedAction == newSessionAction)
            showNewSessionDialog();
        else if (selectedAction == openSessionAction)
            showOpenSessionDialog();
        else if (selectedAction == duplicateSessionAction)
            showDuplicateSessionDialog();
        else if (selectedAction == splitLeftAction)
            splitActivePaneLeft();
        else if (selectedAction == splitRightAction)
            splitActivePaneRight();
        else if (selectedAction == splitUpAction)
            splitActivePaneUp();
        else if (selectedAction == splitDownAction)
            splitActivePaneDown();
        else if (selectedAction == renameSessionAction)
            showRenameSessionDialog();
        else if (selectedAction == clearSessionAction)
            clearSession();
        else if (selectedAction == closeSessionAction)
            closeCurrentSession();
        else if (selectedAction == closePaneAction)
            closeCurrentPane();
        else if (selectedAction == deleteSessionAction)
            deleteCurrentSession();
    };
    tabBar->sessionTabDropped = [this, tabBar](SessionTabBar* sourceTabBar, const QString& sessionName, int targetIndex) {
        moveSessionTab(sourceTabBar, tabBar, sessionName, targetIndex);
    };
    tabBar->sourceMainWindow = [this]() { return this; };
    tabBar->sessionTabDetached = [this, tabBar](const QString& sessionName, const QPoint& globalPos) {
        if (sessionName.isEmpty())
            return;
        ResultDisplay* sourceDisplay = tabBarDisplay(tabBar);
        if (sourceDisplay == nullptr)
            return;
        if (!paneSessionNames(sourceDisplay).contains(sessionName, Qt::CaseInsensitive))
            return;

        const QString previousLayoutJson = m_settings->sessionLayoutJson;
        m_settings->sessionLayoutJson.clear();
        MainWindow* detachedWindow = new MainWindow();
        m_settings->sessionLayoutJson = previousLayoutJson;
        detachedWindow->show();
        detachedWindow->move(globalPos - QPoint(detachedWindow->width() / 4, 18));
        Session* sourceSession = m_loadedSessions.value(sessionName, nullptr);
        if (sourceSession == nullptr)
            return;

        QJsonObject sourceJson;
        sourceSession->serialize(sourceJson);
        sourceJson.insert(QLatin1String(SessionJsonKeys::Session), sessionName);

        const QStringList detachedNames = detachedWindow->m_loadedSessions.keys();
        for (const QString& name : detachedNames) {
            Session* sessionToDelete = detachedWindow->m_loadedSessions.take(name);
            detachedWindow->m_sessionViewportAnchors.remove(name);
            detachedWindow->m_sessionScrollValues.remove(name);
            if (sessionToDelete == detachedWindow->m_session)
                detachedWindow->m_session = nullptr;
            if (sessionToDelete != nullptr)
                delete sessionToDelete;
        }
        Session* movedSession = new Session();
        movedSession->deSerialize(sourceJson, false);
        movedSession->setName(sessionName);
        detachedWindow->m_loadedSessions.insert(sessionName, movedSession);
        detachedWindow->applyUserDefinitions();
        detachedWindow->m_paneSessionTabs.insert(detachedWindow->m_widgets.display, QStringList(sessionName));
        detachedWindow->m_paneSessionNames.insert(detachedWindow->m_widgets.display, sessionName);
        detachedWindow->m_widgets.display->setSession(movedSession);
        detachedWindow->activateSession(movedSession);
        detachedWindow->updatePaneLoadedSessionCounts();
        detachedWindow->updatePaneTabBars();

        removeSessionTabFromPane(sourceDisplay, sessionName, true);
        const auto referencedByRemainingPanes = [this](const QString& name) {
            for (ResultDisplay* display : splitPaneDisplays()) {
                if (paneSessionNames(display).contains(name, Qt::CaseInsensitive))
                    return true;
            }
            return false;
        };
        if (!referencedByRemainingPanes(sessionName)) {
            Session* sessionToDelete = m_loadedSessions.take(sessionName);
            m_sessionViewportAnchors.remove(sessionName);
            m_sessionScrollValues.remove(sessionName);
            if (sessionToDelete != nullptr && sessionToDelete != m_session)
                delete sessionToDelete;
        }
        updatePaneLoadedSessionCounts();
        updatePaneTabBars();
        detachedWindow->saveSessionLayout(false);
        saveSessionLayout(false);
    };
    tabBar->tabCloseRequested = [this, display](const QString& sessionName) {
        switchPaneToSession(display, sessionName);
        closeCurrentSession();
    };
    pane->sessionTabDroppedOnPane = [this, display](SessionTabBar* sourceTabBar, const QString& sessionName, const QPoint& panePos) {
        moveSessionTabToPane(sourceTabBar, display, sessionName, panePos);
    };
    pane->shouldShowOverlayForDrag = [this, display](SessionTabBar* sourceTabBar) {
        ResultDisplay* sourceDisplay = tabBarDisplay(sourceTabBar);
        if (sourceDisplay == nullptr || sourceDisplay != display)
            return true;
        return paneSessionNames(sourceDisplay).size() > 1;
    };
    pane->paneActivated = [this, display, editor]() {
        setActiveEditorDisplayPane(display, editor);
    };
    pane->watchDropTarget(stack);
    pane->setOverlayAreaWidget(stack);
    pane->watchDropTarget(page);
    pane->watchDropTarget(display);
    pane->watchDropTarget(display->viewport());
    pane->watchDropTarget(editor);
    pane->watchDropTarget(editor->viewport());

    return pane;
}

void MainWindow::setActiveEditorDisplayPane(ResultDisplay* display, Editor* editor, bool forceEditorFocus)
{
    if (display == nullptr || editor == nullptr)
        return;

    if (forceEditorFocus) {
        cancelWindowActivationRestore();
        pendingDockTextInputFocusTarget() = nullptr;
    }

    const bool dockWidgetHasFocus = !forceEditorFocus
        && pendingDockFocusTarget() != nullptr;
    const bool dockTextInputHasFocus = !forceEditorFocus
        && (isDockTextInput(QApplication::focusWidget())
            || pendingDockTextInputFocusTarget() != nullptr);
    if (dockWidgetHasFocus || dockTextInputHasFocus) {
        globallyActiveDisplay() = nullptr;
        globallyActiveEditor() = nullptr;
        return;
    } else {
        globallyActiveDisplay() = display;
        globallyActiveEditor() = editor;
    }
    if (activeEditorDisplayPaneActivationInProgress())
        return;

    activeEditorDisplayPaneActivationInProgress() = true;
    struct ActivationGuard {
        ~ActivationGuard()
        {
            activeEditorDisplayPaneActivationInProgress() = false;
        }
    } activationGuard;

    if (m_widgets.display != display || m_widgets.editor != editor) {
        const QString sessionName = m_paneSessionNames.value(display);
        Session* paneSession = m_loadedSessions.value(sessionName, nullptr);
        const bool switchingSession = paneSession != nullptr && paneSession != m_session;
        const bool previousSessionIsLoaded =
            m_session != nullptr && m_loadedSessions.values().contains(m_session);
        if (previousSessionIsLoaded) {
            captureEditorTextInCurrentSession();
            if (m_widgets.display != nullptr
                    && m_paneSessionNames.value(m_widgets.display) == m_session->name()) {
                m_sessionViewportAnchors.insert(m_session->name(), m_widgets.display->viewportTopAnchor());
                QScrollBar* bar = m_widgets.display->verticalScrollBar();
                const int scrollValue = bar->value() == bar->maximum()
                    ? (std::numeric_limits<int>::max)()
                    : bar->value();
                m_sessionScrollValues.insert(m_session->name(), scrollValue);
            }
        } else {
            captureEditorTextInCurrentSession();
        }
        if (switchingSession)
            m_session = nullptr;

        m_widgets.display = display;
        m_widgets.editor = editor;
        m_copyWidget = editor;

        if (paneSession != nullptr && paneSession != m_session)
            activateSession(paneSession);
    }
    if (!dockWidgetHasFocus && !dockTextInputHasFocus) {
        editor->setFocus(Qt::OtherFocusReason);
        QPointer<Editor> editorGuard(editor);
        QTimer::singleShot(0, editor, [editorGuard]() {
            if (editorGuard != nullptr
                && globallyActiveEditor() == editorGuard
                && pendingDockFocusTarget() == nullptr
                && pendingDockTextInputFocusTarget() == nullptr) {
                editorGuard->setFocus(Qt::OtherFocusReason);
            }
        });
    }
    updatePaneEditorCursorVisibility();
    updateActiveSessionPaneTabColor();
    if (!dockWidgetHasFocus && !dockTextInputHasFocus && m_widgets.state != nullptr && m_widgets.state->isVisible())
        showStateLabel(m_widgets.state->text());
}

bool MainWindow::isDockTextInput(QWidget* widget) const
{
    QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget);
    if (lineEdit == nullptr)
        return false;
    if (lineEdit->property("speedcrunchDockTextInput").toBool())
        return true;

    for (QObject* ancestor = widget; ancestor != nullptr; ancestor = ancestor->parent()) {
        QDockWidget* dock = qobject_cast<QDockWidget*>(ancestor);
        if (dock != nullptr && m_allDocks.contains(dock))
            return true;
    }

    for (QDockWidget* dock : m_allDocks) {
        if (dock != nullptr && dock->findChildren<QLineEdit*>().contains(lineEdit))
            return true;
    }

    return false;
}

bool MainWindow::isDockWidgetDescendant(QWidget* widget) const
{
    return dockWidgetForDescendant(widget) != nullptr;
}

QDockWidget* MainWindow::dockWidgetForDescendant(QWidget* widget) const
{
    if (widget == nullptr)
        return nullptr;
    for (QObject* ancestor = widget; ancestor != nullptr; ancestor = ancestor->parent()) {
        QDockWidget* dock = qobject_cast<QDockWidget*>(ancestor);
        if (dock != nullptr && m_allDocks.contains(dock))
            return dock;
    }

    return nullptr;
}

QAbstractItemView* MainWindow::dockItemViewFocusTarget(QWidget* widget) const
{
    if (widget == nullptr)
        return nullptr;

    const auto usableDockView = [this](QAbstractItemView* view) -> QAbstractItemView* {
        if (view == nullptr)
            return nullptr;
        if (!isDockWidgetDescendant(view))
            return nullptr;
        if (view->model() == nullptr || view->model()->rowCount() <= 0)
            return nullptr;
        return view;
    };

    for (QWidget* candidate = widget; candidate != nullptr; candidate = candidate->parentWidget()) {
        if (QAbstractItemView* view = usableDockView(qobject_cast<QAbstractItemView*>(candidate)))
            return view;
    }

    const QPoint globalPos = QCursor::pos();
    for (QDockWidget* dock : m_allDocks) {
        if (dock == nullptr || !dock->isVisible())
            continue;
        if (!dock->rect().contains(dock->mapFromGlobal(globalPos)))
            continue;
        for (QAbstractItemView* view : dock->findChildren<QAbstractItemView*>()) {
            if (!view->isVisible())
                continue;
            if (!view->rect().contains(view->mapFromGlobal(globalPos)))
                continue;
            if (QAbstractItemView* usableView = usableDockView(view))
                return usableView;
        }
    }

    return nullptr;
}

void MainWindow::deactivateActiveEditorForTextInputFocus()
{
    // Dock search fields and other dock text inputs are peers of the expression
    // editor. The focus event is already moving to the dock input here, so this
    // clears only the app-level active-editor state and repaint flags. Calling
    // clearFocus() on the previous editor during the transfer can make Qt fall
    // back to that editor, which would leave two text inputs competing for the
    // primary outline.
    QPointer<Editor> previousActiveEditor = globallyActiveEditor();
    if (previousActiveEditor == nullptr)
        previousActiveEditor = m_widgets.editor;
    if (previousActiveEditor == nullptr && globallyActiveDisplay() == nullptr)
        return;

    DockTextInputFocusTransferGuard focusTransferGuard;

    globallyActiveEditor() = nullptr;
    globallyActiveDisplay() = nullptr;

    if (previousActiveEditor != nullptr) {
        previousActiveEditor->setCustomCursorVisible(false);
        previousActiveEditor->setThemePrimaryColor(
            generatedSurfaceColors(m_settings).primary.background,
            false);
    }
}

void MainWindow::handleApplicationFocusChanged(QWidget* previous, QWidget* focused)
{
    if (focused != nullptr && focused->window() == this)
        lastFocusWidgetInActiveWindow() = focused;
    else if (previous != nullptr && previous->window() == this)
        lastFocusWidgetInActiveWindow() = previous;

    if (!isDockTextInput(focused))
        return;

    pendingDockTextInputFocusTarget() = focused;
    QPointer<QWidget> textInput(focused);
    QTimer::singleShot(100, focused, [textInput]() {
        if (pendingDockTextInputFocusTarget() == textInput)
            pendingDockTextInputFocusTarget() = nullptr;
    });
    deactivateActiveEditorForTextInputFocus();
}

void MainWindow::configureEditorDisplayPane(ResultDisplay* display, Editor* editor)
{
    if (display == nullptr || editor == nullptr)
        return;

    editor->installEventFilter(this);
    editor->viewport()->installEventFilter(this);

    connect(editor, &Editor::textChanged, this, [this, display, editor]() {
        // Programmatic text changes in an inactive pane must not make that pane
        // active. Real editor input activates the pane through the mouse/key/input
        // event path before the text changes.
        if (editor == m_widgets.editor
            || editor->hasFocus()
            || editor->viewport()->hasFocus()) {
            setActiveEditorDisplayPane(display, editor);
        }
    });
    connect(editor, &Editor::returnPressed, this, [this, display, editor]() {
        setActiveEditorDisplayPane(display, editor);
        evaluateEditorExpression();
    });
    connect(editor, &Editor::bulkEvaluationStarted, this, &MainWindow::handleBulkEvaluationStarted);
    connect(editor, &Editor::bulkEvaluationFinished, this, &MainWindow::handleBulkEvaluationFinished);
    connect(editor, &Editor::escapePressed, this, [this, display, editor]() {
        setActiveEditorDisplayPane(display, editor);
        handleEditorEscapePressed();
    });
    connect(editor, &Editor::selectionChanged, this, [this, display, editor]() {
        setActiveEditorDisplayPane(display, editor);
        handleEditorSelectionChange();
    });
    connect(editor, &Editor::autoCalcDisabled, this, [this, editor]() {
        if (editor == m_widgets.editor)
            hideStateLabel();
    });
    connect(editor, &Editor::autoCalcMessageAvailable, this, &MainWindow::handleAutoCalcMessageAvailable);
    connect(editor, &Editor::autoCalcQuantityAvailable, this, &MainWindow::handleAutoCalcQuantityAvailable);
    connect(editor, &Editor::shiftDownPressed, this, &MainWindow::decreaseDisplayFontPointSize);
    connect(editor, &Editor::shiftUpPressed, this, &MainWindow::increaseDisplayFontPointSize);
    connect(editor, &Editor::controlPageUpPressed, display, &ResultDisplay::scrollToTop);
    connect(editor, &Editor::controlPageDownPressed, display, &ResultDisplay::scrollToBottom);
    connect(editor, &Editor::shiftPageUpPressed, display, &ResultDisplay::scrollLineUp);
    connect(editor, &Editor::shiftPageDownPressed, display, &ResultDisplay::scrollLineDown);
    connect(editor, &Editor::pageUpPressed, display, &ResultDisplay::scrollPageUp);
    connect(editor, &Editor::pageDownPressed, display, &ResultDisplay::scrollPageDown);
    connect(editor, &Editor::textChanged, this, [this, editor]() {
        if (editor == m_widgets.editor
            || editor->hasFocus()
            || editor->viewport()->hasFocus()) {
            handleEditorTextChange();
        }
    });
    connect(editor, &Editor::copyAvailable, this, &MainWindow::handleCopyAvailable);
    connect(editor, &Editor::copySequencePressed, this, &MainWindow::copy);
    connect(this, &MainWindow::historyChanged, editor, &Editor::updateHistory);

    connect(display, &ResultDisplay::clicked, this, [this, display, editor]() {
        setActiveEditorDisplayPane(display, editor, true);
        hideStateLabel();
    });
    connect(display, &ResultDisplay::copyAvailable, this, &MainWindow::handleCopyAvailable);
    connect(display, &ResultDisplay::expressionSelected, this, [this, display, editor](const QString& text) {
        setActiveEditorDisplayPane(display, editor, true);
        insertTextIntoEditor(text);
    });
    connect(display, &ResultDisplay::editHistoryEntryRequested, this, &MainWindow::startHistoryEntryEdit);
    connect(display, &ResultDisplay::editHistoryEntryContextRequested, this, &MainWindow::editHistoryEntryContext);
    connect(display, &ResultDisplay::cancelHistoryEditRequested, this, &MainWindow::cancelHistoryEntryEdit);
    connect(display, &ResultDisplay::removeHistoryEntryRequested, this, &MainWindow::removeHistoryEntryAt);
    connect(display, &ResultDisplay::removeHistoryEntriesAboveRequested, this, &MainWindow::removeHistoryEntriesAbove);
    connect(display, &ResultDisplay::removeHistoryEntriesBelowRequested, this, &MainWindow::removeHistoryEntriesBelow);
    connect(display, &ResultDisplay::newSessionRequested, this, &MainWindow::showNewSessionDialog);
    connect(display, &ResultDisplay::openSessionRequested, this, &MainWindow::showOpenSessionDialog);
    connect(display, &ResultDisplay::duplicateSessionRequested, this, &MainWindow::showDuplicateSessionDialog);
    connect(display, &ResultDisplay::splitLeftRequested, this, &MainWindow::splitActivePaneLeft);
    connect(display, &ResultDisplay::splitRightRequested, this, &MainWindow::splitActivePaneRight);
    connect(display, &ResultDisplay::splitUpRequested, this, &MainWindow::splitActivePaneUp);
    connect(display, &ResultDisplay::splitDownRequested, this, &MainWindow::splitActivePaneDown);
    connect(display, &ResultDisplay::renameSessionRequested, this, &MainWindow::showRenameSessionDialog);
    connect(display, &ResultDisplay::clearSessionRequested, this, &MainWindow::clearSession);
    connect(display, &ResultDisplay::closeSessionRequested, this, &MainWindow::closeCurrentSession);
    connect(display, &ResultDisplay::closePaneRequested, this, &MainWindow::closeCurrentPane);
    connect(display, &ResultDisplay::deleteSessionRequested, this, &MainWindow::deleteCurrentSession);
    connect(display, &ResultDisplay::loadedSessionsMenuRequested, this, &MainWindow::showLoadedSessionsMenu);
    connect(display, &ResultDisplay::selectionChanged, this, [this, display, editor]() {
        setActiveEditorDisplayPane(display, editor, true);
        handleDisplaySelectionChange();
    });
    connect(display, &ResultDisplay::shiftWheelUp, this, &MainWindow::increaseDisplayFontPointSize);
    connect(display, &ResultDisplay::shiftWheelDown, this, &MainWindow::decreaseDisplayFontPointSize);
    connect(display, &ResultDisplay::controlWheelUp, this, &MainWindow::increaseDisplayFontPointSize);
    connect(display, &ResultDisplay::controlWheelDown, this, &MainWindow::decreaseDisplayFontPointSize);
    connect(display, &ResultDisplay::shiftControlWheelDown, this, &MainWindow::decreaseOpacity);
    connect(display, &ResultDisplay::shiftControlWheelUp, this, &MainWindow::increaseOpacity);
    connect(this, &MainWindow::historyChanged, display, &ResultDisplay::refresh);
    connect(this, &MainWindow::radixCharacterChanged, display, &ResultDisplay::refresh);
    connect(this, &MainWindow::radixCharacterChanged, editor, &Editor::refreshAutoCalc);
    connect(this, &MainWindow::angleUnitChanged, editor, &Editor::refreshAutoCalc);
    connect(this, &MainWindow::complexNumbersChanged, display, &ResultDisplay::refreshLastHistoryEntry);
    connect(this, &MainWindow::complexNumbersChanged, editor, &Editor::refreshAutoCalc);
    connect(this, &MainWindow::resultFormatChanged, display, &ResultDisplay::refreshLastHistoryEntry);
    connect(this, &MainWindow::resultFormatChanged, editor, &Editor::refreshAutoCalc);
    connect(this, &MainWindow::resultPrecisionChanged, display, &ResultDisplay::refreshLastHistoryEntry);
    connect(this, &MainWindow::resultPrecisionChanged, editor, &Editor::refreshAutoCalc);
    connect(this, &MainWindow::resultRoundingModeChanged, display, &ResultDisplay::refreshLastHistoryEntry);
    connect(this, &MainWindow::resultRoundingModeChanged, editor, &Editor::refreshAutoCalc);
    connect(this, &MainWindow::colorSchemeChanged, display, &ResultDisplay::rehighlight);
    connect(this, &MainWindow::colorSchemeChanged, editor, &Editor::rehighlight);
    connect(this, &MainWindow::syntaxHighlightingChanged, display, &ResultDisplay::rehighlight);
    connect(this, &MainWindow::syntaxHighlightingChanged, editor, &Editor::rehighlight);
}

void MainWindow::splitActivePane(Qt::Orientation orientation, bool insertAfter)
{
    if (m_widgets.splitContainer == nullptr || m_widgets.display == nullptr || m_widgets.editor == nullptr)
        return;

    QWidget* activePane = paneWidgetForDisplay(m_widgets.display);
    QSplitter* parentSplitter = qobject_cast<QSplitter*>(activePane ? activePane->parentWidget() : nullptr);
    if (parentSplitter == nullptr)
        return;

    const int activeIndex = parentSplitter->indexOf(activePane);
    if (activeIndex < 0)
        return;
    const QList<int> parentSizesBefore = parentSplitter->sizes();
    const int activeSize = activeIndex < parentSizesBefore.size()
        ? parentSizesBefore.at(activeIndex)
        : qMax(1, orientation == Qt::Horizontal ? activePane->width() : activePane->height());

    ResultDisplay* display = new ResultDisplay();
    display->setFrameStyle(QFrame::NoFrame);
    display->setFont(m_widgets.display->font());
    display->setHoverHighlightEnabled(m_settings->hoverHighlightResults);
    display->setLoadedSessionCount(1);
    display->rehighlight();

    Editor* editor = new Editor();
    editor->setFrameStyle(QFrame::NoFrame);
    editor->setFont(m_widgets.editor->font());
    editor->setAutoCalcEnabled(m_settings->autoCalc);
    editor->setAutoCompletionEnabled(m_settings->autoCompletion);
    editor->rehighlight();

    Session* newSession = createUntitledSession(false);
    editor->setText(QString());
    editor->setCursorPosition(editor->text().size());

    QWidget* pane = createEditorDisplayPane(display, editor);
    configureEditorDisplayPane(display, editor);

    QSplitter* targetSplitter = parentSplitter;
    if (parentSplitter->orientation() != orientation) {
        QSplitter* nestedSplitter = new QSplitter(orientation);
        nestedSplitter->setChildrenCollapsible(false);
        nestedSplitter->setHandleWidth(UiConfig::SessionPaneSplitterWidth);
        nestedSplitter->setStyleSheet(m_widgets.splitContainer->styleSheet());
        activePane->setParent(nullptr);
        parentSplitter->insertWidget(activeIndex, nestedSplitter);
        if (insertAfter) {
            nestedSplitter->addWidget(activePane);
            nestedSplitter->addWidget(pane);
        } else {
            nestedSplitter->addWidget(pane);
            nestedSplitter->addWidget(activePane);
        }
        targetSplitter = nestedSplitter;
        if (parentSizesBefore.size() == parentSplitter->count())
            parentSplitter->setSizes(parentSizesBefore);
    } else {
        const int insertIndex = insertAfter ? activeIndex + 1 : activeIndex;
        parentSplitter->insertWidget(insertIndex, pane);
    }
    if (newSession != nullptr)
        m_paneSessionNames.insert(display, newSession->name());
    if (newSession != nullptr)
        m_paneSessionTabs.insert(display, QStringList(newSession->name()));
    display->setSession(newSession);
    if (newSession != nullptr)
        editor->setSession(newSession);

    const int firstHalf = qMax(1, activeSize / 2);
    const int secondHalf = qMax(1, activeSize - firstHalf);
    if (targetSplitter == parentSplitter) {
        QList<int> sizes = parentSizesBefore;
        if (activeIndex < sizes.size()) {
            sizes[activeIndex] = insertAfter ? firstHalf : secondHalf;
            sizes.insert(insertAfter ? activeIndex + 1 : activeIndex,
                         insertAfter ? secondHalf : firstHalf);
            if (sizes.size() == targetSplitter->count())
                targetSplitter->setSizes(sizes);
        }
    } else {
        targetSplitter->setSizes(insertAfter
            ? QList<int>({ firstHalf, secondHalf })
            : QList<int>({ firstHalf, secondHalf }));
    }

    display->refresh();
    editor->updateHistory();
    editor->refreshAutoCalc();
    updateSplitterStyleSheet();
    updatePaneLoadedSessionCounts();
    cancelWindowActivationRestore();
    setActiveEditorDisplayPane(display, editor);
    saveSessionLayout();
}

void MainWindow::splitActivePaneLeft()
{
    splitActivePane(Qt::Horizontal, false);
}

void MainWindow::splitActivePaneRight()
{
    splitActivePane(Qt::Horizontal, true);
}

void MainWindow::splitActivePaneUp()
{
    splitActivePane(Qt::Vertical, false);
}

void MainWindow::splitActivePaneDown()
{
    splitActivePane(Qt::Vertical, true);
}

void MainWindow::activateNextChild()
{
    if (m_widgets.display == nullptr)
        return;

    QTabBar* currentTabBar = displayTabBar(m_widgets.display);
    if (currentTabBar != nullptr && currentTabBar->count() > 0) {
        const int currentIndex = currentTabBar->currentIndex();
        if (currentIndex >= 0 && currentIndex + 1 < currentTabBar->count()) {
            currentTabBar->setCurrentIndex(currentIndex + 1);
            return;
        }
    }

    const QList<ResultDisplay*> displays = splitPaneDisplays();
    const int currentPaneIndex = displays.indexOf(m_widgets.display);
    if (currentPaneIndex < 0 || currentPaneIndex + 1 >= displays.size())
        return;

    ResultDisplay* nextDisplay = displays.at(currentPaneIndex + 1);
    QWidget* page = nextDisplay->parentWidget();
    Editor* nextEditor = page ? page->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    if (nextEditor == nullptr)
        return;

    setActiveEditorDisplayPane(nextDisplay, nextEditor);
    const QString sessionName = m_paneSessionNames.value(nextDisplay);
    if (!sessionName.isEmpty())
        switchPaneToSession(nextDisplay, sessionName);
}

void MainWindow::activatePreviousChild()
{
    if (m_widgets.display == nullptr)
        return;

    QTabBar* currentTabBar = displayTabBar(m_widgets.display);
    if (currentTabBar != nullptr && currentTabBar->count() > 0) {
        const int currentIndex = currentTabBar->currentIndex();
        if (currentIndex > 0) {
            currentTabBar->setCurrentIndex(currentIndex - 1);
            return;
        }
    }

    const QList<ResultDisplay*> displays = splitPaneDisplays();
    const int currentPaneIndex = displays.indexOf(m_widgets.display);
    if (currentPaneIndex <= 0)
        return;

    ResultDisplay* previousDisplay = displays.at(currentPaneIndex - 1);
    QWidget* page = previousDisplay->parentWidget();
    Editor* previousEditor = page ? page->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    if (previousEditor == nullptr)
        return;

    setActiveEditorDisplayPane(previousDisplay, previousEditor);
    const QString sessionName = m_paneSessionNames.value(previousDisplay);
    if (!sessionName.isEmpty())
        switchPaneToSession(previousDisplay, sessionName);
}

Session* MainWindow::createUntitledSession(bool activateCreatedSession)
{
    Session* recycledSession = nullptr;
    int recycledNumber = std::numeric_limits<int>::max();
    for (auto it = m_loadedSessions.constBegin(); it != m_loadedSessions.constEnd(); ++it) {
        const QString name = it.key();
        const int number = untitledSessionNumber(name);
        if (number <= 0 || number >= recycledNumber)
            continue;
        if (!isReusableUntitledSession(it.value()))
            continue;

        bool alreadyAttachedToPane = false;
        for (auto paneIt = m_paneSessionTabs.constBegin(); paneIt != m_paneSessionTabs.constEnd(); ++paneIt) {
            if (paneIt.value().contains(name, Qt::CaseInsensitive)) {
                alreadyAttachedToPane = true;
                break;
            }
        }
        if (alreadyAttachedToPane)
            continue;

        recycledSession = it.value();
        recycledNumber = number;
    }

    if (recycledSession != nullptr) {
        applyUserDefinitions();
        if (activateCreatedSession)
            activateSession(recycledSession);
        return recycledSession;
    }

    const QString name = firstAvailableUntitledSessionName(m_loadedSessions);
    Session* session = new Session();
    session->setName(name);
    m_loadedSessions.insert(name, session);
    updatePaneLoadedSessionCounts();

    applyUserDefinitions();
    if (activateCreatedSession)
        activateSession(session);
    return session;
}

QList<ResultDisplay*> MainWindow::splitPaneDisplays() const
{
    QList<ResultDisplay*> displays;
    if (m_widgets.splitContainer == nullptr)
        return displays;

    const auto collectDisplays = [&displays](QWidget* widget, const auto& collectDisplaysRef) -> void {
        if (widget == nullptr)
            return;
        if (ResultDisplay* display = qobject_cast<ResultDisplay*>(widget)) {
            displays.append(display);
            return;
        }
        if (QSplitter* splitter = qobject_cast<QSplitter*>(widget)) {
            for (int i = 0; i < splitter->count(); ++i)
                collectDisplaysRef(splitter->widget(i), collectDisplaysRef);
            return;
        }
        const QList<ResultDisplay*> childDisplays = widget->findChildren<ResultDisplay*>();
        for (ResultDisplay* display : childDisplays)
            displays.append(display);
    };
    collectDisplays(m_widgets.splitContainer, collectDisplays);
    return displays;
}

QList<Editor*> MainWindow::splitPaneEditors() const
{
    QList<Editor*> editors;
    for (ResultDisplay* display : splitPaneDisplays()) {
        QWidget* pane = display->parentWidget();
        if (Editor* editor = pane ? pane->findChild<Editor*>() : nullptr)
            editors.append(editor);
    }
    return editors;
}

QStringList MainWindow::paneSessionNames(ResultDisplay* display) const
{
    QStringList names = m_paneSessionTabs.value(display);
    if (names.isEmpty()) {
        const QString activeName = m_paneSessionNames.value(display);
        if (!activeName.isEmpty())
            names.append(activeName);
    }
    return names;
}

void MainWindow::addSessionToActivePane(const QString& name)
{
    if (m_widgets.display == nullptr || name.isEmpty())
        return;

    QStringList names = paneSessionNames(m_widgets.display);
    const bool tabAdded = !names.contains(name, Qt::CaseInsensitive);
    const bool activeChanged = m_paneSessionNames.value(m_widgets.display).compare(name, Qt::CaseInsensitive) != 0;
    if (tabAdded)
        names.append(name);
    if (!tabAdded && !activeChanged)
        return;

    m_paneSessionTabs.insert(m_widgets.display, names);
    m_paneSessionNames.insert(m_widgets.display, name);
    updatePaneLoadedSessionCounts();
}

ResultDisplay* MainWindow::tabBarDisplay(QTabBar* tabBar) const
{
    return m_tabBarDisplays.value(tabBar, nullptr);
}

QTabBar* MainWindow::displayTabBar(ResultDisplay* display) const
{
    return m_paneTabBars.value(display, nullptr);
}

void MainWindow::switchPaneToSession(ResultDisplay* display, const QString& name)
{
    if (m_shutdownStateSaved)
        return;
    if (display == nullptr || name.isEmpty())
        return;
    if (!m_paneTabBars.contains(display))
        return;

    Session* session = m_loadedSessions.value(name, nullptr);
    if (session == nullptr)
        return;

    QWidget* page = display->parentWidget();
    Editor* editor = page ? page->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    if (editor == nullptr)
        return;

    const QString currentName = m_paneSessionNames.value(display);
    if (!currentName.isEmpty()
        && currentName.compare(name, Qt::CaseInsensitive) != 0) {
        if (Session* currentSession = m_loadedSessions.value(currentName, nullptr))
            currentSession->setEditorText(editor->text());
    }

    const bool paneAlreadyShowsSession =
        display->session() == session
        && m_paneSessionNames.value(display).compare(name, Qt::CaseInsensitive) == 0;

    setActiveEditorDisplayPane(display, editor);
    activateSession(session);
    if (!paneAlreadyShowsSession) {
        editor->setText(session->editorText());
        editor->setCursorPosition(editor->text().size());
    }
    if (!paneAlreadyShowsSession)
        updatePaneTabBars();
}

bool MainWindow::focusOpenSession(const QString& name)
{
    if (name.isEmpty())
        return false;

    QList<QPointer<MainWindow>> candidateWindows;
    candidateWindows.append(QPointer<MainWindow>(this));
    for (const QPointer<MainWindow>& window : allMainWindows()) {
        if (window != nullptr && window != this)
            candidateWindows.append(window);
    }

    for (const QPointer<MainWindow>& window : candidateWindows) {
        if (window == nullptr)
            continue;

        for (ResultDisplay* display : window->splitPaneDisplays()) {
            if (!window->paneSessionNames(display).contains(name, Qt::CaseInsensitive))
                continue;

            QWidget* page = display->parentWidget();
            Editor* editor = page ? page->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
            if (editor == nullptr)
                continue;

            window->setActiveEditorDisplayPane(display, editor);
            window->switchPaneToSession(display, name);
            if (window->isMinimized())
                window->showNormal();
            window->raise();
            window->activateWindow();
            return true;
        }
    }

    return false;
}

void MainWindow::moveSessionTab(QTabBar* sourceTabBar, QTabBar* targetTabBar, const QString& name, int targetIndex)
{
    if (m_shutdownStateSaved)
        return;
    ResultDisplay* sourceDisplay = tabBarDisplay(sourceTabBar);
    ResultDisplay* targetDisplay = tabBarDisplay(targetTabBar);
    if (targetDisplay == nullptr || name.isEmpty())
        return;
    MainWindow* sourceWindow = this;
    if (sourceDisplay == nullptr && sourceTabBar != nullptr)
        sourceWindow = qobject_cast<MainWindow*>(sourceTabBar->window());
    if (sourceDisplay == nullptr && sourceWindow != nullptr)
        sourceDisplay = sourceWindow->tabBarDisplay(sourceTabBar);
    if (sourceDisplay == nullptr || sourceWindow == nullptr)
        return;

    const bool crossWindowMove = sourceWindow != this;
    if (!crossWindowMove && !m_loadedSessions.contains(name))
        return;
    if (crossWindowMove && !sourceWindow->m_loadedSessions.contains(name))
        return;

    QStringList sourceNames = crossWindowMove
        ? sourceWindow->paneSessionNames(sourceDisplay)
        : paneSessionNames(sourceDisplay);
    if (!sourceNames.contains(name, Qt::CaseInsensitive))
        return;
    QStringList targetNames = paneSessionNames(targetDisplay);
    if (crossWindowMove) {
        Session* sourceSession = sourceWindow->m_loadedSessions.value(name, nullptr);
        if (sourceSession == nullptr)
            return;

        QJsonObject movedJson;
        sourceSession->serialize(movedJson);
        movedJson.insert(QLatin1String(SessionJsonKeys::Session), name);
        Session* movedSession = new Session();
        movedSession->deSerialize(movedJson, false);
        movedSession->setName(name);
        m_loadedSessions.insert(name, movedSession);
        applyUserDefinitions();

        if (!targetNames.contains(name, Qt::CaseInsensitive))
            targetNames.insert(qBound(0, targetIndex, targetNames.size()), name);
        m_paneSessionTabs.insert(targetDisplay, targetNames);
        m_paneSessionNames.insert(targetDisplay, name);
        switchPaneToSession(targetDisplay, name);

        sourceWindow->removeSessionTabFromPane(sourceDisplay, name, true);
        const auto referencedByRemainingPanes = [sourceWindow](const QString& sessionName) {
            for (ResultDisplay* display : sourceWindow->splitPaneDisplays()) {
                if (sourceWindow->paneSessionNames(display).contains(sessionName, Qt::CaseInsensitive))
                    return true;
            }
            return false;
        };
        if (!referencedByRemainingPanes(name)) {
            Session* removedSession = sourceWindow->m_loadedSessions.take(name);
            sourceWindow->m_sessionViewportAnchors.remove(name);
            sourceWindow->m_sessionScrollValues.remove(name);
            if (removedSession != nullptr && removedSession != sourceWindow->m_session)
                delete removedSession;
        }

        sourceWindow->updatePaneLoadedSessionCounts();
        sourceWindow->updatePaneTabBars();
        sourceWindow->saveSessionLayout(false);
        updatePaneLoadedSessionCounts();
        updatePaneTabBars();
        saveSessionLayout(false);
        return;
    }

    if (sourceDisplay == targetDisplay) {
        const int sourceIndex = sourceNames.indexOf(name);
        sourceNames.removeAll(name);
        if (sourceIndex >= 0 && sourceIndex < targetIndex)
            --targetIndex;
        sourceNames.insert(qBound(0, targetIndex, sourceNames.size()), name);
        m_paneSessionTabs.insert(sourceDisplay, sourceNames);
        updatePaneTabBars();
        saveSessionLayout(false);
        return;
    }

    sourceNames.removeAll(name);
    if (!targetNames.contains(name, Qt::CaseInsensitive))
        targetNames.insert(qBound(0, targetIndex, targetNames.size()), name);

    if (sourceNames.isEmpty()) {
        if (splitPaneDisplays().size() > 1) {
            m_paneSessionTabs.insert(targetDisplay, targetNames);
            m_paneSessionNames.insert(targetDisplay, name);
            removePaneForDisplay(sourceDisplay);
            switchPaneToSession(targetDisplay, name);
            updatePaneLoadedSessionCounts();
            updatePaneTabBars();
            saveSessionLayout(false);
            return;
        }

        Session* replacement = createUntitledSession(false);
        if (replacement != nullptr)
            sourceNames.append(replacement->name());
    }

    m_paneSessionTabs.insert(sourceDisplay, sourceNames);
    if (m_paneSessionNames.value(sourceDisplay).compare(name, Qt::CaseInsensitive) == 0) {
        m_paneSessionNames.insert(sourceDisplay, sourceNames.first());
        if (Session* sourceSession = m_loadedSessions.value(sourceNames.first(), nullptr)) {
            sourceDisplay->setSession(sourceSession);
            sourceDisplay->refresh();
            QWidget* page = sourceDisplay->parentWidget();
            if (Editor* sourceEditor = page ? page->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr) {
                sourceEditor->setSession(sourceSession);
                sourceEditor->setText(sourceSession->editorText());
                sourceEditor->setCursorPosition(sourceEditor->text().size());
                sourceEditor->updateHistory();
                sourceEditor->refreshAutoCalc();
            }
        }
    }
    m_paneSessionTabs.insert(targetDisplay, targetNames);
    m_paneSessionNames.insert(targetDisplay, name);

    switchPaneToSession(targetDisplay, name);
    updatePaneLoadedSessionCounts();
    updatePaneTabBars();
    saveSessionLayout(false);
}

void MainWindow::removeSessionTabFromPane(ResultDisplay* display, const QString& name, bool closePaneIfEmpty)
{
    if (display == nullptr || name.isEmpty())
        return;

    QStringList names = paneSessionNames(display);
    if (!names.contains(name, Qt::CaseInsensitive))
        return;

    names.removeAll(name);
    if (names.isEmpty()) {
        if (closePaneIfEmpty) {
            removePaneForDisplay(display);
            return;
        }

        Session* replacement = createUntitledSession(false);
        if (replacement != nullptr)
            names.append(replacement->name());
    }

    m_paneSessionTabs.insert(display, names);
    if (m_paneSessionNames.value(display).compare(name, Qt::CaseInsensitive) == 0) {
        m_paneSessionNames.insert(display, names.first());
        if (Session* session = m_loadedSessions.value(names.first(), nullptr)) {
            QWidget* page = display->parentWidget();
            if (Editor* editor = page ? page->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr) {
                // Removing a tab can switch the visible pane to another
                // session without changing keyboard focus. If this is the
                // active pane, update MainWindow::m_session too; otherwise the
                // window-level symbol docks keep reading the removed session
                // until the pane receives a click/focus event.
                if (display == m_widgets.display && editor == m_widgets.editor) {
                    activateSession(session);
                } else {
                    display->setSession(session);
                    display->refresh();
                    editor->setText(session->editorText());
                    editor->setCursorPosition(editor->text().size());
                    editor->updateHistory();
                    editor->refreshAutoCalc();
                }
            }
        }
    }
}

void MainWindow::removePaneForDisplay(ResultDisplay* display)
{
    QWidget* pane = paneWidgetForDisplay(display);
    if (display == nullptr || pane == nullptr)
        return;

    const QList<ResultDisplay*> displays = splitPaneDisplays();
    if (displays.size() <= 1)
        return;

    ResultDisplay* nextDisplay = nullptr;
    const int displayIndex = displays.indexOf(display);
    if (displayIndex >= 0 && displayIndex + 1 < displays.size())
        nextDisplay = displays.at(displayIndex + 1);
    else if (displayIndex > 0)
        nextDisplay = displays.at(displayIndex - 1);
    else {
        for (ResultDisplay* candidate : displays) {
            if (candidate != display) {
                nextDisplay = candidate;
                break;
            }
        }
    }
    if (nextDisplay == nullptr)
        return;

    Editor* nextEditor = nextDisplay->parentWidget()
        ? nextDisplay->parentWidget()->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly)
        : nullptr;
    if (nextEditor == nullptr)
        return;

    QTabBar* tabBar = displayTabBar(display);
    m_paneSessionNames.remove(display);
    m_paneSessionTabs.remove(display);
    m_paneTabBars.remove(display);
    if (tabBar != nullptr)
        m_tabBarDisplays.remove(tabBar);

    if (m_widgets.display == display) {
        m_widgets.display = nextDisplay;
        m_widgets.editor = nextEditor;
        m_copyWidget = nextEditor;
        if (Session* nextSession = m_loadedSessions.value(m_paneSessionNames.value(nextDisplay), nullptr))
            activateSession(nextSession);
    }

    // Tab drags run a nested event loop. Defer physical pane destruction until
    // the drag unwinds so source/target widgets cannot disappear mid-event.
    deletePaneAfterSessionTabDrag(pane);
    normalizeSplitContainerTree();
}

void MainWindow::moveSessionTabToPane(QTabBar* sourceTabBar, ResultDisplay* targetDisplay, const QString& name, const QPoint& panePos)
{
    if (sourceTabBar == nullptr || targetDisplay == nullptr || name.isEmpty())
        return;
    MainWindow* sourceWindow = qobject_cast<MainWindow*>(sourceTabBar->window());
    ResultDisplay* sourceDisplay = tabBarDisplay(sourceTabBar);
    if (sourceDisplay == nullptr && sourceWindow != nullptr)
        sourceDisplay = sourceWindow->tabBarDisplay(sourceTabBar);
    if (sourceDisplay == nullptr || sourceWindow == nullptr)
        return;

    QWidget* targetPane = paneWidgetForDisplay(targetDisplay);
    if (targetPane == nullptr)
        return;
    PaneDropZone zone = PaneDropZone::Center;
    if (SessionPane* sessionPane = dynamic_cast<SessionPane*>(targetPane))
        zone = sessionPane->dropZoneForPanePosition(panePos);

    const bool crossWindowMove = (sourceWindow != this);
    if (crossWindowMove) {
        QTabBar* targetTabBar = displayTabBar(targetDisplay);
        if (targetTabBar == nullptr)
            return;
        const int insertIndex = paneSessionNames(targetDisplay).size();
        moveSessionTab(sourceTabBar, targetTabBar, name, insertIndex);
        if (zone == PaneDropZone::Center)
            return;
        QTabBar* localTargetTabBar = displayTabBar(targetDisplay);
        ResultDisplay* localSourceDisplay = localTargetTabBar ? tabBarDisplay(localTargetTabBar) : nullptr;
        if (localTargetTabBar == nullptr || localSourceDisplay == nullptr)
            return;
        switch (zone) {
        case PaneDropZone::Top:
            splitPaneWithSession(localTargetTabBar, targetDisplay, name, Qt::Vertical, false);
            return;
        case PaneDropZone::Bottom:
            splitPaneWithSession(localTargetTabBar, targetDisplay, name, Qt::Vertical, true);
            return;
        case PaneDropZone::Left:
            splitPaneWithSession(localTargetTabBar, targetDisplay, name, Qt::Horizontal, false);
            return;
        case PaneDropZone::Right:
            splitPaneWithSession(localTargetTabBar, targetDisplay, name, Qt::Horizontal, true);
            return;
        case PaneDropZone::Center:
            return;
        }
    }

    switch (zone) {
    case PaneDropZone::Center:
        if (sourceDisplay == targetDisplay)
            return;
        if (sourceWindow != this) {
            if (QTabBar* targetTabBar = displayTabBar(targetDisplay))
                moveSessionTab(sourceTabBar, targetTabBar, name, paneSessionNames(targetDisplay).size());
            return;
        }
        {
            QStringList targetNames = paneSessionNames(targetDisplay);
            if (!targetNames.contains(name, Qt::CaseInsensitive))
                targetNames.append(name);
            m_paneSessionTabs.insert(targetDisplay, targetNames);
            m_paneSessionNames.insert(targetDisplay, name);
            removeSessionTabFromPane(sourceDisplay, name, true);
            switchPaneToSession(targetDisplay, name);
            updatePaneLoadedSessionCounts();
            saveSessionLayout(false);
        }
        break;
    case PaneDropZone::Top:
        splitPaneWithSession(sourceTabBar, targetDisplay, name, Qt::Vertical, false);
        break;
    case PaneDropZone::Bottom:
        splitPaneWithSession(sourceTabBar, targetDisplay, name, Qt::Vertical, true);
        break;
    case PaneDropZone::Left:
        splitPaneWithSession(sourceTabBar, targetDisplay, name, Qt::Horizontal, false);
        break;
    case PaneDropZone::Right:
        splitPaneWithSession(sourceTabBar, targetDisplay, name, Qt::Horizontal, true);
        break;
    }
}

void MainWindow::splitPaneWithSession(QTabBar* sourceTabBar, ResultDisplay* targetDisplay, const QString& name, Qt::Orientation orientation, bool insertAfter)
{
    ResultDisplay* sourceDisplay = tabBarDisplay(sourceTabBar);
    Session* session = m_loadedSessions.value(name, nullptr);
    if (sourceDisplay == nullptr || targetDisplay == nullptr || session == nullptr)
        return;

    QStringList sourceNames = paneSessionNames(sourceDisplay);
    if (!sourceNames.contains(name, Qt::CaseInsensitive))
        return;

    QWidget* activePane = paneWidgetForDisplay(targetDisplay);
    QSplitter* parentSplitter = qobject_cast<QSplitter*>(activePane ? activePane->parentWidget() : nullptr);
    if (parentSplitter == nullptr)
        return;

    Editor* targetEditor = targetDisplay->parentWidget()
        ? targetDisplay->parentWidget()->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly)
        : nullptr;
    if (targetEditor == nullptr)
        return;

    const int activeIndex = parentSplitter->indexOf(activePane);
    if (activeIndex < 0)
        return;
    const QList<int> parentSizesBefore = parentSplitter->sizes();
    const int activeSize = activeIndex < parentSizesBefore.size()
        ? parentSizesBefore.at(activeIndex)
        : qMax(1, orientation == Qt::Horizontal ? activePane->width() : activePane->height());

    ResultDisplay* display = new ResultDisplay();
    display->setFrameStyle(QFrame::NoFrame);
    display->setFont(targetDisplay->font());
    display->setHoverHighlightEnabled(m_settings->hoverHighlightResults);
    display->setLoadedSessionCount(1);
    display->rehighlight();

    Editor* editor = new Editor();
    editor->setFrameStyle(QFrame::NoFrame);
    editor->setFont(targetEditor->font());
    editor->setAutoCalcEnabled(m_settings->autoCalc);
    editor->setAutoCompletionEnabled(m_settings->autoCompletion);
    editor->setText(session->editorText());
    editor->setCursorPosition(editor->text().size());
    editor->rehighlight();

    QWidget* pane = createEditorDisplayPane(display, editor);
    configureEditorDisplayPane(display, editor);

    QSplitter* targetSplitter = parentSplitter;
    if (parentSplitter->orientation() != orientation) {
        QSplitter* nestedSplitter = new QSplitter(orientation);
        nestedSplitter->setChildrenCollapsible(false);
        nestedSplitter->setHandleWidth(UiConfig::SessionPaneSplitterWidth);
        nestedSplitter->setStyleSheet(m_widgets.splitContainer->styleSheet());
        activePane->setParent(nullptr);
        parentSplitter->insertWidget(activeIndex, nestedSplitter);
        if (insertAfter) {
            nestedSplitter->addWidget(activePane);
            nestedSplitter->addWidget(pane);
        } else {
            nestedSplitter->addWidget(pane);
            nestedSplitter->addWidget(activePane);
        }
        targetSplitter = nestedSplitter;
        if (parentSizesBefore.size() == parentSplitter->count())
            parentSplitter->setSizes(parentSizesBefore);
    } else {
        const int insertIndex = insertAfter ? activeIndex + 1 : activeIndex;
        parentSplitter->insertWidget(insertIndex, pane);
    }

    m_paneSessionNames.insert(display, name);
    m_paneSessionTabs.insert(display, QStringList(name));
    display->setSession(session);
    editor->setSession(session);

    const int firstHalf = qMax(1, activeSize / 2);
    const int secondHalf = qMax(1, activeSize - firstHalf);
    if (targetSplitter == parentSplitter) {
        QList<int> sizes = parentSizesBefore;
        if (activeIndex < sizes.size()) {
            sizes[activeIndex] = insertAfter ? firstHalf : secondHalf;
            sizes.insert(insertAfter ? activeIndex + 1 : activeIndex,
                         insertAfter ? secondHalf : firstHalf);
            if (sizes.size() == targetSplitter->count())
                targetSplitter->setSizes(sizes);
        }
    } else {
        targetSplitter->setSizes(QList<int>({ firstHalf, secondHalf }));
    }

    display->refresh();
    editor->updateHistory();
    editor->refreshAutoCalc();
    removeSessionTabFromPane(sourceDisplay, name, true);
    updateSplitterStyleSheet();
    updatePaneLoadedSessionCounts();
    setActiveEditorDisplayPane(display, editor);
    updatePaneTabBars();
    saveSessionLayout(false);
}

void MainWindow::updatePaneLoadedSessionCounts()
{
    const bool multiplePanes = splitPaneDisplays().size() > 1;
    for (ResultDisplay* display : splitPaneDisplays()) {
        display->setLoadedSessionCount(paneSessionNames(display).size());
        display->setCloseSessionEnabled(multiplePanes || paneSessionNames(display).size() > 1);
    }
    updatePaneTabBars();
}

void MainWindow::updatePaneEditorCursorVisibility()
{
    const QPointer<ResultDisplay> activeDisplay = globallyActiveDisplay();
    const QPointer<Editor> activeEditor = globallyActiveEditor();
    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    QSet<MainWindow*> updatedWindows;
    for (const QPointer<MainWindow>& ptr : allMainWindows()) {
        MainWindow* window = ptr.data();
        if (window == nullptr || updatedWindows.contains(window))
            continue;

        updatedWindows.insert(window);
        QSet<Editor*> updatedEditors;
        for (ResultDisplay* display : window->splitPaneDisplays()) {
            QWidget* page = display->parentWidget();
            Editor* editor = page
                ? page->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly)
                : nullptr;
            const bool active = activeDisplay != nullptr
                && activeEditor != nullptr
                && display == activeDisplay
                && editor == activeEditor;
            if (editor != nullptr) {
                updatedEditors.insert(editor);
                editor->setCustomCursorVisible(active);
                editor->setThemePrimaryColor(surfaces.primary.background, active);
            }
        }
        for (Editor* editor : window->splitPaneEditors()) {
            if (editor == nullptr || updatedEditors.contains(editor))
                continue;
            editor->setCustomCursorVisible(false);
            editor->setThemePrimaryColor(surfaces.primary.background, false);
            editor->rehighlight();
        }
        window->updateActiveSessionPaneTabColor();
    }
}

void MainWindow::updateActiveSessionPaneTabColor()
{
    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    const QList<ResultDisplay*> displays = splitPaneDisplays();
    for (ResultDisplay* display : displays) {
        QTabBar* tabBar = displayTabBar(display);
        SessionTabBar* sessionTabBar = dynamic_cast<SessionTabBar*>(tabBar);
        if (sessionTabBar == nullptr)
            continue;
        sessionTabBar->setActivePaneSelectedTabIndicatorColor(QColor());
    }
    if (displays.size() <= 1)
        return;
    SessionTabBar* activeTabBar =
        dynamic_cast<SessionTabBar*>(displayTabBar(m_widgets.display));
    if (activeTabBar != nullptr)
        activeTabBar->setActivePaneSelectedTabIndicatorColor(surfaces.primary.background);
}

void MainWindow::updatePaneTabBars()
{
    const QList<ResultDisplay*> displays = splitPaneDisplays();
    const bool singlePaneSingleTab = displays.size() == 1 && paneSessionNames(displays.first()).size() == 1;
    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    const ThemeSurfaceColors selectedSessionTab =
        themeSurfaceForShadeIndex(surfaces, UiConfig::SelectedSessionTabFillShade);
    const ThemeSurfaceColors hoveredSessionTab =
        themeSurfaceForShadeIndex(surfaces, UiConfig::HoveredSessionTabFillShade);
    const ThemeSurfaceColors closeButtonHover =
        themeSurfaceForShadeIndex(surfaces, UiConfig::SessionTabCloseButtonHoverFillShade);
    for (ResultDisplay* display : displays) {
        QTabBar* tabBar = displayTabBar(display);
        if (tabBar == nullptr)
            continue;
        static_cast<SessionTabBar*>(tabBar)->applyStyle(
            selectedSessionTab.foreground,
            selectedSessionTab.background,
            hoveredSessionTab.foreground,
            hoveredSessionTab.background,
            surfaces.window.background,
            surfaces.window.foreground,
            closeButtonHover.foreground,
            closeButtonHover.background);

        const QSignalBlocker blocker(tabBar);
        const QStringList names = paneSessionNames(display);
        bool tabsAlreadyMatch = tabBar->count() == names.size();
        for (int i = 0; tabsAlreadyMatch && i < names.size(); ++i)
            tabsAlreadyMatch = tabBar->tabText(i) == names.at(i);
        if (!tabsAlreadyMatch) {
            while (tabBar->count() > 0)
                tabBar->removeTab(0);
            for (const QString& name : names)
                tabBar->addTab(name);
        }

        const int activeIndex = names.indexOf(m_paneSessionNames.value(display));
        tabBar->setCurrentIndex(activeIndex >= 0 ? activeIndex : 0);
        // tabBar->setUsesScrollButtons(names.size() > 4);
        tabBar->setVisible(!singlePaneSingleTab);
        static_cast<SessionTabBar*>(tabBar)->refreshCloseButtons();
    }
    updateSessionWindowTitle();
    updateActiveSessionPaneTabColor();
}

void MainWindow::updateSessionWindowTitle()
{
    const QList<ResultDisplay*> displays = splitPaneDisplays();
    if (displays.size() == 1) {
        const QStringList names = paneSessionNames(displays.first());
        if (names.size() == 1) {
            setWindowTitle(tr("SpeedCrunch - %1").arg(names.first()));
            return;
        }
    }

    setWindowTitle(QStringLiteral("SpeedCrunch"));
}

void MainWindow::normalizeSplitContainerTree()
{
    normalizeSplitterTree(m_widgets.splitContainer);
}

void MainWindow::updateSplitterStyleSheet()
{
    if (m_widgets.splitContainer == nullptr)
        return;

    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    const QColor handle = themeSurfaceForShadeIndex(surfaces, UiConfig::SplitterShade).background;
    const QColor hoveredHandle = UiConfig::SplitterHoverUsesPrimary
        ? surfaces.primary.background
        : themeSurfaceForShadeIndex(surfaces, UiConfig::SplitterHoverShade).background;
    setProperty(DockSeparatorNormalColorProperty, handle);
    setProperty(DockSeparatorActiveColorProperty, hoveredHandle);

    const QString styleSheet = splitterStyleSheet(handle, hoveredHandle);
    const auto applyStyle = [&styleSheet](QSplitter* splitter, const auto& applyStyleRef) -> void {
        if (splitter == nullptr)
            return;
        splitter->setStyleSheet(styleSheet);
        for (int i = 0; i < splitter->count(); ++i) {
            if (QSplitter* childSplitter = qobject_cast<QSplitter*>(splitter->widget(i)))
                applyStyleRef(childSplitter, applyStyleRef);
        }
    };
    applyStyle(m_widgets.splitContainer, applyStyle);

    const QString handleStyleSheet = splitterHandleStyleSheet(handle, hoveredHandle);
    for (QSplitterHandle* splitterHandle : findChildren<QSplitterHandle*>()) {
        splitterHandle->installEventFilter(this);
        splitterHandle->setStyleSheet(handleStyleSheet);
        splitterHandle->update();
    }
}

void MainWindow::refreshPaneThemes()
{
    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    applyThemeBackgroundRoleToWidget(m_widgets.splitContainer, surfaces.window.background);
    for (ResultDisplay* display : splitPaneDisplays()) {
        display->setThemeSurfaceColor(surfaces.result.background);
        display->setThemeInteractionColors(surfaces.editorAndLists.background,
                                           surfaces.headersAndBorders.background,
                                           surfaces.headersAndBorders.foreground,
                                           surfaces.inputs.background,
                                           surfaces.inputs.foreground);
        display->rehighlight();
        QWidget* page = display->parentWidget();
        applyThemeBackgroundRoleToWidget(page, surfaces.result.background);
        applyThemeBackgroundRoleToWidget(page ? page->parentWidget() : nullptr,
                                         surfaces.result.background);
        applyThemeBackgroundRoleToWidget(paneWidgetForDisplay(display),
                                         surfaces.result.background);
    }
    for (Editor* editor : splitPaneEditors()) {
        editor->setThemeSurfaceColor(surfaces.editorAndLists.background,
                                     surfaces.result.background);
        const ThemeSurfaceColors completionPopup =
            themeSurfaceForShadeIndex(surfaces, UiConfig::CompletionPopupBackgroundShade);
        const ThemeSurfaceColors completionScrollbarThumb =
            themeSurfaceForShadeIndex(surfaces, UiConfig::CompletionPopupScrollbarThumbShade);
        const ThemeSurfaceColors completionSelectedRow =
            themeSurfaceForShadeIndex(surfaces, UiConfig::CompletionPopupSelectedRowShade);
        const ThemeSurfaceColors completionOutline =
            themeSurfaceForShadeIndex(surfaces, UiConfig::CompletionPopupOutlineShade);
        editor->setThemeCompletionColors(completionPopup.background,
                                         completionPopup.foreground,
                                         completionScrollbarThumb.background,
                                         completionScrollbarThumb.foreground,
                                         completionSelectedRow.background,
                                         completionSelectedRow.foreground,
                                         completionOutline.background,
                                         UiConfig::CompletionPopupCornerRadius);
        editor->rehighlight();
    }
    updatePaneEditorCursorVisibility();
    updatePaneTabBars();
    updateSplitterStyleSheet();
}

void MainWindow::applyThemeSurfacePalette()
{
    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    const auto applyDockTabBarSurfaces = [this](const GeneratedThemeSurfaces& tabSurfaces) {
        for (QTabBar* tabBar : findChildren<QTabBar*>()) {
            if (m_tabBarDisplays.contains(tabBar))
                continue;
            QWidget* tabBarParent = tabBar->parentWidget();
            if (tabBarParent != nullptr
                && tabBarParent != this
                && !qobject_cast<QDockWidget*>(tabBarParent)) {
                applyDockTabBarBackgroundToWidget(tabBarParent, tabSurfaces.window);
            }
            const QPalette tabBarPalette =
                paletteForThemeSurface(tabBar->palette(), tabSurfaces.headersAndBorders);
            tabBar->setDrawBase(false);
            tabBar->setPalette(tabBarPalette);
            tabBar->setStyleSheet(QStringLiteral(
                "QTabBar { background-color: %1; }"
                "QTabBar::tab {"
                " background-color: transparent; color: %2;"
                " padding: 5px 14px; margin: 2px 1px;"
                "}"
                "QTabBar::tab:!selected:hover {"
                " background-color: %3; color: %4;"
                " border-radius: 10px;"
                "}"
                "QTabBar::tab:selected {"
                " background-color: %5; color: %6;"
                " border-radius: 10px;"
                "}")
                                      .arg(tabSurfaces.window.background.name(),
                                           tabSurfaces.window.foreground.name(),
                                           tabSurfaces.result.background.name(),
                                           tabSurfaces.result.foreground.name(),
                                           tabSurfaces.headersAndBorders.background.name(),
                                           tabSurfaces.headersAndBorders.foreground.name()));
            tabBar->setPalette(tabBarPalette);
        }
    };
    if (UiConfig::OklchThemeDebugReportEnabled) {
        const QString reportKey = oklchThemeReportKey(surfaces);
        if (reportKey != lastOklchThemeReportKey()) {
            const QString reportPath = writeOklchGenerationHtmlReport(surfaces.base,
                                                                      1,
                                                                      surfaces.backgrounds.size() - 2,
                                                                      surfaces.polarity,
                                                                      defaultOklchShadeDistanceFactor(),
                                                                      false,
                                                                      surfaces.backgrounds,
                                                                      surfaces.foregrounds);
            if (!reportPath.isEmpty()) {
                lastOklchThemeReportKey() = reportKey;
                QTextStream stream(stderr);
                stream << "OKLCH HTML report: " << reportPath << Qt::endl;
                scheduleThemeRuntimeDiagnosticsReport();
            }
        }
    }
    const ThemeSurfaceColors& surface = surfaces.window;
    QPalette pal = palette();
    pal.setColor(QPalette::Window, surface.background);
    pal.setColor(QPalette::WindowText, surface.foreground);
    pal.setColor(QPalette::Button, surface.background);
    pal.setColor(QPalette::ButtonText, surface.foreground);
    setPalette(pal);
    setAutoFillBackground(true);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral("QMainWindow { background-color: %1; }")
                      .arg(surface.background.name()));

    if (m_widgets.root)
        applyThemeBackgroundRoleToWidget(m_widgets.root, surface.background);
    applyKeypadThemeSurfacePalette();
    if (QStatusBar* bar = findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly)) {
        const ThemeSurfaceColors statusBarSurface =
            themeSurfaceForShadeIndex(surfaces, UiConfig::StatusBarBackgroundShade);
        bar->setPalette(paletteForThemeSurface(bar->palette(), statusBarSurface));
    }
    for (QDockWidget* dock : m_allDocks) {
        if (dock == nullptr)
            continue;
        applyGeneratedDockContentSurfaces(this, dock, surfaces);
        applyGeneratedDockChromeSurfaces(dock, surfaces);
    }
    for (QMenu* menu : findChildren<QMenu*>())
        applyMenuSurface(menu, surfaces.headersAndBorders, surfaces.inputs);
    applyDockTabBarSurfaces(surfaces);
    if (m_widgets.bitField) {
        const ThemeSurfaceColors bitfieldBackground =
            themeSurfaceForShadeIndex(surfaces, UiConfig::DockBackgroundShade);
        const ThemeSurfaceColors bitfieldHover =
            themeSurfaceForShadeIndex(surfaces, UiConfig::BitfieldBitHoverShade);
        const ThemeSurfaceColors bitfieldPressed =
            themeSurfaceForShadeIndex(surfaces, UiConfig::DockUnfocusedSelectedItemShade);
        const QPalette bitFieldPalette =
            paletteForThemeSurface(m_widgets.bitField->palette(), bitfieldBackground);
        m_widgets.bitField->setPalette(bitFieldPalette);
        m_widgets.bitField->setAutoFillBackground(true);
        m_widgets.bitField->setThemeColors(bitfieldBackground.background,
                                           bitfieldBackground.foreground,
                                           bitfieldHover.background,
                                           bitfieldHover.foreground,
                                           bitfieldPressed.background,
                                           bitfieldPressed.foreground,
                                           surfaces.primary.background,
                                           surfaces.primary.foreground);
    }
    QTimer::singleShot(0, this, [this, applyDockTabBarSurfaces]() {
        const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
        const QList<QDockWidget*> docks = m_allDocks;
        for (QDockWidget* dock : docks) {
            applyGeneratedDockContentSurfaces(this, dock, surfaces);
            applyGeneratedDockChromeSurfaces(dock, surfaces);
        }
        for (QMenu* menu : findChildren<QMenu*>())
            applyMenuSurface(menu, surfaces.headersAndBorders, surfaces.inputs);
        applyDockTabBarSurfaces(surfaces);
    });
}

void MainWindow::applyKeypadThemeSurfacePalette()
{
    if (!m_widgets.keypad)
        return;

    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    const ThemeSurfaceColors keypadBackground =
        themeSurfaceForShadeIndex(surfaces, UiConfig::KeypadBackgroundShade);
    const QPalette keypadPalette = paletteForThemeSurface(palette(), keypadBackground);
    const ThemeSurfaceColors keypadButton =
        themeSurfaceForShadeIndex(surfaces, UiConfig::KeypadButtonShade);
    const ThemeSurfaceColors keypadButtonHover =
        themeSurfaceForShadeIndex(surfaces, UiConfig::KeypadButtonHoverShade);
    const ThemeSurfaceColors keypadButtonPressed =
        themeSurfaceForShadeIndex(surfaces, UiConfig::KeypadButtonPressedShade);

    applyThemeBackgroundRoleToWidget(m_widgets.keypadContainer, keypadBackground.background);
    m_widgets.keypad->setPalette(keypadPalette);
    m_widgets.keypad->setAutoFillBackground(true);
    m_widgets.keypad->setThemeButtonColors(keypadButton.background,
                                           keypadButton.foreground,
                                           keypadButtonHover.background,
                                           keypadButtonHover.foreground,
                                           keypadButtonPressed.background,
                                           keypadButtonPressed.foreground,
                                           surfaces.primary.background);
    QEvent paletteChange(QEvent::PaletteChange);
    QApplication::sendEvent(m_widgets.keypad, &paletteChange);
}

void MainWindow::scheduleThemeRuntimeDiagnosticsReport()
{
    writeThemeRuntimeDiagnosticsReport();

    QPointer<MainWindow> guard(this);
    QTimer::singleShot(150, this, [guard]() {
        if (guard)
            guard->writeThemeRuntimeDiagnosticsReport();
    });
}

void MainWindow::writeThemeRuntimeDiagnosticsReport()
{
    const QString path = oklchThemeReportPath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QString html = QString::fromUtf8(file.readAll());
    file.close();
    if (html.isEmpty())
        return;

    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    QString section;
    QTextStream out(&section);
    out << "\n<!-- speedcrunch-oklch-runtime-start -->\n"
        << "<section class=\"runtime-diagnostics\" "
        << "style=\"margin-top:2rem;padding:1rem;border:1px solid #c6d0c4;"
        << "border-radius:14px;background:#fcfdfb;\">\n"
        << "<h2 style=\"margin:0 0 0.75rem;\">Runtime widget samples</h2>\n"
        << "<p style=\"margin:0 0 1rem;color:#586659;\">Generated after the "
        << "live window has restored its saved layout and the Qt event loop has "
        << "repainted. These values are read from widget palettes, style sheets, "
        << "and widget grabs inside the running application.</p>\n"
        << "<p><strong>Executable:</strong> <code>"
        << QCoreApplication::applicationFilePath().toHtmlEscaped()
        << "</code></p>\n"
        << "<p><strong>Expected shades:</strong> ";
    for (int i = 0; i < surfaces.backgrounds.size(); ++i) {
        if (i > 0)
            out << " ";
        out << "<code>" << i << "=" << debugColorName(surfaces.backgrounds.at(i)) << "</code>";
    }
    out << "</p>\n"
        << "<div style=\"overflow-x:auto;\">"
        << "<table style=\"width:100%;border-collapse:collapse;font-size:0.92rem;\">"
        << "<thead><tr>"
        << "<th style=\"text-align:left;border-bottom:1px solid #c6d0c4;padding:0.4rem;\">Widget</th>"
        << "<th style=\"text-align:left;border-bottom:1px solid #c6d0c4;padding:0.4rem;\">Expected</th>"
        << "<th style=\"text-align:left;border-bottom:1px solid #c6d0c4;padding:0.4rem;\">Palette</th>"
        << "<th style=\"text-align:left;border-bottom:1px solid #c6d0c4;padding:0.4rem;\">Viewport palette</th>"
        << "<th style=\"text-align:left;border-bottom:1px solid #c6d0c4;padding:0.4rem;\">Grab center</th>"
        << "<th style=\"text-align:left;border-bottom:1px solid #c6d0c4;padding:0.4rem;\">Style sheet</th>"
        << "</tr></thead><tbody>\n";

    appendDiagnosticRow(out,
                        QStringLiteral("MainWindow"),
                        debugColorName(surfaces.window.background),
                        paletteColorName(this, QPalette::Window),
                        QStringLiteral("(n/a)"),
                        grabbedCenterColorName(this),
                        shortStyleSheet(this));

    appendDiagnosticRow(out,
                        QStringLiteral("MainSplitContainer"),
                        debugColorName(surfaces.window.background),
                        paletteColorName(m_widgets.splitContainer, QPalette::Window),
                        QStringLiteral("(n/a)"),
                        grabbedCenterColorName(m_widgets.splitContainer),
                        shortStyleSheet(m_widgets.splitContainer));

    int displayIndex = 0;
    for (ResultDisplay* display : splitPaneDisplays()) {
        ++displayIndex;
        QWidget* page = display ? display->parentWidget() : nullptr;
        QWidget* stack = page ? page->parentWidget() : nullptr;
        QWidget* pane = display ? paneWidgetForDisplay(display) : nullptr;
        appendDiagnosticRow(out,
                            QStringLiteral("ResultDisplay %1").arg(displayIndex),
                            debugColorName(surfaces.result.background),
                            paletteColorName(display, QPalette::Base),
                            paletteColorName(display ? display->viewport() : nullptr,
                                             QPalette::Base),
                            grabbedCenterColorName(display ? display->viewport() : nullptr),
                            shortStyleSheet(display));
        appendDiagnosticRow(out,
                            QStringLiteral("ResultDisplay %1 viewport").arg(displayIndex),
                            debugColorName(surfaces.result.background),
                            paletteColorName(display ? display->viewport() : nullptr,
                                             QPalette::Window),
                            QStringLiteral("(n/a)"),
                            grabbedCenterColorName(display ? display->viewport() : nullptr),
                            shortStyleSheet(display ? display->viewport() : nullptr));
        appendDiagnosticRow(out,
                            QStringLiteral("ResultDisplay %1 page").arg(displayIndex),
                            debugColorName(surfaces.result.background),
                            paletteColorName(page, QPalette::Window),
                            QStringLiteral("(n/a)"),
                            grabbedCenterColorName(page),
                            shortStyleSheet(page));
        appendDiagnosticRow(out,
                            QStringLiteral("ResultDisplay %1 stack").arg(displayIndex),
                            debugColorName(surfaces.result.background),
                            paletteColorName(stack, QPalette::Window),
                            QStringLiteral("(n/a)"),
                            grabbedCenterColorName(stack),
                            shortStyleSheet(stack));
        appendDiagnosticRow(out,
                            QStringLiteral("ResultDisplay %1 pane").arg(displayIndex),
                            debugColorName(surfaces.result.background),
                            paletteColorName(pane, QPalette::Window),
                            QStringLiteral("(n/a)"),
                            grabbedCenterColorName(pane),
                            shortStyleSheet(pane));
    }

    int editorIndex = 0;
    for (Editor* editor : splitPaneEditors()) {
        ++editorIndex;
        appendDiagnosticRow(out,
                            QStringLiteral("Editor %1").arg(editorIndex),
                            debugColorName(surfaces.editorAndLists.background),
                            paletteColorName(editor, QPalette::Base),
                            paletteColorName(editor ? editor->viewport() : nullptr,
                                             QPalette::Base),
                            grabbedCenterColorName(editor),
                            shortStyleSheet(editor));
    }

    const ThemeSurfaceColors keypadBackground =
        themeSurfaceForShadeIndex(surfaces, UiConfig::KeypadBackgroundShade);
    appendDiagnosticRow(out,
                        QStringLiteral("Keypad"),
                        debugColorName(keypadBackground.background),
                        paletteColorName(m_widgets.keypad, QPalette::Window),
                        QStringLiteral("(n/a)"),
                        grabbedCenterColorName(m_widgets.keypad),
                        shortStyleSheet(m_widgets.keypad));

    appendDiagnosticRow(out,
                        QStringLiteral("Bitfield"),
                        debugColorName(surfaces.editorAndLists.background),
                        paletteColorName(m_widgets.bitField, QPalette::Window),
                        QStringLiteral("(n/a)"),
                        grabbedCenterColorName(m_widgets.bitField),
                        shortStyleSheet(m_widgets.bitField));

    if (QStatusBar* bar = findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly)) {
        const ThemeSurfaceColors statusBarSurface =
            themeSurfaceForShadeIndex(surfaces, UiConfig::StatusBarBackgroundShade);
        appendDiagnosticRow(out,
                            QStringLiteral("StatusBar"),
                            debugColorName(statusBarSurface.background),
                            paletteColorName(bar, QPalette::Window),
                            QStringLiteral("(n/a)"),
                            grabbedCenterColorName(bar),
                            shortStyleSheet(bar));
    }

    out << "</tbody></table></div>\n"
        << "</section>\n"
        << "<!-- speedcrunch-oklch-runtime-end -->\n";

    const QString startMarker = QStringLiteral("<!-- speedcrunch-oklch-runtime-start -->");
    const QString endMarker = QStringLiteral("<!-- speedcrunch-oklch-runtime-end -->");
    const int start = html.indexOf(startMarker);
    const int end = start >= 0 ? html.indexOf(endMarker, start) : -1;
    if (start >= 0 && end >= 0)
        html.remove(start, end + endMarker.size() - start);

    const int insertAt = html.lastIndexOf(QStringLiteral("</main>"));
    if (insertAt >= 0)
        html.insert(insertAt, section);
    else
        html.append(section);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return;
    file.write(html.toUtf8());
    file.close();
}

void MainWindow::captureVisibleSessionViewports()
{
    for (ResultDisplay* display : splitPaneDisplays()) {
        if (display == nullptr)
            continue;

        QString sessionName = m_paneSessionNames.value(display);
        if (sessionName.isEmpty() && display->session() != nullptr)
            sessionName = display->session()->name();
        if (sessionName.isEmpty())
            continue;

        m_sessionViewportAnchors.insert(sessionName, display->viewportTopAnchor());
        QScrollBar* bar = display->verticalScrollBar();
        const int scrollValue = bar->value() == bar->maximum()
            ? (std::numeric_limits<int>::max)()
            : bar->value();
        m_sessionScrollValues.insert(sessionName, scrollValue);
    }
}

void MainWindow::restoreVisibleSessionViewports()
{
    for (ResultDisplay* display : splitPaneDisplays()) {
        if (display == nullptr)
            continue;

        QString sessionName = m_paneSessionNames.value(display);
        if (sessionName.isEmpty() && display->session() != nullptr)
            sessionName = display->session()->name();
        if (sessionName.isEmpty())
            continue;

        const QPair<int, int> anchor = m_sessionViewportAnchors.value(sessionName, qMakePair(-1, 0));
        const int scrollValue = m_sessionScrollValues.value(sessionName, -1);
        if (anchor.first >= 0)
            display->restoreViewportTopAnchor(anchor);
        if (scrollValue >= 0)
            display->restoreScrollValue(scrollValue);
    }
}

void MainWindow::createBitField() {
    if (m_docks.bitField) {
        m_docks.bitField->show();
        m_docks.bitField->raise();
        m_settings->bitfieldVisible = true;
        return;
    }

    m_docks.bitField = new GenericDock<BitFieldWidget>("MainWindow", QT_TR_NOOP("Bitfield"), this);
    m_docks.bitField->setObjectName("BitfieldDock");
    m_docks.bitField->installEventFilter(this);
    m_docks.bitField->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_widgets.bitField = m_docks.bitField->widget();

    addTabifiedDock(m_docks.bitField, false, Qt::BottomDockWidgetArea);
    m_widgets.display->verticalScrollBar()->setValue(m_widgets.display->verticalScrollBar()->maximum());
    connect(m_widgets.bitField, SIGNAL(bitsChanged(const QString&)), SLOT(handleBitsChanged(const QString&)));
    m_settings->bitfieldVisible = true;
}

void MainWindow::createKeypad()
{
    if (m_widgets.keypad)
        return;

    m_widgets.keypadContainer = new QWidget(m_widgets.root);

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
        m_widgets.keypad = new Keypad(customButtons,
                                      m_widgets.keypadContainer,
                                      m_settings->keypadZoomPercent);
        connect(m_widgets.keypad, &Keypad::customButtonPressed,
                this, &MainWindow::handleCustomKeypadButtonPress);
    } else {
        Keypad::LayoutMode layoutMode = Keypad::LayoutModeScientificWide;
        if (m_settings->keypadMode == Settings::KeypadModeBasicWide)
            layoutMode = Keypad::LayoutModeBasicWide;
        else if (m_settings->keypadMode == Settings::KeypadModeScientificNarrow)
            layoutMode = Keypad::LayoutModeScientificNarrow;
        m_widgets.keypad = new Keypad(layoutMode,
                                      m_widgets.keypadContainer,
                                      m_settings->keypadZoomPercent);
        connect(m_widgets.keypad, SIGNAL(buttonPressed(Keypad::Button)), SLOT(handleKeypadButtonPress(Keypad::Button)));
        connect(this, SIGNAL(radixCharacterChanged()), m_widgets.keypad, SLOT(handleRadixCharacterChange()));
    }
    m_widgets.keypad->setFocusPolicy(Qt::NoFocus);
    m_widgets.keypad->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_widgets.keypad, SIGNAL(customContextMenuRequested(const QPoint&)),
            SLOT(showKeypadContextMenu(const QPoint&)));

    m_layouts.keypad = new QHBoxLayout(m_widgets.keypadContainer);
    m_layouts.keypad->setContentsMargins(0, 0, 0, 0);
    m_layouts.keypad->addStretch();
    m_layouts.keypad->addWidget(m_widgets.keypad);
    m_layouts.keypad->addStretch();
    m_widgets.keypad->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_layouts.root->addWidget(m_widgets.keypadContainer, 0);

    m_widgets.keypadContainer->show();
    m_widgets.keypad->show();
    m_settings->keypadVisible = true;
    applyKeypadThemeSurfacePalette();
}

void MainWindow::createBookDock(bool)
{
    if (m_docks.book) {
        m_docks.book->show();
        m_docks.book->raise();
        m_settings->formulaBookDockVisible = true;
        return;
    }

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
    if (m_docks.constants) {
        m_docks.constants->show();
        m_docks.constants->raise();
        if (takeFocus)
            m_docks.constants->setFocus();
        m_settings->constantsDockVisible = true;
        return;
    }

    m_docks.constants = new GenericDock<ConstantsWidget>("MainWindow", QT_TR_NOOP("Constants"), this);
    m_docks.constants->setObjectName("ConstantsDock");
    m_docks.constants->setMinimumWidth(UiConfig::ConstantsDockMinimumWidth);
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
    if (m_docks.functions) {
        m_docks.functions->show();
        m_docks.functions->raise();
        if (takeFocus)
            m_docks.functions->setFocus();
        m_settings->functionsDockVisible = true;
        return;
    }

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
    if (m_docks.history) {
        m_docks.history->show();
        m_docks.history->raise();
        m_settings->historyDockVisible = true;
        return;
    }

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
    m_docks.history->widget()->setSession(m_session);

    // No focus for this dock.
    addTabifiedDock(m_docks.history, false);
    m_settings->historyDockVisible = true;
}

void MainWindow::createVariablesDock(bool takeFocus)
{
    if (m_docks.variables) {
        m_docks.variables->show();
        m_docks.variables->raise();
        if (takeFocus)
            m_docks.variables->setFocus();
        m_settings->variablesDockVisible = true;
        return;
    }

    m_docks.variables = new GenericDock<VariableListWidget>("MainWindow", QT_TR_NOOP("User Variables"), this);
    m_docks.variables->setObjectName("VariablesDock");
    m_docks.variables->installEventFilter(this);
    m_docks.variables->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.variables->widget(), &VariableListWidget::variableSelected,
            this, &MainWindow::insertVariableIntoEditor);
    connect(m_docks.variables->widget(), &VariableListWidget::variableEdited,
            this, &MainWindow::insertTextIntoEditor);
    m_docks.variables->widget()->setEvaluator(m_evaluator);
    const auto updateVariables = [this]() {
        if (m_docks.variables)
            m_docks.variables->widget()->updateList();
    };
    connect(this, &MainWindow::radixCharacterChanged, this, updateVariables);
    connect(this, &MainWindow::variablesChanged, this, updateVariables);
    connect(m_docks.variables, &QDockWidget::visibilityChanged, this, [updateVariables](bool visible) {
        if (visible)
            updateVariables();
    });

    addTabifiedDock(m_docks.variables, takeFocus);
    m_docks.variables->widget()->setSearchText(m_settings->variablesDockSearchText);
    m_settings->variablesDockVisible = true;
}

void MainWindow::createUserFunctionsDock(bool takeFocus)
{
    if (m_docks.userFunctions) {
        m_docks.userFunctions->show();
        m_docks.userFunctions->raise();
        if (takeFocus)
            m_docks.userFunctions->setFocus();
        m_settings->userFunctionsDockVisible = true;
        return;
    }

    m_docks.userFunctions = new GenericDock<UserFunctionListWidget>("MainWindow", QT_TR_NOOP("User Functions"), this);
    m_docks.userFunctions->setObjectName("UserFunctionsDock");
    m_docks.userFunctions->installEventFilter(this);
    m_docks.userFunctions->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.userFunctions->widget(), &UserFunctionListWidget::userFunctionSelected,
            this, &MainWindow::insertUserFunctionIntoEditor);
    connect(m_docks.userFunctions->widget(), &UserFunctionListWidget::userFunctionEdited,
            this, &MainWindow::insertUserFunctionIntoEditor);
    m_docks.userFunctions->widget()->setEvaluator(m_evaluator);
    const auto updateFunctions = [this]() {
        if (m_docks.userFunctions)
            m_docks.userFunctions->widget()->updateList();
    };
    connect(this, &MainWindow::radixCharacterChanged, this, updateFunctions);
    connect(this, &MainWindow::functionsChanged, this, updateFunctions);
    connect(m_docks.userFunctions, &QDockWidget::visibilityChanged, this, [updateFunctions](bool visible) {
        if (visible)
            updateFunctions();
    });

    addTabifiedDock(m_docks.userFunctions, takeFocus);
    m_docks.userFunctions->widget()->setSearchText(m_settings->userFunctionsDockSearchText);
    m_settings->userFunctionsDockVisible = true;
}

void MainWindow::createUserUnitsDock(bool takeFocus)
{
    if (m_docks.userUnits) {
        m_docks.userUnits->show();
        m_docks.userUnits->raise();
        if (takeFocus)
            m_docks.userUnits->setFocus();
        m_settings->userUnitsDockVisible = true;
        return;
    }

    m_docks.userUnits = new GenericDock<UserUnitListWidget>("MainWindow", QT_TR_NOOP("User Units"), this);
    m_docks.userUnits->setObjectName("UserUnitsDock");
    m_docks.userUnits->installEventFilter(this);
    m_docks.userUnits->setAllowedAreas(Qt::AllDockWidgetAreas);

    connect(m_docks.userUnits->widget(), &UserUnitListWidget::userUnitSelected,
            this, &MainWindow::insertUserUnitIntoEditor);
    connect(m_docks.userUnits->widget(), &UserUnitListWidget::userUnitEdited,
            this, &MainWindow::insertTextIntoEditor);
    m_docks.userUnits->widget()->setEvaluator(m_evaluator);
    const auto updateUnits = [this]() {
        if (m_docks.userUnits)
            m_docks.userUnits->widget()->updateList();
    };
    connect(this, &MainWindow::radixCharacterChanged, this, updateUnits);
    connect(this, &MainWindow::unitsChanged, this, updateUnits);
    connect(m_docks.userUnits, &QDockWidget::visibilityChanged, this, [updateUnits](bool visible) {
        if (visible)
            updateUnits();
    });

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
    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    applyGeneratedDockContentSurfaces(this, newDock, surfaces);
    applyGeneratedDockChromeSurfaces(newDock, surfaces);
    updateSplitterStyleSheet();
    QPointer<QDockWidget> guardedDock(newDock);
    QTimer::singleShot(0, newDock, [this, guardedDock]() {
        if (guardedDock == nullptr)
            return;
        const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
        applyGeneratedDockContentSurfaces(this, guardedDock, surfaces);
        applyGeneratedDockChromeSurfaces(guardedDock, surfaces);
        updateSplitterStyleSheet();
    });
    if (takeFocus)
        newDock->setFocus();
}

void MainWindow::deleteDock(QDockWidget* dock)
{
    // QMainWindow retains internal layout items for dock restoration and tab
    // groups. Keep an attached dock alive and hidden instead of destroying it
    // from its Close event while Qt may still be traversing that layout.
    dock->hide();
}

void MainWindow::createFixedConnections()
{
    ResultDisplay* initialDisplay = m_widgets.display;
    Editor* initialEditor = m_widgets.editor;
    connect(initialEditor, &Editor::textChanged, this, [this, initialDisplay, initialEditor]() {
        if (initialEditor == m_widgets.editor
            || initialEditor->hasFocus()
            || initialEditor->viewport()->hasFocus()) {
            setActiveEditorDisplayPane(initialDisplay, initialEditor);
        }
    });
    connect(initialEditor, &Editor::selectionChanged, this, [this, initialDisplay, initialEditor]() {
        setActiveEditorDisplayPane(initialDisplay, initialEditor);
    });
    connect(initialDisplay, &ResultDisplay::clicked, this, [this, initialDisplay, initialEditor]() {
        setActiveEditorDisplayPane(initialDisplay, initialEditor, true);
    });
    connect(initialDisplay, &ResultDisplay::selectionChanged, this, [this, initialDisplay, initialEditor]() {
        setActiveEditorDisplayPane(initialDisplay, initialEditor, true);
    });
    connect(this, &MainWindow::colorSchemeChanged, this, &MainWindow::updateSplitterStyleSheet);
    connect(this, &MainWindow::colorSchemeChanged, this, &MainWindow::applyThemeSurfacePalette);
    connect(this, &MainWindow::colorSchemeChanged, this, &MainWindow::refreshPaneThemes);
    connect(this, &MainWindow::syntaxHighlightingChanged, this, &MainWindow::refreshPaneThemes);

    connect(m_actions.sessionExportHtml, SIGNAL(triggered()), SLOT(exportHtml()));
    connect(m_actions.sessionExportPlainText, SIGNAL(triggered()), SLOT(exportPlainText()));
    connect(m_actions.sessionImport, SIGNAL(triggered()), SLOT(showSessionImportDialog()));
    connect(m_actions.sessionImportUserDefinitions, SIGNAL(triggered()), SLOT(showUserDefinitionsImportDialog()));
    connect(m_actions.sessionLoad, SIGNAL(triggered()), SLOT(showSessionLoadDialog()));
    connect(m_actions.sessionQuit, &QAction::triggered, qApp, &QCoreApplication::quit);
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
    connect(m_actions.settingsBehaviorHistorySizeLimit, SIGNAL(triggered()), SLOT(setHistorySizeLimit()));
    connect(m_actions.settingsBehaviorSaveWindowPositionOnExit, SIGNAL(toggled(bool)), SLOT(setWindowPositionSaveEnabled(bool)));
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
    connect(m_actions.settingsResultFormatCartesian, SIGNAL(triggered()), SLOT(setResultFormatCartesian()));
    connect(m_actions.settingsResultFormatEngineering, SIGNAL(triggered()), SLOT(setResultFormatEngineering()));
    connect(m_actions.settingsResultFormatFixed, SIGNAL(triggered()), SLOT(setResultFormatFixed()));
    connect(m_actions.settingsResultFormatGeneral, SIGNAL(triggered()), SLOT(setResultFormatGeneral()));
    connect(m_actions.settingsResultFormatHexadecimal, SIGNAL(triggered()), SLOT(setResultFormatHexadecimal()));
    connect(m_actions.settingsImaginaryUnitI, SIGNAL(triggered()), SLOT(setImaginaryUnitI()));
    connect(m_actions.settingsImaginaryUnitJ, SIGNAL(triggered()), SLOT(setImaginaryUnitJ()));
    connect(m_actions.settingsResultFormatOctal, SIGNAL(triggered()), SLOT(setResultFormatOctal()));
    connect(m_actions.settingsResultFormatPolar, SIGNAL(triggered()), SLOT(setResultFormatPolar()));
    connect(m_actions.settingsResultFormatTrigonometric, SIGNAL(triggered()), SLOT(setResultFormatTrigonometric()));
    connect(m_actions.settingsResultFormatCis, SIGNAL(triggered()), SLOT(setResultFormatCis()));
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
    connect(m_actions.helpFacebookGroup, SIGNAL(triggered()), SLOT(openFacebookGroupURL()));
    connect(m_actions.helpNews, SIGNAL(triggered()), SLOT(openNewsURL()));
    connect(m_actions.helpSource, SIGNAL(triggered()), SLOT(openSourceURL()));
    connect(m_actions.helpDonate, SIGNAL(triggered()), SLOT(openDonateURL()));
    connect(m_actions.helpAbout, SIGNAL(triggered()), SLOT(showAboutDialog()));

    connect(m_widgets.editor, &Editor::autoCalcDisabled, this, [this]() {
        if (sender() == m_widgets.editor)
            hideStateLabel();
    });
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
    connect(initialEditor, &Editor::textChanged, this, [this, initialEditor]() {
        if (initialEditor == m_widgets.editor
            || initialEditor->hasFocus()
            || initialEditor->viewport()->hasFocus()) {
            handleEditorTextChange();
        }
    });
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
    connect(m_widgets.display, SIGNAL(newSessionRequested()), SLOT(showNewSessionDialog()));
    connect(m_widgets.display, SIGNAL(openSessionRequested()), SLOT(showOpenSessionDialog()));
    connect(m_widgets.display, SIGNAL(duplicateSessionRequested()), SLOT(showDuplicateSessionDialog()));
    connect(m_widgets.display, SIGNAL(splitLeftRequested()), SLOT(splitActivePaneLeft()));
    connect(m_widgets.display, SIGNAL(splitRightRequested()), SLOT(splitActivePaneRight()));
    connect(m_widgets.display, SIGNAL(splitUpRequested()), SLOT(splitActivePaneUp()));
    connect(m_widgets.display, SIGNAL(splitDownRequested()), SLOT(splitActivePaneDown()));
    connect(m_widgets.display, SIGNAL(renameSessionRequested()), SLOT(showRenameSessionDialog()));
    connect(m_widgets.display, SIGNAL(clearSessionRequested()), SLOT(clearSession()));
    connect(m_widgets.display, SIGNAL(closeSessionRequested()), SLOT(closeCurrentSession()));
    connect(m_widgets.display, SIGNAL(closePaneRequested()), SLOT(closeCurrentPane()));
    connect(m_widgets.display, SIGNAL(deleteSessionRequested()), SLOT(deleteCurrentSession()));
    connect(m_widgets.display, SIGNAL(loadedSessionsMenuRequested(const QPoint&)), SLOT(showLoadedSessionsMenu(const QPoint&)));
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
    connect(this, SIGNAL(syntaxHighlightingChanged()), m_widgets.display, SLOT(rehighlight()));
    connect(this, SIGNAL(syntaxHighlightingChanged()), m_widgets.editor, SLOT(rehighlight()));

    connect(m_actions.settingsDisplayFont, SIGNAL(triggered()), SLOT(showFontDialog()));
    connect(m_actions.settingsDisplayColorSchemeCustom, SIGNAL(triggered()), SLOT(showCustomThemeDialog()));

    connect(this, SIGNAL(languageChanged()), SLOT(retranslateText()));

    const auto bindStandardKey = [this](QKeySequence::StandardKey key, const std::function<void()>& handler) {
        const QList<QKeySequence> bindings = QKeySequence::keyBindings(key);
        for (const QKeySequence& sequence : bindings) {
            QShortcut* shortcut = new QShortcut(sequence, this);
            connect(shortcut, &QShortcut::activated, this, handler);
        }
    };
    bindStandardKey(QKeySequence::AddTab, [this]() { showNewSessionDialog(); });
    bindStandardKey(QKeySequence::NextChild, [this]() { activateNextChild(); });
    bindStandardKey(QKeySequence::PreviousChild, [this]() { activatePreviousChild(); });
    bindStandardKey(QKeySequence::Open, [this]() { showOpenSessionDialog(); });
    bindStandardKey(QKeySequence::Close, [this]() { closeCurrentSession(); });
    bindStandardKey(QKeySequence::Quit, []() { qApp->quit(); });

    QShortcut* splitRightShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+\\")), this);
    connect(splitRightShortcut, &QShortcut::activated, this, &MainWindow::splitActivePaneRight);
    QShortcut* splitDownShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+\\")), this);
    connect(splitDownShortcut, &QShortcut::activated, this, &MainWindow::splitActivePaneDown);
}

void MainWindow::applySettings()
{
    emit languageChanged();

    // QMainWindow state restoration expects every named dock in a saved layout
    // to exist before restoreState() runs. Instantiate docks up front and keep
    // closed panels hidden instead of inserting them after layout restoration.
    const bool formulaBookDockVisible = m_settings->formulaBookDockVisible;
    const bool constantsDockVisible = m_settings->constantsDockVisible;
    const bool functionsDockVisible = m_settings->functionsDockVisible;
    const bool historyDockVisible = m_settings->historyDockVisible;
    const bool variablesDockVisible = m_settings->variablesDockVisible;
    const bool userFunctionsDockVisible = m_settings->userFunctionsDockVisible;
    const bool userUnitsDockVisible = m_settings->userUnitsDockVisible;
    const bool bitfieldVisible = m_settings->bitfieldVisible;

    createBookDock(false);
    setFormulaBookDockVisible(formulaBookDockVisible, false);
    m_actions.viewFormulaBook->setChecked(formulaBookDockVisible);

    createConstantsDock(false);
    setConstantsDockVisible(constantsDockVisible, false);
    m_actions.viewConstants->setChecked(constantsDockVisible);

    createFunctionsDock(false);
    setFunctionsDockVisible(functionsDockVisible, false);
    m_actions.viewFunctions->setChecked(functionsDockVisible);

    createHistoryDock(false);
    setHistoryDockVisible(historyDockVisible, false);
    m_actions.viewHistory->setChecked(historyDockVisible);

    createVariablesDock(false);
    setVariablesDockVisible(variablesDockVisible, false);
    m_actions.viewVariables->setChecked(variablesDockVisible);

    createUserFunctionsDock(false);
    setUserFunctionsDockVisible(userFunctionsDockVisible, false);
    m_actions.viewUserFunctions->setChecked(userFunctionsDockVisible);

    createUserUnitsDock(false);
    setUserUnitsDockVisible(userUnitsDockVisible, false);
    m_actions.viewUserUnits->setChecked(userUnitsDockVisible);

    createBitField();
    setBitfieldVisible(bitfieldVisible);
    m_actions.viewBitfield->setChecked(bitfieldVisible);
    updateKeypadModeActionState();
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
    restoreState(m_settings->windowState, DockLayoutStateVersion);
    if (m_settings->windowState.isEmpty()
        && constantsDockVisible
        && m_docks.constants != nullptr) {
        QPointer<QDockWidget> constantsDock(m_docks.constants);
        QTimer::singleShot(0, this, [this, constantsDock]() {
            if (constantsDock == nullptr || !constantsDock->isVisible())
                return;
            resizeDocks(QList<QDockWidget*> { constantsDock },
                        QList<int> { UiConfig::ConstantsDockDefaultWidth },
                        Qt::Horizontal);
        });
    }

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

    UserDefinitions::loadInto(m_settings);

    restoreSession();
    applyUserDefinitions();

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
    for (ResultDisplay* display : splitPaneDisplays())
        display->setFont(font);
    for (Editor* editor : splitPaneEditors())
        editor->setFont(font);

    if (m_widgets.display != nullptr)
        m_widgets.display->verticalScrollBar()->setValue(m_widgets.display->verticalScrollBar()->maximum());

    updateColorSchemeActionState();
    updateSplitterStyleSheet();

    if (m_widgets.display != nullptr && m_widgets.display->isEmpty())
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
    m_settings->complexNumbers = true;
    m_settings->secondaryComplexNumbers = true;
    m_settings->tertiaryComplexNumbers = true;
    m_settings->quaternaryComplexNumbers = true;
    m_settings->quinaryComplexNumbers = true;
    DMath::complexMode = true;

    if (m_settings->resultComplexForm == ComplexForm::Exponential)
        m_actions.settingsResultFormatPolar->setChecked(true);
    else if (m_settings->resultComplexForm == ComplexForm::Trigonometric)
        m_actions.settingsResultFormatTrigonometric->setChecked(true);
    else if (m_settings->resultComplexForm == ComplexForm::Cis)
        m_actions.settingsResultFormatCis->setChecked(true);
    else if (m_settings->resultComplexForm == ComplexForm::Phasor)
        m_actions.settingsResultFormatPolarAngle->setChecked(true);
    else
        m_actions.settingsResultFormatCartesian->setChecked(true);
}

void MainWindow::checkInitialImaginaryUnit()
{
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
    m_settings->windowState = saveState(DockLayoutStateVersion);
    if (m_widgets.display != nullptr)
        m_settings->displayFont = m_widgets.display->font().toString();

    m_settings->save();
}

void MainWindow::saveSession(QString & fname)
{
    captureEditorTextInCurrentSession();
    QJsonObject json;
    m_session->serialize(json);
    writeSessionJsonToFile(json, fname);
}

void MainWindow::saveSessionLayout(bool captureCurrentViewport)
{
    const bool includeEditorState = m_shutdownStateSaved || applicationShutdownInProgress();
    const auto editorForDisplay = [](ResultDisplay* display) -> Editor* {
        QWidget* page = display ? display->parentWidget() : nullptr;
        return page ? page->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
    };

    if (captureCurrentViewport) {
        QSet<MainWindow*> capturedWindows;
        for (const QPointer<MainWindow>& ptr : allMainWindows()) {
            MainWindow* window = ptr.data();
            if (window == nullptr || capturedWindows.contains(window))
                continue;

            capturedWindows.insert(window);
            window->captureVisibleSessionViewports();
        }
    }

    QStringList sessionNames;
    for (ResultDisplay* display : splitPaneDisplays()) {
        const QStringList names = paneSessionNames(display);
        for (const QString& name : names) {
            if (!name.isEmpty() && !sessionNames.contains(name, Qt::CaseInsensitive))
                sessionNames.append(name);
        }
    }
    if (sessionNames.isEmpty())
        sessionNames = m_loadedSessions.keys();
    sessionNames.sort(Qt::CaseInsensitive);

    QJsonArray tabs;
    const QString activePaneSessionName = m_widgets.display != nullptr
        ? m_paneSessionNames.value(m_widgets.display)
        : QString();
    const QJsonObject activeEditorState = includeEditorState
        ? editorState(editorForDisplay(m_widgets.display))
        : QJsonObject();
    for (const QString& name : sessionNames)
        tabs.append(sessionLayoutEntry(name,
                                       m_sessionViewportAnchors.value(name, qMakePair(-1, 0)),
                                       m_sessionScrollValues.value(name, -1),
                                       activeEditorState,
                                       includeEditorState && name.compare(activePaneSessionName, Qt::CaseInsensitive) == 0));

    const auto layoutNodeForWidget = [this, includeEditorState, &editorForDisplay](QWidget* widget, const auto& layoutNodeForWidgetRef) -> QJsonObject {
        QJsonObject node;
        if (widget == nullptr)
            return node;

        if (QSplitter* splitter = qobject_cast<QSplitter*>(widget)) {
            QJsonArray children;
            for (int i = 0; i < splitter->count(); ++i) {
                const QJsonObject child = layoutNodeForWidgetRef(splitter->widget(i), layoutNodeForWidgetRef);
                if (!child.isEmpty())
                    children.append(child);
            }

            QJsonArray sizes;
            for (int size : splitter->sizes())
                sizes.append(size);

            node.insert(QStringLiteral("type"), QStringLiteral("split"));
            node.insert(QStringLiteral("orientation"),
                        splitter->orientation() == Qt::Horizontal
                            ? QStringLiteral("horizontal")
                            : QStringLiteral("vertical"));
            node.insert(QStringLiteral("sizes"), sizes);
            node.insert(QStringLiteral("children"), children);
            return node;
        }

        ResultDisplay* display = widget->findChild<ResultDisplay*>();
        if (display == nullptr)
            return node;

        const QString paneSessionName = m_paneSessionNames.value(display);
        QJsonArray paneTabs;
        const QStringList paneNames = paneSessionNames(display);
        const QJsonObject paneEditorState = includeEditorState
            ? editorState(editorForDisplay(display))
            : QJsonObject();
        for (const QString& name : paneNames) {
            paneTabs.append(sessionLayoutEntry(name,
                                               m_sessionViewportAnchors.value(name, qMakePair(-1, 0)),
                                               m_sessionScrollValues.value(name, -1),
                                               paneEditorState,
                                               includeEditorState && name.compare(paneSessionName, Qt::CaseInsensitive) == 0));
        }

        node.insert(QStringLiteral("type"), QStringLiteral("pane"));
        node.insert(QStringLiteral("active"), paneSessionName);
        node.insert(QStringLiteral("tabs"), paneTabs);
        return node;
    };

    QJsonObject root;
    const QList<ResultDisplay*> displays = splitPaneDisplays();
    if (m_widgets.splitContainer != nullptr && displays.size() > 1) {
        root = layoutNodeForWidget(m_widgets.splitContainer, layoutNodeForWidget);
        root.insert(QStringLiteral("active"), m_session ? m_session->name() : QString());
        root.insert(QStringLiteral("tabs"), tabs);
    } else {
        root.insert(QStringLiteral("type"), QStringLiteral("tabs"));
        root.insert(QStringLiteral("active"), m_session ? m_session->name() : QString());
        root.insert(QStringLiteral("tabs"), tabs);
    }

    QJsonArray windows;
    int windowIndex = 0;
    QSet<MainWindow*> uniqueWindows;
    for (const QPointer<MainWindow>& ptr : allMainWindows()) {
        MainWindow* windowObject = ptr.data();
        if (windowObject == nullptr || windowObject->m_session == nullptr)
            continue;
        if (uniqueWindows.contains(windowObject))
            continue;
        uniqueWindows.insert(windowObject);

        QJsonArray windowTabs;
        QStringList names;
        for (ResultDisplay* display : windowObject->splitPaneDisplays()) {
            for (const QString& name : windowObject->paneSessionNames(display)) {
                if (!name.isEmpty() && !names.contains(name, Qt::CaseInsensitive))
                    names.append(name);
            }
        }
        if (names.isEmpty())
            names = windowObject->m_loadedSessions.keys();
        names.sort(Qt::CaseInsensitive);
        const QString windowActivePaneSessionName = windowObject->m_widgets.display != nullptr
            ? windowObject->m_paneSessionNames.value(windowObject->m_widgets.display)
            : QString();
        const QJsonObject windowActiveEditorState = includeEditorState
            ? editorState(editorForDisplay(windowObject->m_widgets.display))
            : QJsonObject();
        for (const QString& name : names) {
            windowTabs.append(sessionLayoutEntry(name,
                windowObject->m_sessionViewportAnchors.value(name, qMakePair(-1, 0)),
                windowObject->m_sessionScrollValues.value(name, -1),
                windowActiveEditorState,
                includeEditorState && name.compare(windowActivePaneSessionName, Qt::CaseInsensitive) == 0));
        }

        const auto nodeForWidget = [windowObject, includeEditorState, &editorForDisplay](QWidget* widget, const auto& selfRef) -> QJsonObject {
            QJsonObject node;
            if (widget == nullptr)
                return node;
            if (QSplitter* splitter = qobject_cast<QSplitter*>(widget)) {
                QJsonArray children;
                for (int i = 0; i < splitter->count(); ++i) {
                    const QJsonObject child = selfRef(splitter->widget(i), selfRef);
                    if (!child.isEmpty())
                        children.append(child);
                }
                QJsonArray sizes;
                for (int size : splitter->sizes())
                    sizes.append(size);
                node.insert(QStringLiteral("type"), QStringLiteral("split"));
                node.insert(QStringLiteral("orientation"),
                            splitter->orientation() == Qt::Horizontal ? QStringLiteral("horizontal") : QStringLiteral("vertical"));
                node.insert(QStringLiteral("sizes"), sizes);
                node.insert(QStringLiteral("children"), children);
                return node;
            }
            ResultDisplay* display = widget->findChild<ResultDisplay*>();
            if (display == nullptr)
                return node;
            node.insert(QStringLiteral("type"), QStringLiteral("pane"));
            const QString paneSessionName = windowObject->m_paneSessionNames.value(display);
            node.insert(QStringLiteral("active"), paneSessionName);
            QJsonArray paneTabs;
            const QJsonObject paneEditorState = includeEditorState
                ? editorState(editorForDisplay(display))
                : QJsonObject();
            for (const QString& name : windowObject->paneSessionNames(display)) {
                paneTabs.append(sessionLayoutEntry(name,
                    windowObject->m_sessionViewportAnchors.value(name, qMakePair(-1, 0)),
                    windowObject->m_sessionScrollValues.value(name, -1),
                    paneEditorState,
                    includeEditorState && name.compare(paneSessionName, Qt::CaseInsensitive) == 0));
            }
            node.insert(QStringLiteral("tabs"), paneTabs);
            return node;
        };

        QJsonObject windowRoot;
        const QList<ResultDisplay*> displays = windowObject->splitPaneDisplays();
        if (windowObject->m_widgets.splitContainer != nullptr && displays.size() > 1) {
            windowRoot = nodeForWidget(windowObject->m_widgets.splitContainer, nodeForWidget);
            windowRoot.insert(QStringLiteral("active"), windowObject->m_session ? windowObject->m_session->name() : QString());
            windowRoot.insert(QStringLiteral("tabs"), windowTabs);
        } else {
            windowRoot.insert(QStringLiteral("type"), QStringLiteral("tabs"));
            windowRoot.insert(QStringLiteral("active"), windowObject->m_session ? windowObject->m_session->name() : QString());
            windowRoot.insert(QStringLiteral("tabs"), windowTabs);
        }

        QJsonObject window;
        const QString id = QStringLiteral("window-%1").arg(windowIndex++);
        window.insert(QStringLiteral("id"), id);
        window.insert(QStringLiteral("active"), windowObject == this);
        window.insert(QStringLiteral("root"), windowRoot);
        window.insert(QStringLiteral("statusBarVisible"),
                      windowObject->statusBar() != nullptr && windowObject->statusBar()->isVisible());
        window.insert(QStringLiteral("bitfieldVisible"),
                      windowObject->m_docks.bitField != nullptr && windowObject->m_docks.bitField->isVisible());
        window.insert(QStringLiteral("keypadVisible"), windowObject->m_widgets.keypad != nullptr);
        window.insert(QStringLiteral("windowState"),
                      QString::fromLatin1(windowObject->saveState(DockLayoutStateVersion).toBase64()));
        if (windowObject->m_settings->windowPositionSave)
            window.insert(QStringLiteral("geometry"), QString::fromLatin1(windowObject->saveGeometry().toBase64()));
        windows.append(window);
    }

    QJsonObject layout;
    layout.insert(QStringLiteral("scheme"), 1);
    layout.insert(QStringLiteral("kind"), QStringLiteral("session-layout"));
    layout.insert(QStringLiteral("activeWindow"), QStringLiteral("window-0"));
    layout.insert(QStringLiteral("windows"), windows);

    m_settings->sessionLayoutJson = QString::fromUtf8(
        QJsonDocument(layout).toJson(QJsonDocument::Compact));
    m_settings->saveSessionLayoutJson();
}

void MainWindow::activateSession(Session* session)
{
    if (session == nullptr)
        return;

    const bool previousSessionIsLoaded =
        m_session != nullptr && m_loadedSessions.values().contains(m_session);
    if (previousSessionIsLoaded && m_session != session) {
        captureEditorTextInCurrentSession();
        if (m_widgets.display != nullptr
                && m_paneSessionNames.value(m_widgets.display) == m_session->name()) {
            m_sessionViewportAnchors.insert(m_session->name(), m_widgets.display->viewportTopAnchor());
            QScrollBar* bar = m_widgets.display->verticalScrollBar();
            const int scrollValue = bar->value() == bar->maximum()
                ? (std::numeric_limits<int>::max)()
                : bar->value();
            m_sessionScrollValues.insert(m_session->name(), scrollValue);
        }
    }

    const bool displayAlreadyShowsSession =
        m_widgets.display != nullptr && m_widgets.display->session() == session;

    m_session = session;
    m_evaluator = m_session->evaluator();
    m_evaluator->initializeBuiltInVariables();
    if (m_widgets.display != nullptr)
        m_widgets.display->setSession(m_session);
    if (m_widgets.editor != nullptr)
        m_widgets.editor->setSession(m_session);
    if (m_docks.history)
        m_docks.history->widget()->setSession(m_session);
    if (m_docks.variables)
        m_docks.variables->widget()->setEvaluator(m_evaluator);
    if (m_docks.userFunctions)
        m_docks.userFunctions->widget()->setEvaluator(m_evaluator);
    if (m_docks.userUnits)
        m_docks.userUnits->widget()->setEvaluator(m_evaluator);
    if (m_widgets.display != nullptr)
        addSessionToActivePane(m_session->name());
    m_pendingHistoryEditIndex = -1;
    if (m_widgets.display != nullptr)
        m_widgets.display->setEditingHistoryIndex(-1);
    if (m_widgets.editor != nullptr)
        m_widgets.editor->setHistoryArrowNavigationEnabled(true);
    if (!displayAlreadyShowsSession)
        restoreEditorTextFromCurrentSession();
    if (m_widgets.editor != nullptr)
        m_widgets.editor->updateHistory();
    if (m_widgets.display != nullptr && !displayAlreadyShowsSession)
        m_widgets.display->refresh();
    if (m_docks.history)
        m_docks.history->widget()->updateHistory();
    emit variablesChanged();
    emit functionsChanged();
    emit unitsChanged();
    QTimer::singleShot(0, this, [this]() {
        emit variablesChanged();
        emit functionsChanged();
        emit unitsChanged();
    });
    if (m_widgets.display == nullptr) {
        m_conditions.autoAns = !m_session->historyIsEmpty();
        updatePaneEditorCursorVisibility();
        return;
    }

    m_widgets.display->viewport()->update();
    if (!displayAlreadyShowsSession) {
        const QPair<int, int> anchor = m_sessionViewportAnchors.value(m_session->name(), qMakePair(-1, 0));
        const int scrollValue = m_sessionScrollValues.value(m_session->name(), -1);
        if (anchor.first >= 0) {
            m_widgets.display->restoreViewportTopAnchor(anchor);
            QTimer::singleShot(0, this, [this, anchor]() {
                m_widgets.display->restoreViewportTopAnchor(anchor);
            });
        }
        if (scrollValue >= 0) {
            m_widgets.display->restoreScrollValue(scrollValue);
            QTimer::singleShot(0, this, [this, scrollValue]() {
                m_widgets.display->restoreScrollValue(scrollValue);
            });
        }
    }
    m_conditions.autoAns = !m_session->historyIsEmpty();
    updatePaneEditorCursorVisibility();
}

void MainWindow::captureEditorTextInCurrentSession()
{
    if (m_widgets.editor == nullptr)
        return;

    Session* session = nullptr;
    if (m_widgets.display != nullptr) {
        const QString paneSessionName = m_paneSessionNames.value(m_widgets.display);
        session = m_loadedSessions.value(paneSessionName, nullptr);
    }
    if (session == nullptr)
        session = m_session;
    if (session == nullptr)
        return;

    session->setEditorText(m_widgets.editor->text());
}

void MainWindow::restoreEditorTextFromCurrentSession()
{
    if (m_session == nullptr || m_widgets.editor == nullptr)
        return;

    m_widgets.editor->setText(m_session->editorText());
    m_widgets.editor->setCursorPosition(m_widgets.editor->text().size());
    if (m_widgets.editor->text().trimmed().isEmpty() && m_widgets.bitField)
        m_widgets.bitField->clear();
    else
        m_widgets.editor->refreshAutoCalc();
    QTimer::singleShot(0, this, [this]() {
        if (m_widgets.editor)
            m_widgets.editor->refreshAutoCalc();
    });
    m_widgets.editor->setFocus();
}

MainWindow::MainWindow()
    : QMainWindow()
{
    qApp->setQuitOnLastWindowClosed(false);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    if (primaryMainWindow().isNull())
        primaryMainWindow() = this;
    allMainWindows().append(QPointer<MainWindow>(this));
    if (objectName().isEmpty())
        setObjectName(QStringLiteral("window-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    windowIds().insert(objectName(), QPointer<MainWindow>(this));

    m_session = new Session();
    m_session->setName(firstAvailableUntitledSessionName(m_loadedSessions));
    m_loadedSessions.insert(m_session->name(), m_session);
    m_constants = Constants::instance();
    m_evaluator = m_session->evaluator();
    m_functions = FunctionRepo::instance();
    m_evaluator->initializeBuiltInVariables();

    m_translator = 0;
    m_settings = Settings::instance();
    DMath::complexMode = m_settings->complexNumbers;
    CMath::setImaginaryUnitSymbol(m_settings->imaginaryUnit);

    m_widgets.manual = 0;
    m_widgets.keypad  = 0;
    m_widgets.keypadContainer = 0;

    m_conditions.autoAns = false;

    m_docks.book = 0;
    m_docks.history = 0;
    m_docks.constants = 0;
    m_docks.functions = 0;
    m_docks.variables = 0;
    m_docks.userFunctions = 0;
    m_docks.userUnits = 0;
    m_docks.bitField = 0;

    m_status.angleUnit = 0;
    m_status.angleUnitSection = 0;
    m_status.angleUnitLabel = 0;
    m_status.resultFormat = 0;
    m_status.resultFormatSection = 0;
    m_status.resultFormatLabel = 0;
    m_status.resultPrecision = 0;
    m_status.resultPrecisionSection = 0;
    m_status.resultPrecisionLabel = 0;

    m_copyWidget = 0;
    m_pendingHistoryEditIndex = -1;
    m_shutdownStateSaved = false;
    m_versionCheck = 0;
    m_deferredSessionSaveTimer = new QTimer(this);
    m_deferredSessionSaveTimer->setSingleShot(true);
    m_deferredSessionSaveTimer->setInterval(200);
    m_sessionSavePending = false;
    m_bulkEvaluationInProgress = false;
    m_bulkHistoryChanged = false;
    m_bulkVariablesChanged = false;
    m_bulkFunctionsChanged = false;
    m_bulkUnitsChanged = false;
    connect(m_deferredSessionSaveTimer, &QTimer::timeout, this, &MainWindow::flushPendingSessionSave);
    connect(qApp, &QApplication::focusChanged, this, &MainWindow::handleApplicationFocusChanged);
    qApp->installEventFilter(this);

    createUi();
    applySettings();
    applyThemeSurfacePalette();
    refreshPaneThemes();
    ensureDockSeparatorStyleInstalled();
    updatePaneLoadedSessionCounts();

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
    qApp->removeEventFilter(this);
    windowIds().remove(objectName());
    allMainWindows().removeAll(QPointer<MainWindow>(this));
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
    qDeleteAll(m_loadedSessions);
    m_session = nullptr;
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

    QMessageBox confirmation(this);
    confirmation.setIcon(QMessageBox::Question);
    confirmation.setWindowTitle(tr("Clear History"));
    confirmation.setText(tr("Are you sure you want to clear the calculation history?"));
    confirmation.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    confirmation.setDefaultButton(QMessageBox::No);
    confirmation.setEscapeButton(QMessageBox::No);
    QShortcut clearHistoryEscape(QKeySequence(Qt::Key_Escape), &confirmation);
    connect(&clearHistoryEscape, &QShortcut::activated, &confirmation, &QMessageBox::reject);
    if (confirmation.exec() != QMessageBox::Yes)
        return;

    m_session->clearHistory();
    m_pendingHistoryEditIndex = -1;
    m_widgets.display->setEditingHistoryIndex(-1);
    m_widgets.editor->setHistoryArrowNavigationEnabled(true);
    clearEditorAndBitfield();
    emit historyChanged();

    m_conditions.autoAns = false;
}

void MainWindow::clearSession()
{
    if (m_session->historyIsEmpty()
            && m_session->variablesToList().isEmpty()
            && m_session->UserFunctionsToList().isEmpty()
            && m_session->userUnitsToList().isEmpty()) {
        return;
    }

    QMessageBox confirmation(this);
    confirmation.setIcon(QMessageBox::Question);
    confirmation.setWindowTitle(tr("Clear History"));
    confirmation.setText(tr("Are you sure you want to clear the calculation history?"));
    confirmation.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    confirmation.setDefaultButton(QMessageBox::No);
    confirmation.setEscapeButton(QMessageBox::No);
    QShortcut clearSessionEscape(QKeySequence(Qt::Key_Escape), &confirmation);
    connect(&clearSessionEscape, &QShortcut::activated, &confirmation, &QMessageBox::reject);
    if (confirmation.exec() != QMessageBox::Yes)
        return;

    m_session->clearHistory();
    m_session->clearVariables();
    m_session->clearUserFunctions();
    m_session->clearUserUnits();
    m_pendingHistoryEditIndex = -1;
    m_widgets.display->setEditingHistoryIndex(-1);
    m_widgets.editor->setHistoryArrowNavigationEnabled(true);
    clearEditorAndBitfield();
    m_evaluator->initializeBuiltInVariables();
    applyUserDefinitions();
    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();
    emit unitsChanged();

    m_conditions.autoAns = false;
    saveSessionToDefaultPath();
}

void MainWindow::showNewSessionDialog()
{
    createUntitledSession();
    saveSessionLayout(false);
}

void MainWindow::showOpenSessionDialog()
{
    migrateLegacyHistoryIfNeeded();
    ensureSessionsPath();

    struct SessionFileEntry {
        QString name;
        QString path;
        QJsonObject json;
    };
    QList<SessionFileEntry> entries;

    const QDir dir(sessionsPath());
    const QFileInfoList files = dir.entryInfoList(QStringList(QStringLiteral("*.json")),
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& fileInfo : files) {
        QJsonObject json;
        if (!readValidSessionJson(fileInfo.absoluteFilePath(), &json))
            continue;

        SessionFileEntry entry;
        entry.name = normalizedSessionName(json.value(QLatin1String(SessionJsonKeys::Session)).toString());
        entry.path = fileInfo.absoluteFilePath();
        entry.json = json;
        entries.append(entry);
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Open Session"));
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    QListWidget* list = new QListWidget(&dialog);
    for (int i = 0; i < entries.size(); ++i) {
        QListWidgetItem* item = new QListWidgetItem(entries.at(i).name, list);
        item->setData(Qt::UserRole, i);
        if (m_session != nullptr && entries.at(i).name == m_session->name())
            item->setSelected(true);
    }
    layout->addWidget(list);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, &dialog);
    QPushButton* openButton = buttons->button(QDialogButtonBox::Open);
    openButton->setEnabled(list->currentItem() != nullptr);
    layout->addWidget(buttons);

    connect(list, &QListWidget::currentItemChanged, &dialog, [openButton](QListWidgetItem* current) {
        openButton->setEnabled(current != nullptr);
    });
    connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog](QListWidgetItem*) {
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (list->count() > 0 && list->currentItem() == nullptr)
        list->setCurrentRow(0);
    list->setFocus();

    if (dialog.exec() != QDialog::Accepted || list->currentItem() == nullptr)
        return;

    const int entryIndex = list->currentItem()->data(Qt::UserRole).toInt();
    if (entryIndex < 0 || entryIndex >= entries.size())
        return;

    const SessionFileEntry entry = entries.at(entryIndex);
    if (focusOpenSession(entry.name)) {
        saveSessionLayout(false);
        return;
    }

    Session* selectedSession = m_loadedSessions.value(entry.name, nullptr);
    bool reloadedSession = false;
    if (selectedSession == nullptr) {
        selectedSession = new Session();
        selectedSession->deSerialize(entry.json, false);
        selectedSession->setName(entry.name);
        m_loadedSessions.insert(entry.name, selectedSession);
        updatePaneLoadedSessionCounts();
        reloadedSession = true;
    } else if (selectedSession == m_session) {
        selectedSession->deSerialize(entry.json, false);
        selectedSession->setName(entry.name);
        reloadedSession = true;
    }
    // Deserializing a saved session replaces its symbol tables. Startup global
    // definitions are applied after normal restore, so manual open/reload has
    // to do the same or an empty session appears to have no global symbols.
    if (reloadedSession)
        applyUserDefinitions();

    if (selectedSession != m_session)
        saveSessionToDefaultPath();

    activateSession(selectedSession);
    saveSessionLayout(false);
}

void MainWindow::showDuplicateSessionDialog()
{
    if (m_session == nullptr)
        return;

    captureEditorTextInCurrentSession();

    const QString originalName = normalizedSessionName(m_session->name());
    while (true) {
        bool accepted = false;
        const QString enteredName = QInputDialog::getText(
            this,
            tr("Duplicate Session"),
            tr("Session name:"),
            QLineEdit::Normal,
            originalName + QStringLiteral(" (copy)"),
            &accepted).trimmed();
        if (!accepted)
            return;

        const QString name = normalizedSessionName(enteredName);
        if (loadedSessionNameExists(m_loadedSessions, name) || QFileInfo::exists(sessionFilePath(name))) {
            QMessageBox::warning(this,
                                 tr("Duplicate Session"),
                                 tr("A session named %1 already exists.").arg(name));
            continue;
        }

        QJsonObject duplicateJson;
        m_session->serialize(duplicateJson);
        duplicateJson.insert(QLatin1String(SessionJsonKeys::Session), name);

        const QString duplicatePath = sessionFilePath(name);
        QFile duplicateFile(duplicatePath);
        if (!duplicateFile.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this,
                                 tr("Duplicate Session"),
                                 tr("Could not create session file %1.").arg(duplicatePath));
            continue;
        }

        const QByteArray data = QJsonDocument(duplicateJson).toJson(QJsonDocument::Compact);
        if (duplicateFile.write(data) != data.size()) {
            duplicateFile.close();
            QFile::remove(duplicatePath);
            QMessageBox::warning(this,
                                 tr("Duplicate Session"),
                                 tr("Could not write session file %1.").arg(duplicatePath));
            continue;
        }
        duplicateFile.close();

        Session* duplicateSession = new Session();
        duplicateSession->deSerialize(duplicateJson, false);
        duplicateSession->setName(name);
        m_loadedSessions.insert(name, duplicateSession);
        updatePaneLoadedSessionCounts();

        activateSession(duplicateSession);
        m_conditions.autoAns = !duplicateSession->historyIsEmpty();
        saveSessionLayout(false);
        return;
    }
}

void MainWindow::showRenameSessionDialog()
{
    if (m_session == nullptr)
        return;

    const QString originalName = m_session->name();
    while (true) {
        bool accepted = false;
        const QString enteredName = QInputDialog::getText(
            this,
            tr("Rename Session"),
            tr("Session name:"),
            QLineEdit::Normal,
            originalName,
            &accepted).trimmed();
        if (!accepted)
            return;

        const QString name = normalizedSessionName(enteredName);
        if (name == originalName)
            return;

        if (loadedSessionNameExists(m_loadedSessions, name) || QFileInfo::exists(sessionFilePath(name))) {
            QMessageBox::warning(this,
                                 tr("Rename Session"),
                                 tr("A session named %1 already exists.").arg(name));
            continue;
        }

        saveSessionToDefaultPath();

        const QString originalPath = sessionFilePath(originalName);
        const QString renamedPath = sessionFilePath(name);
        if (QFileInfo::exists(originalPath) && !QFile::rename(originalPath, renamedPath)) {
            QMessageBox::warning(this,
                                 tr("Rename Session"),
                                 tr("Could not rename session file %1.").arg(originalPath));
            continue;
        }

        const QPair<int, int> viewportAnchor = m_sessionViewportAnchors.take(originalName);
        const int scrollValue = m_sessionScrollValues.take(originalName);
        m_loadedSessions.remove(originalName);
        m_session->setName(name);
        m_loadedSessions.insert(name, m_session);
        for (auto it = m_paneSessionNames.begin(); it != m_paneSessionNames.end(); ++it) {
            if (it.value().compare(originalName, Qt::CaseInsensitive) == 0)
                it.value() = name;
        }
        for (auto it = m_paneSessionTabs.begin(); it != m_paneSessionTabs.end(); ++it) {
            QStringList names = it.value();
            for (QString& paneName : names) {
                if (paneName.compare(originalName, Qt::CaseInsensitive) == 0)
                    paneName = name;
            }
            it.value() = names;
        }
        if (viewportAnchor.first >= 0)
            m_sessionViewportAnchors.insert(name, viewportAnchor);
        if (scrollValue >= 0)
            m_sessionScrollValues.insert(name, scrollValue);

        m_widgets.display->viewport()->update();
        updatePaneTabBars();
        {
            QJsonObject json;
            m_session->serialize(json);
            QFile renamedFile(sessionFilePath(name));
            if (renamedFile.open(QIODevice::WriteOnly))
                renamedFile.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
        }
        saveSessionLayout();
        return;
    }
}

void MainWindow::closeCurrentSession()
{
    if (m_session == nullptr)
        return;

    QStringList names = paneSessionNames(m_widgets.display);
    if (names.size() <= 1) {
        if (splitPaneDisplays().size() <= 1) {
            const QString closingName = m_session->name();
            if (shouldDeleteSessionFileOnClose(closingName, m_session))
                QFile::remove(sessionFilePath(closingName));
            else if (sessionHasPersistableContent(m_session))
                saveSessionToDefaultPath();

            // A child window exists only to host its current session set. When
            // the last tab closes there, close the child window instead of
            // creating a replacement untitled session as the primary window
            // does.
            if (this != primaryMainWindow()) {
                close();
                return;
            }

            Session* replacementSession = createUntitledSession(true);
            QStringList paneNames = paneSessionNames(m_widgets.display);
            paneNames.removeAll(closingName);
            if (replacementSession != nullptr && !paneNames.contains(replacementSession->name(), Qt::CaseInsensitive))
                paneNames.append(replacementSession->name());
            m_paneSessionTabs.insert(m_widgets.display, paneNames);
            m_paneSessionNames.insert(m_widgets.display, replacementSession ? replacementSession->name() : QString());

            Session* closingSession = m_loadedSessions.take(closingName);
            m_sessionViewportAnchors.remove(closingName);
            m_sessionScrollValues.remove(closingName);
            delete closingSession;
            updatePaneLoadedSessionCounts();
            saveSessionLayout(false);
            return;
        }

        QWidget* pane = paneWidgetForDisplay(m_widgets.display);
        QSplitter* parentSplitter = qobject_cast<QSplitter*>(pane ? pane->parentWidget() : nullptr);
        if (parentSplitter == nullptr)
            return;
        const int closingPaneIndex = parentSplitter->indexOf(pane);
        const int nextPaneIndex = closingPaneIndex + 1 < parentSplitter->count()
            ? closingPaneIndex + 1
            : qMax(0, closingPaneIndex - 1);
        QWidget* nextPane = parentSplitter->widget(nextPaneIndex);
        ResultDisplay* nextDisplay = nextPane ? nextPane->findChild<ResultDisplay*>() : nullptr;
        Editor* nextEditor = nextPane ? nextPane->findChild<Editor*>() : nullptr;
        if (nextDisplay == nullptr || nextEditor == nullptr || pane == nextPane)
            return;

        captureEditorTextInCurrentSession();
        QTabBar* closingTabBar = displayTabBar(m_widgets.display);
        m_paneSessionNames.remove(m_widgets.display);
        m_paneSessionTabs.remove(m_widgets.display);
        m_paneTabBars.remove(m_widgets.display);
        if (closingTabBar != nullptr)
            m_tabBarDisplays.remove(closingTabBar);
        m_widgets.display = nextDisplay;
        m_widgets.editor = nextEditor;
        m_copyWidget = nextEditor;
        deletePaneAfterSessionTabDrag(pane);
        normalizeSplitContainerTree();
        updatePaneLoadedSessionCounts();
        Session* nextSession = m_loadedSessions.value(m_paneSessionNames.value(nextDisplay), nullptr);
        if (nextSession != nullptr)
            activateSession(nextSession);
        saveSessionLayout(false);
        return;
    }

    const QString closingName = m_session->name();
    if (shouldDeleteSessionFileOnClose(closingName, m_session))
        QFile::remove(sessionFilePath(closingName));
    else if (sessionHasPersistableContent(m_session))
        saveSessionToDefaultPath();

    names.sort(Qt::CaseInsensitive);
    const int closingIndex = names.indexOf(closingName);
    const int nextIndex = closingIndex >= 0 && closingIndex + 1 < names.size()
        ? closingIndex + 1
        : qMax(0, closingIndex - 1);
    const QString nextName = names.value(nextIndex);
    Session* nextSession = m_loadedSessions.value(nextName, nullptr);
    if (nextSession == nullptr || nextSession == m_session)
        return;

    QStringList paneNames = paneSessionNames(m_widgets.display);
    paneNames.removeAll(closingName);
    m_paneSessionTabs.insert(m_widgets.display, paneNames);
    activateSession(nextSession);
    updatePaneLoadedSessionCounts();
    saveSessionLayout(false);
}

void MainWindow::closeCurrentPane()
{
    if (m_widgets.display == nullptr || m_widgets.editor == nullptr || m_session == nullptr)
        return;

    captureEditorTextInCurrentSession();

    ResultDisplay* closingDisplay = m_widgets.display;
    QWidget* closingPane = paneWidgetForDisplay(closingDisplay);
    if (closingPane == nullptr)
        return;

    QStringList closingNames = paneSessionNames(closingDisplay);
    if (closingNames.isEmpty() && m_session != nullptr)
        closingNames.append(m_session->name());

    const auto saveLoadedSession = [this](const QString& name) {
        Session* session = m_loadedSessions.value(name, nullptr);
        if (session == nullptr)
            return;
        if (shouldDeleteSessionFileOnClose(name, session)) {
            QFile::remove(sessionFilePath(name));
            return;
        }
        if (!sessionHasPersistableContent(session))
            return;

        QJsonObject json;
        session->serialize(json);
        QFile file(sessionFilePath(name));
        if (file.open(QIODevice::WriteOnly))
            file.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
    };

    for (const QString& name : closingNames)
        saveLoadedSession(name);

    const QList<ResultDisplay*> displaysBeforeClose = splitPaneDisplays();
    if (displaysBeforeClose.size() <= 1) {
        Session* replacementSession = createUntitledSession(true);
        const QString replacementName = replacementSession ? replacementSession->name() : QString();
        m_paneSessionTabs.insert(closingDisplay, replacementName.isEmpty() ? QStringList() : QStringList(replacementName));
        m_paneSessionNames.insert(closingDisplay, replacementName);

        for (const QString& name : closingNames) {
            if (name == replacementName)
                continue;
            Session* session = m_loadedSessions.take(name);
            m_sessionViewportAnchors.remove(name);
            m_sessionScrollValues.remove(name);
            delete session;
        }

        if (replacementSession != nullptr)
            activateSession(replacementSession);
        updatePaneLoadedSessionCounts();
        saveSessionLayout(false);
        return;
    }

    const int closingDisplayIndex = displaysBeforeClose.indexOf(closingDisplay);
    ResultDisplay* nextDisplay = nullptr;
    if (closingDisplayIndex >= 0 && closingDisplayIndex + 1 < displaysBeforeClose.size())
        nextDisplay = displaysBeforeClose.at(closingDisplayIndex + 1);
    else if (closingDisplayIndex > 0)
        nextDisplay = displaysBeforeClose.at(closingDisplayIndex - 1);
    else {
        for (ResultDisplay* display : displaysBeforeClose) {
            if (display != closingDisplay) {
                nextDisplay = display;
                break;
            }
        }
    }

    if (nextDisplay == nullptr)
        return;

    Editor* nextEditor = nextDisplay->parentWidget()
        ? nextDisplay->parentWidget()->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly)
        : nullptr;
    Session* nextSession = m_loadedSessions.value(m_paneSessionNames.value(nextDisplay), nullptr);
    if (nextEditor == nullptr || nextSession == nullptr)
        return;

    m_paneSessionNames.remove(closingDisplay);
    m_paneSessionTabs.remove(closingDisplay);
    QTabBar* closingTabBar = displayTabBar(closingDisplay);
    m_paneTabBars.remove(closingDisplay);
    if (closingTabBar != nullptr)
        m_tabBarDisplays.remove(closingTabBar);
    m_widgets.display = nextDisplay;
    m_widgets.editor = nextEditor;
    m_copyWidget = nextEditor;

    deletePaneAfterSessionTabDrag(closingPane);
    normalizeSplitContainerTree();

    const auto referencedByRemainingPanes = [this](const QString& name) {
        for (ResultDisplay* display : splitPaneDisplays()) {
            const QStringList names = paneSessionNames(display);
            if (names.contains(name, Qt::CaseInsensitive))
                return true;
        }
        return false;
    };

    activateSession(nextSession);

    for (const QString& name : closingNames) {
        if (referencedByRemainingPanes(name))
            continue;
        Session* session = m_loadedSessions.take(name);
        m_sessionViewportAnchors.remove(name);
        m_sessionScrollValues.remove(name);
        if (session != m_session)
            delete session;
    }

    updatePaneLoadedSessionCounts();
    updatePaneEditorCursorVisibility();
    saveSessionLayout(false);
}

void MainWindow::deleteCurrentSession()
{
    if (m_session == nullptr)
        return;

    QMessageBox confirmation(this);
    confirmation.setIcon(QMessageBox::Question);
    confirmation.setWindowTitle(tr("Delete Session"));
    confirmation.setText(tr("Are you sure you want to delete this session?"));
    confirmation.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    confirmation.setDefaultButton(QMessageBox::No);
    confirmation.setEscapeButton(QMessageBox::No);
    QShortcut deleteSessionEscape(QKeySequence(Qt::Key_Escape), &confirmation);
    connect(&deleteSessionEscape, &QShortcut::activated, &confirmation, &QMessageBox::reject);
    if (confirmation.exec() != QMessageBox::Yes)
        return;

    const QString deletingName = m_session->name();
    const QString deletingPath = sessionFilePath(deletingName);
    Session* deletingSession = m_session;

    m_loadedSessions.remove(deletingName);
    m_sessionViewportAnchors.remove(deletingName);
    m_sessionScrollValues.remove(deletingName);
    m_session = nullptr;
    m_evaluator = nullptr;

    if (QFileInfo::exists(deletingPath))
        QFile::remove(deletingPath);

    Session* nextSession = nullptr;
    bool createdReplacementSession = false;
    if (m_loadedSessions.isEmpty()) {
        const QString name = firstAvailableUntitledSessionName(m_loadedSessions);
        nextSession = new Session();
        nextSession->setName(name);
        m_loadedSessions.insert(name, nextSession);
        createdReplacementSession = true;
    } else {
        QStringList names = m_loadedSessions.keys();
        names.sort(Qt::CaseInsensitive);
        nextSession = m_loadedSessions.value(names.first(), nullptr);
    }

    updatePaneLoadedSessionCounts();
    activateSession(nextSession);
    for (auto it = m_paneSessionNames.begin(); it != m_paneSessionNames.end(); ++it) {
        if (it.value() == deletingName)
            it.value() = nextSession->name();
    }
    for (auto it = m_paneSessionTabs.begin(); it != m_paneSessionTabs.end(); ++it) {
        QStringList names = it.value();
        names.removeAll(deletingName);
        if (names.isEmpty() && nextSession != nullptr)
            names.append(nextSession->name());
        it.value() = names;
    }
    if (createdReplacementSession)
        applyUserDefinitions();

    delete deletingSession;
    m_conditions.autoAns = false;
    saveSessionLayout(false);
}

void MainWindow::showLoadedSessionsMenu(const QPoint& globalPos)
{
    const QStringList paneNames = paneSessionNames(m_widgets.display);
    if (paneNames.isEmpty())
        return;

    QMenu menu(this);
    QStringList names = paneNames;
    names.sort(Qt::CaseInsensitive);
    for (const QString& name : names) {
        QAction* action = menu.addAction(name);
        action->setCheckable(true);
        action->setChecked(m_session != nullptr && name == m_session->name());
        action->setData(name);
    }

    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    applyMenuSurface(&menu, surfaces.headersAndBorders, surfaces.inputs);
    QAction* selectedAction = menu.exec(globalPos);
    if (selectedAction == nullptr)
        return;

    const QString name = selectedAction->data().toString();
    Session* selectedSession = m_loadedSessions.value(name, nullptr);
    if (selectedSession == nullptr || selectedSession == m_session)
        return;

    saveSessionToDefaultPath();

    activateSession(selectedSession);
    saveSessionLayout(false);
}

void MainWindow::clearEditor()
{
    m_widgets.editor->clear();
    if (m_widgets.bitField)
        m_widgets.bitField->clear();
    m_widgets.editor->setFocus();
}

void MainWindow::clearEditorAndBitfield()
{
    clearEditor();
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

void MainWindow::showCustomThemeDialog()
{
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("ThemeDialog"));
    dialog.setWindowTitle(tr("Theme"));

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QWidget* themeWidget = new QWidget(&dialog);
    QHBoxLayout* themeLayout = new QHBoxLayout(themeWidget);
    themeLayout->setContentsMargins(0, 0, 0, 0);
    themeLayout->setSpacing(10);

    QGroupBox* lightThemesGroup = new QGroupBox(tr("Light Themes"), themeWidget);
    QVBoxLayout* lightThemesLayout = new QVBoxLayout(lightThemesGroup);
    QListWidget* lightThemeList = new QListWidget(lightThemesGroup);
    lightThemeList->setObjectName(QStringLiteral("LightThemeList"));
    lightThemesLayout->addWidget(lightThemeList);
    themeLayout->addWidget(lightThemesGroup);

    QGroupBox* darkThemesGroup = new QGroupBox(tr("Dark Themes"), themeWidget);
    QVBoxLayout* darkThemesLayout = new QVBoxLayout(darkThemesGroup);
    QListWidget* darkThemeList = new QListWidget(darkThemesGroup);
    darkThemeList->setObjectName(QStringLiteral("DarkThemeList"));
    darkThemesLayout->addWidget(darkThemeList);
    themeLayout->addWidget(darkThemesGroup);
    layout->addWidget(themeWidget);

    QGroupBox* previewGroup = new QGroupBox(tr("Preview"), &dialog);
    QVBoxLayout* previewGroupLayout = new QVBoxLayout(previewGroup);

    QWidget* previewWidget = new QWidget(previewGroup);
    previewWidget->setObjectName(QStringLiteral("ThemePreview"));
    QVBoxLayout* previewLayout = new QVBoxLayout(previewWidget);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    previewLayout->setSpacing(0);

    QWidget* mainPreviewWidget = new QWidget(previewWidget);
    QHBoxLayout* mainPreviewLayout = new QHBoxLayout(mainPreviewWidget);
    mainPreviewLayout->setContentsMargins(0, 0, 0, 0);
    mainPreviewLayout->setSpacing(0);
    QPlainTextEdit* preview = new QPlainTextEdit(mainPreviewWidget);
    preview->setReadOnly(true);
    preview->setFrameShape(QFrame::NoFrame);
    preview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    preview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    preview->setPlainText(
        QStringLiteral("cos(2 · π + (3 / 2) · π)\n"
                       "= 0.98512127610111389784\n"
                       "\n"
                       "average({1;2;3;4;5}) ? Calculate the average of the list\n"
                       "= 3\n"
                       "\n"
                       "distance = 42 [km] + 195 [m] → [km] ? Marathon distance\n"
                       "= 42.195 km"
                    )
                );
    auto previewHighlighter = new SyntaxHighlighter(preview);
    QWidget* previewScrollbarTrack = new QWidget(mainPreviewWidget);
    const int previewScrollbarWidth = m_widgets.display
        ? m_widgets.display->verticalScrollBar()->sizeHint().width()
        : previewScrollbarTrack->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    previewScrollbarTrack->setFixedWidth(previewScrollbarWidth);
    QVBoxLayout* previewScrollbarLayout = new QVBoxLayout(previewScrollbarTrack);
    previewScrollbarLayout->setContentsMargins(0, 0, 0, 0);
    previewScrollbarLayout->setSpacing(0);
    QWidget* previewScrollbar = new QWidget(previewScrollbarTrack);
    previewScrollbar->setFixedHeight(28);
    previewScrollbarLayout->addWidget(previewScrollbar);
    previewScrollbarLayout->addStretch();
    mainPreviewLayout->addWidget(preview);
    mainPreviewLayout->addWidget(previewScrollbarTrack);

    previewLayout->addWidget(mainPreviewWidget);

    Editor* editorPreview = new Editor(previewWidget);
    editorPreview->setReadOnly(true);
    editorPreview->setFrameShape(QFrame::NoFrame);
    editorPreview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editorPreview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editorPreview->setText(QStringLiteral("sqrt(144) + sin(π / 2)"));
    editorPreview->setCustomCursorVisible(true);
    const int editorPreviewHeight = m_widgets.editor
        ? m_widgets.editor->height()
        : editorPreview->fontMetrics().lineSpacing() + 46;
    editorPreview->setFixedHeight(editorPreviewHeight);
    previewLayout->addWidget(editorPreview);
    previewGroupLayout->addWidget(previewWidget);
    layout->addWidget(previewGroup);

    QGroupBox* rolesGroup = new QGroupBox(tr("Colors"), &dialog);
    QGridLayout* roleLayout = new QGridLayout(rolesGroup);
    roleLayout->setHorizontalSpacing(10);
    roleLayout->setVerticalSpacing(6);
    layout->addWidget(rolesGroup);
    QWidget* rolesWidget = rolesGroup;

    QStringList schemeNames = ColorScheme::enumerate();
    QString selectedSchemeName = m_settings->colorScheme == QLatin1String("Custom")
        ? QString()
        : m_settings->colorScheme;
    bool isCustomScheme = selectedSchemeName.isEmpty();
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
    QPushButton* okButton = nullptr;
    const auto applyColorsToControls = [&colorsByRole, &roleButtons, roleEntries]() {
        for (const auto& roleEntry : roleEntries)
            updateColorButtonStyle(roleButtons.value(roleEntry.second), colorsByRole.value(roleEntry.second));
    };
    const auto setColorsFromScheme = [&colorsByRole, roleEntries](const ColorScheme& scheme) {
        for (const auto& roleEntry : roleEntries)
            colorsByRole[roleEntry.second] = scheme.colorForRole(roleEntry.second);
    };
    const auto applyPreview = [&colorsByRole,
                               preview,
                               previewHighlighter,
                               previewWidget,
                               previewScrollbarTrack,
                               previewScrollbar,
                               editorPreview]() {
        QJsonObject object;
        const auto roles = ColorScheme::roleNames();
        for (const auto& roleEntry : roles)
            object.insert(roleEntry.first, colorsByRole.value(roleEntry.second).name());
        const ColorScheme scheme = ColorScheme::fromJsonObject(object);
        const GeneratedThemeSurfaces surfaces = generatedSurfaceColorsForScheme(scheme);
        previewWidget->setAutoFillBackground(true);
        previewWidget->setAttribute(Qt::WA_StyledBackground, true);
        previewWidget->setStyleSheet(QStringLiteral("QWidget#ThemePreview { background-color: %1; }")
                                         .arg(surfaces.result.background.name()));
        previewHighlighter->setColorScheme(ColorScheme::fromJsonObject(scheme.toJsonObject()));
        const QPalette resultPalette = paletteForThemeSurface(preview->palette(), surfaces.result);
        preview->setPalette(resultPalette);
        preview->viewport()->setPalette(resultPalette);
        preview->setStyleSheet(QStringLiteral("QPlainTextEdit { background-color: %1; color: %2; }")
                                   .arg(surfaces.result.background.name(),
                                        surfaces.result.foreground.name()));
        previewHighlighter->rehighlight();
        const ThemeScrollBarColors resultScrollBars = scrollBarColorsForSurfaceIndex(surfaces, 1);
        previewScrollbarTrack->setStyleSheet(QStringLiteral("background-color: %1;")
                                                 .arg(resultScrollBars.track.name()));
        previewScrollbar->setStyleSheet(QStringLiteral("background-color: %1; border-radius: 4px;")
                                            .arg(resultScrollBars.thumb.name()));

        editorPreview->setThemeSurfaceColor(surfaces.editorAndLists.background,
                                            surfaces.result.background);
        editorPreview->setThemePreviewColorScheme(scheme);
        editorPreview->setThemePrimaryColor(surfaces.primary.background, true);
        editorPreview->setCustomCursorVisible(true);
        editorPreview->rehighlight();
    };
    const auto updateThemeListHeight = [](QListWidget* list) {
        const int visibleRows = qMin(list->count(), 7);
        const int rowHeight = list->sizeHintForRow(0) > 0 ? list->sizeHintForRow(0) : list->fontMetrics().height() + 6;
        list->setMaximumHeight(rowHeight * visibleRows + list->frameWidth() * 2);
    };
    const auto restoreThemeListScroll = [&dialog](QListWidget* list, int scrollValue) {
        if (list == nullptr || list->verticalScrollBar() == nullptr)
            return;

        list->verticalScrollBar()->setValue(scrollValue);
        QPointer<QListWidget> guardedList(list);
        QTimer::singleShot(0, &dialog, [guardedList, scrollValue]() {
            if (guardedList != nullptr && guardedList->verticalScrollBar() != nullptr)
                guardedList->verticalScrollBar()->setValue(scrollValue);
        });
    };
    QHash<QListWidget*, int> pressedThemeListScrollValues;
    const auto rememberThemeListScroll = [&](QListWidget* list) {
        if (list != nullptr && list->verticalScrollBar() != nullptr)
            pressedThemeListScrollValues.insert(list, list->verticalScrollBar()->value());
    };
    const auto restorePressedThemeListScroll = [&](QListWidget* list) {
        if (!pressedThemeListScrollValues.contains(list))
            return;

        restoreThemeListScroll(list, pressedThemeListScrollValues.take(list));
    };
    const auto clearThemeListSelection = [&](QListWidget* list) {
        if (list == nullptr)
            return;

        const int scrollValue = list->verticalScrollBar() ? list->verticalScrollBar()->value() : 0;
        const QSignalBlocker blocker(list);
        list->clearSelection();
        restoreThemeListScroll(list, scrollValue);
    };
    connect(lightThemeList, &QListWidget::itemPressed, &dialog, [&, lightThemeList](QListWidgetItem*) {
        rememberThemeListScroll(lightThemeList);
    });
    connect(darkThemeList, &QListWidget::itemPressed, &dialog, [&, darkThemeList](QListWidgetItem*) {
        rememberThemeListScroll(darkThemeList);
    });
    connect(lightThemeList, &QListWidget::itemClicked, &dialog, [&, lightThemeList](QListWidgetItem*) {
        restorePressedThemeListScroll(lightThemeList);
    });
    connect(darkThemeList, &QListWidget::itemClicked, &dialog, [&, darkThemeList](QListWidgetItem*) {
        restorePressedThemeListScroll(darkThemeList);
    });

    const auto populateThemeList = [&](QListWidget* list, ColorSchemeFilter filter) {
        const QSignalBlocker blocker(list);
        list->clear();
        for (const auto& schemeName : schemeNames) {
            const ColorScheme scheme = ColorScheme::loadByName(schemeName);
            if (!scheme.isValid() || !colorSchemeMatchesFilter(scheme, filter))
                continue;
            auto item = new QListWidgetItem(schemeName, list);
            item->setData(Qt::UserRole, schemeName);
        }
        updateThemeListHeight(list);
    };
    const auto showSelectedTheme = [&](QListWidget* list) {
        if (selectedSchemeName.isEmpty())
            return false;
        for (int row = 0; row < list->count(); ++row) {
            QListWidgetItem* item = list->item(row);
            if (item->data(Qt::UserRole).toString() != selectedSchemeName)
                continue;
            const QSignalBlocker blocker(list);
            list->setCurrentItem(item);
            item->setSelected(true);
            list->setFocus(Qt::OtherFocusReason);
            return true;
        }
        return false;
    };
    const auto populateThemeLists = [&]() {
        schemeNames = ColorScheme::enumerate();
        populateThemeList(lightThemeList, ColorSchemeFilter::Light);
        populateThemeList(darkThemeList, ColorSchemeFilter::Dark);
        if (showSelectedTheme(lightThemeList))
            darkThemeList->clearSelection();
        else if (showSelectedTheme(darkThemeList))
            lightThemeList->clearSelection();
    };

    int roleIndex = 0;
    constexpr int roleRowsPerColumn = 4;
    for (const auto& roleEntry : roleEntries) {
        const ColorScheme::Role role = roleEntry.second;
        QLabel* roleLabel = new QLabel(colorSchemeRoleLabel(role), rolesWidget);
        QPushButton* colorButton = new QPushButton(rolesWidget);
        colorButton->setObjectName(QStringLiteral("ThemeColorButton_%1").arg(roleEntry.first));
        updateColorButtonStyle(colorButton, colorsByRole.value(role));
        roleButtons.insert(role, colorButton);
        const int row = roleIndex % roleRowsPerColumn;
        const int column = (roleIndex / roleRowsPerColumn) * 2;
        roleLayout->addWidget(colorButton, row, column);
        roleLayout->addWidget(roleLabel, row, column + 1);
        connect(colorButton, &QPushButton::clicked, &dialog, [&, role]() {
            const QColor initial = colorsByRole.value(role);
            const QColor chosen = QColorDialog::getColor(initial, &dialog, tr("Select color for %1").arg(colorSchemeRoleLabel(role)));
            if (!chosen.isValid())
                return;
            colorsByRole[role] = chosen;
            selectedSchemeName.clear();
            isCustomScheme = true;
            lightThemeList->clearSelection();
            darkThemeList->clearSelection();
            updateColorButtonStyle(roleButtons.value(role), chosen);
            if (okButton != nullptr)
                okButton->setEnabled(false);
            applyPreview();
        });
        ++roleIndex;
    }

    applyPreview();
    populateThemeLists();

    const auto handleThemeSelection = [&](QListWidget* currentList, QListWidget* otherList, QListWidgetItem* current) {
        if (!current)
            return;
        clearThemeListSelection(otherList);
        const QString schemeName = current->data(Qt::UserRole).toString();
        const ColorScheme scheme = ColorScheme::loadByName(schemeName);
        if (!scheme.isValid())
            return;
        selectedSchemeName = schemeName;
        isCustomScheme = false;
        setColorsFromScheme(scheme);
        applyColorsToControls();
        if (okButton != nullptr)
            okButton->setEnabled(true);
        applyPreview();
        restorePressedThemeListScroll(currentList);
    };
    connect(lightThemeList, &QListWidget::currentItemChanged, &dialog, [&](QListWidgetItem* current) {
        handleThemeSelection(lightThemeList, darkThemeList, current);
    });
    connect(darkThemeList, &QListWidget::currentItemChanged, &dialog, [&](QListWidgetItem* current) {
        handleThemeSelection(darkThemeList, lightThemeList, current);
    });

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    okButton = buttons->button(QDialogButtonBox::Ok);
    QPushButton* importButton = buttons->addButton(tr("Import..."), QDialogButtonBox::ActionRole);
    QPushButton* exportButton = buttons->addButton(tr("Export..."), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    const auto applyCurrentTheme = [&]() {
        if (isCustomScheme || selectedSchemeName.isEmpty()) {
            QJsonObject object;
            for (const auto& roleEntry : roleEntries)
                object.insert(roleEntry.first, colorsByRole.value(roleEntry.second).name());
            m_settings->customColorSchemeJson = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
            m_settings->colorScheme = QStringLiteral("Custom");
        } else {
            m_settings->colorScheme = selectedSchemeName;
        }
        for (const QPointer<MainWindow>& ptr : allMainWindows()) {
            if (MainWindow* window = ptr.data()) {
                window->updateColorSchemeActionState();
                emit window->colorSchemeChanged();
            }
        }
    };
    const auto writableColorSchemesPath = [&]() {
        const auto colorSchemePaths = ColorScheme::fileSystemSearchPaths();
        const QString path = colorSchemePaths.isEmpty() ? QString() : colorSchemePaths.constFirst();
        if (!path.isEmpty())
            QDir().mkpath(path);
        return path;
    };

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
        const QFileInfo importFileInfo(filePath);
        const QString themeName = importFileInfo.completeBaseName();
        if (ColorScheme::isBuiltInName(themeName)) {
            QMessageBox::critical(
                this,
                tr("Error"),
                tr("Can't import theme \"%1\" because it conflicts with a built-in theme.").arg(themeName));
            return;
        }
        const QString colorSchemesPath = writableColorSchemesPath();
        if (colorSchemesPath.isEmpty()) {
            QMessageBox::critical(this, tr("Error"), tr("Can't find a writable theme folder."));
            return;
        }
        const QString destinationPath = QDir(colorSchemesPath).filePath(themeName + QLatin1String(".json"));
        if (QFileInfo::exists(destinationPath)) {
            const QMessageBox::StandardButton answer = QMessageBox::question(
                this,
                tr("Overwrite Theme"),
                tr("A custom theme named \"%1\" already exists. Do you want to overwrite it?").arg(themeName),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);
            if (answer != QMessageBox::Yes)
                return;
            const QFileInfo destinationFileInfo(destinationPath);
            if (destinationFileInfo.absoluteFilePath() != importFileInfo.absoluteFilePath()
                    && !QFile::remove(destinationPath)) {
                QMessageBox::critical(this, tr("Error"), tr("Can't overwrite theme file %1").arg(destinationPath));
                return;
            }
        }
        if (QFileInfo(destinationPath).absoluteFilePath() != importFileInfo.absoluteFilePath()
                && !QFile::copy(filePath, destinationPath)) {
            QMessageBox::critical(this, tr("Error"), tr("Can't copy theme file to %1").arg(destinationPath));
            return;
        }
        for (const auto& roleEntry : roleEntries) {
            const QColor color = importedScheme.colorForRole(roleEntry.second);
            colorsByRole[roleEntry.second] = color;
        }
        for (const auto& roleEntry : roleEntries)
            updateColorButtonStyle(roleButtons.value(roleEntry.second),
                                   colorsByRole.value(roleEntry.second));
        selectedSchemeName = themeName;
        isCustomScheme = false;
        if (okButton != nullptr)
            okButton->setEnabled(true);
        populateThemeLists();
        applyPreview();
    });
    connect(exportButton, &QPushButton::clicked, this, [&, roleEntries]() {
        QString colorSchemesPath;
        if (!selectedSchemeName.isEmpty()) {
            const QString selectedSchemePath = ColorScheme::filePathForName(selectedSchemeName);
            if (!selectedSchemePath.startsWith(QLatin1Char(':'))) {
                const QFileInfo selectedSchemeInfo(selectedSchemePath);
                if (selectedSchemeInfo.exists())
                    colorSchemesPath = selectedSchemeInfo.absolutePath();
            }
        }
        const auto colorSchemePaths = ColorScheme::fileSystemSearchPaths();
        if (colorSchemesPath.isEmpty()) {
            for (const auto& path : colorSchemePaths) {
                if (QDir(path).exists()) {
                    colorSchemesPath = path;
                    break;
                }
            }
        }
        if (colorSchemesPath.isEmpty() && !colorSchemePaths.isEmpty())
            colorSchemesPath = colorSchemePaths.constFirst();
        if (!colorSchemesPath.isEmpty())
            QDir().mkpath(colorSchemesPath);
        QString filePath = QFileDialog::getSaveFileName(
            this, tr("Export Theme"), colorSchemesPath,
            tr("Theme file (*.json);;All files (*)"));
        if (filePath.isEmpty())
            return;
        if (!filePath.endsWith(QLatin1String(".json"), Qt::CaseInsensitive))
            filePath += QLatin1String(".json");
        const QFileInfo exportFileInfo(filePath);
        if (ColorScheme::isBuiltInName(exportFileInfo.completeBaseName())) {
            QMessageBox::critical(
                this,
                tr("Error"),
                tr("Can't export theme as \"%1\" because it conflicts with a built-in theme.").arg(exportFileInfo.completeBaseName()));
            return;
        }
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::critical(this, tr("Error"), tr("Can't write to file %1").arg(filePath));
            return;
        }
        QJsonObject object;
        for (const auto& roleEntry : roleEntries)
            object.insert(roleEntry.first, colorsByRole.value(roleEntry.second).name());
        file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
        file.close();
        const ColorScheme exportedScheme = ColorScheme::loadFromFile(filePath);
        if (exportedScheme.isValid()) {
            selectedSchemeName = exportFileInfo.completeBaseName();
            isCustomScheme = false;
            populateThemeLists();
        }
    });

    dialog.setFixedSize(dialog.sizeHint().expandedTo(QSize(720, 560)));

    if (dialog.exec() != QDialog::Accepted) {
        m_actions.settingsDisplayColorSchemeCustom->setChecked(m_settings->colorScheme == QLatin1String("Custom"));
        return;
    }

    applyCurrentTheme();
}

void MainWindow::selectEditorExpression()
{
    activateWindow();
    m_widgets.editor->selectAll();
    m_widgets.editor->setFocus();
}

void MainWindow::hideStateLabel()
{
    if (m_widgets.state->isVisible()
        && m_widgets.state->text().contains(QStringLiteral("Current result:"))) {
        m_currentResultPreviewDismissed = true;
        if (m_widgets.editor != nullptr)
            m_widgets.editor->dismissCurrentAutoCalc();
    }
    m_widgets.state->hide();
}

void MainWindow::hideCurrentResultPreview()
{
    if (m_widgets.state == nullptr
        || !m_widgets.state->isVisible()
        || !m_widgets.state->text().contains(QStringLiteral("Current result:"))) {
        return;
    }

    hideStateLabel();
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

    QPointer<MainWindow> windowGuard(this);
    QThread* thread = QThread::create([windowGuard, fname, merge]() {
        QJsonObject json;
        const bool ok = readValidSessionJson(fname, &json);

        if (!windowGuard)
            return;

        QMetaObject::invokeMethod(windowGuard.data(), [windowGuard, ok, json, merge]() mutable {
            if (!windowGuard)
                return;
            if (!ok) {
                QMessageBox::critical(windowGuard.data(), MainWindow::tr("Error"),
                                      MainWindow::tr("Can't read the selected session file."));
                return;
            }

            MainWindow* window = windowGuard.data();
            Session loadedSession;
            loadedSession.deSerialize(json, false);
            if (merge) {
                const QList<HistoryEntry> history = loadedSession.historyToList();
                for (const HistoryEntry& entry : history)
                    window->m_session->addHistoryEntry(entry);
                for (const Variable& variable : loadedSession.variablesToList()) {
                    if (variable.type() != Variable::BuiltIn)
                        window->m_session->addVariable(variable);
                }
                for (const UserFunction& function : loadedSession.UserFunctionsToList())
                    window->m_session->addUserFunction(function);
                for (const UserUnit& unit : loadedSession.userUnitsToList())
                    window->m_session->addUserUnit(unit);
            } else {
                *window->m_session = loadedSession;
                window->restoreEditorTextFromCurrentSession();
            }

            emit window->historyChanged();
            emit window->variablesChanged();
            emit window->functionsChanged();
            emit window->unitsChanged();
            window->m_conditions.autoAns = !window->m_session->historyIsEmpty();
        }, Qt::QueuedConnection);
    });
    {
        QMutexLocker locker(&asyncSessionIoMutex());
        asyncSessionIoThreads().append(thread);
    }
    QObject::connect(thread, &QThread::finished, thread, [thread]() {
        unregisterAsyncSessionIoThread(thread);
        thread->deleteLater();
    });
    thread->start();

}

void MainWindow::wrapSelection()
{
    m_widgets.editor->wrapSelection();
}

void MainWindow::saveSessionDialog()
{
    QString filters = tr("SpeedCrunch Sessions (*.json);;All Files (*)");
    const QString sessionBaseName =
        QString(QLatin1String("session-%1")).arg(QDateTime::currentDateTime().toString(QLatin1String("yyyy_MM_dd-HH_mm_ss")));
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
    captureEditorTextInCurrentSession();
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
            historyEntry.setRenderedLines(renderedLinesForHistoryEntry(historyEntry, m_settings, m_evaluator));
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
                                               int* importedVariables, int* importedFunctions, int* importedUnits,
                                               int* ignoredLines, QList<int>* ignoredLineNumbers,
                                               bool dryRun)
{
    const QString globalVariableTag = tr("Global User Variable");
    const QString globalFunctionTag = tr("Global User Function");
    const QString globalUnitTag = tr("Global User Unit");
    int localImportedVariables = 0;
    int localImportedFunctions = 0;
    int localImportedUnits = 0;
    int localIgnoredLines = 0;
    QList<int> localIgnoredLineNumbers;
    Session sessionBackup;
    bool autoAnsBackup = false;

    if (dryRun && m_session) {
        sessionBackup = *m_session;
        autoAnsBackup = m_conditions.autoAns;
    }
    m_evaluator->setAllowGlobalUserDefinitionsOverride(true);

    const auto hasGlobalTag = [](const QString& description, const QString& tag) {
        return description.contains(tag);
    };

    const QList<Variable> existingVariables = m_evaluator->getUserDefinedVariables();
    for (const Variable& variable : existingVariables) {
        if (hasGlobalTag(variable.description(), globalVariableTag))
            m_evaluator->unsetVariable(variable.identifier());
    }

    const QList<UserFunction> existingFunctions = m_evaluator->getUserFunctions();
    for (const UserFunction& function : existingFunctions) {
        if (hasGlobalTag(function.description(), globalFunctionTag))
            m_evaluator->unsetUserFunction(function.name());
    }

    const QList<UserUnit> existingUnits = m_evaluator->getUserUnits();
    for (const UserUnit& unit : existingUnits) {
        if (hasGlobalTag(unit.description(), globalUnitTag))
            m_evaluator->unsetUserUnit(unit.name());
    }

    m_evaluator->clearGlobalUserDefinitionRegistry();

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
        QString expressionWithoutDescription = expression;
        QString explicitDescription;
        splitAssignmentDescriptionForImport(
            expression,
            &expressionWithoutDescription,
            &explicitDescription);

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
            }
            if (target.isFunction && hasExistingUserFunction) {
                m_evaluator->unsetUserFunction(target.identifier);
            } else if (!target.isFunction && !target.isUnit && hasExistingUserFunction) {
                m_evaluator->unsetUserFunction(target.identifier);
            } else if (!target.isFunction && !target.isUnit && hasExistingUserVariable) {
                m_evaluator->unsetVariable(target.identifier);
            } else if (target.isUnit) {
                if (hasExistingUserVariable)
                    m_evaluator->unsetVariable(target.identifier);
                if (hasExistingUserFunction)
                    m_evaluator->unsetUserFunction(target.identifier);
                if (hasExistingUserUnit)
                    m_evaluator->unsetUserUnit(target.identifier);
            }
        }

        m_evaluator->setExpression(expressionWithoutDescription);
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
                    previousVariable.description(),
                    previousVariable.formattedValue());
            }
            if (hasPreviousUserFunction)
                m_evaluator->setUserFunction(previousFunction);
            if (hasExistingUserUnit)
                m_evaluator->setUserUnit(previousUnit);

            ++localIgnoredLines;
            localIgnoredLineNumbers.append(lineNumber);
            continue;
        }

        if (target.isFunction) {
            UserFunction taggedFunction;
            if (findUserFunctionByName(m_evaluator->getUserFunctions(), target.identifier, &taggedFunction)) {
                QString description = explicitDescription;
                if (description.isEmpty())
                    description = taggedFunction.description();
                if (!description.contains(globalFunctionTag))
                    description = description.isEmpty()
                        ? globalFunctionTag
                        : description + QStringLiteral(" · ") + globalFunctionTag;
                if (taggedFunction.description() != description)
                    taggedFunction.setDescription(description);
                m_evaluator->setUserFunction(taggedFunction);
                m_evaluator->registerGlobalUserFunction(target.identifier);
            }
        } else if (target.isUnit) {
            const UserUnit* existingUnit = m_evaluator->getUserUnit(target.identifier);
            if (existingUnit) {
                UserUnit taggedUnit = *existingUnit;
                QString description = explicitDescription;
                if (description.isEmpty())
                    description = taggedUnit.description();
                if (!description.contains(globalUnitTag))
                    description = description.isEmpty()
                        ? globalUnitTag
                        : description + QStringLiteral(" · ") + globalUnitTag;
                if (taggedUnit.description() != description)
                    taggedUnit.setDescription(description);
                m_evaluator->setUserUnit(taggedUnit);
                m_evaluator->registerGlobalUserUnit(target.identifier);
            }
        } else if (m_evaluator->hasVariable(target.identifier)
                   && !m_evaluator->isBuiltInVariable(target.identifier)) {
            const Variable importedVariable = m_evaluator->getVariable(target.identifier);
            QString description = explicitDescription;
            if (description.isEmpty())
                description = importedVariable.description();
            if (!description.contains(globalVariableTag)) {
                description = description.isEmpty()
                    ? globalVariableTag
                    : description + QStringLiteral(" · ") + globalVariableTag;
            }
            m_evaluator->setVariable(
                importedVariable.identifier(),
                importedVariable.value(),
                importedVariable.type(),
                description,
                importedVariable.formattedValue());
            m_evaluator->registerGlobalUserVariable(target.identifier);
        }

        if (target.isFunction)
            ++localImportedFunctions;
        else if (target.isUnit)
            ++localImportedUnits;
        else
            ++localImportedVariables;
    }

    if (!dryRun && (localImportedVariables > 0 || localImportedFunctions > 0 || localImportedUnits > 0)) {
        emit variablesChanged();
        emit functionsChanged();
        emit unitsChanged();
    }

    if (dryRun && m_session) {
        *m_session = sessionBackup;
        m_conditions.autoAns = autoAnsBackup;
    }
    m_evaluator->setAllowGlobalUserDefinitionsOverride(false);

    if (importedVariables)
        *importedVariables = localImportedVariables;
    if (importedFunctions)
        *importedFunctions = localImportedFunctions;
    if (importedUnits)
        *importedUnits = localImportedUnits;
    if (ignoredLines)
        *ignoredLines = localIgnoredLines;
    if (ignoredLineNumbers)
        *ignoredLineNumbers = localIgnoredLineNumbers;
}

void MainWindow::applyUserDefinitions(int* importedVariables,
                                      int* importedFunctions,
                                      int* importedUnits,
                                      int* ignoredLines,
                                      QList<int>* ignoredLineNumbers)
{
    const QString definitionsText = m_settings->startupUserDefinitions.trimmed();
    const int previousImportedVariables = importedVariables ? *importedVariables : 0;
    const int previousImportedFunctions = importedFunctions ? *importedFunctions : 0;
    const int previousImportedUnits = importedUnits ? *importedUnits : 0;
    const int previousIgnoredLines = ignoredLines ? *ignoredLines : 0;

    int totalImportedVariables = 0;
    int totalImportedFunctions = 0;
    int totalImportedUnits = 0;
    int maxIgnoredLines = 0;
    QSet<int> uniqueIgnoredLineNumbers;

    Session* previousSession = m_session;
    const bool previousAutoAns = m_conditions.autoAns;

    const QList<Session*> sessions = m_loadedSessions.values();
    for (Session* session : sessions) {
        if (session == nullptr)
            continue;

        m_session = session;
        m_evaluator = m_session->evaluator();
        m_evaluator->initializeBuiltInVariables();

        int sessionImportedVariables = 0;
        int sessionImportedFunctions = 0;
        int sessionImportedUnits = 0;
        int sessionIgnoredLines = 0;
        QList<int> sessionIgnoredLineNumbers;
        importUserDefinitionsFromText(
            definitionsText,
            true,
            &sessionImportedVariables,
            &sessionImportedFunctions,
            &sessionImportedUnits,
            &sessionIgnoredLines,
            &sessionIgnoredLineNumbers,
            false);

        totalImportedVariables += sessionImportedVariables;
        totalImportedFunctions += sessionImportedFunctions;
        totalImportedUnits += sessionImportedUnits;
        maxIgnoredLines = qMax(maxIgnoredLines, sessionIgnoredLines);
        for (int lineNumber : sessionIgnoredLineNumbers)
            uniqueIgnoredLineNumbers.insert(lineNumber);
    }

    m_session = previousSession;
    m_evaluator = m_session ? m_session->evaluator() : nullptr;
    if (m_session != nullptr)
        m_evaluator->initializeBuiltInVariables();
    m_conditions.autoAns = previousAutoAns;
    if (m_widgets.display != nullptr && m_session != nullptr)
        m_widgets.display->setSession(m_session);
    if (m_widgets.editor != nullptr && m_evaluator != nullptr)
        m_widgets.editor->setSession(m_session);

    if (importedVariables)
        *importedVariables = previousImportedVariables + totalImportedVariables;
    if (importedFunctions)
        *importedFunctions = previousImportedFunctions + totalImportedFunctions;
    if (importedUnits)
        *importedUnits = previousImportedUnits + totalImportedUnits;
    if (ignoredLines)
        *ignoredLines = previousIgnoredLines + maxIgnoredLines;
    if (ignoredLineNumbers) {
        QList<int> sortedIgnoredLineNumbers = uniqueIgnoredLineNumbers.values();
        std::sort(sortedIgnoredLineNumbers.begin(), sortedIgnoredLineNumbers.end());
        for (int lineNumber : sortedIgnoredLineNumbers) {
            if (!ignoredLineNumbers->contains(lineNumber))
                ignoredLineNumbers->append(lineNumber);
        }
    }

    emit variablesChanged();
    emit functionsChanged();
    emit unitsChanged();
}

void MainWindow::showUserDefinitionsImportDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("User Definitions"));
    dialog.setMinimumSize(640, 420);

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QLabel* info = new QLabel(
        tr("These definitions are global and are loaded into every session.\n"
           "They are immutable for sessions and override same-name definitions from the session editor.\n"
           "Enter one definition per line."),
        &dialog);
    info->setWordWrap(true);
    layout->addWidget(info);

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
    lineNumbers->setFixedWidth(36);
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

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal,
        &dialog);
    QPushButton* applyNowButton = buttons->addButton(tr("Apply"), QDialogButtonBox::ActionRole);
    QPushButton* validateButton = buttons->addButton(tr("Validate"), QDialogButtonBox::ActionRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, textEdit, &dialog]() {
        m_settings->startupUserDefinitions = textEdit->toPlainText();
        applyUserDefinitions();
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(validateButton, &QPushButton::clicked, this, [this, textEdit]() {
        int importedVariables = 0;
        int importedFunctions = 0;
        int importedUnits = 0;
        int ignoredLines = 0;
        QList<int> ignoredLineNumbers;
        importUserDefinitionsFromText(
            textEdit->toPlainText(),
            true,
            &importedVariables,
            &importedFunctions,
            &importedUnits,
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
            tr("Imported variables: %1\nImported functions: %2\nImported units: %3\nLine numbers with errors: %4")
                .arg(importedVariables)
                .arg(importedFunctions)
                .arg(importedUnits)
                .arg(ignoredLineNumbersText.isEmpty() ? tr("none") : ignoredLineNumbersText));
    });
    connect(applyNowButton, &QPushButton::clicked, this, [this, textEdit]() {
        int importedVariables = 0;
        int importedFunctions = 0;
        int importedUnits = 0;
        int ignoredLines = 0;
        QList<int> ignoredLineNumbers;
        m_settings->startupUserDefinitions = textEdit->toPlainText();
        applyUserDefinitions(
            &importedVariables,
            &importedFunctions,
            &importedUnits,
            &ignoredLines,
            &ignoredLineNumbers);

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
            tr("Imported variables: %1\nImported functions: %2\nImported units: %3\nLine numbers with errors: %4")
                .arg(importedVariables)
                .arg(importedFunctions)
                .arg(importedUnits)
                .arg(ignoredLineNumbersText.isEmpty() ? tr("none") : ignoredLineNumbersText));
    });

    if (dialog.exec() != QDialog::Accepted)
        return;

    m_settings->startupUserDefinitions = textEdit->toPlainText();
    UserDefinitions::saveFrom(m_settings);

    showStateLabel(tr("User definitions saved."));
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
    if (m_widgets.editor != nullptr)
        m_widgets.editor->setAutoCalcEnabled(b);
}

void MainWindow::setHistorySizeLimit()
{
    bool ok = false;
    const int current = m_session->historyLimit();
    const int value = QInputDialog::getInt(
        this,
        tr("History Size Limit"),
        tr("Maximum number of history entries for this session (0 = unlimited):"),
        current,
        0,
        1000000,
        100,
        &ok);

    if (!ok || value == current)
        return;

    m_session->setHistoryLimit(value);
    m_conditions.autoAns = !m_session->historyIsEmpty();
    emit historyChanged();
    saveSessionToDefaultPath();
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
    if (b && m_widgets.display != nullptr && m_widgets.display->isEmpty())
        showReadyMessage();
    else if (!b)
        hideStateLabel();
}

void MainWindow::setWindowPositionSaveEnabled(bool b)
{
    m_settings->windowPositionSave = b;
}

void MainWindow::setAutoCompletionEnabled(bool b)
{
    m_settings->autoCompletion = b;
    if (m_widgets.editor != nullptr)
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
    if (m_widgets.editor != nullptr)
        m_widgets.editor->refreshAutoCalc();
    emit syntaxHighlightingChanged();
}

void MainWindow::setDigitGroupingIntegerPartOnlyEnabled(bool b)
{
    m_settings->digitGroupingIntegerPartOnly = b;
    emit historyChanged();
    if (m_widgets.editor != nullptr)
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
    if (m_widgets.editor != nullptr)
        m_widgets.editor->refreshAutoCalc();
}

void MainWindow::setHoverHighlightResultsEnabled(bool b)
{
    m_settings->hoverHighlightResults = b;
    for (ResultDisplay* display : splitPaneDisplays())
        display->setHoverHighlightEnabled(b);
}

void MainWindow::setAngleModeDegree()
{
    if (m_settings->angleUnit == 'd')
        return;

    m_settings->angleUnit = 'd';

    for (const QPointer<MainWindow>& ptr : allMainWindows()) {
        if (MainWindow* window = ptr.data())
            window->setStatusBarText();
    }

    m_evaluator->initializeAngleUnits();
    emit angleUnitChanged();
}

void MainWindow::setAngleModeRadian()
{
    if (m_settings->angleUnit == 'r')
        return;

    m_settings->angleUnit = 'r';

    for (const QPointer<MainWindow>& ptr : allMainWindows()) {
        if (MainWindow* window = ptr.data())
            window->setStatusBarText();
    }

    m_evaluator->initializeAngleUnits();
    emit angleUnitChanged();
}

void MainWindow::setAngleModeGradian()
{
    if (m_settings->angleUnit == 'g')
        return;

    m_settings->angleUnit = 'g';

    for (const QPointer<MainWindow>& ptr : allMainWindows()) {
        if (MainWindow* window = ptr.data())
            window->setStatusBarText();
    }

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
    if (msg.contains(QStringLiteral("Current result:")))
        m_lastCurrentResultPreviewMessage = msg;

    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    const ThemeSurfaceColors tooltipSurface =
        themeSurfaceForShadeIndex(surfaces, UiConfig::ResultTooltipBackgroundShade);
    const ThemeSurfaceColors tooltipOutline =
        themeSurfaceForShadeIndex(surfaces, UiConfig::ResultTooltipOutlineShade);
    const QString tooltipStyle = QStringLiteral(R"(
        QLabel {
            background-color: %1;
            color: %2;
            border: %5px solid %3;
            border-radius: %4px;
        }
    )").arg(tooltipSurface.background.name(),
            tooltipSurface.foreground.name(),
            tooltipOutline.background.name())
       .arg(UiConfig::ResultTooltipCornerRadius)
       .arg(UiConfig::OutlineStrokeWidth);
    m_widgets.state->setStyleSheet(tooltipStyle);
    m_widgets.stateCloseButton->setStyleSheet(QStringLiteral(R"(
        QPushButton {
            border: none;
            background: transparent;
            color: %1;
            padding: 0;
            margin: 0;
            outline: none;
        }

        QPushButton:hover {
            background: transparent;
            color: %1;
        }

        QPushButton:pressed {
            background: transparent;
            color: %1;
        }
    )").arg(tooltipSurface.foreground.name()));

    Editor* positionEditor = m_widgets.editor;
    if (positionEditor == nullptr || positionEditor->window() != this)
        positionEditor = globallyActiveEditor();

    const int closeButtonSize = qMax(14, positionEditor->fontMetrics().height() - 2);
    const int closeButtonRightPadding = 2;
    const int closeButtonTopPadding = 1;
    const int closeButtonReservedWidth = closeButtonSize + closeButtonRightPadding + 2;
    m_widgets.state->setContentsMargins(6, 3, closeButtonReservedWidth, 3);
    m_widgets.state->setFont(positionEditor->font());
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
    QPoint pos = mapFromGlobal(
        positionEditor->mapToGlobal(QPoint(UiConfig::ResultTooltipStartMargin, -height)));
    m_widgets.state->move(pos);
}

void MainWindow::handleAutoCalcMessageAvailable(const QString& message)
{
    if (pendingDockFocusTarget() != nullptr)
        return;
    if (Editor* editor = qobject_cast<Editor*>(sender()); editor != nullptr && editor != m_widgets.editor)
        return;
    if (message.contains(QStringLiteral("Current result:"))
        && !m_widgets.state->isVisible()
        && message == m_lastCurrentResultPreviewMessage) {
        return;
    }
    if (m_currentResultPreviewDismissed && message.contains(QStringLiteral("Current result:")))
        return;
    showStateLabel(message);
}

void MainWindow::handleAutoCalcQuantityAvailable(const Quantity& quantity)
{
    if (Editor* editor = qobject_cast<Editor*>(sender()); editor != nullptr && editor != m_widgets.editor)
        return;
    if (m_settings->bitfieldVisible)
        m_widgets.bitField->updateBits(quantity);
}

void MainWindow::setFullScreenEnabled(bool b)
{
    m_settings->windowOnfullScreen = b;
    b ? showFullScreen() : showNormal();
}

bool MainWindow::event(QEvent* e)
{
    if (e != nullptr) {
        const bool updateDockSeparators =
            e->type() == QEvent::Enter
            || e->type() == QEvent::Leave
            || e->type() == QEvent::HoverEnter
            || e->type() == QEvent::HoverMove
            || e->type() == QEvent::HoverLeave
            || e->type() == QEvent::MouseButtonPress
            || e->type() == QEvent::MouseButtonRelease
            || e->type() == QEvent::MouseMove;
        if (updateDockSeparators && !m_allDocks.isEmpty())
            update();

        const bool splitCursor = cursor().shape() == Qt::SplitHCursor
            || cursor().shape() == Qt::SplitVCursor;
        if (splitCursor && e->type() == QEvent::MouseButtonPress) {
            const QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(e);
            if (mouseEvent->button() == Qt::LeftButton)
                hideCurrentResultPreview();
        } else if (splitCursor && e->type() == QEvent::MouseMove) {
            const QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(e);
            if (mouseEvent->buttons() & Qt::LeftButton)
                hideCurrentResultPreview();
        }
    }

    if (e != nullptr
        && (e->type() == QEvent::KeyPress || e->type() == QEvent::ShortcutOverride)) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
        if (keyEvent->key() == Qt::Key_Escape
            && m_widgets.state != nullptr
            && m_widgets.state->isVisible()
            && m_widgets.state->text().contains(QStringLiteral("Current result:"))) {
            m_currentResultPreviewDismissed = true;
            if (m_widgets.editor != nullptr)
                m_widgets.editor->dismissCurrentAutoCalc();
        }
    }

    if (e != nullptr && e->type() == QEvent::WindowDeactivate) {
        QWidget* focusWidget = lastFocusWidgetInActiveWindow();
        if (focusWidget == nullptr)
            focusWidget = QApplication::focusWidget();
        focusWidgetBeforeWindowDeactivate() =
            focusWidget != nullptr && focusWidget->window() == this
            ? focusWidget
            : m_widgets.editor;
        editorBeforeWindowDeactivate() = m_widgets.editor;
    }

    if (e != nullptr && e->type() == QEvent::WindowActivate) {
        // Restore the exact widget that had focus before the app lost focus.
        // On macOS the toolkit may later replay passive editor FocusIn events
        // in widget order, and the replay can otherwise promote the last pane.
        QPointer<QWidget> focusWidget(focusWidgetBeforeWindowDeactivate());
        QPointer<Editor> activeEditor(qobject_cast<Editor*>(focusWidget.data()));
        if (activeEditor == nullptr && focusWidget != nullptr)
            activeEditor = qobject_cast<Editor*>(focusWidget->parentWidget());
        if (activeEditor == nullptr)
            activeEditor = editorBeforeWindowDeactivate();
        if (activeEditor == nullptr)
            activeEditor = m_widgets.editor;
        focusWidgetBeforeWindowDeactivate() = nullptr;
        editorBeforeWindowDeactivate() = nullptr;
        pendingWindowActivationEditor() = activeEditor;
        pendingWindowActivationFocusWidget() = focusWidget;
        const int restoreGeneration = ++windowActivationRestoreGeneration();
        const auto restoreWindowFocus = [this, activeEditor, focusWidget, restoreGeneration]() {
            if (activeEditor == nullptr
                || windowActivationRestoreGeneration() != restoreGeneration)
                return;
            QWidget* pane = activeEditor->parentWidget();
            ResultDisplay* display = pane
                ? pane->findChild<ResultDisplay*>(QString(), Qt::FindDirectChildrenOnly)
                : nullptr;
            if (display != nullptr)
                setActiveEditorDisplayPane(display, activeEditor);
            if (focusWidget != nullptr
                && focusWidget->window() == this
                && windowActivationRestoreGeneration() == restoreGeneration) {
                focusWidget->setFocus(Qt::ActiveWindowFocusReason);
            }
        };
        QTimer::singleShot(0, this, restoreWindowFocus);
        QTimer::singleShot(50, this, restoreWindowFocus);
        QTimer::singleShot(150, this, restoreWindowFocus);
        // Some platforms deliver a delayed focus replay after the activation
        // event has returned. Keep one late restore before clearing the guard.
        QTimer::singleShot(300, this, restoreWindowFocus);
        QTimer::singleShot(350, this, [activeEditor, focusWidget, restoreGeneration]() {
            if (windowActivationRestoreGeneration() == restoreGeneration
                && pendingWindowActivationEditor() == activeEditor
                && pendingWindowActivationFocusWidget() == focusWidget) {
                pendingWindowActivationEditor() = nullptr;
                pendingWindowActivationFocusWidget() = nullptr;
            }
        });

        const QString activeName = m_paneSessionNames.value(m_widgets.display);
        if (!activeName.isEmpty()) {
            if (Session* activeSession = m_loadedSessions.value(activeName, nullptr))
                activateSession(activeSession);
        } else if (m_session != nullptr) {
            m_evaluator = m_session->evaluator();
            m_evaluator->initializeBuiltInVariables();
        }

        const QSignalBlocker statusBarBlocker(m_actions.viewStatusBar);
        QStatusBar* existingStatusBar = findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly);
        m_actions.viewStatusBar->setChecked(existingStatusBar != nullptr && existingStatusBar->isVisible());

        if (m_widgets.keypad == nullptr) {
            const QSignalBlocker keypadBlocker(m_actionGroups.keypad);
            m_actions.viewKeypadDisabled->setChecked(true);
        } else {
            updateKeypadModeActionState();
        }
    }

    return QMainWindow::event(e);
}

bool MainWindow::eventFilter(QObject* o, QEvent* e)
{
    if (qobject_cast<QSplitterHandle*>(o) != nullptr) {
        if (e->type() == QEvent::MouseButtonPress) {
            const QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(e);
            if (mouseEvent->button() == Qt::LeftButton)
                hideCurrentResultPreview();
        } else if (e->type() == QEvent::MouseMove) {
            const QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(e);
            if (mouseEvent->buttons() & Qt::LeftButton)
                hideCurrentResultPreview();
        }
        return QMainWindow::eventFilter(o, e);
    }

    if (QWidget* widget = qobject_cast<QWidget*>(o); isDockTextInput(widget)) {
        if (e->type() == QEvent::MouseButtonPress) {
            pendingDockTextInputFocusTarget() = widget;
            widget->setFocus(Qt::MouseFocusReason);
            deactivateActiveEditorForTextInputFocus();
            QPointer<QWidget> textInput(widget);
            QTimer::singleShot(0, widget, [textInput]() {
                if (textInput != nullptr)
                    textInput->setFocus(Qt::MouseFocusReason);
            });
            QTimer::singleShot(100, widget, [textInput]() {
                if (pendingDockTextInputFocusTarget() == textInput)
                    pendingDockTextInputFocusTarget() = nullptr;
            });
        } else if (e->type() == QEvent::FocusIn) {
            deactivateActiveEditorForTextInputFocus();
            if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget))
                applyDockSearchClearButtonIcon(lineEdit, generatedSurfaceColors(m_settings), true);
        } else if (e->type() == QEvent::FocusOut) {
            if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget))
                applyDockSearchClearButtonIcon(lineEdit, generatedSurfaceColors(m_settings), false);
        }
        return QMainWindow::eventFilter(o, e);
    }

    if (e->type() == QEvent::MouseButtonPress) {
        if (QWidget* widget = qobject_cast<QWidget*>(o)) {
            QDockWidget* dock = dockWidgetForDescendant(widget);
            if (dock != nullptr) {
                QWidget* focusTarget = dock;
                if (QAbstractItemView* view = dockItemViewFocusTarget(widget)) {
                    view->setFocusPolicy(Qt::StrongFocus);
                    focusTarget = view;
                } else {
                    dock->setFocusPolicy(Qt::StrongFocus);
                }
                pendingDockFocusTarget() = focusTarget;
                deactivateActiveEditorForTextInputFocus();
                hideStateLabel();
                pendingDockTextInputFocusTarget() = nullptr;
                const QColor inactivePrimary = generatedSurfaceColors(m_settings).primary.background;
                for (Editor* paneEditor : splitPaneEditors()) {
                    if (paneEditor == nullptr)
                        continue;
                    paneEditor->clearFocus();
                    paneEditor->setCustomCursorVisible(false);
                    paneEditor->setThemePrimaryColor(inactivePrimary, false);
                }
                focusTarget->setFocus(Qt::MouseFocusReason);
                QPointer<QWidget> focusTargetGuard(focusTarget);
                const auto restoreDockFocus = [focusTargetGuard]() {
                    if (focusTargetGuard != nullptr)
                        focusTargetGuard->setFocus(Qt::MouseFocusReason);
                };
                QTimer::singleShot(0, focusTarget, restoreDockFocus);
                QTimer::singleShot(50, focusTarget, restoreDockFocus);
                QTimer::singleShot(250, focusTarget, [focusTargetGuard]() {
                    if (pendingDockFocusTarget() == focusTargetGuard)
                        pendingDockFocusTarget() = nullptr;
                });
            }
        }
    }

    Editor* filteredEditor = qobject_cast<Editor*>(o);
    if (filteredEditor == nullptr) {
        if (QWidget* widget = qobject_cast<QWidget*>(o))
            filteredEditor = qobject_cast<Editor*>(widget->parentWidget());
    }
    if (Editor* editor = filteredEditor) {
        if (e->type() == QEvent::FocusIn && pendingDockFocusTarget() != nullptr) {
            // A dock click is explicit focus intent. Handle it before editor
            // completion focus recovery, otherwise a pending completion owner can
            // pull focus back to an editor and make dock selection use stale pane
            // state.
            QPointer<QWidget> focusTarget(pendingDockFocusTarget());
            editor->clearFocus();
            QTimer::singleShot(0, focusTarget, [focusTarget]() {
                if (focusTarget != nullptr)
                    focusTarget->setFocus(Qt::MouseFocusReason);
            });
            return true;
        }
        if (e->type() == QEvent::FocusIn && pendingDockTextInputFocusTarget() != nullptr) {
            QPointer<QWidget> textInput(pendingDockTextInputFocusTarget());
            QTimer::singleShot(0, textInput, [textInput]() {
                if (textInput != nullptr)
                    textInput->setFocus(Qt::MouseFocusReason);
            });
            return true;
        }
        if (Editor* completionOwner = Editor::completionMouseSelectionOwner()) {
            if (editor != completionOwner
                && (e->type() == QEvent::FocusIn || e->type() == QEvent::MouseButtonPress)) {
                QPointer<Editor> owner(completionOwner);
                QTimer::singleShot(0, completionOwner, [owner]() {
                    if (owner != nullptr) {
                        owner->window()->activateWindow();
                        owner->setFocus(Qt::OtherFocusReason);
                        owner->viewport()->setFocus(Qt::OtherFocusReason);
                    }
                });
                return true;
            }
        }
        if (dockTextInputFocusTransferInProgress() && e->type() == QEvent::FocusIn)
            return QMainWindow::eventFilter(o, e);
        if (e->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
            if (keyEvent->key() == Qt::Key_Escape
                && m_widgets.state->isVisible()
                && m_widgets.state->text().contains(QStringLiteral("Current result:"))) {
                m_currentResultPreviewDismissed = true;
                editor->dismissCurrentAutoCalc();
            }
        }
        // A bare FocusIn is not pane-selection intent. Window activation can
        // replay FocusIn through every editor viewport in layout order, which
        // used to make the last pane active after each app switch. Only input
        // events that originate from the editor path below may select a pane.
        if (e->type() == QEvent::MouseButtonPress
            || e->type() == QEvent::KeyPress
            || e->type() == QEvent::InputMethod) {
            if (pendingWindowActivationEditor() != nullptr
                && (e->type() == QEvent::MouseButtonPress
                    || e->type() == QEvent::KeyPress)) {
                cancelWindowActivationRestore();
            }
            if (pendingWindowActivationEditor() != nullptr
                && editor != pendingWindowActivationEditor()) {
                // Input method events can still arrive while the activation
                // restore is settling. Until mouse/key intent cancels the
                // guard above, keep the previously active editor selected.
                QPointer<Editor> activeEditor(pendingWindowActivationEditor());
                QPointer<MainWindow> window(this);
                QTimer::singleShot(0, this, [window, activeEditor]() {
                    if (activeEditor != nullptr
                        && window != nullptr
                        && pendingWindowActivationEditor() == activeEditor) {
                        QWidget* pane = activeEditor->parentWidget();
                        ResultDisplay* display = pane
                            ? pane->findChild<ResultDisplay*>(QString(), Qt::FindDirectChildrenOnly)
                            : nullptr;
                        if (display != nullptr)
                            window->setActiveEditorDisplayPane(display, activeEditor);
                    }
                });
                return true;
            }
            if (pendingSessionTabActivationEditor() != nullptr
                && editor != pendingSessionTabActivationEditor()) {
                QPointer<Editor> targetEditor(pendingSessionTabActivationEditor());
                QTimer::singleShot(0, targetEditor, [targetEditor]() {
                    if (targetEditor != nullptr)
                        targetEditor->setFocus(Qt::MouseFocusReason);
                });
                return true;
            }
            QWidget* pane = editor->parentWidget();
            ResultDisplay* display = pane ? pane->findChild<ResultDisplay*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
            if (display != nullptr)
                setActiveEditorDisplayPane(display, editor);
        }
        return QMainWindow::eventFilter(o, e);
    }

    if (o == m_widgets.state && e->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(e);
        if (mouseEvent->button() == Qt::LeftButton) {
            hideStateLabel();
            return true;
        }
    }

    if (o == m_status.angleUnitLabel || o == m_status.resultFormatLabel
            || o == m_status.resultPrecisionLabel) {
        if (e->type() == QEvent::MouseButtonPress) {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(e);
            if (mouseEvent->button() == Qt::LeftButton) {
                const QPoint popupPoint(0, static_cast<QWidget*>(o)->height());
                if (o == m_status.angleUnitLabel)
                    showAngleModeContextMenu(m_status.angleUnit->mapFromGlobal(static_cast<QWidget*>(o)->mapToGlobal(popupPoint)));
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

    if (o == m_docks.bitField) {
        if (e->type() == QEvent::Close) {
            deleteBitField();
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

    if (m_widgets.keypadContainer) {
        m_layouts.root->removeWidget(m_widgets.keypadContainer);
        m_widgets.keypadContainer->deleteLater();
        m_widgets.keypadContainer = 0;
    } else {
        m_layouts.root->removeItem(m_layouts.keypad);
        m_layouts.keypad->deleteLater();
    }
    m_layouts.keypad = 0;

    m_settings->keypadVisible = false;
}

void MainWindow::deleteStatusBar()
{
    QStatusBar* existingStatusBar = findChild<QStatusBar*>(QString(), Qt::FindDirectChildrenOnly);
    if (existingStatusBar != nullptr)
        existingStatusBar->hide();

    if (m_status.angleUnitSection)
        m_status.angleUnitSection->deleteLater();
    m_status.angleUnit = 0;
    m_status.angleUnitSection = 0;
    m_status.angleUnitLabel = 0;

    if (m_status.resultFormatSection)
        m_status.resultFormatSection->deleteLater();
    m_status.resultFormat = 0;
    m_status.resultFormatSection = 0;
    m_status.resultFormatLabel = 0;
    if (m_status.resultPrecisionSection)
        m_status.resultPrecisionSection->deleteLater();
    m_status.resultPrecision = 0;
    m_status.resultPrecisionSection = 0;
    m_status.resultPrecisionLabel = 0;

    if (existingStatusBar != nullptr)
        setStatusBar(0);
}

void MainWindow::deleteBitField()
{
    if (!m_docks.bitField)
        return;

    deleteDock(m_docks.bitField);
    m_actions.viewBitfield->setChecked(false);
    m_settings->bitfieldVisible = false;
}

void MainWindow::deleteBookDock()
{
    if (!m_docks.book)
        return;

    deleteDock(m_docks.book);
    m_actions.viewFormulaBook->setChecked(false);
    m_settings->formulaBookDockVisible = false;
}

void MainWindow::deleteConstantsDock()
{
    if (!m_docks.constants)
        return;

    deleteDock(m_docks.constants);
    m_actions.viewConstants->setChecked(false);
    m_settings->constantsDockVisible = false;
}

void MainWindow::deleteFunctionsDock()
{
    if (!m_docks.functions)
        return;

    deleteDock(m_docks.functions);
    m_actions.viewFunctions->setChecked(false);
    m_settings->functionsDockVisible = false;
}

void MainWindow::deleteHistoryDock()
{
    if (!m_docks.history)
        return;

    deleteDock(m_docks.history);
    m_actions.viewHistory->setChecked(false);
    m_settings->historyDockVisible = false;
}

void MainWindow::deleteVariablesDock()
{
    if (!m_docks.variables)
        return;

    deleteDock(m_docks.variables);
    m_actions.viewVariables->setChecked(false);
    m_settings->variablesDockVisible = false;
}

void MainWindow::deleteUserFunctionsDock()
{
    if (!m_docks.userFunctions)
        return;

    deleteDock(m_docks.userFunctions);
    m_actions.viewUserFunctions->setChecked(false);
    m_settings->userFunctionsDockVisible = false;
}

void MainWindow::deleteUserUnitsDock()
{
    if (!m_docks.userUnits)
        return;

    deleteDock(m_docks.userUnits);
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
        updateKeypadModeActionState();
        return;
    }

    if (m_settings->keypadMode == mode && !isCustomMode)
        return;

    const bool wasVisible = isVisibleKeypadMode(m_settings->keypadMode);
    const bool nowVisible = isVisibleKeypadMode(mode);
    m_settings->keypadMode = mode;
    updateKeypadModeActionState();

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

void MainWindow::setResultFormatCartesian()
{
    if (m_settings->resultComplexForm == ComplexForm::Rectangular)
        return;

    m_settings->complexNumbers = true;
    m_settings->secondaryComplexNumbers = true;
    m_settings->tertiaryComplexNumbers = true;
    m_settings->quaternaryComplexNumbers = true;
    m_settings->quinaryComplexNumbers = true;
    m_settings->resultComplexForm = ComplexForm::Rectangular;
    m_settings->secondaryResultComplexForm = ComplexForm::Rectangular;
    m_settings->tertiaryResultComplexForm = ComplexForm::Rectangular;
    m_settings->quaternaryResultComplexForm = ComplexForm::Rectangular;
    m_settings->quinaryResultComplexForm = ComplexForm::Rectangular;
    DMath::complexMode = true;
    setStatusBarText();
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
    if (m_settings->imaginaryUnit == 'i')
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
    if (m_settings->imaginaryUnit == 'j')
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
    if (m_settings->resultComplexForm == ComplexForm::Exponential)
        return;

    m_settings->complexNumbers = true;
    m_settings->secondaryComplexNumbers = true;
    m_settings->tertiaryComplexNumbers = true;
    m_settings->quaternaryComplexNumbers = true;
    m_settings->quinaryComplexNumbers = true;
    m_settings->resultComplexForm = ComplexForm::Exponential;
    m_settings->secondaryResultComplexForm = ComplexForm::Exponential;
    m_settings->tertiaryResultComplexForm = ComplexForm::Exponential;
    m_settings->quaternaryResultComplexForm = ComplexForm::Exponential;
    m_settings->quinaryResultComplexForm = ComplexForm::Exponential;
    DMath::complexMode = true;
    setStatusBarText();
    emit resultFormatChanged();
}

void MainWindow::setResultFormatTrigonometric()
{
    if (m_settings->resultComplexForm == ComplexForm::Trigonometric)
        return;

    m_settings->complexNumbers = true;
    m_settings->secondaryComplexNumbers = true;
    m_settings->tertiaryComplexNumbers = true;
    m_settings->quaternaryComplexNumbers = true;
    m_settings->quinaryComplexNumbers = true;
    m_settings->resultComplexForm = ComplexForm::Trigonometric;
    m_settings->secondaryResultComplexForm = ComplexForm::Trigonometric;
    m_settings->tertiaryResultComplexForm = ComplexForm::Trigonometric;
    m_settings->quaternaryResultComplexForm = ComplexForm::Trigonometric;
    m_settings->quinaryResultComplexForm = ComplexForm::Trigonometric;
    DMath::complexMode = true;
    setStatusBarText();
    emit resultFormatChanged();
}

void MainWindow::setResultFormatCis()
{
    if (m_settings->resultComplexForm == ComplexForm::Cis)
        return;

    m_settings->complexNumbers = true;
    m_settings->secondaryComplexNumbers = true;
    m_settings->tertiaryComplexNumbers = true;
    m_settings->quaternaryComplexNumbers = true;
    m_settings->quinaryComplexNumbers = true;
    m_settings->resultComplexForm = ComplexForm::Cis;
    m_settings->secondaryResultComplexForm = ComplexForm::Cis;
    m_settings->tertiaryResultComplexForm = ComplexForm::Cis;
    m_settings->quaternaryResultComplexForm = ComplexForm::Cis;
    m_settings->quinaryResultComplexForm = ComplexForm::Cis;
    DMath::complexMode = true;
    setStatusBarText();
    emit resultFormatChanged();
}

void MainWindow::setResultFormatPolarAngle()
{
    if (m_settings->resultComplexForm == ComplexForm::Phasor)
        return;

    m_settings->complexNumbers = true;
    m_settings->secondaryComplexNumbers = true;
    m_settings->tertiaryComplexNumbers = true;
    m_settings->quaternaryComplexNumbers = true;
    m_settings->quinaryComplexNumbers = true;
    m_settings->resultComplexForm = ComplexForm::Phasor;
    m_settings->secondaryResultComplexForm = ComplexForm::Phasor;
    m_settings->tertiaryResultComplexForm = ComplexForm::Phasor;
    m_settings->quaternaryResultComplexForm = ComplexForm::Phasor;
    m_settings->quinaryResultComplexForm = ComplexForm::Phasor;
    DMath::complexMode = true;
    setStatusBarText();
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
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kFeedbackUrl)));
}

void MainWindow::openSourceURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kSourceUrl)));
}

void MainWindow::openCommunityURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kCommunityUrl)));
}

void MainWindow::openFacebookGroupURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kFacebookGroupUrl)));
}

void MainWindow::openNewsURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kNewsUrl)));
}

void MainWindow::openDonateURL()
{
    QDesktopServices::openUrl(QUrl(QString::fromLatin1(kDonateUrl)));
}

void MainWindow::copy()
{
    m_copyWidget->copy();
}

void MainWindow::restoreSession(bool restoreHistory) {
    migrateLegacyHistoryIfNeeded();
    ensureSessionsPath();

    if (restoreSessionLayout(restoreHistory))
        return;

    const QString name = m_session->name();
    const QString filePath = sessionFilePath(name);
    QPointer<MainWindow> windowGuard(this);
    QThread* thread = QThread::create([windowGuard, filePath, name, restoreHistory]() {
        QJsonObject json;
        const bool ok = readValidSessionJson(filePath, &json);
        if (ok && !restoreHistory)
            json.remove(QLatin1String(SessionJsonKeys::History));

        if (!windowGuard)
            return;

        QMetaObject::invokeMethod(windowGuard.data(), [windowGuard, ok, json, name, restoreHistory]() mutable {
            if (!windowGuard)
                return;
            if (!ok)
                return;

            Session* loadedSession = new Session();
            loadedSession->deSerialize(json, false);
            loadedSession->setName(name);
            MainWindow* window = windowGuard.data();
            Session* oldSession = window->m_session;
            window->m_loadedSessions.remove(oldSession ? oldSession->name() : QString());
            window->m_loadedSessions.insert(name, loadedSession);
            window->m_session = loadedSession;
            window->m_evaluator = loadedSession->evaluator();
            if (window->m_widgets.display)
                window->m_widgets.display->setSession(loadedSession);
            if (window->m_widgets.editor)
                window->m_widgets.editor->setSession(loadedSession);
            if (window->m_docks.history)
                window->m_docks.history->widget()->setSession(loadedSession);
            if (oldSession != nullptr)
                delete oldSession;

            emit window->historyChanged();
            emit window->variablesChanged();
            emit window->functionsChanged();
            emit window->unitsChanged();
            window->restoreEditorTextFromCurrentSession();
            window->m_conditions.autoAns = restoreHistory && !loadedSession->historyIsEmpty();
        }, Qt::QueuedConnection);
    });
    {
        QMutexLocker locker(&asyncSessionIoMutex());
        asyncSessionIoThreads().append(thread);
    }
    QObject::connect(thread, &QThread::finished, thread, [thread]() {
        unregisterAsyncSessionIoThread(thread);
        thread->deleteLater();
    });
    thread->start();
}

bool MainWindow::restoreSessionLayout(bool restoreHistory)
{
    if (m_settings->sessionLayoutJson.isEmpty())
        return false;

    const QJsonDocument layoutDoc = QJsonDocument::fromJson(m_settings->sessionLayoutJson.toUtf8());
    if (!layoutDoc.isObject())
        return false;

    const QJsonObject layout = layoutDoc.object();
    if (layout.value(QStringLiteral("scheme")).toInt() != 1
            || layout.value(QStringLiteral("kind")).toString() != QLatin1String("session-layout")) {
        return false;
    }

    const QJsonArray windows = layout.value(QStringLiteral("windows")).toArray();
    if (windows.isEmpty())
        return false;

    const QString activeWindowId = layout.value(QStringLiteral("activeWindow")).toString();
    QJsonObject window;
    for (const QJsonValue& value : windows) {
        if (!value.isObject())
            continue;
        const QJsonObject candidate = value.toObject();
        if ((!activeWindowId.isEmpty() && candidate.value(QStringLiteral("id")).toString() == activeWindowId)
                || (activeWindowId.isEmpty() && candidate.value(QStringLiteral("active")).toBool())) {
            window = candidate;
            break;
        }
    }
    if (window.isEmpty() && windows.first().isObject())
        window = windows.first().toObject();
    if (window.isEmpty())
        return false;

    const QJsonObject root = window.value(QStringLiteral("root")).toObject();
    const QString rootType = root.value(QStringLiteral("type")).toString();
    if (rootType != QLatin1String("tabs") && rootType != QLatin1String("split"))
        return false;

    QJsonArray tabs = rootType == QLatin1String("tabs")
        ? root.value(QStringLiteral("tabs")).toArray()
        : QJsonArray();
    const auto appendPaneTabs = [&tabs](const QJsonObject& node, const auto& appendPaneTabsRef) -> void {
        const QString type = node.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("pane")) {
            const QJsonArray paneTabs = node.value(QStringLiteral("tabs")).toArray();
            for (const QJsonValue& tabValue : paneTabs)
                tabs.append(tabValue);
            return;
        }
        if (type != QLatin1String("split"))
            return;
        const QJsonArray children = node.value(QStringLiteral("children")).toArray();
        for (const QJsonValue& childValue : children) {
            if (childValue.isObject())
                appendPaneTabsRef(childValue.toObject(), appendPaneTabsRef);
        }
    };
    if (rootType == QLatin1String("split"))
        appendPaneTabs(root, appendPaneTabs);
    if (tabs.isEmpty())
        return false;

    QString activeSessionName = normalizedSessionName(root.value(QStringLiteral("active")).toString());
    QList<SessionLoadSpec> sessionLoadSpecs;
    QStringList loadNames;

    for (const QJsonValue& value : tabs) {
        if (!value.isObject())
            continue;

        const QJsonObject tab = value.toObject();
        const QString name = normalizedSessionName(tab.value(QStringLiteral("name")).toString());
        if (loadNames.contains(name, Qt::CaseInsensitive))
            continue;
        loadNames.append(name);

        QString fileName = tab.value(QStringLiteral("file")).toString();
        if (fileName.isEmpty())
            fileName = sessionFileBaseName(name) + QLatin1String(".json");

        const QString filePath = QDir(sessionsPath()).filePath(fileName);
        sessionLoadSpecs.append(SessionLoadSpec{ name, filePath, tab });
    }

    if (sessionLoadSpecs.isEmpty())
        return false;

    QPointer<MainWindow> windowGuard(this);
    QThread* thread = QThread::create([windowGuard, sessionLoadSpecs, activeSessionName,
                                       layout, window, root, tabs, restoreHistory]() {
        QHash<QString, QJsonObject> sessionJsons;
        QHash<QString, QPair<int, int>> viewportAnchors;
        QHash<QString, int> scrollValues;

        for (const SessionLoadSpec& spec : sessionLoadSpecs) {
            QJsonObject sessionJson;
            if (!readValidSessionJson(spec.filePath, &sessionJson))
                continue;

            if (!restoreHistory)
                sessionJson.remove(QLatin1String(SessionJsonKeys::History));

            sessionJson.insert(QLatin1String(SessionJsonKeys::Session), spec.name);
            sessionJsons.insert(spec.name, sessionJson);

            const QJsonObject scroll = spec.tab.value(QStringLiteral("scroll")).toObject();
            const int block = scroll.value(QStringLiteral("block")).toInt(-1);
            const int offset = scroll.value(QStringLiteral("offset")).toInt(0);
            const int scrollValue = scroll.value(QStringLiteral("value")).toInt(-1);
            if (block >= 0)
                viewportAnchors.insert(spec.name, qMakePair(block, offset));
            if (scrollValue >= 0)
                scrollValues.insert(spec.name, scrollValue);
        }

        if (!windowGuard)
            return;

        QMetaObject::invokeMethod(windowGuard.data(), [windowGuard, layout, window, root, tabs,
                                                       activeSessionName, restoreHistory,
                                                       sessionJsons, viewportAnchors,
                                                       scrollValues]() mutable {
            if (!windowGuard)
                return;
            windowGuard->finishRestoreSessionLayout(layout, window, root, tabs, activeSessionName,
                                                    restoreHistory, sessionJsons, viewportAnchors,
                                                    scrollValues);
        }, Qt::QueuedConnection);
    });
    {
        QMutexLocker locker(&asyncSessionIoMutex());
        asyncSessionIoThreads().append(thread);
    }
    QObject::connect(thread, &QThread::finished, thread, [thread]() {
        unregisterAsyncSessionIoThread(thread);
        thread->deleteLater();
    });
    thread->start();
    return true;
}

void MainWindow::finishRestoreSessionLayout(const QJsonObject& layout,
                                            const QJsonObject& window,
                                            const QJsonObject& root,
                                            const QJsonArray& tabs,
                                            const QString& activeSessionName,
                                            bool restoreHistory,
                                            QHash<QString, QJsonObject> sessionJsons,
                                            QHash<QString, QPair<int, int>> viewportAnchors,
                                            QHash<QString, int> scrollValues)
{
    const QJsonArray windows = layout.value(QStringLiteral("windows")).toArray();
    const QString rootType = root.value(QStringLiteral("type")).toString();

    if (sessionJsons.isEmpty())
        return;

    QHash<QString, Session*> restoredSessions;
    for (auto it = sessionJsons.constBegin(); it != sessionJsons.constEnd(); ++it) {
        Session* session = new Session();
        session->deSerialize(it.value(), false);
        session->setName(it.key());
        restoredSessions.insert(it.key(), session);
    }

    m_sessionViewportAnchors = viewportAnchors;
    m_sessionScrollValues = scrollValues;

    QString activeSessionNameToRestore = activeSessionName;
    Session* activeSession = restoredSessions.value(activeSessionNameToRestore, nullptr);

    if (activeSession == nullptr) {
        activeSessionNameToRestore = restoredSessions.keys().constFirst();
        activeSession = restoredSessions.value(activeSessionNameToRestore);
    }

    const QFont displayFont = m_widgets.display->font();
    const QFont editorFont = m_widgets.editor->font();
    const QList<Session*> oldSessions = m_loadedSessions.values();
    m_loadedSessions = restoredSessions;
    m_session = nullptr;
    m_evaluator = nullptr;

    m_widgets.display = nullptr;
    m_widgets.editor = nullptr;
    m_copyWidget = nullptr;
    while (m_widgets.splitContainer != nullptr && m_widgets.splitContainer->count() > 0) {
        QWidget* child = m_widgets.splitContainer->widget(0);
        child->setParent(nullptr);
        delete child;
    }

    m_paneSessionNames.clear();
    m_paneSessionTabs.clear();
    m_paneTabBars.clear();
    m_tabBarDisplays.clear();

    const QList<Session*> retainedSessions = restoredSessions.values();
    for (Session* oldSession : oldSessions) {
        if (!retainedSessions.contains(oldSession))
            delete oldSession;
    }

    ResultDisplay* activeDisplay = nullptr;
    Editor* activeEditor = nullptr;
    ResultDisplay* firstDisplay = nullptr;
    Editor* firstEditor = nullptr;

    const auto paneNamesFromNode = [](const QJsonObject& node) -> QStringList {
        QStringList names;
        const QJsonArray paneTabs = node.value(QStringLiteral("tabs")).toArray();
        for (const QJsonValue& tabValue : paneTabs) {
            if (!tabValue.isObject())
                continue;
            const QString name = normalizedSessionName(tabValue.toObject().value(QStringLiteral("name")).toString());
            if (!name.isEmpty() && !names.contains(name, Qt::CaseInsensitive))
                names.append(name);
        }
        const QString activeName = normalizedSessionName(node.value(QStringLiteral("active")).toString());
        if (!activeName.isEmpty() && !names.contains(activeName, Qt::CaseInsensitive))
            names.prepend(activeName);
        return names;
    };

    const auto firstLoadedName = [this](const QStringList& names) -> QString {
        for (const QString& name : names) {
            if (m_loadedSessions.contains(name))
                return name;
        }
        return QString();
    };

    const auto editorStateFromNode = [](const QJsonObject& node, const QString& activeName) -> QJsonObject {
        const QJsonArray paneTabs = node.value(QStringLiteral("tabs")).toArray();
        for (const QJsonValue& tabValue : paneTabs) {
            if (!tabValue.isObject())
                continue;
            const QJsonObject tab = tabValue.toObject();
            const QString tabName = normalizedSessionName(tab.value(QStringLiteral("name")).toString());
            if (tabName.compare(activeName, Qt::CaseInsensitive) == 0)
                return tab.value(QStringLiteral("editor")).toObject();
        }
        return QJsonObject();
    };

    const auto createPane = [this, &displayFont, &editorFont, &activeSessionNameToRestore, &paneNamesFromNode,
                             &activeDisplay, &activeEditor, &firstDisplay, &firstEditor,
                             &firstLoadedName, &editorStateFromNode](const QJsonObject& node) -> QWidget* {
        const QStringList names = paneNamesFromNode(node);
        QStringList loadedNames;
        for (const QString& name : names) {
            if (m_loadedSessions.contains(name) && !loadedNames.contains(name, Qt::CaseInsensitive))
                loadedNames.append(name);
        }
        if (loadedNames.isEmpty())
            return nullptr;

        QString activeName = normalizedSessionName(node.value(QStringLiteral("active")).toString());
        if (!m_loadedSessions.contains(activeName))
            activeName = firstLoadedName(loadedNames);
        if (activeName.isEmpty())
            return nullptr;
        if (!loadedNames.contains(activeName, Qt::CaseInsensitive))
            loadedNames.prepend(activeName);

        ResultDisplay* display = new ResultDisplay();
        display->setFrameStyle(QFrame::NoFrame);
        display->setFont(displayFont);
        display->setHoverHighlightEnabled(m_settings->hoverHighlightResults);
        display->setLoadedSessionCount(1);
        display->rehighlight();

        Editor* editor = new Editor();
        editor->setFrameStyle(QFrame::NoFrame);
        editor->setFont(editorFont);
        editor->setAutoCalcEnabled(m_settings->autoCalc);
        editor->setAutoCompletionEnabled(m_settings->autoCompletion);
        editor->rehighlight();

        QWidget* pane = createEditorDisplayPane(display, editor);
        configureEditorDisplayPane(display, editor);

        m_paneSessionNames.insert(display, activeName);
        m_paneSessionTabs.insert(display, loadedNames);
        Session* paneSession = m_loadedSessions.value(activeName, nullptr);
        display->setSession(paneSession);
        if (paneSession != nullptr)
            editor->setSession(paneSession);
        restoreEditorState(editor,
                           editorStateFromNode(node, activeName),
                           paneSession != nullptr ? paneSession->editorText() : QString());

        if (firstDisplay == nullptr) {
            firstDisplay = display;
            firstEditor = editor;
        }
        if (activeName == activeSessionNameToRestore) {
            activeDisplay = display;
            activeEditor = editor;
        }
        return pane;
    };

    const auto restoreSplitterSizes = [](QSplitter* splitter, const QJsonObject& node) {
        QJsonArray splitSizes = node.value(QStringLiteral("sizes")).toArray();
        if (splitSizes.isEmpty())
            return;
        QList<int> sizes;
        for (const QJsonValue& value : splitSizes)
            sizes.append(value.toInt());
        if (sizes.size() == splitter->count())
            splitter->setSizes(sizes);
    };

    const auto restoreNode = [this, &createPane, &restoreSplitterSizes](const QJsonObject& node,
                                                                        const auto& restoreNodeRef) -> QWidget* {
        const QString type = node.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("pane"))
            return createPane(node);

        QSplitter* splitter = new QSplitter(
            node.value(QStringLiteral("orientation")).toString() == QLatin1String("vertical")
                ? Qt::Vertical
                : Qt::Horizontal);
        splitter->setChildrenCollapsible(false);
        splitter->setHandleWidth(UiConfig::SessionPaneSplitterWidth);
        splitter->setStyleSheet(m_widgets.splitContainer->styleSheet());

        const QJsonArray children = node.value(QStringLiteral("children")).toArray();
        for (const QJsonValue& childValue : children) {
            if (!childValue.isObject())
                continue;
            QWidget* child = restoreNodeRef(childValue.toObject(), restoreNodeRef);
            if (child != nullptr)
                splitter->addWidget(child);
        }
        restoreSplitterSizes(splitter, node);
        return splitter;
    };

    if (rootType == QLatin1String("split")) {
        m_widgets.splitContainer->setOrientation(
            root.value(QStringLiteral("orientation")).toString() == QLatin1String("vertical")
                ? Qt::Vertical
                : Qt::Horizontal);
        const QJsonArray children = root.value(QStringLiteral("children")).toArray();
        for (const QJsonValue& childValue : children) {
            if (!childValue.isObject())
                continue;
            QWidget* child = restoreNode(childValue.toObject(), restoreNode);
            if (child != nullptr)
                m_widgets.splitContainer->addWidget(child);
        }
        restoreSplitterSizes(m_widgets.splitContainer, root);
    }

    if (m_widgets.splitContainer->count() == 0 || firstDisplay == nullptr) {
        while (m_widgets.splitContainer->count() > 0) {
            QWidget* child = m_widgets.splitContainer->widget(0);
            child->setParent(nullptr);
            delete child;
        }
        m_paneSessionNames.clear();
        m_paneSessionTabs.clear();
        m_paneTabBars.clear();
        m_tabBarDisplays.clear();
        activeDisplay = nullptr;
        activeEditor = nullptr;
        firstDisplay = nullptr;
        firstEditor = nullptr;

        QJsonObject pane;
        pane.insert(QStringLiteral("type"), QStringLiteral("pane"));
        pane.insert(QStringLiteral("active"), activeSessionNameToRestore);
        pane.insert(QStringLiteral("tabs"), tabs);
        m_widgets.splitContainer->addWidget(createPane(pane));
    }

    m_widgets.display = activeDisplay ? activeDisplay : firstDisplay;
    m_widgets.editor = activeEditor ? activeEditor : firstEditor;
    m_copyWidget = m_widgets.editor;
    updatePaneLoadedSessionCounts();
    m_session = nullptr;
    activateSession(activeSession);
    m_conditions.autoAns = restoreHistory && !m_session->historyIsEmpty();
    updatePaneEditorCursorVisibility();
    if (window.contains(QStringLiteral("statusBarVisible")))
        setStatusBarVisible(window.value(QStringLiteral("statusBarVisible")).toBool(true));
    if (!isVisible()) {
        const QString windowStateBase64 = window.value(QStringLiteral("windowState")).toString();
        if (!windowStateBase64.isEmpty())
            restoreState(QByteArray::fromBase64(windowStateBase64.toLatin1()), DockLayoutStateVersion);
        else
            restoreState(m_settings->windowState, DockLayoutStateVersion);
    }
    if (window.contains(QStringLiteral("bitfieldVisible")))
        setBitfieldVisible(window.value(QStringLiteral("bitfieldVisible")).toBool(false));
    if (window.contains(QStringLiteral("keypadVisible")))
        setKeypadVisible(window.value(QStringLiteral("keypadVisible")).toBool(false));
    applyThemeSurfacePalette();
    refreshPaneThemes();
    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();
    emit unitsChanged();
    restoreVisibleSessionViewports();
    QTimer::singleShot(0, this, [this]() {
        restoreVisibleSessionViewports();
    });

    if (this == primaryMainWindow() && !g_restoringExtraWindows && !g_multiWindowSpawnDone && windows.size() > 1) {
        g_multiWindowSpawnDone = true;
        g_restoringExtraWindows = true;
        for (int i = 0; i < windows.size(); ++i) {
            const QJsonValue value = windows.at(i);
            if (!value.isObject())
                continue;
            const QJsonObject candidate = value.toObject();
            if (candidate.value(QStringLiteral("id")).toString() == window.value(QStringLiteral("id")).toString())
                continue;

            QJsonObject singleLayout = layout;
            singleLayout.insert(QStringLiteral("windows"), QJsonArray({ candidate }));
            singleLayout.insert(QStringLiteral("activeWindow"), candidate.value(QStringLiteral("id")).toString());
            const QString previousLayoutJson = m_settings->sessionLayoutJson;
            m_settings->sessionLayoutJson = QString::fromUtf8(QJsonDocument(singleLayout).toJson(QJsonDocument::Compact));
            MainWindow* extraWindow = new MainWindow();
            m_settings->sessionLayoutJson = previousLayoutJson;
            extraWindow->show();
            const QString geometryBase64 = candidate.value(QStringLiteral("geometry")).toString();
            if (!geometryBase64.isEmpty())
                extraWindow->restoreGeometry(QByteArray::fromBase64(geometryBase64.toLatin1()));
        }
        g_restoringExtraWindows = false;
    }
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
            saveSessionToDefaultPath();
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
        if (m_bulkEvaluationInProgress)
            m_bulkFunctionsChanged = true;
        else
            emit functionsChanged();
    } else if (m_evaluator->isUserUnitAssign()) {
        result = CMath::nan();
        if (m_bulkEvaluationInProgress)
            m_bulkUnitsChanged = true;
        else
            emit unitsChanged();
    } else if (result.isNan() && !isCommentOnly)
        return;

    const QString interpretedExpr = m_evaluator->interpretedExpression();
    const bool warnHistoryLimitReached = m_session->nextHistoryEntryReachesLimit();
    HistoryEntry historyEntry(enteredExpr, result, interpretedExpr, evalContext);
    historyEntry.setRenderedLines(renderedLinesForHistoryEntry(historyEntry, m_settings, m_evaluator));
    m_session->addHistoryEntry(historyEntry);
    const bool userVariableAssign = m_evaluator->isUserVariableAssign();
    if (m_bulkEvaluationInProgress)
        m_bulkHistoryChanged = true;
    else
        emit historyChanged();
    if (!startedFromHistoryEdit)
        m_widgets.display->verticalScrollBar()->setValue(m_widgets.display->verticalScrollBar()->maximum());
    if (userVariableAssign) {
        if (m_bulkEvaluationInProgress)
            m_bulkVariablesChanged = true;
        else
            emit variablesChanged();
    }
    if (m_evaluator->isUserUnitAssign()) {
        if (m_bulkEvaluationInProgress)
            m_bulkUnitsChanged = true;
        else
            emit unitsChanged();
    }

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
    if (warnHistoryLimitReached) {
        QTimer::singleShot(0, this, [this]() {
            QMessageBox::information(
                this,
                tr("History Size Limit Reached"),
                tr("This calculation fills the last available history slot. "
                   "Future calculations will remove the oldest calculation from history. "
                   "You can increase the limit from Session > History Size Limit."));
        });
    }
    if (m_bulkEvaluationInProgress)
        m_sessionSavePending = true;
    else
        saveSessionToDefaultPath();
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

void MainWindow::handleBulkEvaluationStarted()
{
    m_bulkEvaluationInProgress = true;
    m_bulkHistoryChanged = false;
    m_bulkVariablesChanged = false;
    m_bulkFunctionsChanged = false;
    m_bulkUnitsChanged = false;
}

void MainWindow::handleBulkEvaluationFinished()
{
    m_bulkEvaluationInProgress = false;

    if (m_bulkHistoryChanged)
        emit historyChanged();
    if (m_bulkVariablesChanged)
        emit variablesChanged();
    if (m_bulkFunctionsChanged)
        emit functionsChanged();
    if (m_bulkUnitsChanged)
        emit unitsChanged();

    if (m_bulkHistoryChanged && m_widgets.display)
        m_widgets.display->verticalScrollBar()->setValue(m_widgets.display->verticalScrollBar()->maximum());

    if (m_bulkHistoryChanged || m_bulkVariablesChanged || m_bulkFunctionsChanged || m_bulkUnitsChanged || m_sessionSavePending)
        saveSessionToDefaultPath();
}

void MainWindow::editHistoryEntryContext(int index)
{
    const int historySize = m_session->historySize();
    if (index < 0 || index >= historySize)
        return;

    HistoryEntry entry = m_session->historyEntryAt(index);
    EvaluationContext ctx = entry.context();

    ResultSlotsDialog dialog(tr("Calculation Settings"), ctx, this);

    if (dialog.exec() != QDialog::Accepted)
        return;

    ctx = dialog.evaluationContext(ctx);

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
        rebuiltEntry.setEditTimestamp(entry.editTimestamp());
        rebuiltEntry.setRenderedLines(renderedLinesForHistoryEntry(rebuiltEntry, m_settings, m_evaluator));
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
    saveSessionToDefaultPath();
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
    saveSessionToDefaultPath();
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
    saveSessionToDefaultPath();
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
    const QTextCursor displayCursor = m_widgets.display->textCursor();
    if (displayCursor.hasSelection()) {
        clearTextEditSelection(m_widgets.editor);
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
    if (m_widgets.editor->textCursor().hasSelection()) {
        clearTextEditSelection(m_widgets.display);
        return;
    }

    if (m_widgets.editor->text().trimmed().isEmpty())
        hideStateLabel();
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
    m_currentResultPreviewDismissed = false;
    captureEditorTextInCurrentSession();
    m_widgets.display->clearHoverFeedback();
    clearTextEditSelection(m_widgets.display);
    if (m_widgets.editor->text().trimmed().isEmpty()) {
        hideStateLabel();
        if (m_widgets.bitField)
            m_widgets.bitField->clear();
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

    QTimer::singleShot(0, this, [this]() {
        updateSplitterStyleSheet();
    });
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
    if (primaryMainWindow() == this) {
        const bool shutdownAlreadyInProgress = applicationShutdownInProgress();
        if (!shutdownAlreadyInProgress) {
            // Mark shutdown before saving/restoring layouts so child close events
            // caused by QApplication::quit() do not rewrite the multi-window layout
            // as if each child had been closed manually.
            qApp->setProperty("speedcrunchShutdownInProgress", true);
            appShutdownInProgress() = true;
        }
        persistSessionAndSettingsForShutdown();
        if (!shutdownAlreadyInProgress)
            qApp->quit();
    } else {
        if (!applicationShutdownInProgress()) {
            allMainWindows().removeAll(QPointer<MainWindow>(this));
            saveSessionLayout(false);
        }
    }
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
    if (m_sessionSavePending)
        flushPendingSessionSave();

    if (m_widgets.manual)
        m_settings->manualWindowGeometry = m_settings->windowPositionSave ? m_widgets.manual->saveGeometry() : QByteArray();
    ensureSessionsPath();
    QSet<Session*> savedSessions;
    // Shutdown has to persist every restored window, not just the primary one.
    // Saving by Session* avoids duplicate writes when a session is referenced
    // by more than one pane or window during layout transitions.
    for (const QPointer<MainWindow>& ptr : allMainWindows()) {
        MainWindow* window = ptr.data();
        if (window == nullptr)
            continue;

        window->captureEditorTextInCurrentSession();
        for (auto it = window->m_loadedSessions.constBegin(); it != window->m_loadedSessions.constEnd(); ++it) {
            Session* session = it.value();
            if (session == nullptr || savedSessions.contains(session))
                continue;
            savedSessions.insert(session);

            saveSessionAsync(*session, sessionFilePath(it.key()));
        }
    }
    waitForAsyncSessionIo();
    saveSessionLayout();
    saveSettings();
}

void MainWindow::saveSessionToDefaultPath()
{
    m_sessionSavePending = true;
    m_deferredSessionSaveTimer->start();
}

void MainWindow::flushPendingSessionSave()
{
    if (!m_sessionSavePending)
        return;

    m_sessionSavePending = false;
    ensureSessionsPath();
    captureEditorTextInCurrentSession();

    const QString sessionName = m_session->name().isEmpty()
        ? QLatin1String(SessionJsonKeys::SessionValueMain)
        : m_session->name();
    QString dataPath = sessionFilePath(sessionName);
    saveSessionAsync(*m_session, dataPath);
}

void MainWindow::setResultPrecision(int p)
{
    if (m_settings->resultPrecision == p)
        return;

    m_settings->resultPrecision = p;
    for (const QPointer<MainWindow>& ptr : allMainWindows()) {
        if (MainWindow* window = ptr.data())
            window->setStatusBarText();
    }
    emit resultPrecisionChanged();
}

void MainWindow::setResultFormat(char c)
{
    if (m_settings->resultFormat == c)
        return;

    m_settings->resultFormat = c;
    for (const QPointer<MainWindow>& ptr : allMainWindows()) {
        if (MainWindow* window = ptr.data())
            window->setStatusBarText();
    }
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
        for (const QPointer<MainWindow>& ptr : allMainWindows()) {
            MainWindow* window = ptr.data();
            if (window == nullptr)
                continue;
            window->setStatusBarText();
            for (Editor* editor : window->splitPaneEditors())
                editor->refreshAutoCalc();
        }
    });
    dialog.exec();
}

void MainWindow::increaseDisplayFontPointSize()
{
    if (m_widgets.display != nullptr)
        m_widgets.display->increaseFontPointSize();
    const QFont displayFont = m_widgets.display->font();
    for (ResultDisplay* display : splitPaneDisplays()) {
        if (display != m_widgets.display)
            display->setFont(displayFont);
        if (Editor* editor = display->parentWidget()->findChild<Editor*>())
            editor->setFont(displayFont);
    }
    if (m_widgets.state->isVisible())
        showStateLabel(m_widgets.state->text());
}

void MainWindow::decreaseDisplayFontPointSize()
{
    if (m_widgets.display != nullptr)
        m_widgets.display->decreaseFontPointSize();
    const QFont displayFont = m_widgets.display->font();
    for (ResultDisplay* display : splitPaneDisplays()) {
        if (display != m_widgets.display)
            display->setFont(displayFont);
        if (Editor* editor = display->parentWidget()->findChild<Editor*>())
            editor->setFont(displayFont);
    }
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

    const GeneratedThemeSurfaces surfaces = generatedSurfaceColors(m_settings);
    applyMenuSurface(&menu, surfaces.headersAndBorders, surfaces.inputs);
    menu.exec(m_status.resultPrecision->mapToGlobal(point));
}

void MainWindow::showKeypadContextMenu(const QPoint& point)
{
    if (!m_widgets.keypad)
        return;
    m_menus.keypad->popup(m_widgets.keypad->mapToGlobal(point));
}
