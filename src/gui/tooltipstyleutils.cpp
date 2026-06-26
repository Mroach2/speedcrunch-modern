// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/tooltipstyleutils.h"

#include "gui/uiconfig.h"

#include <QBitmap>
#include <QFrame>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPalette>
#include <QScreen>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace ToolTipStyleUtils {

void applyRoundedPopupMask(QWidget* popup, int cornerRadius)
{
    if (popup == nullptr)
        return;

    if (cornerRadius > 0) {
        QBitmap mask(popup->size());
        mask.fill(Qt::color0);
        QPainter painter(&mask);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::color1);
        painter.drawRoundedRect(QRectF(mask.rect()).adjusted(0, 0, -1, -1),
                                cornerRadius,
                                cornerRadius);
        popup->setMask(mask);
    } else {
        popup->clearMask();
    }
}

QPoint constrainedPopupPosition(QWidget* anchor,
                                const QPoint& globalPos,
                                const QSize& popupSize)
{
    constexpr int kPopupOffsetX = 12;
    constexpr int kPopupOffsetY = 18;

    QPoint pos = globalPos + QPoint(kPopupOffsetX, kPopupOffsetY);
    QScreen* screen = anchor != nullptr ? anchor->screen() : QGuiApplication::primaryScreen();
    if (screen == nullptr)
        return pos;

    const QRect available = screen->availableGeometry();
    if (pos.x() + popupSize.width() > available.right())
        pos.setX(globalPos.x() - popupSize.width() - kPopupOffsetX);
    if (pos.y() + popupSize.height() > available.bottom())
        pos.setY(globalPos.y() - popupSize.height() - kPopupOffsetY);

    pos.setX(qBound(available.left(), pos.x(),
                    qMax(available.left(), available.right() - popupSize.width())));
    pos.setY(qBound(available.top(), pos.y(),
                    qMax(available.top(), available.bottom() - popupSize.height())));
    return pos;
}

QFrame* createPopup(QWidget* parent,
                    const QString& popupObjectName,
                    const QString& labelObjectName,
                    Qt::TextFormat textFormat,
                    QLabel** label,
                    bool transparentForMouseEvents,
                    const QMargins& contentsMargins)
{
    QFrame* popup = new QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint);
    popup->setObjectName(popupObjectName);
    popup->setAutoFillBackground(true);
    popup->setAttribute(Qt::WA_ShowWithoutActivating, true);
    popup->setAttribute(Qt::WA_StyledBackground, true);
    popup->setAttribute(Qt::WA_TransparentForMouseEvents, transparentForMouseEvents);
    popup->setFocusPolicy(Qt::NoFocus);
    popup->setFrameStyle(QFrame::NoFrame);

    QVBoxLayout* layout = new QVBoxLayout(popup);
    layout->setContentsMargins(contentsMargins);
    layout->setSpacing(0);

    QLabel* popupLabel = new QLabel(popup);
    popupLabel->setObjectName(labelObjectName);
    popupLabel->setTextFormat(textFormat);
    popupLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    popupLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(popupLabel);

    if (label != nullptr)
        *label = popupLabel;
    return popup;
}

void applyPopupTheme(QFrame* popup,
                     QLabel* label,
                     QWidget* paletteSource,
                     const PopupTheme& theme)
{
    if (popup == nullptr || label == nullptr)
        return;

    const QPalette sourcePalette = paletteSource != nullptr ? paletteSource->palette() : popup->palette();
    const QColor background = theme.background.isValid()
        ? theme.background
        : sourcePalette.color(QPalette::ToolTipBase);
    const QColor foreground = theme.foreground.isValid()
        ? theme.foreground
        : sourcePalette.color(QPalette::ToolTipText);
    const QColor outline = theme.outline.isValid()
        ? theme.outline
        : background;
    const int cornerRadius = qMax(0, theme.cornerRadius);

    QPalette popupPalette = popup->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        popupPalette.setColor(group, QPalette::Window, background);
        popupPalette.setColor(group, QPalette::WindowText, foreground);
    }
    popup->setPalette(popupPalette);

    QPalette labelPalette = label->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        labelPalette.setColor(group, QPalette::WindowText, foreground);
        labelPalette.setColor(group, QPalette::Text, foreground);
    }
    label->setPalette(labelPalette);

    popup->setStyleSheet(QStringLiteral(
        "QFrame#%1 {"
        " background: %3; color: %4;"
        " border: %6px solid %5;"
        " border-radius: %7px;"
        "}"
        "QLabel#%2 {"
        " background: transparent; color: %4;"
        "}")
                             .arg(popup->objectName(),
                                  label->objectName(),
                                  background.name(),
                                  foreground.name(),
                                  outline.name())
                             .arg(UiConfig::PopupOutlineStrokeWidth)
                             .arg(cornerRadius));
    applyRoundedPopupMask(popup, cornerRadius);
}

void applyTreePopupTheme(QTreeWidget* popup, const TreePopupTheme& theme)
{
    if (popup == nullptr || !theme.background.isValid() || !theme.foreground.isValid())
        return;

    const QColor scrollbarThumb = theme.scrollbarThumb.isValid()
        ? theme.scrollbarThumb
        : theme.background;
    const QColor scrollbarThumbForeground = theme.scrollbarThumbForeground.isValid()
        ? theme.scrollbarThumbForeground
        : theme.foreground;
    const QColor selectedRow = theme.selectedRow.isValid()
        ? theme.selectedRow
        : scrollbarThumb;
    const QColor selectedRowForeground = theme.selectedRowForeground.isValid()
        ? theme.selectedRowForeground
        : scrollbarThumbForeground;
    const QColor outline = theme.outline.isValid()
        ? theme.outline
        : theme.background;
    const int cornerRadius = qMax(0, theme.cornerRadius);

    QPalette palette = popup->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        palette.setColor(group, QPalette::Base, theme.background);
        palette.setColor(group, QPalette::Window, theme.background);
        palette.setColor(group, QPalette::Text, theme.foreground);
        palette.setColor(group, QPalette::WindowText, theme.foreground);
        palette.setColor(group, QPalette::Highlight, selectedRow);
        palette.setColor(group, QPalette::HighlightedText, selectedRowForeground);
    }
    popup->setPalette(palette);
    popup->viewport()->setPalette(palette);
    popup->viewport()->setAutoFillBackground(true);
    popup->setCursor(Qt::PointingHandCursor);
    popup->viewport()->setCursor(Qt::PointingHandCursor);

    popup->setStyleSheet(QStringLiteral(
        "QTreeWidget {"
        " background: %1; color: %2;"
        " selection-background-color: %3; selection-color: %4;"
        " border: %8px solid %6;"
        " border-radius: %7px;"
        "}"
        "QTreeWidget::item:selected {"
        " background: %3; color: %4;"
        "}"
        "QScrollBar:vertical {"
        " background: %1; border: 0; margin: 0; width: 10px;"
        "}"
        "QScrollBar:horizontal {"
        " background: %1; border: 0; margin: 0; height: 10px;"
        "}"
        "QScrollBar::handle:vertical {"
        " background: %5; border: 0; border-radius: 4px; min-height: 20px;"
        "}"
        "QScrollBar::handle:horizontal {"
        " background: %5; border: 0; border-radius: 4px; min-width: 20px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        " background: %1; border: 0; width: 0; height: 0;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical,"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {"
        " background: %1;"
        "}")
        .arg(theme.background.name(),
             theme.foreground.name(),
             selectedRow.name(),
             selectedRowForeground.name(),
             scrollbarThumb.name(),
             outline.name())
        .arg(cornerRadius)
        .arg(UiConfig::PopupOutlineStrokeWidth));
}

QString labelToolTipStyleSheet(const QString& selector,
                               const QColor& background,
                               const QColor& foreground,
                               const QColor& outline,
                               int cornerRadius)
{
    return QStringLiteral(
        "%1 {"
        " background-color: %2;"
        " color: %3;"
        " border: %6px solid %4;"
        " border-radius: %5px;"
        "}")
        .arg(selector,
             background.name(),
             foreground.name(),
             outline.name())
        .arg(qMax(0, cornerRadius))
        .arg(UiConfig::PopupOutlineStrokeWidth);
}

void showPopup(QFrame* popup,
               QLabel* label,
               const QString& text,
               QWidget* anchor,
               const QPoint& globalPos,
               int cornerRadius)
{
    if (popup == nullptr || label == nullptr || text.isEmpty() || anchor == nullptr)
        return;

    label->setText(text);
    popup->adjustSize();
    popup->resize(popup->sizeHint());
    applyRoundedPopupMask(popup, qMax(0, cornerRadius));
    popup->move(constrainedPopupPosition(anchor, globalPos, popup->size()));
    popup->show();
}

}
