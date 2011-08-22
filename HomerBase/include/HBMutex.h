/*
 * Name:    Mutex.h
 * Purpose: wrapper for os independent mutex handling
 * Author:  Thomas Volkert
 * Since:   2010-09-20
 * Version: $Id: HBMutex.h 6 2011-08-22 13:06:22Z silvo $
 */

#ifndef _BASE_MUTEX_
#define _BASE_MUTEX_

#ifdef LINUX
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#define OS_DEP_MUTEX pthread_mutex_t*
#endif
#ifdef WIN32
#include <windows.h>
#include <stdio.h>
#define OS_DEP_MUTEX HANDLE
#endif

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of mutexes
//#define HB_DEBUG_MUTEX

///////////////////////////////////////////////////////////////////////////////

class Mutex
{
public:
    Mutex( );

    virtual ~Mutex( );

    /* every function returns TRUE if successful */
    bool lock();
    bool unlock();
    bool tryLock();
    bool tryLock(int pMSecs);

private:
friend class Condition;
    OS_DEP_MUTEX mMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
