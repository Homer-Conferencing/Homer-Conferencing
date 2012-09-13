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
 * Purpose: real-time transport control protocol handling
 * Author:  Thomas Volkert
 * Since:   2012-09-04
 */

#ifndef _MULTIMEDIA_RTCP_
#define _MULTIMEDIA_RTCP_

#include <Header_Ffmpeg.h>
#include <PacketStatistic.h>

#include <sys/types.h>
#include <string>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of RTP packets
//#define RTCP_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

// ########################## RTCP ###########################################
union RtcpHeader{
    struct{ // send via separate port
        unsigned short int Length;          /* length of report */
        unsigned int ReportType:8;          /* report type */
        unsigned int RC:5;                  /* report counter */
        unsigned int Padding:1;             /* padding flag */
        unsigned int Version:2;             /* protocol version */

        unsigned int Ssrc;                  /* synchronization source */

        unsigned int dummy[5];              /*  */
    } __attribute__((__packed__))RtpBased;
    struct{ // send within media stream as intermediate packets
        unsigned short int Length;          /* length of report */
        unsigned int PlType:8;              /* Payload type (PT) */
        unsigned int Fmt:5;                 /* Feedback message type (FMT) */
        unsigned int Padding:1;             /* padding flag */
        unsigned int Version:2;             /* protocol version */

        unsigned int Timestamp;             /* timestamp */

        unsigned int Ssrc;                  /* synchronization source */

        unsigned int Data[4];
    } __attribute__((__packed__))Feedback;
    uint32_t Data[7];
};

// calculate the size of an RTCP header: "size of structure"
#define RTCP_HEADER_SIZE                      sizeof(RtcpHeader)

///////////////////////////////////////////////////////////////////////////////

class RTCP
{
public:
	RTCP();

    virtual ~RTCP( );

    /* RTCP packetizing */
    static void LogRtcpHeader(RtcpHeader *pRtcpHeader);
    bool RtcpParse(char *&pData, unsigned int &pDataSize, int &pPackets, int &pOctets);
private:
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
