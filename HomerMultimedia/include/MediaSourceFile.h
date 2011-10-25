/*
 * Name:    MediaSourceFile.h
 * Purpose: ffmpeg based local file source
 * Author:  Thomas Volkert
 * Since:   2009-02-24
 * Version: $Id: MediaSourceFile.h,v 1.25 2011/09/10 13:47:02 chaos Exp $
 */

#ifndef _MULTIMEDIA_MEDIA_SOURCE_FILE_
#define _MULTIMEDIA_MEDIA_SOURCE_FILE_

#include <Header_Ffmpeg.h>
#include <MediaSource.h>

#include <string.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MSF_DEBUG_PACKETS
//#define MSF_DEBUG_TIMING

// 33 ms delay for 30 fps -> rounded to 35 ms
#define MSF_FRAME_DROP_THRESHOLD           0 //in us, 0 deactivates frame dropping

///////////////////////////////////////////////////////////////////////////////

class MediaSourceFile:
    public MediaSource
{
public:
    MediaSourceFile(std::string pSourceFile, bool pGrabInRealTime = true /* 1 = frame rate emulation, 0 = grab as fast as possible */);

    virtual ~MediaSourceFile();

    /* video grabbing control */
    virtual GrabResolutions GetSupportedVideoGrabResolutions();
    /* pseudo device control */
    virtual void getVideoDevices(VideoDevicesList &pVList);
    virtual void getAudioDevices(AudioDevicesList &pAList);
    /* seek interface */
    virtual bool SupportsSeeking();
    virtual int64_t GetSeekEnd(); // get maximum seek time in seconds
    virtual bool Seek(int64_t pSeconds, bool pOnlyKeyFrames = true); // seek to absolute position which is given in seconds
    virtual bool SeekRelative(int64_t pSeconds, bool pOnlyKeyFrames = true); // seeks relative to the current position, distance is given in seconds
    virtual int64_t GetSeekPos(); // in seconds

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);

protected:
    /* internal video resolution switch */
    virtual void DoSetVideoGrabResolution(int pResX = 352, int pResY = 288);

private:
    bool                mGrabInRealTime;
    int64_t             mCurPts; // we have to determine this manually during grabbing because cur_dts and everything else in AVStream is buggy for some video/audio files
    bool                mSeekingToPos; // seek to starting point because initial stream detection consumes the first n frames, or seeking to explicit position ("Seek" was called)
};

///////////////////////////////////////////////////////////////////////////////

}} // namepsaces

#endif
