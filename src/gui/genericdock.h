// SPDX-FileCopyrightText: 2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_GENERICDOCK_H
#define GUI_GENERICDOCK_H

#include <QCoreApplication>
#include <QDockWidget>


template <class Widget> class GenericDock : public QDockWidget {
    // Can't have Q_OBJECT in templates. That means no signals, slots, or properties in this class.

public:
    explicit GenericDock(const char* i18nContext, const char* name, QWidget* parent = nullptr)
        : QDockWidget(parent),
          m_name(name),
          m_i18nContext(i18nContext),
          m_widget(new Widget(this))
    {
        setWidget(m_widget);
        setMinimumWidth(200);
        retranslateText();
    }

    void retranslateText()
    {
        setWindowTitle(QCoreApplication::translate(m_i18nContext, m_name));
    }

    Widget* widget() const
    {
        return m_widget;
    }

    void setWidget(Widget* widget)
    {
        QDockWidget::setWidget(widget);
        m_widget = widget;
        setFocusProxy(widget);
    }

protected:
    void changeEvent(QEvent* e) override
    {
        if (e->type() == QEvent::LanguageChange)
            retranslateText();
        else
            QDockWidget::changeEvent(e);
    }

private:
    const char* m_name;
    const char* m_i18nContext;
    Widget* m_widget;
};

#endif
