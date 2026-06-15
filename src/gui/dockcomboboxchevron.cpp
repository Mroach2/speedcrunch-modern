// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/dockcomboboxchevron.h"

#include "gui/uiconfig.h"

#include <QAbstractAnimation>
#include <QAbstractItemView>
#include <QBitmap>
#include <QComboBox>
#include <QEasingCurve>
#include <QEvent>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace {

constexpr int kChevronAnimationMs = 150;
constexpr qreal kChevronOpacity = 0.76;

void removeFrame(QWidget* widget)
{
    QFrame* frame = qobject_cast<QFrame*>(widget);
    if (frame == nullptr)
        return;

    frame->setFrameShape(QFrame::NoFrame);
    frame->setLineWidth(0);
    frame->setMidLineWidth(0);
}

void applyRoundedMask(QWidget* widget)
{
    if (widget == nullptr || widget->size().isEmpty())
        return;

    QBitmap mask(widget->size());
    mask.fill(Qt::color0);
    QPainter painter(&mask);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::color1);
    painter.drawRoundedRect(QRectF(mask.rect()).adjusted(0, 0, -1, -1),
                            UiConfig::CompletionPopupCornerRadius,
                            UiConfig::CompletionPopupCornerRadius);
    widget->setMask(mask);
}

} // namespace

DockComboBoxChevron::DockComboBoxChevron(QComboBox* comboBox)
    : QWidget(comboBox)
    , m_comboBox(comboBox)
    , m_animation(new QVariantAnimation(this))
{
    setObjectName(QStringLiteral("speedcrunchDockComboBoxChevron"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
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
        if (event->type() == QEvent::Show) {
            stylePopupChrome();
            setPopupOpen(true);
        } else if (event->type() == QEvent::Hide) {
            setPopupOpen(false);
        } else if (event->type() == QEvent::Resize) {
            stylePopupChrome();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void DockComboBoxChevron::paintEvent(QPaintEvent*)
{
    if (!m_chevronColor.isValid())
        return;

    const qreal dpr = devicePixelRatioF();
    const auto aligned = [dpr](qreal value) {
        return qRound(value * dpr) / dpr;
    };

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    painter.translate(QPointF(aligned(width() / 2.0), aligned(height() / 2.0)));
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

    stylePopupChrome();
}

QWidget* DockComboBoxChevron::popupChromeWidget() const
{
    if (m_popupWindow != nullptr)
        return m_popupWindow;
    return m_view;
}

void DockComboBoxChevron::reposition()
{
    if (m_comboBox == nullptr)
        return;

    setGeometry(qMax(0, m_comboBox->width() - IndicatorWidth),
                0,
                IndicatorWidth,
                m_comboBox->height());
}

void DockComboBoxChevron::stylePopupChrome()
{
    if (m_view == nullptr)
        return;

    removeFrame(m_view);
    m_view->setAutoFillBackground(false);
    m_view->viewport()->setAutoFillBackground(false);
    m_view->viewport()->setAttribute(Qt::WA_StyledBackground, true);

    QWidget* popupChrome = popupChromeWidget();
    if (popupChrome == nullptr)
        return;

    removeFrame(popupChrome);
    popupChrome->setAutoFillBackground(false);
    popupChrome->setAttribute(Qt::WA_StyledBackground, true);
    if (popupChrome != m_view) {
        popupChrome->setObjectName(QStringLiteral("speedcrunchDockComboBoxPopupChrome"));
        popupChrome->setStyleSheet(QStringLiteral(
            "QWidget#speedcrunchDockComboBoxPopupChrome,"
            "QFrame#speedcrunchDockComboBoxPopupChrome {"
            " background: transparent; border: 0;"
            "}"));
    }
    applyRoundedMask(popupChrome);
}

void DockComboBoxChevron::setPopupOpen(bool open)
{
    if (open)
        stylePopupChrome();

    if (m_popupOpen == open && m_animation->state() != QAbstractAnimation::Running)
        return;

    m_popupOpen = open;
    m_animation->stop();
    m_animation->setStartValue(m_rotation);
    m_animation->setEndValue(open ? 180.0 : 0.0);
    m_animation->start();
}
