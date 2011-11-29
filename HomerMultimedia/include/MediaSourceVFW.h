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
 * Purpose: ffmpeg based local VFW video media source
 * Author:  Thomas Volkert
 * Since:   2010-10-19
 */

#ifdef WIN32
#ifndef _MULTIMEDIA_MEDIA_SOURCE_VFW_
#define _MULTIMEDIA_MEDIA_SOURCE_VFW_

#include <Header_Ffmpeg.h>
#include <MediaSource.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define MSVFW_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class MediaSourceVFW:
    public MediaSource
{
public:
    MediaSourceVFW(std::string pDesiredDevice = "");

    virtual ~MediaSourceVFW();

    /* device control */
    virtual void getVideoDevices(VideoDevicesList &pVList);

    /* grabbing control */
    virtual std::string GetCodecName();
    virtual std::string GetCodecLongName();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);

private:
    bool				mFramesAreUpsideDown[10];
    bool				mDeviceAvailable[10];
    int                 mCurrentInputChannel, mDesiredInputChannel;
	bool				mFirstPixelformatError;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
#endif
