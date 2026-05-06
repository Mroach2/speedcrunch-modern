// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/evaluator.h"
#include "core/settings.h"
#include "gui/splittertreeutils.h"
#include "gui/userunitlistwidget.h"
#include "gui/variablelistwidget.h"

#include <QTest>
#include <QCoreApplication>
#include <QSplitter>
#include <QTreeWidget>

class TestDocksWidgetsUi : public QObject {
    Q_OBJECT

private slots:
    void user_units_dock_shows_rhs_and_description_after_definition();
    void user_variables_dock_keeps_existing_value_text_after_new_definition();
    void splitter_normalization_removes_single_child_nested_splitter_after_pane_close_shape();
};

void TestDocksWidgetsUi::user_units_dock_shows_rhs_and_description_after_definition()
{
    Evaluator* evaluator = Evaluator::instance();
    evaluator->unsetAllUserUnits();

    evaluator->setExpression(QStringLiteral("[cm_s] = 2 [cm/s] ? speed alias"));
    const Quantity result = evaluator->eval();
    QVERIFY(!result.isNan());

    UserUnitListWidget widget;
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

void TestDocksWidgetsUi::user_variables_dock_keeps_existing_value_text_after_new_definition()
{
    Evaluator* evaluator = Evaluator::instance();
    Settings* settings = Settings::instance();

    evaluator->unsetAllUserDefinedVariables();

    const char oldResultFormat = settings->resultFormat;
    const int oldResultPrecision = settings->resultPrecision;

    settings->resultFormat = 'f';
    settings->resultPrecision = 0;
    evaluator->setExpression(QStringLiteral("v_fixed = 100000"));
    QVERIFY(!evaluator->eval().isNan());

    VariableListWidget widget;
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

void TestDocksWidgetsUi::splitter_normalization_removes_single_child_nested_splitter_after_pane_close_shape()
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

QTEST_MAIN(TestDocksWidgetsUi)
#include "testdockswidgetsui.moc"
