// SPDX-FileCopyrightText: 2014-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef BITFIELDWIDGET_H
#define BITFIELDWIDGET_H

#include <QColor>
#include <QLabel>
#include <QWidget>

class Quantity;
class QEnterEvent;
class QFrame;
class QPushButton;
class QGridLayout;
class QHBoxLayout;

class BitWidget : public QLabel {
    Q_OBJECT

public:
    explicit BitWidget(int apos, QWidget* parent = 0);

    bool state() const { return m_state; }
    void setState(bool state);
    void setThemeColors(const QColor& background,
                        const QColor& foreground,
                        const QColor& hoverBackground,
                        const QColor& hoverForeground,
                        const QColor& selectedBackground,
                        const QColor& selectedForeground);
    void setToolTipThemeColors(const QColor& background,
                               const QColor& foreground,
                               const QColor& outline,
                               int cornerRadius);

signals:
    void stateChanged(bool);

protected:
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    void applySummaryPopupTheme();
    void ensureSummaryPopup();
    void hideSummaryPopup();
    void showSummaryPopup(const QPoint& globalPos);
    void updateSummaryPopupMask();
    void updateStyle();

    enum {
        // TODO: have this scale with screen DPI
        SizePixels = 20,
    };

    Q_DISABLE_COPY(BitWidget)

    bool m_state;
    bool m_pressed = false;
    QString m_toolTipText;
    QFrame* m_summaryPopup = nullptr;
    QLabel* m_summaryPopupLabel = nullptr;
    QColor m_summaryPopupBackgroundColor;
    QColor m_summaryPopupForegroundColor;
    QColor m_summaryPopupOutlineColor;
    int m_summaryPopupCornerRadius = 0;
    QColor m_themeBackground;
    QColor m_themeForeground;
    QColor m_themeHoverBackground;
    QColor m_themeHoverForeground;
    QColor m_themeSelectedBackground;
    QColor m_themeSelectedForeground;
};

class BitFieldWidget : public QWidget {
    Q_OBJECT

public:
    explicit BitFieldWidget(QWidget* parent = 0);
    QSize minimumSizeHint() const override;

signals:
    void bitsChanged(const QString&);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void wheelEvent(QWheelEvent*) override;
    void resizeEvent(QResizeEvent*) override;

public slots:
    void clear();
    void updateBits(const Quantity&);
    void updateSize();
    void updateFieldLayout();
    void setThemeColors(const QColor& background,
                        const QColor& foreground,
                        const QColor& hoverBackground = QColor(),
                        const QColor& hoverForeground = QColor(),
                        const QColor& pressedBackground = QColor(),
                        const QColor& pressedForeground = QColor(),
                        const QColor& selectedBackground = QColor(),
                        const QColor& selectedForeground = QColor(),
                        const QColor& buttonBackground = QColor(),
                        const QColor& buttonForeground = QColor(),
                        const QColor& buttonHoverBackground = QColor(),
                        const QColor& buttonHoverForeground = QColor(),
                        const QColor& buttonPressedBackground = QColor(),
                        const QColor& buttonPressedForeground = QColor());
    void setToolTipThemeColors(const QColor& background,
                               const QColor& foreground,
                               const QColor& outline,
                               int cornerRadius);
    void refreshTheme();

private slots:
    void onBitChanged();
    void invertBits();
    void shiftBitsLeft();
    void shiftBitsRight();
    void resetBits();

private:
    void applyButtonSummaryPopupTheme();
    QString buttonSummaryText(const QObject* watched) const;
    void ensureButtonSummaryPopup();
    void hideButtonSummaryPopup();
    void setupButton(QPushButton* button);
    void showButtonSummaryPopup(const QString& text,
                                QWidget* anchor,
                                const QPoint& globalPos);
    void updateButtonSummaryPopupMask();

    enum {
        NumberOfBits = 64
    };

    Q_DISABLE_COPY(BitFieldWidget)

    QList<BitWidget*> m_bitWidgets;
    QList<QHBoxLayout*> m_byteLayouts;

    QGridLayout* m_fieldLayout;
    QGridLayout* m_buttonsLayout;
    QHBoxLayout* m_mainLayout;

    QPushButton* m_resetButton = nullptr;
    QPushButton* m_invertButton = nullptr;
    QPushButton* m_shiftLeftButton = nullptr;
    QPushButton* m_shiftRightButton = nullptr;
    QFrame* m_buttonSummaryPopup = nullptr;
    QLabel* m_buttonSummaryPopupLabel = nullptr;
    QColor m_themeBackground;
    QColor m_themeForeground;
    QColor m_themeHoverBackground;
    QColor m_themeHoverForeground;
    QColor m_themePressedBackground;
    QColor m_themePressedForeground;
    QColor m_themeSelectedBackground;
    QColor m_themeSelectedForeground;
    QColor m_themeButtonBackground;
    QColor m_themeButtonForeground;
    QColor m_themeButtonHoverBackground;
    QColor m_themeButtonHoverForeground;
    QColor m_themeButtonPressedBackground;
    QColor m_themeButtonPressedForeground;
    QColor m_summaryPopupBackgroundColor;
    QColor m_summaryPopupForegroundColor;
    QColor m_summaryPopupOutlineColor;
    int m_summaryPopupCornerRadius = 0;
};

#endif // BITFIELDWIDGET_H
