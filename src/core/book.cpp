// SPDX-FileCopyrightText: 2013-2014, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/book.h"

#define STYLE \
    "<style>" \
        "body {" \
            "background-color: #ffffff;" \
            "color: #000000;" \
        "}" \
        \
        "h2 {" \
            "font-family: \"Times New Roman\", Verdana, Arial, Helvetica, sans-serif;" \
        "}" \
        \
        ".variable {" \
            "font-weight: bold;" \
            "font-size: 10pt;" \
        "}" \
        \
        ".unit {" \
            "font-size: 8pt;" \
            "font-style: italic;" \
        "}" \
        \
        ".caption {" \
            "font-style: italic;" \
            "font-size: 10pt;" \
        \
        "}" \
        \
        "a:link {" \
            "text-decoration: none;" \
        "}" \
        \
        ".page-link a:link {" \
            "color: #555;" \
            "font-family: Verdana, Arial, Helvetica, sans-serif;" \
        "}" \
        \
        ".formula a:link {" \
            "color: SteelBlue;" \
            "font-size: 15pt;" \
            "font-family: \"Times New Roman\", Verdana, Arial, Helvetica, sans-serif;" \
        "}" \
    "</style>"

#define BEGIN QString("<html><head>" STYLE "</head><body>")+
#define SECTION(s) "<h3>"+(s)+"</h3>"
#define BR "<br />"
#define INDEX_LINK "<span class=\"page-link\"><a href=\"index\">"+Book::tr("Index")+"</a></span>"
#define LINK(link,text) "<span class=\"page-link\"><a href=\""#link"\">"+text+"</a></span>"
#define TITLE(s) "<h1>"+(s)+"</h1><div>"
#define SUBTITLE(s) "<h2>"#s+"</h2><div>"
// Clickable formula without a unit block.
#define FORMULA(link,text) "<span class=\"formula\"><a href=\"formula:"#link"\">"#text"</a></span><br />"
// Clickable formula with the same unit text used for editor insertion and display.
#define FORMULA_UNIT(link,text,unit) "<span class=\"formula\"><a href=\"formula:"#link" ["#unit"]\">"#text"</a></span> <span class=\"unit\">"#unit"</span><br />"
// Clickable formula with editor unit text displayed "as" separate HTML unit markup.
#define FORMULA_UNIT_AS(link,text,unit,displayUnit) "<span class=\"formula\"><a href=\"formula:"#link" ["#unit"]\">"#text"</a></span> <span class=\"unit\">"#displayUnit"</span><br />"
#define TABLE "<table><tr>"
#define ROW "</tr><tr>"
#define _TABLE "</div></tr></table>"
#define VARIABLE(s) "<td class=\"variable\">"#s"</td>"
#define CAPTION(s) "<td class=\"caption\">"+(s)+"</td>"
#define END "</body></html>"

namespace {

const auto makeIndexPage = []() -> QString
{
    return
        BEGIN
        TITLE(Book::tr("Index"))
        SECTION(Book::tr("Algebra"))
        LINK(algebra/quadratic-equation, Book::tr("Quadratic Equation")) BR
        LINK(algebra/log-base-conversion, Book::tr("Logarithmic Base Conversion")) BR
        SECTION(Book::tr("Geometry"))
        LINK(geometry/circle, Book::tr("Circle")) BR
        LINK(geometry/sector, Book::tr("Sector")) BR
        LINK(geometry/sphere, Book::tr("Sphere")) BR
        LINK(geometry/cube, Book::tr("Cube")) BR
        LINK(geometry/cone, Book::tr("Cone")) BR
        LINK(geometry/cylinder, Book::tr("Cylinder")) BR
        SECTION(Book::tr("Electronics"))
        LINK(electronics/ohmslaw, Book::tr("Ohm's Law")) BR
        LINK(electronics/power, Book::tr("Power")) BR
        LINK(electronics/reactance, Book::tr("Reactance")) BR
        LINK(electronics/resonance, Book::tr("Resonance")) BR
        SECTION(Book::tr("Radio Frequency"))
        LINK(rf/antennas, Book::tr("Antennas")) BR
        LINK(rf/impedance, Book::tr("Characteristic Impedance (coax)")) BR
        LINK(rf/propagation, Book::tr("Velocity of Propagation (coax)")) BR
        LINK(rf/swr, Book::tr("Standing Wave Ratio & Return Loss")) BR
        LINK(rf/wavelength, Book::tr("Free Space Wavelength")) BR
        END;
};

const auto makeAlgebraQuadraticEquationPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Quadratic Equation"))
        SUBTITLE(a&middot;x<sup>2</sup> + b&middot;x + c = 0)
        FORMULA(x1 = (-b + sqrt(b%5e2 - 4*a*c))/(2*a), x<sub>1</sub> = (-b + &radic;(b<sup>2</sup> - 4&middot;a&middot;c)) / (2&middot;a))
        FORMULA(x2 = (-b - sqrt(b%5e2 - 4*a*c))/(2*a), x<sub>2</sub> = (-b - &radic;(b<sup>2</sup> - 4&middot;a&middot;c)) / (2&middot;a))
        END;
};

const auto makeAlgebraLogBaseConversionPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Logarithmic Base Conversion"))
        FORMULA(y = log(x) / log(a), log<sub>a</sub>x = log(x) / log(a))
        END;
};

const auto makeElectronicsOhmsLawPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Ohm's Law"))
        FORMULA_UNIT(R = V / I, R = V / I, &Omega;)
        FORMULA_UNIT(V = I * R, V = I&middot;R, V)
        FORMULA_UNIT(I = V / R, I = V / R, A)
        TABLE
        VARIABLE(R) CAPTION(Book::tr("resistance")) ROW
        VARIABLE(V) CAPTION(Book::tr("voltage")) ROW
        VARIABLE(I) CAPTION(Book::tr("current")) ROW
        _TABLE
        END;
};

const auto makeElectronicsPowerPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Power"))
        FORMULA_UNIT(P = I * V, P = I&middot;V, W)
        FORMULA_UNIT(P = I%5e2 * R, P = I<sup>2</sup>&middot;R, W)
        FORMULA_UNIT(P = V%5e2 / R, P = V<sup>2</sup> / R, W)
        FORMULA_UNIT(I = P / V, I = P / V, A)
        FORMULA_UNIT(I = sqrt(P / R), I = &radic;(P / R), A)
        FORMULA_UNIT(V = P / I, V = P / I, V)
        FORMULA_UNIT(V = sqrt(P * R), V = &radic;(P&middot;R), V)
        FORMULA_UNIT(R = V%5e2 / P, R = V<sup>2</sup> / P, &Omega;)
        FORMULA_UNIT(R = P / I%5e2, R = P / I<sup>2</sup>, &Omega;)
        TABLE
        VARIABLE(P) CAPTION(Book::tr("power")) ROW
        VARIABLE(I) CAPTION(Book::tr("current")) ROW
        VARIABLE(V) CAPTION(Book::tr("voltage")) ROW
        VARIABLE(R) CAPTION(Book::tr("resistance")) ROW
        _TABLE
        END;
};

const auto makeElectronicsReactancePage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Reactance"))
        FORMULA_UNIT(Xl = 2 * π * f * L, X<sub>L</sub> = 2&middot;π&middot;f&middot;L, &Omega;)
        FORMULA_UNIT(Xc = 1 / (2 * π * f * C), X<sub>C</sub> = 1 / (2&middot;π&middot;f&middot;C), &Omega;)
        FORMULA_UNIT(L = Xl / (2 * π * f), L = X<sub>L</sub> / (2&middot;π&middot;f), H)
        FORMULA_UNIT(C = 1 / (2 * π * f * Xc), C = 1 / (2&middot;π&middot;f&middot;X<sub>C</sub>), F)
        FORMULA_UNIT(f = Xl / (2 * π * L), f = X<sub>L</sub> / (2&middot;π&middot;L), Hz)
        FORMULA_UNIT(f = 1 / (Xc * 2 * π * C), f = 1 / (X<sub>C</sub>&middot;2&middot;π&middot;C), Hz)
        TABLE
        VARIABLE(X<sub>L</sub>) CAPTION(Book::tr("inductive reactance")) ROW
        VARIABLE(X<sub>C</sub>) CAPTION(Book::tr("capacitive reactance")) ROW
        VARIABLE(L) CAPTION(Book::tr("inductance")) ROW
        VARIABLE(C) CAPTION(Book::tr("capacitance")) ROW
        VARIABLE(f) CAPTION(Book::tr("frequency")) ROW
        _TABLE
        END;
};

const auto makeElectronicsResonancePage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Resonance"))
        FORMULA_UNIT(f = 1 / (2 * π * sqrt(L * C)), f = 1 / (2&middot;π&middot;&radic;(L&middot;C)), Hz)
        FORMULA_UNIT(L = 1 / (4 * π%5e2 * f%5e2 * C), L = 1 / (4&middot;π<sup>2</sup>&middot;f<sup>2</sup>&middot;C), H)
        FORMULA_UNIT(C = 1 / (4 * π%5e2 * f%5e2 * L), C = 1 / (4&middot;π<sup>2</sup>&middot;f<sup>2</sup>&middot;L), F)
        TABLE
        VARIABLE(f) CAPTION(Book::tr("resonance frequency")) ROW
        VARIABLE(L) CAPTION(Book::tr("inductance")) ROW
        VARIABLE(C) CAPTION(Book::tr("capacitance")) ROW
        _TABLE
        END;
};

const auto makeGeometryCirclePage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Circle"))
        FORMULA(A = π * r%5e2, A = π&middot;r<sup>2</sup>)
        FORMULA(A = π * (d/2)%5e2, A = π&middot;(d/2)<sup>2</sup>)
        FORMULA(P = 2 * π * r, P = 2&middot;π&middot;r)
        FORMULA(P = π * d, P = π&middot;d)
        TABLE
        VARIABLE(A) CAPTION(Book::tr("area")) ROW
        VARIABLE(P) CAPTION(Book::tr("perimeter")) ROW
        VARIABLE(d) CAPTION(Book::tr("diameter")) ROW
        VARIABLE(r) CAPTION(Book::tr("radius"))
        _TABLE
        END;
};

const auto makeGeometryConePage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Cone"))
        FORMULA(V = 1/3 * π * r%5e2 * h, V = (1/3)&middot;π&middot;r<sup>2</sup>&middot;h)
        FORMULA(A = π * r%5e2 + π * r * s, A = π&middot;r<sup>2</sup> + π&middot;r&middot;s)
        FORMULA(A = π * r%5e2 + π * r * sqrt(r%5e2+h%5e2), A = π&middot;r<sup>2</sup> + π&middot;r&middot;&radic;(r<sup>2</sup>+h<sup>2</sup>))
        TABLE
        VARIABLE(V) CAPTION(Book::tr("volume")) ROW
        VARIABLE(A) CAPTION(Book::tr("surface area")) ROW
        VARIABLE(r) CAPTION(Book::tr("radius")) ROW
        VARIABLE(h) CAPTION(Book::tr("height")) ROW
        VARIABLE(s) CAPTION(Book::tr("slant height"))
        _TABLE
        END;
};

const auto makeGeometryCubePage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Cube"))
        FORMULA(V = l%5e3, V = l<sup>3</sup>)
        FORMULA(A = 6 * l%5e2, A = 6&middot;l<sup>2</sup>)
        FORMULA(Df = sqrt(2) * l, D<sub>f</sub> = &radic;2&middot;l)
        FORMULA(Ds = sqrt(3) * l, D<sub>s</sub> = &radic;3&middot;l)
        TABLE
        VARIABLE(V) CAPTION(Book::tr("volume")) ROW
        VARIABLE(A) CAPTION(Book::tr("surface area")) ROW
        VARIABLE(D<sub>f</sub>) CAPTION(Book::tr("face diagonal")) ROW
        VARIABLE(D<sub>s</sub>) CAPTION(Book::tr("space diagonal")) ROW
        VARIABLE(l) CAPTION(Book::tr("edge length"))
        _TABLE
        END;
};

const auto makeGeometryCylinderPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Cylinder"))
        FORMULA(V = π * r%5e2 * h, V = π&middot;r<sup>2</sup>&middot;h)
        FORMULA(A = (2 * π * r%5e2) + (2 * π * r * h), A = 2&middot;π&middot;r<sup>2</sup> + 2&middot;π&middot;r&middot;h)
        TABLE
        VARIABLE(V) CAPTION(Book::tr("volume")) ROW
        VARIABLE(A) CAPTION(Book::tr("area")) ROW
        VARIABLE(r) CAPTION(Book::tr("radius")) ROW
        VARIABLE(h) CAPTION(Book::tr("height"))
        _TABLE
        END;
};

const auto makeGeometrySectorPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Sector"))
        FORMULA(A = 1/2 * L * r, A = 1/2&middot;L&middot;r)
        FORMULA(A = theta/360 * π * r%5e2, A = &theta;/360&middot;π&middot;r<sup>2</sup>)
        FORMULA(L = theta/180 * π * r, L = &theta;/180&middot;π&middot;r)
        TABLE
        VARIABLE(A) CAPTION(Book::tr("area")) ROW
        VARIABLE(L) CAPTION(Book::tr("arc length")) ROW
        VARIABLE(r) CAPTION(Book::tr("radius")) ROW
        VARIABLE(&theta;) CAPTION(Book::tr("central angle (degrees)"))
        _TABLE
        END;
};

const auto makeGeometrySpherePage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Sphere"))
        FORMULA(A = 4 * π * r%5e2, A = 4&middot;π&middot;r<sup>2</sup>)
        FORMULA(A = π *d%5e2, A = π&middot;d<sup>2</sup>)
        FORMULA(V = 4/3 * π * r%5e3, V = 4/3&middot;π&middot;r<sup>3</sup>)
        FORMULA(V = 1/6 * π * d%5e3, V = 1/6&middot;π&middot;d<sup>3</sup>)
        TABLE
        VARIABLE(A) CAPTION(Book::tr("area")) ROW
        VARIABLE(V) CAPTION(Book::tr("volume")) ROW
        VARIABLE(r) CAPTION(Book::tr("radius")) ROW
        VARIABLE(d) CAPTION(Book::tr("diameter"))
        _TABLE
        END;
};

const auto makeRFAntennasPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Radio Frequency"))
        FORMULA_UNIT_AS(Pd = Pin / (4 * π * r%5e2), Pd = P<sub>in</sub> / (4&middot;π&middot;r<sup>2</sup>), W/m%5e2, W&middot;m<sup>-2</sup>)
        FORMULA_UNIT_AS(E = sqrt(Pd * 120 * π), E = &radic;(P<sub>d</sub>&middot;120&middot;π), V/m, V&middot;m<sup>-1</sup>)
        FORMULA_UNIT_AS(E = sqrt((Pin * 120 * π) / (4 * π * r%5e2)), E = &radic;[(P<sub>in</sub>&middot;120&middot;π) / (4&middot;π&middot;r<sup>2</sup>)], V/m, V&middot;m<sup>-1</sup>)
        FORMULA_UNIT_AS(E = sqrt(30 * Pin) / r, E = &radic;(30&middot;P<sub>in</sub>) / r, V/m, V&middot;m<sup>-1</sup>)
        FORMULA_UNIT_AS(Ed = E * sqrt(D), Ed = E&middot;&radic;D, V/m, V&middot;m<sup>-1</sup>)
        FORMULA_UNIT_AS(Edp = E * sqrt(1.64), Edp = E&middot;&radic;1.64, V/m, V&middot;m<sup>-1</sup>)
        END;
};

const auto makeRFImpedancePage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Characteristic Impedance (coax)"))
        FORMULA_UNIT(Zo = 59.959 / sqrt(er) * ln(d1/d2), Z<sub>0</sub> = 59.959 / &radic;e<sub>r</sub> &middot; ln(d<sub>1</sub>/d<sub>2</sub>), &Omega;)
        TABLE
        VARIABLE(e<sub>r) CAPTION(Book::tr("dielectric constant")) ROW
        VARIABLE(d<sub>1) CAPTION(Book::tr("outer conductor's inner diameter")) ROW
        VARIABLE(d<sub>2</sub>) CAPTION(Book::tr("inner conductor's outer diameter"))
        _TABLE
        END;
};

const auto makeRFPropagationPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Velocity of Propagation (coax)"))
        SUBTITLE(V<sub>p</sub> = c / &radic;e<sub>r</sub>)
        FORMULA_UNIT_AS(Vp = 3e8 / sqrt(er), V<sub>p</sub> = 3e8 / &radic;e<sub>r</sub>, m/s, m&middot;s<sup>-1</sup>)
        FORMULA_UNIT_AS(Vp = 299792458 / sqrt(er), V<sub>p</sub> = 299792458 / &radic;e<sub>r</sub>, m/s, m&middot;s<sup>-1</sup>)
        FORMULA_UNIT_AS(Vp = 983571056.43045 / sqrt(er), V<sub>p</sub> = 983571056.43045 / &radic;e<sub>r</sub>, ft/s, ft&middot;s<sup>-1</sup>)
        TABLE
        VARIABLE(c) CAPTION(Book::tr("speed of light")) ROW
        VARIABLE(e) CAPTION(Book::tr("dielectric constant")) ROW
        _TABLE
        END;
};

const auto makeRFSWRPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Standing Wave Ratio & Return Loss"))
        FORMULA(SWR = (1+r) / (1-r), SWR = (1 + &rho;) / (1 - &rho;))
        FORMULA(r = (SWR-1) / (SWR+1), &rho; = (SWR - 1) / (SWR + 1))
        FORMULA(r = (Z-1) / (Z+1), &rho; = (Z - 1) / (Z + 1))
        FORMULA_UNIT(Z = (1+r) / (1-r), Z = (1 + &rho;) / (1 - &rho;), &Omega;)
        FORMULA(RL = -20 * log(r), R<sub>L</sub> = -20&middot;log(&rho;))
        FORMULA(RL = -20 * log((SWR-1)/(SWR+1)), R<sub>L</sub> = -20&middot;log[(SWR - 1) / (SWR + 1)])
        FORMULA_UNIT(Pr = Pin * r%5e2, P<sub>r</sub> = P<sub>in</sub>&middot;&rho;<sup>2</sup>, W)
        FORMULA_UNIT(Pt = Pin * (1-r%5e2), P<sub>t</sub> = P<sub>in</sub>&middot;(1-&rho;<sup>2</sup>), W)
        TABLE
        VARIABLE(P<sub>in</sub>) CAPTION(Book::tr("input power") + " (W)") ROW
        VARIABLE(P<sub>r</sub>) CAPTION(Book::tr("reflected power") + " (W)") ROW
        VARIABLE(P<sub>t</sub>) CAPTION(Book::tr("transmitted power") + " (W)") ROW
        VARIABLE(R<sub>L</sub>) CAPTION(Book::tr("return loss") + " (dB)") ROW
        VARIABLE(&rho;) CAPTION(Book::tr("reflection coefficient")) ROW
        VARIABLE(SWR) CAPTION(Book::tr("standing wave ratio")) ROW
        VARIABLE(Z) CAPTION(Book::tr("normalized impedance")) ROW
        _TABLE
        END;
};

const auto makeRFWavelengthPage = []() -> QString
{
    return
        BEGIN
        INDEX_LINK
        TITLE(Book::tr("Free Space Wavelength"))
        SUBTITLE(&lambda;<sub>0</sub> = c / f)
        FORMULA_UNIT(L = 3e8 / f, L = 3&times;10<sup>8</sup> / f, m)
        FORMULA_UNIT(L = 299792458 / f, L = 299792458 / f, m)
        FORMULA_UNIT(L = 983571056.43045 / f, L = 983571056.43045 / f, ft)
        TABLE
        VARIABLE(f) CAPTION(Book::tr("frequency (Hz)")) ROW
        _TABLE
        END;
};

} // namespace

void Book::createPages()
{
    addPage("index", +makeIndexPage);
    addPage("algebra/quadratic-equation", +makeAlgebraQuadraticEquationPage);
    addPage("algebra/log-base-conversion", +makeAlgebraLogBaseConversionPage);
    addPage("electronics/ohmslaw", +makeElectronicsOhmsLawPage);
    addPage("electronics/power", +makeElectronicsPowerPage);
    addPage("electronics/reactance", +makeElectronicsReactancePage);
    addPage("electronics/resonance", +makeElectronicsResonancePage);
    addPage("geometry/circle", +makeGeometryCirclePage);
    addPage("geometry/cone", +makeGeometryConePage);
    addPage("geometry/cube", +makeGeometryCubePage);
    addPage("geometry/cylinder", +makeGeometryCylinderPage);
    addPage("geometry/sector", +makeGeometrySectorPage);
    addPage("geometry/sphere", +makeGeometrySpherePage);
    addPage("rf/antennas", +makeRFAntennasPage);
    addPage("rf/impedance", +makeRFImpedancePage);
    addPage("rf/propagation", +makeRFPropagationPage);
    addPage("rf/swr", +makeRFSWRPage);
    addPage("rf/wavelength", +makeRFWavelengthPage);
}
