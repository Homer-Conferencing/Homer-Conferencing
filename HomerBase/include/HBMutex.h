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
 * Purpose: wrapper for os independent mutex handling
 * Author:  Thomas Volkert
 * Since:   2010-09-20
 */

#ifndef _BASE_MUTEX_
#define _BASE_MUTEX_

#if defined(LINUX) || defined(APPLE) || defined(BSD)
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#define OS_DEP_MUTEX pthread_mutex_t
#endif

#include <Header_Windows.h>

#if defined(WINDOWS)
#define OS_DEP_MUTEX HANDLE
#endif

#include <string>

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
    bool lock(int pTimeout = 0 /* in ms, 0 means infinite waiting time */);
    bool unlock();

    /* for debbuging */
    void AssignName(std::string pName);

private:
friend class Condition;

    bool tryLock(int pMSecs = 0);

    OS_DEP_MUTEX    mMutex;
    int             mOwnerThreadId;
    std::string     mName;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
