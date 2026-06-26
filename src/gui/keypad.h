// SPDX-FileCopyrightText: 2014, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_KEYPAD_H
#define GUI_KEYPAD_H

#include <QColor>
#include <QHash>
#include <QList>
#include <QWidget>

class QPushButton;
class QFrame;
class QLabel;

class Keypad : public QWidget {
    Q_OBJECT

public:
    enum LayoutMode {
        LayoutModeScientificWide = 0,
        LayoutModeBasicWide = 1,
        LayoutModeScientificNarrow = 2
    };

    enum Button {
        Key0, Key1, Key2, Key3, Key4, Key5, Key6, Key7, Key8, Key9,
        KeyEquals, KeyPlus, KeyMinus, KeyTimes, KeyDivide,
        KeyRadixChar, KeyClear, KeyEE, KeyLeftPar, KeyRightPar,
        KeyRaise, KeySqrt, KeyCbrt, KeyLg, KeyMod, KeyBackspace, KeyPercent, KeyFactorial, KeyPi, KeyAns,
        KeyX, KeyXEquals, KeyExp, KeyLn, KeySin, KeyAsin, KeyCos,
        KeyAcos, KeyTan, KeyAtan
    };

    struct CustomButtonDescription {
        QString label;
        QString text;
        int action;
        int row;
        int column;
    };

    static QList<CustomButtonDescription> presetCustomButtons(LayoutMode layoutMode,
                                                              QChar radixCharacter,
                                                              int* rows = nullptr,
                                                              int* columns = nullptr);

    explicit Keypad(LayoutMode layoutMode = LayoutModeScientificWide, QWidget* parent = 0, int scalePercent = 100);
    explicit Keypad(const QList<CustomButtonDescription>& customButtons, QWidget* parent = 0, int scalePercent = 100);
    void setThemeButtonColors(const QColor& background,
                              const QColor& foreground,
                              const QColor& hoverBackground,
                              const QColor& hoverForeground,
                              const QColor& pressedBackground,
                              const QColor& pressedForeground,
                              const QColor& primaryBackground);
    void setToolTipThemeColors(const QColor& background,
                               const QColor& foreground,
                               const QColor& outline,
                               int cornerRadius);

signals:
    void buttonPressed(Keypad::Button) const;
    void customButtonPressed(int action, const QString& text) const;

public slots:
    void handleRadixCharacterChange();
    void retranslateText();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void changeEvent(QEvent*) override;

private:
    Q_DISABLE_COPY(Keypad)

    void applySummaryPopupTheme();
    void ensureSummaryPopup();
    void hideSummaryPopup();
    QPushButton* key(Button button) const;
    void createButtons();
    void disableButtonFocus();
    void layoutButtons();
    void createCustomButtons();
    void layoutCustomButtons();
    void updateButtonStyleSheets();
    void setButtonTooltip(Button button, const QString& text);
    void setButtonTooltips();
    void showSummaryPopup(const QString& text, QWidget* anchor, const QPoint& globalPos);
    void sizeButtons();
    void sizeCustomButtons();
    QString tooltipTextForObject(const QObject* watched) const;
    void updateSummaryPopupMask();

    static const struct KeyDescription {
        QString label;
        Button button;
        bool boldFont;
        int gridRow;
        int gridColumn;
    } keyDescriptions[];

    LayoutMode m_layoutMode;
    bool m_isCustom;
    int m_scalePercent;
    QColor m_buttonBackground;
    QColor m_buttonForeground;
    QColor m_buttonHoverBackground;
    QColor m_buttonHoverForeground;
    QColor m_buttonPressedBackground;
    QColor m_buttonPressedForeground;
    QColor m_primaryBackground;
    QColor m_summaryPopupBackgroundColor;
    QColor m_summaryPopupForegroundColor;
    QColor m_summaryPopupOutlineColor;
    int m_summaryPopupCornerRadius = 0;
    QFrame* m_summaryPopup = nullptr;
    QLabel* m_summaryPopupLabel = nullptr;
    QHash<Button, QPair<QPushButton*, const KeyDescription*> > keys;
    QHash<QPushButton*, QString> m_buttonToolTipTexts;
    QList<CustomButtonDescription> m_customButtons;
    QList<QPushButton*> m_customWidgets;
};

#endif
