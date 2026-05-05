// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/dockliststyle.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QHoverEvent>
#include <QLabel>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QWidget>

namespace {

QColor hoverColorForView(const QAbstractItemView* view)
{
    const QPalette palette = view->palette();
    const QColor base = palette.color(QPalette::Base);
    QColor hover = palette.color(QPalette::AlternateBase);
    if (hover == base)
        hover = base.lightness() < 128 ? base.lighter(135) : base.darker(108);
    return hover;
}

class DockListItemDelegate : public QStyledItemDelegate {
public:
    explicit DockListItemDelegate(QAbstractItemView* view)
        : QStyledItemDelegate(view)
        , m_view(view)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        if (index.row() == m_view->property("dockListHoveredRow").toInt()) {
            painter->save();
            painter->fillRect(opt.rect, hoverColorForView(m_view));
            painter->restore();
            opt.backgroundBrush = Qt::NoBrush;
            opt.state &= ~QStyle::State_MouseOver;
        }
        QStyledItemDelegate::paint(painter, opt, index);
    }

private:
    QAbstractItemView* m_view;
};

class DockListCursorFilter : public QObject {
public:
    explicit DockListCursorFilter(QAbstractItemView* view)
        : QObject(view)
        , m_view(view)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched != m_view->viewport())
            return QObject::eventFilter(watched, event);

        if (event->type() == QEvent::MouseMove) {
            const QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            updateHoveredIndex(m_view->indexAt(mouseEvent->pos()));
        } else if (event->type() == QEvent::HoverMove) {
            const QHoverEvent* hoverEvent = static_cast<QHoverEvent*>(event);
            updateHoveredIndex(m_view->indexAt(hoverEvent->position().toPoint()));
        } else if (event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave) {
            updateHoveredIndex(QModelIndex());
            m_view->viewport()->setCursor(Qt::ArrowCursor);
        } else if (event->type() == QEvent::Resize) {
            const QList<QLabel*> labels = m_view->viewport()->findChildren<QLabel*>(
                QString(), Qt::FindDirectChildrenOnly);
            for (QLabel* label : labels) {
                if (label->property("dockListNoMatchLabel").toBool() && label->isVisible())
                    label->setGeometry(m_view->viewport()->rect());
            }
        }

        return QObject::eventFilter(watched, event);
    }

private:
    void updateHoveredIndex(const QModelIndex& hoveredIndex)
    {
        const int previousHoveredRow = m_view->property("dockListHoveredRow").toInt();
        const int hoveredRow = hoveredIndex.isValid() ? hoveredIndex.row() : -1;
        if (hoveredRow != previousHoveredRow) {
            m_view->setProperty("dockListHoveredRow", hoveredRow);
            updateRow(previousHoveredRow);
            updateRow(hoveredRow);
        }
        m_view->viewport()->setCursor(
            hoveredIndex.isValid() ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }

    void updateRow(int row)
    {
        if (row < 0 || !m_view->model())
            return;

        const int columnCount = m_view->model()->columnCount(m_view->rootIndex());
        for (int column = 0; column < columnCount; ++column)
            m_view->viewport()->update(m_view->visualRect(
                m_view->model()->index(row, column, m_view->rootIndex())));
    }

    QAbstractItemView* m_view;
};

} // namespace

namespace DockListStyle {

void apply(QAbstractItemView* view)
{
    view->setAlternatingRowColors(false);
    view->setMouseTracking(true);
    view->viewport()->setMouseTracking(true);
    view->viewport()->setAttribute(Qt::WA_Hover, true);
    view->viewport()->setCursor(Qt::ArrowCursor);
    view->setProperty("dockListHoveredRow", -1);
    view->setItemDelegate(new DockListItemDelegate(view));
    view->viewport()->installEventFilter(new DockListCursorFilter(view));
}

void showCenteredNoMatchLabel(QAbstractItemView* view, QLabel* label)
{
    if (label->parentWidget() != view->viewport())
        label->setParent(view->viewport());
    label->setProperty("dockListNoMatchLabel", true);
    label->setAttribute(Qt::WA_TransparentForMouseEvents);
    label->setGeometry(view->viewport()->rect());
    label->show();
    label->raise();
}

} // namespace DockListStyle
