// SPDX-FileCopyrightText: 2014, 2016, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#include "core/pageserver.h"

QString PageServer::getPageContent(const QString& id)
{
    PageMaker maker = m_toc.value(id);
    if (!maker)
        return QString();
    m_currentPageID = id;
    return maker();
}

QString PageServer::getCurrentPageContent()
{
    if (m_currentPageID.isNull())
        return QString();
    return getPageContent(m_currentPageID);
}
