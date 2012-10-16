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
 * Purpose: session description protocol
 * Author:  Thomas Volkert
 * Since:   2009-04-14
 */

#ifndef _CONFERENCE_SDP_
#define _CONFERENCE_SDP_

#include <HBSocket.h>
#include <string>

using namespace Homer::Base;

namespace Homer { namespace Conference {

///////////////////////////////////////////////////////////////////////////////

#define CODEC_G711ALAW                          1
#define CODEC_G711ULAW                          2
#define CODEC_MP3                               4
#define CODEC_AAC                               8
#define CODEC_PCMS16                            16
#define CODEC_GSM                               32
#define CODEC_AMR_NB                            64
#define CODEC_G722ADPCM                         128

#define CODEC_H261                              1
#define CODEC_H263                              2
#define CODEC_H263P                             4
#define CODEC_H264                              8
#define CODEC_MPEG1VIDEO                        16
#define CODEC_MPEG2VIDEO                        32
#define CODEC_MPEG4                             64
#define CODEC_THEORA                            128
#define CODEC_VP8                               256
#define CODEC_ALL                               0x1FF

enum MediaTransportType{
	MEDIA_TRANSPORT_UDP = 1,
	MEDIA_TRANSPORT_TCP = 2,
	MEDIA_TRANSPORT_UDP_LITE = 4,
	MEDIA_TRANSPORT_RTP_UDP = 16,
	MEDIA_TRANSPORT_RTP_TCP = 32,
	MEDIA_TRANSPORT_RTP_UDP_LITE = 64,
};

///////////////////////////////////////////////////////////////////////////////

class SDP
{
public:
    SDP();

    virtual ~SDP();

    static enum MediaTransportType CreateMediaTransportType(TransportType pSocketType, bool pRtp);
    static enum TransportType GetSocketTypeFromMediaTransportType(enum MediaTransportType pType);
    void SetVideoCodecsSupport(int pSelectedCodecs);
    int GetVideoCodecsSupport();
    void SetAudioCodecsSupport(int pSelectedCodecs);
    int GetAudioCodecsSupport();
    void SetVideoTransportType(enum MediaTransportType pType = MEDIA_TRANSPORT_RTP_UDP);
    void SetAudioTransportType(enum MediaTransportType pType = MEDIA_TRANSPORT_RTP_UDP);
    enum MediaTransportType GetVideoTransportType();
    enum MediaTransportType GetAudioTransportType();

private:
    std::string GetMediaTransportStr(enum MediaTransportType pType);

protected:
    std::string CreateSdpData(int pAudioPort, int pVideoPort);

    int             			mVideoCodecsSupport;
    int             			mAudioCodecsSupport;
    enum MediaTransportType 	mVideoTransportType;
    enum MediaTransportType 	mAudioTransportType;
};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
