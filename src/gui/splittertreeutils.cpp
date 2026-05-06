// SPDX-FileCopyrightText: 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later

#include "gui/splittertreeutils.h"

#include <QSplitter>
#include <QWidget>

void normalizeSplitterTree(QSplitter* rootSplitter)
{
    if (rootSplitter == nullptr)
        return;

    const auto normalize = [](QSplitter* splitter, const auto& normalizeRef) -> void {
        if (splitter == nullptr)
            return;

        for (int i = splitter->count() - 1; i >= 0; --i) {
            if (QSplitter* childSplitter = qobject_cast<QSplitter*>(splitter->widget(i)))
                normalizeRef(childSplitter, normalizeRef);
        }

        for (int i = splitter->count() - 1; i >= 0; --i) {
            QSplitter* childSplitter = qobject_cast<QSplitter*>(splitter->widget(i));
            if (childSplitter == nullptr)
                continue;

            if (childSplitter->count() == 0) {
                childSplitter->setParent(nullptr);
                childSplitter->deleteLater();
                continue;
            }

            if (childSplitter->count() == 1) {
                QWidget* onlyChild = childSplitter->widget(0);
                if (onlyChild != nullptr) {
                    onlyChild->setParent(nullptr);
                    splitter->insertWidget(i, onlyChild);
                }
                childSplitter->setParent(nullptr);
                childSplitter->deleteLater();
            }
        }
    };

    normalize(rootSplitter, normalize);
    while (rootSplitter->count() == 1) {
        QSplitter* onlyChildSplitter = qobject_cast<QSplitter*>(rootSplitter->widget(0));
        if (onlyChildSplitter == nullptr)
            break;

        const Qt::Orientation orientation = onlyChildSplitter->orientation();
        const QList<int> childSizes = onlyChildSplitter->sizes();
        QList<QWidget*> children;
        while (onlyChildSplitter->count() > 0) {
            QWidget* child = onlyChildSplitter->widget(0);
            child->setParent(nullptr);
            children.append(child);
        }

        rootSplitter->setOrientation(orientation);
        for (QWidget* child : children)
            rootSplitter->addWidget(child);
        if (childSizes.size() == rootSplitter->count())
            rootSplitter->setSizes(childSizes);

        onlyChildSplitter->setParent(nullptr);
        onlyChildSplitter->deleteLater();
    }
}
