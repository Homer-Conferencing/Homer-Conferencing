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

#include <sys/types.h>
#include <string>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of send RTP packets
//#define RTP_DEBUG_PACKET_ENCODER_FFMPEG
//#define RTP_DEBUG_PACKET_ENCODER

// the following de/activates debugging of received RTP packets
//#define RTP_DEBUG_PACKET_DECODER



// the following de/activates debugging of RTCP packets
//#define RTCP_DEBUG_PACKETS_DECODER

//#define RTCP_DEBUG_PACKETS_ENCODER
//#define RTCP_DEBUG_PACKET_ENCODER_FFMPEG

///////////////////////////////////////////////////////////////////////////////

// from libavformat/internal.h
#define NTP_OFFSET                          2208988800ULL
#define NTP_OFFSET_US                       (NTP_OFFSET * 1000000ULL)

///////////////////////////////////////////////////////////////////////////////

// ########################## RTCP ###########################################
union RtcpHeader{
    struct{ // send via separate port
        unsigned short int Length;          /* length of report */
        unsigned int Type:8;                /* report type */
        unsigned int RC:5;                  /* report counter */
        unsigned int Padding:1;             /* padding flag */
        unsigned int Version:2;             /* protocol version */
        unsigned int Ssrc;                  /* synchronization source */
        unsigned int Data[5];              /*  */
    } __attribute__((__packed__))General;
    struct{ // send within media stream as intermediate packets
        unsigned short int Length;          /* length of report */
        unsigned int Type:8;                /* Payload type (PT) */
        unsigned int Fmt:5;                 /* Feedback message type (FMT) */
        unsigned int Padding:1;             /* padding flag */
        unsigned int Version:2;             /* protocol version */
        unsigned int Ssrc;                  /* synchronization source */
        unsigned int TimestampHigh;         /* high part of reference timestamp */
        unsigned int TimestampLow;          /* low part of reference timestamp */
        unsigned int RtpTimestamp;          /* reference RTP timestamp */
        unsigned int Packets;               /* packet count */
        unsigned int Octets;                /* byte count */
    } __attribute__((__packed__))Feedback;
    uint32_t Data[7];
};

// calculate the size of an RTCP header: "size of structure"
#define RTCP_HEADER_SIZE                      sizeof(RtcpHeader)

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

///////////////////////////////////////////////////////////////////////////////

// calculate the size of an RTP header: "size of structure"
#define RTP_HEADER_SIZE                      sizeof(RtpHeader)

///////////////////////////////////////////////////////////////////////////////

class RTP
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

    /* RTP packetizing/parsing */
    bool RtpCreate(char *&pData, unsigned int &pDataSize, int64_t pPacketPts);
    unsigned int GetLostPacketsFromRTP();
    static void LogRtpHeader(RtpHeader *pRtpHeader);
    bool RtpParse(char *&pData, int &pDataSize, bool &pIsLastFragment, bool &pIsSenderReport, enum CodecID pCodecId, bool pReadOnly);
    bool OpenRtpEncoder(std::string pTargetHost, unsigned int pTargetPort, AVStream *pInnerStream);
    bool CloseRtpEncoder();

    void RTPRegisterPacketStatistic(Homer::Monitor::PacketStatistic *pStatistic);

    /* RTCP packetizing/parsing */
    static void LogRtcpHeader(RtcpHeader *pRtcpHeader);
    bool RtcpParseSenderReport(char *&pData, int &pDataSize, int64_t &pEndToEndDelay /* in micro seconds */, int &pPackets, int &pOctets);

protected:
    int64_t GetCurrentPtsFromRTP(); // uses the timestamps from the RTP header to derive a valid PTS value
    void GetSynchronizationReferenceFromRTP(uint64_t &pReferenceNtpTime, unsigned int &pReferencePts);

    /* for clock rate adaption, e.g., 8, 44.1, 90 kHz */
    float CalculateClockRateFactor();

private:
    void AnnounceLostPackets(unsigned int pCount);

    /* internal RTP packetizer for h.261 */
    bool OpenRtpEncoderH261(std::string pTargetHost, unsigned int pTargetPort, AVStream *pInnerStream);
    bool RtpCreateH261(char *&pData, unsigned int &pDataSize, int64_t pPacketPts);

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
    enum CodecID        mStreamCodecID;
    unsigned short int  mRemoteSequenceNumberLastPacket;
    unsigned int        mRemoteTimestampLastPacket;
    unsigned int        mRemoteTimestampLastCompleteFrame;
    unsigned int        mRemoteSourceIdentifier;
    unsigned int        mRemoteStartTimestamp;
    uint64_t            mRemoteTimestamp;
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
    /* RTCP */
    Mutex               mSynchDataMutex;
    uint64_t            mRtcpLastRemoteNtpTime; // (NTP timestamp)
    unsigned int        mRtcpLastRemoteTimestamp; // PTS value (without clock rata adaption!)
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
