/*****************************************************************************
 *
 * Copyright (C) 2009 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: ffmpeg based local file source
 * Author:  Thomas Volkert
 * Since:   2009-02-24
 */

#ifndef _MULTIMEDIA_MEDIA_SOURCE_FILE_
#define _MULTIMEDIA_MEDIA_SOURCE_FILE_

#include <Header_Ffmpeg.h>
#include <MediaSource.h>
#include <MediaFifo.h>
#include <HBThread.h>

#include <string.h>

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of received packets
//#define MSF_DEBUG_PACKETS
//#define MSF_DEBUG_TIMING

#define MEDIA_SOURCE_FILE_INPUT_QUEUE_SIZE_LIMIT                  5 // in frames or audio sample blocks

// 33 ms delay for 30 fps -> rounded to 35 ms
#define MSF_FRAME_DROP_THRESHOLD            0 //in us, 0 deactivates frame dropping

#define MSF_SEEK_VARIANCE              (10 * AV_TIME_BASE)
///////////////////////////////////////////////////////////////////////////////

class MediaSourceFile:
    public MediaSource, public Thread
{
public:
    MediaSourceFile(std::string pSourceFile, bool pGrabInRealTime = true /* 1 = frame rate emulation, 0 = grab as fast as possible */);

    virtual ~MediaSourceFile();

    /* video grabbing control */
    virtual GrabResolutions GetSupportedVideoGrabResolutions();

    /* pseudo device control */
    virtual void getVideoDevices(VideoDevicesList &pVList);
    virtual void getAudioDevices(AudioDevicesList &pAList);

    /* fps */
    virtual void SetFrameRate(float pFps);

    /* recording */
    virtual bool SupportsRecording();

    /* seek interface */
    virtual bool SupportsSeeking();
    virtual int64_t GetSeekEnd(); // get maximum seek time in seconds
    virtual bool Seek(int64_t pSeconds, bool pOnlyKeyFrames = true); // seek to absolute position which is given in seconds
    virtual bool SeekRelative(int64_t pSeconds, bool pOnlyKeyFrames = true); // seeks relative to the current position, distance is given in seconds
    virtual int64_t GetSeekPos(); // in seconds

    /* multi channel input interface */
    virtual bool SupportsMultipleInputChannels();
    virtual bool SelectInputChannel(int pIndex);
    virtual std::string CurrentInputChannel();
    virtual std::list<std::string> GetInputChannels();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);

protected:
    /* internal video resolution switch */
    virtual void DoSetVideoGrabResolution(int pResX = 352, int pResY = 288);

private:
    /* decoder */
    virtual void* Run(void* pArgs = NULL); // transcoder main loop
    void StartDecoder(int pFifoEntrySize);
    void StopDecoder();

    /* decoding */
    int                 mDecoderTargetResX;
    int                 mDecoderTargetResY;
    Mutex               mDecoderMutex;
    bool                mDecoderNeeded;
    MediaFifo           *mDecoderFifo;
    MediaFifo           *mDecoderMetaDataFifo;
    bool                mEOFReached;
    /* */
    bool                mGrabInRealTime;
    int64_t             mCurPts; // we have to determine this manually during grabbing because cur_dts and everything else in AVStream is buggy for some video/audio files
    bool                mSeekingToPos; // seek to starting point because initial stream detection consumes the first n frames, or seeking to explicit position ("Seek" was called)
    char                *mResampleBuffer;
    ReSampleContext     *mResampleContext;
    list<string>        mInputChannels;
};

///////////////////////////////////////////////////////////////////////////////

}} // namepsaces

#endif
