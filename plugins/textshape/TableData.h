/* This file is part of the KDE project
 * Copyright (C) 2009 Elvis Stansvik <elvstone@gmail.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef TABLEDATA_H
#define TABLEDATA_H

#include <QtGlobal>
#include <QVector>
#include <QPointF>

class QRectF;
class QTextTableCell;

/**
 * @brief Table data class.
 *
 * This class holds layout helper data for table layout.
 *
 * \sa TableLayout
 */
class TableData
{
public:
    /// Constructor.
    TableData();

private:
    friend class TestTableLayout; // To allow direct testing.
    friend class TableLayout;     // To allow direct manipulation during layout.

    QVector<qreal> m_columnWidths;     /**< Column widths. */
    QVector<qreal> m_columnPositions;  /**< Column positions along X axis. */
    QVector<qreal> m_rowHeights;       /**< Row heights. */
    QVector<qreal> m_rowPositions;     /**< Row positions along Y axis. */

    QVector<QVector<qreal> > m_contentHeights;  /**< Cell content heights. */

    qreal m_width;       /**< Table width. */
    qreal m_height;      /**< Table height. */
    enum TableModels {Collapsing, Seperating};
    TableModels tableModel;
};

#endif // TABLEDATA_H
