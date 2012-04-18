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
 * Purpose: Requirements
 * Author:  Thomas Volkert
 * Since:   2012-04-18
 */

#ifndef _GAPI_EVENT_
#define _GAPI_EVENT_

// to simplify including of all requirements
//#include <RequirementTransmitLossless.h>
//#include <RequirementTransmitChunks.h>
//#include <RequirementTransmitStream.h>
//#include <RequirementTransmitBitErrors.h>
//#include <RequirementTargetPort.h>
//#include <RequirementTransmitOrdered.h>
//#include <RequirementLimitDelay.h>
//#include <RequirementLimitDataRate.h>

#include <IEvent.h>
#include <HBMutex.h>

#include <list>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

typedef std::list<IEvent*> EventSet;

///////////////////////////////////////////////////////////////////////////////

class Events
{
public:
	Events();
	Events(Events &pCopy);
    virtual ~Events();

    virtual std::string getDescription();

    /* overloaded operators */
//    void operator+=(IRequirement pAddRequ);
//    void operator|=(IRequirement pAddRequ);
//    Requirements& operator+(IRequirement pAddRequ);
//    Requirements& operator|(IRequirement pAddRequ);

    /* set manipulation */
    bool add(IEvent *pRequ);

    /* query functions */
    bool contains(int pType);
    IEvent* get(int pType);

private:
    void add(EventSet pSet);
    EventSet getAll();
    void removeAll();

    EventSet      		mEventSet;
    Mutex               mEventSetMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
