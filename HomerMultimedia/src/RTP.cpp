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
     Funktionsstatus:
             Senden                         Empfangen                   Bemerkungen
    ---------------------------------------------------------------------------------------------------------
     h261:   ok (VC)                        ok (VC)                     (bei ekiga kommt es zu abst�rzen oder aber nur bildfehlern)
     h263:   nicht moeglich, h263+ anstatt  ok (linphone, ekiga, VC)    (ffmpeg unterstuetzt zum senden die entsprechende rtp-paketierung nicht, empfang laeuft ueber eigenen rtp-parser)
    h263+:   ok (linphone, VC)              ok (linphone, VC)           (ekiga raucht bei dem codec sowohl beim senden und auch empfangen ab)
     h264:   ok (linphone, VC)              ok (VC)                     (in ekiga nicht unterst�tzt) (linphone raucht beim senden genuesslich ab)
    Mpeg4:   ok (linphone, ekiga, VC)       ok (linphone, ekiga, VC)

     pcma:   ok (VC)                        ?
     pcmu:   ok (VC)                        ?
      mp3:   ok (VC)                        ok (VC)
      aac:   ?                              ?
      gsm:
      amr:

     unsupported:
       mjpeg
 */

/*
     RTP parameters: http://www.iana.org/assignments/rtp-parameters
 */

/*
     MP3 hack:
         Problem: ffmpeg buffers mp3 fragments but there is no value in the RTP headers to store the size of the entire orignal MP3 buffer
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

using namespace std;
using namespace Homer::Monitor;

namespace Homer { namespace Multimedia {

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

// ########################## RTCP ###########################################
union RtcpHeader{
    struct{ // send via separate port
        unsigned short int Length;          /* length of report */
        unsigned int ReportType:8;          /* report type */
        unsigned int RC:5;                  /* report counter */
        unsigned int Padding:1;             /* padding flag */
        unsigned int Version:2;             /* protocol version */

        unsigned int Ssrc;                  /* synchronization source */
    } __attribute__((__packed__))RtpBased;
    struct{ // send within media stream as intermediate packets
        unsigned short int Length;          /* length of report */
        unsigned int PlType:8;              /* Payload type (PT) */
        unsigned int Fmt:5;                 /* Feedback message type (FMT) */
        unsigned int Padding:1;             /* padding flag */
        unsigned int Version:2;             /* protocol version */

        unsigned int Ssrc;                  /* synchronization source */
    } __attribute__((__packed__))Feedback;
    uint32_t Data[2];
};

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
unsigned int RTP::mH261PayloadSizeMax;
void RTP::SetH261PayloadSizeMax(unsigned int pMaxSize)
{//workaround for separation of RTP packetizer and the payload limit problem which is caused by the missing RTP support for H261 within ffmpeg
    mH261PayloadSizeMax = pMaxSize - RTP_HEADER_SIZE - H261_HEADER_SIZE;
}
unsigned int RTP::GetH261PayloadSizeMax()
{
    return mH261PayloadSizeMax;
}
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

///////////////////////////////////////////////////////////////////////////////

RTP::RTP()
{
    LOG(LOG_VERBOSE, "Created");
    mLastSequenceNumber = 0;
    mLastTimestamp = 0;
    mLostPackets = 0;
    mFrameFragmentation = 0;
    mPacketStatistic = NULL;
    mPayloadId = 0;
    mEncoderOpened = false;
    mUseInternalEncoder = false;
    // set SRC to 0 as ffmpeg does
    mSsrc = 0;
}

RTP::~RTP()
{
    LOG(LOG_VERBOSE, "Destroyed");
}

///////////////////////////////////////////////////////////////////////////////

bool RTP::OpenRtpEncoderH261(string pTargetHost, unsigned int pTargetPort, AVStream *pInnerStream)
{
	mSsrc = av_get_random_seed();
	mCurrentTimestamp = av_get_random_seed();

    LOG(LOG_VERBOSE, "Using lib internal rtp packetizer for H261 codec");
    LOG(LOG_INFO, "Opened...");
    LOG(LOG_INFO, "    ..rtp target: %s:%u", pTargetHost.c_str(), pTargetPort);
    LOG(LOG_INFO, "    ..rtp header size: %d", RTP_HEADER_SIZE);
    LOG(LOG_INFO, "    ..rtp TIMESTAMP: %u", mCurrentTimestamp);
    LOG(LOG_INFO, "    ..rtp SRC: %u", mSsrc);
    LOG(LOG_INFO, "  Wrapping following codec...");
    LOG(LOG_INFO, "    ..codec name: %s", pInnerStream->codec->codec->name);
    LOG(LOG_INFO, "    ..codec long name: %s", pInnerStream->codec->codec->long_name);
    LOG(LOG_INFO, "    ..resolution: %d * %d pixels", pInnerStream->codec->width, pInnerStream->codec->height);
//    LOG(LOG_INFO, "    ..codec time_base: %d/%d", mCodecContext->time_base.den, mCodecContext->time_base.num); // inverse
    LOG(LOG_INFO, "    ..stream rfps: %d/%d", pInnerStream->r_frame_rate.num, pInnerStream->r_frame_rate.den);
    LOG(LOG_INFO, "    ..stream time_base: %d/%d", pInnerStream->time_base.den, pInnerStream->time_base.num); // inverse
    LOG(LOG_INFO, "    ..stream codec time_base: %d/%d", pInnerStream->codec->time_base.den, pInnerStream->codec->time_base.num); // inverse
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
    mEncoderOpened = true;
    mUseInternalEncoder = true;
    return true;
}

bool RTP::OpenRtpEncoder(string pTargetHost, unsigned int pTargetPort, AVStream *pInnerStream)
{
    if (mEncoderOpened)
        return false;

    const char *tCodecName = pInnerStream->codec->codec->name;
    mPayloadId = FfmpegNameToPayloadId(tCodecName);
    LOG(LOG_VERBOSE, "New payload id: %4d, Codec: %s", mPayloadId, tCodecName);

    mTargetHost = pTargetHost;
    mTargetPort = pTargetPort;

    if (pInnerStream->codec->codec->id == CODEC_ID_H261)
    	return OpenRtpEncoderH261(pTargetHost, pTargetPort, pInnerStream);

    int                 tResult;
    AVOutputFormat      *tFormat;
    AVStream            *tOuterStream;

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

    // verbose timestamp debugging    mRtpFormatContext->debug = FF_FDEBUG_TS;

    // allocate new stream structure
	tOuterStream = av_new_stream(mRtpFormatContext, 0);//(AVStream*)av_mallocz(sizeof(AVStream));
    if (tOuterStream == NULL)
    {
        LOG(LOG_ERROR, "Memory allocation failed");
        return false;
    }

    // copy stream description from original stream description
    memcpy(tOuterStream, pInnerStream, sizeof(AVStream));
    tOuterStream->priv_data = NULL;
    // create monotone time stamps
    tOuterStream->cur_dts = 0;
    tOuterStream->reference_dts = 0;

    // set target coordinates for rtp stream
    snprintf(mRtpFormatContext->filename, sizeof(mRtpFormatContext->filename), "rtp://%s:%u", pTargetHost.c_str(), pTargetPort);

    // open streaming
    if (url_open(&mURLContext, mRtpFormatContext->filename, URL_WRONLY) < 0)
    {
        LOG(LOG_ERROR, "Could not open RTP streaming to %s:%u", pTargetHost.c_str(), pTargetPort);

        // free codec and stream 0
        av_freep(&mRtpFormatContext->streams[0]);

        // Close the format context
        av_free(mRtpFormatContext);

        return false;
    }
    // patch the max. packet size to rtp payload limit of the original stream
    mURLContext->max_packet_size = tOuterStream->codec->rtp_payload_size;

    int tMaxPacketSize = url_get_max_packet_size(mURLContext);

    // create memory stream and init ffmpeg internal strucutres
    if ((tResult = url_open_dyn_packet_buf(&mRtpFormatContext->pb, tMaxPacketSize)) < 0)
    {
        LOG(LOG_ERROR, "Creation of memory stream failed because of \"%s\".", strerror(AVUNERROR(tResult)));

        // close RTP stream
        url_close(mURLContext);

        // free codec and stream 0
        av_freep(&mRtpFormatContext->streams[0]);

        // Close the format context
        av_free(mRtpFormatContext);

        return false;
    }

    // mark as stream
    mRtpFormatContext->pb->is_streamed = 1;
    // limit packet size, otherwise ffmpeg will deliver unpredictable results ;)
    mRtpFormatContext->pb->max_packet_size = tMaxPacketSize;

    // reset output stream parameters
    if ((tResult = av_set_parameters(mRtpFormatContext, NULL)) < 0)
    {
        LOG(LOG_ERROR, "Invalid output format parameters because of \"%s\".", strerror(AVUNERROR(tResult)));

        // close RTP stream
        url_close(mURLContext);

        // free codec and stream 0
        av_freep(&mRtpFormatContext->streams[0]);

        // Close the format context
        av_free(mRtpFormatContext);

        return false;
    }

    // Dump information about device file
    dump_format(mRtpFormatContext, 0, "RTP Encoder", true);

    // allocate streams private data buffer and write the streams header, if any
    av_write_header(mRtpFormatContext);

    // close memory stream
    uint8_t *tBuffer = NULL;
    url_close_dyn_buf(mRtpFormatContext->pb, &tBuffer);
    av_free(tBuffer);

    LOG(LOG_INFO, "Opened...");
    LOG(LOG_INFO, "    ..rtp target: %s:%u", pTargetHost.c_str(), pTargetPort);
    LOG(LOG_INFO, "    ..rtp header size: %d", RTP_HEADER_SIZE);
    LOG(LOG_INFO, "  Wrapping following codec...");
    LOG(LOG_INFO, "    ..codec name: %s", tOuterStream->codec->codec->name);
    LOG(LOG_INFO, "    ..codec long name: %s", tOuterStream->codec->codec->long_name);
    LOG(LOG_INFO, "    ..resolution: %d * %d pixels", tOuterStream->codec->width, tOuterStream->codec->height);
//    LOG(LOG_INFO, "    ..codec time_base: %d/%d", mCodecContext->time_base.den, mCodecContext->time_base.num); // inverse
    LOG(LOG_INFO, "    ..stream rfps: %d/%d", tOuterStream->r_frame_rate.num, tOuterStream->r_frame_rate.den);
    LOG(LOG_INFO, "    ..stream time_base: %d/%d", tOuterStream->time_base.den, tOuterStream->time_base.num); // inverse
    LOG(LOG_INFO, "    ..stream codec time_base: %d/%d", tOuterStream->codec->time_base.den, tOuterStream->codec->time_base.num); // inverse
    LOG(LOG_INFO, "    ..sample rate: %d Hz", tOuterStream->codec->sample_rate);
    LOG(LOG_INFO, "    ..channels: %d", tOuterStream->codec->channels);
    LOG(LOG_INFO, "    ..i-frame distance: %d pictures", tOuterStream->codec->gop_size);
    LOG(LOG_INFO, "    ..bit rate: %d Hz", tOuterStream->codec->bit_rate);
    LOG(LOG_INFO, "    ..qmin: %d", tOuterStream->codec->qmin);
    LOG(LOG_INFO, "    ..qmax: %d", tOuterStream->codec->qmax);
    LOG(LOG_INFO, "    ..mpeg quant: %d", tOuterStream->codec->mpeg_quant);
    LOG(LOG_INFO, "    ..pixel format: %d", (int)tOuterStream->codec->pix_fmt);
    LOG(LOG_INFO, "    ..sample format: %d", (int)tOuterStream->codec->sample_fmt);
    LOG(LOG_INFO, "    ..frame size: %d bytes", tOuterStream->codec->frame_size);
    LOG(LOG_INFO, "    ..max packet size: %d bytes", mRtpFormatContext->pb->max_packet_size);
    LOG(LOG_INFO, "    ..rtp payload size: %d bytes", tOuterStream->codec->rtp_payload_size);

    mEncoderOpened = true;
    mMp3Hack_EntireBufferSize = 0;

    return true;
}

bool RTP::CloseRtpEncoder()
{
    LOG(LOG_VERBOSE, "Going to close");

    if (mEncoderOpened)
    {
        if (mPayloadId != 31 /* h261 */)
        {
            // write the trailer, if any
            av_write_trailer(mRtpFormatContext);

            // close RTP stream
            url_close(mURLContext);

            // free stream 0
            av_freep(&mRtpFormatContext->streams[0]);

            // Close the format context
            av_free(mRtpFormatContext);
        }

        LOG(LOG_INFO, "...closed");
    }else
        LOG(LOG_INFO, "...wasn't open");

    mEncoderOpened = false;
    mUseInternalEncoder = false;

    return true;
}

void RTP::RTPRegisterPacketStatistic(PacketStatistic *pStatistic)
{
    mPacketStatistic = pStatistic;
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
//            case CODEC_ID_MPEG1VIDEO:
//            case CODEC_ID_MPEG2VIDEO:
            case CODEC_ID_MPEG4:
            case CODEC_ID_AAC:
//            case CODEC_ID_MP2:
            case CODEC_ID_MP3:
            case CODEC_ID_PCM_ALAW:
            case CODEC_ID_PCM_MULAW:
//            case CODEC_ID_PCM_S8:
//            case CODEC_ID_PCM_S16BE:
            case CODEC_ID_PCM_S16LE:
//            case CODEC_ID_PCM_U16BE:
//            case CODEC_ID_PCM_U16LE:
//            case CODEC_ID_PCM_U8:
//            case CODEC_ID_MPEG2TS:
            case CODEC_ID_AMR_NB:
//            case CODEC_ID_AMR_WB:
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
//            case CODEC_ID_MPEG1VIDEO:
//            case CODEC_ID_MPEG2VIDEO:
            case CODEC_ID_MPEG4:
                tResult = 0;
                break;
            case CODEC_ID_AAC:
                tResult = 0;//todo
                break;
//            case CODEC_ID_MP2:
            case CODEC_ID_MP3:
                tResult = sizeof(MPAHeader);
                break;
            case CODEC_ID_PCM_ALAW:
                tResult = 0;
                break;
            case CODEC_ID_PCM_MULAW:
                tResult = 0;
                break;
//            case CODEC_ID_PCM_S8:
//            case CODEC_ID_PCM_S16BE:
            case CODEC_ID_PCM_S16LE:
                tResult = 0;
                break;
//            case CODEC_ID_PCM_U16BE:
//            case CODEC_ID_PCM_U16LE:
//            case CODEC_ID_PCM_U8:
//            case CODEC_ID_MPEG2TS:
            case CODEC_ID_AMR_NB:
                tResult = 0;//todo
                break;
//            case CODEC_ID_AMR_WB:
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

bool RTP::RtpCreate(char *&pData, unsigned int &pDataSize)
{
    AVPacket                    tPacket;
    int                         tResult;
    unsigned int                tMp3Hack_EntireBufferSize;
    if (!mEncoderOpened)
        return false;

    if (pData == NULL)
        return false;

    if (pDataSize <= 0)
        return false;

    //####################################################################
    // for H261 use the internal RTP implementation
    //####################################################################
    if (mUseInternalEncoder)
        return RtpCreateH261(pData, pDataSize);

    // check for supported codecs
    if(!IsPayloadSupported(mRtpFormatContext->streams[0]->codec->codec_id))
    {
        LOG(LOG_ERROR, "Codec %d is unsupported", mRtpFormatContext->streams[0]->codec->codec_id);
        pDataSize = 0;
        return true;
    }

    // save the amount of bytes of the original codec packet
    // HINT: we use this to store the size of the original codec packet within MPA header's MBZ entry
    //       later we use this MBZ entry inside of MediaSourceNet to detect the fragment/packet boundaries
    //       without this hack we wouldn't be able to provide correct processing because of buggy rfc for MPA payload definition
    tMp3Hack_EntireBufferSize = pDataSize;
    av_init_packet(&tPacket);

    // we only have one stream per media stream
    tPacket.stream_index = 0;
    tPacket.data = (uint8_t *)pData;
    tPacket.size = pDataSize; //TODO: pts from stream contextfor packet.pts? -> used in ffmpeg to create rtp packets
    #ifdef RTP_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Encapsulating codec packet:");
        LOG(LOG_VERBOSE, "      ..pts: %ld", tPacket.pts);
        LOG(LOG_VERBOSE, "      ..dts: %ld", tPacket.dts);
        LOG(LOG_VERBOSE, "      ..size: %d", tPacket.size);
        LOG(LOG_VERBOSE, "      ..pos: %ld", tPacket.pos);
    #endif

    //####################################################################
    // create memory stream and init ffmpeg internal structures
    //####################################################################
    int tMaxPacketSize = url_get_max_packet_size(mURLContext);
    #ifdef RTP_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Encapsulate frame with format %s of size: %u while maximum resulting RTP packet size is: %d", MediaSource::FfmpegId2FfmpegFormat(mRtpFormatContext->streams[0]->codec->codec_id).c_str(), pDataSize, tMaxPacketSize);
    #endif
    if ((tResult = url_open_dyn_packet_buf(&mRtpFormatContext->pb, tMaxPacketSize)) < 0)
    {
        LOG(LOG_ERROR, "Creation of memory stream failed because of \"%s\".", strerror(AVUNERROR(tResult)));

        return false;
    }

    // mark as stream
    mRtpFormatContext->pb->is_streamed = 1;
    // limit packet size, otherwise ffmpeg will deliver unpredictable results ;)
    mRtpFormatContext->pb->max_packet_size = tMaxPacketSize;

    //####################################################################
    // send encoded frame to the RTP muxer
    //####################################################################
    if ((tResult = av_write_frame(mRtpFormatContext, &tPacket)) < 0)
    {
        LOG(LOG_ERROR, "Couldn't write encoded frame into RTP buffer because of \"%s\".", strerror(AVUNERROR(tResult)));

        return false;
    }

    // close memory stream and get all the resulting packets for sending
    uint8_t *tData = NULL;
    pDataSize = url_close_dyn_buf(mRtpFormatContext->pb, &tData);
    pData = (char*)tData;
    #ifdef RTP_DEBUG_PACKETS
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

        // go through all created RTP packets
        do{
            tRtpPacketSize = ntohl(*(uint32_t*)(tRtpPacket - 4));

            // if there is no packet data we should leave the send loop
            if (tRtpPacketSize == 0)
                break;

            RtpHeader* tHeader = (RtpHeader*)tRtpPacket;

            // convert from network to host byte order
            for (int i = 0; i < 3; i++)
                tHeader->Data[i] = ntohl(tHeader->Data[i]);

            #ifdef RTP_DEBUG_PACKETS
                LOG(LOG_VERBOSE, "Got RTP packet with payload type: %d", tHeader->PayloadType);
            #endif

            //#################################################################################
            //### patch payload type and hack packet size for MP3 streaming
            //### do this only if we don't have a RTCP packet from the ffmpeg lib
            //### HINT: ffmpeg uses payload type 14 and several others like dyn. range 96-127
            //#################################################################################
            if (tHeader->PayloadType != 72)
            {
                // patch ffmpeg payload values between 96 and 99
                // HINT: ffmpeg uses some strange intermediate packets with payload id 72
                //       -> don't know for what reason, but they should be kept as they are
                switch(mRtpFormatContext->streams[0]->codec->codec_id)
                {
                            case CODEC_ID_PCM_MULAW:
                                            tHeader->PayloadType = 0;
                                            break;
                            case CODEC_ID_PCM_ALAW:
                                            tHeader->PayloadType = 8;
                                            break;
                            case CODEC_ID_MP3:
                                            // HACK: some modification of the standard MPA payload header: use MBZ to signalize the size of the original audio packet
                                            tMPAHeader = (MPAHeader*)(tRtpPacket + RTP_HEADER_SIZE);

                                            // convert from network to host byte order
                                            tMPAHeader->Data[0] = ntohl(tMPAHeader->Data[0]);

                                            #ifdef RTP_DEBUG_PACKETS
                                                LOG(LOG_VERBOSE, "Set MBZ bytes to message size of %u", tMp3Hack_EntireBufferSize);
                                            #endif
                                            tMPAHeader->Mbz = (unsigned short int)tMp3Hack_EntireBufferSize;

                                            // convert from host to network byte order
                                            tMPAHeader->Data[0] = htonl(tMPAHeader->Data[0]);

                                            tHeader->PayloadType = 14;
                                            break;
                            case CODEC_ID_H261:
                                            tHeader->PayloadType = 31;
                                            break;
                            case CODEC_ID_AAC:
                                            tHeader->PayloadType = 100;
                                            break;
                            case CODEC_ID_AMR_NB:
                                            tHeader->PayloadType = 101;
                                            break;
                            case CODEC_ID_H263:
                                            // HINT: don't use the old id 34, otherwise the auto detection of the h263 packetizing scheme wouldn't work
                                            tHeader->PayloadType = 118;
                                            break;
                            case CODEC_ID_H263P:
                                            tHeader->PayloadType = 119; // our own standard, possible because this range is user defined
                                            break;
                            case CODEC_ID_H264:
                                            tHeader->PayloadType = 120; // defacto standard
                                            break;
                            case CODEC_ID_MPEG4:
                                            tHeader->PayloadType = 121; // defacto standard
                                            break;
                }
            }

            // convert from host to network byte order
            for (int i = 0; i < 3; i++)
                tHeader->Data[i] = htonl(tHeader->Data[i]);

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
        #ifdef RTP_DEBUG_PACKETS
            LOG(LOG_VERBOSE, "Resulting RTP stream is invalid (maybe ffmpeg buffers packets until max. packet size is reached)");
        #endif
        return false;
    }

    return true;
}

// HINT: ffmpeg lacks support for rtp encapsulation for h261  codec
bool RTP::RtpCreateH261(char *&pData, unsigned int &pDataSize)
{
    if (!mEncoderOpened)
        return false;

    // calculate the amount of needed RTP packets to encapsulate the whole frame
    unsigned int tPacketCount = (pDataSize + (unsigned int)RTP_MAX_H261_PAYLOAD_SIZE - 1);
    tPacketCount /= (unsigned short int)(RTP_MAX_H261_PAYLOAD_SIZE);
    #ifdef RTP_DEBUG_PACKETS
        LOG(LOG_VERBOSE, "Calculated h261 packets: %u", tPacketCount);
    #endif
    // allocate memory for the RTP packet stream
    // HINT: the size of memory for storing the stream of RTP packets is rounded to multiples of (RTP_MAX_PACKET_SIZE + 4)
    unsigned int tRtpStreamDataMaxSize = tPacketCount * (RTP_MAX_H261_PAYLOAD_SIZE + H261_HEADER_SIZE + RTP_HEADER_SIZE + 4 /* overhead for packet length field */);
    char *tRtpStreamData = (char *)av_malloc(tRtpStreamDataMaxSize);
    unsigned int tRtpStreamDataSize = 0;
    // get a timestamp
    time_t tTimestamp;
    time(&tTimestamp);

    // get pointer to the current working address inside rtp packet stream
    char *tCurrentRtpStreamData = tRtpStreamData;
    for (unsigned int tPacketIndex = 0; tPacketIndex < tPacketCount; tPacketIndex++)
    {
        // size of current frame chunk
        unsigned int tChunkSize = 0;

        if (pDataSize > (RTP_MAX_H261_PAYLOAD_SIZE))
            tChunkSize = (RTP_MAX_H261_PAYLOAD_SIZE);
        else
            tChunkSize = pDataSize;

        tRtpStreamDataSize += 4 + RTP_HEADER_SIZE + H261_HEADER_SIZE + tChunkSize;
        if (tRtpStreamDataSize > tRtpStreamDataMaxSize)
        {
            LOG(LOG_ERROR, "Stream of RTP packets for H261 ecnapsulation too big");
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
        // HEADER: create rtp header
        // #############################################################
        // get pointer to RTP header buffer
        RtpHeader* tRtpHeader  = (RtpHeader*)tCurrentRtpStreamData;

        tRtpHeader->Version = 2; // current RTP-rfc 3550 defines version 2
        tRtpHeader->Padding = 0; // no padding octets
        tRtpHeader->Extension = 0; // no extension header used
        tRtpHeader->CsrcCount = 0; // no usage of CSRCs
        tRtpHeader->Marked = (tPacketIndex == tPacketCount - 1)?1:0; // 1 = last fragment, 0 = intermediate fragment
        tRtpHeader->PayloadType = 31; // 31 = h261
        tRtpHeader->SequenceNumber = ++mLastSequenceNumber; // monotonous growing
        tRtpHeader->Timestamp = tTimestamp; // use linux timestamp
        tRtpHeader->Ssrc = mSsrc; // use the initially computed unique ID

        // convert from host to network byte order
        for (int i = 0; i < 3; i++)
            tRtpHeader->Data[i] = htonl(tRtpHeader->Data[i]);

        // go to the start of the h261 header
        tCurrentRtpStreamData += RTP_HEADER_SIZE;

        // #############################################################
        // HEADER: create h261 header
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
        // PAYLOAD: copy chunk from original frame buffer
        // #############################################################
        // copy data from original buffer
        memcpy(tCurrentRtpStreamData, pData, tChunkSize);
        // jump to the next chunk of the original frame buffer
        pData += tChunkSize;

        // go to the next packet header
        tCurrentRtpStreamData += tChunkSize;
    }
    pData = tRtpStreamData;
    pDataSize = tRtpStreamDataSize;

    return true;
}

unsigned int RTP::GetLostPacketsFromRTP()
{
    return mLostPackets;
}

void RTP::AnnounceLostPackets(unsigned int pCount)
{
    mLostPackets += pCount;
    if (mPacketStatistic != NULL)
        mPacketStatistic->SetLostPacketCount(mLostPackets);
}

// assumption: we are getting one single RTP encapsulated packet, not auto detection of following additional packets included
bool RTP::RtpParse(char *&pData, unsigned int &pDataSize, bool &pIsLastFragment, bool &pIsSenderReport, enum CodecID pCodecId, bool pReadOnly)
{
    pIsLastFragment = false;
    pIsSenderReport= false;

    // is there some data?
    if (pDataSize == 0)
        return false;

    bool tOldH263PayloadDetected = false;
    char *tDataOriginal = pData;

    // check for supported codecs
    switch(pCodecId)
    {
            //supported audio codecs
            case CODEC_ID_PCM_MULAW:
            case CODEC_ID_PCM_ALAW:
            case CODEC_ID_PCM_S16LE:
            case CODEC_ID_MP3:
            case CODEC_ID_AMR_NB:
            case CODEC_ID_AAC:
            //supported video codecs
            case CODEC_ID_H261:
            case CODEC_ID_H263:
            case CODEC_ID_H263P:
            case CODEC_ID_H264:
            case CODEC_ID_MPEG4:
                    break;
            default:
                    LOG(LOG_ERROR, "Codec %d is unsupported by internal RTP parser", pCodecId);
                    pDataSize = 0;
                    pIsLastFragment = true;
                    return false;
    }

    // #############################################################
    // HEADER: parse rtp header
    // #############################################################
    RtpHeader* tRtpHeader = (RtpHeader*)pData;

    // convert from network to host byte order
    for (int i = 0; i < 3; i++)
        tRtpHeader->Data[i] = ntohl(tRtpHeader->Data[i]);

    #ifdef RTP_DEBUG_PACKETS
        // print some verbose outputs
        LOG(LOG_VERBOSE, "################## RTP header ########################");
        LOG(LOG_VERBOSE, "Version: %d", tRtpHeader->Version);
        if (tRtpHeader->Padding)
            LOG(LOG_VERBOSE, "Padding: true");
        else
            LOG(LOG_VERBOSE, "Padding: false");
        if (tRtpHeader->Extension)
            LOG(LOG_VERBOSE, "Extension: true");
        else
            LOG(LOG_VERBOSE, "Extension: false");
        LOG(LOG_VERBOSE, "SSRC: %u", tRtpHeader->Ssrc);
        LOG(LOG_VERBOSE, "CSRC count: %u", tRtpHeader->CsrcCount);
        // HINT: after converting from host to network byte order the original value within the RTP header can't be read anymore
        switch(tRtpHeader->Marked)
        {
            case 0:
                    LOG(LOG_VERBOSE, "Marked: no");
                    break;
            case 1:
                    LOG(LOG_VERBOSE, "Marked: yes");
                    break;
        }
        switch(tRtpHeader->PayloadType)
        {
                    // audio
                    case 0:
                        LOG(LOG_VERBOSE, "Payload type: old PCMU");
                        break;
                    case 8:
                        LOG(LOG_VERBOSE, "Payload type: old PCMA");
                        break;
                    case 14:
                        LOG(LOG_VERBOSE, "Payload type: old mpa (mp2, mp3)");
                        break;
                    // video
                    case 31:
                        LOG(LOG_VERBOSE, "Payload type: old h261");
                        break;
                    case 34:
                        LOG(LOG_VERBOSE, "Payload type: old h263");
                        break;
                    case 72 ... 76:
                        LOG(LOG_VERBOSE, "Payload type: rtcp");
                        break;
                    case 100:
                        LOG(LOG_VERBOSE, "Payload type: aac");
                        break;
                    case 101:
                        LOG(LOG_VERBOSE, "Payload type: amr");
                        break;
                    case 118:
                        LOG(LOG_VERBOSE, "Payload type: h263");
                        break;
                    case 119:
                        LOG(LOG_VERBOSE, "Payload type: h263+");
                        break;
                    case 120:
                        LOG(LOG_VERBOSE, "Payload type: h264");
                        break;
                    case 121:
                        LOG(LOG_VERBOSE, "Payload type: mpeg4");
                        break;
                    default:
                        LOG(LOG_VERBOSE, "Payload type: %d (name: %s)", tRtpHeader->PayloadType, PayloadIdToFfmpegName(tRtpHeader->PayloadType).c_str());
                        break;
        }
        LOG(LOG_VERBOSE, "SequenceNumber: %u", tRtpHeader->SequenceNumber);
        LOG(LOG_VERBOSE, "Time stamp: %10u", tRtpHeader->Timestamp);

    #endif

    unsigned int tCsrcCount = tRtpHeader->CsrcCount;
    if (tCsrcCount > 4)
    {
        LOG(LOG_ERROR, "Found unplausible CSRC value in RTP header");

        pIsLastFragment = false;

        // convert from host to network byte order again
        for (int i = 0; i < 3; i++)
            tRtpHeader->Data[i] = htonl(tRtpHeader->Data[i]);

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
    // #############################################################
    if ((tRtpHeader->PayloadType >= 72) && (tRtpHeader->PayloadType <= 76))
    {// rtcp intermediate packet for streaming feedback received
        RtcpHeader* tRtcpHeader = (RtcpHeader*)pData;

        #ifdef RTP_DEBUG_PACKETS
            // convert from network to host byte order
            for (int i = 0; i < 2; i++)
                tRtcpHeader->Data[i] = ntohl(tRtcpHeader->Data[i]);

            LOG(LOG_VERBOSE, "################## RTCP header ########################");
            LOG(LOG_VERBOSE, "Version: %d", tRtcpHeader->Feedback.Version);
            if (tRtcpHeader->Feedback.Padding)
                LOG(LOG_VERBOSE, "Padding: true");
            else
                LOG(LOG_VERBOSE, "Padding: false");
            switch(tRtcpHeader->Feedback.PlType)
            {
                    case 200:
                            LOG(LOG_VERBOSE, "Report type: sender report");
                            break;
                    case 201:
                            LOG(LOG_VERBOSE, "Report type: receiver report");
                            break;
                    default:
                            LOG(LOG_VERBOSE, "Report type: %d", tRtcpHeader->Feedback.PlType);
                            break;
            }
            LOG(LOG_VERBOSE, "Report length: %d", tRtcpHeader->Feedback.Length);
            LOG(LOG_VERBOSE, "Message type: %d", tRtcpHeader->Feedback.Fmt);
            LOG(LOG_VERBOSE, "SSRC: %u", tRtcpHeader->Feedback.Ssrc);

            // convert from host to network byte order again
            for (int i = 0; i < 2; i++)
                tRtcpHeader->Data[i] = htonl(tRtcpHeader->Data[i]);
        #endif

        pIsLastFragment = false;
        pIsSenderReport = true;

        // convert from host to network byte order again
        for (int i = 0; i < 3; i++)
            tRtpHeader->Data[i] = htonl(tRtpHeader->Data[i]);

        // inform that is not a fragment which includes data for an audio/video decoder, this RTCP packet belongs to the RTP abstraction level
        return false;
    }
    // #############################################################
    // HEADER: rtp => parse and update internal state information
    // #############################################################
    if (!pReadOnly)
    {
        // check if there was a packet order problem
        if ((tRtpHeader->SequenceNumber != 0 /* ignore stream resets */) && (mLastSequenceNumber != 65535) && (mLastTimestamp > 0) && (tRtpHeader->SequenceNumber < mLastSequenceNumber))
        {
            LOG(LOG_ERROR, "Packets in wrong order received (last SN: %d; current SN: %d)", mLastSequenceNumber, tRtpHeader->SequenceNumber);
        }

        // check if there was packet loss
        if ((mLastTimestamp > 0) && (((mLastSequenceNumber != 65535) && (tRtpHeader->SequenceNumber > mLastSequenceNumber + 1)) || ((mLastSequenceNumber == 65535) && (tRtpHeader->SequenceNumber != 0))))
        {
            unsigned int tLostPackets = 0;
            if(mLastTimestamp != 65535)
            {
                tLostPackets = tRtpHeader->SequenceNumber - mLastSequenceNumber - 1;
            }else
            {
                tLostPackets = tRtpHeader->SequenceNumber;
            }

            AnnounceLostPackets(tLostPackets);
            LOG(LOG_ERROR, "Packet loss detected (last SN: %d; current SN: %d), lost %u packets, overall packet loss is now %u", mLastSequenceNumber, tRtpHeader->SequenceNumber, tLostPackets, mLostPackets);
        }

        // check if there was a new frame begun before the last was finished
        if ((mFrameFragmentation) && (mLastTimestamp != tRtpHeader->Timestamp))
        {
            LOG(LOG_ERROR, "Packet belongs to new frame while last frame is incomplete");
        }

        mPayloadId = tRtpHeader->PayloadType;
        mLastSequenceNumber = tRtpHeader->SequenceNumber;
        mLastTimestamp = tRtpHeader->Timestamp;
        // if payload type = 0 or 8 then marker bit does not represent a fragmentation marker
        // if payload type = 14 then RFC 2250 defines the marker bit for discontinuous timestamps instead of fragmentation
        mFrameFragmentation = ((tRtpHeader->PayloadType == 0 /* mulaw */) || (tRtpHeader->PayloadType == 8 /* alaw */) ||  (tRtpHeader->PayloadType == 14 /* mp3 */)) ? false : !tRtpHeader->Marked;

        // store the assigned SSRC identifier
        mSsrc = tRtpHeader->Ssrc;
    }

    // convert from host to network byte order again
    for (int i = 0; i < 3; i++)
        tRtpHeader->Data[i] = htonl(tRtpHeader->Data[i]);

    // #############################################################
    // HEADER: codec headers => parse
    // #############################################################
    // get pointer to codec header buffer
    AMRNBHeader* tAMRNBHeader = (AMRNBHeader*)pData;
    MPAHeader* tMPAHeader = (MPAHeader*)pData;
    G711Header* tG711Header = (G711Header*)pData;
    H261Header* tH261Header = (H261Header*)pData;
    H263Header* tH263Header = (H263Header*)pData;
    H263PHeader* tH263PHeader = (H263PHeader*)pData;
    bool tH263PPictureStart = false;
    H264Header* tH264Header = (H264Header*)pData;
    unsigned char tH264HeaderType = 0;
    bool tH264HeaderFragmentStart = false;
    char tH264HeaderReconstructed = 0;

    switch(pCodecId)
    {
            // audio
            case CODEC_ID_PCM_ALAW:
                            #ifdef RTP_DEBUG_PACKETS
                                LOG(LOG_VERBOSE, "#################### PCMA header #######################");
                                LOG(LOG_VERBOSE, "No additional information");
                            #endif
                            // no fragmentation because our encoder sends raw data
                            mFrameFragmentation = false;
                            break;
            case CODEC_ID_PCM_MULAW:
                            #ifdef RTP_DEBUG_PACKETS
                                LOG(LOG_VERBOSE, "#################### PCMU header #######################");
                                LOG(LOG_VERBOSE, "No additional information");
                            #endif
                            // no fragmentation because our encoder sends raw data
                            mFrameFragmentation = false;
                            break;
            case CODEC_ID_PCM_S16LE:
                            #ifdef RTP_DEBUG_PACKETS
                                LOG(LOG_VERBOSE, "#################### PCM_S16LE header #######################");
                                LOG(LOG_VERBOSE, "No additional information");
                            #endif
                            // no fragmentation because our encoder sends raw data
                            mFrameFragmentation = false;
                            break;
            case CODEC_ID_AMR_NB:
                            LOG(LOG_VERBOSE, "#################### AMR-NB header #######################");
                            LOG(LOG_VERBOSE, "TODO: AMR header parsing");//TODO: implement
                            break;
            case CODEC_ID_AAC:
                            LOG(LOG_VERBOSE, "#################### AAC header #######################");
                            LOG(LOG_VERBOSE, "TODO: AAC header parsing");//TODO: implement
                            break;
            case CODEC_ID_MP3:
                            // convert from network to host byte order
                            tMPAHeader->Data[0] = ntohl(tMPAHeader->Data[0]);

                            pData += 4;

                            #ifdef RTP_DEBUG_PACKETS

                                LOG(LOG_VERBOSE, "#################### MPA header #######################");
                                LOG(LOG_VERBOSE, "Mbz bytes: %hu", tMPAHeader->Mbz);
                                LOG(LOG_VERBOSE, "Fragmentation offset: %hu", tMPAHeader->Offset);
                                if (tMPAHeader->Mbz > 0)
                                {
                                    LOG(LOG_VERBOSE, "HACK: calculated fragment size: %u", (pDataSize - (pData - tDataOriginal)));
                                    LOG(LOG_VERBOSE, "HACK: original frame size: %hu", tMPAHeader->Mbz);
                                }

                            #endif

                            // HACK: auto detect hack which marks the last fragment for us (need this because of stupid payload definition in rfc)
                            //       we do this by storing the size of the original audio packet within the MBZ value
                            if (tMPAHeader->Mbz > 0)
                            {
                                // if fragment ends at packet size or behind (to make sure we are not running into inconsistency) we should mark as complete packet
                                if ((unsigned short int)tMPAHeader->Offset + (pDataSize - (pData - tDataOriginal)) >= tMPAHeader->Mbz)
                                    mFrameFragmentation = false;
                                else
                                    mFrameFragmentation = true;
                            }

                            // convert from host to network byte order
                            tMPAHeader->Data[0] = htonl(tMPAHeader->Data[0]);


// MP3 ADU:
//                                #ifdef RTP_DEBUG_PACKETS
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
                            #ifdef RTP_DEBUG_PACKETS
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
                            // HINT: do we have RTP packets with payload id 34?
                            //       => yes: parse rtp packet according to RFC2190
                            //       => no: parse rtp packet according to RFC4629
                            // HINT: ffmpeg uses for h263 wrongly the rtp packetizing scheme for h263+
                            if (tOldH263PayloadDetected)
                            {
                                // convert from network to host byte order
                                tH263Header->Data[0] = ntohl(tH263Header->Data[0]);

                                #ifdef RTP_DEBUG_PACKETS
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

                                break;
                            }
            case CODEC_ID_H263P:
                            // convert from network to host byte order
                            tH263PHeader->Data[0] = ntohl(tH263PHeader->Data[0]);

                            #ifdef RTP_DEBUG_PACKETS
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
                                #ifdef RTP_DEBUG_PACKETS
                                    LOG(LOG_VERBOSE, "P bit is set: clear first 2 byte of frame data");
                                #endif
                                pData -= 2; // 2 bytes backward
                                //HINT: see section 6.1.1 in RFC 4629
                                if (!pReadOnly)
                                {
                                    pData[0] = 0;
                                    pData[1] = 0;
                                }
                            }
                            break;
            case CODEC_ID_H264:
                            // convert from network to host byte order
                            tH264Header->Data[0] = ntohl(tH264Header->Data[0]);

                            // HINT: convert from network to host byte order not necessary because we have only one byte
                            #ifdef RTP_DEBUG_PACKETS
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
                                                    // use FU header as NAL header, reconstruct the original NAL header
                                                    if (!pReadOnly)
                                                    {
                                                        #ifdef RTP_DEBUG_PACKETS
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
                                pData -= 3;
                                pData[0] = 0;
                                pData[1] = 0;
                                pData[2] = 1;
                            }

                            break;
            case CODEC_ID_MPEG4:
                            #ifdef RTP_DEBUG_PACKETS
                                LOG(LOG_VERBOSE, "#################### MPEG4 header #######################");
                                LOG(LOG_VERBOSE, "No additional information");
                            #endif
                            // no fragmentation because our encoder sends raw data
                            //mFrameFragmentation = false;
                            break;
            default:
                            LOG(LOG_ERROR, "Unsupported codec %d dropped by internal RTP parser", pCodecId);
                            break;
    }

    // decrease data size by the size of the found header structures
    if ((pData - tDataOriginal) > (int)pDataSize)
    {
        LOG(LOG_ERROR, "Illegal value for calculated data size (%u - %u)", pDataSize, pData - tDataOriginal);
        pIsLastFragment = true;
        return false;
    }
    pDataSize -= (pData - tDataOriginal);

    #ifdef RTP_DEBUG_PACKETS
        if (mFrameFragmentation)
            LOG(LOG_VERBOSE, "FRAGMENT");
        else
            LOG(LOG_VERBOSE, "MESSAGE COMPLETE");
    #endif

    // return if packet contains the last fragment of the current frame
    pIsLastFragment = !mFrameFragmentation;
    return true;
}

// for simple usage in SDP.C in Conference-Lib
int RTP::FfmpegNameToPayloadId(std::string pName)
{
    int tResult = 31;

    //video
    if (pName == "h261")
        tResult = 31;
    if (pName == "h263")
        tResult = 34;
    if ((pName == "h263+") || (pName == "h263p"))
        tResult = 119;
    if ((pName == "h264") || (pName == "libx264"))
        tResult = 120;
    if ((pName == "m4v") || (pName == "mpeg4"))
        tResult = 121;

    //audio
    if ((pName == "mulaw") || (pName == "pcm_mulaw"))
        tResult = 0;
    if ((pName == "gsm") || (pName == "libgsm"))
        tResult = 3;
    if ((pName == "alaw") || (pName == "pcm_alaw"))
        tResult = 8;
    if ((pName == "mp3") || (pName == "libmp3lame"))
        tResult = 14;
    if (pName == "aac")
        tResult = 100;
    if (pName == "amr")
        tResult = 101;

    LOGEX(RTP, LOG_VERBOSE, ("Translated " + pName + " to %d").c_str(), tResult);

    return tResult;
}

string RTP::PayloadIdToFfmpegName(int pId)
{
    string tResult = "unsupported";

    switch(pId)
    {
        //video
        case 31:
                tResult = "h261";
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
                tResult = "m4v";
                break;

        //audio
        case 0:
                tResult = "mulaw";
                break;
        case 3:
                tResult = "gsm";
                break;
        case 8:
                tResult = "alaw";
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
        case 96 ... 99:
                tResult = "ffmpeg dynamic";
                break;
    }

    LOGEX(RTP, LOG_VERBOSE, ("Translated %d to " + tResult).c_str(), pId);

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
