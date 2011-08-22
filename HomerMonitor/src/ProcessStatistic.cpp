/*
 * Name:    ProcessStatistic.C
 * Purpose: Implemenation of process statistic
 * Author:  Thomas Volkert
 * Since:   2009-05-18
 * Version: $Id$
*/

#include <ProcessStatistic.h>
#include <HBThread.h>
#include <Logger.h>

namespace Homer { namespace Monitor {

using namespace std;

///////////////////////////////////////////////////////////////////////////////

ProcessStatistic::ProcessStatistic(int pThreadId)
{
    if (pThreadId == -1)
    {
        mRemotelyCreated = false;
        mThreadId = Thread::GetTId();
    }else
    {
        mRemotelyCreated = true;
        mThreadId = pThreadId;
    }

    mName = "hidden thread";
    mLastUserTicsThread = 0;
    mLastKernelTicsThread = 0;
    mLastUserTicsSystem = 0;
    mLastKernelTicsSystem = 0;
}

ProcessStatistic::~ProcessStatistic()
{
}

///////////////////////////////////////////////////////////////////////////////

int ProcessStatistic::GetThreadStatisticId()
{
    return mThreadId;
}

bool ProcessStatistic::IsRemotelyCreated()
{
    return mRemotelyCreated;
}

void ProcessStatistic::AssignThreadName(string pName)
{
    mName = pName;

    LOG(LOG_INFO, "Assign name \"%s\" for thread (Pid:%d Tid:%d)", pName.c_str(), Thread::GetPId(), mThreadId);
}

string ProcessStatistic::GetThreadName()
{
    return mName;
}

ThreadStatisticDescriptor ProcessStatistic::GetThreadStatistic()
{
    ThreadStatisticDescriptor tStat;

    Thread::GetThreadStatistic(mThreadId, tStat.MemVirtual, tStat.MemPhysical, tStat.Pid, tStat.PPid, tStat.LoadUser, tStat.LoadSystem, tStat.LoadTotal, tStat.Priority, tStat.PriorityBase, tStat.ThreadCount, mLastUserTicsThread, mLastKernelTicsThread, mLastUserTicsSystem, mLastKernelTicsSystem);
    tStat.Tid = mThreadId;

    return tStat;
}

///////////////////////////////////////////////////////////////////////////////

}}
