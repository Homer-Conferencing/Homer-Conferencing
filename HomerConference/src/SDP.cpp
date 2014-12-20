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
 * Purpose: Implementation for session description protocol
 * Since:   2009-04-14
 */

#include <string>
#include <sstream>

#include <HBSocket.h>
#include <SDP.h>
#include <RTP.h>
#include <Logger.h>

namespace Homer { namespace Conference {

using namespace std;
using namespace Homer::Base;
using namespace Homer::Multimedia;

///////////////////////////////////////////////////////////////////////////////

SDP::SDP()
{
    mVideoCodec = 0;
    mAudioCodec = 0;
    mVideoTransportType = MEDIA_TRANSPORT_RTP_UDP;
    mAudioTransportType = MEDIA_TRANSPORT_RTP_UDP;
}

SDP::~SDP()
{
}

///////////////////////////////////////////////////////////////////////////////

void SDP::SetVideoCodec(int pSelectedCodec)
{
    LOG(LOG_VERBOSE, "Setting video codec support to: %d", pSelectedCodec);
    mVideoCodec = pSelectedCodec;
}

int SDP::GetVideoCodec()
{
    return mVideoCodec;
}

void SDP::SetAudioCodec(int pSelectedCodec)
{
    LOG(LOG_VERBOSE, "Setting audio codec support to: %d", pSelectedCodec);
    mAudioCodec = pSelectedCodec;
}

int SDP::GetAudioCodec()
{
    return mAudioCodec;
}

enum MediaTransportType SDP::CreateMediaTransportType(TransportType pSocketType, bool pRtp)
{
    switch(pSocketType)
    {
        case SOCKET_UDP:
            return pRtp ? MEDIA_TRANSPORT_RTP_UDP : MEDIA_TRANSPORT_UDP;
        case SOCKET_TCP:
            return pRtp ? MEDIA_TRANSPORT_RTP_TCP : MEDIA_TRANSPORT_TCP;
        case SOCKET_UDP_LITE:
            return pRtp ? MEDIA_TRANSPORT_RTP_UDP_LITE : MEDIA_TRANSPORT_UDP_LITE;
        default:
            break;
    }
    return MEDIA_TRANSPORT_RTP_UDP;
}

enum TransportType SDP::GetSocketTypeFromMediaTransportType(enum MediaTransportType pType)
{
    switch(pType)
    {
        case MEDIA_TRANSPORT_RTP_UDP:
        case MEDIA_TRANSPORT_UDP:
            return SOCKET_UDP;
        case MEDIA_TRANSPORT_RTP_TCP:
        case MEDIA_TRANSPORT_TCP:
            return SOCKET_TCP;
        case MEDIA_TRANSPORT_RTP_UDP_LITE:
        case MEDIA_TRANSPORT_UDP_LITE:
            return SOCKET_UDP_LITE;
        default:
            break;
    }
    return SOCKET_UDP;
}

void SDP::SetVideoTransportType(enum MediaTransportType pType)
{
    LOG(LOG_VERBOSE, "Setting video transport type to: %s", GetMediaTransportStr(pType).c_str());
    mVideoTransportType = pType;
}

void SDP::SetAudioTransportType(enum MediaTransportType pType)
{
    LOG(LOG_VERBOSE, "Setting audio transport type to: %s", GetMediaTransportStr(pType).c_str());
    mAudioTransportType = pType;
}

enum MediaTransportType SDP::GetVideoTransportType()
{
    return mVideoTransportType;
}

enum MediaTransportType SDP::GetAudioTransportType()
{
    return mAudioTransportType;
}

string SDP::GetMediaTransportStr(enum MediaTransportType pType)
{
    string tResult = "";

    //HINT: see http://www.iana.org/assignments/sdp-parameters
    //HINT: see libsofia-sip-ua/sdp/sdp_parse.c
    switch(pType)
    {
        case MEDIA_TRANSPORT_RTP_UDP:
            tResult = "RTP/AVP";
            break;
        case MEDIA_TRANSPORT_RTP_TCP:
            LOG(LOG_ERROR, "Combining RTP and TCP is not supported by sofia-sip, falling back to udp");
            tResult = "tcp"; //ignore and fallback to tcp, correct value would be "RTP/AVP-TCP"
            break;
        case MEDIA_TRANSPORT_RTP_UDP_LITE:
            LOG(LOG_ERROR, "UDP-Lite not supported by sofia-sip, falling back to udp");
            tResult = "udp"; //ignore because this is not supported by sofia-sip, correct value would be "RTP/AVP-FAST"
            break;
        case MEDIA_TRANSPORT_UDP:
            tResult = "udp";
            break;
        case MEDIA_TRANSPORT_TCP:
            tResult = "tcp";
            break;
        case MEDIA_TRANSPORT_UDP_LITE:
            LOG(LOG_ERROR, "UDP-Lite not supported by sofia-sip, falling back to udp");
            tResult = "udp"; //ignore because this is not supported by sofia-sip, correct value would be "udp-lite"
            break;
        default:
            tResult = "unknown";
            break;
    }

    return tResult;
}

int SDP::GetSDPCodecIDFromGuiName(std::string pCodecName)
{
    int tResult = 0;

    if (pCodecName == "H.261")
        tResult = CODEC_H261;
    if (pCodecName == "H.263")
        tResult = CODEC_H263;
    if (pCodecName == "H.263+")
        tResult = CODEC_H263P;
    if (pCodecName == "H.264")
        tResult = CODEC_H264;
    if ((pCodecName == "HEVC") || (pCodecName == "H.265"))
        tResult = CODEC_HEVC;
    if (pCodecName == "MPEG4")
        tResult = CODEC_MPEG4;
    if (pCodecName == "THEORA")
        tResult = CODEC_THEORA;
    if (pCodecName == "VP8")
        tResult = CODEC_VP8;

    // set settings within meeting management
    if (pCodecName == "AMR")
        tResult = CODEC_AMR_NB;
    if (pCodecName == "MP3")
        tResult = CODEC_MP3;
    if (pCodecName == "G711 A-law")
        tResult = CODEC_G711ALAW;
    if (pCodecName == "G711 µ-law")
        tResult = CODEC_G711ULAW;
    if (pCodecName == "AAC")
        tResult = CODEC_AAC;
    if (pCodecName == "PCM16")
        tResult = CODEC_PCMS16;
    if (pCodecName == "GSM")
        tResult = CODEC_GSM;
    if (pCodecName == "G722 adpcm")
        tResult = CODEC_G722ADPCM;

    return tResult;
}

/*************************************************
 *  Video codec to name mapping:
 *  ============================
 *        CODEC_H261                    h261
 *        CODEC_H263                    h263
 *        CODEC_MPEG1VIDEO              mpeg1video
 *        CODEC_MPEG2VIDEO              mpeg2video
 *        CODEC_H263P                   h263+
 *        CODEC_H264                    h264
 *        CODEC_HEVC                    hevc
 *        CODEC_MPEG4                   mpeg4
 *        CODEC_THEORA                  theora
 *        CODEC_VP8                     vp8
 *
 ****************************************************/
unsigned int SDP::GetRTPVideoPayloadID(int pCodecID)
{
    unsigned int tResult = -1;

    if (pCodecID & CODEC_H261)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("h261");
    if (pCodecID & CODEC_H263)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("h263");
    if (pCodecID & CODEC_MPEG1VIDEO)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("mpeg1video");
    if (pCodecID & CODEC_MPEG2VIDEO)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("mpeg2video");
    if (pCodecID & CODEC_H263P)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("h263+");
    if (pCodecID & CODEC_H264)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("h264");
    if (pCodecID & CODEC_HEVC)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("hevc");
    if (pCodecID & CODEC_MPEG4)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("mpeg4");
    if (pCodecID & CODEC_THEORA)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("theora");
    if (pCodecID & CODEC_VP8)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("vp8");

    return tResult;
}

/*************************************************
 *  Audio codec to name mapping:
 *  ============================
 *        CODEC_G711ULAW                ulaw
 *        CODEC_GSM                     gsm
 *        CODEC_G711ALAW                alaw
 *        CODEC_G722ADPCM               g722
 *        CODEC_PCMS16                  pcms16
 *        CODEC_MP3                     mp3
 *        CODEC_AAC                     aac
 *        CODEC_AMR_NB                  amr
 *
 ****************************************************/
unsigned int SDP::GetRTPAudioPayloadID(int pCodecID)
{
    unsigned int tResult = -1;

    if (pCodecID & CODEC_G711ULAW)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("ulaw");
    if (pCodecID & CODEC_GSM)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("gsm");
    if (pCodecID & CODEC_G711ALAW)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("alaw");
    if (pCodecID & CODEC_G722ADPCM)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("g722");
    if (pCodecID & CODEC_PCMS16)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("pcms16");
    if (pCodecID & CODEC_MP3)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("mp3");
    if (pCodecID & CODEC_AAC)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("aac");
    if (pCodecID & CODEC_AMR_NB)
        tResult = RTP::GetPreferedRTPPayloadIDForCodec("amr");

    return tResult;
}

string SDP::CreateSdpData(int pAudioPort, int pVideoPort)
{
    string tResult = "";
    unsigned int tAudioCodec = GetAudioCodec();
    unsigned int tVideoCodec = GetVideoCodec();

    LOG(LOG_VERBOSE, "Create SDP packet for audio port: %d and video port: %d", pAudioPort, pVideoPort);
    LOG(LOG_VERBOSE, "Supported audio codecs: %d and video codecs: %d", tAudioCodec, tVideoCodec);

    // calculate the new audio sdp string
    if (tAudioCodec)
    {
        // rest is filled by SIP library
        tResult += "m=audio " + toString(pAudioPort) + " " + GetMediaTransportStr(mAudioTransportType) + " " + toString(GetRTPAudioPayloadID(tAudioCodec));

        tResult += "\r\n";

        if (tAudioCodec & CODEC_G711ULAW)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("ulaw")) + " PCMU/8000/1\r\n";
        if (tAudioCodec & CODEC_GSM)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("gsm")) + " GSM/8000/1\r\n";
        if (tAudioCodec & CODEC_G711ALAW)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("alaw")) + " PCMA/8000/1\r\n";
        if (tAudioCodec & CODEC_G722ADPCM)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("g722")) + " G722/8000/1\r\n";
        if (tAudioCodec & CODEC_PCMS16)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("pcms16le")) + " L16/44100/2\r\n";
        if (tAudioCodec & CODEC_MP3)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("mp3")) + " MPA/8000/1\r\n";
        if (tAudioCodec & CODEC_AAC)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("aac")) + " AAC/8000/1\r\n";
        if (tAudioCodec & CODEC_AMR_NB)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("amr")) + " AMR/8000/1\r\n";
    }

    // calculate the new video sdp string
    if (tVideoCodec)
    {
        // rest is filled by SIP library
        tResult += "m=video " + toString(pVideoPort) + " "  + GetMediaTransportStr(mVideoTransportType) + " " + toString(GetRTPVideoPayloadID(tVideoCodec));

        tResult += "\r\n";

        if (tVideoCodec & CODEC_H261)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("h261")) + " H261/90000\r\n";
        if (tVideoCodec & CODEC_H263)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("h263")) + " H263/90000\r\n";
        if (tVideoCodec & CODEC_MPEG1VIDEO)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("mpeg1video")) + " MPV/90000\r\n";
        if (tVideoCodec & CODEC_MPEG2VIDEO)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("mpeg2video")) + " MPV/90000\r\n";
        if (tVideoCodec & CODEC_H263P)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("h263+")) + " H263-1998/90000\r\n";
        if (tVideoCodec & CODEC_H264)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("h264")) + " H264/90000\r\n";
        if (tVideoCodec & CODEC_HEVC)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("hevc")) + " HEVC/90000\r\n";
        if (tVideoCodec & CODEC_MPEG4)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("mpeg4")) + " MP4V-ES/90000\r\n";
        if (tVideoCodec & CODEC_THEORA)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("theora")) + " theora/90000\r\n";
        if (tVideoCodec & CODEC_VP8)
            tResult += "a=rtpmap:" + toString(RTP::GetPreferedRTPPayloadIDForCodec("vp8")) + " VP8/90000\r\n";
    }

    LOG(LOG_VERBOSE, "..result: %s", tResult.c_str());

    return tResult;
}
///////////////////////////////////////////////////////////////////////////////

}} //namespace
