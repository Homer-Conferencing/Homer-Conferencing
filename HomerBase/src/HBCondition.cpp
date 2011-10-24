/*****************************************************************************
 *
 * Copyright (C) 2008-2011 Homer-conferencing project
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
 * Name:    Condition.cpp
 * Purpose: Implementation of os independent condition handling
 * Author:  Thomas Volkert
 * Since:   2011-02-05
 * Version: $Id$
 */

#include <Logger.h>
#include <HBCondition.h>

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Condition::Condition()
{
	bool tResult = false;
    #if defined(LINUX) || defined(APPLE)
	    mCondition = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
		tResult = (pthread_cond_init(mCondition, NULL) == 0);
	#endif
	#ifdef WIN32
		mCondition = CreateEvent(NULL, true, false, NULL);
		tResult = (mCondition != NULL);
	#endif
	if (!tResult)
        LOG(LOG_ERROR, "Initiation of condition failed");
}

Condition::~Condition()
{
    bool tResult = false;
    #if defined(LINUX) || defined(APPLE)
		tResult = (pthread_cond_destroy(mCondition) == 0);
	    free(mCondition);
	#endif
	#ifdef WIN32
	    tResult = (CloseHandle(mCondition) != 0);
	#endif
    if (!tResult)
		LOG(LOG_ERROR, "Destruction of condition failed");
}

///////////////////////////////////////////////////////////////////////////////

bool Condition::Wait(Mutex *pMutex, int pTime)
{
    #if defined(LINUX) || defined(APPLE)
        struct timespec tTimeout;

        #if defined(LINUX)
            if (clock_gettime(CLOCK_REALTIME, &tTimeout) == -1)
                LOG(LOG_ERROR, "Failed to get time from clock");
        #endif
        #if defined(APPLE)
            // apple specific implementation for clock_gettime()
            clock_serv_t tCalenderClock;
            mach_timespec_t tMachTimeSpec;
            host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &tCalenderClock);
            clock_get_time(tCalenderClock, &tMachTimeSpec);
            mach_port_deallocate(mach_task_self(), tCalenderClock);
            tTimeout.tv_sec = tMachTimeSpec.tv_sec;
            tTimeout.tv_nsec = tMachTimeSpec.tv_nsec;
        #endif

        // add msecs to current time stamp
        tTimeout.tv_nsec += pTime * 1000 * 1000;

        if (pMutex)
            if (pTime > 0)
                return !pthread_cond_timedwait(mCondition, pMutex->mMutex, &tTimeout);
            else
                return !pthread_cond_wait(mCondition, pMutex->mMutex);
        else
        {
            pthread_mutex_t* tMutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
            pthread_mutex_init(tMutex, NULL);
            pthread_mutex_lock(tMutex);
            if (pTime > 0)
                return !pthread_cond_timedwait(mCondition, tMutex, &tTimeout);
            else
                return !pthread_cond_wait(mCondition, tMutex);
            pthread_mutex_destroy(tMutex);
            free(tMutex);
        }
    #endif

    #ifdef WIN32
        return (WaitForSingleObject(mCondition, (pTime == 0) ? INFINITE : pTime) == WAIT_OBJECT_0);
    #endif
}

bool Condition::SignalOne()
{
    #if defined(LINUX) || defined(APPLE)
		return !pthread_cond_signal(mCondition);
	#endif
	#ifdef WIN32
		return (SetEvent(mCondition) != 0);
	#endif
}

bool Condition::SignalAll()
{
    #if defined(LINUX) || defined(APPLE)
        return !pthread_cond_broadcast(mCondition);
    #endif
    #ifdef WIN32
		return (SetEvent(mCondition) != 0);
    #endif
}

bool Condition::Reset()
{
    #if defined(LINUX) || defined(APPLE)
        return (pthread_cond_init(mCondition, NULL) == 0);
	#endif
	#ifdef WIN32
		return (ResetEvent(mCondition) != 0);
	#endif
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
