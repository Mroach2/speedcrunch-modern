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
#include "gui/splittertreeutils.h"
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
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QDropEvent>
#include <QPushButton>
#include <QScreen>
#include <QGuiApplication>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QScrollBar>
#include <QSet>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <algorithm>
#include <functional>
#include <limits>
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

    QFile mainSession(QDir(sessionDirPath).filePath(QStringLiteral("main.json")));
    if (!mainSession.open(QIODevice::WriteOnly))
        return;

    if (mainSession.write(data) != data.size())
        return;

    mainSession.close();
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

    Evaluator* evaluator = Evaluator::instance();
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

bool shouldDeleteSessionFileOnClose(const QString& sessionName, const Session* session)
{
    return untitledSessionNumber(sessionName) > 0 && isReusableUntitledSession(session);
}

QJsonObject sessionLayoutEntry(const QString& name, const QPair<int, int>& viewportAnchor, int scrollValue)
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
    return entry;
}

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
    case ColorScheme::List: return QStringLiteral("list");
    case ColorScheme::Unit: return QStringLiteral("unit");
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
    button->setStyleSheet(QStringLiteral(R"(
        QPushButton {
            background-color: %1;
            color: %2;
        }
    )").arg(color.name(), textColor));
}

enum class ColorSchemeFilter {
    Dark,
    Light
};

bool colorSchemeMatchesFilter(const ColorScheme& scheme, ColorSchemeFilter filter)
{
    const QColor background = scheme.colorForRole(ColorScheme::Background);
    const int brightness = qRound(0.299 * background.red()
        + 0.587 * background.green()
        + 0.114 * background.blue());

    return filter == ColorSchemeFilter::Dark ? brightness < 128 : brightness >= 128;
}

QColor splitterHandleColorForScheme(const QString& colorSchemeName)
{
    const ColorScheme scheme = ColorScheme::loadByName(colorSchemeName);
    const QColor background = scheme.isValid()
        ? scheme.colorForRole(ColorScheme::Background)
        : QApplication::palette().color(QPalette::Base);
    const int factor = 200;
    return background.lightnessF() >= 0.5
        ? background.darker(factor)
        : background.lighter(factor);
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

bool& appShutdownInProgress()
{
    static bool shuttingDown = false;
    return shuttingDown;
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

    void applyStyle(const QColor& selectedText)
    {
        const QColor fg = selectedText.isValid()
            ? selectedText
            : palette().color(QPalette::WindowText);

            setStyleSheet(QStringLiteral(R"(
                QTabBar::tab {
                    border: 1px solid palette(mid);
                    border-radius: 0px;
                    padding: 5px 12px 5px 12px;
                    margin: 0px;
                }

                QTabBar::tab:selected {
                    border-radius: 0px;
                    color: %2;
                }

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
            )").arg(fg.name()));
    }

    std::function<void(const QString&, const QPoint&)> tabContextMenuRequested;
    std::function<void(SessionTabBar*, const QString&, int)> sessionTabDropped;
    std::function<MainWindow*()> sourceMainWindow;
    std::function<void(const QString&, const QPoint&)> sessionTabDetached;
    std::function<void(const QString&)> tabCloseRequested;

    void refreshCloseButtons()
    {
        for (int i = 0; i < count(); ++i) {
            const bool showButton = i == currentIndex() || i == m_hoveredTabIndex;
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
                        background: rgba(127, 127, 127, 96);
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
            closeButton->setVisible(showButton);
        }
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
            m_dragStartPos = event->pos();
            m_dragTabIndex = tabAt(event->pos());
            m_dragSessionName = m_dragTabIndex >= 0 ? tabText(m_dragTabIndex) : QString();
        }

        QTabBar::mousePressEvent(event);
        refreshCloseButtons();
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const int hoveredTabIndex = tabAt(event->pos());
        if (m_hoveredTabIndex != hoveredTabIndex) {
            m_hoveredTabIndex = hoveredTabIndex;
            refreshCloseButtons();
        }
        setCursor(hoveredTabIndex >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);

        const bool isDraggingTab = (event->buttons() & Qt::LeftButton)
            && m_dragTabIndex >= 0
            && (event->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance();
        if (!isDraggingTab) {
            QTabBar::mouseMoveEvent(event);
            refreshCloseButtons();
            return;
        }
        if (!shouldStartCrossBarDrag(event->pos())) {
            QMouseEvent clampedEvent(event->type(),
                                     clampTabDragPos(event->pos()),
                                     event->globalPosition(),
                                     event->button(),
                                     event->buttons(),
                                     event->modifiers());
            QTabBar::mouseMoveEvent(&clampedEvent);
            refreshCloseButtons();
            return;
        }

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
        QTabBar::leaveEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QTabBar::paintEvent(event);
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

private:
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

    bool shouldStartCrossBarDrag(const QPoint& pos) const
    {
        if (count() == 0)
            return true;

        return pos.y() < 0
            || pos.y() >= height()
            || pos.x() < tabRect(0).left() - QApplication::startDragDistance()
            || pos.x() > tabRect(count() - 1).right() + QApplication::startDragDistance();
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

    QPoint m_dragStartPos;
    int m_dragTabIndex = -1;
    int m_hoveredTabIndex = -1;
    int m_dropIndicatorIndex = -1;
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
        setAcceptDrops(true);
        m_overlay = new QWidget(this);
        m_overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_overlay->setStyleSheet(QStringLiteral("background: rgba(0, 0, 0, 80);"));
        m_overlay->hide();
    }

    std::function<void(SessionTabBar*, const QString&, const QPoint&)> sessionTabDroppedOnPane;
    std::function<bool(SessionTabBar*)> shouldShowOverlayForDrag;
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

    m_menus.symbols = m_menus.settings->addMenu("");
    m_menus.symbols->addAction(m_actions.sessionImportUserDefinitions);

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
    m_widgets.splitContainer->setHandleWidth(1);
    updateSplitterStyleSheet();
    m_layouts.root->addWidget(m_widgets.splitContainer, 1);

    m_widgets.display = new ResultDisplay();
    m_widgets.display->setFrameStyle(QFrame::NoFrame);
    m_widgets.editor = new Editor();
    m_widgets.editor->setFrameStyle(QFrame::NoFrame);
    m_widgets.editor->setFocus();
    m_widgets.editor->installEventFilter(this);
    m_widgets.splitContainer->addWidget(createEditorDisplayPane(m_widgets.display, m_widgets.editor));
    m_paneSessionNames.insert(m_widgets.display, m_session ? m_session->name() : QString());
    m_paneSessionTabs.insert(m_widgets.display, QStringList(m_session ? m_session->name() : QString()));
    m_widgets.display->setSession(m_session);

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
    SessionPane* pane = new SessionPane(m_widgets.splitContainer);
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
    QWidget* page = new QWidget(stack);
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

    connect(tabBar, &QTabBar::currentChanged, this, [this, tabBar, display](int index) {
        if (index < 0)
            return;
        switchPaneToSession(display, tabBar->tabText(index));
        tabBar->refreshCloseButtons();
    });
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
            if (sessionToDelete != nullptr)
                delete sessionToDelete;
        }

        Session* movedSession = new Session();
        detachedWindow->m_evaluator->setSession(movedSession);
        movedSession->deSerialize(sourceJson, false);
        movedSession->setName(sessionName);
        detachedWindow->m_loadedSessions.insert(sessionName, movedSession);
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
    pane->watchDropTarget(stack);
    pane->setOverlayAreaWidget(stack);
    pane->watchDropTarget(page);
    pane->watchDropTarget(display);
    pane->watchDropTarget(display->viewport());
    pane->watchDropTarget(editor);
    pane->watchDropTarget(editor->viewport());

    return pane;
}

void MainWindow::setActiveEditorDisplayPane(ResultDisplay* display, Editor* editor)
{
    if (display == nullptr || editor == nullptr)
        return;
    if (m_widgets.display == display && m_widgets.editor == editor) {
        updatePaneEditorCursorVisibility();
        return;
    }

    captureEditorTextInCurrentSession();
    m_widgets.display = display;
    m_widgets.editor = editor;
    m_copyWidget = editor;

    const QString sessionName = m_paneSessionNames.value(display);
    Session* paneSession = m_loadedSessions.value(sessionName, nullptr);
    if (paneSession != nullptr && paneSession != m_session)
        activateSession(paneSession);
    updatePaneEditorCursorVisibility();
}

void MainWindow::configureEditorDisplayPane(ResultDisplay* display, Editor* editor)
{
    if (display == nullptr || editor == nullptr)
        return;

    editor->installEventFilter(this);

    connect(editor, &Editor::textChanged, this, [this, display, editor]() {
        setActiveEditorDisplayPane(display, editor);
    });
    connect(editor, &Editor::returnPressed, this, [this, display, editor]() {
        setActiveEditorDisplayPane(display, editor);
        evaluateEditorExpression();
    });
    connect(editor, &Editor::escapePressed, this, [this, display, editor]() {
        setActiveEditorDisplayPane(display, editor);
        handleEditorEscapePressed();
    });
    connect(editor, &Editor::selectionChanged, this, [this, display, editor]() {
        setActiveEditorDisplayPane(display, editor);
        handleEditorSelectionChange();
    });
    connect(editor, &Editor::autoCalcDisabled, this, &MainWindow::hideStateLabel);
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
    connect(editor, &Editor::textChanged, this, &MainWindow::handleEditorTextChange);
    connect(editor, &Editor::copyAvailable, this, &MainWindow::handleCopyAvailable);
    connect(editor, &Editor::copySequencePressed, this, &MainWindow::copy);
    connect(this, &MainWindow::historyChanged, editor, &Editor::updateHistory);

    connect(display, &ResultDisplay::clicked, this, [this, display, editor]() {
        setActiveEditorDisplayPane(display, editor);
        hideStateLabel();
    });
    connect(display, &ResultDisplay::copyAvailable, this, &MainWindow::handleCopyAvailable);
    connect(display, &ResultDisplay::expressionSelected, this, [this, display, editor](const QString& text) {
        setActiveEditorDisplayPane(display, editor);
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
        setActiveEditorDisplayPane(display, editor);
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
        nestedSplitter->setHandleWidth(1);
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
    updatePaneLoadedSessionCounts();
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
    if (!names.contains(name, Qt::CaseInsensitive))
        names.append(name);
    m_paneSessionTabs.insert(m_widgets.display, names);
    m_paneSessionNames.insert(m_widgets.display, name);
    updatePaneLoadedSessionCounts();
    updatePaneTabBars();
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

    setActiveEditorDisplayPane(display, editor);
    activateSession(session);
    updatePaneTabBars();
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
        m_evaluator->setSession(movedSession);
        movedSession->deSerialize(movedJson, false);
        movedSession->setName(name);
        m_loadedSessions.insert(name, movedSession);

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
            display->setSession(session);
            display->refresh();
            QWidget* page = display->parentWidget();
            if (Editor* editor = page ? page->findChild<Editor*>(QString(), Qt::FindDirectChildrenOnly) : nullptr) {
                editor->setText(session->editorText());
                editor->setCursorPosition(editor->text().size());
                editor->updateHistory();
                editor->refreshAutoCalc();
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
        nestedSplitter->setHandleWidth(1);
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
    for (Editor* editor : splitPaneEditors())
        editor->setCustomCursorVisible(editor == m_widgets.editor);
}

void MainWindow::updatePaneTabBars()
{
    const QList<ResultDisplay*> displays = splitPaneDisplays();
    const bool singlePaneSingleTab = displays.size() == 1 && paneSessionNames(displays.first()).size() == 1;
    const ColorScheme scheme = ColorScheme::loadByName(m_settings ? m_settings->colorScheme : QString());
    const QColor activeTabText = scheme.isValid()
        ? scheme.colorForRole(ColorScheme::Number)
        : palette().color(QPalette::WindowText);
    for (ResultDisplay* display : displays) {
        QTabBar* tabBar = displayTabBar(display);
        if (tabBar == nullptr)
            continue;
        static_cast<SessionTabBar*>(tabBar)->applyStyle(activeTabText);

        const QSignalBlocker blocker(tabBar);
        while (tabBar->count() > 0)
            tabBar->removeTab(0);
        const QStringList names = paneSessionNames(display);
        for (const QString& name : names)
            tabBar->addTab(name);

        const int activeIndex = names.indexOf(m_paneSessionNames.value(display));
        tabBar->setCurrentIndex(activeIndex >= 0 ? activeIndex : 0);
        // tabBar->setUsesScrollButtons(names.size() > 4);
        tabBar->setVisible(!singlePaneSingleTab);
        static_cast<SessionTabBar*>(tabBar)->refreshCloseButtons();
    }
    updateSessionWindowTitle();
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

    const QColor handle = splitterHandleColorForScheme(m_settings ? m_settings->colorScheme : QString());
    const QString styleSheet = QStringLiteral("QSplitter::handle { background: %1; }").arg(handle.name());
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
}

void MainWindow::refreshPaneThemes()
{
    for (ResultDisplay* display : splitPaneDisplays())
        display->rehighlight();
    for (Editor* editor : splitPaneEditors())
        editor->rehighlight();
    updatePaneTabBars();
    updateSplitterStyleSheet();
}

void MainWindow::createBitField() {
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
    m_widgets.keypad->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_layouts.root->addLayout(m_layouts.keypad, 0);

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
    ResultDisplay* initialDisplay = m_widgets.display;
    Editor* initialEditor = m_widgets.editor;
    connect(initialEditor, &Editor::textChanged, this, [this, initialDisplay, initialEditor]() {
        setActiveEditorDisplayPane(initialDisplay, initialEditor);
    });
    connect(initialEditor, &Editor::selectionChanged, this, [this, initialDisplay, initialEditor]() {
        setActiveEditorDisplayPane(initialDisplay, initialEditor);
    });
    connect(initialDisplay, &ResultDisplay::clicked, this, [this, initialDisplay, initialEditor]() {
        setActiveEditorDisplayPane(initialDisplay, initialEditor);
    });
    connect(initialDisplay, &ResultDisplay::selectionChanged, this, [this, initialDisplay, initialEditor]() {
        setActiveEditorDisplayPane(initialDisplay, initialEditor);
    });
    connect(this, &MainWindow::colorSchemeChanged, this, &MainWindow::updateSplitterStyleSheet);
    connect(this, &MainWindow::colorSchemeChanged, this, &MainWindow::refreshPaneThemes);
    connect(this, &MainWindow::syntaxHighlightingChanged, this, &MainWindow::refreshPaneThemes);

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
    connect(m_actions.helpFacebookGroup, SIGNAL(triggered()), SLOT(openFacebookGroupURL()));
    connect(m_actions.helpNews, SIGNAL(triggered()), SLOT(openNewsURL()));
    connect(m_actions.helpSource, SIGNAL(triggered()), SLOT(openSourceURL()));
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
    bindStandardKey(QKeySequence::Quit, [this]() { close(); });

    QShortcut* splitRightShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+\\")), this);
    connect(splitRightShortcut, &QShortcut::activated, this, &MainWindow::splitActivePaneRight);
    QShortcut* splitDownShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+\\")), this);
    connect(splitDownShortcut, &QShortcut::activated, this, &MainWindow::splitActivePaneDown);
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

    UserDefinitions::loadInto(m_settings);

    restoreSession(m_settings->historySaving != Settings::HistorySavingNever);
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
    for (ResultDisplay* display : splitPaneDisplays())
        display->setFont(font);
    for (Editor* editor : splitPaneEditors())
        editor->setFont(font);

    if (m_widgets.display != nullptr)
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
    if (m_widgets.display != nullptr)
        m_settings->displayFont = m_widgets.display->font().toString();

    m_settings->save();
}

void MainWindow::saveSession(QString & fname, bool saveHistory)
{
    captureEditorTextInCurrentSession();

    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }


    QJsonObject json;
    m_session->serialize(json);
    if (!saveHistory)
        json.remove(QLatin1String(SessionJsonKeys::History));
    QJsonDocument doc(json);
    file.write(doc.toJson(QJsonDocument::Compact));

    file.close();
}

void MainWindow::saveSessionLayout(bool captureCurrentViewport)
{
    if (captureCurrentViewport && m_session != nullptr && m_widgets.display != nullptr) {
        m_sessionViewportAnchors.insert(m_session->name(), m_widgets.display->viewportTopAnchor());
        QScrollBar* bar = m_widgets.display->verticalScrollBar();
        const int scrollValue = bar->value() == bar->maximum()
            ? std::numeric_limits<int>::max()
            : bar->value();
        m_sessionScrollValues.insert(m_session->name(), scrollValue);
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
    for (const QString& name : sessionNames)
        tabs.append(sessionLayoutEntry(name,
                                       m_sessionViewportAnchors.value(name, qMakePair(-1, 0)),
                                       m_sessionScrollValues.value(name, -1)));

    const auto layoutNodeForWidget = [this](QWidget* widget, const auto& layoutNodeForWidgetRef) -> QJsonObject {
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
        for (const QString& name : paneNames) {
            paneTabs.append(sessionLayoutEntry(name,
                                               m_sessionViewportAnchors.value(name, qMakePair(-1, 0)),
                                               m_sessionScrollValues.value(name, -1)));
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
        for (const QString& name : names) {
            windowTabs.append(sessionLayoutEntry(name,
                windowObject->m_sessionViewportAnchors.value(name, qMakePair(-1, 0)),
                windowObject->m_sessionScrollValues.value(name, -1)));
        }

        const auto nodeForWidget = [windowObject](QWidget* widget, const auto& selfRef) -> QJsonObject {
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
            node.insert(QStringLiteral("active"), windowObject->m_paneSessionNames.value(display));
            QJsonArray paneTabs;
            for (const QString& name : windowObject->paneSessionNames(display)) {
                paneTabs.append(sessionLayoutEntry(name,
                    windowObject->m_sessionViewportAnchors.value(name, qMakePair(-1, 0)),
                    windowObject->m_sessionScrollValues.value(name, -1)));
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
        window.insert(QStringLiteral("bitfieldVisible"), windowObject->m_widgets.bitField != nullptr);
        window.insert(QStringLiteral("keypadVisible"), windowObject->m_widgets.keypad != nullptr);
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

    if (m_session != nullptr
            && m_session != session
            && (m_widgets.display == nullptr || m_paneSessionNames.value(m_widgets.display) == m_session->name())) {
        captureEditorTextInCurrentSession();
        m_sessionViewportAnchors.insert(m_session->name(), m_widgets.display->viewportTopAnchor());
        QScrollBar* bar = m_widgets.display->verticalScrollBar();
        const int scrollValue = bar->value() == bar->maximum()
            ? std::numeric_limits<int>::max()
            : bar->value();
        m_sessionScrollValues.insert(m_session->name(), scrollValue);
    }

    m_session = session;
    m_evaluator->setSession(m_session);
    m_evaluator->initializeBuiltInVariables();
    if (m_widgets.display != nullptr)
        m_widgets.display->setSession(m_session);
    if (m_widgets.display != nullptr)
        m_paneSessionNames.insert(m_widgets.display, m_session->name());
    if (m_widgets.display != nullptr)
        addSessionToActivePane(m_session->name());
    m_pendingHistoryEditIndex = -1;
    if (m_widgets.display != nullptr)
        m_widgets.display->setEditingHistoryIndex(-1);
    if (m_widgets.editor != nullptr)
        m_widgets.editor->setHistoryArrowNavigationEnabled(true);
    restoreEditorTextFromCurrentSession();
    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();
    emit unitsChanged();
    QTimer::singleShot(0, this, [this]() {
        emit historyChanged();
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
    m_conditions.autoAns = !m_session->historyIsEmpty();
    updatePaneEditorCursorVisibility();
}

void MainWindow::captureEditorTextInCurrentSession()
{
    if (m_session == nullptr || m_widgets.editor == nullptr)
        return;

    m_session->setEditorText(m_widgets.editor->text());
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
    if (primaryMainWindow().isNull())
        primaryMainWindow() = this;
    allMainWindows().append(QPointer<MainWindow>(this));
    if (objectName().isEmpty())
        setObjectName(QStringLiteral("window-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)));
    windowIds().insert(objectName(), QPointer<MainWindow>(this));

    m_session = new Session();
    m_loadedSessions.insert(m_session->name(), m_session);
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
    m_status.complexForm = 0;
    m_status.complexFormSection = 0;
    m_status.complexFormLabel = 0;

    m_copyWidget = 0;
    m_pendingHistoryEditIndex = -1;
    m_shutdownStateSaved = false;
    m_versionCheck = 0;

    createUi();
    applySettings();
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
    if (m_settings->historySaving == Settings::HistorySavingContinuously)
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
    Session* selectedSession = m_loadedSessions.value(entry.name, nullptr);
    if (selectedSession == nullptr) {
        selectedSession = new Session();
        m_evaluator->setSession(selectedSession);
        selectedSession->deSerialize(entry.json, false);
        selectedSession->setName(entry.name);
        m_loadedSessions.insert(entry.name, selectedSession);
        updatePaneLoadedSessionCounts();
    } else if (selectedSession == m_session) {
        m_evaluator->setSession(selectedSession);
        selectedSession->deSerialize(entry.json, false);
        selectedSession->setName(entry.name);
    }

    if (selectedSession != m_session && m_settings->historySaving == Settings::HistorySavingContinuously)
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
        m_evaluator->setSession(duplicateSession);
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

        if (m_settings->historySaving == Settings::HistorySavingContinuously)
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
            if (m_settings->historySaving == Settings::HistorySavingContinuously) {
                if (shouldDeleteSessionFileOnClose(closingName, m_session))
                    QFile::remove(sessionFilePath(closingName));
                else
                    saveSessionToDefaultPath();
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
    if (m_settings->historySaving == Settings::HistorySavingContinuously) {
        if (shouldDeleteSessionFileOnClose(closingName, m_session))
            QFile::remove(sessionFilePath(closingName));
        else
            saveSessionToDefaultPath();
    }

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
    m_evaluator->setSession(nullptr);

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
    if (m_settings->historySaving == Settings::HistorySavingContinuously)
        saveSessionToDefaultPath();
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

    QAction* selectedAction = menu.exec(globalPos);
    if (selectedAction == nullptr)
        return;

    const QString name = selectedAction->data().toString();
    Session* selectedSession = m_loadedSessions.value(name, nullptr);
    if (selectedSession == nullptr || selectedSession == m_session)
        return;

    if (m_settings->historySaving == Settings::HistorySavingContinuously)
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
    dialog.setWindowTitle(tr("Theme"));

    QVBoxLayout* layout = new QVBoxLayout(&dialog);

    QWidget* themeWidget = new QWidget(&dialog);
    QHBoxLayout* themeLayout = new QHBoxLayout(themeWidget);
    themeLayout->setContentsMargins(0, 0, 0, 0);
    themeLayout->setSpacing(10);

    QGroupBox* lightThemesGroup = new QGroupBox(tr("Light Themes"), themeWidget);
    QVBoxLayout* lightThemesLayout = new QVBoxLayout(lightThemesGroup);
    QListWidget* lightThemeList = new QListWidget(lightThemesGroup);
    lightThemesLayout->addWidget(lightThemeList);
    themeLayout->addWidget(lightThemesGroup);

    QGroupBox* darkThemesGroup = new QGroupBox(tr("Dark Themes"), themeWidget);
    QVBoxLayout* darkThemesLayout = new QVBoxLayout(darkThemesGroup);
    QListWidget* darkThemeList = new QListWidget(darkThemesGroup);
    darkThemesLayout->addWidget(darkThemeList);
    themeLayout->addWidget(darkThemesGroup);
    layout->addWidget(themeWidget);

    QGroupBox* previewGroup = new QGroupBox(tr("Preview"), &dialog);
    QVBoxLayout* previewGroupLayout = new QVBoxLayout(previewGroup);

    QWidget* previewWidget = new QWidget(previewGroup);
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

    QPlainTextEdit* editorPreview = new QPlainTextEdit(previewWidget);
    editorPreview->setReadOnly(true);
    editorPreview->setFrameShape(QFrame::NoFrame);
    editorPreview->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editorPreview->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editorPreview->setPlainText(QStringLiteral("sqrt(144) + sin(π / 2)"));
    editorPreview->setFixedHeight(m_widgets.editor ? m_widgets.editor->height() : editorPreview->sizeHint().height());
    auto editorPreviewHighlighter = new SyntaxHighlighter(editorPreview);
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
    const auto applyColorsToControls = [&colorsByRole, &roleButtons, roleEntries]() {
        for (const auto& roleEntry : roleEntries)
            updateColorButtonStyle(roleButtons.value(roleEntry.second), colorsByRole.value(roleEntry.second));
    };
    const auto setColorsFromScheme = [&colorsByRole, roleEntries](const ColorScheme& scheme) {
        for (const auto& roleEntry : roleEntries)
            colorsByRole[roleEntry.second] = scheme.colorForRole(roleEntry.second);
    };
    const auto applyPreview = [&colorsByRole, preview, previewHighlighter, previewScrollbarTrack, previewScrollbar, editorPreview, editorPreviewHighlighter]() {
        QJsonObject object;
        const auto roles = ColorScheme::roleNames();
        for (const auto& roleEntry : roles)
            object.insert(roleEntry.first, colorsByRole.value(roleEntry.second).name());
        const ColorScheme scheme = ColorScheme::fromJsonObject(object);
        previewHighlighter->setColorScheme(ColorScheme::fromJsonObject(scheme.toJsonObject()));
        editorPreviewHighlighter->setColorScheme(ColorScheme::fromJsonObject(scheme.toJsonObject()));
        QPalette palette = preview->palette();
        palette.setColor(QPalette::Base, scheme.colorForRole(ColorScheme::Background));
        preview->setPalette(palette);
        previewHighlighter->rehighlight();
        previewScrollbarTrack->setStyleSheet(QStringLiteral("background-color: %1;").arg(scheme.colorForRole(ColorScheme::Background).name()));

        QPalette editorPalette = editorPreview->palette();
        editorPalette.setColor(QPalette::Base, scheme.colorForRole(ColorScheme::EditorBackground));
        editorPalette.setColor(QPalette::Text, scheme.colorForRole(ColorScheme::Number));
        editorPreview->setPalette(editorPalette);
        editorPreviewHighlighter->rehighlight();
        previewScrollbar->setStyleSheet(QStringLiteral("background-color: %1;").arg(scheme.colorForRole(ColorScheme::ScrollBar).name()));
    };
    const auto updateThemeListHeight = [](QListWidget* list) {
        const int visibleRows = qMin(list->count(), 7);
        const int rowHeight = list->sizeHintForRow(0) > 0 ? list->sizeHintForRow(0) : list->fontMetrics().height() + 6;
        list->setMaximumHeight(rowHeight * visibleRows + list->frameWidth() * 2);
    };
    const auto populateThemeList = [&](QListWidget* list, ColorSchemeFilter filter) {
        const QSignalBlocker blocker(list);
        list->clear();
        for (const auto& schemeName : schemeNames) {
            const ColorScheme scheme = ColorScheme::loadByName(schemeName);
            if (!scheme.isValid() || !colorSchemeMatchesFilter(scheme, filter))
                continue;
            auto item = new QListWidgetItem(schemeName, list);
            item->setData(Qt::UserRole, schemeName);
            if (schemeName == selectedSchemeName)
                list->setCurrentItem(item);
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
            list->setCurrentItem(item);
            item->setSelected(true);
            list->scrollToItem(item, QAbstractItemView::PositionAtTop);
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
    constexpr int roleRowsPerColumn = 5;
    for (const auto& roleEntry : roleEntries) {
        const ColorScheme::Role role = roleEntry.second;
        QLabel* roleLabel = new QLabel(colorSchemeRoleLabel(role), rolesWidget);
        QPushButton* colorButton = new QPushButton(rolesWidget);
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
            applyPreview();
        });
        ++roleIndex;
    }

    applyPreview();
    populateThemeLists();
    QTimer::singleShot(0, &dialog, populateThemeLists);

    const auto handleThemeSelection = [&](QListWidget* otherList, QListWidgetItem* current) {
        if (!current)
            return;
        const QSignalBlocker blocker(otherList);
        otherList->clearSelection();
        otherList->setCurrentItem(nullptr);
        const QString schemeName = current->data(Qt::UserRole).toString();
        const ColorScheme scheme = ColorScheme::loadByName(schemeName);
        if (!scheme.isValid())
            return;
        selectedSchemeName = schemeName;
        isCustomScheme = false;
        setColorsFromScheme(scheme);
        applyColorsToControls();
        applyPreview();
    };
    connect(lightThemeList, &QListWidget::currentItemChanged, &dialog, [&](QListWidgetItem* current) {
        handleThemeSelection(darkThemeList, current);
    });
    connect(darkThemeList, &QListWidget::currentItemChanged, &dialog, [&](QListWidgetItem* current) {
        handleThemeSelection(lightThemeList, current);
    });

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QPushButton* applyButton = buttons->addButton(QDialogButtonBox::Apply);
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
        m_actions.settingsDisplayColorSchemeCustom->setChecked(m_settings->colorScheme == QLatin1String("Custom"));
        emit colorSchemeChanged();
    };
    connect(applyButton, &QPushButton::clicked, this, applyCurrentTheme);

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
            updateColorButtonStyle(roleButtons.value(roleEntry.second), color);
        }
        selectedSchemeName = themeName;
        isCustomScheme = false;
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
        m_evaluator->setSession(m_session);
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
    m_evaluator->setSession(m_session);
    if (m_session != nullptr)
        m_evaluator->initializeBuiltInVariables();
    m_conditions.autoAns = previousAutoAns;
    if (m_widgets.display != nullptr && m_session != nullptr)
        m_widgets.display->setSession(m_session);

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
    if (b && m_widgets.display != nullptr && m_widgets.display->isEmpty())
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

bool MainWindow::event(QEvent* e)
{
    if (e != nullptr && e->type() == QEvent::WindowActivate) {
        const QString activeName = m_paneSessionNames.value(m_widgets.display);
        if (!activeName.isEmpty()) {
            if (Session* activeSession = m_loadedSessions.value(activeName, nullptr))
                activateSession(activeSession);
        } else if (m_session != nullptr) {
            m_evaluator->setSession(m_session);
            m_evaluator->initializeBuiltInVariables();
        }
    }

    return QMainWindow::event(e);
}

bool MainWindow::eventFilter(QObject* o, QEvent* e)
{
    if (Editor* editor = qobject_cast<Editor*>(o)) {
        if (e->type() == QEvent::FocusIn || e->type() == QEvent::MouseButtonPress) {
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
    if (!m_docks.bitField)
        return;

    disconnect(m_widgets.bitField);
    deleteDock(m_docks.bitField);
    m_docks.bitField = nullptr;
    m_widgets.bitField = 0;
    m_actions.viewBitfield->setChecked(false);
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

    QFile file(sessionFilePath(m_session->name()));
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    QJsonDocument doc(QJsonDocument::fromJson(data));
    QJsonObject json = doc.object();
    if (!restoreHistory)
        json.remove(QLatin1String(SessionJsonKeys::History));
    m_session->deSerialize(json, true);
    m_session->setEditorText(json.value(QLatin1String(SessionJsonKeys::Editor)).toString());

    file.close();
    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();
    emit unitsChanged();
    restoreEditorTextFromCurrentSession();

    m_conditions.autoAns = restoreHistory && !m_session->historyIsEmpty();
}

bool MainWindow::restoreSessionLayout(bool restoreHistory)
{
    static bool restoringExtraWindows = false;
    static bool multiWindowSpawnDone = false;
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

    QHash<QString, Session*> restoredSessions;
    m_sessionViewportAnchors.clear();
    m_sessionScrollValues.clear();
    QString activeSessionName = normalizedSessionName(root.value(QStringLiteral("active")).toString());
    Session* activeSession = nullptr;

    for (const QJsonValue& value : tabs) {
        if (!value.isObject())
            continue;

        const QJsonObject tab = value.toObject();
        const QString name = normalizedSessionName(tab.value(QStringLiteral("name")).toString());
        if (loadedSessionNameExists(restoredSessions, name))
            continue;

        QString fileName = tab.value(QStringLiteral("file")).toString();
        if (fileName.isEmpty())
            fileName = sessionFileBaseName(name) + QLatin1String(".json");

        QJsonObject sessionJson;
        const QString filePath = QDir(sessionsPath()).filePath(fileName);
        if (!readValidSessionJson(filePath, &sessionJson))
            continue;

        if (!restoreHistory)
            sessionJson.remove(QLatin1String(SessionJsonKeys::History));

        Session* session = nullptr;
        if (name == m_session->name() && restoredSessions.isEmpty()) {
            session = m_session;
        } else {
            session = new Session();
        }

        m_evaluator->setSession(session);
        session->deSerialize(sessionJson, false);
        session->setName(name);
        restoredSessions.insert(name, session);
        const QJsonObject scroll = tab.value(QStringLiteral("scroll")).toObject();
        const int block = scroll.value(QStringLiteral("block")).toInt(-1);
        const int offset = scroll.value(QStringLiteral("offset")).toInt(0);
        const int scrollValue = scroll.value(QStringLiteral("value")).toInt(-1);
        if (block >= 0)
            m_sessionViewportAnchors.insert(name, qMakePair(block, offset));
        if (scrollValue >= 0)
            m_sessionScrollValues.insert(name, scrollValue);
        if (name == activeSessionName)
            activeSession = session;
    }

    if (restoredSessions.isEmpty())
        return false;

    if (activeSession == nullptr) {
        activeSessionName = restoredSessions.keys().constFirst();
        activeSession = restoredSessions.value(activeSessionName);
    }

    const QFont displayFont = m_widgets.display->font();
    const QFont editorFont = m_widgets.editor->font();
    const QList<Session*> oldSessions = m_loadedSessions.values();
    for (Session* oldSession : oldSessions) {
        if (!restoredSessions.values().contains(oldSession))
            delete oldSession;
    }
    m_loadedSessions = restoredSessions;

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

    const auto createPane = [this, &displayFont, &editorFont, &activeSessionName, &paneNamesFromNode,
                             &activeDisplay, &activeEditor, &firstDisplay, &firstEditor,
                             &firstLoadedName](const QJsonObject& node) -> QWidget* {
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
        display->setSession(m_loadedSessions.value(activeName, nullptr));

        if (firstDisplay == nullptr) {
            firstDisplay = display;
            firstEditor = editor;
        }
        if (activeName == activeSessionName) {
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
        splitter->setHandleWidth(1);
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
        pane.insert(QStringLiteral("active"), activeSessionName);
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
    if (window.contains(QStringLiteral("bitfieldVisible")))
        setBitfieldVisible(window.value(QStringLiteral("bitfieldVisible")).toBool(false));
    if (window.contains(QStringLiteral("keypadVisible")))
        setKeypadVisible(window.value(QStringLiteral("keypadVisible")).toBool(false));
    emit historyChanged();
    emit variablesChanged();
    emit functionsChanged();
    emit unitsChanged();

    if (this == primaryMainWindow() && !restoringExtraWindows && !multiWindowSpawnDone && windows.size() > 1) {
        multiWindowSpawnDone = true;
        restoringExtraWindows = true;
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
        restoringExtraWindows = false;
    }
    return true;
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
    if (m_settings->historySaving == Settings::HistorySavingContinuously)
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
        rebuiltEntry.setEditTimestamp(entry.editTimestamp());
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
        appShutdownInProgress() = true;
        persistSessionAndSettingsForShutdown();
        qApp->quit();
    } else {
        if (!appShutdownInProgress()) {
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
    if (m_widgets.manual) {
        m_widgets.manual->close();
    }
    ensureSessionsPath();
    const bool saveHistory = m_settings->historySaving != Settings::HistorySavingNever;
    for (auto it = m_loadedSessions.constBegin(); it != m_loadedSessions.constEnd(); ++it) {
        Session* session = it.value();
        if (session == nullptr)
            continue;

        QJsonObject json;
        session->serialize(json);
        if (!saveHistory)
            json.remove(QLatin1String(SessionJsonKeys::History));

        QFile file(sessionFilePath(it.key()));
        if (!file.open(QIODevice::WriteOnly))
            continue;
        file.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
        file.close();
    }
    saveSessionLayout();
    saveSettings();
}

void MainWindow::saveSessionToDefaultPath(bool saveHistory)
{
    ensureSessionsPath();

    QJsonObject json;
    m_session->serialize(json);
    const QString sessionName = json.value(QLatin1String(SessionJsonKeys::Session)).toString(
        QLatin1String(SessionJsonKeys::SessionValueMain));
    QString dataPath = sessionFilePath(sessionName);
    saveSession(dataPath, saveHistory);
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
