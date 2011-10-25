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
 * Name:    SDP.cpp
 * Purpose: Implementation for session description protocol
 * Author:  Thomas Volkert
 * Since:   2009-04-14
 * Version: $Id: SDP.cpp,v 1.18 2011/08/03 20:10:29 chaos Exp $
 */

#include <string>
#include <sstream>

#include <HBSocket.h>
#include <SDP.h>
#include <Logger.h>

namespace Homer { namespace Conference {

using namespace std;
using namespace Homer::Base;

///////////////////////////////////////////////////////////////////////////////

SDP::SDP()
{
    mVideoCodecsSupport = 0;
    mAudioCodecsSupport = 0;
    mVideoTransportType = MEDIA_TRANSPORT_UDP;
    mAudioTransportType = MEDIA_TRANSPORT_UDP;
}

SDP::~SDP()
{
}

///////////////////////////////////////////////////////////////////////////////

void SDP::SetVideoCodecsSupport(int pSelectedCodecs)
{
    mVideoCodecsSupport = pSelectedCodecs;
}

int SDP::GetVideoCodecsSupport()
{
    return mVideoCodecsSupport;
}

void SDP::SetAudioCodecsSupport(int pSelectedCodecs)
{
    mAudioCodecsSupport = pSelectedCodecs;
}

int SDP::GetAudioCodecsSupport()
{
    return mAudioCodecsSupport;
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
            LOG(LOG_ERROR, "UDPlite not supported by sofia-sip, falling back to udp");
            tResult = "udp"; //ignore because this is not supported by sofia-sip, correct value would be "RTP/AVP-FAST"
            break;
        case MEDIA_TRANSPORT_UDP:
            tResult = "udp";
            break;
        case MEDIA_TRANSPORT_TCP:
            tResult = "tcp";
            break;
        case MEDIA_TRANSPORT_UDP_LITE:
            LOG(LOG_ERROR, "UDPlite not supported by sofia-sip, falling back to udp");
            tResult = "udp"; //ignore because this is not supported by sofia-sip, correct value would be "udp-lite"
            break;
        default:
            tResult = "unknown";
            break;
    }

    return tResult;
}

int SDP::CodecNameToPayloadId(std::string pName)
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

    LOGEX(SDP, LOG_VERBOSE, ("Translated " + pName + " to %d").c_str(), tResult);

    return tResult;
}

string SDP::createSdpString(int pAudioPort, int pVideoPort)
{
    string tResult = "";
    unsigned int tSupportedAudioCodecs = GetAudioCodecsSupport();
    unsigned int tSupportedVideoCodecs = GetVideoCodecsSupport();

    LOG(LOG_VERBOSE, "Create SDP packet for audio port: %d and video port: %d", pAudioPort, pVideoPort);
    LOG(LOG_VERBOSE, "Supported audio codecs: %d and video codecs: %d", tSupportedAudioCodecs, tSupportedVideoCodecs);

    // calculate the new audio sdp string
    if (tSupportedAudioCodecs)
    {
        // rest is filled by SIP library
        tResult += "m=audio " + toString(pAudioPort) + " " + GetMediaTransportStr(mAudioTransportType);

        if (tSupportedAudioCodecs & CODEC_MP3)
            tResult += " " + toString(CodecNameToPayloadId("mp3"));
        if (tSupportedAudioCodecs & CODEC_G711A)
            tResult += " " + toString(CodecNameToPayloadId("alaw"));
        if (tSupportedAudioCodecs & CODEC_G711U)
            tResult += " " + toString(CodecNameToPayloadId("mulaw"));
        if (tSupportedAudioCodecs & CODEC_AAC)
            tResult += " " + toString(CodecNameToPayloadId("aac"));
        if (tSupportedAudioCodecs & CODEC_PCMS16LE)
            tResult += " " + toString(CodecNameToPayloadId("pcms16le"));
        if (tSupportedAudioCodecs & CODEC_GSM)
            tResult += " " + toString(CodecNameToPayloadId("gsm"));
        if (tSupportedAudioCodecs & CODEC_AMR)
            tResult += " " + toString(CodecNameToPayloadId("amr"));

        tResult += "\r\n";

        if (tSupportedAudioCodecs & CODEC_G711U)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("mulaw")) + " PCMU/8000/1\r\n";
        if (tSupportedAudioCodecs & CODEC_G711A)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("alaw")) + " PCMA/8000/1\r\n";
        if (tSupportedAudioCodecs & CODEC_MP3)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("mp3")) + " MPA/8000/1\r\n";
        if (tSupportedAudioCodecs & CODEC_AAC)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("aac")) + " AAC/8000/1\r\n";
        if (tSupportedAudioCodecs & CODEC_PCMS16LE)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("pcms16le")) + " PCMS16/8000/1\r\n";
        if (tSupportedAudioCodecs & CODEC_GSM)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("gsm")) + " GSM/8000/1\r\n";
        if (tSupportedAudioCodecs & CODEC_AMR)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("amr")) + " AMR/8000/1\r\n";
    }

    // calculate the new video sdp string
    if (tSupportedVideoCodecs)
    {
        // rest is filled by SIP library
        tResult += "m=video " + toString(pVideoPort) + " "  + GetMediaTransportStr(mVideoTransportType);

        if (tSupportedVideoCodecs & CODEC_MPEG4)
            tResult += " " + toString(CodecNameToPayloadId("m4v"));
        if (tSupportedVideoCodecs & CODEC_H264)
            tResult += " " + toString(CodecNameToPayloadId("h264"));
        if (tSupportedVideoCodecs & CODEC_H263P)
            tResult += " " + toString(CodecNameToPayloadId("h263+"));
        if (tSupportedVideoCodecs & CODEC_H263)
            tResult += " " + toString(CodecNameToPayloadId("h263"));
        if (tSupportedVideoCodecs & CODEC_H261)
            tResult += " " + toString(CodecNameToPayloadId("h261"));

        tResult += "\r\n";

        if (tSupportedVideoCodecs & CODEC_MPEG4)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("m4v")) + " MP4V-ES/90000\r\n";
        if (tSupportedVideoCodecs & CODEC_H264)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("h264")) + " h264/90000\r\n";
        if (tSupportedVideoCodecs & CODEC_H263P)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("h263+")) + " h263-1998/90000\r\n";
        if (tSupportedVideoCodecs & CODEC_H263)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("h263")) + " h263/90000\r\n";
        if (tSupportedVideoCodecs & CODEC_H261)
            tResult += "a=rtpmap:" + toString(CodecNameToPayloadId("h261")) + " h261/90000\r\n";
    }

    LOG(LOG_VERBOSE, "..result: %s", tResult.c_str());

    return tResult;
}

///////////////////////////////////////////////////////////////////////////////

}} //namespace
