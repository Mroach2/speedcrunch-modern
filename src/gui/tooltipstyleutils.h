// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_TOOLTIPSTYLEUTILS_H
#define GUI_TOOLTIPSTYLEUTILS_H

#include <QColor>
#include <QMargins>
#include <QPoint>
#include <QString>
#include <Qt>

class QLabel;
class QFrame;
class QSize;
class QTreeWidget;
class QWidget;

namespace ToolTipStyleUtils {

struct PopupTheme {
    QColor background;
    QColor foreground;
    QColor outline;
    int cornerRadius = 0;
};

struct TreePopupTheme {
    QColor background;
    QColor foreground;
    QColor scrollbarThumb;
    QColor scrollbarThumbForeground;
    QColor selectedRow;
    QColor selectedRowForeground;
    QColor outline;
    int cornerRadius = 0;
};

void applyRoundedPopupMask(QWidget* popup, int cornerRadius);
void applyPopupTheme(QFrame* popup,
                     QLabel* label,
                     QWidget* paletteSource,
                     const PopupTheme& theme);
void applyTreePopupTheme(QTreeWidget* popup, const TreePopupTheme& theme);
QPoint constrainedPopupPosition(QWidget* anchor,
                                const QPoint& globalPos,
                                const QSize& popupSize);
QFrame* createPopup(QWidget* parent,
                    const QString& popupObjectName,
                    const QString& labelObjectName,
                    Qt::TextFormat textFormat,
                    QLabel** label,
                    bool transparentForMouseEvents = false,
                    const QMargins& contentsMargins = QMargins(6, 4, 6, 4));
QString labelToolTipStyleSheet(const QString& selector,
                               const QColor& background,
                               const QColor& foreground,
                               const QColor& outline,
                               int cornerRadius);
void showPopup(QFrame* popup,
               QLabel* label,
               const QString& text,
               QWidget* anchor,
               const QPoint& globalPos,
               int cornerRadius);

}

#endif
