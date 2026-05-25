// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/oklchutils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>

namespace {

// Pi is used only for OKLCH hue conversion between degrees and radians. Keeping
// it local avoids pulling in non-standard constants from <cmath>.
constexpr double kPi = 3.14159265358979323846;

// OKLCH hue wraps around a 360-degree circle. The normalization helper uses
// this value to preserve hue continuity after modulo arithmetic.
constexpr double kDegreesInCircle = 360.0;

// Minimum text contrast used by generated foreground helpers. This is named
// AA for the public behavior, but it is currently set to the stricter 7:1
// target so generated UI foregrounds stay safely above AA.
constexpr double kAaMinimumContrast = 7.0;

// Reserved stricter contrast threshold for future Settings-driven AAA behavior.
// It intentionally matches the current AA threshold while the UI uses 7:1 by
// default everywhere.
constexpr double kAaaMinimumContrast = 7.0;

// Enables generation of the temporary OKLCH HTML diagnostics report. Keeping
// this local prevents unrelated UI code from branching on implementation
// details; report writers return an empty path when it is disabled.
constexpr bool kOklchHtmlReportEnabled = true;

// Default percentage of the available lightness range used by shade generation.
// For dark themes, right-side shades move this fraction toward white; for light
// themes, right-side shades move this fraction toward black.
constexpr double kDefaultShadeDistanceFactor = 0.090;

// Default hue used when the background is too close to neutral to provide a
// stable hue. 250 degrees is a blue-violet accent: familiar for focus/selection
// affordances, but far enough from red/green/yellow semantic colors.
constexpr double kDefaultPrimaryFallbackHueDegrees = 250.0;

// Primary accents are generated from the background in OKLCH, because OKLCH
// separates perceptual lightness, chroma, and hue more predictably than
// HSL/HSV. In HSL/HSV, equal numeric lightness or saturation changes can look
// very different across hues; OKLCH lets the theme code target readable
// accents with deterministic lightness and chroma adjustments.
constexpr double kNeutralBackgroundChromaThreshold = 0.02;
constexpr double kPrimaryMinimumContrast = 3.0;
constexpr double kPrimaryPreferredContrast = 4.5;
constexpr double kDarkPrimaryLightness = 0.74;
constexpr double kLightPrimaryLightness = 0.52;
constexpr double kDarkPrimaryMaximumLightness = 0.86;
constexpr double kLightPrimaryMinimumLightness = 0.36;

// Chroma retention applied per shade step away from the base shade. This keeps
// generated palettes from becoming over-saturated as lightness moves toward the
// extremes while preserving hue as much as sRGB allows.
constexpr double kDefaultShadeChromaDecay = 0.95;

// The window shade sits on the opposite side of the base from the main generated
// ramp. It gets an explicit chroma reduction so it stays visually quieter than
// the result-display/base surface.
constexpr double kWindowShadeChromaMultiplier = 0.50;

// Tinted foregrounds are rejected if gamut fitting has reduced their chroma
// below this fraction of the original tint; otherwise a technically passing
// foreground can look nearly neutral.
constexpr double kMinimumChromaRetention = 0.25;

// Tints below this OKLCH chroma are treated as visually neutral and skipped, so
// foreground selection falls back to the black/white contrast path.
constexpr double kMinimumVisibleTintChroma = 0.015;

// Tiny tolerance for floating-point noise when checking whether linear sRGB
// values are inside gamut before binary-search chroma fitting.
constexpr double kGamutEpsilon = 1e-7;

// Number of binary-search rounds used for chroma fitting and tinted foreground
// lightness adjustment. Thirty-two rounds is deterministic and more precise
// than QColor's 8-bit output can represent.
constexpr int kBinarySearchIterations = 32;

// Bjorn Ottosson's OKLab matrix from linear sRGB to LMS cone responses.
constexpr double kLinearSrgbToLms[3][3] = {
    {0.4122214708, 0.5363325363, 0.0514459929},
    {0.2119034982, 0.6806995451, 0.1073969566},
    {0.0883024619, 0.2817188376, 0.6299787005}
};

// Bjorn Ottosson's OKLab matrix from cube-root LMS to OKLab coordinates.
constexpr double kLmsToOklab[3][3] = {
    {0.2104542553, 0.7936177850, -0.0040720468},
    {1.9779984951, -2.4285922050, 0.4505937099},
    {0.0259040371, 0.7827717662, -0.8086757660}
};

// Inverse OKLab matrix from OKLab coordinates back to cube-root LMS.
constexpr double kOklabToLms[3][3] = {
    {1.0, 0.3963377774, 0.2158037573},
    {1.0, -0.1055613458, -0.0638541728},
    {1.0, -0.0894841775, -1.2914855480}
};

// Final inverse OKLab matrix from cubed LMS responses back to linear sRGB.
constexpr double kLmsToLinearSrgb[3][3] = {
    {4.0767416621, -3.3077115913, 0.2309699292},
    {-1.2684380046, 2.6097574011, -0.3413193965},
    {-0.0041960863, -0.7034186147, 1.7076147010}
};

struct LinearSrgb
{
    double r;
    double g;
    double b;
};

struct Oklab
{
    double l;
    double a;
    double b;
    double alpha;
};

double clamp(double value, double minimum, double maximum)
{
    return std::max(minimum, std::min(maximum, value));
}

double normalizedHue(double hue)
{
    double result = std::fmod(hue, kDegreesInCircle);
    if (result < 0.0)
        result += kDegreesInCircle;
    return result;
}

Oklch sanitizedOklch(const Oklch& color)
{
    return {
        clamp(color.l, 0.0, 1.0),
        std::max(0.0, color.c),
        normalizedHue(color.h),
        clamp(color.alpha, 0.0, 1.0)
    };
}

double srgbToLinear(double channel)
{
    return channel <= 0.04045
        ? channel / 12.92
        : std::pow((channel + 0.055) / 1.055, 2.4);
}

double linearToSrgb(double channel)
{
    return channel <= 0.0031308
        ? 12.92 * channel
        : 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
}

Oklab oklchToOklab(const Oklch& color)
{
    const double radians = color.h * kPi / 180.0;
    return {
        color.l,
        color.c * std::cos(radians),
        color.c * std::sin(radians),
        color.alpha
    };
}

LinearSrgb oklabToLinearSrgb(const Oklab& color)
{
    const double lRoot = kOklabToLms[0][0] * color.l
        + kOklabToLms[0][1] * color.a + kOklabToLms[0][2] * color.b;
    const double mRoot = kOklabToLms[1][0] * color.l
        + kOklabToLms[1][1] * color.a + kOklabToLms[1][2] * color.b;
    const double sRoot = kOklabToLms[2][0] * color.l
        + kOklabToLms[2][1] * color.a + kOklabToLms[2][2] * color.b;

    const double l = lRoot * lRoot * lRoot;
    const double m = mRoot * mRoot * mRoot;
    const double s = sRoot * sRoot * sRoot;

    return {
        kLmsToLinearSrgb[0][0] * l + kLmsToLinearSrgb[0][1] * m
            + kLmsToLinearSrgb[0][2] * s,
        kLmsToLinearSrgb[1][0] * l + kLmsToLinearSrgb[1][1] * m
            + kLmsToLinearSrgb[1][2] * s,
        kLmsToLinearSrgb[2][0] * l + kLmsToLinearSrgb[2][1] * m
            + kLmsToLinearSrgb[2][2] * s
    };
}

LinearSrgb oklchToLinearSrgb(const Oklch& color)
{
    return oklabToLinearSrgb(oklchToOklab(color));
}

bool isLinearSrgbInGamut(const LinearSrgb& color)
{
    return color.r >= -kGamutEpsilon && color.r <= 1.0 + kGamutEpsilon
        && color.g >= -kGamutEpsilon && color.g <= 1.0 + kGamutEpsilon
        && color.b >= -kGamutEpsilon && color.b <= 1.0 + kGamutEpsilon;
}

QColor colorFromLinearSrgb(const LinearSrgb& color, double alpha)
{
    return QColor::fromRgbF(
        clamp(linearToSrgb(clamp(color.r, 0.0, 1.0)), 0.0, 1.0),
        clamp(linearToSrgb(clamp(color.g, 0.0, 1.0)), 0.0, 1.0),
        clamp(linearToSrgb(clamp(color.b, 0.0, 1.0)), 0.0, 1.0),
        clamp(alpha, 0.0, 1.0));
}

Oklch fitOklchToSrgb(const Oklch& source)
{
    Oklch candidate = sanitizedOklch(source);
    if (isLinearSrgbInGamut(oklchToLinearSrgb(candidate)))
        return candidate;

    double lowerChroma = 0.0;
    double upperChroma = candidate.c;
    for (int i = 0; i < kBinarySearchIterations; ++i) {
        const double chroma = (lowerChroma + upperChroma) / 2.0;
        Oklch attempt = candidate;
        attempt.c = chroma;
        if (isLinearSrgbInGamut(oklchToLinearSrgb(attempt)))
            lowerChroma = chroma;
        else
            upperChroma = chroma;
    }

    candidate.c = lowerChroma;
    return candidate;
}

double relativeLuminance(const QColor& color)
{
    const QColor rgb = color.toRgb();
    return 0.2126 * srgbToLinear(rgb.redF())
        + 0.7152 * srgbToLinear(rgb.greenF())
        + 0.0722 * srgbToLinear(rgb.blueF());
}

double contrastRatio(const QColor& first, const QColor& second)
{
    const double firstLuminance = relativeLuminance(first);
    const double secondLuminance = relativeLuminance(second);
    const double lighter = std::max(firstLuminance, secondLuminance);
    const double darker = std::min(firstLuminance, secondLuminance);
    return (lighter + 0.05) / (darker + 0.05);
}

QColor bestBlackOrWhiteForeground(const QColor& background)
{
    const QColor black(Qt::black);
    const QColor white(Qt::white);
    return contrastRatio(background, black) >= contrastRatio(background, white)
        ? black
        : white;
}

QColor primaryColorFromOklch(const Oklch& color)
{
    const Oklch fitted = fitOklchToSrgb(color);
    return colorFromLinearSrgb(oklchToLinearSrgb(fitted), fitted.alpha);
}

QColor contrastFallbackPrimary(const QColor& background)
{
    const QColor fallback = bestBlackOrWhiteForeground(background);
    if (fallback == QColor(Qt::white))
        return QColor(QStringLiteral("#f5f5f5"));
    if (fallback == QColor(Qt::black))
        return QColor(QStringLiteral("#111111"));
    return fallback;
}

double shadeLightnessForIndex(double baseLightness,
                              int index,
                              ThemePolarity polarity,
                              double distanceFactor)
{
    const double factor = clamp(distanceFactor, 0.0, 1.0);
    if (index <= 0) {
        return polarity == ThemePolarity::Dark
            ? baseLightness * (1.0 - factor)
            : baseLightness + ((1.0 - baseLightness) * factor);
    }
    if (index == 1)
        return baseLightness;

    const double percentage = clamp(factor * (index - 1), 0.0, 1.0);
    return polarity == ThemePolarity::Dark
        ? baseLightness + ((1.0 - baseLightness) * percentage)
        : baseLightness * (1.0 - percentage);
}

double shadeChromaMultiplierForIndex(int index)
{
    if (index <= 0)
        return kWindowShadeChromaMultiplier;
    if (index == 1)
        return 1.0;
    return std::pow(kDefaultShadeChromaDecay, index - 1);
}

QString debugColorHex(const QColor& color)
{
    if (!color.isValid())
        return QStringLiteral("<invalid>");
    const QColor::NameFormat format = color.alpha() == 255 ? QColor::HexRgb : QColor::HexArgb;
    return color.name(format).toUpper();
}

QString htmlColorDetails(const QColor& color)
{
    if (!color.isValid())
        return QStringLiteral("&lt;invalid&gt;");

    const Oklch converted = qColorToOklch(color);
    return QStringLiteral(
        "<code>%1</code><code>rgb(%2 %3 %4)</code><code>oklch(%5 %6 %7)</code>")
        .arg(debugColorHex(color).toHtmlEscaped())
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(converted.l, 0, 'f', 3)
        .arg(converted.c, 0, 'f', 3)
        .arg(converted.h, 0, 'f', 1);
}

QString htmlStatusLine(const QString& label, bool passed)
{
    return QStringLiteral("<p><span class=\"status-mark\">%1</span>%2: <code>%3</code></p>")
        .arg(passed ? QStringLiteral("&#10004;") : QStringLiteral("&#10006;"),
             label,
             passed ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
}

bool retainsVisibleTint(const Oklch& fitted, const Oklch& original)
{
    return original.c >= kMinimumVisibleTintChroma
        && fitted.c >= original.c * kMinimumChromaRetention;
}

QColor tintedAaForegroundForBackground(const QColor& background,
                                       const QColor& preferredTint,
                                       double minimumContrast)
{
    if (!background.isValid() || !preferredTint.isValid())
        return QColor();

    const Oklch originalTint = qColorToOklch(preferredTint);
    if (originalTint.c < kMinimumVisibleTintChroma)
        return QColor();

    Oklch fittedTint = fitOklchToSrgb(originalTint);
    QColor candidate = colorFromLinearSrgb(oklchToLinearSrgb(fittedTint), fittedTint.alpha);
    if (retainsVisibleTint(fittedTint, originalTint)
        && contrastRatio(candidate, background) >= minimumContrast) {
        return candidate;
    }

    const double targetLightness = relativeLuminance(background) < 0.5 ? 1.0 : 0.0;
    double failingLightness = originalTint.l;
    double passingLightness = targetLightness;
    QColor passingCandidate;
    Oklch passingTint = originalTint;

    for (int i = 0; i < kBinarySearchIterations; ++i) {
        Oklch attempt = originalTint;
        attempt.l = (failingLightness + passingLightness) / 2.0;
        const Oklch fitted = fitOklchToSrgb(attempt);
        const QColor foreground = colorFromLinearSrgb(oklchToLinearSrgb(fitted), fitted.alpha);
        if (contrastRatio(foreground, background) >= minimumContrast) {
            passingLightness = attempt.l;
            passingTint = fitted;
            passingCandidate = foreground;
        } else {
            failingLightness = attempt.l;
        }
    }

    if (passingCandidate.isValid() && retainsVisibleTint(passingTint, originalTint))
        return passingCandidate;

    return QColor();
}

} // namespace

Oklch qColorToOklch(const QColor& color)
{
    if (!color.isValid())
        return {0.0, 0.0, 0.0, 0.0};

    const QColor rgb = color.toRgb();
    const double r = srgbToLinear(rgb.redF());
    const double g = srgbToLinear(rgb.greenF());
    const double b = srgbToLinear(rgb.blueF());

    const double l = std::cbrt(
        kLinearSrgbToLms[0][0] * r + kLinearSrgbToLms[0][1] * g
        + kLinearSrgbToLms[0][2] * b);
    const double m = std::cbrt(
        kLinearSrgbToLms[1][0] * r + kLinearSrgbToLms[1][1] * g
        + kLinearSrgbToLms[1][2] * b);
    const double s = std::cbrt(
        kLinearSrgbToLms[2][0] * r + kLinearSrgbToLms[2][1] * g
        + kLinearSrgbToLms[2][2] * b);

    const double oklabL = kLmsToOklab[0][0] * l + kLmsToOklab[0][1] * m
        + kLmsToOklab[0][2] * s;
    const double oklabA = kLmsToOklab[1][0] * l + kLmsToOklab[1][1] * m
        + kLmsToOklab[1][2] * s;
    const double oklabB = kLmsToOklab[2][0] * l + kLmsToOklab[2][1] * m
        + kLmsToOklab[2][2] * s;
    const double chroma = std::hypot(oklabA, oklabB);
    const double hue = chroma < 1e-12
        ? 0.0
        : normalizedHue(std::atan2(oklabB, oklabA) * 180.0 / kPi);

    return {oklabL, chroma, hue, rgb.alphaF()};
}

QColor oklchToQColor(const Oklch& color)
{
    const Oklch sanitized = sanitizedOklch(color);
    return colorFromLinearSrgb(oklchToLinearSrgb(sanitized), sanitized.alpha);
}

QColor oklchToValidSrgbQColor(const Oklch& color)
{
    const Oklch fitted = fitOklchToSrgb(color);
    return colorFromLinearSrgb(oklchToLinearSrgb(fitted), fitted.alpha);
}

ThemePolarity themePolarityForBackground(const QColor& background)
{
    // Theme polarity follows OKLCH lightness, not sRGB channel averages, so the
    // generated surfaces and accents use the same perceptual light/dark split.
    return qColorToOklch(background).l < 0.50
        ? ThemePolarity::Dark
        : ThemePolarity::Light;
}

QColor generatePrimaryFromBackground(const QColor& background,
                                     std::optional<double> fallbackHueDegrees)
{
    if (!background.isValid())
        return QColor();

    const Oklch backgroundOklch = qColorToOklch(background);
    const ThemePolarity polarity = themePolarityForBackground(background);
    const bool dark = polarity == ThemePolarity::Dark;

    // Hue is preserved for chromatic backgrounds so the generated accent feels
    // related to the chosen theme instead of introducing an unrelated brand
    // color. Near-neutral backgrounds have an unstable OKLCH hue, so they use a
    // configurable fallback hue before applying the same chroma/lightness rules.
    const double hue = backgroundOklch.c < kNeutralBackgroundChromaThreshold
        ? normalizedHue(fallbackHueDegrees.value_or(kDefaultPrimaryFallbackHueDegrees))
        : backgroundOklch.h;

    // Accent colors deliberately use more chroma than the background so focus,
    // selection, and checked-state affordances read as active controls rather
    // than as another quiet surface. Dark and light themes need different fixed
    // lightness targets: dark themes require a lighter accent, while light
    // themes require a darker one.
    Oklch primary {
        dark ? kDarkPrimaryLightness : kLightPrimaryLightness,
        dark
            ? clamp(std::max(backgroundOklch.c * 2.5, 0.10), 0.10, 0.18)
            : clamp(std::max(backgroundOklch.c * 2.0, 0.09), 0.09, 0.16),
        hue,
        1.0
    };

    // sRGB cannot represent every OKLCH coordinate. Gamut fitting preserves hue
    // and lightness and reduces chroma, because hue carries theme identity and
    // lightness carries contrast; reducing chroma is the least disruptive way to
    // keep the color displayable.
    QColor candidate = primaryColorFromOklch(primary);
    if (contrastRatio(candidate, background) >= kPrimaryPreferredContrast)
        return candidate;

    const double lightnessLimit = dark
        ? kDarkPrimaryMaximumLightness
        : kLightPrimaryMinimumLightness;
    Oklch limitPrimary = primary;
    limitPrimary.l = lightnessLimit;
    QColor limitCandidate = primaryColorFromOklch(limitPrimary);
    const double targetContrast =
        contrastRatio(limitCandidate, background) >= kPrimaryPreferredContrast
        ? kPrimaryPreferredContrast
        : kPrimaryMinimumContrast;

    if (contrastRatio(limitCandidate, background) >= targetContrast) {
        // Move lightness only as far as needed to reach the target contrast.
        // This keeps the generated primary recognizably tied to the fixed
        // accent target while still satisfying readability requirements.
        double failingLightness = primary.l;
        double passingLightness = lightnessLimit;
        QColor passingCandidate = limitCandidate;
        for (int i = 0; i < kBinarySearchIterations; ++i) {
            Oklch attempt = primary;
            attempt.l = (failingLightness + passingLightness) / 2.0;
            const QColor attemptColor = primaryColorFromOklch(attempt);
            if (contrastRatio(attemptColor, background) >= targetContrast) {
                passingLightness = attempt.l;
                passingCandidate = attemptColor;
            } else {
                failingLightness = attempt.l;
            }
        }
        return passingCandidate;
    }

    primary.l = lightnessLimit;
    QColor bestCandidate = limitCandidate;
    double bestContrast = contrastRatio(bestCandidate, background);
    for (int i = 0; i < 8; ++i) {
        primary.c *= 0.85;
        candidate = primaryColorFromOklch(primary);
        const double contrast = contrastRatio(candidate, background);
        if (contrast > bestContrast) {
            bestContrast = contrast;
            bestCandidate = candidate;
        }
        if (contrast >= kPrimaryMinimumContrast)
            return candidate;
    }

    if (bestContrast >= kPrimaryMinimumContrast)
        return bestCandidate;

    // This is intentionally last-resort: black/white-like colors are less
    // expressive as accents, but a readable primary is preferable to preserving
    // chroma when an extreme input color defeats the bounded OKLCH adjustment.
    return contrastFallbackPrimary(background);
}

double defaultOklchShadeDistanceFactor()
{
    return kDefaultShadeDistanceFactor;
}

QVector<QColor> generateOklchShades(const QColor& base,
                                    int count,
                                    ThemePolarity polarity)
{
    return generateOklchShades(base, count, polarity, kDefaultShadeDistanceFactor);
}

QVector<QColor> generateOklchShades(const QColor& base,
                                    int count,
                                    ThemePolarity polarity,
                                    double distanceFactor)
{
    QVector<QColor> shades;
    if (count <= 0)
        return shades;

    shades.reserve(count);
    const Oklch anchor = qColorToOklch(base);
    if (count == 1) {
        shades.append(oklchToValidSrgbQColor(anchor));
        return shades;
    }

    for (int i = 0; i < count; ++i) {
        Oklch shade = anchor;
        shade.l = clamp(shadeLightnessForIndex(anchor.l, i, polarity, distanceFactor),
                        0.0,
                        1.0);
        shade.c = anchor.c * shadeChromaMultiplierForIndex(i);
        shades.append(oklchToValidSrgbQColor(shade));
    }
    return shades;
}

QColor aaForegroundForBackground(const QColor& background, double minimumContrast)
{
    return aaForegroundForBackground(background, background, minimumContrast);
}

QColor aaForegroundForBackground(const QColor& background,
                                 const QColor& preferredTint,
                                 double minimumContrast)
{
    const QColor tinted = tintedAaForegroundForBackground(
        background, preferredTint, minimumContrast);
    if (tinted.isValid())
        return tinted;
    return bestBlackOrWhiteForeground(background);
}

QVector<QColor> aaForegroundsForBackgrounds(const QVector<QColor>& backgrounds,
                                            double minimumContrast)
{
    QVector<QColor> foregrounds;
    foregrounds.reserve(backgrounds.size());
    for (const QColor& background : backgrounds)
        foregrounds.append(aaForegroundForBackground(background, minimumContrast));
    return foregrounds;
}

QVector<QColor> aaForegroundsForBackgrounds(const QVector<QColor>& backgrounds,
                                            const QColor& preferredTint,
                                            double minimumContrast)
{
    QVector<QColor> foregrounds;
    foregrounds.reserve(backgrounds.size());
    for (const QColor& background : backgrounds) {
        foregrounds.append(
            aaForegroundForBackground(background, preferredTint, minimumContrast));
    }
    return foregrounds;
}

QString writeOklchGenerationHtmlReport(const QColor& base,
                                       int leftShadeCount,
                                       int rightShadeCount,
                                       ThemePolarity polarity,
                                       double distanceFactor,
                                       bool requireAllWcag,
                                       const QVector<QColor>& backgrounds,
                                       const QVector<QColor>& foregrounds,
                                       double minimumContrast)
{
    if (!kOklchHtmlReportEnabled)
        return QString();

    const QString reportPath = QDir(QDir::tempPath()).absoluteFilePath(
        QStringLiteral("speedcrunch-oklch-theme-report.html"));
    QFile file(reportPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return QString();

    const QString polarityName = polarity == ThemePolarity::Dark
        ? QStringLiteral("DARK")
        : QStringLiteral("LIGHT");
    const QString executablePath = QCoreApplication::applicationFilePath();
    QTextStream out(&file);
    out << R"HTML(<!doctype html>
<html lang="en">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>OKLCH Shade Stripes</title>
    <style>
:root { --ink: #172118; --muted: #59675a; --line: #d5ddd4; --surface: #ffffff; --wash: #f3f6f1; --accent: #333b4a; --radius: 16px; }
* { box-sizing: border-box; }
.sr-only { position: absolute; width: 1px; height: 1px; padding: 0; overflow: hidden; white-space: nowrap; clip-path: inset(50%); }
body { margin: 0; color: var(--ink); background: var(--wash); font: 15px/1.45 "Avenir Next", "Segoe UI", Arial, sans-serif; }
.page { max-width: 1540px; margin: 0 auto; padding: clamp(1rem, 3vw, 2.5rem); }
.intro { margin-bottom: 1.5rem; }
.eyebrow { margin: 0 0 0.35rem; color: var(--accent); font-size: 0.76rem; font-weight: 700; letter-spacing: 0.14em; text-transform: uppercase; }
h1 { margin: 0; font-size: clamp(1.8rem, 4vw, 2.7rem); font-weight: 700; line-height: 1.12; }
.description { max-width: 58ch; margin: 0.55rem 0 0; color: var(--muted); }
.controls { display: grid; grid-template-columns: repeat(auto-fit, minmax(155px, 1fr)); gap: 1rem; padding: 1.15rem; margin-bottom: 1.75rem; background: var(--surface); border: 1px solid var(--line); border-radius: var(--radius); box-shadow: 0 1px 3px rgb(23 33 24 / 5%); }
.control-group { display: flex; flex-direction: column; gap: 0.45rem; min-width: 0; margin: 0; padding: 0; border: 0; }
.control-group > label, .control-group legend { color: var(--muted); font-size: 0.81rem; font-weight: 650; letter-spacing: 0.03em; text-transform: uppercase; }
.control-group legend { margin-bottom: 0.45rem; }
input[type="number"], input[type="text"] { width: 100%; min-height: 2.75rem; padding: 0.55rem 0.7rem; color: var(--ink); background: #fcfdfb; border: 1px solid #c6d0c4; border-radius: 9px; font: inherit; }
input:focus-visible { outline: 3px solid rgb(51 59 74 / 18%); border-color: var(--accent); }
.color-fields { display: flex; gap: 0.5rem; }
input[type="color"] { flex: 0 0 3rem; height: 2.75rem; padding: 0.22rem; background: #fcfdfb; border: 1px solid #c6d0c4; border-radius: 9px; cursor: pointer; }
.color-fields input[type="text"] { font-family: "SFMono-Regular", Consolas, monospace; text-transform: uppercase; }
.field-message { min-height: 1em; color: #a93425; font-size: 0.77rem; }
.polarity { justify-content: flex-start; }
.choice { display: flex; align-items: center; gap: 0.45rem; color: var(--ink); font-weight: 500; }
.choice input { accent-color: var(--accent); }
.wcag-control { justify-content: center; }
.control-group > .requirement-choice { color: var(--ink); font-size: 0.86rem; font-weight: 600; letter-spacing: 0; text-transform: none; }
.hint { color: var(--muted); font-size: 0.75rem; }
.output-heading { display: flex; flex-wrap: wrap; align-items: baseline; justify-content: space-between; gap: 0.5rem 1rem; margin-bottom: 0.8rem; }
.output-heading h2 { margin: 0; font-size: 1.05rem; }
.output-heading p { margin: 0; color: var(--muted); font-size: 0.88rem; }
.stripe-window { overflow-x: auto; border: 1px solid var(--line); border-radius: var(--radius); background: var(--surface); }
.stripes { --stripe-count: 1; display: grid; grid-template-columns: repeat(var(--stripe-count), minmax(9rem, 1fr)); min-width: max(100%, calc(var(--stripe-count) * 9rem)); min-height: 360px; }
.stripe { display: flex; flex-direction: column; justify-content: space-between; min-height: 360px; padding: 1rem 0.9rem; border-right: 1px solid rgb(0 0 0 / 9%); }
.stripe::selection, .stripe ::selection { color: var(--selection-foreground); background-color: var(--selection-background); }
.stripe:last-child { border-right: 0; }
.stripe-heading { min-height: 3.1rem; }
.level { margin: 0; font-size: 1.1rem; font-weight: 700; letter-spacing: 0.05em; }
.base-marker { margin: 0.18rem 0 0; font-size: 0.68rem; font-weight: 700; letter-spacing: 0.14em; text-transform: uppercase; }
.stripe-content { width: 100%; }
.color-details { margin: 0; }
.color-details code { display: block; margin: 0.24rem 0 0; font-family: "SFMono-Regular", Consolas, monospace; font-size: 0.8rem; }
.foreground { margin: 0.82rem 0 0; font-size: 0.81rem; }
.foreground code { font-family: "SFMono-Regular", Consolas, monospace; font-size: 0.8rem; }
.wcag-report { margin: 1rem 0 0; padding-top: 0.85rem; border-top: 1px solid currentcolor; font-size: 0.78rem; }
.wcag-report p { margin: 0.2rem 0 0; }
.wcag-report code { font-family: "SFMono-Regular", Consolas, monospace; font-size: 0.78rem; }
.status-mark { display: inline-block; width: 1.15em; font-family: inherit; font-weight: 700; }
@media (max-width: 960px) { .controls { grid-template-columns: repeat(2, minmax(150px, 1fr)); } }
@media (max-width: 520px) { .page { padding: 0.9rem; } .controls { grid-template-columns: 1fr; } }
    </style>
    <script>
"use strict";
window.speedCrunchOklchReport = Object.freeze({
)HTML";
    out << "  baseColor: \"" << debugColorHex(base) << "\",\n"
        << "  executablePath: \"" << executablePath.toHtmlEscaped() << "\",\n"
        << "  leftShadeCount: " << leftShadeCount << ",\n"
        << "  rightShadeCount: " << rightShadeCount << ",\n"
        << "  polarity: \"" << polarityName << "\",\n"
        << "  distanceFactor: " << QString::number(distanceFactor, 'f', 3) << ",\n"
        << "  minimumContrast: " << QString::number(minimumContrast, 'f', 2) << ",\n"
        << "  requireAllWcag: " << (requireAllWcag ? "true" : "false") << "\n"
        << "});\n"
        << "document.documentElement.dataset.oklchReport = \"ready\";\n"
        << R"HTML(    </script>
  </head>
  <body>
    <main class="page">
      <header class="intro">
        <p class="eyebrow">Color scale tool</p>
        <h1>OKLCH shade stripes</h1>
        <p class="description">Move each shade by a percentage of the remaining OKLCH lightness range while preserving hue and decaying chroma away from the anchor.</p>
        <p class="description"><strong>Generated by:</strong> <code>)HTML"
        << executablePath.toHtmlEscaped()
        << R"HTML(</code></p>
      </header>
      <form class="controls" id="controls" autocomplete="off">
        <div class="control-group color-control">
          <label for="baseColor">Base color</label>
          <div class="color-fields">
)HTML";
    out << "            <input id=\"baseColor\" name=\"baseColor\" type=\"color\" value=\""
        << debugColorHex(base) << "\">\n"
        << "            <label class=\"sr-only\" for=\"hexColor\">Base color hex value</label>\n"
        << "            <input id=\"hexColor\" name=\"hexColor\" type=\"text\" value=\""
        << debugColorHex(base)
        << "\" maxlength=\"7\" inputmode=\"text\" spellcheck=\"false\" aria-describedby=\"hexError\">\n"
        << R"HTML(          </div>
          <span class="field-message" id="hexError" aria-live="polite"></span>
        </div>
        <div class="control-group">
          <label for="leftCount">Left shade count</label>
)HTML"
        << "          <input id=\"leftCount\" name=\"leftCount\" type=\"number\" min=\"0\" max=\"16\" step=\"1\" value=\""
        << leftShadeCount << "\">\n"
        << R"HTML(        </div>
        <div class="control-group">
          <label for="rightCount">Right shade count</label>
)HTML"
        << "          <input id=\"rightCount\" name=\"rightCount\" type=\"number\" min=\"0\" max=\"16\" step=\"1\" value=\""
        << rightShadeCount << "\">\n"
        << R"HTML(        </div>
        <fieldset class="control-group polarity">
          <legend>Theme polarity</legend>
          <label class="choice"><input type="radio" name="polarity" value="light" )HTML"
        << (polarity == ThemePolarity::Light ? "checked" : "") << "> Light</label>\n"
        << R"HTML(          <label class="choice"><input type="radio" name="polarity" value="dark" )HTML"
        << (polarity == ThemePolarity::Dark ? "checked" : "") << "> Dark</label>\n"
        << R"HTML(        </fieldset>
        <div class="control-group">
          <label for="distanceFactor">Shade distance factor</label>
)HTML"
        << "          <input id=\"distanceFactor\" name=\"distanceFactor\" type=\"number\" min=\"0.01\" max=\"0.20\" step=\"0.01\" value=\""
        << QString::number(distanceFactor, 'f', 2) << "\">\n"
        << R"HTML(        </div>
        <div class="control-group wcag-control">
          <label class="choice requirement-choice" for="requireAllWcag"><input id="requireAllWcag" name="requireAllWcag" type="checkbox" )HTML"
        << (requireAllWcag ? "checked" : "") << "> Require all WCAG specs to pass</label>\n"
        << "          <span class=\"hint\">Targets "
        << (requireAllWcag ? "AAA" : "AA") << " text contrast; shade colors stay fixed.</span>\n"
        << R"HTML(        </div>
      </form>
      <section class="output" aria-labelledby="outputHeading">
        <div class="output-heading">
          <h2 id="outputHeading">Generated stripes</h2>
          <p id="stripeSummary" aria-live="polite">)HTML"
        << backgrounds.size() << " " << (backgrounds.size() == 1 ? "stripe" : "stripes")
        << " | " << polarityName << " | factor " << QString::number(distanceFactor, 'f', 2)
        << " | " << (requireAllWcag ? "AAA REQUIRED" : "AA REQUIRED")
        << R"HTML(</p>
        </div>
        <div class="stripe-window">
          <div class="stripes" id="stripes" role="list" aria-label="Generated OKLCH shade stripes" style="--stripe-count: )HTML"
        << backgrounds.size() << R"HTML(;">
)HTML";

    for (int index = 0; index < backgrounds.size(); ++index) {
        const QColor background = backgrounds.at(index);
        const QColor foreground = foregrounds.value(index);
        const double contrast = foreground.isValid()
            ? contrastRatio(background, foreground)
            : 0.0;
        const bool isBase = index == leftShadeCount;
        out << "            <article class=\"stripe" << (isBase ? " base" : "")
            << "\" role=\"listitem\" style=\"background-color: " << debugColorHex(background)
            << "; color: " << debugColorHex(foreground)
            << "; --selection-background: " << debugColorHex(foreground)
            << "; --selection-foreground: " << debugColorHex(background) << ";\">\n"
            << "              <div class=\"stripe-heading\"><p class=\"level\">"
            << (index + 1) * 100 << "</p>"
            << (isBase ? "<p class=\"base-marker\">Base</p>" : "") << "</div>\n"
            << "              <div class=\"stripe-content\">\n"
            << "                <p class=\"color-details\">" << htmlColorDetails(background) << "</p>\n"
            << "                <p class=\"foreground\">Foreground: <code>"
            << debugColorHex(foreground).toHtmlEscaped() << "</code></p>\n"
            << "                <div class=\"wcag-report\">\n"
            << "                  <p>WCAG: <code>" << QString::number(contrast, 'f', 2)
            << ":1</code></p>\n"
            << "                  " << htmlStatusLine(QStringLiteral("AA"), contrast >= kAaMinimumContrast) << "\n"
            << "                  " << htmlStatusLine(QStringLiteral("AALarge"), contrast >= 3.0) << "\n"
            << "                  " << htmlStatusLine(QStringLiteral("AAA"), contrast >= kAaaMinimumContrast) << "\n"
            << "                  " << htmlStatusLine(QStringLiteral("AAALarge"), contrast >= kAaMinimumContrast) << "\n"
            << "                </div>\n"
            << "              </div>\n"
            << "            </article>\n";
    }

    out << R"HTML(          </div>
        </div>
      </section>
    </main>
  </body>
</html>
)HTML";
    file.close();
    return reportPath;
}
