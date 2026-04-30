// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_NUMBERFORMATDIALOG_H
#define GUI_NUMBERFORMATDIALOG_H

#include "core/settings.h"

#include <QDialog>
#include <QVector>

class QComboBox;

class NumberFormatDialog : public QDialog {
    Q_OBJECT

public:
    explicit NumberFormatDialog(QWidget* parent = nullptr);

    void setSelection(Settings::NumberFormatStyle style);
    Settings::NumberFormatStyle selectedStyle() const;

signals:
    void selectionChanged(Settings::NumberFormatStyle style);

private:
    QComboBox* m_styles;
    QVector<Settings::NumberFormatStyle> m_styleItems;
};

#endif // GUI_NUMBERFORMATDIALOG_H
