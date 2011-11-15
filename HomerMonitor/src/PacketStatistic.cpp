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
 * Purpose: Implemenation of packet statistic
 * Author:  Thomas Volkert
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

#define IP_OVERHEAD                             20

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

    switch(mStreamPacketType)
    {
		case PACKET_TYPE_UDP_LITE:
			pSize += IP_OVERHEAD + UDP_LITE_HEADER_SIZE;
			break;
        case PACKET_TYPE_UDP:
            pSize += IP_OVERHEAD + UDP_HEADER_SIZE;
            break;
        case PACKET_TYPE_TCP:
            pSize += IP_OVERHEAD + TCP_HEADER_SIZE;
            break;
        default:
            break;
    }

    // init last time if we are called for the first time
    if (!mLastTime.ValidTimeStamp())
    {
        mLastTime.UpdateTimeStamp();
        return;
    }

    Time tCurTime;
    tCurTime.UpdateTimeStamp();
    long int tDiff = (long)tCurTime.TimeDiffInUSecs(&mLastTime);

    StatisticEntry tStatEntry;

    mPacketCount++;
    mByteCount += pSize;
    if (pSize < mMinPacketSize)
        mMinPacketSize = pSize;
    if (pSize > mMaxPacketSize)
        mMaxPacketSize = pSize;

    tStatEntry.PacketSize = pSize;
    //tStatEntry.TimeDiff = (int)tDiff;

    tStatEntry.Timestamp = Time::GetTimeStamp();
    tStatEntry.ByteCount = mByteCount;
    mLastTime = tCurTime;

    // lock
    mStatisticsMutex.lock();

    mStatistics.push_back(tStatEntry);
    while (mStatistics.size() > STATISTIC_BUFFER_SIZE)
        mStatistics.pop_front();

    // unlock
    mStatisticsMutex.unlock();
}

void PacketStatistic::ResetPacketStatistic()
{
    // lock
    mStatisticsMutex.lock();

    mPacketCount = 0;
    mByteCount = 0;
    mMinPacketSize = INT_MAX;
    mMaxPacketSize = 0;
    mLostPacketCount = 0;
    mLastTime.InvalidateTimeStamp();
    mStatistics.clear();

    // unlock
    mStatisticsMutex.unlock();
}

void PacketStatistic::SetLostPacketCount(int pPacketCount)
{
    mLostPacketCount = pPacketCount;
}

///////////////////////////////////////////////////////////////////////////////

int PacketStatistic::getAvgPacketSize()
{
    StatisticList::iterator tIt;
    long tResult = 0, tCount = 0, tPacketsSize = 0;

    // lock
    mStatisticsMutex.lock();

    tCount = (long)mStatistics.size();
    for (tIt = mStatistics.begin(); tIt != mStatistics.end(); tIt++)
        tPacketsSize += tIt->PacketSize;

    // unlock
    mStatisticsMutex.unlock();

    if (tCount > 0)
        tResult = tPacketsSize / tCount;
    else
        tResult = 0;

    return (int)tResult;
}

int PacketStatistic::getAvgDataRate()
{
    StatisticList::iterator tIt;
    long long tResult = 0;
    long int tPacketsSize = 0;
    long int tTimesDiff = 0;

    // lock
//    mStatisticsMutex.lock();
//
//    for (tIt = mStatistics.begin(); tIt != mStatistics.end(); tIt++)
//    {
//        tPacketsSize += tIt->PacketSize;
//        tTimesDiff += tIt->TimeDiff;
//    }
//
//    // unlock
//    mStatisticsMutex.unlock();

//    if (tTimesDiff > 0)
//        tResult = 1000 * 1000 * (long long)tPacketsSize / tTimesDiff;
//    else
//        tResult = 0;

    // lock
    mStatisticsMutex.lock();

    int64_t tCurrentTime = Time::GetTimeStamp();
    int64_t tMeasurementStartTime = mStatistics.front().Timestamp;
    int64_t tMeasurementStartByteCount = mStatistics.front().ByteCount;
    int tMeasuredValues = mStatistics.size() - 1;
    int64_t tMeasuredTimeDifference = tCurrentTime - tMeasurementStartTime;
    int64_t tMeasuredByteCountDifference = mByteCount - tMeasurementStartByteCount;

    double tDataRate = 1000000 * tMeasuredByteCountDifference / tMeasuredTimeDifference;

    // unlock
    mStatisticsMutex.unlock();

    return (int)tDataRate;
}

int64_t PacketStatistic::getByteCount()
{
    return mByteCount;
}

int PacketStatistic::getPacketCount()
{
    return mPacketCount;
}

int PacketStatistic::getMinPacketSize()
{
    if (mMinPacketSize != INT_MAX)
        return mMinPacketSize;
    else
        return 0;
}

int PacketStatistic::getMaxPacketSize()
{
    return mMaxPacketSize;
}

int PacketStatistic::getLostPacketCount()
{
    return mLostPacketCount;
}

void PacketStatistic::AssignStreamName(std::string pName)
{
	mName = pName;
}

void PacketStatistic::ClassifyStream(enum DataType pDataType, enum PacketType pPacketType)
{
    mStreamDataType = pDataType;
    mStreamPacketType = pPacketType;
	LOG(LOG_VERBOSE, "Classified stream by data type %d and packet type \"%s\"", (int)pDataType, GetPacketTypeStr().c_str());
}

string PacketStatistic::GetStreamName()
{
	return (mName != "" ? mName : "undefined");
}

enum DataType PacketStatistic::GetDataType()
{
	return mStreamDataType;
}

enum PacketType PacketStatistic::GetPacketType()
{
    return mStreamPacketType;
}

string PacketStatistic::GetPacketTypeStr()
{
    switch(mStreamPacketType)
    {
		case PACKET_TYPE_RAW:
			return "RAW";
		case PACKET_TYPE_TCP:
			return "TCP";
		case PACKET_TYPE_UDP:
			return "UDP";
		case PACKET_TYPE_UDP_LITE:
			return "UDP-Lite";
		default:
			return "N/A";
    }
}

PacketStatisticDescriptor PacketStatistic::GetPacketStatistic()
{
	PacketStatisticDescriptor tStat;

    tStat.Outgoing = IsOutgoingStream();
	tStat.MinPacketSize = getMinPacketSize();
	tStat.MaxPacketSize = getMaxPacketSize();
	tStat.PacketCount = getPacketCount();
	tStat.ByteCount = getByteCount();
	tStat.LostPacketCount = getLostPacketCount();
	tStat.AvgPacketSize = getAvgPacketSize();
	tStat.AvgDataRate = getAvgDataRate();

	return tStat;
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
