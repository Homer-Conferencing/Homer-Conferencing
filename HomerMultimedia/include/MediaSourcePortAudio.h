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
 * Purpose: PortAudio capture for OSX
 * Author:  Thomas Volkert
 * Since:   2012-04-25
 */

#ifndef _MULTIMEDIA_MEDIA_SOURCE_PORT_AUDIO_
#define _MULTIMEDIA_MEDIA_SOURCE_PORT_AUDIO_

#include <MediaFifo.h>
#include <MediaSource.h>

#include <string.h>
struct PaStreamCallbackTimeInfo;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define MSPA_DEBUG_PACKETS
//#define MSPA_DEBUG_HANDLER

///////////////////////////////////////////////////////////////////////////////

class MediaSourcePortAudio:
    public MediaSource
{
public:
    MediaSourcePortAudio(std::string pDesiredDevice = "");

    ~MediaSourcePortAudio();

    /* device control */
    virtual void getAudioDevices(AudioDevicesList &pAList);

    /* recording */
    virtual bool SupportsRecording();

    /* grabbing control */
    virtual void StopGrabbing();
    virtual std::string GetCodecName();
    virtual std::string GetCodecLongName();

    /* global mutexing for port audio */
    // put some additional locking around portaudio's PaInit()
    static void PortAudioInit();
    // put some additional locking around portaudio's stream interface
    static void PortAudioLockStreamInterface();
    static void PortAudioUnlockStreamInterface();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropFrame = false);

private:
    static int RecordedAudioHandler(const void *pInputBuffer, void *pOutputBuffer, unsigned long pInputSize, const PaStreamCallbackTimeInfo* pTimeInfo, unsigned long pStatus, void *pUserData);
    void AssignThreadName();

    bool                mHaveToAssignThreadName;
    /* capturing */
    void                *mStream;
    MediaFifo           *mCaptureFifo;
    bool				mCaptureDuplicateMonoStream;
    /* portaudio init. */
    static Mutex        mPaInitMutex;
    static bool         mPaInitiated;
    /* stream open/close mutex */
    static Mutex        mPaStreamMutex;
};

///////////////////////////////////////////////////////////////////////////////

}} //namespaces

#endif

