/*
 * Name:    ProcessStatistic.h
 * Purpose: Process statistic
 * Author:  Thomas Volkert
 * Since:   2009-05-18
 * Version: $Id: ProcessStatistic.h 14 2011-08-22 13:18:27Z silvo $
 */

#ifndef _MULTIMEDIA_PROCESS_STATISTIC_
#define _MULTIMEDIA_PROCESS_STATISTIC_

#include <HBMutex.h>

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
    int MemVirtual;
    int MemPhysical;
};

typedef std::list<ThreadStatisticDescriptor> ThreadStatisticList;

///////////////////////////////////////////////////////////////////////////////

class ProcessStatistic
{
public:
    /// The default constructor
    ProcessStatistic(int pThreadId = -1);

    /// The destructor.
    virtual ~ProcessStatistic();

    /* get whole statistic */
    ThreadStatisticDescriptor GetThreadStatistic();
    void AssignThreadName(std::string pName);
    std::string GetThreadName();
    int GetThreadStatisticId();
    bool IsRemotelyCreated();

private:
    bool mRemotelyCreated;
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
