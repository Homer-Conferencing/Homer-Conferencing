/*****************************************************************************
 *
 * Copyright (C) 2008 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation for event management
 * Author:  Thomas Volkert
 * Since:   2008-12-09
 */

#include <MeetingEvents.h>

#include <string>

namespace Homer { namespace Conference {

using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

MeetingObservable::MeetingObservable()
{
    mMeetingObserver = NULL;
}

MeetingObservable::~MeetingObservable()
{

}

void MeetingObservable::notifyObservers(GeneralEvent *pEvent)
{
    if (mMeetingObserver != NULL)
        mMeetingObserver->handleMeetingEvent(pEvent);
}

void MeetingObservable::AddObserver(MeetingObserver *pObserver)
{
    mMeetingObserver = pObserver;
}

void MeetingObservable::DeleteObserver(MeetingObserver *pObserver)
{
    mMeetingObserver = NULL;
}

///////////////////////////////////////////////////////////////////////////////

EventManager::EventManager()
{
    mRemainingEvents = 0;
    for (int i = 0; i < MEETING_EVENT_QUEUE_LENGTH; i++)
        mEvents[i] = NULL;
}

EventManager::~EventManager()
{

}

bool EventManager::Fire(GeneralEvent* pEvent)
{
    bool tResult = false;

    // lock
    // return false if timeout occurred while waiting for mutex
    if (!mMutex.tryLock(100))
        return false;

    if (mRemainingEvents < MEETING_EVENT_QUEUE_LENGTH)
    {
        // free memory of processed event
        if (mEvents[mRemainingEvents] != NULL)
            delete mEvents[mRemainingEvents];

        mEvents[mRemainingEvents] = pEvent;
        mRemainingEvents++;
        tResult = true;
    }else
        tResult = false;

    // unlock
    mMutex.unlock();
    return tResult;
}

GeneralEvent* EventManager::Scan()
{
    GeneralEvent* tEvent;

    // lock
    mMutex.lock();

    if (mRemainingEvents)
    {
        mRemainingEvents--;
        tEvent = mEvents[mRemainingEvents];
    }else
        tEvent = NULL;

    // unlock
    mMutex.unlock();

    return tEvent;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
