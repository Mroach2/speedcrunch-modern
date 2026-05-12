// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_RESULTSLOTSDIALOG_H
#define GUI_RESULTSLOTSDIALOG_H

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

signals:
    void settingsApplied();

private:
    struct SlotSettings {
        char notation = '\0';
        int precision = -1;
        bool enabled = true;
    };

    struct RowWidgets {
        QCheckBox* enabled = nullptr;
        QComboBox* notation = nullptr;
        QCheckBox* autoPrecision = nullptr;
        QSpinBox* precision = nullptr;
    };

    void createTable();
    void loadFromSettings();
    void loadRowsToUi();
    void saveRowsToSlots();
    void applyToSettings();
    void setRowControlsEnabled(int row, bool enabled);

    QWidget* m_table;
    std::array<SlotSettings, 5> m_slots;
    std::array<RowWidgets, 5> m_rows;
};

#endif // GUI_RESULTSLOTSDIALOG_H
