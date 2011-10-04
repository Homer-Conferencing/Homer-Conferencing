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
 * Name:    Thread.h
 * Purpose: wrapper for os independent thread handling
 * Author:  Thomas Volkert
 * Since:   2010-09-28
 * Version: $Id$
 */

#ifndef _BASE_THREAD_
#define _BASE_THREAD_

typedef void*(*THREAD_MAIN)(void*);

#ifdef LINUX
#include <pthread.h>
#define OS_DEP_THREAD pthread_t
#endif
#ifdef WIN32
#include <Windows.h>
#include <stdio.h>
#define OS_DEP_THREAD HANDLE
#endif

#include <vector>

namespace Homer { namespace Base {

///////////////////////////////////////////////////////////////////////////////

class Thread
{
public:
	Thread( );

    virtual ~Thread( );

    virtual void* Run(void* pArgs = NULL);
    bool StartThread(void* pArgs = NULL);
    bool StartThread(THREAD_MAIN pMain, void* pArgs = NULL);
    bool StopThread(int pTimeoutInMSecs, void** pResults = NULL); // return pointer to result of thread
    static void Suspend(unsigned int pUSecs);
    static int GetTId();
    static int GetPId();
    static int GetPPId();
    static std::vector<int> GetTIds();
    static bool GetThreadStatistic(int pTid, unsigned long &pMemVirtual, unsigned long &pMemPhysical, int &pPid, int &pPPid, float &pLoadUser, float &pLoadSystem, float &pLoadTotal, int &pPriority, int &pNice, int &pThreadCount, unsigned long long &pLastUserTicsThread, unsigned long long &pLastKernelTicsThread, unsigned long long &pLastUserTicsSystem, unsigned long long &pLastKernelTicsSystem);

private:
    static void* StartThreadStaticWrapperUniversal(void* pThread);
    static void* StartThreadStaticWrapperRun(void* pThread);

    THREAD_MAIN mThreadMain;
    void *mThreadArguments;
    OS_DEP_THREAD mThreadHandle;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
