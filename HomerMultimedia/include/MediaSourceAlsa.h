/*
 * Name:    MediaSourceAlsa.h
 * Purpose: Alsa Audio Capture
 * Author:  Stefan Kögel, Thomas Volkert
 * Since:   2009-05-18
 * Version: $Id: MediaSourceAlsa.h,v 1.8 2011/02/19 22:11:24 chaos Exp $
 */

#ifdef LINUX
#ifndef _MULTIMEDIA_MEDIA_SOURCE_ALSA_
#define _MULTIMEDIA_MEDIA_SOURCE_ALSA_

#include <MediaSource.h>
#include <Header_Alsa.h>

#include <string.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// de/activate debugging of grabbed packets
//#define MSA_DEBUG_PACKETS

// amount of samples
#define MEDIA_SOURCE_ALSA_BUFFER_SIZE               1024

///////////////////////////////////////////////////////////////////////////////

class MediaSourceAlsa:
    public MediaSource
{
public:
    MediaSourceAlsa(std::string pDesiredDevice = "");

    ~MediaSourceAlsa();

    /* device control */
    virtual void getAudioDevices(AudioDevicesList &pAList);
    /* grabbing control */
    virtual void StopGrabbing();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
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
