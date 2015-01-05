/*****************************************************************************
 *
 * Copyright (C) 2008 Thomas Volkert <thomas@homer-conferencing.com>
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
 * Purpose: abstract media source
 * Since:   2008-12-02
 */

#ifndef _MULTIMEDIA_MEDIA_SOURCE_
#define _MULTIMEDIA_MEDIA_SOURCE_

#include <Header_Ffmpeg.h>
#include <PacketStatistic.h>
#include <MediaSinkNet.h>
#include <MediaSinkFile.h>
#include <MediaSink.h>
#include <MediaFilter.h>
#include <HBMutex.h>

#include <vector>
#include <string>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of packets
//#define MS_DEBUG_PACKETS
//#define MS_DEBUG_ENCODER_PACKETS
// the following de/activates debugging of ffmpag mutex management
//#define MS_DEBUG_FFMPEG_MUTEX

//#define MS_DEBUG_RECORDER_PACKETS
//#define MS_DEBUG_RECORDER_FRAMES

///////////////////////////////////////////////////////////////////////////////

// video/audio processing
#define MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE                         16 * 1000 * 1000 // HDTV RGB32 picture: 1920*1080*4 = ca. 7,9 MB

// audio processing
#define MEDIA_SOURCE_MAX_AUDIO_CHANNELS                           32
#define MEDIA_SOURCE_SAMPLES_CAPTURE_FIFO_SIZE                    64 // amount of capture buffers within the FIFO
#define MEDIA_SOURCE_SAMPLES_PLAYBACK_FIFO_SIZE                   64 // amount of playback buffers within the FIFO
#define MEDIA_SOURCE_SAMPLES_PER_BUFFER                           1024
#define MEDIA_SOURCE_SAMPLES_BUFFER_SIZE                          (MEDIA_SOURCE_SAMPLES_PER_BUFFER * 2 /* 16 bit signed int LittleEndian */ * 2 /* stereo */)
#define MEDIA_SOURCE_SAMPLES_MULTI_BUFFER_SIZE                    (4 * MEDIA_SOURCE_SAMPLES_BUFFER_SIZE)
#define MEDIA_SOURCE_SAMPLE_BUFFER_PER_CHANNEL                    8192

///////////////////////////////////////////////////////////////////////////////

/* video */
enum VideoFormat
{
    SQCIF,      /*      128 ×  96       */
    QCIF,       /*      176 × 144       */
    CIF,        /*      352 × 288       */
    CIF4,       /*      704 × 576       */
    DVD,        /*      720 × 576       */
    CIF9,       /*     1056 × 864       */
    EDTV,       /*     1280 x 720       */
    CIF16,      /*     1408 × 1152      */
    HDTV,       /*     1920 x 1080      */
};

enum VideoDeviceType{
    GeneralVideoDevice = 0,
    VideoFile,
    Camera,
    Tv,
    SVideoComp // analog video signal from an external source
};

struct VideoDeviceDescriptor
{
    std::string Name; //used for device selection
    std::string Card; //used to open the real device
    std::string Desc; //additional more verbose device information
    enum VideoDeviceType Type;
};

typedef std::vector<VideoDeviceDescriptor> VideoDevices;

struct VideoFormatDescriptor
{
    std::string         Name;
    int                 ResX;
    int                 ResY;
};

typedef std::vector<VideoFormatDescriptor> GrabResolutions;

// source reflection
enum SourceType
{
    SOURCE_UNKNOWN = -1,
    SOURCE_ABSTRACT = 0,
    SOURCE_MUXER,
    SOURCE_DEVICE,
    SOURCE_MEMORY,
    SOURCE_NETWORK,
    SOURCE_FILE
};

enum MediaType
{
    MEDIA_UNKNOWN = -1,
    MEDIA_VIDEO,
    MEDIA_AUDIO
};

/* audio */
enum AudioDeviceType{
    GeneralAudioDevice = 0,
    AudioFile,
    Microphone,
    TvAudio
};

struct AudioDeviceDescriptor
{
    std::string Name;
    std::string Card;
    std::string Desc;
    std::string IoType;
    enum AudioDeviceType Type;
};

typedef std::vector<AudioDeviceDescriptor> AudioDevices;

struct FrameDescriptor
{
    int64_t     Number;
    void        *Data;
};

struct MetaDataEntry
{
    std::string     Key;
    std::string     Value;
};

typedef std::vector<MetaDataEntry> MetaData;

///////////////////////////////////////////////////////////////////////////////

// possible GrabChunk results
#define GRAB_RES_INVALID                            -1
#define GRAB_RES_EOF                                -2

///////////////////////////////////////////////////////////////////////////////

// event handling
#define MarkOpenGrabDeviceSuccessful()                      EventOpenGrabDeviceSuccessful(GetObjectNameStr(this).c_str(), __LINE__)
#define MarkGrabChunkSuccessful(ChunkNumber)                EventGrabChunkSuccessful(GetObjectNameStr(this).c_str(), __LINE__, ChunkNumber)
#define MarkGrabChunkFailed(Reason)                         EventGrabChunkFailed(GetObjectNameStr(this).c_str(), __LINE__, Reason)

// ffmpeg helpers
#define DescribeInput(CodecId, Format)                      FfmpegDescribeInput(GetObjectNameStr(this).c_str(), __LINE__, CodecId, Format)
#define CreateIOContext(PacketBuffer, PacketBufferSize, ReadFunction, WriteFunction, Opaque, IoContext) \
                                                            FfmpegCreateIOContext(GetObjectNameStr(this).c_str(), __LINE__, PacketBuffer, PacketBufferSize, ReadFunction, WriteFunction, Opaque, IoContext)
#define OpenInput(InputName, InputFormat, IoContext)        FfmpegOpenInput(GetObjectNameStr(this).c_str(), __LINE__, InputName, InputFormat, IoContext)
#define DetectAllStreams()                                  FfmpegDetectAllStreams(GetObjectNameStr(this).c_str(), __LINE__)
#define SelectStream()                                      FfmpegSelectStream(GetObjectNameStr(this).c_str(), __LINE__)
#define OpenDecoder()                                       FfmpegOpenDecoder(GetObjectNameStr(this).c_str(), __LINE__)
#define OpenFormatConverter()                               FfmpegOpenFormatConverter(GetObjectNameStr(this).c_str(), __LINE__)
#define CloseFormatConverter()                              FfmpegCloseFormatConverter(GetObjectNameStr(this).c_str(), __LINE__)
#define CloseAll()                                          FfmpegCloseAll(GetObjectNameStr(this).c_str(), __LINE__)
#define EncodeAndWritePacket(FormatContext, CodecContext, InputFrame, BufferedFrames) FfmpegEncodeAndWritePacket(GetObjectNameStr(this).c_str(), __LINE__, FormatContext, CodecContext, InputFrame, BufferedFrames)
#define DetermineMetaData(MetaDataStorage, DebugLog)        FfmpegDetermineMetaData(GetObjectNameStr(this).c_str(), __LINE__, MetaDataStorage, DebugLog)

///////////////////////////////////////////////////////////////////////////////

/* relaying */
typedef int (*IOFunction)(void *pOpaque, uint8_t *pBuffer, int pBufferSize);

class MediaFilter;

class MediaSource :
    public Homer::Monitor::PacketStatistic
{

public:
    MediaSource(std::string pName = "");

    virtual ~MediaSource();

    static void FfmpegInit();

    static bool IsHEVCEncodingSupported();
    static bool IsHEVCDecodingSupported();

    static void LogSupportedVideoCodecs(bool pSendToLoggerOnly = false);
    static void LogSupportedAudioCodecs(bool pSendToLoggerOnly = false);
    static void LogSupportedInputFormats(bool pSendToLoggerOnly = false);
    static void LogSupportedOutputFormats(bool pSendToLoggerOnly = false);

    virtual void ResetPacketStatistic();

    /* multiplexing */
    virtual bool SupportsMuxing();
    virtual std::string GetMuxingCodec();
    virtual void GetMuxingResolution(int &pResX, int &pResY);
    virtual int GetMuxingBufferCounter(); // how many frames are currently buffered for transcoding?
    virtual int GetMuxingBufferSize(); // how many frames can be buffered for transcoding?

    /* get access to current basic media source */
    virtual MediaSource* GetMediaSource();

    /* codec identifier translation */
    static enum AVCodecID GetCodecIDFromGuiName(std::string pName);
    static std::string GetGuiNameFromCodecID(enum AVCodecID pCodecId);
    std::string GetFormatName(enum AVCodecID pCodecId);

    /* audio */
    static int AudioQuality2BitRate(int pQuality);
    virtual int GetOutputSampleRate();
    virtual int GetOutputChannels();
    virtual int GetInputSampleRate();
    virtual int GetInputChannels();
    virtual std::string GetInputFormatStr();

    /* video */
    static AVFrame *AllocFrame();
    static int FillFrame(AVFrame *pFrame, void *pData, enum PixelFormat pPixFormat, int pWidth, int pHeight);
    static void VideoFormat2Resolution(VideoFormat pFormat, int& pX, int& pY);
    static void VideoString2Resolution(std::string pString, int& pX, int& pY);
    virtual float GetInputFrameRate();
    virtual float GetOutputFrameRate();
    virtual void SetFrameRate(float pFps);

    /* audio/video */
    virtual int GetInputBitRate();

    /* A/V sync. */
    virtual int64_t GetSynchronizationTimestamp(); // in us
    virtual int GetSynchronizationPoints(); // how many synchronization points for deriving synchronization timestamp were included in the input stream till now?
    virtual bool TimeShift(int64_t pOffset); // in us, a value of "0" leads to a re-calibration of RT grabbing

    /* transmission quality */
    virtual int64_t GetEndToEndDelay(); // in us
    virtual float GetRelativeLoss();

    /* frame statistics */
    virtual bool SupportsDecoderFrameStatistics();
    virtual int64_t DecodedIFrames();
    virtual int64_t DecodedPFrames();
    virtual int64_t DecodedBFrames();
    virtual int64_t DecodedSFrames();
    virtual int64_t DecodedSIFrames();
    virtual int64_t DecodedSPFrames();
    virtual int64_t DecodedBIFrames();

    /* video grabbing control */
    virtual void SetVideoGrabResolution(int pResX = 352, int pResY = 288);
    virtual void GetVideoGrabResolution(int &pResX, int &pResY);
    virtual GrabResolutions GetSupportedVideoGrabResolutions();
    virtual void GetVideoSourceResolution(int &pResX, int &pResY);
    virtual void GetVideoDisplayAspectRation(int &pHoriz, int &pVert);
    virtual bool HasVariableOutputFrameRate(); // frame duration can change?
    virtual bool IsSeeking();

    /* grabbing control */
    virtual void StopGrabbing();
    virtual bool IsGrabbingStopped();
    virtual bool Reset(enum MediaType = MEDIA_UNKNOWN);
    virtual enum AVCodecID GetSourceCodec();
    virtual std::string GetSourceCodecStr();
    virtual std::string GetSourceCodecDescription();
    virtual bool SetInputStreamPreferences(std::string pStreamCodec, bool pRtpActivated = false, bool pDoReset = false);
    virtual int GetChunkDropCounter(); // how many chunks were dropped?
    virtual int GetFragmentBufferCounter(); // how many fragments are currently buffered?
    virtual int GetFragmentBufferSize(); // how many fragments can be buffered?

    /* frame buffering to compensate reception jitter and short congestion periods */
    virtual float GetFrameBufferPreBufferingTime();
    virtual void SetFrameBufferPreBufferingTime(float pTime);
    virtual float GetFrameBufferTime();
    virtual float GetFrameBufferTimeMax();
    virtual int GetFrameBufferCounter();
    virtual int GetFrameBufferSize();
    virtual void SetPreBufferingActivation(bool pActive);
    virtual void SetPreBufferingAutoRestartActivation(bool pActive);
    virtual int GetDecoderOutputFrameDelay();

    /* filtering */
    virtual void RegisterMediaFilter(MediaFilter *pMediaFilter);
    virtual bool UnregisterMediaFilter(MediaFilter *pMediaFilter, bool pAutoDelete = true);
    void DeleteAllRegisteredMediaFilters();

    /* simple relaying WITHOUT any reencoding functionality but WITH rtp support*/
    // register/unregister: Berkeley sockets based media sinks
        MediaSinkNet* RegisterMediaSink(std::string pTargetHost, unsigned int pTargetPort, Socket* pSocket, bool pRtpActivation, int pMaxFps = 0 /* max. fps */);
        bool UnregisterMediaSink(std::string pTargetHost, unsigned int pTargetPort, bool pAutoDelete = true);
    // register/unregister: GAPI based network sinks
        MediaSinkNet* RegisterMediaSink(string pTarget, Requirements *pTransportRequirements, bool pRtpActivation, int pMaxFps = 0 /* max. fps */);
        bool UnregisterMediaSink(std::string pTarget, Requirements *pTransportRequirements, bool pAutoDelete = true);
    // register/unregister: file based media sinks
        MediaSinkFile* RegisterMediaSink(std::string pTargetFile, bool pRtpActivation);
        bool UnregisterMediaSink(std::string pTargetFile, bool pAutoDelete = true);
    // register/unregister: already allocated media sinks
        MediaSink* RegisterMediaSink(MediaSink *pMediaSink);
        bool UnregisterMediaSink(MediaSink *pMediaSink, bool pAutoDelete = true);
    // register/unregister: media sink of any type
        bool UnregisterGeneralMediaSink(std::string pId, bool pAutoDelete = true);
    std::vector<std::string> ListRegisteredMediaSinks();
    void DeleteAllRegisteredMediaSinks();
    void SetRtpActivation(bool pState);
    bool GetRtpActivation();

    /* relaying */
    virtual bool SupportsRelaying();
    virtual int GetEncoderBufferedFrames();

    /* recording control WITH reencoding but WITHOUT rtp support */
    virtual bool StartRecording(std::string pSaveFileName, int SaveFileQuality = 100, bool pRealTime = true /* 1 = frame rate emulation, 0 = no pts adaption */); // needs valid mCodecContext, otherwise RGB32 pictures are assumed as input; source resolution must not change during recording is activated
    virtual void StopRecording();
    virtual bool SupportsRecording();
    virtual bool IsRecording();
    virtual int64_t RecordingTime(); // in seconds

    /* device control */
    virtual std::string GetSourceTypeStr();
    virtual enum SourceType GetSourceType();
    virtual std::string GetMediaTypeStr();
    virtual enum MediaType GetMediaType();
    virtual void getVideoDevices(VideoDevices &pVList);
    virtual void getAudioDevices(AudioDevices &pAList);
    virtual bool SelectDevice(std::string pDeviceName, enum MediaType pMediaType, bool &pIsNewDevice);
    virtual std::string GetBroadcasterName();
    virtual std::string GetBroadcasterStreamName();
    virtual std::string GetCurrentDeviceName();
    virtual bool RegisterMediaSource(MediaSource *pMediaSource);
    virtual bool UnregisterMediaSource(MediaSource *pMediaSource, bool pAutoDelete = true);

    /* seek interface */
    virtual bool SupportsSeeking();
    virtual float GetSeekEnd(); // get maximum seek time in seconds
    virtual bool Seek(float pSeconds, bool pOnlyKeyFrames = true);
    virtual float GetSeekPos(); // in seconds

    /* multi stream input interface */
    virtual bool SupportsMultipleInputStreams();
    virtual bool SelectInputStream(int pIndex);
    virtual std::string CurrentInputStream();
    virtual std::vector<std::string> GetInputStreams();
    virtual bool HasInputStreamChanged();

    /* live OSD marking */
    virtual bool SupportsMarking();
    virtual bool MarkerActive();
    virtual void SetMarker(bool pActivation = true);
    virtual void MoveMarker(float pRelX, float pRelY);

    /* meta data*/
    virtual MetaData GetMetaData();

public:
    /* abstract interface which has to be implemented by derived classes */
    virtual bool OpenVideoGrabDevice(int pResX = 352, int pResY = 288, float pFps = 29.97) = 0;
    virtual bool OpenAudioGrabDevice(int pSampleRate = 44100, int pChannels = 2) = 0;
    virtual bool CloseGrabDevice() = 0;
    // for video: grabs RGB32 image with correct resolution, for audio: grabs 16 bit little endian samples of 4KB size
    // see GRAB_RES_* for possible function results
    // HINT: function assumes that given buffer has size of 4kB for audio samples
    virtual int GrabChunk(void* pChunkBuffer, int& pChunkSize, bool pDropChunk = false) = 0;

    /* default memory management based on ffmpeg */
    virtual void* AllocChunkBuffer(int& pChunkBufferSize, enum MediaType pMediaType = MEDIA_UNKNOWN); // allocates a fitting buffer with correct size
    virtual void FreeChunkBuffer(void *pChunk);
    virtual void DeleteAllRegisteredMediaFileSources();

private:
    /* lock manager for ffmpeg internal mutex usage */
    static int FfmpegLockManager(void **pMutex, enum AVLockOp pMutexOperation);

    /* callback function to signal interrupt of avformat_open_input / avformat_find_stream_info */
    static int FindStreamInfoCallback(void *pMediaSource);

protected:
    /* real-time GRABBING */
    virtual void CalibrateRTGrabbing();
    virtual bool WaitForRTGrabbing(); // waits so that a desired input frame rate is simulated

    /* internal video resolution switch */
    virtual void DoSetVideoGrabResolution(int pResX = 352, int pResY = 288);

    /* filtering */
    virtual void RelayChunkToMediaFilters(char* pPacketData, unsigned int pPacketSize, int64_t pPacketTimestamp, bool pIsKeyFrame = false);
    friend class VideoScaler; // for access to "RelayChunkToMediaFilters()"

    /* internal interface for packet relaying */
    virtual void RelayAVPacketToMediaSinks(AVPacket *pAVPacket);
    virtual void RelaySyncTimestampToMediaSinks(int64_t pReferenceNtpTimestamp, int64_t pReferenceFrameTimestamp);

    /* internal interface for stream recordring */
    void RecordFrame(AVFrame *pSourceFrame);
    void RecordRGBPicture(char *pSourcePicture, int pSourcePictureSize);
    void RecordSamples(int16_t *pSourceSamples, int pSourceSamplesSize);

    /* frame stats */
    std::string GetFrameType(AVFrame *pFrame);
    void AnnounceFrame(AVFrame *pFrame);

    /* video fps emulation */
    void InitFpsEmulator(); // auto. called by MarkOpenGrabDeviceSuccessful
    int64_t GetPtsFromFpsEmulator(); //needs correct mFrameRate value

    /* audio silence */
    bool ContainsOnlySilence(void* pChunkBuffer, int pChunkSize);// assumes signed 16 bit integers per sample

    /* event handling */
    void EventOpenGrabDeviceSuccessful(std::string pSource, int pLine);
    void EventGrabChunkSuccessful(std::string pSource, int pLine, int pChunkNumber);
    void EventGrabChunkFailed(std::string pSource, int pLine, std::string pReason);

    /* FFMPEG helpers */
    bool FfmpegDescribeInput(string pSource/* caller source */, int pLine /* caller line */, AVCodecID pCodecId, AVInputFormat **pFormat);

public:
    static bool FfmpegCreateIOContext(string pSource/* caller source */, int pLine /* caller line */, char *pPacketBuffer, int pPacketBufferSize, IOFunction pReadFunction, IOFunction pWriteFunction, void *pOpaque, AVIOContext **pIoContext);

protected:
    /* decoder helpers */
    bool FfmpegOpenInput(string pSource /* caller source */, int pLine /* caller line */, const char *pInputName, AVInputFormat *pInputFormat = NULL, AVIOContext *pIOContext = NULL);
    bool FfmpegDetectAllStreams(string pSource /* caller source */, int pLine /* caller line */); //avformat_open_input must be called before, returns true on success
    bool FfmpegSelectStream(string pSource /* caller source */, int pLine /* caller line */); //avformat_open_input & avformat_find_stream_info must be called before, returns true on success
    bool FfmpegOpenDecoder(string pSource /* caller source */, int pLine /* caller line */); //avformat_open_input & avformat_find_stream_info must be called before, returns true on success
    bool FfmpegOpenFormatConverter(string pSource /* caller source */, int pLine /* caller line */);
    bool FfmpegCloseFormatConverter(string pSource /* caller source */, int pLine /* caller line */);
    bool FfmpegCloseAll(string pSource /* caller source */, int pLine /* caller line */);

    /* encoder helpers */
    bool FfmpegEncodeAndWritePacket(string pSource /* caller source */, int pLine /* caller line */, AVFormatContext *pFormatContext, AVCodecContext *pCodecContext, AVFrame *pInputFrame, int &pBufferedFrames);

    void FfmpegDetermineMetaData(string pSource /* caller source */, int pLine /* caller line */, AVDictionary *pMetaData, bool pDebugLog);


    bool                mMediaSourceOpened;
    bool                mGrabbingStopped;
    MetaData            mMetaData;
    bool                mRecording;
    std::string         mRecordingSaveFileName;
    int                 mFrameNumber;
    int                 mFrameDuration; // the factor by which the frame timestamps are multipled, 1 for usual sources, 3600 for MPEG2TS
    int                 mChunkDropCounter;
    Mutex               mGrabMutex;
    enum MediaType      mMediaType;
    enum SourceType     mSourceType;
    AVFormatContext     *mFormatContext;
    AVStream            *mMediaStream;
    AVCodecContext      *mCodecContext;
    int                 mMediaStreamIndex;
    double              mSourceStartTimeForRTGrabbing;
    double              mSourceTimeShiftForRTGrabbing;
    double              mInputStartPts;
    double              mNumberOfFrames;
    enum AVCodecID      mSourceCodecId;
    bool                mEOFReached;
    /* RT grabbing */
    std::list<int64_t>  mRTGrabbingFrameTimestamps;
    /* audio */
    AVFifoBuffer        *mResampleFifo[MEDIA_SOURCE_MAX_AUDIO_CHANNELS];
    uint8_t             *mResampleBufferPlanes[MEDIA_SOURCE_MAX_AUDIO_CHANNELS];
    HM_SwrContext       *mAudioResampleContext;
    char                *mResampleBuffer;
    int                 mOutputAudioSampleRate;
    int                 mOutputAudioChannels; // 1 - mono, 2 - stereo, ..
    enum AVSampleFormat mOutputAudioFormat;
    int                 mInputAudioSampleRate;
    int                 mInputAudioChannels;
    enum AVSampleFormat mInputAudioFormat;
    int                 mInputBitRate;
    /* audio silence */
    int                 mAudioSilenceThreshold;
    /* video */
    GrabResolutions     mSupportedVideoFormats;
    int                 mSourceResX;
    int                 mSourceResY;
    int                 mTargetResX;
    int                 mTargetResY;
    SwsContext          *mVideoScalerContext;
    /* audio/video */
    float               mInputFrameRate;
    float               mOutputFrameRate; // presentation frame rate
    /* frame stats */
    int64_t             mDecodedIFrames;
    int64_t             mDecodedPFrames;
    int64_t             mDecodedBFrames;
    int64_t             mDecodedSFrames;
    int64_t             mDecodedSIFrames;
    int64_t             mDecodedSPFrames;
    int64_t             mDecodedBIFrames;
    /* frame pre-buffering */
    float               mDecoderFrameBufferTime; // current pre-buffer length
    float               mDecoderFrameBufferTimeMax; // max. pre-buffer length
    float               mDecoderFramePreBufferTime;
    bool                mDecoderFramePreBufferingAutoRestart;
    /* A/V synch. */
    int                 mDecoderOutputFrameDelay;
    /* live OSD marking */
    float               mMarkerRelX;
    float               mMarkerRelY;
    bool                mMarkerActivated;
    /* relaying */
    MediaSinks          mMediaSinks;
    Mutex               mMediaSinksMutex;
    /* filtering */
    MediaFilters        mMediaFilters;
    Mutex               mMediaFiltersMutex;
    friend class MediaSourceMuxer; // for access to "mMediaFilters"

    /* recording */
    HM_SwrContext       *mRecorderAudioResampleContext;
    AVFifoBuffer        *mRecorderResampleFifo[MEDIA_SOURCE_MAX_AUDIO_CHANNELS];
    char                *mRecorderResampleBuffer;
    uint8_t             *mRecorderResampleBufferPlanes[MEDIA_SOURCE_MAX_AUDIO_CHANNELS];
    AVStream            *mRecorderEncoderStream;
    int                 mRecorderAudioSampleRate;
    int                 mRecorderAudioChannels;
    enum AVSampleFormat mRecorderAudioFormat;
    uint64_t            mRecorderAudioChannelLayout;
    AVFormatContext     *mRecorderFormatContext;
    AVCodecContext      *mRecorderCodecContext;
    SwsContext          *mRecorderVideoScalerContext;
    int64_t             mRecorderFrameNumber;
    AVFrame             *mRecorderFinalFrame;
    int64_t             mRecorderStart;
    /* device handling */
    std::string         mDesiredDevice;
    std::string         mCurrentDevice;
    std::string         mCurrentDeviceName;
    /* event handling */
    bool                mLastGrabResultWasError;
    std::string         mLastGrabFailureReason;
    /* multiple channel input */
    int                 mCurrentInputChannel;
    int                 mDesiredInputChannel;
    /* ffmpeg init */
    static Mutex        mFfmpegInitMutex;
    static bool         mFfmpegInitiated;
};

typedef std::vector<MediaSource*>      MediaSources;

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
