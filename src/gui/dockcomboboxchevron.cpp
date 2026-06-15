// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/dockcomboboxchevron.h"

#include <QAbstractAnimation>
#include <QAbstractItemView>
#include <QComboBox>
#include <QEasingCurve>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace {

constexpr int kChevronAnimationMs = 150;
constexpr qreal kChevronOpacity = 0.76;

} // namespace

DockComboBoxChevron::DockComboBoxChevron(QComboBox* comboBox)
    : QWidget(comboBox)
    , m_comboBox(comboBox)
    , m_animation(new QVariantAnimation(this))
{
    setObjectName(QStringLiteral("speedcrunchDockComboBoxChevron"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);

    m_animation->setDuration(kChevronAnimationMs);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_rotation = value.toReal();
        update();
    });
    connect(comboBox, QOverload<int>::of(&QComboBox::activated), this, [this]() {
        setPopupOpen(false);
    });
    comboBox->installEventFilter(this);
    installPopupEventFilters();
    reposition();
    show();
}

void DockComboBoxChevron::apply(QComboBox* comboBox,
                                const QColor& textColor,
                                const QColor& outlineColor)
{
    if (comboBox == nullptr)
        return;

    DockComboBoxChevron* chevron = nullptr;
    if (QWidget* existing = comboBox->findChild<QWidget*>(
            QStringLiteral("speedcrunchDockComboBoxChevron"),
            Qt::FindDirectChildrenOnly)) {
        chevron = dynamic_cast<DockComboBoxChevron*>(existing);
    }
    if (chevron == nullptr)
        chevron = new DockComboBoxChevron(comboBox);

    chevron->setColors(textColor, outlineColor);
    chevron->refresh();
}

void DockComboBoxChevron::setColors(QColor chevronColor, const QColor& outlineColor)
{
    chevronColor.setAlphaF(kChevronOpacity);
    if (m_chevronColor == chevronColor && m_outlineColor == outlineColor)
        return;

    m_chevronColor = chevronColor;
    m_outlineColor = outlineColor;
    update();
}

void DockComboBoxChevron::refresh()
{
    installPopupEventFilters();
    reposition();
    setPopupOpen(m_view != nullptr && m_view->isVisible());
    raise();
}

bool DockComboBoxChevron::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_comboBox) {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::StyleChange:
            reposition();
            break;
        case QEvent::Hide:
            setPopupOpen(false);
            break;
        case QEvent::KeyPress:
        case QEvent::MouseButtonPress:
            installPopupEventFilters();
            break;
        default:
            break;
        }
    } else if (watched == m_view || watched == m_popupWindow) {
        if (event->type() == QEvent::Show)
            setPopupOpen(true);
        else if (event->type() == QEvent::Hide)
            setPopupOpen(false);
    }

    return QWidget::eventFilter(watched, event);
}

void DockComboBoxChevron::paintEvent(QPaintEvent*)
{
    if (!m_chevronColor.isValid() && !m_outlineColor.isValid())
        return;

    const qreal dpr = devicePixelRatioF();
    const auto aligned = [dpr](qreal value) {
        return qRound(value * dpr) / dpr;
    };

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (m_outlineColor.isValid()) {
        const QRectF outlineRect(0.5, 0.5, qMax(0, width() - 1), qMax(0, height() - 1));
        QPen outlinePen(m_outlineColor, 1.0);
        painter.setPen(outlinePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(outlineRect, 8.0, 8.0);
    }

    if (m_chevronColor.isValid()) {
        painter.translate(QPointF(aligned(width() - IndicatorWidth / 2.0), aligned(height() / 2.0)));
        painter.rotate(m_rotation);

        QPainterPath path;
        path.moveTo(QPointF(-5.0, -3.0));
        path.lineTo(QPointF(0.0, 3.0));
        path.lineTo(QPointF(5.0, -3.0));

        QPen pen(m_chevronColor, 1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }
}

void DockComboBoxChevron::installPopupEventFilters()
{
    if (m_comboBox == nullptr)
        return;

    QAbstractItemView* view = m_comboBox->view();
    if (m_view != view) {
        if (m_view != nullptr)
            m_view->removeEventFilter(this);
        m_view = view;
        if (m_view != nullptr)
            m_view->installEventFilter(this);
    }

    QWidget* popupWindow = m_view != nullptr ? m_view->window() : nullptr;
    if (popupWindow == m_comboBox->window())
        popupWindow = nullptr;
    if (m_popupWindow != popupWindow) {
        if (m_popupWindow != nullptr)
            m_popupWindow->removeEventFilter(this);
        m_popupWindow = popupWindow;
        if (m_popupWindow != nullptr)
            m_popupWindow->installEventFilter(this);
    }
}

void DockComboBoxChevron::reposition()
{
    if (m_comboBox == nullptr)
        return;

    setGeometry(0, 0, m_comboBox->width(), m_comboBox->height());
}

void DockComboBoxChevron::setPopupOpen(bool open)
{
    if (m_popupOpen == open && m_animation->state() != QAbstractAnimation::Running)
        return;

    m_popupOpen = open;
    m_animation->stop();
    m_animation->setStartValue(m_rotation);
    m_animation->setEndValue(open ? 180.0 : 0.0);
    m_animation->start();
}
