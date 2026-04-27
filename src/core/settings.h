// This file is part of the SpeedCrunch project
// Copyright (C) 2004 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2005-2006 Johan Thelin <e8johan@gmail.com>
// Copyright (C) 2007-2016 @heldercorreia
// Copyright (C) 2015 Pol Welter <polwelter@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; see the file COPYING.  If not, write to
// the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301, USA.

#ifndef CORE_SETTINGS_H
#define CORE_SETTINGS_H

#include <QtCore/QPoint>
#include <QtCore/QSize>
#include <QtCore/QStringList>
#include <QtCore/QList>

#include "math/floatconvert.h"

class Settings {
public:
    enum UpDownArrowBehavior {
        UpDownArrowBehaviorNever = 0,
        UpDownArrowBehaviorAlways = 1,
        UpDownArrowBehaviorSingleLineOnly = 2
    };

    enum HistorySaving {
        HistorySavingNever = 0,
        HistorySavingOnExit = 1,
        HistorySavingContinuously = 2
    };

    enum KeypadMode {
        KeypadModeDisabled = 0,
        KeypadModeBasicWide = 1,
        KeypadModeBasicNarrow = 2,
        KeypadModeScientificWide = 3,
        KeypadModeScientificNarrow = 4,
        KeypadModeCustom = 5
    };

    enum CustomKeypadButtonAction {
        CustomKeypadActionInsertText = 0,
        CustomKeypadActionBackspace = 1,
        CustomKeypadActionClearExpression = 2,
        CustomKeypadActionEvaluateExpression = 3
    };

    enum NumberFormatStyle {
        NumberFormatSystem = 0, // Legacy value kept for migration compatibility.
        NumberFormatNoGroupingDot = 1,
        NumberFormatNoGroupingComma = 2,
        NumberFormatSIDot = 3,
        NumberFormatSIComma = 4,
        NumberFormatThreeDigitCommaDot = 5,
        NumberFormatThreeDigitCommaDotFraction = 6,
        NumberFormatThreeDigitDotComma = 7,
        NumberFormatThreeDigitDotCommaFraction = 8,
        NumberFormatThreeDigitSpaceDot = 9,
        NumberFormatThreeDigitSpaceComma = 10,
        NumberFormatThreeDigitUnderscoreDot = 11,
        NumberFormatThreeDigitUnderscoreDotFraction = 12,
        NumberFormatThreeDigitUnderscoreComma = 13,
        NumberFormatThreeDigitUnderscoreCommaFraction = 14,
        NumberFormatIndianCommaDot = 15
    };

    enum UnitNegativeExponentStyle {
        UnitNegativeExponentSuperscript = 0,
        UnitNegativeExponentFraction = 1
    };

    enum ResultRoundingMode {
        ResultRoundingHalfAwayFromZero = 0,
        ResultRoundingHalfEven = 1,
        ResultRoundingTowardZero = 2,
        ResultRoundingTowardPositiveInfinity = 3,
        ResultRoundingTowardNegativeInfinity = 4
    };

    struct CustomKeypadButton {
        int row;
        int column;
        QString label;
        QString text;
        CustomKeypadButtonAction action;
    };

    struct CustomKeypad {
        int rows;
        int columns;
        QList<CustomKeypadButton> buttons;
    };

    static Settings* instance();
    static QString getConfigPath();
    static QString getDataPath();
    static QString getCachePath();
    static CustomKeypad defaultCustomKeypad();

    void load();
    void save();

    char radixCharacter() const; // 0 or '*': Automatic.
    void setRadixCharacter(char c = 0);
    bool isRadixCharacterAuto() const;
    bool isRadixCharacterBoth() const;
    bool isDigitGroupingEnabled() const;
    bool isIndianDigitGrouping() const;
    QString digitGroupingSeparator() const;
    char decimalSeparator() const;
    void applyNumberFormatStyle();

    bool complexNumbers;
    char imaginaryUnit; // 'i' or 'j'.

    char angleUnit; // 'r': radian; 'd': degree; 'g': gradian; 't': turn; 'v': revolution.

    // 'g': general; 'f': fixed; 'n': engineering; 'e': scientific; 'r': rational;
    // 'b': binary; 'o': octal; 'h': hexadecimal; 's': sexagesimal.
    char resultFormat; // Main notation.
    char alternativeResultFormat; // Secondary notation; '\0': disabled.
    char tertiaryResultFormat; // Tertiary notation; '\0': disabled.
    char quaternaryResultFormat; // Extra line #3 notation; '\0': disabled.
    char quinaryResultFormat; // Extra line #4 notation; '\0': disabled.
    bool secondaryResultEnabled;
    bool tertiaryResultEnabled;
    bool quaternaryResultEnabled;
    bool quinaryResultEnabled;
    bool multipleResultLinesEnabled; // UI toggle: show/use extra result lines.
    int resultPrecision; // Main precision. See HMath documentation.
    ResultRoundingMode resultRoundingMode;
    UnitNegativeExponentStyle unitNegativeExponentStyle;
    int secondaryResultPrecision; // Secondary precision.
    int tertiaryResultPrecision; // Tertiary precision.
    int quaternaryResultPrecision; // Extra line #3 precision.
    int quinaryResultPrecision; // Extra line #4 precision.
    char resultFormatComplex; // Main complex form: 'c' cartesian; 'p' polar exponential; 'a' polar angle.
    bool secondaryComplexNumbers;
    char secondaryResultFormatComplex; // Secondary complex form.
    bool tertiaryComplexNumbers;
    char tertiaryResultFormatComplex; // Tertiary complex form.
    bool quaternaryComplexNumbers;
    char quaternaryResultFormatComplex; // Extra line #3 complex form.
    bool quinaryComplexNumbers;
    char quinaryResultFormatComplex; // Extra line #4 complex form.

    bool autoAns;
    bool autoCalc;
    bool autoCompletion;
    bool autoCompletionBuiltInFunctions;
    bool autoCompletionBuiltInVariables;
    bool autoCompletionLongFormUnits;
    bool autoCompletionUserFunctions;
    bool autoCompletionUserVariables;
    UpDownArrowBehavior upDownArrowBehavior;
    int digitGrouping;
    bool digitGroupingIntegerPartOnly;
    NumberFormatStyle numberFormatStyle;
    bool hasNumberFormatStyleSetting;
    int maxHistoryEntries; // 0: unlimited.
    HistorySaving historySaving;
    bool leaveLastExpression;
    bool showEmptyHistoryHint;
    bool syntaxHighlighting;
    bool hoverHighlightResults;
    bool windowAlwaysOnTop;
    bool autoResultToClipboard;
    bool simplifyResultExpressions;
    bool windowPositionSave;
    bool singleInstance;
    QString startupUserDefinitions;
    bool startupUserDefinitionsOverwrite;
    bool startupUserDefinitionsApplyBeforeRestore;

    bool constantsDockVisible;
    bool functionsDockVisible;
    bool historyDockVisible;
    bool keypadVisible;
    KeypadMode keypadMode;
    int keypadZoomPercent;
    CustomKeypad customKeypad;
    bool formulaBookDockVisible;
    bool statusBarVisible;
    bool menuBarVisible;
    bool variablesDockVisible;
    bool userFunctionsDockVisible;
    bool userUnitsDockVisible;
    bool windowOnfullScreen;
    bool bitfieldVisible;

    QString colorScheme;
    QString displayFont;

    QString language;

    QByteArray windowState;
    QByteArray windowGeometry;
    QByteArray manualWindowGeometry;

private:
    Settings();
    Q_DISABLE_COPY(Settings)
};

inline Settings::UnitNegativeExponentStyle g_runtimeUnitNegativeExponentStyle =
    Settings::UnitNegativeExponentSuperscript;
inline Settings::ResultRoundingMode g_runtimeResultRoundingMode =
    Settings::ResultRoundingHalfAwayFromZero;

inline void setRuntimeUnitNegativeExponentStyle(Settings::UnitNegativeExponentStyle style)
{
    if (style != Settings::UnitNegativeExponentSuperscript
        && style != Settings::UnitNegativeExponentFraction) {
        return;
    }
    g_runtimeUnitNegativeExponentStyle = style;
}

inline Settings::UnitNegativeExponentStyle runtimeUnitNegativeExponentStyle()
{
    return g_runtimeUnitNegativeExponentStyle;
}

inline void setRuntimeResultRoundingMode(Settings::ResultRoundingMode mode)
{
    if (mode != Settings::ResultRoundingHalfAwayFromZero
        && mode != Settings::ResultRoundingHalfEven
        && mode != Settings::ResultRoundingTowardZero
        && mode != Settings::ResultRoundingTowardPositiveInfinity
        && mode != Settings::ResultRoundingTowardNegativeInfinity) {
        return;
    }

    g_runtimeResultRoundingMode = mode;

    io_rounding_mode ioMode = IO_ROUND_HALF_AWAY_FROM_ZERO;
    if (mode == Settings::ResultRoundingHalfEven)
        ioMode = IO_ROUND_HALF_EVEN;
    else if (mode == Settings::ResultRoundingTowardZero)
        ioMode = IO_ROUND_TOWARD_ZERO;
    else if (mode == Settings::ResultRoundingTowardPositiveInfinity)
        ioMode = IO_ROUND_TOWARD_PLUS_INFINITY;
    else if (mode == Settings::ResultRoundingTowardNegativeInfinity)
        ioMode = IO_ROUND_TOWARD_MINUS_INFINITY;
    float_set_output_rounding_mode(ioMode);
}

inline Settings::ResultRoundingMode runtimeResultRoundingMode()
{
    return g_runtimeResultRoundingMode;
}

#endif
