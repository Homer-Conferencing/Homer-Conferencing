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
 * Name:    Condition.h
 * Purpose: wrapper for os independent conditional waiting
 * Author:  Thomas Volkert
 * Since:   2011-02-05
 * Version: $Id$
 */

#ifndef _BASE_CONDITION_
#define _BASE_CONDITION_

#include <HBMutex.h>

#if defined(LINUX) || defined(APPLE)
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
