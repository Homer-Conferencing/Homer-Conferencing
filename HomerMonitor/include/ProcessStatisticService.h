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
 * Purpose: Process statistic service
 * Author:  Thomas Volkert
 * Since:   2010-11-07
 */

#ifndef _MULTIMEDIA_PROCESS_STATISTIC_SERVICE_
#define _MULTIMEDIA_PROCESS_STATISTIC_SERVICE_

#include <HBMutex.h>
#include <ProcessStatistic.h>

#include <string>
#include <vector>

namespace Homer { namespace Monitor {

///////////////////////////////////////////////////////////////////////////////

#define SVC_PROCESS_STATISTIC ProcessStatisticService::GetInstance()
class ProcessStatistic;
typedef std::vector<ProcessStatistic*>  ProcessStatistics;

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
    ProcessStatistics GetProcessStatistics();

    static void DisableProcessStatisticSupport();

private:
    void UpdateThreadDatabase();

    /* registration interface (without locking, only used internally) */
    bool RegisterProcessStatistic(int pThreadId);
    bool UnregisterProcessStatistic(int pThreadId);

    ProcessStatistics mProcessStatistics;
    Homer::Base::Mutex	  mProcessStatisticsMutex;
    Homer::Base::Mutex    mUpdateThreadDataBaseMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
