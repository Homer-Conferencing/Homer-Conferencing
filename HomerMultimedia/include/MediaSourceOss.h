/*
 * Name:    MediaSourceOss.h
 * Purpose: ffmpeg based local oss audio source
 * Author:  Thomas Volkert
 * Since:   2009-02-11
 * Version: $Id: MediaSourceOss.h,v 1.5 2011/02/19 22:11:24 chaos Exp $
 */

#ifdef LINUX
#ifndef _MULTIMEDIA_MEDIA_SOURCE_OSS_
#define _MULTIMEDIA_MEDIA_SOURCE_OSS_

#include <Header_Ffmpeg.h>
#include <MediaSource.h>

#include <string.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define MSO_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class MediaSourceOss:
    public MediaSource
{
public:
    MediaSourceOss(std::string pDesiredDevice = "");

    virtual ~MediaSourceOss();

    virtual void getAudioDevices(AudioDevicesList &pAList);

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);
};

///////////////////////////////////////////////////////////////////////////////

}} // namespaces

#endif
#endif
