/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implemenation of packet statistic
 * Since:   2009-04-17
*/
#include <PacketStatistic.h>
#include <PacketStatisticService.h>
#include <Logger.h>
#include <HBSocket.h>

#include <limits.h> // INT_MAX
#include <string>

using namespace std;
using namespace Homer::Base;

namespace Homer { namespace Monitor {

///////////////////////////////////////////////////////////////////////////////

PacketStatistic::PacketStatistic(std::string pName)
{
	mStreamDataType = DATA_TYPE_UNKNOWN;
	mStreamOutgoing = false;
	ResetPacketStatistic();
    AssignStreamName(pName);
    if (SVC_PACKET_STATISTIC.RegisterPacketStatistic(this) != this)
    	LOG(LOG_ERROR, "Error when registering packet statistic");
}

PacketStatistic::~PacketStatistic()
{
    if (!SVC_PACKET_STATISTIC.UnregisterPacketStatistic(this))
    	LOG(LOG_ERROR, "Error when unregistering packet statistic");
}

///////////////////////////////////////////////////////////////////////////////

void PacketStatistic::AnnouncePacket(int pSize)
{
    if (pSize == 0)
        return;

    switch(mStreamNetworkType)
    {
        case SOCKET_IPv4:
            pSize += IP4_HEADER_SIZE;
            break;
        case SOCKET_IPv6:
            pSize += IP6_HEADER_SIZE;
            break;
        default:
            break;
    }

    switch(mStreamTransportType)
    {
		case SOCKET_UDP_LITE:
			pSize += UDP_LITE_HEADER_SIZE;
			break;
        case SOCKET_UDP:
            pSize += UDP_HEADER_SIZE;
            break;
        case SOCKET_TCP:
            pSize += TCP_HEADER_SIZE;
            break;
        default:
            break;
    }

    mEndTimeStamp = Time::GetTimeStamp();
    if (mStartTimeStamp == 0)
        mStartTimeStamp = mEndTimeStamp;

    StatisticEntry tStatEntry;
    tStatEntry.PacketSize = pSize;
    tStatEntry.Timestamp = mEndTimeStamp;

    int64_t tTime = Time::GetTimeStamp();
    // lock
    mStatisticsMutex.lock();
    
    // init last time if we are called for the first time
    if (!mLastTime.ValidTimeStamp())
    {
        mLastTime.UpdateTimeStamp();

        // unlock
        mStatisticsMutex.unlock();

        return;
    }

    Time tCurTime;
    tCurTime.UpdateTimeStamp();
    long int tDiff = (long)tCurTime.TimeDiffInUSecs(&mLastTime);
    tStatEntry.TimeDiff = (int)tDiff;

    mPacketCount++;
    mByteCount += pSize;
    if (pSize < mMinPacketSize)
        mMinPacketSize = pSize;
    if (pSize > mMaxPacketSize)
        mMaxPacketSize = pSize;

    tStatEntry.ByteCount = mByteCount;
    mLastTime = tCurTime;

    mStatistics.push_back(tStatEntry);
    while (mStatistics.size() > STATISTIC_MOMENT_DATARATE_REFERENCE_SIZE)
    {
        mStatistics.erase(mStatistics.begin());
    }

    // unlock
    mStatisticsMutex.unlock();

    #ifdef STATISTIC_DEBUG_TIMING
        int64_t tTime2 = Time::GetTimeStamp();
        LOG(LOG_VERBOSE, "PacketStatistic::Lock1 took %"PRId64" us", tTime2 - tTime);
    #endif

    DataRateHistoryDescriptor tHistEntry;
    tHistEntry.TimeStamp = mEndTimeStamp - mStartTimeStamp;
    tHistEntry.Time = mEndTimeStamp;
    tHistEntry.DataRate = GetMomentAvgDataRate();

    tTime = Time::GetTimeStamp();
    mDataRateHistoryMutex.lock();
    int64_t tTime3 = Time::GetTimeStamp();

    if (mDataRateHistory.size() > STATISTIC_MOMENT_DATARATE_HISTORY)
    {
        if (mFirstDataRateHistoryLoss)
        {
            mFirstDataRateHistoryLoss = false;
            LOG(LOG_WARN, "List with measured values of data rate history has reached limit of %d entries, stopping measurement", STATISTIC_MOMENT_DATARATE_HISTORY);
        }
    }else
        mDataRateHistory.push_back(tHistEntry);

    mDataRateHistoryMutex.unlock();
    #ifdef STATISTIC_DEBUG_TIMING
        tTime2 = Time::GetTimeStamp();
        LOG(LOG_VERBOSE, "PacketStatistic::Lock2 took %"PRId64" us", tTime2 - tTime);
        tTime2 = Time::GetTimeStamp();
        LOG(LOG_VERBOSE, "PacketStatistic::Lock2-mutex took %"PRId64" us", tTime3 - tTime);
    #endif
}

void PacketStatistic::ResetPacketStatistic()
{
    // lock
    mStatisticsMutex.lock();

    mStartTimeStamp = 0;
    mEndTimeStamp = 0;
    mPacketCount = 0;
    mByteCount = 0;
    mMinPacketSize = INT_MAX;
    mMaxPacketSize = 0;
    mLostPacketCount = 0;

    mDataRateHistoryMutex.lock();
    mDataRateHistory.clear();
    mDataRateHistoryMutex.unlock();

    mFirstDataRateHistoryLoss = true;
    mLastTime.InvalidateTimeStamp();
    mStatistics.clear();

    // unlock
    mStatisticsMutex.unlock();
}

void PacketStatistic::SetLostPacketCount(uint64_t pPacketCount)
{
    mLostPacketCount = pPacketCount;
}

///////////////////////////////////////////////////////////////////////////////

int PacketStatistic::GetAvgPacketSize()
{
    int64_t tResult = 0, tCount = 0, tPacketsSize = 0;

    if (mPacketCount > 0)
        tResult = mByteCount / mPacketCount;
    else
        tResult = 0;

    return (int)tResult;
}

int PacketStatistic::GetAvgDataRate()
{
    double tDataRate = 0;

    // lock
    mStatisticsMutex.lock();

    if (mStatistics.size() > 1)
    {
        int64_t tCurrentTime = mEndTimeStamp;
        int64_t tMeasurementStartTime = mStartTimeStamp;
        int64_t tMeasurementStartByteCount = 0;
        int tMeasuredValues = mPacketCount - 1;
        int64_t tMeasuredTimeDifference = tCurrentTime - tMeasurementStartTime;
        int64_t tMeasuredByteCountDifference = mByteCount - tMeasurementStartByteCount;

        if(tMeasuredTimeDifference != 0)
            tDataRate = (double)1000000 * tMeasuredByteCountDifference / tMeasuredTimeDifference;
        else
            tDataRate = 0;
    }else
        tDataRate = 0;

    // unlock
    mStatisticsMutex.unlock();

    return (int)tDataRate;
}

int PacketStatistic::GetMomentAvgDataRate()
{
    Statistics::iterator tIt;
    double tDataRate = 0;

    // lock
    mStatisticsMutex.lock();

    if (mStatistics.size() > 1)
    {
        int64_t tCurrentTime = Time::GetTimeStamp();
        int64_t tMeasurementStartTime = mStatistics.front().Timestamp;
        int64_t tMeasurementStartByteCount = mStatistics.front().ByteCount;
        int tMeasuredValues = STATISTIC_MOMENT_DATARATE_REFERENCE_SIZE - 1;
        int64_t tMeasuredTimeDifference = tCurrentTime - tMeasurementStartTime;
        int64_t tMeasuredByteCountDifference = mByteCount - tMeasurementStartByteCount;

        if (tMeasuredTimeDifference != 0)
        	tDataRate = (double)1000000 * tMeasuredByteCountDifference / tMeasuredTimeDifference;
    }

    // unlock
    mStatisticsMutex.unlock();

    return (int)tDataRate;
}

int64_t PacketStatistic::GetByteCount()
{
    return mByteCount;
}

int PacketStatistic::GetPacketCount()
{
    return mPacketCount;
}

int PacketStatistic::GetMinPacketSize()
{
    if (mMinPacketSize != INT_MAX)
        return mMinPacketSize;
    else
        return 0;
}

int PacketStatistic::GetMaxPacketSize()
{
    return mMaxPacketSize;
}

uint64_t PacketStatistic::GetLostPacketCount()
{
    return mLostPacketCount;
}

void PacketStatistic::AssignStreamName(std::string pName)
{
	mName = pName;
}

void PacketStatistic::ClassifyStream(enum DataType pDataType, enum TransportType pTransportType, enum NetworkType pNetworkType)
{
    if ((pDataType != mStreamDataType) || (pTransportType != mStreamTransportType) || (pNetworkType != mStreamNetworkType))
    {
        mStreamDataType = pDataType;
        mStreamTransportType = pTransportType;
        mStreamNetworkType = pNetworkType;
        LOG(LOG_VERBOSE, "Classified stream %s as of data type %d, transport type is \"%s\", network type is \"%s\"", mName.c_str() ,(int)pDataType, GetTransportTypeStr().c_str(), GetNetworkTypeStr().c_str());
    }
}

string PacketStatistic::GetStreamName()
{
	return (mName != "" ? mName : "undefined");
}

enum DataType PacketStatistic::GetDataType()
{
	return mStreamDataType;
}

string PacketStatistic::GetDataTypeStr()
{
	string tResult = "";

	switch(mStreamDataType)
	{
		case DATA_TYPE_VIDEO:
			tResult = "VIDEO";
			break;
		case DATA_TYPE_AUDIO:
			tResult = "AUDIO";
			break;
		case DATA_TYPE_FILE:
			tResult = "FILE";
			break;
		case DATA_TYPE_GENDATA:
			tResult = "DATA";
			break;
		default:
			tResult = "N/A";
			break;
	}

	return tResult;
}

enum TransportType PacketStatistic::GetTransportType()
{
    return mStreamTransportType;
}

string PacketStatistic::GetTransportTypeStr()
{
    switch(mStreamTransportType)
    {
		case SOCKET_RAW:
			return "RAW";
		case SOCKET_TCP:
			return "TCP";
		case SOCKET_UDP:
			return "UDP";
		case SOCKET_UDP_LITE:
			return "UDP-Lite";
		default:
			return "unknown";
    }
}

enum NetworkType PacketStatistic::GetNetworkType()
{
    return mStreamNetworkType;
}

string PacketStatistic::GetNetworkTypeStr()
{
    switch(mStreamNetworkType)
    {
        case SOCKET_RAWNET:
            return "RAW";
        case SOCKET_IPv4:
            return "IPv4";
        case SOCKET_IPv6:
            return "IPv6";
        default:
            return "unknown";
    }
}

PacketStatisticDescriptor PacketStatistic::GetPacketStatistic()
{
	PacketStatisticDescriptor tStat;

    tStat.Outgoing = IsOutgoingStream();
	tStat.MinPacketSize = GetMinPacketSize();
	tStat.MaxPacketSize = GetMaxPacketSize();
	tStat.PacketCount = GetPacketCount();
	tStat.ByteCount = GetByteCount();
	tStat.LostPacketCount = GetLostPacketCount();
	tStat.AvgPacketSize = GetAvgPacketSize();
	tStat.AvgDataRate = GetAvgDataRate();
    tStat.MomentAvgDataRate = GetMomentAvgDataRate();

	return tStat;
}

DataRateHistory PacketStatistic::GetDataRateHistory()
{
    DataRateHistory tResult;

    mDataRateHistoryMutex.lock();

    tResult = mDataRateHistory;

    mDataRateHistoryMutex.unlock();

    return tResult;
}

void PacketStatistic::SetOutgoingStream()
{
    mStreamOutgoing = true;
}

bool PacketStatistic::IsOutgoingStream()
{
    return mStreamOutgoing;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace
