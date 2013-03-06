/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implemenation of process statistic service as singleton
 * Author:  Thomas Volkert
 * Since:   2010-11-07
*/
#include <ProcessStatisticService.h>

#include <Logger.h>
#include <HBThread.h>

#include <vector>

using namespace std;
using namespace Homer::Base;

namespace Homer { namespace Monitor {

ProcessStatisticService sProcessStatisticService;
bool sProcessStatisticSupported = true;

///////////////////////////////////////////////////////////////////////////////

ProcessStatisticService::ProcessStatisticService()
{
}

ProcessStatisticService::~ProcessStatisticService()
{

}

ProcessStatisticService& ProcessStatisticService::GetInstance()
{
    return sProcessStatisticService;
}

void ProcessStatisticService::DisableProcessStatisticSupport()
{
	sProcessStatisticSupported = false;
}

///////////////////////////////////////////////////////////////////////////////

ProcessStatistics ProcessStatisticService::GetProcessStatistics()
{
	ProcessStatistics tResult;
	ProcessStatistics::iterator tIt;

	if (!sProcessStatisticSupported)
		return tResult;

	UpdateThreadDatabase();

	// lock
    mProcessStatisticsMutex.lock();

    tResult = mProcessStatistics;

    // unlock
    mProcessStatisticsMutex.unlock();

    return tResult;
}

void ProcessStatisticService::UpdateThreadDatabase()
{
    ProcessStatistics tNewThreads;
    ProcessStatistics::iterator tDbIt;
    vector<int> tThreadIds;
    vector<int>::iterator tIt;

    if (!sProcessStatisticSupported)
		return;

    tThreadIds = Thread::GetTIds();

    // return immediately for BSD environments because HomerBase lacks support for thread statistics in BSD environments
	#if defined(BSD) && !defined(APPLE)
        return;//TODO
    #endif

	mUpdateThreadDataBaseMutex.lock();

    //LOG(LOG_VERBOSE, "Updating thread database");

    if(tThreadIds.size())
    {
        for (tIt = tThreadIds.begin(); tIt != tThreadIds.end(); tIt++)
        {
            bool tKnownThread = false;

            // lock
            mProcessStatisticsMutex.lock();

            // search if thread is already known
            for (tDbIt = mProcessStatistics.begin(); tDbIt != mProcessStatistics.end(); tDbIt++)
                if ((*tDbIt)->GetThreadStatisticId() == (*tIt))
                    tKnownThread = true;

            // unlock
            mProcessStatisticsMutex.unlock();

            // if it is a new thread then create a statistic for it (it automatically registers itself)
            if (!tKnownThread)
            {
                if (!SVC_PROCESS_STATISTIC.RegisterProcessStatistic(*tIt))
                    LOG(LOG_ERROR, "Error when registering process statistic");
            }
        }
    }

    //LOG(LOG_VERBOSE, "Deleting zombie entries in thread database");
    // find all registrations which represents disappeared threads
    bool tExists = false;
    do {
        // lock
        mProcessStatisticsMutex.lock();

        for (tDbIt = mProcessStatistics.begin(); tDbIt != mProcessStatistics.end(); tDbIt++)
        {
            tExists = false;
            int tThreadId = (*tDbIt)->GetThreadStatisticId();
            for (tIt = tThreadIds.begin(); tIt != tThreadIds.end(); tIt++)
            {
                if (tThreadId == (*tIt))
                {
                    tExists = true;
                    break;
                }
            }
            if (!tExists)
            {
                // delete the unneeded statistic object
                SVC_PROCESS_STATISTIC.UnregisterProcessStatistic(tThreadId);
                break;
            }
        }

        // unlock
        mProcessStatisticsMutex.unlock();

    }while(!tExists);

    //LOG(LOG_VERBOSE, "Thread database updated");

    mUpdateThreadDataBaseMutex.unlock();
}

void ProcessStatisticService::AssignThreadName(std::string pName)
{
	if (!sProcessStatisticSupported)
		return;

    int tCurrentTid = Thread::GetTId();
    ProcessStatistics::iterator tDbIt;
    bool tFound = false;

    UpdateThreadDatabase();

    // lock
    mProcessStatisticsMutex.lock();

    // search if thread is already known
    for (tDbIt = mProcessStatistics.begin(); tDbIt != mProcessStatistics.end(); tDbIt++)
    {
        if ((*tDbIt)->GetThreadStatisticId() == tCurrentTid)
        {
            tFound = true;
            (*tDbIt)->AssignThreadName(pName);
        }
    }
    // unlock
    mProcessStatisticsMutex.unlock();
}

bool ProcessStatisticService::RegisterProcessStatistic(int pThreadId)
{
	if (!sProcessStatisticSupported)
		return false;

    ProcessStatistics::iterator tIt;
    bool tFound = false;

    //LOG(LOG_VERBOSE, "Registering process statistic for thread %d", pThreadId);

    for (tIt = mProcessStatistics.begin(); tIt != mProcessStatistics.end(); tIt++)
    {
        if ((*tIt)->GetThreadStatisticId() == pThreadId)
        {
            //LOG(LOG_VERBOSE, "Statistic for thread %d already registered", pThreadId);
            tFound = true;
            break;
        }
    }

    if (!tFound)
    {
        ProcessStatistic *tStat = new ProcessStatistic(pThreadId);
        mProcessStatistics.push_back(tStat);
    }

    return true;
}

bool ProcessStatisticService::UnregisterProcessStatistic(int pThreadId)
{
	if (!sProcessStatisticSupported)
		return false;

    ProcessStatistics::iterator tIt;
    bool tFound = false;

    //LOG(LOG_VERBOSE, "Unregistering process statistic for thread %d", pThreadId);

    for (tIt = mProcessStatistics.begin(); tIt != mProcessStatistics.end(); tIt++)
    {
        if ((*tIt)->GetThreadStatisticId() == pThreadId)
        {
            tFound = true;
            mProcessStatistics.erase(tIt);
            //LOG(LOG_VERBOSE, "..unregistered");
            break;
        }
    }

    return tFound;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace
