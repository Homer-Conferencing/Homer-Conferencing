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
 * Name:    Mutex.h
 * Purpose: wrapper for os independent mutex handling
 * Author:  Thomas Volkert
 * Since:   2010-09-20
 * Version: $Id$
 */

#ifndef _BASE_MUTEX_
#define _BASE_MUTEX_

#if defined(LINUX) || defined(APPLE)
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
