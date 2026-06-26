// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/oklchutils.h"
#include "gui/uiconfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTest>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kExpectedAaMinimumContrast = 7.0;
constexpr double kExpectedAaaMinimumContrast = 7.0;

double srgbToLinear(double channel)
{
    return channel <= 0.04045
        ? channel / 12.92
        : std::pow((channel + 0.055) / 1.055, 2.4);
}

double relativeLuminance(const QColor& color)
{
    return 0.2126 * srgbToLinear(color.redF())
        + 0.7152 * srgbToLinear(color.greenF())
        + 0.0722 * srgbToLinear(color.blueF());
}

double contrastRatio(const QColor& first, const QColor& second)
{
    const double firstLuminance = relativeLuminance(first);
    const double secondLuminance = relativeLuminance(second);
    return (std::max(firstLuminance, secondLuminance) + 0.05)
        / (std::min(firstLuminance, secondLuminance) + 0.05);
}

double bestBlackOrWhiteContrast(const QColor& background)
{
    return std::max(contrastRatio(QColor(Qt::black), background),
                    contrastRatio(QColor(Qt::white), background));
}

bool colorsAreClose(const QColor& first, const QColor& second)
{
    return std::abs(first.red() - second.red()) <= 1
        && std::abs(first.green() - second.green()) <= 1
        && std::abs(first.blue() - second.blue()) <= 1
        && std::abs(first.alpha() - second.alpha()) <= 1;
}

double hueDistance(double first, double second)
{
    const double difference = std::abs(first - second);
    return std::min(difference, 360.0 - difference);
}

void verifyValidColor(const QColor& color)
{
    QVERIFY(color.isValid());
    QVERIFY(color.redF() >= 0.0 && color.redF() <= 1.0);
    QVERIFY(color.greenF() >= 0.0 && color.greenF() <= 1.0);
    QVERIFY(color.blueF() >= 0.0 && color.blueF() <= 1.0);
    QVERIFY(color.alphaF() >= 0.0 && color.alphaF() <= 1.0);
}

} // namespace

class TestOklchUtils : public QObject
{
    Q_OBJECT

private slots:
    void colors_round_trip_through_oklch();
    void gamut_fitting_handles_excessive_chroma_and_preserves_alpha();
    void shades_handle_empty_and_single_counts();
    void percentage_shades_follow_requested_distance();
    void shade_chroma_decays_away_from_base();
    void light_shades_decrease_in_lightness();
    void dark_shades_increase_in_lightness();
    void default_foregrounds_meet_aa_and_choose_neutral_extremes();
    void preferred_tint_is_kept_when_it_passes_aa();
    void preferred_tint_is_adjusted_to_pass_aa();
    void foregrounds_support_light_tints_dark_tints_and_aaa();
    void visually_lost_tint_falls_back_to_black_or_white();
    void foreground_vector_matches_background_vector();
    void primary_from_dark_neutral_background_is_chromatic_and_readable();
    void primary_from_black_background_is_valid_and_readable();
    void primary_from_light_neutral_background_is_darker_chromatic_accent();
    void primary_from_saturated_background_keeps_hue_family_and_gains_prominence();
    void primary_generation_handles_gamut_edges();
    void generated_primary_meets_minimum_contrast_for_representative_backgrounds();
    void preferred_blue_link_is_adjusted_to_aa_on_dark_formula_book_surface();
    void secondary_link_is_readable_neutral_and_hue_distinct();
    void secondary_link_tracks_theme_hue_when_it_is_distinct();
    void html_report_contains_generation_inputs_and_outputs();
};

void TestOklchUtils::colors_round_trip_through_oklch()
{
    const QVector<QColor> colors = {
        QColor(Qt::white),
        QColor(Qt::black),
        QColor(QStringLiteral("#00ff00"))
    };
    for (const QColor& original : colors) {
        const QColor roundTrip = oklchToQColor(qColorToOklch(original));
        QVERIFY(colorsAreClose(roundTrip, original));
    }
}

void TestOklchUtils::gamut_fitting_handles_excessive_chroma_and_preserves_alpha()
{
    const QColor fitted = oklchToValidSrgbQColor({0.62, 1.2, 145.0, 0.35});

    verifyValidColor(fitted);
    QVERIFY(std::abs(fitted.alphaF() - 0.35) < 0.001);
}

void TestOklchUtils::shades_handle_empty_and_single_counts()
{
    const QColor base(QStringLiteral("#3c7655"));
    QVERIFY(generateOklchShades(base, 0, ThemePolarity::Light).isEmpty());
    QVERIFY(generateOklchShades(base, -1, ThemePolarity::Dark).isEmpty());

    const QVector<QColor> single = generateOklchShades(base, 1, ThemePolarity::Light);
    QCOMPARE(single.size(), 1);
    verifyValidColor(single.first());
    QVERIFY(colorsAreClose(single.first(), base));
}

void TestOklchUtils::percentage_shades_follow_requested_distance()
{
    const QColor base(QStringLiteral("#466353"));
    const double factor = 0.10;
    const double baseLightness = qColorToOklch(base).l;
    const QVector<QColor> dark =
        generateOklchShades(base, 5, ThemePolarity::Dark, factor);
    const QVector<QColor> light =
        generateOklchShades(base, 5, ThemePolarity::Light, factor);

    QCOMPARE(dark.size(), 5);
    QCOMPARE(light.size(), 5);
    QVERIFY(colorsAreClose(dark.at(1), base));
    QVERIFY(colorsAreClose(light.at(1), base));

    const double darkShade0L = qColorToOklch(dark.at(0)).l;
    const double darkShade2L = qColorToOklch(dark.at(2)).l;
    const double darkShade3L = qColorToOklch(dark.at(3)).l;
    QVERIFY(std::abs(darkShade0L - baseLightness * (1.0 - factor)) < 0.004);
    QVERIFY(std::abs(darkShade2L - (baseLightness + (1.0 - baseLightness) * factor)) < 0.004);
    QVERIFY(std::abs(darkShade3L - (baseLightness + (1.0 - baseLightness) * factor * 2.0)) < 0.004);

    const double lightShade0L = qColorToOklch(light.at(0)).l;
    const double lightShade2L = qColorToOklch(light.at(2)).l;
    const double lightShade3L = qColorToOklch(light.at(3)).l;
    QVERIFY(std::abs(lightShade0L - (baseLightness + (1.0 - baseLightness) * factor)) < 0.004);
    QVERIFY(std::abs(lightShade2L - baseLightness * (1.0 - factor)) < 0.004);
    QVERIFY(std::abs(lightShade3L - baseLightness * (1.0 - factor * 2.0)) < 0.004);
}

void TestOklchUtils::shade_chroma_decays_away_from_base()
{
    const QColor base(QStringLiteral("#8a3f68"));
    const QVector<QColor> shades =
        generateOklchShades(base, 5, ThemePolarity::Dark, 0.10);

    QCOMPARE(shades.size(), 5);
    const double baseChroma = qColorToOklch(shades.at(1)).c;
    QVERIFY(baseChroma > 0.01);
    QVERIFY(qColorToOklch(shades.at(0)).c < baseChroma);
    QVERIFY(qColorToOklch(shades.at(2)).c < baseChroma);
    QVERIFY(qColorToOklch(shades.at(3)).c < qColorToOklch(shades.at(2)).c);
    QVERIFY(qColorToOklch(shades.at(4)).c < qColorToOklch(shades.at(3)).c);
}

void TestOklchUtils::light_shades_decrease_in_lightness()
{
    const QVector<QVector<QColor>> palettes = {
        generateOklchShades(QColor(QStringLiteral("#67a87b")), 7, ThemePolarity::Light),
        generateOklchShades(QColor(QStringLiteral("#010101")), 7, ThemePolarity::Light)
    };

    for (const QVector<QColor>& shades : palettes) {
        QCOMPARE(shades.size(), 7);
        for (int i = 0; i < shades.size(); ++i) {
            verifyValidColor(shades.at(i));
            if (i > 0) {
                QVERIFY(qColorToOklch(shades.at(i)).l
                        <= qColorToOklch(shades.at(i - 1)).l + 1e-6);
            }
        }
    }
}

void TestOklchUtils::dark_shades_increase_in_lightness()
{
    const QVector<QVector<QColor>> palettes = {
        generateOklchShades(QColor(QStringLiteral("#24372c")), 7, ThemePolarity::Dark),
        generateOklchShades(QColor(QStringLiteral("#fefefe")), 7, ThemePolarity::Dark)
    };

    for (const QVector<QColor>& shades : palettes) {
        QCOMPARE(shades.size(), 7);
        for (int i = 0; i < shades.size(); ++i) {
            verifyValidColor(shades.at(i));
            if (i > 0) {
                QVERIFY(qColorToOklch(shades.at(i)).l
                        >= qColorToOklch(shades.at(i - 1)).l - 1e-6);
            }
        }
    }
}

void TestOklchUtils::default_foregrounds_meet_aa_and_choose_neutral_extremes()
{
    const QColor whiteBackground(Qt::white);
    const QColor blackBackground(Qt::black);
    QCOMPARE(aaForegroundForBackground(whiteBackground), QColor(Qt::black));
    QCOMPARE(aaForegroundForBackground(blackBackground), QColor(Qt::white));

    const QVector<QColor> backgrounds = {
        whiteBackground,
        blackBackground,
        QColor(QStringLiteral("#888888")),
        QColor(QStringLiteral("#174128"))
    };
    for (const QColor& background : backgrounds) {
        const QColor foreground = aaForegroundForBackground(background);
        const double contrast = contrastRatio(foreground, background);
        if (bestBlackOrWhiteContrast(background) >= kExpectedAaMinimumContrast)
            QVERIFY(contrast >= kExpectedAaMinimumContrast);
        else
            QCOMPARE(contrast, bestBlackOrWhiteContrast(background));
    }

    const QColor greenForeground =
        aaForegroundForBackground(QColor(QStringLiteral("#174128")));
    QVERIFY(greenForeground != QColor(Qt::black));
    QVERIFY(greenForeground != QColor(Qt::white));
}

void TestOklchUtils::preferred_tint_is_kept_when_it_passes_aa()
{
    const QColor background(QStringLiteral("#10241a"));
    const QColor tint(QStringLiteral("#c8edcf"));
    const QColor foreground = aaForegroundForBackground(background, tint);

    QCOMPARE(foreground.name(), tint.name());
    QVERIFY(contrastRatio(foreground, background) >= kExpectedAaMinimumContrast);
}

void TestOklchUtils::preferred_tint_is_adjusted_to_pass_aa()
{
    const QColor background(QStringLiteral("#14251b"));
    const QColor tint(QStringLiteral("#416750"));
    QVERIFY(contrastRatio(tint, background) < kExpectedAaMinimumContrast);

    const QColor foreground = aaForegroundForBackground(background, tint);
    QVERIFY(contrastRatio(foreground, background) >= kExpectedAaMinimumContrast);
    QVERIFY(foreground != QColor(Qt::black));
    QVERIFY(foreground != QColor(Qt::white));
    QVERIFY(foreground.name() != tint.name());
}

void TestOklchUtils::foregrounds_support_light_tints_dark_tints_and_aaa()
{
    const QColor darkBackground(QStringLiteral("#123526"));
    const QColor paleGreen(QStringLiteral("#c8eed5"));
    const QColor lightBackground(QStringLiteral("#e5f2e8"));
    const QColor darkGreen(QStringLiteral("#16442d"));

    const QColor paleForeground = aaForegroundForBackground(darkBackground, paleGreen);
    const QColor darkForeground = aaForegroundForBackground(lightBackground, darkGreen);
    QVERIFY(contrastRatio(paleForeground, darkBackground) >= kExpectedAaMinimumContrast);
    QVERIFY(contrastRatio(darkForeground, lightBackground) >= kExpectedAaMinimumContrast);
    QVERIFY(paleForeground != QColor(Qt::black) && paleForeground != QColor(Qt::white));
    QVERIFY(darkForeground != QColor(Qt::black) && darkForeground != QColor(Qt::white));
    QVERIFY(contrastRatio(
                aaForegroundForBackground(QColor(Qt::black), paleGreen, kExpectedAaaMinimumContrast),
                QColor(Qt::black))
            >= kExpectedAaaMinimumContrast);
}

void TestOklchUtils::visually_lost_tint_falls_back_to_black_or_white()
{
    const QColor background(QStringLiteral("#6f6f6f"));
    const QColor saturatedTint(QStringLiteral("#0000ff"));
    const QColor foreground = aaForegroundForBackground(background, saturatedTint);

    QVERIFY(foreground == QColor(Qt::black) || foreground == QColor(Qt::white));
    QCOMPARE(contrastRatio(foreground, background), bestBlackOrWhiteContrast(background));
}

void TestOklchUtils::foreground_vector_matches_background_vector()
{
    const QVector<QColor> backgrounds = {
        QColor(QStringLiteral("#10241a")),
        QColor(QStringLiteral("#315d43")),
        QColor(QStringLiteral("#6f6f6f")),
        QColor(QStringLiteral("#888888")),
        QColor(QStringLiteral("#e5f2e8"))
    };
    const QVector<QColor> defaults = aaForegroundsForBackgrounds(backgrounds);
    const QVector<QColor> tinted = aaForegroundsForBackgrounds(
        backgrounds, QColor(QStringLiteral("#49845c")));

    QCOMPARE(defaults.size(), backgrounds.size());
    QCOMPARE(tinted.size(), backgrounds.size());
    for (int i = 0; i < backgrounds.size(); ++i) {
        const double defaultContrast = contrastRatio(defaults.at(i), backgrounds.at(i));
        const double tintedContrast = contrastRatio(tinted.at(i), backgrounds.at(i));
        if (bestBlackOrWhiteContrast(backgrounds.at(i)) >= kExpectedAaMinimumContrast) {
            QVERIFY(defaultContrast >= kExpectedAaMinimumContrast);
            QVERIFY(tintedContrast >= kExpectedAaMinimumContrast);
        } else {
            QCOMPARE(defaultContrast, bestBlackOrWhiteContrast(backgrounds.at(i)));
            QCOMPARE(tintedContrast, bestBlackOrWhiteContrast(backgrounds.at(i)));
        }
    }
}

void TestOklchUtils::primary_from_dark_neutral_background_is_chromatic_and_readable()
{
    const QColor background(QStringLiteral("#232136"));
    const QColor primary = generatePrimaryFromBackground(background);
    const Oklch backgroundOklch = qColorToOklch(background);
    const Oklch primaryOklch = qColorToOklch(primary);

    verifyValidColor(primary);
    QVERIFY(primaryOklch.c >= 0.05);
    QVERIFY(primaryOklch.l > backgroundOklch.l);
    QVERIFY(contrastRatio(primary, background) >= 3.0);
    if (backgroundOklch.c < 0.02)
        QVERIFY(hueDistance(primaryOklch.h, 250.0) < 8.0);
    else
        QVERIFY(hueDistance(primaryOklch.h, backgroundOklch.h) < 12.0);
}

void TestOklchUtils::primary_from_black_background_is_valid_and_readable()
{
    const QColor background(QStringLiteral("#000000"));
    const QColor primary = generatePrimaryFromBackground(background);
    const Oklch primaryOklch = qColorToOklch(primary);

    verifyValidColor(primary);
    QVERIFY(primaryOklch.c >= 0.05);
    QVERIFY(primaryOklch.l > 0.65);
    QVERIFY(hueDistance(primaryOklch.h, 250.0) < 8.0);
    QVERIFY(contrastRatio(primary, background) >= 3.0);
}

void TestOklchUtils::primary_from_light_neutral_background_is_darker_chromatic_accent()
{
    const QColor background(QStringLiteral("#f5f5f5"));
    const QColor primary = generatePrimaryFromBackground(background);
    const Oklch backgroundOklch = qColorToOklch(background);
    const Oklch primaryOklch = qColorToOklch(primary);

    verifyValidColor(primary);
    QVERIFY(primaryOklch.c >= 0.04);
    QVERIFY(primaryOklch.l < backgroundOklch.l);
    QVERIFY(hueDistance(primaryOklch.h, 250.0) < 8.0);
    QVERIFY(contrastRatio(primary, background) >= 3.0);
}

void TestOklchUtils::primary_from_saturated_background_keeps_hue_family_and_gains_prominence()
{
    const QColor background(QStringLiteral("#003b46"));
    const QColor primary = generatePrimaryFromBackground(background);
    const Oklch backgroundOklch = qColorToOklch(background);
    const Oklch primaryOklch = qColorToOklch(primary);

    verifyValidColor(primary);
    QVERIFY(hueDistance(primaryOklch.h, backgroundOklch.h) < 12.0);
    QVERIFY(primaryOklch.c > backgroundOklch.c);
    QVERIFY(primaryOklch.l > backgroundOklch.l);
    QVERIFY(contrastRatio(primary, background) >= 3.0);
}

void TestOklchUtils::primary_generation_handles_gamut_edges()
{
    const QVector<QColor> backgrounds = {
        QColor(QStringLiteral("#ff0000")),
        QColor(QStringLiteral("#00ff00")),
        QColor(QStringLiteral("#0000ff")),
        QColor(QStringLiteral("#00ffff")),
        QColor(QStringLiteral("#ff00ff")),
        QColor(QStringLiteral("#ffff00")),
        QColor(QStringLiteral("#ffffff")),
        QColor(QStringLiteral("#000000"))
    };

    for (const QColor& background : backgrounds)
        verifyValidColor(generatePrimaryFromBackground(background));
}

void TestOklchUtils::generated_primary_meets_minimum_contrast_for_representative_backgrounds()
{
    const QVector<QColor> backgrounds = {
        QColor(QStringLiteral("#232136")),
        QColor(QStringLiteral("#000000")),
        QColor(QStringLiteral("#f5f5f5")),
        QColor(QStringLiteral("#003b46")),
        QColor(QStringLiteral("#101010")),
        QColor(QStringLiteral("#fafafa")),
        QColor(QStringLiteral("#663399")),
        QColor(QStringLiteral("#e6d9b8"))
    };

    for (const QColor& background : backgrounds) {
        const QColor primary = generatePrimaryFromBackground(background);
        verifyValidColor(primary);
        QVERIFY(contrastRatio(primary, background) >= 3.0);
    }
}

void TestOklchUtils::preferred_blue_link_is_adjusted_to_aa_on_dark_formula_book_surface()
{
    const QColor background(QStringLiteral("#333348"));
    const QColor preferredLink(QStringLiteral("#0000ff"));
    QVERIFY(contrastRatio(preferredLink, background) < kExpectedAaMinimumContrast);

    const QColor formulaLink = aaForegroundForBackground(background, preferredLink);
    verifyValidColor(formulaLink);
    QVERIFY(contrastRatio(formulaLink, background) >= kExpectedAaMinimumContrast);
    QVERIFY(formulaLink != QColor(Qt::black));
    QVERIFY(formulaLink != QColor(Qt::white));
}

void TestOklchUtils::secondary_link_is_readable_neutral_and_hue_distinct()
{
    const QColor background(QStringLiteral("#232136"));
    const QColor primaryLink = generatePrimaryFromBackground(background);
    const QColor secondaryLink = generateSecondaryLinkFromBackground(background, primaryLink);
    const Oklch primaryOklch = qColorToOklch(primaryLink);
    const Oklch secondaryOklch = qColorToOklch(secondaryLink);

    verifyValidColor(secondaryLink);
    QVERIFY(contrastRatio(secondaryLink, background) >= kExpectedAaMinimumContrast);
    QVERIFY(hueDistance(secondaryOklch.h, primaryOklch.h) >= 45.0);
    QVERIFY(secondaryOklch.c < primaryOklch.c);
}

void TestOklchUtils::secondary_link_tracks_theme_hue_when_it_is_distinct()
{
    const QColor background(QStringLiteral("#123526"));
    const QColor primaryLink(QStringLiteral("#d6c2ff"));
    const QColor secondaryLink = generateSecondaryLinkFromBackground(background, primaryLink);
    const Oklch backgroundOklch = qColorToOklch(background);
    const Oklch primaryOklch = qColorToOklch(primaryLink);
    const Oklch secondaryOklch = qColorToOklch(secondaryLink);

    verifyValidColor(secondaryLink);
    QVERIFY(contrastRatio(secondaryLink, background) >= kExpectedAaMinimumContrast);
    QVERIFY(hueDistance(secondaryOklch.h, primaryOklch.h) >= 45.0);
    QVERIFY(hueDistance(secondaryOklch.h, backgroundOklch.h) < 18.0);
}

void TestOklchUtils::html_report_contains_generation_inputs_and_outputs()
{
    const QColor base(QStringLiteral("#333b4a"));
    const QVector<QColor> backgrounds =
        generateOklchShades(base, 3, ThemePolarity::Dark, 0.1);
    const QVector<QColor> foregrounds = aaForegroundsForBackgrounds(backgrounds);

    const QString path = writeOklchGenerationHtmlReport(base,
                                                         1,
                                                         1,
                                                         ThemePolarity::Dark,
                                                         0.1,
                                                         false,
                                                         backgrounds,
                                                         foregrounds);
    if (!UiConfig::OklchThemeDebugReportEnabled) {
        QVERIFY(path.isEmpty());
        return;
    }

    QVERIFY(!path.isEmpty());
    QVERIFY(path.startsWith(QDir::tempPath()));
    QVERIFY(path.endsWith(QStringLiteral("/speedcrunch-oklch-theme-report.html")));

    QFile report(path);
    QVERIFY(report.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString html = QString::fromUtf8(report.readAll());
    QVERIFY(html.contains(QStringLiteral("<style>")));
    QVERIFY(html.contains(QStringLiteral("<script>")));
    QVERIFY(html.contains(QStringLiteral("window.speedCrunchOklchReport")));
    QVERIFY(!html.contains(QStringLiteral("href=\"styles.css\"")));
    QVERIFY(!html.contains(QStringLiteral("src=\"app.js\"")));
    QVERIFY(html.contains(QStringLiteral("OKLCH shade stripes")));
    QVERIFY(html.contains(QStringLiteral("baseColor: \"#333B4A\"")));
    QVERIFY(html.contains(QStringLiteral("leftShadeCount: 1")));
    QVERIFY(html.contains(QStringLiteral("rightShadeCount: 1")));
    QVERIFY(html.contains(QStringLiteral("polarity: \"DARK\"")));
    QVERIFY(html.contains(QStringLiteral("distanceFactor: 0.100")));
    QVERIFY(html.contains(QStringLiteral("minimumContrast: 7.00")));
    QVERIFY(html.contains(QStringLiteral("Generated stripes")));
    QVERIFY(html.contains(QStringLiteral("background-color: #333B4A")));
    QVERIFY(html.contains(QStringLiteral("Foreground:")));
    QVERIFY(html.contains(QStringLiteral("WCAG: <code>")));
    QVERIFY(html.contains(QStringLiteral("AA: <code>PASS</code>")));
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    TestOklchUtils test;
    return QTest::qExec(&test, argc, argv);
}

#include "testoklch.moc"
