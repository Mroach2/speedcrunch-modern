// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_UICONFIG_H
#define GUI_UICONFIG_H

namespace UiConfig {

constexpr int SessionPaneSplitterWidth = 1;
constexpr int DockSplitterStrokeWidth = 2;
constexpr int OutlineStrokeWidth = 2;
constexpr int PopupOutlineStrokeWidth = 1;
constexpr int ActiveSessionTabIndicatorStrokeWidth = 2;
constexpr int KeypadButtonMargin = 3;
constexpr int KeypadButtonPadding = 3;
constexpr int KeypadButtonCornerRadius = 8;
// 0 keeps keyed buttons on the normal button surface; 100 gives them the full
// primary fill. Intermediate values blend in OKLCH while using the primary hue.
constexpr int KeypadDigitPrimaryHueChromaPercent = 25;
constexpr int KeypadOperatorPrimaryHueChromaPercent = 50;
constexpr int KeypadEvaluatePrimaryHueChromaPercent = 90;
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
constexpr int SelectedSessionTabFillShade = Shade400;
constexpr int HoveredSessionTabFillShade = Shade300;
constexpr int SessionTabCloseButtonHoverFillShade = Shade600;
constexpr int DockBackgroundShade = Shade300;
constexpr int DockHeaderShade = Shade400;
constexpr int DockHeaderButtonFillShade = Shade200;
constexpr int DockHeaderButtonHoverFillShade = Shade300;
constexpr int ConstantsDockMinimumWidth = 120;
constexpr int ConstantsDockDefaultWidth = 320;
constexpr int DockTextInputShade = Shade300;
constexpr int DockTextInputOutlineShade = Shade400;
constexpr int DockTextInputUnfocusedOutlineStrokeWidth = 1;
constexpr int DockHoveredItemShade = Shade500;
constexpr int DockHoveredItemCornerRadius = 6;
constexpr int DockUnfocusedSelectedItemShade = Shade500;
constexpr int KeypadBackgroundShade = Shade200;
constexpr int KeypadButtonShade = Shade300;
constexpr int KeypadButtonHoverShade = Shade400;
constexpr int KeypadButtonPressedShade = Shade500;
constexpr int StatusBarBackgroundShade = Shade100;
constexpr int BitfieldBitHoverShade = Shade400;
constexpr int BitfieldButtonFillShade = Shade400;
constexpr int BitfieldButtonHoverFillShade = Shade500;
constexpr int BitfieldButtonPressedFillShade = Shade600;

} // namespace UiConfig

#endif // GUI_UICONFIG_H
