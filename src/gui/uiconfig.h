// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_UICONFIG_H
#define GUI_UICONFIG_H

namespace UiConfig {

constexpr int SessionPaneSplitterWidth = 1;
constexpr int DockSplitterStrokeWidth = 2;
constexpr int OutlineStrokeWidth = 1;
constexpr int ActiveSessionTabIndicatorStrokeWidth = 2;
constexpr int KeypadButtonMargin = 3;
constexpr int KeypadButtonPadding = 3;
constexpr int KeypadButtonCornerRadius = 8;
// 0 keeps arithmetic operators on the normal button surface; 100 gives them
// the same primary fill as the evaluate button. Intermediate values blend in
// OKLCH while using the primary hue.
constexpr int KeypadOperatorPrimaryHueChromaPercent = 50;
constexpr double KeypadButtonGradientLightnessDelta = 0.035;

// Enables the temporary OKLCH HTML diagnostics report in the system temp
// directory. Keep disabled for normal builds; it is only useful while tuning
// generated theme surfaces.
constexpr bool OklchThemeDebugReportEnabled = false;

// Theme surfaces are generated as six OKLCH shades from the result-display
// background. Keep visual-role-to-shade choices here so small UI tuning does
// not require hunting through individual widgets.
constexpr int Shade100 = 0;
constexpr int Shade200 = 1;
constexpr int Shade300 = 2;
constexpr int Shade400 = 3;
constexpr int Shade500 = 4;
constexpr int Shade600 = 5;

constexpr int WindowBackgroundShade = Shade100;
constexpr int ResultDisplayShade = Shade200;
constexpr int SplitterShade = Shade300;
constexpr bool SplitterHoverUsesPrimary = true;
constexpr int SplitterHoverShade = Shade400;
constexpr int ScrollToBottomButtonBackgroundShade = Shade300;
constexpr int ScrollToBottomButtonHoverBackgroundShade = Shade400;
constexpr int ScrollToBottomButtonOutlineShade = Shade400;
constexpr int ResultTooltipBackgroundShade = Shade400;
constexpr int ResultTooltipOutlineShade = Shade500;
constexpr int ResultTooltipCornerRadius = 8;
constexpr int ResultTooltipStartMargin = 14;
constexpr int CompletionPopupBackgroundShade = Shade400;
constexpr int CompletionPopupScrollbarThumbShade = Shade500;
constexpr int CompletionPopupSelectedRowShade = Shade500;
constexpr int CompletionPopupOutlineShade = Shade600;
constexpr int CompletionPopupCornerRadius = 8;
constexpr int ResultDisplayScrollbarShade = Shade300;
constexpr int ResultDisplayScrollbarHoverShade = Shade400;
constexpr int ResultDisplayScrollbarPressedShade = Shade500;
constexpr int DockBackgroundShade = Shade300;
constexpr int DockHeaderShade = Shade400;
constexpr int ConstantsDockMinimumWidth = 120;
constexpr int ConstantsDockDefaultWidth = 320;
constexpr int DockTextInputShade = Shade300;
constexpr int DockTextInputOutlineShade = Shade400;
constexpr int DockHoveredItemShade = Shade400;
constexpr int DockUnfocusedSelectedItemShade = Shade500;
constexpr int KeypadBackgroundShade = Shade200;
constexpr int KeypadButtonShade = Shade300;
constexpr int KeypadButtonHoverShade = Shade400;
constexpr int KeypadButtonPressedShade = Shade500;
constexpr int StatusBarBackgroundShade = Shade100;
constexpr int BitfieldBitHoverShade = Shade400;

} // namespace UiConfig

#endif // GUI_UICONFIG_H
