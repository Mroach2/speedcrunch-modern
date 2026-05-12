// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_RESULTSLOTSDIALOG_H
#define GUI_RESULTSLOTSDIALOG_H

#include <QDialog>
#include <array>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QSpinBox;

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

    int currentSlotIndex() const;
    void loadFromSettings();
    void saveCurrentUiToSlot(int slotIndex);
    void loadSlotToUi(int slotIndex);
    void applyToSettings();
    void updateAdvancedModeUi(bool advanced);
    void updatePrecisionLabel();

    QComboBox* m_slot;
    QComboBox* m_notation;
    QCheckBox* m_enabled;
    QCheckBox* m_autoPrecision;
    QSpinBox* m_precision;
    QLabel* m_precisionLabel;
    QGroupBox* m_selectorGroup;
    QGroupBox* m_settingsGroup;
    QCheckBox* m_advancedMode;
    std::array<SlotSettings, 5> m_slots;
    int m_activeSlotIndex = 0;
};

#endif // GUI_RESULTSLOTSDIALOG_H
