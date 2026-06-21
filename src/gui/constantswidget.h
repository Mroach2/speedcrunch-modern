// SPDX-FileCopyrightText: 2009-2011, 2013-2014, 2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef GUI_CONSTANTSWIDGET_H
#define GUI_CONSTANTSWIDGET_H

#include <QColor>
#include <QWidget>

class QSize;
class QComboBox;
class QEvent;
class QFrame;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPoint;
class QResizeEvent;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

class ConstantsWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConstantsWidget(QWidget* parent = nullptr);
    ~ConstantsWidget();
    QSize minimumSizeHint() const override;
    QString selectedDomain() const;
    QString selectedSubdomain() const;
    QString searchText() const;
    void restoreState(const QString& domain, const QString& subdomain, const QString& searchText);
    void setSummaryPopupThemeColors(const QColor& background,
                                    const QColor& foreground,
                                    const QColor& outline,
                                    int cornerRadius);

signals:
    void constantSelected(const QString&);

public slots:
    void handleRadixCharacterChange();

protected slots:
    void filter();
    void handleDomainChanged();
    void handleItem(QTreeWidgetItem*);
    void refreshSubdomains();
    void retranslateText();
    void triggerFilter();
    void updateList();

protected:
    void changeEvent(QEvent*) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void applySummaryPopupTheme();
    void ensureSummaryPopup();
    void hideSummaryPopup();
    void scheduleEmptyHeaderStretch();
    void showSummaryPopup(QTreeWidgetItem* item, int column, const QPoint& globalPos);
    void updateSummaryPopupForViewportPosition(const QPoint& viewportPos,
                                               const QPoint& globalPos);
    void updateSummaryPopupMask();
    void updateEmptyHeaderStretch();
    void updateDomainLayout();
    void updateDomainLabelAlignment();

    Q_DISABLE_COPY(ConstantsWidget)

    QComboBox* m_domain;
    QLabel* m_domainLabel;
    QComboBox* m_subdomain;
    QLabel* m_subdomainLabel;
    QLineEdit* m_filter;
    QTimer* m_filterTimer;
    QLabel* m_label;
    QTreeWidget* m_list;
    QLabel* m_noMatchLabel;
    QWidget* m_domainBox;
    QHBoxLayout* m_domainLayout;
    QWidget* m_domainRow1;
    QHBoxLayout* m_domainRow1Layout;
    QWidget* m_domainRow2;
    QHBoxLayout* m_domainRow2Layout;
    QFrame* m_summaryPopup = nullptr;
    QLabel* m_summaryPopupLabel = nullptr;
    QColor m_summaryPopupBackgroundColor;
    QColor m_summaryPopupForegroundColor;
    QColor m_summaryPopupOutlineColor;
    int m_summaryPopupCornerRadius = 0;
    bool m_isCompactDomainLayout = false;
    bool m_domainLayoutInitialized = false;
    bool m_emptyHeaderStretchQueued = false;
};

#endif
