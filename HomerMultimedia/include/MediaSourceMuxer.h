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

#include <vector>
#include <string>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of sent packets
//#define MSM_DEBUG_PACKETS
//#define MSM_DEBUG_PACKET_DISTRIBUTION
//#define MSM_DEBUG_GRABBING
//#define MSM_DEBUG_TIMING

///////////////////////////////////////////////////////////////////////////////

// amount of entries within the input FIFO
#define MEDIA_SOURCE_MUX_INPUT_QUEUE_SIZE_LIMIT                  32

///////////////////////////////////////////////////////////////////////////////

class MediaSourceMuxer:
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
    virtual int GetMuxingBufferCounter();
    virtual int GetMuxingBufferSize();

    /* audio */
    virtual int GetOutputSampleRate();
    virtual int GetOutputChannels();
    virtual int GetInputSampleRate();
    virtual int GetInputChannels();
    virtual std::string GetInputFormatStr();

    /* video */
    virtual float GetInputFrameRate();
    virtual void SetFrameRate(float pFps);

    /* audio/video */
    virtual int GetInputBitRate();

    /* A/V sync. */
    virtual int64_t GetSynchronizationTimestamp();
    virtual int GetSynchronizationPoints(); // how many synchronization points for deriving synchronization timestamp were included in the input stream till now?
    virtual bool TimeShift(int64_t pOffset); // in us

    /* relaying */
    virtual bool SupportsRelaying();
    virtual int GetEncoderBufferedFrames();
    void SetRelayActivation(bool pState);
    void SetRelaySkipSilence(bool pState);
    void SetRelaySkipSilenceThreshold(int pValue);
    int GetRelaySkipSilenceThreshold();
    int GetRelaySkipSilenceSkippedChunks();

    /* get access to current basic media source */
    virtual MediaSource* GetMediaSource();

    /* streaming control */
    static bool IsOutputCodecSupported(std::string pStreamCodec);
    bool SetOutputStreamPreferences(std::string pStreamCodec, int pMediaStreamQuality, int pBitRate, int pMaxPacketSize = 1300 /* works only with RTP packetizing */, bool pDoReset = false, int pResX = 352, int pResY = 288, int pMaxFps = 0);
    enum AVCodecID GetStreamCodecId() { return mStreamCodecId; } // used in RTSPListenerMediaSession

    /* frame stats */
    virtual bool SupportsDecoderFrameStatistics();
    virtual int64_t DecodedIFrames();
    virtual int64_t DecodedPFrames();
    virtual int64_t DecodedBFrames();
    virtual int64_t DecodedSFrames();
    virtual int64_t DecodedSIFrames();
    virtual int64_t DecodedSPFrames();
    virtual int64_t DecodedBIFrames();

    /* end-to-end delay */
    virtual int64_t GetEndToEndDelay();

    /* video grabbing control */
    virtual void SetVideoGrabResolution(int pTargetResX, int pTargetResY);
    virtual void GetVideoGrabResolution(int &pResX, int &pResY);
    virtual GrabResolutions GetSupportedVideoGrabResolutions();
    virtual void GetVideoSourceResolution(int &pResX, int &pResY);
    virtual void SetVideoFlipping(bool pHFlip, bool pVFlip);
    virtual bool HasVariableOutputFrameRate();
    virtual bool IsSeeking();

    /* grabbing control */
    virtual void StopGrabbing();
    virtual bool IsGrabbingStopped();
    virtual bool Reset(enum MediaType = MEDIA_UNKNOWN);
    virtual enum AVCodecID GetSourceCodec();
    virtual std::string GetSourceCodecStr();
    virtual std::string GetSourceCodecDescription();
    virtual bool SetInputStreamPreferences(std::string pStreamCodec, bool pRtpActivated = false, bool pDoReset = false);
    virtual int GetChunkDropCounter();
    virtual int GetChunkBufferCounter();

    /* frame (pre)buffering */
    virtual float GetFrameBufferPreBufferingTime();
    virtual void SetFrameBufferPreBufferingTime(float pTime);
    virtual float GetFrameBufferTime();
    virtual int GetFrameBufferCounter();
    virtual int GetFrameBufferSize();
    virtual void SetPreBufferingActivation(bool pActive);
    virtual void SetPreBufferingAutoRestartActivation(bool pActive);
    virtual int GetDecoderOutputFrameDelay();

    /* recording control */
    virtual bool StartRecording(std::string pSaveFileName, int pSaveFileQuality = 10, bool pRealTime = true /* 1 = frame rate emulation, 0 = no pts adaption */);
    virtual void StopRecording();
    virtual bool SupportsRecording();
    virtual bool IsRecording();
    virtual int64_t RecordingTime(); // in seconds

    /* device control */
    virtual std::string GetSourceTypeStr();
    virtual enum SourceType GetSourceType();
    virtual void getVideoDevices(VideoDevices &pVList);
    virtual void getAudioDevices(AudioDevices &pAList);
    virtual bool SelectDevice(std::string pDesiredDevice, enum MediaType pMediaType, bool &pIsNewDevice); // returns if the new device should be reseted
    virtual std::string GetCurrentDeviceName();
    virtual std::string GetCurrentDevicePeerName();
    virtual void RegisterMediaFilter(MediaFilter *pMediaFilter);
    virtual bool UnregisterMediaFilter(MediaFilter *pMediaFilter, bool pAutoDelete = true);
    virtual bool RegisterMediaSource(MediaSource *pMediaSource);
    virtual bool UnregisterMediaSource(MediaSource *pMediaSource, bool pAutoDelete = true);

    /* seek interface */
    virtual bool SupportsSeeking();
    virtual float GetSeekEnd(); // get maximum seek time in seconds
    virtual bool Seek(float pSeconds, bool pOnlyKeyFrames = true);
    virtual float GetSeekPos(); // in seconds

    /* multi input interface */
    virtual bool SupportsMultipleInputStreams();
    virtual bool SelectInputStream(int pIndex);
    virtual std::string CurrentInputStream();
    virtual std::vector<std::string> GetInputStreams();
    virtual bool HasInputStreamChanged();

    /* live OSD marking */
    virtual bool SupportsMarking();

    /* meta data*/
    virtual MetaData GetMetaData();

public:
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97);
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, int pChannels = 2);
    virtual bool CloseGrabDevice();
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false);
    virtual void* AllocChunkBuffer(int& pChunkBufferSize, enum MediaType pMediaType = MEDIA_UNKNOWN);
    virtual void FreeChunkBuffer(void *pChunk);
    virtual void FreeUnusedRegisteredFileSources();

private:
    /* video resolution limitation depending on video codec capabilities */
    void ValidateVideoResolutionForEncoderCodec(int &pResX, int &pResY, enum AVCodecID pCodec);

    bool OpenVideoMuxer(int pResX = 352, int pResY = 288, float pFps = 29.97);
    bool OpenAudioMuxer(int pSampleRate = 44100, int pChannels = 2);
    bool CloseMuxer();

    /* FPS limitation */
    bool BelowMaxFps(int pFrameNumber);
    int64_t CalculateEncoderPts(int pFrameNumber);

    /* transcoder */
    virtual void* Run(void* pArgs = NULL); // transcoder main loop
    void StartEncoder();
    void StopEncoder();

    void ResetEncoderBuffers();

    static int DistributePacket(void *pOpaque, uint8_t *pBuffer, int pBufferSize);

    MediaSource         *mMediaSource;
    enum AVCodecID      mStreamCodecId;
    int                 mStreamMaxPacketSize;
    int                 mStreamQuality;
    int                 mStreamBitRate;
    int 				mStreamMaxFps;
    int64_t				mStreamMaxFps_LastFrame_Timestamp;
    bool                mStreamActivated;
    char                *mStreamPacketBuffer;
    /* relaying: skip audio silence */
    bool				mRelayingSkipAudioSilence;
    int64_t				mRelayingSkipAudioSilenceSkippedChunks;
    /* encoding */
    Mutex               mEncoderSeekMutex;
    char                *mEncoderChunkBuffer;
    bool                mEncoderThreadNeeded;
    MediaFifo           *mEncoderFifo;
    Mutex				mEncoderFifoState;
    bool				mEncoderHasKeyFrame;
    int64_t             mEncoderPacketTimestamp; // timestamp of the currently processed packet
    Mutex               mEncoderFifoAvailableMutex;
    AVStream            *mEncoderStream;
    int					mEncoderBufferedFrames; // in frames
    int64_t				mEncoderStartTime;
    /* device control */
    MediaSources        mMediaSources;
    Mutex               mMediaSourcesMutex;
    /* video */
    int                 mCurrentStreamingResX, mRequestedStreamingResX;
    int                 mCurrentStreamingResY, mRequestedStreamingResY;
    bool                mVideoHFlip, mVideoVFlip;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
