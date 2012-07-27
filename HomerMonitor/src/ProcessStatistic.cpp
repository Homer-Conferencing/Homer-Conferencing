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
 * Purpose: Implemenation of process statistic
 * Author:  Thomas Volkert
 * Since:   2009-05-18
*/

#include <ProcessStatistic.h>
#include <HBThread.h>
#include <Logger.h>

namespace Homer { namespace Monitor {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

ProcessStatistic::ProcessStatistic(int pThreadId)
{
    mThreadId = pThreadId;
    mName = "hidden thread";
    mLastUserTicsThread = 0;
    mLastKernelTicsThread = 0;
    mLastSystemTime = 0;
}

ProcessStatistic::~ProcessStatistic()
{
}

///////////////////////////////////////////////////////////////////////////////

int ProcessStatistic::GetThreadStatisticId()
{
    return mThreadId;
}

void ProcessStatistic::AssignThreadName(string pName)
{
    mName = pName;

    LOG(LOG_WARN, "Assign name \"%s\" for thread (Pid:%d Tid:%d)", pName.c_str(), Thread::GetTId(), mThreadId);
}

string ProcessStatistic::GetThreadName()
{
    return mName;
}

ThreadStatisticDescriptor ProcessStatistic::GetThreadStatistic()
{
    ThreadStatisticDescriptor tStat;

    Thread::GetThreadStatistic(mThreadId, tStat.MemVirtual, tStat.MemPhysical, tStat.Pid, tStat.PPid, tStat.LoadUser, tStat.LoadSystem, tStat.LoadTotal, tStat.Priority, tStat.PriorityBase, tStat.ThreadCount, mLastUserTicsThread, mLastKernelTicsThread, mLastSystemTime);
    tStat.Tid = mThreadId;

    //LOG(LOG_VERBOSE, "Thread %d => %f total load", mThreadId, tStat.LoadTotal);
    return tStat;
}

///////////////////////////////////////////////////////////////////////////////

}}
