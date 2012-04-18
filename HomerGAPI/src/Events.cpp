/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Events
 * Author:  Thomas Volkert
 * Since:   2012-04-18
 */

#include <GAPI.h>
#include <Events.h>
#include <IEvent.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Events::Events()
{
}

Events::~Events()
{
    removeAll();
}

Events::Events(Events &pCopy)
{
    add(pCopy.getAll());
}

///////////////////////////////////////////////////////////////////////////////

string Events::getDescription()
{
    std::string tResult = "";
    EventSet::iterator tIt;

    mEventSetMutex.lock();

    int tRemainingRequs = mEventSet.size();
    for(tIt = mEventSet.begin(); tIt != mEventSet.end(); tIt++)
    {
        tRemainingRequs--;
        tResult += (*tIt)->getDescription();
        if(tRemainingRequs > 0)
        {
            tResult += ",";
        }
    }

    mEventSetMutex.unlock();

    return tResult;
}

//void Events::operator+=(IEvent &pAddRequ)
//{
//    add(pAddRequ);
//}
//
//void Events::operator|=(IEvent &pAddRequ)
//{
//    add(pAddRequ);
//}
//
//Events& Events::operator|(IEvent &pAddRequ)
//{
//    Events *tResult = new Events(*this);
//    tResult->add(pAddRequ);
//    return *tResult;
//}
//
//Events& Events::operator+(IEvent &pAddRequ)
//{
//    Events *tResult = new Events(*this);
//    tResult->add(pAddRequ);
//    return *tResult;
//}
//
bool Events::add(IEvent *pAddRequ)
{
    bool tResult = true;
    EventSet::iterator tIt;

    LOG(LOG_VERBOSE, "Adding requirement %s", pAddRequ->getDescription().c_str());

    mEventSetMutex.lock();

    // do we already know this type of requirement?
    for(tIt = mEventSet.begin(); tIt != mEventSet.end(); tIt++)
    {
        if((*tIt)->getType() == pAddRequ->getType())
        {
            tResult = false;
            LOG(LOG_WARN, "Event is a duplicate of an already known");
        }
    }

    // is everything okay to add given requirement?
    if(tResult)
    {
        mEventSet.push_back(pAddRequ);
    }

    mEventSetMutex.unlock();

    return tResult;
}

void Events::add(EventSet pSet)
{
    EventSet::iterator tIt;

    mEventSetMutex.lock();

    for (tIt = pSet.begin(); tIt != pSet.end(); tIt++)
    {
        mEventSet.push_back(*tIt);
    }

    mEventSetMutex.unlock();
}

bool Events::contains(int pType)
{
    bool tResult = false;
    EventSet::iterator tIt;

    mEventSetMutex.lock();

    for(tIt = mEventSet.begin(); tIt != mEventSet.end(); tIt++)
    {
        if((*tIt)->getType() == pType)
        {
            tResult = true;
        }
    }

    mEventSetMutex.unlock();

    return tResult;
}

IEvent* Events::get(int pType)
{
    IEvent* tResult = NULL;
    EventSet::iterator tIt;

    mEventSetMutex.lock();

    for(tIt = mEventSet.begin(); tIt != mEventSet.end(); tIt++)
    {
        if((*tIt)->getType() == pType)
        {
            // return first occurrence
            tResult = (*tIt);
            break;
        }
    }

    mEventSetMutex.unlock();

    return tResult;
}

EventSet Events::getAll()
{
    return mEventSet;
}

void Events::removeAll()
{
    LOG(LOG_VERBOSE, "Removing stored requirements");

    EventSet::iterator tIt;

    mEventSetMutex.lock();

    for(tIt = mEventSet.begin(); tIt != mEventSet.end(); tIt++)
    {
        delete (*tIt);
    }

    mEventSetMutex.unlock();
}
///////////////////////////////////////////////////////////////////////////////

}} //namespace
