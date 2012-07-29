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
 * Purpose: Implementation of os independent condition handling
 * Author:  Thomas Volkert
 * Since:   2011-02-05
 */

#include <Logger.h>
#include <HBCondition.h>

#ifdef APPLE
// to get current time stamp
#include <mach/clock.h>
#include <mach/mach.h>
#endif

namespace Homer { namespace Base {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

Condition::Condition()
{
	bool tResult = false;
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		tResult = (pthread_cond_init(&mCondition, NULL) == 0);
	#endif
	#if defined(WIN32) ||defined(WIN64)
		mCondition = CreateEvent(NULL, true, false, NULL);
		tResult = (mCondition != NULL);
	#endif
	if (!tResult)
        LOG(LOG_ERROR, "Initiation of condition failed");
}

Condition::~Condition()
{
    bool tResult = false;
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		tResult = (pthread_cond_destroy(&mCondition) == 0);
	#endif
	#if defined(WIN32) ||defined(WIN64)
	    tResult = (CloseHandle(mCondition) != 0);
	#endif
    if (!tResult)
		LOG(LOG_ERROR, "Destruction of condition failed");
}

///////////////////////////////////////////////////////////////////////////////

bool Condition::Wait(Mutex *pMutex, int pTime)
{
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
        struct timespec tTimeout;
        struct timespec tTimeout1;

        #if defined(LINUX) || defined(BSD)
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
        int tTime = pTime;

        tTimeout1 = tTimeout;

        // add complete seconds if pTime is bigger than 1 sec
        if (tTime >= 1000)
        {
            tTimeout.tv_sec += tTime / 1000;
            tTime %= 1000;
        }
        tTimeout.tv_nsec += tTime * 1000 * 1000;
        if (tTimeout.tv_nsec > (int64_t) 1000 * 1000 * 1000)
        {
            int64_t tAddSecs = tTimeout.tv_nsec / 1000 / 1000 / 1000;
            int64_t tNanoDiff = tAddSecs * 1000 * 1000 * 1000;
            int64_t tNanoSecs = tTimeout.tv_nsec - tNanoDiff;
            tTimeout.tv_nsec = tNanoSecs;
            tTimeout.tv_sec += tAddSecs;
            #ifdef HBC_DEBUG_TIMED
                LOG(LOG_WARN, "Condition ns part of timeout exceeds by %ld seconds", tAddSecs);
            #endif
        }

        if (pMutex)
            if (pTime > 0)
                return !pthread_cond_timedwait(&mCondition, &pMutex->mMutex, &tTimeout);
            else
                return !pthread_cond_wait(&mCondition, &pMutex->mMutex);
        else
        {
        	bool tResult = false;
            pthread_mutex_t tMutex = PTHREAD_MUTEX_INITIALIZER;
            pthread_mutex_lock(&tMutex);
            if (pTime > 0)
            {
                int tRes = pthread_cond_timedwait(&mCondition, &tMutex, &tTimeout);
				switch(tRes)
				{
					case EDEADLK:
						LOG(LOG_ERROR, "Condition already held by calling thread");
						break;
					case EBUSY: // Condition can't be obtained because it is busy
						break;
					case EINVAL:
			            LOG(LOG_WARN, "Ref.  time: %ld / %ld", tTimeout1.tv_sec, tTimeout1.tv_nsec);
			            LOG(LOG_WARN, "Final time: %ld / %ld", tTimeout.tv_sec, tTimeout.tv_nsec);
						LOG(LOG_ERROR, "Specified time of %d was invalid", pTime);
						break;
					case EFAULT:
						LOG(LOG_ERROR, "Invalid condition pointer was given");
						break;
					case ETIMEDOUT:
						LOG(LOG_WARN, "Condition couldn't be obtained in given time of %d ms", pTime);
						break;
					case 0: // Condition was free and is obtained now
						tResult = true;
						break;
					default:
						LOG(LOG_ERROR, "Error occurred while trying to get condition: code %d", tRes);
						break;
				}
        	}else
            {
            	int tRes = pthread_cond_wait(&mCondition, &tMutex);
        		switch(tRes)
        		{
        			case EDEADLK:
        				LOG(LOG_ERROR, "Condition already held by calling thread");
        				break;
        			case EBUSY: // Condition can't be obtained because it is busy
        				break;
        			case EINVAL:
        				LOG(LOG_ERROR, "Condition was found in uninitialized state");
        				break;
        			case EFAULT:
        				LOG(LOG_ERROR, "Invalid condition pointer was given");
        				break;
        			case 0: // Condition was free and is obtained now
        				tResult = true;
        				break;
        			default:
        				LOG(LOG_ERROR, "Error occurred while trying to get condition: code %d", tRes);
        				break;
        		}
            }
            pthread_mutex_unlock(&tMutex);
            pthread_mutex_destroy(&tMutex);
    		return tResult;
        }
    #endif

    #if defined(WIN32) ||defined(WIN64)
        //TODO: fix this - mutex handling here is nothing to rely on!
        if (pMutex != NULL)
        	pMutex->unlock();
        return (WaitForSingleObject(mCondition, (pTime == 0) ? INFINITE : pTime) == WAIT_OBJECT_0);
        if (pMutex != NULL)
        	pMutex->lock();
    #endif

	LOG(LOG_ERROR, "We should never reach this point but we did");
	return false;
}

bool Condition::SignalOne()
{
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
		return !pthread_cond_signal(&mCondition);
	#endif
	#if defined(WIN32) ||defined(WIN64)
		return (SetEvent(mCondition) != 0);
	#endif
}

bool Condition::SignalAll()
{
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
        return !pthread_cond_broadcast(&mCondition);
    #endif
    #if defined(WIN32) ||defined(WIN64)
		return (SetEvent(mCondition) != 0);
    #endif
}

bool Condition::Reset()
{
    #if defined(LINUX) || defined(APPLE) || defined(BSD)
        return (pthread_cond_init(&mCondition, NULL) == 0);
	#endif
	#if defined(WIN32) ||defined(WIN64)
		return (ResetEvent(mCondition) != 0);
	#endif
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
