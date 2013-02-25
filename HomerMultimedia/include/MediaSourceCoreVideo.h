/*****************************************************************************
 *
 * Copyright (C) 2011 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: CoreVideo capture for OSX
 * Author:  Thomas Volkert
 * Since:   2011-11-16
 */

#if defined(APPLE)
#ifndef _MULTIMEDIA_MEDIA_SOURCE_CORE_VIDEO_
#define _MULTIMEDIA_MEDIA_SOURCE_CORE_VIDEO_

#include <MediaSource.h>
#include <Header_CoreVideo.h>

#include <string.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define MSCV_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class MediaSourceCoreVideo:
    public MediaSource
{
public:
    MediaSourceCoreVideo(std::string pDesiredDevice = "");

    ~MediaSourceCoreVideo();

    /* device control */
    virtual void getVideoDevices(VideoDevices &pVList);

    /* recording */
    virtual bool SupportsRecording();

    /* video grabbing control */
    //virtual GrabResolutions GetSupportedVideoGrabResolutions();

    /* grabbing control */
    virtual std::string GetSourceCodecStr();
    virtual std::string GetSourceCodecDescription();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, int pChannels = 2);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropFrame = false);

private:
//    int                 mSampleBufferSize;
};

///////////////////////////////////////////////////////////////////////////////

}} //namespaces

#endif
#endif
