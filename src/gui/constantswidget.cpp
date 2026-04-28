// This file is part of the SpeedCrunch project
// Copyright (C) 2007 Ariya Hidayat <ariya@kde.org>
// Copyright (C) 2008, 2009, 2010, 2011, 2016 @heldercorreia
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; see the file COPYING.  If not, write to
// the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
// Boston, MA 02110-1301, USA.

#include "gui/constantswidget.h"

#include "core/constants.h"
#include "core/settings.h"
#include "core/unicodechars.h"
#include "core/mathdsl.h"

#include <QEvent>
#include <QTimer>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QShortcut>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

static QString constantExpression(const Constant& constant)
{
    if (constant.name == Constants::tr("pi (π)"))
        return QString(UnicodeChars::Pi);
    if (constant.name == Constants::tr("Euler's number (e)"))
        return QStringLiteral("e");

    QString unit = constant.unit;
    unit.replace(UnicodeChars::MiddleDot, MathDsl::MulDotOp);
    return constant.unit.isEmpty()
        ? constant.value
        : QStringLiteral("%1%2[%3]")
            .arg(constant.value, QString(MathDsl::QuantSp), unit);
}

ConstantsWidget::ConstantsWidget(QWidget* parent)
    : QWidget(parent)
{
    m_domainLabel = new QLabel(this);
    m_domain = new QComboBox(this);
    m_domain->setEditable(false);
    m_domain->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_subdomainLabel = new QLabel(this);
    m_subdomain = new QComboBox(this);
    m_subdomain->setEditable(false);
    m_subdomain->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    connect(m_domain, SIGNAL(activated(int)), SLOT(handleDomainChanged()));
    connect(m_subdomain, SIGNAL(activated(int)), SLOT(filter()));

    QWidget* domainBox = new QWidget(this);
    QHBoxLayout* domainLayout = new QHBoxLayout;
    domainBox->setLayout(domainLayout);
    domainLayout->addWidget(m_domainLabel);
    domainLayout->addWidget(m_domain);
    domainLayout->addWidget(m_subdomainLabel);
    domainLayout->addWidget(m_subdomain);
    domainLayout->setContentsMargins(0, 0, 0, 0);

    m_label = new QLabel(this);

    m_filter = new QLineEdit(this);
    m_filter->setMinimumWidth(fontMetrics().horizontalAdvance('X') * 10);
    m_filter->setClearButtonEnabled(true);

    connect(m_filter, SIGNAL(textChanged(const QString &)), SLOT(triggerFilter()));

    QWidget* searchBox = new QWidget(this);
    QHBoxLayout* searchLayout = new QHBoxLayout;
    searchBox->setLayout(searchLayout);
    searchLayout->addWidget(m_label);
    searchLayout->addWidget(m_filter);
    searchLayout->setContentsMargins(0, 0, 0, 0);

    m_list = new QTreeWidget(this);
    m_list->setAutoScroll(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setColumnCount(3);
    m_list->setRootIsDecorated(false);
    m_list->setMouseTracking(true);
    m_list->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_list->setSelectionBehavior(QTreeWidget::SelectRows);
    m_list->setAlternatingRowColors(true);
    m_list->setCursor(QCursor(Qt::PointingHandCursor));

    connect(m_list, SIGNAL(itemActivated(QTreeWidgetItem*, int)), SLOT(handleItem(QTreeWidgetItem*)));
    QShortcut* returnShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    returnShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(returnShortcut, &QShortcut::activated, this, [this]() {
        if (QTreeWidgetItem* current = m_list->currentItem())
            handleItem(current);
    });
    QShortcut* enterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), this);
    enterShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(enterShortcut, &QShortcut::activated, this, [this]() {
        if (QTreeWidgetItem* current = m_list->currentItem())
            handleItem(current);
    });

    QVBoxLayout* layout = new QVBoxLayout;
    setLayout(layout);
    layout->setContentsMargins(3, 3, 3, 3);
    layout->addWidget(domainBox);
    layout->addWidget(searchBox);
    layout->addWidget(m_list);

    m_filterTimer = new QTimer(this);
    m_filterTimer->setInterval(500);
    m_filterTimer->setSingleShot(true);
    connect(m_filterTimer, SIGNAL(timeout()), SLOT(filter()));

    m_noMatchLabel = new QLabel(this);
    m_noMatchLabel->setAlignment(Qt::AlignCenter);
    m_noMatchLabel->adjustSize();
    m_noMatchLabel->hide();

    retranslateText();

    setFocusProxy(m_filter);

    filter();
}

ConstantsWidget::~ConstantsWidget()
{
    m_filterTimer->stop();
}

QString ConstantsWidget::selectedDomain() const { return m_domain->currentText(); }
QString ConstantsWidget::selectedSubdomain() const { return m_subdomain->currentText(); }
QString ConstantsWidget::searchText() const { return m_filter->text(); }
void ConstantsWidget::restoreState(const QString& domain, const QString& subdomain, const QString& searchText)
{
    if (!searchText.isNull())
        m_filter->setText(searchText);
    if (!domain.isEmpty()) {
        const int domainIndex = m_domain->findText(domain);
        if (domainIndex >= 0)
            m_domain->setCurrentIndex(domainIndex);
    }
    refreshSubdomains();
    if (!subdomain.isEmpty()) {
        const int subdomainIndex = m_subdomain->findText(subdomain);
        if (subdomainIndex >= 0)
            m_subdomain->setCurrentIndex(subdomainIndex);
    }
    filter();
}

void ConstantsWidget::handleRadixCharacterChange()
{
    updateList();
}

void ConstantsWidget::retranslateText()
{
    m_domainLabel->setText(tr("Domain"));
    m_subdomainLabel->setText(tr("Subdomain"));
    m_label->setText(tr("Search"));
    m_noMatchLabel->setText(tr("No match found"));

    QStringList titles;
    const QString name = tr("Name");
    const QString value = tr("Value");
    const QString unit = tr("Unit");
    if (layoutDirection() == Qt::LeftToRight)
        titles << name << value << unit;
    else
        titles << name << unit << value;
    m_list->setHeaderLabels(titles);

    updateList();
}

void ConstantsWidget::filter()
{
    const QList<Constant> &clist = Constants::instance()->list();
    const char radixChar = Settings::instance()->radixCharacter();
    QString term = m_filter->text();

    m_filterTimer->stop();
    setUpdatesEnabled(false);

    const QString chosenDomain = m_domain->currentText();
    const QString chosenSubdomain = m_subdomain->currentText();

    m_list->clear();
    for (int k = 0; k < clist.count(); ++k) {
        QStringList str;
        str << clist.at(k).name;
        QString radCh = (radixChar != MathDsl::DotSep) ?
            QString(clist.at(k).value).replace(MathDsl::DotSep, radixChar)
            : clist.at(k).value;

        if (layoutDirection() == Qt::RightToLeft) {
            QString normalizedUnit = clist.at(k).unit;
            normalizedUnit.replace(UnicodeChars::MiddleDot, MathDsl::MulDotOp);
            str << normalizedUnit + UnicodeChars::LeftToRightMark;
            str << radCh;
        } else {
            str << radCh;
            QString normalizedUnit = clist.at(k).unit;
            normalizedUnit.replace(UnicodeChars::MiddleDot, MathDsl::MulDotOp);
            str << normalizedUnit;
        }

        bool include = (chosenDomain == tr("All")) ?
            true : (clist.at(k).domain == chosenDomain);
        if (include && chosenSubdomain != tr("All"))
            include = (clist.at(k).subdomain == chosenSubdomain);

        if (!include)
            continue;

        QTreeWidgetItem* item = nullptr;
        if (term.isEmpty())
            item = new QTreeWidgetItem(m_list, str);
        else if (clist.at(k).name.contains(term, Qt::CaseInsensitive))
            item = new QTreeWidgetItem(m_list, str);
        if (item) {
            item->setData(0, Qt::UserRole, constantExpression(clist.at(k)));

            QString tip;
            tip += QString(UnicodeChars::LeftToRightMark);
            tip += QString("<b>%1</b><br>%2")
                .arg(clist.at(k).name, clist.at(k).value);
            tip += QString(UnicodeChars::LeftToRightMark);
            if (!clist.at(k).unit.isEmpty())
                tip.append(" ").append(QString(clist.at(k).unit).replace(
                    UnicodeChars::MiddleDot, MathDsl::MulDotOp));
            if (radixChar != MathDsl::DotSep)
                tip.replace(MathDsl::DotSep, radixChar);
            tip += QString(UnicodeChars::LeftToRightMark);
            item->setToolTip(0, tip);
            item->setToolTip(1, tip);
            item->setToolTip(2, tip);

            if (layoutDirection() == Qt::RightToLeft) {
                item->setTextAlignment(1, Qt::AlignRight);
                item->setTextAlignment(2, Qt::AlignLeft);
            } else {
                item->setTextAlignment(1, Qt::AlignLeft);
                item->setTextAlignment(2, Qt::AlignLeft);
            }
        }
    }

    m_list->resizeColumnToContents(0);
    m_list->resizeColumnToContents(1);
    m_list->resizeColumnToContents(2);

    if (m_list->topLevelItemCount() > 0) {
        m_noMatchLabel->hide();
        m_list->sortItems(0, Qt::AscendingOrder);
    } else {
        m_noMatchLabel->setGeometry(m_list->geometry());
        m_noMatchLabel->show();
        m_noMatchLabel->raise();
    }

    setUpdatesEnabled(true);
}

void ConstantsWidget::handleItem(QTreeWidgetItem* item)
{
    emit constantSelected(item->data(0, Qt::UserRole).toString());
}

void ConstantsWidget::triggerFilter()
{
    m_filterTimer->stop();
    m_filterTimer->start();
}

void ConstantsWidget::updateList()
{
    const int chosenDomainIndex = m_domain->currentIndex();
    const QString chosenDomain = m_domain->currentText();
    const int chosenSubdomainIndex = m_subdomain->currentIndex();
    const QString chosenSubdomain = m_subdomain->currentText();

    m_domain->clear();
    Constants::instance()->retranslateText();
    m_domain->addItems(Constants::instance()->domains());
    m_domain->insertItem(0, tr("All"));

    int domainIndex = m_domain->findText(chosenDomain);
    if (domainIndex < 0
        && chosenDomainIndex >= 0
        && chosenDomainIndex < m_domain->count()) {
        domainIndex = chosenDomainIndex;
    }
    if (domainIndex < 0) {
        const QString defaultDomain = Constants::instance()->domains().value(0);
        domainIndex = m_domain->findText(defaultDomain);
    }
    if (domainIndex < 0)
        domainIndex = 0;
    m_domain->setCurrentIndex(domainIndex);

    refreshSubdomains();

    int subdomainIndex = m_subdomain->findText(chosenSubdomain);
    if (subdomainIndex < 0
        && chosenSubdomainIndex >= 0
        && chosenSubdomainIndex < m_subdomain->count()) {
        subdomainIndex = chosenSubdomainIndex;
    }
    if (subdomainIndex >= 0)
        m_subdomain->setCurrentIndex(subdomainIndex);

    filter();
}

void ConstantsWidget::handleDomainChanged()
{
    refreshSubdomains();
    filter();
}

void ConstantsWidget::refreshSubdomains()
{
    m_subdomain->clear();
    const QString chosenDomain = m_domain->currentText();
    if (chosenDomain == tr("All")) {
        m_subdomain->insertItem(0, tr("All"));
        m_subdomain->setCurrentIndex(0);
        return;
    }

    m_subdomain->addItems(Constants::instance()->subdomains(chosenDomain));
    m_subdomain->insertItem(0, tr("All"));
    m_subdomain->setCurrentIndex(0);
}

void ConstantsWidget::changeEvent(QEvent* e)
{
    if (e->type() == QEvent::LanguageChange) {
        Constants::instance()->retranslateText();
        retranslateText();
    }
    else
        QWidget::changeEvent(e);
}
