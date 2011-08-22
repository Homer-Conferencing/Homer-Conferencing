/*
 * Name:    Condition.h
 * Purpose: wrapper for os independent conditional waiting
 * Author:  Thomas Volkert
 * Since:   2011-02-05
 * Version: $Id$
 */

#ifndef _BASE_CONDITION_
#define _BASE_CONDITION_

#include <HBMutex.h>

#ifdef LINUX
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#define OS_DEP_COND pthread_cond_t*
#endif
#ifdef WIN32
#include <windows.h>
#include <stdio.h>
#define OS_DEP_COND HANDLE
#endif

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class Condition
{
public:
    Condition();

    virtual ~Condition( );

    bool Wait(Mutex *pMutex = NULL, int pTime = 0); //in ms
    bool SignalOne();
    bool SignalAll();
    bool Reset();

private:
    OS_DEP_COND mCondition;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
