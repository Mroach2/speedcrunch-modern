// SPDX-FileCopyrightText: 2016-2017, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "math/quantity.h"
#include "math/rational.h"
#include "core/units.h"
#include "tests/testcommon.h"

#include <QtCore/QCoreApplication>
#include <QString>

#include <cstdlib>
#include <string>
#include <iostream>

using namespace std;

typedef Quantity::Format Format;

#define CHECK(x,y) check_value(__FILE__,__LINE__,#x,x,y)
#define CHECK_FORMAT(f,x,y) check_format(__FILE__,__LINE__,#x,x,f,y)
#define CHECK_PRECISE(x,y) check_precise(__FILE__,__LINE__,#x,x,y)
#define CHECK_KNOWN_ISSUE(x,y,n) check_value(__FILE__,__LINE__,#x,x,y,n)
#define CHECK_STRING(x,y) {++dmath_total_tests; DisplayErrorOnMismatch(__FILE__,__LINE__,#x,x,y,dmath_failed_tests,dmath_new_failed_tests);}

static int dmath_total_tests  = 0;
static int dmath_failed_tests = 0;
static int dmath_new_failed_tests = 0;

static void check_value(const char* file, int line, const char* msg, const Quantity& q, const char* expected, int issue = 0)
{
    ++dmath_total_tests;
    string result = DMath::format(q, Format::Fixed()).toStdString();
    DisplayErrorOnMismatch(file, line, msg, result, expected, dmath_failed_tests, dmath_new_failed_tests, issue);
}

static void check_format(const char* file, int line, const char* msg, const Quantity& q, Format format, const char* expected)
{
    ++dmath_total_tests;
    string result = DMath::format(q, format).toStdString();
    DisplayErrorOnMismatch(file, line, msg, result, expected, dmath_failed_tests, dmath_new_failed_tests, 0);
}

//static void check_precise(const char* file, int line, const char* msg, const Quantity& q, const char* expected)
//{
//    ++dmath_total_tests;
//    string result = DMath::format(q, Format::Fixed() + Format::Precision(50)).toStdString();
//    DisplayErrorOnMismatch(file, line, msg, result, expected, dmath_failed_tests, dmath_new_failed_tests, 0);
//}

void test_rational()
{
    CHECK_STRING(HMath::format(Rational(123,456).toHNumber()).toStdString(), "0.26973684210526315789");
    CHECK_STRING(Rational(22./7).toString().toStdString(), "22/7");
    CHECK_STRING(Rational(-12345./96457).toString().toStdString(), "-12345/96457");
    CHECK_STRING(Rational(HNumber("-1234")/HNumber("7895")).toString().toStdString(), "-1234/7895");
    CHECK_STRING(Rational(HNumber("-1235000")/HNumber("78950000")).toString().toStdString(), "-247/15790");
    CHECK_STRING(Rational(HNumber("1")/HNumber("7")).toString().toStdString(), "1/7");
    CHECK_STRING(Rational(0.).toString().toStdString(), "0");
    CHECK_STRING(Rational(HNumber(0)).toString().toStdString(), "0");

    Rational approx;
    CHECK_STRING((Rational::approximate(HNumber("1") / HNumber("2"), 1000000, HNumber("1e-20"), &approx)
            ? approx.toString()
            : QString("invalid")).toStdString(), "1/2");
}

void test_create()
{
    CHECK(Units::metre(), "1 metre");
    CHECK(Quantity(CNumber("123.45+654j")), "123.45+654j");
    CHECK(Quantity(HNumber("123.45")), "123.45");
}

void test_basic()
{
    CHECK(Units::metre(), "1 metre");
    CHECK(Quantity(5) * Units::metre(), "5 metre");
    CHECK(Units::candela() + Units::second(), "NaN");
    CHECK(Quantity(3)*Units::mole() - Quantity(HNumber("2.5"))*Units::mole(), "500 mmol");
    CHECK(Units::kilogram()/Units::second(), "1 kilogram·second⁻¹");
    CHECK(Units::metre()*Units::metre(), "1 metre²");
    CHECK(-Quantity(5)*Units::metre(), "-5 metre");
    CNumber foot = CNumber("0.3");
    Quantity a(Quantity(123)*Units::metre());
    a.setDisplayUnit(foot, "foot");
    CHECK(a, "410 foot");
    CHECK(a*Units::second(), "123 metre·second");       // Issue 615
    CHECK(a/Units::second(), "123 metre·second⁻¹");    //
    CHECK(a*HNumber(5), "2050 foot");                   //
    CHECK(a/HNumber(5), "82 foot");                     //

    CHECK(DMath::raise(Units::metre(), 0),"1");
    CHECK(DMath::raise(Units::metre(), Quantity(0)),"1");
    CHECK(DMath::raise(Units::metre(), 0) + DMath::raise(Units::second(), 0),"2");
    CHECK(DMath::raise(Units::metre(), Quantity(0)) + DMath::raise(Units::second(), Quantity(0)),"2");
}

void test_functions()
{
    Quantity tmp1;

    CHECK(DMath::abs(Quantity(CNumber("3+4j"))*Units::metre()), "5 metre");
    CHECK(DMath::round(CNumber("1.234"), 1), "1.2");
    CHECK(DMath::round(Quantity(CNumber("1.234"))*Units::joule(), 0), "NaN");

    DMath::complexMode = true;
    tmp1 = Quantity(CNumber("3+4j"))*Units::ohm();
    CHECK(DMath::conj(Quantity(CNumber("3"))*Units::ohm()), "3 ohm");
    CHECK(DMath::conj(Quantity(CNumber("3+4j"))*Units::ohm()), "(3-4j) ohm");
    DMath::complexMode = false;
    CHECK(DMath::conj(Quantity(CNumber("3"))*Units::ohm()), "3 ohm");
    // for a complex number declared in complex mode, ensure that if we've switched to real mode
    // and call this function that the imaginary part is stripped. 
    CHECK(DMath::conj(tmp1), "3 ohm"); 
    DMath::complexMode = true;

    CHECK(DMath::trunc(CNumber("1.274"), 1), "1.2");
    CHECK(DMath::trunc(Quantity(CNumber("1.234"))*Units::joule(), 0), "NaN");

    CHECK(DMath::real(Quantity(CNumber("3+4j"))*Units::metre()), "3 metre");
    CHECK(DMath::imag(Quantity(CNumber("3+4j"))*Units::metre()), "4 metre");

    CHECK(DMath::sqrt(Quantity(CNumber("36"))*Units::second()), "6 second^(1/2)");
    CHECK(DMath::cbrt(Quantity(CNumber("125"))*Units::second()), "5 second^(1/3)");

    CHECK(DMath::raise(Quantity(CNumber("2")), DMath::pi()), "8.82497782707628762386");
    CHECK(DMath::raise(Quantity(CNumber("2"))*Units::ampere(), DMath::pi()), "NaN");
    CHECK(DMath::raise(Quantity(CNumber("-2"))*Units::ampere(), Quantity(CNumber("1.5"))), "NaN");

    DMath::complexMode = false;
    CHECK(DMath::raise(Quantity(CNumber("-2"))*Units::ampere(), Quantity(CNumber("0.6"))), "-1.51571656651039808235 ampere^(3/5)");
    CHECK(DMath::raise(Quantity(CNumber("-8"))*Units::ampere(), Quantity(CNumber("2") / CNumber("3"))), "4 ampere^(2/3)");
    DMath::complexMode = true;
    CHECK(DMath::raise(Quantity(CNumber("-2"))*Units::ampere(), Quantity(CNumber("0.6"))), "(-0.46838217770735830743+1.44153211743623063689j) ampere^(3/5)");


    // this should do it for all wrapped functions that do not accept dimensional arguments...
    CHECK(DMath::sin(Quantity(DMath::pi())), "0");
    CHECK(DMath::sin(Units::metre()), "NaN");
}

void test_format()
{
    Quantity a = Quantity(CNumber("12365234.45647"));
    CHECK_FORMAT(Format::Binary() + Format::Fixed() + Format::Precision(10), a, "0b101111001010110110110010.0111010011");


    a *= Units::coulomb();
    CHECK_FORMAT(Format::Binary() + Format::Fixed() + Format::Precision(10), a, "0b1100.0101110110 MC");
}


int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    dmath_total_tests = 0;
    dmath_failed_tests = 0;

    test_rational();

    test_create();
    test_basic();
    test_functions();
    test_format();

    cerr.flush();
    if (!dmath_failed_tests)
        return 0;
    cout << dmath_total_tests  << " total, "
         << dmath_failed_tests << " failed, "
         << dmath_new_failed_tests << " new" << endl;
    return dmath_new_failed_tests;
}
