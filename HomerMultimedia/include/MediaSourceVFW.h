/*
 * Name:    MediaSourceVFW.h
 * Purpose: ffmpeg based local VFW video media source
 * Author:  Thomas Volkert
 * Since:   2010-10-19
 * Version: $Id: MediaSourceVFW.h,v 1.9 2011/08/18 11:29:37 chaos Exp $
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
