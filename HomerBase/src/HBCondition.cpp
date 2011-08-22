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
	#ifdef LINUX
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
	#ifdef LINUX
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
    #ifdef LINUX
        struct timespec tTimeout;

		if (clock_gettime(CLOCK_REALTIME, &tTimeout) == -1)
			LOG(LOG_ERROR, "Failed to get time from clock");

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
	#ifdef LINUX
		return !pthread_cond_signal(mCondition);
	#endif
	#ifdef WIN32
		return (SetEvent(mCondition) != 0);
	#endif
}

bool Condition::SignalAll()
{
    #ifdef LINUX
        return !pthread_cond_broadcast(mCondition);
    #endif
    #ifdef WIN32
		return (SetEvent(mCondition) != 0);
    #endif
}

bool Condition::Reset()
{
	#ifdef LINUX
        return (pthread_cond_init(mCondition, NULL) == 0);
	#endif
	#ifdef WIN32
		return (ResetEvent(mCondition) != 0);
	#endif
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
