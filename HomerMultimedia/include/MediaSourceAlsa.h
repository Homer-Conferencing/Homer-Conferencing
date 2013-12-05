/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
 * Copyright (C) 2009-2009 Stefan Koegel
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
 * Purpose: Alsa Audio Capture
 * Since:   2009-05-18
 */

#if defined(LINUX)
#ifndef _MULTIMEDIA_MEDIA_SOURCE_ALSA_
#define _MULTIMEDIA_MEDIA_SOURCE_ALSA_

#include <MediaSource.h>
#include <Header_Alsa.h>

#include <string.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define MSA_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class MediaSourceAlsa:
    public MediaSource
{
public:
    MediaSourceAlsa(std::string pDesiredDevice = "");

    ~MediaSourceAlsa();

    /* device control */
    virtual void getAudioDevices(AudioDevices &pAList);

    /* recording */
    virtual bool SupportsRecording();

    /* grabbing control */
    virtual void StopGrabbing();
    virtual std::string GetSourceCodecStr();
    virtual std::string GetSourceCodecDescription();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, int pChannels = 2);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropFrame = false);

private:
    snd_pcm_t           *mCaptureHandle;
    int                 mSampleBufferSize;
};

///////////////////////////////////////////////////////////////////////////////

}} //namespaces

#endif
#endif
