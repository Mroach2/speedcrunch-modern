// SPDX-FileCopyrightText: 2014-2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef BITFIELDWIDGET_H
#define BITFIELDWIDGET_H

#include <QLabel>
#include <QWidget>

class Quantity;
class QPushButton;
class QGridLayout;
class QHBoxLayout;

class BitWidget : public QLabel {
    Q_OBJECT

public:
    explicit BitWidget(int apos, QWidget* parent = 0);

    bool state() const { return m_state; }
    void setState(bool state);

signals:
    void stateChanged(bool);

protected:
    void mouseReleaseEvent(QMouseEvent*);

private:
    void updateStyle();

    enum {
        // TODO: have this scale with screen DPI
        SizePixels = 20,
    };

    Q_DISABLE_COPY(BitWidget)

    bool m_state;
};

class BitFieldWidget : public QWidget {
    Q_OBJECT

public:
    explicit BitFieldWidget(QWidget* parent = 0);
    QSize minimumSizeHint() const override;

signals:
    void bitsChanged(const QString&);

protected:
    void wheelEvent(QWheelEvent*) override;
    void resizeEvent(QResizeEvent*) override;

public slots:
    void clear();
    void updateBits(const Quantity&);
    void updateSize();
    void updateFieldLayout();

private slots:
    void onBitChanged();
    void invertBits();
    void shiftBitsLeft();
    void shiftBitsRight();
    void resetBits();

private:
    enum {
        NumberOfBits = 64
    };

    Q_DISABLE_COPY(BitFieldWidget)

    QList<BitWidget*> m_bitWidgets;
    QList<QHBoxLayout*> m_byteLayouts;

    QGridLayout* m_fieldLayout;
    QGridLayout* m_buttonsLayout;
    QHBoxLayout* m_mainLayout;

    QPushButton* m_resetButton;
    QPushButton* m_invertButton;
    QPushButton* m_shiftLeftButton;
    QPushButton* m_shiftRightButton;
};

#endif // BITFIELDWIDGET_H
