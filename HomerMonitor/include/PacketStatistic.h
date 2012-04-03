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
 * Purpose: Packet statistic
 * Author:  Thomas Volkert
 * Since:   2009-04-17
 */

#ifndef _MONITOR_PACKET_STATISTIC_
#define _MONITOR_PACKET_STATISTIC_

#include <HBMutex.h>
#include <HBTime.h>

#include <list>
#include <string>
#include <HBSocket.h>

using namespace Homer::Base;

namespace Homer { namespace Monitor {

///////////////////////////////////////////////////////////////////////////////

// reference buffer size for average data rate measurement (current value!)
#define STATISTIC_MOMENT_REFERENCE_SIZE                   8

///////////////////////////////////////////////////////////////////////////////

enum DataType
{
    DATA_TYPE_UNKNOWN = -1,
	DATA_TYPE_VIDEO,
	DATA_TYPE_AUDIO,
	DATA_TYPE_APP_CONTROL,
	DATA_TYPE_APP_DATA
};

struct PacketStatisticDescriptor{
    bool Outgoing;
    int  MinPacketSize;
    int  MaxPacketSize;
    int  PacketCount;
    int64_t ByteCount;
    int  LostPacketCount;
    int  AvgPacketSize;
    int  AvgDataRate;
    int  MomentAvgDataRate;
};

///////////////////////////////////////////////////////////////////////////////
class PacketStatistic
{
    // let RTP announce its generated packets
    friend class RTP;

public:
    PacketStatistic(std::string pName = "");
    virtual ~PacketStatistic();

    /* get simple statistic values */
    int getAvgPacketSize();
    int getAvgDataRate();
    int getMomentAvgDataRate();
    int getPacketCount();
    int64_t getByteCount();
    int getMinPacketSize();
    int getMaxPacketSize();
    int getLostPacketCount();

    /* get statistic values */
    PacketStatisticDescriptor GetPacketStatistic();

    /* classification */
    void AssignStreamName(std::string pName);
    std::string GetStreamName();
    enum DataType GetDataType();
    enum TransportType GetTransportType();
    std::string GetTransportTypeStr();
    enum NetworkType GetNetworkType();
    std::string GetNetworkTypeStr();
    bool IsOutgoingStream();

    /* reset internal states */
    virtual void ResetPacketStatistic();

    void SetLostPacketCount(int pPacketCount);

protected:
    /* update internal states */
    void AnnouncePacket(int pSize /* in bytes */); // timestamp is auto generated
    /* identification */
    void ClassifyStream(enum DataType pDataType = DATA_TYPE_UNKNOWN, enum TransportType pTransportType  = SOCKET_TRANSPORT_TYPE_INVALID, enum NetworkType pNetworkType = SOCKET_RAWNET);
    void SetOutgoingStream();

private:
    struct StatisticEntry{
        int PacketSize;
		int TimeDiff;
        int64_t Timestamp;
        int64_t ByteCount;
    };

    typedef std::list<StatisticEntry>      StatisticList;

    int           mMinPacketSize;
    int           mMaxPacketSize;
    int           mPacketCount;
    int64_t       mByteCount;
    int64_t       mStartTimeStamp;
    int64_t       mEndTimeStamp;
    int           mLostPacketCount;
    Time          mLastTime;
    StatisticList mStatistics;
    Mutex         mStatisticsMutex;
    std::string	  mName;
    enum DataType mStreamDataType;
    enum TransportType mStreamTransportType;
    enum NetworkType mStreamNetworkType;
    bool          mStreamOutgoing;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
