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
 * Purpose: QEvent management
 * Since:   2012-07-03
 */

#ifndef _QMEETING_EVENTS_
#define _QMEETING_EVENTS_

#include <MeetingEvents.h>

using namespace std;
using namespace Homer::Conference;

namespace Homer { namespace Gui {

///////////////////////////////////////////////////////////////////////////////

#define ADD_VIDEO_RELAY                         300000
#define ADD_VIDEO_PREVIEW                       300001

///////////////////////////////////////////////////////////////////////////////

class QMeetingEvent: public QEvent {
public:
    QMeetingEvent(GeneralEvent *pEvent) :
        QEvent(QEvent::User) {
        mMeetingEvent = pEvent;
    }
    virtual ~QMeetingEvent() {
    }
public:
    GeneralEvent* getEvent() {
        return mMeetingEvent;
    }
private:
    GeneralEvent *mMeetingEvent;
};

///////////////////////////////////////////////////////////////////////////////

class AddVideoRelayEvent:
    public Homer::Conference::TEvent<AddVideoRelayEvent, ADD_VIDEO_RELAY>
{
public:
    AddVideoRelayEvent()
    {

    }
};

class AddVideoPreviewEvent:
	public Homer::Conference::TEvent<AddVideoPreviewEvent, ADD_VIDEO_PREVIEW>
{
public:
	AddVideoPreviewEvent()
	{

	}
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
