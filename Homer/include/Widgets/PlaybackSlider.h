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
 * Purpose: modified QSlider for file playback
 * Since:   2012-07-29
 */

#ifndef PLAYBACK_SLIDER_H
#define PLAYBACK_SLIDER_H

#include <QSlider>
#include <QMouseEvent>
#include <QWidget>
#include <QContextMenuEvent>

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

class ParticipantWidget;
class PlaybackSlider :
    public QSlider
{
    Q_OBJECT;
public:
    /// The default constructor
    PlaybackSlider(QWidget* pParent = NULL);

    /// The destructor.
    virtual ~PlaybackSlider();

    void Init(ParticipantWidget *pParticipantWidget);

private:
    virtual void mousePressEvent(QMouseEvent *pEvent);
    virtual void contextMenuEvent(QContextMenuEvent *pContextMenuEvent);

    ParticipantWidget   *mParticipantWidget;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
