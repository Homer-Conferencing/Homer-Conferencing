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

RTCP::RTCP()
{
    LOG(LOG_VERBOSE, "Created");
}

RTCP::~RTCP()
{
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

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
    LOGEX(RTCP, LOG_VERBOSE, "Version: %d", pRtcpHeader->Feedback.Version);
    if (pRtcpHeader->Feedback.Padding)
        LOGEX(RTCP, LOG_VERBOSE, "Padding: true");
    else
        LOGEX(RTCP, LOG_VERBOSE, "Padding: false");
    switch(pRtcpHeader->Feedback.PlType)
    {
            case 200:
                    LOGEX(RTCP, LOG_VERBOSE, "Report type: sender report");
                    break;
            case 201:
                    LOGEX(RTCP, LOG_VERBOSE, "Report type: receiver report");
                    break;
            default:
                    LOGEX(RTCP, LOG_VERBOSE, "Report type: %d", pRtcpHeader->Feedback.PlType);
                    break;
    }
    LOGEX(RTCP, LOG_VERBOSE, "Report length: %d (entire packet size: %d)", pRtcpHeader->Feedback.Length, (pRtcpHeader->Feedback.Length + 1 /* length is reported minus one */) * 4 /* 32 bit words */);
    LOGEX(RTCP, LOG_VERBOSE, "Message type: %d", pRtcpHeader->Feedback.Fmt);
    LOGEX(RTCP, LOG_VERBOSE, "Time stamp: %10u", pRtcpHeader->Feedback.Timestamp);
    LOGEX(RTCP, LOG_VERBOSE, "SSRC: %u", pRtcpHeader->Feedback.Ssrc);
    for (int i = 3; i < pRtcpHeader->Feedback.Length + 1; i++)
    {
        LOGEX(RTCP, LOG_VERBOSE, "RTCP-SR data[%d] = %u", i - 3, pRtcpHeader->Data[i]);
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
    for (int i = 3; i < tRtcpHeaderLength; i++)
        tRtcpHeader->Data[i] = ntohl(tRtcpHeader->Data[i]);


    if (tRtcpHeaderLength == 7 /* need 28 byte sender report */)
    {// update values
        pPackets = tRtcpHeader->Feedback.Data[2];
        pOctets = tRtcpHeader->Feedback.Data[3];
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
