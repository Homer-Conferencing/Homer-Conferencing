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
 * Purpose: DirectShow based local video media source
 * Author:  Thomas Volkert
 * Since:   2012-08-16
 */

#ifdef WIN32
#ifndef _MULTIMEDIA_MEDIA_SOURCE_DSHOW_
#define _MULTIMEDIA_MEDIA_SOURCE_DSHOW_

#include <MediaSource.h>
#include <string>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define MSDS_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class MediaSourceDShow:
    public MediaSource
{
public:
	MediaSourceDShow(std::string pDesiredDevice = "");

    virtual ~MediaSourceDShow();

    /* frame stats */
    virtual bool SupportsDecoderFrameStatistics();

    /* device control */
    virtual void getVideoDevices(VideoDevices &pVList);

    /* recording */
    virtual bool SupportsRecording();

    /* video grabbing control */
    virtual GrabResolutions GetSupportedVideoGrabResolutions();

    /* grabbing control */
    virtual std::string GetCodecName();
    virtual std::string GetCodecLongName();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);

private:
    /* video decoding */
    AVFrame             *mSourceFrame;
    AVFrame             *mRGBFrame;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
#endif
