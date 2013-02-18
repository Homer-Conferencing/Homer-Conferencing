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
 * Purpose: Implementation for real-time transport protocol
 * Author:  Thomas Volkert
 * Since:   2009-10-28
 */

/*
		 Result of functional validation (09.Jan. 2013):
				 Sending                        Receiving
		--------------------------------------------------------------------
		 h261:   ok (HC)                        ok (HC)
		 h263:   ok (Hc, Ekiga)                 ok (HC, Ekiga)
		h263+:   ok (Hc, Ekiga)                 ok (HC, Ekiga)
		 h264:   ok (Hc, Ekiga)                 ok (HC, Ekiga) [time compensation needed]
		Mpeg1:   ok (HC)                        ok (HC)
		Mpeg2:   ok (HC)                        ok (HC)
		Mpeg4:   ok (Hc, Ekiga)                 ok (HC, Ekiga)

  pcma (G711):   ok (Hc, Ekiga)                 ok (HC, Ekiga)
  pcmu (G711):   ok (Hc, Ekiga)                 ok (HC, Ekiga)
adpcm(G722):     ok (Hc, Ekiga)                 ok (HC, Ekiga)
		  mp3:   ok (HC)                        ok (HC) (sync. needs to be reworked)
	  pcm16be:   ok (HC)                        ok (HC)


     RTP parameters: http://www.iana.org/assignments/rtp-parameters

     MP3 hack:
         Problem: ffmpeg buffers mp3 fragments but there is no value in the RTP headers to store the size of the entire original MP3 buffer
         Solution: we store the size of all MP3 fragments in "mMp3Hack_EntireBufferSize" and set MBZ from the RTP header to this value
                   further, we use this value when parsing the RTP packets
 */

#include <string>
#include <sstream>

#include <RTP.h>
#include <Header_Ffmpeg.h>
#include <PacketStatistic.h>
#include <HBSocket.h>
#include <MediaSourceNet.h>
#include <Logger.h>

namespace Homer { namespace Multimedia {

using namespace std;
using namespace Homer::Monitor;

///////////////////////////////////////////////////////////////////////////////

// how many consecutive timestamp overflows do we want to accept before we assume that the remote source has changed?
#define RTP_MAX_CONSECUTIVE_TIMESTAMP_OVERFLOWS								1

// how many consecutive sequence number overflows do we want to accept before we assume that the remote source has changed?
#define RTP_MAX_CONSECUTIVE_SEQUENCE_NUMBER_OVERFLOWS						1

#define RTP_MAX_REMOTE_SOURCE_CHANGED_RESET_SCORE                          32 // how many times should the same payload ID be received until we believe in an actual source change at remote side?

///////////////////////////////////////////////////////////////////////////////

#define IS_RTCP_TYPE(x) 				((x >= 72) && (x <= 76))

///////////////////////////////////////////////////////////////////////////////

#define RTP_PAYLOAD_TYPE_NONE											0x7F

///////////////////////////////////////////////////////////////////////////////

/* ##################################################################################
// ########################## Resulting packet structure ############################
// ##################################################################################
  Size  Offfset        Element
    20      0           IP header
     8     20           UDP header
    12     28           RTP header
     m     40           RTP payload header
     n     40+m         RTP payload = codec packet (rtcp feedback, h261, h263, h263+, h264, mpeg4, PCMA, PCMU, MP3 a.s.o.)

    Ethernet/WLan frame(1500) - IP(20)-UDP(8)-RTP(12) = 1460 bytes RTP payload limit
 */
unsigned int RTP::mH261PayloadSizeMax = 0;

///////////////////////////////////////////////////////////////////////////////

// ########################## AMR-NB (RFC 3267) ###########################################
union AMRNBHeader{ //TODO
    struct{
        unsigned int dummy0:24;
        unsigned int Mi:3;                  /* Mode Index (MI) */
        unsigned int Reserved:5;            /* Reserved bits */
    } __attribute__((__packed__));
    uint32_t Data[1];
};

// ########################## G.711 ###########################################
union G711Header{
    struct{
        unsigned int dummy0:24;
        unsigned int Mi:3;                  /* Mode Index (MI) */
        unsigned int Reserved:5;            /* Reserved bits */
    } __attribute__((__packed__));
    uint32_t Data[1];
};

// ############################ MP3 ADU #######################################
union MP3AduHeader{
    struct{
        unsigned int dummy1:16;
        unsigned int SizeExt:8;
        unsigned int Size:6;                /* ADU size */
        unsigned int T:1;                   /* Descriptor Type flag */
        unsigned int C:1;                   /* Continuation flag */
    } __attribute__((__packed__));
    uint32_t Data[1];
};

// ############################ MPA audio #####################################
union MPAHeader{
    struct{
        unsigned short int Offset:16;       /* fragmentation offset */
        unsigned short int Mbz:16;          /* some funny zero values */
    } __attribute__((__packed__));
    uint32_t Data[1];
};

// ############################ MPV video #####################################
union MPVHeader{
    struct{
        unsigned int Ffc:3;                 /* forward f code? */
        unsigned int Ffv:1;                 /* full pel forward vector? */
        unsigned int Bfc:3;                 /* backward f code? */
        unsigned int Fbv:1;                 /* full pel backward vector? */
        unsigned int PType:3;               /* picture type */
        unsigned int E:1;                   /* end of slice? */
        unsigned int B:1;                   /* beginning of slice? */
        unsigned int S:1;                   /* sequence header present? */
        unsigned int N:1;                   /* N bit: new picture header? */
        unsigned int An:1;                  /* active N bit */
        unsigned int Tr:10;                 /* temporal reference: chronological order of pictures within current GOP */
        unsigned int T:1;                   /* two headers present? next one is MPEG 2 header */
        unsigned int Mbz:5;                 /* some funny zero values */
    } __attribute__((__packed__));
    uint32_t Data[1];
};

// ####################### MPV mpeg2 extension #################################
union MPVMpeg2Header{
    struct{
        unsigned short int dummy0;
        unsigned short int dummy1;
/*        X: Unused (1 bit). Must be set to zero in current
           specification. This space is reserved for future use.
        E: Extensions present (1 bit). If set to 1, this header
        f_[0,0]: forward horizontal f_code (4 bits)
        f_[0,1]: forward vertical f_code (4 bits)
        f_[1,0]: backward horizontal f_code (4 bits)
        f_[1,1]: backward vertical f_code (4 bits)
        DC: intra_DC_precision (2 bits)
        PS: picture_structure (2 bits)
        T: top_field_first (1 bit)
        P: frame_predicted_frame_dct (1 bit)
        C: concealment_motion_vectors (1 bit)
        Q: q_scale type (1 bit)
        V: intra_vlc_format (1 bit)
        A: alternate scan (1 bit)
        R: repeat_first_field (1 bit)
        H: chroma_420_type (1 bit)
        G: progressive frame (1 bit)
        D: composite_display_flag (1 bit). If set to 1, next 32 bits
           following this one contains 12 zeros followed by 20 bits
           of composite display information.*/
    } __attribute__((__packed__));
    uint32_t Data[1];
};

// ########################## H 261 ###########################################
union H261Header{
    struct{
        unsigned int Vmvd:5;                /* Vertical motion vector data (VMVD) */
        unsigned int Hmvd:5;                /* Horizontal motion vector data (HMVD */
        unsigned int Quant:5;               /* Quantizer (QUANT) */
        unsigned int Mbap:5;                /* Macroblock address predictor (MBAP) */
        unsigned int Gobn:4;                /* GOB number (GOBN) */
        unsigned int V:1;                   /* Motion Vector flag (V) */
        unsigned int I:1;                   /* INTRA-frame encoded data (I) */
        unsigned int Ebit:3;                /* End bit position (EBIT) */
        unsigned int Sbit:3;                /* Start bit position (SBIT) */
    } __attribute__((__packed__));
    uint32_t Data[1];
};

#define H261_HEADER_SIZE                    sizeof(H261Header)

// **** H261/RTP payload limitation workaround ***********************************
// HINT: workaround, but static value: RTP_MAX_PAYLOAD_SIZE - H261_HEADER_SIZE,
//       at the moment the packet size limitation works via the limitation of codec packets
#define RTP_MAX_H261_PAYLOAD_SIZE           (mH261PayloadSizeMax)

// *******************************************************************************

// ########################## H 263 ############################################
union H263Header{
    struct{
        unsigned int dummy0:21;
        unsigned int Src:3;                 /* source format: resolution of current picture */
        unsigned int Ebit:3;                /* End bit position (EBIT) */
        unsigned int Sbit:3;                /* Start bit position (SBIT) */
        unsigned int P:1;                   /* optional p-frames: "0" implies normal I or P frame, "1" PB-frames. When F=1, P also indicates modes: mode B if P=0, mode C if P=1 */
        unsigned int F:1;                   /* mode flag: F=0, mode A; F=1, mode B or mode C depending on P bit */

        unsigned int dummy1, dummy2;
    } __attribute__((__packed__));
    struct{
        unsigned int Tr:8;                  /* Temporal Reference for p-frames */
        unsigned int Trb:3;                 /* Temporal Reference for b-frames */
        unsigned int Dbq:2;                 /* Differential quantization parameter */
        unsigned int Reserved:4;            /* Reserved bits */
        unsigned int A:1;                   /* Advanced Prediction option */
        unsigned int S:1;                   /* Syntax-based Arithmetic Coding option */
        unsigned int U:1;                   /* Unrestricted Motion Vector option */
        unsigned int I:1;                   /* i-frame: "0" is intra-coded, "1" is inter-coded */
        unsigned int Src:3;                 /* source format: resolution of current picture */
        unsigned int Ebit:3;                /* End bit position (EBIT) */
        unsigned int Sbit:3;                /* Start bit position (SBIT) */
        unsigned int P:1;                   /* optional p-frames: "0" implies normal I or P frame, "1" PB-frames. When F=1, P also indicates modes: mode B if P=0, mode C if P=1 */
        unsigned int F:1;                   /* mode flag: F=0, mode A; F=1, mode B or mode C depending on P bit */

        unsigned int dummy1, dummy2;
    } __attribute__((__packed__)) ModeA;
    struct{
        unsigned int Reserved:2;            /* Reserved bits */
        unsigned int Mba:9;                 /* address within the GOB */
        unsigned int Gobn:5;                /* GOB number */
        unsigned int Quant:5;               /* Quantizer (QUANT) */
        unsigned int Src:3;                 /* source format: resolution of current picture */
        unsigned int Ebit:3;                /* End bit position (EBIT) */
        unsigned int Sbit:3;                /* Start bit position (SBIT) */
        unsigned int P:1;                   /* optional p-frames: "0" implies normal I or P frame, "1" PB-frames. When F=1, P also indicates modes: mode B if P=0, mode C if P=1 */
        unsigned int F:1;                   /* mode flag: F=0, mode A; F=1, mode B or mode C depending on P bit */

        unsigned int Vmv2:7;                /* Vertical motion vector predictor */
        unsigned int Hmv2:7;                /* Horizontal motion vector predictor */
        unsigned int Vmv1:7;                /* Vertical motion vector predictor */
        unsigned int Hmv1:7;                /* Horizontal motion vector predictor */
        unsigned int A:1;                   /* Advanced Prediction option */
        unsigned int S:1;                   /* Syntax-based Arithmetic Coding option */
        unsigned int U:1;                   /* Unrestricted Motion Vector option */
        unsigned int I:1;                   /* i-frame: "0" is intra-coded, "1" is inter-coded */

        unsigned int dummy1;
    } __attribute__((__packed__)) ModeB;
    struct{
        unsigned int Reserved1:2;           /* Reserved bits */
        unsigned int Mba:9;                 /* address within the GOB */
        unsigned int Gobn:5;                /* GOB number */
        unsigned int Quant:5;               /* Quantizer (QUANT) */
        unsigned int Src:3;                 /* source format: resolution of current picture */
        unsigned int Ebit:3;                /* End bit position (EBIT) */
        unsigned int Sbit:3;                /* Start bit position (SBIT) */
        unsigned int P:1;                   /* optional p-frames: "0" implies normal I or P frame, "1" PB-frames. When F=1, P also indicates modes: mode B if P=0, mode C if P=1 */
        unsigned int F:1;                   /* mode flag: F=0, mode A; F=1, mode B or mode C depending on P bit */

        unsigned int Vmv2:7;                /* Vertical motion vector predictor */
        unsigned int Hmv2:7;                /* Horizontal motion vector predictor */
        unsigned int Vmv1:7;                /* Vertical motion vector predictor */
        unsigned int Hmv1:7;                /* Horizontal motion vector predictor */
        unsigned int A:1;                   /* Advanced Prediction option */
        unsigned int S:1;                   /* Syntax-based Arithmetic Coding option */
        unsigned int U:1;                   /* Unrestricted Motion Vector option */
        unsigned int I:1;                   /* i-frame: "0" is intra-coded, "1" is inter-coded */

        unsigned int Tr:8;                  /* Temporal Reference for p-frames */
        unsigned int Trb:3;                 /* Temporal Reference for b-frames */
        unsigned int Dbq:2;                 /* Differential quantization parameter */
        unsigned int Reserved2:19;          /* Reserved bits */
    } __attribute__((__packed__)) ModeC;
    uint32_t Data[3];
};

// ########################## H 263+ ###########################################
union H263PHeader{
    struct{
        unsigned int dummy1:16;
        unsigned int Pebit:3;               /* amount of ignored bits (PEBIT) */
        unsigned int Plen:6;                /* extra picture header length (PLEN) */
        unsigned int V:1;                   /* Video Redundancy Coding (VRC) */
        unsigned int P:1;                   /* picture start (P) */
        unsigned int Reserved:5;            /* Reserved bits (PR) */
    } __attribute__((__packed__));
    struct{
        unsigned int dummy1:8;

        unsigned int S:1;                   /* sync frame? (S) */
        unsigned int Trunc:4;               /* packet number within each thread */
        unsigned int Tid:3;                 /* Thread ID (TID) */

        unsigned int Pebit:3;               /* amount of ignored bits (PEBIT) */
        unsigned int Plen:6;                /* extra picture header length (PLEN) */
        unsigned int V:1;                   /* Video Redundancy Coding (VRC) */
        unsigned int P:1;                   /* picture start (P) */
        unsigned int Reserved:5;            /* Reserved bits (PR) */
    } __attribute__((__packed__))Vrc;
    uint32_t Data[1];
};

// ########################## H 264 ###########################################
union H264Header{
    struct{
        unsigned int dummy1:24;
        unsigned int Type:5;                /* NAL unit type */
        unsigned int Nri:2;                 /* NAL reference indicator (NRI) */
        unsigned int F:1;                   /* forbidden zero bit (F) */
    } __attribute__((__packed__));
    struct{
        unsigned int dummy1:24;
        unsigned int Type:5;                /* NAL unit type */
        unsigned int Nri:2;                 /* NAL reference indicator (NRI) */
        unsigned int F:1;                   /* forbidden zero bit (F) */
    } __attribute__((__packed__))StapA;
    struct{
        unsigned int dummy1:8;
        unsigned short int Don;             /* decoding order number (DON) */
        unsigned int Type:5;                /* NAL unit type */
        unsigned int Nri:2;                 /* NAL reference indicator (NRI) */
        unsigned int F:1;                   /* forbidden zero bit (F) */
    } __attribute__((__packed__))StapB, Mtap16, Mtap32;
    struct{
        unsigned int dummy1:16;

        unsigned int PlType:5;              /* NAL unit payload type */
        unsigned int R:1;                   /* reserved bit, must be 0 */
        unsigned int E:1;                   /* end of a fragmented NAL unit (E) */
        unsigned int S:1;                   /* start of a fragmented NAL unit (S) */

        unsigned int Type:5;                /* NAL unit type */
        unsigned int Nri:2;                 /* NAL reference indicator (NRI) */
        unsigned int F:1;                   /* forbidden zero bit (F) */
    } __attribute__((__packed__))FuA;
    struct{
        unsigned short int Don;             /* decoding order number (DON) */

        unsigned int PlType:5;              /* NAL unit payload type */
        unsigned int R:1;                   /* reserved bit, must be 0 */
        unsigned int E:1;                   /* end of a fragmented NAL unit (E) */
        unsigned int S:1;                   /* start of a fragmented NAL unit (S) */

        unsigned int Type:5;                /* NAL unit type */
        unsigned int Nri:2;                 /* NAL reference indicator (NRI) */
        unsigned int F:1;                   /* forbidden zero bit (F) */
    } __attribute__((__packed__))FuB;
    uint32_t Data[1];
};

// ############################ THEORA) ############################################
union THEORAHeader{
    struct{
        unsigned int Packets:4;             /* packet count */
        unsigned int TDT:2;                 /* data type */
        unsigned int F:2;                   /* fragment type */
        unsigned int ConfigId:24;           /* configuration ID */
    } __attribute__((__packed__));
    uint32_t Data[1];
};

// ########################## VP8 (webm) ###########################################
union VP8Header{
    struct{
        unsigned int PartID:4;              /* partition index */
        unsigned int S:1;                   /* start of VP8 partition */
        unsigned int N:1;                   /* non-reference frame */
        unsigned int R:1;                   /* must be zero */
        unsigned int X:1;                   /* Extended control bits present */
    } __attribute__((__packed__));
    uint8_t Data[1];
};
union VP8ExtendedHeader{
    struct{
        unsigned int RsvA:5;                 /* must be zero */
        unsigned int T:1;                   /* TID present */
        unsigned int L:1;                   /* TL0PICIDX present */
        unsigned int I:1;                   /* picture ID present */
    } __attribute__((__packed__));
    uint8_t Data[1];
};

///////////////////////////////////////////////////////////////////////////////
// NTP time handling

// from libavformat/internal.h
#define NTP_OFFSET                          2208988800ULL
#define NTP_OFFSET_US                       (NTP_OFFSET * 1000000ULL)

uint64_t RTP::GetNtpTime()
{
    return (av_gettime() / 1000) * 1000 + NTP_OFFSET_US;
}

///////////////////////////////////////////////////////////////////////////////

RTP::RTP()
{
    LOG(LOG_VERBOSE, "Created");
    mH261LocalSequenceNumber = 0;
    mIntermediateFragment = 0;
    mPacketStatistic = NULL;
    mRtpFormatContext = NULL;
    mRtpEncoderOpened = false;
    mH261UseInternalEncoder = false;
    mRtpPacketStream = NULL;
    mRtpPacketBuffer = NULL;
    mTargetHost = "";
    mTargetPort = 0;
    mStreamCodecID = CODEC_ID_NONE;
    mLocalSourceIdentifier = 0;
    Init();
}

RTP::~RTP()
{
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

void RTP::SetH261PayloadSizeMax(unsigned int pMaxSize)
{//workaround for separation of RTP packetizer and the payload limit problem which is caused by the missing RTP support for H261 within ffmpeg
    mH261PayloadSizeMax = pMaxSize - RTP_HEADER_SIZE - H261_HEADER_SIZE;
}

unsigned int RTP::GetH261PayloadSizeMax()
{
    return mH261PayloadSizeMax;
}

///////////////////////////////////////////////////////////////////////////////

void RTP::Init()
{
    mLocalTimestampOffset = 0;
    mRemoteSourceChangedLastPayload = 0;
    mRemoteSourceChangedResetScore = 0;
    mRtcpLastReceivedPackets = 0;
    mReceivedPackets = 0;
    mRtcpLastRemotePackets = 0;
    mRtcpLastRemoteOctets = 0;
    // reset variables
    mRemoteSequenceNumberLastPacket = 0;
    mRemoteTimestampLastPacket = 0;
    mRemoteTimestampLastCompleteFrame = 0;
    mRemoteSourceIdentifier = 0;
    mRemoteStartTimestamp = 0;
    mRemoteStartSequenceNumber = 0;
    mRtcpLastRemoteTimestamp = 0;
    mRtcpLastRemotePackets = 0;
    mRtcpLastRemoteOctets = 0;
    mRtcpLastRemoteNtpTime = 0;
    mRemoteTimestampOverflowShift = 0;
    mRemoteTimestampConsecutiveOverflows = 0;
    mRemoteTimestamp = 0;
    mLastTimestampFromRTPHeader = 0;
    mLastSequenceNumberFromRTPHeader = 0;
    mLostPackets = 0;
    if (mPacketStatistic != NULL)
        mPacketStatistic->SetLostPacketCount(0);
    mPayloadId = RTP_PAYLOAD_TYPE_NONE;
    mRemoteSequenceNumberOverflowShift = 0;
    mRemoteSequenceNumber = 0;
    mRtpEncoderStream = NULL;
    mRtcpLastSenderReport = NULL;
    mRtpRemoteSourceChanged = false;
    mRTCPPacketCounter = 0;
    mRTPPacketCounter = 0;
    mSyncPTS = 0;
    mSyncNTPTime = 0;
}

bool RTP::OpenRtpEncoderH261(string pTargetHost, unsigned int pTargetPort, AVStream *pInnerStream)
{
    LOG(LOG_VERBOSE, "Using lib internal rtp packetizer for H261 codec");
    LOG(LOG_INFO, "Opened...");
    LOG(LOG_INFO, "    ..rtp target: %s:%u", pTargetHost.c_str(), pTargetPort);
    LOG(LOG_INFO, "    ..rtp header size: %d", RTP_HEADER_SIZE);
    LOG(LOG_INFO, "    ..rtp SRC: %u", mLocalSourceIdentifier);
    LOG(LOG_INFO, "  Wrapping following codec...");
    LOG(LOG_INFO, "    ..codec name: %s", pInnerStream->codec->codec->name);
    LOG(LOG_INFO, "    ..codec long name: %s", pInnerStream->codec->codec->long_name);
    LOG(LOG_INFO, "    ..resolution: %d * %d pixels", pInnerStream->codec->width, pInnerStream->codec->height);
//    LOG(LOG_INFO, "    ..codec time_base: %d/%d", mCodecContext->time_base.num, mCodecContext->time_base.den);
    LOG(LOG_INFO, "    ..stream rfps: %d/%d", pInnerStream->r_frame_rate.num, pInnerStream->r_frame_rate.den);
    LOG(LOG_INFO, "    ..stream time_base: %d/%d", pInnerStream->time_base.num, pInnerStream->time_base.den);
    LOG(LOG_INFO, "    ..stream codec time_base: %d/%d", pInnerStream->codec->time_base.num, pInnerStream->codec->time_base.den);
    LOG(LOG_INFO, "    ..sample rate: %d Hz", pInnerStream->codec->sample_rate);
    LOG(LOG_INFO, "    ..channels: %d", pInnerStream->codec->channels);
    LOG(LOG_INFO, "    ..i-frame distance: %d pictures", pInnerStream->codec->gop_size);
    LOG(LOG_INFO, "    ..bit rate: %d Hz", pInnerStream->codec->bit_rate);
    LOG(LOG_INFO, "    ..qmin: %d", pInnerStream->codec->qmin);
    LOG(LOG_INFO, "    ..qmax: %d", pInnerStream->codec->qmax);
    LOG(LOG_INFO, "    ..mpeg quant: %d", pInnerStream->codec->mpeg_quant);
    LOG(LOG_INFO, "    ..pixel format: %d", (int)pInnerStream->codec->pix_fmt);
    LOG(LOG_INFO, "    ..sample format: %d", (int)pInnerStream->codec->sample_fmt);
    LOG(LOG_INFO, "    ..frame size: %d bytes", pInnerStream->codec->frame_size);
    //LOG(LOG_INFO, "    ..max packet size: %d bytes", mRtpFormatContext->pb->max_packet_size);
    LOG(LOG_INFO, "    ..rtp payload size: %d bytes", pInnerStream->codec->rtp_payload_size);
    mRtpEncoderOpened = true;
    mH261UseInternalEncoder = true;
    mH261FirstPacket = true;
    mH261SentPackets = 0;
    mH261SentOctets = 0;
    mH261SenderReports = 0;
    mH261SentNtpTimeBase = GetNtpTime();
    mH261SentOctetsLastSenderReport = 0;
    mH261SentNtpTimeLastSenderReport = 0;
    return true;
}

bool RTP::OpenRtpEncoder(string pTargetHost, unsigned int pTargetPort, AVStream *pInnerStream)
{
    AVDictionary        *tOptions = NULL;
    int					tRes;

    if (mRtpEncoderOpened)
        return false;

    mRtpPacketStream = (char*)malloc(MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE);
    if (mRtpPacketStream == NULL)
        LOG(LOG_ERROR, "Error when allocating memory for RTP packet stream");
    else
        LOG(LOG_VERBOSE, "Created RTP packet stream memory of %d bytes at %p", MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE, mRtpPacketStream);
    mRtpPacketBuffer = (char*)malloc(pInnerStream->codec->rtp_payload_size);
    if (mRtpPacketBuffer == NULL)
        LOG(LOG_ERROR, "Error when allocating memory for RTP packet buffer");
    else
        LOG(LOG_VERBOSE, "Created RTP packet buffer memory of %d bytes at %p", MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE, mRtpPacketBuffer);

    const char *tCodecName = pInnerStream->codec->codec->name;
    mPayloadId = CodecToPayloadId(tCodecName);
    LOG(LOG_VERBOSE, "New payload id: %4u, Codec: %s", mPayloadId, tCodecName);

    mTargetHost = pTargetHost;
    mTargetPort = pTargetPort;
    mStreamCodecID = pInnerStream->codec->codec_id;

    Init();

    // set SRC ID
    mLocalSourceIdentifier = av_get_random_seed();

    if (mStreamCodecID == CODEC_ID_H261)
    	return OpenRtpEncoderH261(pTargetHost, pTargetPort, pInnerStream);

    int                 tResult;
    AVOutputFormat      *tFormat;

    // allocate new format context
    mRtpFormatContext = AV_NEW_FORMAT_CONTEXT();

    // find format
    tFormat = AV_GUESS_FORMAT("rtp", NULL, NULL);
    if (tFormat == NULL)
    {
        LOG(LOG_ERROR, "Invalid suggested format");

        // Close the format context
        av_free(mRtpFormatContext);

        return false;
    }

    // set correct output format
    mRtpFormatContext->oformat = tFormat;

    // verbose timestamp debugging
    if (LOGGER.GetLogLevel() == LOG_WORLD)
    {
        LOG(LOG_WARN, "Enabling ffmpeg timestamp debugging for RTP packetizer");
        mRtpFormatContext->debug = FF_FDEBUG_TS;
    }

    // allocate new stream structure
    mRtpEncoderStream = HM_avformat_new_stream(mRtpFormatContext, 0);
    if (mRtpEncoderStream == NULL)
    {
        LOG(LOG_ERROR, "Memory allocation failed");
        return false;
    }

    // copy stream description from original stream description
    memcpy(mRtpEncoderStream, pInnerStream, sizeof(AVStream));
    mRtpEncoderStream->priv_data = NULL;
    // create monotone timestamps
    mRtpEncoderStream->cur_dts = 0;
    mRtpEncoderStream->reference_dts = 0;

    // set target coordinates for rtp stream
    snprintf(mRtpFormatContext->filename, sizeof(mRtpFormatContext->filename), "rtp://%s:%u", pTargetHost.c_str(), pTargetPort);

    // create I/O context which splits RTP stream into packets
    MediaSource::CreateIOContext(mRtpPacketBuffer, mRtpEncoderStream->codec->rtp_payload_size, NULL, StoreRtpPacket, this, &mAVIOContext);

    // set new I/O context
    mRtpFormatContext->pb = mAVIOContext;

    // limit packet size, otherwise ffmpeg will deliver unpredictable results ;)
    mRtpFormatContext->pb->max_packet_size = mAVIOContext->max_packet_size;

    mRtpFormatContext->start_time_realtime = av_gettime();

//    if ((tRes = av_dict_set(&tOptions, "cname", "www.homer-conferencing.com", 0)) < 0)
//    	LOG(LOG_ERROR, "Failed to set A/V option \"cname\" because %s(0x%x) [option not found = 0x%x]", strerror(AVUNERROR(tRes)), tRes, AVERROR_OPTION_NOT_FOUND);
//
//    if ((tRes = av_dict_set(&tOptions, "ssrc", toString(mLocalSourceIdentifier).c_str(), 0)) < 0)
//        LOG(LOG_ERROR, "Failed to set A/V option \"ssrc\" because %s(0x%x)", strerror(AVUNERROR(tRes)), tRes);

    switch(mStreamCodecID)
    {
        case CODEC_ID_H263:
                // use older rfc2190 for RTP packetizing
                if ((tRes = av_opt_set(mRtpFormatContext->priv_data, "rtpflags", "rfc2190", 0)) < 0)
                	LOG(LOG_ERROR, "Failed to set A/V option \"rtpflags\" because %s(0x%x)", strerror(AVUNERROR(tRes)), tRes);
                break;
        default:
                break;
    }

    // Dump information about device file
    av_dump_format(mRtpFormatContext, 0, "RTP Encoder", true);

    // open RTP stream for avformat_Write_header()
    OpenRtpPacketStream();

    // allocate streams private data buffer and write the streams header, if any
    avformat_write_header(mRtpFormatContext, NULL);

    // close memory stream
    char *tBuffer = NULL;
    CloseRtpPacketStream(&tBuffer);

    int64_t tAVPacketPts = (float)mRtpEncoderStream->pts.val + mRtpEncoderStream->pts.num / mRtpEncoderStream->pts.den;

    LOG(LOG_INFO, "Opened...");
    LOG(LOG_INFO, "    ..rtp target: %s:%u", pTargetHost.c_str(), pTargetPort);
    LOG(LOG_INFO, "    ..rtp header size: %d", RTP_HEADER_SIZE);
    LOG(LOG_INFO, "  Wrapping following codec...");
    LOG(LOG_INFO, "    ..codec name: %s", mRtpEncoderStream->codec->codec->name);
    LOG(LOG_INFO, "    ..codec long name: %s", mRtpEncoderStream->codec->codec->long_name);
    LOG(LOG_INFO, "    ..resolution: %d * %d pixels", mRtpEncoderStream->codec->width, mRtpEncoderStream->codec->height);
//    LOG(LOG_INFO, "    ..codec time_base: %d/%d", mCodecContext->time_base.den, mCodecContext->time_base.num); // inverse
    LOG(LOG_INFO, "    ..stream start real-time: %"PRId64"", mRtpFormatContext->start_time_realtime);
    LOG(LOG_INFO, "    ..stream start time: %"PRId64"", mRtpEncoderStream->start_time);
    LOG(LOG_INFO, "    ..max. delay: %d", mRtpFormatContext->max_delay);
    LOG(LOG_INFO, "    ..audio preload: %d", mRtpFormatContext->audio_preload);
    LOG(LOG_INFO, "    ..start A/V PTS: %"PRId64"", tAVPacketPts);
    LOG(LOG_INFO, "    ..stream rfps: %d/%d", mRtpEncoderStream->r_frame_rate.num, mRtpEncoderStream->r_frame_rate.den);
    LOG(LOG_INFO, "    ..stream time_base: %d/%d", mRtpEncoderStream->time_base.num, mRtpEncoderStream->time_base.den);
    LOG(LOG_INFO, "    ..stream codec time_base: %d/%d", mRtpEncoderStream->codec->time_base.num, mRtpEncoderStream->codec->time_base.den);
    LOG(LOG_INFO, "    ..sample rate: %d Hz", mRtpEncoderStream->codec->sample_rate);
    LOG(LOG_INFO, "    ..channels: %d", mRtpEncoderStream->codec->channels);
    LOG(LOG_INFO, "    ..i-frame distance: %d pictures", mRtpEncoderStream->codec->gop_size);
    LOG(LOG_INFO, "    ..bit rate: %d bit/s", mRtpEncoderStream->codec->bit_rate);
    LOG(LOG_INFO, "    ..qmin: %d", mRtpEncoderStream->codec->qmin);
    LOG(LOG_INFO, "    ..qmax: %d", mRtpEncoderStream->codec->qmax);
    LOG(LOG_INFO, "    ..mpeg quant: %d", mRtpEncoderStream->codec->mpeg_quant);
    LOG(LOG_INFO, "    ..pixel format: %d", (int)mRtpEncoderStream->codec->pix_fmt);
    LOG(LOG_INFO, "    ..sample format: %d", (int)mRtpEncoderStream->codec->sample_fmt);
    LOG(LOG_INFO, "    ..frame size: %d bytes", mRtpEncoderStream->codec->frame_size);
    LOG(LOG_INFO, "    ..max packet size: %d bytes", mAVIOContext->max_packet_size);
    LOG(LOG_INFO, "    ..rtp payload size: %d bytes", mRtpEncoderStream->codec->rtp_payload_size);

    mRtpEncoderOpened = true;
    mMp3Hack_EntireBufferSize = 0;

    return true;
}

bool RTP::CloseRtpEncoder()
{
    LOG(LOG_VERBOSE, "Going to close");

    if (mRtpEncoderOpened)
    {
        if (!mH261UseInternalEncoder /* h261 */)
        {
            // write the trailer, if any
            av_write_trailer(mRtpFormatContext);

            // close RTP stream
            av_free(mAVIOContext);

            // free stream 0
            av_freep(&mRtpEncoderStream);

            // Close the format context
            av_free(mRtpFormatContext);
        }

        if (mRtpPacketStream != NULL)
        {
            delete mRtpPacketBuffer;
            mRtpPacketBuffer = NULL;
            delete mRtpPacketStream;
            mRtpPacketStream = NULL;
        }
        LOG(LOG_INFO, "...closed");
    }else
        LOG(LOG_INFO, "...wasn't open");

    mRtpEncoderOpened = false;
    mH261UseInternalEncoder = false;

    return true;
}

void RTP::RTPRegisterPacketStatistic(PacketStatistic *pStatistic)
{
    mPacketStatistic = pStatistic;
}

unsigned int RTP::GetRTPPayloadType()
{
    return mPayloadId;
}

bool RTP::IsPayloadSupported(enum CodecID pId)
{
    bool tResult = false;

    // check for supported codecs
    switch(pId)
    {
            // list from "libavformat::rtpenc.c::is_supported"
            case CODEC_ID_H261:
            case CODEC_ID_H263:
            case CODEC_ID_H263P:
            case CODEC_ID_H264:
            case CODEC_ID_MPEG1VIDEO:
            case CODEC_ID_MPEG2VIDEO:
            case CODEC_ID_MPEG4:
            case CODEC_ID_MP3:
            case CODEC_ID_PCM_ALAW:
            case CODEC_ID_PCM_MULAW:
            case CODEC_ID_PCM_S16BE:
//            case CODEC_ID_MPEG2TS:
//            case CODEC_ID_VORBIS:
            case CODEC_ID_THEORA:
            case CODEC_ID_VP8:
            case CODEC_ID_ADPCM_G722:
//            case CODEC_ID_ADPCM_G726:
                            tResult = true;
                            break;
            default:
                            tResult = false;
                            break;
    }
    return tResult;
}

int RTP::GetPayloadHeaderSizeMax(enum CodecID pCodec)
{
    int tResult = 0;

    if (!IsPayloadSupported(pCodec))
        return 0;

    switch(pCodec)
    {
            // list from "libavformat::rtpenc.c::is_supported"
            case CODEC_ID_H261:
                tResult = sizeof(H261Header);
                break;
            case CODEC_ID_H263:
                tResult = sizeof(H263Header);
                break;
            case CODEC_ID_H263P:
                tResult = sizeof(H263PHeader);
                break;
            case CODEC_ID_H264:
                tResult = sizeof(H264Header);
                break;
            case CODEC_ID_MPEG1VIDEO:
            case CODEC_ID_MPEG2VIDEO:
                tResult = sizeof(MPVHeader); //HINT: we neglect the MPEG2 add-on header
                break;
            case CODEC_ID_MPEG4:
                tResult = 0;
                break;
            case CODEC_ID_MP3:
                tResult = sizeof(MPAHeader);
                break;
            case CODEC_ID_PCM_ALAW:
                tResult = 0;
                break;
            case CODEC_ID_PCM_MULAW:
                tResult = 0;
                break;
            case CODEC_ID_PCM_S16BE:
                tResult = 0;
                break;
//            case CODEC_ID_MPEG2TS:
//            case CODEC_ID_VORBIS:
            case CODEC_ID_THEORA:
                tResult = sizeof(THEORAHeader);
                break;
            case CODEC_ID_VP8:
                tResult = sizeof(VP8Header); // we neglect the extended header and the 3 other optional header bytes
                break;
            case CODEC_ID_ADPCM_G722:
            	tResult = 0;
            	break;
//            case CODEC_ID_ADPCM_G726:
            default:
                tResult = 0;
                break;
    }

    return tResult;
}

int RTP::GetHeaderSizeMax(enum CodecID pCodec)
{
    return RTP_HEADER_SIZE + GetPayloadHeaderSizeMax(pCodec);
}

void RTP::OpenRtpPacketStream()
{
    mRtpPacketStreamPos = mRtpPacketStream;
}

int RTP::CloseRtpPacketStream(char** pBuffer)
{
    *pBuffer = mRtpPacketStream;
    return (mRtpPacketStreamPos - mRtpPacketStream);
}

int RTP::StoreRtpPacket(void *pOpaque, uint8_t *pBuffer, int pBufferSize)
{
    RTP* tRTPInstance = (RTP*)pOpaque;

    #if defined(RTP_DEBUG_PACKET_ENCODER_FFMPEG) || defined(RTCP_DEBUG_PACKET_ENCODER_FFMPEG)
        LOGEX(RTP, LOG_VERBOSE, "Storing RTP packet of %d bytes", pBufferSize);
        RtpHeader *tRtpHeader = (RtpHeader*)pBuffer;


        for (int i = 0; i < 3; i++)
            tRtpHeader->Data[i] = ntohl(tRtpHeader->Data[i]);
        int tPayloadType = tRtpHeader->PayloadType;
        for (int i = 0; i < 3; i++)
            tRtpHeader->Data[i] = htonl(tRtpHeader->Data[i]);

        RtcpHeader *tRtcpHeader = (RtcpHeader*)pBuffer;
        if ((tPayloadType < 72) || (tPayloadType > 76))
        {
            #ifdef RTP_DEBUG_PACKET_ENCODER_FFMPEG
                LogRtpHeader(tRtpHeader);
            #endif
        }else
        {
            #ifdef RTCP_DEBUG_PACKET_ENCODER_FFMPEG
                LogRtcpHeader(tRtcpHeader);
            #endif
        }
    #endif
    if (!tRTPInstance->mRtpEncoderOpened)
        LOGEX(RTP, LOG_ERROR, "RTP instance wasn't opened yet, RTP packetizing not available");

    // write RTP packet size
    unsigned int *tRtpPacketSize = (unsigned int*)tRTPInstance->mRtpPacketStreamPos;

    *tRtpPacketSize = 0;
    *tRtpPacketSize = htonl((uint32_t) pBufferSize);

    // increase RTP stream position by 4
    tRTPInstance->mRtpPacketStreamPos += 4;

    // copy data from original buffer
    char *tRtpPacket = (char*)tRTPInstance->mRtpPacketStreamPos;
    memcpy(tRtpPacket, pBuffer, pBufferSize);

    // increase RTP stream position by size of RTP packet
    tRTPInstance->mRtpPacketStreamPos += pBufferSize;

    // return the size of the entire RTP packet buffer as result of write operation
    return pBufferSize;
}

int64_t RTP::ReceivedRTPPackets()
{
	return mRTPPacketCounter;
}

int64_t RTP::ReceivedRTCPPackets()
{
	return mRTCPPacketCounter;
}

bool RTP::RtpCreate(char *&pData, unsigned int &pDataSize, int64_t pPacketPts)
{
    AVPacket                    tPacket;
    int                         tResult;
    unsigned int                tMp3Hack_EntireBufferSize;
    if (!mRtpEncoderOpened)
        return false;

    if (pData == NULL)
        return false;

    if (pDataSize <= 0)
        return false;

    //####################################################################
    // for H261 use the internal RTP implementation
    //####################################################################
    if (mH261UseInternalEncoder)
        return RtpCreateH261(pData, pDataSize, pPacketPts);

    //####################################################################
    // for all non H261 codec use the ffmpeg RTP implementation
    //####################################################################
    if (mRtpFormatContext)
    {
        if (mRtpEncoderStream != NULL)
        {
            if(mRtpEncoderStream->codec)
            {
                if (mStreamCodecID != mRtpEncoderStream->codec->codec_id)
                    LOG(LOG_ERROR, "Unsupported codec change in input stream detected");
            }
        }
    }

    // check for supported codecs
    if(!IsPayloadSupported(mStreamCodecID))
    {
        LOG(LOG_ERROR, "Codec %s(%d) is unsupported", mRtpEncoderStream->codec->codec_name, mStreamCodecID);
        pDataSize = 0;
        return true;
    }

    // save the amount of bytes of the original codec packet
    // HINT: we use this to store the size of the original codec packet within MPA header's MBZ entry
    //       later we use this MBZ entry inside of MediaSourceNet to detect the fragment/packet boundaries
    //       without this hack we wouldn't be able to provide correct processing because of buggy rfc for MPA payload definition
    tMp3Hack_EntireBufferSize = pDataSize;

    av_init_packet(&tPacket);
    #ifdef RTP_DEBUG_PACKET_ENCODER_PTS
        LOG(LOG_VERBOSE, "Sending packet with PTS: %"PRId64", outgoing RTP-PTS: %.2f", pPacketPts, (float)pPacketPts * CalculateClockRateFactor());
    #endif

    // we only have one stream per media stream
    tPacket.stream_index = 0;
    tPacket.data = (uint8_t *)pData;
    tPacket.size = pDataSize;
    tPacket.pts = pPacketPts * CalculateClockRateFactor(); // clock rate adaption according to rfc (mpeg uses 90 kHz)
    tPacket.dts = tPacket.pts;

    #ifdef RTP_DEBUG_PACKET_ENCODER
        LOG(LOG_VERBOSE, "Encapsulating codec packet:");
        LOG(LOG_VERBOSE, "      ..pts: %"PRId64"", tPacket.pts);
        LOG(LOG_VERBOSE, "      ..dts: %"PRId64"", tPacket.dts);
        LOG(LOG_VERBOSE, "      ..size: %d", tPacket.size);
        LOG(LOG_VERBOSE, "      ..pos: %"PRId64"", tPacket.pos);
        LOG(LOG_VERBOSE, "      ..packet pts: ", pPacketPts);
    #endif

    //####################################################################
    // create memory stream and init ffmpeg internal structures
    //####################################################################
    #ifdef RTP_DEBUG_PACKET_ENCODER
        LOG(LOG_VERBOSE, "Encapsulate frame of codec %s and size: %u while maximum resulting RTP packet size is: %d", mRtpEncoderStream->codec->codec_name, pDataSize, mAVIOContext->max_packet_size);
    #endif

    // open RTP stream for av_Write_frame()
    OpenRtpPacketStream();

    //####################################################################
    // send encoded frame to the RTP muxer
    //####################################################################
    if ((tResult = av_write_frame(mRtpFormatContext, &tPacket)) < 0)
    {
        LOG(LOG_ERROR, "Couldn't write encoded frame of %u bytes at %p with PTS %"PRId64" into RTP buffer because \"%s\".", pDataSize, pData, pPacketPts, strerror(AVUNERROR(tResult)));

        return false;
    }

    // close memory stream and get all the resulting packets for sending
    char *tData = NULL;
    pDataSize = CloseRtpPacketStream(&tData);
    pData = (char*)tData;
    #ifdef RTP_DEBUG_PACKET_ENCODER
        if (pDataSize == 0)
            LOG(LOG_WARN, "Resulting RTP stream is empty");
        else
            LOG(LOG_VERBOSE, "Resulting RTP stream at %p with size of %d bytes", pData, pDataSize);
    #endif

    //####################################################################
    // patch the payload ID for standardized IDs as defined by rfc 3551
    // usually ffmpeg uses an ID from the dynamic ID range (96 in most cases)
    // which confuses my wireshark and makes debugging impossible ;)
    //####################################################################
    if ((pData != 0) && (pDataSize > 0))
    {
        char *tRtpPacket = pData + 4;
        uint32_t tRtpPacketSize = 0;
        uint32_t tRemainingRtpDataSize = pDataSize;
        MPAHeader* tMPAHeader = NULL;
        bool tFirstOutgoingPacket = true;

        // go through all created RTP packets
        do{
            tRtpPacketSize = ntohl(*(uint32_t*)(tRtpPacket - 4));

            #ifdef RTP_DEBUG_PACKET_ENCODER
                LOG(LOG_VERBOSE, "Found RTP packet at %p with size of %u bytes", tRtpPacket, tRtpPacketSize);
            #endif

            // if there is no packet data we should leave the send loop
            if (tRtpPacketSize == 0)
                break;

            RtpHeader* tRtpHeader = (RtpHeader*)tRtpPacket;

            // convert from network to host byte order
            for (int i = 0; i < 3; i++)
                tRtpHeader->Data[i] = ntohl(tRtpHeader->Data[i]);

            #ifdef RTP_DEBUG_PACKET_ENCODER
                LOG(LOG_VERBOSE, "RTP packet has payload type: %d", tRtpHeader->PayloadType);
            #endif


            //#################################################################################
            //### patch payload type and hack packet size for MP3 streaming
            //### do this only if we don't have a RTCP packet from the ffmpeg lib
            //### HINT: ffmpeg uses payload type 14 and several others like dyn. range 96-127
            //#################################################################################
            bool tRtcpPacket = false;
            if (!IS_RTCP_TYPE(tRtpHeader->PayloadType))
            {// usual RTP packet
                //#################################################################################
                //### derive ffmpeg'S internal RTP timestamp offset
                //#################################################################################
                if (tFirstOutgoingPacket)
                {
                    tFirstOutgoingPacket = false;
                    mLocalTimestampOffset = tRtpHeader->Timestamp - tPacket.pts;
                }

                #ifdef RTP_DEBUG_PACKET_ENCODER_TIMESTAMPS
                    LOG(LOG_VERBOSE, "Sending RTP packet with timestamp: %u, offset: %lu, RTP-PTS: %ld", tRtpHeader->Timestamp, mLocalTimestampOffset, tPacket.pts);
                #endif

                //#################################################################################
                //### patch last audio RTCP sender report if there was any
                //#################################################################################
//                if (mRtpEncoderStream->codec->codec_type == AVMEDIA_TYPE_AUDIO)
                {
                    if (mRtcpLastSenderReport != NULL)
                    {
                        RtcpPatchLiveSenderReport(mRtcpLastSenderReport, tRtpHeader->Timestamp);
                        mRtcpLastSenderReport = NULL;
                    }
                }

                // mark this packet as belonging to RTP
                tRtcpPacket = false;

                //#################################################################################
                //### patch payload ID
                //#################################################################################
                // patch ffmpeg payload values between 96 and 99
                // HINT: ffmpeg uses some strange intermediate packets with payload id 72
                //       -> don't know for what reason, but they should be kept as they are
                switch(mStreamCodecID)
                {
                            case CODEC_ID_PCM_MULAW:
                                            tRtpHeader->PayloadType = 0;
                                            break;
                            case CODEC_ID_PCM_ALAW:
                                            tRtpHeader->PayloadType = 8;
                                            break;
                            case CODEC_ID_ADPCM_G722:
                                            tRtpHeader->PayloadType = 9;
                                            break;
				//            case CODEC_ID_ADPCM_G726:
                            case CODEC_ID_PCM_S16BE:
                                            tRtpHeader->PayloadType = 10;
                                            break;
                            case CODEC_ID_MP3:
                                            // HACK: some modification of the standard MPA payload header: use MBZ to signalize the size of the original audio packet
                                            tMPAHeader = (MPAHeader*)(tRtpPacket + RTP_HEADER_SIZE);

                                            // convert from network to host byte order
                                            tMPAHeader->Data[0] = ntohl(tMPAHeader->Data[0]);

                                            #ifdef RTP_DEBUG_PACKET_ENCODER
                                                LOG(LOG_VERBOSE, "Set MBZ bytes to message size of %u", tMp3Hack_EntireBufferSize);
                                            #endif
                                            tMPAHeader->Mbz = (unsigned short int)tMp3Hack_EntireBufferSize;

                                            // convert from host to network byte order
                                            tMPAHeader->Data[0] = htonl(tMPAHeader->Data[0]);

                                            tRtpHeader->PayloadType = 14;
                                            break;
                            case CODEC_ID_H261:
                                            tRtpHeader->PayloadType = 31;
                                            break;
                            case CODEC_ID_MPEG1VIDEO:
                            case CODEC_ID_MPEG2VIDEO:
                                            tRtpHeader->PayloadType = 32;
                                            break;
                            case CODEC_ID_H263:
                                            tRtpHeader->PayloadType = 34;
                                            break;
                            case CODEC_ID_AAC:
                                            tRtpHeader->PayloadType = 100;
                                            break;
                            case CODEC_ID_AMR_NB:
                                            tRtpHeader->PayloadType = 101;
                                            break;
                            case CODEC_ID_H263P:
                                            tRtpHeader->PayloadType = 119;
                                            break;
                            case CODEC_ID_H264:
                                            tRtpHeader->PayloadType = 120;
                                            break;
                            case CODEC_ID_MPEG4:
                                            tRtpHeader->PayloadType = 121;
                                            break;
                            case CODEC_ID_THEORA:
                                            tRtpHeader->PayloadType = 122;
                                            break;
                            case CODEC_ID_VP8:
                                            tRtpHeader->PayloadType = 123;
                                            break;
				//            case CODEC_ID_MPEG2TS:
				//            case CODEC_ID_VORBIS:
                }

                //#################################################################################
                //### patch source identifier to the generated one
                //#################################################################################
                tRtpHeader->Ssrc = mLocalSourceIdentifier;
            }else
            {// RTCP packet
                RtcpHeader* tRtcpHeader = (RtcpHeader*)tRtpPacket;

                for (int i = 3; i < 7; i++)
                    tRtcpHeader->Data[i] = ntohl(tRtcpHeader->Data[i]);

                //#################################################################################
                //### RTCP sender report
                //#################################################################################
                if (tRtcpHeader->General.Type == RTCP_SENDER_REPORT)
                {// patch the values to the same time value as used in the A/V stream
                    tRtcpHeader->Feedback.Ssrc = mLocalSourceIdentifier;
                    mRtcpLastSenderReport = (char*)tRtcpHeader;
                }

                // mark this packet as belonging to RTCP
                tRtcpPacket = true;

                for (int i = 3; i < 7; i++)
                    tRtcpHeader->Data[i] = htonl(tRtcpHeader->Data[i]);
            }


            //#################################################################################
            //### convert from host to network byte order
            //#################################################################################
            for (int i = 0; i < 3; i++)
                tRtpHeader->Data[i] = htonl(tRtpHeader->Data[i]);

            //#################################################################################
            //### log RTCP packets if desired
            //#################################################################################
            #ifdef RTCP_DEBUG_PACKETS_ENCODER
                if (tRtcpPacket)
                {
                    RtcpHeader *tRtcpHeader = (RtcpHeader*)tRtpHeader;
                    LogRtcpHeader(tRtcpHeader);
                }
            #endif
            #ifdef RTP_DEBUG_PACKET_ENCODER
                if (!tRtcpPacket)
                {
                    // print some verbose outputs
                    LogRtpHeader(tRtpHeader);
                }
            #endif

            // go to the next RTP packet
            tRtpPacket = tRtpPacket + (tRtpPacketSize + 4);
            tRemainingRtpDataSize -= (tRtpPacketSize + 4);
        }while (tRemainingRtpDataSize >= RTP_HEADER_SIZE);

        // we have set the size of the entire original MP3 buffer in each RTP packet, we can reset this value for the next call to "RtpCreate"
        mMp3Hack_EntireBufferSize = 0;

        if (tRemainingRtpDataSize)
            LOG(LOG_ERROR, "Still %u bytes left when paload patches were already finished", tRemainingRtpDataSize);
    }else
    {
        #ifdef RTP_DEBUG_PACKET_ENCODER
            LOG(LOG_VERBOSE, "Resulting RTP stream is invalid (maybe ffmpeg buffers packets until max. packet size is reached)");
        #endif
        return false;
    }

    return true;
}

// HINT: ffmpeg lacks support for rtp encapsulation for h261  codec
bool RTP::RtpCreateH261(char *&pData, unsigned int &pDataSize, int64_t pPacketPts)
{
    if (!mRtpEncoderOpened)
        return false;

    #ifdef RTP_DEBUG_PACKET_ENCODER
        LOG(LOG_VERBOSE, "Encapsulate frame with format h261 of size: %u while maximum resulting RTP packet size is: %d", pDataSize, mH261PayloadSizeMax);
    #endif

    // calculate the amount of needed RTP packets to encapsulate the whole frame
    unsigned int tPacketCount = (pDataSize + (unsigned int)RTP_MAX_H261_PAYLOAD_SIZE - 1);
    tPacketCount /= (unsigned short int)(RTP_MAX_H261_PAYLOAD_SIZE);
    #ifdef RTP_DEBUG_PACKET_ENCODER
        LOG(LOG_VERBOSE, "Calculated h261 packets: %u", tPacketCount);
    #endif
    unsigned int tRtpStreamDataSize = 0;

    // get pointer to the current working address inside rtp packet stream
    char *tCurrentRtpStreamData = mRtpPacketStream;
    #ifdef RTP_DEBUG_PACKET_ENCODER
        LOG(LOG_VERBOSE, "Packet stream at %p", mRtpPacketStream);
    #endif
    for (unsigned int tPacketIndex = 0; tPacketIndex < tPacketCount; tPacketIndex++)
    {
        // #############################################################
        // create RTCP sender report
        // #############################################################
        #ifdef RTP_DEBUG_PACKET_ENCODER_PTS
            LOG(LOG_VERBOSE, "Sending packet with PTS: %"PRId64", outgoing RTP-PTS: %.2f", pPacketPts, (float)pPacketPts * CalculateClockRateFactor());
        #endif
        RtcpCreateH261SenderReport(tCurrentRtpStreamData, tRtpStreamDataSize, pPacketPts);

        // #############################################################
        // create RTP packet with H.261 payload inside
        // #############################################################
        // size of current frame chunk
        unsigned int tChunkSize = 0;

        // check size of remaining payload size
        if (pDataSize > (RTP_MAX_H261_PAYLOAD_SIZE))
        {// bigger than max. payload size
            tChunkSize = (RTP_MAX_H261_PAYLOAD_SIZE);
        }else
        {// remaining payload fits into on additional packet
            tChunkSize = pDataSize;
        }

        tRtpStreamDataSize += 4 + RTP_HEADER_SIZE + H261_HEADER_SIZE + tChunkSize;
        if (tRtpStreamDataSize > MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE)
        {
            LOG(LOG_ERROR, "RTP stream buffer is too small, stopping RTP encapsulation here");
            return false;
        }

        pDataSize -= tChunkSize;
        // set the current rtp packet's size within the resulting packet buffer
        // HINT: convert from host to network byte order to pretend ffmpeg behavior
        unsigned int *tRtpPacketSize = (unsigned int*)tCurrentRtpStreamData;
        *tRtpPacketSize = htonl((uint32_t) RTP_HEADER_SIZE + H261_HEADER_SIZE + tChunkSize);

        // go to the start of the rtp packet
        tCurrentRtpStreamData += 4;
        // #############################################################
        // HEADER: create RTP header
        // #############################################################
        // get pointer to RTP header buffer
        RtpHeader* tRtpHeader  = (RtpHeader*)tCurrentRtpStreamData;

        tRtpHeader->Version = 2; // current RTP-rfc 3550 defines version 2
        tRtpHeader->Padding = 0; // no padding octets
        tRtpHeader->Extension = 0; // no extension header used
        tRtpHeader->CsrcCount = 0; // no usage of CSRCs
        tRtpHeader->Marked = (tPacketIndex == tPacketCount - 1)? 1 : 0; // 1 = last fragment, 0 = intermediate fragment
        tRtpHeader->PayloadType = 31; // 31 = h261
        tRtpHeader->SequenceNumber = ++mH261LocalSequenceNumber; // monotonous growing
        tRtpHeader->Timestamp = pPacketPts * CalculateClockRateFactor() /* 90 kHz clock rate */;
        tRtpHeader->Ssrc = mLocalSourceIdentifier; // use the initially computed unique ID

        // convert from host to network byte order
        for (int i = 0; i < 3; i++)
            tRtpHeader->Data[i] = htonl(tRtpHeader->Data[i]);

        // go to the start of the h261 header
        tCurrentRtpStreamData += RTP_HEADER_SIZE;

        // #############################################################
        // HEADER: create H261 specific RTP payload header
        // #############################################################
        // get pointer to H261 header buffer
        H261Header* tH261Header  = (H261Header*)tCurrentRtpStreamData;

        tH261Header->Sbit = 0;
        tH261Header->Ebit = 0;
        tH261Header->I = false;
        tH261Header->V = true;
        tH261Header->Gobn = 0;
        tH261Header->Mbap = 0;
        tH261Header->Quant = 0;
        tH261Header->Hmvd = 0; // no motion vectors used
        tH261Header->Vmvd = 0; // no motion vectors used

        // convert from host to network byte order
        tH261Header->Data[0] = htonl(tH261Header->Data[0]);

        // go to the start of the h261 header
        tCurrentRtpStreamData += H261_HEADER_SIZE;

        // #############################################################
        // PAYLOAD: copy PAYLOAD to packet buffer
        // #############################################################
        // copy data from original buffer
        memcpy(tCurrentRtpStreamData, pData, tChunkSize);
        // jump to the next chunk of the original frame buffer
        pData += tChunkSize;

        // go to the next packet header
        tCurrentRtpStreamData += tChunkSize;

        //increase packet counter
        mH261SentPackets++;
        mH261SentOctets+= H261_HEADER_SIZE + tChunkSize;
    }
    pData = mRtpPacketStream;
    pDataSize = tRtpStreamDataSize;

    return true;
}

unsigned int RTP::GetLostPacketsFromRTP()
{
    return mLostPackets;
}

float RTP::GetRelativeLostPacketsFromRTP()
{
    return mRelativeLostPackets;
}

void RTP::AnnounceLostPackets(uint64_t pCount)
{
    LOG(LOG_VERBOSE, "Got %"PRIu64" lost packets", pCount);
    mLostPackets += pCount;
    if (mPacketStatistic != NULL)
        mPacketStatistic->SetLostPacketCount(mLostPackets);
}

void RTP::LogRtpHeader(RtpHeader *pRtpHeader)
{
    //HINT: assumes host byte order!

    for (int i = 0; i < 3; i++)
        pRtpHeader->Data[i] = ntohl(pRtpHeader->Data[i]);

    LOGEX(RTP, LOG_VERBOSE, "################## RTP header ########################");
    LOGEX(RTP, LOG_VERBOSE, "Version: %d", pRtpHeader->Version);
    if (pRtpHeader->Padding)
        LOGEX(RTP, LOG_VERBOSE, "Padding: true");
    else
        LOGEX(RTP, LOG_VERBOSE, "Padding: false");
    if (pRtpHeader->Extension)
        LOGEX(RTP, LOG_VERBOSE, "Extension: true");
    else
        LOGEX(RTP, LOG_VERBOSE, "Extension: false");
    LOGEX(RTP, LOG_VERBOSE, "SSRC            : %u", pRtpHeader->Ssrc);
    LOGEX(RTP, LOG_VERBOSE, "CSRC count      : %u", pRtpHeader->CsrcCount);
    // HINT: after converting from host to network byte order the original value within the RTP header can't be read anymore
    if (pRtpHeader->Marked)
        LOGEX(RTP, LOG_VERBOSE, "Marked: yes");
    else
        LOGEX(RTP, LOG_VERBOSE, "Marked: no");
    LOGEX(RTP, LOG_VERBOSE, "Payload type    : %s(%d)", PayloadIdToCodec(pRtpHeader->PayloadType).c_str(), pRtpHeader->PayloadType);
    LOGEX(RTP, LOG_VERBOSE, "SequenceNumber  : %u", pRtpHeader->SequenceNumber);
    LOGEX(RTP, LOG_VERBOSE, "Timestamp (abs.): %10u", pRtpHeader->Timestamp);

    for (int i = 0; i < 3; i++)
        pRtpHeader->Data[i] = htonl(pRtpHeader->Data[i]);
}

uint64_t RTP::GetCurrentPtsFromRTP()
{
    int64_t tResult = 0;

    // clock rate adaption
    tResult = mRemoteTimestamp / CalculateClockRateFactor();

    return tResult;
}

void RTP::GetSynchronizationReferenceFromRTP(uint64_t &pReferenceNtpTime, uint64_t &pReferencePts)
{
    mSynchDataMutex.lock();

    pReferenceNtpTime = mRtcpLastRemoteNtpTime;

    // clock rate adaption
    pReferencePts = mRtcpLastRemoteTimestamp / CalculateClockRateFactor();

    mSynchDataMutex.unlock();
}

unsigned int RTP::GetSourceIdentifierFromRTP()
{
	return mRemoteSourceIdentifier;
}

bool RTP::HasSourceChangedFromRTP()
{
	return mRtpRemoteSourceChanged;
}

float RTP::CalculateClockRateFactor()
{
    float tResult = 1;

    //HINT: mpeg uses its specific clock rate of 90 kHz, the normal clock rate is 1 kHz (time base is 1 ms !)
    //      e.g.: 29.97 video fps means 1/29.97 = 33.367 ms time difference between frames, 33.367 ms * 90 = 3003 timestamp difference (PTS) between frames for play out
    //      e.g.: 23.97 video fps -> 1/23.97 = 41.719 ms, 41.719 ms * 90 = 3755 PTS difference between frames
    //      e.g.  44100 samples per second, 44100/1024 audio frames per second = 43,07 fps -> 1/43,07 = 23,22 ms

    switch(mStreamCodecID)
    {
        case CODEC_ID_PCM_MULAW:
        case CODEC_ID_PCM_ALAW:
        case CODEC_ID_PCM_S16BE:
        case CODEC_ID_MP3:
        case CODEC_ID_ADPCM_G722:
//            case CODEC_ID_ADPCM_G726:
        case CODEC_ID_THEORA:
            tResult = 1;
            break;
        case CODEC_ID_H261:
        case CODEC_ID_H263:
        case CODEC_ID_H263P:
        case CODEC_ID_H264:
        case CODEC_ID_MPEG1VIDEO:
        case CODEC_ID_MPEG2VIDEO:
        case CODEC_ID_MPEG4: //TODO: mpeg 4 is buggy?
            tResult = 90;
            break;
        case CODEC_ID_VP8:
            tResult = 1; //TODO
            break;
//            case CODEC_ID_MPEG2TS:
//            case CODEC_ID_VORBIS:
        default:
            break;
    }

    return tResult;
}

bool RTP::ReceivedCorrectPayload(unsigned int pType)
{
	bool tResult = false;

    switch(mStreamCodecID)
    {
                case CODEC_ID_PCM_MULAW:
                                if (pType == 0)
                                	tResult = true;
                                break;
                case CODEC_ID_PCM_ALAW:
								if (pType == 8)
									tResult = true;
                                break;
                case CODEC_ID_ADPCM_G722:
								if (pType == 9)
									tResult = true;
                                break;
	//            case CODEC_ID_ADPCM_G726:
                case CODEC_ID_PCM_S16BE:
								if (pType == 10)
									tResult = true;
                                break;
                case CODEC_ID_MP3:
								if (pType == 14)
									tResult = true;
                                break;
                case CODEC_ID_H261:
								if (pType == 31)
									tResult = true;
                                break;
                case CODEC_ID_MPEG1VIDEO:
                case CODEC_ID_MPEG2VIDEO:
								if (pType == 32)
									tResult = true;
                                break;
                case CODEC_ID_H263:
								if ((pType == 34) || (pType >= 96))
									tResult = true;
                                break;
                case CODEC_ID_AAC:
                case CODEC_ID_AMR_NB:
                case CODEC_ID_H263P:
                case CODEC_ID_H264:
                case CODEC_ID_MPEG4:
                case CODEC_ID_THEORA:
                case CODEC_ID_VP8:
	//            case CODEC_ID_MPEG2TS:
	//            case CODEC_ID_VORBIS:
								if (pType >= 96)
									tResult = true;
								break;
                default:
								break;
    }

    return tResult;
}

// assumption: we are getting one single RTP encapsulated packet, not auto detection of following additional packets included
bool RTP::RtpParse(char *&pData, int &pDataSize, bool &pIsLastFragment, enum RtcpType &pRtcpType, enum CodecID pCodecId, bool pLoggingOnly)
{
    pIsLastFragment = false;

    // is there some data?
    if (pDataSize == 0)
        return false;

    bool tOldH263PayloadDetected = false;
    char *tRtpPacketStart = pData;

    if ((mStreamCodecID != CODEC_ID_NONE) && (mStreamCodecID != pCodecId))
        LOG(LOG_WARN, "Codec change from %d(%s) to %d(%s) in inout stream detected", mStreamCodecID, avcodec_get_name(mStreamCodecID), pCodecId, avcodec_get_name(pCodecId));

    mStreamCodecID = pCodecId;

    // check for supported codecs
    switch(mStreamCodecID)
    {
            //supported audio codecs
            case CODEC_ID_PCM_MULAW:
            case CODEC_ID_PCM_ALAW:
            case CODEC_ID_PCM_S16BE:
            case CODEC_ID_MP3:
            case CODEC_ID_ADPCM_G722:
//            case CODEC_ID_ADPCM_G726:
            //supported video codecs
            case CODEC_ID_H261:
            case CODEC_ID_H263:
            case CODEC_ID_H263P:
            case CODEC_ID_H264:
            case CODEC_ID_MPEG1VIDEO:
            case CODEC_ID_MPEG2VIDEO:
            case CODEC_ID_MPEG4:
            case CODEC_ID_THEORA:
            case CODEC_ID_VP8:
//            case CODEC_ID_MPEG2TS:
//            case CODEC_ID_VORBIS:
                    break;
            default:
                    LOG(LOG_ERROR, "Codec %d is unsupported by internal RTP parser", mStreamCodecID);
                    pDataSize = 0;
                    pIsLastFragment = true;
                    return false;
    }

    // #############################################################
    // count packets in order to conclude packet loss based on RTCP
    // #############################################################
    if (!pLoggingOnly)
        mReceivedPackets++;

    // #############################################################
    // HEADER: parse rtp header
    // #############################################################
    RtpHeader* tRtpHeader = (RtpHeader*)pData;

    // convert from network to host byte order
    for (int i = 0; i < 3; i++)
        tRtpHeader->Data[i] = ntohl(tRtpHeader->Data[i]);

    unsigned int tCsrcCount = tRtpHeader->CsrcCount;
    if (tCsrcCount > 0)
        LOG(LOG_ERROR, "Found unsupported usage of multimedia stream mixing at remote side");

    if (tCsrcCount > 4)
    {
        LOG(LOG_ERROR, "Found invalid CSRC value %u in RTP header", tCsrcCount);

        pIsLastFragment = false;

        // convert from host to network byte order again
        for (int i = 0; i < 3; i++)
            tRtpHeader->Data[i] = htonl(tRtpHeader->Data[i]);

		#ifdef RTP_DEBUG_PACKET_DECODER

			// print some verbose outputs
			LogRtpHeader(tRtpHeader);

		#endif

        return false;
    }

    // HINT: header size = standard header size + amount of CSRCs * size of one CSRC
    // go to the start of the codec header
    pData += RTP_HEADER_SIZE;
    for (unsigned int j = 1; j < tCsrcCount; j++)
        pData += sizeof(unsigned int);

    // do we have old h263 style rtp packets?
    if (tRtpHeader->PayloadType == 34)
        tOldH263PayloadDetected = true;

    // #############################################################
    // HEADER: rtcp => parse and return immediately
    // RTCP feedback packet within data stream: RFC4585
    // transmitted every 5 seconds
    // #############################################################
    if ((tRtpHeader->PayloadType >= 72) && (tRtpHeader->PayloadType <= 76))
    {// RTCP intermediate packet for streaming feedback received
    	if (!pLoggingOnly)
    		mRTCPPacketCounter++;

    	// RTCP in-stream feedback starts at the beginning of RTP header
        RtcpHeader* tRtcpHeader = (RtcpHeader*)tRtpPacketStart;

        pIsLastFragment = false;
        pRtcpType = (enum RtcpType)tRtcpHeader->General.Type;
        pData = (char*)tRtcpHeader;
        pDataSize = (tRtcpHeader->Feedback.Length + 1) * 4;

        // convert from host to network byte order again
        for (int i = 0; i < 3; i++)
            tRtpHeader->Data[i] = htonl(tRtpHeader->Data[i]);

        #ifdef RTCP_DEBUG_PACKETS_DECODER
            LogRtcpHeader(tRtcpHeader);
        #endif

        // inform that is not a fragment which includes data for an audio/video decoder, this RTCP packet belongs to the RTP abstraction level
        return false;
    }else
    {// usual RTP packet
    	pRtcpType = RTCP_NOT_FOUND;

    	if (!pLoggingOnly)
    		mRTPPacketCounter++;

    	#ifdef RTP_DEBUG_PACKET_DECODER
            for (int i = 0; i < 3; i++)
                tRtpHeader->Data[i] = htonl(tRtpHeader->Data[i]);

            // print some verbose outputs
            LogRtpHeader(tRtpHeader);

            for (int i = 0; i < 3; i++)
                tRtpHeader->Data[i] = ntohl(tRtpHeader->Data[i]);
        #endif
    }

    // #############################################################
    // HEADER: rtp => parse and update internal state information
    // #############################################################
    if (!pLoggingOnly)
    {
        // ###################################################
        // PAYLOAD ID: use RTP header to set the payload type
        // ###################################################
        if (!IS_RTCP_TYPE(tRtpHeader->PayloadType))
        {// we should have received a valid A/V RTP packet
			if (mPayloadId != tRtpHeader->PayloadType)
			{// payload changed
                if (mPayloadId != RTP_PAYLOAD_TYPE_NONE)
                {// we already know a payload type but the current packet does not belong to this type

                    //LOG(LOG_WARN, "Payload change from codec %d(%s) to %u(%s), reset score: %d", mStreamCodecID, avcodec_get_name(mStreamCodecID), tRtpHeader->PayloadType, PayloadIdToCodec(tRtpHeader->PayloadType).c_str(), mRemoteSourceChangedResetScore);

                    // do we receive the same payload as last time?
                    if ((mRemoteSourceChangedLastPayload != 0) && (mRemoteSourceChangedLastPayload == tRtpHeader->PayloadType))
                    {// yes, same payload received -> check scoring
                        mRemoteSourceChangedResetScore++;
                        //LOG(LOG_WARN, "Reset score incread to: %d", mRemoteSourceChangedResetScore);

                        if (mRemoteSourceChangedResetScore >= RTP_MAX_REMOTE_SOURCE_CHANGED_RESET_SCORE)
                        {// we should mark the remote source as "changed"
                            LOG(LOG_WARN, "We received %d consecutive packets of payload type %u(%s), we assume a source change at remote side and trigger reset", mRemoteSourceChangedResetScore, tRtpHeader->PayloadType, PayloadIdToCodec(tRtpHeader->PayloadType).c_str());
                            mRtpRemoteSourceChanged = true;

                            // force a reset of the start timestamp and trigger a re-initialization
                            mRemoteStartTimestamp = 0;
                            mRemoteStartSequenceNumber = 0;
                            mRemoteSourceChangedResetScore = 0;

                            mPayloadId = mRemoteSourceChangedLastPayload;
                        }
                    }else
                    {
                        mRemoteSourceChangedResetScore = 0;
                        mRemoteSourceChangedLastPayload = tRtpHeader->PayloadType;
                    }

                    pIsLastFragment = false;

                    // inform that this is not a usable packet
                    return false;
                }

                // store the payload ID to be able to detect repeating changes
                mPayloadId = tRtpHeader->PayloadType;
			}
        }

        // #######################################################################################
        // SOURCE IDENTIFIER: update the remote source identifier and re-init the start timestamp
        // #######################################################################################
        // store the assigned SSRC identifier
        if (mRemoteSourceIdentifier != tRtpHeader->Ssrc)
        {
            // did the source ID from remote side changed more than one time?
            if (mRemoteSourceIdentifier != 0)
            {
                LOG(LOG_WARN, "Alternating source at remote side detected, will reset start timestamp");
                mRtpRemoteSourceChanged = true;

                // force a reset of the start timestamp and trigger a re-initialization
                mRemoteStartTimestamp = 0;
                mRemoteStartSequenceNumber = 0;
            }

            // store the source ID to be able to detect repeating changes
            mRemoteSourceIdentifier = tRtpHeader->Ssrc;
        }

        // #############################################################
        // START SEQUENCE NUMBER: update the remote start sequence number
        // #############################################################
        if (mRemoteStartSequenceNumber == 0)
        {
        	LOG(LOG_WARN, "Setting remote start sequence number to: %hu", tRtpHeader->SequenceNumber);

            // we have to reset the timestamp calculation
        	mRemoteStartSequenceNumber = tRtpHeader->SequenceNumber;
        }

        // ##########################################################################
        // SEQUENCE NUMBER: update the remote sequence number and react on overflows
        // ##########################################################################
        // do we have a sequence number overflow?
        if ((mLastSequenceNumberFromRTPHeader > tRtpHeader->SequenceNumber) && (mLastSequenceNumberFromRTPHeader - tRtpHeader->SequenceNumber > UINT16_MAX / 2 /* avoid false-positive overflow detection in case of out-of-order packets */))
        {// we have detected an value overflow
        	// shift the SequenceNumber value
        	mRemoteSequenceNumberOverflowShift += UINT16_MAX + 1;

            // update the remote SequenceNumber depending on the overflow shift value
            mRemoteSequenceNumber = mRemoteSequenceNumberOverflowShift + (uint64_t)tRtpHeader->SequenceNumber - mRemoteStartSequenceNumber;

			#ifdef RTP_DEBUG_PACKET_DECODER_SEQUENCE_NUMBERS
            	LOG(LOG_WARN, "Overflow detected and compensated, new remote sequence number: abs=%hu(max: %hu), start=%hu, normalized=%"PRIu64"", tRtpHeader->SequenceNumber, (unsigned short int)UINT16_MAX, mRemoteStartSequenceNumber, mRemoteSequenceNumber);
			#endif

			// increase the "overflow" counter
        	mRemoteSequenceNumberConsecutiveOverflows++;

        	// did we reach the limit of allowed consecutive SequenceNumber overflows?
        	if (mRemoteSequenceNumberConsecutiveOverflows > RTP_MAX_CONSECUTIVE_SEQUENCE_NUMBER_OVERFLOWS)
        	{// yes -> mark source as changed
        		LOG(LOG_WARN, "Detected %d(max. allowed: %d) consecutive sequence number overflows, mark remote source as changed", mRemoteSequenceNumberConsecutiveOverflows, RTP_MAX_CONSECUTIVE_SEQUENCE_NUMBER_OVERFLOWS);
        		mRtpRemoteSourceChanged = true;
        	}
        }else
        {// we don't have an value overflow
            // update the remote SequenceNumber depending on the overflow shift value
            mRemoteSequenceNumber = mRemoteSequenceNumberOverflowShift + (uint64_t)tRtpHeader->SequenceNumber - (uint64_t)mRemoteStartSequenceNumber;

            // reset the "overflow" counter
            mRemoteSequenceNumberConsecutiveOverflows = 0;
			#ifdef RTP_DEBUG_PACKET_DECODER_SEQUENCE_NUMBERS
            	LOG(LOG_VERBOSE, "New remote SequenceNumber: abs=%hu(max: %hu), start=%hu, normalized=%"PRIu64"", tRtpHeader->SequenceNumber, (unsigned short int)UINT16_MAX, mRemoteStartSequenceNumber, mRemoteSequenceNumber);
			#endif
        }
        mLastSequenceNumberFromRTPHeader = tRtpHeader->SequenceNumber;

        // ###########################################################
        // PACKET ORDERING: check if there was a packet order problem
        // ###########################################################
        bool tPacketOutOfOrder = false;
        if ((mRemoteSequenceNumber > 0 /* ignore stream resets */) && (mRemoteSequenceNumberLastPacket > 0) && (mRemoteSequenceNumber < mRemoteSequenceNumberLastPacket))
        {
            LOG(LOG_ERROR, "Packets in wrong order received (%"PRIu64"->%"PRIu64")", mRemoteSequenceNumberLastPacket, mRemoteSequenceNumber);
            tPacketOutOfOrder = true;
        }

        // ############################################
        // PACKET LOSS: check if there was packet loss
        // ############################################
        if ((mRemoteSequenceNumber > 0) && (mRemoteSequenceNumberLastPacket > 0) && (mRemoteSequenceNumber > mRemoteSequenceNumberLastPacket + 1))
        {
            uint64_t tLostPackets = mRemoteSequenceNumber - mRemoteSequenceNumberLastPacket - 1;
            AnnounceLostPackets(tLostPackets);
            LOG(LOG_ERROR, "Packet loss detected (sequ. nr.: %"PRIu64"->%"PRIu64"), lost %"PRIu64" packets, overall packet loss is now %"PRIu64, mRemoteSequenceNumberLastPacket, mRemoteSequenceNumber, tLostPackets, mLostPackets);
        }

        // ############################################################
        // FRAGMENTATION: use RTP header to set the fragmentation flag
        // ############################################################
        // use standard RTP definition to detect fragments, some A/V codecs have extended fragmentation detection mechanism (will be executed in the end of this procedure)
        mIntermediateFragment = !tRtpHeader->Marked;

        // #############################################################
        // START TIMESTAMP: update the remote start timestamp
        // #############################################################
        if (mRemoteStartTimestamp == 0)
        {
            // we have to reset the timestamp calculation
            mRemoteStartTimestamp = tRtpHeader->Timestamp;
        }

        // #############################################################
        // TIMESTAMP: update the remote timestamp and react on overflows
        // #############################################################
        // do we have a timestamp overflow?
        if ((mLastTimestampFromRTPHeader > tRtpHeader->Timestamp) && (mLastTimestampFromRTPHeader - tRtpHeader->Timestamp > UINT32_MAX / 2 /* avoid false-positive overflow detection in case of out-of-order timestamps */) &&  (!tPacketOutOfOrder))
        {// we have detected an value overflow
        	// shift the timestamp value
        	mRemoteTimestampOverflowShift += UINT32_MAX + 1;

            // update the remote timestamp depending on the overflow shift value
            mRemoteTimestamp = mRemoteTimestampOverflowShift + (uint64_t)tRtpHeader->Timestamp - mRemoteStartTimestamp;

			#ifdef RTP_DEBUG_PACKET_DECODER_TIMESTAMPS
            	LOG(LOG_WARN, "Overflow detected and compensated, new remote timestamp: last=%u, abs=%u(max: %u), start=%"PRIu64", normalized=%"PRIu64"", mLastTimestampFromRTPHeader, tRtpHeader->Timestamp, UINT32_MAX, mRemoteStartTimestamp, mRemoteTimestamp);
			#endif

			// increase the "overflow" counter
        	mRemoteTimestampConsecutiveOverflows++;

        	// did we reach the limit of allowed consecutive timestamp overflows?
        	if (mRemoteTimestampConsecutiveOverflows > RTP_MAX_CONSECUTIVE_TIMESTAMP_OVERFLOWS)
        	{// yes -> mark source as changed
        		LOG(LOG_WARN, "Detected %d(max. allowed: %d) consecutive timestamp overflows, mark remote source as changed", mRemoteTimestampConsecutiveOverflows, RTP_MAX_CONSECUTIVE_TIMESTAMP_OVERFLOWS);
        		mRtpRemoteSourceChanged = true;
        	}
        }else
        {// we don't have an value overflow
            // update the remote timestamp depending on the overflow shift value
            mRemoteTimestamp = mRemoteTimestampOverflowShift + (uint64_t)tRtpHeader->Timestamp - mRemoteStartTimestamp;

            // reset the "overflow" counter
            mRemoteTimestampConsecutiveOverflows = 0;

			#ifdef RTP_DEBUG_PACKET_DECODER_TIMESTAMPS
            	LOG(LOG_VERBOSE, "New remote timestamp: abs=%u(max: %u), start=%u, normalized=%"PRIu64", pts=%"PRIu64, tRtpHeader->Timestamp, UINT32_MAX, mRemoteStartTimestamp, mRemoteTimestamp, GetCurrentPtsFromRTP());
			#endif
            #ifdef RTP_DEBUG_PACKET_DECODER_TIMESTAMPS_CONTINUITY
                LOG(LOG_VERBOSE, "New remote timestamp: normalized=%"PRIu64", diff. to last=%"PRIu64, mRemoteTimestamp, mRemoteTimestamp - mRemoteTimestampLastPacket);
            #endif

        }
        mLastTimestampFromRTPHeader = tRtpHeader->Timestamp;

        #ifdef RTP_DEBUG_PACKET_DECODER
            LOGEX(RTP, LOG_VERBOSE, "Timestamp (rel.): %10u", mRemoteTimestamp);
        #endif
    }

    // #############################################################
    // HEADER: codec headers => parse
    // #############################################################
    // get pointer to codec header buffer
    AMRNBHeader* tAMRNBHeader = (AMRNBHeader*)pData;
    MPAHeader* tMPAHeader = (MPAHeader*)pData;
    MPVHeader* tMPVHeader = (MPVHeader*)pData;
    G711Header* tG711Header = (G711Header*)pData;
    H261Header* tH261Header = (H261Header*)pData;
    H263Header* tH263Header = (H263Header*)pData;
    H263PHeader* tH263PHeader = (H263PHeader*)pData;
    bool tH263PPictureStart = false;
    H264Header* tH264Header = (H264Header*)pData;
    THEORAHeader* tTHEORAHeader = (THEORAHeader*)pData;
    VP8Header* tVP8Header = (VP8Header*)pData;
    unsigned char tH264HeaderType = 0;
    bool tH264HeaderFragmentStart = false;
    char tH264HeaderReconstructed = 0;

    switch(mStreamCodecID)
    {
            // audio
            case CODEC_ID_PCM_ALAW:
                            #ifdef RTP_DEBUG_PACKET_DECODER
                                LOG(LOG_VERBOSE, "#################### PCMA header #######################");
                                LOG(LOG_VERBOSE, "No additional information");
                            #endif
                            // no fragmentation because our encoder sends raw data
                            mIntermediateFragment = false;
                            break;
            case CODEC_ID_PCM_MULAW:
                            #ifdef RTP_DEBUG_PACKET_DECODER
                                LOG(LOG_VERBOSE, "#################### PCMU header #######################");
                                LOG(LOG_VERBOSE, "No additional information");
                            #endif
                            // no fragmentation because our encoder sends raw data
                            mIntermediateFragment = false;
                            break;
            case CODEC_ID_PCM_S16BE:
                            #ifdef RTP_DEBUG_PACKET_DECODER
                                LOG(LOG_VERBOSE, "#################### PCM_S16BE header #######################");
                                LOG(LOG_VERBOSE, "No additional information");
                            #endif
                            // no fragmentation because our encoder sends raw data
                            mIntermediateFragment = false;
                            break;
            case CODEC_ID_ADPCM_G722:
							#ifdef RTP_DEBUG_PACKET_DECODER
								LOG(LOG_VERBOSE, "#################### G.722 header #######################");
								LOG(LOG_VERBOSE, "No additional information");
							#endif
							// no fragmentation because our encoder sends raw data
							mIntermediateFragment = false;
            				break;
//            case CODEC_ID_ADPCM_G726:
            case CODEC_ID_MP3:
                            // convert from network to host byte order
                            tMPAHeader->Data[0] = ntohl(tMPAHeader->Data[0]);

                            pData += 4;

                            #ifdef RTP_DEBUG_PACKET_DECODER

                                LOG(LOG_VERBOSE, "#################### MPA header #######################");
                                LOG(LOG_VERBOSE, "Mbz bytes: %hu", tMPAHeader->Mbz);
                                LOG(LOG_VERBOSE, "Fragmentation offset: %hu", tMPAHeader->Offset);
                                if (tMPAHeader->Mbz > 0)
                                {
                                    LOG(LOG_VERBOSE, "HACK: calculated fragment size: %u", (pDataSize - (pData - tRtpPacketStart)));
                                    LOG(LOG_VERBOSE, "HACK: original frame size: %hu", tMPAHeader->Mbz);
                                }

                            #endif

                            // HACK: auto detect hack which marks the last fragment for us
                            //       we do this by storing the size of the original audio packet within the MBZ value
                            if (tMPAHeader->Mbz > 0)
                            {
                                // if fragment ends at packet size or behind (to make sure we are not running into inconsistency) we should mark as complete packet
                                if ((int)tMPAHeader->Offset + ((int)pDataSize - (pData - tRtpPacketStart)) >= (int)tMPAHeader->Mbz -1 /* a difference of 1 is sometimes caused by the MP3 encoder */)
                                    mIntermediateFragment = false;
                                else
                                    mIntermediateFragment = true;
                            }

                            // convert from host to network byte order
                            tMPAHeader->Data[0] = htonl(tMPAHeader->Data[0]);


// MP3 ADU:
//                                #ifdef RTP_DEBUG_PACKET_DECODER
//
//                                    LOG(LOG_VERBOSE, "################# MP3 ADU header #####################");
//                                    if (tMP3Header->C)
//                                        LOG(LOG_VERBOSE, "Continuation flag: yes");
//                                    else
//                                        LOG(LOG_VERBOSE, "Continuation flag: no");
//                                    if (tMP3Header->T)
//                                    {
//                                        LOG(LOG_VERBOSE, "ADU descriptor size: 2 bytes");
//                                        LOG(LOG_VERBOSE, "ADU size: %d", tMP3Header->Size + 64 * tMP3Header->SizeExt);
//                                    }else
//                                    {
//                                        LOG(LOG_VERBOSE, "ADU descriptor size: 1 byte");
//                                        LOG(LOG_VERBOSE, "ADU size: %d", tMP3Header->Size);
//                                    }
//
//                                #endif
//
//                                if (tMP3Header->T)
//                                    pData += 2; // 2 byte ADU descriptor
//                                else
//                                    pData += 1; // 1 byte ADU descriptor

                            break;
            // video
            case CODEC_ID_H261:
                            #ifdef RTP_DEBUG_PACKET_DECODER
                                // convert from network to host byte order
                                tH261Header->Data[0] = ntohl(tH261Header->Data[0]);

                                LOG(LOG_VERBOSE, "################## H261 header ########################");
                                LOG(LOG_VERBOSE, "Start bit pos.: %d", tH261Header->Sbit);
                                LOG(LOG_VERBOSE, "End bit pos.: %d", tH261Header->Ebit);
                                if (tH261Header->I)
                                    LOG(LOG_VERBOSE, "Intra-frame: yes");
                                else
                                    LOG(LOG_VERBOSE, "Intra-frame: no");
                                if (tH261Header->V)
                                    LOG(LOG_VERBOSE, "Motion vector flag: set");
                                else
                                    LOG(LOG_VERBOSE, "Motion vector flag: cleared");
                                LOG(LOG_VERBOSE, "GOB number: %d", tH261Header->Gobn);
                                LOG(LOG_VERBOSE, "MB address predictor: %d", tH261Header->Mbap);
                                LOG(LOG_VERBOSE, "Quantizer: %d", tH261Header->Quant);
                                LOG(LOG_VERBOSE, "Horiz. vector data: %d", tH261Header->Hmvd);
                                LOG(LOG_VERBOSE, "Vert. vector data: %d", tH261Header->Vmvd);

                                // convert from host to network byte order
                                tH261Header->Data[0] = htonl(tH261Header->Data[0]);
                            #endif

                            // go to the start of the h261 payload
                            pData += H261_HEADER_SIZE;
                            break;
            case CODEC_ID_H263:
            case CODEC_ID_H263P:
                            // HINT: do we have RTP packets with payload id 34?
                            //       => yes: parse rtp packet according to RFC2190
                            //       => no: parse rtp packet according to RFC4629
                            if (tOldH263PayloadDetected)
                            {// H263 rtp scheme
                                // convert from network to host byte order
                                tH263Header->Data[0] = ntohl(tH263Header->Data[0]);

                                #ifdef RTP_DEBUG_PACKET_DECODER
                                    LOG(LOG_VERBOSE, "################## H263 header ######################");
                                    if (!tH263Header->F)
                                        LOG(LOG_VERBOSE, "Header mode: A");
                                    else
                                        if (!tH263Header->P)
                                            LOG(LOG_VERBOSE, "Header mode: B");
                                        else
                                            LOG(LOG_VERBOSE, "Header mode: C");
                                    LOG(LOG_VERBOSE, "F bit: %d", tH263Header->F);
                                    LOG(LOG_VERBOSE, "P bit: %d", tH263Header->P);
                                    LOG(LOG_VERBOSE, "Start bit pos.: %d", tH263Header->Sbit);
                                    LOG(LOG_VERBOSE, "End bit pos.: %d", tH263Header->Ebit);
                                    switch(tH263Header->Src)
                                    {
                                                case 1:
                                                    LOG(LOG_VERBOSE, "Source format: SQCIF (128*96)");
                                                    break;
                                                case 2:
                                                    LOG(LOG_VERBOSE, "Source format: QCIF (176*144)");
                                                    break;
                                                case 3:
                                                    LOG(LOG_VERBOSE, "Source format: CIF = (352*288)");
                                                    break;
                                                case 4:
                                                    LOG(LOG_VERBOSE, "Source format: 4CIF = (704*576)");
                                                    break;
                                                case 5:
                                                    LOG(LOG_VERBOSE, "Source format: 16CIF = (1408*1152)");
                                                    break;
                                                default:
                                                    LOG(LOG_VERBOSE, "Source format: %d", tH263Header->Src);
                                    }
                                #endif
                                if (!tH263Header->F)
                                    pData += 4; // mode A
                                else
                                    if (!tH263Header->P)
                                        pData += 8; // mode B
                                    else
                                        pData += 12; // mode C

                                // convert from host to network byte order again
                                tH263Header->Data[0] = htonl(tH263Header->Data[0]);
                            }else
                            {// H263+ rtp scheme
								// convert from network to host byte order
								tH263PHeader->Data[0] = ntohl(tH263PHeader->Data[0]);

								#ifdef RTP_DEBUG_PACKET_DECODER
									LOG(LOG_VERBOSE, "################## H263+ header ######################");
									LOG(LOG_VERBOSE, "Reserved bits: %d", tH263PHeader->Reserved);
									LOG(LOG_VERBOSE, "P bit: %d", tH263PHeader->P);
									LOG(LOG_VERBOSE, "V bit: %d", tH263PHeader->V);
									LOG(LOG_VERBOSE, "Extra picture header length: %d", tH263PHeader->Plen);
									LOG(LOG_VERBOSE, "Amount of ignored bits: %d", tH263PHeader->Pebit);
									if (tH263PHeader->V)
									{
										LOG(LOG_VERBOSE, "################## H263+ VRC header ##################");
										LOG(LOG_VERBOSE, "Thread ID: %d", tH263PHeader->Vrc.Tid);
										LOG(LOG_VERBOSE, "Thread fragment number: %d", tH263PHeader->Vrc.Trunc);
										if (tH263PHeader->Vrc.S)
											LOG(LOG_VERBOSE, "Synch. frame: yes");
										else
											LOG(LOG_VERBOSE, "Synch. frame: no");
									}
								#endif

								// 2 bytes for standard h263+ header
								pData += 2;

								// do we have separate VRC byte?
								if (tH263PHeader->V)
									pData += 1;

								// do we have extra picture header?
								if (tH263PHeader->Plen > 0)
									pData += tH263PHeader->Plen;

								// do we have a picture start?
								if (tH263PHeader->P)
									tH263PPictureStart = true;
								else
									tH263PPictureStart = false;

								// convert from host to network byte order
								tH263PHeader->Data[0] = htonl(tH263PHeader->Data[0]);

								// if P bit was set clear the first 2 bytes of the frame data
								if (tH263PPictureStart)
								{
									#ifdef RTP_DEBUG_PACKET_DECODER
										LOG(LOG_VERBOSE, "P bit is set: clear first 2 byte of frame data");
									#endif
									pData -= 2; // 2 bytes backward
									//HINT: see section 6.1.1 in RFC 4629
									if (!pLoggingOnly)
									{
										pData[0] = 0;
										pData[1] = 0;
									}
								}
                            }
                            break;
            case CODEC_ID_H264:
                            // convert from network to host byte order
                            tH264Header->Data[0] = ntohl(tH264Header->Data[0]);

                            // HINT: convert from network to host byte order not necessary because we have only one byte
                            #ifdef RTP_DEBUG_PACKET_DECODER
                                LOG(LOG_VERBOSE, "################## H264 header ########################");
                                LOG(LOG_VERBOSE, "F bit: %d", tH264Header->F);
                                LOG(LOG_VERBOSE, "NAL ref. ind.: %d", tH264Header->Nri);
                                LOG(LOG_VERBOSE, "NAL unit type: %d", tH264Header->Type);
                                if ((tH264Header->Type == 28 /* Fu-A */) || (tH264Header->Type == 29 /* Fu-B */))
                                {
                                    LOG(LOG_VERBOSE, "################## H264FU header #######################");
                                    LOG(LOG_VERBOSE, "S bit: %d", tH264Header->FuA.S);
                                    LOG(LOG_VERBOSE, "E bit: %d", tH264Header->FuA.E);
                                    LOG(LOG_VERBOSE, "R bit: %d", tH264Header->FuA.R);
                                    LOG(LOG_VERBOSE, "Pl type: %d", tH264Header->FuA.PlType);
                                }
                            #endif

                            tH264HeaderType = tH264Header->Type;
                            if ((tH264Header->Type == 28 /* Fu-A */) || (tH264Header->Type == 29 /* Fu-B */))
                                tH264HeaderFragmentStart = tH264Header->FuA.S;

                            // convert from host to network byte order
                            tH264Header->Data[0] = htonl(tH264Header->Data[0]);

                            // go to the start of the payload data
                            switch(tH264HeaderType)
                            {
                                        // NAL unit  Single NAL unit packet per H.264
                                        case 1 ... 23:
                                                // HINT: RFC3984: The first byte of a NAL unit co-serves as the RTP payload header
                                                break;
                                        // STAP-A    Single-time aggregation packet
                                        case 24:
                                                pData += 1;
                                                break;
                                        // STAP-B    Single-time aggregation packet
                                        case 25:
                                        // MTAP16    Multi-time aggregation packet
                                        case 26:
                                        // MTAP24    Multi-time aggregation packet
                                        case 27:
                                                pData += 3;
                                                break;
                                        // FU-B      Fragmentation unit
                                        case 29:
                                        // FU-A      Fragmentation unit
                                        case 28:
                                                // start fragment?
                                                if (tH264HeaderFragmentStart)
                                                {
													#ifdef RTP_DEBUG_PACKET_DECODER
                                                		LOG(LOG_VERBOSE, "..H264 start fragment");
													#endif
                                                    // use FU header as NAL header, reconstruct the original NAL header
                                                    if (!pLoggingOnly)
                                                    {
                                                        #ifdef RTP_DEBUG_PACKET_DECODER
                                                            LOG(LOG_VERBOSE, "S bit is set: reconstruct NAL header");
                                                            LOG(LOG_VERBOSE, "..part F+NRI: %d", pData[0] & 0xE0);
                                                            LOG(LOG_VERBOSE, "..part TYPE: %d", pData[1] & 0x1F);
                                                        #endif
                                                        tH264HeaderReconstructed = (pData[0] & 0xE0 /* F + NRI */) + (pData[1] & 0x1F /* TYPE */);
                                                        pData[1] = tH264HeaderReconstructed;
                                                    }
                                                    pData += 1;
                                                }else
                                                {
													#ifdef RTP_DEBUG_PACKET_DECODER
														if (tH264Header->FuA.E)
															LOG(LOG_VERBOSE, "..H264 end fragment");
														else
															LOG(LOG_VERBOSE, "..H264 intermediate fragment");
													#endif
                                                    pData += 2;
                                                }
                                                break;
                                        // 0, 30, 31
                                        default:
                                                LOG(LOG_ERROR, "Undefined NAL type");
                                                break;
                            }

                            // in case it is no FU or it is one AND it is the start fragment:
                            //      create start sequence of [0, 0, 1]
                            // HINT: inspired by "h264_handle_packet" from rtp_h264.c from ffmpeg package
                            if (((tH264HeaderType != 28) && (tH264HeaderType != 29)) || (tH264HeaderFragmentStart))
                            {
                            	#ifdef RTP_DEBUG_PACKET_DECODER
								#endif
                            	if (!pLoggingOnly)
                            	{
    								//HINT: make sure that the byte order is correct here because we abuse the former RTP header memory
									pData -= 3;
									// create the start sequence "001"
									pData[0] = 0;
									pData[1] = 0;
									pData[2] = 1;
                            	}
                            }

                            break;
            case CODEC_ID_MPEG1VIDEO:
            case CODEC_ID_MPEG2VIDEO:
                            // convert from network to host byte order
                            tMPVHeader->Data[0] = ntohl(tMPVHeader->Data[0]);

                            pData += 4;

                            #ifdef RTP_DEBUG_PACKET_DECODER

                                LOG(LOG_VERBOSE, "################# MPV (mpeg1/2) header ####################");
                                switch(tMPVHeader->PType)
                                {
                                    case 1:
                                        LOG(LOG_VERBOSE, "Picture type: i-frame");
                                        break;
                                    case 2:
                                        LOG(LOG_VERBOSE, "Picture type: p-frame");
                                        break;
                                    case 3:
                                        LOG(LOG_VERBOSE, "Picture type: b-frame");
                                        break;
                                    case 4:
                                        LOG(LOG_VERBOSE, "Picture type: d-frame");
                                        break;
                                    default:
                                        LOG(LOG_VERBOSE, "Picture type: %d", tMPVHeader->PType); //TODO: rfc 2250 says value "0" is forbidden but ffmpeg uses it for ... what?
                                        break;
                                }

                                LOG(LOG_VERBOSE, "End of slice: %s", tMPVHeader->E ? "yes" : "no");
                                LOG(LOG_VERBOSE, "Begin of slice: %s", tMPVHeader->B ? "yes" : "no");
                                LOG(LOG_VERBOSE, "Sequence header: %s", tMPVHeader->S ? "yes" : "no");
                                LOG(LOG_VERBOSE, "Temporal ref.: %d", tMPVHeader->Tr);
                                LOG(LOG_VERBOSE, "Mpeg2 header present: %s", tMPVHeader->T ? "yes" : "no");
                            #endif

                            // is additional MPEG2 header present? => 4 bytes of additional data before raw mpeg data
                            if(tMPVHeader->T)
                                pData += 4;

                            // convert from host to network byte order
                            tMPVHeader->Data[0] = htonl(tMPVHeader->Data[0]);
                            break;
            case CODEC_ID_MPEG4:
                            #ifdef RTP_DEBUG_PACKET_DECODER
                                LOG(LOG_VERBOSE, "#################### MPEG4 header #######################");
                                LOG(LOG_VERBOSE, "No additional information");
                            #endif
                            break;
            case CODEC_ID_THEORA:
                            // convert from network to host byte order
                            tTHEORAHeader->Data[0] = ntohl(tTHEORAHeader->Data[0]);
                            pData += sizeof(THEORAHeader);
                            pData += 2;

                            // no fragmentmentation?
                            if (tTHEORAHeader->F == 0)
                                mIntermediateFragment = false;

                            // start fragment?
                            if (tTHEORAHeader->F == 1)
                                mIntermediateFragment = true;

                            // continuation fragment?
                            if (tTHEORAHeader->F == 2)
                                mIntermediateFragment = true;

                            // end fragment?
                            if (tTHEORAHeader->F == 3)
                                mIntermediateFragment = false;

                            #ifdef RTP_DEBUG_PACKET_DECODER
                                LOG(LOG_VERBOSE, "################### THEORA header #######################");
                                LOG(LOG_VERBOSE, "Configuration ID: %d", tTHEORAHeader->ConfigId);
                                LOG(LOG_VERBOSE, "Fragment type: %d", tTHEORAHeader->F);
                                LOG(LOG_VERBOSE, "Theora data type: %d", tTHEORAHeader->TDT);
                                LOG(LOG_VERBOSE, "Payload packet count: %d", tTHEORAHeader->Packets);
                            #endif
                            // convert from host to network byte order
                            tTHEORAHeader->Data[0] = htonl(tTHEORAHeader->Data[0]);
                            break;
            case CODEC_ID_VP8:
                            pData++; // default VP 8 header = 1 byte

                            // do we have extended control bits?
                            if (tVP8Header->X)
                            {
                                VP8ExtendedHeader* tVP8ExtendedHeader = (VP8ExtendedHeader*)pData;
                                pData++; // extended control bits = 1 byte

                                // do we have a picture ID?
                                if (tVP8ExtendedHeader->I)
                                    pData++;

                                // do we have a TL0PICIDX?
                                if (tVP8ExtendedHeader->L)
                                    pData++;

                                // do we have a TID?
                                if (tVP8ExtendedHeader->T)
                                    pData++;
                            }

                            #ifdef RTP_DEBUG_PACKET_DECODER
                                LOG(LOG_VERBOSE, "##################### VP8 header ########################");
                                LOG(LOG_VERBOSE, "Extended bits: %d", tVP8Header->X);
                                LOG(LOG_VERBOSE, "Non-reference frame: %d", tVP8Header->N);
                                LOG(LOG_VERBOSE, "Start of partition: %d", tVP8Header->S);
                            #endif
                            break;
//            case CODEC_ID_MPEG2TS:
//            case CODEC_ID_VORBIS:
            default:
                            LOG(LOG_ERROR, "Unsupported codec %d dropped by internal RTP parser", mStreamCodecID);
                            break;
    }

	#ifdef RTP_DEBUG_PACKET_DECODER
		if (mIntermediateFragment)
			LOG(LOG_VERBOSE, "FRAGMENT");
		else
			LOG(LOG_VERBOSE, "MESSAGE COMPLETE");
	#endif

	if (!pLoggingOnly)
	{// update the status variables
		// check if there was a new frame begun before the last was finished
		if ((mRemoteTimestampLastCompleteFrame != mRemoteTimestampLastPacket) && (mRemoteTimestampLastPacket != mRemoteTimestamp))
		{
			AnnounceLostPackets(1);
			LOG(LOG_ERROR, "Packet belongs to new frame while last frame is incomplete, overall packet loss is now %"PRIu64", last complete timestamp: %"PRIu64", last timestamp: %"PRIu64"", mLostPackets, mRemoteTimestampLastCompleteFrame, mRemoteTimestampLastPacket);
		}
		// store the timestamp of the last complete frame
		if (!mIntermediateFragment)
			mRemoteTimestampLastCompleteFrame = mRemoteTimestamp;

		mRemoteSequenceNumberLastPacket = mRemoteSequenceNumber;
		mRemoteTimestampLastPacket = mRemoteTimestamp;
	}else
	{// restore the original unchanged packet memory here

	    // convert from host to network byte order again
	    for (int i = 0; i < 3; i++)
	        tRtpHeader->Data[i] = htonl(tRtpHeader->Data[i]);
	}

    // decrease data size by the size of the found header structures
    if ((pData - tRtpPacketStart) > (int)pDataSize)
    {
        LOG(LOG_ERROR, "Illegal value for calculated data size (%u - %u)", pDataSize, pData - tRtpPacketStart);

        pIsLastFragment = true;

        return false;
    }
    pDataSize -= (pData - tRtpPacketStart);

    // return if packet contains the last fragment of the current frame
    pIsLastFragment = !mIntermediateFragment;

    return true;
}

/*************************************************
 *  Video codec name to RTP id mapping:
 *  ===================================
 *        h261						31
 *        h263						34
 *        mpeg1video				32
 *        mpeg2video				32
 *        h263+ 					119 (HC internal standard)
 *        h264						120 (HC internal standard)
 *        mpeg4						121 (HC internal standard)
 *        theora					122 (HC internal standard)
 *        vp8						123 (HC internal standard)
 *
 *
 *  Audio codec name to RTP id mapping:
 *  ===================================
 *        ulaw						0
 *        gsm						3
 *        alaw						8
 *        g722						9
 *        pcms16					10
 *        mp3						14
 *        aac						100 (HC internal standard)
 *        amr						101 (HC internal standard)
 *
 ****************************************************/
unsigned int RTP::CodecToPayloadId(std::string pName)
{
    unsigned int tResult = -1;

    //video
    if (pName == "h261")
        tResult = 31;
    if ((pName == "mpeg2video") || (pName == "mpeg1video"))
        tResult = 32;
    if (pName == "h263")
        tResult = 34;
    if (pName == "h263+")
        tResult = 119;
    if (pName == "h264")
        tResult = 120;
    if (pName == "mpeg4")
        tResult = 121;
    if ((pName == "theora") || (pName == "libtheora") /* delivered from AVCodec->name */)
        tResult = 122;
    if (pName == "vp8")
        tResult = 123;

    //audio
    if (pName == "ulaw")
        tResult = 0;
    if (pName == "gsm")
        tResult = 3;
    if (pName == "alaw")
        tResult = 8;
    if (pName == "g722")
        tResult = 9;
    if (pName == "pcms16")
        tResult = 10;
    if (pName == "mp3")
        tResult = 14;
    if (pName == "aac")
        tResult = 100;
    if (pName == "amr")
        tResult = 101;

    //LOGEX(RTP, LOG_VERBOSE, ("Translated " + pName + " to %d").c_str(), tResult);

    return tResult;
}

static std::string PayloadType(int pId)
{
    string tResult = "unknown";

    switch(pId)
    {
        //video
        case 31:
        case 32:
        case 34:
        case 118:
        case 119:
        case 120:
        case 121:
        case 122:
        case 123:
                tResult = "VIDEO";
                break;

        //audio
        case 0:
        case 3:
        case 8:
        case 9:
        case 10:
        case 11:
        case 14:
        case 100:
        case 101:
                tResult = "AUDIO";
                break;

        //others
        case 72 ... 76:
                tResult = "RTCP";
                break;

        //others
        case 96 ... 99:
                tResult = "DYNAMIC";
                break;
        default:
                tResult = "unknown";
                break;
    }

    //LOGEX(RTP, LOG_VERBOSE, ("Translated %d to " + tResult).c_str(), pId);

    return tResult;
}

string RTP::PayloadIdToCodec(int pId)
{
    string tResult = "unknown";

    switch(pId)
    {
        //video
        case 31:
                tResult = "h261";
                break;
        case 32:
                tResult = "mpeg1/2";
                break;
        case 34:
        case 118:
                tResult = "h263";
                break;
        case 119:
                tResult = "h263+";
                break;
        case 120:
                tResult = "h264";
                break;
        case 121:
                tResult = "mpeg4";
                break;
        case 122:
                tResult = "theora";
                break;
        case 123:
                tResult = "vp8";
                break;

        //audio
        case 0:
                tResult = "G711 u-law)";
                break;
        case 3:
                tResult = "gsm";
                break;
        case 8:
                tResult = "G711 a-law)";
                break;
        case 9:
                tResult = "G722 adpcm";
                break;
        case 10:
                tResult = "pcm16";
                break;
        case 11:
                tResult = "pcm16 mono";
                break;
        case 14:
                tResult = "mp3";
                break;
        case 100:
                tResult = "aac";
                break;
        case 101:
                tResult = "amr";
                break;

        //others
        case 72 ... 76:
                tResult = "rtcp";
                break;

        //others
        case 96 ... 99:
                tResult = "ffmpeg dynamic";
                break;
    }

    //LOGEX(RTP, LOG_VERBOSE, ("Translated %d to " + tResult).c_str(), pId);

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////// RTCP handling ///////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

string GetRtcpPayloadTypeStr(int pType)
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

void RTP::LogRtcpHeader(RtcpHeader *pRtcpHeader)
{
    // convert from network to host byte order, HACK: exceed array boundaries
    for (int i = 0; i < 3; i++)
        pRtcpHeader->Data[i] = ntohl(pRtcpHeader->Data[i]);
    int tRtcpHeaderLength = pRtcpHeader->Feedback.Length + 1;

    // conver the rest
    for (int i = 3; i < tRtcpHeaderLength; i++)
        pRtcpHeader->Data[i] = ntohl(pRtcpHeader->Data[i]);

    LOGEX(RTP, LOG_VERBOSE, "################## RTCP header ########################");
    LOGEX(RTP, LOG_VERBOSE, "Version: %d", pRtcpHeader->General.Version);
    if (pRtcpHeader->General.Padding)
        LOGEX(RTP, LOG_VERBOSE, "Padding: true");
    else
        LOGEX(RTP, LOG_VERBOSE, "Padding: false");
    LOGEX(RTP, LOG_VERBOSE, "Type: %s", GetRtcpPayloadTypeStr(pRtcpHeader->General.Type).c_str());
    LOGEX(RTP, LOG_VERBOSE, "Counter/format: %d", pRtcpHeader->General.RC);
    LOGEX(RTP, LOG_VERBOSE, "Length: %d (entire packet size: %d)", pRtcpHeader->General.Length, (pRtcpHeader->General.Length + 1 /* length is reported minus one */) * 4 /* 32 bit words */);
    LOGEX(RTP, LOG_VERBOSE, "SSRC            : %u", pRtcpHeader->Feedback.Ssrc);

    // sender report
    if (pRtcpHeader->General.Type == RTCP_SENDER_REPORT)
    {
        LOGEX(RTP, LOG_VERBOSE, "Timestamp(high) : %10u", pRtcpHeader->Feedback.TimestampHigh);
        LOGEX(RTP, LOG_VERBOSE, "Timestamp(low)  : %10u", pRtcpHeader->Feedback.TimestampLow);
        LOGEX(RTP, LOG_VERBOSE, "RTP Timestamp (abs.) : %10u", pRtcpHeader->Feedback.RtpTimestamp);
        LOGEX(RTP, LOG_VERBOSE, "Packets         : %10u", pRtcpHeader->Feedback.Packets);
        LOGEX(RTP, LOG_VERBOSE, "Octets          : %10u", pRtcpHeader->Feedback.Octets);

        int64_t tRemoteTimestamHigh = (int64_t)pRtcpHeader->Feedback.TimestampHigh * 1000 * 1000;
        int64_t tRemoteTimestamLow = ((int64_t)pRtcpHeader->Feedback.TimestampLow * 1000 * 1000) >> 32;
        int64_t tRemoteUsTimestamp = tRemoteTimestamHigh + tRemoteTimestamLow;
        int64_t tRemoteTimestamp = tRemoteUsTimestamp - NTP_OFFSET_US;
        int64_t tLocalTimestamp = av_gettime();

        LOGEX(RTP, LOG_VERBOSE, "Local     time: %"PRId64"", tLocalTimestamp);
        LOGEX(RTP, LOG_VERBOSE, "Remote    time: %"PRId64"", tRemoteTimestamp);
        LOGEX(RTP, LOG_VERBOSE, "Remote US time: %"PRId64"", tRemoteUsTimestamp);
        LOGEX(RTP, LOG_VERBOSE, "Remote to local time difference: %"PRId64" us", tLocalTimestamp - tRemoteTimestamp);
    }

    // convert from host to network byte order, HACK: exceed array boundaries
    for (int i = 0; i < tRtcpHeaderLength; i++)
        pRtcpHeader->Data[i] = htonl(pRtcpHeader->Data[i]);
}

bool RTP::RtcpParseSenderReport(char *&pData, int &pDataSize, int64_t &pEndToEndDelay, unsigned int &pPackets, unsigned int &pOctets, float &pRelativeLoss)
{
    //HINT: assumes network byte order!

    bool tResult = false;
    RtcpHeader* tRtcpHeader = (RtcpHeader*)pData;

    // convert from network to host byte order
    for (int i = 0; i < 7; i++)
        tRtcpHeader->Data[i] = ntohl(tRtcpHeader->Data[i]);
    int tRtcpHeaderLength = tRtcpHeader->Feedback.Length + 1;

    if (tRtcpHeaderLength == 7 /* need 28 byte sender report */)
    {// update values
        uint64_t tRemoteNtpTimestampHigh = (uint64_t)tRtcpHeader->Feedback.TimestampHigh * 1000 * 1000;
        uint64_t tRemoteNtpTimestampLow = ((uint64_t)tRtcpHeader->Feedback.TimestampLow * 1000 * 1000) >> 32;
        uint64_t tRemoteNtpUsTimestamp = tRemoteNtpTimestampHigh + tRemoteNtpTimestampLow;
        uint64_t tRemoteNtpTimestamp = tRemoteNtpUsTimestamp - NTP_OFFSET_US;
        uint64_t tLocalNtpTimestamp = av_gettime();

        // #############################################################
        // START TIMESTAMP: update the remote start timestamp
        // #############################################################
        if (mRemoteStartTimestamp == 0)
        {
            // we have to reset the timestamp calculation
            mRemoteStartTimestamp = tRtcpHeader->Feedback.RtpTimestamp;
        }

        //HINT: the START SEQUENCE NUMBER: cannot be updated because this data is nopt included in the RTCP header

        pEndToEndDelay = tLocalNtpTimestamp - tRemoteNtpTimestamp;
        pPackets = tRtcpHeader->Feedback.Packets;
        pOctets = tRtcpHeader->Feedback.Octets;

        mSynchDataMutex.lock();
        if (mRtcpLastRemotePackets != 0)
        {
            uint64_t tLocallyReceivedPackets = mReceivedPackets - mRtcpLastReceivedPackets;
            uint64_t tRemotelyReportedSentPackets = pPackets - mRtcpLastRemotePackets + 1;
            double tRelativeLoss = 100 - 100 * (double)tLocallyReceivedPackets / tRemotelyReportedSentPackets;
            mRelativeLostPackets = (float)tRelativeLoss;
            pRelativeLoss = mRelativeLostPackets;

            #ifdef RTCP_DEBUG_PACKETS_DECODER
                LOG(LOG_VERBOSE, "Received NTP time: US %lu, high %u, low %u, DE %lu/%lu (diff: %lu)", tRemoteNtpUsTimestamp, tRtcpHeader->Feedback.TimestampHigh, tRtcpHeader->Feedback.TimestampLow, tRemoteNtpTimestamp, av_gettime(), tLocalNtpTimestamp- tRemoteNtpTimestamp);
                LOG(LOG_VERBOSE, "Received packets: %"PRIu64", should have received: %"PRIu64", loss: %.2f", tLocallyReceivedPackets, tRemotelyReportedSentPackets, pRelativeLoss);
            #endif
        }
        mRtcpLastRemoteNtpTime = tRemoteNtpTimestamp;
        mRtcpLastRemoteTimestamp = mRemoteTimestampOverflowShift + (uint64_t)tRtcpHeader->Feedback.RtpTimestamp - mRemoteStartTimestamp;
        mRtcpLastRemotePackets = tRtcpHeader->Feedback.Packets;
        mRtcpLastRemoteOctets = tRtcpHeader->Feedback.Octets;
        mRtcpLastReceivedPackets = mReceivedPackets;
        mSynchDataMutex.unlock();

        tResult = true;
    }else
    {// set fall back values
        pPackets = 0;
        pOctets = 0;
        tResult = false;
    }
    // convert from host to network byte order, HACK: exceed array boundaries
    for (int i = 0; i < 7; i++)
        tRtcpHeader->Data[i] = htonl(tRtcpHeader->Data[i]);

    return tResult;
}

void RTP::SetSynchronizationReferenceForRTP(uint64_t pReferenceNtpTime, uint32_t pReferencePts)
{
    if (!mRtpEncoderOpened)
        return;

    if (pReferencePts < 1)
        return;

	#ifdef RTP_DEBUG_PACKET_ENCODER_TIMESTAMPS
    	LOG(LOG_VERBOSE, "New synchronization for %d codec: %u, clock: %.2f, RTP timestamp: %.2f, timestamp offset: %lu", mStreamCodecID, pReferencePts, CalculateClockRateFactor(), (float)pReferencePts * CalculateClockRateFactor(), mLocalTimestampOffset);
	#endif

	mSyncDataMutex.lock();
    mSyncNTPTime = pReferenceNtpTime;
    mSyncPTS = mLocalTimestampOffset + (uint64_t)(pReferencePts * CalculateClockRateFactor()); // clock rate adaption according to rfc (mpeg uses 90 kHz)
    mSyncDataMutex.unlock();
}

void RTP::RtcpPatchLiveSenderReport(char *pHeader, uint32_t pTimestamp)
{
    uint64_t tNtpTime;
    uint32_t tPts = pTimestamp;

    if (mSyncNTPTime != 0)
    {// use the NTP time which was explicitly given (should be determined right before the A/V encoding step)
        mSyncDataMutex.lock();
        tNtpTime = mSyncNTPTime;
        tPts = mSyncPTS;
        mSyncDataMutex.unlock();
    }else
    {// use the NTP time of the processed A/V packets
        tNtpTime = GetNtpTime();
        tPts = pTimestamp;
    }

    RtcpHeader* tRtcpHeader = (RtcpHeader*)pHeader;

    // convert
    for (int i = 0; i < 7; i++)
        tRtcpHeader->Data[i] = ntohl(tRtcpHeader->Data[i]);

    tRtcpHeader->Feedback.TimestampHigh = tNtpTime / 1000000;
    tRtcpHeader->Feedback.TimestampLow = ((tNtpTime % 1000000) << 32) / 1000000;
    tRtcpHeader->Feedback.RtpTimestamp = tPts;

    #ifdef RTCP_DEBUG_PACKET_ENCODER_FFMPEG
        LOG(LOG_VERBOSE, "Setting RTCP synch.: (RTP %u, NTP: US %lu, high %u/%lu, low %u/%lu,  DE %lu / %lu)", pTimestamp, tNtpTime, tRtcpHeader->Feedback.TimestampHigh, tNtpTime / 1000000, tRtcpHeader->Feedback.TimestampLow, ((tNtpTime % 1000000) << 32) / 1000000, tNtpTime - NTP_OFFSET_US, av_gettime());
    #endif

    for (int i = 0; i < 7; i++)
        tRtcpHeader->Data[i] = htonl(tRtcpHeader->Data[i]);
}

// the following was inspired by RTCP handling in ffmpeg

// RTCP packets use 0.5% of the bandwidth
#define RTCP_TX_RATIO_NUM           5
#define RTCP_TX_RATIO_DEN           1000
#define RTCP_SR_SIZE                28
void RTP::RtcpCreateH261SenderReport(char *&pData /* current stream pointer */, unsigned int &pDataSize /* resulting stream size */, int64_t pCurPts)
{
    uint64_t tNtpTime = GetNtpTime();
    int tRtcpBytes = ((mH261SentOctets - mH261SentOctetsLastSenderReport) * RTCP_TX_RATIO_NUM) / RTCP_TX_RATIO_DEN;
    if ((mH261FirstPacket) || ((tRtcpBytes >= (int)RTCP_HEADER_SIZE /* 0.5 % rules */) && (tNtpTime - mH261SentNtpTimeLastSenderReport > 5000000 /* minimum period between two SRs is 5 seconds */)))
    {// we should create a sender report
        #ifdef RTCP_DEBUG_PACKETS_ENCODER
            LOG(LOG_VERBOSE, "Creating %d bytes H.261 sender report, %d reports already sent..", RTCP_HEADER_SIZE, mH261SenderReports);
        #endif
        mH261SenderReports++;

        // ################################################
        // write sender report packet size to RTP stream
        // ################################################
        unsigned int *tRtpPacketSize = (unsigned int*)pData;
        *tRtpPacketSize = htonl((uint32_t)RTCP_HEADER_SIZE);
        pData += 4;
        pDataSize += 4;

        // ################################################
        // write sender report to RTP stream
        // ################################################
        RtcpHeader* tRtcpHeader = (RtcpHeader*)pData;

        #ifdef RTCP_DEBUG_PACKETS_ENCODER
            LOG(LOG_VERBOSE, "Sender report with PTS: %"PRIu64" (own PTS: %"PRId64")", (uint64_t)pCurPts * CalculateClockRateFactor(), pCurPts);
        #endif
        tRtcpHeader->Feedback.Length = 6;
        tRtcpHeader->Feedback.Type = RTCP_SENDER_REPORT;
        tRtcpHeader->Feedback.Fmt = 0;
        tRtcpHeader->Feedback.Padding = 0;
        tRtcpHeader->Feedback.Version = 2;
        tRtcpHeader->Feedback.Ssrc = mLocalSourceIdentifier;
        tRtcpHeader->Feedback.TimestampHigh = tNtpTime / 1000000;
        tRtcpHeader->Feedback.TimestampLow = ((tNtpTime % 1000000) << 32) / 1000000;
        tRtcpHeader->Feedback.RtpTimestamp = pCurPts * CalculateClockRateFactor() /* 90 kHz clock rate */;
        tRtcpHeader->Feedback.Packets = mH261SentPackets;
        tRtcpHeader->Feedback.Octets = mH261SentOctets;

        // convert from host to network byte order
        for (int i = 0; i < 7; i++)
            tRtcpHeader->Data[i] = htonl(tRtcpHeader->Data[i]);

        #ifdef RTCP_DEBUG_PACKETS_ENCODER
            LogRtcpHeader(tRtcpHeader);
        #endif

        pData += RTCP_SR_SIZE;
        pDataSize += RTCP_SR_SIZE;

        mH261SentOctetsLastSenderReport = mH261SentOctets;
        mH261SentNtpTimeLastSenderReport = tNtpTime;
        mH261FirstPacket = false;
    }
}
///////////////////////////////////////////////////////////////////////////////

}} //namespace
