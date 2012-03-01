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
 * Purpose: media source multiplexer which grabs frames from registered media sources, transcodes them and distributes all of them to the registered media sinks
 * Author:  Thomas Volkert
 * Since:   2009-01-04
 */

#ifndef _MULTIMEDIA_MEDIA_SOURCE_MUXER_
#define _MULTIMEDIA_MEDIA_SOURCE_MUXER_

#include <Header_Ffmpeg.h>
#include <HBMutex.h>
#include <HBThread.h>
#include <MediaSourceNet.h>
#include <MediaSource.h>
#include <MediaFifo.h>
#include <RTP.h>

#include <list>
#include <string>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// maximum packet size of a reeencoded frame
#define MEDIA_SOURCE_MUX_STREAM_PACKET_BUFFER_SIZE              256*1024

#define MEDIA_SOURCE_MUX_INPUT_QUEUE_SIZE_LIMIT                  32

// the following de/activates debugging of sent packets
//#define MSM_DEBUG_PACKETS

///////////////////////////////////////////////////////////////////////////////

class MediaSourceMuxer :
    public MediaSource, public Thread
{
public:
    /// The default constructor
    MediaSourceMuxer(MediaSource *pMediaSource = NULL);

    /// The destructor.
    virtual ~MediaSourceMuxer();

    /* multiplexing */
    virtual bool SupportsMuxing();
    virtual std::string GetMuxingCodec();
    virtual void GetMuxingResolution(int &pResX, int &pResY);

    /* relaying */
    virtual bool SupportsRelaying();

    /* get access to current basic media source */
    virtual MediaSource* GetMediaSource();

    /* streaming control */
    bool SetOutputStreamPreferences(std::string pStreamCodec, int pMediaStreamQuality, int pMaxPacketSize = 1300 /* works only with RTP packetizing */, bool pDoReset = false, int pResX = 352, int pResY = 288, bool pRtpActivated = true, int pMaxFps = 0);
    enum CodecID GetStreamCodecId() { return mStreamCodecId; } // used in RTSPListenerMediaSession

    /* video grabbing control */
    virtual void SetVideoGrabResolution(int pTargetResX, int pTargetResY);
    virtual GrabResolutions GetSupportedVideoGrabResolutions();
    virtual void SetVideoFlipping(bool pHFlip, bool pVFlip);
    virtual void GetVideoSourceResolution(int &pResX, int &pResY);

    /* grabbing control */
    virtual void StopGrabbing();
    virtual bool Reset(enum MediaType = MEDIA_UNKNOWN);
    virtual std::string GetCodecName();
    virtual std::string GetCodecLongName();
    virtual int GetChunkDropConter();

    /* recording control */
    virtual bool StartRecording(std::string pSaveFileName, int pSaveFileQuality = 10, bool pRealTime = true /* 1 = frame rate emulation, 0 = no pts adaption */);
    virtual void StopRecording();
    virtual bool SupportsRecording();
    virtual bool IsRecording();

    /* activation control */
    void SetActivation(bool pState);

    /* device control */
    virtual void getVideoDevices(VideoDevicesList &pVList);
    virtual void getAudioDevices(AudioDevicesList &pAList);
    virtual bool SelectDevice(std::string pDesiredDevice, enum MediaType pMediaType, bool &pIsNewDevice); // returns if the new device should be reseted
    virtual std::string GetCurrentDeviceName();
    virtual bool RegisterMediaSource(MediaSource *pMediaSource);
    virtual bool UnregisterMediaSource(MediaSource *pMediaSource, bool pAutoDelete = true);

    /* fps */
    virtual float GetFrameRate();
    virtual void SetFrameRate(float pFps);

    /* sample rate */
    virtual int GetSampleRate();

    /* seek interface */
    virtual bool SupportsSeeking();
    virtual int64_t GetSeekEnd(); // get maximum seek time in seconds
    virtual bool Seek(int64_t pSeconds, bool pOnlyKeyFrames = true);
    virtual bool SeekRelative(int64_t pSeconds, bool pOnlyKeyFrames = true);
    virtual int64_t GetSeekPos(); // in seconds

    /* multi input interface */
    virtual bool SupportsMultipleInputChannels();
    virtual bool SelectInputChannel(int pIndex);
    virtual std::string CurrentInputChannel();
    virtual std::list<std::string> GetInputChannels();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, bool pStereo = true);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);
    virtual void* AllocChunkBuffer(int& pChunkBufferSize, enum MediaType pMediaType = MEDIA_UNKNOWN);
    virtual void FreeChunkBuffer(void *pChunk);

private:
    bool OpenVideoMuxer(int pResX = 352, int pResY = 288, float pFps = 29.97);
    bool OpenAudioMuxer(int pSampleRate = 44100, bool pStereo = true);
    bool CloseMuxer();

    /* FPS limitation */
    bool BelowMaxFps(int pFrameNumber);

    /* transcoder */
    virtual void* Run(void* pArgs = NULL); // transcoder main loop
    void InitTranscoder(int pFifoEntrySize);
    void DeinitTranscoder();

    static int DistributePacket(void *pOpaque, uint8_t *pBuffer, int pBufferSize);

    MediaSource         *mMediaSource;
    enum CodecID        mStreamCodecId;
    int                 mStreamMaxPacketSize;
    int                 mStreamQuality;
    int 				mStreamMaxFps;
    int64_t				mStreamMaxFps_LastFrame_Timestamp;
    bool                mStreamActivated;
    char                *mStreamPacketBuffer;
    char                *mEncoderChunkBuffer;
    /* transcoding */
    bool                mTranscoderNeeded;
    MediaFifo           *mTranscoderFifo;
    bool				mTranscoderHasKeyFrame;
    /* device control */
    MediaSourcesList    mMediaSources;
    Mutex               mMediaSourcesMutex;
    /* video */
    int                 mCurrentStreamingResX, mRequestedStreamingResX;
    int                 mCurrentStreamingResY, mRequestedStreamingResY;
    bool                mVideoHFlip, mVideoVFlip;
    /* audio */
    AVFifoBuffer        *mSampleFifo;
    char                *mSamplesTempBuffer;

};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
