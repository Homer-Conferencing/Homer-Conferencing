/*****************************************************************************
 *
 * Copyright (C) 2008 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: ffmpeg based local v4l2 video media source
 * Author:  Thomas Volkert
 * Since:   2008-12-01
 */

#if defined(LINUX)
#ifndef _MULTIMEDIA_MEDIA_SOURCE_V4L2_
#define _MULTIMEDIA_MEDIA_SOURCE_V4L2_

#include <Header_Ffmpeg.h>
#include <MediaSource.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define MSV_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class MediaSourceV4L2:
    public MediaSource
{
public:
    MediaSourceV4L2(std::string pDesiredDevice = "");

    virtual ~MediaSourceV4L2();

    /* video grabbing control */
    virtual GrabResolutions GetSupportedVideoGrabResolutions();

    /* grabbing control */
    virtual std::string GetCodecName();
    virtual std::string GetCodecLongName();

    /* device control */
    virtual void getVideoDevices(VideoDevicesList &pVList);
    virtual bool SelectDevice(std::string pDeviceName, enum MediaType pMediaType, bool &pIsNewDevice);

    /* recording */
    virtual bool SupportsRecording();

    /* multi input interface */
    virtual bool SupportsMultipleInputChannels();
    virtual bool SelectInputChannel(int pIndex);
    virtual std::string CurrentInputChannel();
    virtual std::list<std::string> GetInputChannels();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 30);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);

private:
    std::string         mCurrentInputChannelName;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
#endif
