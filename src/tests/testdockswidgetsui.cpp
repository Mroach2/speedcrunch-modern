// This file is part of the SpeedCrunch project
// Copyright (C) 2026 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "core/evaluator.h"
#include "gui/userunitlistwidget.h"

#include <QTest>
#include <QTreeWidget>

class TestDocksWidgetsUi : public QObject {
    Q_OBJECT

private slots:
    void user_units_dock_shows_rhs_and_description_after_definition();
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

QTEST_MAIN(TestDocksWidgetsUi)
#include "testdockswidgetsui.moc"
