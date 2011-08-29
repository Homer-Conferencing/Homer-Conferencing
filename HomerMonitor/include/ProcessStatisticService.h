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
