// This file is part of the SpeedCrunch project
// Copyright (C) 2004-2006 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2007-2009, 2013, 2016, 2026 @heldercorreia
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

#include "core/evaluator.h"
#include "core/numberformatter.h"
#include "core/settings.h"
#include "core/session.h"
#include "core/unicodechars.h"
#include "gui/editorutils.h"
#include "gui/displayformatutils.h"
#include "gui/functiontooltiputils.h"
#include "gui/resultdisplay.h"
#include "gui/resultlineformatutils.h"
#include "gui/simplifiedexpressionutils.h"
#include "math/cmath.h"
#include "core/mathdsl.h"
#include "core/units.h"
#include "tests/testcommon.h"

#include <QApplication>
#include <QMouseEvent>
#include <QTextBlock>

#include <string>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace std;

typedef Quantity::Format Format;

static Evaluator* eval = 0;
static int eval_total_tests = 0;
static int eval_failed_tests = 0;
static int eval_new_failed_tests = 0;
static const QString space(MathDsl::AddWrap);
static const QString slash = space + QString(MathDsl::DivOp) + space;

#define CHECK_AUTOFIX(s,p) checkAutoFix(__FILE__,__LINE__,#s,s,p)
#define CHECK_DIV_BY_ZERO(s) checkDivisionByZero(__FILE__,__LINE__,#s,s)
#define CHECK_EVAL(x,y) checkEval(__FILE__,__LINE__,#x,x,y)
#define CHECK_EVAL_FORMAT(x,y) checkEval(__FILE__,__LINE__,#x,x,y,0,false,true)
#define CHECK_EVAL_FORMAT_HAS_SLASH(x) checkEvalFormatSlash(__FILE__, __LINE__, #x, x, true)
#define CHECK_EVAL_FORMAT_NO_SLASH(x) checkEvalFormatSlash(__FILE__, __LINE__, #x, x, false)
#define CHECK_EVAL_FORMAT_MEDIUM_SPACED_SLASH(x) checkEvalFormatMediumSpacedSlash(__FILE__, __LINE__, #x, x)
#define CHECK_EVAL_FORMAT_EXACT(x,y) checkEvalFormatExact(__FILE__, __LINE__, #x, x, y)
#define CHECK_EVAL_KNOWN_ISSUE(x,y,n) checkEval(__FILE__,__LINE__,#x,x,y,n)
#define CHECK_EVAL_PRECISE(x,y) checkEvalPrecise(__FILE__,__LINE__,#x,x,y)
#define CHECK_EVAL_FAIL(x) checkEval(__FILE__,__LINE__,#x,x,"",0,true)
#define CHECK_EVAL_FORMAT_FAIL(x) checkEval(__FILE__,__LINE__,#x,x,"",0,true,true)
#define CHECK_INTERPRETED(x,y) checkInterpreted(__FILE__,__LINE__,#x,x,y)
#define CHECK_DISPLAY_INTERPRETED(x,y) checkDisplayInterpreted(__FILE__,__LINE__,#x,x,y)
#define CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(x,y) checkDisplaySimplifiedInterpreted(__FILE__,__LINE__,#x,x,y)
#define CHECK_USERFUNC_SET(x) checkEval(__FILE__,__LINE__,#x,x,"NaN")
#define CHECK_USERFUNC_SET_FAIL(x) checkEval(__FILE__,__LINE__,#x,x,"",0,true)
#define CHECK_USERFUNC_DESC(n,d) checkUserFunctionDescription(__FILE__,__LINE__,#n,n,d)
#define CHECK_USERFUNC_INTERPRETED(n,e) checkUserFunctionInterpreted(__FILE__,__LINE__,#n,n,e)
#define CHECK_USERVAR_DESC(n,d) checkUserVariableDescription(__FILE__,__LINE__,#n,n,d)
#define CHECK_USERUNIT_DESC(n,d) checkUserUnitDescription(__FILE__,__LINE__,#n,n,d)

static void checkAutoFix(const char* file, int line, const char* msg, const char* expr, const char* fixed)
{
    ++eval_total_tests;

    string r = eval->autoFix(QString(expr)).toStdString();
    DisplayErrorOnMismatch(file, line, msg, r, fixed, eval_failed_tests, eval_new_failed_tests);
}

static void checkDivisionByZero(const char* file, int line, const char* msg, const QString& expr)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    if (eval->error().isEmpty()) {
        ++eval_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << endl
             << "\tError: " << "division by zero not caught" << endl;
    }
}

static void checkEval(const char* file, int line, const char* msg, const QString& expr,
                      const char* expected, int issue = 0, bool shouldFail = false, bool format = false)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    if (!eval->error().isEmpty()) {
        if (!shouldFail) {
            ++eval_failed_tests;
            cerr << file << "[" << line << "]\t" << msg;
            if (issue)
                cerr << "\t[ISSUE " << issue << "]";
            else {
                cerr << "\t[NEW]";
                ++eval_new_failed_tests;
            }
            cerr << endl;
            cerr << "\tError: " << eval->error().toUtf8().constData() << endl;
        }
    } else {
        QString result = (format ? NumberFormatter::format(rn) : DMath::format(rn, Format::Fixed()));
        result.replace(QString::fromUtf8("−"), "-");
        QString expectedText = QString::fromUtf8(expected);
        if (shouldFail || result != expectedText) {
            ++eval_failed_tests;
            cerr << file << "[" << line << "]\t" << msg;
            if (issue)
                cerr << "\t[ISSUE " << issue << "]";
            else {
                cerr << "\t[NEW]";
                ++eval_new_failed_tests;
            }
            cerr << endl;
            cerr << "\tResult   : " << result.toUtf8().constData() << endl
                 << "\tExpected : "
                 << (shouldFail ? "should fail" : expectedText.toUtf8().constData()) << endl;
        }
    }
}

static void checkEvalFormatSlash(const char* file, int line, const char* msg,
                                 const QString& expr, bool shouldHaveSlash)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    QString result = NumberFormatter::format(rn);
    result.replace(QString::fromUtf8("−"), "-");
    const bool hasSlash = result.contains('/');
    if (hasSlash != shouldHaveSlash) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tResult      : " << result.toUtf8().constData() << endl
             << "\tExpected '/' : " << (shouldHaveSlash ? "yes" : "no") << endl;
    }
}

static void checkEvalFormatMediumSpacedSlash(const char* file, int line, const char* msg,
                                             const QString& expr)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    QString result = NumberFormatter::format(rn);
    result.replace(QString::fromUtf8("−"), "-");
    const QString mediumSlash = slash;
    if (!result.contains(mediumSlash)) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tResult                 : " << result.toUtf8().constData() << endl
             << "\tExpected medium slash  : " << mediumSlash.toUtf8().constData() << endl;
    }
}

static void checkEvalFormatExact(const char* file, int line, const char* msg,
                                 const QString& expr, const QString& expected)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    QString result = NumberFormatter::format(rn);
    result.replace(QString::fromUtf8("−"), "-");
    if (result != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tResult   : " << result.toUtf8().constData() << endl
             << "\tExpected : " << expected.toUtf8().constData() << endl;
    }
}

static void checkEvalPrecise(const char* file, int line, const char* msg, const QString& expr, const char* expected)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    Quantity rn = eval->evalUpdateAns();

    // We compare up to 50 decimals, not exact number because it's often difficult
    // to represent the result as an irrational number, e.g. PI.
    string result = DMath::format(rn, Format::Fixed() + Format::Precision(50)).toStdString();
    DisplayErrorOnMismatch(file, line, msg, result, expected, eval_failed_tests, eval_new_failed_tests, 0);
}

static void checkInterpreted(const char* file, int line, const char* msg, const QString& expr,
                             const char* expected)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    const QString interpreted = eval->interpretedExpression();
    if (interpreted != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tInterpreted: " << interpreted.toUtf8().constData() << endl
             << "\tExpected   : " << expected << endl;
    }
}

static string toCodePointList(const QString& text)
{
    const QVector<uint> cps = text.toUcs4();
    std::ostringstream os;
    for (int i = 0; i < cps.size(); ++i) {
        if (i > 0)
            os << " ";
        os << "U+"
           << std::uppercase << std::hex << std::setfill('0') << std::setw(4)
           << cps.at(i);
    }
    return os.str();
}

static bool hasSpacedOperatorsInsideUnitBrackets(const QString& text)
{
    int pos = 0;
    while (true) {
        const int open = text.indexOf(QLatin1Char('['), pos);
        if (open < 0)
            return false;
        const int close = text.indexOf(QLatin1Char(']'), open + 1);
        if (close < 0)
            return false;

        const QString segment = text.mid(open + 1, close - open - 1);
        if (segment.contains(QStringLiteral(" ·"))
            || segment.contains(QStringLiteral("· "))
            || segment.contains(QStringLiteral(" ×"))
            || segment.contains(QStringLiteral("× "))
            || segment.contains(QStringLiteral(" /"))
            || segment.contains(QStringLiteral("/ "))) {
            return true;
        }
        pos = close + 1;
    }
}

static void checkDisplayInterpreted(const char* file, int line, const char* msg,
                                    const QString& expr, const QString& expected)
{
    ++eval_total_tests;

    eval->setExpression(eval->autoFix(expr));
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    const QString interpreted = eval->interpretedExpression();
    const QString displayed = Evaluator::formatInterpretedExpressionForDisplay(interpreted);
    if (displayed != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tDisplayed : " << displayed.toUtf8().constData() << endl
             << "\tExpected  : " << expected.toUtf8().constData() << endl
             << "\tDisplayed code points: " << toCodePointList(displayed) << endl
             << "\tExpected  code points: " << toCodePointList(expected) << endl;
    }
}

class TestableResultDisplay : public ResultDisplay
{
public:
    explicit TestableResultDisplay(QWidget* parent = 0)
        : ResultDisplay(parent)
    {
    }

    using ResultDisplay::blockRangeForHistoryIndex;
    using ResultDisplay::historyIndexAtPosition;
    using ResultDisplay::markHistoryBlockIndexCacheDirty;
    using ResultDisplay::mouseDoubleClickEvent;

    QPoint pointForBlockStart(int blockNumber) const
    {
        QTextBlock block = document()->findBlockByNumber(blockNumber);
        if (!block.isValid())
            return QPoint(-1, -1);

        QTextCursor cursor(block);
        const QRect rect = cursorRect(cursor);
        if (!rect.isValid())
            return QPoint(-1, -1);

        return rect.center();
    }
};

void test_result_display_history_mapping_after_multiple_lines_toggle_with_comment_only_entry()
{
    Settings* settings = Settings::instance();
    Session* session = const_cast<Session*>(eval->session());

    const bool oldMultipleResultLinesEnabled = settings->multipleResultLinesEnabled;
    const bool oldSecondaryResultEnabled = settings->secondaryResultEnabled;
    const char oldAlternativeResultFormat = settings->alternativeResultFormat;
    const int oldSecondaryResultPrecision = settings->secondaryResultPrecision;
    const bool oldComplexNumbers = settings->complexNumbers;
    const bool oldSecondaryComplexNumbers = settings->secondaryComplexNumbers;
    const char oldSecondaryResultFormatComplex = settings->secondaryResultFormatComplex;

    settings->multipleResultLinesEnabled = true;
    settings->secondaryResultEnabled = true;
    settings->alternativeResultFormat = 'r';
    settings->secondaryResultPrecision = -1;
    settings->complexNumbers = false;
    settings->secondaryComplexNumbers = false;
    settings->secondaryResultFormatComplex = 'c';

    session->clearHistory();
    session->addHistoryEntry(HistoryEntry(QStringLiteral("2/3"), Quantity(2) / Quantity(3)));
    session->addHistoryEntry(HistoryEntry(QStringLiteral("? comment-only history entry"), CMath::nan()));
    session->addHistoryEntry(HistoryEntry(QStringLiteral("1+1"), Quantity(2)));

    auto checkMapping = [](const char* file, int line, const char* label, TestableResultDisplay& display, int expectedHistorySize) {
        ++eval_total_tests;
        if (expectedHistorySize <= 0) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << file << "[" << line << "]\t" << label << "\t[NEW]" << endl
                 << "\tError: expectedHistorySize <= 0" << endl;
            return;
        }

        for (int historyIndex = 0; historyIndex < expectedHistorySize; ++historyIndex) {
            int startBlock = -1;
            int endBlock = -1;
            const bool hasRange = display.blockRangeForHistoryIndex(historyIndex, startBlock, endBlock);
            if (!hasRange || startBlock < 0 || endBlock < startBlock) {
                ++eval_failed_tests;
                ++eval_new_failed_tests;
                cerr << file << "[" << line << "]\t" << label << "\t[NEW]" << endl
                     << "\tInvalid range for history index " << historyIndex << endl;
                continue;
            }

            const QPoint probe = display.pointForBlockStart(startBlock);
            const int mappedIndex = display.historyIndexAtPosition(probe);
            if (mappedIndex != historyIndex) {
                ++eval_failed_tests;
                ++eval_new_failed_tests;
                cerr << file << "[" << line << "]\t" << label << "\t[NEW]" << endl
                     << "\tHistory index: " << historyIndex << endl
                     << "\tMapped index : " << mappedIndex << endl
                     << "\tBlock range  : [" << startBlock << ", " << endBlock << "]" << endl;
            }
        }
    };

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();
    const int historySize = session->historySize();

    checkMapping(__FILE__, __LINE__, "result display mapping before toggle", display, historySize);

    settings->multipleResultLinesEnabled = false;
    display.markHistoryBlockIndexCacheDirty();
    checkMapping(__FILE__, __LINE__, "result display mapping after toggle with stale blocks", display, historySize);

    settings->multipleResultLinesEnabled = oldMultipleResultLinesEnabled;
    settings->secondaryResultEnabled = oldSecondaryResultEnabled;
    settings->alternativeResultFormat = oldAlternativeResultFormat;
    settings->secondaryResultPrecision = oldSecondaryResultPrecision;
    settings->complexNumbers = oldComplexNumbers;
    settings->secondaryComplexNumbers = oldSecondaryComplexNumbers;
    settings->secondaryResultFormatComplex = oldSecondaryResultFormatComplex;
}

static void checkDisplaySimplifiedInterpreted(const char* file, int line, const char* msg,
                                              const QString& expr, const QString& expected)
{
    ++eval_total_tests;

    eval->setExpression(eval->autoFix(expr));
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    const QString interpreted = eval->interpretedExpression();
    const QString displayed = Evaluator::formatInterpretedExpressionSimplifiedForDisplay(interpreted);
    if (displayed != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tDisplayed : " << displayed.toUtf8().constData() << endl
             << "\tExpected  : " << expected.toUtf8().constData() << endl
             << "\tDisplayed code points: " << toCodePointList(displayed) << endl
             << "\tExpected  code points: " << toCodePointList(expected) << endl;
    }
}

static void checkSuppressSimplifiedExpressionLine(const char* file, int line, const char* msg,
                                                  const QString& expr, bool expectedSuppressed)
{
    ++eval_total_tests;

    eval->setExpression(expr);
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    const QString interpreted =
        Evaluator::formatInterpretedExpressionForDisplay(eval->interpretedExpression());
    const QString simplified =
        Evaluator::formatInterpretedExpressionSimplifiedForDisplay(eval->interpretedExpression());
    const bool suppressed = SimplifiedExpressionUtils::shouldSuppressSimplifiedExpressionLine(
        interpreted, simplified);
    if (suppressed != expectedSuppressed) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl
             << "\tInterpreted: " << interpreted.toUtf8().constData() << endl
             << "\tSimplified : " << simplified.toUtf8().constData() << endl
             << "\tSuppressed : " << (suppressed ? "true" : "false") << endl
             << "\tExpected   : " << (expectedSuppressed ? "true" : "false") << endl;
    }
}

static void checkUserFunctionDescription(const char* file, int line, const char* msg,
                                         const QString& functionName,
                                         const QString& expectedDescription)
{
    ++eval_total_tests;

    const auto userFunctions = eval->getUserFunctions();
    for (const auto& function : userFunctions) {
        if (function.name() == functionName) {
            if (function.description() != expectedDescription) {
                ++eval_failed_tests;
                ++eval_new_failed_tests;
                cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
                cerr << "\tDescription: " << function.description().toLatin1().constData() << endl
                     << "\tExpected   : " << expectedDescription.toLatin1().constData() << endl;
            }
            return;
        }
    }

    ++eval_failed_tests;
    ++eval_new_failed_tests;
    cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
    cerr << "\tError: user function not found: " << functionName.toLatin1().constData() << endl;
}

static void checkUserFunctionInterpreted(const char* file, int line, const char* msg,
                                         const QString& functionName,
                                         const QString& expectedInterpreted)
{
    ++eval_total_tests;

    const auto userFunctions = eval->getUserFunctions();
    for (const auto& function : userFunctions) {
        if (function.name() == functionName) {
            if (function.interpretedExpression() != expectedInterpreted) {
                ++eval_failed_tests;
                ++eval_new_failed_tests;
                cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
                cerr << "\tInterpreted: "
                     << function.interpretedExpression().toUtf8().constData() << endl
                     << "\tExpected   : " << expectedInterpreted.toUtf8().constData() << endl;
            }
            return;
        }
    }

    ++eval_failed_tests;
    ++eval_new_failed_tests;
    cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
    cerr << "\tError: user function not found: " << functionName.toLatin1().constData() << endl;
}

static void checkUserVariableDescription(const char* file, int line, const char* msg,
                                         const QString& variableName,
                                         const QString& expectedDescription)
{
    ++eval_total_tests;

    if (!eval->hasVariable(variableName)) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
        cerr << "\tError: user variable not found: " << variableName.toLatin1().constData() << endl;
        return;
    }

    const auto variable = eval->getVariable(variableName);
    if (variable.description() != expectedDescription) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
        cerr << "\tDescription: " << variable.description().toLatin1().constData() << endl
             << "\tExpected   : " << expectedDescription.toLatin1().constData() << endl;
    }
}

static void checkUserUnitDescription(const char* file, int line, const char* msg,
                                     const QString& unitName,
                                     const QString& expectedDescription)
{
    ++eval_total_tests;

    const UserUnit* unit = eval->getUserUnit(unitName);
    if (!unit) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
        cerr << "\tError: user unit not found: " << unitName.toLatin1().constData() << endl;
        return;
    }

    if (unit->description() != expectedDescription) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << file << "[" << line << "]\t" << msg << "\t[NEW]" << endl;
        cerr << "\tDescription: " << unit->description().toLatin1().constData() << endl
             << "\tExpected   : " << expectedDescription.toLatin1().constData() << endl;
    }
}

void test_constants()
{
    CHECK_EVAL("1", "1");
    CHECK_EVAL_FAIL("pi()");
    CHECK_EVAL_FAIL("e()");
    CHECK_EVAL_FAIL("ans()");
    CHECK_EVAL_FAIL("sqrt(pi())");
    CHECK_EVAL_FAIL("cos(pi()");

    // Subscript Unicode letters/digits are accepted in identifiers.
    CHECK_EVAL(QString::fromUtf8("foo₀=2"), "2");
    CHECK_EVAL(QString::fromUtf8("foo₀+1"), "3");
    CHECK_USERFUNC_SET(QString::fromUtf8("f₀(x)=x+1"));
    CHECK_EVAL(QString::fromUtf8("f₀(2)"), "3");
    CHECK_EVAL(QString::fromUtf8("xᵢ=4"), "4");
    CHECK_EVAL(QString::fromUtf8("xᵢ*3"), "12");
}

void test_exponentiation()
{
    CHECK_EVAL("2e10", "20000000000");
    CHECK_EVAL("2E10", "20000000000");
    CHECK_EVAL("2e-10", "0.0000000002");
    CHECK_EVAL("2E0xa", "20000000000");
    CHECK_EVAL("2e-0xa", "0.0000000002");
    CHECK_EVAL("0b10b10", "8");
    CHECK_EVAL("0b10B10", "8");
    CHECK_EVAL("0b10b-10", "0.5");
    CHECK_EVAL("0b10b0d2", "8");
    CHECK_EVAL("0b10b-0d2", "0.5");
    CHECK_EVAL("0o2o10", "33554432");
    CHECK_EVAL("0o2O10", "33554432");
    CHECK_EVAL("0o2C10", "33554432");
    CHECK_EVAL("0o2o-10", "0.00000011920928955078");
    CHECK_EVAL("0o2o0d8", "33554432");
    CHECK_EVAL("0o2o-0d8", "0.00000011920928955078");
    CHECK_EVAL("0x2h10", "36893488147419103232");
    CHECK_EVAL("0x2H10", "36893488147419103232");
    CHECK_EVAL("0x2h-10", "0.00000000000000000011");
    CHECK_EVAL("0x2h0d16", "36893488147419103232");
    CHECK_EVAL("0x2h-0d16", "0.00000000000000000011");
    CHECK_EVAL_FAIL("0x2o11");
    CHECK_EVAL_FAIL("0b2C11");
    CHECK_EVAL_FAIL("0o2b11");
}

void test_unary()
{
    CHECK_EVAL("-0", "0");
    CHECK_EVAL("-1", "-1");
    CHECK_EVAL("--1", "1");
    CHECK_EVAL("---1", "-1");
    CHECK_EVAL("----1", "1");
    CHECK_EVAL("-----1", "-1");
    CHECK_EVAL("------1", "1");

    CHECK_EVAL("-ABS(1)", "-1");
    CHECK_EVAL("-ABS(-2)", "-2");
    CHECK_EVAL("--ABS(-3)", "3");
    CHECK_EVAL("---ABS(-4)", "-4");

    // Operator ^ has higher precedence than unary minus.
    CHECK_EVAL("-1^0", "-1");
    CHECK_EVAL("-1^1", "-1");
    CHECK_EVAL("-1^2", "-1");
    CHECK_EVAL("-1^3", "-1");

    CHECK_EVAL("-2^0", "-1");
    CHECK_EVAL("-2^1", "-2");
    CHECK_EVAL("-2^2", "-4");
    CHECK_EVAL("-2^3", "-8");
    CHECK_EVAL("-2^4", "-16");
    CHECK_EVAL("-2^5", "-32");
    CHECK_EVAL("-2^6", "-64");
    CHECK_EVAL("-2^7", "-128");
    CHECK_EVAL("-2^8", "-256");
    CHECK_EVAL("-2^9", "-512");
    CHECK_EVAL("-2^10", "-1024");

    CHECK_EVAL("(5-7)^2",  "4");
    CHECK_EVAL("-(5-7)^2", "-4");

    CHECK_EVAL("-cos(pi)^3", "1");
    CHECK_EVAL("1*(-cos(pi)^2)", "-1");

    CHECK_EVAL("3^3^3", "7625597484987");

    CHECK_EVAL("1/-1^2", "-1");
    CHECK_EVAL("1*-1^2", "-1");

    // Factorial has higher precedence than unary minus.
    CHECK_EVAL("-1!", "-1");
    CHECK_EVAL("-2!", "-2");
    CHECK_EVAL("-3!", "-6");

    // Unicode minus sign
    CHECK_EVAL("−1", "-1");
    CHECK_EVAL("−(2*3)", "-6");
    CHECK_EVAL("2*−1", "-2");
    CHECK_EVAL("2e−1", "0.2");

    CHECK_INTERPRETED("−pi", "-pi");
    CHECK_INTERPRETED("--pi", "pi");
    CHECK_INTERPRETED("---pi", "-pi");
    CHECK_INTERPRETED("----pi", "pi");
}

void test_binary_power_operator()
{
    // See http://en.wikipedia.org/wiki/Empty_product.
    CHECK_EVAL("0^0", "NaN");

    CHECK_EVAL("1^0", "1");
    CHECK_EVAL("1^1", "1");
    CHECK_EVAL("1^2", "1");
    CHECK_EVAL("1^3", "1");

    CHECK_EVAL("2^0", "1");
    CHECK_EVAL("2^1", "2");
    CHECK_EVAL("2^2", "4");
    CHECK_EVAL("2^3", "8");
    CHECK_EVAL("2^4", "16");
    CHECK_EVAL("2^5", "32");
    CHECK_EVAL("2^6", "64");
    CHECK_EVAL("2^7", "128");
    CHECK_EVAL("2^8", "256");
    CHECK_EVAL("2^9", "512");
    CHECK_EVAL("2^10", "1024");
    CHECK_EVAL("(-13)^(1/3)", "-2.3513346877207574895");
    CHECK_EVAL("(-13)^(1/9)", "-1.32975454563978597291");
    CHECK_EVAL("(-13)^(1/27)", "-1.0996567925533755264");
    CHECK_EVAL("(-13)^(1/3.123)", "2.27347485100383238372");
    CHECK_EVAL("(-13)^(1/9.123)", "1.32465488702830785593");
    CHECK_EVAL("(-13)^(1/27.123)", "1.09918315510346486963");
}

void test_binary_arithmetic_operator_variants()
{
    CHECK_EVAL("0+0", "0");
    CHECK_EVAL("1+0", "1");
    CHECK_EVAL("0+1", "1");
    CHECK_EVAL("1+1", "2");

    CHECK_EVAL("0-0", "0");
    CHECK_EVAL("1-0", "1");
    CHECK_EVAL("0-1", "-1");
    CHECK_EVAL("1-1", "0");

    CHECK_EVAL("0−0", "0");
    CHECK_EVAL("1−0", "1");
    CHECK_EVAL("0−1", "-1");
    CHECK_EVAL("1−1", "0");

    CHECK_EVAL("2*3", "6");
    CHECK_EVAL("2×3", "6");
    CHECK_EVAL("2·3", "6"); // U+22C5 Dot operator.
    CHECK_EVAL("3*2", "6");
    CHECK_EVAL("3×2", "6");
    CHECK_EVAL("3·2", "6");

    CHECK_EVAL("10/2", "5");
    CHECK_EVAL("10÷2", "5");
    CHECK_EVAL("10⧸2", "5"); // U+29F8 Big solidus.
    CHECK_EVAL("2/10", "0.2");
    CHECK_EVAL("2÷10", "0.2");
    CHECK_EVAL("2⧸10", "0.2");
    CHECK_EVAL("1/(1/(1/0.2425-4)-8)-12", "0");
    CHECK_EVAL("1/(1/(1/(2425/10000)-4)-8)-12", "0");
    CHECK_EVAL("1/(1/(1/(1/(1/(1/0.25636-3)-1)-9)-12)-1)-48", "0");
    CHECK_EVAL("1/(1/(1/(1/(1/(1/(25636/100000)-3)-1)-9)-12)-1)-48", "0");
}

void test_units_conversion_parentheses()
{
    // Check that parentheses are added in unit conversion results when needed.
    CHECK_EVAL("1[metre] -> [10 metre]", "0.1 10·metre");
    CHECK_EVAL("1[metre] -> [.1 metre]", "10 .1·metre");
    CHECK_EVAL("1[metre] -> [-1 metre]", "-1 -1 metre");
    CHECK_EVAL("1[metre] -> [0xa metre]", "0.1 0xa·metre");
    CHECK_EVAL("1[metre second] -> [10 metre second]", "0.1 10·metre·second");
    CHECK_EVAL("1[metre] -> [metre + metre]", "0.5 metre + metre");
    CHECK_EVAL("1[metre] -> [metre - 2metre]", "-1 metre - 2metre");
    CHECK_EVAL("1[metre] -> [metre]", "1 metre");
    CHECK_EVAL("1[10 metre] -> [metre]", "10 metre");
    CHECK_EVAL("1[metre] in [metre]", "1 metre");
    CHECK_EVAL("1[metre] IN [metre]", "1 metre");
    CHECK_EVAL("50[yard] + 2[foot] in [cm]", "4632.96 cm");
    CHECK_EVAL("10[metre] in (1[yard] + 2[foot])", "6.56167979002624671916 (1[yard] + 2[foot])");
    CHECK_EVAL(QString::fromUtf8("1[metre] −> [cm]"), "100 cm");
    CHECK_EVAL(QString::fromUtf8("1[metre] → [cm]"), "100 cm");
    CHECK_EVAL(QString::fromUtf8("[hectare] → [m²] → [hectare]"), "1 hectare");
}

void test_units_display_propagation()
{
    // Display-unit propagation should be symmetric for scalar multiplication.
    CHECK_EVAL("3 * 2[metre]", "6 metre");
    CHECK_EVAL("2[metre] * 3", "6 metre");
}

void test_units_short_aliases_and_si_prefixes()
{
    CHECK_EVAL("[m]", "1 metre");
    CHECK_EVAL("[kg]", "1 kilogram");
    CHECK_EVAL("[F]", "1 farad");
    CHECK_EVAL("[V]", "1 volt");
    CHECK_EVAL("[Eh]", "0.00000000000000000436 joule");
    CHECK_EVAL("[mol]", "1 mole");
    CHECK_EVAL(QString::fromUtf8("[Ω]"), "1 ohm");
    CHECK_EVAL("[ha]", u8"10000 metre²");
    CHECK_EVAL("[in]", "0.0254 metre");
    CHECK_EVAL("[foot] -> [in]", "12 in");
    CHECK_EVAL("[in] -> [inch]", "1 inch");

    CHECK_EVAL("[kilometre] -> [metre]", "1000 metre");
    CHECK_EVAL("[km] -> [metre]", "1000 metre");
    CHECK_EVAL("[milligram] -> [gram]", "0.001 gram");
    CHECK_EVAL("[mg] -> [gram]", "0.001 gram");
    CHECK_EVAL("[megaelectronvolt] -> [electronvolt]", "1000000 electronvolt");
    CHECK_EVAL("[MeV] -> [eV]", "1000000 eV");
    CHECK_EVAL_FAIL("[electron_volt]");
    CHECK_EVAL_FAIL("[megaelectron_volt]");
    CHECK_EVAL("[picovolt] -> [volt]", "0.000000000001 volt");
    CHECK_EVAL("[pV] -> [volt]", "0.000000000001 volt");
    CHECK_EVAL(QString::fromUtf8("[ha] -> [m²]"), u8"10000 m²");
    CHECK_EVAL("[ac] -> [acre]", "1 acre");
    CHECK_EVAL("[rpm] -> [revolution_per_minute]", "1 revolution_per_minute");
    CHECK_EVAL("[mph] -> [mile_per_hour]", "1 mile_per_hour");
    CHECK_EVAL("[kph] -> [kilometre_per_hour]", "1 kilometre_per_hour");
    CHECK_EVAL("[kWh] -> [kilowatt_hour]", "1 kilowatt_hour");
    CHECK_EVAL("[mmHg] -> [millimetre_of_mercury]", "1 millimetre_of_mercury");
    CHECK_EVAL("[st] -> [stone]", "1 stone");
    CHECK_EVAL("[dsp] -> [dessert_spoon]", "1 dessert_spoon");
    CHECK_EVAL("[quad] -> [quad]", "1 quad");
    CHECK_EVAL("[fluid_ounce_imp] -> [fluid_ounce_imp]", "1 fluid_ounce_imp");
    CHECK_EVAL("[floz_imp] -> [fluid_ounce_imp]", "1 fluid_ounce_imp");
    CHECK_EVAL("[fluid_ounce_us] -> [fluid_ounce_us]", "1 fluid_ounce_us");
    CHECK_EVAL("[floz_us] -> [fluid_ounce_us]", "1 fluid_ounce_us");
    CHECK_EVAL("[fluid_dram_imp] -> [fluid_dram_imp]", "1 fluid_dram_imp");
    CHECK_EVAL("[fldr_imp] -> [fluid_dram_imp]", "1 fluid_dram_imp");
    CHECK_EVAL("[fluid_dram_us] -> [fluid_dram_us]", "1 fluid_dram_us");
    CHECK_EVAL("[fldr_us] -> [fluid_dram_us]", "1 fluid_dram_us");
    CHECK_EVAL("[gill_imp] -> [gill_imp]", "1 gill_imp");
    CHECK_EVAL("[gill_us] -> [gill_us]", "1 gill_us");
    CHECK_EVAL("[square_inch] -> [square_inch]", "1 square_inch");
    CHECK_EVAL(QString::fromUtf8("[in²] -> [square_inch]"), "1 square_inch");
    CHECK_EVAL("[square_foot] -> [square_foot]", "1 square_foot");
    CHECK_EVAL(QString::fromUtf8("[ft²] -> [square_foot]"), "1 square_foot");
    CHECK_EVAL("[square_yard] -> [square_yard]", "1 square_yard");
    CHECK_EVAL(QString::fromUtf8("[yd²] -> [square_yard]"), "1 square_yard");
    CHECK_EVAL("[cubic_foot] -> [cubic_foot]", "1 cubic_foot");
    CHECK_EVAL(QString::fromUtf8("[ft³] -> [cubic_foot]"), "1 cubic_foot");
    CHECK_EVAL("[cubic_yard] -> [cubic_yard]", "1 cubic_yard");
    CHECK_EVAL(QString::fromUtf8("[yd³] -> [cubic_yard]"), "1 cubic_yard");
    CHECK_EVAL("[cubic_inch] -> [cubic_inch]", "1 cubic_inch");
    CHECK_EVAL(QString::fromUtf8("[in³] -> [cubic_inch]"), "1 cubic_inch");
    CHECK_EVAL("[cubic_millimetre] -> [cubic_millimetre]", "1 cubic_millimetre");
    CHECK_EVAL(QString::fromUtf8("[mm³] -> [cubic_millimetre]"), "1 cubic_millimetre");
    CHECK_EVAL("[square_millimetre] -> [square_millimetre]", "1 square_millimetre");
    CHECK_EVAL(QString::fromUtf8("[mm²] -> [square_millimetre]"), "1 square_millimetre");
    CHECK_EVAL("[cubic_centimetre] -> [cubic_centimetre]", "1 cubic_centimetre");
    CHECK_EVAL(QString::fromUtf8("[cm³] -> [cubic_centimetre]"), "1 cubic_centimetre");
    CHECK_EVAL("[cc] -> [cubic_centimetre]", "1 cubic_centimetre");
    CHECK_EVAL("[cubic_decimetre] -> [cubic_decimetre]", "1 cubic_decimetre");
    CHECK_EVAL(QString::fromUtf8("[dm³] -> [cubic_decimetre]"), "1 cubic_decimetre");
    CHECK_EVAL("[cubic_kilometre] -> [cubic_kilometre]", "1 cubic_kilometre");
    CHECK_EVAL(QString::fromUtf8("[km³] -> [cubic_kilometre]"), "1 cubic_kilometre");
    CHECK_EVAL("[square_kilometre] -> [square_kilometre]", "1 square_kilometre");
    CHECK_EVAL(QString::fromUtf8("[km²] -> [square_kilometre]"), "1 square_kilometre");
    CHECK_EVAL("[barrel_oil] -> [barrel_oil]", "1 barrel_oil");
    CHECK_EVAL("[bbl_oil] -> [barrel_oil]", "1 barrel_oil");
    CHECK_EVAL("[barrel_beer_us] -> [barrel_beer_us]", "1 barrel_beer_us");
    CHECK_EVAL("[bbl_beer] -> [barrel_beer_us]", "1 barrel_beer_us");
    CHECK_EVAL("[square_mile] -> [square_mile]", "1 square_mile");
    CHECK_EVAL(QString::fromUtf8("[mi²] -> [square_mile]"), "1 square_mile");
    CHECK_EVAL("[cubic_mile] -> [cubic_mile]", "1 cubic_mile");
    CHECK_EVAL(QString::fromUtf8("[mi³] -> [cubic_mile]"), "1 cubic_mile");
    CHECK_EVAL("[gallon_imp] -> [gallon_imp]", "1 gallon_imp");
    CHECK_EVAL("[gal_imp] -> [gallon_imp]", "1 gallon_imp");
    CHECK_EVAL("[gallon_us] -> [gallon_us]", "1 gallon_us");
    CHECK_EVAL("[gal_us] -> [gallon_us]", "1 gallon_us");
    CHECK_EVAL("[pint_imp] -> [pint_imp]", "1 pint_imp");
    CHECK_EVAL("[pt_imp] -> [pint_imp]", "1 pint_imp");
    CHECK_EVAL("[pint_us] -> [pint_us]", "1 pint_us");
    CHECK_EVAL("[pt_us] -> [pint_us]", "1 pint_us");
    CHECK_EVAL("[quart_imp] -> [quart_imp]", "1 quart_imp");
    CHECK_EVAL("[qt_imp] -> [quart_imp]", "1 quart_imp");
    CHECK_EVAL("[quart_us] -> [quart_us]", "1 quart_us");
    CHECK_EVAL("[qt_us] -> [quart_us]", "1 quart_us");
    CHECK_EVAL("[tablespoon_imp] -> [tablespoon_imp]", "1 tablespoon_imp");
    CHECK_EVAL("[tbsp_imp] -> [tablespoon_imp]", "1 tablespoon_imp");
    CHECK_EVAL("[tablespoon_us] -> [tablespoon_us]", "1 tablespoon_us");
    CHECK_EVAL("[tbsp_us] -> [tablespoon_us]", "1 tablespoon_us");
    CHECK_EVAL("[tablespoon_au] -> [tablespoon_au]", "1 tablespoon_au");
    CHECK_EVAL("[tbsp_au] -> [tablespoon_au]", "1 tablespoon_au");
    CHECK_EVAL("[teaspoon_imp] -> [teaspoon_imp]", "1 teaspoon_imp");
    CHECK_EVAL("[tsp_imp] -> [teaspoon_imp]", "1 teaspoon_imp");
    CHECK_EVAL("[teaspoon_us] -> [teaspoon_us]", "1 teaspoon_us");
    CHECK_EVAL("[tsp_us] -> [teaspoon_us]", "1 teaspoon_us");
    CHECK_EVAL("[cup_imp] -> [cup_imp]", "1 cup_imp");
    CHECK_EVAL("[cup_jp] -> [cup_jp]", "1 cup_jp");
    CHECK_EVAL("[cup_us] -> [cup_us]", "1 cup_us");
    CHECK_EVAL("[fluid_ounce_imp] -> [litre]", "0.0284130625 litre");
    CHECK_EVAL("[litre] -> [fluid_ounce_imp]", "35.19507972785404600437 fluid_ounce_imp");
    CHECK_EVAL("[fluid_ounce_us] -> [litre]", "0.0295735295625 litre");
    CHECK_EVAL("[litre] -> [fluid_ounce_us]", "33.81402270184299716863 fluid_ounce_us");
    CHECK_EVAL("[fluid_dram_imp] -> [litre]", "0.0035516328125 litre");
    CHECK_EVAL("[litre] -> [fluid_dram_imp]", "281.56063782283236803495 fluid_dram_imp");
    CHECK_EVAL("[fluid_dram_us] -> [litre]", "0.0036966911953125 litre");
    CHECK_EVAL("[litre] -> [fluid_dram_us]", "270.51218161474397734902 fluid_dram_us");
    CHECK_EVAL("[gallon_imp] -> [litre]", "4.54609 litre");
    CHECK_EVAL("[litre] -> [gallon_imp]", "0.21996924829908778753 gallon_imp");
    CHECK_EVAL("[gallon_us] -> [litre]", "3.785411784 litre");
    CHECK_EVAL("[litre] -> [gallon_us]", "0.26417205235814841538 gallon_us");
    CHECK_EVAL("[gill_imp] -> [litre]", "0.1420653125 litre");
    CHECK_EVAL("[litre] -> [gill_imp]", "7.03901594557080920087 gill_imp");
    CHECK_EVAL("[gill_us] -> [litre]", "0.11829411825 litre");
    CHECK_EVAL("[litre] -> [gill_us]", "8.45350567546074929216 gill_us");
    CHECK_EVAL("[pint_imp] -> [litre]", "0.56826125 litre");
    CHECK_EVAL("[litre] -> [pint_imp]", "1.75975398639270230022 pint_imp");
    CHECK_EVAL("[pint_us] -> [litre]", "0.473176473 litre");
    CHECK_EVAL("[litre] -> [pint_us]", "2.11337641886518732304 pint_us");
    CHECK_EVAL("[quart_imp] -> [litre]", "1.1365225 litre");
    CHECK_EVAL("[litre] -> [quart_imp]", "0.87987699319635115011 quart_imp");
    CHECK_EVAL("[quart_us] -> [litre]", "0.946352946 litre");
    CHECK_EVAL("[litre] -> [quart_us]", "1.05668820943259366152 quart_us");
    CHECK_EVAL("[cubic_foot] -> [litre]", "28.316846592 litre");
    CHECK_EVAL("[litre] -> [cubic_foot]", "0.03531466672148859025 cubic_foot");
    CHECK_EVAL("[cubic_foot] -> [cubic_metre]", "0.028316846592 cubic_metre");
    CHECK_EVAL("[cubic_metre] -> [cubic_foot]", "35.31466672148859025044 cubic_foot");
    CHECK_EVAL("[cubic_yard] -> [cubic_metre]", "0.764554857984 cubic_metre");
    CHECK_EVAL("[cubic_metre] -> [cubic_yard]", "1.3079506193143922315 cubic_yard");
    CHECK_EVAL("[cubic_inch] -> [litre]", "0.016387064 litre");
    CHECK_EVAL("[litre] -> [cubic_inch]", "61.02374409473228395276 cubic_inch");
    CHECK_EVAL("[cubic_inch] -> [cubic_metre]", "0.000016387064 cubic_metre");
    CHECK_EVAL("[cubic_metre] -> [cubic_inch]", "61023.74409473228395275688 cubic_inch");
    CHECK_EVAL("[cubic_millimetre] -> [cubic_metre]", "0.000000001 cubic_metre");
    CHECK_EVAL("[cubic_metre] -> [cubic_millimetre]", "1000000000 cubic_millimetre");
    CHECK_EVAL("[cubic_millimetre] -> [litre]", "0.000001 litre");
    CHECK_EVAL("[litre] -> [cubic_millimetre]", "1000000 cubic_millimetre");
    CHECK_EVAL("[cubic_centimetre] -> [cubic_metre]", "0.000001 cubic_metre");
    CHECK_EVAL("[cubic_metre] -> [cubic_centimetre]", "1000000 cubic_centimetre");
    CHECK_EVAL("[cubic_centimetre] -> [litre]", "0.001 litre");
    CHECK_EVAL("[litre] -> [cubic_centimetre]", "1000 cubic_centimetre");
    CHECK_EVAL("[cubic_decimetre] -> [cubic_metre]", "0.001 cubic_metre");
    CHECK_EVAL("[cubic_metre] -> [cubic_decimetre]", "1000 cubic_decimetre");
    CHECK_EVAL("[cubic_decimetre] -> [litre]", "1 litre");
    CHECK_EVAL("[litre] -> [cubic_decimetre]", "1 cubic_decimetre");
    CHECK_EVAL("[cubic_kilometre] -> [cubic_metre]", "1000000000 cubic_metre");
    CHECK_EVAL("[cubic_metre] -> [cubic_kilometre]", "0.000000001 cubic_kilometre");
    CHECK_EVAL("[cubic_kilometre] -> [litre]", "1000000000000 litre");
    CHECK_EVAL("[litre] -> [cubic_kilometre]", "0.000000000001 cubic_kilometre");
    CHECK_EVAL("[cubic_mile] -> [cubic_metre]", "4168181825.440579584 cubic_metre");
    CHECK_EVAL("[cubic_metre] -> [cubic_mile]", "0.00000000023991275858 cubic_mile");
    CHECK_EVAL("[barrel_oil] -> [litre]", "158.987294928 litre");
    CHECK_EVAL("[litre] -> [barrel_oil]", "0.00628981077043210513 barrel_oil");
    CHECK_EVAL("[barrel_beer_us] -> [litre]", "119.240471196 litre");
    CHECK_EVAL("[litre] -> [barrel_beer_us]", "0.00838641436057614017 barrel_beer_us");
    CHECK_EVAL("[square_inch] -> [square_metre]", "0.00064516 square_metre");
    CHECK_EVAL("[square_metre] -> [square_inch]", "1550.00310000620001240002 square_inch");
    CHECK_EVAL("[square_foot] -> [square_metre]", "0.09290304 square_metre");
    CHECK_EVAL("[square_metre] -> [square_foot]", "10.76391041670972230833 square_foot");
    CHECK_EVAL("[square_yard] -> [square_metre]", "0.83612736 square_metre");
    CHECK_EVAL("[square_metre] -> [square_yard]", "1.19599004630108025648 square_yard");
    CHECK_EVAL("[square_mile] -> [square_metre]", "2589988.110336 square_metre");
    CHECK_EVAL("[square_metre] -> [square_mile]", "0.00000038610215854245 square_mile");
    CHECK_EVAL("[square_millimetre] -> [square_metre]", "0.000001 square_metre");
    CHECK_EVAL("[square_metre] -> [square_millimetre]", "1000000 square_millimetre");
    CHECK_EVAL("[square_kilometre] -> [square_metre]", "1000000 square_metre");
    CHECK_EVAL("[square_metre] -> [square_kilometre]", "0.000001 square_kilometre");
    CHECK_EVAL("[tablespoon_imp] -> [litre]", "0.0177581640625 litre");
    CHECK_EVAL("[litre] -> [tablespoon_imp]", "56.31212756456647360699 tablespoon_imp");
    CHECK_EVAL("[tablespoon_us] -> [litre]", "0.01478676478125 litre");
    CHECK_EVAL("[litre] -> [tablespoon_us]", "67.62804540368599433725 tablespoon_us");
    CHECK_EVAL("[tablespoon_au] -> [litre]", "0.02 litre");
    CHECK_EVAL("[litre] -> [tablespoon_au]", "50 tablespoon_au");
    CHECK_EVAL("[teaspoon_imp] -> [litre]", "0.00591938802083333333 litre");
    CHECK_EVAL("[litre] -> [teaspoon_imp]", "168.93638269369942082097 teaspoon_imp");
    CHECK_EVAL("[teaspoon_us] -> [litre]", "0.00492892159375 litre");
    CHECK_EVAL("[litre] -> [teaspoon_us]", "202.88413621105798301176 teaspoon_us");
    CHECK_EVAL("[cup_imp] -> [litre]", "0.284130625 litre");
    CHECK_EVAL("[litre] -> [cup_imp]", "3.51950797278540460044 cup_imp");
    CHECK_EVAL("[cup_jp] -> [litre]", "0.2 litre");
    CHECK_EVAL("[litre] -> [cup_jp]", "5 cup_jp");
    CHECK_EVAL("[cup_us] -> [litre]", "0.2365882365 litre");
    CHECK_EVAL("[litre] -> [cup_us]", "4.22675283773037464608 cup_us");
    CHECK_EVAL("[gal_imp] -> [gal_us]", "1.20094992550485492967 gal_us");
    CHECK_EVAL("[floz_imp] -> [floz_us]", "0.96075994040388394374 floz_us");
    CHECK_EVAL("[qt_imp] -> [qt_us]", "1.20094992550485492967 qt_us");
    CHECK_EVAL("[pt_imp] -> [pt_us]", "1.20094992550485492967 pt_us");
    CHECK_EVAL("[kilometre_per_hour] -> [metre/second]", u8"0.27777777777777777778 metre·second⁻¹");
    CHECK_EVAL("3.6[kilometre_per_hour] -> [metre/second]", u8"1 metre·second⁻¹");
    CHECK_EVAL("1[metre/second] -> [kilometre_per_hour]", "3.6 kilometre_per_hour");
    CHECK_EVAL("[mile_per_hour] -> [kilometre_per_hour]", "1.609344 kilometre_per_hour");
    CHECK_EVAL("1.609344[kilometre_per_hour] -> [mile_per_hour]", "1 mile_per_hour");
    CHECK_EVAL("1e15[Btu] -> [quad]", "1 quad");
    CHECK_EVAL_FORMAT("1[Btu]", u8"1055.05585262[J]");
    CHECK_EVAL_FORMAT("1[Btu] -> [J]", u8"1055.05585262[J]");
    CHECK_EVAL_FORMAT("1[british_thermal_unit]", u8"1055.05585262[J]");
    CHECK_EVAL("1[Btu] -> [joule]", "1055.05585262 joule");
    CHECK_EVAL("1[british_thermal_unit] -> [joule]", "1055.05585262 joule");
    CHECK_EVAL_FORMAT("1[british_thermal_unit] -> [J]", u8"1055.05585262[J]");

    CHECK_EVAL("[mV] -> [volt]", "0.001 volt");
    CHECK_EVAL("[MV] -> [volt]", "1000000 volt");
    CHECK_EVAL_FAIL("[Mv] -> [volt]");
    CHECK_EVAL(QString::fromUtf8("[m²] -> [m²]"), u8"1 m²");
    CHECK_EVAL(QString::fromUtf8("[m³] -> [m³]"), u8"1 m³");
    CHECK_EVAL(QString::fromUtf8("[mm²] -> [m²]"), u8"0.000001 m²");
    CHECK_EVAL(QString::fromUtf8("[mm³] -> [m³]"), u8"0.000000001 m³");
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("[m]"),
        QStringLiteral("1")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[m]"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("[m]"),
        QStringLiteral("1")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[m]"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("[m] + [m]"),
        QStringLiteral("1")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[m]")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("+")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("1")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[m]"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("[m] + [m]"),
        QStringLiteral("1")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[m]")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("+")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("1")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[m]"));
    CHECK_INTERPRETED(QString::fromUtf8("2[m²]+3[mm²]"), "2[m^2]+3[mm^2]");
    CHECK_EVAL("2[]", "2");
    CHECK_INTERPRETED("2[]", "2");
    CHECK_DISPLAY_INTERPRETED("2[]", "2");

    CHECK_EVAL(QString::fromUtf8("[µV] -> [volt]"), "0.000001 volt");
    CHECK_EVAL(QString::fromUtf8("[μV] -> [volt]"), "0.000001 volt");
    CHECK_EVAL("[uV] -> [volt]", "0.000001 volt");
    CHECK_EVAL("[um] -> [metre]", "0.000001 metre");
    CHECK_EVAL_FAIL("[uB] -> [byte]");
    CHECK_EVAL("[kB] -> [byte]", "1000 byte");
    CHECK_EVAL("[MB] -> [byte]", "1000000 byte");
    CHECK_EVAL_FAIL("[daB] -> [byte]");
    CHECK_EVAL_FAIL("[dab] -> [bit]");
    CHECK_EVAL_FAIL("[hB] -> [byte]");
    CHECK_EVAL_FAIL("[hb] -> [bit]");
    CHECK_EVAL("[hl] -> [litre]", "100 litre");
    CHECK_EVAL("[hL] -> [litre]", "100 litre");
    CHECK_EVAL("[dag] -> [gram]", "10 gram");
    CHECK_EVAL("[dag] -> [kilogram]", "0.01 kilogram");
    CHECK_EVAL("[KiB] -> [byte]", "1024 byte");
    CHECK_EVAL("[kb] -> [b]", "1000 b");
    CHECK_EVAL("[Kib] -> [b]", "1024 b");
    CHECK_EVAL("[1 kibibyte] -> [KiB]", "1 KiB");
    CHECK_EVAL("[1 quebibit] -> [Qib]", "1 Qib");

    CHECK_EVAL_FORMAT("1[byte]+2[byte]", "3[B]");
    CHECK_EVAL_FORMAT("1[bit]+2[bit]", "3[b]");
    CHECK_EVAL_FORMAT("2[MB]+3[PB]+4[TB]", "3.004000002[PB]");
    CHECK_EVAL_FAIL("1[B]+8[b]");

    CHECK_EVAL("[mm] -> [metre]", "0.001 metre");
    CHECK_EVAL("[km] -> [metre]", "1000 metre");
    CHECK_EVAL("[kpc] -> [parsec]", "1000 parsec");
    CHECK_EVAL("[Mpc] -> [parsec]", "1000000 parsec");
    CHECK_EVAL("[hm] -> [metre]", "100 metre");
    CHECK_EVAL_FAIL("[hV] -> [volt]");
    CHECK_EVAL("[hPa] -> [pascal]", "100 pascal");
    CHECK_EVAL(QString::fromUtf8("[kΩ] -> [ohm]"), "1000 ohm");
    CHECK_EVAL_FORMAT("2[coulomb]+3[mC]", "2.003[C]");
    CHECK_EVAL_FORMAT("[C^3*m^3*J^(-2)]", u8"1[C³·J⁻²·m³]");
    CHECK_EVAL_FORMAT("[C^4*m^4*J^(-3)]", u8"1[C⁴·J⁻³·m⁴]");
    CHECK_EVAL_FORMAT("[A^4*s^10/(kg^3*m^2)]", u8"1[C⁴·J⁻³·m⁴]");
    CHECK_EVAL_FORMAT("1081.20238677[C*m^(-3)]", u8"1081.20238677[C·m⁻³]");
    CHECK_EVAL_FORMAT("[J/(K*mol)]", u8"1[J·mol⁻¹·K⁻¹]");
}

void test_units_named_derived_canonicalization()
{
    CHECK_EVAL_FORMAT("[kg*m^2/s^2]", "1[J]");
    CHECK_EVAL_FORMAT("[Pa*m^3]", "1[J]");
    CHECK_EVAL_FORMAT("[C*V]", "1[J]");
    CHECK_EVAL_FORMAT("[N*m/s]", "1[W]");

    CHECK_EVAL_FORMAT("[C/s]", "1[A]");
    CHECK_EVAL_FORMAT("[A*s]", "1[C]");

    CHECK_EVAL_FORMAT("[J/s]", "1[W]");
    CHECK_EVAL_FORMAT("[kg*m^2/s^3]", "1[W]");
    CHECK_EVAL_FORMAT("[V*A]", "1[W]");

    CHECK_EVAL_FORMAT("[W/A]", "1[V]");
    CHECK_EVAL_FORMAT("[J/C]", "1[V]");
    CHECK_EVAL_FORMAT("[V/A]", u8"1[Ω]");
    CHECK_EVAL_FORMAT("[A/V]", "1[S]");
    CHECK_EVAL_FORMAT("[1/ohm]", "1[S]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("[1/Ω]"), "1[S]");

    CHECK_EVAL_FORMAT("[C/V]", "1[F]");
    CHECK_EVAL_FORMAT("[V*s]", "1[Wb]");
    CHECK_EVAL_FORMAT("[Wb/m^2]", "1[T]");
    CHECK_EVAL_FORMAT("[Wb/A]", "1[H]");
    CHECK_EVAL_FORMAT("[kg*m/s^2]", "1[N]");
    CHECK_EVAL_FORMAT("[N/m^2]", "1[Pa]");
    CHECK_EVAL_FORMAT("[cd*sr]", "1[lm]");
    CHECK_EVAL_FORMAT("[cd*sr*m^2]", u8"1[lm·m²]");
    CHECK_EVAL_FORMAT("[lm/m^2]", "1[lx]");
    CHECK_EVAL_FORMAT("[mol/s]", "1[kat]");
}

void test_units_derived_si_recognition_and_disambiguation()
{
    CHECK_EVAL("[metre]", "1 metre");
    CHECK_EVAL("[kilogram]", "1 kilogram");
    CHECK_EVAL("[tonne]", "1000 kilogram");
    CHECK_EVAL("[t]", "1000 kilogram");
    CHECK_EVAL("1[tonne] -> [kilogram]", "1000 kilogram");
    CHECK_EVAL("1[t] -> [kilogram]", "1000 kilogram");
    CHECK_EVAL("1000[kilogram] -> [tonne]", "1 tonne");
    CHECK_EVAL("2.51 [t] + 3.2 [kg] + 2342.7 [g]", "2515.5427 kilogram");
    CHECK_EVAL("[second]", "1 second");
    CHECK_EVAL("[coulomb/second]", "1 ampere");
    CHECK_EVAL("[lumen/steradian]", "1 candela");
    CHECK_EVAL("[1/second]", "1 second⁻¹");
    CHECK_EVAL("[newton*metre]", "1 newton·metre");
    CHECK_EVAL("[newton*metre] -> [joule]", "1 joule");
    CHECK_EVAL("[joule/metre^3] -> [pascal]", "1 pascal");
    CHECK_EVAL("[watt*second] -> [joule]", "1 joule");
    CHECK_EVAL("[revolution/minute] -> [revolution_per_minute]", "1 revolution_per_minute");
    CHECK_EVAL("[1609.344 metre/hour] -> [mile_per_hour]", "1 mile_per_hour");
    CHECK_EVAL("[1000 metre/hour] -> [kilometre_per_hour]", "1 kilometre_per_hour");
    CHECK_EVAL("[kilowatt*hour] -> [kilowatt_hour]", "1 kilowatt_hour");
    CHECK_EVAL("[133.322387415*pascal] -> [millimetre_of_mercury]", "1 millimetre_of_mercury");
    CHECK_EVAL("[14*pound] -> [stone]", "1 stone");
    CHECK_EVAL("[10*millilitre] -> [dessert_spoon]", "1 dessert_spoon");
    CHECK_EVAL("[watt*volt]", "1 volt²·ampere");
    CHECK_EVAL("[volt*ampere]", "1 watt");
    CHECK_EVAL("[volt*second/metre^2]", "1 tesla");
    CHECK_EVAL("[becquerel]", "1 becquerel");
    CHECK_EVAL("[2 becquerel + 3 becquerel]", "5 becquerel");
    CHECK_EVAL("[sievert]", "1 sievert");
    CHECK_EVAL("[2 sievert + 3 sievert]", "5 sievert");
    CHECK_EVAL_FAIL("1[hertz] + 1[becquerel]");
    CHECK_EVAL_FAIL("1[gray] + 1[sievert]");

    CHECK_EVAL("a=10", "10");
    CHECK_EVAL("mg=3", "3");
    CHECK_EVAL("l=2", "2");
    CHECK_EVAL("a+mg+l", "15");
    eval->unsetVariable("a");
    eval->unsetVariable("mg");
    eval->unsetVariable("l");
}

void test_units_conversion_compatibility_and_canonicalization()
{
    // Conversion compatibility and canonicalization regressions.
    CHECK_EVAL("[volt] -> [joule/coulomb]", u8"1 joule·coulomb⁻¹");
    CHECK_EVAL("[joule/coulomb] -> [volt]", "1 volt");
    CHECK_EVAL("[watt] -> [joule/second]", u8"1 joule·second⁻¹");
    CHECK_EVAL("[joule/second] -> [watt]", "1 watt");
    CHECK_EVAL("[ampere] -> [coulomb/second]", u8"1 coulomb·second⁻¹");
    CHECK_EVAL("[coulomb/second] -> [ampere]", "1 ampere");
    CHECK_EVAL("[volt] -> [ohm*ampere]", "1 ohm*ampere");
    CHECK_EVAL("[ohm*ampere] -> [volt]", "1 volt");
    CHECK_EVAL("[watt] -> [volt*ampere]", "1 volt*ampere");
    CHECK_EVAL("[volt*ampere] -> [watt]", "1 watt");

    CHECK_EVAL_FAIL("[volt] -> [watt]");
    CHECK_EVAL_FAIL("[watt] -> [volt]");
    CHECK_EVAL_FAIL("[volt] -> [ampere]");
    CHECK_EVAL_FAIL("[ampere] -> [volt]");

    CHECK_EVAL("[volt] -> [volt]", "1 volt");
    CHECK_EVAL("[watt] -> [watt]", "1 watt");
    CHECK_EVAL("[ampere] -> [ampere]", "1 ampere");
    CHECK_EVAL("[kg*m^2/s^4] -> [kg*m^2/s^4]", u8"1 kg·m²·s⁻⁴");
    CHECK_EVAL("[volt^2] -> [volt^2]", "1 volt²");
    CHECK_EVAL("[volt^2] -> [1 * (ohm * ampere)^2]", "1 1 * (ohm * ampere)²");
}

void test_units_temperature_affine_conversions()
{
    CHECK_EVAL("25[degree_celsius] -> [kelvin]", "298.15 kelvin");
    CHECK_EVAL("0[degree_celsius] -> [degree_fahrenheit]", "32 degree_fahrenheit");
    CHECK_EVAL("32[degree_fahrenheit] -> [degree_celsius]", "0 degree_celsius");
    CHECK_EVAL("77[degree_fahrenheit] -> [kelvin]", "298.15 kelvin");
    CHECK_EVAL("298.15[kelvin] -> [degree_fahrenheit]", "77 degree_fahrenheit");
    CHECK_EVAL(QString::fromUtf8("0[°C] -> [kelvin]"), "273.15 kelvin");
    CHECK_EVAL(QString::fromUtf8("0[˚C] -> [kelvin]"), "273.15 kelvin");
    CHECK_EVAL(QString::fromUtf8("32[°F] -> [°C]"), "0 °C");
    CHECK_EVAL(QString::fromUtf8("77[°F] -> [°C]"), "25 °C");
    CHECK_EVAL(QString::fromUtf8("273.15[kelvin] -> [°C]"), "0 °C");
    CHECK_EVAL(QString::fromUtf8("0[°C] -> [°F]"), "32 °F");
    CHECK_EVAL(QString::fromUtf8("32[ºF] -> [ºC]"), "0 °C");
    CHECK_EVAL(QString::fromUtf8("32[˚F] -> [˚C]"), "0 °C");
    CHECK_EVAL_FORMAT(QString::fromUtf8("1[°C]"), u8"1[°C]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("1[˚C]"), u8"1[°C]");
    CHECK_EVAL_FORMAT(QStringLiteral("1[ºC]"), u8"1[°C]");
    CHECK_EVAL_FORMAT(QStringLiteral("1[degree_celsius]"), u8"1[°C]");
    CHECK_EVAL_FORMAT(QStringLiteral("1[degC]"), u8"1[°C]");
    CHECK_EVAL_FORMAT(QStringLiteral("1[Cel]"), u8"1[°C]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("203[°F]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("203[˚F]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QStringLiteral("203[ºF]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QStringLiteral("203[degree_fahrenheit]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QStringLiteral("203[degF]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QStringLiteral("203[Fah]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("1 [°C]"), u8"1[°C]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("203 [°F]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QStringLiteral("1[oC]"), u8"1[°C]");
    CHECK_EVAL_FORMAT(QStringLiteral("203[oF]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("1[°C] -> [°F]"), u8"33.8[°F]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("1[°F] -> [°C]"), u8"-17.22222222222222222222[°C]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("100[°C] -> [°C]"), u8"100[°C]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("100[˚C] -> [˚C]"), u8"100[°C]");
    CHECK_EVAL_FORMAT(QStringLiteral("100[ºC] -> [ºC]"), u8"100[°C]");
    CHECK_EVAL_FORMAT(QStringLiteral("100[degree_celsius] -> [degree_celsius]"), u8"100[°C]");
    CHECK_EVAL_FORMAT(QStringLiteral("100[degC] -> [degC]"), u8"100[°C]");
    CHECK_EVAL_FORMAT(QStringLiteral("100[Cel] -> [Cel]"), u8"100[°C]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("203[°F] -> [°F]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("203[˚F] -> [˚F]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QStringLiteral("203[ºF] -> [ºF]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QStringLiteral("203[degree_fahrenheit] -> [degree_fahrenheit]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QStringLiteral("203[degF] -> [degF]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QStringLiteral("203[Fah] -> [Fah]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QStringLiteral("100[oC] -> [oC]"), u8"100[°C]");
    CHECK_EVAL_FORMAT(QStringLiteral("203[oF] -> [oF]"), u8"203[°F]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("1 [°C] → [°F]"), u8"33.8[°F]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("1 [°F] → [°C]"), u8"-17.22222222222222222222[°C]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("1[K] -> [°C]"), u8"-272.15[°C]");
    CHECK_EVAL_FORMAT(QString::fromUtf8("1[K] -> [°F]"), u8"-457.87[°F]");
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("100 [degC]"),
        QString::fromUtf8("100")
            + QString(MathDsl::QuantSp)
            + QString::fromUtf8("[°C]"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("100 [Cel]"),
        QString::fromUtf8("100")
            + QString(MathDsl::QuantSp)
            + QString::fromUtf8("[°C]"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("100 [K] → [°C]"),
        QString::fromUtf8("100")
            + QString(MathDsl::QuantSp)
            + QString::fromUtf8("[K]")
            + QString(MathDsl::AddWrap)
            + QString::fromUtf8("→")
            + QString(MathDsl::AddWrap)
            + QString::fromUtf8("[°C]"));

    // Scalar arithmetic on affine displays should use the shown temperature
    // scale instead of raw base-Kelvin storage.
    CHECK_EVAL(QString::fromUtf8("2 [°C] -> [°C]"), "2 °C");
    CHECK_EVAL(QString::fromUtf8("ans"), "2 °C");
    CHECK_EVAL(QString::fromUtf8("2*ans"), "4 °C");
    CHECK_EVAL(QString::fromUtf8("ans/2"), "2 °C");
    CHECK_EVAL(QString::fromUtf8("71.2 [°F] + 3 [°F]"), "74.2 °F");
    CHECK_EVAL(QString::fromUtf8("71.2 [°F] - 3 [°F]"), "68.2 °F");
}

void test_units_grouping_and_inverse_presentation()
{
    Settings* settings = Settings::instance();
    const char oldStyle = settings->unitNegativeExponentStyle;

    settings->unitNegativeExponentStyle = Settings::UnitNegativeExponentSuperscript;
    setRuntimeUnitNegativeExponentStyle(Settings::UnitNegativeExponentSuperscript);
    CHECK_EVAL_FORMAT("[m^2*s^-2]", u8"1[J·kg⁻¹]");
    CHECK_EVAL("molmass(C6H12O6) * molmass(C6H12O6)", u8"32456.184336 g²·mol⁻²");
    CHECK_EVAL_FORMAT("molmass(C6H12O6) * molmass(C6H12O6)", u8"32456.184336[g²·mol⁻²]");
    CHECK_EVAL_FORMAT("molmass(C6H12O6) * molmass(C6H12O6) -> [g²/mol²]", u8"32456.184336[g²·mol⁻²]");

    settings->unitNegativeExponentStyle = Settings::UnitNegativeExponentFraction;
    setRuntimeUnitNegativeExponentStyle(Settings::UnitNegativeExponentFraction);

    // If there is at least one positive exponent, render negative exponents
    // in denominator form for readability.
    CHECK_EVAL_FORMAT("[m*kg*s^-1]", u8"1[kg·m/s]");
    CHECK_EVAL_FORMAT("[kg*m^-1*s^-1]", u8"1[Pa·s]");
    CHECK_EVAL_FORMAT("[m^2*s^-1]", u8"1[m²/s]");
    CHECK_EVAL_FORMAT("[kg*m^2*s^-1]", u8"1[kg·m²/s]");
    CHECK_EVAL_FORMAT("[m*s^-1]", u8"1[m/s]");
    CHECK_EVAL_FORMAT("[m/s]", u8"1[m/s]");

    // If all exponents are negative, keep inverse-only exponent notation.
    CHECK_EVAL_FORMAT("[1/s]", u8"1[s⁻¹]");
    CHECK_EVAL_FORMAT("[s^-1]", u8"1[s⁻¹]");
    CHECK_EVAL_FORMAT("[1/(m*s^2)]", u8"1[m⁻¹·s⁻²]");
    CHECK_EVAL_FORMAT("[m^-1*s^-2]", u8"1[m⁻¹·s⁻²]");
    CHECK_EVAL_FORMAT("[m^-1/s^2]", u8"1[m⁻¹·s⁻²]");

    settings->unitNegativeExponentStyle = oldStyle;
    setRuntimeUnitNegativeExponentStyle(oldStyle);
}

void test_units_preferred_derived_forms_regressions()
{
    // Derived units should remain intact (no base expansion).
    CHECK_EVAL_FORMAT("5[N]", "5[N]");
    CHECK_EVAL_FORMAT("12[J]", "12[J]");
    CHECK_EVAL_FORMAT("3[W]", "3[W]");
    CHECK_EVAL_FORMAT("9[Pa]", "9[Pa]");
    CHECK_EVAL_FORMAT("2[V]", "2[V]");
    CHECK_EVAL_FORMAT("7[ohm]", u8"7[Ω]");
    CHECK_EVAL_FORMAT("4[F]", "4[F]");
    CHECK_EVAL_FORMAT("6[H]", "6[H]");
    CHECK_EVAL_FORMAT("8[T]", "8[T]");
    CHECK_EVAL_FORMAT("1[Hz]", "1[Hz]");

    // Combinations of derived units.
    CHECK_EVAL_FORMAT("3[N*m]", u8"3[N·m]");
    CHECK_EVAL_FORMAT("10[J*s]", u8"10[J·s]");
    CHECK_EVAL_FORMAT("2[W*h]", u8"2[W·h]");
    CHECK_EVAL_FORMAT("5[Pa*s]", u8"5[Pa·s]");
    CHECK_EVAL_FORMAT("7[V*A]", "7[W]");
    CHECK_EVAL_FORMAT("4[ohm*m]", u8"4[Ω·m]");
    CHECK_EVAL_FORMAT("6[F*V]", "6[C]");
    CHECK_EVAL_FORMAT("8[H*A]", "8[Wb]");
    CHECK_EVAL_FORMAT("9[T*m^2]", "9[Wb]");

    // Powers of derived units.
    CHECK_EVAL_FORMAT("2[J^2]", u8"2[J²]");
    CHECK_EVAL_FORMAT("4[N^3]", u8"4[N³]");
    CHECK_EVAL_FORMAT("6[Pa^(-1)]", u8"6[Pa⁻¹]");
    CHECK_EVAL_FORMAT("8[W^2]", u8"8[W²]");
    CHECK_EVAL_FORMAT("3[V^3]", u8"3[V³]");
    CHECK_EVAL_FORMAT("5[ohm^2]", u8"5[Ω²]");

    // Mixed derived units should keep derived components.
    CHECK_EVAL_FORMAT("5[J*Pa]", u8"5[J·Pa]");
    CHECK_EVAL_FORMAT("7[N*W]", u8"7[N·W]");
    CHECK_EVAL_FORMAT("3[V*Hz]", u8"3[V·Hz]");
    CHECK_EVAL_FORMAT("9[ohm*F]", u8"9[Ω·F]");
    CHECK_EVAL_FORMAT("2[T*H]", u8"2[T·H]");

    // Expressions combining values should preserve derived units.
    CHECK_EVAL_FORMAT("2[J] + 3[J]", "5[J]");
    CHECK_EVAL_FORMAT("4[N] * 5[m]", u8"20[N·m]");
    CHECK_EVAL_FORMAT("6[W] / 2[s]", u8"3[W / s]");
    CHECK_EVAL_FORMAT("3[V] * 2[A]", "6[W]");
    CHECK_EVAL_FORMAT("8[Pa] * 4[m^2]", "32[N]");

    // Unit cancellation.
    CHECK_EVAL("10[N] / 2[N]", "5");
    CHECK_EVAL("6[J] / 3[J]", "2");
    CHECK_EVAL("8[W] / 4[W]", "2");

    // Prefer derived units over base expansion.
    CHECK_EVAL_FORMAT("3[kg*m*s^(-2)]", "3[N]");
    CHECK_EVAL_FORMAT("5[kg*m^2*s^(-2)]", "5[J]");
    CHECK_EVAL_FORMAT("7[kg*m^(-1)*s^(-2)]", "7[Pa]");

    // Avoid expanding when already in preferred derived form.
    CHECK_EVAL_FORMAT("2[N*m]", u8"2[N·m]");
    CHECK_EVAL_FORMAT("4[J/s]", "4[W]");
    CHECK_EVAL_FORMAT("6[W*s]", u8"6[W·s]");

    // Edge cases.
    CHECK_EVAL_FORMAT("1[N*m/m]", "1[N]");
    CHECK_EVAL_FORMAT("2[J/s]", "2[W]");
    CHECK_EVAL_FORMAT("3[W*s/s]", "3[W]");
    CHECK_EVAL_FORMAT("4[V*A/A]", "4[V]");
}

void test_binary()
{
    test_binary_power_operator();
    test_binary_arithmetic_operator_variants();
}

void test_units()
{
    test_units_conversion_parentheses();
    test_units_display_propagation();
    test_units_short_aliases_and_si_prefixes();
    test_units_named_derived_canonicalization();
    test_units_derived_si_recognition_and_disambiguation();
    test_units_conversion_compatibility_and_canonicalization();
    test_units_temperature_affine_conversions();
    test_units_grouping_and_inverse_presentation();
    test_units_preferred_derived_forms_regressions();

    // Add/subtract must not mix a plain scalar with an explicitly unit-tagged
    // quantity, even when the unit dimension is mathematically dimensionless.
    CHECK_EVAL_FAIL("2 + 358 [deg]");
    CHECK_EVAL_FAIL("2 - 358 [deg]");

    // Multiplication remains valid with explicit angle units.
    ++eval_total_tests;
    eval->setExpression(QStringLiteral("2 * 358 [deg]"));
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tmultiply scalar by explicit angle unit\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    }

    // Exponentiation over explicit value+unit attachment is ambiguous.
    CHECK_EVAL_FAIL("2[kg]^2");
    CHECK_EVAL_FAIL(QString::fromUtf8("2[kg]²"));
}

void test_percent_operator()
{
    CHECK_EVAL("50%", "0.5");
    CHECK_EVAL("(20+30)%", "0.5");
    CHECK_EVAL("50%%", "0.005");
    CHECK_EVAL("-50%", "-0.5");

    CHECK_EVAL("1000+12%", "1120");
    CHECK_EVAL(QString::fromUtf8("100\u205F+\u205F12%"), "112");
    CHECK_EVAL("1000-12%", "880");
    CHECK_EVAL("1000+125%", "2250");
    CHECK_EVAL("1000+(12%)", "1120");
    CHECK_EVAL("1000-(12%)", "880");
    CHECK_EVAL("1000+(-12%)", "880");
    CHECK_EVAL("1000-(-12%)", "1120");
    CHECK_EVAL("1000+(10+2)%", "1120");
    CHECK_EVAL("1000-(10+2)%", "880");

    CHECK_EVAL("1000+10%+10%", "1210");
    CHECK_EVAL("1000-10%-10%", "810");
    CHECK_EVAL("1000+10%-10%", "990");
    CHECK_EVAL("1000-10%+10%", "990");
    CHECK_EVAL("100+10%+10%+10%", "133.1");

    CHECK_EVAL("1000*12%", "120");
    CHECK_EVAL("12%*1000", "120");
    CHECK_EVAL("1000/25%", "4000");
    CHECK_EVAL("200+(10%*2)", "200.2");
    CHECK_EVAL("200+10%^2", "200.01");
    CHECK_EVAL("100+(10%+10%)", "100.11");
    CHECK_EVAL("100-(10%+10%)", "99.89");
    CHECK_EVAL("100-(10%-5%)", "99.905");

    CHECK_EVAL("1+2+3+10%", "6.6");
    CHECK_EVAL("1+(2+3)+10%", "6.6");
    CHECK_EVAL("(1+2+3)+10%", "6.6");
    CHECK_EVAL("1+2+(3+10%)", "6.3");
    CHECK_EVAL("1+99-10%", "90");
    CHECK_EVAL("(1+99)-10%", "90");
    CHECK_EVAL("1+(99-10%)", "90.1");

    CHECK_EVAL("200[metre]+10%", "220 metre");
    CHECK_EVAL("200[metre]-10%", "180 metre");
    CHECK_EVAL("200[metre]+10%+10%", "242 metre");

    CHECK_EVAL_FAIL("%");
    CHECK_EVAL_FAIL("100+%");

    CHECK_INTERPRETED("1000+12%", "1000+12%");
    CHECK_INTERPRETED("1+99+10%", "(1+99)+10%");
    CHECK_INTERPRETED("1+2+3+10%", "(1+2+3)+10%");
    CHECK_INTERPRETED("1+(2+3)+10%", "(1+(2+3))+10%");
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+99+10%"),
        QStringLiteral("100")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("+")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("10%"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("cos pi cos pi + 10%"),
        QString::fromUtf8("cos²(pi)")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("+")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("10%"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1 + cos pi cos pi + 10%"),
        QStringLiteral("(cos²(pi)")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("+")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("1)")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("+")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("10%"));
    CHECK_EVAL(Evaluator::simplifyInterpretedExpression("100+12%"), "112");
    CHECK_EVAL(Evaluator::simplifyInterpretedExpression("1+2+3+10%"), "6.6");
    CHECK_EVAL(Evaluator::simplifyInterpretedExpression("1+(2+3)+10%"), "6.6");
    CHECK_EVAL(Evaluator::simplifyInterpretedExpression("1+cos pi cos pi+10%"), "2.2");
}

void test_bitwise_complement_operator()
{
    CHECK_EVAL("~~~~1", "1");
    CHECK_EVAL("~-~1", "-3");
    CHECK_EVAL("-~-1", "0");
    CHECK_EVAL("~~-1", "-1");
    CHECK_EVAL("(~2)^3", "-27");
    CHECK_EVAL("~(2^3)", "-9");
    CHECK_EVAL("~2^3", "-9");
    CHECK_EVAL("~(-5)", "4");
    CHECK_EVAL("-~(-1)", "0");
    CHECK_EVAL("-5 & 3", "3");
    CHECK_EVAL("-5 | 3", "-5");
    CHECK_EVAL("-5 ^ 3", "-125");
    CHECK_EVAL("~0 >> 1", "-1");
    CHECK_EVAL("~0 << 1", "-2");
    CHECK_EVAL("-1 >> 1", "-1");
    CHECK_EVAL("-8 >> 2", "-2");
    CHECK_EVAL("-9 >> 2", "-3");
    CHECK_EVAL("~1 & 1", "0");
    CHECK_EVAL("~1 | 1", "-1");
    CHECK_EVAL("1 | 2 & 3", "3");
    CHECK_EVAL("(1 | 2) & 3", "3");
    CHECK_EVAL("1 | (2 & 3)", "3");
    CHECK_EVAL("2*~3", "-8");
    CHECK_EVAL("~3*2", "-8");
    CHECK_EVAL("~(3*2)", "-7");
    CHECK_EVAL("~2^3&7", "7");
    CHECK_EVAL("~(2^3&7)", "-1");
    CHECK_EVAL("(~2^3)&7", "7");
    CHECK_EVAL("~(~(~1))", "-2");
    CHECK_EVAL("~(~(~(~1)))", "1");
    CHECK_EVAL("~5", "-6");
    CHECK_EVAL("~(~5)", "5");
    CHECK_EVAL("~(~(~5))", "-6");
    CHECK_EVAL("~(2^3) + ~3! * (~1 & 3) << 1", "-46");

    // Regression: keep unary minus and bitwise complement/function-not consistent.
    CHECK_EVAL("not(-1)", "0");
    CHECK_EVAL("~(-1)", "0");
    CHECK_EVAL("-not(-1)", "0");
    CHECK_EVAL("-~(-1)", "0");

    CHECK_EVAL("not(not(not(not(1))))", "1");
    CHECK_EVAL("not(-not(1))", "-3");
    CHECK_EVAL("-not(-1)", "0");
    CHECK_EVAL("not(not(-1))", "-1");
    CHECK_EVAL("(not(2))^3", "-27");
    CHECK_EVAL("not(2^3)", "-9");
    CHECK_EVAL("not(2^3)", "-9");
    CHECK_EVAL("not(-5)", "4");
    CHECK_EVAL("not(0) >> 1", "-1");
    CHECK_EVAL("not(0) << 1", "-2");
    CHECK_EVAL("not(1) & 1", "0");
    CHECK_EVAL("not(1) | 1", "-1");
    CHECK_EVAL("2*not(3)", "-8");
    CHECK_EVAL("not(3)*2", "-8");
    CHECK_EVAL("not(3*2)", "-7");
    CHECK_EVAL("not(2^3&7)", "-1");
    CHECK_EVAL("not(2^3&7)", "-1");
    CHECK_EVAL("(not(2^3))&7", "7");
    CHECK_EVAL("not(not(not(1)))", "-2");
    CHECK_EVAL("not(not(not(not(1))))", "1");
    CHECK_EVAL("not(5)", "-6");
    CHECK_EVAL("not(not(5))", "5");
    CHECK_EVAL("not(not(not(5)))", "-6");
    CHECK_EVAL("not(2^3) + not(3!) * (not(1) & 3) << 1", "-46");
}

void test_divide_by_zero()
{
    CHECK_DIV_BY_ZERO("1/0");
    CHECK_DIV_BY_ZERO("1 / sin 0");
    CHECK_DIV_BY_ZERO("1/sin(pi)");
    CHECK_DIV_BY_ZERO("1/sin (pi");
    CHECK_DIV_BY_ZERO("1/sin  pi");
    CHECK_DIV_BY_ZERO("1 / cos (pi/2)");
    CHECK_DIV_BY_ZERO("1 / cos(pi/2");
    CHECK_DIV_BY_ZERO("1/cos(pi/2)");
    CHECK_DIV_BY_ZERO("1/tan(0)");
    CHECK_DIV_BY_ZERO("1/tan 0");
    CHECK_DIV_BY_ZERO("-1/trunc(0.123)");
    CHECK_DIV_BY_ZERO("1/round(0.456)");
    CHECK_DIV_BY_ZERO("-1/binompmf(1;10;0)");
    CHECK_DIV_BY_ZERO("-345.3 / binompmf (1; 10; 0)");
    CHECK_DIV_BY_ZERO("mod(1;0)");
    CHECK_DIV_BY_ZERO("emod(1;0)");
    CHECK_DIV_BY_ZERO("powmod(2;10;0)");
}

void test_radix_char()
{
    // Backup current settings
    Settings* settings = Settings::instance();
    char radixCharacter = settings->radixCharacter();

    settings->setRadixCharacter('*');

    CHECK_EVAL("1+.5", "1.5");
    CHECK_EVAL("1+,5", "1.5");
    CHECK_EVAL(".5*,5", "0.25");
    CHECK_EVAL("1.01+2,02", "3.03");

    CHECK_EVAL("0b1.0 + 1", "2");
    CHECK_EVAL("0b1.1 * 1", "1.5");
    CHECK_EVAL("0b01.010 / 1", "1.25");
    CHECK_EVAL("-0b.010 - 1", "-1.25");

    CHECK_EVAL("0o.7 + 1", "1.875");
    CHECK_EVAL("-0o.7 + 1", "0.125");

    CHECK_EVAL("0x.f + 1", "1.9375");
    CHECK_EVAL("-0x.f + 1", "0.0625");

    CHECK_EVAL("1/.1", "10"); // ISSUE 151
    CHECK_EVAL("1/,1", "10"); // ISSUE 151

    // Test automatic detection of radix point when multiple choices are possible
    CHECK_EVAL("1,234.567", "1234.567");
    CHECK_EVAL("1.234,567", "1234.567");
    CHECK_EVAL("1,2,3", "123");
    CHECK_EVAL("1.2.3", "123");
    CHECK_EVAL("1,234,567.89", "1234567.89");
    CHECK_EVAL("1.234.567,89", "1234567.89");
    CHECK_EVAL("1,234.567,89", "1234.56789");
    CHECK_EVAL("1.234,567.89", "1234.56789");

    settings->setRadixCharacter('.');

    CHECK_EVAL("1+0.5", "1.5");
    CHECK_EVAL("1+0,5", "1.5");
    CHECK_EVAL("1/.1", "10");
    CHECK_EVAL("1/,1", "10");
    CHECK_EVAL("1,234.567", "1234.567");
    CHECK_EVAL("1.234,567", "1234.567");
    CHECK_EVAL("1,2,3", "123");
    CHECK_EVAL("1.2.3", "123");
    CHECK_EVAL("1,234,567.89", "1234567.89");
    CHECK_EVAL("1.234.567,89", "1234567.89");
    CHECK_EVAL("1,234.567,89", "1234.56789");
    CHECK_EVAL("1.234,567.89", "1234.56789");

    settings->setRadixCharacter(',');

    CHECK_EVAL("1+0.5", "1.5");
    CHECK_EVAL("1+0,5", "1.5");
    CHECK_EVAL("1/.1", "10");
    CHECK_EVAL("1/,1", "10");
    CHECK_EVAL("1,234.567", "1234.567");
    CHECK_EVAL("1.234,567", "1234.567");
    CHECK_EVAL("1,2,3", "123");
    CHECK_EVAL("1.2.3", "123");
    CHECK_EVAL("1,234,567.89", "1234567.89");
    CHECK_EVAL("1.234.567,89", "1234567.89");
    CHECK_EVAL("1,234.567,89", "1234.56789");
    CHECK_EVAL("1.234,567.89", "1234.56789");

    // Restore old settings
    settings->setRadixCharacter(radixCharacter);
}

void test_thousand_sep()
{
    // Common separators.
    CHECK_EVAL("12_345.678_9", "12345.6789");
    CHECK_EVAL("1234_5.67_89", "12345.6789");
    CHECK_EVAL("1234_56", "123456");
    CHECK_EVAL("_123456", "123456");
    CHECK_EVAL("123456_", "123456");
    CHECK_EVAL("123___456", "123456");
    CHECK_EVAL("._123456", "0.123456");

    CHECK_EVAL("12 345.678 9", "12345.6789");
//    CHECK_EVAL("12'345.678'9", "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12·345.678·9"), "37333.224");
    CHECK_EVAL(QString::fromUtf8("12٫345.678٫9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12٬345.678٬9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12˙345.678˙9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12⎖345.678⎖9"), "12345.6789");

    // Punctuation/symbol separators.
    CHECK_EVAL("12$345.678~9", "12345.6789");
    CHECK_EVAL("12`345.678@9", "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12€345.678€9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12£345.678£9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12¥345.678¥9"), "12345.6789");
    CHECK_EVAL(QString::fromUtf8("12₩345.678₩9"), "12345.6789");

    // Prefix/suffix separators are still accepted.
    CHECK_EVAL("$ 1234.567", "1234.567");
    CHECK_EVAL("1234.567 $", "1234.567");
    CHECK_EVAL(QString::fromUtf8("€ 1234.567"), "1234.567");
    CHECK_EVAL(QString::fromUtf8("1234.567 ¥"), "1234.567");
    CHECK_EVAL("$-10", "-10");
    CHECK_EVAL("$+10", "10");

    // Unicode letters between digits must not be silently ignored.
    CHECK_EVAL_FAIL(QString::fromUtf8("12天2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12é2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12ä2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12ß2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12Ж2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12π2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12Ω2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12क2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12م2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12あ2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12한2"));

    CHECK_EVAL_FAIL(QString::fromUtf8("1天234"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12天345.67"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12.34天56"));
    CHECK_EVAL_FAIL(QString::fromUtf8("12e3天4"));
    CHECK_EVAL_FAIL(QString::fromUtf8("0x1天2"));
    CHECK_EVAL_FAIL(QString::fromUtf8("0b10天01"));
    CHECK_EVAL_FAIL(QString::fromUtf8("0o12天34"));
}

void test_number_format_decimal_separator()
{
    Settings* settings = Settings::instance();
    const Settings::NumberFormatStyle oldNumberFormatStyle = settings->numberFormatStyle;

    settings->numberFormatStyle = Settings::NumberFormatNoGroupingDot;
    settings->applyNumberFormatStyle();
    CHECK_EVAL_FORMAT_EXACT("1/2", QStringLiteral("0.5"));

    settings->numberFormatStyle = Settings::NumberFormatNoGroupingComma;
    settings->applyNumberFormatStyle();
    CHECK_EVAL_FORMAT_EXACT("1/2", QStringLiteral("0,5"));

    settings->numberFormatStyle = oldNumberFormatStyle;
    settings->applyNumberFormatStyle();
}

void test_number_format_styles_matrix()
{
    struct NumberFormatCase {
        Settings::NumberFormatStyle style;
        QString base;
        QString shortValue;
        QString negative;
        QString integerOnly;
    };

    const NumberFormatCase cases[] = {
        {Settings::NumberFormatNoGroupingDot, QStringLiteral("1234567.1234567"),
            QStringLiteral("1234.56"), QStringLiteral("-1234567.1234567"), QStringLiteral("1234567")},
        {Settings::NumberFormatNoGroupingComma, QStringLiteral("1234567,1234567"),
            QStringLiteral("1234,56"), QStringLiteral("-1234567,1234567"), QStringLiteral("1234567")},
        {Settings::NumberFormatSIDot, QStringLiteral("1 234 567.123 456 7"),
            QStringLiteral("1 234.56"), QStringLiteral("-1 234 567.123 456 7"), QStringLiteral("1 234 567")},
        {Settings::NumberFormatSIComma, QStringLiteral("1 234 567,123 456 7"),
            QStringLiteral("1 234,56"), QStringLiteral("-1 234 567,123 456 7"), QStringLiteral("1 234 567")},
        {Settings::NumberFormatThreeDigitCommaDot, QStringLiteral("1,234,567.1234567"),
            QStringLiteral("1,234.56"), QStringLiteral("-1,234,567.1234567"), QStringLiteral("1,234,567")},
        {Settings::NumberFormatThreeDigitCommaDotFraction, QStringLiteral("1,234,567.123,456,7"),
            QStringLiteral("1,234.56"), QStringLiteral("-1,234,567.123,456,7"), QStringLiteral("1,234,567")},
        {Settings::NumberFormatThreeDigitDotComma, QStringLiteral("1.234.567,1234567"),
            QStringLiteral("1.234,56"), QStringLiteral("-1.234.567,1234567"), QStringLiteral("1.234.567")},
        {Settings::NumberFormatThreeDigitDotCommaFraction, QStringLiteral("1.234.567,123.456.7"),
            QStringLiteral("1.234,56"), QStringLiteral("-1.234.567,123.456.7"), QStringLiteral("1.234.567")},
        {Settings::NumberFormatThreeDigitSpaceDot, QStringLiteral("1 234 567.1234567"),
            QStringLiteral("1 234.56"), QStringLiteral("-1 234 567.1234567"), QStringLiteral("1 234 567")},
        {Settings::NumberFormatThreeDigitSpaceComma, QStringLiteral("1 234 567,1234567"),
            QStringLiteral("1 234,56"), QStringLiteral("-1 234 567,1234567"), QStringLiteral("1 234 567")},
        {Settings::NumberFormatThreeDigitUnderscoreDot, QStringLiteral("1_234_567.1234567"),
            QStringLiteral("1_234.56"), QStringLiteral("-1_234_567.1234567"), QStringLiteral("1_234_567")},
        {Settings::NumberFormatThreeDigitUnderscoreDotFraction, QStringLiteral("1_234_567.123_456_7"),
            QStringLiteral("1_234.56"), QStringLiteral("-1_234_567.123_456_7"), QStringLiteral("1_234_567")},
        {Settings::NumberFormatThreeDigitUnderscoreComma, QStringLiteral("1_234_567,1234567"),
            QStringLiteral("1_234,56"), QStringLiteral("-1_234_567,1234567"), QStringLiteral("1_234_567")},
        {Settings::NumberFormatThreeDigitUnderscoreCommaFraction, QStringLiteral("1_234_567,123_456_7"),
            QStringLiteral("1_234,56"), QStringLiteral("-1_234_567,123_456_7"), QStringLiteral("1_234_567")},
        {Settings::NumberFormatIndianCommaDot, QStringLiteral("12,34,567.1234567"),
            QStringLiteral("1,234.56"), QStringLiteral("-12,34,567.1234567"), QStringLiteral("12,34,567")}
    };

    Settings* settings = Settings::instance();
    const Settings::NumberFormatStyle oldNumberFormatStyle = settings->numberFormatStyle;
    const bool oldDigitGroupingIntegerPartOnly = settings->digitGroupingIntegerPartOnly;

    auto checkValue = [&](const QString& label, const QString& actual, const QString& expected, int style) {
        ++eval_total_tests;
        if (actual != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label.toUtf8().constData() << "\t[NEW]" << endl
                 << "\tStyle    : " << style << endl
                 << "\tResult   : " << actual.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    };

    for (const auto& item : cases) {
        settings->numberFormatStyle = item.style;
        settings->applyNumberFormatStyle();

        checkValue(
            QStringLiteral("base literal formatting"),
            DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("1234567.1234567")),
            item.base,
            static_cast<int>(item.style));

        checkValue(
            QStringLiteral("short literal formatting"),
            DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("1234.56")),
            item.shortValue,
            static_cast<int>(item.style));

        checkValue(
            QStringLiteral("negative literal formatting"),
            DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("-1234567.1234567")),
            item.negative,
            static_cast<int>(item.style));

        checkValue(
            QStringLiteral("integer literal formatting"),
            DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("1234567")),
            item.integerOnly,
            static_cast<int>(item.style));
    }

    settings->numberFormatStyle = oldNumberFormatStyle;
    settings->applyNumberFormatStyle();
    settings->digitGroupingIntegerPartOnly = oldDigitGroupingIntegerPartOnly;
}

void test_tolerant_number_input_with_dot_style()
{
    Settings* settings = Settings::instance();
    const Settings::NumberFormatStyle oldNumberFormatStyle = settings->numberFormatStyle;
    settings->numberFormatStyle = Settings::NumberFormatNoGroupingDot;
    settings->applyNumberFormatStyle();

    CHECK_EVAL("1,56", "1.56");
    CHECK_EVAL("12,56", "12.56");
    CHECK_EVAL("123,56", "123.56");
    CHECK_EVAL("1234,56", "1234.56");
    CHECK_EVAL("1,234.56", "1234.56");
    CHECK_EVAL("1.234,56", "1234.56");
    CHECK_EVAL("1,23456", "1.23456");
    CHECK_EVAL("1.23456", "1.23456");
    CHECK_EVAL("1,234,566.789,012", "1234566.789012");
    CHECK_EVAL("1.234.566,789.012", "1234566.789012");

    settings->numberFormatStyle = oldNumberFormatStyle;
    settings->applyNumberFormatStyle();
}

void test_number_format_style_behavior_all_supported_styles()
{
    struct NumberFormatBehaviorCase {
        Settings::NumberFormatStyle style;
        QString displayExpected;
    };

    const NumberFormatBehaviorCase cases[] = {
        {Settings::NumberFormatNoGroupingDot, QStringLiteral("1234567.12345")},
        {Settings::NumberFormatNoGroupingComma, QStringLiteral("1234567,12345")},
        {Settings::NumberFormatThreeDigitCommaDot, QStringLiteral("1,234,567.12345")},
        {Settings::NumberFormatThreeDigitDotComma, QStringLiteral("1.234.567,12345")},
        {Settings::NumberFormatThreeDigitSpaceDot, QStringLiteral("1 234 567.12345")},
        {Settings::NumberFormatSIDot, QStringLiteral("1 234 567.123 45")},
        {Settings::NumberFormatThreeDigitSpaceComma, QStringLiteral("1 234 567,12345")},
        {Settings::NumberFormatSIComma, QStringLiteral("1 234 567,123 45")},
        {Settings::NumberFormatThreeDigitCommaDotFraction, QStringLiteral("1,234,567.123,45")},
        {Settings::NumberFormatThreeDigitDotCommaFraction, QStringLiteral("1.234.567,123.45")},
        {Settings::NumberFormatThreeDigitUnderscoreDot, QStringLiteral("1_234_567.12345")},
        {Settings::NumberFormatThreeDigitUnderscoreDotFraction, QStringLiteral("1_234_567.123_45")},
        {Settings::NumberFormatThreeDigitUnderscoreComma, QStringLiteral("1_234_567,12345")},
        {Settings::NumberFormatThreeDigitUnderscoreCommaFraction, QStringLiteral("1_234_567,123_45")},
        {Settings::NumberFormatIndianCommaDot, QStringLiteral("12,34,567.12345")}
    };

    Settings* settings = Settings::instance();
    const Settings::NumberFormatStyle oldNumberFormatStyle = settings->numberFormatStyle;

    auto checkValue = [&](const QString& label, const QString& actual, const QString& expected, int style) {
        ++eval_total_tests;
        if (actual != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label.toUtf8().constData() << "\t[NEW]" << endl
                 << "\tStyle    : " << style << endl
                 << "\tResult   : " << actual.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    };

    for (const auto& item : cases) {
        settings->numberFormatStyle = item.style;
        settings->applyNumberFormatStyle();

        checkValue(
            QStringLiteral("style display sample formatting"),
            DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("1234567.12345")),
            item.displayExpected,
            static_cast<int>(item.style));

        eval->setExpression(QStringLiteral("1234567.12345"));
        Quantity evaluated = eval->evalUpdateAns();
        QString exact = NumberFormatter::format(evaluated);
        exact.replace(QString::fromUtf8("−"), "-");

        QString exactExpected = QStringLiteral("1234567");
        exactExpected += QChar(settings->decimalSeparator());
        exactExpected += QStringLiteral("12345");
        checkValue(
            QStringLiteral("style selected exact evaluation formatting"),
            exact,
            exactExpected,
            static_cast<int>(item.style));
    }

    settings->numberFormatStyle = oldNumberFormatStyle;
    settings->applyNumberFormatStyle();
}

void test_tolerant_number_input_all_styles()
{
    Settings* settings = Settings::instance();
    const Settings::NumberFormatStyle oldNumberFormatStyle = settings->numberFormatStyle;

    const Settings::NumberFormatStyle styles[] = {
        Settings::NumberFormatNoGroupingDot,
        Settings::NumberFormatNoGroupingComma,
        Settings::NumberFormatSIDot,
        Settings::NumberFormatSIComma,
        Settings::NumberFormatThreeDigitCommaDot,
        Settings::NumberFormatThreeDigitCommaDotFraction,
        Settings::NumberFormatThreeDigitDotComma,
        Settings::NumberFormatThreeDigitDotCommaFraction,
        Settings::NumberFormatThreeDigitSpaceDot,
        Settings::NumberFormatThreeDigitSpaceComma,
        Settings::NumberFormatThreeDigitUnderscoreDot,
        Settings::NumberFormatThreeDigitUnderscoreDotFraction,
        Settings::NumberFormatThreeDigitUnderscoreComma,
        Settings::NumberFormatThreeDigitUnderscoreCommaFraction,
        Settings::NumberFormatIndianCommaDot
    };

    const struct {
        const char* expr;
        const char* expected;
    } cases[] = {
        {"1,56", "1.56"},
        {"12,56", "12.56"},
        {"123,56", "123.56"},
        {"1234,56", "1234.56"},
        {"1,234.56", "1234.56"},
        {"1.234,56", "1234.56"},
        {"1,23456", "1.23456"},
        {"1.23456", "1.23456"},
        {"1,234,566.789,012", "1234566.789012"},
        {"1.234.566,789.012", "1234566.789012"},
        // Ambiguous mixed separators: keep parsing permissive for copy/paste.
        {"1.123123,123123,32", "1.12312312312332"},
        {"1,234,566.564,454545,45.23", "12345665644545454523"},
        {"1.23.23,42342 242.4", "12323.423422424"},
        {"12,3.23,42342 242.4", "12323423422424"}
    };

    for (const auto style : styles) {
        settings->numberFormatStyle = style;
        settings->applyNumberFormatStyle();
        for (const auto& c : cases)
            CHECK_EVAL(c.expr, c.expected);
    }

    settings->numberFormatStyle = oldNumberFormatStyle;
    settings->applyNumberFormatStyle();
}

void test_settings_default_result_line_behavior()
{
    Settings* settings = Settings::instance();

    const bool oldComplexNumbers = settings->complexNumbers;
    const bool oldMultipleLinesEnabled = settings->multipleResultLinesEnabled;
    const bool oldSecondaryEnabled = settings->secondaryResultEnabled;
    const bool oldTertiaryEnabled = settings->tertiaryResultEnabled;
    const bool oldQuaternaryEnabled = settings->quaternaryResultEnabled;
    const bool oldQuinaryEnabled = settings->quinaryResultEnabled;
    const char oldImaginaryUnit = settings->imaginaryUnit;
    const Settings::NumberFormatStyle oldNumberFormatStyle = settings->numberFormatStyle;

    // Emulate first-launch defaults for this behavior check.
    settings->complexNumbers = false;
    settings->multipleResultLinesEnabled = false;
    settings->secondaryResultEnabled = false;
    settings->tertiaryResultEnabled = false;
    settings->quaternaryResultEnabled = false;
    settings->quinaryResultEnabled = false;
    settings->imaginaryUnit = 'i';
    settings->numberFormatStyle = Settings::NumberFormatNoGroupingDot;
    settings->applyNumberFormatStyle();

    auto checkBool = [](const char* file, int line, const char* label, bool actual, bool expected) {
        ++eval_total_tests;
        if (actual != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << file << "[" << line << "]\t" << label << "\t[NEW]" << endl
                 << "\tResult   : " << (actual ? "true" : "false") << endl
                 << "\tExpected : " << (expected ? "true" : "false") << endl;
        }
    };

    auto checkChar = [](const char* file, int line, const char* label, char actual, char expected) {
        ++eval_total_tests;
        if (actual != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << file << "[" << line << "]\t" << label << "\t[NEW]" << endl
                 << "\tResult   : " << actual << endl
                 << "\tExpected : " << expected << endl;
        }
    };

    auto checkInt = [](const char* file, int line, const char* label, int actual, int expected) {
        ++eval_total_tests;
        if (actual != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << file << "[" << line << "]\t" << label << "\t[NEW]" << endl
                 << "\tResult   : " << actual << endl
                 << "\tExpected : " << expected << endl;
        }
    };

    checkBool(__FILE__, __LINE__, "complex numbers default disabled", settings->complexNumbers, false);
    checkBool(__FILE__, __LINE__, "multiple result lines default disabled",
              settings->multipleResultLinesEnabled, false);
    checkBool(__FILE__, __LINE__, "secondary line default disabled", settings->secondaryResultEnabled, false);
    checkBool(__FILE__, __LINE__, "tertiary line default disabled", settings->tertiaryResultEnabled, false);
    checkBool(__FILE__, __LINE__, "quaternary line default disabled", settings->quaternaryResultEnabled, false);
    checkBool(__FILE__, __LINE__, "quinary line default disabled", settings->quinaryResultEnabled, false);
    checkChar(__FILE__, __LINE__, "default imaginary unit", settings->imaginaryUnit, 'i');
    checkInt(__FILE__, __LINE__, "default number format style",
             static_cast<int>(settings->numberFormatStyle),
             static_cast<int>(Settings::NumberFormatNoGroupingDot));

    settings->complexNumbers = oldComplexNumbers;
    settings->multipleResultLinesEnabled = oldMultipleLinesEnabled;
    settings->secondaryResultEnabled = oldSecondaryEnabled;
    settings->tertiaryResultEnabled = oldTertiaryEnabled;
    settings->quaternaryResultEnabled = oldQuaternaryEnabled;
    settings->quinaryResultEnabled = oldQuinaryEnabled;
    settings->imaginaryUnit = oldImaginaryUnit;
    settings->numberFormatStyle = oldNumberFormatStyle;
    settings->applyNumberFormatStyle();
}

void test_extra_result_lines_profile_formatting()
{
    Settings* settings = Settings::instance();
    const bool oldMultipleResultLinesEnabled = settings->multipleResultLinesEnabled;
    const bool oldSecondaryResultEnabled = settings->secondaryResultEnabled;
    const bool oldTertiaryResultEnabled = settings->tertiaryResultEnabled;
    const bool oldQuaternaryResultEnabled = settings->quaternaryResultEnabled;
    const bool oldQuinaryResultEnabled = settings->quinaryResultEnabled;
    const char oldAlternativeResultFormat = settings->alternativeResultFormat;
    const char oldTertiaryResultFormat = settings->tertiaryResultFormat;
    const char oldQuaternaryResultFormat = settings->quaternaryResultFormat;
    const char oldQuinaryResultFormat = settings->quinaryResultFormat;
    const int oldSecondaryResultPrecision = settings->secondaryResultPrecision;
    const int oldTertiaryResultPrecision = settings->tertiaryResultPrecision;
    const int oldQuaternaryResultPrecision = settings->quaternaryResultPrecision;
    const int oldQuinaryResultPrecision = settings->quinaryResultPrecision;
    const bool oldComplexNumbers = settings->complexNumbers;
    const bool oldSecondaryComplexNumbers = settings->secondaryComplexNumbers;
    const bool oldTertiaryComplexNumbers = settings->tertiaryComplexNumbers;
    const bool oldQuaternaryComplexNumbers = settings->quaternaryComplexNumbers;
    const bool oldQuinaryComplexNumbers = settings->quinaryComplexNumbers;
    const char oldSecondaryResultFormatComplex = settings->secondaryResultFormatComplex;
    const char oldTertiaryResultFormatComplex = settings->tertiaryResultFormatComplex;
    const char oldQuaternaryResultFormatComplex = settings->quaternaryResultFormatComplex;
    const char oldQuinaryResultFormatComplex = settings->quinaryResultFormatComplex;
    const char oldImaginaryUnit = settings->imaginaryUnit;
    const Settings::NumberFormatStyle oldNumberFormatStyle = settings->numberFormatStyle;

    settings->numberFormatStyle = Settings::NumberFormatNoGroupingDot;
    settings->applyNumberFormatStyle();
    settings->multipleResultLinesEnabled = true;
    settings->secondaryResultEnabled = true;
    settings->tertiaryResultEnabled = true;
    settings->quaternaryResultEnabled = true;
    settings->quinaryResultEnabled = true;
    settings->alternativeResultFormat = 'f';
    settings->tertiaryResultFormat = 'e';
    settings->quaternaryResultFormat = 'b';
    settings->quinaryResultFormat = 'r';
    settings->secondaryResultPrecision = 2;
    settings->tertiaryResultPrecision = 4;
    settings->quaternaryResultPrecision = -1;
    settings->quinaryResultPrecision = -1;
    settings->complexNumbers = true;
    settings->secondaryComplexNumbers = true;
    settings->tertiaryComplexNumbers = true;
    settings->quaternaryComplexNumbers = true;
    settings->quinaryComplexNumbers = true;
    settings->secondaryResultFormatComplex = 'c';
    settings->tertiaryResultFormatComplex = 'p';
    settings->quaternaryResultFormatComplex = 'a';
    settings->quinaryResultFormatComplex = 'c';
    settings->imaginaryUnit = 'j';
    CMath::setImaginaryUnitSymbol(QLatin1Char('j'));
    DMath::complexMode = true;
    eval->initializeBuiltInVariables();

    auto checkCondition = [](const char* file, int line, const char* label, bool condition,
                             const QString& result, const QString& expectedHint) {
        ++eval_total_tests;
        if (!condition) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << file << "[" << line << "]\t" << label << "\t[NEW]" << endl
                 << "\tResult   : " << result.toUtf8().constData() << endl
                 << "\tExpected : " << expectedHint.toUtf8().constData() << endl;
        }
    };

    eval->setExpression(QStringLiteral("1234.56789"));
    const Quantity realValue = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_total_tests;
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\treal value eval for extra line profiles\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QString line2 = NumberFormatter::format(realValue,
                                                      settings->alternativeResultFormat,
                                                      settings->secondaryResultPrecision,
                                                      settings->complexNumbers && settings->secondaryComplexNumbers,
                                                      settings->secondaryResultFormatComplex);
        const QString line3 = NumberFormatter::format(realValue,
                                                      settings->tertiaryResultFormat,
                                                      settings->tertiaryResultPrecision,
                                                      settings->complexNumbers && settings->tertiaryComplexNumbers,
                                                      settings->tertiaryResultFormatComplex);
        const QString line4 = NumberFormatter::format(realValue,
                                                      settings->quaternaryResultFormat,
                                                      settings->quaternaryResultPrecision,
                                                      settings->complexNumbers && settings->quaternaryComplexNumbers,
                                                      settings->quaternaryResultFormatComplex);
        const QString line5 = NumberFormatter::format(realValue,
                                                      settings->quinaryResultFormat,
                                                      settings->quinaryResultPrecision,
                                                      settings->complexNumbers && settings->quinaryComplexNumbers,
                                                      settings->quinaryResultFormatComplex);

        checkCondition(__FILE__, __LINE__, "line2 fixed precision", line2 == QStringLiteral("1234.57"),
                       line2, QStringLiteral("1234.57"));
        checkCondition(__FILE__, __LINE__, "line3 scientific notation", line3.contains(QLatin1Char('e')),
                       line3, QStringLiteral("contains 'e'"));
        checkCondition(__FILE__, __LINE__, "line4 binary notation", line4.startsWith(QStringLiteral("0b")),
                       line4, QStringLiteral("starts with 0b"));
        checkCondition(__FILE__, __LINE__, "line5 rational notation", line5.contains(QLatin1Char('/')),
                       line5, QStringLiteral("contains '/'"));
    }

    eval->setExpression(QStringLiteral("1+1j"));
    const Quantity complexValue = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_total_tests;
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tcomplex value eval for extra line profiles\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QString cartesian = NumberFormatter::format(complexValue, 'f', 3, true, 'c');
        const QString polarExp = NumberFormatter::format(complexValue, 'f', 3, true, 'p');
        const QString polarAngle = NumberFormatter::format(complexValue, 'f', 3, true, 'a');
        checkCondition(__FILE__, __LINE__, "cartesian complex form", cartesian.contains(QLatin1Char('j')),
                       cartesian, QStringLiteral("contains imaginary unit j"));
        checkCondition(__FILE__, __LINE__, "polar exponential complex form", polarExp.contains(QStringLiteral("exp(")),
                       polarExp, QStringLiteral("contains exp("));
        checkCondition(__FILE__, __LINE__, "polar angle complex form", polarAngle.contains(QString::fromUtf8("∠")),
                       polarAngle, QString::fromUtf8("contains ∠"));
    }

    settings->multipleResultLinesEnabled = oldMultipleResultLinesEnabled;
    settings->secondaryResultEnabled = oldSecondaryResultEnabled;
    settings->tertiaryResultEnabled = oldTertiaryResultEnabled;
    settings->quaternaryResultEnabled = oldQuaternaryResultEnabled;
    settings->quinaryResultEnabled = oldQuinaryResultEnabled;
    settings->alternativeResultFormat = oldAlternativeResultFormat;
    settings->tertiaryResultFormat = oldTertiaryResultFormat;
    settings->quaternaryResultFormat = oldQuaternaryResultFormat;
    settings->quinaryResultFormat = oldQuinaryResultFormat;
    settings->secondaryResultPrecision = oldSecondaryResultPrecision;
    settings->tertiaryResultPrecision = oldTertiaryResultPrecision;
    settings->quaternaryResultPrecision = oldQuaternaryResultPrecision;
    settings->quinaryResultPrecision = oldQuinaryResultPrecision;
    settings->complexNumbers = oldComplexNumbers;
    settings->secondaryComplexNumbers = oldSecondaryComplexNumbers;
    settings->tertiaryComplexNumbers = oldTertiaryComplexNumbers;
    settings->quaternaryComplexNumbers = oldQuaternaryComplexNumbers;
    settings->quinaryComplexNumbers = oldQuinaryComplexNumbers;
    settings->secondaryResultFormatComplex = oldSecondaryResultFormatComplex;
    settings->tertiaryResultFormatComplex = oldTertiaryResultFormatComplex;
    settings->quaternaryResultFormatComplex = oldQuaternaryResultFormatComplex;
    settings->quinaryResultFormatComplex = oldQuinaryResultFormatComplex;
    settings->imaginaryUnit = oldImaginaryUnit;
    CMath::setImaginaryUnitSymbol(QChar(oldImaginaryUnit));
    DMath::complexMode = oldComplexNumbers;
    eval->initializeBuiltInVariables();
    settings->numberFormatStyle = oldNumberFormatStyle;
    settings->applyNumberFormatStyle();
}

void test_sexagesimal()
{
    // Backup current settings
    Settings* settings = Settings::instance();
    char angleUnit = settings->angleUnit;
    char resultFormat = settings->resultFormat;
    int resultPrecision = settings->resultPrecision;

    settings->angleUnit = 'd';
    settings->resultFormat = 's';
    settings->resultPrecision = 2;
    Evaluator::instance()->initializeAngleUnits();

    // Coverage for the documentation tables in doc/src/userguide/syntax.rst
    // ("Sexagesimal Values"): fixed-point decimal and sexagesimal columns.
    CHECK_EVAL("0", "0");
    CHECK_EVAL("°'56", "0.01555555555555555556°");
    CHECK_EVAL("56\"", "0.01555555555555555556°");
    CHECK_EVAL("56[arcsecond]", "0.01555555555555555556°");
    CHECK_EVAL("56[arcsec]", "0.01555555555555555556°");
    CHECK_EVAL("56[mas]", "0.00001555555555555556°");
    CHECK_EVAL("56[milliarcsecond]", "0.00001555555555555556°");
    CHECK_EVAL(QString::fromUtf8("56[µas]"), "0.00000001555555555556°");
    CHECK_EVAL("56[microarcsecond]", "0.00000001555555555556°");
    CHECK_EVAL("[mas] -> [milliarcsecond]", "1 milliarcsecond");
    CHECK_EVAL(QString::fromUtf8("[µas] -> [microarcsecond]"), "1 microarcsecond");
    CHECK_EVAL("[uas] -> [microarcsecond]", "1 microarcsecond");
    CHECK_EVAL("[milliarcsecond] -> [mas]", "1 mas");
    CHECK_EVAL("[microarcsecond] -> [uas]", "1 uas");
    CHECK_EVAL("1[mas] -> [arcsecond]", "0.001 arcsecond");
    CHECK_EVAL("1[mas] -> [arcsec]", "0.001 arcsec");
    CHECK_EVAL("1000[mas] -> [arcsecond]", "1 arcsecond");
    CHECK_EVAL("1000[milliarcsecond] -> [arcsec]", "1 arcsec");
    CHECK_EVAL(QString::fromUtf8("1[µas] -> [arcsecond]"), "0.000001 arcsecond");
    CHECK_EVAL("1[uas] -> [arcsec]", "0.000001 arcsec");
    CHECK_EVAL("1000000[uas] -> [arcsecond]", "1 arcsecond");
    CHECK_EVAL("1000000[microarcsecond] -> [arcsec]", "1 arcsec");
    CHECK_EVAL("1[arcsecond] -> [mas]", "1000 mas");
    CHECK_EVAL("1[arcsec] -> [milliarcsecond]", "1000 milliarcsecond");
    CHECK_EVAL(QString::fromUtf8("1[arcsecond] -> [µas]"), "1000000 µas");
    CHECK_EVAL("1[arcsec] -> [uas]", "1000000 uas");
    CHECK_EVAL("1[arcminute] -> [mas]", "60000 mas");
    CHECK_EVAL("1[arcmin] -> [milliarcsecond]", "60000 milliarcsecond");
    CHECK_EVAL(QString::fromUtf8("1[arcminute] -> [µas]"), "60000000 µas");
    CHECK_EVAL("1[arcmin] -> [uas]", "60000000 uas");
    CHECK_EVAL("60000[mas] -> [arcmin]", "1 arcmin");
    CHECK_EVAL("60000[milliarcsecond] -> [arcminute]", "1 arcminute");
    CHECK_EVAL(QString::fromUtf8("60000000[µas] -> [arcmin]"), "1 arcmin");
    CHECK_EVAL("60000000[uas] -> [arcminute]", "1 arcminute");
    CHECK_EVAL("3600000[mas] -> [degree]", "1°");
    CHECK_EVAL("3600000000[uas] -> [degree]", "1°");
    CHECK_EVAL("((123456789[mas] -> [arcsec]) -> [mas])", "123456789 mas");
    CHECK_EVAL("((123456789[milliarcsecond] -> [arcsecond]) -> [milliarcsecond])", "123456789 milliarcsecond");
    CHECK_EVAL(QString::fromUtf8("((123456789[µas] -> [arcsec]) -> [µas])"), "123456789 µas");
    CHECK_EVAL("((123456789[uas] -> [arcsecond]) -> [uas])", "123456789 uas");
    CHECK_EVAL("56.78\"", "0.01577222222222222222°");
    CHECK_EVAL("8.4381406e4\"", "23.43927944444444444444°");
    CHECK_EVAL(QString::fromUtf8("8.4381406e4″"), "23.43927944444444444444°");
    CHECK_EVAL(QString::fromUtf8("8.4381406e4 ″"), "23.43927944444444444444°");
    CHECK_EVAL("°34", "0.56666666666666666667°");
    CHECK_EVAL("34'", "0.56666666666666666667°");
    CHECK_EVAL(QString::fromUtf8("34′"), "0.56666666666666666667°");
    CHECK_EVAL("34[arcminute]", "0.56666666666666666667°");
    CHECK_EVAL("34[arcmin]", "0.56666666666666666667°");
    CHECK_EVAL("34'56", "0.58222222222222222222°");
    CHECK_EVAL(QString::fromUtf8("34′56″"), "0.58222222222222222222°");
    CHECK_EVAL("12°", "12°");
    CHECK_EVAL("12°34", "12.56666666666666666667°");
    CHECK_EVAL("12°34.5", "12.575°");
    CHECK_EVAL("12°34'56", "12.58222222222222222222°");
    CHECK_EVAL("12°34'56.78", "12.58243888888888888889°");

    CHECK_EVAL("0[second]", "0 second");
    CHECK_EVAL("::56", "56 second");
    CHECK_EVAL("56[second]", "56 second");
    CHECK_EVAL(":34", "2040 second");
    CHECK_EVAL("34[minute]", "2040 second");
    CHECK_EVAL("12:", "43200 second");
    CHECK_EVAL("12[hour]", "43200 second");
    CHECK_EVAL("12:34", "45240 second");
    CHECK_EVAL("12:34.5", "45270 second");
    CHECK_EVAL("12:34:56", "45296 second");
    CHECK_EVAL("12:34:56.78", "45296.78 second");

    CHECK_EVAL_FORMAT("0", "0°00′00″");
    CHECK_EVAL_FORMAT("°'56", "0°00′56.00″");
    CHECK_EVAL_FORMAT("56\"", "0°00′56.00″");
    CHECK_EVAL_FORMAT("56[arcsecond]", "0°00′56.00″");
    CHECK_EVAL_FORMAT("56.78\"", "0°00′56.78″");
    CHECK_EVAL_FORMAT("°34", "0°34′00.00″");
    CHECK_EVAL_FORMAT("34'", "0°34′00.00″");
    CHECK_EVAL_FORMAT("34[arcminute]", "0°34′00.00″");
    CHECK_EVAL_FORMAT("34'56", "0°34′56.00″");
    CHECK_EVAL_FORMAT("12°", "12°00′00.00″");
    CHECK_EVAL_FORMAT("12°34", "12°34′00.00″");
    CHECK_EVAL_FORMAT("12°34.5", "12°34′30.00″");
    CHECK_EVAL_FORMAT("12°34'56", "12°34′56.00″");
    CHECK_EVAL_FORMAT("12°34'56.78", "12°34′56.78″");

    CHECK_EVAL_FORMAT("0[second]", "0:00:00");
    CHECK_EVAL_FORMAT("::56", "0:00:56.00");
    CHECK_EVAL_FORMAT("56[second]", "0:00:56.00");
    CHECK_EVAL_FORMAT(":34", "0:34:00.00");
    CHECK_EVAL_FORMAT("34[minute]", "0:34:00.00");
    CHECK_EVAL_FORMAT("12:", "12:00:00.00");
    CHECK_EVAL_FORMAT("12[hour]", "12:00:00.00");
    CHECK_EVAL_FORMAT("12:34", "12:34:00.00");
    CHECK_EVAL_FORMAT("12:34.5", "12:34:30.00");
    CHECK_EVAL_FORMAT("12:34:56", "12:34:56.00");
    CHECK_EVAL_FORMAT("12:34:56.78", "12:34:56.78");

    CHECK_EVAL_FORMAT_FAIL(":");
    CHECK_EVAL_FORMAT_FAIL("::");
    CHECK_EVAL_FORMAT_FAIL("0:::");
    CHECK_EVAL_FORMAT_FAIL("1.2:34.56");
    CHECK_EVAL_FORMAT_FAIL("12:3.4:56.78");

    CHECK_EVAL_FORMAT("0:", "0:00:00");
    CHECK_EVAL_FORMAT("::56", "0:00:56.00");
    CHECK_EVAL_FORMAT("56[second]", "0:00:56.00");
    CHECK_EVAL_FORMAT(":34", "0:34:00.00");
    CHECK_EVAL_FORMAT(":34:", "0:34:00.00");
    CHECK_EVAL_FORMAT(":34.5:", "0:34:30.00");
    CHECK_EVAL_FORMAT("34.5[minute]", "0:34:30.00");
    CHECK_EVAL_FORMAT("12:", "12:00:00.00");
    CHECK_EVAL_FORMAT("12::", "12:00:00.00");
    CHECK_EVAL_FORMAT("12.3:", "12:18:00.00");
    CHECK_EVAL_FORMAT("12.3[hour]", "12:18:00.00");
    CHECK_EVAL_FORMAT("12:34", "12:34:00.00");
    CHECK_EVAL_FORMAT("12:34:", "12:34:00.00");
    CHECK_EVAL_FORMAT("12:34.", "12:34:00.00");
    CHECK_EVAL_FORMAT("12:34.5", "12:34:30.00");
    CHECK_EVAL_FORMAT("12:34:56", "12:34:56.00");
    CHECK_EVAL_FORMAT("12:34:56.", "12:34:56.00");
    CHECK_EVAL_FORMAT("12:34:56.78", "12:34:56.78");
    CHECK_EVAL("1:30 / 4:00", "0.375");
    CHECK_EVAL("(1:30) / (4:00)", "0.375");

    CHECK_EVAL_FORMAT("-0:", "0:00:00");
    CHECK_EVAL_FORMAT("-::56", "-0:00:56.00");
    CHECK_EVAL_FORMAT("-:34", "-0:34:00.00");
    CHECK_EVAL_FORMAT("-12:", "-12:00:00.00");
    CHECK_EVAL_FORMAT("-12:34", "-12:34:00.00");
    CHECK_EVAL_FORMAT("-12:34.5", "-12:34:30.00");
    CHECK_EVAL_FORMAT("-12:34:56", "-12:34:56.00");
    CHECK_EVAL_FORMAT("-12:34:56.78", "-12:34:56.78");

    CHECK_EVAL_FORMAT_FAIL("°");
    CHECK_EVAL_FORMAT_FAIL("°'");
    CHECK_EVAL_FORMAT_FAIL("°0\"");
    CHECK_EVAL_FORMAT_FAIL("1.2°34.56");
    CHECK_EVAL_FORMAT_FAIL("12°3.4'56.78\"");

    CHECK_EVAL_FORMAT_FAIL("56\"7");
    CHECK_EVAL_FORMAT_FAIL("56.78\"9");
    CHECK_EVAL_FORMAT_FAIL("34'\"5");
    CHECK_EVAL_FORMAT_FAIL("12°'\"3");
    CHECK_EVAL_FORMAT_FAIL("12°34'56.78\"9");

    CHECK_EVAL_FORMAT("0", "0°00′00″");
    CHECK_EVAL_FORMAT("°'56", "0°00′56.00″");
    CHECK_EVAL_FORMAT("'56", "0°00′56.00″");
    CHECK_EVAL_FORMAT("56\"", "0°00′56.00″");
    CHECK_EVAL_FORMAT("56[arcsecond]", "0°00′56.00″");
    CHECK_EVAL_FORMAT("56.78\"", "0°00′56.78″");
    CHECK_EVAL_FORMAT("°34", "0°34′00.00″");
    CHECK_EVAL_FORMAT("34'", "0°34′00.00″");
    CHECK_EVAL_FORMAT("34.5'", "0°34′30.00″");
    CHECK_EVAL_FORMAT("34'\"", "0°34′00.00″");
    CHECK_EVAL_FORMAT("34.5'\"", "0°34′30.00″");
    CHECK_EVAL_FORMAT("34.5[arcminute]", "0°34′30.00″");
    CHECK_EVAL_FORMAT("34'56", "0°34′56.00″");
    eval->setExpression(QStringLiteral("34'56"));
    eval->evalUpdateAns();
    ++eval_total_tests;
    if (eval->error().isEmpty() && eval->interpretedExpression().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tsexagesimal interpreted expression is not empty\t[NEW]" << endl
             << "\tExpression: 34'56" << endl;
    }
    eval->setExpression(QString::fromUtf8("°34"));
    eval->evalUpdateAns();
    ++eval_total_tests;
    if (eval->error().isEmpty() && eval->interpretedExpression().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tsexagesimal interpreted expression is not empty\t[NEW]" << endl
             << "\tExpression: °34" << endl;
    }
    CHECK_EVAL_FORMAT("12°", "12°00′00.00″");
    CHECK_EVAL_FORMAT(QString::fromUtf8("12º"), "12°00′00.00″");
    CHECK_EVAL_FORMAT(QString::fromUtf8("12˚"), "12°00′00.00″");
    CHECK_EVAL_FORMAT(QString::fromUtf8("12∘"), "12°00′00.00″");
    CHECK_EVAL_FORMAT("12°'", "12°00′00.00″");
    CHECK_EVAL_FORMAT("12°'\"", "12°00′00.00″");
    CHECK_EVAL_FORMAT("12.3°", "12°18′00.00″");
    CHECK_EVAL_FORMAT(QString::fromUtf8("12.3º"), "12°18′00.00″");
    CHECK_EVAL_FORMAT("12.3°'", "12°18′00.00″");
    CHECK_EVAL_FORMAT("12.3°'\"", "12°18′00.00″");
    CHECK_EVAL_FORMAT("12°34", "12°34′00.00″");
    CHECK_EVAL_FORMAT(QString::fromUtf8("12º34"), "12°34′00.00″");
    CHECK_EVAL_FORMAT("12°34'", "12°34′00.00″");
    CHECK_EVAL_FORMAT("12°34.", "12°34′00.00″");
    CHECK_EVAL_FORMAT("12°34.5", "12°34′30.00″");
    CHECK_EVAL_FORMAT("12°34.5'", "12°34′30.00″");
    CHECK_EVAL_FORMAT("12°34'56", "12°34′56.00″");
    CHECK_EVAL_FORMAT("12°34'56.", "12°34′56.00″");
    CHECK_EVAL_FORMAT("12°34'56.78", "12°34′56.78″");
    CHECK_EVAL_FORMAT("12°34'56.78\"", "12°34′56.78″");

    CHECK_EVAL_FORMAT("-0", "0°00′00″");
    CHECK_EVAL_FORMAT("-°'56", "-0°00′56.00″");
    CHECK_EVAL_FORMAT("-56\"", "-0°00′56.00″");
    CHECK_EVAL_FORMAT("-°34", "-0°34′00.00″");
    CHECK_EVAL_FORMAT("-34'", "-0°34′00.00″");
    CHECK_EVAL_FORMAT("-12°", "-12°00′00.00″");
    CHECK_EVAL_FORMAT("-12.3°", "-12°18′00.00″");
    CHECK_EVAL_FORMAT("-12°34", "-12°34′00.00″");
    CHECK_EVAL_FORMAT("-12°34.5", "-12°34′30.00″");
    CHECK_EVAL_FORMAT("-12°34'56", "-12°34′56.00″");
    CHECK_EVAL_FORMAT("-12°34'56.78", "-12°34′56.78″");
    CHECK_EVAL_FORMAT("-12°34'56.78\"", "-12°34′56.78″");

    settings->angleUnit = 'g';
    Evaluator::instance()->initializeAngleUnits();

    CHECK_EVAL_FORMAT("100.5", "90°27′00.00″");
    CHECK_EVAL_FORMAT("-200.3", "-180°16′12.00″");
    CHECK_EVAL_FORMAT("12°", "12°00′00.00″");
    CHECK_EVAL_FORMAT("12.3[degree]", "12°18′00.00″");

    settings->angleUnit = 'r';
    Evaluator::instance()->initializeAngleUnits();

    CHECK_EVAL_FORMAT("pi", "180°00′00.00″");
    CHECK_EVAL_FORMAT("-pi", "-180°00′00.00″");
    CHECK_EVAL_FORMAT("2*pi [rad]", "360°00′00.00″");
    CHECK_EVAL_FORMAT("12°", "12°00′00.00″");
    CHECK_EVAL_FORMAT("12.3[degree]", "12°18′00.00″");

    settings->angleUnit = 't';
    Evaluator::instance()->initializeAngleUnits();

    CHECK_EVAL_FORMAT("0.25", "90°00′00.00″");
    CHECK_EVAL_FORMAT("-0.5", "-180°00′00.00″");
    CHECK_EVAL_FORMAT("12°", "12°00′00.00″");
    CHECK_EVAL_FORMAT("12.3[degree]", "12°18′00.00″");

    settings->resultPrecision = 32;

    CHECK_EVAL_FORMAT("::56.78901234567890123456789012345678", "0:00:56.78901234567890123456789012345678");
    CHECK_EVAL_FORMAT(":34.56789012345678901234567890123456", "0:34:34.07340740740734074074073407407360");
    CHECK_EVAL_FORMAT("12.34567890123456789012345678901234:", "12:20:44.44404444444440444444444044442400");

    settings->angleUnit = 'd';
    Evaluator::instance()->initializeAngleUnits();

    CHECK_EVAL_FORMAT("56.78901234567890123456789012345678\"", "0°00′56.78901234567890123456789012345678″");
    CHECK_EVAL_FORMAT("34.56789012345678901234567890123456'", "0°34′34.07340740740734074074073407407360″");
    CHECK_EVAL_FORMAT("12.34567890123456789012345678901234°", "12°20′44.44404444444440444444444044442400″");

    // Round-trip drift checks: arcsec ⇄ DMS ⇄ radians and radians ⇄ arcsec.
    CHECK_EVAL(
        "((((12°34′56.7890123456″ -> [radian]) -> [arcsecond]) -> [radian]) - (12°34′56.7890123456″ -> [radian])) -> [radian]",
        "0 radian");
    CHECK_EVAL(
        "((((1.234567890123456e-4[radian] -> [arcsecond]) -> [radian]) - (1.234567890123456e-4[radian])) -> [radian])",
        "0 radian");

    // Restore old settings
    settings->angleUnit = angleUnit;
    settings->resultFormat = resultFormat;
    settings->resultPrecision = resultPrecision;
    Evaluator::instance()->initializeAngleUnits();
}

void test_rational_format()
{
    Settings* settings = Settings::instance();
    const char angleUnit = settings->angleUnit;
    const char resultFormat = settings->resultFormat;
    const int resultPrecision = settings->resultPrecision;
    const bool complexNumbers = settings->complexNumbers;
    const char imaginaryUnit = settings->imaginaryUnit;
    const char resultFormatComplex = settings->resultFormatComplex;
    const bool complexMode = DMath::complexMode;

    settings->resultFormat = 'r';
    settings->resultPrecision = -1;
    settings->complexNumbers = false;
    DMath::complexMode = false;
    eval->initializeBuiltInVariables();
    const QString dotOperator(MathDsl::MulDotOp);
    const QString dotSpacing = QString(MathDsl::MulDotWrapSp);
    const QString piSymbol = QStringLiteral("pi");
    const auto piOver = [&](int denominator) {
        return piSymbol + slash + QString::number(denominator);
    };
    const auto nPiOver = [&](int numerator, int denominator) {
        return QString::number(numerator) + dotSpacing + dotOperator + dotSpacing + piSymbol
            + slash + QString::number(denominator);
    };
    const auto nPi = [&](int numerator) {
        return QString::number(numerator) + dotSpacing + dotOperator + dotSpacing + piSymbol;
    };
    const auto signedPiOver = [&](int numerator, int denominator) -> QString {
        if (numerator == 1)
            return piOver(denominator);
        if (numerator == -1)
            return QStringLiteral("-") + piOver(denominator);
        return nPiOver(numerator, denominator);
    };

    ++eval_total_tests;
    QString direct = NumberFormatter::format(Quantity(HNumber("0.5")), 'r');
    direct.replace(QString::fromUtf8("−"), "-");
    const QString expectedDirect = QStringLiteral("1")
        + slash
        + QStringLiteral("2");
    if (direct != expectedDirect) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tdirect NumberFormatter rational formatting\t[NEW]" << endl
             << "\tResult   : " << direct.toLatin1().constData() << endl
             << "\tExpected : " << expectedDirect.toLatin1().constData() << endl;
    }

    CHECK_EVAL_FORMAT_HAS_SLASH("1/3");
    CHECK_EVAL_FORMAT_HAS_SLASH("2/4");
    CHECK_EVAL_FORMAT_HAS_SLASH("-10/4");
    CHECK_EVAL_FORMAT_MEDIUM_SPACED_SLASH("2/4");
    CHECK_EVAL_FORMAT("0", "0");
    CHECK_EVAL_FORMAT_HAS_SLASH("0.1");
    CHECK_EVAL_FORMAT_HAS_SLASH("1000001/1000000");
    CHECK_EVAL_FORMAT_HAS_SLASH("0.5[metre]");
    CHECK_EVAL_FORMAT_HAS_SLASH("-0.5[metre]");
    CHECK_EVAL_FORMAT_MEDIUM_SPACED_SLASH("0.5[metre]");
    CHECK_EVAL_FORMAT_HAS_SLASH("1/1000000");
    CHECK_EVAL_FORMAT_HAS_SLASH("355/113");

    // Must fall back to float for denominator overflow / non-rational / huge numerator.
    CHECK_EVAL_FORMAT_NO_SLASH("1/1000001");
    CHECK_EVAL_FORMAT_NO_SLASH("sqrt(2)");
    CHECK_EVAL_FORMAT_NO_SLASH("0.333333333333333333333333333333");
    CHECK_EVAL_FORMAT_NO_SLASH("3000000000");
    CHECK_EVAL_FORMAT_NO_SLASH("exp(1)");

    settings->angleUnit = 'r';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL_FORMAT_EXACT("arcsin(-1)", signedPiOver(-1, 2));
    CHECK_EVAL_FORMAT_EXACT("arcsin(-sqrt(3)/2)", signedPiOver(-1, 3));
    CHECK_EVAL_FORMAT_EXACT("arcsin(-sqrt(2)/2)", signedPiOver(-1, 4));
    CHECK_EVAL_FORMAT_EXACT("arcsin(-1/2)", signedPiOver(-1, 6));
    CHECK_EVAL_FORMAT_EXACT("arcsin(0)", "0");
    CHECK_EVAL_FORMAT_EXACT("arcsin(1/2)", piOver(6));
    CHECK_EVAL_FORMAT_EXACT("arcsin(sqrt(2)/2)", piOver(4));
    CHECK_EVAL_FORMAT_EXACT("arcsin(sqrt(3)/2)", piOver(3));
    CHECK_EVAL_FORMAT_EXACT("arcsin(1)", piOver(2));
    CHECK_EVAL_FORMAT_EXACT("arccos(-1)", piSymbol);
    CHECK_EVAL_FORMAT_EXACT("arccos(-sqrt(3)/2)", nPiOver(5, 6));
    CHECK_EVAL_FORMAT_EXACT("arccos(-sqrt(2)/2)", nPiOver(3, 4));
    CHECK_EVAL_FORMAT_EXACT("arccos(-1/2)", nPiOver(2, 3));
    CHECK_EVAL_FORMAT_EXACT("arccos(0)", piOver(2));
    CHECK_EVAL_FORMAT_EXACT("arccos(1/2)", piOver(3));
    CHECK_EVAL_FORMAT_EXACT("arccos(sqrt(2)/2)", piOver(4));
    CHECK_EVAL_FORMAT_EXACT("arccos(sqrt(3)/2)", piOver(6));
    CHECK_EVAL_FORMAT_EXACT("arccos(1)", "0");
    CHECK_EVAL_FORMAT_EXACT("arctan(-sqrt(3))", signedPiOver(-1, 3));
    CHECK_EVAL_FORMAT_EXACT("arctan(-1)", signedPiOver(-1, 4));
    CHECK_EVAL_FORMAT_EXACT("arctan(-sqrt(3)/3)", signedPiOver(-1, 6));
    CHECK_EVAL_FORMAT_EXACT("arctan(0)", "0");
    CHECK_EVAL_FORMAT_EXACT("arctan(sqrt(3)/3)", piOver(6));
    CHECK_EVAL_FORMAT_EXACT("arctan(1)", piOver(4));
    CHECK_EVAL_FORMAT_EXACT("arctan(sqrt(3))", piOver(3));
    CHECK_EVAL_FORMAT_NO_SLASH("sin(1)");
    CHECK_EVAL_FORMAT_NO_SLASH("cos(1)");
    CHECK_EVAL_FORMAT_NO_SLASH("tan(1)");
    CHECK_EVAL_FORMAT_EXACT("arcsin(sqrt(3)/22)", "0.07881114207211010205");
    CHECK_EVAL_FORMAT_NO_SLASH("arcsin(0.1)");
    CHECK_EVAL_FORMAT_NO_SLASH("arccos(0.1)");
    CHECK_EVAL_FORMAT_NO_SLASH("arctan(0.1)");
    CHECK_EVAL_FORMAT_EXACT("sin(pi/4)", QStringLiteral("sqrt(2)") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_EXACT("sin(pi/3)", QStringLiteral("sqrt(3)") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_EXACT("cos(pi/6)", QStringLiteral("sqrt(3)") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_EXACT("tan(pi/3)", QStringLiteral("sqrt(3)"));
    CHECK_EVAL_FORMAT_EXACT("tan(pi/6)", QStringLiteral("sqrt(3)") + slash + QStringLiteral("3"));

    settings->angleUnit = 'd';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL_FORMAT_EXACT("sin(45)", QStringLiteral("sqrt(2)") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_EXACT("cos(30)", QStringLiteral("sqrt(3)") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_EXACT("tan(60)", QStringLiteral("sqrt(3)"));
    CHECK_EVAL_FORMAT_EXACT("tan(30)", QStringLiteral("sqrt(3)") + slash + QStringLiteral("3"));
    CHECK_EVAL_FORMAT_EXACT("radians(0)", "0");
    CHECK_EVAL_FORMAT_EXACT("radians(30)", piOver(6));
    CHECK_EVAL_FORMAT_EXACT("radians(45)", piOver(4));
    CHECK_EVAL_FORMAT_EXACT("radians(60)", piOver(3));
    CHECK_EVAL_FORMAT_EXACT("radians(90)", piOver(2));
    CHECK_EVAL_FORMAT_EXACT("radians(120)", nPiOver(2, 3));
    CHECK_EVAL_FORMAT_EXACT("radians(135)", nPiOver(3, 4));
    CHECK_EVAL_FORMAT_EXACT("radians(150)", nPiOver(5, 6));
    CHECK_EVAL_FORMAT_EXACT("radians(180)", piSymbol);
    CHECK_EVAL_FORMAT_EXACT("radians(210)", nPiOver(7, 6));
    CHECK_EVAL_FORMAT_EXACT("radians(225)", nPiOver(5, 4));
    CHECK_EVAL_FORMAT_EXACT("radians(240)", nPiOver(4, 3));
    CHECK_EVAL_FORMAT_EXACT("radians(270)", nPiOver(3, 2));
    CHECK_EVAL_FORMAT_EXACT("radians(300)", nPiOver(5, 3));
    CHECK_EVAL_FORMAT_EXACT("radians(315)", nPiOver(7, 4));
    CHECK_EVAL_FORMAT_EXACT("radians(330)", nPiOver(11, 6));
    CHECK_EVAL_FORMAT_EXACT("radians(360)", nPi(2));

    settings->complexNumbers = true;
    settings->imaginaryUnit = 'i';
    CMath::setImaginaryUnitSymbol(QLatin1Char('i'));
    settings->resultFormatComplex = 'c';
    DMath::complexMode = true;
    eval->initializeBuiltInVariables();
    CHECK_EVAL_FORMAT("1+i", "1+1i");
    CHECK_EVAL_FORMAT_NO_SLASH("1+i");

    settings->imaginaryUnit = 'j';
    CMath::setImaginaryUnitSymbol(QLatin1Char('j'));
    eval->initializeBuiltInVariables();
    CHECK_EVAL_FORMAT("1+j", "1+1j");
    CHECK_EVAL_FORMAT_NO_SLASH("1+j");

    // Explicit non-decimal format should still take precedence.
    CHECK_EVAL_FORMAT("hex(31)", "0x1F");

    settings->angleUnit = angleUnit;
    settings->resultFormat = resultFormat;
    settings->resultPrecision = resultPrecision;
    settings->complexNumbers = complexNumbers;
    settings->imaginaryUnit = imaginaryUnit;
    CMath::setImaginaryUnitSymbol(QChar(imaginaryUnit));
    settings->resultFormatComplex = resultFormatComplex;
    DMath::complexMode = complexMode;
    eval->initializeBuiltInVariables();
    Evaluator::instance()->initializeAngleUnits();
}

void test_function_basic()
{
    CHECK_EVAL("ABS(0)", "0");
    CHECK_EVAL("ABS(1)", "1");
    CHECK_EVAL("ABS(-1)", "1");
    CHECK_EVAL("ABS(--1)", "1");
    CHECK_EVAL("ABS(---1)", "1");
    CHECK_EVAL("ABS((1))", "1");
    CHECK_EVAL("ABS((-1))", "1");

    CHECK_EVAL("ABS(1/2)", "0.5");
    CHECK_EVAL("ABS(-1/2)", "0.5");
    CHECK_EVAL("ABS(-1/-2)", "0.5");
    CHECK_EVAL("ABS(1/-2)", "0.5");

    CHECK_EVAL("INT(0)", "0");
    CHECK_EVAL("INT(1)", "1");
    CHECK_EVAL("INT(-1)", "-1");
    CHECK_EVAL("INT(0.5)", "0");
    CHECK_EVAL("INT(-0.75)", "0");
    CHECK_EVAL("INT(-0.9999*1)", "0");
    CHECK_EVAL("INT(0.9999*1)", "0");
    CHECK_EVAL("INT(2.1)", "2");
    CHECK_EVAL("INT(-3.4)", "-3");

    CHECK_EVAL("log(0.123; 0.1234)", "0.99845065797473594741");
    CHECK_EVAL("log10(0.00000000001)", "-11");
    CHECK_EVAL("log10(1e-3)", "-3");
    CHECK_EVAL("log2(0.00000000001)", "-36.54120904376098582657");
    CHECK_EVAL("log2(32)", "5");
    CHECK_EVAL("ln(100)", "4.60517018598809136804");
    CHECK_EVAL("ln(4.0)", "1.38629436111989061883");

    CHECK_EVAL("0!", "1");
    CHECK_EVAL("1!", "1");
    CHECK_EVAL("2!", "2");
    CHECK_EVAL("3!", "6");
    CHECK_EVAL("4!", "24");
    CHECK_EVAL("5!", "120");
    CHECK_EVAL("6!", "720");
    CHECK_EVAL("7!", "5040");
    CHECK_EVAL("(1+1)!^2", "4");

    CHECK_EVAL("(-27)^(1/3)", "-3");
    CHECK_EVAL("(-27)^(-1/3)", "-0.33333333333333333333");
    CHECK_EVAL("(-1)^(1/3)", "-1");
    CHECK_EVAL("(-1)^(1/7)", "-1");
    CHECK_EVAL("(-1)^(2/3)", "1");
    CHECK_EVAL("(-1)^(1/2)", "NaN");

    CHECK_EVAL_PRECISE("exp((1)/2) + exp((1)/2)", "3.29744254140025629369730157562832714330755220142030");

    CHECK_EVAL("√(81)", "9");
    CHECK_EVAL("∛(-27)", "-3");
    CHECK_INTERPRETED("sqrt(81)", "√(81)");
    CHECK_INTERPRETED("cbrt(-27)", "∛(-27)");
    CHECK_INTERPRETED("√(81)", "√(81)");
    CHECK_INTERPRETED("∛(-27)", "∛(-27)");

    // Test functions composition
    CHECK_EVAL("log(10;log(10;1e100))", "2");
    CHECK_EVAL("log(10;abs(-100))", "2");
    CHECK_EVAL("abs(log(10;100))", "2");
    CHECK_EVAL("abs(abs(-100))", "100");
    CHECK_EVAL("sum(10;abs(-100);1)", "111");
    CHECK_EVAL("sum(abs(-100);10;1)", "111");
    CHECK_EVAL("sum(10;1;abs(-100))", "111");

    CHECK_EVAL("ieee754_half_decode(0x7E00)", "NaN");
    CHECK_EVAL("hex(ieee754_half_encode(1))", "0x3C00");
    CHECK_EVAL("hex(ieee754_half_encode(0.99951171875))", "0x3BFF");
    CHECK_EVAL("hex(ieee754_half_encode(0.999755859375))", "0x3C00");

    CHECK_EVAL("ieee754_round_half(0.999755859375)", "1");
    CHECK_EVAL("ieee754_round_float(pi) - ieee754_single_decode(ieee754_single_encode(pi))", "0");
    CHECK_EVAL("ieee754_round_double(pi) - ieee754_double_decode(ieee754_double_encode(pi))", "0");
    CHECK_EVAL("ieee754_round_quad(pi) - ieee754_quad_decode(ieee754_quad_encode(pi))", "0");

    CHECK_EVAL("ieee754_half_residual(0.999755859375) - (0.999755859375 - ieee754_round_half(0.999755859375))", "0");
    CHECK_EVAL("ieee754_float_residual(pi) - (pi - ieee754_round_float(pi))", "0");
    CHECK_EVAL("ieee754_double_residual(pi) - (pi - ieee754_round_double(pi))", "0");
    CHECK_EVAL("ieee754_quad_residual(pi) - (pi - ieee754_round_quad(pi))", "0");

    CHECK_EVAL("ieee754_double_residual(pi)", "0.00000000000000012246");
    CHECK_EVAL("ieee754_double_residual(pi/2)", "0.00000000000000006123");

    CHECK_EVAL("pi/2", "1.57079632679489661923");
    CHECK_EVAL_PRECISE("ieee754_double_residual(pi/2)", "0.00000000000000006123233995736765886130329661375005");
    CHECK_EVAL_PRECISE("ieee754_double_residual(ieee754_double_residual(pi/2))", "-0.00000000000000000000000000000000149738490485916978");
    CHECK_EVAL_PRECISE("ieee754_double_residual(ieee754_double_residual(ieee754_double_residual(pi/2)))", "0.00000000000000000000000000000000000000000000000006");

    CHECK_EVAL("molmass(H)", u8"1.008 g·mol⁻¹");
    CHECK_EVAL("molmass(He)", u8"4.0026 g·mol⁻¹");
    CHECK_EVAL("molmass(C6H12O6)", u8"180.156 g·mol⁻¹");
    CHECK_EVAL(QString::fromUtf8("molmass(H₂O)"), u8"18.015 g·mol⁻¹");
    CHECK_EVAL(QString::fromUtf8("molmass(NaCl)"), u8"58.44 g·mol⁻¹");
    CHECK_EVAL(QString::fromUtf8("molmass(C₆H₁₂O₆)"), u8"180.156 g·mol⁻¹");
    CHECK_EVAL(QString::fromUtf8("molmass(Na₂SO₄)"), u8"142.036 g·mol⁻¹");
    CHECK_EVAL("molmass(C6H12O6)*molmass(C6H12O6)", u8"32456.184336 g²·mol⁻²");
    {
        ++eval_total_tests;
        const QString expr = QStringLiteral("molmass(C6H12O6)*molmass(C6H12O6)");
        eval->setExpression(expr);
        const Quantity value = eval->evalNoAssign();
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tmolmass product tooltip/result formatting eval\t[NEW]" << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
        } else {
            const QStringList lines = ResultLineFormatUtils::formatResultLinesForDisplay(
                expr,
                eval->interpretedExpression(),
                value,
                true,
                true);
            const bool hasCompactSuperscript = lines.last().contains(QString::fromUtf8("g²·mol⁻²"));
            const bool hasCompactFraction = lines.last().contains(QString::fromUtf8("g²/mol²"));
            const bool hasInverseOnlySuperscript = lines.last().contains(QString::fromUtf8("1·mol⁻²"));
            const bool hasInverseOnlyFraction = lines.last().contains(QString::fromUtf8("1/mol²"));
            if (lines.isEmpty()
                || (!hasCompactSuperscript && !hasCompactFraction)
                || hasInverseOnlySuperscript
                || hasInverseOnlyFraction) {
                ++eval_failed_tests;
                ++eval_new_failed_tests;
                cerr << __FILE__ << "[" << __LINE__ << "]\tmolmass product tooltip/result keeps gram numerator\t[NEW]" << endl
                     << "\tResult line: "
                     << (lines.isEmpty() ? "<empty>" : lines.last()).toUtf8().constData() << endl;
            }
        }
    }
    CHECK_EVAL_FAIL("molmass()");
    CHECK_EVAL_FAIL("molmass(C6H12O6;H2O)");
    CHECK_EVAL_FAIL("molmass(c6h12o6)");
    CHECK_EVAL_FAIL("molmass(H0)");
    CHECK_EVAL_FAIL("molmass(H2O1x)");
    CHECK_EVAL_FAIL("molmass(Xx2)");
    CHECK_EVAL_FAIL(QString::fromUtf8("molmass(K4[Fe(CN)6]·3H2O)"));

    CHECK_EVAL("mass(1;C6H12O6)", "180.156 g");
    CHECK_EVAL("mass(2;H2O)", "36.03 g");
    CHECK_EVAL(QString::fromUtf8("mass(0.5;H₂O)"), "9.0075 g");
    CHECK_EVAL("mass(1;C6H12O6)-molmass(C6H12O6)*1[mol]", "0 g");
    CHECK_EVAL_FAIL("mass()");
    CHECK_EVAL_FAIL("mass(1)");
    CHECK_EVAL_FAIL("mass(1;C6H12O6;H2O)");
    CHECK_EVAL_FAIL("mass(1;c6h12o6)");
    CHECK_EVAL_FAIL("mass(1;H0)");

    CHECK_EVAL("molarity(0.50;1.00)", "0.5 mol/L");
    CHECK_EVAL("molarity(1[mol];2[L])", "0.5 mol/L");
    CHECK_EVAL("molarity(1;2[L])", "0.5 mol/L");
    CHECK_EVAL("molarity(1[mol];2)", "0.5 mol/L");
    CHECK_EVAL("molarity(3[mmol];2[mL])", "1.5 mol/L");
    CHECK_EVAL("molarity(250[umol];500[mL])", "0.0005 mol/L");
    CHECK_EVAL("molarity(0.75[mol];300[mL])", "2.5 mol/L");
    CHECK_EVAL("molarity(3[mmol];2[mL])-molarity(0.003[mol];0.002[L])", "0 mol/L");
    CHECK_EVAL("molarity(1[mol];2[L])-0.5[mol/L]", "0 mol/L");
    CHECK_EVAL_FAIL("molarity()");
    CHECK_EVAL_FAIL("molarity(1)");
    CHECK_EVAL_FAIL("molarity(1;2;3)");
    CHECK_EVAL_FAIL("molarity(1[g];1[L])");
    CHECK_EVAL_FAIL("molarity(1[mol];1[s])");
}

void test_function_trig()
{
    CHECK_EVAL_PRECISE("pi", "3.14159265358979323846264338327950288419716939937511");

    CHECK_EVAL("sin(0)", "0");
    CHECK_EVAL("cos(0)", "1");
    CHECK_EVAL("tan(0)", "0");

    CHECK_EVAL("sin(pi)", "0");
    CHECK_EVAL("cos(pi)", "-1");
    CHECK_EVAL("tan(pi)", "0");

    CHECK_EVAL("sin(-pi)", "0");
    CHECK_EVAL("cos(-pi)", "-1");
    CHECK_EVAL("tan(-pi)", "0");

    CHECK_EVAL("sin(--pi)", "0");
    CHECK_EVAL("cos(--pi)", "-1");
    CHECK_EVAL("tan(--pi)", "0");

    CHECK_EVAL("sin(pi/2)", "1");
    CHECK_EVAL("cos(pi/2)", "0");

    CHECK_EVAL("sin(-pi/2)", "-1");
    CHECK_EVAL("cos(-pi/2)", "0");

    CHECK_EVAL("sin(-pi/2) + sin(pi/2)", "0");
    CHECK_EVAL("sin(-pi/2) - sin(pi/2)", "-2");
    CHECK_EVAL("cos(-pi/2) + cos(pi/2)", "0");
    CHECK_EVAL("cos(-pi/2) - cos(pi/2)", "0");

    CHECK_EVAL("arcsin(sin(1))", "1");
    CHECK_EVAL("arccos(cos(1))", "1");
    CHECK_EVAL("arctan(tan(1))", "1");
    CHECK_EVAL("arcsin(0)", "0");
    CHECK_EVAL("arccos(1)", "0");
    CHECK_EVAL("arctan(0)", "0");

    CHECK_EVAL("degrees(0)", "0");
    CHECK_EVAL("degrees(pi/2)", "90");
    CHECK_EVAL("degrees(pi)", "180");
    CHECK_EVAL("degrees(3*pi/2)", "270");
    CHECK_EVAL("degrees(2*pi)", "360");

    CHECK_EVAL("radians(0)", "0");
    CHECK_EVAL("radians(90)/pi", "0.5");
    CHECK_EVAL("radians(180)/pi", "1");
    CHECK_EVAL("radians(270)/pi", "1.5");
    CHECK_EVAL("radians(360)/pi", "2");

    CHECK_EVAL("gradians(0)", "0");
    CHECK_EVAL("gradians(pi/2)", "100");
    CHECK_EVAL("gradians(pi)", "200");
    CHECK_EVAL("gradians(3*pi/2)", "300");
    CHECK_EVAL("gradians(2*pi)", "400");

    CHECK_EVAL("turns(0)", "0");
    CHECK_EVAL("turns(pi/2)", "0.25");
    CHECK_EVAL("turns(pi)", "0.5");
    CHECK_EVAL("turns(3*pi/2)", "0.75");
    CHECK_EVAL("turns(2*pi)", "1");
}

void test_function_stat()
{
    CHECK_EVAL_FAIL("MIN(0)");
    CHECK_EVAL("MIN(0; 1)", "0");
    CHECK_EVAL("MIN(0; 2)", "0");
    CHECK_EVAL("MIN(-1; 0)", "-1");
    CHECK_EVAL("MIN(-1; 1)", "-1");
    CHECK_EVAL("MIN(-0.01; 0)", "-0.01");
    CHECK_EVAL("MIN(0; 1; 2)", "0");
    CHECK_EVAL("MIN(-1; 0; 1; 2)", "-1");
    CHECK_EVAL("MIN(-2; -1; 0; 1; 2)", "-2");

    CHECK_EVAL_FAIL("MAX(0)");
    CHECK_EVAL("MAX(0; 1)", "1");
    CHECK_EVAL("MAX(0; 2)", "2");
    CHECK_EVAL("MAX(-1; 0)", "0");
    CHECK_EVAL("MAX(-1; 1)", "1");
    CHECK_EVAL("MAX(0.01; 0)", "0.01");
    CHECK_EVAL("MAX(0; 1; 2)", "2");
    CHECK_EVAL("MAX(-1; 0; 1; 2)", "2");
    CHECK_EVAL("MAX(-2; -1; 0; 1; 2)", "2");

    CHECK_EVAL_FAIL("SUM(1)");
    CHECK_EVAL("SUM(100;1)", "101");
    CHECK_EVAL("SUM(-100;1)", "-99");
    CHECK_EVAL("SUM(0;0;0)", "0");
    CHECK_EVAL("SUM(100;-1)", "99");
    CHECK_EVAL("SUM(-100;-1)", "-101");
    CHECK_EVAL("SUM(1;2;3;4;5;6)", "21");
    CHECK_EVAL("SUM(1;-2;3;-4;5;-6)", "-3");

    CHECK_EVAL_FAIL("SUMMATION(1;10)");
    CHECK_EVAL_FAIL("SUMMATION(1;2;3;4)");
    CHECK_EVAL_FAIL("SUMMATION(1.5;10;n)");
    CHECK_EVAL_FAIL("SUMMATION(1[metre];10;n)");
    CHECK_EVAL_FAIL("SUMMATION(1;10;n+unknown_var)");
    CHECK_EVAL("SUMMATION(1;1;n)", "1");
    CHECK_EVAL("SUMMATION(3;3;2+n)", "5");
    CHECK_EVAL("SUMMATION(1;4;3)", "12");
    CHECK_EVAL("SUMMATION(-2;2;n)", "0");
    CHECK_EVAL("SUMMATION(1;10;2+n)", "75");
    CHECK_EVAL("∑(1;10;2+n)", "75");
    CHECK_EVAL("SUMMATION(10;1;n)", "55");
    CHECK_EVAL("SUMMATION(-1;-4;n)", "-10");
    CHECK_EVAL("SUMMATION(1;4;n^2)", "30");
    CHECK_EVAL("SUMMATION(1;4;1/n)", "2.08333333333333333333");
    CHECK_EVAL("SUMMATION(1;3;n[metre])", "NaN");
    CHECK_EVAL("SUMMATION(1;3;n^2+n)", "20");
    CHECK_INTERPRETED("summation(1;10;n+1)", "∑(1; 10; n+1)");
    CHECK_EVAL_FAIL("sigma(1;10;n+1)");
    CHECK_INTERPRETED("∑(1;10;n+1)", "∑(1; 10; n+1)");
    CHECK_EVAL("SUMMATION(1;3;SUMMATION(1;2;1))", "6");
    CHECK_EVAL_FAIL("SUMMATION(1;3;SUMMATION(1;n;1))");
    CHECK_EVAL("f(k)=k^2", "NaN");
    CHECK_EVAL_FAIL("SUMMATION(1;4;f(n))");
    CHECK_EVAL("x=10", "10");
    CHECK_EVAL("SUMMATION(1;3;n+x)", "36");
    CHECK_EVAL("x", "10");
    CHECK_EVAL("n=123", "123");
    CHECK_EVAL("SUMMATION(1;3;n)", "6");
    CHECK_EVAL("n", "123");

    CHECK_EVAL_FAIL("PRODUCT(-1)");
    CHECK_EVAL("PRODUCT(100;0)", "0");
    CHECK_EVAL("PRODUCT(100;1)", "100");
    CHECK_EVAL("PRODUCT(-100;1)", "-100");
    CHECK_EVAL("PRODUCT(-100;-1)", "100");
    CHECK_EVAL("PRODUCT(1;1;1)", "1");
    CHECK_EVAL("PRODUCT(1;2;3;4;5;6)", "720");
    CHECK_EVAL("PRODUCT(1;-2;3;-4;5;-6)", "-720");

    CHECK_EVAL_FAIL("AVERAGE(0)");
    CHECK_EVAL("AVERAGE(0;0)", "0");
    CHECK_EVAL("AVERAGE(0;0;0)", "0");
    CHECK_EVAL("AVERAGE(0;1)", "0.5");
    CHECK_EVAL("AVERAGE(0;1;2)", "1");
    CHECK_EVAL("AVERAGE(0;1;0)*3", "1");
    CHECK_EVAL("AVERAGE(1;1;1)", "1");
    CHECK_EVAL("AVERAGE(2;2;2)", "2");
    CHECK_EVAL("AVERAGE(3;3;3)", "3");
    CHECK_EVAL("AVERAGE(0.25;0.75)", "0.5");
    CHECK_EVAL("AVERAGE(2.25;4.75)", "3.5");
    CHECK_EVAL("AVERAGE(1/3;2/3)", "0.5");
    CHECK_EVAL("123 + average(2;3;4;)", "126");

    CHECK_EVAL_FAIL("GEOMEAN(-1e20;0;-1)");
    CHECK_EVAL_FAIL("GEOMEAN(5)");
    CHECK_EVAL("GEOMEAN(1;1)", "1");
    CHECK_EVAL("GEOMEAN(1;4)", "2");
    CHECK_EVAL("GEOMEAN(4;9)", "6");
    CHECK_EVAL("GEOMEAN(3.6;8.1)", "5.4");
    CHECK_EVAL("GEOMEAN(3;4;18)", "6");
    CHECK_EVAL("GEOMEAN(1;1;1)", "1");
    CHECK_EVAL("GEOMEAN(1;1;1;1)", "1");
    CHECK_EVAL_FAIL("GEOMEAN(1;1;1;-1)");

    CHECK_EVAL("VARIANCE(1;-1)", "1");
    CHECK_EVAL("VARIANCE(5[metre]; 13[metre])", "16 metre²");
    // for complex tests of VARIANCE see test_complex

    CHECK_EVAL("int(rand())", "0");
    CHECK_EVAL("rand(0)", "0");
    CHECK_EVAL_FAIL("rand(1;2)");
    CHECK_EVAL_FAIL("rand(-1)");
    CHECK_EVAL_FAIL("rand(1.5)");
    CHECK_EVAL_FAIL("rand(1[metre])");
    CHECK_EVAL_FAIL("rand(1000)");

    CHECK_EVAL("randint(0)", "0");
    CHECK_EVAL("randint(5;5)", "5");
    CHECK_EVAL("randint(-3;-3)", "-3");
    CHECK_EVAL("int(randint(9)/10)", "0");
    CHECK_EVAL("int(randint(-9)/10)", "0");
    CHECK_EVAL("int(randint(3;1)/10)", "0");
    CHECK_EVAL_FAIL("randint()");
    CHECK_EVAL_FAIL("randint(1;2;3)");
    CHECK_EVAL_FAIL("randint(1.5)");
    CHECK_EVAL_FAIL("randint(1;2.5)");
    CHECK_EVAL_FAIL("randint(1[metre])");
}

void test_function_logic()
{
    CHECK_EVAL_FAIL("and(1)");
    CHECK_EVAL_FAIL("or(2)");
    CHECK_EVAL_FAIL("xor(3)");
    CHECK_EVAL_FAIL("popcount()");
    CHECK_EVAL_FAIL("popcount(1;2)");
    CHECK_EVAL_FAIL("popcount(1[metre])");
    CHECK_EVAL_FAIL("popcount(2^300)");

    CHECK_EVAL("and(0;0)", "0");
    CHECK_EVAL("and(0;1)", "0");
    CHECK_EVAL("and(1;0)", "0");
    CHECK_EVAL("and(1;1)", "1");

    CHECK_EVAL("or(0;0)", "0");
    CHECK_EVAL("or(0;1)", "1");
    CHECK_EVAL("or(1;0)", "1");
    CHECK_EVAL("or(1;1)", "1");

    CHECK_EVAL("xor(0;0)", "0");
    CHECK_EVAL("xor(0;1)", "1");
    CHECK_EVAL("xor(1;0)", "1");
    CHECK_EVAL("xor(1;1)", "0");

    CHECK_EVAL("popcount(0)", "0");
    CHECK_EVAL("popcount(1)", "1");
    CHECK_EVAL("popcount(2^254)", "1");
    CHECK_EVAL("popcount(2^255-1)", "255");
    CHECK_EVAL("popcount(-2^255)", "1");
    CHECK_EVAL("popcount(15)", "4");
    CHECK_EVAL("popcount(0xF0F0)", "8");
    CHECK_EVAL("popcount(3.9)", "2");
    CHECK_EVAL("popcount(-3.9)", "255");
    CHECK_EVAL("popcount(0.9)", "0");
    CHECK_EVAL("popcount(-0.9)", "256");
    CHECK_EVAL("popcount(-1)", "256");
    CHECK_EVAL("popcount(and(0x123456789ABCDEF0; not(0x123456789ABCDEF0)))", "0");
    CHECK_EVAL("popcount(or(0x123456789ABCDEF0; not(0x123456789ABCDEF0)))", "256");
    CHECK_EVAL("popcount(123456789) + popcount(not(123456789))", "256");
    CHECK_EVAL("popcount(-123456789) + popcount(not(-123456789))", "256");
}

void test_function_discrete()
{
    CHECK_EVAL("idiv(10;3)", "3");
    CHECK_EVAL("idiv(-10;3)", "-3");
    CHECK_EVAL("idiv(10;-3)", "-3");
    CHECK_EVAL("idiv(-10;-3)", "3");

    CHECK_EVAL("mod(10;3)", "1");
    CHECK_EVAL("mod(-10;3)", "-1");
    CHECK_EVAL("mod(10;-3)", "1");
    CHECK_EVAL("mod(-10;-3)", "-1");

    CHECK_EVAL("emod(10;3)", "1");
    CHECK_EVAL("emod(-10;3)", "2");
    CHECK_EVAL("emod(10;-3)", "-2");
    CHECK_EVAL("emod(-10;-3)", "-1");
    CHECK_EVAL("emod(-1;360)", "359");
    CHECK_EVAL("emod(360;360)", "0");
    CHECK_EVAL("emod(361;360)", "1");
    CHECK_EVAL("emod(-360;360)", "0");
    CHECK_EVAL("emod(-361;360)", "359");
    CHECK_EVAL("emod(721;360)", "1");
    CHECK_EVAL("emod(-721;360)", "359");
    CHECK_EVAL("emod(0;360)", "0");
    CHECK_EVAL("emod(180;360)", "180");
    CHECK_EVAL("emod(-180;360)", "180");
    CHECK_EVAL("emod(1;-360)", "-359");
    CHECK_EVAL("emod(359;-360)", "-1");
    CHECK_EVAL("emod(360;-360)", "0");
    CHECK_EVAL("emod(-1;-360)", "-1");
    CHECK_EVAL("emod(-361;-360)", "-1");
    CHECK_EVAL("emod(2.5;2)", "0.5");
    CHECK_EVAL("emod(-2.5;2)", "1.5");

    CHECK_EVAL("powmod(2;10;7)", "2");
    CHECK_EVAL("powmod(3;233;353)", "248");
    CHECK_EVAL("powmod(-3;233;353)", "105");
    CHECK_EVAL("powmod(123456789;0;97)", "1");
    CHECK_EVAL("powmod(123456789;1234567;1)", "0");
    CHECK_EVAL("powmod(123456789;1234567;-97)", "-80");
    CHECK_EVAL_FAIL("powmod(2;-1;5)");
    CHECK_EVAL_FAIL("powmod(2;3.5;5)");
    CHECK_EVAL_FAIL("powmod(2.5;3;5)");

    CHECK_EVAL("gcd(12;18)", "6");
    CHECK_EVAL("gcd(36;56;210)", "2");
    CHECK_EVAL("gcd(28;120;126)", "2");
    CHECK_EVAL("lcm(12;18)", "36");
    CHECK_EVAL("lcm(36;56;210)", "2520");
    CHECK_EVAL("lcm(0;5)", "0");
    CHECK_EVAL("lcm(-9;27)", "27");
    CHECK_EVAL("round(5.5e-10;2)", "0");
    CHECK_EVAL("round(5.5*10^-10;2)", "0");
    CHECK_EVAL("round(0.00000000055;2)", "0");

    CHECK_EVAL("ncr(-3;-1)", "0");
    CHECK_EVAL("ncr(-3;0)", "1");
    CHECK_EVAL("ncr(-3;1)", "-3");
    CHECK_EVAL("ncr(-3;2)", "6");
    CHECK_EVAL("ncr(-3;3)", "-10");
    CHECK_EVAL("ncr(-3;4)", "15");
    CHECK_EVAL("ncr(-3;5)", "-21");
    CHECK_EVAL("ncr(-2;-1)", "0");
    CHECK_EVAL("ncr(-2;0)", "1");
    CHECK_EVAL("ncr(-2;1)", "-2");
    CHECK_EVAL("ncr(-2;2)", "3");
    CHECK_EVAL("ncr(-2;3)", "-4");
    CHECK_EVAL("ncr(-2;4)", "5");
    CHECK_EVAL("ncr(-2;5)", "-6");

    CHECK_EVAL("ncr(-1;-1)", "0");
    CHECK_EVAL("ncr(-1;0)", "1");
    CHECK_EVAL("ncr(-1;1)", "-1");
    CHECK_EVAL("ncr(-1;2)", "1");
    CHECK_EVAL("ncr(-1;3)", "-1");
    CHECK_EVAL("ncr(-1;4)", "1");
    CHECK_EVAL("ncr(-1;5)", "-1");

    CHECK_EVAL("ncr(0;-1)", "0");
    CHECK_EVAL("ncr(0;0)", "1");
    CHECK_EVAL("ncr(0;1)", "0");

    CHECK_EVAL("ncr(1;-1)", "0");
    CHECK_EVAL("ncr(1;0)", "1");
    CHECK_EVAL("ncr(1;1)", "1");
    CHECK_EVAL("ncr(1;2)", "0");

    CHECK_EVAL("ncr(2;-1)", "0");
    CHECK_EVAL("ncr(2;0)", "1");
    CHECK_EVAL("ncr(2;1)", "2");
    CHECK_EVAL("ncr(2;2)", "1");
    CHECK_EVAL("ncr(2;3)", "0");

    CHECK_EVAL("ncr(3;-1)", "0");
    CHECK_EVAL("ncr(3;0)", "1");
    CHECK_EVAL("ncr(3;1)", "3");
    CHECK_EVAL("ncr(3;2)", "3");
    CHECK_EVAL("ncr(3;3)", "1");
    CHECK_EVAL("ncr(3;4)", "0");

    CHECK_EVAL("ncr(4;-1)", "0");
    CHECK_EVAL("ncr(4;0)", "1");
    CHECK_EVAL("ncr(4;1)", "4");
    CHECK_EVAL("ncr(4;2)", "6");
    CHECK_EVAL("ncr(4;3)", "4");
    CHECK_EVAL("ncr(4;4)", "1");
    CHECK_EVAL("ncr(4;5)", "0");
}

void test_function_simplified()
{
    /* Tests for standard functions */
    CHECK_EVAL("abs 123", "123");
    CHECK_EVAL("abs -123", "123");       /* (issue #600) */
    CHECK_EVAL("10 + abs 123", "133");
    CHECK_EVAL("10 + abs -123", "133");  /* (issue #600) */
    CHECK_EVAL("abs 123 + 10", "133");
    CHECK_EVAL("abs -123 + 10", "133");  /* (issue #600) */
    CHECK_EVAL("10 * abs 123", "1230");
    CHECK_EVAL("abs 123 * 10", "1230");
    /* Tests for user functions (issue #600, cf. discussion) */
    CHECK_USERFUNC_SET("func2(x) = abs(x)");
    CHECK_EVAL("func2 123", "123");
    CHECK_EVAL("func2 -123", "123");
    CHECK_EVAL("10 + func2 123", "133");
    CHECK_EVAL("10 + func2 -123", "133");
    CHECK_EVAL("func2 123 + 10", "133");
    CHECK_EVAL("func2 -123 + 10", "133");
    CHECK_EVAL("10 * func2 123", "1230");
    CHECK_EVAL("func2 123 * 10", "1230");
    CHECK_USERFUNC_SET("f(x) = 5[metre] + x"); /* (issue #656)  */
    CHECK_EVAL("f(2[metre])", "7 metre");      /* (issue #656)  */
    CHECK_EVAL_FAIL("f(2)");                  /* (issue #656)  */
    /* Tests for priority management (issue #451) */
    CHECK_EVAL("log10 10^2", "2");
    CHECK_EVAL("frac 3!",  "0");
    CHECK_EVAL("-log10 10", "-1");
    CHECK_EVAL("2*-log10 10", "-2");
    CHECK_EVAL("--log10 10", "1");
    CHECK_EVAL("-ln 10", "-2.30258509299404568402");
    CHECK_EVAL("-log2 8", "-3");
    CHECK_EVAL("-sin 1", "-0.84147098480789650665");
    CHECK_EVAL("-cos pi", "1");
    CHECK_EVAL("-tan 1", "-1.55740772465490223051");
    CHECK_EVAL("-arcsin 1", "-1.57079632679489661923");
    CHECK_EVAL("-arccos 0", "-1.57079632679489661923");
    CHECK_EVAL("-arctan 1", "-0.78539816339744830962");
    CHECK_EVAL("2*-sin 1", "-1.68294196961579301331");
    CHECK_EVAL("--sin 1", "0.84147098480789650665");
    CHECK_EVAL("10 + -cos pi", "11");
}

void test_auto_fix_parentheses()
{
    CHECK_AUTOFIX("sin(1)", "sin(1)");
    CHECK_AUTOFIX("sin(1", "sin(1)");

    CHECK_AUTOFIX("x+(8-2", "x+(8-2)");
    CHECK_AUTOFIX("x+(8-2)", "x+(8-2)");

    CHECK_AUTOFIX("x+(8-(2*1", "x+(8-(2*1))");
    CHECK_AUTOFIX("x+(8-(2*1)", "x+(8-(2*1))");
    CHECK_AUTOFIX("x+(8-(2*1))", "x+(8-(2*1))");

    CHECK_AUTOFIX("x + sin (pi", "x + sin (pi)");

    CHECK_AUTOFIX("2[m3", "2[m3]");
    CHECK_AUTOFIX("3([m])", "3[m]");
    CHECK_AUTOFIX("3(([[[m]]]))", "3[[[m]]]");
    CHECK_AUTOFIX("3(((([[[m]]])))", "3[[[m]]]");
    CHECK_AUTOFIX("3(((((((([[([m])]]))))", "3[[[m]]]");
    CHECK_AUTOFIX("4[L] -> [dL", "4[L] -> [dL]");
    CHECK_AUTOFIX("3[[m]]", "3[[m]]");
    CHECK_AUTOFIX("3[[m]", "3[[m]]");
    CHECK_AUTOFIX("3[[m", "3[[m]]");
}

void test_auto_fix_ans()
{
    CHECK_AUTOFIX("sin", "sin(ans)");
    CHECK_AUTOFIX("cos", "cos(ans)");
    CHECK_AUTOFIX("tan", "tan(ans)");
    CHECK_AUTOFIX("abs", "abs(ans)");
    CHECK_AUTOFIX("exp", "exp(ans)");
    CHECK_AUTOFIX("log10", "log10(ans)");
    CHECK_AUTOFIX("sqrt", "√(ans)");
    CHECK_AUTOFIX("cbrt", "∛(ans)");
}

void test_auto_fix_trailing_equal()
{
    CHECK_AUTOFIX("1+2=", "1+2");
    CHECK_AUTOFIX("1==", "1");
    CHECK_AUTOFIX("1/3====", "1/3");
    CHECK_AUTOFIX("sin(x+y)=", "sin(x+y)");
    CHECK_AUTOFIX("1+2+", "1+2");
    CHECK_AUTOFIX("1++++", "1");
    CHECK_AUTOFIX("2**", "2");
    CHECK_AUTOFIX("3/", "3");
    CHECK_AUTOFIX("5+cos pi (", "5+cos pi");
    CHECK_AUTOFIX("123 + average(2;3;4;", "123 + average(2;3;4)");

    ++eval_total_tests;
    const QString fixedTrailingPlus = eval->autoFix(QString::fromUtf8("1+2+"));
    eval->setExpression(fixedTrailingPlus);
    const Quantity trailingPlusResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tautofix trailing plus eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl
             << "\tAutoFix: " << fixedTrailingPlus.toUtf8().constData() << endl;
    } else {
        QString formatted = DMath::format(trailingPlusResult, Format::Fixed());
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QStringLiteral("3")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tautofix trailing plus eval\t[NEW]" << endl
                 << "\tResult   : " << formatted.toLatin1().constData() << endl
                 << "\tExpected : 3" << endl
                 << "\tAutoFix  : " << fixedTrailingPlus.toUtf8().constData() << endl;
        }
    }

    ++eval_total_tests;
    const QString fixedTrailingFunctionSep =
        eval->autoFix(QString::fromUtf8("123 + average(2;3;4;"));
    eval->setExpression(fixedTrailingFunctionSep);
    const Quantity trailingFunctionSepResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tautofix trailing function separator eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl
             << "\tAutoFix: " << fixedTrailingFunctionSep.toUtf8().constData() << endl;
    } else {
        QString formatted = DMath::format(trailingFunctionSepResult, Format::Fixed());
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QStringLiteral("126")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tautofix trailing function separator eval\t[NEW]" << endl
                 << "\tResult   : " << formatted.toLatin1().constData() << endl
                 << "\tExpected : 126" << endl
                 << "\tAutoFix  : " << fixedTrailingFunctionSep.toUtf8().constData() << endl;
        }
    }
}

void test_auto_fix_untouch()
{
    CHECK_AUTOFIX("sin(x)", "sin(x)");
    CHECK_AUTOFIX("sin((x/y))", "sin((x/y))");
    CHECK_AUTOFIX("ans", "ans");
    CHECK_AUTOFIX("sin(ans)", "sin(ans)");
    CHECK_AUTOFIX("tan(ans)", "tan(ans)");
    CHECK_AUTOFIX("x=1.2", "x=1.2");
    CHECK_AUTOFIX("1/sin pi", "1/sin pi");
}

void test_auto_fix_powers()
{
    CHECK_AUTOFIX("3¹", "3^1");
    CHECK_AUTOFIX("3⁻¹", "3^(-1)");
    CHECK_AUTOFIX("3¹²³⁴⁵⁶⁷⁸⁹", "3^123456789");
    CHECK_AUTOFIX("3²⁰", "3^20");
    CHECK_AUTOFIX("cos²(pi)", "cos(pi)^2");
    CHECK_AUTOFIX("cos² (pi)", "cos(pi)^2");
    CHECK_AUTOFIX("cos^2(pi)", "cos(pi)^2");
    CHECK_AUTOFIX("cos^(-1)(pi)", "cos(pi)^(-1)");
    CHECK_AUTOFIX("2cos^3(pi)", "2cos(pi)^3");
    CHECK_AUTOFIX("7 + 3²⁰ * 4", "7 + 3^20 * 4");
    CHECK_AUTOFIX("2×pi", "2·pi");
    CHECK_AUTOFIX("2×a", "2·a");
    CHECK_AUTOFIX("4·5", "4·5");
    CHECK_AUTOFIX("2   ×    pi   pi", "2·pi·pi");
    CHECK_AUTOFIX("2          ×pi×  pi", "2·pi·pi");
    CHECK_AUTOFIX("Ω+μ", "Ω+µ");
    CHECK_AUTOFIX("uV+uA", "uV+uA");
    CHECK_AUTOFIX("2×sin(33×3×sin(23)×cos(−pi))×sin(23234)×23⧸2−sin(−12) − 12−12",
                  "2·sin(33×3·sin(23)·cos(−pi))·sin(23234)·23⧸2−sin(−12) − 12−12");
    CHECK_AUTOFIX("2          ×pi×  pi + 2^12.000−2",
                  "2·pi·pi + 2^12.000−2");
    CHECK_AUTOFIX("4/2[s]", "4/2·[s]");
    CHECK_AUTOFIX("1[metre] in [metre]", "1[metre] in [metre]");
    CHECK_AUTOFIX("1[metre] IN [metre]", "1[metre] IN [metre]");
    CHECK_AUTOFIX("sqrt(16)+cbrt(27)", "√(16)+∛(27)");
    CHECK_AUTOFIX("asqrt(16)+cbrtfoo(27)", "asqrt(16)+cbrtfoo(27)");
    CHECK_AUTOFIX("2 + 3^() + 4", "2 + 3 + 4");
    CHECK_AUTOFIX("2 + 3 ^ (   ) + 4", "2 + 3  + 4");

    ++eval_total_tests;
    eval->setExpression(eval->autoFix(QStringLiteral("4/2[s]")));
    const Quantity autoFixedValueUnitDivisionResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tautofix value-unit denominator grouping eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QString formatted = DMath::format(autoFixedValueUnitDivisionResult, Format::Fixed());
        if (formatted != QStringLiteral("2 second")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tautofix value-unit denominator grouping eval\t[NEW]" << endl
                 << "\tResult   : " << formatted.toUtf8().constData() << endl
                 << "\tExpected : 2 second" << endl;
        }
    }

    ++eval_total_tests;
    const QString fixedNoOpPower = eval->autoFix(QString::fromUtf8("2 + 3^() + 4"));
    eval->setExpression(fixedNoOpPower);
    const Quantity fixedNoOpPowerResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tautofix empty parenthesized power eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl
             << "\tAutoFix: " << fixedNoOpPower.toUtf8().constData() << endl;
    } else {
        QString formatted = DMath::format(fixedNoOpPowerResult, Format::Fixed());
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QStringLiteral("9")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tautofix empty parenthesized power eval\t[NEW]" << endl
                 << "\tResult   : " << formatted.toLatin1().constData() << endl
                 << "\tExpected : 9" << endl
                 << "\tAutoFix  : " << fixedNoOpPower.toUtf8().constData() << endl;
        }
    }

    // Selection text copied from result display may contain medium spaces and
    // paragraph separators; auto-fix should still produce a valid expression.
    ++eval_total_tests;
    const QString selectionText = QString::fromUtf8(
        "−1 · (2²) · 69\u2029− 39 × 2⁵ − 828 · (2²) + 39");
    const QString fixedSelectionText = eval->autoFix(selectionText);
    eval->setExpression(fixedSelectionText);
    Quantity selectionResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tautofix display selection text eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl
             << "\tAutoFix: " << fixedSelectionText.toUtf8().constData() << endl;
    } else {
        QString formatted = DMath::format(selectionResult, Format::Fixed());
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QStringLiteral("-4797")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tautofix display selection text eval\t[NEW]" << endl
                 << "\tResult   : " << formatted.toLatin1().constData() << endl
                 << "\tExpected : -4797" << endl
                 << "\tAutoFix  : " << fixedSelectionText.toUtf8().constData() << endl;
        }
    }

    ++eval_total_tests;
    const QString fixedFunctionPowerSelection =
        eval->autoFix(QString::fromUtf8("1+cos²(pi)"));
    eval->setExpression(fixedFunctionPowerSelection);
    const Quantity fixedFunctionPowerResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tautofix function superscript eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl
             << "\tAutoFix: " << fixedFunctionPowerSelection.toUtf8().constData() << endl;
    } else {
        QString formatted = DMath::format(fixedFunctionPowerResult, Format::Fixed());
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QStringLiteral("2")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tautofix function superscript eval\t[NEW]" << endl
                 << "\tResult   : " << formatted.toUtf8().constData() << endl
                 << "\tExpected : 2" << endl
                 << "\tAutoFix  : " << fixedFunctionPowerSelection.toUtf8().constData() << endl;
        }
    }

    ++eval_total_tests;
    const QString fixedImplicitMulFunctionPower =
        eval->autoFix(QString::fromUtf8("2cos³(pi)"));
    eval->setExpression(fixedImplicitMulFunctionPower);
    const Quantity fixedImplicitMulFunctionPowerResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tautofix implicit mul function superscript eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl
             << "\tAutoFix: " << fixedImplicitMulFunctionPower.toUtf8().constData() << endl;
    } else {
        QString formatted = DMath::format(fixedImplicitMulFunctionPowerResult, Format::Fixed());
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QStringLiteral("-2")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tautofix implicit mul function superscript eval\t[NEW]" << endl
                 << "\tResult   : " << formatted.toUtf8().constData() << endl
                 << "\tExpected : -2" << endl
                 << "\tAutoFix  : " << fixedImplicitMulFunctionPower.toUtf8().constData() << endl;
        }
    }

    ++eval_total_tests;
    const QString fixedImplicitMulFunctionPower2 =
        eval->autoFix(QString::fromUtf8("2cos²(pi)"));
    eval->setExpression(fixedImplicitMulFunctionPower2);
    const Quantity fixedImplicitMulFunctionPowerResult2 = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tautofix implicit mul function superscript eval 2\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl
             << "\tAutoFix: " << fixedImplicitMulFunctionPower2.toUtf8().constData() << endl;
    } else {
        QString formatted = DMath::format(fixedImplicitMulFunctionPowerResult2, Format::Fixed());
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QStringLiteral("2")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tautofix implicit mul function superscript eval 2\t[NEW]" << endl
                 << "\tResult   : " << formatted.toUtf8().constData() << endl
                 << "\tExpected : 2" << endl
                 << "\tAutoFix  : " << fixedImplicitMulFunctionPower2.toUtf8().constData() << endl;
        }
    }

    ++eval_total_tests;
    const QString fixedCaretFunctionPower = eval->autoFix(QString::fromUtf8("2cos^3(pi)"));
    eval->setExpression(fixedCaretFunctionPower);
    const Quantity fixedCaretFunctionPowerResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tautofix caret function power eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl
             << "\tAutoFix: " << fixedCaretFunctionPower.toUtf8().constData() << endl;
    } else {
        QString formatted = DMath::format(fixedCaretFunctionPowerResult, Format::Fixed());
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QStringLiteral("-2")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tautofix caret function power eval\t[NEW]" << endl
                 << "\tResult   : " << formatted.toUtf8().constData() << endl
                 << "\tExpected : -2" << endl
                 << "\tAutoFix  : " << fixedCaretFunctionPower.toUtf8().constData() << endl;
        }
    }
}

void test_auto_fix_shift_add_sub()
{
    CHECK_AUTOFIX("0x4f<<8+0x4c", "(0x4f << 8) + 0x4c");
    CHECK_AUTOFIX("0x4f >> 8 + 0x4c", "(0x4f >> 8) + 0x4c");
    CHECK_AUTOFIX("0x4f<<8-0x4c", "(0x4f << 8) - 0x4c");

    // Keep explicit RHS grouping untouched.
    CHECK_AUTOFIX("0x4f<<(8+0x4c)", "0x4f<<(8+0x4c)");
}

void test_comments()
{
    CHECK_EVAL("ncr(3;3) ? this is because foo",  "1");
    CHECK_EVAL("? this is a comment-only line", "NaN");
    CHECK_EVAL("   ? this is an indented comment-only line", "NaN");
    CHECK_EVAL("21", "21");
    CHECK_EVAL("? foo", "NaN");
    CHECK_EVAL("1+ans", "22");
}

void test_comment_and_description_edge_cases()
{
    // Autofix should normalize spacing before comments while preserving comment text.
    CHECK_AUTOFIX("x=1?foo", "x=1 ? foo");
    CHECK_AUTOFIX("x=1    ?    foo bar", "x=1 ? foo bar");
    CHECK_AUTOFIX("1+2 ? foo ? bar", "1+2 ? foo ? bar");

    // Interpreted and display-interpreted output should preserve comment tails.
    CHECK_INTERPRETED("2×pi ? calc area", "2·pi ? calc area");
    CHECK_DISPLAY_INTERPRETED(
        "2*3?c",
        QStringLiteral("2") + space + QString(MathDsl::MulCrossOp)
            + space + QStringLiteral("3 ? c"));
    CHECK_INTERPRETED("1[metre] -> [cm]",
                      u8"1[metre]→cm");
    CHECK_INTERPRETED("1[metre] in [cm]",
                      u8"1[metre]→cm");

    // Explicit empty description should be stored as empty.
    CHECK_EVAL("vardesc2 = 1 ? ", "1");
    CHECK_USERVAR_DESC("vardesc2", "");
    CHECK_USERFUNC_SET("funcdesc2(x) = x + 1 ? ");
    CHECK_USERFUNC_DESC("funcdesc2", "");

    // Description text should preserve punctuation and additional question marks.
    CHECK_EVAL("vardesc3 = 3 ? unit m/s ? approx", "3");
    CHECK_USERVAR_DESC("vardesc3", "unit m/s ? approx");
}

void test_user_functions()
{
    // Check user functions can be defined and used
    CHECK_USERFUNC_SET("func1(x;b) = x * b + 10");
    CHECK_USERFUNC_SET("gf(x) = cos x^-pi");
    CHECK_USERFUNC_INTERPRETED("gf", "cos(x^(-pi))");
    CHECK_EVAL("func1(2;5)", "20"); // = 2 * 5 + 10
    // Check some expected error conditions
    CHECK_EVAL_FAIL("func1()");
    CHECK_EVAL_FAIL("func1(2)");
    CHECK_EVAL_FAIL("func1(2;5;1)");

    // Check user functions can call other user functions
    CHECK_USERFUNC_SET("func2(x;b) = func1(x;b) - 10");
    CHECK_EVAL("func2(2;5)", "10"); // = (2 * 5 + 10) - 10

    // Check user functions can be redefined
    CHECK_USERFUNC_SET("func2(x;b) = 10 + func1(x;b)");
    CHECK_EVAL("func2(2;5)", "30"); // = 10 + (2 * 5 + 10)

    // Check user functions can refer to other user functions not defined
    CHECK_USERFUNC_SET("func2(x;b) = 10 * x + func3(x;b)");
    CHECK_EVAL_FAIL("func2(2;5)");
    CHECK_USERFUNC_SET("func3(x;b) = x - b");
    CHECK_EVAL("func2(2;5)", "17"); // = 10 * 2 + (2 - 5)

    // Check user functions can call builtin functions
    CHECK_USERFUNC_SET("func2(x;b) = sum(func1(x;b);5)");
    CHECK_EVAL("func2(2;5)", "25"); // = sum((2 * 5 + 10);5)

    // Check recursive user functions are not allowed
    CHECK_USERFUNC_SET("func_r(x) = x * func_r(x)");
    CHECK_EVAL_FAIL("func_r(1)");
    CHECK_USERFUNC_SET("func_r1(x) = x * func_r2(x)");
    CHECK_USERFUNC_SET("func_r2(x) = x + func_r1(x)");
    CHECK_EVAL_FAIL("func_r1(1)");

    // Check user functions can refer to user variables
    CHECK_USERFUNC_SET("func1(x;b) = x * b - var1");
    CHECK_EVAL_FAIL("func1(2;5)");
    CHECK_EVAL("var1 = 5", "5");
    CHECK_EVAL("func1(2;5)", "5");  // = 2 * 5 - 5

    // Check conflicts between the names of user functions arguments and user variables
    CHECK_EVAL("arg1 = 10", "10");
    CHECK_USERFUNC_SET("func1(arg1;arg2) = arg1 * arg2");
    CHECK_EVAL("func1(2;5)", "10"); // = 2 * 5 (not 10 * 5)

    // Check user functions names can not mask builtin functions
    CHECK_USERFUNC_SET_FAIL("sum(x;b) = x * b");

    // Check user functions names can not mask user variables
    CHECK_EVAL("var1 = 5", "5");
    CHECK_USERFUNC_SET_FAIL("var1(x;b) = x * b");

    // Check user functions can take no argument
    CHECK_EVAL("func1() = 2 * 10", "20");
    // NOTE: we use CHECK_EVAL to define the function, because its result is known at definition
    //       time. CHECK_USERFUNC_SET expects NaN, which is what is returned when the function body
    //       contains at least one component whose value can not be known at definition time
    //       (e.g., reference to user function arguments or to user/builtin functions).
    CHECK_EVAL("func1()", "20");    // = 2 * 5

    // Check optional descriptions on user function definitions.
    CHECK_USERFUNC_SET("funcdesc(x) = x + 1 ? Calculate Foo");
    CHECK_USERFUNC_DESC("funcdesc", "Calculate Foo");
    CHECK_EVAL("funcdesc(2)", "3");
    CHECK_USERFUNC_SET(
        QStringLiteral("testshift(x)=1") + space + QStringLiteral("<<")
            + space + QStringLiteral("x+0"));
    CHECK_EVAL("testshift(8)", "256");

    // Check redefining a function without description clears it.
    CHECK_USERFUNC_SET("funcdesc(x) = x + 2");
    CHECK_USERFUNC_DESC("funcdesc", "");

    // Check optional descriptions on user variable definitions.
    CHECK_EVAL("vardesc = 7 ? Lucky seven", "7");
    CHECK_USERVAR_DESC("vardesc", "Lucky seven");
    CHECK_EVAL("vardesc", "7");

    // Check redefining a variable without description clears it.
    CHECK_EVAL("vardesc = 8", "8");
    CHECK_USERVAR_DESC("vardesc", "");
}

void test_user_units()
{
    CHECK_EVAL("[two_cubic_metres] = 2[m^3]", "2 cubic_metre");
    CHECK_EVAL("1[two_cubic_metres] -> [m^3]", "2 m³");

    CHECK_EVAL("[ten_metres] = 10[m]", "10 metre");
    CHECK_EVAL("3[ten_metres] -> [m]", "30 m");

    CHECK_EVAL("[cm_s] = [cm/s]", "0.01 metre·second⁻¹");
    CHECK_EVAL("100[cm_s] -> [m/s]", "1 m·s⁻¹");

    CHECK_EVAL("[unit_desc] = [m/s] ? speed alias", "1 metre·second⁻¹");
    CHECK_USERUNIT_DESC("unit_desc", "speed alias");
}

void test_complex()
{
    // Check for basic complex number processing
    CHECK_EVAL("1j", "1i");
    CHECK_EVAL("0.1j", "0.1i");
    CHECK_EVAL(".1j", "0.1i");
    CHECK_EVAL("1E12j", "1000000000000i");
    CHECK_EVAL("0.1E12j", "100000000000i");
    CHECK_EVAL("1E-12j", "0.000000000001i");
    CHECK_EVAL("0.1E-12j", "0.0000000000001i");
    // Check for some bugs introduced by first versions of complex number processing
    CHECK_EVAL("0.1", "0.1");
    // Check for basic complex number evaluation
    CHECK_EVAL("(1+1j)*(1-1j)", "2");
    CHECK_EVAL("(1+1j)*(1+1j)", "2i");


    CHECK_EVAL("VARIANCE(1j;-1j)", "1");
    CHECK_EVAL("VARIANCE(1j;-1j;1;-1)", "1");
    CHECK_EVAL("VARIANCE(2j;-2j;1;-1)", "2.5");
}

void test_angle_mode(Settings* settings)
{
    const char savedExponentStyle =
        settings->unitNegativeExponentStyle;
    settings->unitNegativeExponentStyle = Settings::UnitNegativeExponentSuperscript;
    setRuntimeUnitNegativeExponentStyle(Settings::UnitNegativeExponentSuperscript);

    const auto checkAngularRateDisplayContains = [&](const QString& expression,
                                                     char angleUnit,
                                                     const QString& angleSymbol) {
        settings->angleUnit = angleUnit;
        Evaluator::instance()->initializeAngleUnits();
        ++eval_total_tests;
        eval->setExpression(expression);
        const QString display = NumberFormatter::format(eval->evalUpdateAns());
        const QString expectedAngleToken =
            angleUnit == 'd' ? Units::degreeAliasSymbol() : angleSymbol;
        const bool hasAngleSymbol = display.contains(expectedAngleToken);
        const bool hasSuperscriptPerSecond = display.contains(QString::fromUtf8("s⁻¹"));
        const bool hasSlashPerSecond = display.contains(QStringLiteral("/s"))
                                       || display.contains(QStringLiteral("/ s"));
        if (!eval->error().isEmpty()
            || !hasAngleSymbol
            || !hasSuperscriptPerSecond
            || hasSlashPerSecond) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tangle mode angular-rate display unit\t[NEW]" << endl
                 << "\tExpression: " << expression.toUtf8().constData() << endl
                 << "\tResult: " << display.toUtf8().constData() << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
        }
    };
    const auto checkAngleCompositeProductDisplayContains = [&](const QString& expression,
                                                               char angleUnit,
                                                               const QString& angleSymbol,
                                                               const QStringList& requiredTokens
                                                               = QStringList{QString(::unitSymbol(UnitId::Second))},
                                                               const QStringList& forbiddenTokens
                                                               = QStringList{}) {
        settings->angleUnit = angleUnit;
        Evaluator::instance()->initializeAngleUnits();
        ++eval_total_tests;
        eval->setExpression(expression);
        const QString display = NumberFormatter::format(eval->evalUpdateAns());
        const QString expectedAngleToken =
            angleUnit == 'd' ? Units::degreeAliasSymbol() : angleSymbol;
        const bool hasAngleToken = display.contains(expectedAngleToken);
        bool hasRequiredTokens = true;
        for (const QString& token : requiredTokens) {
            if (!display.contains(token)) {
                hasRequiredTokens = false;
                break;
            }
        }
        bool hasForbiddenTokens = false;
        for (const QString& token : forbiddenTokens) {
            if (display.contains(token)) {
                hasForbiddenTokens = true;
                break;
            }
        }
        if (!eval->error().isEmpty()
            || !hasAngleToken
            || !hasRequiredTokens
            || hasForbiddenTokens)
        {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tangle mode composite-angle product display unit\t[NEW]" << endl
                 << "\tExpression: " << expression.toUtf8().constData() << endl
                 << "\tResult: " << display.toUtf8().constData() << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
        }
    };
    const auto checkCompositeAngleAliasDisplay = [&](const QString& expression,
                                                     const QString& expectedAlias,
                                                     const QString& forbiddenSymbol) {
        ++eval_total_tests;
        eval->setExpression(expression);
        const QString display = NumberFormatter::format(eval->evalUpdateAns());
        if (!eval->error().isEmpty()
            || !display.contains(expectedAlias)
            || display.contains(forbiddenSymbol))
        {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tcomposite angle alias display unit\t[NEW]" << endl
                 << "\tExpression: " << expression.toUtf8().constData() << endl
                 << "\tResult: " << display.toUtf8().constData() << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
        }
    };
    const auto checkDivByAngleKeepsAngleDisplay = [&](const QString& expression,
                                                       const QString& expectedAngleToken) {
        ++eval_total_tests;
        eval->setExpression(expression);
        const QString display = NumberFormatter::format(eval->evalUpdateAns());
        if (!eval->error().isEmpty()
            || !display.contains(QString(::unitSymbol(UnitId::Metre)))
            || !display.contains(expectedAngleToken)
            || display.endsWith(QStringLiteral("[m]")))
        {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tlength-per-angle keeps angle denominator display unit\t[NEW]" << endl
                 << "\tExpression: " << expression.toUtf8().constData() << endl
                 << "\tResult: " << display.toUtf8().constData() << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
        }
    };
    const auto checkAngleExponentAliasInInterpretedExpression =
        [&](const QString& expression,
            const QString& expectedToken,
            const QString& forbiddenToken) {
        ++eval_total_tests;
        eval->setExpression(expression);
        eval->evalUpdateAns();
        const QString interpreted = Evaluator::formatInterpretedExpressionForDisplay(
            eval->interpretedExpression());
        if (!eval->error().isEmpty()
            || !interpreted.contains(expectedToken)
            || interpreted.contains(forbiddenToken))
        {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tangle exponent alias preserved in interpreted expression\t[NEW]" << endl
                 << "\tExpression: " << expression.toUtf8().constData() << endl
                 << "\tInterpreted: " << interpreted.toUtf8().constData() << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
        }
    };
    const auto checkAngleInversePowerUnitDisplay = [&](const QString& expression,
                                                       const QString& expectedUnitToken) {
        ++eval_total_tests;
        eval->setExpression(expression);
        const QString display = NumberFormatter::format(eval->evalUpdateAns());
        const QString bracketedExpectedToken =
            QString(MathDsl::UnitStart) + expectedUnitToken + QString(MathDsl::UnitEnd);
        if (!eval->error().isEmpty()
            || !display.contains(bracketedExpectedToken))
        {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\texplicit angle inverse-power keeps display unit\t[NEW]" << endl
                 << "\tExpression: " << expression.toUtf8().constData() << endl
                 << "\tResult: " << display.toUtf8().constData() << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
        }
    };

    settings->angleUnit = 'r';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL("sin(pi)", "0");
    CHECK_EVAL("arcsin(-1)", "-1.57079632679489661923");
    CHECK_EVAL("arccos(0)", "1.57079632679489661923");
    CHECK_EVAL("tan(arctan(1))", "1");
    CHECK_EVAL("tan(arctan2(1;1))", "1");
    CHECK_EVAL("sin(arcsin(0.25))", "0.25");
    CHECK_EVAL("cos(arccos(0.25))", "0.25");
    CHECK_EVAL("tan(arctan(0.25))", "0.25");
    CHECK_EVAL("sin(1j)", "1.17520119364380145688i");
    CHECK_EVAL("arcsin(-2)", "-1.57079632679489661923+1.31695789692481670863i");
    CHECK_EVAL("[radian]","1 rad");
    CHECK_EVAL("[rad]","1 rad");
    CHECK_EVAL("cos(pi*[rad])", "-1");
    CHECK_EVAL(QString::fromUtf8("cos(180°)"), "-1");
    CHECK_EVAL("cos(180*[degree])", "-1");
    CHECK_EVAL("cos(200*[gradian])", "-1");
    CHECK_EVAL("cos(200*[gon])", "-1");
    CHECK_EVAL("cos(0.5*[turn])", "-1");
    CHECK_EVAL("cos(0.5*[revolution])", "-1");
    CHECK_EVAL("cos(0.5*[rev])", "-1");
    CHECK_EVAL("[degree]","0.01745329251994329577 rad");
    CHECK_EVAL("[deg]","0.01745329251994329577 rad");
    CHECK_EVAL(QString::fromUtf8("[˚]"), "0.01745329251994329577 rad");
    CHECK_EVAL("[gradian]","0.01570796326794896619 rad");
    CHECK_EVAL("[gon]","0.01570796326794896619 rad");
    CHECK_EVAL("[turn]","6.28318530717958647693 rad");
    CHECK_EVAL("[tr]","6.28318530717958647693 rad");
    CHECK_EVAL("[revolution]","6.28318530717958647693 rad");
    CHECK_EVAL("[rev]","6.28318530717958647693 rad");
    CHECK_EVAL("1 [tr]", "6.28318530717958647693 rad");
    CHECK_EVAL(QString::fromUtf8("2 [µrad]"), "0.000002 rad");
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg second]"), 'r', Units::angleModeUnitSymbol('r'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg second metre]"), 'r', Units::angleModeUnitSymbol('r'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·s·m]"), 'r', Units::angleModeUnitSymbol('r'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·J·m·J·s/J²]"), 'r', Units::angleModeUnitSymbol('r'),
                                              QStringList{QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Second))},
                                              QStringList{QString(::unitSymbol(UnitId::Joule))});
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·m·N]"), 'r', Units::angleModeUnitSymbol('r'),
                                              QStringList{QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Newton))});
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·kg·m^3·s^-1]"), 'r', Units::angleModeUnitSymbol('r'),
                                              QStringList{QString(::unitSymbol(UnitId::Joule)),
                                                          QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Second))},
                                              QStringList{QString(::unitSymbol(UnitId::Kilogram))});
    checkCompositeAngleAliasDisplay(QString::fromUtf8("1 [rad/m] -> [°/m]"),
                                    Units::degreeAliasSymbol(),
                                    QString(UnicodeChars::DegreeSign));
    checkCompositeAngleAliasDisplay(QString::fromUtf8("1 [rad/m] -> [′/m]"),
                                    Units::arcminuteAliasSymbol(),
                                    QString(UnicodeChars::Prime));
    checkCompositeAngleAliasDisplay(QString::fromUtf8("1 [rad/m] -> [″/m]"),
                                    Units::arcsecondAliasSymbol(),
                                    QString(UnicodeChars::DoublePrime));
    checkAngleExponentAliasInInterpretedExpression(
        QStringLiteral("1 [arcmin²]"),
        QStringLiteral("arcmin") + MathDsl::Pow2,
        QString(UnicodeChars::Prime) + MathDsl::Pow2);
    checkAngleExponentAliasInInterpretedExpression(
        QStringLiteral("1 [arcmin") + MathDsl::PowNeg + MathDsl::Pow1 + QStringLiteral("]"),
        QStringLiteral("arcmin") + MathDsl::PowNeg + MathDsl::Pow1,
        QString(UnicodeChars::Prime) + MathDsl::PowNeg + MathDsl::Pow1);
    checkDivByAngleKeepsAngleDisplay(QStringLiteral("1 [m/arcmin]"), Units::angleModeUnitSymbol('r'));
    checkAngularRateDisplayContains(QStringLiteral("360 [deg second^-1]"), 'r', Units::angleModeUnitSymbol('r'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rev/min]"), 'r', Units::angleModeUnitSymbol('r'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rev*min^-1]"), 'r', Units::angleModeUnitSymbol('r'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rpm]"), 'r', Units::angleModeUnitSymbol('r'));

    settings->angleUnit = 'd';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL("sin(180)", "0");
    CHECK_EVAL("arcsin(-1)", "-90");
    CHECK_EVAL("arccos(0)", "90");
    CHECK_EVAL("arctan(1)", "45");
    CHECK_EVAL("arctan2(1;1)", "45");
    CHECK_EVAL("sin(arcsin(0.25))", "0.25");
    CHECK_EVAL("cos(arccos(0.25))", "0.25");
    CHECK_EVAL("tan(arctan(0.25))", "0.25");
    CHECK_EVAL_FAIL("sin(1j)");
    CHECK_EVAL("arcsin(-2)", "-90+75.4561292902168920041i");
    CHECK_EVAL("[radian]","57.2957795130823208768°");
    CHECK_EVAL("[rad]","57.2957795130823208768°");
    CHECK_EVAL("cos(pi*[rad])", "-1");
    CHECK_EVAL("cos(180[rad])", "-0.59846006905785813897");
    CHECK_EVAL("cos(180*[degree])", "-1");
    CHECK_EVAL("cos(200*[gradian])", "-1");
    CHECK_EVAL("cos(200*[gon])", "-1");
    CHECK_EVAL("cos(0.5*[turn])", "-1");
    CHECK_EVAL("cos(0.5*[revolution])", "-1");
    CHECK_EVAL("cos(0.5*[rev])", "-1");
    CHECK_EVAL("pi [rad]", "180°");
    CHECK_EVAL("pi[rad] -> [°]", "180°");
    CHECK_EVAL("[degree]","1°");
    CHECK_EVAL("[deg]","1°");
    CHECK_EVAL("1 -> [degree]", "1°");
    CHECK_EVAL("1 -> [deg]", "1°");
    CHECK_EVAL("[gradian]","0.9°");
    CHECK_EVAL("[gon]","0.9°");
    CHECK_EVAL("[turn]","360°");
    CHECK_EVAL("[tr]","360°");
    CHECK_EVAL("[revolution]","360°");
    CHECK_EVAL("[rev]","360°");
    CHECK_EVAL("1 [tr]", "360°");
    CHECK_EVAL(QString::fromUtf8("2 [µrad]"), "0.00011459155902616464°");
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg second]"), 'd', Units::angleModeUnitSymbol('d'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg second metre]"), 'd', Units::angleModeUnitSymbol('d'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·s·m]"), 'd', Units::angleModeUnitSymbol('d'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·J·m·J·s/J²]"), 'd', Units::angleModeUnitSymbol('d'),
                                              QStringList{QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Second))},
                                              QStringList{QString(::unitSymbol(UnitId::Joule))});
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·m·N]"), 'd', Units::angleModeUnitSymbol('d'),
                                              QStringList{QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Newton))});
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·kg·m^3·s^-1]"), 'd', Units::angleModeUnitSymbol('d'),
                                              QStringList{QString(::unitSymbol(UnitId::Joule)),
                                                          QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Second))},
                                              QStringList{QString(::unitSymbol(UnitId::Kilogram))});
    ++eval_total_tests;
    eval->setExpression(QStringLiteral("360 [deg second^-1]"));
    {
        const QString display = NumberFormatter::format(eval->evalUpdateAns());
        if (!eval->error().isEmpty()
            || !display.contains(QStringLiteral("deg"))
            || !display.contains(QString::fromUtf8("s⁻¹"))
            || display.contains(QString::fromUtf8("°")))
        {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tdegree alias preserved in composite angle unit\t[NEW]" << endl
                 << "\tResult: " << display.toUtf8().constData() << endl
                 << "\tError : " << qPrintable(eval->error()) << endl;
        }
    }
    checkAngularRateDisplayContains(QStringLiteral("360 [deg second^-1]"), 'd', Units::angleModeUnitSymbol('d'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rev/min]"), 'd', Units::angleModeUnitSymbol('d'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rev*min^-1]"), 'd', Units::angleModeUnitSymbol('d'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rpm]"), 'd', Units::angleModeUnitSymbol('d'));
    checkDivByAngleKeepsAngleDisplay(QStringLiteral("1 [m/arcmin]"), Units::degreeAliasSymbol());
    CHECK_EVAL_KNOWN_ISSUE("arcsin(0.25)", "14.47751218592992387877", 781);

    settings->angleUnit = 'g';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL("sin(200)", "0");
    CHECK_EVAL("arcsin(-1)", "-100");
    CHECK_EVAL("arccos(0)", "100");
    CHECK_EVAL("arctan(1)", "50");
    CHECK_EVAL("arctan2(1;1)", "50");
    CHECK_EVAL("sin(arcsin(0.25))", "0.25");
    CHECK_EVAL("cos(arccos(0.25))", "0.25");
    CHECK_EVAL("tan(arctan(0.25))", "0.25");
    CHECK_EVAL_FAIL("sin(1j)");
    CHECK_EVAL("arcsin(-2)", "-100+83.84014365579654667122i");
    CHECK_EVAL("[radian]","63.66197723675813430755 gon");
    CHECK_EVAL("[rad]","63.66197723675813430755 gon");
    CHECK_EVAL("cos(pi*[rad])", "-1");
    CHECK_EVAL("cos(180*[degree])", "-1");
    CHECK_EVAL("cos(200*[gradian])", "-1");
    CHECK_EVAL("cos(200*[gon])", "-1");
    CHECK_EVAL("cos(0.5*[turn])", "-1");
    CHECK_EVAL("cos(0.5*[revolution])", "-1");
    CHECK_EVAL("cos(0.5*[rev])", "-1");
    CHECK_EVAL("[degree]","1.11111111111111111111 gon");
    CHECK_EVAL("[deg]","1.11111111111111111111 gon");
    CHECK_EVAL("[gradian]","1 gon");
    CHECK_EVAL("[gon]","1 gon");
    CHECK_EVAL("[turn]","400 gon");
    CHECK_EVAL("[tr]","400 gon");
    CHECK_EVAL("[revolution]","400 gon");
    CHECK_EVAL("[rev]","400 gon");
    CHECK_EVAL("1 [tr]", "400 gon");
    checkAngleInversePowerUnitDisplay(
        QStringLiteral("1 [arcmin") + MathDsl::PowNeg + MathDsl::Pow2 + QStringLiteral("]"),
        Units::angleModeUnitSymbol('g') + QString(MathDsl::PowNeg) + QString(MathDsl::Pow2));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg second]"), 'g', Units::angleModeUnitSymbol('g'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg second metre]"), 'g', Units::angleModeUnitSymbol('g'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·s·m]"), 'g', Units::angleModeUnitSymbol('g'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·J·m·J·s/J²]"), 'g', Units::angleModeUnitSymbol('g'),
                                              QStringList{QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Second))},
                                              QStringList{QString(::unitSymbol(UnitId::Joule))});
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·m·N]"), 'g', Units::angleModeUnitSymbol('g'),
                                              QStringList{QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Newton))});
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·kg·m^3·s^-1]"), 'g', Units::angleModeUnitSymbol('g'),
                                              QStringList{QString(::unitSymbol(UnitId::Joule)),
                                                          QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Second))},
                                              QStringList{QString(::unitSymbol(UnitId::Kilogram))});
    checkAngularRateDisplayContains(QStringLiteral("1 [rev/min]"), 'g', Units::angleModeUnitSymbol('g'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rev*min^-1]"), 'g', Units::angleModeUnitSymbol('g'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rpm]"), 'g', Units::angleModeUnitSymbol('g'));

    settings->angleUnit = 't';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL("sin(0.5)", "0");
    CHECK_EVAL("arcsin(-1)", "-0.25");
    CHECK_EVAL("arccos(0)", "0.25");
    CHECK_EVAL("arctan(1)", "0.125");
    CHECK_EVAL("arctan2(1;1)", "0.125");
    CHECK_EVAL("sin(arcsin(0.25))", "0.25");
    CHECK_EVAL("cos(arccos(0.25))", "0.25");
    CHECK_EVAL("tan(arctan(0.25))", "0.25");
    CHECK_EVAL_FAIL("sin(1j)");
    CHECK_EVAL("arcsin(-2)", "-0.25+0.20960035913949136668i");
    CHECK_EVAL("[radian]","0.15915494309189533577 tr");
    CHECK_EVAL("[rad]","0.15915494309189533577 tr");
    CHECK_EVAL("cos(pi*[rad])", "-1");
    CHECK_EVAL("cos(180*[degree])", "-1");
    CHECK_EVAL("cos(200*[gradian])", "-1");
    CHECK_EVAL("cos(200*[gon])", "-1");
    CHECK_EVAL("cos(0.5*[turn])", "-1");
    CHECK_EVAL("cos(0.5*[revolution])", "-1");
    CHECK_EVAL("cos(0.5*[rev])", "-1");
    CHECK_EVAL("[degree]","0.00277777777777777778 tr");
    CHECK_EVAL("[deg]","0.00277777777777777778 tr");
    CHECK_EVAL("[gradian]","0.0025 tr");
    CHECK_EVAL("[gon]","0.0025 tr");
    CHECK_EVAL("[turn]","1 tr");
    CHECK_EVAL("[tr]","1 tr");
    CHECK_EVAL("[revolution]","1 tr");
    CHECK_EVAL("[rev]","1 tr");
    CHECK_EVAL("1 [tr]", "1 tr");
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg second]"), 't', Units::angleModeUnitSymbol('t'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg second metre]"), 't', Units::angleModeUnitSymbol('t'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·s·m]"), 't', Units::angleModeUnitSymbol('t'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·J·m·J·s/J²]"), 't', Units::angleModeUnitSymbol('t'),
                                              QStringList{QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Second))},
                                              QStringList{QString(::unitSymbol(UnitId::Joule))});
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·m·N]"), 't', Units::angleModeUnitSymbol('t'),
                                              QStringList{QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Newton))});
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·kg·m^3·s^-1]"), 't', Units::angleModeUnitSymbol('t'),
                                              QStringList{QString(::unitSymbol(UnitId::Joule)),
                                                          QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Second))},
                                              QStringList{QString(::unitSymbol(UnitId::Kilogram))});
    checkAngularRateDisplayContains(QStringLiteral("1 [rev/min]"), 't', Units::angleModeUnitSymbol('t'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rev*min^-1]"), 't', Units::angleModeUnitSymbol('t'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rpm]"), 't', Units::angleModeUnitSymbol('t'));

    settings->angleUnit = 'v';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL("sin(0.5)", "0");
    CHECK_EVAL("arcsin(-1)", "-0.25");
    CHECK_EVAL("arccos(0)", "0.25");
    CHECK_EVAL("arctan(1)", "0.125");
    CHECK_EVAL("arctan2(1;1)", "0.125");
    CHECK_EVAL("sin(arcsin(0.25))", "0.25");
    CHECK_EVAL("cos(arccos(0.25))", "0.25");
    CHECK_EVAL("tan(arctan(0.25))", "0.25");
    CHECK_EVAL_FAIL("sin(1j)");
    CHECK_EVAL("arcsin(-2)", "-0.25+0.20960035913949136668i");
    CHECK_EVAL("[radian]","0.15915494309189533577 rev");
    CHECK_EVAL("[rad]","0.15915494309189533577 rev");
    CHECK_EVAL("cos(pi*[rad])", "-1");
    CHECK_EVAL("cos(180*[degree])", "-1");
    CHECK_EVAL("cos(200*[gradian])", "-1");
    CHECK_EVAL("cos(200*[gon])", "-1");
    CHECK_EVAL("cos(0.5*[turn])", "-1");
    CHECK_EVAL("cos(0.5*[revolution])", "-1");
    CHECK_EVAL("cos(0.5*[rev])", "-1");
    CHECK_EVAL("[degree]","0.00277777777777777778 rev");
    CHECK_EVAL("[deg]","0.00277777777777777778 rev");
    CHECK_EVAL("[gradian]","0.0025 rev");
    CHECK_EVAL("[gon]","0.0025 rev");
    CHECK_EVAL("[turn]","1 rev");
    CHECK_EVAL("[tr]","1 rev");
    CHECK_EVAL("[revolution]","1 rev");
    CHECK_EVAL("[rev]","1 rev");
    CHECK_EVAL("1 [tr]", "1 rev");
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg second]"), 'v', Units::angleModeUnitSymbol('v'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg second metre]"), 'v', Units::angleModeUnitSymbol('v'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·s·m]"), 'v', Units::angleModeUnitSymbol('v'));
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·J·m·J·s/J²]"), 'v', Units::angleModeUnitSymbol('v'),
                                              QStringList{QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Second))},
                                              QStringList{QString(::unitSymbol(UnitId::Joule))});
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·m·N]"), 'v', Units::angleModeUnitSymbol('v'),
                                              QStringList{QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Newton))});
    checkAngleCompositeProductDisplayContains(QStringLiteral("360 [deg·kg·m^3·s^-1]"), 'v', Units::angleModeUnitSymbol('v'),
                                              QStringList{QString(::unitSymbol(UnitId::Joule)),
                                                          QString(::unitSymbol(UnitId::Metre)),
                                                          QString(::unitSymbol(UnitId::Second))},
                                              QStringList{QString(::unitSymbol(UnitId::Kilogram))});
    checkAngularRateDisplayContains(QStringLiteral("1 [rev/min]"), 'v', Units::angleModeUnitSymbol('v'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rev*min^-1]"), 'v', Units::angleModeUnitSymbol('v'));
    checkAngularRateDisplayContains(QStringLiteral("1 [rpm]"), 'v', Units::angleModeUnitSymbol('v'));

    settings->unitNegativeExponentStyle = savedExponentStyle;
    setRuntimeUnitNegativeExponentStyle(savedExponentStyle);
}

void test_implicit_multiplication()
{
    CHECK_EVAL("av = 5", "5");
    CHECK_EVAL("5av", "25");
    CHECK_EVAL("5.av", "25");
    CHECK_EVAL("5.0av", "25");
    CHECK_EVAL("5e2av", "2500");
    CHECK_EVAL_FAIL("a5");
    CHECK_EVAL("av 5", "25");
    CHECK_EVAL("2av^3", "250");
    CHECK_EVAL("bb=2", "2");
    CHECK_EVAL_FAIL("ab");
    CHECK_EVAL("b=3", "3");
    CHECK_EVAL("av b", "15");
    CHECK_EVAL("av[b]", "5 b");
    CHECK_EVAL("eps = 10", "10");
    CHECK_EVAL("5 eps", "50");
    CHECK_EVAL("Eren = 1001", "1001");
    CHECK_EVAL("7 Eren", "7007");
    CHECK_EVAL("Eren 5", "5005");
    CHECK_EVAL("f() = 123", "123");
    CHECK_EVAL("2f()", "246");
    CHECK_EVAL("5   5", "55");
    CHECK_INTERPRETED("av*b", "av·b");
    CHECK_INTERPRETED("av*(b)", "av·b");
    CHECK_INTERPRETED("sin(pi)*cos(pi)", "sin(pi)·cos(pi)");
    CHECK_INTERPRETED("f()*av", "f()·av");
    CHECK_INTERPRETED("sqrt(4)*av", "√(4)·av");
    CHECK_INTERPRETED("av*2", "av·2");
    CHECK_INTERPRETED("2*av", "2·av");
    CHECK_INTERPRETED("2*3", "2×3");

    // Check implicit multiplication between numbers fails
    // CHECK_EVAL_FAIL("10.   0.2");
    CHECK_EVAL_FAIL("10 0x10");
    CHECK_EVAL_FAIL("10 #10");
    CHECK_EVAL_FAIL("0b104");
    CHECK_EVAL_FAIL("0b10 4");
    CHECK_EVAL_FAIL("0b10 0x4");
    CHECK_EVAL_FAIL("0o109");
    CHECK_EVAL_FAIL("0o10 9");
    CHECK_EVAL_FAIL("0o10 0x9");
    // CHECK_EVAL_FAIL("12.12.12");
    CHECK_EVAL_FAIL("12e12.12");
    CHECK_EVAL("0b10av", "10");
    CHECK_EVAL("0o2av", "10");
    CHECK_EVAL("5(5)", "25");

    CHECK_EVAL("av sin(pi/2)", "5");
    CHECK_EVAL("av sqrt(4)",   "10");
    CHECK_EVAL("av sqrt(av^2)", "25");
    CHECK_INTERPRETED("2×sin pi", "2·sin(pi)");
    CHECK_INTERPRETED("av sin(pi/2)", "av·sin(pi/2)");
    CHECK_INTERPRETED("av sqrt(4)", "av·√(4)");
    CHECK_INTERPRETED("av sqrt(av^2)", "av·√(av^2)");

    /* Tests issue 538 */
    /* 3 sin (3 pi) was evaluated but not 3 sin (3) */
    CHECK_EVAL("3 sin (3 pi)", "0");
    CHECK_EVAL("3 sin (3)",    "0.4233600241796016663");
    CHECK_INTERPRETED("3 sin (3 pi)", "3·sin(3·pi)");
    CHECK_INTERPRETED("3 sin (3)", "3·sin(3)");

    CHECK_EVAL("2 (2 + 1)", "6");
    CHECK_EVAL("2 (av)", "10");
    CHECK_INTERPRETED("2 (2 + 1)", "2·(2+1)");
    CHECK_INTERPRETED("2 (av)", "2·av");
    CHECK_INTERPRETED("(1+2)(3+4)", "(1+2)·(3+4)");
    CHECK_INTERPRETED("(-1+2)(3-4)", "(-1+2)·(3-4)");

    /* Tests issue 598 */
    CHECK_EVAL("2(av)^3", "250");
    CHECK_INTERPRETED("2(av)^3", "2·av^3");

    CHECK_EVAL("6/2(2+1)", "9");
    CHECK_INTERPRETED("6/2(2+1)", "(6/2)·(2+1)");
    CHECK_INTERPRETED("1/2 sqrt(3)", "(1/2)·√(3)");
    CHECK_INTERPRETED("1/2(2+3)", "(1/2)·(2+3)");
    CHECK_INTERPRETED("4/2[s]", "4/2[s]");
    CHECK_INTERPRETED("2/3(4/5)", "(2/3)·((4/5))");
    CHECK_INTERPRETED("10\\3(2)", "(10\\3)·2");
    CHECK_INTERPRETED("-2(3+4)", "-(2·(3+4))");
    CHECK_INTERPRETED("~2(3)", "~(2·3)");
    CHECK_INTERPRETED("2^2(2)", "2^2·2");
    CHECK_INTERPRETED("2^2(2)(2)", "2^2·(2·2)");
    CHECK_INTERPRETED("2^2(2*2)", "2^2·(2×2)");
    CHECK_INTERPRETED("2^2(2)+3", "2^2·2+3");
    CHECK_INTERPRETED("2 sin pi cos pi + 2", "2·sin(pi)·cos(pi)+2");
    CHECK_INTERPRETED("1[metre second^-1]", "1[metre·second^(-1)]");
    CHECK_INTERPRETED("1[second^-2]", "1[second^(-2)]");
    CHECK_INTERPRETED("1/2^3", "1/2^3");
    CHECK_INTERPRETED("2^12!", "2^(12!)");
    CHECK_INTERPRETED("2^12.1!", "2^(12.1!)");
    CHECK_INTERPRETED("1/2!", "1/(2!)");
    CHECK_INTERPRETED("2^12-2", "2^12-2");
    CHECK_INTERPRETED("2^12.-2", "2^12-2");
    CHECK_INTERPRETED("2^12.000-2", "2^12-2");
    CHECK_INTERPRETED("2^12.12", "2^(12.12)");
    CHECK_INTERPRETED("2^12.000-2+1/(1×2^3×3)-2^12!+2^12.1!",
                      "2^12-2+1/(1·(2^3)·3)-2^(12!)+2^(12.1!)");
    CHECK_INTERPRETED("123.567 × 10^0", "123.567·10^0");
    CHECK_INTERPRETED(QString::fromUtf8("pi  −−−−−3"), "pi-3");
    CHECK_INTERPRETED(QString::fromUtf8("pi  −−−−−−3"), "pi+3");
    CHECK_INTERPRETED("1/(1×2^3×3)", "1/(1·(2^3)·3)");
    CHECK_INTERPRETED("x=1/2 sqrt(3)", "x=(1/2)·√(3)");
    CHECK_INTERPRETED("gf(t)=t/2 sqrt(3)", "gf(t)=(t/2)·√(3)");
    CHECK_INTERPRETED("sin 23       cos 232323×pi×pi   2",
                      "sin(23)·cos(232323)·pi·pi·2");
    CHECK_INTERPRETED("sin 23       cos 232323×pi×pi   2×cos pi×pi×23",
                      "sin(23)·cos(232323)·pi·pi·2·cos(pi)·pi·23");
    CHECK_INTERPRETED("sin 23       cos 232323×pi×pi   2×cos pi×pi×23  23 × 323",
                      "sin(23)·cos(232323)·pi·pi·2·cos(pi)·pi·2323×323");
    CHECK_DISPLAY_INTERPRETED(QString::fromUtf8("123.567 × 10⁰"),
                              QString::fromUtf8("123.567 × 10⁰"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(QString::fromUtf8("123.567 × 10⁰"),
                                         QString::fromUtf8("123.567 × 10⁰"));
    CHECK_EVAL("2^2(2)", "8");
    CHECK_EVAL("2^2(2)(2)", "16");
    CHECK_EVAL("2^2(2*2)", "16");
    CHECK_EVAL("2^2(2)+3", "11");
    CHECK_EVAL_FAIL("pi (2)");

    CHECK_EVAL_KNOWN_ISSUE("2(7.5−0.5)+4(12−0.75)", "59", 1155);
    CHECK_EVAL_KNOWN_ISSUE("2+2(5)", "12", 1155);
}

void test_display_interpreted_spacing()
{
    Settings* settings = Settings::instance();
    const QString dot(MathDsl::MulDotOp);
    const QString multiplication = QString(MathDsl::MulCrossOp);
    const QString unicodeMinusSign(UnicodeChars::MinusSign);
    const QString dotSpaced = space + dot + space;
    const QString plus = space + QStringLiteral("+") + space;
    const QString minus = space + unicodeMinusSign + space;
    const QString times = space + multiplication + space;
    const QString divide = space + QStringLiteral("/") + space;
    const QString integerDivide = space + QStringLiteral("\\") + space;
    const QString equals = space + QStringLiteral("=") + space;
    const QString shiftLeft = space + QStringLiteral("<<") + space;
    const QString shiftRight = space + QStringLiteral(">>") + space;

    CHECK_DISPLAY_INTERPRETED(
        "sin(2·pi+3)-1+2×3·sin(pi)",
        QString::fromUtf8("sin(2")
            + dotSpaced
            + QStringLiteral("pi")
            + plus
            + QStringLiteral("3)")
            + minus
            + QStringLiteral("1")
            + plus
            + QStringLiteral("2")
            + times
            + QStringLiteral("3")
            + dotSpaced
            + QStringLiteral("sin(pi)"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("2×sin pi"),
        QString::fromUtf8("2")
            + dotSpaced
            + QStringLiteral("sin(pi)"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("2 * 358 [deg]"),
        QString::fromUtf8("2")
            + times
            + QStringLiteral("358")
            + QString::fromUtf8("°"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("2 [deg] * 3"),
        QStringLiteral("2")
            + QString::fromUtf8("°")
            + times
            + QStringLiteral("3"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("360 [°]"),
        QStringLiteral("360")
            + QString::fromUtf8("°"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("360 [deg]"),
        QStringLiteral("360")
            + QString::fromUtf8("°"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("360 [degree]"),
        QStringLiteral("360")
            + QString::fromUtf8("°"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("360 [°/s]"),
        QString::fromUtf8("360")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[deg/s]"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("360 [deg/s]"),
        QString::fromUtf8("360")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[deg/s]"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("360 [degree/s]"),
        QString::fromUtf8("360")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[deg/s]"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("360 [s⁻¹·°]"),
        QString::fromUtf8("360")
            + QString(MathDsl::QuantSp)
            + QString::fromUtf8("[deg · s⁻¹]"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("2 * x"),
        QString::fromUtf8("2")
            + dotSpaced
            + QStringLiteral("x"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("2 sin pi cos pi + 2"),
        QString::fromUtf8("2")
            + dotSpaced
            + QStringLiteral("sin(pi)")
            + dotSpaced
            + QStringLiteral("cos(pi)")
            + plus
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("2 sin pi cos pi + 2 × 23 / cos pi − sin −pi^2"),
        QString::fromUtf8("2")
            + dotSpaced
            + QStringLiteral("sin(pi)")
            + dotSpaced
            + QStringLiteral("cos(pi)")
            + plus
            + QStringLiteral("2")
            + times
            + QStringLiteral("23")
            + divide
            + QStringLiteral("cos(pi)")
            + minus
            + QStringLiteral("sin((")
            + unicodeMinusSign
            + QString::fromUtf8("pi)²)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("23/2+10\\3"),
        QStringLiteral("23")
            + divide
            + QStringLiteral("2")
            + plus
            + QStringLiteral("10")
            + integerDivide
            + QStringLiteral("3"));
    const bool complexNumbers = settings->complexNumbers;
    const char imaginaryUnit = settings->imaginaryUnit;
    const bool complexMode = DMath::complexMode;
    settings->complexNumbers = true;
    settings->imaginaryUnit = 'i';
    CMath::setImaginaryUnitSymbol(QLatin1Char('i'));
    DMath::complexMode = true;
    eval->initializeBuiltInVariables();
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2+3i"),
        QStringLiteral("2+3i"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2-3i"),
        QString::fromUtf8("2−3i"));
    settings->imaginaryUnit = 'j';
    CMath::setImaginaryUnitSymbol(QLatin1Char('j'));
    eval->initializeBuiltInVariables();
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2+3j"),
        QStringLiteral("2+3j"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2-3j"),
        QString::fromUtf8("2−3j"));
    DMath::complexMode = complexMode;
    settings->complexNumbers = complexNumbers;
    settings->imaginaryUnit = imaginaryUnit;
    CMath::setImaginaryUnitSymbol(QChar(imaginaryUnit));
    eval->initializeBuiltInVariables();
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1<<2"),
        QStringLiteral("1")
            + shiftLeft
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1>>2"),
        QStringLiteral("1")
            + shiftRight
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("sin -12"),
        QString::fromUtf8("sin(") + unicodeMinusSign + QStringLiteral("12)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1[metre second^-1]"),
        QStringLiteral("1")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[")
            + QStringLiteral("m")
            + dotSpaced
            + QString::fromUtf8("s⁻¹]")
            );
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1[second^-2]"),
        QStringLiteral("1")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[")
            + QString::fromUtf8("s⁻²]"));
    ++eval_total_tests;
    eval->setExpression(QStringLiteral("4.0015061833e-3[kg*mol^-1]"));
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tunit spacing with narrow no-break space\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QString unitJoin = QString(MathDsl::QuantSp) + QStringLiteral("[");
        const QString interpretedDisplayed =
            Evaluator::formatInterpretedExpressionForDisplay(eval->interpretedExpression());
        const QString simplifiedDisplayed = DisplayFormatUtils::applyDigitGroupingForDisplay(
            Evaluator::formatInterpretedExpressionSimplifiedForDisplay(eval->interpretedExpression()));
        const QString resultDisplayed = DisplayFormatUtils::applyDigitGroupingForDisplay(
            NumberFormatter::format(eval->evalUpdateAns()));
        if (!interpretedDisplayed.contains(unitJoin)
            || !simplifiedDisplayed.contains(unitJoin)
            || !resultDisplayed.contains(unitJoin)) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tunit spacing with narrow no-break space\t[NEW]" << endl
                 << "\tInterpreted: " << interpretedDisplayed.toUtf8().constData() << endl
                 << "\tSimplified : " << simplifiedDisplayed.toUtf8().constData() << endl
                 << "\tResult     : " << resultDisplayed.toUtf8().constData() << endl;
        }
    }
    ++eval_total_tests;
    eval->setExpression(QStringLiteral("x=0.5"));
    eval->evalUpdateAns();
    eval->setExpression(QStringLiteral("x[m]"));
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tvalue-unit spacing for variable attachment\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QString interpretedDisplayed =
            Evaluator::formatInterpretedExpressionForDisplay(eval->interpretedExpression());
        const QString expected =
            QStringLiteral("x")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[m]");
        if (interpretedDisplayed != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tvalue-unit spacing for variable attachment\t[NEW]" << endl
                 << "\tDisplayed: " << interpretedDisplayed.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }
    ++eval_total_tests;
    eval->setExpression(QStringLiteral("ffff()=1"));
    eval->evalUpdateAns();
    eval->setExpression(QStringLiteral("ffff()[m]"));
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tvalue-unit spacing for function attachment\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QString interpretedDisplayed =
            Evaluator::formatInterpretedExpressionForDisplay(eval->interpretedExpression());
        const QString expected =
            QStringLiteral("ffff()")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[m]");
        if (interpretedDisplayed != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tvalue-unit spacing for function attachment\t[NEW]" << endl
                 << "\tDisplayed: " << interpretedDisplayed.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }
    ++eval_total_tests;
    const QString valueUnitMulDisplayed =
        DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("750 · [h]"));
    const QString valueUnitMulExpected = QStringLiteral("750")
        + QString(MathDsl::QuantSp)
        + QStringLiteral("[h]");
    if (valueUnitMulDisplayed != valueUnitMulExpected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tvalue-unit multiplication keeps bracketed unit formatting\t[NEW]" << endl
             << "\tDisplayed : " << valueUnitMulDisplayed.toUtf8().constData() << endl
             << "\tExpected  : " << valueUnitMulExpected.toUtf8().constData() << endl;
    }
    ++eval_total_tests;
    eval->setExpression(QStringLiteral("750 · [h]"));
    const Quantity valueUnitMulResult = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tresult lines keep bracketed unit attachment\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QStringList lines = ResultLineFormatUtils::formatResultLinesForDisplay(
            QStringLiteral("750 · [h]"),
            eval->interpretedExpression(),
            valueUnitMulResult,
            true,
            true);
        const QString expectedLine = QStringLiteral("750")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[h]");
    if (lines.isEmpty() || lines.first() != expectedLine) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tresult lines keep bracketed unit attachment\t[NEW]" << endl
             << "\tFirst line: " << (lines.isEmpty() ? "<empty>" : lines.first().toUtf8().constData()) << endl
             << "\tExpected  : " << expectedLine.toUtf8().constData() << endl;
        }
    }
    ++eval_total_tests;
    {
        const QString explicitGrouped = ResultLineFormatUtils::formattedExpressionLineForDisplay(
            QStringLiteral("4/(2[s])"),
            QStringLiteral("4/2[s]"));
        const QString expected = QStringLiteral("4")
            + divide
            + QStringLiteral("(2")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[s])");
        if (explicitGrouped != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tpreserve explicit denominator grouping for unit attachment\t[NEW]" << endl
                 << "\tDisplayed: " << explicitGrouped.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }
    ++eval_total_tests;
    {
        const QString implicitGrouped = ResultLineFormatUtils::formattedExpressionLineForDisplay(
            QStringLiteral("4/2[s]"),
            QStringLiteral("4/2[s]"));
        const QString expected = QStringLiteral("4")
            + divide
            + QStringLiteral("2")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[s]");
        if (implicitGrouped != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tkeep implicit denominator unit attachment ungrouped\t[NEW]" << endl
                 << "\tDisplayed: " << implicitGrouped.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }
    ++eval_total_tests;
    eval->setExpression(QString::fromUtf8("4[s·s]/(2[s])"));
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tcombine repeated unit factors in interpreted display\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QString interpretedDisplayed =
            ResultLineFormatUtils::formattedExpressionLineForDisplay(
                QString::fromUtf8("4[s·s]/(2[s])"),
                eval->interpretedExpression());
        const QString expected = QStringLiteral("4")
            + QString(MathDsl::QuantSp)
            + QString::fromUtf8("[s²]")
            + divide
            + QStringLiteral("(2")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[s])");
        if (interpretedDisplayed != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tcombine repeated unit factors in interpreted display\t[NEW]" << endl
                 << "\tDisplayed: " << interpretedDisplayed.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }
    ++eval_total_tests;
    eval->setExpression(QString::fromUtf8("4[s·s/s]/(2[s])"));
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tcancel unit factors in interpreted display\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QString interpretedDisplayed =
            ResultLineFormatUtils::formattedExpressionLineForDisplay(
                QString::fromUtf8("4[s·s/s]/(2[s])"),
                eval->interpretedExpression());
        const QString expected = QStringLiteral("4")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[s]")
            + divide
            + QStringLiteral("(2")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[s])");
        if (interpretedDisplayed != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tcancel unit factors in interpreted display\t[NEW]" << endl
                 << "\tDisplayed: " << interpretedDisplayed.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("cos(pi)^2"),
        QStringLiteral("cos")
            + QString::fromUtf8("²")
            + QStringLiteral("(pi)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1/2^3"),
        QStringLiteral("1")
            + divide
            + QStringLiteral("2³"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12!"),
        QStringLiteral("2^(12!)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12.1!"),
        QStringLiteral("2^(12.1!)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1/2!"),
        QStringLiteral("1")
            + divide
            + QStringLiteral("(2!)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12-2"),
        QString::fromUtf8("2¹²")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12.-2"),
        QString::fromUtf8("2¹²")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12.000-2"),
        QString::fromUtf8("2¹²")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12.000-2+1/(1×2^3×3)-2^12!+2^12.1!"),
        QString::fromUtf8("2¹²")
            + minus
            + QStringLiteral("2")
            + plus
            + QStringLiteral("1")
            + divide
            + QStringLiteral("(1")
            + dotSpaced
            + QStringLiteral("(2³)")
            + dotSpaced
            + QStringLiteral("3)")
            + minus
            + QStringLiteral("2^(12!)")
            + plus
            + QStringLiteral("2^(12.1!)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("1/(1×2^3×3)"),
        QStringLiteral("1")
            + divide
            + QStringLiteral("(1")
            + dotSpaced
            + QStringLiteral("(2³)")
            + dotSpaced
            + QStringLiteral("3)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^123"),
        QString::fromUtf8("2¹²³"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("2^12.12"),
        QStringLiteral("2^(12.12)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2*e*e-1"),
        QStringLiteral("2")
            + dotSpaced
            + QString::fromUtf8("e²")
            + minus
            + QStringLiteral("1"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+2*2*2+pi*pi^2+average(2;3;4)"),
        QString::fromUtf8("2³")
            + plus
            + QString::fromUtf8("pi³")
            + plus
            + QStringLiteral("average(2;3;4)")
            + plus
            + QStringLiteral("1"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QString::fromUtf8("1+2*2*2+π*π^2+average(2;3;4)"),
        QString::fromUtf8("2³")
            + plus
            + QString::fromUtf8("π³")
            + plus
            + QStringLiteral("average(2;3;4)")
            + plus
            + QStringLiteral("1"));
    CHECK_EVAL("foo1=2", "2");
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("foo1 foo1 + foo1 foo1 foo1 foo1 foo1 × 2 × foo1^4"),
        QString::fromUtf8("foo1²")
            + plus
            + QString::fromUtf8("foo1⁹")
            + dotSpaced
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("sin(pi)*sin(pi)"),
        QString::fromUtf8("sin²(pi)"));
    CHECK_USERFUNC_SET("f(x)=x");
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("f(pi)*f(pi)"),
        QString::fromUtf8("f²(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QString::fromUtf8("f(pi)·f(pi)"),
        QString::fromUtf8("f²(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QString::fromUtf8("f(pi) · f(pi)"),
        QString::fromUtf8("f²(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("sin(pi)*2*sin(pi)^2"),
        QString::fromUtf8("sin³(pi)")
            + dotSpaced
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2*cos(pi)^2*cos(pi)^2/(3*cos(pi))"),
        QStringLiteral("2")
            + slash
            + QStringLiteral("3")
            + dotSpaced
            + QString::fromUtf8("cos³(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1*cos(pi)^2*cos(pi)^2/(2*cos(pi))"),
        QStringLiteral("1")
            + slash
            + QStringLiteral("2")
            + dotSpaced
            + QString::fromUtf8("cos³(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1*cos(pi)^2*cos(pi)^2/(13*cos(pi))"),
        QStringLiteral("1")
            + slash
            + QStringLiteral("13")
            + dotSpaced
            + QString::fromUtf8("cos³(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1024*cos(pi)^2*cos(pi)^2/(64*cos(pi))"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("16")
            + dotSpaced
            + QString::fromUtf8("cos³(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1024*cos(pi)^2*cos(pi)^2/(-64*cos(pi))"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("16")
            + dotSpaced
            + QString::fromUtf8("cos³(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1024*cos(pi)^2*cos(pi)^2/(-64*cos(pi))"),
        QStringLiteral("16")
            + dotSpaced
            + QString::fromUtf8("cos³(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("5*cos(pi)^4/(10*cos(pi))"),
        QStringLiteral("1")
            + slash
            + QStringLiteral("2")
            + dotSpaced
            + QString::fromUtf8("cos³(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("6*cos(pi)^4/(3*cos(pi))"),
        QStringLiteral("2")
            + dotSpaced
            + QString::fromUtf8("cos³(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-6*cos(pi)^4/(3*cos(pi))"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("2")
            + dotSpaced
            + QString::fromUtf8("cos³(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("6*cos(pi)^4/(-3*cos(pi))"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("2")
            + dotSpaced
            + QString::fromUtf8("cos³(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-6*cos(pi)^4/(-3*cos(pi))"),
        QStringLiteral("2")
            + dotSpaced
            + QString::fromUtf8("cos³(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("14*cos(pi)^7/(21*cos(pi)^2)"),
        QStringLiteral("2")
            + slash
            + QStringLiteral("3")
            + dotSpaced
            + QString::fromUtf8("cos⁵(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("14*cos(pi)^7/(7*cos(pi)^2)"),
        QStringLiteral("2")
            + dotSpaced
            + QString::fromUtf8("cos⁵(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("(2*cos(pi)/(3*pi*4))*(343+4343)"),
        QStringLiteral("781")
            + dotSpaced
            + QStringLiteral("cos(pi)")
            + slash
            + QStringLiteral("pi"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("(2*cos(pi)/(3*pi*4))*(343+4343)+2*e"),
        QStringLiteral("781")
            + dotSpaced
            + QStringLiteral("cos(pi)")
            + slash
            + QStringLiteral("pi")
            + plus
            + QStringLiteral("2")
            + dotSpaced
            + QStringLiteral("e"));
    CHECK_EVAL("(-1024*cos(pi)^2*cos(pi)^2/(64*cos(pi)))", "16");
    CHECK_EVAL("(1024*cos(pi)^2*cos(pi)^2/(-64*cos(pi)))", "16");
    CHECK_EVAL("(-1024*cos(pi)^2*cos(pi)^2/(-64*cos(pi)))", "-16");
    CHECK_EVAL("(1*cos(pi)^2*cos(pi)^2/(13*cos(pi)))", "-0.07692307692307692308");
    CHECK_EVAL("(1*cos(pi)^2*cos(pi)^2/(2*cos(pi)))", "-0.5");
    CHECK_EVAL("(2*cos(pi)^2*cos(pi)^2/(3*cos(pi)))", "-0.66666666666666666667");
    CHECK_AUTOFIX("2*cos(pi)^2*cos^2(pi)/(3*cos(pi))",
                  "2·cos(pi)^2·cos(pi)^2/(3·cos(pi))");
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+pi pi pi/2+3"),
        QStringLiteral("1")
            + slash
            + QStringLiteral("2")
            + dotSpaced
            + QString::fromUtf8("pi³")
            + plus
            + QStringLiteral("4"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+2*3/pi*pi*pi/2+3"),
        QStringLiteral("3")
            + dotSpaced
            + QStringLiteral("pi")
            + plus
            + QStringLiteral("4"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+2*3/pi*pi*pi*pi/2+3"),
        QStringLiteral("3")
            + dotSpaced
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("4"));
    CHECK_EVAL("abc()=2", "2");
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("abc()*abc()^3/abc()"),
        QString::fromUtf8("abc³()"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2*abc()^4/(8*abc())"),
        QStringLiteral("1")
            + slash
            + QStringLiteral("4")
            + dotSpaced
            + QString::fromUtf8("abc³()"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-2*abc()^4/(8*abc())"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("1")
            + slash
            + QStringLiteral("4")
            + dotSpaced
            + QString::fromUtf8("abc³()"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2*abc()^4/(-8*abc())"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("1")
            + slash
            + QStringLiteral("4")
            + dotSpaced
            + QString::fromUtf8("abc³()"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-2*abc()^4/(-8*abc())"),
        QStringLiteral("1")
            + slash
            + QStringLiteral("4")
            + dotSpaced
            + QString::fromUtf8("abc³()"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+(2*3/abc())*abc()*abc()/2+3*pi*pi*2+1"),
        QStringLiteral("3")
            + dotSpaced
            + QStringLiteral("abc()")
            + plus
            + QStringLiteral("6")
            + dotSpaced
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1+(2*3/abc())*4*abc()*abc()/2+3*pi*pi*2+1"),
        QStringLiteral("12")
            + dotSpaced
            + QStringLiteral("abc()")
            + plus
            + QStringLiteral("6")
            + dotSpaced
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2+3+1+2*3/e*4*e*e/2+3*pi*pi*2+1+cos(pi)^8/cos(pi)^2"),
        QStringLiteral("12")
            + dotSpaced
            + QStringLiteral("e")
            + plus
            + QStringLiteral("6")
            + dotSpaced
            + QString::fromUtf8("pi²")
            + plus
            + QString::fromUtf8("cos⁶(pi)")
            + plus
            + QStringLiteral("7"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2+3*pi*pi*pi/pi*pi*pi/pi+1+e^5/e^2-23-1/(cos(pi)*cos(pi))"),
        QStringLiteral("3")
            + dotSpaced
            + QString::fromUtf8("pi³")
            + plus
            + QString::fromUtf8("e³")
            + minus
            + QStringLiteral("1")
            + divide
            + QString::fromUtf8("cos²(pi)")
            + minus
            + QStringLiteral("20"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2+3*pi*pi*pi/pi*pi*pi/pi+1+e^5/e^2-23-1/(5.5+3*cos(pi)*cos(pi)+2)"),
        QStringLiteral("3")
            + dotSpaced
            + QString::fromUtf8("pi³")
            + plus
            + QString::fromUtf8("e³")
            + minus
            + QStringLiteral("1")
            + divide
            + QStringLiteral("(3")
            + dotSpaced
            + QString::fromUtf8("cos²(pi)")
            + plus
            + QStringLiteral("7.5)")
            + minus
            + QStringLiteral("20"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1-5.5*2+3*pi*pi*pi/pi/pi+1+e^5/e^2-23-1/(5.67+4*cos(pi)*cos(pi)+2-8/2+1)+9/3+2"),
        QStringLiteral("3")
            + dotSpaced
            + QStringLiteral("pi")
            + plus
            + QString::fromUtf8("e³")
            + minus
            + QStringLiteral("1")
            + divide
            + QStringLiteral("(4")
            + dotSpaced
            + QString::fromUtf8("cos²(pi)")
            + plus
            + QStringLiteral("4.67)")
            + minus
            + QStringLiteral("27"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("(12/68)[metre]"),
        QStringLiteral("(12")
            + slash
            + QStringLiteral("68)")
            + QStringLiteral("[m]"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("12/(68[metre])"),
        QStringLiteral("12")
            + divide
            + QStringLiteral("68")
            + QString(MathDsl::QuantSp)
            + QStringLiteral("[m]"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("12/68*1000"),
        QStringLiteral("176.470588235294"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2+cos(-pi^2)+pi+3+pi"),
        QStringLiteral("cos(")
            + QString(UnicodeChars::MinusSign)
            + QString::fromUtf8("pi²)")
            + plus
            + QStringLiteral("2")
            + dotSpaced
            + QStringLiteral("pi")
            + plus
            + QStringLiteral("5"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1+1e78+cos(pi)-1e78-1"),
        QStringLiteral("cos(pi)")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1-1e7+cos(pi)-1e7+1e7-1+1e7"),
        QStringLiteral("cos(pi)")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1-100+cos(pi)-100+100-1+100"),
        QStringLiteral("cos(pi)")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1-e+cos(pi)-e+e-1+e"),
        QStringLiteral("cos(pi)")
            + minus
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-2*3*2"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("2")
            + times
            + QStringLiteral("3")
            + times
            + QStringLiteral("2"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2*3*(-2)"),
        QStringLiteral("2")
            + times
            + QStringLiteral("3")
            + times
            + QStringLiteral("(")
            + QString(UnicodeChars::MinusSign)
            + QStringLiteral("2)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("−1/2/cos pi −1.345 sin(−pi^2+2.5/cos(e))  × 4 sin(−pi^2+2.5/cos(e))+ 4 tan(2) ×5 tan(2) ×6 tan 2 ×7 −1.23"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("1")
            + slash
            + QStringLiteral("2")
            + slash
            + QStringLiteral("cos(")
            + QStringLiteral("pi)")
            + minus
            + QStringLiteral("1.345")
            + dotSpaced
            + QStringLiteral("sin(")
            + QString(UnicodeChars::MinusSign)
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("2.5")
            + slash
            + QStringLiteral("cos(e))")
            + dotSpaced
            + QStringLiteral("4")
            + dotSpaced
            + QStringLiteral("sin(")
            + QString(UnicodeChars::MinusSign)
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("2.5")
            + slash
            + QStringLiteral("cos(e))")
            + plus
            + QStringLiteral("4")
            + dotSpaced
            + QStringLiteral("tan(2)")
            + dotSpaced
            + QStringLiteral("5")
            + dotSpaced
            + QStringLiteral("tan(2)")
            + dotSpaced
            + QStringLiteral("6")
            + dotSpaced
            + QStringLiteral("tan(2)")
            + dotSpaced
            + QStringLiteral("7")
            + minus
            + QStringLiteral("1.23"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("−1/2/cos pi −1.345 sin(−pi^2+2.5/cos(e))  × 4 sin(−pi^2+2.5/cos(e))+ 4 tan(2) ×5 tan(2) ×6 tan 2 ×7 −1.23"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("1")
            + slash
            + QStringLiteral("2")
            + slash
            + QStringLiteral("cos(")
            + QStringLiteral("pi)")
            + minus
            + QStringLiteral("5.38")
            + dotSpaced
            + QStringLiteral("sin²(")
            + QString(UnicodeChars::MinusSign)
            + QString::fromUtf8("pi²")
            + plus
            + QStringLiteral("2.5")
            + slash
            + QStringLiteral("cos(e))")
            + plus
            + QStringLiteral("840")
            + dotSpaced
            + QStringLiteral("tan³(2)")
            + minus
            + QStringLiteral("1.23"));
    ++eval_total_tests;
    eval->setExpression(QStringLiteral("-1+1e78+cos(pi)-1e78-1"));
    Quantity largeCancellationBase = eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tlarge cancellation base eval\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        const QString simplifiedLargeCancellationExpr =
            Evaluator::simplifyInterpretedExpression(eval->interpretedExpression());
        eval->setExpression(simplifiedLargeCancellationExpr);
        Quantity largeCancellationSimplified = eval->evalNoAssign();
        const QString simplifiedValue = DMath::format(largeCancellationSimplified, Format::Fixed());
        if (!eval->error().isEmpty() || simplifiedValue != QLatin1String("-3")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tlarge cancellation simplified eval\t[NEW]" << endl
                 << "\tSimplified expression: " << simplifiedLargeCancellationExpr.toUtf8().constData() << endl
                 << "\tResult   : " << simplifiedValue.toUtf8().constData() << endl
                 << "\tExpected : -3" << endl;
        }
    }
    CHECK_DISPLAY_INTERPRETED(
        QString::fromUtf8("12×1/2/3/4×23+23−sin(2)+23 cos(23)"),
        QStringLiteral("12")
            + times
            + QStringLiteral("1")
            + divide
            + QStringLiteral("2")
            + divide
            + QStringLiteral("3")
            + divide
            + QStringLiteral("4")
            + times
            + QStringLiteral("23")
            + plus
            + QStringLiteral("23")
            + minus
            + QStringLiteral("sin(2)")
            + plus
            + QStringLiteral("23")
            + dotSpaced
            + QStringLiteral("cos(23)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("x=1/2 sqrt(3)"),
        QStringLiteral("x")
            + equals
            + QStringLiteral("(1")
            + divide
            + QStringLiteral("2)")
            + dotSpaced
            + QStringLiteral("√(3)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("gf(t)=t/2 sqrt(3)"),
        QStringLiteral("gf(t)")
            + equals
            + QStringLiteral("(t")
            + divide
            + QStringLiteral("2)")
            + dotSpaced
            + QStringLiteral("√(3)"));
}

void test_format()
{
    Settings* settings = Settings::instance();
    const char savedResultFormat = settings->resultFormat;

    CHECK_EVAL("bin(123)", "0b1111011");
    CHECK_EVAL("oct(123)", "0o173");
    CHECK_EVAL("hex(123)", "0x7B");
    CHECK_EVAL("sci(123)", "1.23e2");
    CHECK_EVAL("eng(123)", "123e0");
    CHECK_EVAL("sci(12341)", "1.2341e4");
    CHECK_EVAL("eng(12341)", "12.341e3");
    CHECK_EVAL("1+sci(123456.789)", "123457.789");
    CHECK_EVAL("sci(123456.789)+1", "123457.789");
    CHECK_EVAL("1+eng(123456.789)", "123457.789");
    CHECK_EVAL("eng(123456.789)+1", "123457.789");
    CHECK_EVAL("eng(0.000123456)", "123.456e-6");
    CHECK_EVAL("eng(0.000123456; -3)", "0.123456e-3");
    CHECK_EVAL("eng(0.000123456; -6)", "123.456e-6");
    CHECK_EVAL("eng(0.000123456; 0)", "0.000123456");
    CHECK_EVAL("eng(0.000123456; 3)", "0.000000123456e3");
    CHECK_EVAL("eng(0.000123456; 6)", "0.000000000123456e6");
    CHECK_EVAL("eng(0.000123456; -3)", "0.123456e-3");
    CHECK_EVAL("eng(0.000123456; -12)", "123456000e-12");
    CHECK_EVAL_FORMAT_EXACT("rat(0.5)", QStringLiteral("1") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_EXACT("ratio(0.5)", QStringLiteral("1") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_EXACT("rational(0.5)", QStringLiteral("1") + slash + QStringLiteral("2"));
    CHECK_EVAL_FORMAT_NO_SLASH("rat(exp(1))");

    CHECK_EVAL("binpad(15)", "0b00001111");
    CHECK_EVAL("hexpad(15)", "0x0F");
    CHECK_EVAL("octpad(15)", "0o017");

    CHECK_EVAL("binpad(1536)", "0b0000011000000000");
    CHECK_EVAL("hexpad(1536)", "0x0600");
    CHECK_EVAL("octpad(1536)", "0o003000");

    CHECK_EVAL("binpad(1536; 32)", "0b00000000000000000000011000000000");
    CHECK_EVAL("hexpad(1536; 32)", "0x00000600");
    CHECK_EVAL("octpad(1536; 32)", "0o00000003000");

    CHECK_EVAL("binpad(-5)", "-0b00000101");
    CHECK_EVAL("hexpad(-255; 16)", "-0x00FF");
    CHECK_EVAL("octpad(-9)", "-0o011");

    // Explicit widths below current representation should not truncate.
    CHECK_EVAL("binpad(255; 7)", "0b11111111");
    CHECK_EVAL("hexpad(255; 4)", "0xFF");
    CHECK_EVAL("octpad(511; 8)", "0o777");

    // Width rounding per radix digit size.
    CHECK_EVAL("binpad(5; 9)", "0b000000101");
    CHECK_EVAL("hexpad(5; 9)", "0x005");
    CHECK_EVAL("octpad(5; 9)", "0o005");
    CHECK_EVAL("octpad(9; 10)", "0o0011");

    // Zero and byte boundary transitions.
    CHECK_EVAL("binpad(0)", "0");
    CHECK_EVAL("hexpad(0)", "0");
    CHECK_EVAL("octpad(0)", "0");
    CHECK_EVAL("binpad(256)", "0b0000000100000000");
    CHECK_EVAL("hexpad(256)", "0x0100");
    CHECK_EVAL("octpad(256)", "0o000400");

    settings->resultFormat = 's';
    CHECK_EVAL("bin(23.45)", "0b10111.01110011001100110011");
    CHECK_EVAL("hex(23.45)", "0x17.73333333333333333333");
    CHECK_EVAL("oct(23.45)", "0o27.34631463146314631463");
    CHECK_EVAL("sci(23.45)", "2.345e1");
    CHECK_EVAL("eng(23.45)", "23.45e0");
    CHECK_EVAL("binpad(23)", "0b00010111");
    CHECK_EVAL("hexpad(23)", "0x17");
    CHECK_EVAL("octpad(23)", "0o027");

    settings->resultFormat = 'r';
    CHECK_EVAL("sci(23.45)", "2.345e1");
    CHECK_EVAL("eng(23.45)", "23.45e0");
    CHECK_EVAL("bin(23.45)", "0b10111.01110011001100110011");
    CHECK_EVAL("hex(23.45)", "0x17.73333333333333333333");
    CHECK_EVAL("oct(23.45)", "0o27.34631463146314631463");
    CHECK_EVAL("binpad(23)", "0b00010111");
    CHECK_EVAL("hexpad(23)", "0x17");
    CHECK_EVAL("octpad(23)", "0o027");
    settings->resultFormat = savedResultFormat;

    CHECK_EVAL_FAIL("binpad(1.5)");
    CHECK_EVAL_FAIL("binpad(1[metre])");
    CHECK_EVAL_FAIL("hexpad(1+j)");
    CHECK_EVAL_FAIL("hexpad(10; 0)");
    CHECK_EVAL_FAIL("octpad(10; -8)");
    CHECK_EVAL_FAIL("binpad(10; 3.5)");
    CHECK_EVAL_FAIL("binpad(10; 1e1000)");
    CHECK_EVAL_FAIL("eng(1; [metre])");
    CHECK_EVAL_FAIL("eng(1; 3.14)");
    CHECK_EVAL_FAIL("eng(0.000123456; -1)");
    CHECK_EVAL_FAIL("eng(0.000123456; -2)");
    CHECK_EVAL_FAIL("eng(0.000123456; -4)");
    CHECK_EVAL_FAIL("eng(0.000123456; 2)");

    CHECK_EVAL("polar(3+4j)", "5 · exp(i · 0.92729521800161223243)");

    const char savedComplexForm = settings->resultFormatComplex;
    const char savedAngleUnit = settings->angleUnit;

    settings->resultFormatComplex = 'a';
    settings->angleUnit = 'r';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL_FORMAT_EXACT("1+1j", QString::fromUtf8("1.4142135623730950488 ∠ 0.78539816339744830962"));

    settings->angleUnit = 'd';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL_FORMAT_EXACT("1+1j", QString::fromUtf8("1.4142135623730950488 ∠ 45"));

    settings->angleUnit = 'g';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL_FORMAT_EXACT("1+1j", QString::fromUtf8("1.4142135623730950488 ∠ 50"));

    settings->angleUnit = 't';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL_FORMAT_EXACT("1+1j", QString::fromUtf8("1.4142135623730950488 ∠ 0.125"));
    settings->angleUnit = 'v';
    Evaluator::instance()->initializeAngleUnits();
    CHECK_EVAL_FORMAT_EXACT("1+1j", QString::fromUtf8("1.4142135623730950488 ∠ 0.125"));

    settings->resultFormatComplex = savedComplexForm;
    settings->angleUnit = savedAngleUnit;
    Evaluator::instance()->initializeAngleUnits();
}

void test_result_rounding_mode_formatting()
{
    Settings* settings = Settings::instance();
    const char savedMode = settings->resultRoundingMode;

    auto checkFixed0 = [&](const char* label, const char* value, const char* expected) {
        ++eval_total_tests;
        QString formatted = NumberFormatter::format(
            Quantity(HNumber(value)), 'f', 0, false, 'c');
        formatted.replace(QString::fromUtf8("−"), "-");
        if (formatted != QString::fromUtf8(expected)) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tResult   : " << formatted.toUtf8().constData() << endl
                 << "\tExpected : " << expected << endl;
        }
    };

    settings->resultRoundingMode = Settings::ResultRoundingHalfAwayFromZero;
    setRuntimeResultRoundingMode(settings->resultRoundingMode);
    checkFixed0("half-away +2.5", "2.5", "3");
    checkFixed0("half-away -2.5", "-2.5", "-3");
    CHECK_EVAL("round(2.5)", "3");
    CHECK_EVAL("round(2.5[m])", "3 metre");

    settings->resultRoundingMode = Settings::ResultRoundingHalfEven;
    setRuntimeResultRoundingMode(settings->resultRoundingMode);
    checkFixed0("half-even +2.5", "2.5", "2");
    checkFixed0("half-even +3.5", "3.5", "4");
    checkFixed0("half-even -2.5", "-2.5", "-2");
    CHECK_EVAL("round(2.5)", "2");
    CHECK_EVAL("round(2.5[m])", "2 metre");

    settings->resultRoundingMode = Settings::ResultRoundingTowardZero;
    setRuntimeResultRoundingMode(settings->resultRoundingMode);
    checkFixed0("toward-zero +2.9", "2.9", "2");
    checkFixed0("toward-zero -2.9", "-2.9", "-2");
    checkFixed0("toward-zero +2.5", "2.5", "2");

    settings->resultRoundingMode = Settings::ResultRoundingTowardPositiveInfinity;
    setRuntimeResultRoundingMode(settings->resultRoundingMode);
    checkFixed0("toward-plus-inf +2.1", "2.1", "3");
    checkFixed0("toward-plus-inf -2.1", "-2.1", "-2");

    settings->resultRoundingMode = Settings::ResultRoundingTowardNegativeInfinity;
    setRuntimeResultRoundingMode(settings->resultRoundingMode);
    checkFixed0("toward-minus-inf +2.9", "2.9", "2");
    checkFixed0("toward-minus-inf -2.1", "-2.1", "-3");

    settings->resultRoundingMode = savedMode;
    setRuntimeResultRoundingMode(savedMode);
}


void test_datetime()
{
    // NOTE We cannot test datetime() with only 1 argument as it depends on current timezone (and DST?)

    CHECK_EVAL("datetime(1538242871;    0)", "20180929.174111");
    CHECK_EVAL("datetime(1514761200;    1)", "20180101.000000");
    CHECK_EVAL("datetime(1514764800;   -1)", "20171231.230000");
    CHECK_EVAL("datetime(1551464695;13.75)", "20190302.080955");  // GMT +13:45
    CHECK_EVAL("datetime(1551464695; -3.5)", "20190301.145455");  // GMT -03:30
}

void test_epoch()
{
    // NOTE We avoid testing epoch() with only 1 argument because it depends on local timezone (and DST).
    CHECK_EVAL("epoch(20180929.174111; 0)", "1538242871");
    CHECK_EVAL("epoch(20180101.000000; 1)", "1514761200");
    CHECK_EVAL("epoch(20171231.230000; -1)", "1514764800");
    CHECK_EVAL("epoch(20190302.080955; 13.75)", "1551464695");
    CHECK_EVAL("epoch(20190301.145455; -3.5)", "1551464695");
}

void test_expression_operator_normalization()
{
    const QString normalized = EditorUtils::normalizeExpressionOperators(
        QString::fromUtf8("1＋2 8/4÷2 "
                          "9-4 9－4 9﹣4 9‐4 9‑4 9–4 9—4 9―4 9⁃4 "
                          "2∗3·4·5∙6*7⨉8⨯9✕10✖11"));
    const std::string normalizedStd = normalized.toStdString();
    ++eval_total_tests;
    DisplayErrorOnMismatch(__FILE__, __LINE__, "normalizeExpressionOperators",
                           normalizedStd,
                           "1+2 8⧸4⧸2 "
                           "9−4 9−4 9−4 9−4 9−4 9−4 9−4 9−4 9−4 "
                           "2×3×4×5×6×7×8×9×10×11",
                           eval_failed_tests, eval_new_failed_tests);

    const QStringList parsed = EditorUtils::parsePastedExpressions(
        QString::fromUtf8("1＋2\n"
                          "8/4\n"
                          "10÷2\n"
                          "9-4\n"
                          "9－4\n"
                          "9﹣4\n"
                          "9‐4\n"
                          "9‑4\n"
                          "9–4\n"
                          "9—4\n"
                          "9―4\n"
                          "9⁃4\n"
                          "2∗3\n"
                          "4·5\n\n"
                          "6✖7"));
    if (parsed.size() != 15
        || parsed.at(0) != QString::fromUtf8("1+2")
        || parsed.at(1) != QString::fromUtf8("8⧸4")
        || parsed.at(2) != QString::fromUtf8("10⧸2")
        || parsed.at(3) != QString::fromUtf8("9−4")
        || parsed.at(4) != QString::fromUtf8("9−4")
        || parsed.at(5) != QString::fromUtf8("9−4")
        || parsed.at(6) != QString::fromUtf8("9−4")
        || parsed.at(7) != QString::fromUtf8("9−4")
        || parsed.at(8) != QString::fromUtf8("9−4")
        || parsed.at(9) != QString::fromUtf8("9−4")
        || parsed.at(10) != QString::fromUtf8("9−4")
        || parsed.at(11) != QString::fromUtf8("9−4")
        || parsed.at(12) != QString::fromUtf8("2×3")
        || parsed.at(13) != QString::fromUtf8("4×5")
        || parsed.at(14) != QString::fromUtf8("6×7")) {
        ++eval_total_tests;
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tparsePastedExpressions\t[NEW]" << endl
             << "\tResult   : " << parsed.join("|").toUtf8().constData() << endl
             << "\tExpected : 1+2|8⧸4|10⧸2|"
                "9−4|9−4|9−4|9−4|9−4|9−4|9−4|9−4|9−4|"
                "2×3|4×5|6×7" << endl;
    } else {
        ++eval_total_tests;
    }

    const QString editorNormalized = EditorUtils::normalizeExpressionOperatorsForEditorInput(
        QString::fromUtf8("2·3 4·5 6*7 8/4 10÷2 9⧸3 𝜋 𝝅 𝞹 𝛑 Ω μ sqrt cbrt asqrt cbrtfoo"));
    const std::string editorNormalizedStd = editorNormalized.toStdString();
    ++eval_total_tests;
    DisplayErrorOnMismatch(__FILE__, __LINE__, "normalizeExpressionOperatorsForEditorInput",
                           editorNormalizedStd,
                           "2·3 4·5 6×7 8/4 10/2 9/3 π π π π Ω µ √ ∛ asqrt cbrtfoo",
                           eval_failed_tests, eval_new_failed_tests);

    const QString implicitMulWithLatinLetter =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2"), 1, QStringLiteral("a"));
    {
        const QString expected = QStringLiteral("a");
        ++eval_total_tests;
        if (implicitMulWithLatinLetter != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped implicit multiplication after digit (latin)\t[NEW]" << endl
                 << "\tResult   : " << implicitMulWithLatinLetter.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString implicitMulWithNonLatinLetter =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QString::fromUtf8("2"), 1, QString::fromUtf8("β"));
    {
        const QString expected = QString::fromUtf8("β");
        ++eval_total_tests;
        if (implicitMulWithNonLatinLetter != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped implicit multiplication after digit (non-latin)\t[NEW]" << endl
                 << "\tResult   : " << implicitMulWithNonLatinLetter.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString implicitMulWithSuperscriptDigit =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QString::fromUtf8("2³"), 2, QStringLiteral("x"));
    {
        const QString expected = QStringLiteral("x");
        ++eval_total_tests;
        if (implicitMulWithSuperscriptDigit != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped implicit multiplication after superscript digit\t[NEW]" << endl
                 << "\tResult   : " << implicitMulWithSuperscriptDigit.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString implicitMulWithOpeningParAfterDigit =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2"), 1, QStringLiteral("("));
    {
        const QString expected = QString(MathDsl::MulCrossWrapSp)
            + QString(MathDsl::MulCrossOp)
            + QString(MathDsl::MulCrossWrapSp)
            + QStringLiteral("(");
        ++eval_total_tests;
        if (implicitMulWithOpeningParAfterDigit != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped implicit multiplication before opening par after digit\t[NEW]" << endl
                 << "\tResult   : " << implicitMulWithOpeningParAfterDigit.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString implicitMulWithOpeningBracketAfterLetterAndSpaces =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QString::fromUtf8("β   "), 4, QStringLiteral("["));
    {
        const QString expected = QString(MathDsl::MulDotWrapSp)
            + QString(MathDsl::MulDotOp)
            + QString(MathDsl::MulDotWrapSp)
            + QStringLiteral("[");
        ++eval_total_tests;
        if (implicitMulWithOpeningBracketAfterLetterAndSpaces != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped implicit multiplication before opening bracket after non-latin letter and spaces\t[NEW]" << endl
                 << "\tResult   : " << implicitMulWithOpeningBracketAfterLetterAndSpaces.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString implicitMulWithOpeningParAfterSuperscriptAndSpaces =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QString::fromUtf8("2³   "), 5, QStringLiteral("("));
    {
        const QString expected = QString(MathDsl::MulCrossWrapSp)
            + QString(MathDsl::MulCrossOp)
            + QString(MathDsl::MulCrossWrapSp)
            + QStringLiteral("(");
        ++eval_total_tests;
        if (implicitMulWithOpeningParAfterSuperscriptAndSpaces != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped implicit multiplication before opening par after superscript and spaces\t[NEW]" << endl
                 << "\tResult   : " << implicitMulWithOpeningParAfterSuperscriptAndSpaces.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString implicitMulWithOpeningParAfterVariableSuperscript =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QString::fromUtf8("pi³"), 3, QStringLiteral("("));
    {
        const QString expected = QString(MathDsl::MulDotWrapSp)
            + QString(MathDsl::MulDotOp)
            + QString(MathDsl::MulDotWrapSp)
            + QStringLiteral("(");
        ++eval_total_tests;
        if (implicitMulWithOpeningParAfterVariableSuperscript != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped implicit multiplication before opening par after variable superscript\t[NEW]" << endl
                 << "\tResult   : " << implicitMulWithOpeningParAfterVariableSuperscript.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString noImplicitMulWithOpeningParAfterOperator =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2+"), 2, QStringLiteral("("));
    {
        const QString expected = QStringLiteral("(");
        ++eval_total_tests;
        if (noImplicitMulWithOpeningParAfterOperator != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tno implicit multiplication before opening par after operator\t[NEW]" << endl
                 << "\tResult   : " << noImplicitMulWithOpeningParAfterOperator.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString noImplicitMulWithLetterAfterLetter =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("ab"), 2, QStringLiteral("c"));
    {
        const QString expected = QStringLiteral("c");
        ++eval_total_tests;
        if (noImplicitMulWithLetterAfterLetter != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tno implicit multiplication with letter typed after letter\t[NEW]" << endl
                 << "\tResult   : " << noImplicitMulWithLetterAfterLetter.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString noImplicitMulWithDigitAfterLetter =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("a"), 1, QStringLiteral("2"));
    {
        const QString expected = QStringLiteral("2");
        ++eval_total_tests;
        if (noImplicitMulWithDigitAfterLetter != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tno implicit multiplication with digit typed after letter\t[NEW]" << endl
                 << "\tResult   : " << noImplicitMulWithDigitAfterLetter.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString typedAdditionAfterDigit =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2"), 1, QStringLiteral("+"));
    {
        const QString expected = QString(MathDsl::AddWrap)
            + QString(MathDsl::AddOp)
            + QString(MathDsl::AddWrap);
        ++eval_total_tests;
        if (typedAdditionAfterDigit != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped addition after digit uses configured spacing\t[NEW]" << endl
                 << "\tResult   : " << typedAdditionAfterDigit.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString typedSubtractionAfterSuperscriptDigit =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QString::fromUtf8("2³"), 2, QString::fromUtf8("−"));
    {
        const QString expected = QString(MathDsl::SubWrapSp)
            + QString(MathDsl::SubOp)
            + QString(MathDsl::SubWrapSp);
        ++eval_total_tests;
        if (typedSubtractionAfterSuperscriptDigit != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped subtraction after superscript uses configured spacing\t[NEW]" << endl
                 << "\tResult   : " << typedSubtractionAfterSuperscriptDigit.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString typedDivisionAfterDigit =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2"), 1, QStringLiteral("/"));
    {
        const QString expected = QString(MathDsl::DivWrap)
            + QString(MathDsl::DivOp)
            + QString(MathDsl::DivWrap);
        ++eval_total_tests;
        if (typedDivisionAfterDigit != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped division after digit uses configured spacing\t[NEW]" << endl
                 << "\tResult   : " << typedDivisionAfterDigit.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString typedMultiplicationAfterDigit =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2"), 1, QString::fromUtf8("×"));
    {
        const QString expected = QString(MathDsl::MulCrossWrapSp)
            + QString(MathDsl::MulCrossOp)
            + QString(MathDsl::MulCrossWrapSp);
        ++eval_total_tests;
        if (typedMultiplicationAfterDigit != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped multiplication after digit uses configured spacing\t[NEW]" << endl
                 << "\tResult   : " << typedMultiplicationAfterDigit.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString typedAdditionAfterNonLatinLetterAndSpaces =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QString::fromUtf8("β   "), 4, QStringLiteral("+"));
    {
        const QString expected = QString(MathDsl::AddOp)
            + QString(MathDsl::AddWrap);
        ++eval_total_tests;
        if (typedAdditionAfterNonLatinLetterAndSpaces != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped addition after non-latin letter and spaces uses configured spacing\t[NEW]" << endl
                 << "\tResult   : " << typedAdditionAfterNonLatinLetterAndSpaces.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString typedDivisionAfterSuperscriptAndSpaces =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QString::fromUtf8("2³   "), 5, QStringLiteral("/"));
    {
        const QString expected = QString(MathDsl::DivOp)
            + QString(MathDsl::DivWrap);
        ++eval_total_tests;
        if (typedDivisionAfterSuperscriptAndSpaces != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped division after superscript and spaces uses configured spacing\t[NEW]" << endl
                 << "\tResult   : " << typedDivisionAfterSuperscriptAndSpaces.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString typedSubtractionAfterClosingParenAndSpaces =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("(2)   "), 5, QString::fromUtf8("−"));
    {
        const QString expected = QString(MathDsl::SubOp)
            + QString(MathDsl::SubWrapSp);
        ++eval_total_tests;
        if (typedSubtractionAfterClosingParenAndSpaces != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped subtraction after closing paren and spaces uses configured spacing\t[NEW]" << endl
                 << "\tResult   : " << typedSubtractionAfterClosingParenAndSpaces.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString typedAdditionAfterClosingBracket =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("[2]"), 3, QStringLiteral("+"));
    {
        const QString expected = QString(MathDsl::AddWrap)
            + QString(MathDsl::AddOp)
            + QString(MathDsl::AddWrap);
        ++eval_total_tests;
        if (typedAdditionAfterClosingBracket != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped addition after closing bracket uses configured spacing\t[NEW]" << endl
                 << "\tResult   : " << typedAdditionAfterClosingBracket.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString typedAdditionAfterLetter =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("a"), 1, QStringLiteral("+"));
    {
        const QString expected = QString(MathDsl::AddWrap)
            + QString(MathDsl::AddOp)
            + QString(MathDsl::AddWrap);
        ++eval_total_tests;
        if (typedAdditionAfterLetter != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped addition after letter uses configured spacing\t[NEW]" << endl
                 << "\tResult   : " << typedAdditionAfterLetter.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString typedAdditionAfterTrailingSpace =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2 "), 2, QStringLiteral("+"));
    {
        const QString expected = QString(MathDsl::AddOp)
            + QString(MathDsl::AddWrap);
        ++eval_total_tests;
        if (typedAdditionAfterTrailingSpace != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped addition after trailing space avoids duplicate left space\t[NEW]" << endl
                 << "\tResult   : " << typedAdditionAfterTrailingSpace.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString scientificNotationWithLowerE =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2"), 1, QStringLiteral("e"));
    {
        const QString expected = QStringLiteral("e");
        ++eval_total_tests;
        if (scientificNotationWithLowerE != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped scientific notation lower e exception\t[NEW]" << endl
                 << "\tResult   : " << scientificNotationWithLowerE.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString scientificNotationWithUpperE =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2"), 1, QStringLiteral("E"));
    {
        const QString expected = QStringLiteral("E");
        ++eval_total_tests;
        if (scientificNotationWithUpperE != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped scientific notation upper E exception\t[NEW]" << endl
                 << "\tResult   : " << scientificNotationWithUpperE.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString inOperatorPrefixWithLowerI =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2"), 1, QStringLiteral("i"));
    {
        const QString expected = QStringLiteral(" i");
        ++eval_total_tests;
        if (inOperatorPrefixWithLowerI != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped in-operator prefix lower i exception\t[NEW]" << endl
                 << "\tResult   : " << inOperatorPrefixWithLowerI.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString inOperatorPrefixWithUpperI =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2"), 1, QStringLiteral("I"));
    {
        const QString expected = QStringLiteral(" I");
        ++eval_total_tests;
        if (inOperatorPrefixWithUpperI != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped in-operator prefix upper I exception\t[NEW]" << endl
                 << "\tResult   : " << inOperatorPrefixWithUpperI.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString inOperatorPrefixWithExistingSpace =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("2 "), 2, QStringLiteral("i"));
    {
        const QString expected = QStringLiteral("i");
        ++eval_total_tests;
        if (inOperatorPrefixWithExistingSpace != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped in-operator prefix keeps existing spacing\t[NEW]" << endl
                 << "\tResult   : " << inOperatorPrefixWithExistingSpace.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    const QString plainIdentifierContinuationWithI =
        EditorUtils::adjustedTypedTextForImplicitMultiplicationAfterDigit(
            QStringLiteral("p"), 1, QStringLiteral("i"));
    {
        const QString expected = QStringLiteral("i");
        ++eval_total_tests;
        if (plainIdentifierContinuationWithI != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\ttyped i after identifier keeps plain identifier continuation\t[NEW]" << endl
                 << "\tResult   : " << plainIdentifierContinuationWithI.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    }

    ++eval_total_tests;
    if (!EditorUtils::shouldIgnoreTypedSpaceAfterDigit(QStringLiteral("2"), 1)
        || !EditorUtils::shouldIgnoreTypedSpaceAfterDigit(QStringLiteral("pi"), 2)
        || !EditorUtils::shouldIgnoreTypedSpaceAfterDigit(QStringLiteral("cos(3)"), 6)
        || !EditorUtils::shouldIgnoreTypedSpaceAfterDigit(QString::fromUtf8("2³"), 2)
        || EditorUtils::shouldIgnoreTypedSpaceAfterDigit(QStringLiteral("2+"), 2)) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tignore typed space after digit/superscript\t[NEW]" << endl
             << "\tResult   : mismatch" << endl
             << "\tExpected : true,true,true,true,false" << endl;
    }

    const QStringList parsedForEditor = EditorUtils::parsePastedExpressionsForEditorInput(
        QString::fromUtf8("2·3\n"
                          "4·5\n"
                          "6*7\n"
                          "8/4\n"
                          "10÷2\n"
                          "9⧸3\n"
                          "𝜋+1\n"
                          "𝝅+1\n"
                          "𝞹+1\n"
                          "𝛑+1\n"
                          "sqrt(4)\n"
                          "cbrt(8)\n"
                          "asqrt(4)"));
    if (parsedForEditor.size() != 13
        || parsedForEditor.at(0) != QString::fromUtf8("2·3")
        || parsedForEditor.at(1) != QString::fromUtf8("4·5")
        || parsedForEditor.at(2) != QString::fromUtf8("6×7")
        || parsedForEditor.at(3) != QString::fromUtf8("8/4")
        || parsedForEditor.at(4) != QString::fromUtf8("10/2")
        || parsedForEditor.at(5) != QString::fromUtf8("9/3")
        || parsedForEditor.at(6) != QString::fromUtf8("π+1")
        || parsedForEditor.at(7) != QString::fromUtf8("π+1")
        || parsedForEditor.at(8) != QString::fromUtf8("π+1")
        || parsedForEditor.at(9) != QString::fromUtf8("π+1")
        || parsedForEditor.at(10) != QString::fromUtf8("√(4)")
        || parsedForEditor.at(11) != QString::fromUtf8("∛(8)")
        || parsedForEditor.at(12) != QString::fromUtf8("asqrt(4)")) {
        ++eval_total_tests;
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tparsePastedExpressionsForEditorInput\t[NEW]" << endl
             << "\tResult   : " << parsedForEditor.join("|").toUtf8().constData() << endl
             << "\tExpected : 2·3|4·5|6×7|8/4|10/2|9/3|π+1|π+1|π+1|π+1|√(4)|∛(8)|asqrt(4)" << endl;
    } else {
        ++eval_total_tests;
    }

    const QString piDisplay = UnicodeChars::normalizePiForDisplay(
        QString::fromUtf8("pi 𝜋 𝝅 𝞹 𝛑 alpha_pi beta pi2 pi² -pi +pi/pi"));
    ++eval_total_tests;
    DisplayErrorOnMismatch(__FILE__, __LINE__, "normalizePiForDisplay",
                           piDisplay.toStdString(),
                           "π π π π π alpha_pi beta pi2 π² -π +π/π",
                           eval_failed_tests, eval_new_failed_tests);

    ++eval_total_tests;
    eval->setExpression(QString::fromUtf8("2pi pi^2"));
    eval->evalUpdateAns();
    const QString threadExpressionDisplay = UnicodeChars::normalizePiForDisplay(
        Evaluator::formatInterpretedExpressionForDisplay(eval->interpretedExpression()));
    DisplayErrorOnMismatch(__FILE__, __LINE__, "normalizePiForDisplay thread expression",
                           threadExpressionDisplay.toStdString(),
                           "2 · π · π²",
                           eval_failed_tests, eval_new_failed_tests);

    ++eval_total_tests;
    QString ignoredTrailing;
    const bool ignoreTrailingPlus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1+2+"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingPlus || ignoredTrailing != QString::fromUtf8("1+2")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken trailing plus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingPlus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1+2" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingMultiPlus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1++++"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingMultiPlus || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken multi plus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingMultiPlus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingMultiMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("2-------"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingMultiMinus || ignoredTrailing != QString::fromUtf8("2")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken multi minus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingMultiMinus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 2" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingSingleMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1-"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingSingleMinus || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken single minus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingSingleMinus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingDoubleMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1--"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingDoubleMinus || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken double minus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingDoubleMinus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingTripleMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1---"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingTripleMinus || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken triple minus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingTripleMinus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingUnicodeMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(
            QString::fromUtf8("1−−−"),
            &ignoredTrailing);
    if (!ignoreTrailingUnicodeMinus || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken unicode minus\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingUnicodeMinus ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingMixedPlusMinusA =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1++--++-"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingMixedPlusMinusA || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken mixed +/- A\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingMixedPlusMinusA ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingMixedPlusMinusB =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1--+++-"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingMixedPlusMinusB || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken mixed +/- B\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingMixedPlusMinusB ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingSemicolon =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("average(2;3;4;"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingSemicolon || ignoredTrailing != QString::fromUtf8("average(2;3;4")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken trailing semicolon\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingSemicolon ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : average(2;3;4" << endl;
    }

    ignoredTrailing.clear();
    const bool ignoreTrailingSingleStar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1*"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingSingleStar || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken single star\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingSingleStar ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingDoubleStar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1**"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingDoubleStar || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken double star\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingDoubleStar ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingCaret =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1^"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingCaret || ignoredTrailing != QString::fromUtf8("1")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken caret\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingCaret ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 1" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingBackslash =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("3\\"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingBackslash || ignoredTrailing != QString::fromUtf8("3")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken trailing backslash\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingBackslash ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 3" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingSinglePar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("4("),
                                                             &ignoredTrailing);
    if (!ignoreTrailingSinglePar || ignoredTrailing != QString::fromUtf8("4")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken single parenthesis\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingSinglePar ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 4" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingMultiPar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("5(((("),
                                                             &ignoredTrailing);
    if (!ignoreTrailingMultiPar || ignoredTrailing != QString::fromUtf8("5")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken multi parenthesis\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingMultiPar ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 5" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingDiv =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("3/"),
                                                             &ignoredTrailing);
    if (!ignoreTrailingDiv || ignoredTrailing != QString::fromUtf8("3")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken trailing slash\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingDiv ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 3" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool ignoreTrailingPar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("5+cos pi ("),
                                                             &ignoredTrailing);
    if (!ignoreTrailingPar || ignoredTrailing != QString::fromUtf8("5+cos pi")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken trailing open parenthesis\t[NEW]" << endl
             << "\tResult   : " << (ignoreTrailingPar ? ignoredTrailing : QString("<none>")).toUtf8().constData()
             << endl
             << "\tExpected : 5+cos pi" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectTrailingStar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1+2+*"),
                                                             &ignoredTrailing);
    if (rejectTrailingStar) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject +*\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectTrailingDoubleDiv =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("3//"),
                                                             &ignoredTrailing);
    if (rejectTrailingDoubleDiv) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject //\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectInvalidParen =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("5+cos pi ()"),
                                                             &ignoredTrailing);
    if (rejectInvalidParen) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject ()\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectTrailingTripleStar =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1***"),
                                                             &ignoredTrailing);
    if (rejectTrailingTripleStar) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject ***\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectClosingParen =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1)"),
                                                             &ignoredTrailing);
    if (rejectClosingParen) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject )\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectClosingParens =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("1)))"),
                                                             &ignoredTrailing);
    if (rejectClosingParens) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject )))\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectOnlyPlus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("+"),
                                                             &ignoredTrailing);
    if (rejectOnlyPlus) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject +\t[NEW]" << endl;
    }

    ++eval_total_tests;
    ignoredTrailing.clear();
    const bool rejectOnlyMixedPlusMinus =
        EditorUtils::expressionWithoutIgnorableTrailingToken(QString::fromUtf8("-+-+-++--+"),
                                                             &ignoredTrailing);
    if (rejectOnlyMixedPlusMinus) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\texpressionWithoutIgnorableTrailingToken reject mixed +/- only\t[NEW]" << endl;
    }
}

void test_session_history_limit()
{
    Settings* settings = Settings::instance();
    const int oldMaxHistoryEntries = settings->maxHistoryEntries;

    settings->maxHistoryEntries = 3;
    Session session;
    session.addHistoryEntry(HistoryEntry("1+1", Quantity(2)));
    session.addHistoryEntry(HistoryEntry("2+2", Quantity(4)));
    session.addHistoryEntry(HistoryEntry("3+3", Quantity(6)));
    session.addHistoryEntry(HistoryEntry("4+4", Quantity(8)));
    session.addHistoryEntry(HistoryEntry("5+5", Quantity(10)));

    ++eval_total_tests;
    const QList<HistoryEntry> history = session.historyToList();
    if (history.size() != 3 || history.at(0).expr() != "3+3" || history.at(2).expr() != "5+5") {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\thistory trim on add\t[NEW]" << endl;
    }

    settings->maxHistoryEntries = 2;
    session.applyHistoryLimit();

    ++eval_total_tests;
    const QList<HistoryEntry> trimmedHistory = session.historyToList();
    if (trimmedHistory.size() != 2 || trimmedHistory.at(0).expr() != "4+4" || trimmedHistory.at(1).expr() != "5+5") {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\thistory trim on apply limit\t[NEW]" << endl;
    }

    QJsonObject json;
    QJsonArray histEntries;
    for (int i = 1; i <= 4; ++i) {
        QJsonObject entry;
        HistoryEntry(QString::number(i), Quantity(i)).serialize(entry);
        histEntries.append(entry);
    }
    json["speedcrunch"] = QString(SPEEDCRUNCH_VERSION);
    json["hst"] = histEntries;

    Session loaded;
    loaded.deSerialize(json, false);

    ++eval_total_tests;
    const QList<HistoryEntry> loadedHistory = loaded.historyToList();
    if (loadedHistory.size() != 2 || loadedHistory.at(0).expr() != "3" || loadedHistory.at(1).expr() != "4") {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\thistory trim on deserialize\t[NEW]" << endl;
    }

    QJsonObject jsonWithCommentTail;
    QJsonArray histWithCommentTail;
    {
        QJsonObject entry;
        HistoryEntry("21", Quantity(21)).serialize(entry);
        histWithCommentTail.append(entry);
    }
    {
        QJsonObject entry;
        HistoryEntry("? foo", CMath::nan()).serialize(entry);
        histWithCommentTail.append(entry);
    }
    jsonWithCommentTail["speedcrunch"] = QString(SPEEDCRUNCH_VERSION);
    jsonWithCommentTail["hst"] = histWithCommentTail;

    Session loadedWithCommentTail;
    loadedWithCommentTail.deSerialize(jsonWithCommentTail, false);

    ++eval_total_tests;
    if (!loadedWithCommentTail.hasVariable("ans")
        || DMath::format(loadedWithCommentTail.getVariable("ans").value(), Format::Fixed()) != "21") {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tans recovery on deserialize with comment tail\t[NEW]" << endl;
    }

    settings->maxHistoryEntries = 0;
    Session unlimited;
    for (int i = 0; i < 4; ++i)
        unlimited.addHistoryEntry(HistoryEntry(QString("u%1").arg(i), Quantity(i)));

    ++eval_total_tests;
    if (unlimited.historyToList().size() != 4) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\thistory unlimited mode\t[NEW]" << endl;
    }

    settings->maxHistoryEntries = oldMaxHistoryEntries;
}

void test_session_deserialize_without_history()
{
    Session source;
    Session restored;
    Session* const previousSession = const_cast<Session*>(Evaluator::instance()->session());

    Evaluator::instance()->setSession(&source);
    Evaluator::instance()->initializeBuiltInVariables();

    source.addVariable(Variable("persistedVar", Quantity(42), Variable::UserDefined,
                                "Saved variable"));
    source.addUserFunction(UserFunction("persistedFunc", QStringList() << "x", "x+1"));
    source.addHistoryEntry(HistoryEntry("1+1", Quantity(2)));

    QJsonObject json;
    source.serialize(json);
    json.remove("hst");

    Evaluator::instance()->setSession(&restored);
    restored.deSerialize(json, false);

    ++eval_total_tests;
    const bool hasVar = restored.hasVariable("persistedVar")
        && DMath::format(restored.getVariable("persistedVar").value(), Format::Fixed()) == "42";
    const bool hasVarDescription = restored.hasVariable("persistedVar")
        && restored.getVariable("persistedVar").description() == "Saved variable";
    const bool hasFunc = restored.hasUserFunction("persistedFunc");
    const bool hasNoHistory = restored.historyToList().isEmpty();
    if (!hasVar || !hasVarDescription || !hasFunc || !hasNoHistory) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tsession deserialize without history keeps vars/functions\t[NEW]" << endl;
    }

    Evaluator::instance()->setSession(previousSession);
    if (previousSession)
        Evaluator::instance()->initializeBuiltInVariables();
}

void test_function_usage_tooltip()
{
    auto checkTooltip = [](const QString& expression,
                           int cursorPosition,
                           const QString& expected,
                           const char* label)
    {
        ++eval_total_tests;
        const QString actual = FunctionTooltipUtils::activeFunctionUsageTooltip(
            eval, expression, cursorPosition);
        if (actual != expected) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tActual   : " << actual.toUtf8().constData() << endl
                 << "\tExpected : " << expected.toUtf8().constData() << endl;
        }
    };

    checkTooltip(
        "gcd(",
        4,
        "<b>gcd</b>(<b>n<sub>1</sub></b>; n<sub>2</sub>; ...)",
        "tooltip shows built-in usage with HTML subscript"
    );

    checkTooltip(
        "gcd(3;",
        6,
        "<b>gcd</b>(n<sub>1</sub>; <b>n<sub>2</sub></b>; ...)",
        "tooltip highlights second parametre after first separator"
    );

    checkTooltip(
        "gcd(3;4;",
        8,
        "<b>gcd</b>(n<sub>1</sub>; n<sub>2</sub>; <b>...</b>)",
        "tooltip highlights variadic placeholder when typing further arguments"
    );

    checkTooltip(
        "gcd(abs(3);",
        11,
        "<b>gcd</b>(n<sub>1</sub>; <b>n<sub>2</sub></b>; ...)",
        "tooltip tracks outer function in nested calls"
    );

    checkTooltip(
        "lcm(",
        4,
        "<b>lcm</b>(<b>n<sub>1</sub></b>; n<sub>2</sub>; ...)",
        "tooltip shows usage for lcm"
    );

    checkTooltip(
        "lcm",
        3,
        "<b>lcm</b>(<b>n<sub>1</sub></b>; n<sub>2</sub>; ...)",
        "tooltip shows usage when cursor is after typed function name"
    );

    checkTooltip(
        "mass()",
        5,
        "<b>mass</b>(<b>mol</b>; formula)",
        "tooltip shows usage when cursor is inside empty function call"
    );

    checkTooltip(
        "mass()",
        6,
        "<b>mass</b>(<b>mol</b>; formula)",
        "tooltip shows usage at closing parenthesis boundary of empty function call"
    );

    checkTooltip(
        "2+3",
        3,
        "",
        "no tooltip outside function calls"
    );

    eval->setExpression("fff(pao;barro;gis)=pao+barro+gis");
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tsetup user function for tooltip\t[NEW]" << endl
             << "\tError: " << eval->error().toUtf8().constData() << endl;
        return;
    }

    checkTooltip(
        "fff(1;",
        6,
        "<b>fff</b>(pao;<b>barro</b>;gis)",
        "tooltip highlights current user-function parametre"
    );
}

void test_grouped_numeric_literal_display_format()
{
    Settings* settings = Settings::instance();
    const Settings::NumberFormatStyle oldNumberFormatStyle = settings->numberFormatStyle;
    const bool oldDigitGroupingIntegerPartOnly = settings->digitGroupingIntegerPartOnly;
    struct GroupingCase {
        Settings::NumberFormatStyle style;
        QString separator;
        bool indian;
    };
    const GroupingCase cases[] = {
        {Settings::NumberFormatThreeDigitCommaDot, QStringLiteral(","), false},
        {Settings::NumberFormatThreeDigitUnderscoreDot, QStringLiteral("_"), false},
        {Settings::NumberFormatIndianCommaDot, QStringLiteral(","), true}
    };

    for (const auto& groupingCase : cases) {
        settings->numberFormatStyle = groupingCase.style;
        settings->applyNumberFormatStyle();
        settings->digitGroupingIntegerPartOnly = false;

        ++eval_total_tests;
        const QString groupedLiteral =
            DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("234936"));
        const QString expectedLiteral = groupingCase.indian
            ? QString(QStringLiteral("2") + groupingCase.separator
                      + QStringLiteral("34") + groupingCase.separator
                      + QStringLiteral("936"))
            : QString(QStringLiteral("234") + groupingCase.separator + QStringLiteral("936"));
        if (groupedLiteral != expectedLiteral) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tgroup numeric literal for display\t[NEW]" << endl
                 << "\tStyle    : " << static_cast<int>(groupingCase.style) << endl
                 << "\tResult   : " << groupedLiteral.toUtf8().constData() << endl
                 << "\tExpected : " << expectedLiteral.toUtf8().constData() << endl;
        }

        ++eval_total_tests;
        const QString groupedFractional =
            DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("12345.6789"));
        const QString expectedFractional =
            (groupingCase.indian
                ? QStringLiteral("12") + groupingCase.separator + QStringLiteral("345.")
                : QStringLiteral("12") + groupingCase.separator + QStringLiteral("345."))
            + QStringLiteral("678") + groupingCase.separator + QStringLiteral("9");
        if (groupedFractional != expectedFractional) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tgroup fractional when integer-only is off\t[NEW]" << endl
                 << "\tStyle    : " << static_cast<int>(groupingCase.style) << endl
                 << "\tResult   : " << groupedFractional.toUtf8().constData() << endl
                 << "\tExpected : " << expectedFractional.toUtf8().constData() << endl;
        }

        settings->digitGroupingIntegerPartOnly = true;
        ++eval_total_tests;
        const QString groupedIntegerOnly =
            DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("12345.6789"));
        const QString expectedIntegerOnly = QStringLiteral("12") + groupingCase.separator + QStringLiteral("345.6789");
        if (groupedIntegerOnly != expectedIntegerOnly) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tkeep fractional ungrouped when integer-only is on\t[NEW]" << endl
                 << "\tStyle    : " << static_cast<int>(groupingCase.style) << endl
                 << "\tResult   : " << groupedIntegerOnly.toUtf8().constData() << endl
                 << "\tExpected : " << expectedIntegerOnly.toUtf8().constData() << endl;
        }

        settings->digitGroupingIntegerPartOnly = false;
        eval->setExpression("234234+234+234+234");
        const Quantity value = eval->evalUpdateAns();
        ++eval_total_tests;
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate grouped dedup baseline\t[NEW]" << endl
                 << "\tStyle    : " << static_cast<int>(groupingCase.style) << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
        } else {
            const QString groupedResult =
                DisplayFormatUtils::applyDigitGroupingForDisplay(NumberFormatter::format(value));
            if (groupedResult != expectedLiteral) {
                ++eval_failed_tests;
                ++eval_new_failed_tests;
                cerr << __FILE__ << "[" << __LINE__ << "]\tgrouped literal matches formatted result\t[NEW]" << endl
                     << "\tStyle    : " << static_cast<int>(groupingCase.style) << endl
                     << "\tLiteral  : " << expectedLiteral.toUtf8().constData() << endl
                     << "\tResult   : " << groupedResult.toUtf8().constData() << endl;
            }
        }
    }

    settings->numberFormatStyle = oldNumberFormatStyle;
    settings->applyNumberFormatStyle();
    settings->digitGroupingIntegerPartOnly = oldDigitGroupingIntegerPartOnly;
}

void test_pasted_standalone_numeric_literal_reformatting()
{
    Settings* settings = Settings::instance();
    const Settings::NumberFormatStyle oldStyle = settings->numberFormatStyle;
    const bool oldIntegerOnly = settings->digitGroupingIntegerPartOnly;

    settings->numberFormatStyle = Settings::NumberFormatNoGroupingDot;
    settings->applyNumberFormatStyle();
    settings->digitGroupingIntegerPartOnly = true;

    QString formatted;

    ++eval_total_tests;
    const bool okFlatDotDecimal =
        NumberFormatter::tryFormatStandaloneNumericLiteralForDisplay(
            QStringLiteral("15.999"), &formatted);
    if (!okFlatDotDecimal || formatted != QStringLiteral("15.999")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tpreserve decimal dot in no-grouping dot style paste\t[NEW]" << endl
             << "\tResult   : " << (okFlatDotDecimal ? formatted : QString("<invalid>")).toUtf8().constData() << endl
             << "\tExpected : 15.999" << endl;
    }

    settings->numberFormatStyle = Settings::NumberFormatNoGroupingComma;
    settings->applyNumberFormatStyle();
    settings->digitGroupingIntegerPartOnly = true;

    ++eval_total_tests;
    const bool okFlatCommaDecimal =
        NumberFormatter::tryFormatStandaloneNumericLiteralForDisplay(
            QStringLiteral("15,999"), &formatted);
    if (!okFlatCommaDecimal || formatted != QStringLiteral("15,999")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tpreserve decimal comma in no-grouping comma style paste\t[NEW]" << endl
             << "\tResult   : " << (okFlatCommaDecimal ? formatted : QString("<invalid>")).toUtf8().constData() << endl
             << "\tExpected : 15,999" << endl;
    }

    settings->numberFormatStyle = Settings::NumberFormatThreeDigitDotComma;
    settings->applyNumberFormatStyle();
    settings->digitGroupingIntegerPartOnly = true;

    ++eval_total_tests;
    const bool okUsGrouped =
        NumberFormatter::tryFormatStandaloneNumericLiteralForDisplay(
            QStringLiteral("1,234,567.890123"), &formatted);
    if (!okUsGrouped || formatted != QStringLiteral("1.234.567,890123")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tformat pasted standalone grouped decimal\t[NEW]" << endl
             << "\tResult   : " << (okUsGrouped ? formatted : QString("<invalid>")).toUtf8().constData() << endl
             << "\tExpected : 1.234.567,890123" << endl;
    }

    ++eval_total_tests;
    const bool okPlain =
        NumberFormatter::tryFormatStandaloneNumericLiteralForDisplay(
            QStringLiteral("-1234567.890123"), &formatted);
    if (!okPlain || formatted != QStringLiteral("-1.234.567,890123")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tformat pasted standalone plain decimal\t[NEW]" << endl
             << "\tResult   : " << (okPlain ? formatted : QString("<invalid>")).toUtf8().constData() << endl
             << "\tExpected : -1.234.567,890123" << endl;
    }

    ++eval_total_tests;
    const bool okExpression =
        NumberFormatter::tryFormatStandaloneNumericLiteralForDisplay(
            QStringLiteral("1,234+5"), &formatted);
    if (okExpression) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tdo not treat expressions as standalone numeric literal\t[NEW]" << endl
             << "\tResult   : " << formatted.toUtf8().constData() << endl
             << "\tExpected : <invalid>" << endl;
    }

    ++eval_total_tests;
    const bool okUpperExponent =
        NumberFormatter::tryFormatStandaloneNumericLiteralForDisplay(
            QStringLiteral("1.23E+45"), &formatted);
    if (!okUpperExponent || formatted != QStringLiteral("1,23e+45")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tnormalize standalone numeric exponent marker to lower e\t[NEW]" << endl
             << "\tResult   : " << (okUpperExponent ? formatted : QString("<invalid>")).toUtf8().constData() << endl
             << "\tExpected : 1,23e+45" << endl;
    }

    settings->numberFormatStyle = oldStyle;
    settings->applyNumberFormatStyle();
    settings->digitGroupingIntegerPartOnly = oldIntegerOnly;
}

void test_display_root_aliases_for_result_lines()
{
    ++eval_total_tests;
    const QString sqrtDisplay =
        DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("sqrt(2)/2"));
    if (sqrtDisplay != QString::fromUtf8("√(2) / 2")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tformat sqrt alias for display\t[NEW]" << endl
             << "\tResult   : " << sqrtDisplay.toUtf8().constData() << endl
             << "\tExpected : √(2) / 2" << endl;
    }

    ++eval_total_tests;
    const QString cbrtDisplay =
        DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("cbrt(8)"));
    if (cbrtDisplay != QString::fromUtf8("∛(8)")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tformat cbrt alias for display\t[NEW]" << endl
             << "\tResult   : " << cbrtDisplay.toUtf8().constData() << endl
             << "\tExpected : ∛(8)" << endl;
    }

    ++eval_total_tests;
    const QString untouchedIdentifier =
        DisplayFormatUtils::applyDigitGroupingForDisplay(QStringLiteral("asqrt(2)+cbrtfoo(8)"));
    if (untouchedIdentifier != QStringLiteral("asqrt(2) + cbrtfoo(8)")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tdo not rewrite root-like identifiers\t[NEW]" << endl
             << "\tResult   : " << untouchedIdentifier.toUtf8().constData() << endl
             << "\tExpected : asqrt(2) + cbrtfoo(8)" << endl;
    }
}

void test_display_spacing_stability_for_unit_conversion()
{
    ++eval_total_tests;
    const QString input =
        QStringLiteral("3")
        + QString(MathDsl::QuantSp)
        + QString::fromUtf8("[kg·m²/s⁴] → [kg·m²/s⁴]");
    const QString output = DisplayFormatUtils::applyDigitGroupingForDisplay(input);
    if (output != input) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tunit conversion display stability\t[NEW]" << endl
             << "\tResult   : " << output.toUtf8().constData() << endl
             << "\tExpected : " << input.toUtf8().constData() << endl;
    }
}

void test_display_conversion_with_unicode_spaces_and_cross_units()
{
    ++eval_total_tests;
    const QString expr = QString::fromUtf8("3 [kg×m²/s⁴] → [kg×m²/s⁴]");

    eval->setExpression(eval->autoFix(expr));
    eval->evalUpdateAns();
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tunicode conversion display\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    const QString interpreted = eval->interpretedExpression();
    const QString displayed = DisplayFormatUtils::applyDigitGroupingForDisplay(
        Evaluator::formatInterpretedExpressionForDisplay(interpreted));
    const QString expected = QStringLiteral("3")
        + QString(MathDsl::QuantSp)
        + QString::fromUtf8("[kg·m²/s⁴] → kg·m²/s⁴");

    if (displayed != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tunicode conversion display\t[NEW]" << endl
             << "\tDisplayed : " << displayed.toUtf8().constData() << endl
             << "\tExpected  : " << expected.toUtf8().constData() << endl
             << "\tDisplayed code points: " << toCodePointList(displayed) << endl
             << "\tExpected  code points: " << toCodePointList(expected) << endl;
    }

    ++eval_total_tests;
    const QString simplifiedDisplayed = DisplayFormatUtils::applyDigitGroupingForDisplay(
        Evaluator::formatInterpretedExpressionSimplifiedForDisplay(interpreted));
    const bool corrupted = simplifiedDisplayed.contains(QStringLiteral("//"))
        || simplifiedDisplayed.contains(QStringLiteral("··"))
        || hasSpacedOperatorsInsideUnitBrackets(simplifiedDisplayed);
    if (corrupted) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tunicode conversion simplified display\t[NEW]" << endl
             << "\tSimplified: " << simplifiedDisplayed.toUtf8().constData() << endl
             << "\tSimplified code points: " << toCodePointList(simplifiedDisplayed) << endl;
    }
}

void test_preserve_brackets_for_displayed_conversion_target()
{
    ++eval_total_tests;
    const QString source = QString::fromUtf8("3[kg·m²/s⁴] -> [kg·m²/s⁴]");
    const QString interpretedDisplayed = QStringLiteral("3")
        + QString(MathDsl::QuantSp)
        + QString::fromUtf8("[kg·m²/s⁴] → kg·m²/s⁴");
    const QString expected = QStringLiteral("3")
        + QString(MathDsl::QuantSp)
        + QString::fromUtf8("[kg·m²/s⁴] → [kg·m²/s⁴]");

    const QString preserved = DisplayFormatUtils::preserveConversionTargetBracketsForDisplay(
        interpretedDisplayed, source);
    if (preserved != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tpreserve conversion target brackets\t[NEW]" << endl
             << "\tResult   : " << preserved.toUtf8().constData() << endl
             << "\tExpected : " << expected.toUtf8().constData() << endl;
    }
}

void test_preserve_brackets_for_displayed_conversion_target_without_source_hint()
{
    ++eval_total_tests;
    const QString source = QString::fromUtf8("3[m] -> km");
    const QString interpretedDisplayed = QStringLiteral("3")
        + QString(MathDsl::QuantSp)
        + QString::fromUtf8("[m] → km");
    const QString expected = QStringLiteral("3")
        + QString(MathDsl::QuantSp)
        + QString::fromUtf8("[m] → [km]");

    const QString preserved = DisplayFormatUtils::preserveConversionTargetBracketsForDisplay(
        interpretedDisplayed, source);
    if (preserved != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tpreserve conversion target brackets without source hint\t[NEW]" << endl
             << "\tResult   : " << preserved.toUtf8().constData() << endl
             << "\tExpected : " << expected.toUtf8().constData() << endl;
    }
}

void test_result_display_preserves_conversion_target_brackets()
{
    Session* session = const_cast<Session*>(eval->session());
    session->clearHistory();
    session->addHistoryEntry(HistoryEntry(
        QString::fromUtf8("3 [m] → [km]"),
        Quantity(3) / Quantity(1000),
        QString::fromUtf8("3[m]→km")));

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();

    ++eval_total_tests;
    const QString displayed = display.document()->findBlockByNumber(0).text();
    const QString expected = QStringLiteral("3")
        + QString(MathDsl::QuantSp)
        + QString::fromUtf8("[m] → [km]");
    if (displayed != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tresult display preserve conversion target brackets\t[NEW]" << endl
             << "\tDisplayed: " << displayed.toUtf8().constData() << endl
             << "\tExpected : " << expected.toUtf8().constData() << endl;
    }

    session->clearHistory();
}

void test_result_display_adds_normalized_sexagesimal_simplification_line()
{
    Settings* settings = Settings::instance();
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    settings->simplifyResultExpressions = true;

    eval->setExpression(QStringLiteral("1:120:3600"));
    const Quantity value = eval->evalUpdateAns();

    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate sexagesimal normalization case\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        settings->simplifyResultExpressions = oldSimplifyResultExpressions;
        return;
    }

    Session* session = const_cast<Session*>(eval->session());
    session->clearHistory();
    session->addHistoryEntry(HistoryEntry(
        QStringLiteral("1:120:3600"),
        value,
        eval->interpretedExpression()));

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();

    ++eval_total_tests;
    const QString simplifiedLine = display.document()->findBlockByNumber(1).text();
    if (simplifiedLine != QStringLiteral("= 4:00:00")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tnormalized sexagesimal line\t[NEW]" << endl
             << "\tLine     : " << simplifiedLine.toUtf8().constData() << endl
             << "\tExpected : = 4:00:00" << endl;
    }

    ++eval_total_tests;
    const QString scalarLine = display.document()->findBlockByNumber(2).text();
    if (!scalarLine.contains(QStringLiteral("s"))
        || scalarLine.contains(QString(MathDsl::UnitStart))
        || scalarLine.contains(QString(MathDsl::UnitEnd))) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tscalar seconds line keeps unit\t[NEW]" << endl
             << "\tLine     : " << scalarLine.toUtf8().constData() << endl
             << "\tExpected : contains s without []" << endl;
    }

    session->clearHistory();
    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
}

void test_result_display_adds_normalized_sexagesimal_simplification_line_for_arithmetic()
{
    Settings* settings = Settings::instance();
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    settings->simplifyResultExpressions = true;

    eval->setExpression(QStringLiteral("23:50:40 + 5:20:50"));
    const Quantity value = eval->evalUpdateAns();

    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate sexagesimal arithmetic normalization case\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        settings->simplifyResultExpressions = oldSimplifyResultExpressions;
        return;
    }

    Session* session = const_cast<Session*>(eval->session());
    session->clearHistory();
    session->addHistoryEntry(HistoryEntry(
        QStringLiteral("23:50:40 + 5:20:50"),
        value,
        eval->interpretedExpression()));

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();

    ++eval_total_tests;
    const QString simplifiedLine = display.document()->findBlockByNumber(1).text();
    if (simplifiedLine != QStringLiteral("= 29:11:30")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tnormalized sexagesimal arithmetic line\t[NEW]" << endl
             << "\tLine     : " << simplifiedLine.toUtf8().constData() << endl
             << "\tExpected : = 29:11:30" << endl;
    }

    ++eval_total_tests;
    const QString scalarLine = display.document()->findBlockByNumber(2).text();
    if (!scalarLine.contains(QStringLiteral("s"))
        || scalarLine.contains(QString(MathDsl::UnitStart))
        || scalarLine.contains(QString(MathDsl::UnitEnd))) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tscalar seconds arithmetic line keeps unit\t[NEW]" << endl
             << "\tLine     : " << scalarLine.toUtf8().constData() << endl
             << "\tExpected : contains s without []" << endl;
    }

    session->clearHistory();
    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
}

void test_result_display_preserves_fractional_seconds_in_normalized_sexagesimal_line()
{
    Settings* settings = Settings::instance();
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    settings->simplifyResultExpressions = true;

    eval->setExpression(QStringLiteral("12:34:56.75 + 1:25:30.50"));
    const Quantity value = eval->evalUpdateAns();

    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate fractional sexagesimal arithmetic normalization case\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        settings->simplifyResultExpressions = oldSimplifyResultExpressions;
        return;
    }

    Session* session = const_cast<Session*>(eval->session());
    session->clearHistory();
    session->addHistoryEntry(HistoryEntry(
        QStringLiteral("12:34:56.75 + 1:25:30.50"),
        value,
        eval->interpretedExpression()));

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();

    ++eval_total_tests;
    const QString simplifiedLine = display.document()->findBlockByNumber(1).text();
    if (simplifiedLine != QStringLiteral("= 14:00:27.25")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tnormalized sexagesimal arithmetic fractional seconds line\t[NEW]" << endl
             << "\tLine     : " << simplifiedLine.toUtf8().constData() << endl
             << "\tExpected : = 14:00:27.25" << endl;
    }

    ++eval_total_tests;
    const QString scalarLine = display.document()->findBlockByNumber(2).text();
    if (!scalarLine.contains(QStringLiteral("50427.25"))
        || !scalarLine.contains(QStringLiteral("s"))
        || scalarLine.contains(QString(MathDsl::UnitStart))
        || scalarLine.contains(QString(MathDsl::UnitEnd))) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tscalar seconds fractional arithmetic line keeps value and unit\t[NEW]" << endl
             << "\tLine     : " << scalarLine.toUtf8().constData() << endl
             << "\tExpected : contains 50427.25 and s without []" << endl;
    }

    session->clearHistory();
    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
}

void test_result_display_normalized_sexagesimal_line_with_time_conversion_target()
{
    Settings* settings = Settings::instance();
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    settings->simplifyResultExpressions = true;

    eval->setExpression(QStringLiteral("5:59:59.875 + 0:00:00.250 -> [ms]"));
    const Quantity value = eval->evalUpdateAns();

    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate sexagesimal conversion normalization case\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        settings->simplifyResultExpressions = oldSimplifyResultExpressions;
        return;
    }

    Session* session = const_cast<Session*>(eval->session());
    session->clearHistory();
    session->addHistoryEntry(HistoryEntry(
        QStringLiteral("5:59:59.875 + 0:00:00.250 -> [ms]"),
        value,
        eval->interpretedExpression()));

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();

    ++eval_total_tests;
    const QString simplifiedLine = display.document()->findBlockByNumber(1).text();
    if (simplifiedLine != QString::fromUtf8("= 6:00:00.125 → [ms]")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tnormalized sexagesimal conversion line\t[NEW]" << endl
             << "\tLine     : " << simplifiedLine.toUtf8().constData() << endl
             << "\tExpected : = 6:00:00.125 → [ms]" << endl;
    }

    ++eval_total_tests;
    const QString scalarLine = display.document()->findBlockByNumber(2).text();
    if (!scalarLine.contains(QStringLiteral("21600125"))
        || !scalarLine.contains(QStringLiteral("ms"))
        || scalarLine.contains(QString(MathDsl::UnitStart))
        || scalarLine.contains(QString(MathDsl::UnitEnd))) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tscalar milliseconds conversion line keeps value and unit\t[NEW]" << endl
             << "\tLine     : " << scalarLine.toUtf8().constData() << endl
             << "\tExpected : contains 21600125 and ms without []" << endl;
    }

    session->clearHistory();
    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
}

void test_sexagesimal_arithmetic_with_per_term_conversion_targets()
{
    eval->setExpression(QString::fromUtf8("−5:59:59.875 → [ms] + 0:00:00.250 → [ms]"));
    const Quantity value = eval->evalUpdateAns();

    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate per-term converted sexagesimal arithmetic\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    ++eval_total_tests;
    const QString scalar = NumberFormatter::format(value);
    // Result rendering may use Unicode minus and omit a space before [unit].
    // Normalize only the minus sign so the assertion focuses on numeric value.
    const QString scalarAsciiMinus = QString(scalar).replace(UnicodeChars::MinusSign, QLatin1Char('-'));
    if (!scalarAsciiMinus.contains(QStringLiteral("-21599625"))
        || !scalar.contains(QStringLiteral("[ms]"))) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tper-term converted sexagesimal arithmetic scalar result\t[NEW]" << endl
             << "\tResult   : " << scalar.toUtf8().constData() << endl
             << "\tExpected : contains -21599625 and [ms]" << endl;
    }
}

void test_result_display_mixed_per_term_time_conversions()
{
    Settings* settings = Settings::instance();
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    settings->simplifyResultExpressions = true;

    eval->setExpression(QString::fromUtf8("−5:59:59.875 → [h] + 0:00:00.250 → [s] − 10:0:0 → [ms]"));
    const Quantity value = eval->evalUpdateAns();

    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate mixed per-term time conversions\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        settings->simplifyResultExpressions = oldSimplifyResultExpressions;
        return;
    }

    Session* session = const_cast<Session*>(eval->session());
    session->clearHistory();
    session->addHistoryEntry(HistoryEntry(
        QString::fromUtf8("−5:59:59.875 → [h] + 0:00:00.250 → [s] − 10:0:0 → [ms]"),
        value,
        eval->interpretedExpression()));

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();

    ++eval_total_tests;
    const QString simplifiedLine = display.document()->findBlockByNumber(1).text();
    if (simplifiedLine != QString::fromUtf8("= −15:59:59.625 → [ms]")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tmixed per-term time conversions normalized line\t[NEW]" << endl
             << "\tLine     : " << simplifiedLine.toUtf8().constData() << endl
             << "\tExpected : = −15:59:59.625 → [ms]" << endl;
    }

    ++eval_total_tests;
    const QString scalarLine = display.document()->findBlockByNumber(2).text();
    if (!scalarLine.contains(QStringLiteral("57599625"))
        || !scalarLine.contains(QStringLiteral("ms"))
        || scalarLine.contains(QString(MathDsl::UnitStart))
        || scalarLine.contains(QString(MathDsl::UnitEnd))
        || !scalarLine.contains(QString(UnicodeChars::MinusSign))) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tmixed per-term time conversions scalar line\t[NEW]" << endl
             << "\tLine     : " << scalarLine.toUtf8().constData() << endl
             << "\tExpected : contains −57599625 and ms without []" << endl;
    }

    session->clearHistory();
    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
}

void test_result_display_mixed_per_term_time_conversions_in_sexagesimal_notation()
{
    Settings* settings = Settings::instance();
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    const char oldResultFormat = settings->resultFormat;
    settings->simplifyResultExpressions = true;
    settings->resultFormat = 's';

    eval->setExpression(QString::fromUtf8("−5:59:59.875 → [h] + 0:00:00.250 → [s] − 10:0:0 → [ms]"));
    const Quantity value = eval->evalUpdateAns();

    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate mixed per-term time conversions in sexagesimal notation\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        settings->simplifyResultExpressions = oldSimplifyResultExpressions;
        settings->resultFormat = oldResultFormat;
        return;
    }

    Session* session = const_cast<Session*>(eval->session());
    session->clearHistory();
    session->addHistoryEntry(HistoryEntry(
        QString::fromUtf8("−5:59:59.875 → [h] + 0:00:00.250 → [s] − 10:0:0 → [ms]"),
        value,
        eval->interpretedExpression()));

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();

    ++eval_total_tests;
    const QString simplifiedLine = display.document()->findBlockByNumber(1).text();
    if (simplifiedLine != QString::fromUtf8("= −15:59:59.625 → [ms]")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tmixed per-term sexagesimal normalized line keeps conversion target brackets\t[NEW]" << endl
             << "\tLine     : " << simplifiedLine.toUtf8().constData() << endl
             << "\tExpected : = −15:59:59.625 → [ms]" << endl;
    }

    ++eval_total_tests;
    const QString scalarLine = display.document()->findBlockByNumber(2).text();
    if (scalarLine != QString::fromUtf8("= −15:59:59.625")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tmixed per-term sexagesimal final value line\t[NEW]" << endl
             << "\tLine     : " << scalarLine.toUtf8().constData() << endl
             << "\tExpected : = −15:59:59.625" << endl;
    }

    session->clearHistory();
    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
    settings->resultFormat = oldResultFormat;
}

void test_result_display_strips_unit_brackets_and_double_click_restores_canonical_unit_expression()
{
    Session* session = const_cast<Session*>(eval->session());
    session->clearHistory();
    eval->setExpression(QStringLiteral("2[s]"));
    const Quantity value = eval->evalUpdateAns();
    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate result display unit strip case\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        session->clearHistory();
        return;
    }
    session->addHistoryEntry(HistoryEntry(
        QStringLiteral("2[s]"),
        value,
        eval->interpretedExpression()));

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();

    ++eval_total_tests;
    const QString displayedResultLine = display.document()->findBlockByNumber(1).text();
    if (!displayedResultLine.contains(QStringLiteral("s"))
        || displayedResultLine.contains(QString(MathDsl::UnitStart))
        || displayedResultLine.contains(QString(MathDsl::UnitEnd))) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tresult display strips unit brackets\t[NEW]" << endl
             << "\tDisplayed: " << displayedResultLine.toUtf8().constData() << endl
             << "\tExpected : contains s and no []" << endl;
    }

    QString selectedExpression;
    QObject::connect(&display, &ResultDisplay::expressionSelected,
                     [&selectedExpression](const QString& text) {
                         selectedExpression = text;
                     });

    QTextCursor cursor(display.document()->findBlockByNumber(1));
    display.setTextCursor(cursor);
    const QPoint eventPos = display.pointForBlockStart(1);
    QMouseEvent event(
        QEvent::MouseButtonDblClick,
        QPointF(eventPos),
        QPointF(display.mapToGlobal(eventPos)),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    display.mouseDoubleClickEvent(&event);

    const QString expectedSelectedExpression = QStringLiteral("2")
        + QString(MathDsl::QuantSp)
        + QString(MathDsl::UnitStart)
        + QStringLiteral("s")
        + QString(MathDsl::UnitEnd);

    ++eval_total_tests;
    if (selectedExpression != expectedSelectedExpression) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tdouble-click restores canonical unit expression\t[NEW]" << endl
             << "\tSelected: " << selectedExpression.toUtf8().constData() << endl
             << "\tExpected: " << expectedSelectedExpression.toUtf8().constData() << endl;
    }

    session->clearHistory();
}

void test_result_display_uses_base10_exponent_notation()
{
    Session* session = const_cast<Session*>(eval->session());
    Settings* settings = Settings::instance();
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    const char oldResultFormat = settings->resultFormat;
    const int oldResultPrecision = settings->resultPrecision;

    settings->simplifyResultExpressions = false;
    settings->resultFormat = 'g';
    settings->resultPrecision = -1;
    session->clearHistory();

    const struct {
        const char* expr;
        QString expectedResultLine;
    } cases[] = {
        { "1.23e45", QString::fromUtf8("= 1.23 × 10⁴⁵") },
        { "123e-45", QString::fromUtf8("= 1.23 × 10⁻⁴³") }
    };

    for (const auto& tc : cases) {
        eval->setExpression(QString::fromLatin1(tc.expr));
        const Quantity value = eval->evalUpdateAns();
        ++eval_total_tests;
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate scientific result display case\t[NEW]" << endl
                 << "\tExpression: " << tc.expr << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
            settings->simplifyResultExpressions = oldSimplifyResultExpressions;
            settings->resultFormat = oldResultFormat;
            settings->resultPrecision = oldResultPrecision;
            session->clearHistory();
            return;
        }

        session->clearHistory();
        session->addHistoryEntry(HistoryEntry(
            QString::fromLatin1(tc.expr),
            value,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString resultLine = display.document()->findBlockByNumber(1).text();
        if (resultLine != tc.expectedResultLine) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tresult display uses base-10 exponent notation\t[NEW]" << endl
                 << "\tExpression: " << tc.expr << endl
                 << "\tResult   : " << resultLine.toUtf8().constData() << endl
                 << "\tExpected : " << tc.expectedResultLine.toUtf8().constData() << endl;
        }
    }

    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
    settings->resultFormat = oldResultFormat;
    settings->resultPrecision = oldResultPrecision;
    session->clearHistory();
}

void test_result_display_omits_zero_power_of_ten_generically()
{
    Session* session = const_cast<Session*>(eval->session());
    Settings* settings = Settings::instance();
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    const char oldResultFormat = settings->resultFormat;
    const int oldResultPrecision = settings->resultPrecision;

    settings->simplifyResultExpressions = false;
    settings->resultPrecision = -1;
    session->clearHistory();

    const struct {
        const char* expr;
        char resultFormat;
        QString expectedResultLine;
    } cases[] = {
        { "4.091", 'g', QString::fromUtf8("= 4.091") },
        { "4.091", 'e', QString::fromUtf8("= 4.091") },
        { "4.091", 'n', QString::fromUtf8("= 4.091") },
        { "4.091", 'f', QString::fromUtf8("= 4.091") },
        { "sci(4.091)", 'e', QString::fromUtf8("= 4.091") },
        { "eng(4.091)", 'e', QString::fromUtf8("= 4.091") },
        { "sci(4.091)", 'n', QString::fromUtf8("= 4.091") },
        { "eng(4.091)", 'n', QString::fromUtf8("= 4.091") },
        { "sci(4.091) ? automatic decimal", 'g', QString::fromUtf8("= 4.091") },
        { "eng(4.091) ? automatic decimal", 'g', QString::fromUtf8("= 4.091") },
        { "4.091 ? here notation = engineering", 'g', QString::fromUtf8("= 4.091") },
        { "4.091 ? here notation = scientific", 'g', QString::fromUtf8("= 4.091") }
    };

    for (const auto& tc : cases) {
        settings->resultFormat = tc.resultFormat;
        eval->setExpression(QString::fromLatin1(tc.expr));
        const Quantity value = eval->evalUpdateAns();
        ++eval_total_tests;
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate zero exponent omission case\t[NEW]" << endl
                 << "\tExpression: " << tc.expr << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
            continue;
        }

        session->clearHistory();
        session->addHistoryEntry(HistoryEntry(
            QString::fromLatin1(tc.expr),
            value,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString resultLine = display.document()->findBlockByNumber(1).text();
        if (resultLine != tc.expectedResultLine) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tresult display omits zero scientific power generically\t[NEW]" << endl
                 << "\tExpression: " << tc.expr << endl
                 << "\tResult   : " << resultLine.toUtf8().constData() << endl
                 << "\tExpected : " << tc.expectedResultLine.toUtf8().constData() << endl;
        }
    }

    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
    settings->resultFormat = oldResultFormat;
    settings->resultPrecision = oldResultPrecision;
    session->clearHistory();
}

void test_result_display_double_click_preserves_compact_angle_suffix()
{
    Session* session = const_cast<Session*>(eval->session());
    Settings* settings = Settings::instance();
    const char oldAngleUnit = settings->angleUnit;
    settings->angleUnit = 'd';
    Evaluator::instance()->initializeAngleUnits();
    session->clearHistory();

    eval->setExpression(QStringLiteral("(21600 / 2)[arcminute]"));
    const Quantity value = eval->evalUpdateAns();
    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate compact angle suffix paste case\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        settings->angleUnit = oldAngleUnit;
        Evaluator::instance()->initializeAngleUnits();
        session->clearHistory();
        return;
    }

    session->addHistoryEntry(HistoryEntry(
        QStringLiteral("(21600 / 2)[arcminute]"),
        value,
        eval->interpretedExpression()));

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();

    QString selectedExpression;
    QObject::connect(&display, &ResultDisplay::expressionSelected,
                     [&selectedExpression](const QString& text) {
                         selectedExpression = text;
                     });

    QTextCursor cursor(display.document()->findBlockByNumber(1));
    display.setTextCursor(cursor);
    const QPoint eventPos = display.pointForBlockStart(1);
    QMouseEvent event(
        QEvent::MouseButtonDblClick,
        QPointF(eventPos),
        QPointF(display.mapToGlobal(eventPos)),
        Qt::LeftButton,
        Qt::LeftButton,
        Qt::NoModifier);
    display.mouseDoubleClickEvent(&event);

    ++eval_total_tests;
    if (selectedExpression != QString::fromUtf8("180°")) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tdouble-click preserves compact angle suffix\t[NEW]" << endl
             << "\tSelected: " << selectedExpression.toUtf8().constData() << endl
             << "\tExpected: 180°" << endl;
    }

    settings->angleUnit = oldAngleUnit;
    Evaluator::instance()->initializeAngleUnits();
    session->clearHistory();
}

void test_result_display_keeps_quantsp_before_degree_celsius_and_fahrenheit()
{
    Session* session = const_cast<Session*>(eval->session());
    session->clearHistory();

    eval->setExpression(QString::fromUtf8("77 [°F] -> [°C]"));
    const Quantity value = eval->evalUpdateAns();
    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate fahrenheit to celsius display spacing case\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        session->clearHistory();
        return;
    }

    session->addHistoryEntry(HistoryEntry(
        QString::fromUtf8("77 [°F] -> [°C]"),
        value,
        eval->interpretedExpression()));

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();

    ++eval_total_tests;
    const QString resultLine = display.document()->findBlockByNumber(1).text();
    const QString expectedSuffix = QString(MathDsl::QuantSp) + QString::fromUtf8("°C");
    if (!resultLine.endsWith(expectedSuffix)) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tresult display keeps QuantSp before °C\t[NEW]" << endl
             << "\tLine     : " << resultLine.toUtf8().constData() << endl
             << "\tExpected : ends with "
             << expectedSuffix.toUtf8().constData() << endl;
    }

    session->clearHistory();
}

void test_result_display_shows_angle_mode_unit_suffix_for_explicit_angle_input()
{
    Settings* settings = Settings::instance();
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    Session* session = const_cast<Session*>(eval->session());
    session->clearHistory();
    auto checkAngleModeSuffix = [&](char angleUnit,
                                    const QString& expectedSuffix,
                                    bool expectNoSpaceBeforeUnit,
                                    const char* label) {
        settings->angleUnit = angleUnit;
        Evaluator::instance()->initializeAngleUnits();
        session->clearHistory();
        eval->setExpression(QStringLiteral("2 [arcsecond]"));
        const Quantity value = eval->evalUpdateAns();

        ++eval_total_tests;
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
            return;
        }

        session->addHistoryEntry(HistoryEntry(
            QStringLiteral("2 [arcsecond]"),
            value,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString resultLine = display.document()->findBlockByNumber(1).text();
        const QString expectedEnding = expectNoSpaceBeforeUnit
            ? expectedSuffix
            : (QString(MathDsl::QuantSp) + expectedSuffix);
        const bool degreeSpacingOk = !expectNoSpaceBeforeUnit
            || (!resultLine.endsWith(QString(MathDsl::QuantSp) + expectedSuffix)
                && !resultLine.endsWith(QStringLiteral(" ") + expectedSuffix));
        if (!resultLine.endsWith(expectedEnding)
            || resultLine.contains(QString(MathDsl::UnitStart))
            || resultLine.contains(QString(MathDsl::UnitEnd))
            || !degreeSpacingOk)
        {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tLine     : " << resultLine.toUtf8().constData() << endl
                 << "\tExpected : ends with "
                 << expectedEnding.toUtf8().constData()
                 << " and has no []" << endl;
        }
    };
    auto checkTrigOutputHasNoAngleSuffix = [&](char angleUnit, const char* label) {
        settings->angleUnit = angleUnit;
        Evaluator::instance()->initializeAngleUnits();
        session->clearHistory();
        eval->setExpression(QString::fromUtf8("cos(180°)"));
        const Quantity value = eval->evalUpdateAns();

        ++eval_total_tests;
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
            return;
        }

        session->addHistoryEntry(HistoryEntry(
            QString::fromUtf8("cos(180°)"),
            value,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString resultLine = display.document()->findBlockByNumber(1).text();
        const QString angleSuffixSpaced = QString(MathDsl::QuantSp) + Units::angleModeUnitSymbol(angleUnit);
        const QString angleSuffixCompact = Units::angleModeUnitSymbol(angleUnit);
        if (resultLine.endsWith(angleSuffixSpaced)
            || resultLine.endsWith(angleSuffixCompact)
            || resultLine.contains(QString(MathDsl::UnitStart))
            || resultLine.contains(QString(MathDsl::UnitEnd))) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tLine     : " << resultLine.toUtf8().constData() << endl
                 << "\tExpected : scalar trig result with no angle suffix or []" << endl;
        }
    };
    auto checkScientificAngleUnitSpacing = [&](const QString& expr,
                                               const QString& expectedSuffix,
                                               const char* label) {
        const char previousResultFormat = settings->resultFormat;
        settings->angleUnit = 'd';
        settings->resultFormat = 'e';
        settings->simplifyResultExpressions = false;
        Evaluator::instance()->initializeAngleUnits();
        session->clearHistory();
        eval->setExpression(expr);
        const Quantity value = eval->evalUpdateAns();

        ++eval_total_tests;
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
            settings->resultFormat = previousResultFormat;
            return;
        }

        session->addHistoryEntry(HistoryEntry(
            expr,
            value,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString resultLine = display.document()->findBlockByNumber(1).text();
        const QString expectedEnding = QString(MathDsl::QuantSp) + expectedSuffix;
        const bool hasScientificPower = resultLine.contains(QString::fromUtf8("× 10"));
        if (!hasScientificPower
            || !resultLine.endsWith(expectedEnding)
            || resultLine.endsWith(QStringLiteral(" ") + expectedSuffix)) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tLine     : " << resultLine.toUtf8().constData() << endl
                 << "\tExpected : scientific notation and ending with "
                 << expectedEnding.toUtf8().constData() << endl;
        }
        settings->resultFormat = previousResultFormat;
    };
    auto checkSimplifiedLineForRepeatedTrigWithDegreeSignInResultDisplay = [&]() {
        settings->angleUnit = 'd';
        settings->simplifyResultExpressions = true;
        Evaluator::instance()->initializeAngleUnits();
        session->clearHistory();
        eval->setExpression(QString::fromUtf8(
            "cos(180°) + cos(180°) + cos(180°) + cos(180°)"));
        const Quantity value = eval->evalUpdateAns();

        ++eval_total_tests;
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tresult display simplified line for repeated trig degree-sign input\t[NEW]" << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
            return;
        }

        session->addHistoryEntry(HistoryEntry(
            QString::fromUtf8("cos(180°) + cos(180°) + cos(180°) + cos(180°)"),
            value,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString line1 = display.document()->findBlockByNumber(1).text();
        QString line2 = display.document()->findBlockByNumber(2).text();
        line2.replace(QString(MathDsl::SubOp), QString(MathDsl::SubOpAl1));
        const QString wrappedDot = QString(MathDsl::MulDotWrapSp)
            + QString(MathDsl::MulDotOp)
            + QString(MathDsl::MulDotWrapSp);
        // Guard the exact display contract: grouped coefficients must keep
        // wrapped middle-dot spacing ("4 · cos(...)"), not compact "4·cos(...)".
        const bool hasBracketedDegreeArg =
            line1.contains(QString::fromUtf8("cos(180"))
            && line1.contains(QString::fromUtf8("[°])"));
        const bool hasCompactDegreeArg =
            line1.contains(QString::fromUtf8("cos(180"))
            && line1.contains(QString::fromUtf8("°)"));
        const bool hasSimplifiedTrigLine =
            line1.startsWith(QStringLiteral("= "))
            && line1.contains(QString::fromUtf8("4"))
            && line1.contains(wrappedDot)
            && (hasBracketedDegreeArg || hasCompactDegreeArg);
        const bool hasFinalResultLine = line2.startsWith(QStringLiteral("= "))
            && line2.contains(QStringLiteral("-4"));
        if (!hasSimplifiedTrigLine || !hasFinalResultLine) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tresult display simplified line for repeated trig degree-sign input\t[NEW]" << endl
                 << "\tText     : " << display.document()->toPlainText().toUtf8().constData() << endl
                 << "\tExpected : simplified trig line and final = -4 line" << endl;
        }
    };
    auto checkSimplifiedLineForMixedDegreeAliasesInResultDisplay = [&]() {
        settings->angleUnit = 'd';
        settings->simplifyResultExpressions = true;
        Evaluator::instance()->initializeAngleUnits();
        session->clearHistory();
        const QString expr = QString::fromUtf8(
            "cos(180[deg]) + cos(180[degree]) + cos(180 [°])+ cos(180 [deg])");
        eval->setExpression(expr);
        const Quantity value = eval->evalUpdateAns();

        ++eval_total_tests;
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tresult display simplified line for mixed degree aliases\t[NEW]" << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
            return;
        }

        session->addHistoryEntry(HistoryEntry(
            expr,
            value,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        const QString line1 = display.document()->findBlockByNumber(1).text();
        QString line2 = display.document()->findBlockByNumber(2).text();
        line2.replace(QString(MathDsl::SubOp), QString(MathDsl::SubOpAl1));
        const bool hasBracketedDegreeArg =
            line1.contains(QString::fromUtf8("cos(180"))
            && line1.contains(QString::fromUtf8("[°])"));
        const bool hasCompactDegreeArg =
            line1.contains(QString::fromUtf8("cos(180"))
            && line1.contains(QString::fromUtf8("°)"));
        const bool hasSimplifiedTrigLine =
            line1.startsWith(QStringLiteral("= "))
            && line1.contains(QString::fromUtf8("4"))
            && (hasBracketedDegreeArg || hasCompactDegreeArg);
        const bool hasFinalResultLine = line2.startsWith(QStringLiteral("= "))
            && line2.contains(QStringLiteral("-4"));

        ++eval_total_tests;
        if (!hasSimplifiedTrigLine || !hasFinalResultLine) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tresult display simplified line for mixed degree aliases\t[NEW]" << endl
                 << "\tText     : " << display.document()->toPlainText().toUtf8().constData() << endl
                 << "\tExpected : simplified trig line with coefficient 4 and final = -4 line" << endl;
        }
    };

    checkAngleModeSuffix('r', Units::angleModeUnitSymbol('r'), false,
                         "result display angle suffix in radian mode");
    checkAngleModeSuffix('d', Units::angleModeUnitSymbol('d'), true,
                         "result display angle suffix in degree mode");
    checkAngleModeSuffix('g', Units::angleModeUnitSymbol('g'), false,
                         "result display angle suffix in gradian mode");
    checkAngleModeSuffix('t', Units::angleModeUnitSymbol('t'), false,
                         "result display angle suffix in turn mode");
    checkAngleModeSuffix('v', Units::angleModeUnitSymbol('v'), false,
                         "result display angle suffix in revolution mode");
    checkTrigOutputHasNoAngleSuffix('r',
                                    "result display trig output has no radian suffix");
    checkTrigOutputHasNoAngleSuffix('d',
                                    "result display trig output has no degree suffix");
    checkSimplifiedLineForRepeatedTrigWithDegreeSignInResultDisplay();
    checkSimplifiedLineForMixedDegreeAliasesInResultDisplay();
    checkScientificAngleUnitSpacing(
        QStringLiteral("1e8 [arcsecond]"),
        QString::fromUtf8("°"),
        "result display scientific degree suffix keeps QuantSp");
    checkScientificAngleUnitSpacing(
        QStringLiteral("1e8 [arcsecond] -> [arcminute]"),
        QString::fromUtf8("′"),
        "result display scientific arcminute suffix keeps QuantSp");
    checkScientificAngleUnitSpacing(
        QStringLiteral("1e8 [arcsecond] -> [arcsecond]"),
        QString::fromUtf8("″"),
        "result display scientific arcsecond suffix keeps QuantSp");

    settings->angleUnit = 'r';
    Evaluator::instance()->initializeAngleUnits();
    session->clearHistory();
    eval->setExpression(QString::fromUtf8("1°2′3″"));
    const Quantity sexagesimalValue = eval->evalUpdateAns();
    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tresult display angle suffix for sexagesimal literal\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        session->addHistoryEntry(HistoryEntry(
            QString::fromUtf8("1°2′3″"),
            sexagesimalValue,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString resultLine = display.document()->findBlockByNumber(1).text();
        const QString expectedEnding = QString(MathDsl::QuantSp) + Units::angleModeUnitSymbol('r');
        if (!resultLine.endsWith(expectedEnding)
            || resultLine.contains(QString(MathDsl::UnitStart))
            || resultLine.contains(QString(MathDsl::UnitEnd))) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tresult display angle suffix for sexagesimal literal\t[NEW]" << endl
                 << "\tLine     : " << resultLine.toUtf8().constData() << endl
                 << "\tExpected : ends with "
                 << expectedEnding.toUtf8().constData()
                 << " and has no []" << endl;
        }
    }

    settings->angleUnit = 'r';
    Evaluator::instance()->initializeAngleUnits();
    session->clearHistory();
    eval->setExpression(QString::fromUtf8("−57°17′44.80624709635515647336″"));
    const Quantity negativeSexagesimalValue = eval->evalUpdateAns();
    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tresult display angle suffix for negative sexagesimal literal\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        session->addHistoryEntry(HistoryEntry(
            QString::fromUtf8("−57°17′44.80624709635515647336″"),
            negativeSexagesimalValue,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString resultLine = display.document()->findBlockByNumber(1).text();
        const QString expectedEnding = QString(MathDsl::QuantSp) + Units::angleModeUnitSymbol('r');
        if (!resultLine.endsWith(expectedEnding)
            || resultLine.contains(QString(MathDsl::UnitStart))
            || resultLine.contains(QString(MathDsl::UnitEnd))) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tresult display angle suffix for negative sexagesimal literal\t[NEW]" << endl
                 << "\tLine     : " << resultLine.toUtf8().constData() << endl
                 << "\tExpected : ends with "
                 << expectedEnding.toUtf8().constData()
                 << " and has no []" << endl;
        }
    }

    settings->angleUnit = 'd';
    Evaluator::instance()->initializeAngleUnits();
    session->clearHistory();
    eval->setExpression(QStringLiteral("1 [rev]→[°]"));
    const Quantity degreeConversionValue = eval->evalUpdateAns();
    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tresult display spacing for degree conversion target\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        session->addHistoryEntry(HistoryEntry(
            QStringLiteral("1 [rev]→[°]"),
            degreeConversionValue,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString resultLine = display.document()->findBlockByNumber(1).text();
        if (!resultLine.endsWith(QStringLiteral("360°"))) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tresult display spacing for degree conversion target\t[NEW]" << endl
                 << "\tLine     : " << resultLine.toUtf8().constData() << endl
                 << "\tExpected : ends with 360°" << endl;
        }
    }

    settings->angleUnit = 'd';
    Evaluator::instance()->initializeAngleUnits();
    session->clearHistory();
    eval->setExpression(QStringLiteral("1 [rev] -> [arcmin]"));
    const Quantity arcminuteConversionValue = eval->evalUpdateAns();
    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tresult display spacing for arcminute conversion target\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        session->addHistoryEntry(HistoryEntry(
            QStringLiteral("1 [rev] -> [arcmin]"),
            arcminuteConversionValue,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString resultLine = display.document()->findBlockByNumber(1).text();
        if (!resultLine.endsWith(QString::fromUtf8("21600′"))) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tresult display spacing for arcminute conversion target\t[NEW]" << endl
                 << "\tLine     : " << resultLine.toUtf8().constData() << endl
                 << "\tExpected : ends with 21600′" << endl;
        }
    }

    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
    Evaluator::instance()->initializeAngleUnits();
    session->clearHistory();
}

void test_value_unit_separator_normalization()
{
    const QString expectedWithSeparator = QStringLiteral("1")
        + QString(MathDsl::QuantSp)
        + QStringLiteral("[kg]");
    const QString expectedHexWithSeparator = QStringLiteral("0xA")
        + QString(MathDsl::QuantSp)
        + QStringLiteral("[kg]");

    ++eval_total_tests;
    const QString noSpaceInput =
        DisplayFormatUtils::applyValueUnitSpacingForDisplay(QStringLiteral("1[kg]"));
    if (noSpaceInput != expectedWithSeparator) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tnormalize value-unit without space\t[NEW]" << endl
             << "\tResult   : " << noSpaceInput.toUtf8().constData() << endl
             << "\tExpected : " << expectedWithSeparator.toUtf8().constData() << endl;
    }

    ++eval_total_tests;
    const QString regularSpaceInput =
        DisplayFormatUtils::applyValueUnitSpacingForDisplay(QStringLiteral("1 [kg]"));
    if (regularSpaceInput != expectedWithSeparator) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tnormalize value-unit regular space\t[NEW]" << endl
             << "\tResult   : " << regularSpaceInput.toUtf8().constData() << endl
             << "\tExpected : " << expectedWithSeparator.toUtf8().constData() << endl;
    }

    ++eval_total_tests;
    const QString noSpaceHexInput =
        DisplayFormatUtils::applyValueUnitSpacingForDisplay(QStringLiteral("0xA[kg]"));
    if (noSpaceHexInput != expectedHexWithSeparator) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tnormalize value-unit hex without space\t[NEW]" << endl
             << "\tResult   : " << noSpaceHexInput.toUtf8().constData() << endl
             << "\tExpected : " << expectedHexWithSeparator.toUtf8().constData() << endl;
    }

    ++eval_total_tests;
    const QString regularSpaceHexInput =
        DisplayFormatUtils::applyValueUnitSpacingForDisplay(QStringLiteral("0xA [kg]"));
    if (regularSpaceHexInput != expectedHexWithSeparator) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tnormalize value-unit hex regular space\t[NEW]" << endl
             << "\tResult   : " << regularSpaceHexInput.toUtf8().constData() << endl
             << "\tExpected : " << expectedHexWithSeparator.toUtf8().constData() << endl;
    }
}

void test_result_display_preserves_standalone_sexagesimal_angles_without_implicit_conversion()
{
    Settings* settings = Settings::instance();
    Session* session = const_cast<Session*>(eval->session());
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    const bool oldMultipleResultLinesEnabled = settings->multipleResultLinesEnabled;
    const char oldResultFormat = settings->resultFormat;
    const int oldResultPrecision = settings->resultPrecision;
    const char oldAngleUnit = settings->angleUnit;

    settings->simplifyResultExpressions = false;
    settings->multipleResultLinesEnabled = false;
    settings->resultPrecision = -1;
    settings->angleUnit = 'd';
    Evaluator::instance()->initializeAngleUnits();

    struct Case {
        char format;
        const char* label;
    };
    const Case cases[] = {
        {'s', "sexagesimal format preserves standalone sexagesimal angle"},
        {'g', "automatic decimal format preserves standalone sexagesimal angle"}
    };

    for (const Case& tc : cases) {
        settings->resultFormat = tc.format;
        session->clearHistory();
        eval->setExpression(QString::fromUtf8("1°2′3″"));
        const Quantity value = eval->evalUpdateAns();

        ++eval_total_tests;
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << tc.label << "\t[NEW]" << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
            continue;
        }

        session->addHistoryEntry(HistoryEntry(
            QString::fromUtf8("1°2′3″"),
            value,
            eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        const QString resultLine = display.document()->findBlockByNumber(1).text();
        const QString expressionLine = display.document()->findBlockByNumber(0).text();

        ++eval_total_tests;
        if (expressionLine != QString::fromUtf8("1°2′3″")) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << tc.label << " expression line\t[NEW]" << endl
                 << "\tLine     : " << expressionLine.toUtf8().constData() << endl
                 << "\tExpected : 1°2′3″" << endl;
        }

        ++eval_total_tests;
        if (!resultLine.contains(QString::fromUtf8("°"))
            || !resultLine.contains(QString::fromUtf8("′"))
            || !resultLine.contains(QString::fromUtf8("″"))
            || resultLine.contains(QString::fromUtf8("[″]"))
            || resultLine.contains(QString::fromUtf8("1.034"))) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << tc.label << " result line\t[NEW]" << endl
                 << "\tLine     : " << resultLine.toUtf8().constData() << endl
                 << "\tExpected : sexagesimal angle with no implicit [″] or decimal degrees" << endl;
        }
    }

    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
    settings->multipleResultLinesEnabled = oldMultipleResultLinesEnabled;
    settings->resultFormat = oldResultFormat;
    settings->resultPrecision = oldResultPrecision;
    settings->angleUnit = oldAngleUnit;
    Evaluator::instance()->initializeAngleUnits();
    session->clearHistory();
}

void test_result_display_adds_simplified_line_for_unit_conversion_when_interpreted_is_empty()
{
    Settings* settings = Settings::instance();
    Session* session = const_cast<Session*>(eval->session());
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    const char oldUnitExponentStyle = settings->unitNegativeExponentStyle;

    settings->simplifyResultExpressions = true;
    settings->unitNegativeExponentStyle = Settings::UnitNegativeExponentSuperscript;
    setRuntimeUnitNegativeExponentStyle(Settings::UnitNegativeExponentSuperscript);
    session->clearHistory();

    const QString expression =
        QStringLiteral("molmass(C6H12O6) * molmass(C6H12O6) -> [g²/mol²]");
    eval->setExpression(expression);
    const Quantity value = eval->evalUpdateAns();

    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tresult display simplified line for conversion expression\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        // Reproduce the display path where interpreted text may be empty.
        session->addHistoryEntry(HistoryEntry(expression, value, QString()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString line1 = display.document()->findBlockByNumber(1).text();
        const QString line2 = display.document()->findBlockByNumber(2).text();
        const bool hasSimplifiedLine =
            line1.startsWith(QStringLiteral("= "))
            && line1.contains(QString::fromUtf8("molmass²(C6H12O6)"));
        const bool hasFinalResultLine =
            line2.startsWith(QStringLiteral("= "))
            && line2.contains(QString::fromUtf8("32456.184336"))
            && line2.contains(QString::fromUtf8("g²"))
            && line2.contains(QString::fromUtf8("mol⁻²"));
        if (!hasSimplifiedLine || !hasFinalResultLine) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tresult display simplified line for conversion expression\t[NEW]" << endl
                 << "\tText     : " << display.document()->toPlainText().toUtf8().constData() << endl
                 << "\tExpected : simplified molmass² line and final result line" << endl;
        }
    }

    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
    settings->unitNegativeExponentStyle = oldUnitExponentStyle;
    setRuntimeUnitNegativeExponentStyle(oldUnitExponentStyle);
    session->clearHistory();
}

void test_result_display_adds_simplified_line_for_unit_conversion_with_interpreted_expression()
{
    Settings* settings = Settings::instance();
    Session* session = const_cast<Session*>(eval->session());
    const bool oldSimplifyResultExpressions = settings->simplifyResultExpressions;
    const char oldUnitExponentStyle = settings->unitNegativeExponentStyle;

    settings->simplifyResultExpressions = true;
    settings->unitNegativeExponentStyle = Settings::UnitNegativeExponentSuperscript;
    setRuntimeUnitNegativeExponentStyle(Settings::UnitNegativeExponentSuperscript);
    session->clearHistory();

    const QString expression =
        QStringLiteral("molmass(C6H12O6) * molmass(C6H12O6) -> [g²/mol²]");
    eval->setExpression(expression);
    const Quantity value = eval->evalUpdateAns();

    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__
             << "]\tresult display simplified line for conversion with interpreted expression\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
    } else {
        session->addHistoryEntry(HistoryEntry(expression, value, eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        ++eval_total_tests;
        const QString line1 = display.document()->findBlockByNumber(1).text();
        const QString line2 = display.document()->findBlockByNumber(2).text();
        const bool hasSimplifiedLine =
            line1.startsWith(QStringLiteral("= "))
            && line1.contains(QString::fromUtf8("molmass²(C6H12O6)"));
        const bool hasFinalResultLine =
            line2.startsWith(QStringLiteral("= "))
            && line2.contains(QString::fromUtf8("32456.184336"))
            && line2.contains(QString::fromUtf8("g²"))
            && line2.contains(QString::fromUtf8("mol⁻²"));
        if (!hasSimplifiedLine || !hasFinalResultLine) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__
                 << "]\tresult display simplified line for conversion with interpreted expression\t[NEW]" << endl
                 << "\tText     : " << display.document()->toPlainText().toUtf8().constData() << endl
                 << "\tExpected : simplified molmass² line and final result line" << endl;
        }
    }

    settings->simplifyResultExpressions = oldSimplifyResultExpressions;
    settings->unitNegativeExponentStyle = oldUnitExponentStyle;
    setRuntimeUnitNegativeExponentStyle(oldUnitExponentStyle);
    session->clearHistory();
}

void test_result_display_shows_pi_multiples_for_radian_angle_results()
{
    Settings* settings = Settings::instance();
    Session* session = const_cast<Session*>(eval->session());
    const char oldAngleUnit = settings->angleUnit;
    const char oldResultFormat = settings->resultFormat;
    const bool oldSimplify = settings->simplifyResultExpressions;
    const bool oldMultipleResultLinesEnabled = settings->multipleResultLinesEnabled;

    settings->resultFormat = 'g';
    settings->simplifyResultExpressions = false;
    settings->multipleResultLinesEnabled = false;

    auto normalizeDisplaySpaces = [](QString text) {
        text.replace(MathDsl::QuantSp, QLatin1Char(' '));
        text.replace(UnicodeChars::NoBreakSpace, QLatin1Char(' '));
        return text;
    };

    auto checkPiLine = [&](char angleUnit,
                           const QString& expression,
                           const QString& expectedPiLine,
                           const char* label) {
        settings->angleUnit = angleUnit;
        Evaluator::instance()->initializeAngleUnits();
        session->clearHistory();

        eval->setExpression(expression);
        const Quantity value = eval->evalUpdateAns();

        ++eval_total_tests;
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
            return;
        }

        session->addHistoryEntry(HistoryEntry(expression, value, eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        const QString text = display.document()->toPlainText();
        const QString normalizedText = normalizeDisplaySpaces(text);
        const QString normalizedExpected = normalizeDisplaySpaces(expectedPiLine);
        ++eval_total_tests;
        if (!normalizedText.contains(normalizedExpected)) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tExpression: " << expression.toUtf8().constData() << endl
                 << "\tText      : " << text.toUtf8().constData() << endl
                 << "\tExpected  : contains "
                 << expectedPiLine.toUtf8().constData() << endl;
        }
    };

    const QString radSuffix = QString(MathDsl::QuantSp) + Units::angleModeUnitSymbol('r');
    const QString multiplication = QString(MathDsl::MulDotWrapSp)
        + QString(MathDsl::MulDotOp)
        + QString(MathDsl::MulDotWrapSp);
    const QString division = QString(MathDsl::DivWrap)
        + QString(MathDsl::DivOp)
        + QString(MathDsl::DivWrap);

    checkPiLine(
        'r',
        QString::fromUtf8("360°"),
        QStringLiteral("= 2") + multiplication + QString::fromUtf8("π") + radSuffix,
        "result display adds 2·pi rad line for 360 degree input in radian mode");
    checkPiLine(
        'd',
        QString::fromUtf8("180° -> [rad]"),
        QStringLiteral("= ") + QString::fromUtf8("π") + radSuffix,
        "result display adds pi rad line for explicit degree-to-radian conversion");
    checkPiLine(
        'r',
        QString::fromUtf8("270°"),
        QStringLiteral("= 3") + division + QStringLiteral("2")
            + multiplication + QString::fromUtf8("π") + radSuffix,
        "result display adds 3/2·pi rad line for 270 degree input in radian mode");
    checkPiLine(
        'd',
        QStringLiteral("50 [gon] -> [rad]"),
        QStringLiteral("= ") + QString::fromUtf8("π") + division + QStringLiteral("4") + radSuffix,
        "result display adds pi/4 rad line for gradian-to-radian conversion");
    checkPiLine(
        'r',
        QString::fromUtf8("180″"),
        QStringLiteral("= ") + QString::fromUtf8("π") + division + QStringLiteral("3600") + radSuffix,
        "result display adds pi/3600 rad line for arcsecond input in radian mode");
    checkPiLine(
        'r',
        QString::fromUtf8("180°"),
        QStringLiteral("= ") + QString::fromUtf8("π") + radSuffix,
        "result display adds pi rad line for degree input in radian mode");
    checkPiLine(
        'r',
        QString::fromUtf8("180′"),
        QStringLiteral("= ") + QString::fromUtf8("π") + division + QStringLiteral("60") + radSuffix,
        "result display adds pi/60 rad line for arcminute input in radian mode");

    auto checkArctanDedup = [&](const QString& expression,
                                const QString& expectedPiLine,
                                const QString& redundantLine,
                                const char* label) {
        settings->angleUnit = 'r';
        Evaluator::instance()->initializeAngleUnits();
        session->clearHistory();
        eval->setExpression(expression);
        const Quantity arctanValue = eval->evalUpdateAns();
        ++eval_total_tests;
        if (!eval->error().isEmpty()) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tError: " << qPrintable(eval->error()) << endl;
            return;
        }

        session->addHistoryEntry(HistoryEntry(expression, arctanValue, eval->interpretedExpression()));

        TestableResultDisplay display;
        display.resize(800, 600);
        display.refresh();

        const QString text = display.document()->toPlainText();
        const QStringList lines = normalizeDisplaySpaces(text).split(QLatin1Char('\n'));
        const bool hasPiLine = lines.contains(normalizeDisplaySpaces(expectedPiLine));
        const bool hasRedundantLine = lines.contains(normalizeDisplaySpaces(redundantLine));

        ++eval_total_tests;
        if (!hasPiLine || hasRedundantLine) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\t" << label << "\t[NEW]" << endl
                 << "\tText     : " << text.toUtf8().constData() << endl
                 << "\tExpected : contains "
                 << expectedPiLine.toUtf8().constData()
                 << " and does not contain "
                 << redundantLine.toUtf8().constData() << endl;
        }
    };

    checkArctanDedup(
        QStringLiteral("arctan(1)"),
        QStringLiteral("= ") + QString::fromUtf8("π") + division + QStringLiteral("4") + radSuffix,
        QStringLiteral("= ") + QString::fromUtf8("π") + division + QStringLiteral("4"),
        "result display omits redundant trig symbolic line when pi/4 rad line exists");
    checkArctanDedup(
        QString::fromUtf8("arctan(−1)"),
        QStringLiteral("= ") + QString::fromUtf8("−π") + division + QStringLiteral("4") + radSuffix,
        QStringLiteral("= ") + QString::fromUtf8("-π") + division + QStringLiteral("4"),
        "result display omits redundant trig symbolic line when -pi/4 rad line exists");

    settings->angleUnit = oldAngleUnit;
    settings->resultFormat = oldResultFormat;
    settings->simplifyResultExpressions = oldSimplify;
    settings->multipleResultLinesEnabled = oldMultipleResultLinesEnabled;
    Evaluator::instance()->initializeAngleUnits();
    session->clearHistory();
}

void test_result_display_compacts_bracketed_degree_inside_trig_call()
{
    Settings* settings = Settings::instance();
    Session* session = const_cast<Session*>(eval->session());
    const char oldAngleUnit = settings->angleUnit;

    settings->angleUnit = 'r';
    Evaluator::instance()->initializeAngleUnits();
    session->clearHistory();

    const QString expr = QString::fromUtf8("tan(180°)");
    eval->setExpression(expr);
    const Quantity value = eval->evalUpdateAns();

    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate tan(180°) compact-degree display case\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        settings->angleUnit = oldAngleUnit;
        Evaluator::instance()->initializeAngleUnits();
        session->clearHistory();
        return;
    }

    session->addHistoryEntry(HistoryEntry(expr, value, eval->interpretedExpression()));

    TestableResultDisplay display;
    display.resize(800, 600);
    display.refresh();

    const QString expressionLine = display.document()->findBlockByNumber(0).text();
    const QString resultLine = display.document()->findBlockByNumber(1).text();

    ++eval_total_tests;
    if (expressionLine != expr) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tresult display keeps compact degree suffix in trig expression line\t[NEW]" << endl
             << "\tLine     : " << expressionLine.toUtf8().constData() << endl
             << "\tExpected : " << expr.toUtf8().constData() << endl;
    }

    ++eval_total_tests;
    if (!resultLine.startsWith(QStringLiteral("= 0"))) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tresult display tan(180°) numeric line\t[NEW]" << endl
             << "\tLine     : " << resultLine.toUtf8().constData() << endl
             << "\tExpected : starts with '= 0'" << endl;
    }

    settings->angleUnit = oldAngleUnit;
    Evaluator::instance()->initializeAngleUnits();
    session->clearHistory();
}

void test_trig_symbolic_fraction_half()
{
    eval->setExpression(QStringLiteral("cos(pi/3)"));
    const Quantity value = eval->evalUpdateAns();

    ++eval_total_tests;
    if (!eval->error().isEmpty()) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tevaluate cos(pi/3) for trig symbolic fraction\t[NEW]" << endl
             << "\tError: " << qPrintable(eval->error()) << endl;
        return;
    }

    const QString expected = QStringLiteral("1")
        + QString(MathDsl::AddWrap)
        + QStringLiteral("/")
        + QString(MathDsl::AddWrap)
        + QStringLiteral("2");
    const QString symbolic = NumberFormatter::formatTrigSymbolic(value);
    ++eval_total_tests;
    if (symbolic != expected) {
        ++eval_failed_tests;
        ++eval_new_failed_tests;
        cerr << __FILE__ << "[" << __LINE__ << "]\tformat trig symbolic half\t[NEW]" << endl
             << "\tResult   : " << symbolic.toUtf8().constData() << endl
             << "\tExpected : " << expected.toUtf8().constData() << endl;
    }
}

void test_non_informative_numeric_simplified_row_suppression()
{
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "suppress simplified line for 1+2-3/2",
        QStringLiteral("1+2-3/2"), true);
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "suppress simplified line for 1+.5-1.5+3",
        QStringLiteral("1+.5-1.5+3"), true);
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "suppress simplified line for unicode multiply/divide arithmetic",
        QString::fromUtf8("2/3×4"), true);
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "do not suppress symbolic simplification",
        QStringLiteral("2*e*e-1"), false);
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "suppress commutative swap for 1+cos(pi)",
        QStringLiteral("1+cos(pi)"), true);
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "suppress additive reorder for 2*cos(3)+3-cbrt(3)",
        QStringLiteral("2*cos(3)+3-cbrt(3)"), true);
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "do not suppress symbolic reduction with trailing 1 multiplier",
        QStringLiteral("cos (pi)^2/ cos pi  1"), false);
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "do not suppress symbolic reduction with explicit multiplication by 1",
        QStringLiteral("1*cos(pi)^2/cos(pi)"), false);
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("cos (pi)^2/ cos pi  1"),
        QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("1*cos(pi)^2/cos(pi)"),
        QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("2*cos(pi)^2/cos(pi)"),
        QStringLiteral("2")
            + QString(MathDsl::AddWrap)
            + QString(MathDsl::MulDotOp)
            + QString(MathDsl::AddWrap)
            + QStringLiteral("cos(pi)"));
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "do not suppress symbolic reduction when wrapped factor is multiplied",
        QStringLiteral("(cos(pi)^2/cos(pi))*2"), false);
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("(cos(pi)^2/cos(pi))*2"),
        QStringLiteral("2")
            + QString(MathDsl::AddWrap)
            + QString(MathDsl::MulDotOp)
            + QString(MathDsl::AddWrap)
            + QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("(2*cos(pi)/(3*pi*4))*(343+4343)-2*e"),
        QStringLiteral("781")
            + QString(MathDsl::AddWrap)
            + QString(MathDsl::MulDotOp)
            + QString(MathDsl::AddWrap)
            + QStringLiteral("cos(pi)")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("/")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("pi")
            + QString(MathDsl::AddWrap)
            + QString(UnicodeChars::MinusSign)
            + QString(MathDsl::AddWrap)
            + QStringLiteral("2")
            + QString(MathDsl::AddWrap)
            + QString(MathDsl::MulDotOp)
            + QString(MathDsl::AddWrap)
            + QStringLiteral("e"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("cos(pi)^2/cos(pi)"),
        QStringLiteral("cos²(pi)")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("/")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("cos(pi)^2/cos(pi) 1"),
        QStringLiteral("cos²(pi)")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("/")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("cos(pi)")
            + QString(MathDsl::AddWrap)
            + QString(MathDsl::MulDotOp)
            + QString(MathDsl::AddWrap)
            + QStringLiteral("1"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("cos(pi/3)^2^3"),
        QStringLiteral("cos(")
            + QStringLiteral("pi")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("/")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("3)^(2")
            + QString::fromUtf8("³)")
            );
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("cos(pi/3)^2^3"),
        QString::fromUtf8("cos⁸(pi")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("/")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("3)"));
    CHECK_DISPLAY_INTERPRETED(
        QStringLiteral("(cos(pi/3))^2^3"),
        QStringLiteral("cos(")
            + QStringLiteral("pi")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("/")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("3)^(2")
            + QString::fromUtf8("³)")
            );
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("(cos(pi/3))^2^3"),
        QString::fromUtf8("cos⁸(pi")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("/")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("3)"));
    ++eval_total_tests;
    {
        const QString superscriptInput = QString::fromUtf8("cos²(pi/3)³");
        const QString normalized = eval->autoFix(superscriptInput);
        const QString expectedNormalized = QStringLiteral("(cos(pi/3)^2)^3");
        if (normalized != expectedNormalized) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tcos²(pi/3)^3 autofix\t[NEW]" << endl
                 << "\tNormalized: " << normalized.toUtf8().constData() << endl
                 << "\tExpected  : " << expectedNormalized.toUtf8().constData() << endl;
        } else {
            eval->setExpression(normalized);
            eval->evalUpdateAns();
            if (!eval->error().isEmpty()) {
                ++eval_failed_tests;
                ++eval_new_failed_tests;
                cerr << __FILE__ << "[" << __LINE__ << "]\tcos²(pi/3)^3 eval\t[NEW]" << endl
                     << "\tError: " << qPrintable(eval->error()) << endl;
            } else {
                const QString displayed = Evaluator::formatInterpretedExpressionForDisplay(
                    eval->interpretedExpression());
                const QString expectedDisplayed =
                    QString::fromUtf8("(cos²(pi")
                    + QString(MathDsl::AddWrap)
                    + QStringLiteral("/")
                    + QString(MathDsl::AddWrap)
                    + QStringLiteral("3))")
                    + QString::fromUtf8("³");
                const QString expectedSimplified =
                    QString::fromUtf8("cos⁶(pi")
                    + QString(MathDsl::AddWrap)
                    + QStringLiteral("/")
                    + QString(MathDsl::AddWrap)
                    + QStringLiteral("3)");
                const QString simplifiedDisplayed = Evaluator::formatInterpretedExpressionSimplifiedForDisplay(
                    eval->interpretedExpression());
                if (displayed != expectedDisplayed || simplifiedDisplayed != expectedSimplified) {
                    ++eval_failed_tests;
                    ++eval_new_failed_tests;
                    cerr << __FILE__ << "[" << __LINE__ << "]\tcos²(pi/3)^3 display\t[NEW]" << endl
                         << "\tDisplayed: " << displayed.toUtf8().constData() << endl
                         << "\tExpected : " << expectedDisplayed.toUtf8().constData() << endl
                         << "\tSimplif. : " << simplifiedDisplayed.toUtf8().constData() << endl
                         << "\tExpected : " << expectedSimplified.toUtf8().constData() << endl;
                }
            }
        }
    }
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "do not suppress symbolic reduction with -1 multiplier",
        QStringLiteral("-1*cos(pi)^2/cos(pi)+1"), false);
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1*cos(pi)^2/cos(pi)+1"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("cos(pi)")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("+")
            + QString(MathDsl::AddWrap)
            + QStringLiteral("1"));
    checkSuppressSimplifiedExpressionLine(
        __FILE__, __LINE__, "do not suppress symbolic reduction with divide by -1",
        QStringLiteral("-1*cos(pi)^2/cos(pi)/-1"), false);
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1*cos(pi)^2/cos(pi)/-1"),
        QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1*cos(pi)^2/cos(pi)/(-1)"),
        QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1*cos(pi)^2/cos(pi)/1"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1*cos(pi)^2/cos(pi)*1"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1*cos(pi)^2/cos(pi)*(-1)"),
        QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("cos(pi)*(-1)"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("cos(pi)*-1"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1*cos(pi)"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("(-1)*cos(pi)"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("(-1) cos(pi)"),
        QString(UnicodeChars::MinusSign)
            + QStringLiteral("cos(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1*cos(pi)^2*cos(pi)*cos(pi)/cos(pi)^2"),
        QString::fromUtf8("−cos²(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1*cos(pi)^2*cos(pi)*cos(pi)/-cos(pi)^2"),
        QString::fromUtf8("cos²(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("-1*cos(pi)^2*cos(pi)*cos(pi)/(-cos(pi)^2)"),
        QString::fromUtf8("cos²(pi)"));
    CHECK_DISPLAY_SIMPLIFIED_INTERPRETED(
        QStringLiteral("0-1*cos(pi)^2*cos(pi)*cos(pi)/(-cos(pi)^2)"),
        QString::fromUtf8("cos²(pi)"));

    {
        Settings* settings = Settings::instance();
        const char oldAngleUnit = settings->angleUnit;
        settings->angleUnit = 'd';
        Evaluator::instance()->initializeAngleUnits();

        eval->setExpression(QStringLiteral("cos(pi)*cos(pi)*cos(pi)+cos(pi[rad])+cos(pi[rad])"));
        eval->evalUpdateAns();
        const QString interpreted = eval->interpretedExpression();
        const QString simplified = Evaluator::simplifyInterpretedExpression(interpreted);

        ++eval_total_tests;
        if (eval->error().isEmpty() && simplified == interpreted) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tsimplify trig expression with explicit angle unit\t[NEW]" << endl
                 << "\tInterpreted: " << interpreted.toUtf8().constData() << endl
                 << "\tSimplified : " << simplified.toUtf8().constData() << endl;
        }
        const QString interpretedDisplay =
            Evaluator::formatInterpretedExpressionForDisplay(interpreted);
        const QString simplifiedDisplay =
            Evaluator::formatInterpretedExpressionSimplifiedForDisplay(interpreted);
        ++eval_total_tests;
        if (eval->error().isEmpty() && simplifiedDisplay == interpretedDisplay) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tshow simplified display line for trig with explicit angle unit\t[NEW]" << endl
                 << "\tInterpreted: " << interpretedDisplay.toUtf8().constData() << endl
                 << "\tSimplified : " << simplifiedDisplay.toUtf8().constData() << endl;
        }

        settings->angleUnit = oldAngleUnit;
        Evaluator::instance()->initializeAngleUnits();
    }

    {
        Settings* settings = Settings::instance();
        const char oldAngleUnit = settings->angleUnit;
        settings->angleUnit = 'd';
        Evaluator::instance()->initializeAngleUnits();

        const QString expr = QString::fromUtf8(
            "cos(pi)*cos(pi)*cos(pi)+cos(pi[rad])+cos(pi[°])+cos(pi[rad])");
        CHECK_EVAL(expr, "-0.00600462826322460879");

        eval->setExpression(expr);
        eval->evalUpdateAns();
        const QString interpreted = eval->interpretedExpression();
        const QString simplified = Evaluator::simplifyInterpretedExpression(interpreted);

        ++eval_total_tests;
        const QString expectedSimplified =
            QStringLiteral("cos(pi)^3+2")
            + QString(MathDsl::MulDotOp)
            + QString::fromUtf8("cos(pi[rad])+cos(pi[°])");
        if (eval->error().isEmpty() && simplified != expectedSimplified) {
            ++eval_failed_tests;
            ++eval_new_failed_tests;
            cerr << __FILE__ << "[" << __LINE__ << "]\tsimplify mixed explicit angle units in degree mode\t[NEW]" << endl
                 << "\tInterpreted: " << interpreted.toUtf8().constData() << endl
                 << "\tSimplified : " << simplified.toUtf8().constData() << endl
                 << "\tExpected   : " << expectedSimplified.toUtf8().constData() << endl;
        }

        settings->angleUnit = oldAngleUnit;
        Evaluator::instance()->initializeAngleUnits();
    }
}


int main(int argc, char* argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);

    Settings* settings = Settings::instance();
    settings->angleUnit = 'r';
    settings->numberFormatStyle = Settings::NumberFormatNoGroupingDot;
    settings->applyNumberFormatStyle();
    settings->complexNumbers = false;
    settings->imaginaryUnit = 'i';
    settings->resultRoundingMode = Settings::ResultRoundingHalfAwayFromZero;
    setRuntimeResultRoundingMode(settings->resultRoundingMode);
    CMath::setImaginaryUnitSymbol(QLatin1Char('i'));
    DMath::complexMode = false;

    eval = Evaluator::instance();

    eval->initializeBuiltInVariables();

    test_constants();
    test_exponentiation();
    test_unary();
    test_binary();
    test_units();
    test_percent_operator();
    test_bitwise_complement_operator();

    test_divide_by_zero();
    test_radix_char();

    test_thousand_sep();
    test_number_format_decimal_separator();
    test_number_format_styles_matrix();
    test_tolerant_number_input_with_dot_style();
    test_number_format_style_behavior_all_supported_styles();
    test_tolerant_number_input_all_styles();
    test_settings_default_result_line_behavior();
    test_extra_result_lines_profile_formatting();
    test_sexagesimal();
    test_rational_format();

    test_function_basic();
    test_function_trig();
    test_function_stat();
    test_function_logic();
    test_function_discrete();
    test_function_simplified();

    test_auto_fix_parentheses();
    test_auto_fix_ans();
    test_auto_fix_trailing_equal();
    test_auto_fix_powers();
    test_auto_fix_shift_add_sub();
    test_auto_fix_untouch();

    test_comments();
    test_comment_and_description_edge_cases();
    test_result_display_history_mapping_after_multiple_lines_toggle_with_comment_only_entry();

    test_user_functions();
    test_user_units();

    test_implicit_multiplication();
    test_display_interpreted_spacing();

    settings->complexNumbers = true;
    DMath::complexMode = true;
    eval->initializeBuiltInVariables();
    test_complex();
    test_format();
    test_result_rounding_mode_formatting();
    test_datetime();
    test_epoch();
    test_expression_operator_normalization();
    test_session_history_limit();
    test_session_deserialize_without_history();
    test_function_usage_tooltip();
    test_grouped_numeric_literal_display_format();
    test_pasted_standalone_numeric_literal_reformatting();
    test_display_root_aliases_for_result_lines();
    test_display_spacing_stability_for_unit_conversion();
    test_display_conversion_with_unicode_spaces_and_cross_units();
    test_preserve_brackets_for_displayed_conversion_target();
    test_preserve_brackets_for_displayed_conversion_target_without_source_hint();
    test_result_display_preserves_conversion_target_brackets();
    test_result_display_adds_normalized_sexagesimal_simplification_line();
    test_result_display_adds_normalized_sexagesimal_simplification_line_for_arithmetic();
    test_result_display_preserves_fractional_seconds_in_normalized_sexagesimal_line();
    test_result_display_normalized_sexagesimal_line_with_time_conversion_target();
    test_sexagesimal_arithmetic_with_per_term_conversion_targets();
    test_result_display_mixed_per_term_time_conversions();
    test_result_display_mixed_per_term_time_conversions_in_sexagesimal_notation();
    test_result_display_strips_unit_brackets_and_double_click_restores_canonical_unit_expression();
    test_result_display_uses_base10_exponent_notation();
    test_result_display_omits_zero_power_of_ten_generically();
    test_result_display_double_click_preserves_compact_angle_suffix();
    test_result_display_shows_angle_mode_unit_suffix_for_explicit_angle_input();
    test_result_display_keeps_quantsp_before_degree_celsius_and_fahrenheit();
    test_value_unit_separator_normalization();
    test_result_display_preserves_standalone_sexagesimal_angles_without_implicit_conversion();
    test_result_display_adds_simplified_line_for_unit_conversion_when_interpreted_is_empty();
    test_result_display_adds_simplified_line_for_unit_conversion_with_interpreted_expression();
    test_result_display_shows_pi_multiples_for_radian_angle_results();
    test_result_display_compacts_bracketed_degree_inside_trig_call();
    test_trig_symbolic_fraction_half();
    test_non_informative_numeric_simplified_row_suppression();

    test_angle_mode(settings);

    if (!eval_failed_tests)
        return 0;

    cout << eval_total_tests  << " total, "
         << eval_failed_tests << " failed, "
         << eval_new_failed_tests << " new" << endl;
    return eval_new_failed_tests;
}
