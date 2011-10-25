/*
 * Name:    MediaSourceV4L2.h
 * Purpose: ffmpeg based local v4l2 video media source
 * Author:  Thomas Volkert
 * Since:   2008-12-01
 * Version: $Id: MediaSourceV4L2.h,v 1.13 2011/08/12 08:39:58 chaos Exp $
 */

#ifdef LINUX
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
    /* device control */
    virtual void getVideoDevices(VideoDevicesList &pVList);
    virtual bool SelectDevice(std::string pDeviceName, enum MediaType pMediaType, bool &pIsNewDevice);
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
    int                 mCurrentInputChannel, mDesiredInputChannel;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
#endif
