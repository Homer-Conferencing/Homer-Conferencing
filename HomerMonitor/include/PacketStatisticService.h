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
