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
    RemoveAll();
}

Requirements::Requirements(Requirements &pCopy)
{
    Add(pCopy.GetAll());
}

///////////////////////////////////////////////////////////////////////////////

string Requirements::GetDescription()
{
    std::string tResult = "";
    RequirementSet::iterator tIt;

    mRequirementSetMutex.lock();

    int tRemainingRequs = mRequirementSet.size();
    for(tIt = mRequirementSet.begin(); tIt != mRequirementSet.end(); tIt++)
    {
        tRemainingRequs--;
        tResult += (*tIt)->GetDescription();
        if(tRemainingRequs > 0)
        {
            tResult += ",";
        }
    }

    mRequirementSetMutex.unlock();

    return tResult;
}

//void Requirements::operator+=(IRequirement &pAddRequ)
//{
//    add(pAddRequ);
//}
//
//void Requirements::operator|=(IRequirement &pAddRequ)
//{
//    add(pAddRequ);
//}
//
//Requirements& Requirements::operator|(IRequirement &pAddRequ)
//{
//    Requirements *tResult = new Requirements(*this);
//    tResult->add(pAddRequ);
//    return *tResult;
//}
//
//Requirements& Requirements::operator+(IRequirement &pAddRequ)
//{
//    Requirements *tResult = new Requirements(*this);
//    tResult->add(pAddRequ);
//    return *tResult;
//}
//
bool Requirements::Add(IRequirement *pAddRequ)
{
    bool tResult = true;
    RequirementSet::iterator tIt;

    LOG(LOG_VERBOSE, "Adding requirement %s", pAddRequ->GetDescription().c_str());

    mRequirementSetMutex.lock();

    // do we already know this type of requirement?
    for(tIt = mRequirementSet.begin(); tIt != mRequirementSet.end(); tIt++)
    {
        if((*tIt)->GetType() == pAddRequ->GetType())
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

void Requirements::Add(RequirementSet pSet)
{
    RequirementSet::iterator tIt;

    mRequirementSetMutex.lock();

    for (tIt = pSet.begin(); tIt != pSet.end(); tIt++)
    {
        mRequirementSet.push_back(*tIt);
    }

    mRequirementSetMutex.unlock();
}

bool Requirements::Contains(int pType)
{
    bool tResult = false;
    RequirementSet::iterator tIt;

    mRequirementSetMutex.lock();

    for(tIt = mRequirementSet.begin(); tIt != mRequirementSet.end(); tIt++)
    {
        if((*tIt)->GetType() == pType)
        {
            tResult = true;
        }
    }

    mRequirementSetMutex.unlock();

    return tResult;
}

IRequirement* Requirements::Get(int pType)
{
    IRequirement* tResult = NULL;
    RequirementSet::iterator tIt;

    mRequirementSetMutex.lock();

    for(tIt = mRequirementSet.begin(); tIt != mRequirementSet.end(); tIt++)
    {
        if((*tIt)->GetType() == pType)
        {
            // return first occurrence
            tResult = (*tIt);
            break;
        }
    }

    mRequirementSetMutex.unlock();

    return tResult;
}

RequirementSet Requirements::GetAll()
{
    return mRequirementSet;
}

void Requirements::RemoveAll()
{
    LOG(LOG_VERBOSE, "Removing stored requirements");

    RequirementSet::iterator tIt;

    mRequirementSetMutex.lock();

    for(tIt = mRequirementSet.begin(); tIt != mRequirementSet.end(); tIt++)
    {
        delete (*tIt);
    }

    mRequirementSetMutex.unlock();
}
///////////////////////////////////////////////////////////////////////////////

}} //namespace
