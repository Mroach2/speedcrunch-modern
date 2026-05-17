// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_RESULTSLOTSDIALOG_H
#define GUI_RESULTSLOTSDIALOG_H

#include "core/sessionhistory.h"

#include <QDialog>
#include <array>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QWidget;

class ResultSlotsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ResultSlotsDialog(QWidget* parent = nullptr);
    ResultSlotsDialog(const QString& title, const EvaluationContext& context, QWidget* parent = nullptr);
    EvaluationContext evaluationContext(const EvaluationContext& baseContext) const;

signals:
    void settingsApplied();

private:
    struct SlotSettings {
        char notation = '\0';
        int precision = -1;
        char complexForm = ComplexForm::Default;
        bool enabled = true;
    };

    struct RowWidgets {
        QCheckBox* enabled = nullptr;
        QComboBox* notation = nullptr;
        QCheckBox* autoPrecision = nullptr;
        QSpinBox* precision = nullptr;
    };

    void buildDialog(const QString& title);
    void finalizeSize();
    void createTable();
    void loadFromSettings();
    void loadFromEvaluationContext(const EvaluationContext& context);
    void loadRowsToUi();
    void saveRowsToSlots();
    void applyToSettings();
    void setRowControlsEnabled(int row, bool enabled);

    QWidget* m_table;
    bool m_applyToSettings = true;
    std::array<SlotSettings, 5> m_slots;
    std::array<RowWidgets, 5> m_rows;
};

#endif // GUI_RESULTSLOTSDIALOG_H
