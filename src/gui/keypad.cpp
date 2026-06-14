// SPDX-FileCopyrightText: 2014, 2017, 2021, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/keypad.h"

#include "core/settings.h"
#include "core/unicodechars.h"
#include "core/mathdsl.h"
#include "gui/oklchutils.h"
#include "gui/uiconfig.h"

#include <QLocale>
#include <QHash>
#include <QApplication>
#include <QGridLayout>
#include <QPoint>
#include <QPushButton>
#include <QStyle>
#include <QStyleOptionButton>
#include <QTimer>

#if QT_VERSION >= 0x040400 && defined(Q_WS_MAC) && !defined(QT_NO_STYLE_MAC)
#include <QMacStyle>
#endif

const Keypad::KeyDescription Keypad::keyDescriptions[] = {
    {QString::fromLatin1("0"), Key0, true, 3, 0},
    {QString::fromLatin1("1"), Key1, true, 2, 0},
    {QString::fromLatin1("2"), Key2, true, 2, 1},
    {QString::fromLatin1("3"), Key3, true, 2, 2},
    {QString::fromLatin1("4"), Key4, true, 1, 0},
    {QString::fromLatin1("5"), Key5, true, 1, 1},
    {QString::fromLatin1("6"), Key6, true, 1, 2},
    {QString::fromLatin1("7"), Key7, true, 0, 0},
    {QString::fromLatin1("8"), Key8, true, 0, 1},
    {QString::fromLatin1("9"), Key9, true, 0, 2},
    {QString::fromLatin1(","), KeyRadixChar, true, 3, 1},
    {QString::fromUtf8("="), KeyEquals, true, 3, 2},
    {QString::fromUtf8("÷"), KeyDivide, true, 0, 3},
    {QString(MathDsl::MulCrossOp), KeyTimes, true, 1, 3},
    {QString(UnicodeChars::MinusSign), KeyMinus, true, 2, 3},
    {QString::fromUtf8("+"), KeyPlus, true, 3, 3},
    {QString::fromLatin1("arccos"), KeyAcos, false, 2, 8},
    {QString::fromLatin1("ans"), KeyAns, false, 1, 9},
    {QString::fromLatin1("arcsin"), KeyAsin, false, 1, 8},
    {QString::fromLatin1("arctan"), KeyAtan, false, 3, 8},
    {QString::fromUtf8("⌧"), KeyClear, false, 0, 4},
    {QString::fromLatin1("cos"), KeyCos, false, 2, 7},
    {QString::fromLatin1("E"), KeyEE, false, 1, 4},
    {QString::fromLatin1("exp"), KeyExp, false, 0, 7},
    {QString::fromLatin1("!"), KeyFactorial, false, 3, 5},
    {QString::fromLatin1("ln"), KeyLn, false, 0, 8},
    {QString::fromLatin1("("), KeyLeftPar, false, 2, 4},
    {QString::fromUtf8("⌫"), KeyBackspace, false, 0, 5},
    {QString::fromLatin1("%"), KeyPercent, false, 3, 4},
    {QString::fromLatin1("^"), KeyRaise, false, 1, 5},
    {QString::fromLatin1(")"), KeyRightPar, false, 2, 5},
    {QString::fromLatin1("sin"), KeySin, false, 1, 7},
    {QString::fromLatin1("tan"), KeyTan, false, 3, 7},
    {QString::fromLatin1("x="), KeyXEquals, false, 3, 9},
    {QString::fromLatin1("x"), KeyX, false, 2, 9},
    {QString::fromUtf8("∛"), KeyCbrt, false, 1, 6},
    {QString::fromLatin1("lg"), KeyLg, false, 2, 6},
    {QString::fromLatin1("mod"), KeyMod, false, 3, 6},
    {QString::fromUtf8("π"), KeyPi, false, 0, 9},
    {QString::fromUtf8("√"), KeySqrt, false, 0, 6}
};

namespace {
struct LayoutEntry {
    Keypad::Button button;
    int row;
    int column;
};

const LayoutEntry s_basicWideLayout[] = {
    {Keypad::Key7, 0, 0}, {Keypad::Key8, 0, 1}, {Keypad::Key9, 0, 2}, {Keypad::KeyDivide, 0, 3}, {Keypad::KeyBackspace, 0, 4},
    {Keypad::Key4, 1, 0}, {Keypad::Key5, 1, 1}, {Keypad::Key6, 1, 2}, {Keypad::KeyTimes, 1, 3}, {Keypad::KeyLeftPar, 1, 4},
    {Keypad::Key1, 2, 0}, {Keypad::Key2, 2, 1}, {Keypad::Key3, 2, 2}, {Keypad::KeyMinus, 2, 3}, {Keypad::KeyRightPar, 2, 4},
    {Keypad::Key0, 3, 0}, {Keypad::KeyRadixChar, 3, 1}, {Keypad::KeyEquals, 3, 2}, {Keypad::KeyPlus, 3, 3}, {Keypad::KeyPercent, 3, 4}
};

const LayoutEntry s_scientificWideLayout[] = {
    {Keypad::Key7, 0, 0}, {Keypad::Key8, 0, 1}, {Keypad::Key9, 0, 2}, {Keypad::KeyDivide, 0, 3},
    {Keypad::KeyClear, 0, 4}, {Keypad::KeyBackspace, 0, 5}, {Keypad::KeySqrt, 0, 6},
    {Keypad::KeyExp, 0, 7}, {Keypad::KeyLn, 0, 8}, {Keypad::KeyPi, 0, 9},
    {Keypad::Key4, 1, 0}, {Keypad::Key5, 1, 1}, {Keypad::Key6, 1, 2}, {Keypad::KeyTimes, 1, 3},
    {Keypad::KeyEE, 1, 4}, {Keypad::KeyRaise, 1, 5}, {Keypad::KeyCbrt, 1, 6},
    {Keypad::KeySin, 1, 7}, {Keypad::KeyAsin, 1, 8}, {Keypad::KeyAns, 1, 9},
    {Keypad::Key1, 2, 0}, {Keypad::Key2, 2, 1}, {Keypad::Key3, 2, 2}, {Keypad::KeyMinus, 2, 3},
    {Keypad::KeyLeftPar, 2, 4}, {Keypad::KeyRightPar, 2, 5}, {Keypad::KeyLg, 2, 6},
    {Keypad::KeyCos, 2, 7}, {Keypad::KeyAcos, 2, 8}, {Keypad::KeyX, 2, 9},
    {Keypad::Key0, 3, 0}, {Keypad::KeyRadixChar, 3, 1}, {Keypad::KeyEquals, 3, 2}, {Keypad::KeyPlus, 3, 3},
    {Keypad::KeyPercent, 3, 4}, {Keypad::KeyFactorial, 3, 5}, {Keypad::KeyMod, 3, 6},
    {Keypad::KeyTan, 3, 7}, {Keypad::KeyAtan, 3, 8}, {Keypad::KeyXEquals, 3, 9}
};

const LayoutEntry s_scientificNarrowLayout[] = {
    {Keypad::Key7, 0, 0}, {Keypad::Key8, 0, 1}, {Keypad::Key9, 0, 2}, {Keypad::KeyDivide, 0, 3}, {Keypad::KeyClear, 0, 4},
    {Keypad::Key4, 1, 0}, {Keypad::Key5, 1, 1}, {Keypad::Key6, 1, 2}, {Keypad::KeyTimes, 1, 3}, {Keypad::KeyBackspace, 1, 4},
    {Keypad::Key1, 2, 0}, {Keypad::Key2, 2, 1}, {Keypad::Key3, 2, 2}, {Keypad::KeyMinus, 2, 3}, {Keypad::KeyLeftPar, 2, 4},
    {Keypad::Key0, 3, 0}, {Keypad::KeyRadixChar, 3, 1}, {Keypad::KeyEquals, 3, 2}, {Keypad::KeyPlus, 3, 3}, {Keypad::KeyRightPar, 3, 4},
    {Keypad::KeyEE, 4, 0}, {Keypad::KeySqrt, 4, 1}, {Keypad::KeyExp, 4, 2}, {Keypad::KeyLn, 4, 3}, {Keypad::KeyPi, 4, 4},
    {Keypad::KeyRaise, 5, 0}, {Keypad::KeyCbrt, 5, 1}, {Keypad::KeySin, 5, 2}, {Keypad::KeyAsin, 5, 3}, {Keypad::KeyAns, 5, 4},
    {Keypad::KeyPercent, 6, 0}, {Keypad::KeyLg, 6, 1}, {Keypad::KeyCos, 6, 2}, {Keypad::KeyAcos, 6, 3}, {Keypad::KeyX, 6, 4},
    {Keypad::KeyFactorial, 7, 0}, {Keypad::KeyMod, 7, 1}, {Keypad::KeyTan, 7, 2}, {Keypad::KeyAtan, 7, 3}, {Keypad::KeyXEquals, 7, 4}
};

void layoutEntries(Keypad::LayoutMode layoutMode, const LayoutEntry** entries, int* count)
{
    *entries = nullptr;
    *count = 0;
    if (layoutMode == Keypad::LayoutModeScientificWide) {
        *entries = s_scientificWideLayout;
        *count = int(sizeof s_scientificWideLayout / sizeof s_scientificWideLayout[0]);
    } else if (layoutMode == Keypad::LayoutModeScientificNarrow) {
        *entries = s_scientificNarrowLayout;
        *count = int(sizeof s_scientificNarrowLayout / sizeof s_scientificNarrowLayout[0]);
    } else if (layoutMode == Keypad::LayoutModeBasicWide) {
        *entries = s_basicWideLayout;
        *count = int(sizeof s_basicWideLayout / sizeof s_basicWideLayout[0]);
    }
}

QString customButtonInsertText(Keypad::Button button, QChar radixCharacter)
{
    switch (button) {
    case Keypad::Key0: return QString::fromLatin1("0");
    case Keypad::Key1: return QString::fromLatin1("1");
    case Keypad::Key2: return QString::fromLatin1("2");
    case Keypad::Key3: return QString::fromLatin1("3");
    case Keypad::Key4: return QString::fromLatin1("4");
    case Keypad::Key5: return QString::fromLatin1("5");
    case Keypad::Key6: return QString::fromLatin1("6");
    case Keypad::Key7: return QString::fromLatin1("7");
    case Keypad::Key8: return QString::fromLatin1("8");
    case Keypad::Key9: return QString::fromLatin1("9");
    case Keypad::KeyPlus: return QString::fromLatin1("+");
    case Keypad::KeyMinus: return QString::fromUtf8("−");
    case Keypad::KeyTimes: return QString(MathDsl::MulCrossOp);
    case Keypad::KeyDivide: return QString::fromUtf8("÷");
    case Keypad::KeyEE: return QString::fromLatin1("e");
    case Keypad::KeyLeftPar: return QString::fromLatin1("(");
    case Keypad::KeyRightPar: return QString::fromLatin1(")");
    case Keypad::KeyRaise: return QString::fromLatin1("^");
    case Keypad::KeyPercent: return QString::fromLatin1("%");
    case Keypad::KeyFactorial: return QString::fromLatin1("!");
    case Keypad::KeyX: return QString::fromLatin1("x");
    case Keypad::KeyXEquals: return QString::fromLatin1("x=");
    case Keypad::KeyPi: return QString::fromLatin1("pi");
    case Keypad::KeyAns: return QString::fromLatin1("ans");
    case Keypad::KeySqrt: return QString::fromLatin1("sqrt(");
    case Keypad::KeyCbrt: return QString::fromLatin1("cbrt(");
    case Keypad::KeyLg: return QString::fromLatin1("lg(");
    case Keypad::KeyMod: return QString::fromLatin1("mod(");
    case Keypad::KeyLn: return QString::fromLatin1("ln(");
    case Keypad::KeyExp: return QString::fromLatin1("exp(");
    case Keypad::KeySin: return QString::fromLatin1("sin(");
    case Keypad::KeyCos: return QString::fromLatin1("cos(");
    case Keypad::KeyTan: return QString::fromLatin1("tan(");
    case Keypad::KeyAcos: return QString::fromLatin1("arccos(");
    case Keypad::KeyAtan: return QString::fromLatin1("arctan(");
    case Keypad::KeyAsin: return QString::fromLatin1("arcsin(");
    case Keypad::KeyRadixChar: return QString(radixCharacter);
    default: break;
    }
    return QString();
}

QString customButtonLabel(Keypad::Button button, QChar radixCharacter)
{
    switch (button) {
    case Keypad::Key0: return QString::fromLatin1("0");
    case Keypad::Key1: return QString::fromLatin1("1");
    case Keypad::Key2: return QString::fromLatin1("2");
    case Keypad::Key3: return QString::fromLatin1("3");
    case Keypad::Key4: return QString::fromLatin1("4");
    case Keypad::Key5: return QString::fromLatin1("5");
    case Keypad::Key6: return QString::fromLatin1("6");
    case Keypad::Key7: return QString::fromLatin1("7");
    case Keypad::Key8: return QString::fromLatin1("8");
    case Keypad::Key9: return QString::fromLatin1("9");
    case Keypad::KeyEquals: return QString::fromLatin1("=");
    case Keypad::KeyPlus: return QString::fromLatin1("+");
    case Keypad::KeyMinus: return QString::fromUtf8("−");
    case Keypad::KeyTimes: return QString(MathDsl::MulCrossOp);
    case Keypad::KeyDivide: return QString::fromUtf8("÷");
    case Keypad::KeyRadixChar: return QString(radixCharacter);
    case Keypad::KeyClear: return QString::fromUtf8("⌧");
    case Keypad::KeyEE: return QString::fromLatin1("E");
    case Keypad::KeyLeftPar: return QString::fromLatin1("(");
    case Keypad::KeyRightPar: return QString::fromLatin1(")");
    case Keypad::KeyRaise: return QString::fromLatin1("^");
    case Keypad::KeySqrt: return QString::fromUtf8("√");
    case Keypad::KeyCbrt: return QString::fromUtf8("∛");
    case Keypad::KeyLg: return QString::fromLatin1("lg");
    case Keypad::KeyMod: return QString::fromLatin1("mod");
    case Keypad::KeyBackspace: return QString::fromUtf8("⌫");
    case Keypad::KeyPercent: return QString::fromLatin1("%");
    case Keypad::KeyFactorial: return QString::fromLatin1("!");
    case Keypad::KeyPi: return QString::fromUtf8("π");
    case Keypad::KeyAns: return QString::fromLatin1("ans");
    case Keypad::KeyX: return QString::fromLatin1("x");
    case Keypad::KeyXEquals: return QString::fromLatin1("x=");
    case Keypad::KeyExp: return QString::fromLatin1("exp");
    case Keypad::KeyLn: return QString::fromLatin1("ln");
    case Keypad::KeySin: return QString::fromLatin1("sin");
    case Keypad::KeyAsin: return QString::fromLatin1("arcsin");
    case Keypad::KeyCos: return QString::fromLatin1("cos");
    case Keypad::KeyAcos: return QString::fromLatin1("arccos");
    case Keypad::KeyTan: return QString::fromLatin1("tan");
    case Keypad::KeyAtan: return QString::fromLatin1("arctan");
    default: break;
    }
    return QString();
}

int customButtonAction(Keypad::Button button)
{
    switch (button) {
    case Keypad::KeyBackspace: return Settings::CustomKeypadActionBackspace;
    case Keypad::KeyClear: return Settings::CustomKeypadActionClearExpression;
    case Keypad::KeyEquals: return Settings::CustomKeypadActionEvaluateExpression;
    default: break;
    }
    return Settings::CustomKeypadActionInsertText;
}

QHash<Keypad::Button, QPoint> createLayoutMap(Keypad::LayoutMode layoutMode)
{
    QHash<Keypad::Button, QPoint> map;

    const LayoutEntry* entries = nullptr;
    int count = 0;
    layoutEntries(layoutMode, &entries, &count);

    for (int i = 0; i < count; ++i)
        map.insert(entries[i].button, QPoint(entries[i].column, entries[i].row));
    return map;
}

QFont scaledFont(const QFont& base, int scalePercent)
{
    const qreal scale = qreal(scalePercent) / 100.0;
    QFont font(base);
    if (font.pointSizeF() > 0.0) {
        font.setPointSizeF(font.pointSizeF() * scale);
    } else if (font.pixelSize() > 0) {
        font.setPixelSize(qMax(1, qRound(font.pixelSize() * scale)));
    }
    return font;
}

enum class KeypadButtonVisualRole {
    Normal,
    ArithmeticOperator,
    Evaluate
};

struct KeypadButtonColors {
    QColor background;
    QColor foreground;
    QColor hoverBackground;
    QColor hoverForeground;
    QColor pressedBackground;
    QColor pressedForeground;
};

bool isArithmeticOperatorButton(Keypad::Button button)
{
    switch (button) {
    case Keypad::KeyPlus:
    case Keypad::KeyMinus:
    case Keypad::KeyTimes:
    case Keypad::KeyDivide:
        return true;
    default:
        return false;
    }
}

KeypadButtonVisualRole visualRoleForButton(Keypad::Button button)
{
    if (button == Keypad::KeyEquals)
        return KeypadButtonVisualRole::Evaluate;
    if (isArithmeticOperatorButton(button))
        return KeypadButtonVisualRole::ArithmeticOperator;
    return KeypadButtonVisualRole::Normal;
}

bool isArithmeticOperatorText(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.size() != 1)
        return false;

    const QChar ch = trimmed.at(0);
    return MathDsl::isAdditionOperator(ch)
        || MathDsl::isAdditionOperatorAlias(ch)
        || MathDsl::isSubtractionOperator(ch)
        || MathDsl::isSubtractionOperatorAlias(ch)
        || MathDsl::isMultiplicationOperator(ch)
        || MathDsl::isMultiplicationOperatorAlias(ch)
        || MathDsl::isDivisionOperator(ch)
        || MathDsl::isDivisionOperatorAlias(ch);
}

KeypadButtonVisualRole visualRoleForCustomButton(
    const Keypad::CustomButtonDescription& button)
{
    if (button.action == Settings::CustomKeypadActionEvaluateExpression)
        return KeypadButtonVisualRole::Evaluate;
    if (button.action == Settings::CustomKeypadActionInsertText
            && isArithmeticOperatorText(button.text)) {
        return KeypadButtonVisualRole::ArithmeticOperator;
    }
    return KeypadButtonVisualRole::Normal;
}

QColor oklchWithLightnessOffset(const QColor& color, double offset)
{
    if (!color.isValid())
        return color;

    Oklch oklch = qColorToOklch(color);
    oklch.l = qBound(0.0, oklch.l + offset, 1.0);
    const QColor adjusted = oklchToValidSrgbQColor(oklch);
    return adjusted.isValid() ? adjusted : color;
}

QColor keypadGradientTopColor(const QColor& background)
{
    return oklchWithLightnessOffset(background, UiConfig::KeypadButtonGradientLightnessDelta);
}

QColor keypadGradientBottomColor(const QColor& background)
{
    return oklchWithLightnessOffset(background, -UiConfig::KeypadButtonGradientLightnessDelta);
}

QColor keypadPrimaryStateBackground(const QColor& primary,
                                    const QColor& stateBackground,
                                    const QColor& normalBackground)
{
    if (!primary.isValid() || !stateBackground.isValid() || !normalBackground.isValid())
        return primary.isValid() ? primary : stateBackground;

    Oklch accent = qColorToOklch(primary);
    const Oklch state = qColorToOklch(stateBackground);
    const Oklch normal = qColorToOklch(normalBackground);
    const double offset = state.l - normal.l;
    if (qAbs(offset) < 1e-9)
        return primary;

    accent.l = qBound(0.0, accent.l + offset, 1.0);
    const QColor color = oklchToValidSrgbQColor(accent);
    return color.isValid() ? color : primary;
}

QColor keypadPrimaryHueFillBackground(const QColor& primary,
                                      const QColor& stateBackground,
                                      const QColor& normalBackground)
{
    if (!primary.isValid() || !stateBackground.isValid() || !normalBackground.isValid())
        return stateBackground;

    const double primaryRatio =
        double(qBound(0, UiConfig::KeypadOperatorPrimaryHueChromaPercent, 100)) / 100.0;
    if (primaryRatio <= 0.0)
        return stateBackground;

    const QColor primaryStateBackground =
        keypadPrimaryStateBackground(primary, stateBackground, normalBackground);
    if (primaryRatio >= 1.0)
        return primaryStateBackground;

    const Oklch stateOklch = qColorToOklch(stateBackground);
    const Oklch primaryStateOklch = qColorToOklch(primaryStateBackground);

    // The percent is a blend toward the full primary state, not just a chroma
    // multiplier. That makes 100 match the evaluate button exactly while 50
    // still reads as a quieter primary-hue operator fill.
    Oklch fill {
        stateOklch.l + (primaryStateOklch.l - stateOklch.l) * primaryRatio,
        stateOklch.c + (primaryStateOklch.c - stateOklch.c) * primaryRatio,
        primaryStateOklch.h,
        stateOklch.alpha
    };
    const QColor color = oklchToValidSrgbQColor(fill);
    return color.isValid() ? color : stateBackground;
}

KeypadButtonColors keypadButtonColorsForRole(const KeypadButtonColors& normalColors,
                                             const QColor& primaryBackground,
                                             KeypadButtonVisualRole role)
{
    if (!primaryBackground.isValid() || role == KeypadButtonVisualRole::Normal)
        return normalColors;

    const auto withForegrounds = [](const QColor& background,
                                    const QColor& hoverBackground,
                                    const QColor& pressedBackground) {
        return KeypadButtonColors {
            background,
            aaForegroundForBackground(background),
            hoverBackground,
            aaForegroundForBackground(hoverBackground),
            pressedBackground,
            aaForegroundForBackground(pressedBackground)
        };
    };

    if (role == KeypadButtonVisualRole::Evaluate) {
        const QColor background = keypadPrimaryStateBackground(
            primaryBackground, normalColors.background, normalColors.background);
        const QColor hoverBackground = keypadPrimaryStateBackground(
            primaryBackground, normalColors.hoverBackground, normalColors.background);
        const QColor pressedBackground = keypadPrimaryStateBackground(
            primaryBackground, normalColors.pressedBackground, normalColors.background);
        return withForegrounds(background, hoverBackground, pressedBackground);
    }

    const QColor background =
        keypadPrimaryHueFillBackground(
            primaryBackground, normalColors.background, normalColors.background);
    const QColor hoverBackground =
        keypadPrimaryHueFillBackground(
            primaryBackground, normalColors.hoverBackground, normalColors.background);
    const QColor pressedBackground =
        keypadPrimaryHueFillBackground(
            primaryBackground, normalColors.pressedBackground, normalColors.background);
    return withForegrounds(background, hoverBackground, pressedBackground);
}

QPalette keypadButtonPalette(const QPalette& source,
                             const QColor& background,
                             const QColor& foreground)
{
    QPalette palette = source;
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        palette.setColor(group, QPalette::Button, background);
        palette.setColor(group, QPalette::ButtonText, foreground);
        palette.setColor(group, QPalette::Window, background);
        palette.setColor(group, QPalette::WindowText, foreground);
    }
    return palette;
}

QString keypadButtonStyleSheet(const QPalette& palette,
                               const QColor& background,
                               const QColor& foreground,
                               const QColor& hoverBackground,
                               const QColor& hoverForeground,
                               const QColor& pressedBackground,
                               const QColor& pressedForeground);

void applyKeypadButtonStyle(QPushButton* button,
                            const QPalette& sourcePalette,
                            const KeypadButtonColors& colors)
{
    if (button == nullptr)
        return;

    // Qt repolishes a widget when its stylesheet changes, and that can reset
    // palette roles exposed through QPalette. Apply the stylesheet first, then
    // restore the semantic button/text colors so runtime theme changes keep the
    // same inspectable palette roles as startup theme application.
    button->setStyleSheet(keypadButtonStyleSheet(sourcePalette,
                                                 colors.background,
                                                 colors.foreground,
                                                 colors.hoverBackground,
                                                 colors.hoverForeground,
                                                 colors.pressedBackground,
                                                 colors.pressedForeground));
    button->setPalette(keypadButtonPalette(sourcePalette,
                                           colors.background,
                                           colors.foreground));
}

QString keypadButtonBackgroundStyle(const QColor& background)
{
    return QString::fromLatin1(
        " background-color: %1;"
        " background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
        " stop: 0 %2, stop: 1 %3);")
        .arg(background.name(),
             keypadGradientTopColor(background).name(),
             keypadGradientBottomColor(background).name());
}

QString keypadButtonStyleSheet(const QPalette& palette,
                               const QColor& background,
                               const QColor& foreground,
                               const QColor& hoverBackground,
                               const QColor& hoverForeground,
                               const QColor& pressedBackground,
                               const QColor& pressedForeground);
QString keypadButtonStyleSheet(const QPalette& palette)
{
    return keypadButtonStyleSheet(palette,
                                  palette.color(QPalette::Button),
                                  palette.color(QPalette::ButtonText),
                                  palette.color(QPalette::Highlight),
                                  palette.color(QPalette::HighlightedText),
                                  palette.color(QPalette::Mid),
                                  palette.color(QPalette::ButtonText));
}

QString keypadButtonStyleSheet(const QPalette& palette,
                               const QColor& background,
                               const QColor& foreground,
                               const QColor& hoverBackground,
                               const QColor& hoverForeground,
                               const QColor& pressedBackground,
                               const QColor& pressedForeground)
{
    Q_UNUSED(palette);
    const QString margin = QString::number(UiConfig::KeypadButtonMargin);
    const QString cornerRadius = QString::number(UiConfig::KeypadButtonCornerRadius);
    const QString padding = QString::number(UiConfig::KeypadButtonPadding);
    return QString::fromLatin1(
        "QPushButton {"
        " border: none;"
        " border-radius: %2px;"
        " margin: %1px;"
        " padding: %3px;"
        "%4"
        " color: %5;"
        "}"
        "QPushButton:hover {"
        " border: none;"
        "%6"
        " color: %7;"
        "}"
        "QPushButton:pressed {"
        " border: none;"
        "%8"
        " color: %9;"
        "}")
        .arg(margin,
             cornerRadius,
             padding,
             keypadButtonBackgroundStyle(background),
             foreground.name(),
             keypadButtonBackgroundStyle(hoverBackground),
             hoverForeground.name(),
             keypadButtonBackgroundStyle(pressedBackground),
             pressedForeground.name());
}
} // namespace

Keypad::Keypad(LayoutMode layoutMode, QWidget* parent, int scalePercent)
    : QWidget(parent)
    , m_layoutMode(layoutMode)
    , m_isCustom(false)
    , m_scalePercent(scalePercent)
{
    createButtons();
    sizeButtons();
    layoutButtons();
    setButtonTooltips();
    disableButtonFocus();
    setLayoutDirection(Qt::LeftToRight);
}

Keypad::Keypad(const QList<CustomButtonDescription>& customButtons, QWidget* parent, int scalePercent)
    : QWidget(parent)
    , m_layoutMode(LayoutModeScientificWide)
    , m_isCustom(true)
    , m_scalePercent(scalePercent)
    , m_customButtons(customButtons)
{
    createCustomButtons();
    sizeCustomButtons();
    layoutCustomButtons();
    disableButtonFocus();
    setLayoutDirection(Qt::LeftToRight);
}

QList<Keypad::CustomButtonDescription> Keypad::presetCustomButtons(LayoutMode layoutMode,
                                                                   QChar radixCharacter,
                                                                   int* rows,
                                                                   int* columns)
{
    QList<CustomButtonDescription> buttons;
    const LayoutEntry* entries = nullptr;
    int count = 0;
    layoutEntries(layoutMode, &entries, &count);
    if (!entries || count <= 0) {
        if (rows)
            *rows = 1;
        if (columns)
            *columns = 1;
        return buttons;
    }

    int maxRow = 0;
    int maxColumn = 0;
    for (int i = 0; i < count; ++i) {
        maxRow = qMax(maxRow, entries[i].row);
        maxColumn = qMax(maxColumn, entries[i].column);

        const QString label = customButtonLabel(entries[i].button, radixCharacter);
        if (label.isEmpty())
            continue;

        CustomButtonDescription button;
        button.row = entries[i].row;
        button.column = entries[i].column;
        button.label = label;
        button.action = customButtonAction(entries[i].button);
        button.text = (button.action == Settings::CustomKeypadActionInsertText)
            ? customButtonInsertText(entries[i].button, radixCharacter)
            : QString();
        buttons.append(button);
    }

    if (rows)
        *rows = maxRow + 1;
    if (columns)
        *columns = maxColumn + 1;
    return buttons;
}

void Keypad::handleRadixCharacterChange()
{
    if (m_isCustom)
        return;
    key(KeyRadixChar)->setText(QString(QChar(Settings::instance()->radixCharacter())));
}

void Keypad::retranslateText()
{
    if (m_isCustom)
        return;
    setButtonTooltips();
    handleRadixCharacterChange();
}

QPushButton* Keypad::key(Button button) const
{
    Q_ASSERT(keys.contains(button));
    return keys.value(button).first;
}

void Keypad::createButtons()
{
    keys.clear();

    QFont boldFont;
    boldFont.setBold(true);
    QFont emphasizedBoldFont = boldFont;
    emphasizedBoldFont.setPointSize(emphasizedBoldFont.pointSize() + 3);
    boldFont = scaledFont(boldFont, m_scalePercent);
    emphasizedBoldFont = scaledFont(emphasizedBoldFont, m_scalePercent);

    static const int keyDescriptionsCount = int(sizeof keyDescriptions / sizeof keyDescriptions[0]);
    for (int i = 0; i < keyDescriptionsCount; ++i) {
        const KeyDescription* description = keyDescriptions + i;
        QPushButton* key = new QPushButton(description->label, this);
        key->setCursor(Qt::PointingHandCursor);
        key->setStyleSheet(keypadButtonStyleSheet(palette()));
        key->setFont(description->boldFont ? emphasizedBoldFont : boldFont);
        const QPair<QPushButton*, const KeyDescription*> hashValue(key, description);
        keys.insert(description->button, hashValue);

        QObject::connect(key, &QPushButton::clicked, this, [this, description]() {
            emit buttonPressed(description->button);
        });
    }

    handleRadixCharacterChange();
}

void Keypad::disableButtonFocus()
{
    QHashIterator<Button, QPair<QPushButton*, const KeyDescription*> > i(keys);
    while (i.hasNext()) {
        i.next();
        i.value().first->setFocusPolicy(Qt::NoFocus);
    }
    for (auto* button : m_customWidgets)
        button->setFocusPolicy(Qt::NoFocus);
}

void Keypad::layoutButtons()
{
    if (m_isCustom)
        return;

    int layoutSpacing = 0;

#if QT_VERSION >= 0x040400 && defined(Q_WS_MAC) && !defined(QT_NO_STYLE_MAC)
    // Workaround for a layouting bug in QMacStyle, Qt 4.4.0. Buttons would overlap.
    if (qobject_cast<QMacStyle *>(p->style()))
        layoutSpacing = -1;
#endif

    QGridLayout* layout = new QGridLayout(this);
    layout->setContentsMargins(UiConfig::KeypadButtonMargin,
                               UiConfig::KeypadButtonMargin,
                               UiConfig::KeypadButtonMargin,
                               UiConfig::KeypadButtonMargin);
    layout->setSpacing(layoutSpacing);

    // Hide everything first; only buttons added to the active layout are shown.
    QHashIterator<Button, QPair<QPushButton*, const KeyDescription*> > hideIter(keys);
    while (hideIter.hasNext()) {
        hideIter.next();
        hideIter.value().first->hide();
    }

    const QHash<Button, QPoint> positions = createLayoutMap(m_layoutMode);
    QHashIterator<Button, QPair<QPushButton*, const KeyDescription*> > i(keys);
    while (i.hasNext()) {
        i.next();
        if (!positions.contains(i.key()))
            continue;

        QWidget* widget = i.value().first;
        const QPoint pos = positions.value(i.key());
        layout->addWidget(widget, pos.y(), pos.x());
        widget->show();
    }
}

void Keypad::createCustomButtons()
{
    QFont boldFont;
    boldFont.setBold(true);
    boldFont = scaledFont(boldFont, m_scalePercent);

    for (const auto& description : m_customButtons) {
        QPushButton* key = new QPushButton(description.label, this);
        key->setCursor(Qt::PointingHandCursor);
        key->setFont(boldFont);
        key->setStyleSheet(keypadButtonStyleSheet(palette()));
        QObject::connect(key, &QPushButton::clicked, this, [this, description]() {
            emit customButtonPressed(description.action, description.text);
        });
        m_customWidgets.append(key);
    }
}

void Keypad::layoutCustomButtons()
{
    if (!m_isCustom)
        return;

    int layoutSpacing = 0;
    QGridLayout* layout = new QGridLayout(this);
    layout->setContentsMargins(UiConfig::KeypadButtonMargin,
                               UiConfig::KeypadButtonMargin,
                               UiConfig::KeypadButtonMargin,
                               UiConfig::KeypadButtonMargin);
    layout->setSpacing(layoutSpacing);

    const int count = qMin(m_customButtons.size(), m_customWidgets.size());
    for (int i = 0; i < count; ++i)
        layout->addWidget(m_customWidgets.at(i), m_customButtons.at(i).row, m_customButtons.at(i).column);
}

void Keypad::updateButtonStyleSheets()
{
    const bool hasThemeButtonColors = m_buttonBackground.isValid();
    const KeypadButtonColors normalColors {
        m_buttonBackground,
        m_buttonForeground,
        m_buttonHoverBackground,
        m_buttonHoverForeground,
        m_buttonPressedBackground,
        m_buttonPressedForeground
    };
    const QString fallbackStyleSheet = keypadButtonStyleSheet(palette());
    QHashIterator<Button, QPair<QPushButton*, const KeyDescription*> > i(keys);
    while (i.hasNext()) {
        i.next();
        QPushButton* button = i.value().first;
        if (hasThemeButtonColors) {
            const KeypadButtonColors colors = keypadButtonColorsForRole(
                normalColors, m_primaryBackground, visualRoleForButton(i.key()));
            applyKeypadButtonStyle(button, palette(), colors);
        } else {
            button->setStyleSheet(fallbackStyleSheet);
        }
    }
    for (int index = 0; index < m_customWidgets.size(); ++index) {
        QPushButton* button = m_customWidgets.at(index);
        if (hasThemeButtonColors) {
            const KeypadButtonVisualRole role = index < m_customButtons.size()
                ? visualRoleForCustomButton(m_customButtons.at(index))
                : KeypadButtonVisualRole::Normal;
            const KeypadButtonColors colors = keypadButtonColorsForRole(
                normalColors, m_primaryBackground, role);
            applyKeypadButtonStyle(button, palette(), colors);
        } else {
            button->setStyleSheet(fallbackStyleSheet);
        }
    }
}

void Keypad::setThemeButtonColors(const QColor& background,
                                  const QColor& foreground,
                                  const QColor& hoverBackground,
                                  const QColor& hoverForeground,
                                  const QColor& pressedBackground,
                                  const QColor& pressedForeground,
                                  const QColor& primaryBackground)
{
    m_buttonBackground = background;
    m_buttonForeground = foreground;
    m_buttonHoverBackground = hoverBackground;
    m_buttonHoverForeground = hoverForeground;
    m_buttonPressedBackground = pressedBackground;
    m_buttonPressedForeground = pressedForeground;
    m_primaryBackground = primaryBackground;
    updateButtonStyleSheets();

    // Runtime theme changes can still have queued Qt palette/style polish work
    // from parent widgets after this call returns. Reapply once at the end of
    // the event turn so keyed buttons keep their role-specific fills instead
    // of being flattened back to the inherited QPushButton palette.
    QTimer::singleShot(0, this, [this]() {
        updateButtonStyleSheets();
    });
}

void Keypad::setButtonTooltips()
{
    if (m_isCustom)
        return;

    key(KeyAcos)->setToolTip(Keypad::tr("Inverse cosine"));
    key(KeyAns)->setToolTip(Keypad::tr("The last result"));
    key(KeyAsin)->setToolTip(Keypad::tr("Inverse sine"));
    key(KeyAtan)->setToolTip(Keypad::tr("Inverse tangent"));
    key(KeyEquals)->setToolTip(Keypad::tr("Evaluate expression"));
    key(KeyDivide)->setToolTip(Keypad::tr("Division"));
    key(KeyTimes)->setToolTip(Keypad::tr("Multiplication"));
    key(KeyMinus)->setToolTip(Keypad::tr("Subtraction"));
    key(KeyPlus)->setToolTip(Keypad::tr("Addition"));
    key(KeyClear)->setToolTip(Keypad::tr("Clear expression"));
    key(KeyCos)->setToolTip(Keypad::tr("Cosine"));
    key(KeyBackspace)->setToolTip(Keypad::tr("Backspace"));
    key(KeyEE)->setToolTip(Keypad::tr("Scientific notation"));
    key(KeyExp)->setToolTip(Keypad::tr("Exponential"));
    key(KeyFactorial)->setToolTip(Keypad::tr("Factorial"));
    key(KeyLn)->setToolTip(Keypad::tr("Natural logarithm"));
    key(KeyLeftPar)->setToolTip(Keypad::tr("Left parenthesis"));
    key(KeyCbrt)->setToolTip(Keypad::tr("Cube root"));
    key(KeyLg)->setToolTip(Keypad::tr("Common logarithm"));
    key(KeyMod)->setToolTip(Keypad::tr("Modulo"));
    key(KeyPercent)->setToolTip(Keypad::tr("Contextual percentage"));
    key(KeyRaise)->setToolTip(Keypad::tr("Power"));
    key(KeyRightPar)->setToolTip(Keypad::tr("Right parenthesis"));
    key(KeySin)->setToolTip(Keypad::tr("Sine"));
    key(KeySqrt)->setToolTip(Keypad::tr("Square root"));
    key(KeyTan)->setToolTip(Keypad::tr("Tangent"));
    key(KeyPi)->setToolTip(Keypad::tr("Pi"));
    key(KeyRadixChar)->setToolTip(Keypad::tr("Decimal separator"));
    key(KeyXEquals)->setToolTip(Keypad::tr("Assign variable x"));
    key(KeyX)->setToolTip(Keypad::tr("The variable x"));
}

void Keypad::sizeButtons()
{
    if (m_isCustom)
        return;

    // The same font in all buttons, so just pick one.
    QFontMetrics fm = key(Key0)->fontMetrics();

    int maxWidth = fm.horizontalAdvance(key(KeyAcos)->text());
    const int textHeight = qMax(fm.lineSpacing(), 14);

    QStyle::ContentsType type = QStyle::CT_ToolButton;
    QStyleOptionButton option;
    const QWidget* exampleWidget = key(KeyAcos);
    option.initFrom(exampleWidget);
    QSize minSize = QSize(maxWidth, textHeight);
    QSize size = exampleWidget->style()->sizeFromContents(type, &option, minSize, exampleWidget);


#ifdef Q_WS_X11
    // We would like to use the button size as indicated by the widget style, but in some cases,
    // e.g. KDE's Plastik or Oxygen, another few pixels (typically 5) are added as the content
    // margin, thereby making the button incredibly wider than necessary. Workaround: take only
    // the hinted height, adjust the width ourselves (with our own margin).
    maxWidth += 6;
    int hh = size.height();
    size = QSize(qMax(hh, maxWidth), hh);
#endif

    const int side = qMax(size.width(), size.height());
    size = QSize(side, side);

    // limit the size of the buttons
    QHashIterator<Button, QPair<QPushButton*, const KeyDescription*> > i(keys);
    while (i.hasNext()) {
        i.next();
        i.value().first->setFixedSize(size);
    }
}

void Keypad::sizeCustomButtons()
{
    if (!m_isCustom || m_customWidgets.isEmpty())
        return;

    // Match preset keypad sizing baseline exactly (bold, larger numeric key font).
    QFont boldFont = m_customWidgets.first()->font();
    boldFont.setBold(true);
    boldFont.setPointSize(boldFont.pointSize() + 3);
    boldFont = scaledFont(boldFont, m_scalePercent);
    const QFontMetrics fm(boldFont);
    const int maxWidth = fm.horizontalAdvance(QStringLiteral("arccos"));
    const int textHeight = qMax(fm.lineSpacing(), 14);

    QStyle::ContentsType type = QStyle::CT_ToolButton;
    QStyleOptionButton option;
    const QWidget* exampleWidget = m_customWidgets.first();
    option.initFrom(exampleWidget);
    QSize minSize = QSize(maxWidth, textHeight);
    QSize size = exampleWidget->style()->sizeFromContents(type, &option, minSize, exampleWidget);

#ifdef Q_WS_X11
    maxWidth += 6;
    int hh = size.height();
    size = QSize(qMax(hh, maxWidth), hh);
#endif

    const int side = qMax(size.width(), size.height());
    size = QSize(side, side);

    for (auto* button : m_customWidgets)
        button->setFixedSize(size);
}

void Keypad::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateText();
    const bool paletteChanged = event->type() == QEvent::PaletteChange
        || event->type() == QEvent::ApplicationPaletteChange;
    QWidget::changeEvent(event);
    if (paletteChanged)
        updateButtonStyleSheets();
}
