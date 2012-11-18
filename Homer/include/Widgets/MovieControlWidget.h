/*****************************************************************************
 *
 * Copyright (C) 2012 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: movie control widget
 * Author:  Thomas Volkert
 * Since:   2012-11-16
 */

#ifndef _MOVIE_CONTROL_WIDGET_
#define _MOVIE_CONTROL_WIDGET_

#include <Widgets/PlaybackSlider.h>
#include <QWidget>

#include <ui_MovieControlWidget.h>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////
class MovieControlWidget:
    public QWidget,
    public Ui_MovieControlWidget
{
    Q_OBJECT;

public:
    MovieControlWidget(QWidget *pParent);

    virtual ~MovieControlWidget();

private:
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
