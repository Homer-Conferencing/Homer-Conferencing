/*
 * Name:    PacketStatisticService.cpp
 * Purpose: Implemenation of packet statistic service as singleton
 * Author:  Thomas Volkert
 * Since:   2010-10-27
 * Version: $Id$
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

PacketStatisticsList PacketStatisticService::GetPacketStatistics()
{
	PacketStatisticsList tResult;

	// lock
    mPacketStatisticsMutex.lock();

    tResult = mPacketStatistics;

    // unlock
    mPacketStatisticsMutex.unlock();

    return tResult;
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
