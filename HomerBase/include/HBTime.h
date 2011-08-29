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
 * Name:    Time.h
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
