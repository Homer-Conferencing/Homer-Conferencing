/*****************************************************************************
 *
 * Copyright (C) 2012 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: Implementation for real-time transport control protocol
 * Author:  Thomas Volkert
 * Since:   2012-09-04
 */

#include <string>
#include <sstream>

#include <RTCP.h>

using namespace std;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

#define RTCP_SENDER_REPORT                  200

// from libavformat/internal.h
#define NTP_OFFSET                          2208988800ULL
#define NTP_OFFSET_US                       (NTP_OFFSET * 1000000ULL)

///////////////////////////////////////////////////////////////////////////////

RTCP::RTCP()
{
    LOG(LOG_VERBOSE, "Created");
}

RTCP::~RTCP()
{
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////
string GetPayloadType(int pType)
{
    string tResult = "unknown";
    switch(pType)
    {
        case RTCP_SENDER_REPORT:
                tResult = "sender report";
                break;
        case 201:
                tResult = "receiver report";
                break;
        case 202:
                tResult = "source description";
                break;
        case 203:
                tResult = "goodbye signal";
                break;
        case 204:
                tResult = "application defined";
                break;
        default:
                tResult = "type " + toString(pType);
                break;
    }
    return tResult;
}

uint64_t GetNtpTime()
{
    return (av_gettime() / 1000 ) * 1000 + NTP_OFFSET_US;
}

void RTCP::LogRtcpHeader(RtcpHeader *pRtcpHeader)
{
    // convert from network to host byte order, HACK: exceed array boundaries
    for (int i = 0; i < 3; i++)
        pRtcpHeader->Data[i] = ntohl(pRtcpHeader->Data[i]);
    int tRtcpHeaderLength = pRtcpHeader->Feedback.Length + 1;

    // conver the rest
    for (int i = 3; i < tRtcpHeaderLength; i++)
        pRtcpHeader->Data[i] = ntohl(pRtcpHeader->Data[i]);

    LOGEX(RTCP, LOG_VERBOSE, "################## RTCP header ########################");
    LOGEX(RTCP, LOG_VERBOSE, "Version: %d", pRtcpHeader->General.Version);
    if (pRtcpHeader->General.Padding)
        LOGEX(RTCP, LOG_VERBOSE, "Padding: true");
    else
        LOGEX(RTCP, LOG_VERBOSE, "Padding: false");
    LOGEX(RTCP, LOG_VERBOSE, "Type: %s", GetPayloadType(pRtcpHeader->General.Type).c_str());
    LOGEX(RTCP, LOG_VERBOSE, "Counter/format: %d", pRtcpHeader->General.RC);
    LOGEX(RTCP, LOG_VERBOSE, "Length: %d (entire packet size: %d)", pRtcpHeader->General.Length, (pRtcpHeader->General.Length + 1 /* length is reported minus one */) * 4 /* 32 bit words */);
    LOGEX(RTCP, LOG_VERBOSE, "SSRC            : %u", pRtcpHeader->Feedback.Ssrc);

    // sender report
    if (pRtcpHeader->General.Type == RTCP_SENDER_REPORT)
    {
        LOGEX(RTCP, LOG_VERBOSE, "Timestamp(high) : %10u", pRtcpHeader->Feedback.TimestampHigh);
        LOGEX(RTCP, LOG_VERBOSE, "Timestamp(low)  : %10u", pRtcpHeader->Feedback.TimestampLow);
        LOGEX(RTCP, LOG_VERBOSE, "RTP Timestamp   : %10u", pRtcpHeader->Feedback.RtpTimestamp);
        LOGEX(RTCP, LOG_VERBOSE, "Packets         : %10u", pRtcpHeader->Feedback.Packets);
        LOGEX(RTCP, LOG_VERBOSE, "Octets          : %10u", pRtcpHeader->Feedback.Octets);

        int64_t tTime2 = GetNtpTime();
        LOGEX(RTCP, LOG_WARN, "time of day2: %ld", tTime2);

        unsigned int tTimeHigh = tTime2 / 1000000;
        unsigned int tTimeLow = ((tTime2 % 1000000) << 32) / 1000000;

        LOGEX(RTCP, LOG_VERBOSE, "LocalTime(high) : %u, %u", tTimeHigh, ntohl(tTimeHigh));
        LOGEX(RTCP, LOG_VERBOSE, "LocalTime(low)  : %u, %u", tTimeLow, ntohl(tTimeLow));
    }

    // convert from host to network byte order, HACK: exceed array boundaries
    for (int i = 0; i < tRtcpHeaderLength; i++)
        pRtcpHeader->Data[i] = htonl(pRtcpHeader->Data[i]);
}

bool RTCP::RtcpParse(char *&pData, int &pDataSize, int &pPackets, int &pOctets)
{
    //HINT: assumes network byte order!

    bool tResult = false;
    RtcpHeader* tRtcpHeader = (RtcpHeader*)pData;

    // convert from network to host byte order, HACK: exceed array boundaries
    for (int i = 0; i < 3; i++)
        tRtcpHeader->Data[i] = ntohl(tRtcpHeader->Data[i]);
    int tRtcpHeaderLength = tRtcpHeader->Feedback.Length + 1;

    // conver the rest
    for (int i = 2; i < tRtcpHeaderLength; i++)
        tRtcpHeader->Data[i] = ntohl(tRtcpHeader->Data[i]);


    if (tRtcpHeaderLength == 7 /* need 28 byte sender report */)
    {// update values
        pPackets = tRtcpHeader->Feedback.Packets;
        pOctets = tRtcpHeader->Feedback.Octets;
        tResult = true;
    }else
    {// set fall back values
        pPackets = 0;
        pOctets = 0;
        tResult = false;
    }
    // convert from host to network byte order, HACK: exceed array boundaries
    for (int i = 0; i < tRtcpHeaderLength; i++)
        tRtcpHeader->Data[i] = htonl(tRtcpHeader->Data[i]);

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
