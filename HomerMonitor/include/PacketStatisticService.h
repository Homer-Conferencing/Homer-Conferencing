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
 * Name:    PacketStatisticService.h
 * Purpose: Packet statistic service
 * Author:  Thomas Volkert
 * Since:   2010-10-27
 * Version: $Id$
 */

#ifndef _MONITOR_PACKET_STATISTIC_SERVICE_
#define _MONITOR_PACKET_STATISTIC_SERVICE_

#include <HBMutex.h>
#include <PacketStatistic.h>

namespace Homer { namespace Monitor {

///////////////////////////////////////////////////////////////////////////////

#define SVC_PACKET_STATISTIC PacketStatisticService::GetInstance()

typedef std::list<PacketStatistic*>  PacketStatisticsList;

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
    PacketStatisticsList GetPacketStatistics();

    /* registration interface */
    PacketStatistic* RegisterPacketStatistic(PacketStatistic *pStat);
    bool UnregisterPacketStatistic(PacketStatistic *pStat);

private:
    PacketStatisticsList mPacketStatistics;
    Mutex				 mPacketStatisticsMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
