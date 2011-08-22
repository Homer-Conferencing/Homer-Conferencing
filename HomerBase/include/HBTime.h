/*
 * Name:    Mutex.h
 * Purpose: wrapper for os independent time handling
 * Author:  Thomas Volkert
 * Since:   2010-09-23
 * Version: $Id$
 */

#ifndef _BASE_TIME_
#define _BASE_TIME_

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#ifdef LINUX
#include <sys/time.h>
#endif

#ifdef WIN32
#include <sys/timeb.h>
#endif

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class Time
{
public:
    Time( );

    virtual ~Time( );

    bool ValidTimeStamp();
    void InvalidateTimeStamp();
    int64_t UpdateTimeStamp(); // in µs
    int64_t TimeDiffInUSecs(Time *pTime);
    static int64_t GetTimeStamp(); // in µs
    static bool GetNow(int *pDay, int *pMonth, int *pYear, int *pHour, int *pMin, int *pSec);

    Time& operator=(const Time &pTime);

private:
    int64_t     mTimeStamp; // in µs
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
