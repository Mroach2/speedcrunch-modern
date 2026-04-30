// SPDX-FileCopyrightText: 2013-2014, 2026 SpeedCrunch developers
// SPDX-License-Identifier: GPL-2.0-or-later


#ifndef CORE_BOOK_H
#define CORE_BOOK_H

#include "core/pageserver.h"

class Book : public PageServer {
    Q_OBJECT

public:
    explicit Book(QObject* parent = 0) : PageServer(parent) { createPages(); }
    virtual void createPages();

private:
    Q_DISABLE_COPY(Book)
};

#endif // CORE_BOOK_H
