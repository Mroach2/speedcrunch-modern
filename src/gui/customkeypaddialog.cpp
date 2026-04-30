// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/customkeypaddialog.h"
#include "gui/keypad.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {
const int ColumnIndexRow = 0;
const int ColumnIndexColumn = 1;
const int ColumnIndexLabel = 2;
const int ColumnIndexAction = 3;
const int ColumnIndexText = 4;
const int TableColumns = 5;

QString actionText(Settings::CustomKeypadButtonAction action)
{
    switch (action) {
    case Settings::CustomKeypadActionBackspace:
        return QObject::tr("Backspace");
    case Settings::CustomKeypadActionClearExpression:
        return QObject::tr("Clear expression");
    case Settings::CustomKeypadActionEvaluateExpression:
        return QObject::tr("Evaluate expression");
    case Settings::CustomKeypadActionInsertText:
    default:
        return QObject::tr("Insert text");
    }
}

Settings::CustomKeypad presetKeypad(int layout)
{
    Settings::CustomKeypad keypad;
    Keypad::LayoutMode layoutMode = Keypad::LayoutModeBasicWide;
    if (layout == 1)
        layoutMode = Keypad::LayoutModeScientificWide;
    else if (layout == 2)
        layoutMode = Keypad::LayoutModeScientificNarrow;

    const QList<Keypad::CustomButtonDescription> presetButtons = Keypad::presetCustomButtons(
        layoutMode, Settings::instance()->radixCharacter(), &keypad.rows, &keypad.columns);
    for (const auto& presetButton : presetButtons) {
        Settings::CustomKeypadButton button;
        button.row = presetButton.row;
        button.column = presetButton.column;
        button.label = presetButton.label;
        button.text = presetButton.text;
        button.action = static_cast<Settings::CustomKeypadButtonAction>(presetButton.action);
        keypad.buttons.append(button);
    }

    return keypad;
}
}

CustomKeypadDialog::CustomKeypadDialog(const Settings::CustomKeypad& keypad, QWidget* parent)
    : QDialog(parent)
    , m_rowsSpin(new QSpinBox(this))
    , m_columnsSpin(new QSpinBox(this))
    , m_table(new QTableWidget(this))
    , m_currentRows(1)
    , m_currentColumns(1)
{
    setWindowTitle(tr("Custom Keypad"));
    resize(860, 520);

    m_rowsSpin->setRange(1, 20);
    m_columnsSpin->setRange(1, 20);

    const int initialRows = qBound(1, keypad.rows, 20);
    const int initialColumns = qBound(1, keypad.columns, 20);
    m_currentRows = initialRows;
    m_currentColumns = initialColumns;
    m_rowsSpin->setValue(initialRows);
    m_columnsSpin->setValue(initialColumns);

    m_cells = QList<Cell>(initialRows * initialColumns);
    for (const auto& button : keypad.buttons) {
        if (button.row < 0 || button.row >= initialRows || button.column < 0 || button.column >= initialColumns)
            continue;
        const int index = cellIndex(button.row, button.column);
        m_cells[index].label = button.label;
        m_cells[index].text = button.text;
        m_cells[index].action = button.action;
    }

    QGridLayout* topControlsLayout = new QGridLayout;
    topControlsLayout->addWidget(new QLabel(tr("Rows:"), this), 0, 0);
    topControlsLayout->addWidget(m_rowsSpin, 0, 1);
    topControlsLayout->addWidget(new QLabel(tr("Columns:"), this), 0, 2);
    topControlsLayout->addWidget(m_columnsSpin, 0, 3);
    topControlsLayout->addWidget(new QLabel(tr("Copy preset:"), this), 0, 4);
    QComboBox* presetCombo = new QComboBox(this);
    presetCombo->addItem(tr("Basic"), PresetLayoutBasicWide);
    presetCombo->addItem(tr("Scientific (wide)"), PresetLayoutScientificWide);
    presetCombo->addItem(tr("Scientific (narrow)"), PresetLayoutScientificNarrow);
    topControlsLayout->addWidget(presetCombo, 0, 5);
    auto* presetApplyButton = new QPushButton(tr("Apply"), this);
    topControlsLayout->addWidget(presetApplyButton, 0, 6);
    topControlsLayout->setColumnStretch(7, 1);

    m_table->setColumnCount(TableColumns);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);

    QStringList labels;
    labels << tr("Row") << tr("Column") << tr("Label") << tr("Behavior") << tr("Text");
    m_table->setHorizontalHeaderLabels(labels);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_rowsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        rebuildCellStorage(value, m_columnsSpin->value());
    });
    connect(m_columnsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int value) {
        rebuildCellStorage(m_rowsSpin->value(), value);
    });
    connect(presetApplyButton, &QPushButton::clicked, this, [this, presetCombo]() {
        applyPresetLayout(static_cast<PresetLayout>(presetCombo->currentData().toInt()));
    });

    QVBoxLayout* rootLayout = new QVBoxLayout(this);
    rootLayout->addLayout(topControlsLayout);
    rootLayout->addWidget(m_table);
    rootLayout->addWidget(buttons);

    buildTableFromCells();
}

Settings::CustomKeypad CustomKeypadDialog::customKeypad() const
{
    Settings::CustomKeypad keypad;
    keypad.rows = m_rowsSpin->value();
    keypad.columns = m_columnsSpin->value();

    const QList<Cell> cells = readCellsFromTable();
    for (int row = 0; row < keypad.rows; ++row) {
        for (int column = 0; column < keypad.columns; ++column) {
            const Cell& cell = cells.at(cellIndex(row, column));
            if (cell.label.isEmpty() && cell.text.isEmpty() && cell.action == Settings::CustomKeypadActionInsertText)
                continue;

            Settings::CustomKeypadButton button;
            button.row = row;
            button.column = column;
            button.label = cell.label;
            button.text = cell.text;
            button.action = cell.action;
            keypad.buttons.append(button);
        }
    }

    return keypad;
}

int CustomKeypadDialog::cellIndex(int row, int column) const
{
    return row * m_currentColumns + column;
}

void CustomKeypadDialog::buildTableFromCells()
{
    const int rows = m_rowsSpin->value();
    const int columns = m_columnsSpin->value();
    const int entries = rows * columns;

    m_table->setRowCount(entries);

    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const int index = row * columns + column;
            const Cell& cell = m_cells.at(index);

            auto* rowItem = new QTableWidgetItem(QString::number(row + 1));
            rowItem->setFlags(rowItem->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(index, ColumnIndexRow, rowItem);

            auto* columnItem = new QTableWidgetItem(QString::number(column + 1));
            columnItem->setFlags(columnItem->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(index, ColumnIndexColumn, columnItem);

            m_table->setItem(index, ColumnIndexLabel, new QTableWidgetItem(cell.label));

            QComboBox* actionCombo = new QComboBox(m_table);
            actionCombo->addItem(actionText(Settings::CustomKeypadActionInsertText), Settings::CustomKeypadActionInsertText);
            actionCombo->addItem(actionText(Settings::CustomKeypadActionBackspace), Settings::CustomKeypadActionBackspace);
            actionCombo->addItem(actionText(Settings::CustomKeypadActionClearExpression), Settings::CustomKeypadActionClearExpression);
            actionCombo->addItem(actionText(Settings::CustomKeypadActionEvaluateExpression), Settings::CustomKeypadActionEvaluateExpression);
            const int comboIndex = actionCombo->findData(cell.action);
            actionCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : 0);
            m_table->setCellWidget(index, ColumnIndexAction, actionCombo);

            auto* textItem = new QTableWidgetItem(cell.text);
            if (cell.action != Settings::CustomKeypadActionInsertText)
                textItem->setFlags(textItem->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(index, ColumnIndexText, textItem);

            connect(actionCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, index, actionCombo]() {
                QTableWidgetItem* textItem = m_table->item(index, ColumnIndexText);
                if (!textItem)
                    return;
                const auto action = static_cast<Settings::CustomKeypadButtonAction>(actionCombo->currentData().toInt());
                Qt::ItemFlags flags = textItem->flags();
                if (action == Settings::CustomKeypadActionInsertText)
                    flags |= Qt::ItemIsEditable;
                else
                    flags &= ~Qt::ItemIsEditable;
                textItem->setFlags(flags);
            });
        }
    }

    m_table->resizeColumnsToContents();
}

void CustomKeypadDialog::rebuildCellStorage(int rows, int columns)
{
    const QList<Cell> oldCells = readCellsFromTable();
    const int oldRows = m_currentRows;
    const int oldColumns = m_currentColumns;

    QList<Cell> newCells(rows * columns);
    const int copyRows = qMin(rows, oldRows);
    const int copyColumns = qMin(columns, oldColumns);
    for (int row = 0; row < copyRows; ++row) {
        for (int column = 0; column < copyColumns; ++column)
            newCells[row * columns + column] = oldCells.at(row * oldColumns + column);
    }

    m_cells = newCells;
    m_currentRows = rows;
    m_currentColumns = columns;
    buildTableFromCells();
}

void CustomKeypadDialog::applyPresetLayout(CustomKeypadDialog::PresetLayout presetLayout)
{
    const Settings::CustomKeypad preset = presetKeypad(static_cast<int>(presetLayout));
    {
        const QSignalBlocker rowBlocker(m_rowsSpin);
        const QSignalBlocker columnBlocker(m_columnsSpin);
        m_rowsSpin->setValue(preset.rows);
        m_columnsSpin->setValue(preset.columns);
    }

    m_currentRows = preset.rows;
    m_currentColumns = preset.columns;
    m_cells = QList<Cell>(preset.rows * preset.columns);
    for (const auto& button : preset.buttons) {
        if (button.row < 0 || button.row >= preset.rows || button.column < 0 || button.column >= preset.columns)
            continue;
        const int index = cellIndex(button.row, button.column);
        m_cells[index].label = button.label;
        m_cells[index].text = button.text;
        m_cells[index].action = button.action;
    }

    buildTableFromCells();
}

QList<CustomKeypadDialog::Cell> CustomKeypadDialog::readCellsFromTable() const
{
    const int rows = m_rowsSpin->value();
    const int columns = m_columnsSpin->value();
    const int entries = rows * columns;
    QList<Cell> cells(entries);

    for (int index = 0; index < entries; ++index) {
        const auto* labelItem = m_table->item(index, ColumnIndexLabel);
        const auto* textItem = m_table->item(index, ColumnIndexText);
        const auto* actionCombo = qobject_cast<QComboBox*>(m_table->cellWidget(index, ColumnIndexAction));

        cells[index].label = labelItem ? labelItem->text() : QString();
        cells[index].text = textItem ? textItem->text() : QString();
        cells[index].action = actionCombo
            ? static_cast<Settings::CustomKeypadButtonAction>(actionCombo->currentData().toInt())
            : Settings::CustomKeypadActionInsertText;
    }

    return cells;
}
