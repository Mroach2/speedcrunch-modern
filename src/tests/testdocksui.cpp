// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/evaluator.h"
#include "core/session.h"
#include "core/settings.h"
#include "gui/constantswidget.h"
#include "gui/splittertreeutils.h"
#include "gui/userunitlistwidget.h"
#include "gui/variablelistwidget.h"

#include <QSignalSpy>
#include <QTest>
#include <QCoreApplication>
#include <QTranslator>
#include <QSplitter>
#include <QTreeWidget>

class ConstantsTestTranslator : public QTranslator {
public:
    QString translate(const char* context,
                      const char* sourceText,
                      const char* disambiguation = nullptr,
                      int n = -1) const override
    {
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);

        if (qstrcmp(context, "Constants") == 0
            && qstrcmp(sourceText, "pi (π)") == 0) {
            return QStringLiteral("translated pi");
        }

        return QString();
    }
};

class TestableConstantsWidget : public ConstantsWidget {
public:
    using ConstantsWidget::handleItem;
    using ConstantsWidget::updateList;
};

class TestDocksUi : public QObject {
    Q_OBJECT

private slots:
    void constants_dock_inserts_pi_symbol_with_translated_name();
    void user_units_dock_shows_rhs_and_description_after_definition();
    void user_variables_dock_keeps_existing_value_text_after_new_definition();
    void splitter_normalization_removes_single_child_nested_splitter_after_pane_close_shape();
};

void TestDocksUi::constants_dock_inserts_pi_symbol_with_translated_name()
{
    ConstantsTestTranslator translator;
    QCoreApplication::installTranslator(&translator);

    TestableConstantsWidget widget;
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.updateList();

    QTreeWidget* tree = widget.findChild<QTreeWidget*>();
    QVERIFY(tree != nullptr);

    QTreeWidgetItem* piItem = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = tree->topLevelItem(i);
        if (item && item->text(0) == QStringLiteral("translated pi")) {
            piItem = item;
            break;
        }
    }

    QVERIFY(piItem != nullptr);

    QSignalSpy spy(&widget, &ConstantsWidget::constantSelected);
    widget.handleItem(piItem);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.takeFirst().at(0).toString(), QString::fromUtf8("π"));

    QCoreApplication::removeTranslator(&translator);
}

void TestDocksUi::user_units_dock_shows_rhs_and_description_after_definition()
{
    Session session;
    Evaluator* evaluator = session.evaluator();
    evaluator->unsetAllUserUnits();

    evaluator->setExpression(QStringLiteral("[cm_s] = 2 [cm/s] ? speed alias"));
    const Quantity result = evaluator->eval();
    QVERIFY(!result.isNan());

    UserUnitListWidget widget;
    widget.setEvaluator(evaluator);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.updateList();

    QTreeWidget* tree = widget.findChild<QTreeWidget*>();
    QVERIFY(tree != nullptr);
    QCOMPARE(tree->topLevelItemCount(), 1);

    QTreeWidgetItem* item = tree->topLevelItem(0);
    QVERIFY(item != nullptr);
    QCOMPARE(item->text(0), QStringLiteral("cm_s"));
    QCOMPARE(item->text(1), QStringLiteral("2[cm/s]"));
    QCOMPARE(item->text(2), QStringLiteral("speed alias"));
}

void TestDocksUi::user_variables_dock_keeps_existing_value_text_after_new_definition()
{
    Session session;
    Evaluator* evaluator = session.evaluator();
    Settings* settings = Settings::instance();

    evaluator->unsetAllUserDefinedVariables();

    const char oldResultFormat = settings->resultFormat;
    const int oldResultPrecision = settings->resultPrecision;

    settings->resultFormat = 'f';
    settings->resultPrecision = 0;
    evaluator->setExpression(QStringLiteral("v_fixed = 100000"));
    QVERIFY(!evaluator->eval().isNan());

    VariableListWidget widget;
    widget.setEvaluator(evaluator);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));
    widget.updateList();

    QTreeWidget* tree = widget.findChild<QTreeWidget*>();
    QVERIFY(tree != nullptr);

    QTreeWidgetItem* fixedItem = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = tree->topLevelItem(i);
        if (item && item->text(0) == QStringLiteral("v_fixed")) {
            fixedItem = item;
            break;
        }
    }
    QVERIFY(fixedItem != nullptr);
    const QString fixedValueBefore = fixedItem->text(1);
    QVERIFY(!fixedValueBefore.isEmpty());

    settings->resultFormat = 'e';
    settings->resultPrecision = 2;
    evaluator->setExpression(QStringLiteral("v_sci = 100000"));
    QVERIFY(!evaluator->eval().isNan());

    widget.updateList();

    fixedItem = nullptr;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = tree->topLevelItem(i);
        if (item && item->text(0) == QStringLiteral("v_fixed")) {
            fixedItem = item;
            break;
        }
    }
    QVERIFY(fixedItem != nullptr);
    QCOMPARE(fixedItem->text(1), fixedValueBefore);

    settings->resultFormat = oldResultFormat;
    settings->resultPrecision = oldResultPrecision;
}

void TestDocksUi::splitter_normalization_removes_single_child_nested_splitter_after_pane_close_shape()
{
    QSplitter root(Qt::Horizontal);
    QWidget* leftPane = new QWidget();
    QSplitter* nested = new QSplitter(Qt::Vertical);
    QWidget* topPane = new QWidget();
    QWidget* bottomPane = new QWidget();

    nested->addWidget(topPane);
    nested->addWidget(bottomPane);
    root.addWidget(leftPane);
    root.addWidget(nested);

    // Simulate closing one pane from a nested splitter.
    topPane->setParent(nullptr);
    topPane->deleteLater();
    QCoreApplication::processEvents();

    normalizeSplitterTree(&root);
    QCoreApplication::processEvents();

    QCOMPARE(root.count(), 2);
    QVERIFY(root.widget(0) != nullptr);
    QVERIFY(root.widget(1) != nullptr);
    QVERIFY(qobject_cast<QSplitter*>(root.widget(0)) == nullptr);
    QVERIFY(qobject_cast<QSplitter*>(root.widget(1)) == nullptr);
}

int main(int argc, char** argv)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    TestDocksUi test;
    return QTest::qExec(&test, argc, argv);
}

#include "testdocksui.moc"
