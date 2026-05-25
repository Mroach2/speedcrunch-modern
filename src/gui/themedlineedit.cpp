// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/themedlineedit.h"

#include <QApplication>
#include <QFocusEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QProxyStyle>
#include <QStyle>
#include <QStyleOptionFrame>
#include <QTimer>
#include <QVariant>

namespace {
const char* kHideNativeCursorProperty = "speedcrunchHideNativeTextCursor";
constexpr int kThemedCursorWidth = 2;
constexpr int kThemedCursorXOffset = 1;

class ThemedLineEditStyle : public QProxyStyle {
public:
    int pixelMetric(PixelMetric metric,
                    const QStyleOption* option = nullptr,
                    const QWidget* widget = nullptr) const override
    {
        if (metric == QStyle::PM_TextCursorWidth
            && dynamic_cast<const ThemedLineEdit*>(widget) != nullptr
            && widget->property(kHideNativeCursorProperty).toBool()) {
            return 0;
        }
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
};
}

ThemedLineEdit::ThemedLineEdit(QWidget* parent)
    : QLineEdit(parent)
    , m_cursorBlinkTimer(new QTimer(this))
{
    QStyle* cursorStyle = new ThemedLineEditStyle;
    cursorStyle->setParent(this);
    setStyle(cursorStyle);

    m_cursorBlinkTimer->setSingleShot(false);
    connect(m_cursorBlinkTimer, &QTimer::timeout, this, [this]() {
        m_cursorVisible = !m_cursorVisible;
        update(themedCursorRect());
    });
    connect(this, &QLineEdit::cursorPositionChanged, this, [this]() {
        showCursorAndRestartBlink();
    });
    connect(this, &QLineEdit::textEdited, this, [this]() {
        showCursorAndRestartBlink();
    });
}

void ThemedLineEdit::setCursorColor(const QColor& color)
{
    if (m_cursorColor == color)
        return;

    m_cursorColor = color;
    update();
}

void ThemedLineEdit::focusInEvent(QFocusEvent* event)
{
    QLineEdit::focusInEvent(event);
    showCursorAndRestartBlink();
}

void ThemedLineEdit::focusOutEvent(QFocusEvent* event)
{
    hideCursorAndStopBlink();
    QLineEdit::focusOutEvent(event);
}

void ThemedLineEdit::paintEvent(QPaintEvent* event)
{
    setProperty(kHideNativeCursorProperty, true);
    QLineEdit::paintEvent(event);
    setProperty(kHideNativeCursorProperty, QVariant());

    if (!hasFocus() || !m_cursorVisible || !m_cursorColor.isValid())
        return;

    const QRect cursorRect = themedCursorRect();
    QPainter painter(this);
    painter.setPen(Qt::NoPen);
    painter.fillRect(cursorRect, m_cursorColor);
}

void ThemedLineEdit::showCursorAndRestartBlink()
{
    m_cursorVisible = true;
    if (hasFocus()) {
        const int cursorFlashTime = QApplication::cursorFlashTime();
        const int blinkInterval = cursorFlashTime > 0 ? qMax(1, cursorFlashTime / 2) : 500;
        m_cursorBlinkTimer->start(blinkInterval);
    }
    update();
}

void ThemedLineEdit::hideCursorAndStopBlink()
{
    m_cursorBlinkTimer->stop();
    m_cursorVisible = false;
    update(themedCursorRect());
}

QRect ThemedLineEdit::themedCursorRect() const
{
    const QRect nativeRect = inputMethodQuery(Qt::ImCursorRectangle).toRect();
    if (nativeRect.isValid() && nativeRect.height() > 0) {
        return QRect(nativeRect.x() + (nativeRect.width() - kThemedCursorWidth) / 2 + kThemedCursorXOffset,
                     nativeRect.y(),
                     kThemedCursorWidth,
                     nativeRect.height());
    }

    QStyleOptionFrame option;
    initStyleOption(&option);
    const QRect contentRect = style()->subElementRect(QStyle::SE_LineEditContents, &option, this);
    const int height = qMax(1, contentRect.height() - 4);
    return QRect(contentRect.left(),
                 contentRect.top() + (contentRect.height() - height) / 2,
                 kThemedCursorWidth,
                 height);
}
