// SPDX-FileCopyrightText: 2007-2010, 2013-2014, 2021, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_ABOUTBOX_H
#define GUI_ABOUTBOX_H

#include <QDialog>

class AboutBox : public QDialog {
    Q_OBJECT

public:
    explicit AboutBox(QWidget *parent = 0, Qt::WindowFlags f = QFlag(0));

private:
    Q_DISABLE_COPY(AboutBox)
};

#endif // GUI_ABOUTBOX_H
