// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_DOCKLISTSTYLE_H
#define GUI_DOCKLISTSTYLE_H

class QAbstractItemView;
class QLabel;

namespace DockListStyle {

void apply(QAbstractItemView* view);
void showCenteredNoMatchLabel(QAbstractItemView* view, QLabel* label);

} // namespace DockListStyle

#endif // GUI_DOCKLISTSTYLE_H
