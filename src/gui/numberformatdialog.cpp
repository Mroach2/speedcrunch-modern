// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/numberformatdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLayout>
#include <QVBoxLayout>

NumberFormatDialog::NumberFormatDialog(QWidget* parent)
    : QDialog(parent)
    , m_styles(new QComboBox(this))
{
    setWindowTitle(tr("Number Format"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    QLabel* intro = new QLabel(
        tr("<b>Select the number format for display.</b><br/><br/>Note: for input numbers, obvious formats are accepted even when they differ from the selected display format.<br/>"),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    auto addStyle = [this](Settings::NumberFormatStyle style, const QString& example) {
        m_styleItems.append(style);
        m_styles->addItem(example);
    };

    addStyle(Settings::NumberFormatNoGroupingDot, QStringLiteral("1234567.12345"));
    addStyle(Settings::NumberFormatNoGroupingComma, QStringLiteral("1234567,12345"));
    addStyle(Settings::NumberFormatThreeDigitCommaDot, QStringLiteral("1,234,567.12345"));
    addStyle(Settings::NumberFormatThreeDigitDotComma, QStringLiteral("1.234.567,12345"));
    addStyle(Settings::NumberFormatThreeDigitSpaceDot, QStringLiteral("1 234 567.12345"));
    addStyle(Settings::NumberFormatThreeDigitSpaceComma, QStringLiteral("1 234 567,12345"));
    addStyle(Settings::NumberFormatSIDot, QStringLiteral("1 234 567.123 45"));
    addStyle(Settings::NumberFormatSIComma, QStringLiteral("1 234 567,123 45"));
    addStyle(Settings::NumberFormatThreeDigitCommaDotFraction, QStringLiteral("1,234,567.123,45"));
    addStyle(Settings::NumberFormatThreeDigitDotCommaFraction, QStringLiteral("1.234.567,123.45"));
    addStyle(Settings::NumberFormatThreeDigitUnderscoreDot, QStringLiteral("1_234_567.12345"));
    addStyle(Settings::NumberFormatThreeDigitUnderscoreComma, QStringLiteral("1_234_567,12345"));
    addStyle(Settings::NumberFormatThreeDigitUnderscoreDotFraction, QStringLiteral("1_234_567.123_45"));
    addStyle(Settings::NumberFormatThreeDigitUnderscoreCommaFraction, QStringLiteral("1_234_567,123_45"));
    addStyle(Settings::NumberFormatIndianCommaDot, QStringLiteral("12,34,567.12345"));
    layout->addWidget(m_styles);

    QDialogButtonBox* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_styles, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0 && index < m_styleItems.size())
            emit selectionChanged(m_styleItems.at(index));
    });
}

void NumberFormatDialog::setSelection(Settings::NumberFormatStyle style)
{
    const int index = m_styleItems.indexOf(style);
    if (index >= 0)
        m_styles->setCurrentIndex(index);
    else
        m_styles->setCurrentIndex(0);
}

Settings::NumberFormatStyle NumberFormatDialog::selectedStyle() const
{
    const int index = m_styles->currentIndex();
    if (index < 0 || index >= m_styleItems.size())
        return Settings::NumberFormatNoGroupingDot;
    return m_styleItems.at(index);
}
