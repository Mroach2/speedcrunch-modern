// SPDX-FileCopyrightText: 2014-2017, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "bitfieldwidget.h"

#include "core/mathdsl.h"
#include "gui/uiconfig.h"
#include "math/quantity.h"

#include <cmath>

#include <QBitmap>
#include <QEnterEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListIterator>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QVBoxLayout>

namespace {

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

QString bitFieldButtonStyleSheet(const QColor& background,
                                 const QColor& foreground,
                                 const QColor& hoverBackground,
                                 const QColor& hoverForeground,
                                 const QColor& pressedBackground,
                                 const QColor& pressedForeground)
{
    return QStringLiteral(
        "QPushButton {"
        " border: none;"
        " border-radius: 5px;"
        " padding: 2px;"
        " background-color: %1;"
        " color: %2;"
        "}"
        "QPushButton:hover {"
        " border: none;"
        " background-color: %3;"
        " color: %4;"
        "}"
        "QPushButton:pressed {"
        " border: none;"
        " background-color: %5;"
        " color: %6;"
        "}")
        .arg(background.name(),
             foreground.name(),
             hoverBackground.name(),
             hoverForeground.name(),
             pressedBackground.name(),
             pressedForeground.name());
}

} // namespace

BitWidget::BitWidget(int bitPosition, QWidget* parent)
    : QLabel(parent),
    m_state(false)
{
    HNumber number(HMath::raise(HNumber(2), bitPosition));
    m_toolTipText = QString("2<sup>%1</sup> = %2")
        .arg(bitPosition)
        .arg(HMath::format(number, Quantity::Format::Decimal()));

    setText(QString("%1").arg(bitPosition));
    setObjectName("BitWidget");
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true);
    updateStyle();
}

void BitWidget::setState(bool state)
{
    if (state != m_state) {
        m_state = state;
        setProperty("bitState", m_state);
        updateStyle();
        update();
    }
}

void BitWidget::updateStyle()
{
    const QPalette palette = this->palette();
    const QColor background = m_themeBackground.isValid()
        ? m_themeBackground
        : palette.color(QPalette::Window);
    const QColor foreground = m_themeForeground.isValid()
        ? m_themeForeground
        : palette.color(QPalette::WindowText);
    const QColor hoverBackground = m_themeHoverBackground.isValid()
        ? m_themeHoverBackground
        : palette.color(QPalette::Highlight);
    const QColor hoverForeground = m_themeHoverForeground.isValid()
        ? m_themeHoverForeground
        : palette.color(QPalette::HighlightedText);
    const QColor selectedBackground = m_themeSelectedBackground.isValid()
        ? m_themeSelectedBackground
        : palette.color(QPalette::Mid);
    const QColor selectedForeground = m_themeSelectedForeground.isValid()
        ? m_themeSelectedForeground
        : palette.color(QPalette::ButtonText);
    setProperty("bitState", m_state);
    setProperty("bitPressed", m_pressed);
    setStyleSheet(
        QString("QLabel { background-color: %1; color: %2; }"
                "QLabel[bitState=\"true\"], QLabel[bitPressed=\"true\"] {"
                " background-color: %5; color: %6;"
                "}"
                "QLabel:hover { background-color: %3; color: %4; }"
                // Keep selected/pressed bits visually selected when they also match :hover.
                "QLabel[bitState=\"true\"]:hover, QLabel[bitPressed=\"true\"]:hover {"
                " background-color: %5; color: %6;"
                "}")
            .arg(background.name(),
                 foreground.name(),
                 hoverBackground.name(),
                 hoverForeground.name(),
                 selectedBackground.name(),
                 selectedForeground.name())
    );
}

void BitWidget::setThemeColors(const QColor& background,
                               const QColor& foreground,
                               const QColor& hoverBackground,
                               const QColor& hoverForeground,
                               const QColor& selectedBackground,
                               const QColor& selectedForeground)
{
    m_themeBackground = background;
    m_themeForeground = foreground;
    m_themeHoverBackground = hoverBackground;
    m_themeHoverForeground = hoverForeground;
    m_themeSelectedBackground = selectedBackground;
    m_themeSelectedForeground = selectedForeground;
    updateStyle();
}

void BitWidget::setToolTipThemeColors(const QColor& background,
                                      const QColor& foreground,
                                      const QColor& outline,
                                      int cornerRadius)
{
    m_summaryPopupBackgroundColor = background;
    m_summaryPopupForegroundColor = foreground;
    m_summaryPopupOutlineColor = outline;
    m_summaryPopupCornerRadius = qMax(0, cornerRadius);
    applySummaryPopupTheme();
}

void BitWidget::enterEvent(QEnterEvent* event)
{
    QLabel::enterEvent(event);
    showSummaryPopup(event->globalPosition().toPoint());
}

void BitWidget::leaveEvent(QEvent* event)
{
    QLabel::leaveEvent(event);
    hideSummaryPopup();
}

void BitWidget::mouseMoveEvent(QMouseEvent* event)
{
    QLabel::mouseMoveEvent(event);
    showSummaryPopup(event->globalPosition().toPoint());
}

void BitWidget::mousePressEvent(QMouseEvent*)
{
    hideSummaryPopup();
    m_pressed = true;
    updateStyle();
}

void BitWidget::mouseReleaseEvent(QMouseEvent*)
{
    m_pressed = false;
    setState(!m_state);
    emit stateChanged(m_state);
}

void BitWidget::applySummaryPopupTheme()
{
    if (m_summaryPopup == nullptr)
        return;

    const QColor background = m_summaryPopupBackgroundColor.isValid()
        ? m_summaryPopupBackgroundColor
        : palette().color(QPalette::ToolTipBase);
    const QColor foreground = m_summaryPopupForegroundColor.isValid()
        ? m_summaryPopupForegroundColor
        : palette().color(QPalette::ToolTipText);
    const QColor outline = m_summaryPopupOutlineColor.isValid()
        ? m_summaryPopupOutlineColor
        : background;
    const int cornerRadius = qMax(0, m_summaryPopupCornerRadius);

    QPalette popupPalette = m_summaryPopup->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        popupPalette.setColor(group, QPalette::Window, background);
        popupPalette.setColor(group, QPalette::WindowText, foreground);
    }
    m_summaryPopup->setPalette(popupPalette);

    QPalette labelPalette = m_summaryPopupLabel->palette();
    for (const QPalette::ColorGroup group : {QPalette::Active,
                                             QPalette::Inactive,
                                             QPalette::Disabled}) {
        labelPalette.setColor(group, QPalette::WindowText, foreground);
        labelPalette.setColor(group, QPalette::Text, foreground);
    }
    m_summaryPopupLabel->setPalette(labelPalette);

    m_summaryPopup->setStyleSheet(QStringLiteral(
        "QFrame#bitSummaryPopup {"
        " background: %1; color: %2;"
        " border: %4px solid %3;"
        " border-radius: %5px;"
        "}"
        "QLabel#bitSummaryPopupLabel {"
        " background: transparent; color: %2;"
        "}")
                                      .arg(background.name(),
                                           foreground.name(),
                                           outline.name())
                                      .arg(UiConfig::PopupOutlineStrokeWidth)
                                      .arg(cornerRadius));
    updateSummaryPopupMask();
}

void BitWidget::ensureSummaryPopup()
{
    if (m_summaryPopup != nullptr)
        return;

    m_summaryPopup = new QFrame(this, Qt::ToolTip | Qt::FramelessWindowHint);
    m_summaryPopup->setObjectName(QStringLiteral("bitSummaryPopup"));
    m_summaryPopup->setAutoFillBackground(true);
    m_summaryPopup->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_summaryPopup->setAttribute(Qt::WA_StyledBackground, true);
    m_summaryPopup->setFocusPolicy(Qt::NoFocus);
    m_summaryPopup->setFrameStyle(QFrame::NoFrame);

    QVBoxLayout* layout = new QVBoxLayout(m_summaryPopup);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(0);

    m_summaryPopupLabel = new QLabel(m_summaryPopup);
    m_summaryPopupLabel->setObjectName(QStringLiteral("bitSummaryPopupLabel"));
    m_summaryPopupLabel->setTextFormat(Qt::RichText);
    m_summaryPopupLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_summaryPopupLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(m_summaryPopupLabel);

    applySummaryPopupTheme();
}

void BitWidget::hideSummaryPopup()
{
    if (m_summaryPopup != nullptr)
        m_summaryPopup->hide();
}

void BitWidget::showSummaryPopup(const QPoint& globalPos)
{
    if (m_toolTipText.isEmpty())
        return;

    ensureSummaryPopup();
    m_summaryPopupLabel->setText(m_toolTipText);
    m_summaryPopup->adjustSize();
    m_summaryPopup->resize(m_summaryPopup->sizeHint());
    updateSummaryPopupMask();
    m_summaryPopup->move(constrainedPopupPosition(this, globalPos, m_summaryPopup->size()));
    m_summaryPopup->show();
}

void BitWidget::updateSummaryPopupMask()
{
    if (m_summaryPopup == nullptr)
        return;

    applyRoundedPopupMask(m_summaryPopup, qMax(0, m_summaryPopupCornerRadius));
}

BitFieldWidget::BitFieldWidget(QWidget* parent) :
    QWidget(parent)
{
    setObjectName(QStringLiteral("BitFieldWidget"));
    setAutoFillBackground(true);
    setLayoutDirection(Qt::LeftToRight);

    refreshTheme();

    m_bitWidgets.reserve(NumberOfBits);
    for (int i = 0; i < NumberOfBits; ++i) {
        BitWidget* bitWidget = new BitWidget(i, this);
        connect(bitWidget, SIGNAL(stateChanged(bool)), this, SLOT(onBitChanged()));
        m_bitWidgets.append(bitWidget);
    }

    m_byteLayouts.reserve(NumberOfBits / 8);
    for (int i = 0; i < NumberOfBits; i += 8) {
      QHBoxLayout* byteLayout(new QHBoxLayout);
      byteLayout->setSpacing(5);

      // Each byte is drawn over 3 UI elements (1 for each nibble and 1 for space)
      for (int nibble = 1 ; nibble >= 0 ; --nibble) {
          // Draw each nibble (4-bits) in a single box
          QHBoxLayout* nibbleLayout(new QHBoxLayout);

          // Disable items spacing so that it looks like a table
          nibbleLayout->setSpacing(0);

          for (int j = 3; j >= 0; --j) {
              const int bitIndex = i + (nibble * 4) + j;
              nibbleLayout->addWidget(m_bitWidgets.at(bitIndex));
          }

          // Change the name of first bits so that the proper style is applied
          nibbleLayout->itemAt(0)->widget()->setObjectName("FirstBitWidget");

          byteLayout->addLayout(nibbleLayout, Qt::AlignCenter);
        }

        // Draw the space between each byte
        QLabel* byteSpaceLabel = new QLabel;
        byteLayout->addWidget(byteSpaceLabel);

        m_byteLayouts.append(byteLayout);
    }

    m_resetButton = new QPushButton("0");
    m_resetButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_resetButton->setCursor(Qt::PointingHandCursor);
    connect(m_resetButton, SIGNAL(clicked()), this, SLOT(resetBits()));

    m_invertButton = new QPushButton("~");
    m_invertButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_invertButton->setCursor(Qt::PointingHandCursor);
    connect(m_invertButton, SIGNAL(clicked()), this, SLOT(invertBits()));

    m_shiftLeftButton = new QPushButton("<<");
    m_shiftLeftButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_shiftLeftButton->setCursor(Qt::PointingHandCursor);
    connect(m_shiftLeftButton, SIGNAL(clicked()), this, SLOT(shiftBitsLeft()));

    m_shiftRightButton = new QPushButton(">>");
    m_shiftRightButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_shiftRightButton->setCursor(Qt::PointingHandCursor);
    connect(m_shiftRightButton, SIGNAL(clicked()), this, SLOT(shiftBitsRight()));

    m_buttonsLayout = new QGridLayout;
    m_buttonsLayout->addWidget(m_resetButton, 0, 0);
    m_buttonsLayout->addWidget(m_invertButton, 0, 1);
    m_buttonsLayout->addWidget(m_shiftLeftButton, 1, 0);
    m_buttonsLayout->addWidget(m_shiftRightButton, 1, 1);

    m_fieldLayout = new QGridLayout;

    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->addStretch();
    m_mainLayout->addLayout(m_fieldLayout);
    m_mainLayout->addLayout(m_buttonsLayout);
    m_mainLayout->addStretch();

    this->updateFieldLayout();

    // The following needs to be done AFTER the widgets have been added to BitFieldWidget
    // as the style sheet will not be applied otherwise
    this->updateSize();

    // Update the field layout again, because the widgets size has changed now
    this->updateFieldLayout();
}

void BitFieldWidget::refreshTheme()
{
    // Build the CSS border color using 50% opacity (same result as previous method with painting).
    const QPalette palette = this->palette();
    const QColor background = m_themeBackground.isValid()
        ? m_themeBackground
        : palette.color(QPalette::Window);
    const QColor foreground = m_themeForeground.isValid()
        ? m_themeForeground
        : palette.color(QPalette::WindowText);
    const QColor hoverBackground = m_themeHoverBackground.isValid()
        ? m_themeHoverBackground
        : palette.color(QPalette::Highlight);
    const QColor hoverForeground = m_themeHoverForeground.isValid()
        ? m_themeHoverForeground
        : palette.color(QPalette::HighlightedText);
    const QColor pressedBackground = m_themePressedBackground.isValid()
        ? m_themePressedBackground
        : palette.color(QPalette::Mid);
    const QColor pressedForeground = m_themePressedForeground.isValid()
        ? m_themePressedForeground
        : palette.color(QPalette::ButtonText);
    const QColor buttonBackground = m_themeButtonBackground.isValid()
        ? m_themeButtonBackground
        : background;
    const QColor buttonForeground = m_themeButtonForeground.isValid()
        ? m_themeButtonForeground
        : foreground;
    const QColor buttonHoverBackground = m_themeButtonHoverBackground.isValid()
        ? m_themeButtonHoverBackground
        : hoverBackground;
    const QColor buttonHoverForeground = m_themeButtonHoverForeground.isValid()
        ? m_themeButtonHoverForeground
        : hoverForeground;
    const QColor buttonPressedBackground = m_themeButtonPressedBackground.isValid()
        ? m_themeButtonPressedBackground
        : pressedBackground;
    const QColor buttonPressedForeground = m_themeButtonPressedForeground.isValid()
        ? m_themeButtonPressedForeground
        : pressedForeground;
    const QColor borderColor = foreground;
    const QString cssBorderColor = QString("rgba(%1, %2, %3, %4)")
        .arg(borderColor.red())
        .arg(borderColor.green())
        .arg(borderColor.blue())
        .arg(0.5);

    setStyleSheet(QString("QWidget#BitFieldWidget {"
                          " background-color: %1; color: %2;"
                          "}"
                          "QLabel#BitWidget, QLabel#FirstBitWidget {"
                          " qproperty-alignment: 'AlignHCenter | AlignVCenter';"
                          " border-top: 1px solid %3;"
                          " border-bottom: 1px solid %3;"
                          " border-right: 1px solid %3;"
                          " padding: 1px;"
                          " background-color : %1; color : %2;"
                          "}"
                          "QLabel#FirstBitWidget {"
                          " border-left: 1px solid %3;"
                          "}")
                      .arg(background.name(),
                           foreground.name(),
                           cssBorderColor));
    const QString buttonStyle = bitFieldButtonStyleSheet(buttonBackground,
                                                        buttonForeground,
                                                        buttonHoverBackground,
                                                        buttonHoverForeground,
                                                        buttonPressedBackground,
                                                        buttonPressedForeground);
    for (QPushButton* button : {m_resetButton,
                                m_invertButton,
                                m_shiftLeftButton,
                                m_shiftRightButton}) {
        if (button == nullptr)
            continue;
        QPalette buttonPalette = button->palette();
        buttonPalette.setColor(QPalette::Button, buttonBackground);
        buttonPalette.setColor(QPalette::ButtonText, buttonForeground);
        buttonPalette.setColor(QPalette::Highlight, buttonHoverBackground);
        buttonPalette.setColor(QPalette::HighlightedText, buttonHoverForeground);
        button->setPalette(buttonPalette);
        button->setStyleSheet(buttonStyle);
    }
    const QColor selectedBackground = m_themeSelectedBackground.isValid()
        ? m_themeSelectedBackground
        : pressedBackground;
    const QColor selectedForeground = m_themeSelectedForeground.isValid()
        ? m_themeSelectedForeground
        : pressedForeground;
    for (BitWidget* bitWidget : m_bitWidgets)
        bitWidget->setThemeColors(background,
                                  foreground,
                                  hoverBackground,
                                  hoverForeground,
                                  selectedBackground,
                                  selectedForeground);
    for (BitWidget* bitWidget : m_bitWidgets)
        bitWidget->setToolTipThemeColors(m_summaryPopupBackgroundColor,
                                         m_summaryPopupForegroundColor,
                                         m_summaryPopupOutlineColor,
                                         m_summaryPopupCornerRadius);
}

void BitFieldWidget::setThemeColors(const QColor& background,
                                    const QColor& foreground,
                                    const QColor& hoverBackground,
                                    const QColor& hoverForeground,
                                    const QColor& pressedBackground,
                                    const QColor& pressedForeground,
                                    const QColor& selectedBackground,
                                    const QColor& selectedForeground,
                                    const QColor& buttonBackground,
                                    const QColor& buttonForeground,
                                    const QColor& buttonHoverBackground,
                                    const QColor& buttonHoverForeground,
                                    const QColor& buttonPressedBackground,
                                    const QColor& buttonPressedForeground)
{
    m_themeBackground = background;
    m_themeForeground = foreground;
    m_themeHoverBackground = hoverBackground;
    m_themeHoverForeground = hoverForeground;
    m_themePressedBackground = pressedBackground;
    m_themePressedForeground = pressedForeground;
    m_themeSelectedBackground = selectedBackground;
    m_themeSelectedForeground = selectedForeground;
    m_themeButtonBackground = buttonBackground;
    m_themeButtonForeground = buttonForeground;
    m_themeButtonHoverBackground = buttonHoverBackground;
    m_themeButtonHoverForeground = buttonHoverForeground;
    m_themeButtonPressedBackground = buttonPressedBackground;
    m_themeButtonPressedForeground = buttonPressedForeground;
    refreshTheme();
}

void BitFieldWidget::setToolTipThemeColors(const QColor& background,
                                           const QColor& foreground,
                                           const QColor& outline,
                                           int cornerRadius)
{
    m_summaryPopupBackgroundColor = background;
    m_summaryPopupForegroundColor = foreground;
    m_summaryPopupOutlineColor = outline;
    m_summaryPopupCornerRadius = qMax(0, cornerRadius);
    for (BitWidget* bitWidget : m_bitWidgets)
        bitWidget->setToolTipThemeColors(m_summaryPopupBackgroundColor,
                                         m_summaryPopupForegroundColor,
                                         m_summaryPopupOutlineColor,
                                         m_summaryPopupCornerRadius);
}

QSize BitFieldWidget::minimumSizeHint() const
{
    QSize hint = QWidget::minimumSizeHint();
    const int byteHeight = m_byteLayouts.isEmpty() ? 0 : m_byteLayouts.first()->sizeHint().height();
    const int controlsHeight = m_buttonsLayout ? m_buttonsLayout->sizeHint().height() : 0;
    const QMargins margins = contentsMargins() + m_mainLayout->contentsMargins();
    hint.setHeight(qMax(byteHeight, controlsHeight) + margins.top() + margins.bottom());
    return hint;
}

/** Update the bitfield layout based on the size of BitFieldWidget and its children.
 * This method should be called during initialization (but only after all the BitFieldWidget
 * members have been set) and every time the size of BitFieldWidget is changed.
 */
void BitFieldWidget::updateFieldLayout()
{
    // Empty current field layout
    while (m_fieldLayout->count() > 0) {
        m_fieldLayout->removeItem(m_fieldLayout->itemAt(0));
    }

    // Compute how much horizontal space we have to draw the bitfield
    int widgetWidth = this->size().width();
    int buttonsWidth = m_buttonsLayout->sizeHint().width();
    int byteWidgetWidth = m_byteLayouts.at(0)->sizeHint().width();
    auto contentMargins = this->contentsMargins() + m_mainLayout->contentsMargins();
    int spacesWidth = contentMargins.left() + contentMargins.right() + m_mainLayout->spacing() * 3;
    int availableWidth = widgetWidth - (buttonsWidth + spacesWidth);

    // Find out how much bytes per row can be shown within availableWidth
    int bytesPerRow = 1;
    if (byteWidgetWidth > 0 && byteWidgetWidth <= availableWidth) {
        // Make it a power of 2 so that only 8/16/32/64 bits per row are possible
        bytesPerRow = pow(2, floor(log2(availableWidth / byteWidgetWidth)));
    }

    // Populate the field layout
    int maxRows = NumberOfBits / (8 * bytesPerRow);
    for (int col = 0 ; col < bytesPerRow ; ++col) {
        const int colOffset = (bytesPerRow - col - 1);

        for (int row = 0 ; row < maxRows ; ++row) {
            int byteIndex = (maxRows - row - 1) * bytesPerRow + colOffset;
            auto* byteLayout = m_byteLayouts.at(byteIndex);

            m_fieldLayout->addLayout(byteLayout, row, col);
        }
    }

    // Update the minimum widget size so that its width can be reduced by the user
    if (byteWidgetWidth + buttonsWidth > 0)
        this->setMinimumWidth(byteWidgetWidth + buttonsWidth + spacesWidth);
}

/** Update the size of each BitWidget so that it is the same, as well as the size of the buttons.
 * This method should be called during initialization, and every time the UI font is changed.
 */
void BitFieldWidget::updateSize()
{
    // Compute bit widgets max size and apply it to all bit widgets
    QSize maxSize(0, 0);

    for (auto& bitWidget : m_bitWidgets) {
        auto widgetSize = bitWidget->sizeHint();
        if (maxSize.width() < widgetSize.width())
            maxSize.setWidth(widgetSize.width());
        if (maxSize.height() < widgetSize.height())
            maxSize.setHeight(widgetSize.height());
    }

    // Make the box be a square
    if (maxSize.width() < maxSize.height())
        maxSize.setWidth(maxSize.height());
    else
        maxSize.setHeight(maxSize.width());

    // Apply maxSize to all bit widgets
    for (auto& widget : m_bitWidgets)
        widget->setFixedSize(maxSize);

    // TODO: find some more justifiable size calculation.
    int buttonHeight = maxSize.height() * 4 / 3;
    int buttonWidth = buttonHeight * 2;

    m_resetButton->setFixedSize(buttonWidth, buttonHeight);
    m_invertButton->setFixedSize(buttonWidth, buttonHeight);
    m_shiftLeftButton->setFixedSize(buttonWidth, buttonHeight);
    m_shiftRightButton->setFixedSize(buttonWidth, buttonHeight);
}

void BitFieldWidget::wheelEvent(QWheelEvent* we)
{
    if (we->angleDelta().y() > 0)
        shiftBitsLeft();
    else
        shiftBitsRight();
}

void BitFieldWidget::resizeEvent(QResizeEvent*)
{
  this->updateFieldLayout();
}

void BitFieldWidget::updateBits(const Quantity& number)
{
    // Create a binary copy of number
    // (simply converting it to binary will not work if its base is defined)
    Quantity binNumber(number);
    binNumber.setFormat(Quantity::Format::Fixed() + Quantity::Format::Binary() + Quantity(number).format());
    QString binaryNumberString = DMath::format(binNumber);
    QListIterator<BitWidget*> bitsIterator(m_bitWidgets);

    if (number.isZero() || !number.isInteger())
        binaryNumberString.clear();
    else if (number.isNegative())
        binaryNumberString.remove(0, 3); // Remove '-0b'.
    else
        binaryNumberString.remove(0, 2); // Remove '0b'.

    auto iterator = binaryNumberString.end();
    while (bitsIterator.hasNext()) {
        if (iterator != binaryNumberString.begin()) {
            --iterator;
            bitsIterator.next()->setState(*iterator == '1');
        } else
            bitsIterator.next()->setState(false);
    }
}

void BitFieldWidget::onBitChanged()
{
    QListIterator<BitWidget*> bitsIterator(m_bitWidgets);
    QString expression;

    while (bitsIterator.hasNext())
        expression.prepend(bitsIterator.next()->state() ? "1" : "0");

    QRegularExpression leadingZerosPattern(QString("^0{,%1}").arg(NumberOfBits - 1));
    expression.remove(leadingZerosPattern);
    expression.prepend(MathDsl::BinPrefix);

    emit bitsChanged(expression);
}

void BitFieldWidget::invertBits()
{
    foreach (BitWidget* w, m_bitWidgets)
        w->setState(!w->state());

    onBitChanged();
}

void BitFieldWidget::clear()
{
    for (auto& w : m_bitWidgets)
        w->setState(false);
}

void BitFieldWidget::resetBits()
{
    clear();
    onBitChanged();
}

void BitFieldWidget::shiftBitsLeft()
{
    auto it = m_bitWidgets.constEnd();
    auto itBegin = m_bitWidgets.constBegin();

    --it;
    while (it != itBegin) {
        (*it)->setState((*(it-1))->state());
        --it;
    }

    (*itBegin)->setState(false);
    onBitChanged();
}

void BitFieldWidget::shiftBitsRight()
{
    auto it = m_bitWidgets.constBegin();
    auto itEnd = m_bitWidgets.constEnd();

    --itEnd;
    while (it != itEnd) {
        (*it)->setState((*(it+1))->state());
        it++;
    }

    (*itEnd)->setState(false);
    onBitChanged();
}
