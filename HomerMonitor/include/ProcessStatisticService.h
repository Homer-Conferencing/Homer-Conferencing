/*
 * Name:    ProcessStatisticService.h
 * Purpose: Process statistic service
 * Author:  Thomas Volkert
 * Since:   2010-11-07
 * Version: $Id$
 */

#ifndef _MULTIMEDIA_PROCESS_STATISTIC_SERVICE_
#define _MULTIMEDIA_PROCESS_STATISTIC_SERVICE_

#include <HBMutex.h>
#include <ProcessStatistic.h>

namespace Homer { namespace Monitor {

///////////////////////////////////////////////////////////////////////////////

#define SVC_PROCESS_STATISTIC ProcessStatisticService::GetInstance()

typedef std::list<ProcessStatistic*>  ProcessStatisticsList;

///////////////////////////////////////////////////////////////////////////////

class ProcessStatisticService
{
public:
    /// The default constructor
	ProcessStatisticService();

    /// The destructor.
    virtual ~ProcessStatisticService();

    static ProcessStatisticService& GetInstance();

    void AssignThreadName(std::string pName);

    /* get statistics */
    ProcessStatisticsList GetProcessStatistics();

    static void DisableProcessStatisticSupport();

private:
    void UpdateThreadDatabase();

    /* registration interface (without locking, only used internally) */
    ProcessStatistic* RegisterProcessStatistic(ProcessStatistic *pStat);
    bool UnregisterProcessStatistic(ProcessStatistic *pStat);

    ProcessStatisticsList mProcessStatistics;
    Mutex		   		  mProcessStatisticsMutex;
    Mutex                 mUpdateThreadDataBaseMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
