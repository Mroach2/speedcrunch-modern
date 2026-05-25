// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GUI_THEMEDLINEEDIT_H
#define GUI_THEMEDLINEEDIT_H

#include <QColor>
#include <QLineEdit>

class QFocusEvent;
class QPaintEvent;
class QTimer;

class ThemedLineEdit : public QLineEdit {
public:
    explicit ThemedLineEdit(QWidget* parent = nullptr);

    QColor cursorColor() const { return m_cursorColor; }
    void setCursorColor(const QColor& color);

protected:
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void showCursorAndRestartBlink();
    void hideCursorAndStopBlink();
    QRect themedCursorRect() const;

    QColor m_cursorColor;
    QTimer* m_cursorBlinkTimer;
    bool m_cursorVisible = false;
};

#endif
