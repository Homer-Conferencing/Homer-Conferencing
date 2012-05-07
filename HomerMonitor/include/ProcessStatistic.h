/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Process statistic
 * Author:  Thomas Volkert
 * Since:   2009-05-18
 */

#ifndef _MULTIMEDIA_PROCESS_STATISTIC_
#define _MULTIMEDIA_PROCESS_STATISTIC_

#include <HBMutex.h>
#include <ProcessStatisticService.h>

#include <string>
#include <list>

using namespace Homer::Base;

namespace Homer { namespace Monitor {

///////////////////////////////////////////////////////////////////////////////
struct ThreadStatisticDescriptor{
    int Tid;
    int Pid;
    int PPid;
    float LoadUser;
    float LoadSystem;
    float LoadTotal;
    int Priority;
    int PriorityBase;
    int ThreadCount;
    unsigned long MemVirtual;
    unsigned long MemPhysical;
};

typedef std::list<ThreadStatisticDescriptor> ThreadStatisticList;

///////////////////////////////////////////////////////////////////////////////
class ProcessStatisticService;

class ProcessStatistic
{
public:
    /// The destructor.
    virtual ~ProcessStatistic();

    /* get whole statistic */
    ThreadStatisticDescriptor GetThreadStatistic();
    void AssignThreadName(std::string pName);
    std::string GetThreadName();
    int GetThreadStatisticId();

private:
friend class ProcessStatisticService;
    /// The default constructor
    ProcessStatistic(int pThreadId);

    int mThreadId;
    std::string mName;
    unsigned long long mLastUserTicsThread;
    unsigned long long mLastKernelTicsThread;
    unsigned long long mLastUserTicsSystem;
    unsigned long long mLastKernelTicsSystem;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
