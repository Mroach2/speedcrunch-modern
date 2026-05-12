// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/notationandprecisiondialog.h"

#include "core/settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
char notationFromComboData(const QVariant& data)
{
    const QString value = data.toString();
    return value.isEmpty() ? '\0' : value.at(0).toLatin1();
}

bool isDecimalNotation(char notation)
{
    return notation == 'g' || notation == 'f' || notation == 'n' || notation == 'e';
}
}

ResultSlotsDialog::ResultSlotsDialog(QWidget* parent)
    : QDialog(parent)
    , m_slot(new QComboBox(this))
    , m_notation(new QComboBox(this))
    , m_enabled(new QCheckBox(tr("Enable this result line"), this))
    , m_autoPrecision(new QCheckBox(tr("Automatic"), this))
    , m_precision(new QSpinBox(this))
    , m_precisionLabel(new QLabel(this))
    , m_selectorGroup(new QGroupBox(tr("Result Line"), this))
    , m_settingsGroup(new QGroupBox(tr("Result Configuration"), this))
    , m_advancedMode(new QCheckBox(tr("Advanced mode: show multiple result lines"), this))
{
    setWindowTitle(tr("Notation & Precision"));

    QVBoxLayout* root = new QVBoxLayout(this);

    QVBoxLayout* selectorLayout = new QVBoxLayout(m_selectorGroup);
    QFormLayout* selectorForm = new QFormLayout();
    selectorLayout->addLayout(selectorForm);

    m_slot->addItem(tr("Main Line"));
    m_slot->addItem(tr("Extra Line #1"));
    m_slot->addItem(tr("Extra Line #2"));
    m_slot->addItem(tr("Extra Line #3"));
    m_slot->addItem(tr("Extra Line #4"));
    selectorForm->addRow(tr("Configure:"), m_slot);
    root->addWidget(m_selectorGroup);

    QVBoxLayout* settingsLayout = new QVBoxLayout(m_settingsGroup);
    QFormLayout* form = new QFormLayout();
    settingsLayout->addLayout(form);
    root->addWidget(m_settingsGroup);

    m_notation->addItem(tr("Automatic decimal"), QStringLiteral("g"));
    m_notation->addItem(tr("Fixed-point decimal"), QStringLiteral("f"));
    m_notation->addItem(tr("Engineering decimal"), QStringLiteral("n"));
    m_notation->addItem(tr("Scientific decimal"), QStringLiteral("e"));
    m_notation->insertSeparator(m_notation->count());
    m_notation->addItem(tr("Rational"), QStringLiteral("r"));
    m_notation->addItem(tr("Binary"), QStringLiteral("b"));
    m_notation->addItem(tr("Octal"), QStringLiteral("o"));
    m_notation->addItem(tr("Hexadecimal"), QStringLiteral("h"));
    m_notation->addItem(tr("Sexagesimal"), QStringLiteral("s"));
    form->addRow(QString(), m_enabled);
    form->addRow(tr("Notation:"), m_notation);

    m_precision->setRange(0, 50);
    m_precision->setValue(8);
    QHBoxLayout* precisionRow = new QHBoxLayout();
    precisionRow->addWidget(m_autoPrecision);
    precisionRow->addWidget(m_precision);
    form->addRow(m_precisionLabel, precisionRow);

    root->addWidget(m_advancedMode);

    QDialogButtonBox* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);

    connect(m_autoPrecision, &QCheckBox::toggled, this, [this](bool checked) {
        m_precision->setEnabled(!checked);
    });
    connect(m_notation, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
        updatePrecisionLabel();
    });
    connect(m_enabled, &QCheckBox::toggled, this, [this](bool enabled) {
        const bool isMain = (m_activeSlotIndex == 0);
        const bool allowEditing = isMain || enabled;
        m_notation->setEnabled(allowEditing);
        m_autoPrecision->setEnabled(allowEditing);
        m_precision->setEnabled(allowEditing && !m_autoPrecision->isChecked());
    });
    connect(m_slot, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        const int previousSlot = m_activeSlotIndex;
        m_activeSlotIndex = qBound(0, index, 4);
        if (previousSlot != m_activeSlotIndex)
            saveCurrentUiToSlot(previousSlot);
        loadSlotToUi(m_activeSlotIndex);
    });
    connect(m_advancedMode, &QCheckBox::toggled, this, [this](bool advanced) {
        saveCurrentUiToSlot(m_activeSlotIndex);
        if (!advanced) {
            m_activeSlotIndex = 0;
            m_slot->setCurrentIndex(0);
            loadSlotToUi(0);
        }
        updateAdvancedModeUi(advanced);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        saveCurrentUiToSlot(m_activeSlotIndex);
        applyToSettings();
        emit settingsApplied();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadFromSettings();
    m_activeSlotIndex = 0;
    m_slot->setCurrentIndex(0);
    loadSlotToUi(0);
    m_advancedMode->setChecked(Settings::instance()->multipleResultLinesEnabled);
    updateAdvancedModeUi(m_advancedMode->isChecked());
}

int ResultSlotsDialog::currentSlotIndex() const
{
    const int index = m_slot->currentIndex();
    return qBound(0, index, 4);
}

void ResultSlotsDialog::loadFromSettings()
{
    Settings* settings = Settings::instance();
    m_slots[0].notation = settings->resultFormat;
    m_slots[0].precision = settings->resultPrecision;
    m_slots[0].enabled = true;

    m_slots[1].notation = settings->alternativeResultFormat;
    m_slots[1].precision = settings->secondaryResultPrecision;
    m_slots[1].enabled = settings->secondaryResultEnabled;

    m_slots[2].notation = settings->tertiaryResultFormat;
    m_slots[2].precision = settings->tertiaryResultPrecision;
    m_slots[2].enabled = settings->tertiaryResultEnabled;

    m_slots[3].notation = settings->quaternaryResultFormat;
    m_slots[3].precision = settings->quaternaryResultPrecision;
    m_slots[3].enabled = settings->quaternaryResultEnabled;

    m_slots[4].notation = settings->quinaryResultFormat;
    m_slots[4].precision = settings->quinaryResultPrecision;
    m_slots[4].enabled = settings->quinaryResultEnabled;
}

void ResultSlotsDialog::saveCurrentUiToSlot(int slotIndex)
{
    const int slot = qBound(0, slotIndex, 4);
    SlotSettings& target = m_slots[slot];
    target.notation = notationFromComboData(m_notation->currentData());
    target.enabled = (slot == 0) ? true : m_enabled->isChecked();
    target.precision = m_autoPrecision->isChecked() ? -1 : m_precision->value();
}

void ResultSlotsDialog::loadSlotToUi(int slotIndex)
{
    const int slot = qBound(0, slotIndex, 4);
    const SlotSettings& source = m_slots[slot];
    const bool isMain = (slot == 0);
    m_enabled->setVisible(!isMain);
    m_enabled->setChecked(isMain ? true : source.enabled);
    const int notationIndex = m_notation->findData(QString(QChar(source.notation)));
    m_notation->setCurrentIndex(notationIndex >= 0 ? notationIndex : m_notation->findData(QStringLiteral("g")));

    m_autoPrecision->setChecked(source.precision < 0);
    m_precision->setValue(source.precision < 0 ? 8 : source.precision);
    m_precision->setEnabled(source.precision >= 0);

    const bool allowEditing = isMain || source.enabled;
    m_notation->setEnabled(allowEditing);
    m_autoPrecision->setEnabled(allowEditing);
    m_precision->setEnabled(allowEditing && source.precision >= 0);
    updatePrecisionLabel();
}

void ResultSlotsDialog::applyToSettings()
{
    Settings* settings = Settings::instance();
    settings->multipleResultLinesEnabled = m_advancedMode->isChecked();
    settings->complexNumbers = true;
    settings->resultFormat = m_slots[0].notation;
    settings->resultPrecision = m_slots[0].precision;
    const char complexFormat = settings->resultFormatComplex;

    settings->alternativeResultFormat = m_slots[1].notation;
    settings->secondaryResultEnabled = m_slots[1].enabled;
    settings->secondaryResultPrecision = m_slots[1].precision;
    settings->secondaryComplexNumbers = true;
    settings->secondaryResultFormatComplex = complexFormat;

    settings->tertiaryResultFormat = m_slots[2].notation;
    settings->tertiaryResultEnabled = m_slots[2].enabled;
    settings->tertiaryResultPrecision = m_slots[2].precision;
    settings->tertiaryComplexNumbers = true;
    settings->tertiaryResultFormatComplex = complexFormat;

    settings->quaternaryResultFormat = m_slots[3].notation;
    settings->quaternaryResultEnabled = m_slots[3].enabled;
    settings->quaternaryResultPrecision = m_slots[3].precision;
    settings->quaternaryComplexNumbers = true;
    settings->quaternaryResultFormatComplex = complexFormat;

    settings->quinaryResultFormat = m_slots[4].notation;
    settings->quinaryResultEnabled = m_slots[4].enabled;
    settings->quinaryResultPrecision = m_slots[4].precision;
    settings->quinaryComplexNumbers = true;
    settings->quinaryResultFormatComplex = complexFormat;
}

void ResultSlotsDialog::updateAdvancedModeUi(bool advanced)
{
    m_selectorGroup->setVisible(advanced);
    adjustSize();
}

void ResultSlotsDialog::updatePrecisionLabel()
{
    const char notation = notationFromComboData(m_notation->currentData());
    const QString label = isDecimalNotation(notation)
        ? tr("Decimal places:")
        : tr("Fractional digits:");
    m_precisionLabel->setText(label);
}
