/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
 *
 * This software is free software.
 * Your are allowed to redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This source is published in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License version 2
 * along with this program. Otherwise, you can write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 * Alternatively, you find an online version of the license text under
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 *****************************************************************************/

/*
 * Purpose: modified QLabel for linked pixmaps
 * Author:  Thomas Volkert
 * Since:   2012-08-21
 */

#ifndef LINKED_PIXMAP_LABEL_H
#define LINKED_PIXMAP_LABEL_H

#include <QLabel>
#include <QString>
#include <QMouseEvent>
#include <QWidget>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class LinkedPixmapLabel :
    public QLabel
{
    Q_OBJECT;
public:
    /// The default constructor
    LinkedPixmapLabel(QWidget* pParent = NULL);

    /// The destructor.
    virtual ~LinkedPixmapLabel();

    void Init(QString pLink);

private:
    virtual void mousePressEvent(QMouseEvent *pEvent);

    QString mLink;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
