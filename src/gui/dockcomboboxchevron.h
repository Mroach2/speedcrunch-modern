// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_DOCKCOMBOBOXCHEVRON_H
#define GUI_DOCKCOMBOBOXCHEVRON_H

#include <QColor>
#include <QPointer>
#include <QWidget>

class QAbstractItemView;
class QComboBox;
class QEvent;
class QPaintEvent;
class QVariantAnimation;

class DockComboBoxChevron : public QWidget {
public:
    static constexpr int IndicatorWidth = 28;

    explicit DockComboBoxChevron(QComboBox* comboBox);

    static void apply(QComboBox* comboBox, const QColor& textColor, const QColor& outlineColor);

    void setColors(QColor chevronColor, const QColor& outlineColor);
    void refresh();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void installPopupEventFilters();
    void reposition();
    void setPopupOpen(bool open);

    QPointer<QComboBox> m_comboBox;
    QPointer<QAbstractItemView> m_view;
    QPointer<QWidget> m_popupWindow;
    QVariantAnimation* m_animation;
    QColor m_chevronColor;
    QColor m_outlineColor;
    qreal m_rotation = 0.0;
    bool m_popupOpen = false;
};

#endif // GUI_DOCKCOMBOBOXCHEVRON_H
