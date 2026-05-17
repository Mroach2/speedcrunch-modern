// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/notationandprecisiondialog.h"

#include "core/settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace {
char notationFromComboData(const QVariant& data)
{
    const QString value = data.toString();
    return value.isEmpty() ? '\0' : value.at(0).toLatin1();
}

QLabel* createHeaderLabel(const QString& text, QWidget* parent)
{
    QLabel* label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

QComboBox* createNotationCombo(QWidget* parent)
{
    QComboBox* combo = new QComboBox(parent);
    combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    combo->addItem(QObject::tr("Automatic decimal"), QStringLiteral("g"));
    combo->addItem(QObject::tr("Fixed-point decimal"), QStringLiteral("f"));
    combo->addItem(QObject::tr("Engineering decimal"), QStringLiteral("n"));
    combo->addItem(QObject::tr("Scientific decimal"), QStringLiteral("e"));
    combo->insertSeparator(combo->count());
    combo->addItem(QObject::tr("Rational"), QStringLiteral("r"));
    combo->addItem(QObject::tr("Binary"), QStringLiteral("b"));
    combo->addItem(QObject::tr("Octal"), QStringLiteral("o"));
    combo->addItem(QObject::tr("Hexadecimal"), QStringLiteral("h"));
    combo->addItem(QObject::tr("Sexagesimal"), QStringLiteral("s"));
    return combo;
}

}

ResultSlotsDialog::ResultSlotsDialog(QWidget* parent)
    : QDialog(parent)
    , m_table(new QWidget(this))
{
    buildDialog(tr("Notation & Precision"));
    loadFromSettings();
    loadRowsToUi();
    finalizeSize();
}

ResultSlotsDialog::ResultSlotsDialog(const QString& title, const EvaluationContext& context, QWidget* parent)
    : QDialog(parent)
    , m_table(new QWidget(this))
    , m_applyToSettings(false)
{
    buildDialog(title);
    loadFromEvaluationContext(context);
    loadRowsToUi();
    finalizeSize();
}

void ResultSlotsDialog::buildDialog(const QString& title)
{
    setWindowTitle(title);
    QVBoxLayout* root = new QVBoxLayout(this);
    createTable();
    root->addWidget(m_table);

    QDialogButtonBox* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        saveRowsToSlots();
        if (m_applyToSettings) {
            applyToSettings();
            emit settingsApplied();
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ResultSlotsDialog::finalizeSize()
{
    m_table->setFixedSize(m_table->sizeHint());
    setFixedSize(sizeHint());
}

void ResultSlotsDialog::createTable()
{
    QGridLayout* grid = new QGridLayout(m_table);
    grid->setColumnStretch(0, 0);
    grid->setColumnStretch(1, 0);
    grid->setColumnStretch(2, 0);
    grid->setColumnStretch(3, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);

    grid->addWidget(createHeaderLabel(tr("Result Line"), m_table), 0, 0);
    grid->addWidget(createHeaderLabel(tr("Enabled"), m_table), 0, 1);
    grid->addWidget(createHeaderLabel(tr("Notation"), m_table), 0, 2);
    grid->addWidget(createHeaderLabel(tr("Decimal Places"), m_table), 0, 3);

    const QStringList rowNames = {
        tr("Main Line"),
        tr("Extra Line #1"),
        tr("Extra Line #2"),
        tr("Extra Line #3"),
        tr("Extra Line #4")
    };

    for (int row = 0; row < 5; ++row) {
        const int gridRow = row + 1;
        grid->addWidget(new QLabel(rowNames.at(row), m_table), gridRow, 0);

        RowWidgets widgets;
        widgets.enabled = new QCheckBox(m_table);
        widgets.enabled->setChecked(row == 0);
        widgets.enabled->setEnabled(row != 0);
        grid->addWidget(widgets.enabled, gridRow, 1, Qt::AlignCenter);

        widgets.notation = createNotationCombo(m_table);
        grid->addWidget(widgets.notation, gridRow, 2);

        QWidget* precisionWidget = new QWidget(m_table);
        QHBoxLayout* precisionLayout = new QHBoxLayout(precisionWidget);
        precisionLayout->setContentsMargins(0, 0, 0, 0);
        widgets.autoPrecision = new QCheckBox(tr("Auto"), precisionWidget);
        widgets.precision = new QSpinBox(precisionWidget);
        widgets.precision->setRange(0, 50);
        precisionLayout->addWidget(widgets.autoPrecision);
        precisionLayout->addWidget(widgets.precision);
        grid->addWidget(precisionWidget, gridRow, 3);

        m_rows[row] = widgets;

        connect(widgets.enabled, &QCheckBox::toggled, this, [this, row](bool enabled) {
            setRowControlsEnabled(row, enabled);
        });
        connect(widgets.autoPrecision, &QCheckBox::toggled, this, [this, row](bool automatic) {
            m_rows[row].precision->setEnabled(m_rows[row].enabled->isChecked() && !automatic);
        });
    }
}

void ResultSlotsDialog::loadFromSettings()
{
    Settings* settings = Settings::instance();
    m_slots[0].notation = settings->resultFormat;
    m_slots[0].precision = settings->resultPrecision;
    m_slots[0].complexForm = settings->resultComplexForm;
    m_slots[0].enabled = true;

    m_slots[1].notation = settings->alternativeResultFormat;
    m_slots[1].precision = settings->secondaryResultPrecision;
    m_slots[1].complexForm = settings->secondaryResultComplexForm;
    m_slots[1].enabled = settings->secondaryResultEnabled;

    m_slots[2].notation = settings->tertiaryResultFormat;
    m_slots[2].precision = settings->tertiaryResultPrecision;
    m_slots[2].complexForm = settings->tertiaryResultComplexForm;
    m_slots[2].enabled = settings->tertiaryResultEnabled;

    m_slots[3].notation = settings->quaternaryResultFormat;
    m_slots[3].precision = settings->quaternaryResultPrecision;
    m_slots[3].complexForm = settings->quaternaryResultComplexForm;
    m_slots[3].enabled = settings->quaternaryResultEnabled;

    m_slots[4].notation = settings->quinaryResultFormat;
    m_slots[4].precision = settings->quinaryResultPrecision;
    m_slots[4].complexForm = settings->quinaryResultComplexForm;
    m_slots[4].enabled = settings->quinaryResultEnabled;
}

void ResultSlotsDialog::loadFromEvaluationContext(const EvaluationContext& context)
{
    m_slots[0].notation = context.main.fmt;
    m_slots[0].precision = context.main.prec;
    m_slots[0].complexForm = context.main.cplx;
    m_slots[0].enabled = true;

    for (int i = 1; i < 5; ++i) {
        m_slots[i] = SlotSettings();
        m_slots[i].enabled = (i - 1) < context.extras.size();
        if (m_slots[i].enabled) {
            const ResultLineContext& line = context.extras.at(i - 1);
            m_slots[i].notation = line.fmt;
            m_slots[i].precision = line.prec;
            m_slots[i].complexForm = line.cplx;
        }
    }
}

void ResultSlotsDialog::loadRowsToUi()
{
    for (int row = 0; row < 5; ++row) {
        const SlotSettings& source = m_slots[row];
        RowWidgets& widgets = m_rows[row];

        widgets.enabled->setChecked(row == 0 ? true : source.enabled);
        const int notationIndex = widgets.notation->findData(QString(QChar(source.notation)));
        widgets.notation->setCurrentIndex(notationIndex >= 0
            ? notationIndex
            : widgets.notation->findData(QStringLiteral("g")));

        widgets.autoPrecision->setChecked(source.precision < 0);
        widgets.precision->setValue(source.precision < 0 ? 8 : source.precision);

        setRowControlsEnabled(row, widgets.enabled->isChecked());
    }
}

void ResultSlotsDialog::saveRowsToSlots()
{
    for (int row = 0; row < 5; ++row) {
        const RowWidgets& widgets = m_rows[row];
        SlotSettings& target = m_slots[row];
        target.enabled = (row == 0) ? true : widgets.enabled->isChecked();
        target.notation = notationFromComboData(widgets.notation->currentData());
        target.precision = widgets.autoPrecision->isChecked() ? -1 : widgets.precision->value();
    }
}

void ResultSlotsDialog::applyToSettings()
{
    Settings* settings = Settings::instance();
    settings->multipleResultLinesEnabled =
        m_slots[1].enabled || m_slots[2].enabled || m_slots[3].enabled || m_slots[4].enabled;
    settings->complexNumbers = true;
    settings->resultFormat = m_slots[0].notation;
    settings->resultPrecision = m_slots[0].precision;
    settings->resultComplexForm = m_slots[0].complexForm;

    settings->alternativeResultFormat = m_slots[1].notation;
    settings->secondaryResultEnabled = m_slots[1].enabled;
    settings->secondaryResultPrecision = m_slots[1].precision;
    settings->secondaryComplexNumbers = true;
    settings->secondaryResultComplexForm = m_slots[1].complexForm;

    settings->tertiaryResultFormat = m_slots[2].notation;
    settings->tertiaryResultEnabled = m_slots[2].enabled;
    settings->tertiaryResultPrecision = m_slots[2].precision;
    settings->tertiaryComplexNumbers = true;
    settings->tertiaryResultComplexForm = m_slots[2].complexForm;

    settings->quaternaryResultFormat = m_slots[3].notation;
    settings->quaternaryResultEnabled = m_slots[3].enabled;
    settings->quaternaryResultPrecision = m_slots[3].precision;
    settings->quaternaryComplexNumbers = true;
    settings->quaternaryResultComplexForm = m_slots[3].complexForm;

    settings->quinaryResultFormat = m_slots[4].notation;
    settings->quinaryResultEnabled = m_slots[4].enabled;
    settings->quinaryResultPrecision = m_slots[4].precision;
    settings->quinaryComplexNumbers = true;
    settings->quinaryResultComplexForm = m_slots[4].complexForm;
}

EvaluationContext ResultSlotsDialog::evaluationContext(const EvaluationContext& baseContext) const
{
    EvaluationContext context = baseContext;
    context.main.fmt = m_slots[0].notation;
    context.main.prec = m_slots[0].precision;
    context.main.cplx = m_slots[0].complexForm;
    context.complexOn = true;
    context.extras.clear();

    for (int i = 1; i < 5; ++i) {
        if (!m_slots[i].enabled)
            continue;
        ResultLineContext line;
        line.fmt = m_slots[i].notation;
        line.prec = m_slots[i].precision;
        line.cplx = m_slots[i].complexForm;
        context.extras.append(line);
    }

    return context;
}

void ResultSlotsDialog::setRowControlsEnabled(int row, bool enabled)
{
    RowWidgets& widgets = m_rows[row];
    widgets.notation->setEnabled(enabled);
    widgets.autoPrecision->setEnabled(enabled);
    widgets.precision->setEnabled(enabled && !widgets.autoPrecision->isChecked());
}
