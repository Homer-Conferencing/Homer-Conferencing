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
 * Author:  Thomas Volkert
 * Since:   2008-12-02
 */

#ifndef _MULTIMEDIA_MEDIA_SOURCE_
#define _MULTIMEDIA_MEDIA_SOURCE_

#include <Header_Ffmpeg.h>
#include <PacketStatistic.h>
#include <MediaSinkNet.h>
#include <MediaSinkFile.h>
#include <MediaSink.h>
#include <HBMutex.h>

#include <vector>
#include <string>

using namespace Homer::Base;

namespace Homer { namespace Multimedia {

///////////////////////////////////////////////////////////////////////////////

// the following de/activates debugging of packets
//#define MS_DEBUG_PACKETS
// the following de/activates debugging of ffmpag mutex management
//#define MS_DEBUG_FFMPEG_MUTEX

///////////////////////////////////////////////////////////////////////////////

// video/audio processing
#define MEDIA_SOURCE_AV_CHUNK_BUFFER_SIZE                         16 * 1000 * 1000 // HDTV RGB32 picture: 1920*1080*4 = ca. 7,9 MB

// audio processing
#define MEDIA_SOURCE_SAMPLES_CAPTURE_FIFO_SIZE                    64 // amount of capture buffers within the FIFO
#define MEDIA_SOURCE_SAMPLES_PLAYBACK_FIFO_SIZE                   64 // amount of playback buffers within the FIFO
#define MEDIA_SOURCE_SAMPLES_PER_BUFFER                           1024
#define MEDIA_SOURCE_SAMPLES_BUFFER_SIZE                          (MEDIA_SOURCE_SAMPLES_PER_BUFFER * 2 /* 16 bit LittleEndian */ * 2 /* stereo */)
#define MEDIA_SOURCE_SAMPLES_MULTI_BUFFER_SIZE                    (4 * MEDIA_SOURCE_SAMPLES_BUFFER_SIZE)

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
    Tv
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
    Microphone
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

struct ChunkDescriptor
{
    int64_t     Pts;
};

///////////////////////////////////////////////////////////////////////////////

// possible GrabChunk results
#define GRAB_RES_INVALID                            -1
#define GRAB_RES_EOF                                -2

///////////////////////////////////////////////////////////////////////////////

// event handling
#define MarkOpenGrabDeviceSuccessful()              		EventOpenGrabDeviceSuccessful(GetObjectNameStr(this).c_str(), __LINE__)
#define MarkGrabChunkSuccessful(ChunkNumber)        		EventGrabChunkSuccessful(GetObjectNameStr(this).c_str(), __LINE__, ChunkNumber)
#define MarkGrabChunkFailed(Reason)                 		EventGrabChunkFailed(GetObjectNameStr(this).c_str(), __LINE__, Reason)

// ffmpeg helpers
#define DescribeInput(CodecId, Format)						FfmpegDescribeInput(GetObjectNameStr(this).c_str(), __LINE__, CodecId, Format)
#define CreateIOContext(PacketBuffer, PacketBufferSize, ReadFunction, WriteFunction, Opaque, IoContext) \
															FfmpegCreateIOContext(GetObjectNameStr(this).c_str(), __LINE__, PacketBuffer, PacketBufferSize, ReadFunction, WriteFunction, Opaque, IoContext)
#define OpenInput(InputName, InputFormat, IoContext)       	FfmpegOpenInput(GetObjectNameStr(this).c_str(), __LINE__, InputName, InputFormat, IoContext)
#define DetectAllStreams()            						FfmpegDetectAllStreams(GetObjectNameStr(this).c_str(), __LINE__)
#define SelectStream()              						FfmpegSelectStream(GetObjectNameStr(this).c_str(), __LINE__)
#define OpenDecoder()										FfmpegOpenDecoder(GetObjectNameStr(this).c_str(), __LINE__)
#define OpenFormatConverter()								FfmpegOpenFormatConverter(GetObjectNameStr(this).c_str(), __LINE__)
#define CloseAll()											FfmpegCloseAll(GetObjectNameStr(this).c_str(), __LINE__)

///////////////////////////////////////////////////////////////////////////////

/* relaying */
class MediaSource;
typedef std::vector<MediaSink*>        MediaSinks;
typedef std::vector<MediaSource*>      MediaSources;
typedef int (*IOFunction)(void *pOpaque, uint8_t *pBuffer, int pBufferSize);

class MediaSource :
    public Homer::Monitor::PacketStatistic
{

public:
    MediaSource(std::string pName = "");

    virtual ~MediaSource();

    static void FfmpegInit();

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
    enum CodecID GetCodecIDFromGuiName(std::string pName);
    static std::string GetGuiNameFromCodecID(enum CodecID pCodecId);
    std::string GetFormatName(enum CodecID pCodecId);

    /* audio */
    static int AudioQuality2BitRate(int pQuality);
    virtual int GetOutputSampleRate();
    virtual int GetOutputChannels();
    virtual int GetInputSampleRate();
    virtual int GetInputChannels();

    /* video */
    static AVFrame *AllocFrame();
    static int FillFrame(AVFrame *pFrame, void *pData, enum PixelFormat pPixFormat, int pWidth, int pHeight);
    static void VideoFormat2Resolution(VideoFormat pFormat, int& pX, int& pY);
    static void VideoString2Resolution(std::string pString, int& pX, int& pY);
    virtual float GetFrameRate();
    virtual float GetFrameRatePlayout();
    virtual void SetFrameRate(float pFps);

    /* frame statistics */
    virtual bool SupportsDecoderFrameStatistics();
    virtual int64_t DecodedIFrames();
    virtual int64_t DecodedPFrames();
    virtual int64_t DecodedBFrames();

    /* video grabbing control */
    virtual void SetVideoGrabResolution(int pResX = 352, int pResY = 288);
    virtual void GetVideoGrabResolution(int &pResX, int &pResY);
    virtual GrabResolutions GetSupportedVideoGrabResolutions();
    virtual void GetVideoSourceResolution(int &pResX, int &pResY);

    /* grabbing control */
    virtual void StopGrabbing();
    virtual bool Reset(enum MediaType = MEDIA_UNKNOWN);
    virtual enum CodecID GetCodecID();
    virtual std::string GetCodecName();
    virtual std::string GetCodecLongName();
    virtual bool SetInputStreamPreferences(std::string pStreamCodec, bool pDoReset = false);
    virtual int GetChunkDropCounter(); // how many chunks were dropped?
    virtual int GetFragmentBufferCounter(); // how many fragments are currently buffered?
    virtual int GetFragmentBufferSize(); // how many fragments can be buffered?

    /* frame buffering to compensate reception jitter and short congestion periods */
    virtual float GetFrameBufferTime();
    virtual float GetFrameBufferTimeMax();
    virtual int GetFrameBufferCounter();
    virtual int GetFrameBufferSize();

    /* simple relaying WITHOUT any reencoding functionality but WITH rtp support*/
	// register/unregister: Berkeley sockets based media sinks
    	MediaSinkNet* RegisterMediaSink(std::string pTargetHost, unsigned int pTargetPort, Socket* pSocket, bool pRtpActivation, int pMaxFps = 0 /* max. fps */);
    	bool UnregisterMediaSink(std::string pTargetHost, unsigned int pTargetPort, bool pAutoDelete = true);
	// register/unregister: GAPI based network sinks
    	MediaSinkNet* RegisterMediaSink(string pTarget, Requirements *pTransportRequirements, bool pRtpActivation, int pMaxFps = 0 /* max. fps */);
    	bool UnregisterMediaSink(std::string pTarget, Requirements *pTransportRequirements, bool pAutoDelete = true);
	// register/unregister: file based media sinks
    	MediaSinkFile* RegisterMediaSink(std::string pTargetFile);
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
    virtual bool SupportsRelaying();

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
    virtual std::string GetCurrentDeviceName();
    virtual std::string GetCurrentDevicePeerName();
    virtual bool RegisterMediaSource(MediaSource *pMediaSource);
    virtual bool UnregisterMediaSource(MediaSource *pMediaSource, bool pAutoDelete = true);

    /* seek interface */
    virtual bool SupportsSeeking();
    virtual float GetSeekEnd(); // get maximum seek time in seconds
    virtual bool Seek(float pSeconds, bool pOnlyKeyFrames = true);
    virtual bool SeekRelative(float pSeconds, bool pOnlyKeyFrames = true);
    virtual float GetSeekPos(); // in seconds

    /* multi stream input interface */
    virtual bool SupportsMultipleInputStreams();
    virtual bool SelectInputStream(int pIndex);
    virtual std::string CurrentInputStream();
    virtual std::vector<std::string> GetInputStreams();

    /* live OSD marking */
    virtual bool SupportsMarking();
    virtual bool MarkerActive();
    virtual void SetMarker(bool pActivation = true);
    virtual void MoveMarker(float pRelX, float pRelY);

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
    virtual void FreeUnusedRegisteredFileSources();

private:
    /* lock manager for ffmpeg internal mutex usage */
    static int FfmpegLockManager(void **pMutex, enum AVLockOp pMutexOperation);

protected:
    /* internal video resolution switch */
    virtual void DoSetVideoGrabResolution(int pResX = 352, int pResY = 288);

    /* internal interface for packet relaying */
    virtual void RelayPacketToMediaSinks(char* pPacketData, unsigned int pPacketSize, bool pIsKeyFrame = false);

    /* internal interface for stream recordring */
    void RecordFrame(AVFrame *pSourceFrame);
    void RecordSamples(int16_t *pSourceSamples, int pSourceSamplesSize);

    /* frame stats */
    void AnnounceFrame(AVFrame *pFrame);

    /* video fps emulation */
    void FpsEmulationInit(); // auto. called by MarkOpenGrabDeviceSuccessful
    int64_t FpsEmulationGetPts(); //needs correct mFrameRate value

    /* audio silence */
    bool ContainsOnlySilence(void* pChunkBuffer, int pChunkSize);// assumes signed 16 bit integers per sample

    /* event handling */
    void EventOpenGrabDeviceSuccessful(std::string pSource, int pLine);
    void EventGrabChunkSuccessful(std::string pSource, int pLine, int pChunkNumber);
    void EventGrabChunkFailed(std::string pSource, int pLine, std::string pReason);

    /* FFMPEG helpers */
    bool FfmpegDescribeInput(string pSource/* caller source */, int pLine /* caller line */, CodecID pCodecId, AVInputFormat **pFormat);
public:
    static bool FfmpegCreateIOContext(string pSource/* caller source */, int pLine /* caller line */, char *pPacketBuffer, int pPacketBufferSize, IOFunction pReadFunction, IOFunction pWriteFunction, void *pOpaque, AVIOContext **pIoContext);
protected:
    bool FfmpegOpenInput(string pSource /* caller source */, int pLine /* caller line */, const char *pInputName, AVInputFormat *pInputFormat = NULL, AVIOContext *pIOContext = NULL);
    bool FfmpegDetectAllStreams(string pSource /* caller source */, int pLine /* caller line */); //avformat_open_input must be called before, returns true on success
    bool FfmpegSelectStream(string pSource /* caller source */, int pLine /* caller line */); //avformat_open_input & avformat_find_stream_info must be called before, returns true on success
    bool FfmpegOpenDecoder(string pSource /* caller source */, int pLine /* caller line */); //avformat_open_input & avformat_find_stream_info must be called before, returns true on success
    bool FfmpegOpenFormatConverter(string pSource /* caller source */, int pLine /* caller line */);
    bool FfmpegCloseAll(string pSource /* caller source */, int pLine /* caller line */);

    bool                mMediaSourceOpened;
    bool                mGrabbingStopped;
    bool                mRecording;
    std::string         mRecordingSaveFileName;
    int                 mChunkNumber;
    int                 mChunkDropCounter;
    Mutex               mGrabMutex;
    enum MediaType      mMediaType;
    enum SourceType     mSourceType;
    AVFormatContext     *mFormatContext;
    AVCodecContext      *mCodecContext;
    int                 mMediaStreamIndex;
    double              mSourceStartTimeForRTGrabbing;
    double              mSourceStartPts;
    double              mNumberOfFrames;
    enum CodecID        mSourceCodecId;
    bool                mEOFReached;
    /* audio */
    ReSampleContext     *mAudioResampleContext;
    char                *mResampleBuffer;
    AVFifoBuffer        *mRecorderSampleFifo;
    char                *mRecorderSamplesTempBuffer;
    int                 mOutputAudioSampleRate;
    int                 mOutputAudioChannels; // 1 - mono, 2 - stereo, ..
    enum AVSampleFormat mOutputAudioFormat;
    int					mInputAudioSampleRate;
    int  				mInputAudioChannels;
    enum AVSampleFormat mInputAudioFormat;
    /* video */
    GrabResolutions     mSupportedVideoFormats;
    int                 mSourceResX;
    int                 mSourceResY;
    int                 mTargetResX;
    int                 mTargetResY;
    float               mFrameRate; // ffmpeg internal referen frame rate
    float 				mRealFrameRate; // presentation frame rate
    SwsContext          *mScalerContext;
    /* frame stats */
    int64_t             mDecodedIFrames;
    int64_t             mDecodedPFrames;
    int64_t             mDecodedBFrames;
    /* frame pre-buffering */
    float				mDecoderBufferTime; // current pre-buffer length
    float               mDecoderBufferTimeMax; // max. pre-buffer length
    /* live OSD marking */
    float               mMarkerRelX;
    float               mMarkerRelY;
    bool                mMarkerActivated;
    /* relaying */
    MediaSinks          mMediaSinks;
    Mutex               mMediaSinksMutex;
    bool                mRtpActivated;
    /* recording */
    AVFormatContext     *mRecorderFormatContext;
    AVCodecContext      *mRecorderCodecContext;
    SwsContext          *mRecorderScalerContext;
    char                *mRecorderEncoderChunkBuffer;
    int                 mRecorderChunkNumber;
    int64_t             mRecorderStartPts; // for synchronized playback we calculate the position within a media stream and write the value into PTS entry of an encoded packet
    bool                mRecorderRealTime;
    AVFrame             *mRecorderFinalFrame;
    int64_t             mRecorderStart;
    /* device handling */
    std::string         mDesiredDevice;
    std::string         mCurrentDevice;
    std::string			mCurrentDeviceName;
    /* event handling */
    bool                mLastGrabResultWasError;
    std::string         mLastGrabFailureReason;
    /* multiple channel input */
    int                 mCurrentInputChannel;
    int                 mDesiredInputChannel;
    /* ffmpeg init */
    static Mutex 		mFfmpegInitMutex;
    static bool 		mFfmpegInitiated;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace

#endif
