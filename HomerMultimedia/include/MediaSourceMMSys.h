/*
 * Name:    MediaSourceMMSys.h
 * Purpose: ffmpeg based local MMSys audio media source
 * Author:  Thomas Volkert
 * Since:   2010-10-29
 * Version: $Id: MediaSourceMMSys.h,v 1.10 2011/05/06 16:57:26 chaos Exp $
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
#define MEDIA_SOURCE_MMSYS_BUFFER_SIZE               1024
#define MEDIA_SOURCE_MMSYS_BUFFER_AMOUNT			   16
#define MEDIA_SOURCE_MMSYS_BUFFER_SIZE_BYTES 		MEDIA_SOURCE_MMSYS_BUFFER_SIZE * 2 /* 16 bit LittleEndian */ * 2 /* stereo */
#define MEDIA_SOURCE_MMSYS_BUFFER_QUEUE_SIZE     	  128

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
    virtual void getAudioDevices(AudioDevicesList &pAList);
    /* grabbing control */
    virtual void StopGrabbing();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);

private:
    static void EventHandler(HWAVEIN pCapturDevice, UINT pMessage, DWORD pInstance, DWORD pParam1, DWORD pParam2);

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
