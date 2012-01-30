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
 * Purpose: Implemenation of packet statistic service as singleton
 * Author:  Thomas Volkert
 * Since:   2010-10-27
*/
#include <PacketStatisticService.h>

#include <Logger.h>

using namespace Homer::Base;

namespace Homer { namespace Monitor {

PacketStatisticService sPacketStatisticService;

///////////////////////////////////////////////////////////////////////////////

PacketStatisticService::PacketStatisticService()
{

}

PacketStatisticService::~PacketStatisticService()
{

}

PacketStatisticService& PacketStatisticService::GetInstance()
{
    return sPacketStatisticService;
}

///////////////////////////////////////////////////////////////////////////////

PacketStatisticsList PacketStatisticService::GetPacketStatisticsAccess()
{
	PacketStatisticsList tResult;

	// lock
    mPacketStatisticsMutex.lock();

    tResult = mPacketStatistics;

    return tResult;
}

void PacketStatisticService::ReleasePacketStatisticsAccess()
{
    // unlock
    mPacketStatisticsMutex.unlock();
}

PacketStatistic* PacketStatisticService::RegisterPacketStatistic(PacketStatistic *pStat)
{
    PacketStatisticsList::iterator tIt;
    bool tFound = false;

    if (pStat == NULL)
        return NULL;

    LOG(LOG_VERBOSE, "Registering packet statistic: %s", pStat->GetStreamName().c_str());

    // lock
    mPacketStatisticsMutex.lock();

    for (tIt = mPacketStatistics.begin(); tIt != mPacketStatistics.end(); tIt++)
    {
        if (*tIt == pStat)
        {
            LOG(LOG_VERBOSE, "Statistic already registered");
            tFound = true;
            break;
        }
    }

    if (!tFound)
    	mPacketStatistics.push_back(pStat);

    // unlock
    mPacketStatisticsMutex.unlock();

    return pStat;
}

bool PacketStatisticService::UnregisterPacketStatistic(PacketStatistic *pStat)
{
    PacketStatisticsList::iterator tIt;
    bool tFound = false;

    if (pStat == NULL)
        return false;

    LOG(LOG_VERBOSE, "Unregistering packet statistic: %s", pStat->GetStreamName().c_str());

    // lock
    mPacketStatisticsMutex.lock();

    for (tIt = mPacketStatistics.begin(); tIt != mPacketStatistics.end(); tIt++)
    {
        if (*tIt == pStat)
        {
            tFound = true;
            mPacketStatistics.erase(tIt);
            LOG(LOG_VERBOSE, "..unregistered");
            break;
        }
    }

    // unlock
    mPacketStatisticsMutex.unlock();

    return tFound;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace
