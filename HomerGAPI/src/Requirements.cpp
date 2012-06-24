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
 * Since:   2011-12-08
 */

#include <GAPI.h>
#include <Requirements.h>
#include <IRequirement.h>

#include <Logger.h>

#include <string>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Requirements::Requirements()
{
}

Requirements::~Requirements()
{
}

///////////////////////////////////////////////////////////////////////////////

string Requirements::getDescription()
{
    std::string tResult = "";
    RequirementSet::iterator tIt;

    mRequirementSetMutex.lock();

    int tRemainingRequs = mRequirementSet.size();
    for(tIt = mRequirementSet.begin(); tIt != mRequirementSet.end(); tIt++)
    {
        tRemainingRequs--;
        tResult += (*tIt)->getDescription();
        if(tRemainingRequs > 0)
        {
            tResult += ",";
        }
    }

    mRequirementSetMutex.unlock();

    return tResult;
}

bool Requirements::add(IRequirement *pAddRequ)
{
    bool tResult = true;
    RequirementSet::iterator tIt;

    LOG(LOG_VERBOSE, "Adding requirement %s", pAddRequ->getDescription().c_str());

    mRequirementSetMutex.lock();

    // do we already know this type of requirement?
    for(tIt = mRequirementSet.begin(); tIt != mRequirementSet.end(); tIt++)
    {
        if((*tIt)->getType() == pAddRequ->getType())
        {
            tResult = false;
            LOG(LOG_WARN, "Requirement is a duplicate of an already known");
        }
    }

    // is everything okay to add given requirement?
    if(tResult)
    {
        mRequirementSet.push_back(pAddRequ);
    }

    mRequirementSetMutex.unlock();

    return tResult;
}

bool Requirements::contains(int pType)
{
    bool tResult = false;
    RequirementSet::iterator tIt;

    mRequirementSetMutex.lock();

    for(tIt = mRequirementSet.begin(); tIt != mRequirementSet.end(); tIt++)
    {
        if((*tIt)->getType() == pType)
        {
            tResult = true;
        }
    }

    mRequirementSetMutex.unlock();

    return tResult;
}

IRequirement* Requirements::get(int pType)
{
    IRequirement* tResult = NULL;
    RequirementSet::iterator tIt;

    mRequirementSetMutex.lock();

    for(tIt = mRequirementSet.begin(); tIt != mRequirementSet.end(); tIt++)
    {
        if((*tIt)->getType() == pType)
        {
            // return first occurrence
            tResult = (*tIt);
            break;
        }
    }

    mRequirementSetMutex.unlock();

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
