// SPDX-FileCopyrightText: 2009-2011, 2014, 2016, 2021, 2024, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "gui/constantswidget.h"

#include "core/constants.h"
#include "core/settings.h"
#include "core/unicodechars.h"
#include "core/mathdsl.h"
#include "gui/dockliststyle.h"
#include "gui/themedlineedit.h"
#include "gui/uiconfig.h"

#include <QAbstractScrollArea>
#include <QBitmap>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
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
    if (constant.domainType == ConstantDomain::Mathematics
        && constant.unit.isEmpty()
        && constant.value == QStringLiteral("3.14159265358979323846264338327950288419716939937511"))
        return QString(UnicodeChars::Pi);
    if (constant.domainType == ConstantDomain::Mathematics
        && constant.unit.isEmpty()
        && constant.value == QStringLiteral("2.71828182845904523536028747135266249775724709369996"))
        return QStringLiteral("e");

    QString unit = constant.unit;
    unit.replace(UnicodeChars::MiddleDot, MathDsl::MulDotOp);
    return constant.unit.isEmpty()
        ? constant.value
        : QStringLiteral("%1%2[%3]")
            .arg(constant.value, QString(MathDsl::QuantSp), unit);
}

static QString displayValue(const Constant& constant)
{
    if (constant.domainType != ConstantDomain::Mathematics)
        return constant.value;

    const int dotPos = constant.value.indexOf(MathDsl::DotSep);
    if (dotPos < 0)
        return constant.value;

    return constant.value.left(qMin(constant.value.length(), dotPos + 16));
}

static void applyRoundedPopupMask(QWidget* popup, int cornerRadius)
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

static QPoint constrainedPopupPosition(QWidget* anchor,
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

ConstantsWidget::ConstantsWidget(QWidget* parent)
    : QWidget(parent)
{
    constexpr int kControlRowHorizontalPadding = 8;
    constexpr int kControlRowVerticalPadding = 6;

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

    m_domainBox = new QWidget(this);
    m_domainLayout = new QHBoxLayout;
    m_domainBox->setLayout(m_domainLayout);
    m_domainLayout->setContentsMargins(kControlRowHorizontalPadding,
                                       kControlRowVerticalPadding,
                                       kControlRowHorizontalPadding,
                                       kControlRowVerticalPadding);
    m_domainLayout->setSpacing(6);

    m_domainRow1 = new QWidget(this);
    m_domainRow1Layout = new QHBoxLayout;
    m_domainRow1->setLayout(m_domainRow1Layout);
    m_domainRow1Layout->setContentsMargins(0, 0, 0, 0);
    m_domainRow1Layout->setSpacing(6);
    m_domainRow1Layout->addWidget(m_domainLabel);
    m_domainRow1Layout->addWidget(m_domain);

    m_domainRow2 = new QWidget(this);
    m_domainRow2Layout = new QHBoxLayout;
    m_domainRow2->setLayout(m_domainRow2Layout);
    m_domainRow2Layout->setContentsMargins(0, 0, 0, 0);
    m_domainRow2Layout->setSpacing(6);
    m_domainRow2Layout->addWidget(m_subdomainLabel);
    m_domainRow2Layout->addWidget(m_subdomain);

    m_label = new QLabel(this);

    m_filter = new ThemedLineEdit(this);
    m_filter->setMinimumWidth(fontMetrics().horizontalAdvance('X') * 10);
    m_filter->setClearButtonEnabled(true);

    connect(m_filter, SIGNAL(textChanged(const QString &)), SLOT(triggerFilter()));

    QWidget* searchBox = new QWidget(this);
    QHBoxLayout* searchLayout = new QHBoxLayout;
    searchBox->setLayout(searchLayout);
    searchLayout->addWidget(m_label);
    searchLayout->addWidget(m_filter);
    searchLayout->setContentsMargins(kControlRowHorizontalPadding,
                                     kControlRowVerticalPadding,
                                     kControlRowHorizontalPadding,
                                     kControlRowVerticalPadding);

    m_list = new QTreeWidget(this);
    m_list->setAutoScroll(true);
    m_list->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setColumnCount(3);
    m_list->setRootIsDecorated(false);
    m_list->setMouseTracking(true);
    m_list->viewport()->setMouseTracking(true);
    m_list->viewport()->setAttribute(Qt::WA_Hover, true);
    m_list->setEditTriggers(QTreeWidget::NoEditTriggers);
    m_list->setSelectionBehavior(QTreeWidget::SelectRows);
    m_list->header()->setStretchLastSection(false);
    DockListStyle::apply(m_list);
    m_list->installEventFilter(this);
    m_list->viewport()->installEventFilter(this);
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ConstantsWidget::hideSummaryPopup);
    connect(m_list->horizontalScrollBar(), &QScrollBar::valueChanged,
            this, &ConstantsWidget::hideSummaryPopup);

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
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_domainBox);
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
    updateDomainLayout();

    setFocusProxy(m_filter);

    filter();
}

ConstantsWidget::~ConstantsWidget()
{
    m_filterTimer->stop();
    hideSummaryPopup();
}

QSize ConstantsWidget::minimumSizeHint() const
{
    QSize hint = QWidget::minimumSizeHint();
    hint.setWidth(UiConfig::ConstantsDockMinimumWidth);
    return hint;
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

void ConstantsWidget::setSummaryPopupThemeColors(const QColor& background,
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

    updateDomainLabelAlignment();
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

    hideSummaryPopup();
    m_list->clear();
    for (int k = 0; k < clist.count(); ++k) {
        QStringList str;
        str << clist.at(k).name;
        const QString value = displayValue(clist.at(k));
        QString radCh = (radixChar != MathDsl::DotSep) ?
            QString(value).replace(MathDsl::DotSep, radixChar)
            : value;

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

    const bool hasMatches = m_list->topLevelItemCount() > 0;
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_list->resizeColumnToContents(0);
    m_list->resizeColumnToContents(1);
    m_list->resizeColumnToContents(2);

    if (hasMatches) {
        m_noMatchLabel->hide();
        m_list->sortItems(0, Qt::AscendingOrder);
    } else {
        updateEmptyHeaderStretch();
        scheduleEmptyHeaderStretch();
        DockListStyle::showCenteredNoMatchLabel(m_list, m_noMatchLabel);
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
        m_subdomainLabel->setVisible(true);
        m_subdomain->setVisible(true);
        return;
    }

    const QStringList subdomains = Constants::instance()->subdomains(chosenDomain);
    m_subdomain->addItems(subdomains);
    m_subdomain->insertItem(0, tr("All"));
    m_subdomain->setCurrentIndex(0);

    const bool hasConfiguredSubdomains = !subdomains.isEmpty();
    m_subdomainLabel->setVisible(hasConfiguredSubdomains);
    m_subdomain->setVisible(hasConfiguredSubdomains);
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

bool ConstantsWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_list || watched == m_list->viewport()) {
        switch (event->type()) {
        case QEvent::ToolTip: {
            QHelpEvent* helpEvent = static_cast<QHelpEvent*>(event);
            const QPoint viewportPos = watched == m_list->viewport()
                ? helpEvent->pos()
                : m_list->viewport()->mapFrom(m_list, helpEvent->pos());
            updateSummaryPopupForViewportPosition(viewportPos, helpEvent->globalPos());
            return true;
        }
        case QEvent::MouseMove: {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
            const QPoint viewportPos = watched == m_list->viewport()
                ? mouseEvent->pos()
                : m_list->viewport()->mapFrom(m_list, mouseEvent->pos());
            updateSummaryPopupForViewportPosition(viewportPos,
                                                  mouseEvent->globalPosition().toPoint());
            break;
        }
        case QEvent::HoverMove: {
            QHoverEvent* hoverEvent = static_cast<QHoverEvent*>(event);
            const QPoint viewportPos = watched == m_list->viewport()
                ? hoverEvent->position().toPoint()
                : m_list->viewport()->mapFrom(m_list, hoverEvent->position().toPoint());
            updateSummaryPopupForViewportPosition(viewportPos,
                                                  m_list->viewport()->mapToGlobal(viewportPos));
            break;
        }
        case QEvent::Hide:
        case QEvent::KeyPress:
        case QEvent::Leave:
        case QEvent::MouseButtonDblClick:
        case QEvent::MouseButtonPress:
        case QEvent::Resize:
        case QEvent::Wheel:
            hideSummaryPopup();
            break;
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void ConstantsWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateDomainLayout();
    updateEmptyHeaderStretch();
    scheduleEmptyHeaderStretch();
}

void ConstantsWidget::ensureSummaryPopup()
{
    if (m_summaryPopup != nullptr)
        return;

    m_summaryPopup = new QFrame(this, Qt::ToolTip | Qt::FramelessWindowHint);
    m_summaryPopup->setObjectName(QStringLiteral("constantsSummaryPopup"));
    m_summaryPopup->setAutoFillBackground(true);
    m_summaryPopup->setAttribute(Qt::WA_ShowWithoutActivating, true);
    m_summaryPopup->setAttribute(Qt::WA_StyledBackground, true);
    m_summaryPopup->setFocusPolicy(Qt::NoFocus);
    m_summaryPopup->setFrameStyle(QFrame::NoFrame);

    QVBoxLayout* layout = new QVBoxLayout(m_summaryPopup);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(0);

    m_summaryPopupLabel = new QLabel(m_summaryPopup);
    m_summaryPopupLabel->setObjectName(QStringLiteral("constantsSummaryPopupLabel"));
    m_summaryPopupLabel->setTextFormat(Qt::RichText);
    m_summaryPopupLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_summaryPopupLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(m_summaryPopupLabel);

    applySummaryPopupTheme();
}

void ConstantsWidget::applySummaryPopupTheme()
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
        "QFrame#constantsSummaryPopup {"
        " background: %1; color: %2;"
        " border: %4px solid %3;"
        " border-radius: %5px;"
        "}"
        "QLabel#constantsSummaryPopupLabel {"
        " background: transparent; color: %2;"
        "}")
                                      .arg(background.name(),
                                           foreground.name(),
                                           outline.name())
                                      .arg(UiConfig::PopupOutlineStrokeWidth)
                                      .arg(cornerRadius));
    updateSummaryPopupMask();
}

void ConstantsWidget::hideSummaryPopup()
{
    if (m_summaryPopup != nullptr)
        m_summaryPopup->hide();
}

void ConstantsWidget::showSummaryPopup(QTreeWidgetItem* item,
                                       int column,
                                       const QPoint& globalPos)
{
    if (item == nullptr)
        return;

    const QString tip = item->toolTip(column).isEmpty()
        ? item->toolTip(0)
        : item->toolTip(column);
    if (tip.isEmpty()) {
        hideSummaryPopup();
        return;
    }

    ensureSummaryPopup();
    m_summaryPopupLabel->setText(tip);
    m_summaryPopup->adjustSize();
    m_summaryPopup->resize(m_summaryPopup->sizeHint());
    updateSummaryPopupMask();
    m_summaryPopup->move(constrainedPopupPosition(m_list,
                                                  globalPos,
                                                  m_summaryPopup->size()));
    m_summaryPopup->show();
}

void ConstantsWidget::updateSummaryPopupForViewportPosition(const QPoint& viewportPos,
                                                            const QPoint& globalPos)
{
    const QModelIndex index = m_list->indexAt(viewportPos);
    QTreeWidgetItem* item = index.isValid() ? m_list->itemAt(viewportPos) : nullptr;
    if (item == nullptr) {
        hideSummaryPopup();
        return;
    }

    showSummaryPopup(item, index.column(), globalPos);
}

void ConstantsWidget::updateSummaryPopupMask()
{
    if (m_summaryPopup == nullptr)
        return;

    applyRoundedPopupMask(m_summaryPopup, qMax(0, m_summaryPopupCornerRadius));
}

void ConstantsWidget::scheduleEmptyHeaderStretch()
{
    if (m_emptyHeaderStretchQueued
        || m_list == nullptr
        || m_list->topLevelItemCount() > 0) {
        return;
    }

    m_emptyHeaderStretchQueued = true;
    QTimer::singleShot(0, this, [this]() {
        m_emptyHeaderStretchQueued = false;
        updateEmptyHeaderStretch();
    });
}

void ConstantsWidget::updateEmptyHeaderStretch()
{
    if (m_list == nullptr || m_list->topLevelItemCount() > 0)
        return;

    QHeaderView* header = m_list->header();
    if (header == nullptr || header->count() < 3)
        return;

    const int fixedWidth = header->sectionSize(0) + header->sectionSize(1);
    const int targetWidth = qMax(header->width(), m_list->viewport()->width());
    if (targetWidth <= fixedWidth)
        return;

    header->resizeSection(2, qMax(header->sectionSizeHint(2), targetWidth - fixedWidth));
}

void ConstantsWidget::updateDomainLayout()
{
    const int minInlineWidthByControls =
        m_domainLabel->sizeHint().width() + m_domain->minimumSizeHint().width()
        + m_subdomainLabel->sizeHint().width() + m_subdomain->minimumSizeHint().width()
        + (m_domainLayout->spacing() * 3);
    // Side docks are often still wide enough to satisfy minimum hints while visually cramped.
    // Use both control hints and a practical width cap so right/left panes stack reliably.
    const int practicalInlineWidth = 560;
    const int availableWidth = m_domainBox->width() > 0 ? m_domainBox->width() : width();
    const bool compact = availableWidth < std::max(minInlineWidthByControls, practicalInlineWidth);
    if (m_domainLayoutInitialized && compact == m_isCompactDomainLayout)
        return;

    m_domainLayoutInitialized = true;
    m_isCompactDomainLayout = compact;
    while (QLayoutItem* item = m_domainLayout->takeAt(0))
        delete item;

    if (compact) {
        QVBoxLayout* stacked = new QVBoxLayout;
        stacked->setContentsMargins(0, 0, 0, 0);
        stacked->setSpacing(4);
        stacked->addWidget(m_domainRow1);
        stacked->addWidget(m_domainRow2);
        m_domainLayout->addLayout(stacked);
    } else {
        m_domainLayout->addWidget(m_domainRow1);
        m_domainLayout->addWidget(m_domainRow2);
    }

    updateDomainLabelAlignment();
}

void ConstantsWidget::updateDomainLabelAlignment()
{
    if (m_isCompactDomainLayout) {
        const int labelWidth = std::max(m_domainLabel->sizeHint().width(),
                                        m_subdomainLabel->sizeHint().width());
        m_domainLabel->setFixedWidth(labelWidth);
        m_subdomainLabel->setFixedWidth(labelWidth);
    } else {
        m_domainLabel->setMinimumWidth(0);
        m_domainLabel->setMaximumWidth(QWIDGETSIZE_MAX);
        m_subdomainLabel->setMinimumWidth(0);
        m_subdomainLabel->setMaximumWidth(QWIDGETSIZE_MAX);
    }
}
