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
 * Purpose: real-time transport protocol handling
 * Author:  Thomas Volkert
 * Since:   2009-01-20
 */

#ifndef _MULTIMEDIA_RTP_
#define _MULTIMEDIA_RTP_

#include <Header_Ffmpeg.h>
#include <PacketStatistic.h>
#include <RTCP.h>

#include <sys/types.h>
#include <string>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of send RTP packets
//#define RTP_DEBUG_PACKET_ENCODER

// the following de/activates debugging of received RTP packets
//#define RTP_DEBUG_PACKET_DECODER

///////////////////////////////////////////////////////////////////////////////

// ########################## RTP ############################################
union RtpHeader{
    struct{
        unsigned short int SequenceNumber; /* sequence number */

        unsigned int PayloadType:7;         /* payload type */
        unsigned int Marked:1;              /* marker bit */
        unsigned int CsrcCount:4;           /* CSRC count */
        unsigned int Extension:1;           /* header extension flag */
        unsigned int Padding:1;             /* padding flag */
        unsigned int Version:2;             /* protocol version */

        unsigned int Timestamp;             /* timestamp */

        unsigned int Ssrc;                  /* synchronization source */
        //HINT: we do not support CSRC because it is not necessary!
        //unsigned int Csrc[1];               /* optional CSRC list */
    } __attribute__((__packed__));
    uint32_t Data[3];
};


// calculate the size of an RTP header: "size of structure"
#define RTP_HEADER_SIZE                      sizeof(RtpHeader)

// maximum packet size, including encoded data/TS and RTP header
// HINT: values above 1k would lead to unacceptable picture quality in wireless LAN
// HINT: 576 bytes should every host be able to process, 20 bytes for IP header
// HINT: 1448 byte should be the maximum packet size (1500 - IP - UDP - UMTP)
//       UMTP = "UDP Multicast Tunneling Protocol"
// HINT: 1472 bytes are the value from the ffmpeg library
// HINT: this value is only used within h261 packetizer, in remaining cases the value
//       from the MediaSourceMuxer is used
//#define RTP_MAX_PACKET_SIZE                     1472 //576
//#define RTP_MAX_PAYLOAD_SIZE                    RTP_MAX_PACKET_SIZE - RTP_HEADER_SIZE

///////////////////////////////////////////////////////////////////////////////

class RTP:
	public RTCP
{
public:
    RTP();

    virtual ~RTP( );

    static int CodecToPayloadId(std::string pName);
    static std::string PayloadIdToCodec(int pId);
    static bool IsPayloadSupported(enum CodecID pId);
    static int GetPayloadHeaderSizeMax(enum CodecID pCodec);// calculate the maximum header size of the RTP payload (not the RTP header!)
    static int GetHeaderSizeMax(enum CodecID pCodec);
    static void SetH261PayloadSizeMax(unsigned int pMaxSize);
    static unsigned int GetH261PayloadSizeMax();

    /* RTP packetizing */
    bool RtpCreate(char *&pData, unsigned int &pDataSize);
    unsigned int GetLostPacketsFromRTP();
    static void LogRtpHeader(RtpHeader *pRtpHeader);
    bool RtpParse(char *&pData, unsigned int &pDataSize, bool &pIsLastFragment, bool &pIsSenderReport, enum CodecID pCodecId, bool pReadOnly);
    bool OpenRtpEncoder(std::string pTargetHost, unsigned int pTargetPort, AVStream *pInnerStream);
    bool CloseRtpEncoder();

    void RTPRegisterPacketStatistic(Homer::Monitor::PacketStatistic *pStatistic);

protected:
    int64_t GetPtsFromRTP(); // uses the timestamps from the RTP header to derive a valid PTS value

private:
    void AnnounceLostPackets(unsigned int pCount);

    /* internal RTP packetizer for h.261 */
    bool OpenRtpEncoderH261(std::string pTargetHost, unsigned int pTargetPort, AVStream *pInnerStream);
    bool RtpCreateH261(char *&pData, unsigned int &pDataSize);

    /* RTP packet stream */
    static int StoreRtpPacket(void *pOpaque, uint8_t *pBuffer, int pBufferSize);
    void OpenRtpPacketStream();
    int CloseRtpPacketStream(char** pBuffer);

    Homer::Monitor::PacketStatistic *mPacketStatistic;
    AVFormatContext     *mRtpFormatContext;
    int                 mPayloadId;
    bool                mIntermediateFragment;
    bool                mEncoderOpened;
    std::string         mTargetHost;
    unsigned int        mTargetPort;
    unsigned int        mLostPackets;
    unsigned int        mLocalSourceIdentifier;
    unsigned short int  mRemoteSequenceNumberLastPacket;
    unsigned int        mRemoteTimestampLastPacket;
    unsigned int        mRemoteTimestampLastCompleteFrame;
    unsigned int        mRemoteSourceIdentifier;
    unsigned int        mRemoteStartTimestamp;
    unsigned int        mRemoteTimestamp;
    /* MP3 RTP hack */
    unsigned int        mMp3Hack_EntireBufferSize;
    /* RTP packet stream */
    AVIOContext         *mAVIOContext;
    char                *mRtpPacketBuffer;
    char                *mRtpPacketStream;
    char                *mRtpPacketStreamPos;
    /* H261 RTP encoder */
    static unsigned int mH261PayloadSizeMax;
    bool                mH261UseInternalEncoder;
    unsigned short int  mH261LocalSequenceNumber;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
