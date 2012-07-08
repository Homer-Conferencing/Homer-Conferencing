/*****************************************************************************
 *
 * Copyright (C) 2010 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: ffmpeg based local MMSys audio media source
 * Author:  Thomas Volkert
 * Since:   2010-10-29
 */

#ifdef WIN32
#ifndef _MULTIMEDIA_MEDIA_SOURCE_MMSYS_
#define _MULTIMEDIA_MEDIA_SOURCE_MMSYS_

#include <MediaSource.h>
#include <Header_MMSys.h>
#include <HBCondition.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define MMSYS_DEBUG_PACKETS

// amount of samples
#define MEDIA_SOURCE_MMSYS_BUFFER_AMOUNT			   16
#define MEDIA_SOURCE_MMSYS_BUFFER_QUEUE_SIZE     	  128

///////////////////////////////////////////////////////////////////////////////

#ifndef CALLBACK
#define CALLBACK __stdcall
#endif

///////////////////////////////////////////////////////////////////////////////

struct MMSysDataChunkDesc
{
	char  *Data;
	int   Size;
};

class MediaSourceMMSys:
    public MediaSource
{
public:
	MediaSourceMMSys(std::string pDesiredDevice = "");

    virtual ~MediaSourceMMSys();

    /* device control */
    virtual void getAudioDevices(AudioDevices &pAList);

    /* recording */
    virtual bool SupportsRecording();

    /* grabbing control */
    virtual void StopGrabbing();
    virtual std::string GetCodecName();
    virtual std::string GetCodecLongName();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);

private:
    static void CALLBACK EventHandler(HWAVEIN pCapturDevice, UINT pMessage, DWORD pInstance, DWORD pParam1, DWORD pParam2);

    //HANDLE			mCaptureEvent;
    HWAVEIN      	mCaptureHandle;
    WAVEHDR			mCaptureBufferDesc[MEDIA_SOURCE_MMSYS_BUFFER_AMOUNT];
    char			*mCaptureBuffer[MEDIA_SOURCE_MMSYS_BUFFER_AMOUNT];
	int				mSampleBufferSize;
	Mutex			mMutexStateData;
	Condition 		mWaitCondition;
	MMSysDataChunkDesc mQueue[MEDIA_SOURCE_MMSYS_BUFFER_QUEUE_SIZE];
	int				mQueueWritePtr, mQueueReadPtr, mQueueSize;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
#endif
