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
 * Purpose: Packet statistic service
 * Since:   2010-10-27
 */

#ifndef _MONITOR_PACKET_STATISTIC_SERVICE_
#define _MONITOR_PACKET_STATISTIC_SERVICE_

#include <HBMutex.h>
#include <PacketStatistic.h>

#include <vector>

namespace Homer { namespace Monitor {

///////////////////////////////////////////////////////////////////////////////

#define SVC_PACKET_STATISTIC PacketStatisticService::GetInstance()

typedef std::vector<PacketStatistic*>  PacketStatistics;

///////////////////////////////////////////////////////////////////////////////

class PacketStatisticService
{
public:
    /// The default constructor
	PacketStatisticService();

    /// The destructor.
    virtual ~PacketStatisticService();

    static PacketStatisticService& GetInstance();

    /* get statistics */
    PacketStatistics GetPacketStatisticsAccess();
    void ReleasePacketStatisticsAccess();

    /* registration interface */
    PacketStatistic* RegisterPacketStatistic(PacketStatistic *pStat);
    bool UnregisterPacketStatistic(PacketStatistic *pStat);

private:
    PacketStatistics mPacketStatistics;
    Mutex				 mPacketStatisticsMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
